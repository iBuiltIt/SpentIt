#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <cerrno>
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
        auto s=ImportStore::open(path,password);auto aborted=mapping;aborted[0].account=4;aborted[0].creation->identity=4;aborted[1].account=5;aborted[1].creation->identity=5;
        app::OperationContext capture;s.prepare_import_group("abort",bytes,qif::DateOrder::dmy,aborted,capture);app::OperationContext resume;s.resume_import_group("abort",resume);
        s.select_account(4);imports::DraftRow repair{"2026-01-01","","",Transaction::repaired_import({},Money(2500,cad),{{"Food","",Money(2500,cad)}})};s.save({"abort-account-0",0},1,repair);
        s.abort_import_group("abort");check(s.account().identity==1);s.abort_import_group("abort");check(s.accounts().size()==3 && s.next_account_identity()==6);
        rejected([&]{s.select_account(4);});rejected([&]{s.create_account(4,"First synthetic",cad,"2026-01-01");});
        app::OperationContext retry;check(s.prepare_import_group("abort",bytes,qif::DateOrder::dmy,aborted,retry).state==app::GroupState::aborted);s.verify();
    }
    {auto s=ImportStore::open(path,password);check(s.accounts().size()==3 && s.import_groups().size()==2);s.verify();}no_leaks(d.path);
}
void group_transfers(){
    for(bool destination_first:{false,true}) {
        Directory d;const auto path=d.path/"group-transfer";
        const std::string bytes="!Account\nNOrigin synthetic\nTBank\nB-10\n^\n!Type:Bank\nD1/1/2026\nT-10\nL[Destination synthetic]\n^\n!Account\nNDestination synthetic\nTCash\nB40\n^\n!Type:Cash\n";
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
        app::OperationContext again;s.prepare_import_group("second",bytes,qif::DateOrder::dmy,mapping,again);app::OperationContext retry;check(s.prepare_import_group("retry-pending",bytes,qif::DateOrder::dmy,mapping,retry).id=="second");app::OperationContext process;s.resume_import_group("second",process);
        check(s.row({"second-account-0",0}).decision.duplicate_of==imports::Occurrence{"first-account-0",0});check(s.row({"second-account-0",1}).decision.duplicate_of==imports::Occurrence{"first-account-0",1});
        s.decide({"second-account-0",1},1,{imports::Disposition::include,{}});const auto receipt=s.commit("second-account-0",2,"repeat-commit");check(receipt.included==1 && receipt.excluded==1 && receipt.after==Money(3000,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"swapped");std::string bytes;std::vector<app::ImportAccountMapping> mapping;
        for(int i=0;i<2;++i){const auto name="Mapped source "+std::to_string(i);bytes+="!Account\nN"+name+"\nTBank\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n";mapping.push_back({static_cast<std::size_t>(i),i+2,app::AccountCreation{i+2,name,cad,"2026-01-01"}});}
        app::OperationContext capture;s.prepare_import_group("first",bytes,qif::DateOrder::dmy,mapping,capture);app::OperationContext resume;s.resume_import_group("first",resume);for(std::size_t i=0;i<2;++i)s.review_import_balance("first",i,1,app::ReportedBalanceChoice::zero,"2026-01-01");s.select_account(2);s.commit("first-account-0",1,"commit-0");s.select_account(3);s.commit("first-account-1",1,"commit-1");
        app::OperationContext again;s.prepare_import_group("swap",bytes,qif::DateOrder::dmy,{{0,3,{}},{1,2,{}}},again);app::OperationContext process;s.resume_import_group("swap",process);s.select_account(3);check(s.row({"swap-account-0",0}).decision.disposition==imports::Disposition::unresolved);s.select_account(2);check(s.row({"swap-account-1",0}).decision.disposition==imports::Disposition::unresolved);s.abort_import_group("swap");check(s.accounts().size()==3);s.verify();
    }
    {
        Directory d;auto s=create(d.path/"single");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Destination",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        const std::string bytes="D1/1/2026\nT-10\nL[Destination]\n^\nD2/1/2026\nTbad\n^\n";s.ingest("single",bytes,qif::DateOrder::dmy);
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
    const auto first=kind==5?"-10":(kind==3 || kind==6)?"bad":"10";
    return std::string("!Account\nNFault first\nTBank\nB")+(kind==5?"-10":"100")+"\n^\n!Type:Bank\nD1/1/2026\nT"+first+"\nPGROUP-FAULT-MARKER\n"+(kind==5?"L[Fault second]\n":"")+"^\n!Account\nNFault second\nTCash\nB"+(kind==5?"40":"-10")+"\n^\n!Type:Cash\n"+(kind==5?"":"D2/1/2026\nT-10\n^\n");
}
std::vector<app::ImportAccountMapping> group_fault_mapping(){return {{0,2,app::AccountCreation{2,"Fault first",cad,"2026-01-01"}},{1,3,app::AccountCreation{3,"Fault second",cad,"2026-01-01"}}};}
imports::DraftRow group_fault_repair(){return {"2026-01-01","GROUP-FAULT-MARKER","",Transaction::repaired_import({},Money(1000,cad),{{"","",Money(1000,cad)}})};}
void group_fault_setup(const fs::path& path,int kind){
    auto s=create(path);if(kind==0)return;app::OperationContext capture;s.prepare_import_group("fault",group_fault_source(kind),qif::DateOrder::dmy,group_fault_mapping(),capture);if(kind==1)return;
    app::OperationContext resume;s.resume_import_group("fault",resume);if(kind==2 || kind==6)return;
    if(kind==3){s.select_account(2);s.save({"fault-account-0",0},1,group_fault_repair());return;}
    if(kind==5){s.select_account(2);app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="fault-link";r.origin=imports::Occurrence{"fault-account-0",0};r.allocation="source-0";r.destination=3;r.batch_revision=1;r.expected_sequence=s.transfer_sequence();s.apply_transfer("link",r);s.select_account(3);r={};r.action=app::TransferAction::accept;r.link="fault-link";r.expected_sequence=s.transfer_sequence();s.apply_transfer("accept",r);}
    s.review_import_balance("fault",0,1,kind==5?app::ReportedBalanceChoice::zero:app::ReportedBalanceChoice::closing,"2026-01-01");s.review_import_balance("fault",1,1,kind==5?app::ReportedBalanceChoice::closing:app::ReportedBalanceChoice::zero,"2026-01-01");
}
void group_fault_execute(ImportStore& s,int kind){
    app::OperationContext context;
    if(kind==0)s.prepare_import_group("fault",group_fault_source(kind),qif::DateOrder::dmy,group_fault_mapping(),context);
    else if(kind==1)s.resume_import_group("fault",context);
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
        {4,false,"DROP TRIGGER balance_events_immutable;UPDATE import_balance_events SET reviewed_change=reviewed_change+1 WHERE source_account=0"},
        {4,false,"DROP TRIGGER balance_events_immutable;UPDATE import_balance_events SET opening=opening+1 WHERE source_account=0"},
        {4,false,"DROP TRIGGER balance_history;UPDATE import_group_accounts SET opening=opening+1 WHERE source_account=0"},
        {4,true,"DROP TRIGGER group_events_keep;DELETE FROM import_group_events WHERE state=1"},
        {4,true,"DROP TRIGGER group_transition;DROP TRIGGER group_transition_history;UPDATE import_groups SET publication='missing-receipt'"},
        {4,false,"INSERT INTO retired_import_accounts(account_id,group_id) VALUES(2,'fault')"},
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
    if(kind==1)return saved->state==app::GroupState::reviewing;
    if(kind==2)return saved->accounts[0].balance_choice!=app::ReportedBalanceChoice::unreviewed;
    if(kind==3){const bool done=saved->state==app::GroupState::aborted;check(s.accounts().size()==(done?1:3));return done;}
    if(kind==4 || kind==5){s.select_account(kind==4?2:3);const bool done=kind==4?s.outcome("publish").has_value():s.transfer_outcome("publish-transfer").has_value();check((saved->state==app::GroupState::started)==done);check(s.account().opening.units()==(done?(kind==4?9000:3000):0));return done;}
    s.select_account(2);const bool done=s.row({"fault-account-0",0}).draft.has_value();check(s.batches()[0].revision==(done?2:1));return done;
}
void group_fault_recovered(const fs::path& path,int kind){
    auto s=ImportStore::open(path,password);(void)group_fault_done(s,kind);group_fault_execute(s,kind);s.verify();const auto group=s.import_groups()[0];
    if(kind==0)check(group.state==app::GroupState::captured && s.accounts().size()==3);
    if(kind==1)check(group.state==app::GroupState::reviewing && group.accounts.size()==2);
    if(kind==2)check(group.accounts[0].proposed_opening==Money(9000,cad));
    if(kind==3)check(group.state==app::GroupState::aborted && s.accounts().size()==1);
    if(kind==4)check(s.committed_balance()==Money(10000,cad));
    if(kind==5)check(s.committed_balance()==Money(4000,cad));
    if(kind==6)check(s.row({"fault-account-0",0}).draft->transaction.effective_total()==Money(1000,cad));
}
void group_faults(bool crash){
    fault_vfs::install();
    for(int kind=0;kind<7;++kind){
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
int main(int argc,char** argv){try{std::string mode=argc>1?argv[1]:"basics";if(mode=="basics")basics();else if(mode=="combined")combined_recovery();else if(mode=="combined_io")combined_io();else if(mode=="groups"){groups();group_transfers();group_balances();group_duplicates_and_protection();}else if(mode=="group_io")group_faults(false);else if(mode=="group_crashes")group_faults(true);else if(mode=="validation"){validation();group_validation();}else if(mode=="io")faults(false);else if(mode=="crashes")faults(true);else return 2;std::cout<<"Accounts "<<mode<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
