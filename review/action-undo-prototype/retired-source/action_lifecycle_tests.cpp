#include "action_journal_runtime.hpp"
#include "fault_vfs.hpp"
#include <unistd.h>
#include <sys/wait.h>
using namespace action_probe;
using namespace spentit;
namespace {
const Currency cad("CAD");
int cleanup_mode=0;
app::OperationContext* cancellation=nullptr;
void cleanup(sqlite3* db){
    require(!sqlite3_get_autocommit(db),"Cleanup escaped close transaction");
    sql(db,"DELETE FROM probe_inverse; DELETE FROM probe_payload_pins");collect_payloads(db);
    if(cleanup_mode==1)throw std::runtime_error("Synthetic cleanup failure");
    if(cleanup_mode==2){require(cancellation,"Missing cancellation control");cancellation->request();}
}
std::vector<Table> attach(sqlite3* db){storage::action_payload_probe::load_specs(db);const auto manifest=tables(db);install(db,manifest);install_payload_pins(db);storage::action_payload_probe::close_cleanup=cleanup;return manifest;}
Canonical entire_state(sqlite3* db){
    std::vector<Table> manifest;Query names(db,"SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name");
    while(names.next()){Table t{names.text(0),{}, {}};Query columns(db,"PRAGMA table_info("+sql_identifier(t.name)+")");while(columns.next())t.columns.push_back(columns.text(1));manifest.push_back(std::move(t));}
    Canonical out;for(const auto& t:manifest){Query q(db,"SELECT "+expression(t.columns,"t")+" FROM "+sql_identifier(t.name)+" t");auto& rows=out[t.name];while(q.next())rows.push_back(q.text(0));std::sort(rows.begin(),rows.end());}return out;
}
void seed(const std::filesystem::path& path){
    storage::action_payload_probe::close_cleanup=nullptr;
    {auto s=storage::ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");s.ingest("first",source(20),qif::DateOrder::dmy);s.ingest("second",source(20,20),qif::DateOrder::dmy);s.verify();s.certify({});}
    {auto s=storage::ImportStore::open(path,password);auto* db=connection;const auto manifest=attach(db);sql(db,"UPDATE probe_context SET action=1");const auto preview=s.review_import_undo("second");s.undo_import("remove","second",preview.token);sql(db,"UPDATE probe_context SET action=0; BEGIN IMMEDIATE");coalesce(db,manifest);sql(db,"COMMIT");require(number(db,"SELECT count(*) FROM probe_inverse")>0&&number(db,"SELECT count(*) FROM probe_payload_pins")>0,"Missing seeded journal/pins");s.verify();}
    storage::action_payload_probe::close_cleanup=nullptr;
}
void assert_retained(sqlite3* db,const Canonical& before){require(sqlite3_get_autocommit(db)!=0,"Failed close left transaction open");require(entire_state(db)==before,"Failed close changed semantic/journal/payload state");require(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0,"Failed close FK mismatch");}
void snapshot_query_equivalence(const std::filesystem::path& path){
    auto s=storage::ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");s.ingest("first",source(20),qif::DateOrder::dmy);s.accept_all("accept");
    const imports::Occurrence id{"first",0};auto draft=s.committed_entry(id).current.draft;draft.payee="Corrected";
    app::ChangeRequest request{app::ChangeKind::correct,id,draft,{}};const auto preview=s.preview_committed_change(request);s.apply_committed_change("edit",request,preview.expected,preview.preview_sequence);
    auto* db=connection;const auto manifest=attach(db);(void)manifest;
    require(number(db,"SELECT count(*) FROM entry_revisions WHERE draft_revision IS NOT NULL AND date IS NULL")>0&&number(db,"SELECT count(*) FROM entry_revisions WHERE draft_revision IS NULL AND date IS NOT NULL")>0,"Snapshot fixture lacks fallback/explicit cases");
    const std::string original="SELECT active,date,payee,memo,total FROM entry_values WHERE batch=?1 AND ordinal=?2 AND revision=?3";
    const auto rewritten=storage::action_payload_probe::rewrite(original);
    const std::string audit="SELECT 0 AS tag,0 AS position,active AS a,date AS b,payee AS c,memo AS d,total AS e,NULL AS f FROM entry_values WHERE batch=?1 AND ordinal=?2 AND revision=?3 UNION ALL SELECT 1,0,payee_owner,memo_owner,manual_split_amounts,remainder_allocation,NULL,NULL FROM entry_fields WHERE batch=?1 AND ordinal=?2 AND revision=?3 UNION ALL SELECT 2,position,category,memo,units,NULL,NULL,NULL FROM entry_allocation_values WHERE batch=?1 AND ordinal=?2 AND revision=?3 UNION ALL SELECT 3,position,identity,source_ordinal,category_id,catalog_revision,category_owner,memo_owner FROM entry_allocation_meta WHERE batch=?1 AND ordinal=?2 AND revision=?3 ORDER BY tag,position";
    auto result=[&](const std::string& query,const std::string& batch,std::int64_t ordinal,std::int64_t revision){Query q(db,query);require(sqlite3_bind_text(q.q,1,batch.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_int64(q.q,2,ordinal)==SQLITE_OK&&sqlite3_bind_int64(q.q,3,revision)==SQLITE_OK,"Cannot bind snapshot comparison");std::vector<std::string> rows;while(q.next()){std::string row;for(int n=0;n<sqlite3_column_count(q.q);++n){row+=std::to_string(sqlite3_column_type(q.q,n))+":";const auto value=q.text(n);row+=std::to_string(value.size())+":"+value;}rows.push_back(std::move(row));}return rows;};
    auto compare=[&](const std::string& batch,std::int64_t ordinal,std::int64_t revision){require(result(original,batch,ordinal,revision)==result(rewritten,batch,ordinal,revision),"Snapshot query changed typed values");require(result(audit,batch,ordinal,revision)==result(storage::action_payload_probe::rewrite(audit),batch,ordinal,revision),"Audit UNION query changed typed values");};
    Query keys(db,"SELECT batch,ordinal,revision FROM entry_revisions");while(keys.next()){require(!result(original,keys.text(0),keys.number(1),keys.number(2)).empty(),"Existing snapshot comparison is empty");compare(keys.text(0),keys.number(1),keys.number(2));}compare("missing",0,0);compare("first",1000,0);compare("first",0,1000);
    for(const auto& query:{original,rewritten,audit,storage::action_payload_probe::rewrite(audit)}){std::cout<<((query==original||query==audit)?"Original":"Scalar")<<" snapshot query plan:\n";Query plan(db,"EXPLAIN QUERY PLAN "+query);while(plan.next())std::cout<<plan.text(3)<<'\n';}
    s.verify();s.certify({});storage::action_payload_probe::close_cleanup=nullptr;
    sqlite3* fixture=nullptr;require(sqlite3_open(":memory:",&fixture)==SQLITE_OK,"Cannot open query edge fixture");db=fixture;
    try{
        sql(db,"CREATE TABLE entry_revisions(batch TEXT,ordinal INTEGER,revision INTEGER,draft_revision INTEGER,PRIMARY KEY(batch,ordinal,revision)); CREATE TABLE entry_allocations(batch TEXT,ordinal INTEGER,revision INTEGER,position INTEGER,category TEXT,memo TEXT,units INTEGER); CREATE TABLE allocations(batch TEXT,ordinal INTEGER,revision INTEGER,position INTEGER,category TEXT,memo TEXT,units INTEGER); CREATE TABLE entry_allocation_meta(batch TEXT,ordinal INTEGER,revision INTEGER,position INTEGER,category_id TEXT,catalog_revision INTEGER,PRIMARY KEY(batch,ordinal,revision,position)); CREATE TABLE category_versions(id TEXT,catalog_revision INTEGER,revision INTEGER,name TEXT); CREATE VIEW entry_allocation_values AS SELECT batch,ordinal,revision,position,category,memo,units FROM entry_allocations UNION ALL SELECT a.batch,a.ordinal,e.revision,a.position,CASE WHEN m.category_id IS NULL THEN a.category ELSE (SELECT c.name FROM category_versions c WHERE c.id=m.category_id AND c.catalog_revision<=m.catalog_revision ORDER BY c.revision DESC LIMIT 1) END,a.memo,a.units FROM entry_revisions e JOIN allocations a ON a.batch=e.batch AND a.ordinal=e.ordinal AND a.revision=e.draft_revision LEFT JOIN entry_allocation_meta m ON m.batch=e.batch AND m.ordinal=e.ordinal AND m.revision=e.revision AND m.position=a.position WHERE e.draft_revision IS NOT NULL;");
        for(int n=0;n<6;++n)sql(db,"INSERT INTO entry_revisions VALUES('fixture',"+std::to_string(n)+",0,0); INSERT INTO allocations VALUES('fixture',"+std::to_string(n)+",0,0,'Original','',-1)");
        sql(db,"INSERT INTO entry_allocation_meta VALUES('fixture',1,0,0,NULL,1),('fixture',2,0,0,'bound',2),('fixture',3,0,0,'missing',1),('fixture',4,0,0,'null-name',1),('fixture',5,0,0,'future-only',1); INSERT INTO category_versions VALUES('bound',1,1,'Old'),('bound',2,2,'Current'),('bound',3,3,'Future'),('null-name',1,1,NULL),('future-only',2,1,'Future'); INSERT INTO entry_allocations VALUES('fixture',0,0,1,'Explicit','',1);");
        const std::string allocations="SELECT position,category,memo,units FROM entry_allocation_values WHERE batch=?1 AND ordinal=?2 AND revision=?3 ORDER BY position";
        for(int n=0;n<6;++n){const auto old=result(allocations,"fixture",n,0);require(!old.empty()&&old==result(storage::action_payload_probe::rewrite(allocations),"fixture",n,0),"Allocation NULL/category/multiplicity semantics changed");}
        require(result(allocations,"absent",0,0)==result(storage::action_payload_probe::rewrite(allocations),"absent",0,0),"Absent allocation semantics changed");
    }catch(...){sqlite3_close(fixture);throw;}sqlite3_close(fixture);
    std::cout<<"Original/scalar snapshot and audit UNION typed equality passed for fallback, corrected, NULL draft reference and absent keys.\n";
    std::cout<<"Allocation typed equality passed for missing/NULL metadata, retained category versions, missing/future category version, NULL name and UNION ALL multiplicity.\n";
}
void close_failures(const std::filesystem::path& path){
    seed(path);auto s=storage::ImportStore::open(path,password);auto* db=connection;attach(db);const auto before=entire_state(db);const auto generation=number(db,"SELECT generation FROM document_session");
    bool refused=false;cleanup_mode=1;try{s.certify({});}catch(const std::exception&){refused=true;}cleanup_mode=0;require(refused,"Injected cleanup did not fail");assert_retained(db,before);s.verify();
    app::OperationContext context;cancellation=&context;cleanup_mode=2;refused=false;try{s.certify({},&context);}catch(const std::exception&){refused=true;}cleanup_mode=0;cancellation=nullptr;require(refused,"Close cancellation did not stop commit");assert_retained(db,before);s.verify();
    s.observer([](storage::ImportPhase phase){if(phase==storage::ImportPhase::before_commit)throw std::runtime_error("Synthetic close commit refusal");});refused=false;try{s.certify({});}catch(const std::exception&){refused=true;}s.observer({});require(refused,"Close boundary failure missing");assert_retained(db,before);s.verify();
    fault_vfs::arm(1,SQLITE_FULL);refused=false;try{s.certify({});}catch(const std::exception&){refused=true;}const bool fired=fault_vfs::fired;fault_vfs::disarm();require(fired&&refused,"Close disk-full fault did not fire");assert_retained(db,before);s.verify();
    s.observer([](storage::ImportPhase phase){if(phase==storage::ImportPhase::after_commit)throw std::runtime_error("Synthetic lost close acknowledgement");});s.certify({});s.observer({});
    require(number(db,"SELECT generation FROM document_session")==generation+1&&number(db,"SELECT clean FROM document_session")==1,"Successful close marker missing");
    require(number(db,"SELECT count(*) FROM probe_inverse")==0&&number(db,"SELECT count(*) FROM probe_payload_pins")==0,"Successful close retained inverse/pins");
    require(number(db,"SELECT count(*) FROM probe_payload_sources")==1,"Successful close retained removed body");
    storage::action_payload_probe::close_cleanup=nullptr;
    std::cout<<"Close cleanup failure, cancellation after cleanup, precommit refusal, disk-full and lost acknowledgement passed.\n";
}
void crash_close(const std::filesystem::path& path,storage::ImportPhase phase){
    seed(path);const auto child=fork();require(child>=0,"Cannot fork close test");
    if(child==0){try{storage::action_payload_probe::mutable_specs().clear();auto s=storage::ImportStore::open(path,password);attach(connection);s.observer([&](storage::ImportPhase event){if(event==phase)_exit(73);});s.certify({});_exit(74);}catch(...){_exit(75);}}
    int status=0;require(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==73,"Close child did not exit at boundary");
    storage::action_payload_probe::mutable_specs().clear();auto s=storage::ImportStore::open(path,password);auto* db=connection;attach(db);s.verify();require(s.balance()==Money(-2000,cad),"Crash changed current money");
    const bool retained=phase==storage::ImportPhase::before_commit;require((number(db,"SELECT count(*) FROM probe_inverse")>0)==retained&&(number(db,"SELECT count(*) FROM probe_payload_pins")>0)==retained,"Crash expiry outcome mismatch");require(number(db,"SELECT count(*) FROM probe_payload_sources")== (retained?2:1),"Crash body outcome mismatch");
    s.certify({});storage::action_payload_probe::close_cleanup=nullptr;
    std::cout<<(retained?"Precommit":"Postcommit")<<" process exit recovered coherent journal/pins/payloads and money.\n";
}
}
int main(){std::filesystem::path root;try{
    fault_vfs::install();require(sqlite3_auto_extension(reinterpret_cast<void(*)()>(extension))==SQLITE_OK,"Cannot observe test connection");
    auto name=(std::filesystem::temp_directory_path()/"spentit-action-lifecycle-XXXXXX").string();const auto* result=mkdtemp(name.data());require(result,"Cannot create lifecycle fixture");root=std::filesystem::canonical(result);
    snapshot_query_equivalence(root/"query.spentit");close_failures(root/"failures.spentit");crash_close(root/"before.spentit",storage::ImportPhase::before_commit);crash_close(root/"after.spentit",storage::ImportPhase::after_commit);
    std::filesystem::remove_all(root);std::cout<<"Prototype close lifecycle checks passed (bounded tests, not a full fault sweep).\n";return 0;
}catch(const std::exception& error){storage::action_payload_probe::close_cleanup=nullptr;std::cerr<<error.what()<<'\n';if(!root.empty())std::filesystem::remove_all(root);return 1;}}
