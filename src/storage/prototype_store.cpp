#include <spentit/prototype_store.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <climits>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#include "windows_files.hpp"
#endif
#include <limits>

namespace spentit::storage {
namespace {
[[noreturn]] void fail(int code, const char* message = "Encrypted storage operation failed") {
    throw StorageError(code, message); // Never expose SQL, keys, paths or record values.
}
void ok(int rc) { if (rc != SQLITE_OK) fail(rc); }
void exec(sqlite3* db, const char* sql) { ok(sqlite3_exec(db, sql, nullptr, nullptr, nullptr)); }
class Statement {
public:
    Statement(sqlite3* db, const char* sql) { ok(sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr)); }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void text(int i, std::string_view value) {
        if (value.size() > 1024 * 1024) fail(SQLITE_TOOBIG);
        ok(sqlite3_bind_text(stmt_, i, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    void blob(int i, std::string_view value) {
        if (value.size() > 1024 * 1024) fail(SQLITE_TOOBIG);
        ok(sqlite3_bind_blob(stmt_, i, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    void integer(int i, std::int64_t value) { ok(sqlite3_bind_int64(stmt_, i, value)); }
    bool row() {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        fail(rc);
    }
    void done() { if (row()) fail(SQLITE_MISUSE); }
    std::int64_t integer(int i) const {
        if (sqlite3_column_type(stmt_, i) != SQLITE_INTEGER) fail(SQLITE_CORRUPT);
        return sqlite3_column_int64(stmt_, i);
    }
    std::string bytes(int i) const {
        const auto* ptr = static_cast<const char*>(sqlite3_column_blob(stmt_, i));
        const int size = sqlite3_column_bytes(stmt_, i);
        if (size == 0) return {};
        if (!ptr) fail(SQLITE_NOMEM);
        return {ptr, static_cast<std::size_t>(size)};
    }
private:
    sqlite3_stmt* stmt_ = nullptr;
};
std::string scalar(sqlite3* db, const char* sql) {
    Statement query(db, sql);
    if (!query.row()) fail(SQLITE_ERROR);
    const auto value = query.bytes(0);
    if (query.row()) fail(SQLITE_ERROR);
    return value;
}
class Atomic {
public:
    explicit Atomic(sqlite3* db) : db_(db) { exec(db_, "BEGIN IMMEDIATE"); }
    ~Atomic() { if (active_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
    void commit() { exec(db_, "COMMIT"); active_ = false; }
private:
    sqlite3* db_;
    bool active_ = true;
};
void identifier(std::string_view id) {
    if (id.empty() || id.size() > 128 || id.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string_view::npos)
        fail(SQLITE_MISUSE, "Invalid prototype identifier");
}
void exclusive_file(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto created = win::exclusive_file(path);
    if (!created) fail(created.code, created.message);
#else
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) fail(SQLITE_CANTOPEN, "Cannot create new storage file exclusively");
    if (::close(fd) != 0) fail(SQLITE_IOERR_CLOSE);
#endif
}
void sync_directory(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto synced = win::sync_parent(path);
    if (!synced) fail(synced.code, synced.message);
#else
    auto parent = path.parent_path();
    if (parent.empty()) parent = ".";
    const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) fail(SQLITE_IOERR_FSYNC);
    const int rc = ::fsync(fd);
    const int closed = ::close(fd);
    if (rc || closed) fail(SQLITE_IOERR_FSYNC);
#endif
}
constexpr const char* schema = R"SQL(
PRAGMA application_id=1397770576;
PRAGMA user_version=1;
CREATE TABLE sources(id TEXT PRIMARY KEY, evidence BLOB NOT NULL) STRICT;
CREATE TABLE drafts(id TEXT PRIMARY KEY REFERENCES sources(id), definition TEXT NOT NULL,
 code TEXT NOT NULL CHECK(length(code)=3), scale INTEGER NOT NULL CHECK(scale BETWEEN 0 AND 9),
 original INTEGER NOT NULL, total INTEGER NOT NULL, state TEXT NOT NULL CHECK(state IN ('draft','committed'))) STRICT;
CREATE TABLE allocations(draft TEXT NOT NULL REFERENCES drafts(id), ordinal INTEGER NOT NULL CHECK(ordinal>=0),
 category TEXT NOT NULL, memo TEXT NOT NULL, units INTEGER NOT NULL, PRIMARY KEY(draft,ordinal)) STRICT;
CREATE TABLE history(operation TEXT PRIMARY KEY) STRICT;
CREATE TABLE postings(draft TEXT PRIMARY KEY REFERENCES drafts(id), operation TEXT NOT NULL REFERENCES history(operation)) STRICT;
CREATE TRIGGER source_no_update BEFORE UPDATE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_no_delete BEFORE DELETE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_no_update BEFORE UPDATE ON history BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_no_delete BEFORE DELETE ON history BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER posting_no_update BEFORE UPDATE ON postings BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER posting_no_delete BEFORE DELETE ON postings BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_no_delete BEFORE DELETE ON drafts BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_snapshot BEFORE UPDATE ON drafts WHEN
 OLD.state!='draft' OR NEW.state!='committed' OR NEW.id!=OLD.id OR NEW.definition!=OLD.definition OR
 NEW.code!=OLD.code OR NEW.scale!=OLD.scale OR NEW.original!=OLD.original OR NEW.total!=OLD.total OR
 NOT EXISTS(SELECT 1 FROM postings WHERE draft=OLD.id)
 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_no_update BEFORE UPDATE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_no_delete BEFORE DELETE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_no_late_insert BEFORE INSERT ON allocations
 WHEN (SELECT state FROM drafts WHERE id=NEW.draft)!='draft'
 BEGIN SELECT RAISE(ABORT,'immutable'); END;
)SQL";
}
struct PrototypeStore::Impl {
    sqlite3* db = nullptr;
    ~Impl() { if (db) sqlite3_close_v2(db); }
};
PrototypeStore::PrototypeStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PrototypeStore::~PrototypeStore() = default;
PrototypeStore::PrototypeStore(PrototypeStore&&) noexcept = default;
PrototypeStore& PrototypeStore::operator=(PrototypeStore&&) noexcept = default;
PrototypeStore PrototypeStore::create(const std::filesystem::path& path, std::string_view password) {
    return connect(path, password, true);
}
PrototypeStore PrototypeStore::open(const std::filesystem::path& path, std::string_view password) {
    return connect(path, password, false);
}
PrototypeStore PrototypeStore::connect(const std::filesystem::path& path, std::string_view password, bool create) {
    if (password.empty() || password.size() > 1024) fail(SQLITE_MISUSE, "Password length is invalid");
    if (create) exclusive_file(path);
    auto impl = std::make_unique<Impl>();
#ifdef _WIN32
    const auto filename = win::sqlite_filename(path);
    ok(sqlite3_open_v2(filename.c_str(), &impl->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, nullptr));
#else
    ok(sqlite3_open_v2(path.c_str(), &impl->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, nullptr));
#endif
    sqlite3* db = impl->db;
    sqlite3_limit(db, SQLITE_LIMIT_LENGTH, 2 * 1024 * 1024);
    sqlite3_limit(db, SQLITE_LIMIT_SQL_LENGTH, 64 * 1024);
    sqlite3_limit(db, SQLITE_LIMIT_ATTACHED, 0);
    ok(sqlite3_extended_result_codes(db, 1));
    ok(sqlite3_busy_timeout(db, 1000));
    if (scalar(db, "PRAGMA cipher_version") != "4.19.0 community") fail(SQLITE_ERROR, "Unexpected SQLCipher version");
    ok(sqlite3_key(db, password.data(), static_cast<int>(password.size())));
    exec(db, "PRAGMA cipher_compatibility=4; PRAGMA cipher_memory_security=ON");
    (void)scalar(db, "SELECT count(*) FROM sqlite_master"); // Authenticate BEFORE mutations.
    if (!create && (scalar(db, "PRAGMA application_id") != "1397770576" || scalar(db, "PRAGMA user_version") != "1"))
        fail(SQLITE_SCHEMA, "Unsupported document schema; no migration was attempted");
    exec(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=EXTRA; PRAGMA fullfsync=ON;"
             "PRAGMA foreign_keys=ON; PRAGMA temp_store=MEMORY; PRAGMA secure_delete=ON");
    for (const auto& setting : {std::pair{"PRAGMA journal_mode", "delete"},
         {"PRAGMA synchronous", "3"}, {"PRAGMA fullfsync", "1"}, {"PRAGMA foreign_keys", "1"},
         {"PRAGMA temp_store", "2"}, {"PRAGMA secure_delete", "1"}, {"PRAGMA cipher_page_size", "4096"},
         {"PRAGMA kdf_iter", "256000"}, {"PRAGMA cipher_use_hmac", "1"},
         {"PRAGMA cipher_hmac_algorithm", "HMAC_SHA512"},
         {"PRAGMA cipher_kdf_algorithm", "PBKDF2_HMAC_SHA512"},
         {"PRAGMA cipher_memory_security", "1"},
         {"PRAGMA cipher_plaintext_header_size", "0"}})
        if (scalar(db, setting.first) != setting.second) fail(SQLITE_ERROR, "Required storage setting was not applied");
    if (!sqlite3_compileoption_used("TEMP_STORE=3")) fail(SQLITE_ERROR, "Memory-only temporary storage is required");
    PrototypeStore store(std::move(impl));
    if (create) {
        Atomic transaction(db);
        exec(db, schema);
        transaction.commit();
        sync_directory(path);
    }
    store.verify();
    return store;
}
void PrototypeStore::stage(std::string_view id, std::string_view evidence, const Transaction& value) {
    identifier(id);
    if (!value.original_imported_amount()) fail(SQLITE_MISUSE, "Prototype requires imported evidence");
    if (value.allocations().size() > 10000) fail(SQLITE_TOOBIG);
    std::size_t bytes = evidence.size();
    if (bytes > 1024 * 1024) fail(SQLITE_TOOBIG);
    for (const auto& allocation : value.allocations()) {
        if (allocation.category.size() > 1024 * 1024 || allocation.memo.size() > 1024 * 1024) fail(SQLITE_TOOBIG);
        bytes += allocation.category.size() + allocation.memo.size();
        if (bytes > 8 * 1024 * 1024) fail(SQLITE_TOOBIG);
    }
    Atomic transaction(impl_->db);
    Statement source(impl_->db, "INSERT INTO sources VALUES(?,?)");
    source.text(1, id); source.blob(2, evidence); source.done();
    Statement draft(impl_->db, "INSERT INTO drafts VALUES(?,?,?,?,?,?,'draft')");
    const auto& currency = value.effective_total().currency();
    draft.text(1, id); draft.text(2, currency.definition_id()); draft.text(3, currency.code());
    draft.integer(4, currency.scale()); draft.integer(5, value.original_imported_amount()->units());
    draft.integer(6, value.effective_total().units()); draft.done();
    std::int64_t ordinal = 0;
    for (const auto& allocation : value.allocations()) {
        Statement row(impl_->db, "INSERT INTO allocations VALUES(?,?,?,?,?)");
        row.text(1, id); row.integer(2, ordinal++); row.text(3, allocation.category);
        row.text(4, allocation.memo); row.integer(5, allocation.amount.units()); row.done();
    }
    transaction.commit();
}
Draft PrototypeStore::load(std::string_view id) const {
    Statement row(impl_->db, "SELECT evidence,definition,code,scale,original,total,state FROM drafts JOIN sources USING(id) WHERE id=?");
    row.text(1, id);
    if (!row.row()) fail(SQLITE_NOTFOUND, "Draft does not exist");
    const auto currency = Currency::from_definition(row.bytes(1));
    if (currency.code() != row.bytes(2) || currency.scale() != row.integer(3)) fail(SQLITE_CORRUPT);
    const Money original(row.integer(4), currency), total(row.integer(5), currency);
    const auto evidence = row.bytes(0), state = row.bytes(6);
    if (state != "draft" && state != "committed") fail(SQLITE_CORRUPT);
    Statement rows(impl_->db, "SELECT ordinal,category,memo,units FROM allocations WHERE draft=? ORDER BY ordinal");
    rows.text(1, id);
    std::vector<Allocation> allocations;
    std::size_t bytes = evidence.size();
    while (rows.row()) {
        if (allocations.size() >= 10000 || rows.integer(0) != static_cast<std::int64_t>(allocations.size())) fail(SQLITE_CORRUPT);
        allocations.push_back({rows.bytes(1), rows.bytes(2), Money(rows.integer(3), currency)});
        bytes += allocations.back().category.size() + allocations.back().memo.size();
        if (bytes > 8 * 1024 * 1024) fail(SQLITE_TOOBIG);
    }
    // Original split interpretation may have been corrected. Preserve its amount
    // without pretending the final allocations also sum to the imported total.
    auto value = Transaction::imported(original, {{"", "", original}}).corrected(total, std::move(allocations));
    return {std::string(id), evidence, std::move(value), state == "committed"};
}
void PrototypeStore::commit(std::vector<std::string> ids, std::string_view operation, const CommitObserver& observer) {
    identifier(operation);
    if (ids.empty() || ids.size() > 10000) fail(SQLITE_MISUSE);
    for (const auto& id : ids) identifier(id);
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) fail(SQLITE_MISUSE);
    Atomic transaction(impl_->db);
    Statement prior(impl_->db, "SELECT draft FROM postings WHERE operation=? ORDER BY draft");
    prior.text(1, operation);
    std::vector<std::string> previous;
    while (prior.row()) previous.push_back(prior.bytes(0));
    if (!previous.empty()) {
        if (previous != ids) fail(SQLITE_CONSTRAINT, "Operation identity was reused for different drafts");
        for (const auto& id : ids) if (!load(id).committed) fail(SQLITE_CORRUPT);
        transaction.commit();
        return;
    }
    for (const auto& id : ids) if (load(id).committed) fail(SQLITE_CONSTRAINT, "Draft is already committed");
    Statement history(impl_->db, "INSERT INTO history VALUES(?)");
    history.text(1, operation); history.done();
    for (const auto& id : ids) {
        Statement posting(impl_->db, "INSERT INTO postings VALUES(?,?)");
        posting.text(1, id); posting.text(2, operation); posting.done();
        Statement state(impl_->db, "UPDATE drafts SET state='committed' WHERE id=? AND state='draft'");
        state.text(1, id); state.done();
        if (sqlite3_changes(impl_->db) != 1) fail(SQLITE_CONSTRAINT);
    }
    if (observer) observer(CommitPhase::before_durable_commit);
    transaction.commit();
    if (observer) observer(CommitPhase::after_durable_commit);
}
std::vector<Money> PrototypeStore::committed_amounts() const {
    Statement rows(impl_->db, "SELECT draft FROM postings ORDER BY draft");
    std::vector<Money> result;
    while (rows.row()) result.push_back(load(rows.bytes(0)).transaction.effective_total());
    return result;
}
std::size_t PrototypeStore::history_count() const {
    Statement row(impl_->db, "SELECT count(*) FROM history");
    if (!row.row()) fail(SQLITE_CORRUPT);
    return static_cast<std::size_t>(row.integer(0));
}
void PrototypeStore::verify() const {
    if (scalar(impl_->db, "PRAGMA integrity_check") != "ok") fail(SQLITE_CORRUPT);
    for (const auto query : {"PRAGMA cipher_integrity_check", "PRAGMA foreign_key_check"}) {
        Statement check(impl_->db, query);
        if (check.row()) fail(SQLITE_CORRUPT);
    }
    Statement relations(impl_->db, "SELECT 1 FROM drafts d LEFT JOIN postings p ON d.id=p.draft "
        "WHERE (d.state='committed') != (p.draft IS NOT NULL) UNION ALL "
        "SELECT 1 FROM history h WHERE NOT EXISTS(SELECT 1 FROM postings p WHERE p.operation=h.operation)");
    if (relations.row()) fail(SQLITE_CORRUPT);
    Statement drafts(impl_->db, "SELECT id FROM drafts");
    while (drafts.row()) (void)load(drafts.bytes(0));
}
void PrototypeStore::backup_to(const std::filesystem::path& destination, std::string_view password) const {
    auto candidate = destination;
    candidate += ".partial";
    {
        auto target = create(candidate, password);
        sqlite3_backup* backup = sqlite3_backup_init(target.impl_->db, "main", impl_->db, "main");
        if (!backup) fail(sqlite3_errcode(target.impl_->db));
        const int step = sqlite3_backup_step(backup, -1);
        const int finish = sqlite3_backup_finish(backup);
        if (step != SQLITE_DONE) fail(step);
        ok(finish);
        target.verify();
    }
    { auto restored = open(candidate, password); restored.verify(); }
    // link is an atomic no-replace publication on this POSIX prototype.
#ifdef _WIN32
    {
        const auto published = win::publish_link(candidate, destination, "Backup publication failed; candidate retained");
        if (!published) fail(published.code, published.message);
    }
#else
    if (::link(candidate.c_str(), destination.c_str()) != 0) fail(SQLITE_CANTOPEN, "Backup publication failed; candidate retained");
#endif
    sync_directory(destination);
#ifdef _WIN32
    {
        const auto removed = win::remove_file(candidate, "Backup published; candidate cleanup failed");
        if (!removed) fail(removed.code, removed.message);
    }
#else
    if (::unlink(candidate.c_str()) != 0) fail(SQLITE_IOERR_DELETE, "Backup published; candidate cleanup failed");
#endif
    sync_directory(destination);
}
} // namespace spentit::storage
