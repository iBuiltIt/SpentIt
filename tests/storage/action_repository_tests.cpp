#include <spentit/import_store.hpp>
#include <algorithm>
#include <filesystem>
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
    try{if(argc>1 && std::string_view(argv[1])=="composed_faults"){composed_faults(path);return 0;}if(argc>1 && std::string_view(argv[1])=="faults"){acceptance_faults(path);return 0;}corrupt_lineage(path);grouped_import(std::filesystem::path(path)/"grouped.db");relationships(std::filesystem::path(path)/"relationships.db");currencies(std::filesystem::path(path)/"currencies.db");repair(std::filesystem::path(path)/"repair.db");composed_rules(std::filesystem::path(path)/"rules.db");run(std::filesystem::path(path)/"synthetic.db");committed(std::filesystem::path(path)/"committed.db");pending_acceptance(std::filesystem::path(path)/"pending.db");acceptance_decisions(std::filesystem::path(path)/"decisions.db");
        {auto reopened=ImportStore::open(std::filesystem::path(path)/"committed.db","synthetic");reopened.verify();check(reopened.batches().empty() && !reopened.action_head(),"Retired import is collected at close");}
        std::cout<<"Repository action tests passed\n";}
    catch(const storage::ImportError& error){std::cerr<<error.what()<<" code="<<error.code()<<'\n';return 1;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
