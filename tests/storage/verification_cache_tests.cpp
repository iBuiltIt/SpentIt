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
    int audits=0;app::OperationContext context;context.progress=[&](std::string_view phase){if(phase=="Checking file")++audits;};
    // Only the first unclean opening audits. Missing caches never trigger audits.
    for(int session=0;session<3;++session){
        audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check(audits==(session==0?1:0));
        auto row=*s.row({"input",0}).draft;row.memo="session "+std::to_string(session);
        s.save({"input",0},session,row);
        app::OperationContext cancelled;cancelled.request();fails([&]{s.certify(cache,&cancelled);});
        check(!fs::exists(cache));s.certify(cache,&context);check(audits==(session==0?1:0));
        s.certify(cache,&context);check(audits==(session==0?1:0) && !fs::exists(cache));
    }
    // An obsolete approval location (even a regular file instead of a directory)
    // cannot change policy or block an edited/uncommitted session after a rebuild.
    write(cache,"SpentIt verification 1\nobsolete-build\ninvalid-hash\n");
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check(audits==0);
     s.ingest("later","!Type:Bank\nD2026-10-02\nT-2.00\nPLater synthetic import\n^\n",qif::DateOrder::dmy);
     s.certify(cache,&context);check(audits==0);}
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check(audits==0);
     check(s.balance()==Money(-1434,cad));check(!s.ledger(true).back().committed);
     s.verify(&context);check(audits==1);s.certify(cache,&context);check(audits==1);}
    check(bytes(cache)=="SpentIt verification 1\nobsolete-build\ninvalid-hash\n");
    fs::remove(cache);
    const auto before=bytes(path);fails([&]{ImportStore::open_cached(path,"wrong",cache);});check(bytes(path)==before);
    {audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check(audits==0);
     s.backup_to(d.path/"backup",key,{},&context);check(audits==3);}
    {audits=0;auto s=ImportStore::open_cached(d.path/"backup",key,cache,&context);check(audits==1);check(s.ledger(true).front().draft.memo=="session 2");s.verify();}
    auto corrupt=bytes(path);corrupt[8192]^=1;write(path,corrupt);
    fails([&]{ImportStore::open_cached(path,key,cache);});write(path,before);
    // Explicit audits still detect damaged encrypted pages with warm caches.
    {auto s=ImportStore::open(path,key);const auto original=bytes(path);auto damaged=original;damaged[8192]^=1;write(path,damaged);fails([&]{s.verify();});write(path,original);}
    const auto gatewayPath=d.path/"gateway",gatewayCache=d.path/"gateway-cache";
    {auto g=document_gateway(gatewayCache);g->create(gatewayPath,false,"","Synthetic",cad,"2026-10-01");g->close();g->open(gatewayPath,std::nullopt);audits=0;g->close_verified(context);check(audits==0 && !fs::exists(gatewayCache));}
    {auto g=document_gateway(gatewayCache);audits=0;g->open_controlled(gatewayPath,std::nullopt,context);check(audits==0);g->check_file(context);check(audits==1);g->close();}
    std::cout<<"Encrypted clean-marker opens/closes without audit/cache, cancellation, backups, authentication and corruption checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
