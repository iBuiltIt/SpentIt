#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <string>
#include <sqlite3.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-account-password";
const Currency cad("CAD"),gbp("GBP"),jpy("JPY"),bhd("BHD");
const char* date="2026-01-01";
const std::string bytes="D1/1/2026\nT-10.00\nPSYNTHETIC-LIFECYCLE\n^\n";
const char* empty_deletion="Empty-account deletion requires an untouched zero opening, the creation opening date, no imports, no transfer participation, and another account remaining in the document.";
struct Directory {
    fs::path path;
    explicit Directory(const char* name){auto s=(fs::temp_directory_path()/name).string();auto* p=mkdtemp(s.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location where=std::source_location::current()){if(!value)throw std::runtime_error("Account lifecycle assertion failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F fn,std::string_view text,const std::source_location where=std::source_location::current()){try{fn();throw std::runtime_error("missing rejection at line "+std::to_string(where.line()));}catch(const std::exception& error){if(std::string(error.what()).find(text)==std::string::npos)throw std::runtime_error(std::string(error.what())+" (wanted "+std::string(text)+") at line "+std::to_string(where.line()));}}
ImportStore create(const fs::path& path){return ImportStore::create(path,password,"Checking",cad,date);}
std::string file_bytes(const fs::path& path){std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
std::string query(const fs::path& path,const char* sql){sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);sqlite3_stmt* statement=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&statement,nullptr)==SQLITE_OK);check(sqlite3_step(statement)==SQLITE_ROW);const auto* text=sqlite3_column_text(statement,0);std::string value=text?reinterpret_cast<const char*>(text):"";check(sqlite3_finalize(statement)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);return value;}
void no_leaks(const fs::path& dir){for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file()){const auto stored=file_bytes(entry.path());check(stored.find(password)==std::string::npos && stored.find("SYNTHETIC-LIFECYCLE")==std::string::npos && !stored.starts_with("SQLite format 3"));}}
void publish(ImportStore& store,std::string id="checking",Money opening=Money(10000,cad)){store.opening(store.account().revision,date,opening);store.ingest(id,bytes,qif::DateOrder::dmy);store.commit(id,0,id+"-commit");}
app::TransferRequest transfer_command(ImportStore& store,app::TransferAction action){app::TransferRequest request{};request.action=action;request.link="manual";request.expected_sequence=store.transfer_sequence();return request;}
void commit_manual_link(ImportStore& store){
    store.create_account(2,"Visa",cad,date);store.opening(0,date,Money(20000,cad));
    auto created=transfer_command(store,app::TransferAction::manual);created.destination=2;created.amount=Money(-2000,cad);created.date=date;created.payee="Synthetic transfer";
    store.apply_transfer("create-link",created);
    store.apply_transfer("accept-origin",transfer_command(store,app::TransferAction::accept));
    store.select_account(2);store.opening(0,date,Money(0,cad));store.apply_transfer("accept-destination",transfer_command(store,app::TransferAction::accept));store.apply_transfer("commit-destination",transfer_command(store,app::TransferAction::commit_local));
    store.select_account(1);if(transfers::protected_draft(store.transfers().at(0).link,1))store.apply_transfer("clear-origin",transfer_command(store,app::TransferAction::clear_local));store.apply_transfer("commit-origin",transfer_command(store,app::TransferAction::commit_local));
}
void behavior(){
    // An untouched zero opening is usable without an acknowledgement, including
    // both sides of a transfer. Corrections remain historied before/after Commit.
    {
        Directory fresh("spentit-opening-default-XXXXXX");const auto path=fresh.path/"doc";
        {
            auto store=create(path);
            store.correct_opening("first-edit",date,Money(2500,cad));
            check(store.account().opening==Money(2500,cad));
            store.undo_account_operation("undo-first","first-edit");
            check(store.account().opening==Money(0,cad));
            store.create_account(2,"Default zero",cad,date);store.select_account(2);
            check(!store.account().opening_confirmed);
            store.ingest("default-import",bytes,qif::DateOrder::dmy);
            check(store.acceptance_review().warnings.empty());
            check(store.review_sheet().questions.empty());
            store.accept_all("accept-default",false);
            check(store.committed_balance()==Money(-1000,cad));
            check(store.account().opening==Money(0,cad));
            store.correct_opening("later-edit",date,Money(5000,cad));
            check(store.committed_balance()==Money(4000,cad));
            check(store.evidence("default-import")==bytes);
            store.undo_account_operation("undo-later","later-edit");
            check(store.committed_balance()==Money(-1000,cad));
            store.create_account(3,"Transfer zero",cad,date);store.select_account(3);
            store.create_account(4,"Other zero",cad,date);
            auto request=transfer_command(store,app::TransferAction::manual);
            request.destination=4;request.amount=Money(-100,cad);request.date=date;request.payee="Default opening transfer";
            store.apply_transfer("zero-transfer",request);
            for(const auto& warning:store.acceptance_review().warnings)check(warning.reason.find("opening")==std::string::npos);
            store.accept_all("accept-zero-transfer",true);
            check(store.committed_balance()==Money(-100,cad));
            store.select_account(4);check(store.committed_balance()==Money(100,cad));
            store.verify();
        }
        auto reopened=ImportStore::open(path,password);reopened.verify();
        reopened.select_account(2);check(reopened.committed_balance()==Money(-1000,cad));
        reopened.select_account(4);check(reopened.committed_balance()==Money(100,cad));
    }
    Directory dir("spentit-lifecycle-XXXXXX");const auto path=dir.path/"doc";
    {
        auto store=create(path);check(query(path,"PRAGMA application_id")=="1397770577");check(query(path,"PRAGMA user_version")==(store.rules_supported()?"48":"47"));
        store.create_account(2,"Savings",cad,date);publish(store);const auto balance=store.committed_balance();const auto revision=store.account().revision;const auto evidence=store.evidence("checking");
        rejects([&]{store.rename_account("rename"," ");},"Account name is required");
        rejects([&]{store.rename_account("rename","");},"Account name is required");
        rejects([&]{store.rename_account("rename",std::string(qif::max_field+1,'A'));},"Invalid text field");
        const auto renamed=store.rename_account("rename","  Kept interior  ");
        check(renamed.kind==0 && renamed.identity==1 && renamed.name=="  Kept interior  " && renamed.opening==Money(10000,cad) && !renamed.archived && !renamed.retired);
        check(store.account().name=="  Kept interior  " && store.account().revision==revision && store.account().currency==cad && store.committed_balance()==balance && store.evidence("checking")==evidence);
        const auto creation=store.account_creation_outcome(1);check(creation && creation->name=="Checking" && creation->currency==cad && creation->date==date);
        const auto retry=store.rename_account("rename","  Kept interior  ");check(retry.operation==renamed.operation && retry.name==renamed.name && retry.revision==renamed.revision);
        rejects([&]{store.rename_account("rename","Other");},"Operation identity was reused with different content");
        check(store.create_account(1,"Checking",cad,date).name=="  Kept interior  ");
        rejects([&]{store.create_account(1,"Other",cad,date);},"Account identity was reused with different content");
        store.verify();
    }
    {
        auto store=ImportStore::open(path,password);store.select_account(2);store.opening(0,date,Money(5000,cad));const auto balance=store.committed_balance();
        store.ingest("pending","D2/1/2026\nT-1.00\nPPending\n^\n",qif::DateOrder::dmy);
        rejects([&]{store.archive_account("archive");},"Finish or discard the pending import before archiving");
        check(!store.account().archived && store.batches().size()==1 && store.evidence("pending").find("Pending")!=std::string::npos);
        store.discard("pending",0);const auto archived=store.archive_account("archive");
        check(archived.archived && store.account().archived && store.committed_balance()==balance && store.accounts().size()==1 && store.archived_accounts().size()==1 && store.archived_accounts()[0].identity==2);
        rejects([&]{store.ingest("later",bytes,qif::DateOrder::dmy);},"Archived accounts are not targets");
        rejects([&]{store.commit("pending",1,"late");},"Archived accounts are not targets");
        store.select_account(1);rejects([&]{auto request=transfer_command(store,app::TransferAction::manual);request.destination=2;request.amount=Money(-100,cad);request.date=date;request.payee="Blocked";store.apply_transfer("blocked",request);},"Archived accounts are not targets");
        store.select_account(2);rejects([&]{store.archive_account("again");},"Account is already archived");
        check(store.archive_account("archive").operation=="archive" && store.committed_balance()==balance);
        rejects([&]{store.correct_opening("blocked-opening",date,Money(5000,cad));},"Reactivate the account before changing its opening or currency");
        rejects([&]{store.reassign_currency("blocked-currency",gbp);},"Reactivate the account before changing its opening or currency");
        const auto restored=store.reactivate_account("reactivate");
        check(!restored.archived && !store.account().archived && store.committed_balance()==balance && store.accounts().size()==2 && store.account().opening==Money(5000,cad));
        check(store.reactivate_account("reactivate").operation=="reactivate");
        rejects([&]{store.reactivate_account("again");},"Account is already active");
        store.archive_account("archive-again");store.select_account(1);store.archive_account("archive-home");check(store.accounts().empty());
        store.release_account_selection();check(!store.account_selected());
        rejects([&]{store.rename_account("none","Still there");},"No active account is selected. Archived accounts remain available.");
        store.select_account(2);check(store.account().archived && store.committed_balance()==balance);store.reactivate_account("reactivate-savings");store.select_account(1);store.reactivate_account("reactivate-home");store.verify();
    }
    {
        auto store=ImportStore::open(path,password);
        rejects([&]{store.delete_account("last");},empty_deletion);
        store.create_account(3,"Untouched",cad,date);store.select_account(3);store.opening(0,date,Money(0,cad));const auto confirmed=store.delete_account("confirmed-zero");
        check(confirmed.retired && confirmed.identity==3 && !store.account_selected() && store.accounts().size()==2);
        check(store.delete_account("confirmed-zero").operation=="confirmed-zero");
        rejects([&]{store.delete_account("again");},empty_deletion);
        rejects([&]{store.select_account(3);},"Account identity was retired");
        rejects([&]{store.create_account(3,"Untouched",cad,date);},"Account identity was retired");
        check(store.next_account_identity()==4);
        store.create_account(4,"Dated",cad,date);store.select_account(4);store.opening(0,"2026-01-02",Money(0,cad));rejects([&]{store.delete_account("dated");},empty_deletion);check(store.account().identity==4);
        store.create_account(5,"Funded",cad,date);store.select_account(5);store.opening(0,date,Money(1,cad));rejects([&]{store.delete_account("funded");},empty_deletion);
        store.create_account(6,"Imported",cad,date);store.select_account(6);store.opening(0,date,Money(0,cad));store.ingest("imported",bytes,qif::DateOrder::dmy);store.discard("imported",0);rejects([&]{store.delete_account("imported");},empty_deletion);
        store.create_account(7,"Corrected",cad,date);store.select_account(7);store.opening(0,date,Money(0,cad));store.correct_opening("same-opening",date,Money(0,cad));rejects([&]{store.delete_account("corrected");},empty_deletion);
        store.create_account(8,"Renamed empty",cad,date);store.select_account(8);const auto revision=store.account().revision;store.rename_account("rename-empty","Renamed empty");check(store.account().revision==revision);
        store.archive_account("archive-empty");const auto removed=store.delete_account("remove-empty");check(removed.retired && removed.identity==8 && store.next_account_identity()==9);
        store.select_account(1);store.create_account(9,"Visa",cad,date);auto request=transfer_command(store,app::TransferAction::manual);request.destination=9;request.amount=Money(-100,cad);request.date=date;request.payee="Synthetic transfer";store.apply_transfer("link-delete",request);store.select_account(9);rejects([&]{store.delete_account("linked");},empty_deletion);
        store.verify();
    }
    {
        Directory opening_dir("spentit-lifecycle-opening-XXXXXX");auto store=create(opening_dir.path/"doc");store.create_account(2,"Other",cad,date);publish(store);check(store.committed_balance()==Money(9000,cad) && store.account().frozen);
        rejects([&]{store.correct_opening("late","2026-01-02",Money(4000,cad));},"The opening date must be on or before the earliest accepted transaction.");
        check(store.committed_balance()==Money(9000,cad));
        const auto corrected=store.correct_opening("opening","2026-01-01",Money(4000,cad));
        check(corrected.opening==Money(4000,cad) && corrected.opening_date==date && store.committed_balance()==Money(3000,cad) && store.account().frozen && store.ledger(false).at(0).draft.transaction.effective_total()==Money(-1000,cad));
        check(store.evidence("checking")==bytes);check(store.undoable_account_operation()==std::optional<std::string>{"opening"});
        store.rename_account("after-opening","Renamed checking");check(store.undoable_account_operation()==std::optional<std::string>{"opening"});
        const auto undone=store.undo_account_operation("undo-opening","opening");
        check(undone.opening==Money(10000,cad) && store.account().opening==Money(10000,cad) && store.committed_balance()==Money(9000,cad) && !store.undoable_account_operation());
        check(store.undo_account_operation("undo-opening","opening").operation=="undo-opening");
        rejects([&]{store.opening(store.account().revision,date,Money(1,cad));},"Use the explicit opening correction after an account command");
        store.correct_opening("opening-again",date,Money(4000,cad));store.ingest("later","D3/1/2026\nT-1.00\nPLater\n^\n",qif::DateOrder::dmy);
        rejects([&]{store.undo_account_operation("undo-late","opening-again");},"A later financial operation prevents Undo");
        check(store.committed_balance()==Money(3000,cad));store.discard("later",0);store.select_account(2);
        check(store.correct_opening("unconfirmed",date,Money(0,cad)).opening==Money(0,cad));
        store.select_account(1);store.verify();store.backup_to(opening_dir.path/"backup",password);
        auto restored=ImportStore::open(opening_dir.path/"backup",password);check(restored.account().opening==Money(4000,cad) && restored.committed_balance()==Money(3000,cad));restored.verify();
    }
    {
        Directory money_dir("spentit-lifecycle-money-XXXXXX");const auto money_path=money_dir.path/"doc";
        auto store=create(money_path);publish(store, "checking", Money(2000,cad));check(store.committed_balance()==Money(1000,cad));
        const auto equal=store.reassign_currency("equal",gbp);
        check(equal.currency==gbp && store.account().currency==gbp && store.account().opening==Money(2000,gbp) && store.committed_balance()==Money(1000,gbp));
        check(store.ledger(false).at(0).draft.transaction.effective_total()==Money(-1000,gbp));
        check(store.ledger(false).at(0).draft.transaction.original_imported_amount()==Money(-1000,cad));
        check(store.account_creation_outcome(1)->currency==cad && store.evidence("checking")==bytes);
        check(store.reassign_currency("equal",gbp).operation=="equal");
        rejects([&]{store.reassign_currency("same",gbp);},"Account currency is unchanged");
        const auto undone=store.undo_account_operation("undo-equal","equal");
        check(undone.currency==cad && store.account().currency==cad && store.account().opening==Money(2000,cad) && store.committed_balance()==Money(1000,cad));
        check(store.ledger(false).at(0).draft.transaction.effective_total()==Money(-1000,cad) && store.ledger(false).at(0).draft.transaction.original_imported_amount()==Money(-1000,cad));
        store.reassign_currency("pad",bhd);check(store.account().opening==Money(20000,bhd) && store.committed_balance()==Money(10000,bhd) && store.ledger(false).at(0).draft.transaction.effective_total()==Money(-10000,bhd) && store.ledger(false).at(0).draft.transaction.original_imported_amount()==Money(-1000,cad));
        store.verify();store.backup_to(money_dir.path/"backup",password);
        check(query(money_path,"SELECT currency||':'||original_units FROM source_occurrences WHERE batch='checking' AND ordinal=0")==cad.definition_id()+":-1000");
        auto restored=ImportStore::open(money_dir.path/"backup",password);check(restored.account().currency==bhd && restored.evidence("checking")==bytes && restored.committed_entry({"checking",0}).current.draft.transaction.original_imported_amount()==Money(-1000,cad));restored.verify();
        check(query(money_dir.path/"backup","SELECT currency||':'||original_units FROM source_occurrences WHERE batch='checking' AND ordinal=0")==cad.definition_id()+":-1000");
    }
    {
        Directory scale_dir("spentit-lifecycle-scale-XXXXXX");auto store=create(scale_dir.path/"doc");store.opening(0,date,Money(0,cad));store.ingest("whole","D1/1/2026\nT20.00\nPWhole\n^\n",qif::DateOrder::dmy);store.commit("whole",0,"whole-commit");
        store.reassign_currency("yen",jpy);check(store.account().currency==jpy && store.committed_balance()==Money(20,jpy) && store.committed_entry({"whole",0}).current.draft.transaction.original_imported_amount()==Money(2000,cad));store.verify();
    }
    {
        Directory fraction_dir("spentit-lifecycle-fraction-XXXXXX");auto store=create(fraction_dir.path/"doc");store.opening(0,date,Money(0,cad));store.ingest("fraction","D1/1/2026\nT20.01\nPFraction\n^\n",qif::DateOrder::dmy);store.commit("fraction",0,"fraction-commit");
        rejects([&]{store.reassign_currency("yen",jpy);},"Rescale would drop a non-zero fractional digit");
        check(store.account().currency==cad && store.committed_balance()==Money(2001,cad) && !store.account_command_outcome("yen"));store.verify();
    }
    {
        Directory overflow_dir("spentit-lifecycle-overflow-XXXXXX");auto store=create(overflow_dir.path/"doc");store.opening(0,date,Money(922337203685477581,cad));
        rejects([&]{store.reassign_currency("overflow",bhd);},"overflow");
        check(store.account().currency==cad && store.account().opening==Money(922337203685477581,cad) && !store.account_command_outcome("overflow"));store.verify();
    }
    {
        Directory draft_dir("spentit-lifecycle-draft-XXXXXX");auto store=create(draft_dir.path/"doc");store.opening(0,date,Money(0,cad));store.ingest("draft",bytes,qif::DateOrder::dmy);
        store.reassign_currency("draft-currency",gbp);
        const auto row=store.row({"draft",0});check(row.draft && row.draft->transaction.effective_total()==Money(-1000,gbp) && row.draft->transaction.original_imported_amount()==Money(-1000,cad) && row.source.original_amount==Money(-1000,cad));
        const std::string statement="OFXHEADER:100\nDATA:OFXSGML\nVERSION:102\nSECURITY:NONE\nENCODING:USASCII\nCHARSET:1252\nCOMPRESSION:NONE\nOLDFILEUID:NONE\nNEWFILEUID:NONE\n\n<OFX>\n<BANKMSGSRSV1><STMTTRNRS><TRNUID>1<STATUS><CODE>0<SEVERITY>INFO</STATUS><STMTRS><CURDEF>CAD\n<BANKACCTFROM><BANKID>999<ACCTID>1001<ACCTTYPE>CHECKING</BANKACCTFROM><BANKTRANLIST><STMTTRN><TRNTYPE>DEBIT<DTPOSTED>20260101120000<TRNAMT>-1.00<FITID>SYNTHETIC-1<NAME>Coffee</STMTTRN></BANKTRANLIST></STMTRS></STMTTRNRS></BANKMSGSRSV1>\n</OFX>\n";
        app::OperationContext context;rejects([&]{store.prepare_import_group("mismatch",statement,qif::DateOrder::mdy,{{0,1,{}}},context,"ofx");},"Statement currency CAD does not match the target account");
        store.verify();
    }
    {
        Directory link_dir("spentit-lifecycle-link-XXXXXX");auto store=create(link_dir.path/"doc");commit_manual_link(store);check(store.committed_balance()==Money(18000,cad));store.select_account(2);check(store.committed_balance()==Money(2000,cad));store.select_account(1);
        store.reassign_currency("link-currency",gbp);const auto link=store.transfers().at(0).link;
        check(link.preserved_amounts && link.origin.amount==Money(-2000,gbp) && link.destination.amount==Money(2000,cad) && link.origin.state==transfers::State::committed && link.destination.state==transfers::State::committed);
        check(store.committed_balance()==Money(18000,gbp));store.select_account(2);check(store.account().currency==cad && store.committed_balance()==Money(2000,cad));
        store.select_account(1);store.undo_account_operation("undo-link","link-currency");const auto restored=store.transfers().at(0).link;
        check(!restored.preserved_amounts && restored.origin.amount==Money(-2000,cad) && restored.destination.amount==Money(2000,cad) && store.committed_balance()==Money(18000,cad));
        store.verify();
    }
    {
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,"UPDATE account SET opening=opening+1 WHERE singleton=1",nullptr,nullptr,nullptr)!=SQLITE_OK);
        check(sqlite3_exec(db,"UPDATE account SET opening_date='2026-02-01' WHERE singleton=1",nullptr,nullptr,nullptr)!=SQLITE_OK);
        check(sqlite3_exec(db,"UPDATE account SET definition='edited' WHERE singleton=1",nullptr,nullptr,nullptr)!=SQLITE_OK);
        check(sqlite3_exec(db,"UPDATE account SET name='Edited' WHERE singleton=1",nullptr,nullptr,nullptr)!=SQLITE_OK);
        check(sqlite3_close(db)==SQLITE_OK);auto store=ImportStore::open(path,password);check(store.account().name=="  Kept interior  " && store.account().currency==cad && store.account().opening==Money(10000,cad));store.verify();
    }
    no_leaks(dir.path);
}
template<class Prepare,class Run,class Done>
void boundary(const char* name,Prepare prepare,Run run,Done done){
    int writes=0;{Directory dir("spentit-lifecycle-count-XXXXXX");const auto path=dir.path/"doc";prepare(path);auto store=ImportStore::open(path,password);fault_vfs::arm(-1);run(store);writes=fault_vfs::seen;fault_vfs::disarm();}check(writes>0);
    for(const int position:{1,(writes+1)/2,writes}){
        Directory dir("spentit-lifecycle-io-XXXXXX");const auto path=dir.path/"doc";prepare(path);
        {auto store=ImportStore::open(path,password);fault_vfs::arm(position,position==1?SQLITE_FULL:SQLITE_IOERR_WRITE);try{run(store);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}fault_vfs::disarm();}
        auto store=ImportStore::open(path,password);run(store);check(done(store));store.verify();
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
        Directory dir("spentit-lifecycle-observer-XXXXXX");const auto path=dir.path/"doc";prepare(path);const auto child=fork();check(child>=0);
        if(child==0){auto store=ImportStore::open(path,password);store.observer([&](ImportPhase at){if(at==phase)_exit(72);});run(store);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        auto store=ImportStore::open(path,password);check(done(store)==(phase==ImportPhase::after_commit));run(store);check(done(store));store.verify();
    }
    std::cout<<"Account lifecycle boundary "<<name<<": "<<writes<<" write/sync positions\n";
}
void boundaries(){
    fault_vfs::install();
    boundary("rename",[](const fs::path& path){auto store=create(path);store.create_account(2,"Savings",cad,date);},[](ImportStore& store){store.rename_account("op","Renamed");},[](ImportStore& store){return store.account().name=="Renamed" && store.account_command_outcome("op").has_value();});
    boundary("archive",[](const fs::path& path){auto store=create(path);store.create_account(2,"Savings",cad,date);store.select_account(2);store.opening(0,date,Money(500,cad));},[](ImportStore& store){store.select_account(2);store.archive_account("op");},[](ImportStore& store){store.select_account(2);return store.account().archived && store.committed_balance()==Money(500,cad);});
    boundary("reactivate",[](const fs::path& path){auto store=create(path);store.create_account(2,"Savings",cad,date);store.select_account(2);store.opening(0,date,Money(500,cad));store.archive_account("setup");},[](ImportStore& store){store.select_account(2);store.reactivate_account("op");},[](ImportStore& store){store.select_account(2);return !store.account().archived && store.committed_balance()==Money(500,cad);});
    boundary("delete",[](const fs::path& path){auto store=create(path);store.create_account(2,"Savings",cad,date);},[](ImportStore& store){if(!store.account_command_outcome("op"))store.select_account(2);store.delete_account("op");},[](ImportStore& store){return store.account_command_outcome("op").has_value() && store.accounts().size()==1;});
    boundary("default-acceptance",[](const fs::path& path){auto store=create(path);store.ingest("default",bytes,qif::DateOrder::dmy);},[](ImportStore& store){store.accept_all("op",false);},[](ImportStore& store){return store.acceptance_outcome("op").has_value() && store.committed_balance()==Money(-1000,cad) && store.account().revision==0;});
    boundary("default-opening",[](const fs::path& path){auto store=create(path);},[](ImportStore& store){store.correct_opening("op",date,Money(4000,cad));},[](ImportStore& store){return store.account().opening==Money(4000,cad) && store.committed_balance()==Money(4000,cad);});
    boundary("opening",[](const fs::path& path){auto store=create(path);publish(store);},[](ImportStore& store){store.correct_opening("op",date,Money(4000,cad));},[](ImportStore& store){return store.account().opening==Money(4000,cad) && store.committed_balance()==Money(3000,cad);});
    boundary("undo-opening",[](const fs::path& path){auto store=create(path);publish(store);store.correct_opening("setup",date,Money(4000,cad));},[](ImportStore& store){store.undo_account_operation("op","setup");},[](ImportStore& store){return store.account().opening==Money(10000,cad) && store.committed_balance()==Money(9000,cad);});
    boundary("currency",[](const fs::path& path){auto store=create(path);publish(store, "checking", Money(2000,cad));},[](ImportStore& store){store.reassign_currency("op",gbp);},[](ImportStore& store){return store.account().currency==gbp && store.committed_balance()==Money(1000,gbp) && store.evidence("checking")==bytes;});
    boundary("undo-currency",[](const fs::path& path){auto store=create(path);publish(store, "checking", Money(2000,cad));store.reassign_currency("setup",gbp);},[](ImportStore& store){store.undo_account_operation("op","setup");},[](ImportStore& store){return store.account().currency==cad && store.committed_balance()==Money(1000,cad) && store.ledger(false).at(0).draft.transaction.original_imported_amount()==Money(-1000,cad);});
}
}
int main(int argc,char** argv){try{const std::string mode=argc>1?argv[1]:"behavior";if(mode=="behavior")behavior();else if(mode=="boundaries")boundaries();else return 2;std::cout<<"Account lifecycle "<<mode<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
