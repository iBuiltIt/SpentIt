#include "action_chain.hpp"
#include <sqlite3.h>
#include <functional>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

using spentit::storage::ActionChain;
using namespace spentit::app;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void sql(sqlite3* db, const char* text) {
    check(sqlite3_exec(db, text, nullptr, nullptr, nullptr) == SQLITE_OK, "SQL failed");
}
template<class F> void refuses(F f) {
    bool failed = false;
    try { f(); } catch (const std::runtime_error&) { failed = true; }
    check(failed, "Expected refusal");
}
struct Database {
    sqlite3* db = nullptr;
    explicit Database(const char* path = ":memory:") {
        check(sqlite3_open(path, &db) == SQLITE_OK, "Open failed");
        check(sqlite3_key(db, "synthetic-test-key", 18) == SQLITE_OK, "Key failed");
        sql(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=EXTRA; PRAGMA fullfsync=ON; PRAGMA foreign_keys=ON; PRAGMA temp_store=MEMORY; PRAGMA secure_delete=ON");
    }
    ~Database() { sqlite3_close(db); }
    void transaction(const std::function<void()>& f) {
        sql(db, "BEGIN IMMEDIATE");
        try { f(); sql(db, "COMMIT"); }
        catch (...) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); throw; }
    }
};
const std::string digest(32, 'x');
ActionSummary action(std::string id, ActionKind kind = ActionKind::transaction) {
    ActionSummary result; result.id = id; result.payload = id; result.label = "Edit transaction";
    result.kind = kind; result.count = 1; return result;
}
void initialize(ActionChain& chain, sqlite3* db) {
    sql(db, "CREATE TABLE document_session(singleton INTEGER PRIMARY KEY,generation INTEGER); INSERT INTO document_session VALUES(1,0); CREATE TABLE mutation_generation(singleton INTEGER PRIMARY KEY,revision INTEGER); INSERT INTO mutation_generation VALUES(1,0)");
    chain.initialize();
}
void chain_tests() {
    Database d; ActionChain c(d.db);
    refuses([&]{c.initialize();});
    d.transaction([&]{initialize(c,d.db);});
    const auto initial = c.token();
    check(!c.head() && initial.epoch == 0, "Initial state");
    d.transaction([&]{c.applied(initial, action("A"), digest);});
    const auto a = c.token();
    d.transaction([&]{c.applied(a, action("B", ActionKind::review), digest);});
    const auto b = c.token();
    d.transaction([&]{c.reversed(b, "undoB", digest);});
    check(c.head()->id == "A" && c.token().epoch > b.epoch, "Pop without rewinding epoch");
    check(c.outcome("B", digest) == ActionChain::Outcome::undone, "Original retry reports Undo");
    d.transaction([&]{refuses([&]{c.durable_outcome("B", digest);});});
    check(c.durable_outcome("B", digest) == ActionChain::Outcome::undone, "Durable outcome requires a completed transaction");
    const auto restored = c.token();
    d.transaction([&]{c.reversed(b, "undoB", digest); c.applied(a, action("B", ActionKind::review), digest);});
    check(c.token() == restored, "Retries cannot pop or republish an older action");
    refuses([&]{d.transaction([&]{c.reversed(b, "anotherUndo", digest);});});
    refuses([&]{d.transaction([&]{c.reversed(restored, "A", digest);});});
    refuses([&]{c.outcome("B", std::string(32, 'y'));});
    d.transaction([&]{c.compatible_change(restored);});
    check(c.head()->id == "A", "Definitions preserve head");
    refuses([&]{d.transaction([&]{c.reversed(restored, "stale", digest);});});
    d.transaction([&]{c.applied(c.token(), action("C", ActionKind::rule_application), digest);});
    check(c.head()->parent == "A", "Branch excludes undone action");
    c.verify();
    const auto page = c.summaries(0, 2);
    check(page.size() == 2 && page[0].id == "C" && page[1].id == "B", "Paged summaries retain undone labels");
    check(c.summaries(page.back().sequence, 2).front().id == "A", "Exclusive page cursor");
    const auto before_noop = c.token();
    d.transaction([&]{c.no_change(before_noop, "noop", digest);});
    check(c.token() == before_noop, "No-op adds no action or epoch");
    refuses([&]{c.require_empty();});
    // A rejected transaction rolls back both the action and the caller's writes.
    sql(d.db, "CREATE TABLE financial_test(value INTEGER); INSERT INTO financial_test VALUES(10)");
    refuses([&]{d.transaction([&]{
        sql(d.db, "UPDATE financial_test SET value=20");
        c.applied(c.token(), action("failed"), digest);
        throw std::runtime_error("Simulated domain validation failure");
    });});
    check(c.token() == before_noop && !c.outcome("failed", digest), "Rollback preserves head and retry state");
    sqlite3_stmt* query = nullptr;
    check(sqlite3_prepare_v2(d.db, "SELECT value FROM financial_test", -1, &query, nullptr) == SQLITE_OK, "Prepare");
    check(sqlite3_step(query) == SQLITE_ROW && sqlite3_column_int(query, 0) == 10, "Caller writes roll back together");
    sqlite3_finalize(query);
    const auto sequence = c.head()->sequence;
    d.transaction([&]{c.expire(c.token());});
    check(!c.head() && c.summaries(0, 10).empty(), "Close expires summaries");
    check(c.outcome("A", digest) == ActionChain::Outcome::expired, "Close retains compact retry");
    d.transaction([&]{c.applied(initial, action("A"), digest);});
    check(!c.head(), "Expired retry cannot execute again");
    d.transaction([&]{c.applied(c.token(), action("D"), digest);});
    check(c.head()->sequence > sequence, "Sequence is never reused after expiry");
    c.verify();
    refuses([&]{d.transaction([&]{
        sql(d.db, "UPDATE action_receipts SET status=1 WHERE id='D'"); c.verify();
    });});
    c.verify();
    const auto overflow_before = c.token();
    refuses([&]{d.transaction([&]{
        sql(d.db, "UPDATE mutation_generation SET revision=9223372036854775807");
        c.applied(c.token(), action("overflow"), digest);
    });});
    check(c.token() == overflow_before && !c.outcome("overflow", digest), "Epoch overflow rolls back publication");
    sql(d.db, "UPDATE action_headers SET parent=id WHERE id='D'");
    refuses([&]{c.verify();});
}
void import_tests() {
    Database d; ActionChain c(d.db); d.transaction([&]{initialize(c,d.db);});
    const auto initial = c.token();
    d.transaction([&]{c.begin_import(initial, action("source", ActionKind::import), digest);});
    check(!c.head() && c.preparing()->id == "source", "Preparing import is not an applied head");
    check(c.outcome("source", digest) == ActionChain::Outcome::preparing, "Preparation is not completed");
    d.transaction([&]{c.begin_import(initial, action("source", ActionKind::import), digest);});
    refuses([&]{d.transaction([&]{c.applied(c.token(), action("unrelated"), digest);});});
    refuses([&]{d.transaction([&]{c.compatible_change(c.token());});});
    refuses([&]{d.transaction([&]{c.expire(c.token());});});
    refuses([&]{d.transaction([&]{c.finish_import("source", false);});});
    d.transaction([&]{c.import_stage("source", ImportStage::materialized);});
    refuses([&]{d.transaction([&]{c.import_stage("source", ImportStage::captured);});});
    d.transaction([&]{c.import_stage("source", ImportStage::awaiting_decision);});
    c.verify();
    d.transaction([&]{c.finish_import("source", false);});
    check(c.head()->id == "source" && !c.preparing(), "One published import root");
    check(c.summaries(0, 200).size() == 1, "Import stages create no child actions");
    d.transaction([&]{c.begin_import(c.token(), action("cancelled", ActionKind::import), digest);});
    d.transaction([&]{c.finish_import("cancelled", true);});
    check(c.head()->id == "source" && c.outcome("cancelled", digest) == ActionChain::Outcome::cancelled, "Cancellation retains parent");
    c.verify();
    d.transaction([&]{c.reversed(c.token(), "undoImport", digest);});
    c.require_empty(); c.verify();
    // These are coordinator tests only; no imported financial rows are written.
}
void failure_tests() {
    Database d; ActionChain c(d.db); d.transaction([&]{initialize(c,d.db);});
    d.transaction([&]{c.applied(c.token(), action("A"), digest);});
    const auto before = c.token();
    // A failure after root/receipt writes must roll them back with the command.
    sql(d.db, "CREATE TRIGGER fail_head BEFORE UPDATE OF head ON action_state BEGIN SELECT RAISE(ABORT,'injected'); END");
    refuses([&]{d.transaction([&]{c.applied(before, action("failed"), digest);});});
    check(c.token() == before && !c.outcome("failed", digest), "Failed publication rolls back");
    sql(d.db, "DROP TRIGGER fail_head");
    sqlite3_commit_hook(d.db, [](void*) {return 1;}, nullptr);
    refuses([&]{d.transaction([&]{c.reversed(before, "undoRejected", digest);});});
    sqlite3_commit_hook(d.db, nullptr, nullptr);
    check(c.token() == before && !c.outcome("undoRejected", digest), "Rejected commit preserves Undo");
    // SQLite's page limit produces a real SQLITE_FULL after the action writes.
    sql(d.db, "CREATE TABLE fault_payload(bytes BLOB)");
    sqlite3_stmt* q = nullptr;
    check(sqlite3_prepare_v2(d.db, "PRAGMA page_count", -1, &q, nullptr) == SQLITE_OK, "Prepare page count");
    check(sqlite3_step(q) == SQLITE_ROW, "Read page count");
    const auto pages = sqlite3_column_int(q, 0); sqlite3_finalize(q);
    sql(d.db, ("PRAGMA max_page_count=" + std::to_string(pages + 1)).c_str());
    bool observed_full = false;
    refuses([&]{d.transaction([&]{
        c.applied(before, action("diskFull"), digest);
        const auto result = sqlite3_exec(d.db, "INSERT INTO fault_payload VALUES(zeroblob(1048576))", nullptr, nullptr, nullptr);
        observed_full = result == SQLITE_FULL;
        throw std::runtime_error("Disk full");
    });});
    check(observed_full, "Expected SQLite full result");
    check(c.token() == before && !c.outcome("diskFull", digest), "Full disk preserves prior chain");
    c.verify();
    // Close failures cannot expire recoverable history.
    sqlite3_commit_hook(d.db, [](void*) {return 1;}, nullptr);
    refuses([&]{d.transaction([&]{c.expire(before);});});
    sqlite3_commit_hook(d.db, nullptr, nullptr);
    check(c.token() == before && c.head()->id == "A", "Failed close retains action and session");
    c.verify();
}
void recovery_tests() {
    std::string pattern = (std::filesystem::temp_directory_path() / "spentit-action-chain-XXXXXX").string();
    check(mkdtemp(pattern.data()) != nullptr, "Temporary directory failed");
    struct Cleanup { std::string path; ~Cleanup() {std::filesystem::remove_all(path);} } cleanup{pattern};
    const auto path = (std::filesystem::path(pattern) / "synthetic.db").string();
    {
        Database d(path.c_str()); ActionChain c(d.db);
        d.transaction([&]{initialize(c,d.db); c.applied(c.token(), action("A"), digest);});
    }
    for (bool committed : {false, true}) {
        const auto pid = fork(); check(pid >= 0, "Fork failed");
        if (pid == 0) {
            try {
                Database d(path.c_str()); ActionChain c(d.db); sql(d.db, "BEGIN IMMEDIATE");
                c.applied(c.token(), action(committed ? "saved" : "unsaved"), digest);
                if (committed) sql(d.db, "COMMIT");
                _exit(0);
            } catch (...) { _exit(2); }
        }
        int status = 0; check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child failed");
        Database d(path.c_str()); ActionChain c(d.db); c.verify();
        check(c.head()->id == (committed ? "saved" : "A"), "Crash retains exactly the durable root");
        check(!c.outcome("unsaved", digest), "Uncommitted receipt rolled back after exit");
    }
    {
        Database d(path.c_str()); ActionChain c(d.db);
        d.transaction([&]{c.begin_import(c.token(), action("source", ActionKind::import), digest);});
        d.transaction([&]{c.import_stage("source", ImportStage::awaiting_decision);});
    }
    {
        Database d(path.c_str()); ActionChain c(d.db); c.verify();
        check(c.import_stage("source") == ImportStage::awaiting_decision && c.head()->id == "saved", "Import decision stage survives reopen");
        refuses([&]{c.require_idle();});
    }
}
}
int main() {
    try { chain_tests(); import_tests(); failure_tests(); recovery_tests(); std::cout << "Action chain tests passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
