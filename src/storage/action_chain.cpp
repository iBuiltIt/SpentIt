#include "action_chain.hpp"
#include "spentit/import_store.hpp"

#include <sqlite3.h>
#include <limits>
#include <stdexcept>

namespace spentit::storage {
namespace {
[[noreturn]] void reject(const char* message, int code = SQLITE_CONSTRAINT) { throw ImportError(code, message); }
void check(int code) {
    if (code != SQLITE_OK) reject("Cannot read or save Undo history", code);
}
void sql(sqlite3* db, const char* text) { check(sqlite3_exec(db, text, nullptr, nullptr, nullptr)); }
class Query {
public:
    Query(sqlite3* db, const char* text) { check(sqlite3_prepare_v2(db, text, -1, &stmt_, nullptr)); }
    ~Query() { sqlite3_finalize(stmt_); }
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;
    void text(int index, std::string_view value) {
        check(sqlite3_bind_text64(stmt_, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8));
    }
    void number(int index, std::int64_t value) { check(sqlite3_bind_int64(stmt_, index, value)); }
    void blob(int index, std::string_view value) {
        check(sqlite3_bind_blob64(stmt_, index, value.data(), value.size(), SQLITE_TRANSIENT));
    }
    bool row() {
        const auto result = sqlite3_step(stmt_);
        if (result == SQLITE_ROW) return true;
        if (result != SQLITE_DONE) reject("Cannot read or save Undo history", result);
        return false;
    }
    void done() { if (row()) reject("Unexpected Undo history result"); }
    std::string text(int index) const {
        const auto* data = sqlite3_column_text(stmt_, index);
        const auto length = sqlite3_column_bytes(stmt_, index);
        return data ? std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(length)) : std::string{};
    }
    std::int64_t number(int index) const { return sqlite3_column_int64(stmt_, index); }
    std::string blob(int index) const {
        const auto* data = sqlite3_column_blob(stmt_, index);
        return data ? std::string(static_cast<const char*>(data), static_cast<std::size_t>(sqlite3_column_bytes(stmt_, index))) : std::string{};
    }
private:
    sqlite3_stmt* stmt_ = nullptr;
};
app::ActionSummary summary(const Query& q) {
    return {q.text(0), q.text(1), q.number(2), static_cast<app::ActionKind>(q.number(3)),
            static_cast<app::ActionState>(q.number(4)), q.text(5), q.text(6), q.number(7)};
}
void identity(std::string_view id) {
    if (id.empty() || id.size() > 128 || id.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string_view::npos)
        reject("Invalid Undo operation identity");
}
void receipt(sqlite3* db, std::string_view id, std::string_view digest, int status, std::string_view target, int kind) {
    identity(id);
    if (digest.size() != 32) reject("Invalid Undo request digest");
    Query q(db, "INSERT INTO action_receipts VALUES(?1,?2,?3,?4,?5)");
    q.text(1, id); q.blob(2, digest); q.number(3, status); q.text(4, target); q.number(5, kind); q.done();
}
}

void ActionChain::transaction() const {
    if (sqlite3_get_autocommit(db_)) reject("Undo changes require the financial transaction", SQLITE_MISUSE);
}
void ActionChain::initialize() {
    transaction();
    sql(db_, R"SQL(
CREATE TABLE action_state(singleton INTEGER PRIMARY KEY CHECK(singleton=1),head TEXT NOT NULL,preparing TEXT NOT NULL) STRICT;
INSERT INTO action_state VALUES(1,'','');
CREATE TABLE action_headers(sequence INTEGER PRIMARY KEY AUTOINCREMENT CHECK(sequence>0),id TEXT NOT NULL UNIQUE,session INTEGER NOT NULL CHECK(session>=0),parent TEXT NOT NULL,kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 7),state INTEGER NOT NULL CHECK(state BETWEEN 0 AND 3),payload TEXT NOT NULL,label TEXT NOT NULL CHECK(length(label)<=256),count INTEGER NOT NULL CHECK(count>=0),stage INTEGER NOT NULL CHECK(stage BETWEEN 0 AND 2),CHECK(state!=0 OR kind=7)) STRICT;
CREATE TABLE action_receipts(id TEXT PRIMARY KEY,digest BLOB NOT NULL CHECK(length(digest)=32),status INTEGER NOT NULL CHECK(status BETWEEN 0 AND 5),target TEXT NOT NULL,kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 3)) STRICT, WITHOUT ROWID;
CREATE INDEX action_inverse_target ON action_receipts(target) WHERE kind=1;
)SQL");
}
app::ActionToken ActionChain::token() const {
    Query q(db_, "SELECT d.generation,m.revision,a.head FROM document_session d JOIN mutation_generation m USING(singleton) JOIN action_state a USING(singleton) WHERE a.singleton=1");
    if (!q.row()) reject("Undo session is missing");
    return {q.number(0), q.number(1), q.text(2)};
}
std::optional<app::ActionSummary> ActionChain::head() const {
    const auto current = token();
    if (current.head.empty()) return {};
    Query q(db_, "SELECT id,parent,sequence,kind,state,payload,label,count FROM action_headers WHERE id=? AND state=1");
    q.text(1, current.head);
    if (!q.row()) reject("Undo head is inconsistent");
    return summary(q);
}
std::vector<app::ActionSummary> ActionChain::summaries(std::int64_t before, std::size_t limit) const {
    if (before < 0 || limit == 0 || limit > 200) reject("Invalid Undo history page");
    Query q(db_, "SELECT id,parent,sequence,kind,state,payload,label,count FROM action_headers WHERE (?1=0 OR sequence<?1) ORDER BY sequence DESC LIMIT ?2");
    q.number(1, before); q.number(2, static_cast<std::int64_t>(limit));
    std::vector<app::ActionSummary> rows;
    while (q.row()) rows.push_back(summary(q));
    return rows;
}
std::optional<app::ActionSummary> ActionChain::preparing() const {
    Query q(db_, "SELECT id,parent,sequence,kind,state,payload,label,count FROM action_headers WHERE id=(SELECT preparing FROM action_state) AND state=0");
    if (!q.row()) {
        Query state(db_, "SELECT preparing FROM action_state");
        if (!state.row() || !state.text(0).empty()) reject("Import preparation is inconsistent");
        return {};
    }
    return summary(q);
}
void ActionChain::require_current(const app::ActionToken& expected) const {
    if (token() != expected) reject("Changes were saved. Refresh the action and try again.");
}
void ActionChain::require_idle() const {
    if (preparing()) reject("Finish or cancel the import first.");
}
void ActionChain::require_empty() const {
    require_idle();
    if (head()) reject("Undo the current actions, or close and reopen the file first.");
}
std::optional<ActionChain::Outcome> ActionChain::outcome(std::string_view id, std::string_view digest) const {
    identity(id);
    if (digest.size() != 32) reject("Invalid Undo request digest");
    Query q(db_, "SELECT digest,status FROM action_receipts WHERE id=?"); q.text(1, id);
    if (!q.row()) return {};
    if (q.blob(0) != digest) reject("Operation identity was reused with different content");
    return static_cast<Outcome>(q.number(1));
}
std::optional<ActionChain::Outcome> ActionChain::durable_outcome(std::string_view id, std::string_view digest) const {
    if (!sqlite3_get_autocommit(db_)) throw app::OperationOutcomeUnknown();
    return outcome(id, digest);
}
void ActionChain::advance() {
    Query q(db_, "UPDATE mutation_generation SET revision=revision+1 WHERE singleton=1 AND revision<9223372036854775807"); q.done();
    if (sqlite3_changes(db_) != 1) reject("Undo revision capacity exhausted");
}
bool ActionChain::retry(std::string_view id, std::string_view digest, int kind) const {
    if (!outcome(id, digest)) return false;
    Query q(db_, "SELECT kind FROM action_receipts WHERE id=?"); q.text(1, id);
    if (!q.row() || q.number(0) != kind) reject("Operation identity belongs to another command");
    return true;
}
void ActionChain::insert(const app::ActionSummary& action, std::string_view digest, bool pending) {
    identity(action.id); identity(action.payload);
    if (action.label.size() > 256 || action.count < 0 || static_cast<int>(action.kind) < 0 || static_cast<int>(action.kind) > 7)
        reject("Invalid Undo action summary");
    const auto current = token();
    Query q(db_, "INSERT INTO action_headers(id,session,parent,kind,state,payload,label,count,stage) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,0)");
    q.text(1, action.id); q.number(2, current.session); q.text(3, current.head);
    q.number(4, static_cast<int>(action.kind)); q.number(5, pending ? 0 : 1);
    q.text(6, action.payload); q.text(7, action.label); q.number(8, action.count); q.done();
    if (!pending) receipt(db_, action.id, digest, 0, action.id, 0);
    // Preparing requests keep their fingerprint but are not completed outcomes.
    // Its durable stages use the same identity and canonical digest.
    if (pending) receipt(db_, action.id, digest, 5, action.id, 3);
    Query set(db_, pending ? "UPDATE action_state SET preparing=? WHERE singleton=1" : "UPDATE action_state SET head=? WHERE singleton=1");
    set.text(1, action.id); set.done(); advance();
}
void ActionChain::applied(const app::ActionToken& expected, const app::ActionSummary& action, std::string_view digest) {
    transaction();
    if (retry(action.id, digest, 0)) return;
    require_current(expected); require_idle();
    if (action.kind == app::ActionKind::import) reject("Import requires durable preparation");
    insert(action, digest, false);
}
void ActionChain::no_change(const app::ActionToken& expected, std::string_view id, std::string_view digest) {
    transaction(); if (retry(id, digest, 2)) return;
    require_current(expected); require_idle(); receipt(db_, id, digest, 3, "", 2);
}
void ActionChain::compatible_change(const app::ActionToken& expected) {
    transaction(); require_current(expected); require_idle(); advance();
}
void ActionChain::begin_import(const app::ActionToken& expected, const app::ActionSummary& action, std::string_view digest) {
    transaction();
    if (retry(action.id, digest, 3)) return;
    require_current(expected); require_idle();
    if (action.kind != app::ActionKind::import) reject("Import preparation requires an import action");
    insert(action, digest, true);
}
app::ImportStage ActionChain::import_stage(std::string_view id) const {
    const auto pending = preparing();
    if (!pending || pending->id != id) reject("This import is not awaiting completion");
    Query q(db_, "SELECT stage FROM action_headers WHERE id=?"); q.text(1, id);
    if (!q.row()) reject("Import preparation is missing");
    return static_cast<app::ImportStage>(q.number(0));
}
void ActionChain::import_stage(std::string_view id, app::ImportStage stage) {
    transaction();
    const auto old = import_stage(id);
    if (static_cast<int>(stage) < static_cast<int>(old) || static_cast<int>(stage) > 2)
        reject("Import preparation cannot move backwards");
    if (stage == old) return;
    Query q(db_, "UPDATE action_headers SET stage=?2 WHERE id=?1");
    q.text(1, id); q.number(2, static_cast<int>(stage)); q.done(); advance();
}
void ActionChain::finish_import(std::string_view id, bool cancelled) {
    transaction();
    const auto pending = preparing();
    if (!pending || pending->id != id) reject("This import is not awaiting completion");
    if (!cancelled && import_stage(id) == app::ImportStage::captured)
        reject("Materialize the import before completing it");
    if (pending->parent != token().head) reject("Import parent changed during preparation");
    Query q(db_, "UPDATE action_headers SET state=?2 WHERE id=?1");
    q.text(1, id); q.number(2, cancelled ? 3 : 1); q.done();
    Query r(db_, "UPDATE action_receipts SET status=?2 WHERE id=?1");
    r.text(1, id); r.number(2, cancelled ? 2 : 0); r.done();
    Query state(db_, cancelled ? "UPDATE action_state SET preparing='' WHERE singleton=1" : "UPDATE action_state SET preparing='',head=?1 WHERE singleton=1");
    if (!cancelled) state.text(1, id);
    state.done(); advance();
}
void ActionChain::reversed(const app::ActionToken& expected, std::string_view operation, std::string_view digest) {
    transaction(); if (retry(operation, digest, 1)) return;
    require_current(expected); require_idle(); const auto current = head();
    if (!current) reject("There is no action to undo");
    receipt(db_, operation, digest, 0, current->id, 1);
    Query q(db_, "UPDATE action_headers SET state=2 WHERE id=?"); q.text(1, current->id); q.done();
    Query r(db_, "UPDATE action_receipts SET status=1 WHERE id=?"); r.text(1, current->id); r.done();
    Query set(db_, "UPDATE action_state SET head=? WHERE singleton=1"); set.text(1, current->parent); set.done(); advance();
}
void ActionChain::expire(const app::ActionToken& expected) {
    transaction(); require_current(expected); require_idle();
    if (expected.session == std::numeric_limits<std::int64_t>::max()) reject("Undo session capacity exhausted");
    // Domain reachability/pruning occurs in this same caller-owned transaction.
    // Compact receipts retain no target identity, snapshots or financial values.
    advance();
    sql(db_, "UPDATE action_receipts SET status=4,target=''; DELETE FROM action_headers; UPDATE action_state SET head='' WHERE singleton=1; UPDATE document_session SET generation=generation+1 WHERE singleton=1");
}
void ActionChain::verify() const {
    const auto current = token();
    std::int64_t visited = 0;
    auto next = current.head;
    std::optional<std::int64_t> last;
    while (!next.empty()) {
        Query q(db_, "SELECT parent,sequence,session,state FROM action_headers WHERE id=?"); q.text(1, next);
        if (!q.row() || (last && q.number(1) >= *last) || q.number(2) != current.session || q.number(3) != 1)
            reject("Undo history chain is inconsistent");
        last = q.number(1); next = q.text(0); ++visited;
    }
    Query count(db_, "SELECT count(*) FROM action_headers WHERE state=1");
    if (!count.row() || count.number(0) != visited) reject("Undo history has unreachable actions");
    const auto pending = preparing();
    Query preps(db_, "SELECT count(*) FROM action_headers WHERE state=0");
    if (!preps.row() || preps.number(0) != (pending ? 1 : 0) || (pending && pending->parent != current.head))
        reject("Import preparation is inconsistent");
    Query invalid(db_, R"SQL(SELECT 1 FROM action_headers h LEFT JOIN action_receipts r ON r.id=h.id
WHERE h.session!=(SELECT generation FROM document_session) OR r.id IS NULL OR r.target!=h.id
OR r.status!=CASE h.state WHEN 0 THEN 5 WHEN 1 THEN 0 WHEN 2 THEN 1 ELSE 2 END
OR r.kind!=CASE h.kind WHEN 7 THEN 3 ELSE 0 END
OR (SELECT count(*) FROM action_receipts u WHERE u.kind=1 AND u.target=h.id)!=CASE h.state WHEN 2 THEN 1 ELSE 0 END
OR (h.parent!='' AND NOT EXISTS(SELECT 1 FROM action_headers p WHERE p.id=h.parent AND p.sequence<h.sequence)) LIMIT 1)SQL");
    if (invalid.row()) reject("Undo action receipt is inconsistent");
    Query receipts(db_, R"SQL(SELECT 1 FROM action_receipts r WHERE
 (r.status=4 AND r.target!='') OR
 (r.status!=4 AND r.kind=2 AND (r.target!='' OR r.status!=3)) OR
 (r.status!=4 AND r.kind!=2 AND NOT EXISTS(SELECT 1 FROM action_headers h WHERE h.id=r.target)) OR
 (r.status!=4 AND r.kind=1 AND (r.status!=0 OR NOT EXISTS(SELECT 1 FROM action_headers h WHERE h.id=r.target AND h.state=2)))
 LIMIT 1)SQL");
    if (receipts.row()) reject("Undo completion receipt is inconsistent");
}
} // namespace spentit::storage
