#include <spentit/import_store.hpp>
#include <spentit/storage_document_gateway.hpp>
#include <filesystem>
#include <spentit/diagnostics.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sys/wait.h>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
void check(bool value,const std::source_location& at=std::source_location::current()){if(!value)throw std::runtime_error("Assertion at "+std::to_string(at.line()));}
template<class F> void fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
struct Directory {fs::path path;Directory(){std::string s=(fs::temp_directory_path()/"spentit-approval-XXXXXX").string();check(mkdtemp(s.data()));path=fs::canonical(s);}~Directory(){std::error_code e;fs::remove_all(path,e);}};
constexpr auto key="synthetic-cache-test";
const Currency cad("CAD");
const std::string source="!Type:Bank\nD2026-10-01\nT-12.34\nPSYNTHETIC PRIVATE PAYEE\n^\n";
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& value){std::ofstream out(p,std::ios::binary|std::ios::trunc);out<<value;check(bool(out));}
}
int main(){try{
    if(const auto* setting=std::getenv("SPENTIT_CLOSE_ROWS")) {
        const int count=std::stoi(setting);check(count>0 && count<=20000);
        Directory d;const auto path=d.path/"timing",cache=d.path/"cache";
        {auto s=ImportStore::create(path,key,"Synthetic",cad,"2000-01-01");s.opening(0,"2000-01-01",Money(0,cad));
        std::string input="!Type:Bank\n";
        for(int i=0;i<count;++i)input+="D"+std::to_string(i%28+1)+"/"+std::to_string(i/28%12+1)+"/"+std::to_string(2000+i/336)+"\nT-1.00\nPSynthetic "+std::to_string(i)+"\n^\n";
        s.ingest("input",input,qif::DateOrder::dmy);s.commit("input",0,"accept");}
        diagnostics::Collector::instance().enable();
        auto measure=[&](const char* label,auto action){
            const auto before=diagnostics::Collector::instance().snapshot();const auto start=std::chrono::steady_clock::now();action();
            const auto after=diagnostics::Collector::instance().snapshot();
            std::cout<<label<<" ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
            for(unsigned i=0;i<after.metrics.size();++i){const auto& a=after.metrics[i];const auto& b=before.metrics[i];if(a.calls>b.calls)std::cout<<diagnostics::name(static_cast<diagnostics::Code>(i))<<" calls "<<a.calls-b.calls<<" us "<<a.inclusive-b.inclusive<<'\n';}
        };
        auto closing_process=[&](const char* label,bool edit) {
            int ready[2];check(pipe(ready)==0);std::cout.flush();const auto child=fork();check(child>=0);
            if(child==0){
                ::close(ready[0]);
                {auto s=ImportStore::open_cached(path,key,cache);
                if(edit)s.ingest("edit","!Type:Bank\nD1/1/2099\nT-2.00\nPSynthetic edit\n^\n",qif::DateOrder::dmy);
                check(s.projected_balance()->units()==-count*100-(edit?200:0));
                check(::write(ready[1],"x",1)==1);::close(ready[1]);
                measure(label,[&]{s.certify(cache);});}
                std::cout.flush();_exit(0);
            }
            ::close(ready[1]);char signal=0;check(::read(ready[0],&signal,1)==1);::close(ready[0]);
            const auto start=std::chrono::steady_clock::now();int status=0;
            check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
            std::cout<<label<<" close-to-process-exit-ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
        };
        closing_process("first-unchanged-close",false);
        closing_process("approved-unchanged-close",false);
        closing_process("edited-close",true);
        {auto s=ImportStore::open(path,key);check(s.projected_balance()->units()==-count*100-200);}
        return 0;
    }

    Directory d;const auto path=d.path/"dataset",cache=d.path/"cache";
    {auto s=ImportStore::create(path,key,"Synthetic",cad,"2026-10-01");s.opening(0,"2026-10-01",Money(0,cad));s.ingest("input",source,qif::DateOrder::dmy);}
    int audits=0;app::OperationContext context;context.progress=[&](std::string_view phase){if(phase=="Verifying dataset integrity")++audits;};
    auto open=[&](bool expected){audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check((audits==1)==expected);check(s.ledger(true).size()==1);check(s.ledger(true).front().draft.transaction.effective_total()==Money(-1234,cad));};
    open(true); // no approval
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}) {
        const auto trial=d.path/(phase==ImportPhase::before_commit?"before":"after");
        fs::copy_file(path,trial);const auto approval=trial.string()+"-approval";
        audits=0;auto s=ImportStore::open_cached(trial,key,approval,&context);
        auto row=*s.row({"input",0}).draft;row.memo="interrupted edit";
        s.observer([&](ImportPhase at){if(at==phase)throw std::runtime_error("synthetic interruption");});
        fails([&]{s.save({"input",0},0,row);});s.observer({});
        s.certify(approval,&context);check(audits==2);
        check(s.row({"input",0}).draft->memo==(phase==ImportPhase::after_commit?"interrupted edit":""));
    }
    // A full opening audit survives read-only inspection and avoids a second audit.
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check(audits==1);
    (void)s.row({"input",0});s.certify(cache,&context);check(audits==1);}
    fs::remove_all(cache);
    // A rolled-back command invalidates even when no bytes changed.
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);auto row=*s.row({"input",0}).draft;
    fails([&]{s.save({"input",0},999,row);});s.certify(cache,&context);check(audits==2);}
    fs::remove_all(cache);
    // Successful writes invalidate the in-memory audit.
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);auto row=*s.row({"input",0}).draft;
    row.memo="session edit";s.save({"input",0},0,row);s.certify(cache,&context);check(audits==2);}
    fs::remove_all(cache);
    // Cancellation during fingerprinting cannot publish; retry audits safely.
    {auto s=ImportStore::open_cached(path,key,cache);app::OperationContext cancelled;cancelled.request();
    fails([&]{s.certify(cache,&cancelled);});check(!fs::exists(cache));audits=0;s.certify(cache,&context);check(audits==1);}
    fs::remove_all(cache);
    // An externally damaged file must not inherit the opening approval.
    {auto s=ImportStore::open_cached(path,key,cache);const auto before=bytes(path);auto damaged=before;damaged[8192]^=1;write(path,damaged);
    fails([&]{s.certify(cache);});check(!fs::exists(cache));write(path,before);}
    {auto s=ImportStore::open(path,key);s.certify(cache,&context);fails([&]{ImportStore::open_cached(path,key,cache);});}
    check(fs::directory_iterator(cache)!=fs::directory_iterator{});
    const auto receipt=fs::directory_iterator(cache)->path();const auto approved=bytes(receipt);const auto original=bytes(path);
    check(approved.find("SYNTHETIC")==approved.npos && approved.find(key)==approved.npos && approved.find(path.string())==approved.npos);
    open(false);
    fails([&]{ImportStore::open_cached(path,"wrong",cache);});check(bytes(path)==original);
    write(receipt,"truncated");open(true);write(receipt,approved);
    auto stale=approved;stale[24]=stale[24]=='a'?'b':'a';write(receipt,stale);open(true);write(receipt,approved);
    // Sidecars invalidate approval even if SQLite removes an empty stale journal.
    write(path.string()+"-journal","");open(true);fs::remove(path.string()+"-journal");open(false);
    {auto s=ImportStore::open_cached(path,key,cache);auto row=*s.row({"input",0}).draft;row.memo="Synthetic edit";s.save({"input",0},1,row);}
    open(true); // changed bytes cannot use old approval
    {auto s=ImportStore::open(path,key);s.certify(cache);s.backup_to(d.path/"backup",key);}
    open(false);
    {audits=0;auto s=ImportStore::open_cached(d.path/"backup",key,cache,&context);check(audits==1);check(s.ledger(true).front().draft.memo=="Synthetic edit");}
    const auto updated=bytes(path);auto corrupt=updated;corrupt[8192]^=1;write(path,corrupt);
    fails([&]{ImportStore::open_cached(path,key,cache);});write(path,updated);open(false);
    fs::remove(receipt);
    // Cancellation and process death before audit completion never publish approval.
    {auto s=ImportStore::open(path,key);app::OperationContext cancelled;cancelled.progress=[](std::string_view phase){if(phase=="Verifying dataset integrity")throw app::OperationCancelled();};fails([&]{s.certify(cache,&cancelled);});check(!fs::exists(receipt));s.verify();}
    auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,key);app::OperationContext interrupted;interrupted.progress=[](std::string_view phase){if(phase=="Verifying dataset integrity")_exit(19);};s.certify(cache,&interrupted);_exit(20);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==19);check(!fs::exists(receipt));open(true);
    // The audit checks actual encrypted pages even when the connection has warm caches.
    {auto s=ImportStore::open(path,key);const auto before=bytes(path);auto damaged=before;damaged[8192]^=1;write(path,damaged);fails([&]{s.certify(cache);});check(!fs::exists(receipt));write(path,before);}
    // A process killed after publication leaves a usable approval and releases locks.
    child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,key);s.certify(cache);_exit(21);}check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==21);open(false);
    // Failed approval publication preserves the dataset and permits a safe retry.
    const auto blocked=d.path/"not-a-directory";write(blocked,"occupied");
    {auto s=ImportStore::open(path,key);const auto before=bytes(path);fails([&]{s.certify(blocked);});check(bytes(path)==before);s.certify(cache);}
    open(false);
    // Graceful gateway close generates approval; emergency close deliberately does not.
    const auto gatewayPath=d.path/"gateway",gatewayCache=d.path/"gateway-cache";
    {auto g=document_gateway(gatewayCache);g->create(gatewayPath,false,"","Synthetic",cad,"2026-10-01");check(g->lazy_review());g->close();check(!fs::exists(gatewayCache));g->open(gatewayPath,std::nullopt);g->close_verified(context);}
    {auto g=document_gateway(gatewayCache);audits=0;g->open_controlled(gatewayPath,std::nullopt,context);check(audits==0);g->close();}
    std::cout<<"Verification approval, changed/corrupt input, recovery, cancellation, interruption, publication failure and gateway tests passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
