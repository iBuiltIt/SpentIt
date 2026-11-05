#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <source_location>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
using namespace spentit;
using namespace spentit::storage;
void check(bool v,const std::source_location at=std::source_location::current()){if(!v)throw std::runtime_error("Report storage assertion "+std::to_string(at.line()));}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
int main(){std::filesystem::path dir;try{
    fault_vfs::install();
    auto name=(std::filesystem::temp_directory_path()/"spentit-reports-XXXXXX").string();auto p=mkdtemp(name.data());if(!p)throw std::runtime_error("Temporary directory failed");dir=std::filesystem::canonical(p);
    const auto path=dir/"document";const Currency cad("CAD");constexpr auto password="synthetic-reports";
    reports::Catalog saved;
    {
        auto store=ImportStore::create(path,password,"Checking",cad,"2026-01-01");
        check(store.report_catalog().revision==0);
        reports::Definition d{"r","Report","grouped_bar",{1},{""},{{"this_year",{},{}},{"last_year",{},{}}}};
        saved={0,{d}};store.save_reports(saved);check(store.report_catalog().revision==1);saved=store.report_catalog();
        auto invalid=saved;invalid.definitions.front().name="";rejects([&]{store.save_reports(invalid);});check(store.report_catalog().revision==1);
        auto retry=saved;retry.revision=0;store.save_reports(retry);check(store.report_catalog().revision==1);
        auto stale=retry;stale.definitions[0].name="Conflicting retry";rejects([&]{store.save_reports(stale);});
        auto next=saved;next.definitions.front().name="Changed";
        store.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw std::runtime_error("Synthetic precommit fault");});rejects([&]{store.save_reports(next);});store.observer({});check(store.report_catalog().definitions==saved.definitions);
        store.observer([](ImportPhase phase){if(phase==ImportPhase::after_commit)throw std::runtime_error("Synthetic postcommit fault");});rejects([&]{store.save_reports(next);});store.observer({});check(store.report_catalog().definitions==next.definitions);saved=store.report_catalog();
        store.opening(0,"2026-01-01",Money(10000,cad));
        store.ingest("a","D1/1/2026\nT-10\nPSynthetic\n^\n",qif::DateOrder::dmy);store.commit("a",0,"accept-a");
        store.ingest("b","D2/1/2026\nT-3\nPNew\n^\n",qif::DateOrder::dmy);
        app::OperationContext context;auto entries=store.report_entries({1},context);check(entries.size()==2 && entries[0].committed && !entries[1].committed);check(store.account().identity==1);
        auto report=reports::calculate(saved.definitions.front(),"2026-11-04",entries,{{"","Uncategorized"}},{cad});check(report.currencies[0].totals[0].amount.units()==1300);
        rejects([&]{store.report_entries({99},context);});check(store.account().identity==1);
        store.create_account(2,"Savings",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));store.select_account(1);
        const auto category=store.preview_category_change({app::CategoryAction::create,"fees","Fees","",CategoryKind::leaf});store.apply_category_change("create-fees",category.request,category.catalog_revision);
        store.ingest("transfer","D3/1/2026\nT-102\nPTransfer with fee\nS[Old Savings export]\n$-100\nSFees\n$-2\n^\n",qif::DateOrder::dmy);
        auto transferRow=store.row({"transfer",0});
        app::TransferRequest request{};request.action=app::TransferAction::from_draft;request.link="report-transfer";request.origin=imports::Occurrence{"transfer",0};request.allocation=transferRow.draft->transaction.allocations()[0].identity;request.destination=2;request.expected_sequence=store.transfer_sequence();
        store.apply_transfer("make-report-transfer",request);
        auto both=store.report_entries({1,2},context);check(store.account().identity==1);
        auto def=saved.definitions[0];def.accounts={1,2};def.categories={"","fees"};auto totals=reports::calculate(def,"2026-11-04",both,{{"","Uncategorized"},{"fees","Fees"}},{cad});check(totals.currencies[0].totals[0].amount.units()==1500);
        def.accounts={1};totals=reports::calculate(def,"2026-11-04",store.report_entries({1},context),{{"","Uncategorized"},{"fees","Fees"}},{cad});check(totals.currencies[0].totals[0].amount.units()==1500);
        check(std::count_if(both.begin(),both.end(),[](const auto& e){return e.transfer;})==2);
        check(std::count_if(both.begin(),both.end(),[](const auto& e){return e.category=="fees" && !e.transfer && e.amount.units()==-200;})==1);
        app::OperationContext cancelled;cancelled.request();rejects([&]{store.report_entries({1,2},cancelled);});check(store.account().identity==1);
        auto edit=*store.row({"b",0}).draft;auto splits=edit.transaction.allocations();splits[0].amount=Money(-400,cad);edit.transaction=edit.transaction.corrected(Money(-400,cad),splits);store.save({"b",0},0,edit);
        totals=reports::calculate(def,"2026-11-04",store.report_entries({1},context),{{"","Uncategorized"},{"fees","Fees"}},{cad});check(totals.currencies[0].totals[0].amount.units()==1600);

        store.backup_to(dir/"backup",password);
    }
    {auto restored=ImportStore::open(dir/"backup",password);check(restored.report_catalog().definitions==saved.definitions);restored.verify();}
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
        auto pid=fork();check(pid>=0);
        if(pid==0){try{auto store=ImportStore::open(path,password);auto next=store.report_catalog();next.definitions[0].name="Crash change";store.observer([&](ImportPhase p){if(p==phase)_exit(73);});store.save_reports(next);_exit(74);}catch(...){_exit(75);}}
        int status=0;waitpid(pid,&status,0);check(WIFEXITED(status) && WEXITSTATUS(status)==73);
        auto reopened=ImportStore::open(path,password);const auto current=reopened.report_catalog();check(current.definitions[0].name==(phase==ImportPhase::before_commit?"Changed":"Crash change"));reopened.verify();
    }
    int writes=0;
    {auto store=ImportStore::open(path,password);auto next=store.report_catalog();next.definitions[0].name="Measure writes";fault_vfs::arm(-1);store.save_reports(next);writes=fault_vfs::seen;fault_vfs::disarm();}
    check(writes>0);
    for(int nth=1;nth<=writes;++nth){
        reports::Catalog before,after;
        {auto store=ImportStore::open(path,password);before=store.report_catalog();after=before;after.definitions[0].name="Fault "+std::to_string(nth);
            fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{store.save_reports(after);}catch(const std::exception&){}const bool fired=fault_vfs::fired;fault_vfs::disarm();check(fired);}
        auto reopened=ImportStore::open(path,password);auto current=reopened.report_catalog();
        check((current.revision==before.revision && current.definitions==before.definitions) || (current.revision==before.revision+1 && current.definitions==after.definitions));
        reopened.save_reports(after);check(reopened.report_catalog().definitions==after.definitions);reopened.verify();
    }
    std::cout<<"Report write/sync fault positions: "<<writes<<"\n";
    std::filesystem::remove_all(dir);std::cout<<"Report persistence, atomicity, crashes, snapshot and restored backup passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(!dir.empty())std::filesystem::remove_all(dir);return 1;}}
