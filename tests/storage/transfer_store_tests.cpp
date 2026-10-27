#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <source_location>
#include <sys/wait.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
const Currency cad("CAD");constexpr auto password="synthetic-transfer-password";
struct Directory{fs::path path;Directory(){auto s=(fs::temp_directory_path()/"spentit-transfers-XXXXXX").string();auto p=mkdtemp(s.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}~Directory(){std::error_code e;fs::remove_all(path,e);}};
void check(bool value,const std::source_location at=std::source_location::current()){if(!value)throw std::runtime_error("Transfer store assertion at line "+std::to_string(at.line()));}
template<class F>void rejected(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed);}
app::TransferRequest command(ImportStore& s,app::TransferAction action,std::string id="manual"){app::TransferRequest r{};r.action=action;r.link=id;r.expected_sequence=s.transfer_sequence();return r;}
auto create(ImportStore& s){auto r=command(s,app::TransferAction::manual);r.destination=2;r.amount=Money(-10000,cad);r.date="2026-10-26";r.payee="SYNTHETIC-TRANSFER-MARKER";return r;}
void basics(){Directory d;auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");auto request=create(s);const auto created=s.apply_transfer("create",request);check(created.change.units()==0);check(s.apply_transfer("create",request).sequence==created.sequence);request.amount=Money(-9999,cad);rejected([&]{s.apply_transfer("create",request);});
 s.opening(0,"2026-10-26",Money(20000,cad));check(s.projected_balance()==Money(10000,cad));check(s.committed_balance()==Money(20000,cad));check(s.ledger(true).size()==1 && s.ledger(false).empty());
 rejected([&]{s.apply_transfer("unaccepted",command(s,app::TransferAction::commit_local));});
 s.apply_transfer("accept-origin",command(s,app::TransferAction::accept));check(s.committed_balance()==Money(20000,cad));
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.apply_transfer("accept-destination",command(s,app::TransferAction::accept));s.apply_transfer("commit-destination",command(s,app::TransferAction::commit_local));check(s.committed_balance()==Money(10000,cad));
 s.select_account(1);check(transfers::protected_draft(s.transfers()[0].link,1));s.apply_transfer("clear-protected",command(s,app::TransferAction::clear_local));check(s.projected_balance()==Money(10000,cad));s.apply_transfer("commit-origin",command(s,app::TransferAction::commit_local));check(s.committed_balance()==Money(10000,cad));
 s.ingest("ordinary","D27/10/2026\nT-5\nPOrdinary\n^\n",qif::DateOrder::dmy);s.commit("ordinary",0,"commit-ordinary");check(s.committed_balance()==Money(9500,cad));s.verify();s.backup_to(d.path/"backup",password);}
 {auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(9500,cad));check(s.ledger(false).size()==2);s.select_account(2);check(s.committed_balance()==Money(10000,cad));s.verify();}
 {auto s=ImportStore::open(d.path/"backup",password);check(s.transfers().size()==1 && s.committed_balance()==Money(9500,cad));}
}
void import_flow(){Directory d;const auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));
 s.ingest("origin","D26/10/2026\nT-100\nPTransfer\n^\nD26/10/2026\nT-5\nPDiscard me\n^\n",qif::DateOrder::dmy);
 auto r=command(s,app::TransferAction::from_draft,"imported");r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign",r);
 rejected([&]{s.save({"origin",0},0,*s.row({"origin",0}).draft);});
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.apply_transfer("accept",command(s,app::TransferAction::accept,"imported"));s.apply_transfer("commit-local",command(s,app::TransferAction::commit_local,"imported"));
 s.select_account(1);s.discard("origin",0);check(s.batches()[0].state==app::BatchState::protected_drafts);check(s.projected_balance()==Money(10000,cad));
 s.ingest("new","D27/10/2026\nT-2\nPNew\n^\n",qif::DateOrder::dmy);check(s.projected_balance()==Money(9800,cad));s.commit("origin",1,"commit-protected");s.commit("new",0,"commit-new");check(s.committed_balance()==Money(9800,cad));
 s.select_account(2);s.ingest("bank","D28/10/2026\nT100\nPBank evidence\n^\n",qif::DateOrder::dmy);
 r=command(s,app::TransferAction::propose_match,"imported");r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("propose",r);
 check(s.committed_balance()==Money(10000,cad));check(s.projected_balance()==Money(10000,cad));check(s.ledger(true).size()==1);s.discard("bank",0);check(!s.transfers()[0].link.destination.proposed_evidence);check(s.committed_balance()==Money(10000,cad));
 s.ingest("bank-again","D28/10/2026\nT100\nPBank evidence\n^\n",qif::DateOrder::dmy);r=command(s,app::TransferAction::propose_match,"imported");r.evidence=transfers::BankEvidence{{2,{"bank-again",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("propose-again",r);
 const auto receipt=s.commit("bank-again",0,"attach");check(receipt.before==Money(10000,cad) && receipt.change.units()==0 && receipt.after==Money(10000,cad));check(s.ledger(false).size()==1);check(s.transfers()[0].link.destination.attached_evidence.has_value());s.verify();s.backup_to(d.path/"backup",password);}
 {auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(9800,cad));s.select_account(2);check(s.committed_balance()==Money(10000,cad));s.verify();}
 {auto s=ImportStore::open(d.path/"backup",password);s.select_account(2);check(s.transfers()[0].link.destination.date=="2026-10-28");}
}

void split_flow(){Directory d;const auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.create_account(3,"Savings",cad,"2026-10-26");s.create_account(4,"Foreign",Currency("USD"),"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));
 s.ingest("split","D26/10/2026\nT-100\nPSplit transfer\nSFirst\n$-60\nSSecond\n$-50\nSRefund\n$10\n^\n",qif::DateOrder::dmy);
 auto r=command(s,app::TransferAction::from_draft,"one");r.origin=imports::Occurrence{"split",0};r.allocation="source-0";r.destination=4;rejected([&]{s.apply_transfer("currency",r);});r.destination=2;s.apply_transfer("first",r);
 r=command(s,app::TransferAction::from_draft,"two");r.origin=imports::Occurrence{"split",0};r.allocation="source-1";r.destination=3;s.apply_transfer("second",r);
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.apply_transfer("accept",command(s,app::TransferAction::accept,"one"));s.apply_transfer("publish",command(s,app::TransferAction::commit_local,"one"));
 s.select_account(1);s.discard("split",0);check(s.transfers().size()==2);check(s.projected_balance()==Money(10000,cad));s.commit("split",1,"origin");
 s.select_account(3);check(s.projected_balance()==Money(5000,cad));check(s.committed_balance()==Money(0,cad));s.verify();s.backup_to(d.path/"backup",password);}
 {auto s=ImportStore::open(d.path/"backup",password);check(s.committed_balance()==Money(10000,cad));s.select_account(3);check(transfers::protected_draft(s.transfers()[0].link,3));}
}
void manual_evidence(){Directory d;const auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));s.apply_transfer("create",create(s));
 s.ingest("bank","D27/10/2026\nT-100\nPManual origin evidence\n^\n",qif::DateOrder::dmy);auto r=command(s,app::TransferAction::propose_match);r.evidence=transfers::BankEvidence{{1,{"bank",0},"source-0"},Money(-9500,cad),"2026-10-27"};rejected([&]{s.apply_transfer("mismatch",r);});r.evidence->amount=Money(-10000,cad);s.apply_transfer("match",r);check(s.projected_balance()==Money(10000,cad));const auto receipt=s.commit("bank",0,"commit");check(receipt.change==Money(-10000,cad));check(s.transfers()[0].link.origin.manually_entered);check(s.ledger(false).size()==1);
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.ingest("destination-bank","D28/10/2026\nT100\nPDestination evidence\n^\n",qif::DateOrder::dmy);
 r=command(s,app::TransferAction::propose_match);r.evidence=transfers::BankEvidence{{2,{"destination-bank",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("destination-match",r);check(s.committed_balance()==Money(0,cad));check(s.projected_balance()==Money(10000,cad));s.commit("destination-bank",0,"destination-publish");check(s.committed_balance()==Money(10000,cad));check(s.ledger(false).size()==1);s.verify();}
 {auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(10000,cad));check(s.transfers()[0].link.origin.date=="2026-10-27");}
}

void no_leaks(const fs::path& path){for(const auto& entry:fs::directory_iterator(path))if(entry.is_regular_file()){std::ifstream stream(entry.path(),std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(stream),{}};check(!bytes.starts_with("SQLite format 3") && bytes.find(password)==bytes.npos && bytes.find("SYNTHETIC-TRANSFER-MARKER")==bytes.npos);}}
void fault_setup(const fs::path& path,int mode){
 auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.select_account(1);
 if(mode==0)return;
 if(mode==2 || mode==9){s.apply_transfer("create",create(s));return;}
 s.ingest("origin","D26/10/2026\nT-100\nPSYNTHETIC-TRANSFER-MARKER\n^\nD26/10/2026\nT-5\nPDiscarded\n^\n",qif::DateOrder::dmy);
 if(mode==1)return;
 auto r=command(s,app::TransferAction::from_draft);r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign",r);
 s.select_account(2);s.apply_transfer("accept",command(s,app::TransferAction::accept));if(mode==3)return;
 s.apply_transfer("destination-commit",command(s,app::TransferAction::commit_local));if(mode==4 || mode==7)return;
 s.select_account(1);s.commit("origin",0,"origin-commit");s.select_account(2);s.ingest("bank","D28/10/2026\nT100\nPBank evidence\n^\n",qif::DateOrder::dmy);if(mode==5)return;
 r=command(s,app::TransferAction::propose_match);r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("proposal",r);
}
void fault_execute(ImportStore& s,int mode,std::int64_t expected){
 s.select_account(mode==3 || mode==5 || mode==6 || mode==8?2:1);
 if(mode==4){s.commit("origin",0,"operation");return;}if(mode==6){s.commit("bank",0,"operation");return;}
 if(mode==7){s.discard("origin",0);return;}if(mode==8){s.discard("bank",0);return;}
 auto r=command(s,app::TransferAction::accept);r.expected_sequence=expected;
 if(mode==0){r=create(s);r.expected_sequence=expected;}
 if(mode==1){r.action=app::TransferAction::from_draft;r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;}
 if(mode==3)r.action=app::TransferAction::commit_local;
 if(mode==5){r.action=app::TransferAction::propose_match;r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(10000,cad),"2026-10-28"};}
 if(mode==9)r.action=app::TransferAction::clear_local;
 s.apply_transfer("operation",r);
}
void faults(bool crash){
 fault_vfs::install();
 for(int mode=0;mode<10;++mode){Directory fixture;const auto seed=fixture.path/"seed";fault_setup(seed,mode);std::int64_t expected;
 {auto s=ImportStore::open(seed,password);expected=s.transfer_sequence();}
 int count=0;{Directory sample;const auto path=sample.path/"doc";fs::copy_file(seed,path);auto s=ImportStore::open(path,password);fault_vfs::arm(-1);fault_execute(s,mode,expected);count=fault_vfs::seen;fault_vfs::disarm();s.verify();}check(count>0);
 for(int position=1;position<=count;++position){Directory d;const auto path=d.path/"doc";fs::copy_file(seed,path);
  if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);fault_execute(s,mode,expected);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));}
  else{{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{fault_execute(s,mode,expected);}catch(const std::exception&){}fault_vfs::disarm();}}
  {auto s=ImportStore::open(path,password);check(s.transfer_sequence()==expected || s.transfer_sequence()==expected+1);s.verify();fault_execute(s,mode,expected);s.verify();check(s.transfer_sequence()==expected+1);}no_leaks(d.path);
 }
 for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;const auto path=d.path/"doc";fs::copy_file(seed,path);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});fault_execute(s,mode,expected);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);auto s=ImportStore::open(path,password);check(s.transfer_sequence()==expected+(phase==ImportPhase::after_commit?1:0));fault_execute(s,mode,expected);s.verify();}
 std::cout<<"Transfer operation "<<mode<<": "<<count<<" write/sync positions\n"<<std::flush;
 }
}
void validation(){
 for(const char* sql:{"UPDATE transfer_versions SET payload=x'00'", "UPDATE transfer_heads SET sequence=999", "UPDATE transfer_events SET local_change=1", "UPDATE transfer_events SET account=2", "DELETE FROM transfer_events", "DELETE FROM transfer_heads"}){
  Directory d;const auto path=d.path/"doc";fault_setup(path,2);
  sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
  check(sqlite3_exec(db,"DROP TRIGGER transfer_versions_immutable;DROP TRIGGER transfer_head_step;DROP TRIGGER transfer_events_immutable;DROP TRIGGER transfer_events_keep;DROP TRIGGER transfer_heads_keep",nullptr,nullptr,nullptr)==SQLITE_OK);
  check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);rejected([&]{ImportStore::open(path,password);});no_leaks(d.path);
 }
}

}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"";if(name=="basics")basics();else if(name=="imports")import_flow();else if(name=="splits")split_flow();else if(name=="manual_evidence")manual_evidence();else if(name=="io")faults(false);else if(name=="crashes")faults(true);else if(name=="validation")validation();else return 2;std::cout<<"Transfer store "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
