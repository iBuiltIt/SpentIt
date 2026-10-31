#include <spentit/diagnostics.hpp>
#include <cstdlib>
#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <cerrno>
#include <chrono>
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
void no_leaks(const fs::path& dir){for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file()){std::ifstream f(entry.path(),std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(f),{}};check(bytes.find(password)==bytes.npos && bytes.find("PRIVATE-SYNTHETIC-ACCOUNTS")==bytes.npos && bytes.find("GROUP-FAULT-MARKER")==bytes.npos && !bytes.starts_with("SQLite format 3"));}}
void exact_record_evidence(ImportStore& store,std::string_view batch){
    const auto parsed=store.parsed_source(batch);const auto bytes=store.evidence(batch);
    for(std::size_t i=0;i<parsed.records.size();++i){const auto& r=parsed.records[i];check(store.record_evidence({std::string(batch),i})==bytes.substr(r.begin,r.end-r.begin));}
    bool missing=false;try{store.record_evidence({std::string(batch),parsed.records.size()});}catch(const std::exception&){missing=true;}check(missing);
}
void group_benchmark() {
    Directory d;auto store=create(d.path/"benchmark");
    std::string bytes="!Account\nNSynthetic\nTBank\n^\n!Type:Bank\n";
    for(std::size_t i=0;i<19436;++i) {
        bytes+="D2026-01-01\nT-10.38\nPSynthetic\n";
        bytes+=(i==294 || i==570 || i==571 || i==893)?"SSynthetic\n":"LSynthetic\n";
        bytes+="Cc\n^\n";
    }
    auto start=std::chrono::steady_clock::now();
    const auto timing=[&](const char* phase){const auto now=std::chrono::steady_clock::now();std::cout<<phase<<" ms: "<<std::chrono::duration<double,std::milli>(now-start).count()<<std::endl;start=now;};
    app::OperationContext capture;
    store.prepare_import_group("benchmark",bytes,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Synthetic",cad,"2026-01-01"}}},capture);
    timing("Capture and initial group status");
    app::OperationContext resume;
    const auto group=store.resume_import_group("benchmark",resume);
    timing("Materialize and returned group status");
    check(group.blocked_records==4 && group.unresolved>=4);
    check(store.import_groups().at(0).blocked_records==4);
    timing("Reload group status");
    store.select_account(2);check(store.committed_balance()==Money(0,cad));
    store.abort_import_group("benchmark");
    timing("Abort");
}
void group_snapshot_invalidation() {
    Directory d;auto s=create(d.path/"snapshots");
    app::OperationContext capture;
    s.prepare_import_group("snapshot","!Type:Bank\nD2026-01-02\nT-10\nLGeneral\n^\n",qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Snapshot",cad,"2026-01-01"}}},capture);
    app::OperationContext resume;s.resume_import_group("snapshot",resume);s.select_account(2);
    const imports::Occurrence id{"snapshot-account-0",0};
    const auto snapshot=[&]{return s.import_groups().at(0);};
    check(snapshot().accounts[0].reviewed_change==Money(-1000,cad));
    s.select_account(1);check(snapshot().accounts[0].reviewed_change==Money(-1000,cad));s.select_account(2);
    auto edit=*s.row(id).draft;
    auto first=edit.transaction.allocations();first[0].amount=Money(-1100,cad);
    edit.transaction=edit.transaction.corrected(Money(-1100,cad),first);
    s.save(id,1,edit);
    check(snapshot().unresolved_transfers==0 && snapshot().accounts[0].reviewed_change==Money(-1100,cad));
    auto next=*s.row(id).draft;auto allocations=next.transaction.allocations();allocations[0].category="General";allocations[0].amount=Money(-1200,cad);
    next.transaction=next.transaction.corrected(Money(-1200,cad),allocations);
    s.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw ImportError(SQLITE_IOERR,"Synthetic pre-commit failure");});
    rejected([&]{s.save(id,2,next);});s.observer({});
    check(snapshot().unresolved_transfers==0 && snapshot().accounts[0].reviewed_change==Money(-1100,cad));
    s.save(id,2,next);
    check(snapshot().unresolved_transfers==0 && snapshot().accounts[0].reviewed_change==Money(-1200,cad));
    s.review_import_balance("snapshot",0,3,app::ReportedBalanceChoice::ignore,"2026-01-01");
    check(snapshot().unresolved==0);
    s.commit(id.batch,3,"snapshot-commit");check(snapshot().state==app::GroupState::started);
    s.verify();
}
void groups(){
    Directory d;const auto path=d.path/"group";
    const std::string bytes="!Option:AutoSwitch\n!Account\nNFirst synthetic\nTBank\nB25\n^\n!Type:Bank\nD1/1/2026\nTbad\nLFood/Class\n^\n!Account\nNSecond synthetic\nTCCard\nB-10\n^\n!Type:CCard\nD2/1/2026\nT-10\n^\n";
    const std::vector<app::ImportAccountMapping> mapping{{0,2,app::AccountCreation{2,"First synthetic",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Second synthetic",cad,"2026-01-01"}}};
    {
        auto s=create(path);app::OperationContext capture;auto group=s.prepare_import_group("group",bytes,qif::DateOrder::dmy,mapping,capture);
        check(group.state==app::GroupState::captured && group.repairs_total==1 && group.repairs_done==0 && s.accounts().size()==3);s.verify();
        auto changed=mapping;changed[0].creation->date="2026-01-02";app::OperationContext retry;rejected([&]{s.prepare_import_group("group",bytes,qif::DateOrder::dmy,changed,retry);});
        app::OperationContext cancelled;cancelled.request();bool was_cancelled=false;try{s.resume_import_group("group",cancelled);}catch(const app::OperationCancelled&){was_cancelled=true;}check(was_cancelled && s.import_groups()[0].state==app::GroupState::captured);
    }
    {
        auto s=ImportStore::open(path,password);app::OperationContext resume;auto group=s.resume_import_group("group",resume);check(group.state==app::GroupState::reviewing);s.verify();
        s.select_account(3);s.review_import_balance("group",1,1,app::ReportedBalanceChoice::zero,"2026-01-01");
        rejected([&]{s.commit("group-account-1",1,"too-soon");});check(s.committed_balance()==Money(0,cad));
        s.select_account(2);check(s.evidence("group-account-0")==bytes);rejected([&]{s.opening(0,"2026-01-01",Money(25,cad));});
        exact_record_evidence(s,"group-account-0");rejected([&]{s.record_evidence({"group-account-1",0});});
        app::RowView row=s.row({"group-account-0",0});check(row.source.repairable && !row.source.original_amount);
        imports::DraftRow repair{"2026-01-01","","",Transaction::repaired_import({},Money(2500,cad),{{"Food","",Money(2500,cad)}})};
        s.save(row.identity,1,repair);exact_record_evidence(s,"group-account-0");s.decide(row.identity,2,{imports::Disposition::include,{}});
        group=s.import_groups()[0];check(group.repairs_done==1 && group.repairs_total==1);
        s.review_import_balance("group",0,3,app::ReportedBalanceChoice::zero,"2026-01-01");check(s.account().opening.units()==0 && !s.account().opening_confirmed);
        check(s.import_groups()[0].unresolved==0);check(s.commit("group-account-0",3,"first-commit").after==Money(2500,cad));
        rejected([&]{s.abort_import_group("group");});s.select_account(3);
        s.review_import_balance("group",1,1,app::ReportedBalanceChoice::closing,"2026-01-01");check(s.import_groups()[0].unresolved==0);check(s.commit("group-account-1",1,"second-commit").after==Money(-1000,cad));s.verify();
        exact_record_evidence(s,"group-account-1");s.backup_to(d.path/"backup",password);
    }
    {auto restored=ImportStore::open(d.path/"backup",password);check(restored.import_groups()[0].state==app::GroupState::started);restored.select_account(2);exact_record_evidence(restored,"group-account-0");restored.select_account(3);exact_record_evidence(restored,"group-account-1");restored.verify();}
    {
        auto s=ImportStore::open(path,password);auto aborted=mapping;auto abort_bytes=bytes;
        abort_bytes.replace(abort_bytes.find("First synthetic"),15,"Abort first synthetic");abort_bytes.replace(abort_bytes.find("Second synthetic"),16,"Abort second synthetic");
        aborted[0].creation->name="Abort first synthetic";aborted[1].creation->name="Abort second synthetic";aborted[0].account=4;aborted[0].creation->identity=4;aborted[1].account=5;aborted[1].creation->identity=5;
        app::OperationContext capture;s.prepare_import_group("abort",abort_bytes,qif::DateOrder::dmy,aborted,capture);app::OperationContext resume;s.resume_import_group("abort",resume);
        s.select_account(4);imports::DraftRow repair{"2026-01-01","","",Transaction::repaired_import({},Money(2500,cad),{{"Food","",Money(2500,cad)}})};s.save({"abort-account-0",0},1,repair);
        s.abort_import_group("abort");check(s.account().identity==1);s.abort_import_group("abort");check(s.accounts().size()==3 && s.next_account_identity()==6);
        rejected([&]{s.select_account(4);});rejected([&]{s.create_account(4,"First synthetic",cad,"2026-01-01");});
        app::OperationContext retry;check(s.prepare_import_group("abort",abort_bytes,qif::DateOrder::dmy,aborted,retry).state==app::GroupState::aborted);s.verify();
    }
    {auto s=ImportStore::open(path,password);check(s.accounts().size()==3 && s.import_groups().size()==2);s.verify();}no_leaks(d.path);
}
void account_aliases_test() {
    for(bool repair:{false,true}) {
        Directory d;const auto path=d.path/"aliases";
        const std::string bytes="!Type:Cat\nNBank Export A\nE\n^\nNFees\nE\n^\n!Account\nNBank Export A\nTBank\n^\n!Type:Bank\nD2026-02-02\nT800\nL[Bank Export B]\n^\n!Account\nNBank Export B\nTBank\n^\n!Type:Bank\nD"+std::string(repair?"2026-02-03":"2026-02-02")+"\nT-802.25\nSbank export a\n$-800\nSFees\n$-2.25\n^\n";
        {auto s=create(path);s.create_account(2,"Original wallet",cad,"2026-01-01");s.create_account(3,"Original savings",cad,"2026-01-01");
         for(auto id:{2,3}){s.select_account(id);s.opening(0,"2026-01-01",Money(0,cad));}
         s.select_account(2);s.rename_account("rename-wallet","My wallet");
         app::OperationContext capture;s.prepare_import_group("aliases",bytes,qif::DateOrder::dmy,{{0,2,{}},{1,3,{}}},capture);
         check(s.resolve_account_name("Original wallet")==2 && s.resolve_account_name("bank export a")==2 && s.account().name=="My wallet");
         rejected([&]{s.create_account(4,"BANK EXPORT A",cad,"2026-01-01");});
         rejected([&]{s.rename_account("collision","Original savings");});
         rejected([&]{s.apply_category_change("collision-category",{app::CategoryAction::create,"alias-category","Bank Export A",{},CategoryKind::leaf},0);});
         app::OperationContext resume;s.resume_import_group("aliases",resume);
         check(s.transfers().size()==(repair?2:1));
         if(repair){
             const auto sheet=s.review_sheet();const auto question=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==app::ReviewKind::link && q.rows.size()>1;});check(question!=sheet.questions.end());
             app::ReviewAnswer link;link.question=question->key;link.generation=sheet.generation;link.action=app::ReviewAction::link;link.selected={{"aliases-account-0",0},{"aliases-account-1",0}};link.left_allocation="source-0";link.right_allocation="source-0";s.answer_review("link-delayed",link);
         }
         s.verify();}
        {auto s=ImportStore::open(path,password);check(s.resolve_account_name("Original wallet")==2 && s.resolve_account_name("BANK EXPORT A")==2);
         app::OperationContext resolve;s.resolve_import_transfers("aliases",resolve);s.select_account(2);check(s.import_groups()[0].unresolved_transfers==0);
         exact_record_evidence(s,"aliases-account-0");s.commit("aliases-account-0",1,"wallet-commit");
         check(s.category_catalog().categories.size()==1 && s.category_catalog().categories[0].name=="Fees");
         s.select_account(3);exact_record_evidence(s,"aliases-account-1");check(s.evidence("aliases-account-1")==bytes);s.commit("aliases-account-1",1,"savings-commit");check(s.committed_balance()==Money(-80225,cad));s.verify();s.backup_to(d.path/"backup",password);}
        {auto s=ImportStore::open(d.path/"backup",password);check(s.resolve_account_name("Bank Export A")==2);s.select_account(2);check(s.account().name=="My wallet" && s.committed_balance()==Money(80000,cad));s.verify();}
    }
}
void mapped_transfers() {
    const auto source=[](std::string first,std::string second){return "!Account\nNExport A\nTBank\n^\n!Type:Bank\n"+first+"!Account\nNExport B\nTBank\n^\n!Type:Bank\n"+second;};
    const std::string left="D2026-01-02\nT-12\nETransfer memo\nS[Export B]\n$-10\nSFees\n$-2\n^\n";
    const std::string right="D2026-01-02\nT10\nL[Export A]\n^\n";
    for(bool destination_first:{false,true}) for(int count:{1,3}) {
        std::string ls,rs;for(int n=0;n<count;++n){ls+=left;rs+=right;}
        Directory d;const auto path=d.path/"mapped";
        {auto s=create(path);s.create_account(2,"Different target A",cad,"2026-01-01");s.create_account(3,"Different target B",cad,"2026-01-01");
         for(auto account:{2,3}){s.select_account(account);s.opening(0,"2026-01-01",Money(0,cad));}
         app::OperationContext capture;s.prepare_import_group("mapped",source(ls,rs),qif::DateOrder::dmy,{{0,2,{}},{1,3,{}}},capture);
         app::OperationContext resume;const auto group=s.resume_import_group("mapped",resume);
         check(group.unresolved==0 && group.unresolved_transfers==0 && s.transfers().size()==static_cast<std::size_t>(count));
         s.select_account(2);exact_record_evidence(s,"mapped-account-0");check(s.evidence("mapped-account-0")==source(ls,rs));
         s.select_account(3);exact_record_evidence(s,"mapped-account-1");
         const auto sequence=s.transfer_sequence();app::OperationContext retry;s.resolve_import_transfers("mapped",retry);check(s.transfer_sequence()==sequence);
         s.select_account(2);check(s.projected_balance()==Money(-1200*count,cad) && s.committed_balance()==Money(0,cad));
         s.select_account(3);check(s.projected_balance()==Money(1000*count,cad) && s.committed_balance()==Money(0,cad));
         s.select_account(destination_first?3:2);s.commit(destination_first?"mapped-account-1":"mapped-account-0",1,"first");
         s.select_account(destination_first?2:3);check(s.committed_balance()==Money(0,cad));s.commit(destination_first?"mapped-account-0":"mapped-account-1",1,"second");
         s.verify();s.backup_to(d.path/"backup",password);}
        for(const auto& file:{path,d.path/"backup"}){auto s=ImportStore::open(file,password);s.select_account(2);check(s.committed_balance()==Money(-1200*count,cad) && s.ledger(false).size()==static_cast<std::size_t>(count));s.select_account(3);check(s.committed_balance()==Money(1000*count,cad) && s.ledger(false).size()==static_cast<std::size_t>(count));s.verify();}
    }
    // Ambiguous groups retain every imported occurrence and generate every explicit counterpart.
    {Directory d;auto s=create(d.path/"ambiguous");app::OperationContext capture;s.prepare_import_group("ambiguous",source(left+left,right+right+right),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export A",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Export B",cad,"2026-01-01"}}},capture);app::OperationContext resume;auto group=s.resume_import_group("ambiguous",resume);s.select_account(2);check(s.transfers().size()==5 && group.unresolved_transfers==0);const auto sheet=s.review_sheet();check(std::count_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return !q.waiting_link.empty();})==5);s.select_account(2);check(s.balance()==Money(-5400,cad));s.select_account(3);check(s.balance()==Money(5000,cad));const auto sequence=s.transfer_sequence();app::OperationContext retry;s.resolve_import_transfers("ambiguous",retry);check(s.transfer_sequence()==sequence);s.abort_import_group("ambiguous");s.verify();}
    {Directory d;auto s=create(d.path/"linked-candidate");app::OperationContext capture;s.prepare_import_group("linked",source(left+left,right),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export A",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Export B",cad,"2026-01-01"}}},capture);app::OperationContext resume;s.resume_import_group("linked",resume);s.select_account(2);
     const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==app::ReviewKind::link && q.rows.size()>1;});check(q!=sheet.questions.end());app::ReviewAnswer request;request.question=q->key;request.generation=sheet.generation;request.action=app::ReviewAction::link;request.selected={q->rows[0].identity,q->rows[1].identity};request.left_allocation="source-0";request.right_allocation="source-0";s.answer_review("chosen-operation",request);
     app::OperationContext remaining;s.resolve_import_transfers("linked",remaining);s.select_account(2);check(s.balance()==Money(-2400,cad));s.select_account(3);check(s.balance()==Money(2000,cad));const auto after=s.review_sheet();check(std::count_if(after.questions.begin(),after.questions.end(),[](const auto& q){return !q.waiting_link.empty();})==1);s.abort_import_group("linked");s.verify();}
    // Invalid destinations remain correction tasks. Valid explicit destinations
    // generate counterparts even with differing dates or unmatched amounts.
    for(int variant=0;variant<6;++variant) {
        Directory d;auto s=create(d.path/"unresolved");auto l=left,r=right;
        if(variant==0)r.clear();
        if(variant==1)l.replace(l.find("[Export B]"),10,"[Missing]");
        if(variant==2)l.replace(l.find("[Export B]"),10,"[Export A]");
        if(variant==5)r.replace(r.find("T10"),3,"T11");
        if(variant==3)r.replace(r.find("2026-01-02"),10,"2026-01-03");
        app::OperationContext capture;s.prepare_import_group("unresolved",source(l,r),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export A",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Export B",variant==4?jpy:cad,"2026-01-01"}}},capture);
        app::OperationContext resume;const auto group=s.resume_import_group("unresolved",resume);s.select_account(2);const std::size_t generated[]={1,1,1,2,0,2};check(s.transfers().size()==generated[variant]);
        const std::string expected[]={"No reciprocal draft", "absent from", "own account", "different date", "different currency", "exact opposite amount"};
        if(variant==1 || variant==2 || variant==4)check(group.blockers[0].reason.find(expected[variant])!=std::string::npos);else check(group.unresolved_transfers==0);
        s.select_account(2);check(s.committed_balance()==Money(0,cad));s.abort_import_group("unresolved");s.verify();
    }
    {Directory d;auto s=create(d.path/"cancel");
     app::OperationContext capture;s.prepare_import_group("cancel",source(left+left,right+right),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export A",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Export B",cad,"2026-01-01"}}},capture);
     app::OperationContext cancelled;cancelled.progress=[&](std::string_view phase){if(phase=="Mapped transfer pair staged")cancelled.request();};bool stopped=false;try{s.resume_import_group("cancel",cancelled);}catch(const app::OperationCancelled&){stopped=true;}s.select_account(2);check(stopped && s.transfers().empty() && s.import_groups()[0].state==app::GroupState::captured);s.verify();app::OperationContext retry;s.resume_import_group("cancel",retry);check(s.transfers().size()==2);s.abort_import_group("cancel");s.verify();}

    // Resolution after a repair uses current amounts and is restart-safe.
    {Directory d;
const auto path=d.path/"repair";
auto s=create(path);
auto bad=right;
bad.replace(bad.find("T10"),3,"Tbad");
app::OperationContext capture;
s.prepare_import_group("repair",source(left+left,bad+right),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export A",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Export B",cad,"2026-01-01"}}},capture);
app::OperationContext resume;
s.resume_import_group("repair",resume);s.select_account(2);
check(s.transfers().size()==3);
s.select_account(3);
imports::DraftRow edit{"2026-01-02","","",Transaction::repaired_import({},Money(1000,cad),{{"[Export A]","",Money(1000,cad),{},"source-0",0}})};
s.save({"repair-account-1",0},1,edit);
app::OperationContext pending_review;s.resolve_import_transfers("repair",pending_review);
check(s.transfers().size()==3); // Unincluded repaired record must not generate money.
s.decide({"repair-account-1",0},2,{imports::Disposition::include,{}});
app::OperationContext cancel;cancel.request();bool cancelled=false;
try{s.resolve_import_transfers("repair",cancel);}catch(const app::OperationCancelled&){cancelled=true;}
check(cancelled && s.transfers().size()==3);
s.review_import_balance("repair",0,1,app::ReportedBalanceChoice::ignore,"2026-01-01");
s.review_import_balance("repair",1,3,app::ReportedBalanceChoice::ignore,"2026-01-01");
app::OperationContext resolve;s.resolve_import_transfers("repair",resolve);
check(s.transfers().size()==4);
check(s.import_groups()[0].unresolved_transfers==0);
const auto linked_sequence=s.transfer_sequence();app::OperationContext again;s.resolve_import_transfers("repair",again);
check(s.transfer_sequence()==linked_sequence);
s.verify();
s.abort_import_group("repair");
check(s.accounts().size()==1);
s.verify();
}
}
void mapping_openings() {
    const std::string bytes="!Account\nNNew account\nTBank\nB100.00\n^\n!Type:Bank\nD2010-01-01\nT-10\nPValid\n^\nD2009-01-01\nTbad\nPRepair\n^\n";
    for(bool reported:{false,true}){
        Directory d;const auto path=d.path/"opening";const std::vector<app::ImportAccountMapping> mappings{{0,2,app::AccountCreation{2,"New account",cad,"2009-01-01"},reported}};
        {auto s=create(path);app::OperationContext capture;s.prepare_import_group("opening",bytes,qif::DateOrder::dmy,mappings,capture);s.select_account(2);check(s.account().opening==Money(reported?10000:0,cad) && s.account().opening_confirmed);s.verify();
         auto changed=mappings;changed[0].use_reported_opening=!reported;app::OperationContext retry;rejected([&]{s.prepare_import_group("opening",bytes,qif::DateOrder::dmy,changed,retry);});}
        {auto s=ImportStore::open(path,password);app::OperationContext resume;s.resume_import_group("opening",resume);s.select_account(2);check(s.import_groups()[0].accounts[0].balance_choice==(reported?app::ReportedBalanceChoice::fixed_reported:app::ReportedBalanceChoice::fixed_zero));
         imports::DraftRow repair{"2009-01-01","Repair","",Transaction::repaired_import({},Money(2000,cad),{{"","",Money(2000,cad)}})};s.save({"opening-account-0",1},1,repair);s.decide({"opening-account-0",1},2,{imports::Disposition::include,{}});check(s.import_groups()[0].unresolved==0);s.accept_all("accept");check(s.balance()==Money(reported?11000:1000,cad));s.verify();s.backup_to(d.path/"backup",password);}
        {auto s=ImportStore::open(d.path/"backup",password);s.select_account(2);check(s.account().opening==Money(reported?10000:0,cad));s.verify();}
    }
    {Directory d;auto s=create(d.path/"invalid");auto bad=bytes;bad.replace(bad.find("B100.00"),7,"Binvalid");app::OperationContext capture;bool invalid=false;try{s.prepare_import_group("invalid",bad,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"New account",cad,"1900-01-01"},true}},capture);}catch(const std::exception&){invalid=true;}check(invalid);check(s.accounts().size()==1 && !s.import_group_outcome("invalid"));s.verify();}
}
void mapping_opening_faults() {
    fault_vfs::install();
    const std::string bytes="!Account\nNNew account\nTBank\nB100.00\n^\n!Type:Bank\nD2009-01-01\nT-10\nPOPENING-CHOICE-SYNTHETIC-MARKER\n^\n";
    const std::vector<app::ImportAccountMapping> mappings{{0,2,app::AccountCreation{2,"New account",cad,"2009-01-01"},true}};
    for(int mode=0;mode<4;++mode){Directory d;const auto base=d.path/"base";
        {auto s=create(base);if(mode>0){app::OperationContext c;s.prepare_import_group("opening",bytes,qif::DateOrder::dmy,mappings,c);}if(mode>1){app::OperationContext c;s.resume_import_group("opening",c);}s.verify();}
        const auto apply=[&](ImportStore& s){app::OperationContext c;if(mode==0)s.prepare_import_group("opening",bytes,qif::DateOrder::dmy,mappings,c);else if(mode==1)s.resume_import_group("opening",c);else if(mode==2)s.accept_all("accept");else s.abort_import_group("opening");};
        const auto done=[&](ImportStore& s){const auto group=s.import_group_outcome("opening");return group && (mode==0 || (mode==1 && group->state==app::GroupState::reviewing) || (mode==2 && group->state==app::GroupState::started) || (mode==3 && group->state==app::GroupState::aborted));};
        const auto verify=[&](ImportStore& s){s.verify();if(mode==3)check(s.accounts().size()==1);else {s.select_account(2);check(s.account().opening==Money(10000,cad) && s.account().opening_confirmed);check(s.balance()==Money(mode==0?10000:9000,cad));}};
        int positions=0;
        for(int nth=1;nth<2000;++nth){const auto path=d.path/"trial";fs::copy_file(base,path,fs::copy_options::overwrite_existing);bool fired;
            {auto s=ImportStore::open(path,password);fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const std::exception&){}fired=fault_vfs::fired;fault_vfs::disarm();}
            {auto s=ImportStore::open(path,password);s.verify();apply(s);check(done(s));verify(s);s.backup_to(d.path/"backup",password);}
            {auto s=ImportStore::open(d.path/"backup",password);verify(s);}fs::remove(d.path/"backup");
            if(!fired){positions=nth-1;break;}
        }check(positions>0);
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){const auto path=d.path/"crash";fs::copy_file(base,path,fs::copy_options::overwrite_existing);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](auto at){if(at==phase)_exit(71);});apply(s);_exit(72);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71);auto s=ImportStore::open(path,password);check(done(s)==(phase==ImportPhase::after_commit));apply(s);verify(s);}
        std::ifstream file(base,std::ios::binary);const std::string raw{std::istreambuf_iterator<char>(file),{}};check(raw.find("OPENING-CHOICE-SYNTHETIC-MARKER")==std::string::npos);std::cout<<"Opening choice mode "<<mode<<": "<<positions<<" write/sync positions"<<std::endl;
    }
}
void group_categories(){
    Directory d;const auto path=d.path/"categories";
    const std::string bytes="!Option:AutoSwitch\n!Account\nNFirst\nTBank\nB1,190.00\n^\nNSecond\nTCCard\nB-20\n^\n!Clear:AutoSwitch\n!Type:Cat\nNTravel\nE\n^\nNTravel:Flights\nE\n^\nNTelephone/Cellular\nE\nDSource description\n^\nNUnused\nI\n^\nNLater only\nE\n^\n!Account\nNFirst\nTBank\n^\n!Type:Bank\nD2026-01-01\nT1,200.00\nPSTARTING BALANCE\rSynthetic continuation\rTnot-an-amount\n^\nD2026-01-02\nT-10\nSTravel:Flights\n$-15\nSTelephone/Cellular\n$5\n^\n!Account\nNSecond\nTCCard\n^\n!Type:CCard\nD2026-01-03\nT-20\nLLater only\n^\n";
    const std::vector<app::ImportAccountMapping> mappings{{0,2,app::AccountCreation{2,"First",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Second",cad,"2026-01-01"}}};
    {
        auto s=create(path);s.apply_category_change("existing",{app::CategoryAction::create,"travel","Travel",{},CategoryKind::leaf},0);
        s.apply_category_change("similar",{app::CategoryAction::create,"similar","Travel: Flight",{},CategoryKind::leaf},1);
        app::OperationContext capture;s.prepare_import_group("cat",bytes,qif::DateOrder::dmy,mappings,capture);
        check(s.category_catalog().categories.size()==2);s.verify();
    }
    {
        auto s=ImportStore::open(path,password);app::OperationContext resume;s.resume_import_group("cat",resume);
        s.select_account(2);check(s.row({"cat-account-0",1}).draft->transaction.allocations()[1].category=="Telephone/Cellular");
        check(s.category_catalog().categories.size()==2 && !s.row({"cat-account-0",1}).draft->transaction.allocations()[0].category_binding);
        s.backup_to(d.path/"pending-backup",password);s.abort_import_group("cat");s.abort_import_group("cat");check(s.category_catalog().categories.size()==2);s.verify();
    }
    {auto s=ImportStore::open(path,password);check(s.import_groups()[0].state==app::GroupState::aborted && s.category_catalog().categories.size()==2);}
    {
        auto s=ImportStore::open(d.path/"pending-backup",password);s.select_account(2);
        s.review_import_balance("cat",0,1,app::ReportedBalanceChoice::closing,"2026-01-01");s.review_import_balance("cat",1,1,app::ReportedBalanceChoice::zero,"2026-01-01");
        check(s.import_groups()[0].accounts[0].proposed_opening==Money(0,cad));
        check(s.commit("cat-account-0",1,"publish-cat").after==Money(119000,cad));check(s.category_catalog().categories.size()==6);
        check(s.commit("cat-account-0",1,"publish-cat").after==Money(119000,cad));
        const auto entry=s.committed_entry({"cat-account-0",1});const auto& allocation=entry.current.draft.transaction.allocations()[0];
        check(allocation.category_binding && allocation.category=="Travel: Flights" && allocation.category_owner==FieldOwnership::source);
        std::string category,unused;
        for(const auto& c:s.category_catalog().categories){if(c.name=="Later only")category=c.identity;if(c.name=="Unused")unused=c.identity;}
        check(!category.empty() && !unused.empty());
        rejected([&]{s.apply_category_change("cannot-delete-pending",{app::CategoryAction::erase,category,"",{},CategoryKind::leaf},s.category_catalog().revision);});
        s.apply_category_change("delete-unused",{app::CategoryAction::erase,unused,"",{},CategoryKind::leaf},s.category_catalog().revision);
        s.apply_category_change("rename-imported",{app::CategoryAction::rename,category,"Air travel",{},CategoryKind::leaf},s.category_catalog().revision);
        s.select_account(3);check(s.batches()[0].state==app::BatchState::pending);s.commit("cat-account-1",1,"publish-second");
        check(s.committed_entry({"cat-account-1",0}).current.draft.transaction.allocations()[0].category_binding->identity==category);
        auto edit=s.committed_entry({"cat-account-1",0}).current.draft;edit.memo="Edited memo";
        auto preview=s.preview_committed_change({app::ChangeKind::correct,{"cat-account-1",0},edit,{}});
        s.apply_committed_change_with_catalog("edit",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);
        s.verify();s.backup_to(d.path/"published-backup",password);
    }
    {auto s=ImportStore::open(d.path/"published-backup",password);check(s.category_catalog().categories.size()==6);s.verify();}
    // An unavailable existing name must roll back earlier creations in the same publication.
    {auto s=create(d.path/"archived");s.opening(0,"2026-01-01",Money(0,cad));
     s.apply_category_change("create-archived",{app::CategoryAction::create,"archived","Archived",{},CategoryKind::leaf},0);
     s.apply_category_change("archive",{app::CategoryAction::archive,"archived","",{},CategoryKind::leaf},1);
     app::OperationContext c;s.prepare_import_group("archived","!Type:Cat\nNNew pending\nE\n^\nNArchived\nE\n^\n",qif::DateOrder::dmy,{{0,1,{}}},c);app::OperationContext r;s.resume_import_group("archived",r);
     rejected([&]{s.commit("archived-account-0",1,"archive-conflict");});check(s.category_catalog().categories.size()==1 && s.import_groups()[0].state==app::GroupState::reviewing);s.verify();
     s.abort_import_group("archived");check(s.category_catalog().categories.size()==1);}
    // A category-only source uses the same publication boundary and supports unused names.
    {auto s=create(d.path/"only");s.opening(0,"2026-01-01",Money(0,cad));app::OperationContext c;s.prepare_import_group("only","!Type:Cat\nNUnused only\nE\n^\n",qif::DateOrder::dmy,{{0,1,{}}},c);app::OperationContext r;s.resume_import_group("only",r);check(s.category_catalog().categories.empty());s.commit("only-account-0",1,"only-commit");check(s.category_catalog().categories.size()==1);s.verify();}
    no_leaks(d.path);
}
void group_transfers(){
    for(bool destination_first:{false,true}) {
        Directory d;const auto path=d.path/"group-transfer";
        const std::string bytes="!Account\nNOrigin synthetic\nTBank\nB-10\n^\n!Type:Bank\nD1/1/2026\nT-10\nL[Old destination export]\n^\n!Account\nNDestination synthetic\nTCash\nB40\n^\n!Type:Cash\n";
        {
            auto s=create(path);const std::vector<app::ImportAccountMapping> mappings{{0,2,app::AccountCreation{2,"Origin synthetic",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Destination synthetic",cad,"2026-01-01"}}};
            app::OperationContext capture;s.prepare_import_group("transfers",bytes,qif::DateOrder::dmy,mappings,capture);app::OperationContext resume;s.resume_import_group("transfers",resume);
            s.review_import_balance("transfers",0,1,app::ReportedBalanceChoice::zero,"2026-01-01");s.review_import_balance("transfers",1,1,app::ReportedBalanceChoice::closing,"2026-01-01");
            s.select_account(2);app::TransferRequest request{};request.action=app::TransferAction::from_draft;request.link="linked";request.origin=imports::Occurrence{"transfers-account-0",0};request.allocation="source-0";request.destination=3;request.batch_revision=1;request.expected_sequence=s.transfer_sequence();s.apply_transfer("link",request);
            auto group=s.import_groups()[0];check(group.accounts[1].reviewed_change==Money(1000,cad) && group.accounts[1].balance_choice==app::ReportedBalanceChoice::unreviewed);
            s.select_account(3);request={};request.action=app::TransferAction::accept;request.link="linked";request.expected_sequence=s.transfer_sequence();s.apply_transfer("accept",request);
            request.action=app::TransferAction::commit_local;request.expected_sequence=s.transfer_sequence();rejected([&]{s.apply_transfer("stale-balance",request);});
            s.review_import_balance("transfers",1,1,app::ReportedBalanceChoice::closing,"2026-01-01");check(s.committed_balance()==Money(0,cad) && s.import_groups()[0].accounts[1].proposed_opening==Money(3000,cad));
            const auto commit_destination=[&]{s.select_account(3);request.action=app::TransferAction::commit_local;request.expected_sequence=s.transfer_sequence();s.apply_transfer("destination",request);check(s.committed_balance()==Money(4000,cad));};
            if(destination_first)commit_destination();
            s.select_account(2);check(s.commit("transfers-account-0",1,"origin").after==Money(-1000,cad));
            if(!destination_first)commit_destination();
            s.select_account(3);s.commit("transfers-account-1",1,"empty-account-review");s.verify();
        }
        {auto s=ImportStore::open(path,password);s.select_account(3);check(s.committed_balance()==Money(4000,cad));s.verify();}
    }
}
void group_balances(){
    struct Case {const char* balance;app::ReportedBalanceChoice choice;std::int64_t opening,after;};
    for(const auto& c:std::vector<Case>{{"100",app::ReportedBalanceChoice::opening,10000,11000},{"100",app::ReportedBalanceChoice::closing,9000,10000},{"-20",app::ReportedBalanceChoice::closing,-3000,-2000},{"invalid",app::ReportedBalanceChoice::ignore,0,1000}}) {
        Directory d;const auto path=d.path/"balance";
        {auto s=create(path);const std::string bytes=std::string("!Account\nNBalance synthetic\nTBank\nB")+c.balance+"\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n";
        app::OperationContext capture;s.prepare_import_group("balance",bytes,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Balance synthetic",cad,"2026-01-01"}}},capture);app::OperationContext resume;s.resume_import_group("balance",resume);
        s.review_import_balance("balance",0,1,c.choice,"2026-01-01");s.select_account(2);check(s.committed_balance().units()==0 && s.import_groups()[0].accounts[0].proposed_opening->units()==c.opening);check(s.commit("balance-account-0",1,"commit").after.units()==c.after);s.verify();}
        {auto s=ImportStore::open(path,password);s.select_account(2);check(s.account().opening.units()==c.opening && s.committed_balance().units()==c.after);}
    }
    for(const auto bad:{"1.001","92233720368547758.07"}) {
        Directory d;auto s=create(d.path/"invalid");const std::string bytes=std::string("!Account\nNBalance synthetic\nTBank\nB")+bad+"\n^\n!Type:Bank\nD1/1/2026\nT-1\n^\n";
        app::OperationContext capture;s.prepare_import_group("invalid",bytes,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Balance synthetic",cad,"2026-01-01"}}},capture);app::OperationContext resume;s.resume_import_group("invalid",resume);
        bool failed=false;try{s.review_import_balance("invalid",0,1,app::ReportedBalanceChoice::closing,"2026-01-01");}catch(const FinancialError&){failed=true;}check(failed);s.verify();
    }
    {
        Directory d;auto s=create(d.path/"existing");s.opening(0,"2026-01-01",Money(7500,cad));
        const std::string bytes="!Account\nNExisting source name\nTBank\nB100\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n";
        app::OperationContext capture;s.prepare_import_group("existing",bytes,qif::DateOrder::dmy,{{0,1,{}}},capture);app::OperationContext resume;s.resume_import_group("existing",resume);check(s.commit("existing-account-0",1,"commit").after==Money(8500,cad));check(s.account().opening==Money(7500,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"mixed");const std::string bytes="!Account\nNMixed synthetic\nTBank\nB92233720368547758.07\n^\n!Type:Bank\nD1/1/2026\nT92233720368547758.07\n^\nD2/1/2026\nT0.01\n^\nD3/1/2026\nT-0.01\n^\n";
        app::OperationContext capture;s.prepare_import_group("mixed",bytes,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Mixed synthetic",cad,"2026-01-01"}}},capture);app::OperationContext resume;s.resume_import_group("mixed",resume);check(s.import_groups()[0].accounts[0].reviewed_change==Money(INT64_MAX,cad));s.review_import_balance("mixed",0,1,app::ReportedBalanceChoice::zero,"2026-01-01");s.select_account(2);check(s.commit("mixed-account-0",1,"commit").after==Money(INT64_MAX,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"discard-after-start");std::string bytes;std::vector<app::ImportAccountMapping> mappings;
        for(int i=0;i<3;++i){const auto name="Synthetic "+std::to_string(i);bytes+="!Account\nN"+name+"\nTBank\nB10\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n";mappings.push_back({static_cast<std::size_t>(i),i+2,app::AccountCreation{i+2,name,cad,"2026-01-01"}});}
        app::OperationContext capture;s.prepare_import_group("three",bytes,qif::DateOrder::dmy,mappings,capture);app::OperationContext resume;s.resume_import_group("three",resume);for(int i=0;i<3;++i)s.review_import_balance("three",static_cast<std::size_t>(i),1,app::ReportedBalanceChoice::zero,"2026-01-01");
        s.select_account(2);s.commit("three-account-0",1,"first");s.select_account(3);s.discard("three-account-1",1);s.select_account(4);check(s.commit("three-account-2",1,"third").after==Money(1000,cad));s.verify();
    }
}
void group_duplicates_and_protection(){
    {
        Directory d;auto s=create(d.path/"duplicates");s.opening(0,"2026-01-01",Money(0,cad));
        const std::string bytes="!Account\nNRepeated source\nTBank\n^\n!Type:Bank\nD1/1/2026\nT10\n^\nD1/1/2026\nT10\n^\n";
        const std::vector<app::ImportAccountMapping> mapping{{0,1,{}}};app::OperationContext capture;s.prepare_import_group("first",bytes,qif::DateOrder::dmy,mapping,capture);app::OperationContext resume;s.resume_import_group("first",resume);check(s.commit("first-account-0",1,"first-commit").included==2);
        app::OperationContext again;s.prepare_import_group("second",bytes,qif::DateOrder::dmy,mapping,again);app::OperationContext retry;check(s.prepare_import_group("second",bytes,qif::DateOrder::dmy,mapping,retry).id=="second");app::OperationContext process;s.resume_import_group("second",process);
        check(s.row({"second-account-0",0}).decision.duplicate_of==imports::Occurrence{"first-account-0",0});check(s.row({"second-account-0",1}).decision.duplicate_of==imports::Occurrence{"first-account-0",1});
        s.decide({"second-account-0",1},1,{imports::Disposition::include,{}});const auto receipt=s.commit("second-account-0",2,"repeat-commit");check(receipt.included==1 && receipt.excluded==1 && receipt.after==Money(3000,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"swapped");std::string bytes;std::vector<app::ImportAccountMapping> mapping;
        for(int i=0;i<2;++i){const auto name="Mapped source "+std::to_string(i);bytes+="!Account\nN"+name+"\nTBank\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n";mapping.push_back({static_cast<std::size_t>(i),i+2,app::AccountCreation{i+2,name,cad,"2026-01-01"}});}
        app::OperationContext capture;s.prepare_import_group("first",bytes,qif::DateOrder::dmy,mapping,capture);app::OperationContext resume;s.resume_import_group("first",resume);for(std::size_t i=0;i<2;++i)s.review_import_balance("first",i,1,app::ReportedBalanceChoice::zero,"2026-01-01");s.select_account(2);s.commit("first-account-0",1,"commit-0");s.select_account(3);s.commit("first-account-1",1,"commit-1");
        app::OperationContext again;rejected([&]{s.prepare_import_group("swap",bytes,qif::DateOrder::dmy,{{0,3,{}},{1,2,{}}},again);});check(!s.import_group_outcome("swap"));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"single");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Destination",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        const std::string bytes="D1/1/2026\nT-10\nL[Old destination export]\n^\nD2/1/2026\nTbad\n^\n";s.ingest("single",bytes,qif::DateOrder::dmy);
        app::TransferRequest request{};request.action=app::TransferAction::from_draft;request.link="single-link";request.origin=imports::Occurrence{"single",0};request.allocation="source-0";request.destination=2;request.expected_sequence=s.transfer_sequence();s.apply_transfer("assign",request);s.select_account(2);request={};request.action=app::TransferAction::accept;request.link="single-link";request.expected_sequence=s.transfer_sequence();s.apply_transfer("accept",request);request.action=app::TransferAction::commit_local;request.expected_sequence=s.transfer_sequence();rejected([&]{s.apply_transfer("blocked",request);});check(s.committed_balance()==Money(0,cad));
        s.select_account(1);imports::DraftRow repair{"2026-01-02","","",Transaction::repaired_import({},Money(100,cad),{{"","",Money(100,cad)}})};s.save({"single",1},0,repair);s.discard("single",1);check(s.evidence("single")==bytes && s.ledger(true).empty());s.verify();
    }
}
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
void association_validation() {
    for(const char* sql:{
        "DELETE FROM source_associations WHERE source_ordinal=0",
        "INSERT INTO source_associations VALUES('evidence',1,'evidence',0)",
        "UPDATE source_associations SET ledger_ordinal=1 WHERE source_ordinal=0"}) {
        Directory d;const auto path=d.path/"association-corruption";
        {auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));s.ingest("evidence",input+input,qif::DateOrder::dmy);s.commit("evidence",0,"publish");s.verify();}
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,"DROP TRIGGER source_associations_immutable;DROP TRIGGER source_associations_keep;DROP INDEX source_membership",nullptr,nullptr,nullptr)==SQLITE_OK);
        check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        rejected([&]{ImportStore::open(path,password);});no_leaks(d.path);
    }
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
        // The raw connection does not register spentit_account_command(). Drop the triggers that call it so this corruption can be planted. open() must still reject the document.
        check(sqlite3_exec(db,"DROP TRIGGER account_creation_retained;DROP TRIGGER account_creation_immutable;DROP TRIGGER batch_account_immutable;DROP TRIGGER source_immutable;DROP TRIGGER account_name_command;DROP TRIGGER frozen_opening;DROP TRIGGER account_definition_command",nullptr,nullptr,nullptr)==SQLITE_OK);
        check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        rejected([&]{ImportStore::open(path,password);});no_leaks(d.path);
    }
}
std::string group_fault_source(int kind){
    if(kind>=7){const std::string l="D2026-01-01\nT-10\nLFault second\n^\n",r="D2026-01-01\nT10\nLFault first\n^\n";auto last=r;if(kind==8)last.replace(last.find("T10"),3,"Tbad");return "!Account\nNFault first\nTBank\n^\n!Type:Bank\n"+l+l+"!Account\nNFault second\nTBank\n^\n!Type:Bank\n"+last+r;}

    const auto first=kind==5?"-10":(kind==3 || kind==6)?"bad":"10";
    return std::string("!Type:Cat\nNFault category\nE\n^\nNUnused fault category\nI\n^\n!Account\nNFault first\nTBank\nB")+(kind==5?"-10":"100")+"\n^\n!Type:Bank\nD1/1/2026\nT"+first+"\nPGROUP-FAULT-MARKER\n"+(kind==5?"L[Old destination export]\n":"LFault category\n")+"^\n!Account\nNFault second\nTCash\nB"+(kind==5?"40":"-10")+"\n^\n!Type:Cash\n"+(kind==5?"":"D2/1/2026\nT-10\n^\n");
}
std::vector<app::ImportAccountMapping> group_fault_mapping(){return {{0,2,app::AccountCreation{2,"Fault first",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Fault second",cad,"2026-01-01"}}};}
imports::DraftRow group_fault_repair(){return {"2026-01-01","GROUP-FAULT-MARKER","",Transaction::repaired_import({},Money(1000,cad),{{"","",Money(1000,cad)}})};}
void group_fault_setup(const fs::path& path,int kind){
    auto s=create(path);if(kind==0)return;app::OperationContext capture;s.prepare_import_group("fault",group_fault_source(kind),qif::DateOrder::dmy,group_fault_mapping(),capture);if(kind==1 || kind==7)return;
    app::OperationContext resume;s.resume_import_group("fault",resume);if(kind==2 || kind==6 || kind==9)return;
    if(kind==8){s.select_account(3);imports::DraftRow edit{"2026-01-01","","",Transaction::repaired_import({},Money(1000,cad),{{"Fault first","",Money(1000,cad),{},"source-0",0}})};s.save({"fault-account-1",0},1,edit);s.decide({"fault-account-1",0},2,{imports::Disposition::include,{}});return;}
    if(kind==3){s.select_account(2);s.save({"fault-account-0",0},1,group_fault_repair());return;}
    if(kind==5){s.select_account(2);app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="fault-link";r.origin=imports::Occurrence{"fault-account-0",0};r.allocation="source-0";r.destination=3;r.batch_revision=1;r.expected_sequence=s.transfer_sequence();s.apply_transfer("link",r);s.select_account(3);r={};r.action=app::TransferAction::accept;r.link="fault-link";r.expected_sequence=s.transfer_sequence();s.apply_transfer("accept",r);}
    s.review_import_balance("fault",0,1,kind==5?app::ReportedBalanceChoice::zero:app::ReportedBalanceChoice::closing,"2026-01-01");s.review_import_balance("fault",1,1,kind==5?app::ReportedBalanceChoice::closing:app::ReportedBalanceChoice::zero,"2026-01-01");
}
void group_fault_execute(ImportStore& s,int kind){
    app::OperationContext context;
    if(kind==0)s.prepare_import_group("fault",group_fault_source(kind),qif::DateOrder::dmy,group_fault_mapping(),context);
    else if(kind==1 || kind==7)s.resume_import_group("fault",context);
    else if(kind==8)s.resolve_import_transfers("fault",context);
    else if(kind==9)s.abort_import_group("fault");
    else if(kind==2)s.review_import_balance("fault",0,1,app::ReportedBalanceChoice::closing,"2026-01-01");
    else if(kind==3)s.abort_import_group("fault");
    else if(kind==4){s.select_account(2);s.commit("fault-account-0",1,"publish");}
    else if(kind==5){s.select_account(3);app::TransferRequest r{};r.action=app::TransferAction::commit_local;r.link="fault-link";r.expected_sequence=2;s.apply_transfer("publish-transfer",r);}
    else {s.select_account(2);if(s.batches()[0].revision==1)s.save({"fault-account-0",0},1,group_fault_repair());}
}
void group_validation(){
    bool restored_checked=false;
    struct Corruption {int setup_kind;bool execute;const char* sql;};
    for(const auto& item:std::vector<Corruption>{
        {4,false,"DROP TRIGGER group_mapping_immutable;UPDATE import_group_accounts SET account_id=1 WHERE source_account=0"},
        {7,true,"DROP TRIGGER transfer_versions_immutable;UPDATE transfer_versions SET payload=x'00'"},
        {7,true,"DROP TRIGGER transfer_events_immutable;UPDATE transfer_events SET request=x'00'"},
        {7,true,"DROP TRIGGER transfer_events_immutable;UPDATE transfer_events SET local_change=1"},
        {7,true,"DROP TRIGGER transfer_heads_keep;DELETE FROM transfer_heads"},
        {4,false,"DROP TRIGGER balance_events_immutable;UPDATE import_balance_events SET reviewed_change=reviewed_change+1 WHERE source_account=0"},
        {4,false,"DROP TRIGGER balance_events_immutable;UPDATE import_balance_events SET opening=opening+1 WHERE source_account=0"},
        {4,false,"DROP TRIGGER balance_history;UPDATE import_group_accounts SET opening=opening+1 WHERE source_account=0"},
        {4,true,"DROP TRIGGER group_events_keep;DELETE FROM import_group_events WHERE state=1"},
        {4,true,"DROP TRIGGER group_transition;DROP TRIGGER group_transition_history;UPDATE import_groups SET publication='missing-receipt'"},
        {4,false,"INSERT INTO retired_import_accounts(account_id,group_id) VALUES(2,'fault')"},
        {4,true,"DROP TRIGGER category_versions_immutable;DROP TRIGGER category_operations_immutable;UPDATE category_versions SET name='Forged unused category' WHERE id='qif-cat-fault-1';UPDATE category_operations SET name='Forged unused category' WHERE category='qif-cat-fault-1'"},
        {3,true,"DROP TRIGGER account_name_command;DROP TRIGGER frozen_opening;DROP TRIGGER account_definition_command;UPDATE account SET name='' WHERE singleton=2"}}){
        Directory d;const auto path=d.path/"tampered-group";group_fault_setup(path,item.setup_kind);
        {auto s=ImportStore::open(path,password);if(item.execute)group_fault_execute(s,item.setup_kind);s.verify();if(!restored_checked)s.backup_to(d.path/"restore",password);}
        if(!restored_checked){auto restored=ImportStore::open(d.path/"restore",password);restored.verify();restored.select_account(2);check(restored.evidence("fault-account-0").find("GROUP-FAULT-MARKER")!=std::string::npos);restored_checked=true;}
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        // Authenticated synthetic corruption bypasses only the relevant immutability guards.
        check(sqlite3_exec(db,item.sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        rejected([&]{ImportStore::open(path,password);});no_leaks(d.path);
    }
}
bool group_fault_done(ImportStore& s,int kind){
    const auto saved=s.import_group_outcome("fault");
    if(kind==0){check(s.accounts().size()==(saved?3:1));return saved.has_value();}
    check(saved.has_value());
    if(kind==1 || kind==7){const bool done=saved->state==app::GroupState::reviewing;if(kind==7){s.select_account(2);check(s.transfers().size()==(done?2:0));}return done;}
    if(kind==8){s.select_account(2);const auto count=s.transfers().size();check(count==3 || count==4);return count==4;}
    if(kind==9){const bool done=saved->state==app::GroupState::aborted;check(s.accounts().size()==(done?1:3));return done;}
    if(kind==2)return saved->accounts[0].balance_choice!=app::ReportedBalanceChoice::unreviewed;
    if(kind==3){const bool done=saved->state==app::GroupState::aborted;check(s.accounts().size()==(done?1:3));check(s.category_catalog().categories.empty());return done;}
    if(kind==4 || kind==5){s.select_account(kind==4?2:3);const bool done=kind==4?s.outcome("publish").has_value():s.transfer_outcome("publish-transfer").has_value();check((saved->state==app::GroupState::started)==done);check(s.category_catalog().categories.size()==(done?2:0));check(s.account().opening.units()==(done?(kind==4?9000:3000):0));return done;}
    s.select_account(2);const bool done=s.row({"fault-account-0",0}).draft.has_value();check(s.batches()[0].revision==(done?2:1));return done;
}
void group_fault_recovered(const fs::path& path,int kind){
    auto s=ImportStore::open(path,password);(void)group_fault_done(s,kind);group_fault_execute(s,kind);s.verify();const auto group=s.import_groups()[0];
    if(kind==0)check(group.state==app::GroupState::captured && s.accounts().size()==3);
    if(kind==1 || kind==7)check(group.state==app::GroupState::reviewing && group.accounts.size()==2);
    if(kind==8){s.select_account(2);check(s.transfers().size()==4);check(s.balance()==Money(-4000,cad));s.select_account(3);check(s.balance()==Money(4000,cad));}
    if(kind==9)check(group.state==app::GroupState::aborted && s.accounts().size()==1);
    if(kind==2)check(group.accounts[0].proposed_opening==Money(9000,cad));
    if(kind==3)check(group.state==app::GroupState::aborted && s.accounts().size()==1);
    if(kind==4)check(s.committed_balance()==Money(10000,cad));
    if(kind==5)check(s.committed_balance()==Money(4000,cad));
    if(kind==6)check(s.row({"fault-account-0",0}).draft->transaction.effective_total()==Money(1000,cad));
}
void group_faults(bool crash,int first=0){
    fault_vfs::install();
    for(int kind=first;kind<10;++kind){
        std::cout<<"Mapped fault sweep kind "<<kind<<std::endl;
        int count=0;{Directory d;group_fault_setup(d.path/"doc",kind);auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);group_fault_execute(s,kind);count=fault_vfs::seen;fault_vfs::disarm();}check(count>0);
        for(int position=1;position<=count;++position){Directory d;const auto path=d.path/"doc";group_fault_setup(path,kind);
            if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);group_fault_execute(s,kind);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));}
            else{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{group_fault_execute(s,kind);}catch(const ImportError&){}fault_vfs::disarm();}
            group_fault_recovered(path,kind);no_leaks(d.path);
        }
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;const auto path=d.path/"doc";group_fault_setup(path,kind);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});group_fault_execute(s,kind);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);{auto s=ImportStore::open(path,password);check(group_fault_done(s,kind)==(phase==ImportPhase::after_commit));}group_fault_recovered(path,kind);}
        std::cout<<"Group operation "<<kind<<": "<<count<<" write/sync positions"<<std::endl;
    }
}
// Run the same history across rule-enabled and rule-disabled current formats.
// Every crash case resumes the entire workflow, not only the interrupted command.
constexpr int combined_steps=15;
const std::string combined_source="!Account\nNCombined synthetic\nTBank\nB100\n^\n!Type:Bank\nD1/1/2026\nTinvalid\nPGROUP-FAULT-MARKER\nLFood\n^\n";
const imports::Occurrence combined_id{"combined-account-0",0};
std::int64_t revision(ImportStore& s,std::string_view id){for(const auto& b:s.batches())if(b.id==id)return b.revision;throw std::runtime_error("Missing combined batch");}
bool combined_done(ImportStore& s,int step){
    s.select_account(step==0?1:2);
    const auto group=s.import_group_outcome("combined");
    switch(step){
    case 0:return group.has_value();
    case 1:return group->state!=app::GroupState::captured;
    case 2:return s.row(combined_id).draft.has_value();
    case 3:return s.row(combined_id).decision.disposition==imports::Disposition::include;
    case 4:return s.category_change_outcome("combined-category").has_value();
    case 5:return !s.rules_supported() || s.rule_save_outcome("combined-rules").has_value();
    case 6:return !s.rules_supported() || s.rule_apply_outcome("combined-apply").has_value();
    case 7:return group->accounts[0].balance_choice!=app::ReportedBalanceChoice::unreviewed;
    case 8:return s.outcome("combined-commit").has_value();
    case 9:return s.committed_change_outcome("combined-correct").has_value();
    case 10:return s.committed_change_outcome("combined-undo").has_value();
    case 11:for(const auto& b:s.batches())if(b.id=="overlap")return true;return false;
    case 12:return s.row({"overlap",0}).decision.disposition==imports::Disposition::duplicate;
    case 13:return s.outcome("combined-overlap").has_value();
    case 14:return s.category_change_outcome("combined-rename").has_value();
    }return false;
}
void combined_step(ImportStore& s,int step){
    s.select_account(step==0?1:2);app::OperationContext context;
    switch(step){
    case 0:s.prepare_import_group("combined",combined_source,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Combined synthetic",cad,"2026-01-01"}}},context);break;
    case 1:s.resume_import_group("combined",context);break;
    case 2:s.save(combined_id,revision(s,combined_id.batch),{"2026-01-01","GROUP-FAULT-MARKER","",Transaction::repaired_import({},Money(1000,cad),{{"Food","",Money(1000,cad),{},"source-0",0}})});break;
    case 3:s.decide(combined_id,revision(s,combined_id.batch),{imports::Disposition::include,{}});break;
    case 4:{app::CategoryRequest request{app::CategoryAction::create,"combined-food","Food",{},CategoryKind::leaf};const auto p=s.preview_category_change(request);s.apply_category_change("combined-category",p.request,p.catalog_revision);break;}
    case 5:if(s.rules_supported()){app::RuleDefinitionImage rule;rule.identity="combined-rule";rule.label="Synthetic classification";rule.enabled=true;rule.scope=rules::Scope::transaction;rule.input=rules::InputField::payee;rule.expression="GROUP-FAULT-MARKER";rule.actions.category="combined-food";s.save_rules("combined-rules",{{rule}},s.rule_image().revision,context);}break;
    case 6:if(s.rules_supported()){check(s.preview_pending_rules({combined_id.batch,revision(s,combined_id.batch),std::nullopt,false},context).changed==0);auto p=s.preview_pending_rules({combined_id.batch,revision(s,combined_id.batch),std::nullopt,true},context);check(p.changed==1);s.apply_pending_rules("combined-apply",{p.batch,p.batch_revision,p.ruleset_revision,p.catalog_revision,p.replace_manual},context);}break;
    case 7:s.review_import_balance("combined",0,revision(s,combined_id.batch),app::ReportedBalanceChoice::closing,"2026-01-01");break;
    case 8:s.commit(combined_id.batch,revision(s,combined_id.batch),"combined-commit");break;
    case 9:case 10:{app::ChangeRequest request{step==9?app::ChangeKind::correct:app::ChangeKind::undo,combined_id,{},step==10?"combined-correct":""};if(step==9){auto draft=s.committed_entry(combined_id).current.draft;auto a=draft.transaction.allocations();a[0].amount=Money(1200,cad);draft.transaction=draft.transaction.corrected(Money(1200,cad),a);request.correction=draft;}const auto p=s.preview_committed_change(request);s.apply_committed_change_with_catalog(step==9?"combined-correct":"combined-undo",p.request,p.expected,p.preview_sequence,p.catalog_revision);break;}
    case 11:s.ingest("overlap","D1/1/2026\nT10\nPGROUP-FAULT-MARKER\nLFood\n^\n",qif::DateOrder::dmy);break;
    case 12:s.decide({"overlap",0},revision(s,"overlap"),{imports::Disposition::duplicate,combined_id});break;
    case 13:s.commit("overlap",revision(s,"overlap"),"combined-overlap");break;
    case 14:{app::CategoryRequest request{app::CategoryAction::rename,"combined-food","Meals",{}};const auto p=s.preview_category_change(request);s.apply_category_change("combined-rename",p.request,p.catalog_revision);break;}
    }
}
void combined_verify(ImportStore& s){
    s.select_account(2);s.verify();exact_record_evidence(s,combined_id.batch);check(s.committed_balance()==Money(10000,cad));check(s.evidence(combined_id.batch)==combined_source);check(!s.committed_entry(combined_id).current.draft.transaction.original_imported_amount());check(s.committed_entry(combined_id).current.draft.transaction.effective_total()==Money(1000,cad));check(s.outcome("combined-overlap")->included==0 && s.outcome("combined-overlap")->excluded==1);check(s.ledger(false).size()==1);check(s.committed_changes().size()==2);
    if(s.rules_supported()){const auto binding=s.committed_entry(combined_id).current.draft.transaction.allocations()[0].category_binding;check(binding && binding->identity=="combined-food");}
}
template<class F>void isolated_combined(F work){
    // The coordinator never opens SQLCipher. Each scenario starts without the
    // previous scenario's ASan allocator/quarantine state before it forks a
    // deliberate crash child. No sanitizer checks or durability are disabled.
    const auto child=fork();check(child>=0);
    if(child==0){try{work();_exit(0);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;_exit(1);}}
    int status=0;pid_t waited;do{waited=waitpid(child,&status,0);}while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=0)throw std::runtime_error("Isolated combined scenario failed, status "+std::to_string(status));
}
void combined_case(int interrupted,ImportPhase phase){
        Directory d;const auto path=d.path/"combined";bool supported=false;
        {auto s=create(path);supported=s.rules_supported();s.opening(0,"2026-01-01",Money(0,cad));for(int step=0;step<interrupted;++step)combined_step(s,step);}
        if(interrupted>=0 && (!supported && (interrupted==5 || interrupted==6)))return;
        if(interrupted>=0){
            const auto child=fork();check(child>=0);
            if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});combined_step(s,interrupted);_exit(0);}
            int status=0;pid_t waited;do{waited=waitpid(child,&status,0);}while(waited<0 && errno==EINTR);
            if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=72)throw std::runtime_error("Combined child exit mismatch at step "+std::to_string(interrupted)+", phase "+std::to_string(static_cast<int>(phase))+", status "+std::to_string(status));
            auto s=ImportStore::open(path,password);
            if(combined_done(s,interrupted)!=(phase==ImportPhase::after_commit))throw std::runtime_error("Combined durable boundary mismatch at step "+std::to_string(interrupted)+", phase "+std::to_string(static_cast<int>(phase)));
        }
        {auto s=ImportStore::open(path,password);for(int step=std::max(0,interrupted);step<combined_steps;++step)if(!combined_done(s,step))combined_step(s,step);combined_verify(s);s.backup_to(d.path/"restored",password);}
        {auto restored=ImportStore::open(d.path/"restored",password);combined_verify(restored);}
        no_leaks(d.path);
}
void combined_recovery(){
    for(int interrupted=-1;interrupted<combined_steps;++interrupted)
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit})
            isolated_combined([&]{combined_case(interrupted,phase);});
}
void combined_io(){
    fault_vfs::install();int cases=0;
    for(int interrupted=0;interrupted<combined_steps;++interrupted)isolated_combined([&]{
        int writes=0;bool supported=false;
        {Directory d;auto s=create(d.path/"count");supported=s.rules_supported();s.opening(0,"2026-01-01",Money(0,cad));for(int step=0;step<interrupted;++step)combined_step(s,step);if(!supported && (interrupted==5 || interrupted==6))return;fault_vfs::arm(-1);combined_step(s,interrupted);writes=fault_vfs::seen;fault_vfs::disarm();}check(writes>0);
        // Representative first/middle/last write failures complement full
        // per-command sweeps; every case finishes and restores the full history.
        for(const int position:{1,(writes+1)/2,writes}){
            Directory d;const auto path=d.path/"combined";
            {auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));for(int step=0;step<interrupted;++step)combined_step(s,step);fault_vfs::arm(position,cases%2==0?SQLITE_FULL:SQLITE_IOERR_WRITE);try{combined_step(s,interrupted);}catch(const ImportError&){}fault_vfs::disarm();}
            {auto s=ImportStore::open(path,password);s.verify();(void)combined_done(s,interrupted);for(int step=interrupted;step<combined_steps;++step)if(!combined_done(s,step))combined_step(s,step);combined_verify(s);s.backup_to(d.path/"restore",password);}
            {auto s=ImportStore::open(d.path/"restore",password);combined_verify(s);}no_leaks(d.path);++cases;
        }
        std::cout<<"Combined workflow I/O step "<<interrupted<<": "<<cases<<" cases"<<std::endl;
    });
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
int main(int argc,char** argv){if(std::getenv("SPENTIT_TEST_DIAGNOSTICS"))spentit::diagnostics::Collector::instance().enable();try{std::string mode=argc>1?argv[1]:"basics";if(mode=="group_benchmark")group_benchmark();else if(mode=="basics")basics();else if(mode=="combined")combined_recovery();else if(mode=="combined_io")combined_io();else if(mode=="aliases")account_aliases_test();else if(mode=="mapping_openings")mapping_openings();else if(mode=="mapping_opening_faults")mapping_opening_faults();else if(mode=="groups"){account_aliases_test();group_snapshot_invalidation();groups();mapped_transfers();group_categories();group_transfers();group_balances();group_duplicates_and_protection();}else if(mode=="mapped_io")group_faults(false,7);else if(mode=="mapped_crashes")group_faults(true,7);else if(mode=="group_io")group_faults(false);else if(mode=="group_crashes")group_faults(true);else if(mode=="validation"){validation();group_validation();association_validation();}else if(mode=="io")faults(false);else if(mode=="crashes")faults(true);else return 2;std::cout<<"Accounts "<<mode<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
