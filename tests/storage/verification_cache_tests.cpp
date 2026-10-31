#include <spentit/import_store.hpp>
#include <spentit/storage_document_gateway.hpp>
#include <filesystem>
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
    Directory d;const auto path=d.path/"dataset",cache=d.path/"cache";
    {auto s=ImportStore::create(path,key,"Synthetic",cad,"2026-10-01");s.opening(0,"2026-10-01",Money(0,cad));s.ingest("input",source,qif::DateOrder::dmy);}
    int audits=0;app::OperationContext context;context.progress=[&](std::string_view phase){if(phase=="Verifying dataset integrity")++audits;};
    auto open=[&](bool expected){audits=0;auto s=ImportStore::open_cached(path,key,cache,&context);check((audits==1)==expected);check(s.ledger(true).size()==1);check(s.ledger(true).front().draft.transaction.effective_total()==Money(-1234,cad));};
    open(true); // no approval
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
    {auto s=ImportStore::open_cached(path,key,cache);auto row=*s.row({"input",0}).draft;row.memo="Synthetic edit";s.save({"input",0},0,row);}
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
