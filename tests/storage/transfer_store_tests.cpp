#include "session_test_helpers.hpp"
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
void check(bool value,const std::source_location at=std::source_location::current()){if(std::getenv("SPENTIT_TEST_TRACE"))std::cerr<<"check "<<at.line()<<"\n";if(!value)throw std::runtime_error("Transfer store assertion at line "+std::to_string(at.line()));}
template<class F>void rejected(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed);}
template<class F>void message_has(const char* text,F fn,const std::source_location at=std::source_location::current()){try{fn();check(false);}catch(const std::exception& error){if(std::string(error.what()).find(text)==std::string::npos)throw std::runtime_error("Expected "+std::string(text)+" at line "+std::to_string(at.line())+", got "+error.what());}}
transfers::AllocationFact allocation_fact(std::int64_t account,std::string batch,std::string allocation,Money amount,std::string date,transfers::State state,bool imported,bool generated=false){return {{account,{std::move(batch),0},std::move(allocation)},amount,std::move(date),state,imported,generated,false};}
app::TransferView view_of(ImportStore& store,const std::string& id){for(const auto& value:store.transfers())if(value.link.identity==id)return value;throw std::runtime_error("Missing transfer "+id);}
bool shows(ImportStore& store,const std::string& batch){for(const auto& row:store.ledger(true))if(row.identity.batch==batch)return true;return false;}
app::TransferRequest command(ImportStore& s,app::TransferAction action,std::string id="manual"){app::TransferRequest r{};r.action=action;r.link=id;r.expected_sequence=s.transfer_sequence();return r;}
app::TransferRequest merge_of(ImportStore& store,std::string link,transfers::AllocationFact left,transfers::AllocationFact right,std::int64_t left_revision,std::int64_t right_revision,std::string payee=""){auto request=command(store,app::TransferAction::merge,std::move(link));request.kept=request.link+"-kept";request.left=std::move(left);request.right=std::move(right);request.batch_revision=left_revision;request.right_revision=right_revision;request.payee=std::move(payee);return request;}
app::TransferRequest retarget_of(ImportStore& store,std::string link,transfers::AllocationFact origin,std::int64_t revision,std::string category,std::string category_id=""){auto request=command(store,app::TransferAction::retarget,std::move(link));request.kept=request.link+"-kept";request.mode=0;request.left=std::move(origin);request.batch_revision=revision;request.category_path=std::move(category);request.category_id=std::move(category_id);return request;}
app::ChangeReceipt apply_change(ImportStore& store,const std::string& id,const app::ChangeRequest& request){const auto preview=store.preview_committed_change(request);const auto receipt=store.apply_committed_change(id,request,preview.expected,preview.preview_sequence);check(store.apply_committed_change(id,request,preview.expected,preview.preview_sequence).sequence==receipt.sequence);return receipt;}
bool contains(const std::string& value,const char* text){return value.find(text)!=std::string::npos;}
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
// These fixtures deliberately use unresolved historical export labels so the
// explicit destination-assignment commands remain independently exercised.
void import_flow(){Directory d;const auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));
 s.ingest("origin","D26/10/2026\nT-100\nPTransfer\nL[Old Visa export]\n^\nD26/10/2026\nT-5\nPDiscard me\nL[Unresolved discarded destination]\n^\n",qif::DateOrder::dmy);
 rejected([&]{s.commit("origin",0,"unresolved");});check(s.committed_balance()==Money(20000,cad));
 auto edit=*s.row({"origin",0}).draft;auto parts=edit.transaction.allocations();parts[0].category="Food";edit.transaction=edit.transaction.corrected(edit.transaction.effective_total(),parts);rejected([&]{s.save({"origin",0},0,edit);});
 auto r=command(s,app::TransferAction::from_draft,"imported");r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign",r);
 rejected([&]{s.save({"origin",0},0,*s.row({"origin",0}).draft);});
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.apply_transfer("accept",command(s,app::TransferAction::accept,"imported"));s.apply_transfer("commit-local",command(s,app::TransferAction::commit_local,"imported"));
 s.select_account(1);test_session::close_reopen(s,path,password);s.discard("origin",0);check(s.batches()[0].state==app::BatchState::protected_drafts);check(s.projected_balance()==Money(10000,cad));
 s.ingest("new","D27/10/2026\nT-2\nPNew\n^\n",qif::DateOrder::dmy);check(s.projected_balance()==Money(9800,cad));s.commit("origin",1,"commit-protected");s.commit("new",0,"commit-new");check(s.committed_balance()==Money(9800,cad));
 s.select_account(2);s.ingest("bank","D28/10/2026\nT100\nPBank evidence\nL[Old Checking export]\n^\n",qif::DateOrder::dmy);
 r=command(s,app::TransferAction::propose_match,"imported");r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("propose",r);
 check(s.committed_balance()==Money(10000,cad));check(s.projected_balance()==Money(10000,cad));check(s.ledger(true).size()==1);test_session::close_reopen(s,path,password);s.rollback_account("forget-bank",s.rollback_review().token);check(!s.transfers()[0].link.destination.proposed_evidence);check(s.committed_balance()==Money(10000,cad));
 s.ingest("bank-again","D28/10/2026\nT100\nPBank evidence\nL[Old Checking export]\n^\n",qif::DateOrder::dmy);r=command(s,app::TransferAction::propose_match,"imported");r.evidence=transfers::BankEvidence{{2,{"bank-again",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("propose-again",r);
 check(s.row({"bank-again",0}).matched_generated);check(s.acceptance_review().transactions==0);check(s.import_result("bank-again").already_present==1);check(s.ledger(false).size()==1);check(!s.transfers()[0].link.destination.proposed_evidence);s.verify();s.backup_to(d.path/"backup",password);}
 {auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(9800,cad));s.select_account(2);check(s.committed_balance()==Money(10000,cad));s.verify();}
 {auto s=ImportStore::open(d.path/"backup",password);s.select_account(2);check(s.transfers()[0].link.destination.date=="2026-10-26");}
}

void split_flow(){Directory d;const auto path=d.path/"doc";
 {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.create_account(3,"Savings",cad,"2026-10-26");s.create_account(4,"Foreign",Currency("USD"),"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));
 s.ingest("split","D26/10/2026\nT-100\nPSplit transfer\nS[Old Visa export]\n$-60\nS[Old Savings export]\n$-50\nSRefund\n$10\n^\n",qif::DateOrder::dmy);
 auto r=command(s,app::TransferAction::from_draft,"one");r.origin=imports::Occurrence{"split",0};r.allocation="source-0";r.destination=4;rejected([&]{s.apply_transfer("currency",r);});r.destination=2;s.apply_transfer("first",r);
 r=command(s,app::TransferAction::from_draft,"two");r.origin=imports::Occurrence{"split",0};r.allocation="source-1";r.destination=3;s.apply_transfer("second",r);
 s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.apply_transfer("accept",command(s,app::TransferAction::accept,"one"));s.apply_transfer("publish",command(s,app::TransferAction::commit_local,"one"));
 s.select_account(1);test_session::close_reopen(s,path,password);s.discard("split",0);check(s.transfers().size()==2);check(s.projected_balance()==Money(10000,cad));s.commit("split",1,"origin");
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

void qif_destinations(){
 {
  Directory d;const auto path=d.path/"doc";
  {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Renamed destination",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(0,cad));
   s.ingest("repeated","D26/10/2026\nT-19\nS[Old export name]\n$-10\nS[Old export name]\n$-10\nSRefund\n$1\n^\n",qif::DateOrder::dmy);}
  {auto s=ImportStore::open(path,password);check(s.transfers().empty());rejected([&]{s.commit("repeated",0,"blocked");});
   for(int index:{0,1}) {
    auto request=command(s,app::TransferAction::from_draft,"repeat-"+std::to_string(index));request.origin=imports::Occurrence{"repeated",0};request.allocation="source-"+std::to_string(index);request.destination=2;
    s.apply_transfer("resolve-"+std::to_string(index),request);
    if(index==0)rejected([&]{s.commit("repeated",0,"partly-resolved");});
   }
   s.commit("repeated",0,"resolved");check(s.committed_balance()==Money(-1900,cad));s.select_account(2);check(s.projected_balance()==Money(2000,cad));check(s.transfers().size()==2);s.verify();
  }
 }
 for(bool reverse:{false,true}) {
  Directory d;const auto path=d.path/"doc";
  {
   auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");
   rejected([&]{s.create_account(3,"Visa",cad,"2026-10-26");});s.create_account(3,"Other Visa",cad,"2026-10-26");s.create_account(4,"Foreign",Currency("USD"),"2026-10-26");
   for(int account:{1,2}){s.select_account(account);s.opening(0,"2026-10-26",Money(0,cad));}
   const auto first=reverse?2:1,second=reverse?1:2;
   s.select_account(first);
   const std::string source=reverse?"D26/10/2026\nT100\nL[Old Checking export]\n^\n":"D26/10/2026\nT-100\nL[Old Visa export]\n^\n";
   s.ingest("first",source,qif::DateOrder::dmy);check(s.evidence("first")==source);
   rejected([&]{s.commit("first",0,"unresolved");});check(s.transfers().empty());
   auto request=command(s,app::TransferAction::from_draft,"qif");request.origin=imports::Occurrence{"first",0};request.allocation="source-0";
   for(int destination:{0,first,4,999}){request.destination=destination;rejected([&]{s.apply_transfer("invalid",request);});}
   request.destination=second;const auto receipt=s.apply_transfer("resolve",request);check(s.apply_transfer("resolve",request).sequence==receipt.sequence);
   s.commit("first",0,"publish-first");s.ingest("duplicate",source,qif::DateOrder::dmy);check(s.commit("duplicate",0,"duplicate-review").change.units()==0);check(s.transfers().size()==1);s.select_account(second);
   const std::string bank=reverse?"D28/10/2026\nT-100\nL[Old Visa export]\n^\n":"D28/10/2026\nT100\nL[Old Checking export]\n^\n";
   s.ingest("second",bank,qif::DateOrder::dmy);rejected([&]{s.commit("second",0,"unmatched");});
   request=command(s,app::TransferAction::propose_match,"qif");request.evidence=transfers::BankEvidence{{second,{"second",0},"source-0"},Money(reverse?-10000:10000,cad),"2026-10-28"};
   s.apply_transfer("match",request);check(s.projected_balance()==Money(reverse?-10000:10000,cad));
   s.commit("second",0,"publish-second");check(s.ledger(false).size()==1);check(s.transfers().size()==1);
   s.verify();s.backup_to(d.path/"backup",password);
  }
  {auto restored=ImportStore::open(d.path/"backup",password);check(restored.committed_balance()==Money(-10000,cad));restored.select_account(2);check(restored.committed_balance()==Money(10000,cad));restored.select_account(3);check(restored.committed_balance()==Money(0,cad));}
 }
}

void management(){
    const auto usd=Currency("USD");
    {
        Directory d;const auto path=d.path/"doc";
        {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.create_account(3,"Savings",cad,"2026-10-26");
            for(int id:{1,2,3}){s.select_account(id);s.opening(0,"2026-10-26",Money(0,cad));}s.select_account(1);
            s.ingest("whole","D26/10/2026\nT-100\nPWhole\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("visa95","D28/10/2026\nT95\nPVisa ninety five\n^\n",qif::DateOrder::dmy);s.select_account(1);
            const auto whole=allocation_fact(1,"whole","source-0",Money(-10000,cad),"2026-10-26",transfers::State::draft,true);
            const auto visa=allocation_fact(2,"visa95","source-0",Money(9500,cad),"2026-10-28",transfers::State::draft,true);
            message_has("disagree",[&]{s.apply_transfer("unequal-whole",merge_of(s,"nope",whole,visa,0,0));});check(s.transfers().empty());test_session::close_reopen(s,path,password);s.discard("whole",0);
            s.ingest("split","D26/10/2026\nT-100\nPSplit\nSFees\n$-5\nS[Old Visa export]\n$-95\n^\n",qif::DateOrder::dmy);
            const auto fee=allocation_fact(1,"split","source-0",Money(-500,cad),"2026-10-26",transfers::State::draft,true);
            const auto part=allocation_fact(1,"split","source-1",Money(-9500,cad),"2026-10-26",transfers::State::draft,true);
            message_has("disagree",[&]{s.apply_transfer("unequal-fee",merge_of(s,"fee-link",fee,visa,0,0));});check(s.transfers().empty());
            const auto linked=merge_of(s,"pay-95",part,visa,0,0,"Split");const auto receipt=s.apply_transfer("link-95",linked);check(receipt.change.units()==0 && s.apply_transfer("link-95",linked).sequence==receipt.sequence);
            const auto view=view_of(s,"pay-95");check(view.link.origin.endpoint.allocation=="source-1" && view.link.origin.endpoint.entry.batch=="split" && view.link.destination.endpoint.entry.batch=="visa95");
            check(view.link.origin.amount==Money(-9500,cad) && view.link.destination.amount==Money(9500,cad) && view.link.origin.date=="2026-10-26" && view.link.destination.date=="2026-10-28");
            check(s.row({"split",0}).draft->transaction.allocations().at(0).amount==Money(-500,cad));s.select_account(2);check(s.row({"visa95",0}).draft->transaction.allocations().at(0).amount==Money(9500,cad));s.select_account(1);
            bool saw=false;for(const auto& entry:s.transfer_journal())if(entry.action=="Merge" && entry.link=="pay-95"){check(entry.origin_account==1 && entry.destination_account==2 && entry.origin_amount==Money(-9500,cad) && entry.destination_amount==Money(9500,cad) && entry.origin_date=="2026-10-26" && !entry.unresolved && entry.sequence>0);saw=true;}check(saw);
            auto stale=linked;stale.expected_sequence=s.transfer_sequence()+4;message_has("preview changed",[&]{s.apply_transfer("stale-link",stale);});auto changed=linked;changed.payee="Other";message_has("reused",[&]{s.apply_transfer("link-95",changed);});
            s.commit("split",0,"commit-split");s.select_account(2);check(view_of(s,"pay-95").link.destination.state==transfers::State::draft);s.commit("visa95",0,"commit-visa95");check(view_of(s,"pay-95").link.origin.state==transfers::State::committed && view_of(s,"pay-95").link.destination.state==transfers::State::committed && view_of(s,"pay-95").link.destination.amount==Money(9500,cad));
            s.select_account(1);s.ingest("twice","D27/10/2026\nT-190\nPTwice\nS[Old Visa export]\n$-95\nS[Old Visa export]\n$-95\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("visa-a","D30/10/2026\nT95\nPVisa A\n^\n",qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("link-a",merge_of(s,"pay-a",allocation_fact(1,"twice","source-0",Money(-9500,cad),"2026-10-27",transfers::State::draft,true),allocation_fact(2,"visa-a","source-0",Money(9500,cad),"2026-10-30",transfers::State::draft,true),0,0));
            s.select_account(2);s.commit("visa-a",0,"commit-visa-a");check(view_of(s,"pay-a").link.destination.state==transfers::State::committed);s.ingest("visa-b","D29/10/2026\nT95\nPVisa B\n^\n",qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("link-b",merge_of(s,"pay-b",allocation_fact(1,"twice","source-1",Money(-9500,cad),"2026-10-27",transfers::State::draft,true),allocation_fact(2,"visa-b","source-0",Money(9500,cad),"2026-10-29",transfers::State::draft,true),0,0));
            check(view_of(s,"pay-a").link.origin.endpoint!=view_of(s,"pay-b").link.origin.endpoint && view_of(s,"pay-a").link.destination.endpoint!=view_of(s,"pay-b").link.destination.endpoint);
            s.commit("twice",0,"commit-twice");s.select_account(2);s.commit("visa-b",0,"commit-visa-b");s.select_account(1);
            s.ingest("plain","D26/10/2026\nT-3\nPShop\n^\n",qif::DateOrder::dmy);auto edit=*s.row({"plain",0}).draft;auto parts=edit.transaction.allocations();parts[0].category="Visa";parts[0].category_owner=FieldOwnership::manual;edit.transaction=edit.transaction.corrected(edit.transaction.effective_total(),std::move(parts));rejected([&]{s.save({"plain",0},0,edit);});check(s.transfers().size()==3);
            s.verify();s.backup_to(d.path/"backup",password);}
        {auto s=ImportStore::open(path,password);check(view_of(s,"pay-95").link.destination.amount==Money(9500,cad));check(s.transfer_journal().size()>=3);s.verify();}
        {auto s=ImportStore::open(d.path/"backup",password);check(s.transfers().size()==3 && view_of(s,"pay-b").link.origin.endpoint.allocation=="source-1");}
    }
    {
        Directory d;const auto path=d.path/"doc";
        {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(0,cad));s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.select_account(1);
            s.ingest("origin","D26/10/2026\nT-100\nPOrigin\n^\n",qif::DateOrder::dmy);auto assign=command(s,app::TransferAction::from_draft,"generated-link");assign.origin=imports::Occurrence{"origin",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign",assign);
            s.select_account(2);message_has("origin controls",[&]{s.apply_transfer("delete-dest",command(s,app::TransferAction::clear_local,"generated-link"));});check(shows(s,"~transfer-generated-link-destination"));
            s.select_account(1);test_session::close_reopen(s,path,password);s.discard("origin",0);s.select_account(2);check(!shows(s,"~transfer-generated-link-destination"));s.select_account(1);check(view_of(s,"generated-link").link.origin.state==transfers::State::removed && view_of(s,"generated-link").link.destination.state==transfers::State::removed);
            s.ingest("origin-b","D26/10/2026\nT-100\nPOrigin\n^\n",qif::DateOrder::dmy);assign=command(s,app::TransferAction::from_draft,"generated-b");assign.origin=imports::Occurrence{"origin-b",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-b",assign);
            s.select_account(2);s.ingest("visa-row","D28/10/2026\nT100\nPIndependent\n^\n",qif::DateOrder::dmy);
            const auto generated=allocation_fact(2,"~transfer-generated-b-destination","generated",Money(10000,cad),"2026-10-26",transfers::State::draft,false,true);
            const auto independent=allocation_fact(2,"visa-row","source-0",Money(10000,cad),"2026-10-28",transfers::State::draft,true);
            const auto request=merge_of(s,"generated-b",generated,independent,0,0);const auto merged=s.apply_transfer("merge-dup",request);check(merged.change.units()==0 && s.apply_transfer("merge-dup",request).sequence==merged.sequence);
            const auto linked=view_of(s,"generated-b");check(linked.link.destination.endpoint.entry.batch=="visa-row" && linked.link.destination.amount==Money(10000,cad) && linked.link.destination.date=="2026-10-28" && linked.link.origin.amount==Money(-10000,cad) && linked.link.origin.date=="2026-10-26");
            check(!shows(s,"~transfer-generated-b-destination") && shows(s,"visa-row") && s.row({"visa-row",0}).draft->transaction.effective_total()==Money(10000,cad) && !s.record_evidence({"visa-row",0}).empty());
            s.select_account(1);s.commit("origin-b",0,"commit-origin");s.select_account(2);check(s.committed_balance()==Money(0,cad));s.commit("visa-row",0,"commit-visa-row");check(view_of(s,"generated-b").link.destination.state==transfers::State::committed && view_of(s,"generated-b").link.destination.amount==Money(10000,cad));s.select_account(1);
            s.ingest("origin-c","D27/10/2026\nT-100\nPProtected\n^\n",qif::DateOrder::dmy);assign=command(s,app::TransferAction::from_draft,"generated-c");assign.origin=imports::Occurrence{"origin-c",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-c",assign);s.commit("origin-c",0,"commit-c");
            s.select_account(2);s.apply_transfer("clear-protected",command(s,app::TransferAction::clear_local,"generated-c"));check(view_of(s,"generated-c").link.destination.endpoint.allocation=="generated" && view_of(s,"generated-c").link.destination.state==transfers::State::draft);
            s.ingest("visa-c","D30/10/2026\nT100\nPLater\n^\n",qif::DateOrder::dmy);
            s.apply_transfer("merge-protected",merge_of(s,"generated-c",allocation_fact(2,"~transfer-generated-c-destination","generated",Money(10000,cad),"2026-10-27",transfers::State::draft,false,true),allocation_fact(2,"visa-c","source-0",Money(10000,cad),"2026-10-30",transfers::State::draft,true),0,0));
            check(view_of(s,"generated-c").link.origin.state==transfers::State::committed && view_of(s,"generated-c").link.destination.endpoint.entry.batch=="visa-c" && view_of(s,"generated-c").link.origin.amount==Money(-10000,cad));
            s.commit("visa-c",0,"commit-visa-c");s.select_account(1);s.ingest("origin-d","D02/11/2026\nT-100\nPCommitted generated\n^\n",qif::DateOrder::dmy);assign=command(s,app::TransferAction::from_draft,"generated-d");assign.origin=imports::Occurrence{"origin-d",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-d",assign);
            s.select_account(2);s.apply_transfer("accept-d",command(s,app::TransferAction::accept,"generated-d"));s.apply_transfer("commit-d",command(s,app::TransferAction::commit_local,"generated-d"));s.ingest("visa-d","D03/11/2026\nT100\nPExtra\n^\n",qif::DateOrder::dmy);
            const auto visa_evidence=s.record_evidence({"visa-d",0});check(!visa_evidence.empty());
            const auto retiring=merge_of(s,"generated-d",allocation_fact(2,"~transfer-generated-d-destination","generated",Money(10000,cad),"2026-11-02",transfers::State::committed,false,true),allocation_fact(2,"visa-d","source-0",Money(10000,cad),"2026-11-03",transfers::State::draft,true),0,0);
            s.select_account(1);message_has("generated duplicate",[&]{s.apply_transfer("merge-wrong-account",retiring);});s.select_account(2);const auto before=s.committed_balance();
            const auto looked=s.preview_transfer(retiring);check(looked.change==Money(-10000,cad) && looked.before==before && contains(looked.note,"Visa") && contains(looked.note,"Checking"));
            const auto retired=s.apply_transfer("merge-committed-generated",retiring);check(retired.change==Money(-10000,cad) && retired.before==before && retired.after==before+retired.change && s.apply_transfer("merge-committed-generated",retiring).sequence==retired.sequence);
            const auto retired_view=view_of(s,"generated-d");check(s.committed_balance()==before+Money(-10000,cad) && retired_view.link.destination.endpoint.entry.batch=="visa-d" && retired_view.link.destination.endpoint.allocation=="source-0" && retired_view.link.destination.date=="2026-11-03");
            check(retired_view.link.origin.amount==Money(-10000,cad) && retired_view.link.origin.date=="2026-11-02" && !shows(s,"~transfer-generated-d-destination") && s.record_evidence({"visa-d",0})==visa_evidence);
            s.select_account(1);s.commit("origin-d",0,"commit-origin-d");s.ingest("unequal","D26/10/2026\nT-50\nPUnequal\n^\n",qif::DateOrder::dmy);
            message_has("disagree",[&]{s.apply_transfer("merge-unequal",merge_of(s,"bad",allocation_fact(1,"unequal","source-0",Money(-5000,cad),"2026-10-26",transfers::State::draft,true),allocation_fact(2,"visa-d","source-0",Money(10000,cad),"2026-11-03",transfers::State::draft,true),0,0));});
            s.create_account(4,"Foreign",usd,"2026-10-26");s.select_account(4);s.opening(0,"2026-10-26",Money(0,usd));s.ingest("fx-row","D28/10/2026\nT-100\nPForeign\n^\n",qif::DateOrder::dmy);
            message_has("Cross-currency",[&]{s.apply_transfer("merge-fx",merge_of(s,"fx",allocation_fact(2,"visa-d","source-0",Money(10000,cad),"2026-11-03",transfers::State::draft,true),allocation_fact(4,"fx-row","source-0",Money(-10000,usd),"2026-10-28",transfers::State::draft,true),0,0));});
            s.verify();s.backup_to(d.path/"backup",password);}
        {auto s=ImportStore::open(d.path/"backup",password);s.select_account(2);check(view_of(s,"generated-b").link.destination.date=="2026-10-28" && s.committed_balance().units()>0);s.verify();}
    }
    {
        Directory d;const auto path=d.path/"doc";
        {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(0,cad));s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.select_account(1);
            s.ingest("left-row","D26/10/2026\nT-100\nPLeft\n^\n",qif::DateOrder::dmy);s.commit("left-row",0,"left-commit");s.select_account(2);s.ingest("right-row","D28/10/2026\nT100\nPRight\n^\n",qif::DateOrder::dmy);s.commit("right-row",0,"right-commit");
            const auto left=allocation_fact(1,"left-row","source-0",Money(-10000,cad),"2026-10-26",transfers::State::committed,true);const auto right=allocation_fact(2,"right-row","source-0",Money(10000,cad),"2026-10-28",transfers::State::committed,true);
            const auto request=merge_of(s,"merged",left,right,1,1,"Left");const auto before=s.committed_balance();const auto receipt=s.apply_transfer("merge-both",request);check(receipt.change.units()==0 && receipt.before==before && s.apply_transfer("merge-both",request).sequence==receipt.sequence);
            const auto linked=view_of(s,"merged");check(linked.link.origin.state==transfers::State::committed && linked.link.destination.state==transfers::State::committed && linked.link.origin.amount==Money(-10000,cad) && linked.link.destination.amount==Money(10000,cad) && linked.link.destination.date=="2026-10-28");
            check(s.committed_balance()==before && s.ledger(false).size()==1);s.select_account(1);check(s.committed_balance()==Money(-10000,cad) && s.ledger(false).size()==1 && shows(s,"left-row"));
            message_has("another command",[&]{s.apply_transfer("left-commit",request);});
            s.ingest("draft-left","D26/10/2026\nT-40\nPDraft left\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("draft-right","D27/10/2026\nT40\nPDraft right\n^\n",qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("merge-drafts",merge_of(s,"drafts",allocation_fact(1,"draft-left","source-0",Money(-4000,cad),"2026-10-26",transfers::State::draft,true),allocation_fact(2,"draft-right","source-0",Money(4000,cad),"2026-10-27",transfers::State::draft,true),0,0));
            test_session::close_reopen(s,path,password);s.select_account(2);message_has("transfer destination",[&]{s.discard("draft-right",0);});s.select_account(1);
            s.discard("draft-left",0);check(view_of(s,"drafts").link.unresolved && view_of(s,"drafts").link.origin.state==transfers::State::removed && view_of(s,"drafts").link.destination.amount==Money(4000,cad) && view_of(s,"drafts").link.destination.state==transfers::State::draft);
            s.select_account(2);check(s.row({"draft-right",0}).draft->transaction.effective_total()==Money(4000,cad));s.commit("draft-right",0,"commit-draft-right");
            s.select_account(1);s.ingest("keep-left","D30/10/2026\nT-40\nPKeep\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("keep-right","D31/10/2026\nT40\nPKeep right\n^\n",qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("merge-keep",merge_of(s,"keep",allocation_fact(1,"keep-left","source-0",Money(-4000,cad),"2026-10-30",transfers::State::draft,true),allocation_fact(2,"keep-right","source-0",Money(4000,cad),"2026-10-31",transfers::State::draft,true),0,0));
            s.select_account(2);s.commit("keep-right",0,"commit-right-only");s.select_account(1);test_session::close_reopen(s,path,password);s.discard("keep-left",0);
            bool retained=false;for(const auto& batch:s.batches())if(batch.id=="keep-left"){check(batch.state==app::BatchState::protected_drafts);retained=true;}check(retained);
            check(!view_of(s,"keep").link.unresolved && view_of(s,"keep").link.destination.state==transfers::State::committed && view_of(s,"keep").link.destination.amount==Money(4000,cad));
            s.verify();s.backup_to(d.path/"backup",password);}
        {auto s=ImportStore::open(d.path/"backup",password);check(view_of(s,"merged").link.origin.amount==Money(-10000,cad));s.verify();}
    }
    {
        Directory d;const auto path=d.path/"doc";
        {auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.create_account(3,"Savings",cad,"2026-10-26");
            app::CategoryRequest created{app::CategoryAction::create,"fees","Fees",{},CategoryKind::leaf};const auto preview=s.preview_category_change(created);s.apply_category_change("fees",preview.request,preview.catalog_revision);
            for(int id:{1,2,3}){s.select_account(id);s.opening(0,"2026-10-26",Money(0,cad));}s.select_account(1);
            s.ingest("origin","D26/10/2026\nT-100\nPOrigin\n^\n",qif::DateOrder::dmy);auto assign=command(s,app::TransferAction::from_draft,"generated-link");assign.origin=imports::Occurrence{"origin",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign",assign);
            const auto origin=allocation_fact(1,"origin","source-0",Money(-10000,cad),"2026-10-26",transfers::State::draft,true);
            auto category=retarget_of(s,"generated-link",origin,0,"Fees","fees");const auto changed=s.apply_transfer("retarget-category",category);check(changed.change.units()==0 && s.apply_transfer("retarget-category",category).sequence==changed.sequence);
            check(view_of(s,"generated-link").link.origin.state==transfers::State::removed && view_of(s,"generated-link").link.destination.state==transfers::State::removed);
            check(s.row({"origin",0}).draft->transaction.allocations().at(0).category=="Fees" && s.row({"origin",0}).draft->transaction.allocations().at(0).amount==Money(-10000,cad));
            s.select_account(2);check(!shows(s,"~transfer-generated-link-destination"));
            bool category_row=false;for(const auto& entry:s.transfer_journal())if(entry.action=="Change category" && entry.link=="generated-link")category_row=true;check(category_row);
            s.select_account(1);s.commit("origin",1,"commit-categorized");s.ingest("move","D27/10/2026\nT-100\nPMove\n^\n",qif::DateOrder::dmy);assign=command(s,app::TransferAction::from_draft,"move-link");assign.origin=imports::Occurrence{"move",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-move",assign);
            auto next=retarget_of(s,"move-link",allocation_fact(1,"move","source-0",Money(-10000,cad),"2026-10-27",transfers::State::draft,true),0,"","");next.mode=1;next.generated=transfers::Endpoint{3,{"~transfer-stamp-destination",0},"generated"};next.generated_currency=cad.definition_id();next.category_id.clear();next.category_path.clear();
            s.apply_transfer("retarget-account",next);check(view_of(s,"move-link").link.destination.endpoint.account==3 && view_of(s,"move-link").link.destination.endpoint.entry.batch=="~transfer-stamp-destination");
            s.select_account(2);check(!shows(s,"~transfer-move-link-destination"));s.select_account(3);check(shows(s,"~transfer-stamp-destination"));s.rename_account("rename-savings","Reserve");check(s.account().identity==3 && view_of(s,"move-link").link.destination.endpoint.account==3);
            bool destination_row=false;for(const auto& entry:s.transfer_journal())if(entry.action=="Change destination")destination_row=true;check(destination_row);
            s.select_account(1);s.commit("move",0,"commit-move");s.ingest("fresh","D02/11/2026\nT-100\nPFresh\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("already","D28/10/2026\nT100\nPAlready\n^\n",qif::DateOrder::dmy);s.select_account(1);
            auto fresh=retarget_of(s,"fresh-link",allocation_fact(1,"fresh","source-0",Money(-10000,cad),"2026-11-02",transfers::State::draft,true),0,"","");fresh.mode=1;fresh.generated=transfers::Endpoint{2,{"~transfer-fresh-link-destination",0},"generated"};fresh.generated_currency=cad.definition_id();
            s.apply_transfer("generate-only",fresh);check(view_of(s,"fresh-link").link.destination.endpoint.entry.batch=="~transfer-fresh-link-destination");s.select_account(2);check(shows(s,"already") && shows(s,"~transfer-fresh-link-destination"));
            s.select_account(1);s.commit("fresh",0,"commit-fresh");s.select_account(2);s.commit("already",0,"commit-already");s.select_account(1);s.ingest("committed-origin","D26/10/2026\nT-70\nPCommitted origin\n^\n",qif::DateOrder::dmy);s.select_account(2);s.ingest("committed-dest","D28/10/2026\nT70\nPCommitted dest\n^\n",qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("merge-open",merge_of(s,"open",allocation_fact(1,"committed-origin","source-0",Money(-7000,cad),"2026-10-26",transfers::State::draft,true),allocation_fact(2,"committed-dest","source-0",Money(7000,cad),"2026-10-28",transfers::State::draft,true),0,0));
            s.select_account(2);s.commit("committed-dest",0,"commit-dest");const auto dest_balance=s.committed_balance();s.select_account(1);
            auto release=retarget_of(s,"open",allocation_fact(1,"committed-origin","source-0",Money(-7000,cad),"2026-10-26",transfers::State::draft,true),0,"Fees","fees");
            s.apply_transfer("unlink-committed",release);check(view_of(s,"open").link.unresolved && view_of(s,"open").link.destination.amount==Money(7000,cad) && view_of(s,"open").link.destination.state==transfers::State::committed && view_of(s,"open").link.destination.date=="2026-10-28");
            s.select_account(2);check(s.committed_balance()==dest_balance);check(s.committed_entry({"committed-dest",0}).current.draft.transaction.effective_total()==Money(7000,cad));
            s.select_account(1);check(s.row({"committed-origin",0}).draft->transaction.allocations().at(0).category=="Fees");
            s.commit("committed-origin",1,"commit-origin-later");check(view_of(s,"open").link.unresolved && view_of(s,"open").link.destination.amount==Money(7000,cad));
            s.ingest("locked","D26/10/2026\nT-20\nPLocked\n^\n",qif::DateOrder::dmy);assign=command(s,app::TransferAction::from_draft,"locked-link");assign.origin=imports::Occurrence{"locked",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-locked",assign);s.commit("locked",0,"commit-locked");
            s.select_account(2);const auto visa_locked=s.committed_balance();s.select_account(1);
            const auto rewrite=retarget_of(s,"locked-link",allocation_fact(1,"locked","source-0",Money(-2000,cad),"2026-10-26",transfers::State::committed,true),1,"Fees","fees");
            const auto rewrite_preview=s.preview_transfer(rewrite);check(rewrite_preview.change.units()==0);
            const auto rewritten=s.apply_transfer("rewrite-committed",rewrite);check(rewritten.change.units()==0 && s.apply_transfer("rewrite-committed",rewrite).sequence==rewritten.sequence);
            const auto locked=view_of(s,"locked-link");check(locked.link.origin.state==transfers::State::removed && locked.link.destination.state==transfers::State::removed && locked.link.origin.amount==Money(-2000,cad));
            check(s.committed_entry({"locked",0}).current.draft.transaction.effective_total()==Money(-2000,cad) && s.committed_entry({"locked",0}).current.draft.transaction.allocations().at(0).category=="Fees");
            s.select_account(2);check(s.committed_balance()==visa_locked && !shows(s,"~transfer-locked-link-destination"));s.select_account(1);
            auto manual_cat=command(s,app::TransferAction::manual,"manual-cat");manual_cat.destination=2;manual_cat.amount=Money(-3000,cad);manual_cat.date="2026-10-26";manual_cat.payee="SYNTHETIC-TRANSFER-MARKER";s.apply_transfer("create-manual-cat",manual_cat);
            s.select_account(2);s.apply_transfer("accept-manual-cat-dest",command(s,app::TransferAction::accept,"manual-cat"));s.apply_transfer("commit-manual-cat-dest",command(s,app::TransferAction::commit_local,"manual-cat"));
            s.select_account(1);s.apply_transfer("accept-manual-cat-origin",command(s,app::TransferAction::accept,"manual-cat"));s.apply_transfer("commit-manual-cat-origin",command(s,app::TransferAction::commit_local,"manual-cat"));
            auto manual_category=retarget_of(s,"manual-cat",allocation_fact(1,"~transfer-manual-cat-origin","manual",Money(-3000,cad),"2026-10-26",transfers::State::committed,false),0,"Fees","fees");manual_category.left->manually_entered=true;
            const auto manual_preview=s.preview_transfer(manual_category);check(manual_preview.change.units()==0);
            const auto manual_receipt=s.apply_transfer("retarget-manual-cat",manual_category);check(manual_receipt.change.units()==0 && s.apply_transfer("retarget-manual-cat",manual_category).sequence==manual_receipt.sequence);
            const auto categorized=view_of(s,"manual-cat");check(categorized.link.origin_category=="Fees" && categorized.link.unresolved && categorized.link.origin.amount==Money(-3000,cad) && categorized.link.destination.amount==Money(3000,cad));s.verify();
            s.apply_transfer("create-manual",create(s));s.select_account(2);s.apply_transfer("accept-manual",command(s,app::TransferAction::accept));s.apply_transfer("commit-manual",command(s,app::TransferAction::commit_local));s.select_account(1);s.apply_transfer("accept-manual-origin",command(s,app::TransferAction::accept));s.apply_transfer("commit-manual-origin",command(s,app::TransferAction::commit_local));
            s.select_account(2);s.reassign_currency("scale-visa",usd);s.select_account(1);const auto preserved=view_of(s,"manual");check(preserved.link.preserved_amounts);
            auto repair=retarget_of(s,"manual",allocation_fact(1,"~transfer-manual-origin","manual",preserved.link.origin.amount,preserved.link.origin.date,transfers::State::committed,false),0,"Fees","fees");repair.left->manually_entered=true;
            message_has("two currencies",[&]{s.apply_transfer("repair-preserved",repair);});
            check(view_of(s,"manual").link.preserved_amounts && !view_of(s,"manual").link.unresolved && view_of(s,"manual").link.destination.amount==preserved.link.destination.amount);
            app::CategoryRequest rename{app::CategoryAction::rename,"fees","Card fees",{}};const auto renamed=s.preview_category_change(rename);s.apply_category_change("rename-fees",renamed.request,renamed.catalog_revision);
            s.verify();s.backup_to(d.path/"backup",password);}
        {auto restored=ImportStore::open(d.path/"backup",password);check(view_of(restored,"move-link").link.destination.endpoint.account==3);restored.select_account(3);check(restored.account().name=="Reserve");restored.verify();}
    }
    {
        Directory d;const auto path=d.path/"doc";
        auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(0,cad));s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.select_account(1);
        auto link_pair=[&](const std::string& link,const std::string& origin,const std::string& destination,const std::string& origin_qif,const std::string& destination_qif,const std::string& origin_date,const std::string& destination_date){
            s.select_account(1);s.ingest(origin,origin_qif,qif::DateOrder::dmy);s.select_account(2);s.ingest(destination,destination_qif,qif::DateOrder::dmy);s.select_account(1);
            s.apply_transfer("link-"+link,merge_of(s,link,allocation_fact(1,origin,"source-0",Money(-9500,cad),origin_date,transfers::State::draft,true),allocation_fact(2,destination,"source-0",Money(9500,cad),destination_date,transfers::State::draft,true),0,0));
            s.commit(origin,0,"commit-"+origin);s.select_account(2);s.commit(destination,0,"commit-"+destination);s.select_account(1);
        };
        link_pair("link-a","chk-a","visa-a","D02/11/2026\nT-95\nPChecking A\n^\n","D04/11/2026\nT95\nPVisa A\n^\n","2026-11-02","2026-11-04");
        message_has(s.bulk_supported()?"one at a time":"not supported",[&]{s.preview_bulk({app::ChangeKind::remove,{{"chk-a",0}},{},{}});});
        const auto checking_evidence=s.record_evidence({"chk-a",0});s.select_account(2);const auto visa_evidence=s.record_evidence({"visa-a",0});const auto visa_balance=s.committed_balance();s.select_account(1);
        const auto checking_balance=s.committed_balance();const auto visa_date=view_of(s,"link-a").link.destination.date;
        auto dated=s.committed_entry({"chk-a",0}).current.draft;dated.date="2026-11-20";
        app::ChangeRequest date_request{app::ChangeKind::correct,{"chk-a",0},dated,{}};
        const auto date_preview=s.preview_committed_change(date_request);check(date_preview.change.units()==0 && contains(date_preview.relationship,"remains"));
        const auto dated_receipt=apply_change(s,"date-a",date_request);check(dated_receipt.change.units()==0 && s.committed_balance()==checking_balance && view_of(s,"link-a").link.origin_date_corrected && view_of(s,"link-a").link.origin.date=="2026-11-20" && view_of(s,"link-a").link.destination.date==visa_date && view_of(s,"link-a").link.destination.amount==Money(9500,cad));
        apply_change(s,"undo-date-a",{app::ChangeKind::undo,{"chk-a",0},{},"date-a"});
        check(!view_of(s,"link-a").link.origin_date_corrected && !view_of(s,"link-a").link.unresolved && view_of(s,"link-a").link.origin.date=="2026-11-02" && s.committed_balance()==checking_balance);
        auto amount=s.committed_entry({"chk-a",0}).current.draft;auto parts=amount.transaction.allocations();parts[0].amount=Money(-9000,cad);amount.transaction=amount.transaction.corrected(Money(-9000,cad),parts);
        app::ChangeRequest amount_request{app::ChangeKind::correct,{"chk-a",0},amount,{}};
        const auto amount_preview=s.preview_committed_change(amount_request);check(amount_preview.change==Money(500,cad) && contains(amount_preview.relationship,"ends") && contains(amount_preview.relationship,"Visa"));
        const auto amount_receipt=apply_change(s,"amount-a",amount_request);check(amount_receipt.change==Money(500,cad) && s.committed_balance()==checking_balance+Money(500,cad));
        check(view_of(s,"link-a").link.unresolved && view_of(s,"link-a").link.destination.amount==Money(9500,cad) && view_of(s,"link-a").link.destination.date==visa_date);
        s.select_account(2);check(s.committed_balance()==visa_balance);s.select_account(1);
        apply_change(s,"undo-amount-a",{app::ChangeKind::undo,{"chk-a",0},{},"amount-a"});
        check(!view_of(s,"link-a").link.unresolved && view_of(s,"link-a").link.origin.amount==Money(-9500,cad) && s.committed_entry({"chk-a",0}).current.draft.transaction.effective_total()==Money(-9500,cad) && s.committed_balance()==checking_balance);
        apply_change(s,"amount-again",amount_request);check(view_of(s,"link-a").link.unresolved && s.committed_balance()==checking_balance+Money(500,cad));
        s.select_account(2);auto visa_edit=s.committed_entry({"visa-a",0}).current.draft;visa_edit.date="2026-12-01";
        const auto visa_preview=s.preview_committed_change({app::ChangeKind::correct,{"visa-a",0},visa_edit,{}});check(visa_preview.change.units()==0 && visa_preview.relationship.empty());
        apply_change(s,"visa-date",{app::ChangeKind::correct,{"visa-a",0},visa_edit,{}});check(s.committed_balance()==visa_balance && s.committed_entry({"visa-a",0}).current.draft.date=="2026-12-01");
        s.select_account(1);message_has("Newer transaction",[&]{s.preview_committed_change({app::ChangeKind::undo,{"chk-a",0},{},"amount-again"});});
        auto reused=s.committed_entry({"chk-a",0}).current.draft;reused.payee="Different synthetic";
        const auto reused_preview=s.preview_committed_change({app::ChangeKind::correct,{"chk-a",0},reused,{}});
        message_has("reused",[&]{s.apply_committed_change("amount-again",{app::ChangeKind::correct,{"chk-a",0},reused,{}},reused_preview.expected,reused_preview.preview_sequence);});
        check(s.record_evidence({"chk-a",0})==checking_evidence);s.select_account(2);check(s.record_evidence({"visa-a",0})==visa_evidence);s.select_account(1);
        link_pair("link-b","chk-b","visa-b","D06/11/2026\nT-95\nPChecking B\n^\n","D08/11/2026\nT95\nPVisa B\n^\n","2026-11-06","2026-11-08");
        const auto before_removal=s.committed_balance();s.select_account(2);const auto visa_before_removal=s.committed_balance();s.select_account(1);
        const auto removal_preview=s.preview_committed_change({app::ChangeKind::remove,{"chk-b",0},{},{}});check(removal_preview.change==Money(9500,cad) && contains(removal_preview.relationship,"ends") && contains(removal_preview.relationship,"Visa"));
        const auto removed=apply_change(s,"remove-b",{app::ChangeKind::remove,{"chk-b",0},{},{}});check(removed.change==Money(9500,cad) && s.committed_balance()==before_removal+Money(9500,cad) && view_of(s,"link-b").link.unresolved && view_of(s,"link-b").link.destination.amount==Money(9500,cad));
        s.select_account(2);check(s.committed_balance()==visa_before_removal && s.committed_entry({"visa-b",0}).current.draft.transaction.effective_total()==Money(9500,cad));s.select_account(1);
        apply_change(s,"undo-remove-b",{app::ChangeKind::undo,{"chk-b",0},{},"remove-b"});
        check(!view_of(s,"link-b").link.unresolved && view_of(s,"link-b").link.origin.state==transfers::State::committed && view_of(s,"link-b").link.destination.state==transfers::State::committed && s.committed_balance()==before_removal && s.committed_entry({"chk-b",0}).current.active);
        s.select_account(2);const auto visa_generated=s.committed_balance();s.select_account(1);
        s.ingest("chk-c","D12/11/2026\nT-40\nPGenerated C\n^\n",qif::DateOrder::dmy);auto assign=command(s,app::TransferAction::from_draft,"link-c");assign.origin=imports::Occurrence{"chk-c",0};assign.allocation="source-0";assign.destination=2;s.apply_transfer("assign-c",assign);s.commit("chk-c",0,"commit-c");
        const auto generated_preview=s.preview_committed_change({app::ChangeKind::remove,{"chk-c",0},{},{}});check(contains(generated_preview.relationship,"generated") && contains(generated_preview.relationship,"removed") && generated_preview.change==Money(4000,cad));
        apply_change(s,"remove-c",{app::ChangeKind::remove,{"chk-c",0},{},{}});
        check(view_of(s,"link-c").link.origin.state==transfers::State::removed && view_of(s,"link-c").link.destination.state==transfers::State::removed);
        s.select_account(2);check(s.committed_balance()==visa_generated && !shows(s,"~transfer-link-c-destination"));s.select_account(1);s.verify();
    }
}
void no_leaks(const fs::path& path){for(const auto& entry:fs::directory_iterator(path))if(entry.is_regular_file()){std::ifstream stream(entry.path(),std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(stream),{}};check(!bytes.starts_with("SQLite format 3") && bytes.find(password)==bytes.npos && bytes.find("SYNTHETIC-TRANSFER-MARKER")==bytes.npos);}}
void fault_setup(const fs::path& path,int mode){
 auto s=ImportStore::create(path,password,"Checking",cad,"2026-10-26");s.create_account(2,"Visa",cad,"2026-10-26");s.opening(0,"2026-10-26",Money(20000,cad));s.select_account(2);s.opening(0,"2026-10-26",Money(0,cad));s.select_account(1);
 if(mode>=10){
  if(mode==12){
   s.ingest("left-row","D26/10/2026\nT-100\nPLeft\n^\n",qif::DateOrder::dmy);s.commit("left-row",0,"commit-left");
   s.select_account(2);s.ingest("right-row","D28/10/2026\nT100\nPRight\n^\n",qif::DateOrder::dmy);s.commit("right-row",0,"commit-right");return;
  }
  if(mode==13){
   s.ingest("origin-d","D02/11/2026\nT-100\nPCommitted generated\n^\n",qif::DateOrder::dmy);
   auto r=command(s,app::TransferAction::from_draft,"generated-d");r.origin=imports::Occurrence{"origin-d",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign-d",r);
   s.select_account(2);s.apply_transfer("accept-d",command(s,app::TransferAction::accept,"generated-d"));s.apply_transfer("commit-d",command(s,app::TransferAction::commit_local,"generated-d"));s.ingest("visa-d","D03/11/2026\nT100\nPExtra\n^\n",qif::DateOrder::dmy);return;
  }
  if(mode==14){
   s.ingest("locked","D26/10/2026\nT-20\nPLocked\n^\n",qif::DateOrder::dmy);
   auto r=command(s,app::TransferAction::from_draft,"locked-link");r.origin=imports::Occurrence{"locked",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign-locked",r);s.commit("locked",0,"commit-locked");return;
  }
  s.ingest("origin","D26/10/2026\nT-100\nPOrigin\n^\n",qif::DateOrder::dmy);
  auto r=command(s,app::TransferAction::from_draft,"generated-link");r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign",r);
  if(mode==10){s.select_account(2);s.ingest("visa-row","D28/10/2026\nT100\nPVisa\n^\n",qif::DateOrder::dmy);}
  return;
 }
 if(mode==0)return;
 if(mode==2 || mode==9){s.apply_transfer("create",create(s));return;}
 s.ingest("origin","D26/10/2026\nT-100\nPSYNTHETIC-TRANSFER-MARKER\nL[Old Visa export]\n^\nD26/10/2026\nT-5\nPDiscarded\n^\n",qif::DateOrder::dmy);
 if(mode==1)return;
 auto r=command(s,app::TransferAction::from_draft);r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("assign",r);
 s.select_account(2);s.apply_transfer("accept",command(s,app::TransferAction::accept));if(mode==3)return;
 s.apply_transfer("destination-commit",command(s,app::TransferAction::commit_local));if(mode==4 || mode==7)return;
 s.select_account(1);s.commit("origin",0,"origin-commit");s.select_account(2);s.ingest("bank","D28/10/2026\nT100\nPBank evidence\nL[Old Checking export]\n^\n",qif::DateOrder::dmy);if(mode==5)return;
 r=command(s,app::TransferAction::propose_match);r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(10000,cad),"2026-10-28"};s.apply_transfer("proposal",r);
}
void fault_execute(ImportStore& s,int mode,std::int64_t expected){
 s.select_account(mode==3 || mode==5 || mode==6 || mode==8 || mode==10 || mode==13?2:1);
 if(mode>=10){
  app::TransferRequest r{};r.expected_sequence=expected;r.kept="operation-kept";r.payee="Synthetic";
  if(mode==10){r.action=app::TransferAction::merge;r.link="generated-link";r.left=allocation_fact(2,"~transfer-generated-link-destination","generated",Money(10000,cad),"2026-10-26",transfers::State::draft,false,true);r.right=allocation_fact(2,"visa-row","source-0",Money(10000,cad),"2026-10-28",transfers::State::draft,true);r.batch_revision=0;r.right_revision=0;}
  else if(mode==11){r.action=app::TransferAction::retarget;r.link="generated-link";r.mode=0;r.category_path="Fees";r.left=allocation_fact(1,"origin","source-0",Money(-10000,cad),"2026-10-26",transfers::State::draft,true);r.batch_revision=0;}
  else if(mode==13){r.action=app::TransferAction::merge;r.link="generated-d";r.left=allocation_fact(2,"~transfer-generated-d-destination","generated",Money(10000,cad),"2026-11-02",transfers::State::committed,false,true);r.right=allocation_fact(2,"visa-d","source-0",Money(10000,cad),"2026-11-03",transfers::State::draft,true);r.batch_revision=0;r.right_revision=0;}
  else if(mode==14){r.action=app::TransferAction::retarget;r.link="locked-link";r.mode=0;r.category_path="Fees";r.left=allocation_fact(1,"locked","source-0",Money(-2000,cad),"2026-10-26",transfers::State::committed,true);r.batch_revision=1;}
  else{r.action=app::TransferAction::merge;r.link="merged";r.left=allocation_fact(1,"left-row","source-0",Money(-10000,cad),"2026-10-26",transfers::State::committed,true);r.right=allocation_fact(2,"right-row","source-0",Money(10000,cad),"2026-10-28",transfers::State::committed,true);r.batch_revision=1;r.right_revision=1;}
  s.apply_transfer("operation",r);return;
 }
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
void faults(bool crash,int first_mode=0){
 check(first_mode>=0 && first_mode<15);
 fault_vfs::install();
 for(int mode=first_mode;mode<15;++mode){Directory fixture;const auto seed=fixture.path/"seed";fault_setup(seed,mode);if(mode==7 || mode==8){auto store=ImportStore::open(seed,password);store.certify({});}std::int64_t expected;
 {auto s=ImportStore::open(seed,password);expected=s.transfer_sequence();}
 std::int64_t completed_sequence=0;
 int count=0;{Directory sample;const auto path=sample.path/"doc";fs::copy_file(seed,path);auto s=ImportStore::open(path,password);fault_vfs::arm(-1);fault_execute(s,mode,expected);count=fault_vfs::seen;fault_vfs::disarm();s.verify();completed_sequence=s.transfer_sequence();}check(count>0 && completed_sequence>=expected);
 for(int position=1;position<=count;++position){Directory d;const auto path=d.path/"doc";fs::copy_file(seed,path);
  if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);fault_execute(s,mode,expected);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));}
  else{{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{fault_execute(s,mode,expected);}catch(const std::exception&){}fault_vfs::disarm();}}
  {auto s=ImportStore::open(path,password);check(s.transfer_sequence()==expected || s.transfer_sequence()==completed_sequence);s.verify();fault_execute(s,mode,expected);s.verify();check(s.transfer_sequence()==completed_sequence);}no_leaks(d.path);
 }
 for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;const auto path=d.path/"doc";fs::copy_file(seed,path);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});fault_execute(s,mode,expected);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);auto s=ImportStore::open(path,password);check(s.transfer_sequence()==(phase==ImportPhase::after_commit?completed_sequence:expected));fault_execute(s,mode,expected);s.verify();}
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
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"";if(name=="basics")basics();else if(name=="imports")import_flow();else if(name=="management")management();else if(name=="splits")split_flow();else if(name=="qif")qif_destinations();else if(name=="manual_evidence")manual_evidence();else if(name=="io")faults(false,argc>2?std::stoi(argv[2]):0);else if(name=="crashes")faults(true,argc>2?std::stoi(argv[2]):0);else if(name=="validation")validation();else return 2;std::cout<<"Transfer store "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
