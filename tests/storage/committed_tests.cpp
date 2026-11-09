#include "session_test_helpers.hpp"
#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include "import_legacy_schema.hpp"
#include "import_schema3_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <sys/wait.h>

using namespace spentit;
using namespace spentit::storage;
namespace fs = std::filesystem;
namespace {
constexpr auto password = "synthetic-committed-password";
constexpr auto marker = "PRIVATE-SYNTHETIC-COMMITTED-510b";
const Currency cad("CAD");
const imports::Occurrence first{"first",0}, second{"first",1};
const std::string purchase = "D10/08/2026\nT-29.31\nPPRIVATE-SYNTHETIC-COMMITTED-510b\nEfee\n$-0.14\nEtransport\n$-29.17\n^\n";
const std::string source = purchase + "D11/08/2026\nT50\nPIncome\nNother\n^\n";
struct Directory {
    fs::path path;
    Directory() {
        auto pattern=(fs::temp_directory_path()/"spentit-committed-XXXXXX").string();
        auto* value=mkdtemp(pattern.data());
        if(!value) throw std::runtime_error("Temporary directory failed");
        path=fs::canonical(value);
    }
    ~Directory() {std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location& location=std::source_location::current()) {
    if(!value) throw std::runtime_error("Committed assertion failed at line "+std::to_string(location.line()));
}
template<class F> void rejects(F&& work,const std::source_location& location=std::source_location::current()) {
    bool rejected=false;
    try {work();} catch(const std::exception&) {rejected=true;}
    check(rejected,location);
}
std::string bytes(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(file),{}};
}
void no_leaks(const fs::path& dir) {
    for(const auto& file:fs::directory_iterator(dir)) if(file.is_regular_file()) {
        const auto data=bytes(file.path());
        check(data.find(password)==data.npos && data.find(marker)==data.npos && !data.starts_with("SQLite format 3"));
    }
}
ImportStore create(const fs::path& path) {return ImportStore::create(path,password,"Synthetic account",cad,"2026-08-01");}
void prepare(const fs::path& path) {
    auto store=create(path);store.opening(0,"2026-08-01",Money(10000,cad));
    store.ingest("first",source,qif::DateOrder::dmy);store.commit("first",0,"publication-first");
}
std::int64_t revision(const ImportStore& store,std::string_view batch) {
    for(const auto& value:store.batches()) if(value.id==batch)return value.revision;
    throw std::runtime_error("Missing synthetic batch");
}
app::ChangeRequest correction(ImportStore& store,const imports::Occurrence& id,std::int64_t total) {
    auto draft=store.committed_entry(id).current.draft;
    draft.transaction=draft.transaction.corrected(Money(total,cad),{{"Edited","synthetic correction",Money(total,cad)}});
    return {app::ChangeKind::correct,id,std::move(draft),{}};
}
app::ChangeRequest removal(const imports::Occurrence& id) {return {app::ChangeKind::remove,id,{}, {}};}
app::ChangeRequest undo(const imports::Occurrence& id,std::string operation) {return {app::ChangeKind::undo,id,{},std::move(operation)};}
app::ChangeReceipt apply(ImportStore& store,std::string_view id,const app::ChangeRequest& request) {
    const auto preview=store.preview_committed_change(request);
    return store.apply_committed_change(id,request,preview.expected,preview.preview_sequence);
}
bool eligible(const ImportStore& store,const imports::Occurrence& id,std::string_view operation) {
    for(const auto& item:store.committed_history(id))if(item.operation==operation)return item.undo_eligible;
    return false;
}
void sessions() {
    Directory dir;const auto path=dir.path/"doc";prepare(path);
    int audits=0;app::OperationContext context;
    context.progress=[&](std::string_view phase){if(phase=="Checking file")++audits;};
    std::optional<app::ChangePreview> original;
    {
        auto store=ImportStore::open(path,password,&context);check(audits==1);
        original=store.preview_committed_change(correction(store,first,-3000));
        store.apply_committed_change("session-edit",original->request,original->expected,original->preview_sequence);
        check(store.committed_changes().size()==1);
        store.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw std::runtime_error("synthetic close failure");});
        rejects([&]{store.certify({},&context);});
        check(store.committed_changes().size()==1);
        store.observer({});store.verify();
    }
    {
        audits=0;auto store=ImportStore::open(path,password,&context);check(audits==1);
        check(store.committed_changes().size()==1);
        store.ingest("pending","D12/08/2026\nT-5.00\nPPending synthetic\n^\n",qif::DateOrder::dmy);
        check(!store.undoable_imports().empty());
        audits=0;store.certify({},&context);check(audits==0);
        store.certify({},&context);check(audits==0);
    }
    for(int session=0;session<3;++session){
        audits=0;auto store=ImportStore::open_cached(path,password,dir.path/"missing-cache",&context);check(audits==0);
        check(store.committed_changes().empty() && store.committed_history(first).empty());
        check(store.committed_balance().units()==12000);
        check(store.committed_entry(first).current.draft.transaction.original_imported_amount()->units()==-2931);
        check(store.evidence("first")==source);
        check(!store.undoable_imports().empty());
        rejects([&]{store.preview_committed_change(undo(first,"session-edit"));});
        bool completed=false;try{store.apply_committed_change("session-edit",original->request,original->expected,original->preview_sequence);}catch(const app::OperationAlreadyCompleted& result){completed=result.operation=="session-edit";}check(completed);
        auto conflict=original->request;conflict.correction->memo="different request";
        bool refused=false;try{store.apply_committed_change("session-edit",conflict,original->expected,original->preview_sequence);}catch(const ImportError&){refused=true;}check(refused);
        audits=0;store.verify(&context);check(audits==1);
        store.certify({},&context);check(audits==1);
    }
    {
        auto store=ImportStore::open(path,password);
        const auto imports=store.undoable_imports();check(imports.size()==1);
        const auto preview=store.review_import_undo("pending");
        store.undo_import("undo-pending","pending",preview.token);
        check(store.undoable_imports().empty());store.verify();
        store.backup_to(dir.path/"backup",password);
        store.certify({});
    }
    {
        audits=0;auto backup=ImportStore::open(dir.path/"backup",password,&context);check(audits==1);
        check(backup.committed_changes().empty());backup.verify();backup.certify({});
    }
    no_leaks(dir.path);
}

std::int64_t count_rows(const fs::path& path,const char* sql) {
    sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
    sqlite3_stmt* row=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&row,nullptr)==SQLITE_OK);check(sqlite3_step(row)==SQLITE_ROW);const auto value=sqlite3_column_int64(row,0);check(sqlite3_finalize(row)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);return value;
}
void session_pruning() {
    Directory dir;const auto path=dir.path/"doc";prepare(path);
    for(int generation=0;generation<4;++generation){
        {
            auto s=ImportStore::open(path,password);
            for(int edit=0;edit<8;++edit){auto request=correction(s,first,-3000-edit);request.correction->memo="Expired synthetic memo "+std::to_string(generation*8+edit);apply(s,"edit-"+std::to_string(generation*8+edit),request);}
            s.ingest("draft-"+std::to_string(generation),"D12/08/2026\nT-1.00\nPPending "+std::to_string(generation)+"\n^\n",qif::DateOrder::dmy);
            const imports::Occurrence draft{"draft-"+std::to_string(generation),0};
            for(int edit=0;edit<8;++edit){auto row=*s.row(draft).draft;row.memo="Transient draft memo "+std::to_string(edit);s.save(draft,revision(s,draft.batch),row);}
            s.verify();const auto start=std::chrono::steady_clock::now();s.certify({});
            std::cout<<"baseline "<<generation+1<<": close ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<", file bytes "<<fs::file_size(path)<<'\n';
        }
        check(count_rows(path,"SELECT count(*) FROM entry_revisions WHERE batch='first' AND ordinal=0")==2);
        check(count_rows(path,"SELECT count(*) FROM entry_revisions WHERE revision>0 AND memo LIKE 'Expired synthetic memo %'")==1);
        check(count_rows(path,"SELECT count(*) FROM command_completions WHERE expired=1")==8*(generation+1));
        check(count_rows(path,"SELECT count(*) FROM sqlite_master WHERE name IN('snapshot_facts','allocation_origins')")==0);
        check(count_rows(path,"SELECT count(*) FROM ledger_operations WHERE kind IN(1,2,3)")==0);
        check(count_rows(path,"SELECT count(*) FROM events WHERE kind='save'")<=generation+1);
        {auto s=ImportStore::open(path,password);check(s.committed_changes().empty());check(s.committed_balance().units()==11993);s.verify();s.certify({});}
    }
    {auto s=ImportStore::open(path,password);s.backup_to(dir.path/"backup",password);}
    {auto s=ImportStore::open(dir.path/"backup",password);s.verify();check(s.committed_balance().units()==11993);}
    const auto currency_path=dir.path/"currency";prepare(currency_path);
    {auto s=ImportStore::open(currency_path,password);s.reassign_currency("sterling",Currency("GBP"));const auto e=s.committed_entry(first);auto d=e.current.draft;d.payee="Current currency baseline";d.payee_owner=FieldOwnership::manual;s.edit_transaction("currency-edit",first,revision(s,first.batch),e.token,d);s.verify();s.certify({});}
    check(count_rows(currency_path,"SELECT count(*) FROM account_amount_revisions a WHERE a.kind IN(2,3) AND NOT EXISTS(SELECT 1 FROM entry_revisions e WHERE e.batch=a.batch AND e.ordinal=a.ordinal AND e.revision=a.revision)")==0);
    {auto s=ImportStore::open(currency_path,password);check(s.committed_balance()==Money(12069,Currency("GBP")));s.verify();s.reassign_currency("cad-again",cad);s.verify();s.certify({});}
    {auto s=ImportStore::open(currency_path,password);check(s.committed_balance()==Money(12069,cad));s.verify();s.certify({});}
}

void session_editor() {
    Directory dir;const auto path=dir.path/"doc";prepare(path);
    std::optional<imports::DraftRow> original;std::optional<app::EntryToken> token;
    std::int64_t prior=0;app::LedgerSelectionEdit selected;
    {
        auto store=ImportStore::open(path,password);original=store.committed_entry(first).current.draft;
        original->memo="Expired editor text";token=store.committed_entry(first).token;prior=revision(store,first.batch);
        store.edit_transaction("editor",first,prior,token,*original);
        store.edit_transaction("editor",first,prior,token,*original);
        selected.action=app::LedgerSelectionAction::rename_payee;selected.account=store.account().identity;selected.generation=store.ledger_edit_generation();selected.selected={first};selected.payee="Expired selection payee";
        app::OperationContext context;store.edit_transactions("selection",selected,context);
        auto latest=store.committed_entry(first);auto next=latest.current.draft;next.memo="Current memo";next.payee="Current payee";
        store.edit_transaction("editor-last",first,revision(store,first.batch),latest.token,next);
        store.verify();store.certify({});
    }
    check(count_rows(path,"SELECT count(*) FROM entry_revisions WHERE memo='Expired editor text' OR payee='Expired selection payee'")==0);
    check(count_rows(path,"SELECT count(*) FROM review_answers WHERE question='ledger-edit-batch' AND request!=''")==0);
    {
        auto store=ImportStore::open(path,password);bool completed=false;
        try{store.edit_transaction("editor",first,prior,token,*original);}catch(const app::OperationAlreadyCompleted& e){completed=e.operation=="editor";}check(completed);
        auto changed=*original;changed.memo="Different request";bool conflict=false;try{store.edit_transaction("editor",first,prior,token,changed);}catch(const ImportError&){conflict=true;}check(conflict);
        app::OperationContext context;completed=false;try{store.edit_transactions("selection",selected,context);}catch(const app::OperationAlreadyCompleted&){completed=true;}check(completed);
        selected.payee="Conflicting payee";conflict=false;try{store.edit_transactions("selection",selected,context);}catch(const ImportError&){conflict=true;}check(conflict);
        apply(store,"remove",removal(first));store.certify({});
    }
    {
        auto store=ImportStore::open(path,password);store.ingest("removed-reimport",purchase,qif::DateOrder::dmy);
        check(store.row({"removed-reimport",0}).decision.disposition!=imports::Disposition::duplicate);
        check(!store.committed_entry(first).current.active);store.verify();store.certify({});
    }
    {auto store=ImportStore::open(path,password);store.verify();check(!store.committed_entry(first).current.active);}
}

void session_linked_pruning() {
    Directory dir;const auto path=dir.path/"linked";
    {auto s=ImportStore::create(path,password,"Checking",cad,"2026-01-01");s.create_account(2,"Savings",cad,"2026-01-01");
     s.ingest("out","D01/01/2026\nT-10\nPPayment\n^\n",qif::DateOrder::dmy);
     const auto row=s.row({"out",0});app::TransferRequest r{};r.action=app::TransferAction::retarget;r.mode=1;r.link="link";r.kept="kept";r.expected_sequence=s.transfer_sequence();r.batch_revision=revision(s,"out");
     r.left=transfers::AllocationFact{{1,{"out",0},row.draft->transaction.allocations().front().identity},Money(-1000,cad),row.draft->date,transfers::State::draft,true};r.generated=transfers::Endpoint{2,{"~transfer-link",0},"generated"};r.generated_currency=cad.definition_id();app::OperationContext c;s.assign_transfer_batch("assign",{r},c);s.accept_all("accept",true);s.certify({});}
    for(int session=0;session<3;++session){
        {auto s=ImportStore::open(path,password);for(int edit=0;edit<8;++edit){const auto e=s.committed_entry({"out",0});auto d=e.current.draft;d.memo="Linked memo "+std::to_string(session*8+edit);d.memo_owner=FieldOwnership::manual;s.edit_transaction("linked-"+std::to_string(session*8+edit),{"out",0},revision(s,"out"),e.token,d);}app::LedgerSelectionEdit deletion;deletion.action=app::LedgerSelectionAction::remove;deletion.account=1;deletion.generation=s.ledger_edit_generation();deletion.selected={{"out",0}};app::OperationContext c;const auto op="delete-"+std::to_string(session);s.edit_transactions(op,deletion,c);s.undo_transaction_edit(op+"-undo",op);s.verify();s.certify({});}
        check(count_rows(path,"SELECT count(*) FROM review_answers WHERE question='ledger-delete' OR question LIKE 'ledger-delete-%'")==0);
        check(count_rows(path,"SELECT count(*) FROM review_link_history h JOIN review_answers a ON a.id=h.answer WHERE a.question='ledger-edit'")<=1);
        check(count_rows(path,"SELECT count(*) FROM entry_revisions WHERE batch='out' AND ordinal=0")<=4);
        {auto s=ImportStore::open(path,password);check(s.committed_balance()==Money(-1000,cad));check(s.committed_changes().empty());s.verify();s.select_account(2);check(s.committed_balance()==Money(1000,cad));s.certify({});}
    }
}

void session_duplicates() {
    Directory dir;const auto path=dir.path/"doc";prepare(path);
    {
        auto store=ImportStore::open(path,password);
        apply(store,"correct",correction(store,first,-3000));
        store.certify({});
    }
    {
        auto store=ImportStore::open(path,password);
        store.ingest("duplicate",source,qif::DateOrder::dmy);
        check(store.committed_changes().empty());
        check(store.row({"duplicate",0}).decision.disposition==imports::Disposition::duplicate);
        store.verify();store.certify({});
    }
    {
        auto store=ImportStore::open(path,password);
        check(store.committed_changes().empty());
        check(store.row({"duplicate",0}).decision.disposition==imports::Disposition::duplicate);
        check(store.committed_balance().units()==12000);
        store.verify();
        const auto preview=store.review_import_undo("duplicate");
        store.undo_import("undo-duplicate","duplicate",preview.token);
        check(store.committed_balance().units()==12000);store.verify();
    }
}

void session_marker_faults() {
    fault_vfs::install();Directory dir;const auto seed=dir.path/"seed",path=dir.path/"trial";prepare(seed);
    {auto s=ImportStore::open(seed,password);s.certify({});}
    auto copy=[&]{std::error_code e;fs::remove(path,e);fs::remove(path.string()+"-journal",e);check(fs::copy_file(seed,path));};
    copy();fault_vfs::arm(-1);{auto s=ImportStore::open(path,password);}const int positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);
    for(int position=1;position<=positions;++position){
        copy();fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
        try{auto s=ImportStore::open(path,password);}catch(const std::exception&){}fault_vfs::disarm();
        {auto s=ImportStore::open(path,password);check(s.committed_balance().units()==12069);s.verify();s.certify({});}
        copy();const auto child=fork();check(child>=0);
        if(child==0){fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);auto s=ImportStore::open(path,password);_exit(72);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71);
        {auto s=ImportStore::open(path,password);check(s.committed_balance().units()==12069);s.verify();s.certify({});}
    }
    std::cout<<"session activation: "<<positions<<" write/sync failures and process exits passed\n";
}

void session_corruption() {
    Directory dir;const auto seed=dir.path/"seed";prepare(seed);
    {auto s=ImportStore::open(seed,password);apply(s,"first-edit",correction(s,first,-3000));apply(s,"second-edit",correction(s,first,-3100));s.certify({});}
    int serial=0;
    for(const auto* sql:{
        "DROP TRIGGER entry_revisions_immutable; UPDATE entry_revisions SET total=total+1 WHERE batch='first' AND ordinal=0 AND revision=2",
        "DROP TRIGGER record_immutable; UPDATE records SET original=original+1 WHERE batch='first' AND ordinal=0",
        "DROP TRIGGER head_step; UPDATE entry_heads SET revision=999 WHERE batch='first' AND ordinal=0"}){
        const auto path=dir.path/("damaged-"+std::to_string(serial++));check(fs::copy_file(seed,path));
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        rejects([&]{auto store=ImportStore::open(path,password);store.verify();});
    }
}

void session_faults() {
    fault_vfs::install();Directory dir;const auto seed=dir.path/"seed",path=dir.path/"trial";
    prepare(seed);
    {auto s=ImportStore::open(seed,password);apply(s,"expired-intermediate",correction(s,first,-3100));apply(s,"session-edit",correction(s,first,-3000));}
    auto copy=[&]{std::error_code error;fs::remove(path,error);fs::remove(path.string()+"-journal",error);check(fs::copy_file(seed,path));};
    copy();int positions=0;
    {auto s=ImportStore::open(path,password);fault_vfs::arm(-1);s.certify({});positions=fault_vfs::seen;fault_vfs::disarm();}
    check(positions>0);
    for(int position=1;position<=positions;++position){
        copy();
        {auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
         try{s.certify({});}catch(const std::exception&){}fault_vfs::disarm();}
        {auto s=ImportStore::open(path,password);check(s.committed_balance().units()==12000);
         check(s.committed_changes().empty() || s.committed_changes().size()==2);
         check(count_rows(path,"SELECT count(*) FROM entry_revisions WHERE batch='first' AND ordinal=0")== (s.committed_changes().empty()?2:3));try{check(s.committed_change_outcome("session-edit").has_value());}catch(const app::OperationAlreadyCompleted& completed){check(completed.operation=="session-edit");}
         s.verify();s.certify({});s.certify({});}
        {auto s=ImportStore::open(path,password);check(s.committed_changes().empty());s.verify();}
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
        copy();const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});s.certify({});_exit(73);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        auto s=ImportStore::open(path,password);
        check(s.committed_changes().empty()==(phase==ImportPhase::after_commit));
        check(s.committed_balance().units()==12000);s.verify();s.certify({});
    }
    copy();
    {auto s=ImportStore::open(path,password);s.observer([](ImportPhase at){if(at==ImportPhase::after_commit)throw std::runtime_error("lost acknowledgement");});s.certify({});s.certify({});}
    {auto s=ImportStore::open(path,password);check(s.committed_changes().empty());s.verify();}
    std::cout<<"session close: "<<positions<<" write/sync failure positions, both commit crash boundaries, and lost acknowledgement passed\n";
}

void basics() {
    Directory dir;prepare(dir.path/"doc");
    {
        auto store=ImportStore::open(dir.path/"doc",password);
        check(store.committed_changes_supported());
        const auto original=store.outcome("publication-first");check(original && original->after.units()==12069);
        const auto original_sequence=store.committed_entry(first).sequence;
        auto request=correction(store,first,-3000);
        request.correction->date="2026-08-12";
        request.correction->payee="Corrected synthetic payee";
        request.correction->transaction=request.correction->transaction.corrected(Money(-3000,cad),{{"Fees","fee",Money(-14,cad)},{"Transport","transport",Money(-2986,cad)}});
        const auto preview=store.preview_committed_change(request);
        check(preview.before.units()==12069 && preview.change.units()==-69 && preview.after.units()==12000);
        check(store.committed_balance().units()==12069 && store.committed_changes().empty());
        const auto receipt=store.apply_committed_change("correction-first",request,preview.expected,preview.preview_sequence);
        check(receipt.before.units()==12069 && receipt.change.units()==-69 && receipt.after.units()==12000);
        check(receipt.after_entry.revision==1 && receipt.after_entry.active);
        check(store.committed_entry(first).sequence==original_sequence);
        check(store.row(first).draft->transaction.effective_total().units()==-2931);
        check(store.committed_entry(first).current.draft.transaction.original_imported_amount()->units()==-2931);
        check(store.evidence("first")==source && store.outcome("publication-first")->after.units()==12069);
        const auto ledger=store.ledger(false);
        check(ledger.size()==2);
        const auto found=std::find_if(ledger.begin(),ledger.end(),[](const auto& row){return row.identity==first;});
        check(found!=ledger.end() && found->draft.date=="2026-08-12" && found->draft.transaction.effective_total().units()==-3000);
        store.ingest("later","D13/08/2026\nT10\nPLater\nNunique\n^\n",qif::DateOrder::dmy);
        const auto next=store.commit("later",0,"publication-later");
        check(next.before.units()==12000 && next.change.units()==1000 && next.after.units()==13000);
        const auto retry=store.apply_committed_change("correction-first",request,preview.expected,preview.preview_sequence);
        check(retry.after.units()==12000 && store.committed_balance().units()==13000);
        auto changed=request;changed.correction->payee="Conflicting retry";
        rejects([&]{store.apply_committed_change("correction-first",changed,preview.expected,preview.preview_sequence);});
        rejects([&]{store.apply_committed_change("publication-first",request,preview.expected,preview.preview_sequence);});
        store.ingest("collision","D14/08/2026\nT1\nPCollision\n^\n",qif::DateOrder::dmy);
        rejects([&]{store.commit("collision",0,"correction-first");});
        check(revision(store,"collision")==0);check(!eligible(store,first,"correction-first"));
        store.undo_import("undo-collision","collision",store.ledger_edit_generation());
        store.undo_action("undo-later-accept",store.preview_undo("publication-later",store.action_token()));
        store.undo_import("undo-later-import","later",store.ledger_edit_generation());
        check(eligible(store,first,"correction-first"));
        apply(store,"undo-first",undo(first,"correction-first"));
        check(store.committed_balance().units()==12069);
        check(store.committed_entry(first).current.draft.date=="2026-08-10");
        rejects([&]{store.preview_committed_change(undo(first,"publication-first"));});
        store.verify();no_leaks(dir.path);
    }
    auto store=ImportStore::open(dir.path/"doc",password);
    check(store.committed_balance().units()==12069 && store.committed_change_outcome("undo-first").has_value());

    check(store.committed_change_outcome("correction-first")->after.units()==12000);store.verify();
}
void history() {
    Directory dir;prepare(dir.path/"doc");auto store=ImportStore::open(dir.path/"doc",password);
    apply(store,"correct",correction(store,first,-3000));
    const auto stale=store.preview_committed_change(undo(first,"correct"));
    apply(store,"other",correction(store,second,5100));
    check(!eligible(store,first,"correct"));
    rejects([&]{store.apply_committed_change("stale-preview",stale.request,stale.expected,stale.preview_sequence);});
    check(!store.committed_change_outcome("stale-preview"));
    apply(store,"undo-other",undo(second,"other"));apply(store,"undo-correct",undo(first,"correct"));
    check(store.committed_balance().units()==12069);
    rejects([&]{store.preview_committed_change(undo(first,"correct"));});
    apply(store,"remove",removal(first));
    check(store.committed_balance().units()==15000 && !store.committed_entry(first).current.active);
    check(store.ledger(false).size()==1 && store.committed_entry(first).current.draft.transaction.effective_total().units()==-2931);
    rejects([&]{store.preview_committed_change(removal(first));});
    rejects([&]{store.preview_committed_change(correction(store,first,-1));});
    apply(store,"restore",undo(first,"remove"));check(store.committed_balance().units()==12069);
    rejects([&]{store.preview_committed_change(undo(first,"restore"));});apply(store,"remove-again",removal(first));check(store.committed_balance().units()==15000);
    const auto page=store.committed_changes(0,2);check(page.size()==2 && page[0].sequence>page[1].sequence);
    check(page[0].operation=="remove-again" && !page[0].active);
    const auto next=store.committed_changes(page.back().sequence,2);check(next.size()==2 && next[0].sequence<page.back().sequence);
    rejects([&]{store.committed_changes(0,0);});rejects([&]{store.committed_changes(0,101);});
    const auto items=store.committed_history(first);
    check(std::count_if(items.begin(),items.end(),[](const auto& item){return item.kind==app::ChangeKind::publication;})==1);
    for(const auto& item:items)if(item.kind==app::ChangeKind::publication)check(!item.undo_eligible);
    const auto unchanged=bytes(dir.path/"doc");
    for(const auto phase:{"Verifying baseline and session changes","Verifying category history"}) {
        app::OperationContext cancelled;bool reached=false,stopped=false;
        cancelled.progress=[&](std::string_view at){if(at==phase){reached=true;cancelled.request();}};
        try{store.verify(&cancelled);}catch(const app::OperationCancelled&){stopped=true;}
        check(reached && stopped && bytes(dir.path/"doc")==unchanged);
        store.verify(); // Rebuild scoped state after cancellation; never reuse a partial audit.
        check(bytes(dir.path/"doc")==unchanged);
    }
}
void review() {
    Directory dir;prepare(dir.path/"doc");
    {
        auto store=ImportStore::open(dir.path/"doc",password);
        store.ingest("duplicate",source,qif::DateOrder::dmy);
        check(store.row({"duplicate",0}).decision.disposition==imports::Disposition::duplicate);
        apply(store,"correct",correction(store,first,-3000));
        check(!store.projected_balance());
        check(store.row({"duplicate",0}).review_stale);
        rejects([&]{store.ledger(true);});
        auto draft=*store.row({"duplicate",0}).draft;draft.payee="Draft save must not reconfirm";
        store.save({"duplicate",0},0,draft);
        rejects([&]{store.commit("duplicate",1,"duplicate-op");});
        check(store.committed_balance().units()==12000);store.verify();
    }
    {
        auto store=ImportStore::open(dir.path/"doc",password);
        check(!store.projected_balance());
        check(store.row({"duplicate",0}).review_stale);
        rejects([&]{store.commit("duplicate",1,"duplicate-op");});
        store.decide({"duplicate",0},1,{imports::Disposition::duplicate,first});
        check(!store.row({"duplicate",0}).review_stale && store.projected_balance().has_value());
        const auto receipt=store.commit("duplicate",2,"duplicate-op");check(receipt.change.units()==0);
        check(!eligible(store,first,"correct"));
        apply(store,"remove",removal(first));
        store.ingest("removed-repeat",source,qif::DateOrder::dmy);
        check(store.row({"removed-repeat",0}).decision.disposition==imports::Disposition::unresolved);
        check(store.row({"removed-repeat",1}).decision.disposition==imports::Disposition::duplicate);
        check(!store.projected_balance());rejects([&]{store.commit("removed-repeat",0,"removed-repeat-op");});
        store.decide({"removed-repeat",0},0,{imports::Disposition::duplicate,first});
        check(store.commit("removed-repeat",1,"removed-repeat-op").change.units()==0);
        check(!store.committed_entry(first).current.active && store.committed_balance().units()==15000);
        check(!eligible(store,first,"remove"));store.verify();
        store.ingest("legitimate",purchase+purchase,qif::DateOrder::dmy);
        store.decide({"legitimate",0},0,{imports::Disposition::duplicate,first});
        rejects([&]{store.decide({"legitimate",1},1,{imports::Disposition::duplicate,first});});
        store.decide({"legitimate",1},1,{imports::Disposition::include,{}});
        check(store.commit("legitimate",2,"legitimate-op").change.units()==-2931);
        check(store.committed_balance().units()==12069 && store.ledger(false).size()==2);
        check(!store.committed_entry(first).current.active);
        rejects([&]{store.committed_entry({"legitimate",0});});store.verify();
    }
    auto store=ImportStore::open(dir.path/"doc",password);store.verify();no_leaks(dir.path);
}
void dependencies() {
    for(int mode=0;mode<3;++mode) {
        Directory dir;prepare(dir.path/"doc");auto store=ImportStore::open(dir.path/"doc",password);
        store.ingest("reference",source,qif::DateOrder::dmy);
        apply(store,"correct",correction(store,first,-3000));check(eligible(store,first,"correct"));
        if(mode==0)store.decide({"reference",0},0,{imports::Disposition::include,{}});
        if(mode==1){rejects([&]{store.discard("reference",0);});test_session::close_reopen(store,dir.path/"doc",password);store.discard("reference",0);}
        if(mode==2){store.decide({"reference",0},0,{imports::Disposition::duplicate,first});store.commit("reference",1,"reference-op");}
        check(!eligible(store,first,"correct"));rejects([&]{store.preview_committed_change(undo(first,"correct"));});store.verify();
    }
}
void exact() {
    Directory dir;prepare(dir.path/"doc");auto store=ImportStore::open(dir.path/"doc",password);
    auto request=correction(store,first,220000);
    request.correction->transaction=request.correction->transaction.corrected(Money(220000,cad),{{"Salary","",Money(300000,cad)},{"Tax","",Money(-70000,cad)},{"Other","",Money(-10000,cad)}});
    apply(store,"mixed",request);check(store.committed_balance().units()==235000);
    const auto original=store.committed_entry(first).current;
    auto invalid=correction(store,first,100);invalid.correction->date="2026-02-31";
    rejects([&]{store.preview_committed_change(invalid);});
    invalid=correction(store,first,100);invalid.correction->transaction=invalid.correction->transaction.corrected(Money(100,cad),{{"[Other Account]","",Money(100,cad)}});
    rejects([&]{store.preview_committed_change(invalid);});
    const Currency gbp("GBP");invalid=correction(store,first,100);
    invalid.correction->transaction=Transaction::imported(Money(100,gbp),{{"Other","",Money(100,gbp)}});
    rejects([&]{store.preview_committed_change(invalid);});
    check(store.committed_entry(first).current.revision==original.revision);
    store.verify();
    Directory limits;
    auto limit=create(limits.path/"doc");limit.opening(0,"2026-08-01",Money(0,cad));
    limit.ingest("minimum","D1/1/2026\nT-92233720368547758.08\nPBoundary\n^\n",qif::DateOrder::dmy);limit.commit("minimum",0,"minimum-op");
    auto overflow=correction(limit,{"minimum",0},std::numeric_limits<std::int64_t>::max());
    rejects([&]{limit.preview_committed_change(overflow);});
    check(limit.committed_balance().units()==std::numeric_limits<std::int64_t>::min());
    // A small representable difference from INT64_MIN must not negate that minimum.
    apply(limit,"minimum-plus-one",correction(limit,{"minimum",0},std::numeric_limits<std::int64_t>::min()+1));
    check(limit.committed_balance().units()==std::numeric_limits<std::int64_t>::min()+1);limit.verify();
}
void backup() {
    Directory dir;prepare(dir.path/"doc");
    {
        auto store=ImportStore::open(dir.path/"doc",password);
        apply(store,"correct",correction(store,first,-3000));apply(store,"remove",removal(second));
        store.ingest("pending","D1/9/2026\nT5\nPPending\n^\n",qif::DateOrder::dmy);
        check(store.committed_balance().units()==7000 && store.projected_balance()->units()==7500);
        store.backup_to(dir.path/"backup",password);
    }
    {
        auto restored=ImportStore::open(dir.path/"backup",password);restored.verify();
        check(restored.committed_balance().units()==7000 && restored.projected_balance()->units()==7500);
        check(restored.evidence("first")==source && restored.outcome("publication-first")->after.units()==12069);
        check(restored.committed_change_outcome("correct")->after.units()==12000);
        check(!eligible(restored,second,"remove"));
        restored.backup_to(dir.path/"working",password);
    }
    auto working=ImportStore::open(dir.path/"working",password);
    working.undo_import("undo-pending","pending",working.ledger_edit_generation());
    apply(working,"restore",undo(second,"remove"));
    check(working.committed_balance().units()==12000 && working.projected_balance()->units()==12000);
    working.verify();no_leaks(dir.path);
}
void legacy_create(const fs::path& path,int version) {
    sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);
    check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
    check(sqlite3_exec(db,version==3?schema3_import_schema:legacy_import_schema,nullptr,nullptr,nullptr)==SQLITE_OK);
    if(version==1)check(sqlite3_exec(db,"PRAGMA user_version=1",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_stmt* insert=nullptr;
    check(sqlite3_prepare_v2(db,"INSERT INTO account VALUES(1,'Synthetic',?,'2026-08-01',0,0,0,0)",-1,&insert,nullptr)==SQLITE_OK);
    const auto definition=cad.definition_id();check(sqlite3_bind_text(insert,1,definition.data(),static_cast<int>(definition.size()),SQLITE_TRANSIENT)==SQLITE_OK);
    check(sqlite3_step(insert)==SQLITE_DONE);check(sqlite3_finalize(insert)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
}
void legacy() {
    for(int version:{1,2,3}) {Directory dir;legacy_create(dir.path/"doc",version);const auto before=bytes(dir.path/"doc");rejects([&]{ImportStore::open(dir.path/"doc",password);});check(bytes(dir.path/"doc")==before);no_leaks(dir.path);}
}

void validation() {
    const char* damage[] = {
        // Consistent allocation/total tampering still conflicts with operation history.
        ("DROP TRIGGER entry_revisions_immutable;DROP TRIGGER entry_allocations_immutable;"
        "UPDATE entry_revisions SET total=total+1 WHERE batch='first' AND ordinal=0 AND revision=1;"
        "UPDATE entry_allocations SET units=units+1 WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0"),
        "DROP TRIGGER ledger_operations_immutable;UPDATE ledger_operations SET after=after+1 WHERE id='corrupt-correction'",
        "DROP TRIGGER head_step;UPDATE entry_heads SET revision=0 WHERE batch='first' AND ordinal=0",
        "DROP TRIGGER change_dependencies_keep;DELETE FROM change_dependencies WHERE operation='corrupt-correction'",
        "DROP TRIGGER history_order_keep;DELETE FROM history_order WHERE operation='corrupt-correction'",
        // Save is not a review action and cannot refresh a stale target stamp.
        "DROP TRIGGER review_stamps_immutable;UPDATE review_stamps SET target_revision=1,review_revision=1 WHERE batch='reference' AND ordinal=0 AND revision=1",
        "DROP TRIGGER review_stamps_keep;DELETE FROM review_stamps WHERE batch='reference' AND ordinal=0 AND revision=1",
        // An internally valid orphan interpretation must not become invisible history.
        ("INSERT INTO entry_revisions(batch,ordinal,revision,active,date,payee,memo,total) SELECT batch,ordinal,99,active,date,payee,memo,total FROM entry_revisions WHERE batch='first' AND ordinal=0 AND revision=1;"
        "INSERT INTO entry_allocations SELECT batch,ordinal,99,position,category,memo,units FROM entry_allocations WHERE batch='first' AND ordinal=0 AND revision=1"),
        "DROP TRIGGER operation_immutable;UPDATE operations SET before=before+1,after=after+1 WHERE id='publication-first'",
        ("DROP TRIGGER history_order_immutable;UPDATE history_order SET operation='temporary' WHERE operation='order-second';"
        "UPDATE history_order SET operation='order-second' WHERE operation='order-third';"
        "UPDATE history_order SET operation='order-third' WHERE operation='temporary'"),
        ("DROP TRIGGER history_order_immutable;UPDATE history_order SET event_revision=99 WHERE batch='reference' AND event_revision=1;"
        "UPDATE history_order SET event_revision=1 WHERE batch='reference' AND event_revision=2;"
        "UPDATE history_order SET event_revision=2 WHERE batch='reference' AND event_revision=99"),
        // The operation-scoped revision lookup must reject missing and ambiguous keys.
        "DROP TRIGGER ledger_operations_immutable;UPDATE ledger_operations SET result_revision=7 WHERE id='corrupt-correction'",
        ("INSERT INTO ledger_operations(id,kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence,catalog_revision) "
        "SELECT 'duplicate-revision',kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence,catalog_revision FROM ledger_operations WHERE id='corrupt-correction'"),
        "PRAGMA user_version=1",
        "PRAGMA user_version=2",
        "PRAGMA user_version=3"
    };
    for(std::size_t index=0;index<std::size(damage);++index) {
        const auto* sql=damage[index];
        Directory dir;prepare(dir.path/"doc");
        {
            auto store=ImportStore::open(dir.path/"doc",password);
            store.ingest("reference",source,qif::DateOrder::dmy);
            apply(store,"corrupt-correction",correction(store,first,-3000));
            auto draft=*store.row({"reference",0}).draft;draft.memo="Saved stale review";
            store.save({"reference",0},0,draft);
            if(index==9){apply(store,"order-second",correction(store,first,-3100));apply(store,"order-third",correction(store,second,5100));}
            if(index==10)store.decide({"reference",0},1,{imports::Disposition::duplicate,first});
            store.verify();
        }
        sqlite3* raw=nullptr;check(sqlite3_open((dir.path/"doc").c_str(),&raw)==SQLITE_OK);
        check(sqlite3_key(raw,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(raw,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(raw)==SQLITE_OK);
        const auto unchanged=bytes(dir.path/"doc");
        rejects([&]{ImportStore::open(dir.path/"doc",password);});
        check(bytes(dir.path/"doc")==unchanged);no_leaks(dir.path);
    }
}
app::ChangePreview fault_preview(ImportStore& store,int kind) {
    return store.preview_committed_change(kind==0?correction(store,first,-3000):kind==1?removal(first):undo(first,"seed-correction"));
}
void fault_setup(const fs::path& path,int kind) {
    prepare(path);
    if(kind==2){auto store=ImportStore::open(path,password);apply(store,"seed-correction",correction(store,first,-3000));}
}
void recovery(const fs::path& path,const app::ChangePreview& preview) {
    auto store=ImportStore::open(path,password);store.verify();
    const auto existing=store.committed_change_outcome("fault-operation");
    check(store.committed_balance()==(existing?preview.after:preview.before));
    const auto result=store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence);
    check(result.after==preview.after && store.committed_balance()==preview.after);
    check(store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence).after==preview.after);
    check(store.outcome("publication-first")->after.units()==12069 && store.evidence("first")==source);store.verify();
}
void faults(bool crash) {
    fault_vfs::install();
    for(int kind=0;kind<3;++kind) {
        int positions=0;
        {
            Directory dir;fault_setup(dir.path/"doc",kind);auto store=ImportStore::open(dir.path/"doc",password);
            const auto preview=fault_preview(store,kind);fault_vfs::arm(-1);
            store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence);
            positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);
        }
        for(int position=1;position<=positions;++position) {
            Directory dir;fault_setup(dir.path/"doc",kind);
            const auto preview=[&]{auto store=ImportStore::open(dir.path/"doc",password);return fault_preview(store,kind);}();
            if(crash) {
                const auto child=fork();check(child>=0);
                if(child==0){auto store=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);
                    store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence);_exit(0);}
                int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));
            } else {
                auto store=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
                try{store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence);}
                catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}
                check(fault_vfs::fired);fault_vfs::disarm();
            }
            no_leaks(dir.path);recovery(dir.path/"doc",preview);
        }
        if(crash)for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}) {
            Directory dir;fault_setup(dir.path/"doc",kind);
            const auto preview=[&]{auto store=ImportStore::open(dir.path/"doc",password);return fault_preview(store,kind);}();
            const auto child=fork();check(child>=0);
            if(child==0){auto store=ImportStore::open(dir.path/"doc",password);
                store.observer([&](ImportPhase current){if(current==phase)_exit(72);});
                store.apply_committed_change("fault-operation",preview.request,preview.expected,preview.preview_sequence);_exit(0);}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
            {auto store=ImportStore::open(dir.path/"doc",password);check(store.committed_change_outcome("fault-operation").has_value()==(phase==ImportPhase::after_commit));}
            recovery(dir.path/"doc",preview);no_leaks(dir.path);
        }
        std::cout<<"committed kind "<<kind<<": "<<positions<<" fault positions\n";
    }
}
}
int main(int argc,char** argv) {
    try {
        const std::string mode=argc>1?argv[1]:"";
        if(mode=="session_linked_pruning")session_linked_pruning();else if(mode=="session_marker_faults")session_marker_faults();else if(mode=="session_corruption")session_corruption();else if(mode=="session_editor")session_editor();else if(mode=="session_pruning")session_pruning();else if(mode=="session_duplicates")session_duplicates();else if(mode=="session_faults")session_faults();else if(mode=="sessions")sessions();else if(mode=="basics")basics();else if(mode=="history")history();else if(mode=="review")review();
        else if(mode=="dependencies")dependencies();else if(mode=="exact")exact();else if(mode=="backup")backup();
        else if(mode=="legacy")legacy();else if(mode=="validation")validation();else if(mode=="io")faults(false);else if(mode=="crashes")faults(true);else return 2;
        std::cout<<"Committed "<<mode<<" passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
