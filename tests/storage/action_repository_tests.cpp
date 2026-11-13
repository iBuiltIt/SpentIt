#include <spentit/import_store.hpp>
#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <unistd.h>
#include "fault_vfs.hpp"
#include <sys/wait.h>

using namespace spentit;
using namespace spentit::app;
using spentit::storage::ImportStore;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string source(std::string_view payee){return "!Type:Bank\nD01/01/2026\nT-10.00\nP"+std::string(payee)+"\n^\n";}
void edit(ImportStore& store,std::string_view operation,const imports::Occurrence& id,std::string_view payee) {
    const auto row=store.row(id);auto draft=*row.draft;draft.payee=payee;draft.payee_owner=FieldOwnership::manual;
    const auto batches=store.batches();
    const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& value){return value.id==id.batch;});
    check(batch!=batches.end(),"Missing batch");
    store.edit_transaction(operation,id,batch->revision,store.transaction_accepted(id)?std::optional<EntryToken>{store.committed_entry(id).token}:std::nullopt,draft);
}
void undo(ImportStore& store,std::string_view operation,std::string_view expected) {
    if(std::getenv("SPENTIT_TEST_TRACE"))std::cerr<<"Undo "<<expected<<"\n";
    const auto head=store.action_head();check(head && head->id==expected,"Unexpected Undo head");
    store.undo_action(operation,store.preview_undo(head->id,store.action_token()));store.verify();
}
void run(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("first",source("Original"),qif::DateOrder::dmy);
    check(store.action_head()->kind==ActionKind::import,"Import must be one root");
    const imports::Occurrence id{"first",0};
    edit(store,"editA",id,"A");edit(store,"editB",id,"B");
    undo(store,"undoB","editB");check(store.row(id).draft->payee=="A","Undo B restores A");
    const auto preview=store.preview_undo("editA",store.action_token());
    store.apply_category_change("category",{CategoryAction::create,"food","Food",{},CategoryKind::leaf},store.category_catalog().revision);
    check(store.action_head()->id=="editA","Definition save preserves head");
    bool stale=false;try{store.undo_action("stale",preview);}catch(const storage::ImportError&){stale=true;}check(stale,"Definition save invalidates old preview");
    store.ingest("later",source("Later"),qif::DateOrder::dmy);
    undo(store,"undoLater","later");
    check(store.balance().units()==-1000 && store.row(id).draft->payee=="A","Import Undo preserves earlier edit and balance");
    undo(store,"undoA","editA");check(store.row(id).draft->payee=="Original","Earlier edit remains undoable");
    undo(store,"undoFirst","first");check(store.balance().units()==0 && !store.action_head(),"Whole chain unwinds");
    check(store.category_catalog().categories.size()==1,"Standalone category remains");
    imports::DraftRow manual{"2026-01-02","Manual","",Transaction::manual(Money(-250,Currency("CAD")),{{"","",Money(-250,Currency("CAD"))}})};

    const auto manual_id=store.create_manual("manual",manual);
    const auto unchanged=store.action_token();
    const auto manual_row=store.row(manual_id);store.edit_transaction("noEdit",manual_id,0,{},*manual_row.draft);
    check(store.action_token()==unchanged,"No-op edit preserves the Undo token");store.edit_transaction("noEdit",manual_id,0,{},*manual_row.draft);
    edit(store,"manualEdit",manual_id,"Changed manual");
    store.correct_opening("openingA","2026-01-01",Money(500,Currency("CAD")));
    store.rename_account("rename","Renamed checking");
    store.correct_opening("openingB","2026-01-01",Money(1000,Currency("CAD")));
    undo(store,"undoOpeningB","openingB");undo(store,"undoOpeningA","openingA");
    check(store.account().name=="Renamed checking" && store.balance().units()==-250,"Opening Undo preserves current account name");
    undo(store,"undoManualEdit","manualEdit");undo(store,"undoManual","manual");
    check(store.balance().units()==0 && !store.action_head(),"Manual creation and edit unwind together");
    store.certify({});
}
void committed(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("accepted",source("Accepted"),qif::DateOrder::dmy);
    const imports::Occurrence accepted{"accepted",0};
    edit(store,"beforeAccept",accepted,"Before acceptance");
    store.accept_all("accept",true);
    edit(store,"acceptedA",accepted,"A");edit(store,"acceptedB",accepted,"B");
    undo(store,"undoAcceptedB","acceptedB");undo(store,"undoAcceptedA","acceptedA");
    auto corrected=store.committed_entry(accepted).current.draft;corrected.payee="Direct";
    const auto direct_preview=store.preview_committed_change({ChangeKind::correct,accepted,corrected,{}});
    store.apply_committed_change_with_catalog("direct",direct_preview.request,direct_preview.expected,direct_preview.preview_sequence,direct_preview.catalog_revision);
    undo(store,"undoDirect","direct");
    check(store.committed_entry(accepted).current.draft.payee=="Before acceptance","Direct committed edit joins the common chain");
    const auto next=store.preview_committed_change({ChangeKind::correct,accepted,corrected,{}});
    store.apply_committed_change_with_catalog("legacyDirect",next.request,next.expected,next.preview_sequence,next.catalog_revision);
    const auto inverse=store.preview_committed_change({ChangeKind::undo,accepted,{},"legacyDirect"});
    store.apply_committed_change_with_catalog("legacyUndo",inverse.request,inverse.expected,inverse.preview_sequence,inverse.catalog_revision);
    check(store.action_head()->id=="accept","Legacy inverse pops the common action head");
    store.verify();
    const auto backup=std::filesystem::path(path.string()+"-backup");store.backup_to(backup,"synthetic");
    {auto restored=ImportStore::open(backup,"synthetic");check(restored.action_head()->id=="accept","Open-session backup retains common head");undo(restored,"backupUndoAccept","accept");check(!restored.transaction_accepted(accepted),"Restored backup can reverse acceptance");restored.verify();}
    undo(store,"undoAccept","accept");
    check(!store.transaction_accepted(accepted) && store.balance().units()==-1000,"Undo acceptance returns the same transaction to pending");
    undo(store,"undoBeforeAccept","beforeAccept");
    check(store.row(accepted).draft->payee=="Accepted","Pre-acceptance edit remains reachable");
    store.accept_all("acceptAgain",true);store.verify();
    check(store.committed_entry(accepted).current.revision>0,"Reacceptance allocates a fresh revision");
    undo(store,"undoAcceptAgain","acceptAgain");
    undo(store,"undoAcceptedImport","accepted");
    store.verify();store.certify({});
 }
void pending_acceptance(const std::filesystem::path& path) {
    {auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
     store.ingest("first",source("Original"),qif::DateOrder::dmy);store.accept_all("accept",true);
     undo(store,"undoAccept","accept");store.certify({});}
    {auto store=ImportStore::open(path,"synthetic");store.verify();
     check(!store.action_head() && store.balance().units()==-1000 && !store.transaction_accepted({"first",0}),"Close/reopen keeps revoked acceptance pending");
     store.accept_all("reaccept",true);store.verify();store.certify({});}
    {auto store=ImportStore::open(path,"synthetic");store.verify();check(store.transaction_accepted({"first",0}),"Reacceptance survives close/reopen");}
}
void acceptance_decisions(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("first",source("Original"),qif::DateOrder::dmy);
    store.ingest("candidate",source("Candidate"),qif::DateOrder::dmy);
    const auto original=store.row({"candidate",0}).decision;
    check(original.disposition==imports::Disposition::unresolved,"Synthetic candidate requires a decision");
    store.create_account(2,"Foreign",Currency("USD"),"2026-01-01");store.select_account(2);
    imports::DraftRow manual{"2026-01-02","Manual","",Transaction::manual(Money(-250,Currency("USD")),{{"","",Money(-250,Currency("USD"))}})};
    store.create_manual("manual",manual);store.accept_all("acceptAll",true);store.verify();
    undo(store,"undoAll","acceptAll");
    check(!store.account().frozen && store.balance().units()==-250,"Foreign account returns to its pending state");
    store.select_account(1);
    check(store.row({"candidate",0}).decision.disposition==original.disposition && !store.account().frozen,"Ignore warnings Undo restores the original decision and freeze state");
    store.select_account(2);undo(store,"undoManual","manual");
    store.select_account(1);undo(store,"undoCandidate","candidate");undo(store,"undoFirst","first");store.certify({});
}

void relationships(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");
    TransferRequest request{};request.action=TransferAction::manual;request.link="manual-link";request.destination=2;request.amount=Money(-1000,Currency("CAD"));request.date="2026-01-01";request.expected_sequence=store.transfer_sequence();
    store.apply_transfer("transfer",request);store.verify();
    store.reassign_currency("transfer-currency",Currency("USD"));undo(store,"undoTransferCurrency","transfer-currency");
    request={};request.action=TransferAction::accept;request.link="manual-link";request.expected_sequence=store.transfer_sequence();
    store.apply_transfer("accept-side",request);store.verify();
    request.action=TransferAction::commit_local;request.expected_sequence=store.transfer_sequence();store.apply_transfer("commit-side",request);store.verify();
    store.ingest("bank",source("Bank"),qif::DateOrder::dmy);
    request={};request.action=TransferAction::propose_match;request.link="manual-link";request.expected_sequence=store.transfer_sequence();request.evidence=transfers::BankEvidence{{1,{"bank",0},"source-0"},Money(-1000,Currency("CAD")),"2026-01-01"};
    store.apply_transfer("match-bank",request);check(store.row({"bank",0}).matched_generated,"Direct match attaches bank source");store.verify();
    undo(store,"undoMatchBank","match-bank");check(!store.row({"bank",0}).matched_generated,"Undo match restores independent bank row");
    undo(store,"undoBank","bank");
    undo(store,"undoCommitSide","commit-side");undo(store,"undoAcceptSide","accept-side");undo(store,"undoTransfer","transfer");
    check(store.balance().units()==0,"Transfer chain restores balance");store.certify({});
}
void currencies(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("first",source("Original"),qif::DateOrder::dmy);
    edit(store,"beforeCurrency",{"first",0},"Before currency");
    store.reassign_currency("currencyA",Currency("USD"));
    edit(store,"currencyEdit",{"first",0},"Foreign currency");undo(store,"undoCurrencyEdit","currencyEdit");
    store.reassign_currency("currencyB",Currency("GBP"));undo(store,"undoCurrencyB","currencyB");undo(store,"undoCurrencyA","currencyA");
    check(store.balance()==Money(-1000,Currency("CAD")),"Currency chain restores original currency and amount");
    undo(store,"undoBeforeCurrency","beforeCurrency");store.verify();store.certify({});
}
void repair(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("bad","!Type:Bank\nD01/01/2026\nTbad\nPBroken\n^\n",qif::DateOrder::dmy);
    const imports::Occurrence id{"bad",0};check(!store.row(id).draft,"Repair fixture starts without a draft");
    imports::DraftRow fixed{"2026-01-01","Broken","",Transaction::repaired_import({},Money(-1000,Currency("CAD")),{{"","",Money(-1000,Currency("CAD"))}})};
    store.save(id,0,fixed);store.verify();const auto repair=store.action_head()->id;
    store.decide(id,1,{imports::Disposition::include,{}});const auto decision=store.action_head()->id;
    undo(store,"undoDecision",decision);undo(store,"undoRepair",repair);
    check(!store.row(id).draft && store.balance().units()==0,"Undo first repair returns the original failed row");
    undo(store,"undoBad","bad");store.certify({});
}

void grouped_import(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("earlier",source("Earlier"),qif::DateOrder::dmy);edit(store,"earlierEdit",{"earlier",0},"Saved earlier");
    const std::string bytes="!Account\nNImported account\nTBank\nB100.00\n^\n!Type:Bank\nD2026-01-02\nT-10\nPImported row\nLImported Food\n^\n";
    OperationContext capture;store.prepare_import_group("group",bytes,qif::DateOrder::dmy,{{0,2,AccountCreation{2,"Imported account",Currency("CAD"),"2026-01-01"},true}},capture);
    OperationContext materialize;store.resume_import_group("group",materialize);check(store.action_preparing(),"Initial category choice remains part of import");
    bool blocked=false;try{store.correct_opening("blocked","2026-01-01",Money(1,Currency("CAD")));}catch(const std::exception&){blocked=true;}check(blocked,"Preparing import blocks unrelated writes");
    OperationContext choose;store.resolve_import_categories("group",{"Imported Food"},choose);check(!store.action_preparing() && store.action_head()->id=="group","Category choice publishes one import action");
    store.select_account(2);edit(store,"groupEdit",{"group-account-0",0},"Edited import");undo(store,"undoGroupEdit","groupEdit");undo(store,"undoGroup","group");
    check(store.account().opening==Money(10000,Currency("CAD")) && store.accounts().size()==2 && store.category_catalog().categories.size()==1,"Import Undo retains created account, opening and category");
    store.select_account(1);undo(store,"undoEarlierEdit","earlierEdit");check(store.row({"earlier",0}).draft->payee=="Earlier","Earlier action remains reachable after grouped import removal");store.verify();store.certify({});
}

void composed_rules(const std::filesystem::path& path) {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    if(!store.rules_supported())return;
    store.ingest("first",source("Original"),qif::DateOrder::dmy);
    store.apply_category_change("category",{CategoryAction::create,"food","Food",{},CategoryKind::leaf},store.category_catalog().revision);
    RuleDefinitionImage rule;rule.identity="food-rule";rule.label="Food";rule.enabled=true;rule.expression="Original";rule.actions.category="food";
    AccountRuleRequest request{store.account().identity,store.ledger_edit_generation(),store.rule_image().revision,store.category_catalog().revision,false,false,rule.identity};
    OperationContext context;
    store.save_and_apply_rules("save-apply",{{rule}},request.ruleset_revision,request,context);
    check(store.rule_image().ordered.size()==1 && store.row({"first",0}).draft->transaction.allocations().front().category=="Food","Save and apply saves both halves");
    store.verify();undo(store,"undoApply","save-apply");
    check(store.rule_image().ordered.size()==1 && store.row({"first",0}).draft->transaction.allocations().front().category.empty(),"Undo application retains saved rule");
    const auto head=store.action_head();
    store.save_and_apply_rules("save-apply",{{rule}},request.ruleset_revision,request,context);
    check(store.action_head()->id==head->id && store.row({"first",0}).draft->transaction.allocations().front().category.empty(),"Retry after Undo cannot apply again");
    auto invalid=rule;invalid.actions.category="missing";request.generation=store.ledger_edit_generation();request.ruleset_revision=store.rule_image().revision;
    bool rejected=false;try{OperationContext failure;store.save_and_apply_rules("bad-save-apply",{{invalid}},request.ruleset_revision,request,failure);}catch(const std::exception&){rejected=true;}
    check(rejected && store.rule_image().ordered.front().actions.category=="food" && store.action_head()->id==head->id,"Invalid composed request saves neither half");
    store.verify();store.certify({});
}

void acceptance_faults(const std::filesystem::path& directory) {
    fault_vfs::install();
    for(const bool linked:{false,true}){
    const auto seed=directory/(linked?"linked-seed.db":"seed.db");
    {auto store=ImportStore::create(seed,"synthetic","Checking",Currency("CAD"),"2026-01-01");store.ingest("first",source("Original"),qif::DateOrder::dmy);
     if(linked){store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");TransferRequest request{};request.action=TransferAction::manual;request.link="transfer-link";request.destination=2;request.amount=Money(-1000,Currency("CAD"));request.date="2026-01-01";request.expected_sequence=store.transfer_sequence();store.apply_transfer("transfer",request);}
     store.accept_all("accept",true);}
    for(const bool crash:{false,true})for(int boundary=1;boundary<1000;++boundary){
        const auto file=directory/(std::string(crash?"crash-":"io-")+std::to_string(boundary));std::filesystem::copy_file(seed,file);
        bool fired=false;
        if(crash){const auto child=fork();check(child>=0,"fork failed");if(child==0){try{auto store=ImportStore::open(file,"synthetic");const auto preview=store.preview_undo("accept",store.action_token());fault_vfs::arm(boundary,SQLITE_IOERR_WRITE,true,true);store.undo_action("undoAccept",preview);_exit(0);}catch(...){_exit(72);}}int status=0;check(waitpid(child,&status,0)==child,"wait failed");check(WIFEXITED(status) && (WEXITSTATUS(status)==0 || WEXITSTATUS(status)==71),"Unexpected crash child outcome");fired=WEXITSTATUS(status)==71;
        }else{{auto store=ImportStore::open(file,"synthetic");const auto preview=store.preview_undo("accept",store.action_token());fault_vfs::arm(boundary,boundary%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{store.undo_action("undoAccept",preview);}catch(const std::exception&){}fired=fault_vfs::fired;fault_vfs::disarm();}}
        {auto recovered=ImportStore::open(file,"synthetic");recovered.verify();const auto accepted=recovered.transaction_accepted({"first",0});check(recovered.action_head()->id==(accepted?"accept":linked?"transfer":"first"),"Acceptance and Undo head must commit atomically");check(recovered.balance().units()==(linked?-2000:-1000) && recovered.row({"first",0}).draft->payee=="Original","Failure preserves financial state and source");if(accepted)undo(recovered,"retryUndo","accept");recovered.certify({});}
        std::filesystem::remove(file);if(!fired){std::cout<<(crash?"Crash":"I/O")<<(linked?" linked":" plain")<<" acceptance Undo boundaries: "<<boundary-1<<'\n';break;}check(boundary<999,"Fault sweep did not terminate");
    }
    }
}

void composed_faults(const std::filesystem::path& directory) {
    fault_vfs::install();const auto seed=directory/"composed-seed.db";
    {auto store=ImportStore::create(seed,"synthetic","Checking",Currency("CAD"),"2026-01-01");if(!store.rules_supported())return;store.ingest("first",source("Original"),qif::DateOrder::dmy);store.apply_category_change("food",{CategoryAction::create,"food","Food",{},CategoryKind::leaf},store.category_catalog().revision);}
    const auto apply=[](ImportStore& store){RuleDefinitionImage rule;rule.identity="food-rule";rule.label="Food";rule.expression="Original";rule.actions.category="food";AccountRuleRequest request{store.account().identity,store.ledger_edit_generation(),store.rule_image().revision,store.category_catalog().revision,false,false,rule.identity};OperationContext context;store.save_and_apply_rules("save-apply",{{rule}},request.ruleset_revision,request,context);};
    for(const bool crash:{false,true})for(int boundary=1;boundary<1000;++boundary){
        const auto file=directory/(std::string(crash?"compose-crash-":"compose-io-")+std::to_string(boundary));std::filesystem::copy_file(seed,file);bool fired=false;
        if(crash){const auto child=fork();check(child>=0,"fork failed");if(child==0){try{auto store=ImportStore::open(file,"synthetic");fault_vfs::arm(boundary,SQLITE_IOERR_WRITE,true,true);apply(store);_exit(0);}catch(...){_exit(72);}}int status=0;check(waitpid(child,&status,0)==child,"wait failed");check(WIFEXITED(status) && (WEXITSTATUS(status)==0 || WEXITSTATUS(status)==71),"Unexpected composed crash outcome");fired=WEXITSTATUS(status)==71;}
        else{{auto store=ImportStore::open(file,"synthetic");fault_vfs::arm(boundary,boundary%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(store);}catch(const std::exception&){}fired=fault_vfs::fired;fault_vfs::disarm();}}
        {auto recovered=ImportStore::open(file,"synthetic");recovered.verify();const bool saved=!recovered.rule_image().ordered.empty();const bool applied=recovered.row({"first",0}).draft->transaction.allocations().front().category=="Food";check(saved==applied && recovered.action_head()->id==(saved?"save-apply":"first"),"Definition and application commit atomically");if(!saved)apply(recovered);undo(recovered,"undoApply","save-apply");check(recovered.rule_image().ordered.size()==1 && recovered.balance().units()==-1000,"Composed Undo retains definition and money");recovered.certify({});}
        std::filesystem::remove(file);if(!fired){std::cout<<(crash?"Crash":"I/O")<<" composed boundaries: "<<boundary-1<<'\n';break;}check(boundary<999,"Composed sweep did not terminate");
    }
}

app::AccountHistoryPage history_page(ImportStore& store,std::int64_t account,std::optional<imports::Occurrence> focus={}) {
    return store.account_change_history(account,0,50,focus);
}
const app::HistoryActionView* find_action(const app::AccountHistoryPage& page,std::string_view id) {
    for(const auto& action:page.actions) if(action.id==id) return &action;
    return nullptr;
}
void rename_payees(ImportStore& store,std::string_view operation,const std::vector<imports::Occurrence>& selected,std::string_view payee) {
    OperationContext context;
    store.edit_transactions(operation,{LedgerSelectionAction::rename_payee,store.account().identity,store.ledger_edit_generation(),selected,std::string(payee),false,{},false},context);
}
void change_history(const std::filesystem::path& path) {
    {
        auto store=ImportStore::create(path.string()+"-dirty","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("keep",source("Keep"),qif::DateOrder::dmy);
    }
    {auto store=ImportStore::open(path.string()+"-dirty","synthetic");
     check(store.action_head() && store.action_head()->label=="Import file","Uncertified reopen keeps the chain");
     const auto page=history_page(store,store.account().identity);
     check(page.actions.size()==1 && page.actions.front().count==1,"History remains after an uncertified close");
     store.certify({});}
    std::string payee;
    {
    auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");
    std::string rows="!Type:Bank\n";
    for(int i=0;i<4;++i) rows+="D01/01/2026\nT-10.00\nPOriginal\n^\n";
    store.ingest("four",rows,qif::DateOrder::dmy);
    auto imported=history_page(store,1);
    check(imported.actions.size()==1 && imported.actions.front().label=="Import file" && imported.actions.front().count==4,"Four imported rows are one Import file action");
    for(const auto& action:imported.actions) check(action.label!="Correct opening","Creation opening is not an Undo action");
    std::vector<imports::Occurrence> four;
    for(int i=0;i<4;++i) four.push_back({"four",static_cast<std::size_t>(i)});
    rename_payees(store,"rename4",four,"Renamed");
    auto renamed=history_page(store,1);
    check(renamed.actions.front().id=="rename4" && renamed.actions.front().label=="Rename payee" && renamed.actions.front().count==4,"One rename is one row");
    check(renamed.actions.front().changes.size()==1 && renamed.actions.front().changes.front().field=="Payee" && renamed.actions.front().changes.front().before_text=="Original" && renamed.actions.front().changes.front().after_text=="Renamed","Payee before and after");
    check(renamed.actions.front().balance.change.units==0 && !renamed.actions.front().balance.change.definition.empty(),"Payee-only balance change is zero");
    check(renamed.actions.front().members.size()==4,"The rename lists its transactions");
    for(const auto& member:renamed.actions.front().members) check(member.state=="New","New transactions stay marked New");
    bool import_listed=false;
    for(const auto& action:renamed.actions) if(action.label=="Import file"){import_listed=true;check(!action.lists_transactions && action.members.empty(),"Import does not list each transaction");}
    check(import_listed && store.action_head()->id=="rename4","Undo is available only while the rename is newest");
    edit(store,"one",{"four",0},"One");
    auto later=history_page(store,1);
    check(later.actions.front().id=="one" && store.action_head()->id=="one" && find_action(later,"rename4"),"A later edit hides Undo of the rename");
    undo(store,"undoOne","one");
    auto restored=history_page(store,1);
    check(store.action_head()->id=="rename4" && !find_action(restored,"one"),"Undoing the edit restores the rename and drops the edit");
    store.accept_all("accept4",true);
    auto accepted=history_page(store,1);
    check(accepted.actions.front().label=="Accept transactions" && !accepted.actions.front().lists_transactions,"Acceptance is its own row");
    const auto* earlier=find_action(accepted,"rename4");
    check(earlier && earlier->members.size()==4,"The earlier rename remains");
    for(const auto& member:earlier->members) check(member.state=="Accepted","Accepted mark follows the transaction");
    if(store.bulk_supported()) {
        store.apply_category_change("food",{CategoryAction::create,"food","Food",{},CategoryKind::leaf},store.category_catalog().revision);
        const auto preview=store.preview_bulk({ChangeKind::correct,four,"food",{}});
        store.apply_bulk("bulk-food",preview);
        const auto bulk=history_page(store,1);
        check(bulk.actions.front().label=="Change category" && bulk.actions.front().id=="bulk-food","A bulk category change is one row");
        check(store.action_head()->label=="Edit transactions" && store.action_display_label(*store.action_head())=="Change category","Display label replaces the stored bulk label");
        bool category=false;for(const auto& change:bulk.actions.front().changes) if(change.field=="Category" && change.after_text=="Food") category=true;
        check(category,"Category before and after");
    }
    store.ingest("extra",source("Fresh"),qif::DateOrder::dmy);
    rename_payees(store,"mixed",{{"extra",0},{"four",0}},"Mixed");
    auto mixed=history_page(store,1);
    check(mixed.actions.front().id=="mixed" && mixed.actions.front().count==2,"Mixed New and Accepted rename is one row");
    bool saw_new=false,saw_accepted=false;
    for(const auto& member:mixed.actions.front().members){if(member.state=="New")saw_new=true;if(member.state=="Accepted")saw_accepted=true;}
    check(saw_new && saw_accepted,"Each transaction keeps its own mark");
    store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");
    const std::string both="!Account\nNChecking\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-10.00\nPLeft\n^\n!Account\nNSavings\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-5.00\nPRight\n^\n";
    OperationContext capture,materialize;store.prepare_import_group("both",both,qif::DateOrder::dmy,{{0,1,{}},{1,2,{}}},capture);store.resume_import_group("both",materialize);
    auto checking=history_page(store,1);const auto* import_both=find_action(checking,"both");
    check(import_both && import_both->label=="Import file" && import_both->count==2 && !import_both->lists_transactions,"One import names every account and lists no transactions");
    check(import_both->accounts.size()==2 && import_both->accounts[0]=="Checking" && import_both->accounts[1]=="Savings","Both accounts are named");
    check(find_action(history_page(store,2),"both"),"The import appears in the other account");
    store.select_account(2);edit(store,"other",{"both-account-1",0},"Other side");store.select_account(1);
    auto hidden=history_page(store,1);
    check(store.action_head()->id=="other" && !find_action(hidden,"other"),"A newer action in another account is not a row here");
    check(history_page(store,1,imports::Occurrence{"four",0}).focus=="mixed","History focus selects the newest action that includes the transaction");
    bool refused=false;
    for(const auto& change:store.committed_changes()) {
        if(change.identity!=imports::Occurrence{"four",0}) continue;
        try {store.preview_committed_change({ChangeKind::undo,change.identity,{},change.operation});}
        catch(const storage::ImportError& error) {if(std::string(error.what()).find("cannot be undone individually")!=std::string::npos){refused=true;break;}}
    }
    check(refused,"A member of a selection cannot be undone alone");
    if(store.rules_supported()) {
        if(!store.bulk_supported()) store.apply_category_change("food",{CategoryAction::create,"food","Food",{},CategoryKind::leaf},store.category_catalog().revision);
        store.ingest("cafeearly",source("Cafe"),qif::DateOrder::dmy);
        RuleDefinitionImage rule;rule.identity="cafe-rule";rule.label="Cafe";rule.enabled=true;rule.expression="Cafe";rule.actions.category="food";
        OperationContext saved;store.save_rules("save-cafe-rule",{{rule}},store.rule_image().revision,saved);
        OperationContext cafe_capture,cafe_resume;
        store.prepare_import_group("cafegroup","!Type:Bank\nD02/01/2026\nT-3.00\nPCafe\n^\n",qif::DateOrder::dmy,{{0,1,{}}},cafe_capture);
        store.resume_import_group("cafegroup",cafe_resume);
        const auto during=history_page(store,1);
        for(const auto& action:during.actions) check(action.kind!=ActionKind::rule_application,"Rules that run during an import are not their own row");
        check(store.action_head()->id=="cafegroup","The import remains the newest action");
        check(store.row({"cafegroup-account-0",0}).draft->transaction.allocations().front().category=="Food","The saved rule still runs during the import");
        AccountRuleRequest request{store.account().identity,store.ledger_edit_generation(),store.rule_image().revision,store.category_catalog().revision,false,false,rule.identity};
        OperationContext apply;store.save_and_apply_rules("apply-cafe",{{rule}},request.ruleset_revision,request,apply);
        const auto applied=history_page(store,1);
        check(applied.actions.front().id=="apply-cafe" && applied.actions.front().label=="Apply rules","A later Apply rules action is its own row");
        undo(store,"undoCafe","apply-cafe");
        check(store.rule_image().ordered.size()==1 && store.row({"cafeearly",0}).draft->transaction.allocations().front().category.empty(),"Undo keeps the saved rule and restores the transaction");
    }
    bool invalid=false;try{store.account_change_history(1,-1,50,{});}catch(const storage::ImportError&){invalid=true;}check(invalid,"Invalid Undo history page");
    bool missing=false;try{store.account_change_history(99,0,50,{});}catch(const storage::ImportError& error){missing=std::string(error.what()).find("not in the file")!=std::string::npos;}check(missing,"Missing account is refused");
    payee=store.committed_entry({"four",0}).current.draft.payee;
    const auto backup=path.string()+"-backup";store.backup_to(backup,"synthetic");
    {auto restored_file=ImportStore::open(backup,"synthetic");check(restored_file.action_head() && !history_page(restored_file,1).actions.empty(),"Opening a backup keeps the list");}
    store.certify({});
    }
    {auto reopened=ImportStore::open(path,"synthetic");check(!reopened.action_head() && history_page(reopened,1).actions.empty(),"Close and reopen clears the list");check(reopened.committed_entry({"four",0}).current.draft.payee==payee,"The payee change remains");}
}

std::string payee_of(ImportStore& store,const imports::Occurrence& id) {
    if(store.transaction_accepted(id))return store.committed_entry(id).current.draft.payee;
    return store.row(id).draft->payee;
}
std::string bank_rows(std::initializer_list<const char*> payees) {
    std::string rows="!Type:Bank\n";
    for(const auto* payee:payees) rows+=std::string("D01/01/2026\nT-10.00\nP")+payee+"\n^\n";
    return rows;
}
bool refuses_latest(ImportStore& store,std::string_view id) {
    try{store.preview_undo(id,store.action_token());}catch(const storage::ImportError& error){return std::string(error.what()).find("Undo the latest action first.")!=std::string::npos;}
    return false;
}
bool import_listed(ImportStore& store,std::string_view source) {
    const auto imports=store.undoable_imports();
    return std::any_of(imports.begin(),imports.end(),[&](const auto& item){return item.source==source;});
}
void held_import_undo(const std::filesystem::path& directory) {
{
    auto store=ImportStore::create(directory/"held-import.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
    const std::string coffee="D01/02/2026\nT-1\nPCoffee\n^\n";
    const std::string changed="D01/02/2026\nT-1.00\nPCoffee\nMAnother export\n^\n";
    store.ingest("base",coffee,qif::DateOrder::dmy);
    store.accept_all("accept",true);
    const auto beforeHold=history_page(store,1);
    const auto* open=find_action(beforeHold,"base");
    check(open && open->undoable,"Import Undo is available before another import holds its transactions");
    store.ingest("again",changed,qif::DateOrder::dmy);
    check(store.row({"again",0}).decision.duplicate_of==imports::Occurrence{"base",0},"The later import holds the original transaction");
    const auto held=history_page(store,1);
    const auto* base=find_action(held,"base");
    const auto* again=find_action(held,"again");
    const auto* accept=find_action(held,"accept");
    check(base && !base->undoable && base->undo_wait=="Undo the later Import file before this can be undone.","Change history names the later import to undo first");
    check(again && again->undoable,"The holding import can be undone");
    check(accept && accept->undoable,"Acceptance can still be undone");
    const auto blocked=store.preview_connected_undo("base",store.action_token());
    check(blocked.refusal=="Undo the other import before undoing this change.","Undo and the button use the same refusal");
    const auto balance=store.balance();
    bool failed=false;
    try{store.undo_connected("blocked",blocked);}catch(const storage::ImportError& error){failed=std::string(error.what()).find("Undo the other import before undoing this change.")!=std::string::npos;}
    check(failed && store.balance()==balance && store.action_head() && store.action_head()->id=="again","A refused Undo writes nothing");
    const auto acceptance=store.preview_connected_undo("accept",store.action_token());
    check(acceptance.refusal.empty(),"Acceptance Undo does not remove the held import");
    store.undo_connected("undoAccept",acceptance);
    check(!store.transaction_accepted({"base",0}) && !import_listed(store,"base") && import_listed(store,"again"),"Review omits the import another import still holds");
    const auto stillHeld=history_page(store,1);
    const auto* still=find_action(stillHeld,"base");
    check(still && !still->undoable,"Change history still omits Undo after acceptance is undone");
    undo(store,"undoAgain","again");
    const auto released=history_page(store,1);
    const auto* releasedBase=find_action(released,"base");
    check(import_listed(store,"base") && releasedBase && releasedBase->undoable,"Undo returns after the other import is undone");
    const auto ready=store.preview_connected_undo("base",store.action_token());
    check(ready.refusal.empty() && ready.lines.back().id=="base","Confirming Undo removes the import");
    store.undo_connected("undoBase",ready);
    check(store.current_ledger().empty() && store.balance().units()==0,"The import leaves the ledger");
    store.verify();
    store.certify({});
}
    auto reopened=ImportStore::open(directory/"held-import.db","synthetic");
    check(reopened.current_ledger().empty() && reopened.balance().units()==0 && reopened.batches().empty(),"Close collects the retired import");
    reopened.verify();
}
void older_acceptance_undo(const std::filesystem::path& directory) {
    auto store=ImportStore::create(directory/"older-accept.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.ingest("first","!Type:Bank\nD01/01/2026\nT-10.00\nPFirst\n^\nD02/01/2026\nT-5.00\nPSecond\n^\n",qif::DateOrder::dmy);
    store.accept_all("acceptFirst",true);
    check(store.account().frozen,"Acceptance freezes the account");
    store.ingest("second","D03/01/2026\nT-2.00\nPLater\n^\n",qif::DateOrder::dmy);
    store.accept_all("acceptSecond",true);
    const auto preview=store.preview_connected_undo("acceptFirst",store.action_token());
    bool includesLater=false;for(const auto& line:preview.lines)if(line.id=="acceptSecond")includesLater=true;
    check(preview.refusal.empty() && !includesLater,"The earlier acceptance undoes without the later one");
    const auto page=history_page(store,1);
    const auto* earlier=find_action(page,"acceptFirst");
    check(earlier && earlier->undoable && earlier->undo_wait.empty(),"The earlier acceptance can be undone");
    store.undo_connected("undoOlder",preview);
    check(!store.transaction_accepted({"first",0}) && !store.transaction_accepted({"first",1}),"The earlier transactions return to New");
    check(store.transaction_accepted({"second",0}) && store.account().frozen,"The later acceptance keeps the account frozen");
    store.verify();
    store.undo_connected("undoLater",store.preview_connected_undo("acceptSecond",store.action_token()));
    check(!store.transaction_accepted({"second",0}) && !store.account().frozen,"The last acceptance thaws the account");
    store.verify();
}
void older_acceptance_undo_many_accounts(const std::filesystem::path& directory) {
    auto store=ImportStore::create(directory/"many-accept.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");
    store.create_account(3,"Card",Currency("CAD"),"2026-01-01");
    const std::string first="!Account\nNChecking\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-10.00\nPOne\n^\nD02/01/2026\nT-4.00\nPTwo\n^\n!Account\nNSavings\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-5.00\nPSave\n^\n!Account\nNCard\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-7.00\nPCard\n^\n";
    OperationContext capture,materialize;
    store.prepare_import_group("wide",first,qif::DateOrder::dmy,{{0,1,{}},{1,2,{}},{2,3,{}}},capture);
    store.resume_import_group("wide",materialize);
    store.accept_all("acceptWide",true);
    for(const auto id:{(std::int64_t)1,(std::int64_t)2,(std::int64_t)3}){store.select_account(id);check(store.account().frozen,"Each accepted account is frozen");}
    store.select_account(1);
    store.ingest("later","D03/01/2026\nT-2.00\nPLater\n^\n",qif::DateOrder::dmy);
    store.accept_all("acceptLater",true);
    const auto preview=store.preview_connected_undo("acceptWide",store.action_token());
    bool includesLater=false;for(const auto& line:preview.lines)if(line.id=="acceptLater")includesLater=true;
    check(preview.refusal.empty() && !includesLater && find_action(history_page(store,1),"acceptWide")->undoable,"The wide acceptance can be undone on its own");
    store.undo_connected("undoWide",preview);
    check(!store.transaction_accepted({"wide-account-0",0}) && !store.transaction_accepted({"wide-account-1",0}) && !store.transaction_accepted({"wide-account-2",0}),"The earlier transactions return to New");
    check(store.transaction_accepted({"later",0}),"The later checking transaction stays accepted");
    store.select_account(1);check(store.account().frozen,"Checking stays frozen");
    store.select_account(2);check(!store.account().frozen,"Savings thaws");
    store.select_account(3);check(!store.account().frozen,"Card thaws");
    store.verify();
}
void held_import_other_account(const std::filesystem::path& directory) {
    auto store=ImportStore::create(directory/"held-other.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
    store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");
    const std::string coffee="D01/02/2026\nT-1\nPCoffee\n^\n";
    store.ingest("base",coffee,qif::DateOrder::dmy);
    store.accept_all("accept",true);
    store.select_account(2);
    store.ingest("other",coffee,qif::DateOrder::dmy);
    const auto row=store.row({"other",0});
    const auto batches=store.batches();
    const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& value){return value.id==row.identity.batch;});
    check(batch!=batches.end(),"Missing savings import");
    bool linked=false;
    for(const auto& candidate:row.candidates){
        if(candidate.batch.find("base")==std::string::npos)continue;
        store.decide(row.identity,batch->revision,{imports::Disposition::duplicate,candidate});
        linked=true;break;
    }
    store.select_account(1);
    const auto page=history_page(store,1);
    const auto* base=find_action(page,"base");
    check(base,"The checking import stays on its account");
    if(!linked){check(base->undoable,"Another account's separate import does not block Undo");return;}
    check(!base->undoable && base->undo_wait.find("Switch to Savings")!=std::string::npos && base->undo_wait.find("undo")!=std::string::npos,"The row names the other account when its import is not in this list");
    check(!find_action(page,"other"),"The other account's import is not a row here");
}
void connected_undo(const std::filesystem::path& directory) {
    {
        auto store=ImportStore::create(directory/"separate.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("four",bank_rows({"A","B","C","D"}),qif::DateOrder::dmy);
        rename_payees(store,"r0",{{"four",0}},"A1");
        rename_payees(store,"r1",{{"four",1}},"B1");
        rename_payees(store,"r2",{{"four",2}},"C1");
        rename_payees(store,"r3",{{"four",3}},"D1");
        const auto preview=store.preview_connected_undo("r0",store.action_token());
        check(preview.lines.size()==1 && preview.lines.front().id=="r0","Separate payee groups undo alone");
        store.observer([&](storage::ImportPhase phase){if(phase==storage::ImportPhase::before_commit)throw std::runtime_error("stop before save");});
        bool stopped=false;try{store.undo_connected("stopSeparate",preview);}catch(const std::runtime_error&){stopped=true;}
        store.observer({});
        check(stopped && payee_of(store,{"four",0})=="A1" && store.action_head()->id=="r3","A refused save leaves every payee applied");
        store.verify();
        store.undo_connected("undoSeparate",store.preview_connected_undo("r0",store.action_token()));
        check(payee_of(store,{"four",0})=="A" && payee_of(store,{"four",1})=="B1" && payee_of(store,{"four",2})=="C1" && payee_of(store,{"four",3})=="D1","Only the chosen payee returns");
        check(store.action_head()->id=="r3" && refuses_latest(store,"r0"),"Older Undo stays off the newest-action command");
        const auto page=history_page(store,1);
        check(find_action(page,"r1") && find_action(page,"r2") && find_action(page,"r3") && !find_action(page,"r0"),"The undone rename leaves the chain");
        undo(store,"undoNewest","r3");
        check(payee_of(store,{"four",3})=="D" && store.action_head()->id=="r2","The newest action still undoes by itself");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"chain.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("src",bank_rows({"A","B","C","D","E"}),qif::DateOrder::dmy);
        edit(store,"c1",{"src",2},"C1");
        rename_payees(store,"abc",{{"src",0},{"src",1},{"src",2}},"Group");
        edit(store,"d1",{"src",3},"D1");
        rename_payees(store,"bc",{{"src",1},{"src",2}},"Later");
        edit(store,"b3",{"src",1},"B3");
        edit(store,"e1",{"src",4},"E1");
        edit(store,"c4",{"src",2},"C4");
        store.correct_opening("open1","2026-01-01",Money(500,Currency("CAD")));
        const auto preview=store.preview_connected_undo("c1",store.action_token());
        std::vector<std::string> ids;for(const auto& line:preview.lines)ids.push_back(line.id);
        check(ids==std::vector<std::string>{"c4","b3","bc","abc","c1"},"Connected Undo lists the shared transactions newest first");
        store.undo_connected("undoChain",preview);
        check(payee_of(store,{"src",0})=="A" && payee_of(store,{"src",1})=="B" && payee_of(store,{"src",2})=="C","Shared payees return to the imported text");
        check(payee_of(store,{"src",3})=="D1" && payee_of(store,{"src",4})=="E1","Unrelated payees stay");
        check(store.action_head()->id=="open1" && store.account().opening.units()==500,"An opening that wrote no transaction stays applied");
        const auto page=history_page(store,1);
        check(find_action(page,"open1") && find_action(page,"e1") && find_action(page,"d1") && find_action(page,"src") && !find_action(page,"c1"),"Unrelated actions remain on the page");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"accept.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("one",source("Pay"),qif::DateOrder::dmy);
        rename_payees(store,"ren",{{"one",0}},"Renamed");
        store.accept_all("accept",true);
        const auto preview=store.preview_connected_undo("ren",store.action_token());
        check(preview.lines.size()==2 && preview.lines[0].id=="accept" && preview.lines[1].id=="ren","Acceptance after a rename is on the list");
        store.undo_connected("undoRenamed",preview);
        check(!store.transaction_accepted({"one",0}) && payee_of(store,{"one",0})=="Pay" && store.action_head()->id=="one","Undo returns the transaction to New and keeps the import");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"stale.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("two",bank_rows({"Keep","Other"}),qif::DateOrder::dmy);
        rename_payees(store,"keep",{{"two",0}},"Kept");
        const auto preview=store.preview_connected_undo("keep",store.action_token());
        edit(store,"other",{"two",1},"Edited");
        bool stale=false;try{store.undo_connected("staleUndo",preview);}catch(const storage::ImportError& error){stale=std::string(error.what()).find("Refresh the action and try again.")!=std::string::npos;}
        check(stale && payee_of(store,{"two",0})=="Kept" && payee_of(store,{"two",1})=="Edited" && store.action_head()->id=="other","A stale Undo writes nothing");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"import.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("src",bank_rows({"Left","Right"}),qif::DateOrder::dmy);
        edit(store,"editLeft",{"src",0},"Changed");
        imports::DraftRow manual{"2026-01-02","Kept manual","",Transaction::manual(Money(-250,Currency("CAD")),{{"","",Money(-250,Currency("CAD"))}})};
        store.create_manual("kept",manual);
        const auto preview=store.preview_connected_undo("src",store.action_token());
        check(preview.lines.size()==2 && preview.lines[0].id=="editLeft" && preview.lines[1].id=="src","An import closure includes its later edit only");
        store.undo_connected("undoSrc",preview);
        check(store.action_head() && store.action_head()->id=="kept" && payee_of(store,{"kept",0})=="Kept manual","The unrelated manual remains");
        bool removed=false;try{const auto row=store.row({"src",0});removed=!row.draft.has_value();}catch(...){removed=true;}
        check(removed,"Undoing the import removes its rows");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"layers.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("src",bank_rows({"A","B","C","D","E","F","G"}),qif::DateOrder::dmy);
        store.accept_all("accept",true);
        std::vector<imports::Occurrence> all;
        for(std::size_t i=0;i<7;++i) all.push_back({"src",i});
        rename_payees(store,"base",all,"First");
        edit(store,"layer1",{"src",0},"Second");
        edit(store,"layer2",{"src",0},"Third");
        const auto preview=store.preview_connected_undo("base",store.action_token());
        std::vector<std::string> ids;for(const auto& line:preview.lines)ids.push_back(line.id);
        check(ids==std::vector<std::string>{"layer2","layer1","base"},"Two later edits of an accepted selection stay on its Undo list");
        store.undo_connected("undoLayers",preview);
        const char* restored[]={"A","B","C","D","E","F","G"};
        for(std::size_t i=0;i<7;++i) check(payee_of(store,{"src",i})==restored[i],"Accepted selection and its later edits return together");
        check(store.action_head() && store.action_head()->id=="accept","Acceptance stays after the selection Undo");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"layers-top.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.ingest("src",bank_rows({"A","B","C","D","E","F","G"}),qif::DateOrder::dmy);
        store.accept_all("accept",true);
        edit(store,"older",{"src",0},"Older");
        std::vector<imports::Occurrence> all;
        for(std::size_t i=0;i<7;++i) all.push_back({"src",i});
        rename_payees(store,"newer",all,"Newer");
        const auto preview=store.preview_connected_undo("older",store.action_token());
        check(preview.lines.size()==2 && preview.lines[0].id=="newer" && preview.lines[1].id=="older","A later accepted selection is on the earlier edit's list");
        store.undo_connected("undoOlder",preview);
        const char* restored[]={"A","B","C","D","E","F","G"};
        for(std::size_t i=0;i<7;++i) check(payee_of(store,{"src",i})==restored[i],"Undoing the earlier edit also restores the later selection");
        check(store.action_head() && store.action_head()->id=="accept","Acceptance stays after the layered Undo");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory/"grouped-accept.db","synthetic","Checking",Currency("CAD"),"2026-01-01");
        store.create_account(2,"Savings",Currency("CAD"),"2026-01-01");
        const std::string qif="!Account\nNChecking\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-10.00\nPLeft\n^\n!Account\nNSavings\nTBank\n^\n!Type:Bank\nD01/01/2026\nT-5.00\nPRight\n^\n";
        OperationContext capture,materialize;
        store.prepare_import_group("both",qif,qif::DateOrder::dmy,{{0,1,{}},{1,2,{}}},capture);
        store.resume_import_group("both",materialize);
        store.accept_all("accept",true);
        check(store.current_ledger().size()==1,"Accepted grouped import is in the checking ledger");
        store.select_account(2);
        check(store.current_ledger().size()==1,"Accepted grouped import is in the savings ledger");
        store.select_account(1);
        const auto preview=store.preview_connected_undo("both",store.action_token());
        check(preview.lines.size()==2 && preview.lines[0].id=="accept" && preview.lines[1].id=="both","Accepted grouped import undo includes Accept");
        store.undo_connected("undoGrouped",preview);
        check(store.current_ledger().empty() && !store.action_head(),"Undoing the accepted grouped import removes it");
        store.select_account(2);
        check(store.current_ledger().empty(),"The other account loses the imported rows too");
        store.verify();
    }
}
void corrupt_lineage(const std::filesystem::path& directory) {
    int serial=0;
    for(const auto* sql:{"UPDATE review_undo_members SET consumed=0", "UPDATE review_undo_members SET original_child='missing'"}) {
        const auto path=directory/("corrupt-"+std::to_string(++serial));
        {auto store=ImportStore::create(path,"synthetic","Checking",Currency("CAD"),"2026-01-01");store.ingest("first",source("Original"),qif::DateOrder::dmy);edit(store,"A",{"first",0},"A");edit(store,"B",{"first",0},"B");undo(store,"undoB","B");}
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK,"Open synthetic corruption fixture");check(sqlite3_key(db,"synthetic",9)==SQLITE_OK,"Unlock synthetic fixture");
        check(sqlite3_exec(db,"DROP TRIGGER review_undo_members_immutable",nullptr,nullptr,nullptr)==SQLITE_OK,"Drop fixture trigger");check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK,"Alter synthetic lineage");check(sqlite3_close(db)==SQLITE_OK,"Close fixture");
        bool rejected=false;try{auto store=ImportStore::open(path,"synthetic");store.verify();}catch(const storage::ImportError&){rejected=true;}check(rejected,"Forged restoration lineage must fail recovery");
    }
}

}
int main(int argc,char** argv){
    auto path=(std::filesystem::temp_directory_path()/"spentit-action-repository-XXXXXX").string();
    if(!mkdtemp(path.data()))return 2;
    struct Cleanup{std::string path;~Cleanup(){std::filesystem::remove_all(path);}} cleanup{path};
    try{if(argc>1 && std::string_view(argv[1])=="composed_faults"){composed_faults(path);return 0;}if(argc>1 && std::string_view(argv[1])=="faults"){acceptance_faults(path);return 0;}if(argc>1 && std::string_view(argv[1])=="held_import"){held_import_undo(path);older_acceptance_undo(path);older_acceptance_undo_many_accounts(path);held_import_other_account(path);std::cout<<"Held import Undo passed\n";return 0;}corrupt_lineage(path);held_import_undo(path);older_acceptance_undo(path);older_acceptance_undo_many_accounts(path);held_import_other_account(path);grouped_import(std::filesystem::path(path)/"grouped.db");relationships(std::filesystem::path(path)/"relationships.db");currencies(std::filesystem::path(path)/"currencies.db");repair(std::filesystem::path(path)/"repair.db");composed_rules(std::filesystem::path(path)/"rules.db");run(std::filesystem::path(path)/"synthetic.db");committed(std::filesystem::path(path)/"committed.db");pending_acceptance(std::filesystem::path(path)/"pending.db");acceptance_decisions(std::filesystem::path(path)/"decisions.db");change_history(std::filesystem::path(path)/"history.db");connected_undo(path);
        {auto reopened=ImportStore::open(std::filesystem::path(path)/"committed.db","synthetic");reopened.verify();check(reopened.batches().empty() && !reopened.action_head(),"Retired import is collected at close");}
        std::cout<<"Repository action tests passed\n";}
    catch(const storage::ImportError& error){std::cerr<<error.what()<<" code="<<error.code()<<'\n';return 1;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
