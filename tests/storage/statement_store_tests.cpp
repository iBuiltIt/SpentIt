#include <spentit/import_store.hpp>
#include <spentit/statements.hpp>
#include "fault_vfs.hpp"
#include <fstream>
#include <iostream>
#include <source_location>
#include <sqlite3.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs = std::filesystem;
namespace {
constexpr auto password = "synthetic-statement-password";
const Currency cad("CAD");
struct Directory {
    fs::path path;
    Directory() {
        auto s = (fs::temp_directory_path() / "spentit-statements-XXXXXX").string();
        auto* p = mkdtemp(s.data());
        if(!p) throw std::runtime_error("Temporary directory failed");
        path = fs::canonical(p);
    }
    ~Directory() { std::error_code error; fs::remove_all(path, error); }
};
void check(bool value, const std::source_location where = std::source_location::current()) {
    if(!value) throw std::runtime_error("Statement store assertion failed at line " + std::to_string(where.line()));
}
template<class F> void rejected(F f) {
    bool failed = false;
    try { f(); } catch(const ImportError&) { failed = true; }
    check(failed);
}
ImportStore create(const fs::path& path) { return ImportStore::create(path, password, "Checking", cad, "2026-01-01"); }
std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
void write_file(const fs::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output));
}
std::string query(const fs::path& path, const char* sql) {
    sqlite3* db = nullptr;
    check(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    check(sqlite3_key(db, password, static_cast<int>(std::string_view(password).size())) == SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    check(sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK);
    check(sqlite3_step(statement) == SQLITE_ROW);
    const auto* text = sqlite3_column_text(statement, 0);
    std::string value = text ? reinterpret_cast<const char*>(text) : "";
    check(sqlite3_finalize(statement) == SQLITE_OK);
    check(sqlite3_close(db) == SQLITE_OK);
    return value;
}
void no_leaks(const fs::path& dir) {
    for(const auto& entry : fs::directory_iterator(dir)) if(entry.is_regular_file() && entry.path().extension() != ".ofx") {
        const auto bytes = read_file(entry.path());
        check(bytes.find(password) == std::string::npos && bytes.find("SYNTHETIC-STATEMENT-MARKER") == std::string::npos && !bytes.starts_with("SQLite format 3"));
    }
}
std::string sgml(std::string body) {
    return "OFXHEADER:100\nDATA:OFXSGML\nVERSION:102\nSECURITY:NONE\nENCODING:USASCII\nCHARSET:1252\nCOMPRESSION:NONE\nOLDFILEUID:NONE\nNEWFILEUID:NONE\n\n<OFX>\n" + body + "</OFX>\n";
}
std::string bank(std::string txns, std::string cur = "CAD", std::string acct = "1001") {
    return "<BANKMSGSRSV1>\n<STMTTRNRS>\n<TRNUID>1\n<STATUS>\n<CODE>0\n<SEVERITY>INFO\n</STATUS>\n<STMTRS>\n<CURDEF>" + cur +
        "\n<BANKACCTFROM>\n<BANKID>999\n<ACCTID>" + acct + "\n<ACCTTYPE>CHECKING\n</BANKACCTFROM>\n<BANKTRANLIST>\n" + txns +
        "</BANKTRANLIST>\n</STMTRS>\n</STMTTRNRS>\n</BANKMSGSRSV1>\n";
}
std::string txn(std::string fit, std::string date, std::string amount, std::string name = "Coffee") {
    std::string id = fit.empty() ? std::string{} : "<FITID>" + fit + "\n";
    return "<STMTTRN>\n<TRNTYPE>DEBIT\n<DTPOSTED>" + date + "\n<TRNAMT>" + amount + "\n" + id + "<NAME>" + name + "\n</STMTTRN>\n";
}
std::int64_t revision(ImportStore& store, std::string_view id) {
    for(const auto& batch : store.batches()) if(batch.id == id) return batch.revision;
    throw std::runtime_error("Missing batch");
}
app::ImportGroup capture(ImportStore& store, std::string id, std::string_view bytes, const std::vector<app::ImportAccountMapping>& mapping, std::string_view label = "ofx") {
    app::OperationContext context;
    return store.prepare_import_group(id, bytes, qif::DateOrder::mdy, mapping, context, label);
}
app::ImportGroup materialize(ImportStore& store, std::string_view id) {
    app::OperationContext context;
    return store.resume_import_group(id, context);
}
void common_matching() {
    for(const auto prior:{"qif","ofx","qfx","qbo","aso"})for(const auto incoming:{"qif","ofx","qfx","qbo","aso"}) {
        Directory d;const auto path=d.path/"doc";
        auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));
        const auto ingest=[&](std::string id,std::string format,bool base){
            std::string qif,ofx;
            const auto add=[&](std::string date,std::string name){
                qif+="D"+date+"\nT-12.34\nP"+name+"\n^\n";
                std::erase(date,'-');ofx+=txn("",date,"-12.34",name);
            };
            if(base){add("2010-01-01","Old purchase");add("2026-02-01","Original merchant");add("2026-02-02","Next day purchase");}
            else {add("2026-02-01","Different export description");add("2026-02-03","Independent purchase");}
            if(format=="qif"){s.ingest(id,qif,qif::DateOrder::dmy);return id;}
            capture(s,id,sgml(bank(ofx)),{{0,1,{}}},format);materialize(s,id);return id+"-account-0";
        };
        const auto base=ingest("base",prior,true);check(s.review_sheet().questions.empty());s.accept_all("accept-base");
        const auto added=ingest("new",incoming,false);const auto row=s.row({added,0});
        check(row.decision.disposition==imports::Disposition::unresolved);
        check(row.candidates==std::vector<imports::Occurrence>{{base,1}});
        const auto separate=s.row({added,1});check(separate.decision.disposition==imports::Disposition::include && separate.candidates.empty());
        const auto sheet=s.review_sheet();check(sheet.questions.size()==1 && sheet.questions[0].kind==app::ReviewKind::same && sheet.questions[0].rows.size()==2);
        for(const auto& member:sheet.questions[0].rows)check(member.identity==imports::Occurrence{base,1} || member.identity==imports::Occurrence{added,0});
        s.verify();s.backup_to(d.path/"backup",password);
        auto restored=ImportStore::open(d.path/"backup",password);restored.verify();check(restored.row({added,0}).candidates==row.candidates);
    }
    // Explicit merges retain all bank identities; disagreement must not turn
    // into an ID-less amount match merely because more than one ID is attached.
    {
        Directory d;auto s=create(d.path/"ids");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base","D01/02/2026\nT-12.34\nPOriginal\n^\n",qif::DateOrder::dmy);s.accept_all("accept");
        const auto add=[&](std::string id,std::string fit,std::string date){capture(s,id,sgml(bank(txn(fit,date,"-12.34"))),{{0,1,{}}});materialize(s,id);};
        const auto merge=[&](std::string operation,std::string batch){app::ReviewAnswer answer;answer.generation=s.review_sheet().generation;answer.question="selected";answer.action=app::ReviewAction::merge;answer.selected={{"base",0},{batch,0}};answer.survivor=imports::Occurrence{"base",0};s.answer_review(operation,answer);};
        add("a","ID-A","20260201");merge("merge-a","a-account-0");
        add("b","ID-B","20260202");check(s.row({"b-account-0",0}).candidates.empty());merge("merge-b","b-account-0");
        add("c","ID-C","20260201");check(s.row({"c-account-0",0}).candidates.empty());
        add("conflict","ID-A","20260203");const auto conflict=s.row({"conflict-account-0",0});
        check(conflict.decision.disposition==imports::Disposition::unresolved && conflict.candidates==std::vector<imports::Occurrence>{{"base",0}});
        s.verify();s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
        check(restored.row({"conflict-account-0",0}).candidates==conflict.candidates);
    }

    // Construct the saved state of an earlier explicit off-date decision using
    // synthetic storage only. The production APIs no longer offer that candidate.
    // A save event without a human duplicate decision must NOT get the allowance.
    for(const bool confirmed:{false,true}) {
        Directory d;const auto path=d.path/"historical";
        {
            auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));
            s.ingest("base","D01/02/2026\nT-12.34\nPOriginal\n^\n",qif::DateOrder::dmy);s.accept_all("accept");
            capture(s,"dated",sgml(bank(txn("","20260202","-12.34"))),{{0,1,{}}});materialize(s,"dated");
            if(confirmed)s.decide({"dated-account-0",0},revision(s,"dated-account-0"),{imports::Disposition::include,{}});
            else s.save({"dated-account-0",0},revision(s,"dated-account-0"),*s.row({"dated-account-0",0}).draft);
        }
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,R"SQL(BEGIN;
          DROP TRIGGER version_immutable;
          UPDATE versions SET decision=2,duplicate_batch='base',duplicate_ordinal=0 WHERE batch='dated-account-0' AND revision=(SELECT max(revision) FROM versions WHERE batch='dated-account-0');
          CREATE TRIGGER version_immutable BEFORE UPDATE ON versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
          DROP TRIGGER source_associations_immutable;
          UPDATE source_associations SET ledger_batch='base',ledger_ordinal=0 WHERE source_batch='dated-account-0';
          CREATE TRIGGER source_associations_immutable BEFORE UPDATE ON source_associations BEGIN SELECT RAISE(ABORT,'immutable'); END;
          INSERT INTO review_stamps SELECT v.batch,v.ordinal,v.revision,t.revision,t.active,v.revision FROM versions v JOIN review_targets t ON t.batch='base' AND t.ordinal=0 WHERE v.batch='dated-account-0' AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch);
          COMMIT;)SQL",nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        if(!confirmed){rejected([&]{auto invalid=ImportStore::open(path,password);});continue;}
        auto s=ImportStore::open(path,password);s.verify();check(s.row({"dated-account-0",0}).candidates.empty());
        check(s.commit("dated-account-0",revision(s,"dated-account-0"),"accept-dated").excluded==1);
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();check(restored.current_ledger().size()==1);
    }

}
void aso_review();
void review() {
    const auto qif = std::string("D01/02/2026\nT-12.34\nPCoffee\n^\n");
    const auto overlap = sgml(bank(txn("FIT-1", "20260201120000.000[-5:EST]", "-12.34")));
    Directory d;
    const auto path = d.path / "doc";
    const auto external = d.path / "overlap.ofx";
    write_file(external, overlap);
    {
        auto s = create(path);
        s.opening(0, "2026-01-01", Money(0, cad));
        s.ingest("qif", qif, qif::DateOrder::dmy);
        check(s.commit("qif", 0, "qif-commit").after == Money(-1234, cad));
        check(capture(s, "overlap", overlap, {{0, 1, {}}}).profile == qif::DateOrder::dmy);
        check(s.import_groups()[0].state == app::GroupState::captured);
        materialize(s, "overlap");
        s.select_account(1);
        const auto row = s.row({"overlap-account-0", 0});
        check(row.decision.disposition == imports::Disposition::duplicate);
        check(row.candidates.size() == 1 && row.candidates[0] == imports::Occurrence{"qif", 0});
        // Automatic attachment has already supplied the QIF target with this bank ID.
        check(imports::candidate_basis(row.source, s.row({"qif", 0}).source) == imports::MatchBasis::same_fitid);
        check(row.source.format == "ofx" && row.source.fitid == "FIT-1" && row.source.time_evidence == "120000.000[-5:EST]");
        check(row.source.date == "2026-02-01" && row.source.original_amount && row.source.original_amount->units() == -1234);
        const auto rev = revision(s, "overlap-account-0");
        check(s.committed_balance() == Money(-1234, cad) && s.ledger(false).size() == 1);
        const auto receipt = s.commit("overlap-account-0", rev, "overlap-commit");
        check(receipt.included == 0 && receipt.excluded == 1 && receipt.after == Money(-1234, cad));
        check(s.commit("overlap-account-0", rev, "overlap-commit").after == receipt.after);
        check(s.ledger(false).size() == 1 && s.evidence("overlap-account-0") == overlap && s.evidence("qif") == qif);
        check(read_file(external) == overlap);
        s.backup_to(d.path / "backup", password);
        s.verify();
    }
    check(query(path, "SELECT format||'/'||grammar FROM source_headers WHERE source_id='qif'") == "qif/qif");
    check(query(path, "SELECT ifnull(fitid,'') FROM source_occurrences WHERE batch='qif' AND ordinal=0") == "");
    check(query(path, "SELECT format||'/'||grammar FROM source_headers WHERE source_id='overlap'") == "ofx/ofx-sgml");
    check(query(path, "SELECT ledger_batch||':'||ledger_ordinal FROM source_associations WHERE source_batch='overlap-account-0' AND source_ordinal=0") == "qif:0");
    check(query(path, "SELECT count(*) FROM source_associations WHERE source_batch='qif'") == "1");
    check(query(path, "PRAGMA user_version") == (query(path, "SELECT count(*) FROM sqlite_master WHERE name='rule_state'") == "1" ? "40" : "39"));
    {
        auto restored = ImportStore::open(d.path / "backup", password);
        restored.select_account(1);
        check(restored.evidence("overlap-account-0") == overlap && restored.evidence("qif") == qif);
        check(restored.committed_balance() == Money(-1234, cad) && restored.ledger(false).size() == 1);
        restored.verify();
    }
    {
        auto s = ImportStore::open(path, password);
        const auto changed = sgml(bank(txn("FIT-1", "20260201", "-12.35")));
        capture(s, "changed", changed, {{0, 1, {}}});
        materialize(s, "changed");
        const auto row = s.row({"changed-account-0", 0});
        check(row.decision.disposition == imports::Disposition::unresolved && row.candidates.size() == 1);
        check(imports::candidate_basis(row.source, s.row({"qif", 0}).source) == imports::MatchBasis::uncertain_fitid);
        const auto rev = revision(s, "changed-account-0");
        s.decide({"changed-account-0", 0}, rev, {imports::Disposition::include, {}});
        check(s.commit("changed-account-0", rev + 1, "changed-commit").after == Money(-2469, cad));
        check(s.ledger(false).size() == 2);
        const auto repeat = sgml(bank(txn("FIT-2", "20260201", "-12.34")));
        capture(s, "repeat", repeat, {{0, 1, {}}});
        materialize(s, "repeat");
        const auto repeated = s.row({"repeat-account-0", 0});
        // FIT-1 is now associated with the QIF ledger row, so a different FITID is a legitimate repeat.
        check(repeated.candidates.empty() && repeated.decision.disposition == imports::Disposition::include);
        check(!imports::candidate_basis(repeated.source, s.row({"qif", 0}).source));
        check(s.commit("repeat-account-0", revision(s, "repeat-account-0"), "repeat-commit").after == Money(-3703, cad));
        check(s.ledger(false).size() == 3);
        const auto dated = sgml(bank(txn("", "20260202", "-12.34")));
        capture(s, "dated", dated, {{0, 1, {}}});
        materialize(s, "dated");
        const auto uncertain = s.row({"dated-account-0", 0});
        check(uncertain.decision.disposition == imports::Disposition::include && uncertain.candidates.empty());
        check(!imports::candidate_basis(uncertain.source, s.row({"qif", 0}).source));
        const auto date_rev = revision(s, "dated-account-0");
        bool refused=false;try{s.decide({"dated-account-0", 0}, date_rev, {imports::Disposition::duplicate, imports::Occurrence{"qif", 0}});}catch(const std::invalid_argument&){refused=true;}check(refused);
        check(s.commit("dated-account-0", date_rev, "dated-commit").included == 1);
        check(s.committed_balance() == Money(-4937, cad) && s.ledger(false).size() == 4);
        s.verify();
    }
    {
        Directory fresh;
        auto s = create(fresh.path / "doc");
        s.opening(0, "2026-01-01", Money(0, cad));
        const auto pair = sgml(bank(txn("FIT-A", "20260201", "-1.00") + txn("FIT-B", "20260201", "-1.00")));
        capture(s, "pair", pair, {{0, 1, {}}});
        materialize(s, "pair");
        check(s.row({"pair-account-0", 0}).decision.disposition == imports::Disposition::include);
        check(s.row({"pair-account-0", 1}).decision.disposition == imports::Disposition::include);
        check(s.row({"pair-account-0", 0}).candidates.empty() && s.row({"pair-account-0", 1}).candidates.empty());
        check(s.commit("pair-account-0", revision(s, "pair-account-0"), "pair-commit").included == 2);
        check(s.ledger(false).size() == 2 && s.committed_balance() == Money(-200, cad));
        const auto later = sgml(bank(txn("FIT-C", "20260201", "-1.00")));
        capture(s, "later", later, {{0, 1, {}}});
        materialize(s, "later");
        const auto distinct = s.row({"later-account-0", 0});
        check(distinct.candidates.empty() && distinct.decision.disposition == imports::Disposition::include);
        check(!imports::candidate_basis(distinct.source, s.row({"pair-account-0", 0}).source));
        check(s.commit("later-account-0", revision(s, "later-account-0"), "later-commit").included == 1);
        check(s.ledger(false).size() == 3 && s.committed_balance() == Money(-300, cad));
        const auto same = sgml(bank(txn("FIT-1", "20260201", "-1.00") + txn("FIT-1", "20260203", "-1.00")));
        capture(s, "same", same, {{0, 1, {}}});
        materialize(s, "same");
        check(s.row({"same-account-0", 0}).decision.disposition == imports::Disposition::unresolved);
        const auto second = s.row({"same-account-0", 1});
        check(second.decision.disposition == imports::Disposition::unresolved && second.candidates.empty());
        const auto same_rev = revision(s, "same-account-0");
        rejected([&] { s.commit("same-account-0", same_rev, "same-commit"); });
        bool sibling = false;
        try { s.decide({"same-account-0", 1}, same_rev, {imports::Disposition::duplicate, imports::Occurrence{"same-account-0", 0}}); }
        catch(const std::invalid_argument&) { sibling = true; }
        catch(const ImportError&) { sibling = true; }
        check(sibling);
        s.decide({"same-account-0", 1}, same_rev, {imports::Disposition::include, {}});
        s.decide({"same-account-0", 0}, same_rev + 1, {imports::Disposition::include, {}});
        check(s.commit("same-account-0", same_rev + 2, "same-commit").included == 2);
        check(s.ledger(false).size() == 5);
        const auto missing = sgml(bank("<STMTTRN>\n<TRNTYPE>DEBIT\n<DTPOSTED>20260201\n<FITID>FIT-M\n<NAME>Coffee\n</STMTTRN>\n"));
        capture(s, "missing", missing, {{0, 1, {}}});
        materialize(s, "missing");
        const auto repair_row = s.row({"missing-account-0", 0});
        check(repair_row.source.repairable && !repair_row.source.original_amount && !repair_row.draft);
        check(repair_row.decision.disposition == imports::Disposition::unresolved);
        const auto missing_rev = revision(s, "missing-account-0");
        rejected([&] { s.commit("missing-account-0", missing_rev, "missing-commit"); });
        imports::DraftRow repair{"2026-02-01", "Coffee", "", Transaction::repaired_import({}, Money(-100, cad), {{"", "", Money(-100, cad)}})};
        s.save({"missing-account-0", 0}, missing_rev, repair);
        s.decide({"missing-account-0", 0}, missing_rev + 1, {imports::Disposition::include, {}});
        check(s.evidence("missing-account-0") == missing);
        check(s.commit("missing-account-0", missing_rev + 2, "missing-commit").after == Money(-600, cad));
        s.verify();
    }
    {
        Directory refused;
        const auto bad_path = refused.path / "bad.ofx";
        const std::string malformed = "OFXHEADER:100\nDATA:OFXSGML\nVERSION:102\n\n<OFX><BANKMSGSRSV1>";
        const std::string investment = sgml("<INVSTMTMSGSRSV1><INVSTMTTRNRS></INVSTMTTRNRS></INVSTMTMSGSRSV1>\n");
        const std::string dollars = sgml(bank(txn("FIT-1", "20260201", "-1.00"), "USD"));
        const std::string not_ofx = "not an OFX statement\n";
        write_file(bad_path, malformed);
        auto s = create(refused.path / "doc");
        s.opening(0, "2026-01-01", Money(0, cad));
        for(const auto& sample : {std::pair{malformed, std::string("ofx")}, {investment, std::string("ofx")}, {dollars, std::string("qfx")}, {not_ofx, std::string("aso")}, {overlap, std::string("qif")}}) {
            rejected([&] { capture(s, "refused", sample.first, {{0, 1, {}}}, sample.second); });
            check(s.import_groups().empty() && s.batches().size() == 0);
        }
        check(read_file(bad_path) == malformed);
        const auto two = sgml(bank(txn("FIT-1", "20260201", "-1.00"), "CAD", "1001") + bank(txn("FIT-2", "20260202", "-2.00"), "CAD", "2002"));
        const std::vector<app::ImportAccountMapping> mapping{{0, 1, {}}, {1, 2, app::AccountCreation{2, "999 2002", cad, "2026-01-01"}}};
        check(capture(s, "two", two, mapping).accounts.size() == 2);
        check(capture(s, "two", two, mapping).state == app::GroupState::captured);
        materialize(s, "two");
        s.review_import_balance("two", 1, 1, app::ReportedBalanceChoice::zero, "2026-01-01");
        s.select_account(1);
        check(s.commit("two-account-0", revision(s, "two-account-0"), "first-account").included == 1);
        s.select_account(2);
        check(s.commit("two-account-1", revision(s, "two-account-1"), "second-account").after == Money(-200, cad));
        check(s.evidence("two-account-1") == two);
        s.select_account(1);
        check(s.evidence("two-account-0") == two);
        s.verify();
        no_leaks(refused.path);
    }
    no_leaks(d.path);
    aso_review();
}
void aso_review() {
    const auto bytes = read_file(fs::path(__FILE__).parent_path().parent_path() / "fixtures/statements/card-sgml.aso");
    check(!bytes.empty() && bytes.find("USERKEY") == std::string::npos && bytes.find("SECURITY:TYPE1\r\n") != std::string::npos);
    Directory d;
    const auto path = d.path / "doc";
    const auto external = d.path / "card.aso";
    write_file(external, bytes);
    {
        auto s = create(path);
        s.opening(0, "2026-01-01", Money(0, cad));
        check(capture(s, "card", bytes, {{0, 1, {}}}, "aso").profile == qif::DateOrder::dmy);
        materialize(s, "card");
        s.select_account(1);
        const auto debit = s.row({"card-account-0", 0});
        const auto credit = s.row({"card-account-0", 1});
        check(debit.decision.disposition == imports::Disposition::include && debit.candidates.empty());
        check(credit.decision.disposition == imports::Disposition::include && credit.candidates.empty());
        check(debit.source.format == "aso" && debit.source.grammar == "ofx-sgml" && debit.source.fitid == "SYNTH-DEBIT-A");
        check(debit.source.date == "2026-01-15" && debit.source.time_evidence == "143000[-5:EST]" && debit.source.original_amount && debit.source.original_amount->units() == -123);
        check(debit.source.payee == "Coffee" && debit.source.trntype == "DEBIT");
        check(credit.source.format == "aso" && credit.source.grammar == "ofx-sgml" && credit.source.fitid == "SYNTH-CREDIT-B");
        check(credit.source.date == "2026-01-16" && credit.source.time_evidence == "120000[0:GMT]" && credit.source.original_amount && credit.source.original_amount->units() == 400);
        check(credit.source.payee == "Refund" && credit.source.trntype == "CREDIT");
        const auto rev = revision(s, "card-account-0");
        const auto receipt = s.commit("card-account-0", rev, "card-commit");
        check(receipt.included == 2 && receipt.excluded == 0 && receipt.after == Money(277, cad));
        check(s.ledger(false).size() == 2 && s.evidence("card-account-0") == bytes && read_file(external) == bytes);
        auto again = bytes;
        const auto uid = again.find("<TRNUID>1\r\n");
        check(uid != std::string::npos);
        again.replace(uid, std::string("<TRNUID>1\r\n").size(), "<TRNUID>2\r\n");
        capture(s, "again", again, {{0, 1, {}}}, "aso");
        materialize(s, "again");
        const auto first = s.row({"again-account-0", 0});
        const auto second = s.row({"again-account-0", 1});
        check(first.decision.disposition == imports::Disposition::duplicate && first.candidates.size() == 1 && first.candidates[0] == imports::Occurrence{"card-account-0", 0});
        check(imports::candidate_basis(first.source, s.row({"card-account-0", 0}).source) == imports::MatchBasis::same_fitid);
        check(second.decision.disposition == imports::Disposition::duplicate && second.candidates.size() == 1 && second.candidates[0] == imports::Occurrence{"card-account-0", 1});
        check(imports::candidate_basis(second.source, s.row({"card-account-0", 1}).source) == imports::MatchBasis::same_fitid);
        const auto again_rev = revision(s, "again-account-0");
        s.decide({"again-account-0", 0}, again_rev, {imports::Disposition::duplicate, imports::Occurrence{"card-account-0", 0}});
        s.decide({"again-account-0", 1}, again_rev + 1, {imports::Disposition::duplicate, imports::Occurrence{"card-account-0", 1}});
        const auto dup = s.commit("again-account-0", again_rev + 2, "again-commit");
        check(dup.included == 0 && dup.excluded == 2 && dup.after == Money(277, cad));
        check(s.commit("again-account-0", again_rev + 2, "again-commit").after == dup.after);
        check(s.ledger(false).size() == 2 && s.committed_balance() == Money(277, cad) && s.evidence("again-account-0") == again);
        s.verify();
    }
    check(query(path, "SELECT format||'/'||grammar FROM source_headers WHERE source_id='card'") == "aso/ofx-sgml");
    check(query(path, "SELECT format||'/'||grammar FROM source_occurrences WHERE batch='card-account-0' AND ordinal=0") == "aso/ofx-sgml");
    check(query(path, "SELECT format||'/'||grammar FROM source_occurrences WHERE batch='again-account-0' AND ordinal=1") == "aso/ofx-sgml");
    const auto header = query(path, "SELECT header FROM source_headers WHERE source_id='card'");
    check(header.find("TYPE1") != std::string::npos && header.find("USERKEY") == std::string::npos && header.find("SYNTH-DEBIT-A") == std::string::npos);
    check(query(path, "SELECT ledger_batch||':'||ledger_ordinal FROM source_associations WHERE source_batch='again-account-0' AND source_ordinal=0") == "card-account-0:0");
    check(query(path, "SELECT ledger_batch||':'||ledger_ordinal FROM source_associations WHERE source_batch='again-account-0' AND source_ordinal=1") == "card-account-0:1");
    check(query(path, "SELECT count(*) FROM source_associations WHERE ledger_batch='card-account-0' AND source_batch='card-account-0'") == "2");
    no_leaks(d.path);
}
void strict_overlap() {
    // Deliberately synthetic, with two legitimate same-day/same-amount payments.
    const auto first=txn("OVERLAP-A","20260201","-1.00","Cafe & Books");
    const auto second=txn("OVERLAP-B","20260201","-1.00","Cafe & Books");
    const auto older=txn("OVERLAP-C","20260115","-2.00","Earlier payment");
    {
        {
        Directory partial;auto s=create(partial.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("partial","D01/02/2026\nT-1.00\nPOne\n^\nD02/02/2026\nTbad\nPTwo\n^\n",qif::DateOrder::dmy);
        check(s.acceptance_review().transactions==1 && s.acceptance_review().failed_records==1);
        check(s.accept_all("partial-accept").transactions==1);
        check(s.acceptance_review().transactions==0 && s.acceptance_review().failed_records==1);
        check(s.current_ledger().size()==1 && s.committed_balance()==Money(-100,cad));s.verify();
        imports::DraftRow repaired{"2026-02-02","Two","",Transaction::repaired_import({},Money(-200,cad),{{"","",Money(-200,cad)}})};
        s.save({"partial",1},revision(s,"partial"),repaired);
        s.decide({"partial",1},revision(s,"partial"),{imports::Disposition::include,{}});
        check(s.accept_all("repair-accept").transactions==1);
        check(s.current_ledger().size()==2 && s.balance()==Money(-300,cad));s.verify();
    }
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        capture(s,"pending",sgml(bank(first+second)),{{0,1,{}}},"aso");materialize(s,"pending");
        capture(s,"same-pending",sgml(bank(first+second)),{{0,1,{}}},"aso");materialize(s,"same-pending");check(s.import_result("same-pending-account-0").already_present==2 && s.acceptance_review().transactions==2);
        capture(s,"pending-overlap",sgml(bank(second+first)),{{0,1,{}}},"qfx");materialize(s,"pending-overlap");
        check(s.row({"pending-overlap-account-0",0}).decision.disposition==imports::Disposition::duplicate);
        check(s.current_ledger().size()==2 && s.committed_balance()==Money(0,cad));
        check(s.batches().back().completed && !s.batches().front().completed);
        capture(s,"pending-longer",sgml(bank(older+first)),{{0,1,{}}},"ofx");materialize(s,"pending-longer");
        check(s.current_ledger().size()==3 && s.balance()==Money(-400,cad));s.verify();
        s.backup_to(d.path/"pending-backup",password);
        auto restored=ImportStore::open(d.path/"pending-backup",password);
        check(restored.current_ledger().size()==3 && restored.committed_balance()==Money(0,cad));
        check(restored.acceptance_review().transactions==3);
        const auto accepted=restored.accept_all("dataset-accept");check(accepted.transactions==3);
        check(restored.accept_all("dataset-accept").transactions==3);
        check(restored.acceptance_review().transactions==0 && restored.committed_balance()==Money(-400,cad));
        restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("origin","D01/02/2026\nT-10\nL[Old Visa export]\n^\n",qif::DateOrder::dmy);
        app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="payment";r.expected_sequence=s.transfer_sequence();r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;
        s.apply_transfer("make-transfer",r);
        check(s.acceptance_review().transactions==2);rejected([&]{s.accept_all("accept-transfer");});
        check(s.accept_all("accept-transfer",true).transactions==2);s.verify();
        check(s.committed_balance()==Money(-1000,cad));s.select_account(2);check(s.committed_balance()==Money(1000,cad));
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();check(restored.acceptance_review().transactions==0);
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        const std::string qif="D01/02/2026\nT-1\nPOne\n^\nD01/02/2026\nT-1\nPOne\n^\n";
        s.ingest("qif-first",qif,qif::DateOrder::dmy);s.ingest("qif-again",qif,qif::DateOrder::dmy);
        check(s.current_ledger().size()==2 && s.batches().back().completed);
        check(s.acceptance_review().transactions==2);std::size_t validations=0;
        s.observer([&](ImportPhase p){if(p==ImportPhase::validate_row)++validations;});s.accept_all("qif-accept");s.observer({});check(validations==0);s.verify();
        const auto entry=s.committed_entry({"qif-first",0});auto edit=entry.current.draft;edit.payee="Corrected";
        app::ChangeRequest request{app::ChangeKind::correct,{"qif-first",0},edit,{}};
        const auto preview=s.preview_committed_change(request);s.apply_committed_change("after-accept",request,preview.expected,preview.preview_sequence);s.verify();
    }
    for(bool short_first:{false,true}) {
        Directory d;
        {
            auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
            const auto initial=sgml(bank(short_first?first+second:older+first+second));
            capture(s,"initial",initial,{{0,1,{}}},"aso");materialize(s,"initial");
            s.commit("initial-account-0",revision(s,"initial-account-0"),"initial-commit");
            const auto next=sgml(bank(short_first?older+second+first:second+first));
            capture(s,"next",next,{{0,1,{}}},"qfx");materialize(s,"next");
            for(std::size_t i=short_first?1:0;i<(short_first?3:2);++i)
                check(s.row({"next-account-0",i}).decision.disposition==imports::Disposition::duplicate);
            check(s.balance()==Money(-400,cad) && s.current_ledger().size()==3);
            const auto receipt=s.commit("next-account-0",revision(s,"next-account-0"),"next-commit");
            check(receipt.included==(short_first?1:0) && receipt.excluded==2);
            // Several associations for one target are not several candidates.
            for(const auto label:{"ofx","qfx","qbo","aso"}) {
                const auto id=std::string("again-")+label;
                const auto changed=sgml(bank(txn("OVERLAP-A","20260201","-1.00","Cafe &amp; Books")));
                capture(s,id,changed,{{0,1,{}}},label);materialize(s,id);
                check(s.row({id+"-account-0",0}).decision.disposition==imports::Disposition::duplicate);
                const auto done=s.commit(id+"-account-0",revision(s,id+"-account-0"),id+"-commit");
                check(done.included==0 && done.excluded==1 && done.after==Money(-400,cad));
            }
            s.backup_to(d.path/"backup",password);s.verify();
        }
        for(const auto name:{"doc","backup"}) {
            auto s=ImportStore::open(d.path/name,password);
            check(s.balance()==Money(-400,cad) && s.current_ledger().size()==3);s.verify();
        }
    }
    {
        Directory ambiguous;auto s=create(ambiguous.path/"doc");
        s.opening(0,"2026-01-01",Money(0,cad));
        capture(s,"twins",sgml(bank(first+first)),{{0,1,{}}});materialize(s,"twins");
        for(std::size_t n=0;n<2;++n)s.decide({"twins-account-0",n},revision(s,"twins-account-0"),{imports::Disposition::include,{}});
        s.commit("twins-account-0",revision(s,"twins-account-0"),"twins-commit");
        capture(s,"ambiguous",sgml(bank(first)),{{0,1,{}}},"qfx");materialize(s,"ambiguous");
        check(s.row({"ambiguous-account-0",0}).decision.disposition==imports::Disposition::unresolved);
        check(s.row({"ambiguous-account-0",0}).candidates.size()==2);
        check(s.current_ledger().size()==3); // unresolved input is never silently dropped
        s.verify();
    }
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
    capture(s,"base",sgml(bank(first)),{{0,1,{}}});materialize(s,"base");
    s.commit("base-account-0",revision(s,"base-account-0"),"base-commit");
    auto edit=s.committed_entry({"base-account-0",0}).current.draft;
    edit.date="2026-02-09";edit.payee="Manually corrected";
    edit.transaction=Transaction::repaired_import(Money(-100,cad),Money(-150,cad),{{"","",Money(-150,cad)}});
    app::ChangeRequest correction{app::ChangeKind::correct,{"base-account-0",0},edit,{}};
    const auto preview=s.preview_committed_change(correction);
    s.apply_committed_change("correct",correction,preview.expected,preview.preview_sequence);
    capture(s,"edited",sgml(bank(first)),{{0,1,{}}},"qbo");materialize(s,"edited");
    check(s.row({"edited-account-0",0}).decision.disposition==imports::Disposition::duplicate);
    s.commit("edited-account-0",revision(s,"edited-account-0"),"edited-commit");
    check(s.balance()==Money(-150,cad) && s.current_ledger()[0].draft.payee=="Manually corrected");
    auto conflict=[&](std::string id,std::string rows) {
        capture(s,id,sgml(bank(rows)),{{0,1,{}}});materialize(s,id);
        const auto b=id+"-account-0";
        check(s.row({b,0}).decision.disposition==imports::Disposition::unresolved);
        rejected([&]{s.commit(b,revision(s,b),id+"-commit");});
        s.abort_import_group(id);s.verify();
    };
    conflict("date",txn("OVERLAP-A","20260202","-1.00"));
    conflict("amount",txn("OVERLAP-A","20260201","-1.01"));
    auto type=first;type.replace(type.find("DEBIT"),5,"CREDIT");conflict("type",type);
    conflict("missing",txn("","20260201","-1.00"));
    conflict("repeated",first+first);
    // Removed targets are reviewable; no resurrection or silent suppression.
    app::ChangeRequest removal{app::ChangeKind::remove,{"base-account-0",0},{},{}};
    const auto remove_preview=s.preview_committed_change(removal);
    s.apply_committed_change("remove",removal,remove_preview.expected,remove_preview.preview_sequence);
    conflict("removed",first);
    check(s.current_ledger().empty() && s.balance()==Money(0,cad));
    // The same bank ID in another resolved account is independent.
    s.create_account(2,"Savings",cad,"2026-01-01");s.select_account(2);
    s.opening(s.account().revision,"2026-01-01",Money(0,cad));
    capture(s,"other-account",sgml(bank(first,"CAD","2002")),{{0,2,{}}});materialize(s,"other-account");
    check(s.row({"other-account-account-0",0}).decision.disposition==imports::Disposition::include);
    s.verify();
}
// All fixtures here are independently synthetic. Changed memo/order/range
// prevents whole-file equality from supplying the expected decisions.
void exact_reimport() {
    const std::string coffee="D01/02/2026\nT-1\nPCoffee\n^\n";
    const std::string changed="D01/02/2026\nT-1.00\nPCoffee\nMAnother export\n^\n";
    for(bool accepted:{false,true}) {
        Directory d;
        {
            auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
            s.ingest("base",coffee,qif::DateOrder::dmy);
            if(accepted) {
                s.accept_all("accept");
                auto edit=s.committed_entry({"base",0}).current.draft;
                edit.date="2026-02-09";edit.payee="Corrected";
                edit.transaction=Transaction::repaired_import(Money(-100,cad),Money(-150,cad),{{"","",Money(-150,cad)}});
                app::ChangeRequest request{app::ChangeKind::correct,{"base",0},edit,{}};
                const auto preview=s.preview_committed_change(request);
                s.apply_committed_change("correct",request,preview.expected,preview.preview_sequence);
            }
            s.ingest("again",changed,qif::DateOrder::dmy);
            check(s.row({"again",0}).decision.duplicate_of==imports::Occurrence{"base",0});
            check(s.import_result("again").already_present==1 && s.batches().back().completed);
            check(s.acceptance_review().transactions==(accepted?0:1));
            check(s.balance()==Money(accepted?-150:-100,cad) && s.current_ledger().size()==1);
            // Repeated exports attach to the original identity, not the previous association.
            s.ingest("third",changed+"\n",qif::DateOrder::dmy);
            check(s.row({"third",0}).decision.duplicate_of==imports::Occurrence{"base",0});
            s.backup_to(d.path/"backup",password);s.verify();
        }
        for(const auto name:{"doc","backup"}) {
            auto s=ImportStore::open(d.path/name,password);
            check(s.current_ledger().size()==1 && s.balance()==Money(accepted?-150:-100,cad));
            check(s.row({"again",0}).decision.duplicate_of==imports::Occurrence{"base",0});s.verify();
        }
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        const std::string split="D01/02/2026\nT-1\nPCoffee\nSFood\n$-2\nSRefund\n$1\n^\n";
        const std::string other="D02/02/2026\nT-2\nPOther\n^\n";
        s.ingest("base",split+other,qif::DateOrder::dmy);s.accept_all("accept");
        s.ingest("reverse",other+changed,qif::DateOrder::dmy);
        check(s.row({"reverse",0}).decision.duplicate_of==imports::Occurrence{"base",1});
        check(s.row({"reverse",1}).decision.duplicate_of==imports::Occurrence{"base",0});
        s.ingest("shorter",changed,qif::DateOrder::dmy);
        check(s.import_result("shorter").already_present==1);
        check(s.current_ledger().size()==2 && s.balance()==Money(-300,cad));
        check(s.committed_entry({"base",0}).current.draft.transaction.allocations().size()==2);s.verify();
    }
    // Cross-format recognition works in either order and retains bank-ID protection.
    for(bool qif_first:{false,true}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        if(qif_first)s.ingest("qif",coffee,qif::DateOrder::dmy);
        capture(s,"bank",sgml(bank(txn("ID-A","20260201","-1.00"))),{{0,1,{}}});materialize(s,"bank");
        if(!qif_first)s.ingest("qif",coffee,qif::DateOrder::dmy);
        check(s.current_ledger().size()==1 && s.balance()==Money(-100,cad));
        capture(s,"different-id",sgml(bank(txn("ID-B","20260201","-1.00"))),{{0,1,{}}});materialize(s,"different-id");
        check(s.row({"different-id-account-0",0}).decision.disposition==imports::Disposition::include);
        check(s.current_ledger().size()==2 && s.balance()==Money(-200,cad));s.verify();
    }
    // Ambiguity on either side must not be resolved by iteration order.
    for(bool repeated_target:{false,true}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base",repeated_target?coffee+coffee:coffee,qif::DateOrder::dmy);
        s.ingest("repeat",repeated_target?changed:changed+changed,qif::DateOrder::dmy);
        check(s.row({"repeat",0}).decision.disposition==imports::Disposition::unresolved);
        if(!repeated_target)check(s.row({"repeat",1}).decision.disposition==imports::Disposition::unresolved);
        check(s.current_ledger().size()==3 && s.balance()==Money(-300,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base",coffee,qif::DateOrder::dmy);
        s.ingest("longer",changed+"D02/02/2026\nT-2\nPNew\n^\n",qif::DateOrder::dmy);
        check(s.import_result("longer").already_present==1 && s.import_result("longer").added==1);
        check(s.current_ledger().size()==2 && s.balance()==Money(-300,cad));
        // Existing explicit Include can reverse a pending automatic decision.
        s.decide({"longer",0},revision(s,"longer"),{imports::Disposition::include,{}});
        check(s.current_ledger().size()==3 && s.balance()==Money(-400,cad));s.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base",coffee,qif::DateOrder::dmy);s.accept_all("accept");
        app::ChangeRequest remove{app::ChangeKind::remove,{"base",0},{},{}};
        const auto preview=s.preview_committed_change(remove);
        s.apply_committed_change("remove",remove,preview.expected,preview.preview_sequence);
        s.ingest("repeat",changed,qif::DateOrder::dmy);
        check(s.row({"repeat",0}).decision.disposition==imports::Disposition::unresolved);
        check(s.current_ledger().size()==1 && s.balance()==Money(-100,cad));s.verify();
        s.create_account(2,"Other",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("other",changed,qif::DateOrder::dmy);
        check(s.row({"other",0}).decision.disposition==imports::Disposition::include);s.verify();
    }
    for(bool accepted:{false,true}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base",coffee,qif::DateOrder::dmy);
        if(accepted)s.accept_all("accept");
        s.ingest("repeat",changed+"D02/02/2026\nT-2\nPNew\n^\n",qif::DateOrder::dmy);
        s.rollback_account("rollback",s.rollback_review().token);
        check(s.current_ledger().size()==(accepted?1:0) && s.balance()==Money(accepted?-100:0,cad));
        s.verify();s.backup_to(d.path/"backup",password);
        auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
        check(restored.current_ledger().size()==(accepted?1:0));
        check(query(d.path/"doc","SELECT count(*) FROM source_occurrences WHERE payee='New'")=="0");
    }
    // Exact means exact: changed dates, payee case, absent payees or malformed
    // source facts do not qualify, even if the amount agrees.
    for(const auto source:{"D02/02/2026\nT-1\nPCoffee\n^\n", "D01/02/2026\nT-1\nPcoffee\n^\n",
                          "D01/02/2026\nT-1\n^\n", "D01/02/2026\nTbad\nPCoffee\n^\n"}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("base",coffee,qif::DateOrder::dmy);s.ingest("changed",source,qif::DateOrder::dmy);
        check(s.row({"changed",0}).decision.disposition!=imports::Disposition::duplicate);s.verify();
    }
}
void rollback() {
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("keep","D01/02/2026\nT-1\nPAccepted\n^\n",qif::DateOrder::dmy);s.accept_all("accept");
        const std::string source="D02/02/2026\nT-2\nPTransient\n^\n";
        s.ingest("first",source,qif::DateOrder::dmy);s.ingest("second",source,qif::DateOrder::dmy);
        auto preview=s.rollback_review();check(preview.transactions==1);
        check(s.rollback_account("rollback",preview.token).transactions==1);check(s.rollback_account("rollback",preview.token).transactions==1);
        check(s.balance()==Money(-100,cad) && s.current_ledger().size()==1 && s.batches().size()==1);s.verify();
        check(query(d.path/"doc","SELECT count(*) FROM sources")=="1");
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("partial","D01/02/2026\nT-1\nPAccepted\n^\nD02/02/2026\nTbad\nPFailed\n^\n",qif::DateOrder::dmy);s.accept_all("accept");
        check(s.rollback_review().failures==1);s.rollback_account("remove-failure",s.rollback_review().token);
        check(s.balance()==Money(-100,cad) && s.acceptance_review().failed_records==0);check(s.row({"partial",1}).excluded_by_clear);s.verify();
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Other",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        const auto bytes=sgml(bank(txn("ONE","20260201","-1.00"),"CAD","1001")+bank(txn("TWO","20260201","2.00"),"CAD","1002"));
        capture(s,"group",bytes,{{0,1,{}},{1,2,{}}});materialize(s,"group");
        s.rollback_account("first-account",s.rollback_review().token);check(s.batches().empty());s.select_account(2);check(s.balance()==Money(200,cad));s.verify();
        s.accept_all("accept-other");s.verify();s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("origin","D01/02/2026\nT-10\nL[Old Visa export]\n^\n",qif::DateOrder::dmy);
        app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="payment";r.expected_sequence=s.transfer_sequence();r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("make",r);s.accept_all("accept",true);
        s.select_account(2);s.ingest("bank","D01/02/2026\nT10\nPBank payment\n^\n",qif::DateOrder::dmy);r={};r.action=app::TransferAction::propose_match;r.link="payment";r.expected_sequence=s.transfer_sequence();r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(1000,cad),"2026-02-01"};s.apply_transfer("match",r);
        s.rollback_account("remove-bank-source",s.rollback_review().token);check(s.balance()==Money(1000,cad) && s.current_ledger().size()==1);check(!s.transfers().front().link.destination.proposed_evidence);s.verify();
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");
        const std::string bytes="!Type:Cat\nNUnused category\nE\n^\n!Account\nNExport name\nTBank\nB25\n^\n!Type:Bank\nD01/02/2026\nT-1\nPTransient\n^\n";
        app::OperationContext context;s.prepare_import_group("created",bytes,qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Export name",cad,"2026-01-01"}}},context);materialize(s,"created");s.select_account(2);
        s.review_import_balance("created",0,1,app::ReportedBalanceChoice::opening,"2026-01-01");s.rollback_account("remove-work",s.rollback_review().token);
        check(s.accounts().size()==2 && s.resolve_account_name("Export name")==2);check(s.batches().empty());s.verify();
        s.ingest("later","D02/02/2026\nT-2\nPNew transaction\n^\n",qif::DateOrder::dmy);s.accept_all("accept-later");check(s.account().opening==Money(2500,cad));check(s.balance()==Money(2300,cad));check(s.category_catalog().categories.size()==1);s.verify();
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
    {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));
        const std::string bytes="D01/02/2026\nT-1\nPOriginal\n^\n";
        s.ingest("accepted",bytes,qif::DateOrder::dmy);s.accept_all("accept");s.ingest("repeat",bytes,qif::DateOrder::dmy);
        auto edit=s.committed_entry({"accepted",0}).current.draft;edit.payee="Corrected";
        app::ChangeRequest change{app::ChangeKind::correct,{"accepted",0},edit,{}};auto preview=s.preview_committed_change(change);s.apply_committed_change("correct",change,preview.expected,preview.preview_sequence);
        s.rollback_account("remove-repeat",s.rollback_review().token);s.verify();check(s.committed_entry({"accepted",0}).current.draft.payee=="Corrected");
        app::ChangeRequest undo{app::ChangeKind::undo,{"accepted",0},{},"correct"};preview=s.preview_committed_change(undo);s.apply_committed_change("undo",undo,preview.expected,preview.preview_sequence);check(s.balance()==Money(-100,cad));s.verify();
    }
    for(bool independent:{false,true}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("origin","D01/02/2026\nT-10\nL[Old Visa export]\n^\n",qif::DateOrder::dmy);
        app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="payment";r.expected_sequence=s.transfer_sequence();r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("make",r);
        if(independent){s.select_account(2);s.ingest("bank","D01/02/2026\nT10\nPBank payment\n^\n",qif::DateOrder::dmy);r={};r.action=app::TransferAction::propose_match;r.link="payment";r.expected_sequence=s.transfer_sequence();r.evidence=transfers::BankEvidence{{2,{"bank",0},"source-0"},Money(1000,cad),"2026-02-01"};s.apply_transfer("match",r);s.select_account(1);}
        s.rollback_account("clear",s.rollback_review().token);check(s.current_ledger().empty());s.select_account(2);check(s.balance()==Money(independent?1000:0,cad));check(s.transfers().empty());s.verify();
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.verify();
    }
}
void partial_transfer_guards() {
    Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
    app::TransferRequest manual{};manual.action=app::TransferAction::manual;manual.link="manual";manual.expected_sequence=s.transfer_sequence();manual.destination=2;manual.amount=Money(-1000,cad);manual.date="2026-02-01";s.apply_transfer("manual",manual);s.accept_all("accept-manual",true);
    s.select_account(2);s.ingest("partial","D01/02/2026\nT10\nPBank\n^\nD02/02/2026\nTbad\n^\n",qif::DateOrder::dmy);s.accept_all("accept-valid");
    app::TransferRequest request{};request.action=app::TransferAction::propose_match;request.link="manual";request.expected_sequence=s.transfer_sequence();request.batch_revision=revision(s,"partial");request.evidence=transfers::BankEvidence{{2,{"partial",0},"source-0"},Money(1000,cad),"2026-02-01"};rejected([&]{s.apply_transfer("invalid-match",request);});
    request={};request.action=app::TransferAction::from_draft;request.link="invalid";request.expected_sequence=s.transfer_sequence();request.origin=imports::Occurrence{"partial",0};request.allocation="source-0";request.destination=1;request.batch_revision=revision(s,"partial");rejected([&]{s.apply_transfer("invalid-generate",request);});check(s.balance()==Money(2000,cad));s.verify();
}
void rollback_stale_reopen() {
    Directory d;std::int64_t stale;
    {auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.ingest("one","D01/02/2026\nT1\n^\n",qif::DateOrder::dmy);stale=s.rollback_review().token;s.rollback_account("first",stale);s.ingest("two","D02/02/2026\nT2\n^\n",qif::DateOrder::dmy);s.rollback_account("second",s.rollback_review().token);s.ingest("three","D03/02/2026\nT3\n^\n",qif::DateOrder::dmy);}
    {auto s=ImportStore::open(d.path/"doc",password);rejected([&]{s.rollback_account("first",stale);});check(s.balance()==Money(300,cad));s.verify();}
}
void generated_ownership() {
    for(bool retain:{false,true}) {
        Directory d;auto s=create(d.path/"doc");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("origin","D01/02/2026\nT-10\nL[Old Visa export]\n^\n",qif::DateOrder::dmy);
        app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="payment";r.expected_sequence=s.transfer_sequence();r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("make",r);s.accept_all("accept",true);
        s.select_account(2);const auto payment=txn("PAYMENT","20260201","10.00");capture(s,"bank",sgml(bank(payment)),{{0,2,{}}});materialize(s,"bank");
        r={};r.action=app::TransferAction::propose_match;r.link="payment";r.expected_sequence=s.transfer_sequence();r.batch_revision=revision(s,"bank-account-0");r.evidence=transfers::BankEvidence{{2,{"bank-account-0",0},"source-0"},Money(1000,cad),"2026-02-01"};s.apply_transfer("match",r);
        check(s.row({"bank-account-0",0}).matched_generated && s.acceptance_review().transactions==0 && s.balance()==Money(1000,cad));check(s.import_result("bank-account-0").already_present==1);
        if(s.rules_supported()){app::OperationContext context;const auto preview=s.preview_pending_rules({"bank-account-0",revision(s,"bank-account-0"),std::nullopt,false},context);check(preview.evaluated==0);s.apply_pending_rules("matched-source-rules",{preview.batch,preview.batch_revision,preview.ruleset_revision,preview.catalog_revision,preview.replace_manual},context);}
        s.verify();
        rejected([&]{s.save({"bank-account-0",0},revision(s,"bank-account-0"),*s.row({"bank-account-0",0}).draft);});
        capture(s,"repeat",sgml(bank(payment+(retain?txn("NEW","20260202","-2.00"):""))),{{0,2,{}}},"qfx");materialize(s,"repeat");
        check(s.row({"repeat-account-0",0}).decision.disposition==imports::Disposition::duplicate);
        check(s.acceptance_review().transactions==(retain?1:0));if(retain)s.accept_all("accept-new");s.verify();
        s.rollback_account("forget-bank",s.rollback_review().token);check(s.balance()==Money(retain?800:1000,cad));check(s.current_ledger().size()==(retain?2:1));s.verify();
        s.backup_to(d.path/"backup",password);auto restored=ImportStore::open(d.path/"backup",password);restored.select_account(2);check(restored.balance()==Money(retain?800:1000,cad));restored.verify();
    }
}
void generated_faults() {
    fault_vfs::install();
    const auto match=[](ImportStore& s) {app::TransferRequest r{};r.action=app::TransferAction::propose_match;r.link="payment";r.expected_sequence=s.transfer_sequence();r.batch_revision=revision(s,"bank-account-0");r.evidence=transfers::BankEvidence{{2,{"bank-account-0",0},"source-0"},Money(1000,cad),"2026-02-01"};s.apply_transfer("match",r);};
    for(int mode:{0,1,2}) {
        const auto setup=[&](const fs::path& path) {
            auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
            s.ingest("origin","D01/02/2026\nT-10\nL[Old Visa export]\n^\n",qif::DateOrder::dmy);
            app::TransferRequest r{};r.action=app::TransferAction::from_draft;r.link="payment";r.expected_sequence=s.transfer_sequence();r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;s.apply_transfer("make",r);s.accept_all("accept",true);s.select_account(2);
            const auto payment=txn("PAYMENT","20260201","10.00");capture(s,"bank",sgml(bank(payment)),{{0,2,{}}});materialize(s,"bank");
            if(mode>0)match(s);
            if(mode==2){capture(s,"retained",sgml(bank(payment+txn("NEW","20260202","-2.00"))),{{0,2,{}}},"qfx");materialize(s,"retained");s.accept_all("retain");}
        };
        const auto apply=[&](ImportStore& s){s.select_account(2);if(mode==0)match(s);else s.rollback_account("forget",s.rollback_review().token);};
        const auto completed=[&](ImportStore& s){s.select_account(2);return mode==0?s.transfer_outcome("match").has_value():s.rollback_outcome("forget").has_value();};
        const auto recover=[&](const fs::path& path){auto s=ImportStore::open(path,password);s.select_account(2);const bool done=completed(s);check(s.balance()==Money(mode==2?800:mode==0 && !done?2000:1000,cad));if(!done)apply(s);check(s.balance()==Money(mode==2?800:1000,cad));s.verify();};
        int count=0;{Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);s.select_account(2);fault_vfs::arm(-1);apply(s);count=fault_vfs::seen;fault_vfs::disarm();}check(count>0);
        for(int n=1;n<=count;++n){Directory d;setup(d.path/"doc");{auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}fault_vfs::disarm();}recover(d.path/"doc");no_leaks(d.path);}
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;setup(d.path/"doc");const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});apply(s);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);{auto s=ImportStore::open(d.path/"doc",password);check(completed(s)==(phase==ImportPhase::after_commit));}recover(d.path/"doc");}
        std::cout<<"Generated ownership mode "<<mode<<": "<<count<<" fault positions\n";
    }
}
void rollback_faults() {
    fault_vfs::install();
    auto setup=[](const fs::path& path){auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));s.ingest("accepted","D01/02/2026\nT-1\nPAccepted\n^\n",qif::DateOrder::dmy);s.accept_all("accept");s.ingest("pending","D02/02/2026\nT-2\nPDiscarded\n^\n",qif::DateOrder::dmy);};
    auto recover=[](const fs::path& path){auto s=ImportStore::open(path,password);const bool complete=s.rollback_outcome("clear").has_value();check(s.balance()==Money(complete?-100:-300,cad));check(s.current_ledger().size()==(complete?1:2));s.rollback_account("clear",s.rollback_review().token);check(s.balance()==Money(-100,cad));s.verify();};
    int count=0;{Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);const auto token=s.rollback_review().token;fault_vfs::arm(-1);s.rollback_account("clear",token);count=fault_vfs::seen;fault_vfs::disarm();}check(count>0);
    for(int n=1;n<=count;++n){Directory d;setup(d.path/"doc");{auto s=ImportStore::open(d.path/"doc",password);const auto token=s.rollback_review().token;fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.rollback_account("clear",token);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}fault_vfs::disarm();}recover(d.path/"doc");no_leaks(d.path);}
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;setup(d.path/"doc");const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});s.rollback_account("clear",s.rollback_review().token);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);{auto s=ImportStore::open(d.path/"doc",password);check(s.rollback_outcome("clear").has_value()==(phase==ImportPhase::after_commit));}recover(d.path/"doc");}
    std::cout<<"Rollback fault positions: "<<count<<'\n';
}
void acceptance_faults() {
    fault_vfs::install();
    auto setup=[](const fs::path& path) {
        auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("first","D01/02/2026\nT-1\nPOne\n^\nD02/02/2026\nTbad\nPRepair\n^\n",qif::DateOrder::dmy);
        s.create_account(2,"Other",Currency("USD"),"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,Currency("USD")));
        s.ingest("second","D01/02/2026\nT2\nPTwo\n^\n",qif::DateOrder::dmy);
    };
    auto recovered=[](const fs::path& path) {
        auto s=ImportStore::open(path,password);const bool accepted=s.acceptance_outcome("all").has_value();
        check(s.acceptance_review().transactions==(accepted?0:2) && s.acceptance_review().failed_records==1);
        check(s.committed_balance()==Money(accepted?-100:0,cad));s.select_account(2);
        check(s.committed_balance()==Money(accepted?200:0,Currency("USD")));
        const auto receipt=s.accept_all("all");check(receipt.transactions==2 && receipt.failed_records==1);
        check(s.accept_all("all").transactions==2);s.verify();
    };
    int count=0;
    {Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);s.accept_all("all");count=fault_vfs::seen;fault_vfs::disarm();}
    check(count>0);
    for(int n=1;n<=count;++n) {
        Directory d;setup(d.path/"doc");
        {auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
         try{s.accept_all("all");}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}fault_vfs::disarm();}
        recovered(d.path/"doc");no_leaks(d.path);
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}) {
        Directory d;setup(d.path/"doc");const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});s.accept_all("all");_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        {auto s=ImportStore::open(d.path/"doc",password);check(s.acceptance_outcome("all").has_value()==(phase==ImportPhase::after_commit));}
        recovered(d.path/"doc");no_leaks(d.path);
    }
    std::cout<<"Dataset acceptance fault positions: "<<count<<'\n';
}
void overlap_faults(bool exact=false) {
    fault_vfs::install();
    const auto text=sgml(bank(txn("FAULT-A","20260201","-1.00","SYNTHETIC-STATEMENT-MARKER")));
    auto setup=[&](const fs::path& path) {
        auto s=create(path);s.opening(0,"2026-01-01",Money(0,cad));
        if(exact) {
            s.ingest("base-account-0","D01/02/2026\nT-1\nPSYNTHETIC-STATEMENT-MARKER\n^\n",qif::DateOrder::dmy);
        } else { capture(s,"base",text,{{0,1,{}}},"aso");materialize(s,"base"); }
        s.commit("base-account-0",revision(s,"base-account-0"),"base-commit");
        capture(s,"overlap",text,{{0,1,{}}},"qfx");
    };
    auto reimport=[&](ImportStore& s){if(!s.import_group_outcome("overlap"))capture(s,"overlap",text,{{0,1,{}}},"qfx");};
    auto recovered=[&](const fs::path& path) {
        auto s=ImportStore::open(path,password);reimport(s);
        materialize(s,"overlap");
        check(s.row({"overlap-account-0",0}).decision.disposition==imports::Disposition::duplicate);
        check(s.current_ledger().size()==1 && s.balance()==Money(-100,cad));
        if(!s.outcome("overlap-commit"))s.commit("overlap-account-0",revision(s,"overlap-account-0"),"overlap-commit");
        check(s.current_ledger().size()==1 && s.balance()==Money(-100,cad));s.verify();
    };
    for(bool publication:{false,true}) {
        int count=0;
        {
            Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);reimport(s);
            if(publication)materialize(s,"overlap");
            fault_vfs::arm(-1);
            if(publication)s.commit("overlap-account-0",revision(s,"overlap-account-0"),"overlap-commit");
            else materialize(s,"overlap");
            count=fault_vfs::seen;fault_vfs::disarm();
        }
        check(count>0);
        for(int n=1;n<=count;++n) {
            Directory d;setup(d.path/"doc");
            {
                auto s=ImportStore::open(d.path/"doc",password);reimport(s);
                if(publication)materialize(s,"overlap");
                fault_vfs::arm(n,n%2?SQLITE_FULL:SQLITE_IOERR_WRITE);
                try {
                    if(publication)s.commit("overlap-account-0",revision(s,"overlap-account-0"),"overlap-commit");
                    else materialize(s,"overlap");
                } catch(const ImportError&) {} catch(const app::OperationOutcomeUnknown&) {}
                fault_vfs::disarm();
            }
            recovered(d.path/"doc");no_leaks(d.path);
        }
        for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}) {
            Directory d;setup(d.path/"doc");
            if(publication){auto s=ImportStore::open(d.path/"doc",password);reimport(s);materialize(s,"overlap");}
            const auto child=fork();check(child>=0);
            if(child==0) {
                auto s=ImportStore::open(d.path/"doc",password);reimport(s);
                s.observer([&](ImportPhase at){if(at==phase)_exit(72);});
                if(publication)s.commit("overlap-account-0",revision(s,"overlap-account-0"),"overlap-commit");
                else materialize(s,"overlap");
                _exit(0);
            }
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
            {
                auto s=ImportStore::open(d.path/"doc",password);
                if(publication)check(s.outcome("overlap-commit").has_value()==(phase==ImportPhase::after_commit));
                else if(phase==ImportPhase::after_commit)check(s.import_group_outcome("overlap")->state==app::GroupState::reviewing);
                else check(!s.import_group_outcome("overlap"));
            }
            recovered(d.path/"doc");no_leaks(d.path);
        }
        std::cout << (publication?"Overlap association":"Overlap preparation") << " fault positions: " << count << '\n';
    }
}
void ready(const fs::path& path, const std::string& text) {
    auto s = create(path);
    s.opening(0, "2026-01-01", Money(0, cad));
    write_file(path.parent_path() / "source.ofx", text);
    capture(s, "ofx", text, {{0, 1, {}}});
    materialize(s, "ofx");
}
void finish(const fs::path& path, const std::string& text) {
    auto s = ImportStore::open(path, password);
    s.verify();
    if(!s.outcome("ofx-commit")) check(s.commit("ofx-account-0", revision(s, "ofx-account-0"), "ofx-commit").after == Money(-100, cad));
    check(s.committed_balance() == Money(-100, cad) && s.ledger(false).size() == 1 && s.evidence("ofx-account-0") == text);
    check(read_file(path.parent_path() / "source.ofx") == text);
    s.verify();
}
void faults() {
    fault_vfs::install();
    const auto text = sgml(bank(txn("FIT-1", "20260201", "-1.00", "SYNTHETIC-STATEMENT-MARKER")));
    int count = 0;
    {
        Directory d;
        ready(d.path / "doc", text);
        auto s = ImportStore::open(d.path / "doc", password);
        fault_vfs::arm(-1);
        s.commit("ofx-account-0", 1, "ofx-commit");
        count = fault_vfs::seen;
        fault_vfs::disarm();
    }
    check(count > 0);
    for(int n = 1; n <= count; ++n) {
        Directory d;
        ready(d.path / "doc", text);
        {
            auto s = ImportStore::open(d.path / "doc", password);
            fault_vfs::arm(n, n % 2 ? SQLITE_FULL : SQLITE_IOERR_WRITE);
            try { s.commit("ofx-account-0", 1, "ofx-commit"); }
            catch(const ImportError&) {}
            catch(const app::OperationOutcomeUnknown&) {}
            fault_vfs::disarm();
        }
        no_leaks(d.path);
        finish(d.path / "doc", text);
    }
    {
        Directory d;
        ready(d.path / "doc", text);
        const auto child = fork();
        check(child >= 0);
        if(child == 0) {
            auto s = ImportStore::open(d.path / "doc", password);
            fault_vfs::arm(std::max(1, count / 2), SQLITE_IOERR_WRITE, true, true);
            s.commit("ofx-account-0", 1, "ofx-commit");
            _exit(0);
        }
        int status = 0;
        check(waitpid(child, &status, 0) == child && WIFEXITED(status) && (WEXITSTATUS(status) == 71 || WEXITSTATUS(status) == 0));
        finish(d.path / "doc", text);
        no_leaks(d.path);
    }
    for(const auto phase : {ImportPhase::before_commit, ImportPhase::after_commit}) {
        Directory d;
        ready(d.path / "doc", text);
        const auto child = fork();
        check(child >= 0);
        if(child == 0) {
            auto s = ImportStore::open(d.path / "doc", password);
            s.observer([&](ImportPhase at) { if(at == phase) _exit(72); });
            s.commit("ofx-account-0", 1, "ofx-commit");
            _exit(0);
        }
        int status = 0;
        check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 72);
        {
            auto s = ImportStore::open(d.path / "doc", password);
            check(s.outcome("ofx-commit").has_value() == (phase == ImportPhase::after_commit));
        }
        finish(d.path / "doc", text);
        no_leaks(d.path);
    }
    {
        Directory d;
        auto s = create(d.path / "doc");
        s.opening(0, "2026-01-01", Money(0, cad));
        capture(s, "ofx", text, {{0, 1, {}}});
        app::OperationContext cancelled;
        cancelled.progress = [&](std::string_view phase) {
            if(phase == "Preparing all source account drafts") cancelled.request();
        };
        bool was_cancelled = false;
        try { s.resume_import_group("ofx", cancelled); }
        catch(const app::OperationCancelled&) { was_cancelled = true; }
        check(was_cancelled && s.import_groups()[0].state == app::GroupState::captured);
        app::OperationContext retry;
        check(s.resume_import_group("ofx", retry).state == app::GroupState::reviewing);
        check(s.commit("ofx-account-0", revision(s, "ofx-account-0"), "ofx-commit").after == Money(-100, cad));
        check(s.evidence("ofx-account-0") == text);
        s.verify();
        no_leaks(d.path);
    }
    std::cout << "statement commit fault positions: " << count << "\n";
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "";
        if(name == "review") review();
        else if(name == "reimport") exact_reimport();
        else if(name == "reimport_faults") overlap_faults(true);
        else if(name == "overlap") strict_overlap();
        else if(name == "rollback_faults") rollback_faults();
        else if(name == "generated_faults") generated_faults();
        else if(name == "rollback") {rollback();partial_transfer_guards();rollback_stale_reopen();generated_ownership();}
        else if(name == "acceptance_faults") acceptance_faults();
        else if(name == "overlap_faults") overlap_faults();
        else if(name == "common_matching") common_matching();
        else if(name == "faults") faults();
        else return 2;
        std::cout << "Statement store " << name << " passed\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
