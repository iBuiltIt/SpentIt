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
        check(row.decision.disposition == imports::Disposition::unresolved);
        check(row.candidates.size() == 1 && row.candidates[0] == imports::Occurrence{"qif", 0});
        check(imports::candidate_basis(row.source, s.row({"qif", 0}).source) == imports::MatchBasis::same_source_fields);
        check(row.source.format == "ofx" && row.source.fitid == "FIT-1" && row.source.time_evidence == "120000.000[-5:EST]");
        check(row.source.date == "2026-02-01" && row.source.original_amount && row.source.original_amount->units() == -1234);
        const auto rev = revision(s, "overlap-account-0");
        rejected([&] { s.commit("overlap-account-0", rev, "overlap-commit"); });
        check(s.committed_balance() == Money(-1234, cad) && s.ledger(false).size() == 1);
        s.decide({"overlap-account-0", 0}, rev, {imports::Disposition::duplicate, imports::Occurrence{"qif", 0}});
        const auto receipt = s.commit("overlap-account-0", rev + 1, "overlap-commit");
        check(receipt.included == 0 && receipt.excluded == 1 && receipt.after == Money(-1234, cad));
        check(s.commit("overlap-account-0", rev + 1, "overlap-commit").after == receipt.after);
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
    check(query(path, "PRAGMA user_version") == (query(path, "SELECT count(*) FROM sqlite_master WHERE name='rule_state'") == "1" ? "24" : "23"));
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
        check(uncertain.decision.disposition == imports::Disposition::unresolved);
        check(imports::candidate_basis(uncertain.source, s.row({"qif", 0}).source) == imports::MatchBasis::uncertain_date);
        const auto date_rev = revision(s, "dated-account-0");
        s.decide({"dated-account-0", 0}, date_rev, {imports::Disposition::duplicate, imports::Occurrence{"qif", 0}});
        check(s.commit("dated-account-0", date_rev + 1, "dated-commit").excluded == 1);
        check(s.committed_balance() == Money(-3703, cad) && s.ledger(false).size() == 3);
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
        check(s.row({"same-account-0", 0}).decision.disposition == imports::Disposition::include);
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
        check(s.commit("same-account-0", same_rev + 1, "same-commit").included == 2);
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
        check(first.decision.disposition == imports::Disposition::unresolved && first.candidates.size() == 1 && first.candidates[0] == imports::Occurrence{"card-account-0", 0});
        check(imports::candidate_basis(first.source, s.row({"card-account-0", 0}).source) == imports::MatchBasis::same_fitid);
        check(second.decision.disposition == imports::Disposition::unresolved && second.candidates.size() == 1 && second.candidates[0] == imports::Occurrence{"card-account-0", 1});
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
        else if(name == "faults") faults();
        else return 2;
        std::cout << "Statement store " << name << " passed\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
