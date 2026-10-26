#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sys/wait.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-account-password";
const Currency cad("CAD"),jpy("JPY");
const std::string input="D1/1/2026\nT-10\nPPRIVATE-SYNTHETIC-ACCOUNTS\n^\n";
struct Directory {
    fs::path path;
    Directory(){auto s=(fs::temp_directory_path()/"spentit-accounts-XXXXXX").string();auto* p=mkdtemp(s.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location where=std::source_location::current()){if(!value)throw std::runtime_error("Account assertion failed at line "+std::to_string(where.line()));}
template<class F> void rejected(F f){bool failed=false;try{f();}catch(const ImportError&){failed=true;}check(failed);}
ImportStore create(const fs::path& path){return ImportStore::create(path,password,"Checking",cad,"2026-01-01");}
void setup(const fs::path& path){auto s=create(path);s.opening(0,"2026-01-01",Money(10000,cad));s.ingest("checking",input,qif::DateOrder::dmy);s.commit("checking",0,"checking-commit");s.create_account(2,"Japan",jpy,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(500,jpy));s.ingest("japan",input,qif::DateOrder::dmy);}
void no_leaks(const fs::path& dir){for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file()){std::ifstream f(entry.path(),std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(f),{}};check(bytes.find(password)==bytes.npos && bytes.find("PRIVATE-SYNTHETIC-ACCOUNTS")==bytes.npos && !bytes.starts_with("SQLite format 3"));}}
void basics(){
    Directory d;const auto path=d.path/"doc";setup(path);
    {
        auto s=ImportStore::open(path,password);check(s.account().identity==1);check(s.accounts().size()==2);check(s.committed_balance()==Money(9000,cad));
        check(s.batches().size()==1 && s.ledger(true).size()==1);
        rejected([&]{s.row({"japan",0});});rejected([&]{s.commit("japan",0,"wrong-account");});rejected([&]{s.discard("japan",0);});rejected([&]{s.evidence("japan");});
        rejected([&]{s.select_account(999);});check(s.account().identity==1);
        s.ingest("checking-next",input,qif::DateOrder::dmy); // independent pending workspace
        s.select_account(2);check(s.batches().size()==1);check(s.row({"japan",0}).candidates.empty());check(s.projected_balance()==Money(490,jpy));
        rejected([&]{s.ingest("checking",input,qif::DateOrder::dmy);});
        app::OperationContext context;rejected([&]{s.ingest_controlled("checking",input,qif::DateOrder::dmy,context);});
        rejected([&]{s.outcome("checking-commit");});
        std::int64_t japan_revision=0;
        if(s.rules_supported()){
            app::OperationContext context;app::RuleDefinitionImage rule;rule.identity="payee";rule.label="Synthetic rule";rule.expression="PRIVATE";rule.actions.payee_template="Reviewed";
            s.save_rules("rules",{{rule}},0,context);app::RuleApplyRequest request{"japan",0,1,0,false};
            s.select_account(1);rejected([&]{s.apply_pending_rules("apply-japan",request,context);});s.select_account(2);
            const auto applied=s.apply_pending_rules("apply-japan",request,context);japan_revision=applied.batch_after;
            check(s.row({"japan",0}).draft->payee=="Reviewed");s.select_account(1);check(s.row({"checking-next",0}).draft->payee=="PRIVATE-SYNTHETIC-ACCOUNTS");rejected([&]{s.rule_apply_outcome("apply-japan");});s.select_account(2);
        }
        auto receipt=s.commit("japan",japan_revision,"japan-commit");check(receipt.before==Money(500,jpy) && receipt.after==Money(490,jpy));
        check(s.commit("japan",japan_revision,"japan-commit").after==receipt.after);
        auto edit=s.committed_entry({"japan",0}).current.draft;
        edit.transaction=edit.transaction.corrected(Money(-20,jpy),{{"","",Money(-20,jpy)}});
        auto preview=s.preview_committed_change({app::ChangeKind::correct,{"japan",0},edit,{}});
        s.select_account(1);rejected([&]{s.apply_committed_change_with_catalog("correction",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);});
        check(s.committed_balance()==Money(9000,cad));s.select_account(2);
        s.apply_committed_change_with_catalog("correction",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);
        check(s.committed_balance()==Money(480,jpy));check(s.committed_changes().size()==1);
        auto undo=s.preview_committed_change({app::ChangeKind::undo,{"japan",0},{},"correction"});
        s.apply_committed_change_with_catalog("undo",undo.request,undo.expected,undo.preview_sequence,undo.catalog_revision);check(s.committed_balance()==Money(490,jpy));
        if(s.bulk_supported()){
            rejected([&]{s.preview_bulk({app::ChangeKind::remove,{{"checking",0},{"japan",0}},{},{}});});
            auto bulk=s.preview_bulk({app::ChangeKind::remove,{{"japan",0}},{},{}});s.apply_bulk("bulk-japan",bulk);check(s.committed_balance()==Money(500,jpy));
            s.select_account(1);rejected([&]{s.bulk_outcome("bulk-japan");});s.select_account(2);
            auto restore=s.preview_bulk({app::ChangeKind::undo,{{"japan",0}},{},"bulk-japan"});s.apply_bulk("bulk-undo",restore);check(s.committed_balance()==Money(490,jpy));
        }
        s.create_account(2,"Japan",jpy,"2026-01-01"); // retry uses retained creation, not changed opening
        rejected([&]{s.create_account(2,"Changed",jpy,"2026-01-01");});
        s.select_account(1);check(s.committed_changes().empty());
        s.verify();s.backup_to(d.path/"backup",password);no_leaks(d.path);
    }
    for(const auto& file:{path,d.path/"backup"}){auto s=ImportStore::open(file,password);check(s.committed_balance()==Money(9000,cad));s.select_account(2);check(s.committed_balance()==Money(490,jpy));s.verify();}
    // Same currency and same bytes in another account must still be a new movement.
    auto s=ImportStore::open(path,password);s.create_account(3,"Savings",cad,"2026-01-01");s.select_account(3);s.opening(0,"2026-01-01",Money(0,cad));s.ingest("savings",input,qif::DateOrder::dmy);check(s.row({"savings",0}).candidates.empty());s.commit("savings",0,"savings-commit");s.verify();
}
void validation(){
    for(const char* sql:{
        "DELETE FROM account_creations WHERE identity=2",
        "UPDATE account SET name='' WHERE singleton=2",
        "UPDATE account_creations SET opening_date='not-a-date' WHERE identity=2",
        "UPDATE batches SET account_id=999 WHERE id='japan'",
        "DELETE FROM account WHERE singleton=1"}){
        Directory d;const auto path=d.path/"doc";setup(path);
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,"DROP TRIGGER account_creation_retained;DROP TRIGGER account_creation_immutable;DROP TRIGGER batch_account_immutable;DROP TRIGGER source_immutable",nullptr,nullptr,nullptr)==SQLITE_OK);
        check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        rejected([&]{ImportStore::open(path,password);});no_leaks(d.path);
    }
}
void execute(ImportStore& s,bool creation){if(creation)s.create_account(3,"Savings",cad,"2026-01-01");else{s.select_account(2);s.commit("japan",0,"japan-commit");}}
void recovered(const fs::path& path,bool creation){auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(9000,cad));s.select_account(2);const auto result=s.outcome("japan-commit");check(s.committed_balance()==Money(result?490:500,jpy));if(creation){check(!result);check(s.accounts().size()==2 || s.accounts().size()==3);}execute(s,creation);s.verify();}
void faults(bool crash){
    fault_vfs::install();
    for(const bool creation:{true,false}){
        int count=0;{Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);execute(s,creation);count=fault_vfs::seen;fault_vfs::disarm();}check(count>0);
        for(int position=1;position<=count;++position){Directory d;const auto path=d.path/"doc";setup(path);
            if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);execute(s,creation);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));}
            else{{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{execute(s,creation);}catch(const ImportError&){}fault_vfs::disarm();}}
            recovered(path,creation);no_leaks(d.path);
        }
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;const auto path=d.path/"doc";setup(path);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});execute(s,creation);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
            {auto s=ImportStore::open(path,password);s.select_account(2);check((creation?s.accounts().size()==3:s.outcome("japan-commit").has_value())==(phase==ImportPhase::after_commit));}recovered(path,creation);
        }
        std::cout<<(creation?"creation":"commit")<<": "<<count<<" write/sync positions\n";
    }
}
}
int main(int argc,char** argv){try{std::string mode=argc>1?argv[1]:"basics";if(mode=="basics")basics();else if(mode=="validation")validation();else if(mode=="io")faults(false);else if(mode=="crashes")faults(true);else return 2;std::cout<<"Accounts "<<mode<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
