#pragma once
// Development-only measurement probe. Runs REAL ImportStore commands and captures
// their first row images in the same SQLCipher transaction. This is not a shipped
// Undo implementation: in particular deleted immutable bodies are moved into the
// probe journal, not yet split from active memberships in the production schema.
#include <spentit/import_store.hpp>
#include <sqlite3.h>
#ifdef SPENTIT_ACTION_PAYLOAD_PROBE
#include "action_payload_schema.hpp"
#endif
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace action_probe {
using namespace spentit;
using storage::ImportStore;
constexpr auto password="synthetic-storage-comparison";
sqlite3* connection=nullptr;
int extension(sqlite3* db,char**,const sqlite3_api_routines*) { connection=db;return SQLITE_OK; }
void require(bool test,const char* message) {if(!test)throw std::runtime_error(message);}
void sql(sqlite3* db,const std::string& text) {
    char* error=nullptr;const int rc=sqlite3_exec(db,text.c_str(),nullptr,nullptr,&error);
    if(rc!=SQLITE_OK){std::string detail=error?error:"SQL failure";sqlite3_free(error);throw std::runtime_error(detail);}
}
struct Query {
    sqlite3_stmt* q=nullptr;
    Query(sqlite3* db,const std::string& text){require(sqlite3_prepare_v2(db,text.c_str(),-1,&q,nullptr)==SQLITE_OK,"Probe query prepare failed");}
    ~Query(){sqlite3_finalize(q);}
    bool next(){const auto rc=sqlite3_step(q);require(rc==SQLITE_ROW||rc==SQLITE_DONE,"Probe query step failed");return rc==SQLITE_ROW;}
    std::string text(int n){const auto* p=sqlite3_column_text(q,n);return p?std::string(reinterpret_cast<const char*>(p),static_cast<std::size_t>(sqlite3_column_bytes(q,n))):std::string{};}
    std::int64_t number(int n){return sqlite3_column_int64(q,n);}
};
std::int64_t number(sqlite3* db,const std::string& text){Query q(db,text);require(q.next(),"Missing probe scalar");return q.number(0);}
std::string sql_identifier(const std::string& value){std::string out="\"";for(char c:value){out+=c;if(c=='\"')out+=c;}return out+'\"';}
void unsigned_value(std::string& out,std::uint64_t value){do{auto byte=static_cast<unsigned char>(value&127);value>>=7;if(value)byte|=128;out+=static_cast<char>(byte);}while(value);}
// Version 1: value count, then SQLite type tag and exact signed-int64 bits or
// byte length/body. NULL and empty TEXT/BLOB remain distinct; REAL is refused.
void pack(sqlite3_context* ctx,int count,sqlite3_value** values){try{
    std::string out(1,'\1');unsigned_value(out,static_cast<std::uint64_t>(count));
    for(int i=0;i<count;++i){const int type=sqlite3_value_type(values[i]);out+=static_cast<char>(type);
        if(type==SQLITE_INTEGER){auto value=static_cast<std::uint64_t>(sqlite3_value_int64(values[i]));for(int n=0;n<8;++n){out+=static_cast<char>(value&255);value>>=8;}}
        else if(type==SQLITE_TEXT||type==SQLITE_BLOB){const auto size=sqlite3_value_bytes(values[i]);unsigned_value(out,static_cast<std::uint64_t>(size));if(size)out.append(static_cast<const char*>(sqlite3_value_blob(values[i])),static_cast<std::size_t>(size));}
        else if(type!=SQLITE_NULL){sqlite3_result_error(ctx,"Unsupported journal value type",-1);return;}
    }
    sqlite3_result_blob64(ctx,out.data(),out.size(),SQLITE_TRANSIENT);
}catch(...){sqlite3_result_error_nomem(ctx);}}
struct Table {std::string name;std::vector<std::string> columns,keys;};
std::vector<Table> tables(sqlite3* db){std::vector<Table> out;Query names(db,"SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name NOT LIKE 'probe_%' ORDER BY name");while(names.next()){
    Table table{names.text(0),{}, {}};std::map<int,std::string> keys;Query columns(db,"PRAGMA table_info("+sql_identifier(table.name)+")");while(columns.next()){table.columns.push_back(columns.text(1));if(columns.number(5))keys.emplace(static_cast<int>(columns.number(5)),columns.text(1));}
    for(const auto& [order,key]:keys){(void)order;table.keys.push_back(key);}require(!table.keys.empty(),"Probe refuses a table without a stable declared key");out.push_back(std::move(table));
}return out;}
std::string expression(const std::vector<std::string>& columns,const std::string& prefix){std::string out="probe_pack(";for(const auto& column:columns){if(out.back()!='(')out+=',';out+=prefix+'.'+sql_identifier(column);}return out+')';}
std::string key_match(const std::vector<std::string>& keys,const std::string& left,const std::string& right){std::string out;for(const auto& key:keys){if(!out.empty())out+=" AND ";out+=left+'.'+sql_identifier(key)+" IS "+right+'.'+sql_identifier(key);}return out;}
std::string key_parameters(const std::vector<std::string>& keys){std::string out;for(const auto& key:keys){if(!out.empty())out+=" AND ";out+=sql_identifier(key)+" IS ?";}return out;}
std::string source(int count,int offset=0){std::string text;for(int i=0;i<count;++i)text+="D01/02/2026\nT-1.00\nPSynthetic "+std::to_string(i+offset)+"\nN"+std::to_string(i+offset)+"\n^\n";return text;}
// Monotonic receipts and markers are measured separately, not reversed. This
// probe captures ALL other tables including disposable caches; later integration
// must replace this deliberately conservative policy with the checked manifest.
const std::set<std::string> monotonic={"operation_registry","command_completions","document_session","mutation_generation"};
void install(sqlite3* db,const std::vector<Table>& manifest){
    require(sqlite3_create_function_v2(db,"probe_pack",-1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,pack,nullptr,nullptr,nullptr)==SQLITE_OK,"Cannot register probe codec");
    sql(db,"PRAGMA application_id=1397770578"); // Not an openable SpentIt document.
    sql(db,"CREATE TABLE IF NOT EXISTS probe_inverse(action INTEGER NOT NULL,table_id INTEGER NOT NULL,key BLOB NOT NULL,prior BLOB,PRIMARY KEY(action,table_id,key)) STRICT, WITHOUT ROWID;");
    sql(db,"CREATE TEMP TABLE probe_context(action INTEGER NOT NULL); INSERT INTO probe_context VALUES(0);");
    for(std::size_t i=0;i<manifest.size();++i){const auto& t=manifest[i];if(monotonic.contains(t.name))continue;
        // Capture an existing key before INSERT OR REPLACE removes it, even with
        // recursive_triggers off. Ignored inserts retain an identical image and
        // are coalesced before a real coordinator would publish an action.
        sql(db,"CREATE TEMP TRIGGER "+sql_identifier("probe_replace_"+std::to_string(i))+" BEFORE INSERT ON main."+sql_identifier(t.name)+" WHEN (SELECT action FROM probe_context)>0 BEGIN INSERT INTO probe_inverse SELECT (SELECT action FROM probe_context),"+std::to_string(i)+","+expression(t.keys,"t")+","+expression(t.columns,"t")+" FROM "+sql_identifier(t.name)+" t WHERE "+key_match(t.keys,"t","NEW")+" ON CONFLICT(action,table_id,key) DO NOTHING; END;");
        sql(db,"CREATE TEMP TRIGGER "+sql_identifier("probe_key_"+std::to_string(i))+" BEFORE UPDATE ON main."+sql_identifier(t.name)+" WHEN "+expression(t.keys,"OLD")+"!="+expression(t.keys,"NEW")+" BEGIN SELECT RAISE(ABORT,'Prototype refuses key-changing updates'); END;");
        for(const auto& operation:{"INSERT","UPDATE","DELETE"}){
            const bool inserted=std::string_view(operation)=="INSERT";const auto prefix=inserted?"NEW":"OLD";
            sql(db,"CREATE TEMP TRIGGER "+sql_identifier("probe_"+std::to_string(i)+operation)+(inserted?" AFTER ":" BEFORE ")+operation+" ON main."+sql_identifier(t.name)+" WHEN (SELECT action FROM probe_context)>0 BEGIN INSERT INTO probe_inverse VALUES((SELECT action FROM probe_context),"+std::to_string(i)+","+expression(t.keys,prefix)+","+(inserted?"NULL":expression(t.columns,"OLD"))+") ON CONFLICT(action,table_id,key) DO NOTHING; END;");
        }
    }
}


#ifndef SPENTIT_ACTION_PAYLOAD_PROBE
// Liveness experiment on an already-open synthetic fixture. Conversion itself
// is measured and is not the production initialization strategy/storage gate.
void split_source_payload(sqlite3* db){
    sql(db,"BEGIN IMMEDIATE; CREATE TABLE probe_source_payload(id TEXT PRIMARY KEY,bytes BLOB NOT NULL) STRICT; INSERT INTO probe_source_payload SELECT * FROM sources; ALTER TABLE sources RENAME TO source_memberships; ALTER TABLE source_memberships DROP COLUMN bytes; ALTER TABLE source_memberships ADD COLUMN source_order INTEGER NOT NULL DEFAULT 0; DROP TRIGGER evidence_immutable; UPDATE source_memberships SET source_order=rowid; CREATE TRIGGER evidence_immutable BEFORE UPDATE ON source_memberships BEGIN SELECT RAISE(ABORT,'immutable'); END; CREATE UNIQUE INDEX probe_source_order ON source_memberships(source_order);");
    sql(db,"CREATE VIEW sources AS SELECT m.id,p.bytes,m.source_order AS rowid FROM source_memberships m JOIN probe_source_payload p USING(id);");
    sql(db,"CREATE TRIGGER source_payload_insert INSTEAD OF INSERT ON sources BEGIN INSERT INTO probe_source_payload VALUES(NEW.id,NEW.bytes); INSERT INTO source_memberships VALUES(NEW.id,NEW.rowid); END;");
    sql(db,"CREATE TRIGGER source_membership_delete INSTEAD OF DELETE ON sources BEGIN DELETE FROM source_memberships WHERE id=OLD.id; END;");
    sql(db,"CREATE TRIGGER source_payload_immutable BEFORE UPDATE ON probe_source_payload BEGIN SELECT RAISE(ABORT,'Immutable source payload'); END;");
    sql(db,"CREATE TABLE probe_source_pins(action INTEGER NOT NULL,payload TEXT NOT NULL REFERENCES probe_source_payload(id),PRIMARY KEY(action,payload)) STRICT, WITHOUT ROWID; CREATE TRIGGER source_payload_live BEFORE DELETE ON probe_source_payload WHEN EXISTS(SELECT 1 FROM source_memberships WHERE id=OLD.id) BEGIN SELECT RAISE(ABORT,'Live source payload'); END; COMMIT;");
}
void install_source_pin(sqlite3* db){
    sql(db,"CREATE TEMP TRIGGER probe_pin_source BEFORE DELETE ON main.source_memberships WHEN (SELECT action FROM probe_context)>0 BEGIN INSERT INTO probe_source_pins SELECT action,OLD.id FROM probe_context WHERE action>0 ON CONFLICT(action,payload) DO NOTHING; END;");
}

#endif
#ifdef SPENTIT_ACTION_PAYLOAD_PROBE
void install_payload_pins(sqlite3* db){
    sql(db,"CREATE TABLE IF NOT EXISTS probe_payload_pins(action INTEGER NOT NULL,name TEXT NOT NULL,key BLOB NOT NULL,PRIMARY KEY(action,name,key)) STRICT, WITHOUT ROWID; CREATE INDEX IF NOT EXISTS probe_pin_body ON probe_payload_pins(name,key)");
    for(const auto& [name,spec]:storage::action_payload_probe::specs()){
        sql(db,"CREATE TEMP TRIGGER "+sql_identifier("probe_pin_"+name)+" BEFORE DELETE ON main."+sql_identifier(spec.membership)+" WHEN (SELECT action FROM probe_context)>0 BEGIN INSERT INTO probe_payload_pins SELECT action,'"+name+"',"+expression(spec.keys,"OLD")+" FROM probe_context WHERE action>0 ON CONFLICT(action,name,key) DO NOTHING; END;");
        sql(db,"CREATE TEMP TRIGGER "+sql_identifier("probe_pin_keep_"+name)+" BEFORE DELETE ON main."+sql_identifier(spec.payload)+" WHEN EXISTS(SELECT 1 FROM probe_payload_pins WHERE name='"+name+"' AND key="+expression(spec.keys,"OLD")+") BEGIN SELECT RAISE(ABORT,'Pinned payload is retained'); END;");
    }
}
void collect_payloads(sqlite3* db){
    for(const auto& [name,spec]:storage::action_payload_probe::specs())
        sql(db,"DELETE FROM "+sql_identifier(spec.payload)+" AS p WHERE NOT EXISTS(SELECT 1 FROM "+sql_identifier(spec.membership)+" m WHERE "+storage::action_payload_probe::equality(spec.keys,"m","p")+") AND NOT EXISTS(SELECT 1 FROM probe_payload_pins WHERE name='"+name+"' AND key="+expression(spec.keys,"p")+")");
}
#endif
struct Cell {int type;std::int64_t integer=0;std::string bytes;};
std::vector<Cell> unpack(const std::string& blob){
    std::size_t at=0;
    auto byte=[&](){require(at<blob.size(),"Truncated inverse codec");return static_cast<unsigned char>(blob[at++]);};
    auto varint=[&](){std::uint64_t out=0;for(int shift=0;shift<64;shift+=7){const auto b=byte();require(shift<63||(b&254)==0,"Inverse varint overflow");out|=static_cast<std::uint64_t>(b&127)<<shift;if(!(b&128))return out;}throw std::runtime_error("Invalid inverse varint");};
    require(byte()==1,"Unsupported inverse codec");const auto count=varint();require(count<=128,"Invalid inverse field count");std::vector<Cell> result;
    for(std::uint64_t i=0;i<count;++i){Cell c{byte(),0,{}};if(c.type==SQLITE_INTEGER){std::uint64_t n=0;for(int shift=0;shift<64;shift+=8)n|=static_cast<std::uint64_t>(byte())<<shift;c.integer=static_cast<std::int64_t>(n);}
        else if(c.type==SQLITE_TEXT||c.type==SQLITE_BLOB){const auto n=varint();require(n<=blob.size()-at,"Truncated inverse body");c.bytes=blob.substr(at,static_cast<std::size_t>(n));at+=static_cast<std::size_t>(n);}
        else require(c.type==SQLITE_NULL,"Invalid inverse type");result.push_back(std::move(c));}
    require(at==blob.size(),"Trailing inverse data");return result;
}
void bind(sqlite3_stmt* stmt,int n,const Cell& c){int rc=SQLITE_ERROR;
    if(c.type==SQLITE_INTEGER)rc=sqlite3_bind_int64(stmt,n,c.integer);
    else if(c.type==SQLITE_NULL)rc=sqlite3_bind_null(stmt,n);
    else if(c.type==SQLITE_TEXT)rc=sqlite3_bind_text64(stmt,n,c.bytes.data(),c.bytes.size(),SQLITE_TRANSIENT,SQLITE_UTF8);
    else if(c.type==SQLITE_BLOB)rc=sqlite3_bind_blob64(stmt,n,c.bytes.data(),c.bytes.size(),SQLITE_TRANSIENT);
    require(rc==SQLITE_OK,"Inverse bind failed");
}
using Canonical=std::map<std::string,std::vector<std::string>>;
Canonical canonical(sqlite3* db,const std::vector<Table>& manifest){Canonical result;for(const auto& t:manifest){if(monotonic.contains(t.name))continue;Query q(db,"SELECT "+expression(t.columns,"t")+" FROM "+sql_identifier(t.name)+" t");auto& rows=result[t.name];while(q.next())rows.push_back(q.text(0));std::sort(rows.begin(),rows.end());}return result;}
struct RestorePermission {std::string table,verb,row;};
void authorized(sqlite3_context* ctx,int argc,sqlite3_value** values){
    if(argc!=3){sqlite3_result_error(ctx,"Invalid restoration authorization",-1);return;}
    auto* allowed=static_cast<RestorePermission*>(sqlite3_user_data(ctx));
    auto value=[&](int n){const auto* p=sqlite3_value_blob(values[n]);const auto size=sqlite3_value_bytes(values[n]);return p?std::string(static_cast<const char*>(p),static_cast<std::size_t>(size)):std::string{};};
    sqlite3_result_int(ctx,allowed->table==value(0)&&allowed->verb==value(1)&&allowed->row==value(2));
}

void validate_registry(sqlite3* db,const Table& table,const std::vector<Cell>& row){
    const std::map<std::string,int> domains={{"ledger_operations",0},{"category_operations",1},{"rule_operations",2},{"rule_applications",3},{"account_operations",4},{"dataset_acceptances",5},{"link_exclusion_events",6}};
    const auto domain=domains.find(table.name);if(domain==domains.end())return;
    const auto column=table.name=="link_exclusion_events"?"operation":"id";
    const auto id=std::find(table.columns.begin(),table.columns.end(),column);require(id!=table.columns.end(),"Registry table key missing");const auto index=static_cast<std::size_t>(id-table.columns.begin());require(index<row.size()&&row[index].type==SQLITE_TEXT,"Malformed registry identity");
    Query registry(db,"SELECT domain FROM operation_registry WHERE id=?");bind(registry.q,1,row[index]);require(registry.next()&&registry.number(0)==domain->second,"Restoration registry domain mismatch");require(!registry.next(),"Ambiguous registration");
}
std::string str_literal(std::string value){std::string out="'";for(char c:value){out+=c;if(c=='\'')out+=c;}return out+'\'';}
// Deliberately bounded restoration experiment. Existing trigger guards are
// bypassed only for the exact table/verb/encoded row currently authorized by one
// durable inverse entry. Constraint/FK checks remain enabled. This is NOT the
// production registry-aware capability or action coordinator.
void restore(sqlite3* db,const std::vector<Table>& manifest,std::int64_t action=1){
    struct Trigger {std::string name,table,text;};std::vector<Trigger> triggers;
    {Query q(db,"SELECT name,tbl_name,sql FROM sqlite_schema WHERE type='trigger' ORDER BY name");while(q.next()){if(std::any_of(manifest.begin(),manifest.end(),[&](const auto& t){return t.name==q.text(1);}))triggers.push_back({q.text(0),q.text(1),q.text(2)});}}
    struct Inverse {std::size_t table;std::string key;std::optional<std::string> prior;};std::vector<Inverse> entries;
    {Query q(db,"SELECT table_id,key,prior FROM probe_inverse WHERE action="+std::to_string(action)+" ORDER BY table_id,key");while(q.next())entries.push_back({static_cast<std::size_t>(q.number(0)),q.text(1),sqlite3_column_type(q.q,2)==SQLITE_NULL?std::nullopt:std::optional(q.text(2))});}
    RestorePermission permission;
    require(sqlite3_create_function_v2(db,"probe_authorized",3,SQLITE_UTF8,&permission,authorized,nullptr,nullptr,nullptr)==SQLITE_OK,"Restore capability setup failed");
    try {
        sql(db,"BEGIN IMMEDIATE; PRAGMA defer_foreign_keys=ON;");
        for(const auto& trigger:triggers){
            const auto found=std::find_if(manifest.begin(),manifest.end(),[&](const auto& t){return t.name==trigger.table;});require(found!=manifest.end(),"Unknown trigger table");
            std::smatch match;require(std::regex_search(trigger.text,match,std::regex(" (BEFORE|AFTER) (INSERT|UPDATE|DELETE) ")),"Unknown trigger operation");const auto verb=match[2].str();
            const auto begin=trigger.text.find(" BEGIN");require(begin!=std::string::npos,"Unknown trigger body");const auto when=trigger.text.find(" WHEN ");
            const auto guard="NOT probe_authorized("+str_literal(trigger.table)+","+str_literal(verb)+","+expression(found->columns,verb=="DELETE"?"OLD":"NEW")+")";
            std::string rewritten;
            if(when!=std::string::npos&&when<begin)rewritten=trigger.text.substr(0,when)+" WHEN ("+trigger.text.substr(when+6,begin-when-6)+") AND "+guard+trigger.text.substr(begin);
            else rewritten=trigger.text.substr(0,begin)+" WHEN "+guard+trigger.text.substr(begin);
            sql(db,"DROP TRIGGER "+sql_identifier(trigger.name));sql(db,rewritten);
        }
        for(const auto& e:entries){const auto& t=manifest.at(e.table);const auto key=unpack(e.key);require(key.size()==t.keys.size(),"Inverse key shape mismatch");
            const auto where=key_parameters(t.keys);
            Query current(db,"SELECT "+expression(t.columns,"t")+" FROM "+sql_identifier(t.name)+" t WHERE "+where);for(std::size_t i=0;i<key.size();++i)bind(current.q,static_cast<int>(i+1),key[i]);
            if(current.next()){
                permission={t.name,"DELETE",current.text(0)};require(!current.next(),"Nonunique inverse key");
                Query remove(db,"DELETE FROM "+sql_identifier(t.name)+" AS t WHERE "+where);for(std::size_t i=0;i<key.size();++i)bind(remove.q,static_cast<int>(i+1),key[i]);require(!remove.next(),"Unexpected delete result");permission={};
            }
        }
        for(const auto& e:entries)if(e.prior){const auto& t=manifest.at(e.table);const auto row=unpack(*e.prior);require(row.size()==t.columns.size(),"Inverse row shape mismatch");validate_registry(db,t,row);std::string statement="INSERT INTO "+sql_identifier(t.name)+" VALUES(";for(std::size_t i=0;i<row.size();++i){if(i)statement+=',';statement+='?';}statement+=')';
            permission={t.name,"INSERT",*e.prior};Query insert(db,statement);for(std::size_t i=0;i<row.size();++i)bind(insert.q,static_cast<int>(i+1),row[i]);require(!insert.next(),"Unexpected insert result");permission={};
        }
        {Query errors(db,"PRAGMA foreign_key_check");while(errors.next())std::cerr<<"FK "<<errors.text(0)<<" parent="<<errors.text(2)<<"\n";}
        require(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0,"Restored foreign key mismatch");
        for(const auto& trigger:triggers){sql(db,"DROP TRIGGER "+sql_identifier(trigger.name));sql(db,trigger.text);}
        sql(db,"COMMIT");
    }catch(...){permission={};sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_create_function_v2(db,"probe_authorized",3,SQLITE_UTF8,nullptr,nullptr,nullptr,nullptr,nullptr);throw;}
    permission={};require(sqlite3_create_function_v2(db,"probe_authorized",3,SQLITE_UTF8,nullptr,nullptr,nullptr,nullptr,nullptr)==SQLITE_OK,"Cannot clear restore capability");
}

void coalesce(sqlite3* db,const std::vector<Table>& manifest,std::int64_t action=1){
    for(std::size_t i=0;i<manifest.size();++i){const auto& t=manifest[i];
        struct Image{std::string key;std::optional<std::string> prior;};std::vector<Image> images;
        {Query q(db,"SELECT key,prior FROM probe_inverse WHERE action="+std::to_string(action)+" AND table_id="+std::to_string(i));while(q.next())images.push_back({q.text(0),sqlite3_column_type(q.q,1)==SQLITE_NULL?std::nullopt:std::optional(q.text(1))});}
        Query current(db,"SELECT "+expression(t.columns,"t")+" FROM "+sql_identifier(t.name)+" t WHERE "+key_parameters(t.keys));
        Query erase(db,"DELETE FROM probe_inverse WHERE action="+std::to_string(action)+" AND table_id="+std::to_string(i)+" AND key=?");
        for(const auto& image:images){const auto key=unpack(image.key);require(key.size()==t.keys.size(),"Coalesce key shape mismatch");
            sqlite3_reset(current.q);sqlite3_clear_bindings(current.q);for(std::size_t n=0;n<key.size();++n)bind(current.q,static_cast<int>(n+1),key[n]);
            const bool exists=current.next();const bool unchanged=exists?(image.prior&&current.text(0)==*image.prior):!image.prior;
            if(exists)require(!current.next(),"Coalesce key is not unique");
            if(unchanged){sqlite3_reset(erase.q);sqlite3_clear_bindings(erase.q);require(sqlite3_bind_blob64(erase.q,1,image.key.data(),image.key.size(),SQLITE_TRANSIENT)==SQLITE_OK,"Coalesce bind failed");require(!erase.next(),"Unexpected coalesce result");}
        }
    }
}
void self_check(){
    sqlite3* db=nullptr;require(sqlite3_open(":memory:",&db)==SQLITE_OK,"Cannot open codec fixture");
    try {
        sql(db,"PRAGMA foreign_keys=ON; CREATE TABLE sample(id INTEGER PRIMARY KEY,value BLOB,text TEXT,number INTEGER) STRICT; INSERT INTO sample VALUES(1,x'0061','',-9223372036854775808)");
        const auto manifest=tables(db);install(db,manifest);const auto before=canonical(db,manifest);
        sql(db,"UPDATE probe_context SET action=1; INSERT OR IGNORE INTO sample VALUES(1,x'62','ignored',0)");coalesce(db,manifest);
        require(number(db,"SELECT count(*) FROM probe_inverse")==0,"Ignored insert created an inverse");
        sql(db,"BEGIN; UPDATE sample SET value=x'006162'; INSERT OR REPLACE INTO sample VALUES(1,NULL,'replacement',9223372036854775807); COMMIT");
        require(number(db,"SELECT count(*) FROM probe_inverse")==1,"First image not coalesced");
        require(sqlite3_exec(db,"UPDATE sample SET id=2",nullptr,nullptr,nullptr)==SQLITE_CONSTRAINT,"Key-changing update was not refused");
        sql(db,"UPDATE probe_context SET action=0");restore(db,manifest);require(canonical(db,manifest)==before,"Replacement round trip failed");
        sql(db,"DELETE FROM probe_inverse; UPDATE probe_context SET action=1; BEGIN; UPDATE sample SET text='rollback'; ROLLBACK");
        require(number(db,"SELECT count(*) FROM probe_inverse")==0&&canonical(db,manifest)==before,"Rollback left journal or changed state");
        sql(db,"BEGIN; INSERT INTO sample VALUES(2,x'','',0); DELETE FROM sample WHERE id=2; COMMIT");coalesce(db,manifest);
        require(number(db,"SELECT count(*) FROM probe_inverse")==0,"Net no-op retained payload");
        require(sqlite3_exec(db,"SELECT probe_pack(1.5)",nullptr,nullptr,nullptr)==SQLITE_ERROR,"Floating point codec was accepted");
        bool malformed=false;try{(void)unpack(std::string("\1\1\3\x7f",4));}catch(const std::runtime_error&){malformed=true;}require(malformed,"Truncated codec accepted");
        sql(db,"UPDATE probe_context SET action=0");
        require(number(db,"SELECT count(*) FROM pragma_function_list WHERE name='probe_authorized'")==0,"Restoration capability survived");
        sqlite3_close(db);
    }catch(...){sqlite3_close(db);throw;}
    
    db=nullptr;require(sqlite3_open(":memory:",&db)==SQLITE_OK,"Cannot open registry fixture");
    try {
        sql(db,"CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL) STRICT; CREATE TABLE account_operations(id TEXT PRIMARY KEY,body TEXT NOT NULL) STRICT; CREATE TRIGGER register_account BEFORE INSERT ON account_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,4); END; INSERT INTO account_operations VALUES('old','exact');");
        const auto manifest=tables(db);install(db,manifest);
        sql(db,"UPDATE probe_context SET action=1; DELETE FROM account_operations; UPDATE probe_context SET action=0; UPDATE operation_registry SET domain=0");
        const auto after=canonical(db,manifest);bool refused=false;try{restore(db,manifest);}catch(const std::runtime_error&){refused=true;}require(refused&&canonical(db,manifest)==after,"Wrong-domain restoration did not fail atomically");
        require(number(db,"SELECT count(*) FROM pragma_function_list WHERE name='probe_authorized'")==0,"Failed restoration capability survived");
        sql(db,"DELETE FROM operation_registry");refused=false;try{restore(db,manifest);}catch(const std::runtime_error&){refused=true;}require(refused&&canonical(db,manifest)==after,"Missing registration did not fail atomically");
        sql(db,"INSERT INTO operation_registry VALUES('old',4)");restore(db,manifest);require(number(db,"SELECT count(*) FROM account_operations WHERE id='old' AND body='exact'")==1,"Original same-domain restoration failed");
        require(number(db,"SELECT count(*) FROM operation_registry")==1,"Restoration registered identity twice");sqlite3_close(db);
    }catch(...){sqlite3_close(db);throw;}
    std::cout<<"Codec/capture regressions passed (ignored insert, same-PK replacement, key-change refusal, rollback, net no-op, integer extrema, NULL/empty/binary, malformed and REAL refusal; original/missing/wrong-domain registration).\n";
}

void audit_payloads(const std::filesystem::path& path){
    sqlite3* db=nullptr;require(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK,"Cannot open synthetic payload audit");
    try {
        require(sqlite3_key(db,password,static_cast<int>(std::char_traits<char>::length(password)))==SQLITE_OK,"Cannot key synthetic payload audit");
        std::vector<std::string> bodies;{Query q(db,"SELECT name FROM sqlite_schema WHERE type='table' AND name LIKE 'probe_payload_%' AND name NOT IN('probe_payload_highwater','probe_payload_pins') ORDER BY name");while(q.next())bodies.push_back(q.text(0));}
        require(bodies.size()==10,"Unexpected split body inventory");std::int64_t total=0;
        for(const auto& body:bodies){const auto name=body.substr(std::string("probe_payload_").size());const auto member=name+"_membership";std::map<int,std::string> ordered;
            Query fields(db,"PRAGMA table_info("+sql_identifier(body)+")");while(fields.next())if(fields.number(5))ordered.emplace(static_cast<int>(fields.number(5)),fields.text(1));require(!ordered.empty(),"Payload missing primary key");
            std::string match;for(const auto& [order,key]:ordered){(void)order;if(!match.empty())match+=" AND ";match+="m."+sql_identifier(key)+" IS p."+sql_identifier(key);}
            const auto count=number(db,"SELECT count(*) FROM "+sql_identifier(body)+" p WHERE NOT EXISTS(SELECT 1 FROM "+sql_identifier(member)+" m WHERE "+match+")");total+=count;
            std::cout<<"payload_audit,"<<name<<",bodies="<<number(db,"SELECT count(*) FROM "+sql_identifier(body))<<",members="<<number(db,"SELECT count(*) FROM "+sql_identifier(member))<<",without_live_membership="<<count<<'\n';
        }
        std::cout<<"payload_audit,total_without_live_membership="<<total<<'\n';
        require(total==0,"Unreferenced payloads remain; not a clean structural comparison");
        sqlite3_close(db);
    }catch(...){sqlite3_close(db);throw;}
}
#ifndef SPENTIT_ACTION_PAYLOAD_PROBE
void production_refusal(const std::filesystem::path& path){
    auto bytes=[&](){std::ifstream file(path,std::ios::binary);require(static_cast<bool>(file),"Cannot read synthetic isolation fixture");return std::string(std::istreambuf_iterator<char>(file),{});};
    const auto before=bytes();bool refused=false;
    try{auto store=ImportStore::open(path,password);(void)store;}catch(const storage::ImportError& error){require(error.code()==SQLITE_SCHEMA,"Unexpected isolation refusal");refused=true;}
    require(refused&&bytes()==before,"Production did not refuse prototype unchanged");
    std::cout<<"Production open refused prototype unchanged.\n";
}
#endif
void measure(sqlite3* db,const std::filesystem::path& path,const std::string& phase,const std::vector<Table>& manifest){
    const auto pages=number(db,"PRAGMA page_count"),free=number(db,"PRAGMA freelist_count"),page=number(db,"PRAGMA page_size");
    std::cout<<phase<<",file="<<std::filesystem::file_size(path)<<",live_page_bytes="<<(pages-free)*page<<",free_page_bytes="<<free*page
      <<",inverse_keys="<<number(db,"SELECT count(*) FROM probe_inverse")<<",inverse_key_bytes="<<number(db,"SELECT coalesce(sum(length(key)),0) FROM probe_inverse")
      <<",inverse_image_bytes="<<number(db,"SELECT coalesce(sum(length(prior)),0) FROM probe_inverse")<<'\n';
    Query counts(db,"SELECT table_id,count(*),sum(length(key)),coalesce(sum(length(prior)),0) FROM probe_inverse GROUP BY table_id ORDER BY table_id");
    while(counts.next())std::cout<<"table,"<<phase<<','<<manifest.at(static_cast<std::size_t>(counts.number(0))).name<<",keys="<<counts.number(1)<<",key_bytes="<<counts.number(2)<<",image_bytes="<<counts.number(3)<<'\n';
}
}
