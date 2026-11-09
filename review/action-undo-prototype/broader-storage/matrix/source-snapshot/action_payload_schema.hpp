#pragma once
// Development-only schema adapter for the representative action-journal probe.
// Include this before macro-redirecting sqlite3_exec/sqlite3_prepare_v2 in the
// isolated ImportStore translation unit. Production builds never include it.
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace spentit::storage::action_payload_probe {
struct Spec {
    std::string name,payload,membership;
    std::vector<std::string> keys,columns,membership_columns;
};
inline std::map<std::string,Spec>& mutable_specs(){static std::map<std::string,Spec> values;return values;}
inline const std::map<std::string,Spec>& specs(){return mutable_specs();}
inline const std::set<std::string>& split_tables(){
    static const std::set<std::string> names={"sources","records","versions","allocations","source_headers","source_occurrences","entry_revisions","entry_allocations","draft_allocation_meta","entry_allocation_meta"};
    return names;
}
inline std::string trim(std::string value){
    const auto first=value.find_first_not_of(" \r\n\t");if(first==std::string::npos)return {};
    const auto last=value.find_last_not_of(" \r\n\t");return value.substr(first,last-first+1);
}
inline std::string quote(std::string_view value){
    std::string out="\"";for(char c:value){out+=c;if(c=='\"')out+=c;}return out+'\"';
}
inline std::string joined(const std::vector<std::string>& values,std::string_view prefix={}){
    std::string out;for(const auto& value:values){if(!out.empty())out+=',';out+=prefix;out+=quote(value);}return out;
}
inline std::vector<std::string> clauses(std::string_view text){
    std::vector<std::string> out;std::size_t start=0;int depth=0;char quoted=0;
    for(std::size_t i=0;i<text.size();++i){const char c=text[i];if(quoted){if(c==quoted){if(i+1<text.size()&&text[i+1]==quoted)++i;else quoted=0;}continue;}
        if(c=='\''||c=='\"'){quoted=c;continue;}if(c=='(')++depth;else if(c==')')--depth;
        else if(c==','&&depth==0){out.push_back(trim(std::string(text.substr(start,i-start))));start=i+1;}
        if(depth<0)throw std::runtime_error("Invalid probe schema parentheses");
    }
    if(depth||quoted)throw std::runtime_error("Unterminated probe schema clause");
    out.push_back(trim(std::string(text.substr(start))));return out;
}
inline std::size_t closing(std::string_view sql,std::size_t open){
    int depth=0;char quoted=0;for(std::size_t i=open;i<sql.size();++i){const char c=sql[i];if(quoted){if(c==quoted){if(i+1<sql.size()&&sql[i+1]==quoted)++i;else quoted=0;}continue;}
        if(c=='\''||c=='\"'){quoted=c;continue;}if(c=='(')++depth;else if(c==')'&&--depth==0)return i;
    }throw std::runtime_error("Unterminated probe table declaration");
}
inline std::vector<std::string> list_in(std::string_view clause){
    const auto open=clause.find('(');if(open==std::string_view::npos)throw std::runtime_error("Missing probe key list");const auto close=closing(clause,open);
    auto result=clauses(clause.substr(open+1,close-open-1));for(const auto& name:result)if(!std::regex_match(name,std::regex("[a-z_]+")))throw std::runtime_error("Unsupported probe key identifier");return result;
}
inline std::string equality(const std::vector<std::string>& columns,std::string_view a,std::string_view b){
    std::string out;for(const auto& name:columns){if(!out.empty())out+=" AND ";out+=a;out+='.';out+=quote(name);out+=" IS ";out+=b;out+='.';out+=quote(name);}return out;
}
inline void replace_all(std::string& value,const std::string& old,const std::string& replacement){
    std::size_t pos=0;while((pos=value.find(old,pos))!=std::string::npos){value.replace(pos,old.size(),replacement);pos+=replacement.size();}
}
inline std::string adapt_table(const std::string& name,const std::string& body,const std::string& suffix){
    Spec spec{name,"probe_payload_"+name,name+"_membership",{}, {}, {}};
    std::vector<std::string> body_parts,foreign_keys;
    std::map<std::string,std::string> types;
    std::set<std::string> not_null;
    for(const auto& part:clauses(body)){
        if(part.starts_with("FOREIGN KEY")){foreign_keys.push_back(part);continue;}
        if(part.starts_with("PRIMARY KEY")){spec.keys=list_in(part);body_parts.push_back(part);continue;}
        if(part.starts_with("CHECK")||part.starts_with("UNIQUE")){body_parts.push_back(part);continue;}
        std::smatch column;
        if(!std::regex_search(part,column,std::regex("^([a-z_]+) (INTEGER|TEXT|BLOB)\\b")))throw std::runtime_error("Unsupported probe column declaration");
        const auto field=column[1].str();spec.columns.push_back(field);types.emplace(field,column[2].str());
        if(part.find("NOT NULL")!=std::string::npos)not_null.insert(field);
        if(part.find("PRIMARY KEY")!=std::string::npos)spec.keys.push_back(field);
        const auto ref=part.find(" REFERENCES ");
        if(ref!=std::string::npos){foreign_keys.push_back("FOREIGN KEY("+field+")"+part.substr(ref));body_parts.push_back(part.substr(0,ref));}
        else body_parts.push_back(part);
    }
    if(spec.keys.empty())throw std::runtime_error("Payload table lacks stable keys");
    spec.membership_columns=spec.keys;
    for(const auto& fk:foreign_keys)for(const auto& column:list_in(fk))if(std::find(spec.membership_columns.begin(),spec.membership_columns.end(),column)==spec.membership_columns.end())spec.membership_columns.push_back(column);
    std::string sql="CREATE TABLE "+quote(spec.payload)+"(";
    for(const auto& part:body_parts){if(sql.back()!='(')sql+=',';sql+=part;}sql+=')'+suffix+';';
    sql+="CREATE TABLE "+quote(spec.membership)+"(";
    for(const auto& column:spec.membership_columns){if(sql.back()!='(')sql+=',';sql+=quote(column)+' '+types.at(column);if(not_null.contains(column))sql+=" NOT NULL";}
    if(name=="sources")sql+=",probe_order INTEGER NOT NULL UNIQUE CHECK(probe_order>0)";
    sql+=",PRIMARY KEY("+joined(spec.keys)+")";
    for(const auto& fk:foreign_keys)sql+=','+fk;
    sql+=",FOREIGN KEY("+joined(spec.keys)+") REFERENCES "+quote(spec.payload)+"("+joined(spec.keys)+")) STRICT, WITHOUT ROWID;";
    // The body owns value constraints; this exact-match guard makes duplicate
    // FK columns in membership authoritative only when equal to that body.
    sql+="CREATE TRIGGER "+quote("probe_member_body_"+name)+" BEFORE INSERT ON "+quote(spec.membership)+" WHEN NOT EXISTS(SELECT 1 FROM "+quote(spec.payload)+" p WHERE "+equality(spec.membership_columns,"p","NEW")+") BEGIN SELECT RAISE(ABORT,'Payload membership does not match its body'); END;";
    sql+="CREATE TRIGGER "+quote("probe_member_update_"+name)+" BEFORE UPDATE ON "+quote(spec.membership)+" BEGIN SELECT RAISE(ABORT,'Immutable payload membership'); END;";
    sql+="CREATE TRIGGER "+quote("probe_body_update_"+name)+" BEFORE UPDATE ON "+quote(spec.payload)+" BEGIN SELECT RAISE(ABORT,'Immutable action payload'); END;";
    sql+="CREATE TRIGGER "+quote("probe_body_keep_"+name)+" BEFORE DELETE ON "+quote(spec.payload)+" WHEN EXISTS(SELECT 1 FROM "+quote(spec.membership)+" m WHERE "+equality(spec.keys,"m","OLD")+") BEGIN SELECT RAISE(ABORT,'Active payload is retained'); END;";
    sql+="CREATE VIEW "+quote(name)+" AS SELECT "+joined(spec.columns,"p.");if(name=="sources")sql+=",m.probe_order AS rowid";
    sql+=" FROM "+quote(spec.membership)+" m JOIN "+quote(spec.payload)+" p ON "+equality(spec.keys,"m","p")+';';
    sql+="CREATE TRIGGER "+quote("probe_insert_"+name)+" INSTEAD OF INSERT ON "+quote(name)+" BEGIN ";
    sql+="SELECT CASE WHEN EXISTS(SELECT 1 FROM "+quote(spec.payload)+" p WHERE "+equality(spec.keys,"p","NEW")+") AND NOT EXISTS(SELECT 1 FROM "+quote(spec.payload)+" p WHERE "+equality(spec.columns,"p","NEW")+") THEN RAISE(ABORT,'Payload identity cannot be reused') END;";
    sql+="INSERT INTO "+quote(spec.payload)+"("+joined(spec.columns)+") VALUES("+joined(spec.columns,"NEW.")+");";
    if(name=="sources")sql+="UPDATE probe_payload_highwater SET value=value+1 WHERE name='source_order' AND value<9223372036854775807; SELECT CASE WHEN changes()!=1 THEN RAISE(ABORT,'Source order capacity exhausted') END;";
    sql+="INSERT INTO "+quote(spec.membership)+"("+joined(spec.membership_columns);if(name=="sources")sql+=",probe_order";
    sql+=") VALUES("+joined(spec.membership_columns,"NEW.");if(name=="sources")sql+=",(SELECT value FROM probe_payload_highwater WHERE name='source_order')";sql+="); END;";
    sql+="CREATE TRIGGER "+quote("probe_delete_"+name)+" INSTEAD OF DELETE ON "+quote(name)+" BEGIN DELETE FROM "+quote(spec.membership)+" WHERE ";
    for(std::size_t i=0;i<spec.keys.size();++i){if(i)sql+=" AND ";sql+=quote(spec.keys[i])+" IS OLD."+quote(spec.keys[i]);}sql+="; END;";
    sql+="CREATE TRIGGER "+quote("probe_update_"+name)+" INSTEAD OF UPDATE ON "+quote(name)+" BEGIN SELECT RAISE(ABORT,'Immutable payload view'); END;";
    if(name=="records")sql+="CREATE TRIGGER probe_record_sequence AFTER INSERT ON "+quote(spec.payload)+" BEGIN UPDATE probe_payload_highwater SET value=max(value,NEW.sequence) WHERE name='record_sequence'; END;";
    mutable_specs().insert_or_assign(name,spec);return sql;
}
inline std::string rewrite(std::string_view original){
    // The isolated adapter accepts only its distinct on-disk application ID.
    // This maps that validated prototype identity to the legacy C++ gate's
    // expected result; a production-format file maps to -1 and is refused.
    if(trim(std::string(original))=="PRAGMA application_id")
        return "SELECT CASE application_id WHEN 1397770578 THEN 1397770577 ELSE -1 END FROM pragma_application_id";
    std::string sql(original);
    replace_all(sql,"SELECT coalesce(max(sequence),0) FROM records","SELECT max(coalesce(max(sequence),0),(SELECT value FROM probe_payload_highwater WHERE name='record_sequence')) FROM records");
    replace_all(sql,"SELECT coalesce(max(sequence),0)+1 FROM records","SELECT max(coalesce(max(sequence),0),(SELECT value FROM probe_payload_highwater WHERE name='record_sequence'))+1 FROM records");
    replace_all(sql,"PRAGMA application_id=1397770577","PRAGMA application_id=1397770578");
    if(original.find("CREATE TABLE ")==std::string_view::npos &&
       original.find("CREATE TRIGGER ")==std::string_view::npos &&
       original.find("CREATE INDEX ")==std::string_view::npos &&
       original.find("CREATE UNIQUE INDEX ")==std::string_view::npos &&
       original.find("REFERENCES ")==std::string_view::npos &&
       original.find("INTO sources ")==std::string_view::npos &&
       original.find("sqlite_master WHERE type='table' AND name")==std::string_view::npos)return sql;
    // Match only known original declarations; generated payload names cannot
    // match this pass, making a wrapper call deterministic and non-recursive.
    for(const auto& name:split_tables()){
        const std::string needle="CREATE TABLE "+name+'(';std::size_t start=0;
        while((start=sql.find(needle,start))!=std::string::npos){
            const auto open=start+needle.size()-1,close=closing(sql,open),end=sql.find(';',close);
            if(end==std::string::npos)throw std::runtime_error("Unterminated payload table DDL");
            const auto transformed=adapt_table(name,sql.substr(open+1,close-open-1),sql.substr(close+1,end-close-1));
            sql.replace(start,end-start+1,transformed);start+=transformed.size();
        }
    }
    // Original incoming/outgoing live relationships always target membership.
    // Generated body FKs use distinct quoted payload names and remain untouched.
    for(const auto& name:split_tables()){
        replace_all(sql,"REFERENCES "+name+'(',"REFERENCES "+quote(name+"_membership")+'(');
        // Existing forward validations and generated side effects run once when
        // the immutable body is created. Keep guards operate on live membership.
        for(const auto& event:{"INSERT","UPDATE","DELETE"}){
            const std::regex pattern("((?:BEFORE|AFTER) "+std::string(event)+" ON )"+name+"\\b");
            sql=std::regex_replace(sql,pattern,"$1"+quote(std::string(event)=="DELETE"?name+"_membership":"probe_payload_"+name));
        }
        sql=std::regex_replace(sql,std::regex("(CREATE (?:UNIQUE )?INDEX [a-z_]+ ON )"+name+"\\("),"$1"+quote("probe_payload_"+name)+"(");
    }
    // The source view exposes stable order without changing the body's shape.
    sql=std::regex_replace(sql,std::regex("(INSERT(?: OR [A-Z]+)? INTO sources) VALUES"),"$1(id,bytes) VALUES");
    // Only the isolated repository's format discovery sees original-name views
    // as expected semantic structures. Normal production open still rejects it.
    replace_all(sql,"sqlite_master WHERE type='table' AND name","sqlite_master WHERE type IN('table','view') AND name");
    if(original.find("CREATE TABLE sources(")!=std::string_view::npos)
        sql="CREATE TABLE probe_payload_highwater(name TEXT PRIMARY KEY,value INTEGER NOT NULL CHECK(value>=0)) STRICT, WITHOUT ROWID; INSERT INTO probe_payload_highwater VALUES('source_order',0),('record_sequence',0);"+sql;
    return sql;
}

// Reload only after the caller has authenticated/opened the isolated prototype.
// The map is process metadata, not an ownership root or a durable journal.
inline void load_specs(sqlite3* db){
    struct Read {
        sqlite3_stmt* row=nullptr;
        Read(sqlite3* connection,const std::string& sql){if(::sqlite3_prepare_v2(connection,sql.c_str(),-1,&row,nullptr)!=SQLITE_OK)throw std::runtime_error("Cannot read payload schema metadata");}
        ~Read(){::sqlite3_finalize(row);}
        bool next(){const int rc=::sqlite3_step(row);if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)throw std::runtime_error("Cannot step payload schema metadata");return rc==SQLITE_ROW;}
        std::string text(int n)const{const auto* value=::sqlite3_column_text(row,n);return value?std::string(reinterpret_cast<const char*>(value),static_cast<std::size_t>(::sqlite3_column_bytes(row,n))):std::string{};}
        int integer(int n)const{return ::sqlite3_column_int(row,n);}
    };
    {Read id(db,"PRAGMA application_id");if(!id.next()||id.integer(0)!=1397770578)throw std::runtime_error("Payload schema metadata requires a prototype document");}
    std::map<std::string,Spec> recovered;
    for(const auto& name:split_tables()){
        Spec spec{name,"probe_payload_"+name,name+"_membership",{}, {}, {}};
        std::map<int,std::string> body_keys,member_keys;
        std::map<std::string,std::string> body_types;
        {Read columns(db,"PRAGMA table_info("+quote(spec.payload)+")");while(columns.next()){
            const auto column=columns.text(1);spec.columns.push_back(column);body_types.emplace(column,columns.text(2));
            if(columns.integer(5)>0)body_keys.emplace(columns.integer(5),column);
        }}
        if(spec.columns.empty()||body_keys.empty())throw std::runtime_error("Missing immutable payload structure");
        for(const auto& [order,key]:body_keys){(void)order;spec.keys.push_back(key);}
        bool source_order=false;
        {Read columns(db,"PRAGMA table_info("+quote(spec.membership)+")");while(columns.next()){
            const auto column=columns.text(1);
            if(column=="probe_order"){
                if(name!="sources"||columns.text(2)!="INTEGER"||columns.integer(3)!=1||columns.integer(5)!=0)throw std::runtime_error("Invalid source order structure");
                source_order=true;continue;
            }
            const auto type=body_types.find(column);if(type==body_types.end()||type->second!=columns.text(2))throw std::runtime_error("Payload membership column mismatch");
            spec.membership_columns.push_back(column);if(columns.integer(5)>0)member_keys.emplace(columns.integer(5),column);
        }}
        if(member_keys!=body_keys||spec.membership_columns.empty()||(name=="sources")!=source_order)throw std::runtime_error("Payload membership identity mismatch");
        // A body must exist even when nullable secondary FK columns are absent.
        std::map<int,std::map<int,std::pair<std::string,std::string>>> body_links;
        {Read refs(db,"PRAGMA foreign_key_list("+quote(spec.membership)+")");while(refs.next())if(refs.text(2)==spec.payload)
            body_links[refs.integer(0)].emplace(refs.integer(1),std::make_pair(refs.text(3),refs.text(4)));
        }
        bool linked=false;
        for(const auto& [id,parts]:body_links){(void)id;if(parts.size()!=spec.keys.size())continue;bool exact=true;
            for(std::size_t i=0;i<spec.keys.size();++i){const auto part=parts.find(static_cast<int>(i));if(part==parts.end()||part->second.first!=spec.keys[i]||part->second.second!=spec.keys[i])exact=false;}
            linked=linked||exact;
        }
        if(!linked)throw std::runtime_error("Payload membership has no exact body reference");
        std::vector<std::string> view_columns;
        {Read columns(db,"PRAGMA table_info("+quote(name)+")");while(columns.next())if(!(name=="sources"&&columns.text(1)=="rowid"))view_columns.push_back(columns.text(1));}
        if(view_columns!=spec.columns)throw std::runtime_error("Payload semantic view mismatch");
        recovered.emplace(name,std::move(spec));
    }
    mutable_specs()=std::move(recovered);
}
using CloseCleanup=void(*)(sqlite3*);
inline thread_local CloseCleanup close_cleanup=nullptr;
inline constexpr std::string_view close_publication_sql="DELETE FROM review_stack; DELETE FROM session_history; UPDATE document_session SET generation=generation+1,clean=1,history_cut=max(history_cut,(SELECT coalesce(max(seq),0) FROM sqlite_sequence WHERE name='history_order')),financial_cut=max(financial_cut,(SELECT coalesce(max(seq),0) FROM sqlite_sequence WHERE name='ledger_operations')) WHERE singleton=1";
inline void collect_unreferenced_payloads(sqlite3* db){
    if(::sqlite3_get_autocommit(db)!=0)throw std::runtime_error("Payload collection requires the close transaction");
    load_specs(db);
    sqlite3_stmt* journals=nullptr;
    if(::sqlite3_prepare_v2(db,"SELECT count(*) FROM sqlite_schema WHERE type='table' AND name IN('probe_inverse','probe_payload_pins','probe_source_pins')",-1,&journals,nullptr)!=SQLITE_OK)throw std::runtime_error("Cannot check prototype journal ownership");
    const int result=::sqlite3_step(journals);const bool journaled=result==SQLITE_ROW&&::sqlite3_column_int(journals,0)!=0;::sqlite3_finalize(journals);
    if(result!=SQLITE_ROW)throw std::runtime_error("Cannot read prototype journal ownership");
    if(journaled)throw std::runtime_error("Journaled prototype close requires its cleanup hook");
    for(const auto& [name,spec]:specs()){
        (void)name;
        const auto statement="DELETE FROM "+quote(spec.payload)+" AS p WHERE NOT EXISTS(SELECT 1 FROM "+quote(spec.membership)+" m WHERE "+equality(spec.keys,"m","p")+")";
        if(::sqlite3_exec(db,statement.c_str(),nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error("Unreferenced payload collection failed");
    }
}
inline int exec(sqlite3* db,const char* text,int(*callback)(void*,int,char**,char**),void* arg,char** error){
    const auto original=text?std::string_view(text):std::string_view{};
    if(original.starts_with("DELETE FROM review_stack; DELETE FROM session_history; UPDATE document_session")){
        if(original!=close_publication_sql)throw std::runtime_error("Unrecognized prototype close publication statement");
        if(::sqlite3_get_autocommit(db)!=0)throw std::runtime_error("Prototype close cleanup requires an active transaction");
        if(close_cleanup)close_cleanup(db);else collect_unreferenced_payloads(db);
    }
    const auto sql=rewrite(original);return ::sqlite3_exec(db,sql.c_str(),callback,arg,error);
}
inline int prepare(sqlite3* db,const char* text,int bytes,sqlite3_stmt** statement,const char** tail){
    if(tail)throw std::runtime_error("Payload probe does not support multi-statement prepared tails");
    const auto size=bytes<0?std::char_traits<char>::length(text):static_cast<std::size_t>(bytes);
    const auto sql=rewrite(std::string_view(text,size));return ::sqlite3_prepare_v2(db,sql.c_str(),-1,statement,nullptr);
}
} // namespace spentit::storage::action_payload_probe
