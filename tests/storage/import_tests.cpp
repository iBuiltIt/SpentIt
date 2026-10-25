#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <chrono>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-import-password";
constexpr auto marker="PRIVATE-SYNTHETIC-IMPORT-9ba74";
const Currency cad("CAD");
const std::string source="!Type:Bank\nD10/08/2026\nT-29.31\nPPRIVATE-SYNTHETIC-IMPORT-9ba74\nEfee\n$-0.14\nEtransport\n$-29.17\n^\nD11/08/2026\nT50\nPIncome\n^\n";
struct Directory {
    fs::path path;
    Directory(){ std::string s=(fs::temp_directory_path()/"spentit-import-XXXXXX").string(); auto* p=mkdtemp(s.data()); if(!p) throw std::runtime_error("Temporary directory failed"); path=fs::canonical(p); }
    ~Directory(){std::error_code e; fs::remove_all(path,e);}
};
void check(bool b){if(!b) throw std::runtime_error("Import assertion failed");}
template<class F> void fails(F f){bool threw=false;try{f();}catch(const ImportError&){threw=true;}catch(const FinancialError&){threw=true;}catch(const std::invalid_argument&){threw=true;}check(threw);}
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void no_leaks(const fs::path& dir){for(const auto& p:fs::directory_iterator(dir))if(p.is_regular_file()){auto s=bytes(p);check(s.find(password)==s.npos && s.find(marker)==s.npos && !s.starts_with("SQLite format 3"));}}
ImportStore create(const fs::path& p){return ImportStore::create(p,password,"Synthetic account",cad,"2026-08-01");}
void prepare(const fs::path& p){auto s=create(p);s.opening(0,"2026-08-01",Money(10000,cad));s.ingest("first",source,qif::DateOrder::dmy);}
void basics(){
    Directory d;const auto p=d.path/"doc";prepare(p);
    {
        auto s=ImportStore::open(p,password);check(s.committed_balance().units()==10000 && s.projected_balance()->units()==12069);
        check(s.ingest("first",source,qif::DateOrder::dmy).id=="first");check(s.ingest("retry",source,qif::DateOrder::dmy).id=="first");
        fails([&]{s.ingest("first",source+"\n",qif::DateOrder::dmy);});
        auto row=s.row({"first",0});auto edit=*row.draft;
        edit.transaction=edit.transaction.corrected(Money(-3000,cad),{{"Fees","fee",Money(-14,cad)},{"Transport","trip",Money(-2986,cad)}});
        s.save({"first",0},0,edit);check(s.projected_balance()->units()==12000);
        fails([&]{s.save({"first",0},0,edit);});
        check(s.evidence("first")==source && s.row({"first",0}).draft->transaction.original_imported_amount()->units()==-2931);
    }
    {
        auto s=ImportStore::open(p,password);check(s.projected_balance()->units()==12000);
        auto receipt=s.commit("first",1,"commit-first");check(receipt.included==2 && receipt.excluded==0 && receipt.before.units()==10000 && receipt.change.units()==2000 && receipt.after.units()==12000);
        check(s.commit("first",1,"commit-first").after.units()==12000);fails([&]{s.commit("first",0,"commit-first");});
        fails([&]{s.opening(1,"2026-08-01",Money(0,cad));});fails([&]{s.discard("first",2);});
        check(s.ledger(false).size()==2 && s.account().frozen);s.verify();no_leaks(d.path);
    }
    auto s=ImportStore::open(p,password);check(s.outcome("commit-first")->after.units()==12000);fails([&]{ImportStore::open(p,password);});
}
void review(){
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-08-01",Money(0,cad));
    const std::string purchase="D1/1/2026\nT-10\nPRepeated\n^\n";
    s.ingest("one",purchase,qif::DateOrder::dmy);s.commit("one",0,"one-op");
    s.ingest("two",purchase+purchase,qif::DateOrder::dmy);
    check(!s.projected_balance());check(s.row({"two",0}).decision.disposition==imports::Disposition::unresolved);
    fails([&]{s.commit("two",0,"two-op");});
    s.decide({"two",0},0,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});
    fails([&]{s.decide({"two",1},1,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});});
    s.decide({"two",1},1,{imports::Disposition::include,{}});check(s.projected_balance()->units()==-2000);
    auto receipt=s.commit("two",2,"two-op");check(receipt.included==1 && receipt.excluded==1);
    s.ingest("three",purchase+purchase,qif::DateOrder::dmy);
    check(s.row({"three",0}).decision.duplicate_of==std::optional(imports::Occurrence{"one",0}));
    check(s.row({"three",1}).decision.duplicate_of==std::optional(imports::Occurrence{"two",1}));
    check(s.projected_balance()->units()==-2000);auto zero=s.commit("three",0,"three-op");check(zero.included==0 && zero.excluded==2 && zero.change.units()==0);
    s.ingest("four",purchase+purchase,qif::DateOrder::dmy);s.decide({"four",1},0,{imports::Disposition::include,{}});
    check(s.commit("four",1,"four-op").after.units()==-3000);s.verify();
    // Changed payee/reference is uncertain, not permission to silently duplicate.
    const auto variant=std::string("D1/1/2026\nT-10\nPReformatted payee\nNnew-reference\n^\n");
    s.ingest("variant",variant,qif::DateOrder::dmy);auto possible=s.row({"variant",0});
    check(possible.decision.disposition==imports::Disposition::unresolved && possible.candidates.size()==3);
    check(imports::candidate_basis(possible.source,s.row({"one",0}).source)==imports::MatchBasis::same_date_amount);
    s.decide({"variant",0},0,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});
    check(s.commit("variant",1,"variant-op").change.units()==0);s.verify();
}
void errors(){
    Directory d;auto s=create(d.path/"doc");
    const auto b=s.ingest("bad","!Type:Bank\nD1/1/2026\nT1\nL[Other]\n^\n",qif::DateOrder::dmy);
    check(b.blocked && !s.projected_balance());fails([&]{s.ledger(true);});fails([&]{s.commit("bad",0,"bad-op");});
    check(s.committed_balance().units()==0);s.discard("bad",0);s.discard("bad",0);check(!s.evidence("bad").empty());
    s.ingest("good",source,qif::DateOrder::dmy);auto edit=*s.row({"good",0}).draft;edit.date="2026-02-30";
    fails([&]{s.save({"good",0},0,edit);});check(s.batches().back().revision==0);
    edit=*s.row({"good",0}).draft;edit.transaction=Transaction::manual(Money(-2931,cad),{{"","",Money(-2931,cad)}});
    fails([&]{s.save({"good",0},0,edit);});
    edit=*s.row({"good",0}).draft;edit.transaction=edit.transaction.corrected(Money(-2931,cad),{{"[Other]","",Money(-2931,cad)}});
    fails([&]{s.save({"good",0},0,edit);});
    fails([&]{s.ingest("another","D1/1/2026\nT1\n^",qif::DateOrder::dmy);});s.verify();
    fails([&]{ImportStore::open(d.path/"missing",password);});check(!fs::exists(d.path/"missing"));
    fails([&]{create(d.path/"doc");});
    no_leaks(d.path);
}
void backup(){
    Directory d;prepare(d.path/"doc");
    {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"backup",password);fails([&]{s.backup_to(d.path/"backup",password);});}
    {auto s=ImportStore::open(d.path/"backup",password);check(s.projected_balance()->units()==12069);s.backup_to(d.path/"restored",password);}
    {auto s=ImportStore::open(d.path/"restored",password);check(s.commit("first",0,"restored-op").after.units()==12069);s.backup_to(d.path/"committed",password);}
    {auto s=ImportStore::open(d.path/"committed",password);check(s.commit("first",0,"restored-op").after.units()==12069);s.verify();}
    const auto before=bytes(d.path/"committed");fails([&]{ImportStore::open(d.path/"committed","wrong");});check(bytes(d.path/"committed")==before);no_leaks(d.path);
}
const std::string repair_source="D31/02/2026\nT-10\nPRepair date\n^\nD1/3/2026\nT-20\nPSplit repair\nSFood\n$-19\n^\n";
imports::DraftRow repaired_row(const qif::Record& source,std::string date,std::int64_t amount) {
    return {std::move(date),source.payee,source.memo,
        Transaction::corrected_import(*source.original_amount,Money(amount,cad),{{"Food","explicit repair",Money(amount,cad)}})};
}
void repairs(){
    Directory d;
    {auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(10000,cad));
        s.ingest("prior","D1/2/2026\nT-10\nPPrior\n^\n",qif::DateOrder::dmy);s.commit("prior",0,"prior-op");
        s.ingest("repair",repair_source,qif::DateOrder::dmy);
        const auto original=s.row({"repair",0});check(!original.draft && original.source.repairable);
        auto edit=repaired_row(original.source,"2026-02-28",-1000);
        auto invalid=edit;invalid.date="2026-02-31";fails([&]{s.save({"repair",0},0,invalid);});
        invalid=edit;invalid.transaction=Transaction::imported(Money(-999,cad),{{"","",Money(-999,cad)}});
        fails([&]{s.save({"repair",0},0,invalid);});check(!s.row({"repair",0}).draft);
        s.save({"repair",0},0,edit);check(s.batches().back().blocked && !s.projected_balance());
        check(s.row({"repair",0}).source.diagnostic.has_value());
        check(s.row({"repair",0}).candidates.size()==1);
        check(s.row({"repair",0}).source.date.empty());
        check(imports::candidate_basis(s.row({"repair",0}).source,s.row({"prior",0}).source)==imports::MatchBasis::same_amount_unknown_date);
        s.save({"repair",1},1,repaired_row(s.row({"repair",1}).source,"2026-03-01",-2100));
        check(!s.batches().back().blocked && !s.projected_balance());
        fails([&]{s.commit("repair",2,"repair-op");});
        s.decide({"repair",0},2,{imports::Disposition::duplicate,imports::Occurrence{"prior",0}});
        s.decide({"repair",1},3,{imports::Disposition::include,{}});
        check(s.projected_balance()->units()==6900);s.backup_to(d.path/"pending",password);s.verify();}
    {auto s=ImportStore::open(d.path/"doc",password);check(s.evidence("repair")==repair_source);
        check(s.row({"repair",1}).draft->transaction.original_imported_amount()->units()==-2000);
        fails([&]{s.save({"repair",1},1,repaired_row(s.row({"repair",1}).source,"2026-03-01",-2000));});
        auto receipt=s.commit("repair",4,"repair-op");check(receipt.included==1 && receipt.excluded==1 && receipt.after.units()==6900);
        s.backup_to(d.path/"committed",password);}
    for(const auto* name:{"pending","committed"}){auto s=ImportStore::open(d.path/name,password);
        check(s.commit("repair",4,"repair-op").after.units()==6900);s.verify();}
    {auto s=ImportStore::open(d.path/"doc",password);
        s.ingest("again",repair_source,qif::DateOrder::dmy);
        check(s.batches().back().blocked); // no auto-repair or silent exclusion
        s.discard("again",0);s.verify();}
    // A repairable first diagnostic must not hide later unsupported content.
    {auto s=create(d.path/"unsafe");s.ingest("bad","D31/02/2026\nT-10\nL[Other]\n^\n",qif::DateOrder::dmy);
        check(!s.row({"bad",0}).source.repairable);
        auto edit=repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000);
        fails([&]{s.save({"bad",0},0,edit);});check(s.batches()[0].blocked);s.verify();}
    {auto s=create(d.path/"header");s.ingest("bad","D31/02/2026\nT-10\n^\n!Type:Invst\n",qif::DateOrder::dmy);
        s.save({"bad",0},0,repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000));
        s.decide({"bad",0},1,{imports::Disposition::include,{}});check(s.batches()[0].blocked);s.verify();}
    {auto s=create(d.path/"unknown-date");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("bad","D31/02/2026\nT-10\n^\n",qif::DateOrder::dmy);
        s.save({"bad",0},0,repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000));
        s.decide({"bad",0},1,{imports::Disposition::include,{}});s.commit("bad",2,"op");
        s.ingest("new","D1/4/2026\nT-10\n^\n",qif::DateOrder::dmy);
        check(s.row({"new",0}).decision.disposition==imports::Disposition::unresolved);
        check(s.row({"new",0}).candidates.size()==1);s.verify();}
    // Schema 1 remains readable without an implicit upgrade and cannot store repairs.
    {auto s=create(d.path/"legacy");s.ingest("bad","D31/02/2026\nT-10\n^\n",qif::DateOrder::dmy);}
    {sqlite3* db=nullptr;check(sqlite3_open((d.path/"legacy").c_str(),&db)==SQLITE_OK);
        check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,"PRAGMA user_version=1",nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);}
    const auto legacy=bytes(d.path/"legacy");
    {auto s=ImportStore::open(d.path/"legacy",password);check(!s.row({"bad",0}).source.repairable);
        fails([&]{s.save({"bad",0},0,repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000));});}
    check(bytes(d.path/"legacy")==legacy);
    no_leaks(d.path);
}
void operation(ImportStore& s,int kind){
    if(kind==0)s.ingest("new",source,qif::DateOrder::dmy);
    if(kind==1){auto r=*s.row({"first",0}).draft;r.payee="edited";s.save({"first",0},0,r);}
    if(kind==2)s.discard("first",0);
    if(kind==4)s.save({"first",0},0,repaired_row(s.row({"first",0}).source,"2026-02-28",-1000));
    if(kind==3)s.commit("first",0,"op");
}
void setup(const fs::path& path,int kind){if(kind==0){auto s=create(path);}else if(kind==4){auto s=create(path);s.ingest("first","D31/02/2026\nT-10\n^\n",qif::DateOrder::dmy);}else prepare(path);}
void recovered(const fs::path& path,int kind){
    auto s=ImportStore::open(path,password);s.verify();
    if(kind==0){check(s.batches().size()<=1);s.ingest("new",source,qif::DateOrder::dmy);check(s.batches().size()==1);}
    if(kind==1){const auto b=s.batches().at(0);check(b.revision==0 || b.revision==1);check(s.row({"first",0}).draft->payee==(b.revision==0?marker:"edited"));}
    if(kind==2){const auto b=s.batches().at(0);check(b.state==app::BatchState::pending || b.state==app::BatchState::discarded);s.discard("first",0);check(s.committed_balance().units()==10000);}
    if(kind==4){const auto b=s.batches()[0];check(b.revision==0 || b.revision==1);
        check(s.row({"first",0}).draft.has_value()==(b.revision==1));check(b.blocked==(b.revision==0));
        if(b.revision==0)operation(s,4);check(!s.projected_balance());s.verify();}
    if(kind==3){check(s.committed_balance().units()==10000 || s.committed_balance().units()==12069);check(s.commit("first",0,"op").after.units()==12069);check(s.ledger(false).size()==2);}
}
void io(bool crash){
    fault_vfs::install();
    for(int kind=0;kind<5;++kind){
        int count=0;
        {Directory d;setup(d.path/"doc",kind);auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);operation(s,kind);count=fault_vfs::seen;fault_vfs::disarm();}
        check(count>0);
        for(int n=1;n<=count;++n){
            Directory d;setup(d.path/"doc",kind);
            if(crash){
                const auto child=fork();check(child>=0);
                if(child==0){auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,SQLITE_IOERR_WRITE,true,true);operation(s,kind);_exit(0);}
                int status=0;check(waitpid(child,&status,0)==child);check(WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));
            }else{
                auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
                try{operation(s,kind);}catch(const ImportError&){}fault_vfs::disarm();
            }
            no_leaks(d.path);recovered(d.path/"doc",kind);
        }
        std::cout<<"operation "<<kind<<": "<<count<<" fault positions\n";
    }
    if(crash)for(int kind:{3,4})for(auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
        Directory d;setup(d.path/"doc",kind);const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});operation(s,kind);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        {auto s=ImportStore::open(d.path/"doc",password);check((kind==3?s.outcome("op").has_value():s.row({"first",0}).draft.has_value())==(phase==ImportPhase::after_commit));}
        recovered(d.path/"doc",kind);
    }
}
void backup_failures(bool crash=false){
    fault_vfs::install();int count=0;
    {Directory d;prepare(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);s.backup_to(d.path/"baseline",password);count=fault_vfs::seen;fault_vfs::disarm();}
    for(int n=1;n<=count;++n){
        Directory d;prepare(d.path/"doc");std::string prior;
        {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"prior",password);prior=bytes(d.path/"prior");}
        if(crash){
            const auto child=fork();check(child>=0);
            if(child==0){auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,SQLITE_IOERR_WRITE,true,true);s.backup_to(d.path/"candidate",password);_exit(0);}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71);
        }else{
            auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
            fails([&]{s.backup_to(d.path/"candidate",password);});check(fault_vfs::fired);fault_vfs::disarm();s.verify();
        }
        check(bytes(d.path/"prior")==prior);auto restored=ImportStore::open(d.path/"prior",password);check(restored.projected_balance()->units()==12069);no_leaks(d.path);
        auto original=ImportStore::open(d.path/"doc",password);check(original.evidence("first")==source);
        original.backup_to(d.path/"retry",password);
        auto retry=ImportStore::open(d.path/"retry",password);check(retry.projected_balance()->units()==12069);
    }
    std::cout<<"backup: "<<count<<" fault positions\n";
}
void backup_publication(){
    for(bool committed:{false,true})for(bool crash:{false,true})
    for(auto phase:{BackupPhase::before_publish,BackupPhase::before_publish_sync,
                   BackupPhase::before_cleanup,BackupPhase::before_cleanup_sync,BackupPhase::complete}){
        Directory d;prepare(d.path/"doc");
        {auto s=ImportStore::open(d.path/"doc",password);
            if(committed)s.commit("first",0,"op");
            s.backup_to(d.path/"prior",password);}
        const auto prior=bytes(d.path/"prior"),original=bytes(d.path/"doc");
        bool reached=false;
        const auto inject=[&](BackupPhase p){if(p==phase){reached=true;if(crash)_exit(73);throw ImportError(SQLITE_IOERR,"Injected publication boundary failure");}};
        if(crash){
            const auto child=fork();check(child>=0);
            if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"candidate",password,inject);_exit(0);}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
        }else{
            auto s=ImportStore::open(d.path/"doc",password);
            fails([&]{s.backup_to(d.path/"candidate",password,inject);});check(reached);
        }
        check(bytes(d.path/"prior")==prior && bytes(d.path/"doc")==original);
        const bool published=phase!=BackupPhase::before_publish;
        const bool cleaned=phase==BackupPhase::before_cleanup_sync || phase==BackupPhase::complete;
        check(fs::exists(d.path/"candidate")==published);
        check(fs::exists(d.path/"candidate.partial")==!cleaned);
        // An interrupted link/unlink publication leaves two names for one inode.
        // Normal opens must still reject it; recover a separate copy, never
        // weaken hard-link validation or silently remove retained evidence.
        if(published && !cleaned)fails([&]{ImportStore::open(d.path/"candidate",password);});
        fs::copy_file(d.path/(published?"candidate":"candidate.partial"),d.path/"recovered");
        for(const auto* name:{"doc","prior","recovered"}){
            auto s=ImportStore::open(d.path/name,password);s.verify();check(s.evidence("first")==source);
            check(s.committed_balance().units()==(committed?12069:10000));
            check(s.projected_balance()->units()==12069);
            check(s.outcome("op").has_value()==committed);
        }
        {auto s=ImportStore::open(d.path/"recovered",password);check(s.commit("first",0,"op").after.units()==12069);check(s.ledger(false).size()==2);}
        {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"retry",password);}
        {auto s=ImportStore::open(d.path/"retry",password);check(s.projected_balance()->units()==12069);}
        no_leaks(d.path);
    }
    std::cout<<"backup publication: 20 boundary cases\n";
}
void opening_policy(){
    Directory d;auto s=create(d.path/"doc");s.ingest("one",source,qif::DateOrder::dmy);
    fails([&]{s.commit("one",0,"op");});s.opening(0,"2026-08-01",Money(0,cad));
    check(s.batches().at(0).revision==1);fails([&]{s.commit("one",0,"op");});
    check(s.commit("one",1,"op").after.units()==2069);s.verify();
}
void validation(){
    Directory d;prepare(d.path/"doc");
    // Deliberately corrupt authenticated synthetic state; no user data involved.
    auto alter=[&](const char* sql){sqlite3* db=nullptr;check(sqlite3_open((d.path/"doc").c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);};
    alter("PRAGMA user_version=99");fails([&]{ImportStore::open(d.path/"doc",password);});alter("PRAGMA user_version=1");
    alter("DROP TRIGGER allocation_immutable; UPDATE allocations SET units=units+1 WHERE batch='first' AND ordinal=0 AND position=0");
    fails([&]{ImportStore::open(d.path/"doc",password);});
}
void benchmark(std::size_t n){
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-08-01",Money(0,cad));std::string input;
    for(std::size_t i=0;i<n;++i) input+="D1/1/2026\nT0.01\nN"+std::to_string(i)+"\nPSynthetic\n^\n";
    const auto start=std::chrono::steady_clock::now();s.ingest("large",input,qif::DateOrder::dmy);
    const auto staged=std::chrono::steady_clock::now();std::cout<<"Ingest finished ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(staged-start).count()<<std::endl;check(s.commit("large",0,"large-op").after.units()==static_cast<std::int64_t>(n));
    const auto committed=std::chrono::steady_clock::now();std::cout<<"Commit finished ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(committed-staged).count()<<std::endl;check(s.ledger(false).size()==n);
    const auto queried=std::chrono::steady_clock::now();
    std::cout<<n<<" rows ingest/commit/ledger ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(staged-start).count()<<'/'
        <<std::chrono::duration_cast<std::chrono::milliseconds>(committed-staged).count()<<'/'<<std::chrono::duration_cast<std::chrono::milliseconds>(queried-committed).count()<<'\n';
}
}
int main(int argc,char** argv){try{
    const std::string mode=argc>1?argv[1]:"";
    if(mode=="basics")basics();else if(mode=="review")review();else if(mode=="repairs")repairs();else if(mode=="errors")errors();else if(mode=="backup")backup();
    else if(mode=="io")io(false);else if(mode=="crashes")io(true);else if(mode=="backup_failures")backup_failures();else if(mode=="backup_crashes")backup_failures(true);else if(mode=="backup_publication")backup_publication();else if(mode=="validation")validation();else if(mode=="opening")opening_policy();else if(mode=="benchmark")benchmark(argc>2?std::stoul(argv[2]):100000);else return 2;
    std::cout<<"Import "<<mode<<" passed\n";
}catch(const ImportError& e){std::cerr<<"Storage code "<<e.code()<<": "<<e.what()<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
