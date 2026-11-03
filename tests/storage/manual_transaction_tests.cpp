#include <spentit/import_store.hpp>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include "fault_vfs.hpp"
using namespace spentit;
using namespace spentit::app;
using spentit::storage::ImportStore;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection");}
imports::DraftRow manual(std::string amount="-10.00") {
    auto money=Money::parse(amount,Currency("CAD"));
    return {"2026-01-01","Manual shop","My note",Transaction::manual(money,{{"","",money}})};
}
LedgerSelectionEdit deletion(ImportStore& s,std::vector<imports::Occurrence> ids) {
    LedgerSelectionEdit r;r.action=LedgerSelectionAction::remove;r.account=s.account().identity;r.generation=s.ledger_edit_generation();r.selected=std::move(ids);return r;
}
std::string current_category(ImportStore& s,const imports::Occurrence& id) {
    const auto draft=s.transaction_accepted(id)?s.committed_entry(id).current.draft:*s.row(id).draft;
    return draft.transaction.allocations()[0].category;
}
std::vector<TransferRequest> category_requests(ImportStore& s,const std::vector<imports::Occurrence>& ids) {
    std::vector<TransferRequest> requests;
    for(const auto& id:ids) {
        const bool accepted=s.transaction_accepted(id);
        const auto draft=accepted?s.committed_entry(id).current.draft:*s.row(id).draft;
        const auto& a=draft.transaction.allocations().front();
        const bool imported=draft.transaction.original_imported_amount().has_value();
        TransferRequest r{};r.action=TransferAction::retarget;r.mode=0;r.link="category-"+id.batch;r.kept=r.link+"-kept";
        r.expected_sequence=s.transfer_sequence();r.category_id="expense";r.category_path="Expense";
        r.left=transfers::AllocationFact{{s.account().identity,id,a.identity},a.amount,draft.date,accepted?transfers::State::committed:transfers::State::draft,imported,false,!imported};
        for(const auto& b:s.batches())if(b.id==id.batch)r.batch_revision=b.revision;
        requests.push_back(r);
    }
    return requests;
}
void category_tests(const std::filesystem::path& dir) {
    const auto path=dir/"category";OperationContext context;
    const std::vector<imports::Occurrence> ids{{"accepted",0},{"pending",0},{"bank",0}};
    {auto s=ImportStore::create(path,"test-password","Categories",Currency("CAD"),"2025-01-01");
    s.apply_category_change("category",{CategoryAction::create,"expense","Expense",""},0);
    s.create_manual("accepted",manual());s.accept_all("accept");s.create_manual("pending",manual("-2.00"));
    s.ingest("bank","!Type:Bank\nD03/01/2026\nT-3.00\nPBank\n^\n",qif::DateOrder::dmy);}
    {auto s=ImportStore::open(path,"test-password");auto requests=category_requests(s,ids);
    auto bad=requests;bad[1].left->imported=true;bad[1].left->manually_entered=false;
    rejects([&]{s.assign_transfer_batch("bad-kind",bad,context);});check(!s.review_outcome("bad-kind"),"Forged source kind rejected atomically");
    bad=requests;bad[1].batch_revision++;rejects([&]{s.assign_transfer_batch("stale",bad,context);});
    OperationContext cancelled;int writes=0;cancelled.progress=[&](auto phase){if(phase=="Assigning transfers" && ++writes==2)cancelled.request();};
    rejects([&]{s.assign_transfer_batch("cancel",requests,cancelled);});check(!s.review_outcome("cancel"),"Category cancellation rolls back");
    for(const auto& id:ids)check(current_category(s,id).empty(),"Rejected batch preserves all categories");
    s.assign_transfer_batch("assign",requests,context);s.assign_transfer_batch("assign",requests,context);
    check(!s.transaction_accepted(ids[1]) && !s.imported_original(ids[1]),"Assignment neither accepts nor invents imported original");
    check(s.balance().units()==-1500,"Category preserves balance");s.verify();}
    {auto s=ImportStore::open(path,"test-password");
    for(const auto& id:ids)check(current_category(s,id)=="Expense","Mixed categories survive reopening");
    s.undo_review("undo");for(const auto& id:ids)check(current_category(s,id).empty(),"Mixed category Undo");s.verify();
    auto requests=category_requests(s,{ids[1]});s.assign_transfer_batch("single",requests,context);
    requests=category_requests(s,{ids[1]});requests[0].category_id.clear();requests[0].category_path.clear();s.assign_transfer_batch("uncategorized",requests,context);
    check(current_category(s,ids[1]).empty(),"Manual Uncategorized");s.undo_review("undo-uncategorized");check(current_category(s,ids[1])=="Expense","Manual Uncategorized Undo");s.verify();}
}
void category_recovery(const std::filesystem::path& dir) {
    const auto base=dir/"category-base";const std::vector<imports::Occurrence> ids{{"one",0},{"two",0}};
    {auto s=ImportStore::create(base,"test-password","Recovery",Currency("CAD"),"2025-01-01");
    s.apply_category_change("category",{CategoryAction::create,"expense","Expense",""},0);
    s.create_manual("one",manual());s.accept_all("accept");s.create_manual("two",manual("2.00"));}
    const auto check_atomic=[&](ImportStore& s) {
        const auto first=current_category(s,ids[0]);
        check(first.empty() || first=="Expense","Unexpected category");
        check(current_category(s,ids[1])==first,"Partial category batch");
        check(s.balance().units()==-800,"Category failure changed balance");
    };
    for(const int nth:{1,2,4,8,16,32,64,128,256,512}) {
        const auto trial=dir/("category-fault-"+std::to_string(nth));std::filesystem::copy_file(base,trial);bool fired=false;std::vector<TransferRequest> requests;
        {auto s=ImportStore::open(trial,"test-password");requests=category_requests(s,ids);OperationContext c;
        fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.assign_transfer_batch("assign",requests,c);}catch(...){}fired=fault_vfs::fired;fault_vfs::disarm();}
        {auto s=ImportStore::open(trial,"test-password");check_atomic(s);OperationContext c;s.assign_transfer_batch("assign",requests,c);
        check(current_category(s,ids[0])=="Expense","Category fault retry");check_atomic(s);s.verify();
        s.undo_review("undo");check(current_category(s,ids[0]).empty(),"Category fault Undo");check_atomic(s);s.verify();}
        if(!fired)break;
    }
    for(auto phase:{storage::ImportPhase::before_commit,storage::ImportPhase::after_commit}) {
        const auto trial=dir/(phase==storage::ImportPhase::before_commit?"category-before":"category-after");std::filesystem::copy_file(base,trial);std::vector<TransferRequest> requests;
        {auto s=ImportStore::open(trial,"test-password");requests=category_requests(s,ids);}
        const auto child=fork();check(child>=0,"fork");if(child==0){try{auto s=ImportStore::open(trial,"test-password");s.observer([&](auto point){if(point==phase)_exit(71);});OperationContext c;s.assign_transfer_batch("assign",requests,c);_exit(72);}catch(...){_exit(73);}}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71,"Category crash boundary");
        auto s=ImportStore::open(trial,"test-password");check_atomic(s);
        check(current_category(s,ids[0])==(phase==storage::ImportPhase::after_commit?"Expense":""),"Category durable boundary");
        OperationContext c;s.assign_transfer_batch("assign",requests,c);check_atomic(s);s.verify();
    }
}
void manual_rules(const std::filesystem::path& dir) {
    const auto path=dir/"manual-rules";OperationContext context;
    {auto s=ImportStore::create(path,"test-password","Rules",Currency("CAD"),"2025-01-01");if(!s.rules_supported())return;
    const auto id=s.create_manual("manual",manual());check(s.match_saved_rules(id,context).error.empty(),"Manual empty rule inspection");
    s.apply_category_change("category",{CategoryAction::create,"expense","Expense",""},0);
    RuleDefinitionImage rule;rule.identity="rule";rule.label="Manual shop";rule.enabled=true;rule.scope=rules::Scope::transaction;rule.input=rules::InputField::payee;rule.expression="Manual shop";rule.actions.category="expense";rule.actions.payee_template="Must remain manual";
    s.save_rules("rules",{{rule}},s.rule_image().revision,context);
    check(s.match_saved_rules(id,context).error.empty(),"Manual saved rule inspection");
    AccountRuleRequest request;request.account=s.account().identity;request.generation=s.ledger_edit_generation();request.ruleset_revision=s.rule_image().revision;request.catalog_revision=s.category_catalog().revision;
    s.apply_account_rules("apply",request,context);
    check(s.row(id).draft->payee=="Manual shop" && current_category(s,id)=="Expense","Manual rule category and text protection");
    check(s.match_saved_rules(id,context).error.empty(),"Applied manual category re-evaluation");s.verify();}
    {auto s=ImportStore::open(path,"test-password");check(s.match_saved_rules({"manual",0},context).error.empty(),"Manual rules reopen");s.verify();}
}
void deletion_tests(const std::filesystem::path& dir) {
    const auto path=dir/"delete";OperationContext context;
    {auto s=ImportStore::create(path,"test-password","Delete",Currency("CAD"),"2025-01-01");s.create_manual("manual",manual());}
    {auto s=ImportStore::open(path,"test-password");auto request=deletion(s,{{"manual",0}});
    s.edit_transactions("delete",request,context);s.edit_transactions("delete",request,context);
    check(s.current_ledger().empty() && s.balance().units()==0 && s.projected_balance()->units()==0,"Delete reopened manual balance");
    check(!s.transaction_accepted({"manual",0}),"Delete must not accept");s.verify();}
    {auto s=ImportStore::open(path,"test-password");check(s.current_ledger().empty(),"Deletion survives restart");
    s.undo_transaction_edit("restore","delete");check(s.current_ledger().size()==1 && s.balance().units()==-1000,"Delete Undo");s.verify();
    s.accept_all("accept");s.ingest("import","!Type:Bank\nD02/01/2026\nT-2.00\nPFirst\n^\nD03/01/2026\nT-3.00\nPSecond\n^\n",qif::DateOrder::dmy);
    const auto request=deletion(s,{{"manual",0},{"import",0}});s.edit_transactions("mixed",request,context);
    check(s.current_ledger().size()==1 && s.balance().units()==-300,"Mixed removal preserves other imported row");s.verify();
    s.undo_transaction_edit("mixed-undo","mixed");check(s.current_ledger().size()==3 && s.balance().units()==-1500,"Mixed Undo");s.verify();
    auto invalid=deletion(s,{{"manual",0},{"missing",0}});rejects([&]{s.edit_transactions("invalid",invalid,context);});check(s.current_ledger().size()==3,"Invalid selection atomic");
    s.edit_transactions("remove-import",deletion(s,{{"import",0}}),context);s.accept_all("accept-rest");check(!s.transaction_accepted({"import",0}) && s.current_ledger().size()==2 && s.balance().units()==-1300,"Accept skips deleted rows");s.verify();
    s.ingest("again","!Type:Bank\nD02/01/2026\nT-2.00\nPFirst\nMAnother export\n^\n",qif::DateOrder::dmy);check(s.current_ledger().size()==3,"Reimport does not match deleted row");s.verify();}
    {auto s=ImportStore::open(path,"test-password");check(s.current_ledger().size()==3 && s.balance().units()==-1500,"Mixed reopen");}
}
void deletion_recovery(const std::filesystem::path& dir) {
    const auto base=dir/"delete-base";
    {auto s=ImportStore::create(base,"test-password","Recovery",Currency("CAD"),"2025-01-01");s.create_manual("one",manual());s.accept_all("accept");s.create_manual("two",manual("2.00"));}
    for(const int nth:{1,2,4,8,16,32,64,128,256,512}) {
        const auto trial=dir/("delete-fault-"+std::to_string(nth));std::filesystem::copy_file(base,trial);bool fired=false;LedgerSelectionEdit request;
        {auto s=ImportStore::open(trial,"test-password");request=deletion(s,{{"one",0},{"two",0}});OperationContext context;
        fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.edit_transactions("remove",request,context);}catch(...){}fired=fault_vfs::fired;fault_vfs::disarm();}
        {auto s=ImportStore::open(trial,"test-password");const auto count=s.current_ledger().size();check(count==0 || count==2,"Partial mixed deletion");
        OperationContext context;s.edit_transactions("remove",request,context);check(s.current_ledger().empty() && s.balance().units()==0,"Delete failure retry");s.verify();
        s.undo_transaction_edit("undo","remove");check(s.current_ledger().size()==2 && s.balance().units()==-800,"Delete failure Undo");s.verify();}
        if(!fired)break;
    }
    for(auto phase:{storage::ImportPhase::before_commit,storage::ImportPhase::after_commit}) {
        const auto trial=dir/(phase==storage::ImportPhase::before_commit?"delete-before":"delete-after");std::filesystem::copy_file(base,trial);LedgerSelectionEdit request;
        {auto s=ImportStore::open(trial,"test-password");request=deletion(s,{{"one",0},{"two",0}});}
        const auto child=fork();check(child>=0,"fork");if(child==0){try{auto s=ImportStore::open(trial,"test-password");s.observer([&](auto point){if(point==phase)_exit(71);});OperationContext context;s.edit_transactions("remove",request,context);_exit(72);}catch(...){_exit(73);}}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71,"Delete crash boundary");
        auto s=ImportStore::open(trial,"test-password");check(s.current_ledger().size()==(phase==storage::ImportPhase::after_commit?0u:2u),"Delete durable boundary");
        OperationContext context;s.edit_transactions("remove",request,context);check(s.current_ledger().empty(),"Delete crash retry");s.verify();
    }
    {auto s=ImportStore::open(base,"test-password");auto request=deletion(s,{{"one",0},{"two",0}});OperationContext context;int writes=0;
    context.progress=[&](auto phase){if(phase=="Updating selected transactions" && ++writes==2)context.request();};
    rejects([&]{s.edit_transactions("cancel",request,context);});check(s.current_ledger().size()==2 && !s.review_outcome("cancel"),"Cancelled deletion atomic");s.verify();}
}
void recovery(const std::filesystem::path& dir) {
    const auto base=dir/"base";
    {auto s=ImportStore::create(base,"test-password","Recovery",Currency("CAD"),"2025-01-01");}
    for(const int nth:{1,2,4,8,16,32,64,128,256,512}){const auto trial=dir/("fault-"+std::to_string(nth));std::filesystem::copy_file(base,trial);bool fired=false;
        {auto s=ImportStore::open(trial,"test-password");fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.create_manual("entry",manual());}catch(...){}fired=fault_vfs::fired;fault_vfs::disarm();}
        {auto s=ImportStore::open(trial,"test-password");check(s.current_ledger().size()<=1,"Partial creation");s.create_manual("entry",manual());check(s.current_ledger().size()==1 && s.balance().units()==-1000,"Creation retry");s.verify();}
        if(!fired)break;
    }
    for(auto phase:{storage::ImportPhase::before_commit,storage::ImportPhase::after_commit}){
        const auto trial=dir/(phase==storage::ImportPhase::before_commit?"before":"after");std::filesystem::copy_file(base,trial);
        const auto child=fork();check(child>=0,"fork");if(child==0){try{auto s=ImportStore::open(trial,"test-password");s.observer([&](auto point){if(point==phase)_exit(71);});s.create_manual("entry",manual());_exit(72);}catch(...){_exit(73);}}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71,"Crash boundary");
        auto s=ImportStore::open(trial,"test-password");check(s.current_ledger().size()==(phase==storage::ImportPhase::after_commit?1u:0u),"Durable boundary");s.create_manual("entry",manual());check(s.current_ledger().size()==1,"Crash retry");s.verify();
    }
}
int main(){
    const auto dir=std::filesystem::canonical(std::filesystem::temp_directory_path())/("spentit-manual-"+std::to_string(getpid()));std::filesystem::create_directory(dir);const auto path=dir/"test.spentit";
    try {
        fault_vfs::install();category_tests(dir);category_recovery(dir);manual_rules(dir);deletion_tests(dir);deletion_recovery(dir);
        {auto s=ImportStore::create(path,"test-password","Checking",Currency("CAD"),"2025-01-01");
        const auto id=s.create_manual("manual-one",manual());check(id.batch=="manual-one","Identity");
        check(s.balance().units()==-1000,"Balance");check(!s.row(id).draft->transaction.original_imported_amount(),"No invented original");check(!s.imported_original(id),"No imported original");check(s.undoable_imports().empty(),"Manual is not an import");
        s.create_manual("manual-one",manual());check(s.current_ledger().size()==1,"Retry duplicates entry");rejects([&]{s.create_manual("manual-one",manual("-11.00"));});s.verify();}
        {auto s=ImportStore::open(path,"test-password");check(s.current_ledger().size()==1,"Reopen");
        auto b=s.ingest("bank","!Type:Bank\nD01/01/2026\nT-10.00\nPBank shop\n^\n",qif::DateOrder::dmy);
        check(s.current_ledger().size()==2,"Manual must not auto-match");
        const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::same;});check(q!=sheet.questions.end(),"Manual match suggestion");
        ReviewAnswer a;a.generation=sheet.generation;a.question=q->key;a.action=ReviewAction::merge;a.selected={{"manual-one",0},{"bank",0}};a.survivor=imports::Occurrence{"manual-one",0};s.answer_review("match",a);
        check(s.current_ledger().size()==1 && s.balance().units()==-1000,"Merge counts once");check(s.current_ledger()[0].draft.memo=="My note","Merge preserves notes");check(s.imported_original({"manual-one",0})==imports::Occurrence{"bank",0},"Attached original");s.verify();
        s.undo_review("undo-match");check(s.current_ledger().size()==2 && !s.imported_original({"manual-one",0}),"Merge Undo");s.verify();
        a.generation=s.review_sheet().generation;s.answer_review("match-again",a);s.accept_all("accept");s.verify();
        s.ingest("bank-again","!Type:Bank\nD01/01/2026\nT-10.00\nPBank shop\nMSecond export\n^\n",qif::DateOrder::dmy);
        check(s.current_ledger().size()==1,"Attached reimport recognition");s.verify();}
        {auto s=ImportStore::open(path,"test-password");check(s.balance().units()==-1000,"Accepted reopen");
        s.create_manual("cancel",manual("0.00"));const auto gen=s.ledger_edit_generation();s.undo_manual("cancel",gen);s.undo_manual("cancel",gen);check(s.current_ledger().size()==1,"Creation Undo and retry");s.verify();
        s.create_manual("stale",manual("1.00"));const auto stale=s.ledger_edit_generation();s.create_manual("later",manual("2.00"));rejects([&]{s.undo_manual("stale",stale);});s.verify();}
        {const auto extra=dir/"edits";auto s=ImportStore::create(extra,"test-password","Edits",Currency("CAD"),"2025-01-01");
        s.create_manual("editing",manual());const auto id=imports::Occurrence{"editing",0};auto edit=*s.row(id).draft;edit.date="2026-02-02";edit.payee="Changed manually";
        edit.transaction=Transaction::manual(Money::parse("-12.00",Currency("CAD")),{{"","Debit",Money::parse("-15.00",Currency("CAD"))},{"","Refund",Money::parse("3.00",Currency("CAD"))}});
        s.edit_transaction("edit",id,s.batches().front().revision,{},edit);check(s.balance().units()==-1200,"Signed manual splits");s.verify();
        s.undo_transaction_edit("undo-edit","edit");check(s.balance().units()==-1000 && s.row(id).draft->date=="2026-01-01","Manual edit Undo");s.verify();
        s.accept_all("accept-manual");auto entry=s.committed_entry(id);edit=entry.current.draft;edit.date="2026-03-03";
        s.edit_transaction("accepted-edit",id,s.batches().front().revision,entry.token,edit);s.verify();s.undo_transaction_edit("accepted-undo","accepted-edit");s.verify();
        s.create_manual("discardable",manual("5.00"));const auto review=s.rollback_review();s.rollback_account("discard",review.token);check(s.current_ledger().size()==1 && s.balance().units()==-1000,"Account discard preserves accepted manual work");s.verify();
        auto bad=manual();bad.date="2026-02-30";rejects([&]{s.create_manual("bad-date",bad);});
        rejects([&]{auto invalid=manual();invalid.transaction=Transaction::manual(Money(1,Currency("CAD")),{{"","",Money(2,Currency("CAD"))}});s.create_manual("bad-splits",invalid);});
        s.observer([](auto phase){if(phase==storage::ImportPhase::after_commit)throw std::runtime_error("lost acknowledgement");});
        rejects([&]{s.create_manual("ack",manual());});s.observer({});check(s.manual_outcome("ack"),"Durable acknowledgement lookup");s.create_manual("ack",manual());s.verify();}
        {auto s=ImportStore::create(dir/"corrected-match","test-password","Matching",Currency("CAD"),"2025-01-01");
        s.apply_category_change("category",{CategoryAction::create,"expense","Expense",""},0);
        auto entry=manual();auto allocations=entry.transaction.allocations();allocations[0].category="Expense";allocations[0].category_binding=CategoryBinding{"expense",s.category_catalog().revision};entry.transaction=Transaction::manual(entry.transaction.effective_total(),allocations);
        const auto id=s.create_manual("manual",entry);s.verify();auto changed=*s.row(id).draft;changed.date="2026-02-02";allocations=changed.transaction.allocations();allocations[0].amount=Money(-1200,Currency("CAD"));changed.transaction=Transaction::manual(allocations[0].amount,allocations);
        s.edit_transaction("change",id,s.batches().front().revision,{},changed);
        s.ingest("wrong-date","!Type:Bank\nD01/01/2026\nT-12.00\nPOther\n^\n",qif::DateOrder::dmy);
        s.ingest("wrong-amount","!Type:Bank\nD02/02/2026\nT-13.00\nPOther\n^\n",qif::DateOrder::dmy);
        check(s.review_sheet().questions.empty(),"Manual suggestions require current date and amount");
        s.ingest("bank","!Type:Bank\nD02/02/2026\nT-12.00\nPBank\n^\n",qif::DateOrder::dmy);
        const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::same;});check(q!=sheet.questions.end(),"Corrected manual match");
        ReviewAnswer a;a.generation=sheet.generation;a.question=q->key;a.action=ReviewAction::merge;a.selected={id,{"bank",0}};a.survivor=id;s.answer_review("merge",a);s.accept_all("accept");s.verify();
        check(s.committed_entry(id).current.draft.transaction.allocations()[0].category_binding->identity=="expense","Category retained by Merge");
        s.ingest("again","!Type:Bank\nD02/02/2026\nT-12.00\nPBank\nMAnother export\n^\n",qif::DateOrder::dmy);check(s.current_ledger().size()==3,"Corrected manual reimport");s.verify();}
        {auto s=ImportStore::create(dir/"ambiguous","test-password","Ambiguous",Currency("CAD"),"2025-01-01");
        s.ingest("bank","!Type:Bank\nD01/01/2026\nT-10.00\nPBank\n^\n",qif::DateOrder::dmy);s.accept_all("accept-bank");s.create_manual("first",manual());s.create_manual("second",manual());
        ReviewAnswer a;a.generation=s.review_sheet().generation;a.question="selected";a.action=ReviewAction::merge;a.selected={{"bank",0},{"first",0},{"second",0}};a.survivor=imports::Occurrence{"bank",0};
        rejects([&]{s.answer_review("wrong-survivor",a);});check(s.current_ledger().size()==3,"Ambiguous Merge rejection is atomic");
        a.survivor=imports::Occurrence{"first",0};s.answer_review("manual-survivor",a);check(s.current_ledger().size()==1 && s.current_ledger()[0].identity.batch=="first","Chosen manual survivor");s.verify();
        s.undo_review("undo-ambiguous");check(s.current_ledger().size()==3,"Ambiguous Merge Undo");s.verify();}
        recovery(dir);
        std::filesystem::remove_all(dir);std::cout<<"manual transaction checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(dir);return 1;}
}
