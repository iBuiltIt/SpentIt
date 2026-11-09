#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <iostream>
#include <source_location>
#include <sys/wait.h>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto key="synthetic-cleanup-key";
const Currency cad("CAD");
void check(bool value,const std::source_location at=std::source_location::current()) {if(!value)throw std::runtime_error("Cleanup assertion at "+std::to_string(at.line()));}
struct Directory {fs::path path;Directory(){auto s=(fs::temp_directory_path()/"spentit-cleanup-XXXXXX").string();auto p=mkdtemp(s.data());if(!p)throw std::runtime_error("temp directory");path=fs::canonical(p);}~Directory(){std::error_code e;fs::remove_all(path,e);}};
const std::string prior="!Type:Bank\nD2026-01-01\nT-10.00\nPPrevious\n^\n";
const std::string mapped="!Account\nNChecking\nTBank\n^\n!Type:Bank\nD2026-02-01\nT-2.00\nPNew\nL[New account]\n^\n!Account\nNNew account\nTBank\n^\n!Type:Bank\nD2026-02-01\nT2.00\nPNew\nL[Checking]\n^\n";
std::vector<app::ImportAccountMapping> mappings={{0,1,{}},{1,2,app::AccountCreation{2,"New account",cad,"2026-02-01"},false}};
ImportStore create(const fs::path& path){auto s=ImportStore::create(path,key,"Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.ingest("prior",prior,qif::DateOrder::dmy);return s;}
void preserved(ImportStore& s){s.select_account(1);check(s.balance().units()==-1000);check(s.batches().size()==1 && s.batches()[0].id=="prior");check(s.accounts().size()==1);s.verify();}
void capture(ImportStore& s,std::string_view id="interrupted"){app::OperationContext c;s.prepare_import_group(id,mapped,qif::DateOrder::dmy,mappings,c);}
void run(){
 Directory d;fault_vfs::install();
 // Explicit source-scoped removal is idempotent, retaining prior pending work.
 {auto s=create(d.path/"basic");capture(s);check(s.accounts().size()==2);check(s.cleanup_incomplete_imports("interrupted"));check(!s.cleanup_incomplete_imports("interrupted"));preserved(s);
  app::OperationContext a,b;s.prepare_import_group("again",mapped,qif::DateOrder::dmy,mappings,a);s.resume_import_group("again",b);check(!s.cleanup_incomplete_imports());s.verify();}
 {auto s=ImportStore::open(d.path/"basic",key);check(s.accounts().size()==2);check(!s.startup_import_cleanup());s.select_account(1);check(s.balance().units()==-1200);s.select_account(2);check(s.balance().units()==200);s.verify();}
 // Accepted linked postings and a later pending import are independent owners.
 {auto s=ImportStore::open(d.path/"basic",key);s.accept_all("accept-all");s.select_account(1);s.ingest("later","!Type:Bank\nD2026-03-01\nT-1\nPLater\n^\n",qif::DateOrder::dmy);const auto journal=s.transfer_journal().size();app::OperationContext c;s.prepare_import_group("owned",mapped,qif::DateOrder::dmy,{{0,1,{}},{1,2,{}}},c);check(s.cleanup_incomplete_imports("owned"));check(s.transfer_journal().size()==journal);check(s.acceptance_review().transactions==1);check(s.balance().units()==-1300);s.select_account(2);check(s.balance().units()==200);s.verify();}
 // A preparing import prevents clean close; dirty reopen performs scoped cleanup.
 {auto s=create(d.path/"cached");capture(s);bool refused=false;try{s.certify(d.path/"cache");}catch(const ImportError&){refused=true;}check(refused && s.accounts().size()==2);}
 {auto s=ImportStore::open_cached(d.path/"cached",key,d.path/"cache");check(s.startup_import_cleanup());preserved(s);}
 // Cancellation during row population rolls back all generated relationships.
 for(int row:{1,2}){auto s=create(d.path/("cancel"+std::to_string(row)));capture(s);app::OperationContext c;int rows=0;s.observer([&](ImportPhase p){if(p==ImportPhase::materialize_row && ++rows==row)c.request();});bool cancelled=false;try{s.resume_import_group("interrupted",c);}catch(const app::OperationCancelled&){cancelled=true;}s.observer({});check(cancelled);s.cleanup_incomplete_imports("interrupted");preserved(s);}
 // Single-account Cancel removes capture; completed malformed imports survive.
 {auto s=create(d.path/"single");app::OperationContext c;c.progress=[&](std::string_view phase){if(phase=="Import saved")c.request();};bool cancelled=false;try{s.ingest_controlled("cancelled",prior,qif::DateOrder::dmy,c);}catch(const app::OperationCancelled&){cancelled=true;}check(cancelled);preserved(s);app::OperationContext done;s.ingest_controlled("malformed","!Type:Bank\nD2026-02-02\nTbad\nPBroken\n^\n",qif::DateOrder::dmy,done);}
 {auto s=ImportStore::open(d.path/"single",key);check(s.batches().size()==2 && !s.startup_import_cleanup());s.verify();}
 // Process exit on each side of preparation and cleanup commits.
 for(bool preparing:{false,true})for(bool after:{false,true}){
  auto path=d.path/(std::string(preparing?"prepare":"cleanup")+(after?"after":"before"));{auto s=create(path);capture(s);}
  const auto child=fork();check(child>=0);
  if(child==0){
   // Open would clean capture: create a fresh capture within the live connection.
   auto s=ImportStore::open(path,key);capture(s,"replacement");s.observer([&](ImportPhase phase){if(phase==(after?ImportPhase::after_commit:ImportPhase::before_commit))_exit(72);});
   if(preparing){app::OperationContext c;s.resume_import_group("replacement",c);}else s.cleanup_incomplete_imports("replacement");_exit(1);
  }
  int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
  auto s=ImportStore::open(path,key);if(preparing && after){check(s.accounts().size()==2);s.select_account(1);check(s.balance().units()==-1200);s.verify();}else preserved(s);
 }
 // I/O failure before commit preserves the complete capture; retry removes it.
 {auto s=create(d.path/"failure");capture(s);s.observer([](ImportPhase p){if(p==ImportPhase::before_commit)throw std::runtime_error("injected I/O failure");});bool failed=false;try{s.cleanup_incomplete_imports("interrupted");}catch(const std::exception&){failed=true;}s.observer({});check(failed && s.accounts().size()==2);s.verify();check(s.cleanup_incomplete_imports("interrupted"));preserved(s);}
 // Every measured write/sync boundary receives a real VFS disk-full or I/O failure.
 int writes=0;{auto s=create(d.path/"count");capture(s);fault_vfs::arm(-1);s.cleanup_incomplete_imports("interrupted");writes=fault_vfs::seen;fault_vfs::disarm();}check(writes>0);
 for(int n=1;n<=writes;++n){const auto path=d.path/("io"+std::to_string(n));{auto s=create(path);capture(s);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.cleanup_incomplete_imports("interrupted");}catch(const std::exception&){}const bool fired=fault_vfs::fired;fault_vfs::disarm();check(fired);}auto s=ImportStore::open(path,key);preserved(s);}
 std::cout<<"Cleanup write/sync fault positions: "<<writes<<'\n';
 // Lost cleanup acknowledgement is resolved without reporting false failure.
 {auto s=create(d.path/"ack");capture(s);s.observer([](ImportPhase p){if(p==ImportPhase::after_commit)throw std::runtime_error("lost acknowledgement");});check(s.cleanup_incomplete_imports("interrupted"));s.observer({});preserved(s);}
}
}
int main(){try{run();return 0;}catch(const ImportError& e){std::cerr<<"Storage code "<<e.code()<<": "<<e.what()<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
