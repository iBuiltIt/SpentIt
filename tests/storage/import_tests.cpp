#include <spentit/diagnostics.hpp>
#include <cstdlib>
#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include "import_legacy_schema.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <chrono>
#include <source_location>
#include <spawn.h>
extern char** environ;
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-import-password";
constexpr auto marker="PRIVATE-SYNTHETIC-IMPORT-9ba74";
const Currency cad("CAD");
fs::path test_binary;
const std::string source="!Type:Bank\nD10/08/2026\nT-29.31\nPPRIVATE-SYNTHETIC-IMPORT-9ba74\nEfee\n$-0.14\nEtransport\n$-29.17\n^\nD11/08/2026\nT50\nPIncome\n^\n";
struct Directory {
    fs::path path;
    Directory(){ std::string s=(fs::temp_directory_path()/"spentit-import-XXXXXX").string(); auto* p=mkdtemp(s.data()); if(!p) throw std::runtime_error("Temporary directory failed"); path=fs::canonical(p); }
    ~Directory(){std::error_code e; fs::remove_all(path,e);}
};
void check(bool b,const std::source_location& at=std::source_location::current()){if(!b) throw std::runtime_error("Import assertion failed at line "+std::to_string(at.line()));}
template<class F> void fails(F f){bool threw=false;try{f();}catch(const ImportError&){threw=true;}catch(const FinancialError&){threw=true;}catch(const std::invalid_argument&){threw=true;}check(threw);}
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void no_leaks(const fs::path& dir){for(const auto& p:fs::directory_iterator(dir))if(p.is_regular_file()){auto s=bytes(p);check(s.find(password)==s.npos && s.find(marker)==s.npos && !s.starts_with("SQLite format 3"));}}
ImportStore create(const fs::path& p){return ImportStore::create(p,password,"Synthetic account",cad,"2026-08-01");}
void prepare(const fs::path& p){auto s=create(p);s.opening(0,"2026-08-01",Money(10000,cad));s.ingest("first",source,qif::DateOrder::dmy);}
void legacy_create(const fs::path& path,int schema_version) {
    sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);
    check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
    check(sqlite3_exec(db,legacy_import_schema,nullptr,nullptr,nullptr)==SQLITE_OK);
    if(schema_version==1)check(sqlite3_exec(db,"PRAGMA user_version=1",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_stmt* statement=nullptr;
    check(sqlite3_prepare_v2(db,"INSERT INTO account VALUES(1,'Synthetic account',?,'2026-08-01',0,0,0,0)",-1,&statement,nullptr)==SQLITE_OK);
    const auto definition=cad.definition_id();check(sqlite3_bind_text(statement,1,definition.data(),static_cast<int>(definition.size()),SQLITE_TRANSIENT)==SQLITE_OK);
    check(sqlite3_step(statement)==SQLITE_DONE);check(sqlite3_finalize(statement)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
}
void embedded_returns(){
    Directory d;const auto path=d.path/"embedded-text";
    const std::string raw="D2026-09-27\nT-10.38\nPSYNTHETIC SHOP\rTEXT AGAIN\rT999.00\nMFirst\rSecond\nEPart one\rPart two\nSFood\n$-10.38\n^\n";
    {
        auto s=create(path);s.opening(0,"2026-08-01",Money(0,cad));s.ingest("cr",raw,qif::DateOrder::dmy);
        auto row=s.row({"cr",0});check(row.draft && row.draft->payee=="SYNTHETIC SHOP\rTEXT AGAIN\rT999.00");
        auto edit=*row.draft;s.save(row.identity,0,edit);check(s.commit("cr",1,"cr-commit").after==Money(-1038,cad));
        edit.memo="Changed\rMemo";const auto preview=s.preview_committed_change({app::ChangeKind::correct,row.identity,edit,{}});
        s.apply_committed_change_with_catalog("cr-edit",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);
        s.verify();s.backup_to(d.path/"restored",password);
    }
    for(const auto& p:{path,d.path/"restored"}) {
        auto s=ImportStore::open(p,password);check(s.evidence("cr")==raw && s.record_evidence({"cr",0})==raw);
        const auto entry=s.committed_entry({"cr",0});check(entry.current.draft.payee=="SYNTHETIC SHOP\rTEXT AGAIN\rT999.00" && entry.current.draft.memo=="Changed\rMemo");
        check(entry.current.draft.transaction.allocations()[0].memo=="Part one\rPart two" && s.committed_balance()==Money(-1038,cad));s.verify();
    }
    no_leaks(d.path);
}
void read_cache_invalidation(){
    Directory d;const auto path=d.path/"cache";prepare(path);
    auto s=ImportStore::open(path,password);
    const auto total=[&]{return s.ledger(true).front().draft.transaction.effective_total().units();};
    check(total()==-2931);check(total()==-2931);
    auto edit=*s.row({"first",0}).draft;
    edit.transaction=edit.transaction.corrected(Money(-3000,cad),{{"Fees","fee",Money(-3000,cad)}});
    bool inside=false;
    s.observer([&](ImportPhase phase){if(phase==ImportPhase::before_commit){
        // Transactional reads must see their own write, never the cached head.
        check(total()==-3000);inside=true;throw ImportError(SQLITE_IOERR,"Injected rollback");
    }});
    fails([&]{s.save({"first",0},0,edit);});s.observer({});
    check(inside && total()==-2931); // Rolled-back reads cannot escape the transaction.
    s.save({"first",0},0,edit);check(total()==-3000);
    s.verify();check(total()==-3000);
    s.create_account(2,"Other",cad,"2026-08-01");s.select_account(2);
    check(s.ledger(true).empty());fails([&]{s.row({"first",0});});
    s.select_account(1);check(total()==-3000);
    no_leaks(d.path);
}
void basics(){
    read_cache_invalidation();
    embedded_returns();
    Directory d;const auto p=d.path/"doc";prepare(p);
    {
        auto s=ImportStore::open(p,password);check(s.committed_balance().units()==10000 && s.projected_balance()->units()==12069);
        check(s.ingest("first",source,qif::DateOrder::dmy).id=="first");check(s.ingest("retry",source,qif::DateOrder::dmy).id=="retry");check(s.row({"retry",0}).decision.disposition==imports::Disposition::duplicate);s.discard("retry",0);
        fails([&]{s.ingest("first",source+"\n",qif::DateOrder::dmy);});
        auto row=s.row({"first",0});auto edit=*row.draft;
        edit.transaction=edit.transaction.corrected(Money(-3000,cad),{{"Fees","fee",Money(-14,cad)},{"Transport","trip",Money(-2986,cad)}});
        s.save({"first",0},0,edit);check(s.projected_balance()->units()==12000);
        fails([&]{s.save({"first",0},0,edit);});
        check(s.evidence("first")==source && s.row({"first",0}).draft->transaction.original_imported_amount()->units()==-2931);
    }
    {
        auto s=ImportStore::open(p,password);check(s.projected_balance()->units()==12000);
        auto receipt=s.commit("first",1,"commit-first");check(receipt.included==2 && receipt.excluded==0 && receipt.before.units()==10000 && receipt.change.units()==2000 && receipt.after.units()==12000);
        check(s.commit("first",1,"commit-first").after.units()==12000);fails([&]{s.commit("first",0,"commit-first");});
        fails([&]{s.opening(1,"2026-08-01",Money(0,cad));});fails([&]{s.discard("first",2);});
        check(s.ledger(false).size()==2 && s.account().frozen);s.verify();no_leaks(d.path);
    }
    auto s=ImportStore::open(p,password);check(s.outcome("commit-first")->after.units()==12000);fails([&]{ImportStore::open(p,password);});
}
void review(){
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-08-01",Money(0,cad));
    const std::string purchase="D1/1/2026\nT-10\nPRepeated\n^\n";
    s.ingest("one",purchase,qif::DateOrder::dmy);s.commit("one",0,"one-op");
    s.ingest("two",purchase+purchase,qif::DateOrder::dmy);
    check(!s.projected_balance());check(s.row({"two",0}).decision.disposition==imports::Disposition::unresolved);
    fails([&]{s.commit("two",0,"two-op");});
    s.decide({"two",0},0,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});
    fails([&]{s.decide({"two",1},1,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});});
    s.decide({"two",1},1,{imports::Disposition::include,{}});check(s.projected_balance()->units()==-2000);
    auto receipt=s.commit("two",2,"two-op");check(receipt.included==1 && receipt.excluded==1);
    s.ingest("three",purchase+purchase,qif::DateOrder::dmy);
    check(s.row({"three",0}).decision.duplicate_of==std::optional(imports::Occurrence{"one",0}));
    check(s.row({"three",1}).decision.duplicate_of==std::optional(imports::Occurrence{"two",1}));
    check(s.projected_balance()->units()==-2000);auto zero=s.commit("three",0,"three-op");check(zero.included==0 && zero.excluded==2 && zero.change.units()==0);
    s.ingest("four",purchase+purchase,qif::DateOrder::dmy);s.decide({"four",1},0,{imports::Disposition::include,{}});
    check(s.commit("four",1,"four-op").after.units()==-3000);s.verify();
    // Changed payee/reference is uncertain, not permission to silently duplicate.
    const auto variant=std::string("D1/1/2026\nT-10\nPReformatted payee\nNnew-reference\n^\n");
    s.ingest("variant",variant,qif::DateOrder::dmy);auto possible=s.row({"variant",0});
    check(possible.decision.disposition==imports::Disposition::unresolved && possible.candidates.size()==3);
    check(imports::candidate_basis(possible.source,s.row({"one",0}).source)==imports::MatchBasis::same_date_amount);
    s.decide({"variant",0},0,{imports::Disposition::duplicate,imports::Occurrence{"one",0}});
    check(s.commit("variant",1,"variant-op").change.units()==0);s.verify();
}
void errors(){
    Directory d;auto s=create(d.path/"doc");
    const auto b=s.ingest("bad","!Type:Bank\nD1/1/2026\nT1\nL[Other\n^\n",qif::DateOrder::dmy);
    check(b.blocked && !s.projected_balance());fails([&]{s.ledger(true);});fails([&]{s.commit("bad",0,"bad-op");});
    check(s.committed_balance().units()==0);s.discard("bad",0);s.discard("bad",0);check(!s.evidence("bad").empty());
    s.ingest("good",source,qif::DateOrder::dmy);auto edit=*s.row({"good",0}).draft;edit.date="2026-02-30";
    fails([&]{s.save({"good",0},0,edit);});check(s.batches().back().revision==0);
    edit=*s.row({"good",0}).draft;edit.transaction=Transaction::manual(Money(-2931,cad),{{"","",Money(-2931,cad)}});
    fails([&]{s.save({"good",0},0,edit);});
    edit=*s.row({"good",0}).draft;edit.transaction=edit.transaction.corrected(Money(-2931,cad),{{"[Other]","",Money(-2931,cad)}});
    fails([&]{s.save({"good",0},0,edit);});
    check(s.ingest("another","D1/1/2026\nT1\n^",qif::DateOrder::dmy).state==app::BatchState::pending);s.verify();
    fails([&]{ImportStore::open(d.path/"missing",password);});check(!fs::exists(d.path/"missing"));
    fails([&]{create(d.path/"doc");});
    no_leaks(d.path);
}
void backup(){
    Directory d;prepare(d.path/"doc");
    {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"backup",password);fails([&]{s.backup_to(d.path/"backup",password);});}
    {auto s=ImportStore::open(d.path/"backup",password);check(s.projected_balance()->units()==12069);s.backup_to(d.path/"restored",password);}
    {auto s=ImportStore::open(d.path/"restored",password);check(s.commit("first",0,"restored-op").after.units()==12069);s.backup_to(d.path/"committed",password);}
    {auto s=ImportStore::open(d.path/"committed",password);check(s.commit("first",0,"restored-op").after.units()==12069);s.verify();}
    const auto before=bytes(d.path/"committed");fails([&]{ImportStore::open(d.path/"committed","wrong");});check(bytes(d.path/"committed")==before);no_leaks(d.path);
}
const std::string repair_source="D31/02/2026\nT-10\nPRepair date\n^\nD1/3/2026\nT-20\nPSplit repair\nSFood\n$-19\n^\n";
imports::DraftRow repaired_row(const qif::Record& source,std::string date,std::int64_t amount) {
    return {std::move(date),source.payee,source.memo,
        Transaction::repaired_import(source.original_amount,Money(amount,cad),{{"Food","explicit repair",Money(amount,cad)}})};
}
void amount_repairs(){
    Directory d;const std::string raw="!Type:Cash\nD1/1/2026\nTinvalid\nPMissing total\nLFood/Family\n^\n";
    {auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(10000,cad));s.ingest("unknown",raw,qif::DateOrder::dmy);
        const auto source=s.row({"unknown",0}).source;check(source.repairable && !source.original_amount && source.source_class=="Family");
        imports::DraftRow edit{"2026-01-01",source.payee,"",Transaction::repaired_import({},Money(-1000,cad),{{"Food","",Money(-1000,cad)}})};
        auto invalid=edit;invalid.transaction=Transaction::repaired_import({},Money(-1000,Currency("USD")),{{"Food","",Money(-1000,Currency("USD"))}});fails([&]{s.save({"unknown",0},0,invalid);});
        invalid.transaction=Transaction::imported(Money(0,cad),{{"Food","",Money(0,cad)}});fails([&]{s.save({"unknown",0},0,invalid);});
        s.save({"unknown",0},0,edit);fails([&]{s.commit("unknown",1,"publish");});s.decide({"unknown",0},1,{imports::Disposition::include,{}});
        s.backup_to(d.path/"pending",password);check(!s.row({"unknown",0}).draft->transaction.original_imported_amount());s.verify();
    }
    {auto s=ImportStore::open(d.path/"doc",password);check(s.commit("unknown",2,"publish").after.units()==9000);check(!s.committed_entry({"unknown",0}).current.draft.transaction.original_imported_amount());
        auto correction=s.committed_entry({"unknown",0}).current.draft;auto allocations=correction.transaction.allocations();allocations[0].amount=Money(-1100,cad);correction.transaction=correction.transaction.corrected(Money(-1100,cad),allocations);
        app::ChangeRequest request{app::ChangeKind::correct,{"unknown",0},correction,{}};auto preview=s.preview_committed_change(request);s.apply_committed_change("correct-unknown",request,preview.expected,preview.preview_sequence);check(s.committed_balance().units()==8900);
        request={app::ChangeKind::undo,{"unknown",0},{},"correct-unknown"};preview=s.preview_committed_change(request);s.apply_committed_change("undo-unknown",request,preview.expected,preview.preview_sequence);check(s.committed_balance().units()==9000 && !s.committed_entry({"unknown",0}).current.draft.transaction.original_imported_amount());
        s.ingest("known","!Type:CCard\nD1/1/2026\nT-10\nPMissing total\n^\n",qif::DateOrder::dmy);const auto r=s.row({"known",0});check(r.candidates.size()==1 && r.decision.disposition==imports::Disposition::unresolved);
        check(imports::candidate_basis(r.source,s.row({"unknown",0}).source)==imports::MatchBasis::unknown_original_amount);
        s.decide({"known",0},0,{imports::Disposition::duplicate,imports::Occurrence{"unknown",0}});check(s.commit("known",1,"known-op").change.units()==0);s.backup_to(d.path/"committed",password);s.verify();}
    {auto s=ImportStore::open(d.path/"pending",password);check(s.commit("unknown",2,"publish").after.units()==9000);s.verify();}
    {auto s=ImportStore::open(d.path/"committed",password);check(s.evidence("unknown")==raw && s.committed_balance().units()==9000);s.verify();}
    {auto s=create(d.path/"transfer");s.ingest("unknown","D1/1/2026\nS[Destination]/Family\n$bad\nSFood\n$2\n^\n",qif::DateOrder::dmy);
        auto source=s.row({"unknown",0}).source;Allocation a{"[Destination]","",Money(-1000,cad),{},"source-0",0};Allocation b{"Food","",Money(200,cad),{},"source-1",1};
        imports::DraftRow edit{"2026-01-01","","",Transaction::repaired_import({},Money(-800,cad),{a,b})};
        auto invalid=edit;invalid.transaction=Transaction::repaired_import({},Money(-800,cad),{{"Food","",Money(-800,cad)}});fails([&]{s.save({"unknown",0},0,invalid);});
        s.save({"unknown",0},0,edit);s.decide({"unknown",0},1,{imports::Disposition::include,{}});fails([&]{s.commit("unknown",2,"unresolved-transfer");});s.verify();
    }
    no_leaks(d.path);
}
void amount_candidate_limit(){
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));std::string many;
    for(int i=0;i<4097;++i)many+="D1/1/2026\nT1\n^\n";
    s.ingest("many",many,qif::DateOrder::dmy);s.commit("many",0,"many-op");
    s.ingest("unknown","D1/1/2026\nTbad\n^\n",qif::DateOrder::dmy);s.save({"unknown",0},0,repaired_row(s.row({"unknown",0}).source,"2026-01-01",100));
    bool bounded=false;try{(void)s.row({"unknown",0});}catch(const ImportError& e){bounded=std::string(e.what()).find("4096")!=std::string::npos;}check(bounded);s.verify();
}
void repairs(){
    Directory d;
    {auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(10000,cad));
        s.ingest("prior","D1/2/2026\nT-10\nPPrior\n^\n",qif::DateOrder::dmy);s.commit("prior",0,"prior-op");
        s.ingest("repair",repair_source,qif::DateOrder::dmy);
        const auto original=s.row({"repair",0});check(!original.draft && original.source.repairable);
        auto edit=repaired_row(original.source,"2026-02-28",-1000);
        auto invalid=edit;invalid.date="2026-02-31";fails([&]{s.save({"repair",0},0,invalid);});
        invalid=edit;invalid.transaction=Transaction::imported(Money(-999,cad),{{"","",Money(-999,cad)}});
        fails([&]{s.save({"repair",0},0,invalid);});check(!s.row({"repair",0}).draft);
        s.save({"repair",0},0,edit);check(s.batches().back().blocked && !s.projected_balance());
        check(s.row({"repair",0}).source.diagnostic.has_value());
        check(s.row({"repair",0}).candidates.size()==1);
        check(s.row({"repair",0}).source.date.empty());
        check(imports::candidate_basis(s.row({"repair",0}).source,s.row({"prior",0}).source)==imports::MatchBasis::same_amount_unknown_date);
        s.save({"repair",1},1,repaired_row(s.row({"repair",1}).source,"2026-03-01",-2100));
        check(!s.batches().back().blocked && !s.projected_balance());
        fails([&]{s.commit("repair",2,"repair-op");});
        s.decide({"repair",0},2,{imports::Disposition::duplicate,imports::Occurrence{"prior",0}});
        s.decide({"repair",1},3,{imports::Disposition::include,{}});
        check(s.projected_balance()->units()==6900);s.backup_to(d.path/"pending",password);s.verify();}
    {auto s=ImportStore::open(d.path/"doc",password);check(s.evidence("repair")==repair_source);
        check(s.row({"repair",1}).draft->transaction.original_imported_amount()->units()==-2000);
        fails([&]{s.save({"repair",1},1,repaired_row(s.row({"repair",1}).source,"2026-03-01",-2000));});
        auto receipt=s.commit("repair",4,"repair-op");check(receipt.included==1 && receipt.excluded==1 && receipt.after.units()==6900);
        s.backup_to(d.path/"committed",password);}
    for(const auto* name:{"pending","committed"}){auto s=ImportStore::open(d.path/name,password);
        check(s.commit("repair",4,"repair-op").after.units()==6900);s.verify();}
    {auto s=ImportStore::open(d.path/"doc",password);
        s.ingest("again",repair_source,qif::DateOrder::dmy);
        check(s.batches().back().blocked); // no auto-repair or silent exclusion
        s.discard("again",0);s.verify();}
    // A repairable first diagnostic must not hide later unsupported content.
    {auto s=create(d.path/"unsafe");s.ingest("bad","D31/02/2026\nT-10\nL[Other\n^\n",qif::DateOrder::dmy);
        check(!s.row({"bad",0}).source.repairable);
        auto edit=repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000);
        fails([&]{s.save({"bad",0},0,edit);});check(s.batches()[0].blocked);s.verify();}
    {auto s=create(d.path/"header");s.ingest("bad","D31/02/2026\nT-10\n^\n!Type:Invst\n",qif::DateOrder::dmy);
        s.save({"bad",0},0,repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000));
        s.decide({"bad",0},1,{imports::Disposition::include,{}});check(s.batches()[0].blocked);s.verify();}
    {auto s=create(d.path/"unknown-date");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("bad","D31/02/2026\nT-10\n^\n",qif::DateOrder::dmy);
        s.save({"bad",0},0,repaired_row(s.row({"bad",0}).source,"2026-02-28",-1000));
        s.decide({"bad",0},1,{imports::Disposition::include,{}});s.commit("bad",2,"op");
        s.ingest("new","D1/4/2026\nT-10\n^\n",qif::DateOrder::dmy);
        check(s.row({"new",0}).decision.disposition==imports::Disposition::unresolved);
        check(s.row({"new",0}).candidates.size()==1);s.verify();}
    // Disposable pre-release formats are rejected without modifying evidence.
    legacy_create(d.path/"legacy",1);const auto legacy=bytes(d.path/"legacy");
    fails([&]{ImportStore::open(d.path/"legacy",password);});check(bytes(d.path/"legacy")==legacy);
    no_leaks(d.path);
}
void cancellation(){
    // Multibyte UTF-8 skips exact byte moduli; threshold checkpoints must still fire.
    std::string multibyte;for(int i=0;i<10000;++i)multibyte+="\xe2\x82\xac";
    int checkpoints=0;bool stopped=false;
    try{qif::parse(multibyte,qif::DateOrder::dmy,cad,[&]{if(++checkpoints==3)throw app::OperationCancelled();});}catch(const app::OperationCancelled&){stopped=true;}
    check(stopped && checkpoints==3);
    Directory d;const auto path=d.path/"doc";
    {auto s=create(path);
        app::OperationContext pre;pre.request();
        bool cancelled=false;try{s.ingest_controlled("new",source,qif::DateOrder::dmy,pre);}catch(const app::OperationCancelled&){cancelled=true;}
        check(cancelled && s.batches().empty());
        app::OperationContext limited;const auto oversized=std::string(qif::max_file+1,'x');
        fails([&]{s.ingest_controlled("too-big",oversized,qif::DateOrder::dmy,limited);});check(s.batches().empty());
        app::OperationContext context;context.progress=[&](std::string_view phase){if(phase=="Import saved")context.request();};
        cancelled=false;try{s.ingest_controlled("new",source,qif::DateOrder::dmy,context);}catch(const app::OperationCancelled&){cancelled=true;}
        check(cancelled && s.batches().empty());s.verify();
        fails([&]{s.commit("new",0,"op");});
    }
    for(int cancel_row:{1,2}) {
        auto s=ImportStore::open(path,password);app::OperationContext context;int rows=0;
        s.observer([&](ImportPhase phase){if(phase==ImportPhase::materialize_row && ++rows==cancel_row)context.request();});
        bool cancelled=false;try{s.ingest_controlled("new",source,qif::DateOrder::dmy,context);}catch(const app::OperationCancelled&){cancelled=true;}
        check(cancelled && rows==cancel_row && s.batches().empty());s.observer({});s.verify();
    }
    {auto s=ImportStore::open(path,password);app::OperationContext context;
        const auto prepared=s.ingest_controlled("new",source,qif::DateOrder::dmy,context);check(!prepared.processing_incomplete);
        auto row=*s.row({"new",0}).draft;row.payee="Saved later";s.save({"new",0},1,row);
        s.opening(0,"2026-08-01",Money(0,cad));
        const auto receipt=s.commit("new",3,"op");check(receipt.change.units()==2069);s.verify();
        app::OperationContext duplicates;s.ingest_controlled("again",source,qif::DateOrder::dmy,duplicates);
        check(s.row({"again",0}).decision.disposition==imports::Disposition::duplicate);s.verify();
    }
    // Both durable boundaries reject late cancellation and resolve lost acknowledgement.
    for(int boundary:{1,2}) for(bool after:{false,true}) {
        Directory one;auto s=create(one.path/"doc");app::OperationContext context;int commits=0;
        s.observer([&](ImportPhase phase){if(phase==ImportPhase::before_commit){
                ++commits;if(commits==boundary)check(!context.request());
                bool refused=false;try{s.durable_outcome("op");}catch(const app::OperationOutcomeUnknown&){refused=true;}
                check(refused); // An open transaction cannot prove a durable outcome.
            }
            if(commits==boundary && phase==(after?ImportPhase::after_commit:ImportPhase::before_commit))throw ImportError(SQLITE_IOERR,"Injected boundary failure");});
        try{s.ingest_controlled("new",source,qif::DateOrder::dmy,context);}catch(const ImportError&){}
        s.observer({});s.verify();
        if(boundary==1 && !after)check(s.batches().empty());
        else check(s.batches()[0].state==(boundary==2 && after?app::BatchState::pending:app::BatchState::captured));
    }
    for(int boundary:{1,2}) for(bool after:{false,true}) {
        Directory one;{auto s=create(one.path/"doc");}
        const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(one.path/"doc",password);app::OperationContext context;int commits=0;
            s.observer([&](ImportPhase phase){if(phase==ImportPhase::before_commit)++commits;
                if(commits==boundary && phase==(after?ImportPhase::after_commit:ImportPhase::before_commit))_exit(72);});
            s.ingest_controlled("new",source,qif::DateOrder::dmy,context);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        auto s=ImportStore::open(one.path/"doc",password);s.verify();
        if(boundary==2 && after)check(s.batches()[0].state==app::BatchState::pending);
        else check(s.batches().empty());
        app::OperationContext retry;
        if(s.batches().empty())s.ingest_controlled("new",source,qif::DateOrder::dmy,retry);
        else if(s.batches()[0].state==app::BatchState::captured)s.resume("new",0,retry);
        check(s.projected_balance()->units()==2069 && s.batches().size()==1);no_leaks(one.path);
    }
    // Authenticated tampering must not turn evidence-only capture into drafts or
    // detach the first versions from their recorded materialization event.
    for(int damage=0;damage<5;++damage) {
        Directory one;
        {auto s=create(one.path/"doc");app::OperationContext context;
            if(damage==0 || damage>=3)context.progress=[&](std::string_view phase){if(phase=="Import saved")throw std::runtime_error("Interrupted preparation");};
            try{s.ingest_controlled("new",source,qif::DateOrder::dmy,context);}catch(const std::runtime_error&){}
        }
        sqlite3* db=nullptr;check(sqlite3_open((one.path/"doc").c_str(),&db)==SQLITE_OK);
        check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        const char* changes=damage==0?"DROP TRIGGER event_immutable;UPDATE events SET kind='ingest' WHERE revision=0":
            damage==1?"DROP TRIGGER event_immutable;UPDATE events SET kind='save' WHERE revision=1":
            damage==2?"DROP TRIGGER source_immutable;UPDATE batches SET materialized_revision=0":
            damage==3?"DROP TRIGGER source_immutable;UPDATE batches SET count=count+1":
            "DROP TRIGGER record_pending;INSERT INTO records VALUES('new',0,1,X'44','2026-01-01',0,'','')";
        check(sqlite3_exec(db,changes,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        const auto unchanged=bytes(one.path/"doc");fails([&]{ImportStore::open(one.path/"doc",password);});check(bytes(one.path/"doc")==unchanged);
    }
    {Directory zero;auto s=create(zero.path/"doc");app::OperationContext context;
        context.progress=[&](std::string_view phase){if(phase=="Import saved")context.request();};
        const std::string invalid="\xff";try{s.ingest_controlled("invalid",invalid,qif::DateOrder::dmy,context);}catch(const app::OperationCancelled&){}
        check(s.batches().empty());
        app::OperationContext retry;const auto resumed=s.ingest_controlled("invalid",invalid,qif::DateOrder::dmy,retry);check(resumed.blocked && resumed.count==0);
        fails([&]{s.commit("invalid",1,"invalid-op");});s.discard("invalid",1);s.verify();
    }
    for(int version:{1,2}) {Directory old;legacy_create(old.path/"doc",version);const auto before=bytes(old.path/"doc");fails([&]{ImportStore::open(old.path/"doc",password);});check(bytes(old.path/"doc")==before);}
    no_leaks(d.path);
}
void operation(ImportStore& s,int kind){
    if(kind==5){app::OperationContext context;s.ingest_controlled("new",source,qif::DateOrder::dmy,context);}
    if(kind==0)s.ingest("new",source,qif::DateOrder::dmy);
    if(kind==1){auto r=*s.row({"first",0}).draft;r.payee="edited";s.save({"first",0},0,r);}
    if(kind==2)s.discard("first",0);
    if(kind==4 || kind==6)s.save({"first",0},0,repaired_row(s.row({"first",0}).source,"2026-02-28",-1000));
    if(kind==3)s.commit("first",0,"op");
    if(kind==7)s.commit("first",2,"op");
}
void setup(const fs::path& path,int kind){
    if(kind==6 || kind==7){auto s=create(path);s.opening(0,"2026-01-01",Money(10000,cad));s.ingest("first","D31/02/2026\nTbad\n^\n",qif::DateOrder::dmy);if(kind==7){operation(s,6);s.decide({"first",0},1,{imports::Disposition::include,{}});}return;}
    if(kind==0 || kind==5){auto s=create(path);}else if(kind==4){auto s=create(path);s.ingest("first","D31/02/2026\nT-10\n^\n",qif::DateOrder::dmy);}else prepare(path);}
void recovered(const fs::path& path,int kind){
    auto s=ImportStore::open(path,password);s.verify();
    if(kind==5){
        app::OperationContext context;
        if(s.batches().empty())s.ingest_controlled("new",source,qif::DateOrder::dmy,context);
        const auto batch=s.batches().at(0);
        if(batch.state==app::BatchState::captured){app::OperationContext retry;s.resume("new",batch.revision,retry);}
        check(s.batches().size()==1 && s.batches()[0].state==app::BatchState::pending && s.projected_balance()->units()==2069);
        check(s.evidence("new")==source);s.verify();
    }
    if(kind==0){check(s.batches().size()<=1);s.ingest("new",source,qif::DateOrder::dmy);check(s.batches().size()==1);}
    if(kind==1){const auto b=s.batches().at(0);check(b.revision==0 || b.revision==1);check(s.row({"first",0}).draft->payee==(b.revision==0?marker:"edited"));}
    if(kind==2){const auto b=s.batches().at(0);check(b.state==app::BatchState::pending || b.state==app::BatchState::discarded);s.discard("first",0);check(s.committed_balance().units()==10000);}
    if(kind==4 || kind==6){const auto b=s.batches()[0];check(b.revision==0 || b.revision==1);
        check(s.row({"first",0}).draft.has_value()==(b.revision==1));check(b.blocked==(b.revision==0));
        if(b.revision==0)operation(s,kind);
        check(!s.projected_balance());
        s.verify();}
    if(kind==7){check(s.committed_balance().units()==10000 || s.committed_balance().units()==9000);check(s.commit("first",2,"op").after.units()==9000);check(!s.committed_entry({"first",0}).current.draft.transaction.original_imported_amount());}
    if(kind==3){check(s.committed_balance().units()==10000 || s.committed_balance().units()==12069);check(s.commit("first",0,"op").after.units()==12069);check(s.ledger(false).size()==2);}
}
void io(bool crash){
    fault_vfs::install();
    for(int kind=0;kind<8;++kind){
        int count=0;
        {Directory d;setup(d.path/"doc",kind);auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);operation(s,kind);count=fault_vfs::seen;fault_vfs::disarm();}
        check(count>0);
        for(int n=1;n<=count;++n){
            Directory d;setup(d.path/"doc",kind);
            if(crash){
                const auto child=fork();check(child>=0);
                if(child==0){auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,SQLITE_IOERR_WRITE,true,true);operation(s,kind);_exit(0);}
                int status=0;check(waitpid(child,&status,0)==child);check(WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));
            }else{
                auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
                try{operation(s,kind);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}fault_vfs::disarm();
            }
            no_leaks(d.path);recovered(d.path/"doc",kind);
        }
        std::cout<<"operation "<<kind<<": "<<count<<" fault positions\n";
    }
    if(crash)for(int kind:{3,4,6,7})for(auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
        Directory d;setup(d.path/"doc",kind);const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});operation(s,kind);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        {auto s=ImportStore::open(d.path/"doc",password);check(((kind==3 || kind==7)?s.outcome("op").has_value():s.row({"first",0}).draft.has_value())==(phase==ImportPhase::after_commit));}
        recovered(d.path/"doc",kind);
    }
}
void backup_failures(bool crash=false){
    fault_vfs::install();int count=0;
    {Directory d;prepare(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);s.backup_to(d.path/"baseline",password);count=fault_vfs::seen;fault_vfs::disarm();}
    for(int n=1;n<=count;++n){
        Directory d;prepare(d.path/"doc");std::string prior;
        {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"prior",password);prior=bytes(d.path/"prior");}
        if(crash){
            const auto child=fork();check(child>=0);
            if(child==0){auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,SQLITE_IOERR_WRITE,true,true);s.backup_to(d.path/"candidate",password);_exit(0);}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71);
        }else{
            auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
            fails([&]{s.backup_to(d.path/"candidate",password);});check(fault_vfs::fired);fault_vfs::disarm();s.verify();
        }
        check(bytes(d.path/"prior")==prior);auto restored=ImportStore::open(d.path/"prior",password);check(restored.projected_balance()->units()==12069);no_leaks(d.path);
        auto original=ImportStore::open(d.path/"doc",password);check(original.evidence("first")==source);
        original.backup_to(d.path/"retry",password);
        auto retry=ImportStore::open(d.path/"retry",password);check(retry.projected_balance()->units()==12069);
    }
    std::cout<<"backup: "<<count<<" fault positions\n";
}
void backup_publication(){
    for(bool committed:{false,true})for(bool crash:{false,true})
    for(auto phase:{BackupPhase::before_publish,BackupPhase::before_publish_sync,
                   BackupPhase::before_cleanup,BackupPhase::before_cleanup_sync,BackupPhase::complete}){
        Directory d;prepare(d.path/"doc");
        {auto s=ImportStore::open(d.path/"doc",password);
            if(committed)s.commit("first",0,"op");
            s.backup_to(d.path/"prior",password);}
        const auto prior=bytes(d.path/"prior"),original=bytes(d.path/"doc");
        bool reached=false;
        const auto inject=[&](BackupPhase p){if(p==phase){reached=true;if(crash)_exit(73);throw ImportError(SQLITE_IOERR,"Injected publication boundary failure");}};
        if(crash){
            const auto child=fork();check(child>=0);
            if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"candidate",password,inject);_exit(0);}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
        }else{
            auto s=ImportStore::open(d.path/"doc",password);
            fails([&]{s.backup_to(d.path/"candidate",password,inject);});check(reached);
        }
        check(bytes(d.path/"prior")==prior && bytes(d.path/"doc")==original);
        const bool published=phase!=BackupPhase::before_publish;
        const bool cleaned=phase==BackupPhase::before_cleanup_sync || phase==BackupPhase::complete;
        check(fs::exists(d.path/"candidate")==published);
        check(fs::exists(d.path/"candidate.partial")==!cleaned);
        // An interrupted link/unlink publication leaves two names for one inode.
        // Normal opens must still reject it; recover a separate copy, never
        // weaken hard-link validation or silently remove retained evidence.
        if(published && !cleaned)fails([&]{ImportStore::open(d.path/"candidate",password);});
        fs::copy_file(d.path/(published?"candidate":"candidate.partial"),d.path/"recovered");
        for(const auto* name:{"doc","prior","recovered"}){
            auto s=ImportStore::open(d.path/name,password);s.verify();check(s.evidence("first")==source);
            check(s.committed_balance().units()==(committed?12069:10000));
            check(s.projected_balance()->units()==12069);
            check(s.outcome("op").has_value()==committed);
        }
        {auto s=ImportStore::open(d.path/"recovered",password);check(s.commit("first",0,"op").after.units()==12069);check(s.ledger(false).size()==2);}
        {auto s=ImportStore::open(d.path/"doc",password);s.backup_to(d.path/"retry",password);}
        {auto s=ImportStore::open(d.path/"retry",password);check(s.projected_balance()->units()==12069);}
        no_leaks(d.path);
    }
    std::cout<<"backup publication: 20 boundary cases\n";
}
void backup_cancellation(){
    {Directory d;auto s=create(d.path/"doc");std::string input;
        for(int i=0;i<200;++i) input+="D1/1/2026\nT1\nM"+std::string(8192,'x')+"\n^\n";
        s.ingest("large",input,qif::DateOrder::dmy);
        app::OperationContext context;int chunks=0;bool cancelled=false;
        context.progress=[&](std::string_view phase){if(phase=="Copying backup" && ++chunks==2)check(context.request());};
        try{s.backup_to(d.path/"cancelled",password,{},&context);}catch(const app::OperationCancelled&){cancelled=true;}
        check(cancelled && chunks==2 && !fs::exists(d.path/"cancelled"));s.verify();
        s.backup_to(d.path/"retry",password);auto restored=ImportStore::open(d.path/"retry",password);check(restored.evidence("large")==input);
    }
    for(const std::string phase:{"Preparing backup","Copying backup","Checking backup","Checking imported transactions","Checking backup","Finishing backup"}) {
        Directory d;prepare(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);
        const auto original=s.evidence("first");const auto balance=s.committed_balance();
        s.backup_to(d.path/"prior",password);const auto prior=bytes(d.path/"prior");
        app::OperationContext context;bool reached=false,cancelled=false;
        context.progress=[&](std::string_view value){if(value==phase){reached=true;check(context.request());}};
        try{s.backup_to(d.path/"candidate",password,{},&context);}catch(const app::OperationCancelled&){cancelled=true;}
        check(reached && cancelled && !std::filesystem::exists(d.path/"candidate"));
        check(bytes(d.path/"prior")==prior && s.evidence("first")==original && s.committed_balance()==balance);s.verify();
        s.backup_to(d.path/"retry",password);
        {auto restored=ImportStore::open(d.path/"retry",password);check(restored.evidence("first")==original && restored.committed_balance()==balance);}
        if(phase=="Finishing backup") {auto candidate=ImportStore::open(d.path/"candidate.partial",password);check(candidate.evidence("first")==original);}
        no_leaks(d.path);
    }
    {Directory d;prepare(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);app::OperationContext context;bool late=false;
        s.backup_to(d.path/"published",password,[&](BackupPhase phase){if(phase==BackupPhase::before_publish_sync){late=true;check(!context.request());}},&context);
        check(late && context.state.load()==app::OperationContext::sealed);auto restored=ImportStore::open(d.path/"published",password);check(restored.evidence("first")==s.evidence("first"));}
}
void opening_policy(){
    Directory d;auto s=create(d.path/"doc");s.ingest("one",source,qif::DateOrder::dmy);
    check(s.acceptance_review().warnings.empty());
    check(s.commit("one",0,"op").after.units()==2069);s.verify();
    check(s.account().opening.units()==0 && s.account().revision==0);
    fails([&]{s.opening(0,"2026-08-01",Money(0,cad));}); // Frozen values use historied correction.
}
void local_locks(){
    Directory d;const auto path=d.path/"local.spentit";
    {
        auto s=create(path);
        check(!fs::exists(path.string()+".lock"));
        const auto original=bytes(path);
        fails([&]{ImportStore::open(path,password);});
        fails([&]{ImportStore::create(path,password,"Replace",cad,"2026-11-04",true);});
        check(bytes(path)==original);
        const auto renamed=d.path/"renamed.spentit";
        fs::rename(path,renamed);
        fails([&]{ImportStore::open(renamed,password);});
        fails([&]{ImportStore::create(renamed,password,"Replace",cad,"2026-11-04",true);});
        check(bytes(renamed)==original);fs::rename(renamed,path);
        s.backup_to(d.path/"backup.spentit",password);
        check(!fs::exists(d.path/"backup.spentit.partial.lock"));
        check(!fs::exists(d.path/"backup.spentit.partial"));
        auto independent=ImportStore::open(d.path/"backup.spentit",password);
        independent.verify(); // A backup is a distinct file, not a global app lock.
    }
    fails([&]{ImportStore::open(path,"incorrect password");});
    {auto s=ImportStore::open(path,password);s.verify();}
    {auto s=ImportStore::open(d.path/"backup.spentit",password);s.verify();}
    for(const auto& entry:fs::directory_iterator(d.path))check(entry.path().extension()!=".lock");
}
void validation(){
    Directory d;prepare(d.path/"doc");
    // Deliberately corrupt authenticated synthetic state; no user data involved.
    auto alter=[&](const char* sql){sqlite3* db=nullptr;check(sqlite3_open((d.path/"doc").c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);};
    alter("PRAGMA user_version=99");fails([&]{ImportStore::open(d.path/"doc",password);});alter("PRAGMA user_version=4");
    alter("DROP TRIGGER allocation_immutable; UPDATE allocations SET units=units+1 WHERE batch='first' AND ordinal=0 AND position=0");
    fails([&]{ImportStore::open(d.path/"doc",password);});
}
pid_t replacement_child(const fs::path& path,int position) {
    // Start with a fresh runtime: instrumented macOS children must not allocate
    // between fork and exec (Foundation/libplatform locks are not fork-safe).
    auto binary=test_binary.string(),filename=path.string(),target=std::to_string(position);
    char mode[]="replacement_child";
    char* arguments[]{binary.data(),mode,filename.data(),target.data(),nullptr};
    pid_t child=0;check(posix_spawn(&child,binary.c_str(),nullptr,nullptr,arguments,environ)==0);return child;
}
void replacement(){
    Directory d;const auto path=d.path/"replace.spentit";prepare(path);const auto original=bytes(path);
    auto replace=[&]{return ImportStore::create(path,password,"Replacement",cad,"2026-10-28",true,{"DD/MM/YYYY",true});};
    fails([&]{create(path);});check(bytes(path)==original);
    {auto held=ImportStore::open(path,password);fails([&]{replace();});check(bytes(path)==original);}
    fails([&]{ImportStore::create(path,password,"",cad,"2026-10-28",true);});check(bytes(path)==original);
    fails([&]{ImportStore::create(path,password,"Replacement",cad,"2026-10-28",true,{"YYYY-MM",true});});check(bytes(path)==original);
    const auto alias=d.path/"alias";fs::create_symlink(path,alias);
    fails([&]{ImportStore::create(alias,password,"Replacement",cad,"2026-10-28",true);});check(bytes(path)==original);
    {auto s=replace();check(s.batches().empty() && s.account().name=="Replacement" && s.date_format()=="DD/MM/YYYY" && s.category_catalog().categories.size()==46);s.verify();}
    {auto s=ImportStore::open(path,password);check(s.batches().empty() && s.date_format()=="DD/MM/YYYY" && s.category_catalog().categories.size()==46);s.verify();}
    fault_vfs::install();fault_vfs::arm(100000);{auto s=replace();}const int count=fault_vfs::seen;fault_vfs::disarm();check(count>0);
    for(int error:{SQLITE_FULL,SQLITE_IOERR_WRITE})for(int position:{1,std::max(1,count/2),count}) {
        fault_vfs::arm(position,error);bool failed=false;try{auto s=replace();}catch(const ImportError&){failed=true;}
        const bool fired=fault_vfs::fired;fault_vfs::disarm();check(fired && failed);
        try {auto recovered=ImportStore::open(path,password);check(recovered.date_format()=="DD/MM/YYYY" && recovered.category_catalog().categories.size()==46);}
        catch(const ImportError&) {} // An interrupted destructive creation may be incomplete, never partially usable.
        // Overwrite is destructive by owner choice. Failure must be reported;
        // an explicit retry must replace any incomplete database and journals.
        {auto s=replace();s.verify();check(s.batches().empty());}
        {auto s=ImportStore::open(path,password);s.verify();}
    }
    for(int position:{1,std::max(1,count/2),count}) {
        const auto child=replacement_child(path,position);
        int status=0;const auto waited=waitpid(child,&status,0);if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=71)std::cerr<<"Creation interruption position "<<position<<" of "<<count<<", process status "<<status<<"\n";check(waited==child && WIFEXITED(status) && WEXITSTATUS(status)==71);
        try {auto recovered=ImportStore::open(path,password);check(recovered.date_format()=="DD/MM/YYYY" && recovered.category_catalog().categories.size()==46);}
        catch(const ImportError&) {}
        {auto s=replace();s.verify();}
    }
    const auto child=replacement_child(path,0);
    int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
    {auto s=ImportStore::open(path,password);s.verify();check(s.date_format()=="DD/MM/YYYY" && s.category_catalog().categories.size()==46);}
    no_leaks(d.path);
}
void benchmark(std::size_t n){
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-08-01",Money(0,cad));std::string input;
    for(std::size_t i=0;i<n;++i) input+="D1/1/2026\nT0.01\nN"+std::to_string(i)+"\nPSynthetic\n^\n";
    const auto start=std::chrono::steady_clock::now();s.ingest("large",input,qif::DateOrder::dmy);
    const auto staged=std::chrono::steady_clock::now();std::cout<<"Ingest finished ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(staged-start).count()<<std::endl;check(s.commit("large",0,"large-op").after.units()==static_cast<std::int64_t>(n));
    const auto committed=std::chrono::steady_clock::now();std::cout<<"Commit finished ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(committed-staged).count()<<std::endl;check(s.ledger(false).size()==n);
    const auto queried=std::chrono::steady_clock::now();
    std::cout<<n<<" rows ingest/commit/ledger ms: "<<std::chrono::duration_cast<std::chrono::milliseconds>(staged-start).count()<<'/'
        <<std::chrono::duration_cast<std::chrono::milliseconds>(committed-staged).count()<<'/'<<std::chrono::duration_cast<std::chrono::milliseconds>(queried-committed).count()<<'\n';
}
}
int main(int argc,char** argv){if(std::getenv("SPENTIT_TEST_DIAGNOSTICS"))spentit::diagnostics::Collector::instance().enable();try{test_binary=fs::absolute(argv[0]);
    if(argc==4 && std::string_view(argv[1])=="replacement_child"){fault_vfs::install();const int position=std::stoi(argv[3]);if(position>0)fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);{auto store=ImportStore::create(argv[2],password,"Replacement",cad,"2026-10-28",true,{"DD/MM/YYYY",true});}_exit(72);}

    const std::string mode=argc>1?argv[1]:"";
    if(mode=="local_locks")local_locks();else if(mode=="replacement")replacement();else if(mode=="amount_candidate_limit")amount_candidate_limit();else if(mode=="amount_repairs")amount_repairs();else if(mode=="basics")basics();else if(mode=="cancellation")cancellation();else if(mode=="review")review();else if(mode=="repairs")repairs();else if(mode=="errors")errors();else if(mode=="backup")backup();
    else if(mode=="io")io(false);else if(mode=="crashes")io(true);else if(mode=="backup_failures")backup_failures();else if(mode=="backup_crashes")backup_failures(true);else if(mode=="backup_publication")backup_publication();else if(mode=="backup_cancellation")backup_cancellation();else if(mode=="validation")validation();else if(mode=="opening")opening_policy();else if(mode=="benchmark")benchmark(argc>2?std::stoul(argv[2]):100000);else return 2;
    std::cout<<"Import "<<mode<<" passed\n";
}catch(const ImportError& e){std::cerr<<"Storage code "<<e.code()<<": "<<e.what()<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
