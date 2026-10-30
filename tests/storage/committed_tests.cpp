#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include "import_legacy_schema.hpp"
#include "import_schema3_fixture.hpp"
#include <algorithm>
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
    throw std::runtime_error("Missing synthetic history");
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
        check(revision(store,"collision")==0);store.discard("collision",0);
        check(eligible(store,first,"correction-first"));
        apply(store,"undo-first",undo(first,"correction-first"));
        check(store.committed_balance().units()==13069);
        check(store.committed_entry(first).current.draft.date=="2026-08-10");
        rejects([&]{store.preview_committed_change(undo(first,"publication-first"));});
        store.verify();no_leaks(dir.path);
    }
    auto store=ImportStore::open(dir.path/"doc",password);
    check(store.committed_balance().units()==13069 && store.committed_changes().size()==2);
    check(store.outcome("publication-later")->before.units()==12000);
    check(store.committed_change_outcome("correction-first")->after.units()==12000);store.verify();
}
void history() {
    Directory dir;prepare(dir.path/"doc");auto store=ImportStore::open(dir.path/"doc",password);
    apply(store,"correct",correction(store,first,-3000));
    const auto stale=store.preview_committed_change(undo(first,"correct"));
    apply(store,"other",correction(store,second,5100));
    check(eligible(store,first,"correct"));
    rejects([&]{store.apply_committed_change("stale-preview",stale.request,stale.expected,stale.preview_sequence);});
    check(!store.committed_change_outcome("stale-preview"));
    apply(store,"undo-correct",undo(first,"correct"));
    check(store.committed_balance().units()==12169);
    rejects([&]{store.preview_committed_change(undo(first,"correct"));});
    apply(store,"remove",removal(first));
    check(store.committed_balance().units()==15100 && !store.committed_entry(first).current.active);
    check(store.ledger(false).size()==1 && store.committed_entry(first).current.draft.transaction.effective_total().units()==-2931);
    rejects([&]{store.preview_committed_change(removal(first));});
    rejects([&]{store.preview_committed_change(correction(store,first,-1));});
    apply(store,"restore",undo(first,"remove"));check(store.committed_balance().units()==12169);
    apply(store,"undo-restore",undo(first,"restore"));check(store.committed_balance().units()==15100);
    const auto page=store.committed_changes(0,2);check(page.size()==2 && page[0].sequence>page[1].sequence);
    check(page[0].operation=="undo-restore" && !page[0].active);
    const auto next=store.committed_changes(page.back().sequence,2);check(next.size()==2 && next[0].sequence<page.back().sequence);
    rejects([&]{store.committed_changes(0,0);});rejects([&]{store.committed_changes(0,101);});
    const auto items=store.committed_history(first);
    check(std::count_if(items.begin(),items.end(),[](const auto& item){return item.kind==app::ChangeKind::publication;})==1);
    for(const auto& item:items)if(item.kind==app::ChangeKind::publication)check(!item.undo_eligible);
    store.verify();
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
        if(mode==1)store.discard("reference",0);
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
        check(eligible(restored,second,"remove"));
        restored.backup_to(dir.path/"working",password);
    }
    auto working=ImportStore::open(dir.path/"working",password);
    apply(working,"restore",undo(second,"remove"));
    check(working.committed_balance().units()==12000 && working.projected_balance()->units()==12500);
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
        if(mode=="basics")basics();else if(mode=="history")history();else if(mode=="review")review();
        else if(mode=="dependencies")dependencies();else if(mode=="exact")exact();else if(mode=="backup")backup();
        else if(mode=="legacy")legacy();else if(mode=="validation")validation();else if(mode=="io")faults(false);else if(mode=="crashes")faults(true);else return 2;
        std::cout<<"Committed "<<mode<<" passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
