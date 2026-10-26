#include <spentit/import_store.hpp>
#include <sys/file.h>
#include <sys/stat.h>
#include <set>
#include <map>
#include <sqlite3.h>
#include <algorithm>
#include <climits>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

namespace spentit::storage {
namespace {
[[noreturn]] void fail(int code, const char* message = "Encrypted storage operation failed") {
    throw ImportError(code, message); // Never expose SQL, keys, paths or record values.
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
        if (value.size() > static_cast<std::size_t>(qif::max_file)) fail(SQLITE_TOOBIG);
        ok(sqlite3_bind_text(stmt_, i, value.empty() ? "" : value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    void blob(int i, std::string_view value) {
        if (value.size() > static_cast<std::size_t>(qif::max_file)) fail(SQLITE_TOOBIG);
        ok(sqlite3_bind_blob(stmt_, i, value.empty() ? "" : value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    void integer(int i, std::int64_t value) { ok(sqlite3_bind_int64(stmt_, i, value)); }
    bool row() {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        fail(rc);
    }
    void done() { if (row()) fail(SQLITE_MISUSE); }
    bool null(int i) const { return sqlite3_column_type(stmt_,i)==SQLITE_NULL; }
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
        fail(SQLITE_MISUSE, "Invalid operation identifier");
}
void exclusive_file(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) fail(SQLITE_CANTOPEN, "Cannot create new storage file exclusively");
    if (::close(fd) != 0) fail(SQLITE_IOERR_CLOSE);
}
void sync_directory(const std::filesystem::path& path) {
    auto parent = path.parent_path();
    if (parent.empty()) parent = ".";
    const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) fail(SQLITE_IOERR_FSYNC);
    const int rc = ::fsync(fd);
    const int closed = ::close(fd);
    if (rc || closed) fail(SQLITE_IOERR_FSYNC);
}
void safe_text(std::string_view s) {
    if (s.size()>qif::max_field || !qif::valid_utf8(s)) fail(SQLITE_MISUSE,"Invalid text field");
    for (const char ch:s) { const auto c=static_cast<unsigned char>(ch); if ((c<32 && c!='\t') || c==127) fail(SQLITE_MISUSE,"Invalid text field"); }
}
void iso_date(std::string_view s) {
    if (s.size()!=10 || s[4]!='-' || s[7]!='-') fail(SQLITE_MISUSE,"Invalid financial date");
    if (qif::date(std::string(s.substr(8,2))+"/"+std::string(s.substr(5,2))+"/"+std::string(s.substr(0,4)),qif::DateOrder::dmy)!=s)
        fail(SQLITE_MISUSE,"Invalid financial date");
}
std::int64_t integer(std::size_t v) {
    if (v>static_cast<std::size_t>(INT64_MAX)) fail(SQLITE_TOOBIG);
    return static_cast<std::int64_t>(v);
}
constexpr const char* schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=5;
CREATE TABLE account(singleton INTEGER PRIMARY KEY CHECK(singleton=1),name TEXT NOT NULL,definition TEXT NOT NULL,
 opening_date TEXT NOT NULL,opening INTEGER NOT NULL,frozen INTEGER NOT NULL CHECK(frozen IN(0,1)),revision INTEGER NOT NULL CHECK(revision>=0),confirmed INTEGER NOT NULL CHECK(confirmed IN(0,1))) STRICT;
CREATE TABLE sources(id TEXT PRIMARY KEY,bytes BLOB NOT NULL) STRICT;
CREATE TABLE batches(id TEXT PRIMARY KEY REFERENCES sources(id),profile INTEGER NOT NULL CHECK(profile IN(0,1)),parser TEXT NOT NULL,
 state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),revision INTEGER NOT NULL CHECK(revision>=0),
 count INTEGER NOT NULL CHECK(count BETWEEN 0 AND 200000),blocked INTEGER NOT NULL CHECK(blocked IN(0,1)),materialized_revision INTEGER,
 CHECK((materialized_revision IS NULL AND state IN(2,3) AND blocked=1) OR
 (materialized_revision IS NOT NULL AND materialized_revision BETWEEN 0 AND revision AND state IN(0,1,2)))) STRICT;
CREATE UNIQUE INDEX single_pending ON batches((1)) WHERE state IN(0,3);
CREATE TABLE records(batch TEXT NOT NULL REFERENCES batches(id),ordinal INTEGER NOT NULL,sequence INTEGER NOT NULL UNIQUE CHECK(sequence>0),
 raw BLOB NOT NULL,source_date TEXT NOT NULL,original INTEGER NOT NULL,payee TEXT NOT NULL,reference TEXT NOT NULL,
 PRIMARY KEY(batch,ordinal)) STRICT;
CREATE INDEX candidate_key ON records(source_date,original,payee,reference);
CREATE TABLE versions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),
 date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,total INTEGER NOT NULL,
 decision INTEGER NOT NULL CHECK(decision IN(0,1,2)),duplicate_batch TEXT NOT NULL,duplicate_ordinal INTEGER NOT NULL,
 PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal),
 CHECK((decision=2 AND length(duplicate_batch)>0 AND duplicate_ordinal>=0) OR (decision!=2 AND duplicate_batch='' AND duplicate_ordinal=-1))) STRICT;
CREATE TABLE allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL CHECK(position>=0),
 category TEXT NOT NULL,memo TEXT NOT NULL,units INTEGER NOT NULL,
 PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision) REFERENCES versions(batch,ordinal,revision)) STRICT;
CREATE TABLE operations(id TEXT PRIMARY KEY,batch TEXT NOT NULL UNIQUE REFERENCES batches(id),revision INTEGER NOT NULL,
 included INTEGER NOT NULL,excluded INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL) STRICT;
CREATE TABLE events(sequence INTEGER PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN('capture','materialize','ingest','save','decide','discard','commit','opening')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.count!=OLD.count OR OLD.state NOT IN(0,3) OR NEW.revision!=OLD.revision+1 OR
 (NEW.materialized_revision IS NOT OLD.materialized_revision AND NOT
 (OLD.state=3 AND NEW.state=0 AND OLD.materialized_revision IS NULL AND NEW.materialized_revision=NEW.revision))
 BEGIN SELECT RAISE(ABORT,'invalid batch transition'); END;
CREATE TRIGGER evidence_keep BEFORE DELETE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER evidence_immutable BEFORE UPDATE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_keep BEFORE DELETE ON batches BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_immutable BEFORE UPDATE ON records BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_keep BEFORE DELETE ON records BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_immutable BEFORE UPDATE ON versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_keep BEFORE DELETE ON versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_immutable BEFORE UPDATE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_keep BEFORE DELETE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_immutable BEFORE UPDATE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_keep BEFORE DELETE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_immutable BEFORE UPDATE ON events BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_keep BEFORE DELETE ON events BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER frozen_opening BEFORE UPDATE ON account WHEN OLD.frozen=1 AND
 (NEW.opening!=OLD.opening OR NEW.opening_date!=OLD.opening_date OR NEW.definition!=OLD.definition OR NEW.frozen!=1)
 BEGIN SELECT RAISE(ABORT,'frozen opening'); END;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER record_pending BEFORE INSERT ON records WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TABLE entry_revisions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),active INTEGER NOT NULL CHECK(active IN(0,1)),date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,total INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TABLE entry_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category TEXT NOT NULL,memo TEXT NOT NULL,units INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_heads(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE ledger_operations(sequence INTEGER PRIMARY KEY,id TEXT NOT NULL UNIQUE,kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 3),batch TEXT NOT NULL,ordinal INTEGER NOT NULL,prior_revision INTEGER NOT NULL,result_revision INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL,undo_of TEXT NOT NULL,preview_sequence INTEGER NOT NULL,catalog_revision INTEGER NOT NULL DEFAULT 0) STRICT;
CREATE TABLE change_dependencies(operation TEXT NOT NULL REFERENCES ledger_operations(id),batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,state INTEGER NOT NULL,PRIMARY KEY(operation,batch)) STRICT;
CREATE TABLE history_order(sequence INTEGER PRIMARY KEY,batch TEXT,event_revision INTEGER,operation TEXT,category_operation TEXT,UNIQUE(batch,event_revision),UNIQUE(operation),UNIQUE(category_operation),FOREIGN KEY(batch,event_revision) REFERENCES events(batch,revision),FOREIGN KEY(operation) REFERENCES ledger_operations(id),FOREIGN KEY(category_operation) REFERENCES category_operations(id),CHECK((batch IS NOT NULL AND event_revision IS NOT NULL AND operation IS NULL AND category_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NOT NULL AND category_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NOT NULL))) STRICT;
CREATE TABLE review_stamps(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,target_revision INTEGER NOT NULL,target_active INTEGER NOT NULL CHECK(target_active IN(0,1)),review_revision INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES versions(batch,ordinal,revision),FOREIGN KEY(batch,review_revision) REFERENCES events(batch,revision) DEFERRABLE INITIALLY DEFERRED) STRICT;
CREATE INDEX retained_references ON versions(duplicate_batch,duplicate_ordinal,batch,revision);
CREATE TRIGGER history_order_capacity BEFORE INSERT ON history_order WHEN coalesce((SELECT max(sequence) FROM history_order),0)=9223372036854775807 BEGIN SELECT RAISE(ABORT,'History order capacity exhausted'); END;
CREATE TRIGGER financial_order_capacity BEFORE INSERT ON ledger_operations WHEN coalesce((SELECT max(sequence) FROM ledger_operations),0)=9223372036854775807 BEGIN SELECT RAISE(ABORT,'Financial order capacity exhausted'); END;
CREATE TRIGGER order_event AFTER INSERT ON events BEGIN INSERT INTO history_order(batch,event_revision) VALUES(NEW.batch,NEW.revision); END;
CREATE TRIGGER order_operation AFTER INSERT ON ledger_operations BEGIN INSERT INTO history_order(operation) VALUES(NEW.id); END;
CREATE TRIGGER head_step BEFORE UPDATE ON entry_heads WHEN NEW.batch!=OLD.batch OR NEW.ordinal!=OLD.ordinal OR NEW.revision!=OLD.revision+1 BEGIN SELECT RAISE(ABORT,'invalid head'); END;
CREATE TRIGGER entry_revisions_immutable BEFORE UPDATE ON entry_revisions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_revisions_keep BEFORE DELETE ON entry_revisions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocations_immutable BEFORE UPDATE ON entry_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocations_keep BEFORE DELETE ON entry_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER ledger_operations_immutable BEFORE UPDATE ON ledger_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER ledger_operations_keep BEFORE DELETE ON ledger_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_immutable BEFORE UPDATE ON change_dependencies BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_keep BEFORE DELETE ON change_dependencies BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_immutable BEFORE UPDATE ON history_order BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_keep BEFORE DELETE ON history_order BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_immutable BEFORE UPDATE ON review_stamps BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_keep BEFORE DELETE ON review_stamps BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_heads_keep BEFORE DELETE ON entry_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE category_state(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision INTEGER NOT NULL CHECK(revision>=0)) STRICT;
INSERT INTO category_state VALUES(1,0);
CREATE TABLE category_versions(id TEXT NOT NULL,revision INTEGER NOT NULL,catalog_revision INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL CHECK(kind IN(0,1)),archived INTEGER NOT NULL CHECK(archived IN(0,1)),deleted INTEGER NOT NULL CHECK(deleted IN(0,1)),PRIMARY KEY(id,revision)) STRICT;
CREATE TABLE category_heads(id TEXT PRIMARY KEY,revision INTEGER NOT NULL,FOREIGN KEY(id,revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE category_operations(id TEXT PRIMARY KEY,action INTEGER NOT NULL,category TEXT NOT NULL,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL,catalog_before INTEGER NOT NULL,catalog_after INTEGER NOT NULL UNIQUE,before_revision INTEGER,after_revision INTEGER NOT NULL,FOREIGN KEY(category,after_revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL CHECK(domain IN(0,1))) STRICT;
CREATE TABLE draft_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES versions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE draft_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES allocations(batch,ordinal,revision,position),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
CREATE TABLE entry_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES entry_allocations(batch,ordinal,revision,position),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
CREATE TRIGGER register_financial BEFORE INSERT ON ledger_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,0); END;
CREATE TRIGGER register_category BEFORE INSERT ON category_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,1); END;
CREATE TRIGGER order_category AFTER INSERT ON category_operations BEGIN INSERT INTO history_order(category_operation) VALUES(NEW.id); END;
CREATE TRIGGER category_head_step BEFORE UPDATE ON category_heads WHEN NEW.id!=OLD.id OR NEW.revision!=OLD.revision+1 BEGIN SELECT RAISE(ABORT,'invalid category head'); END;
CREATE TRIGGER category_heads_keep BEFORE DELETE ON category_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER category_versions_immutable BEFORE UPDATE ON category_versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER category_versions_keep BEFORE DELETE ON category_versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER category_operations_immutable BEFORE UPDATE ON category_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER category_operations_keep BEFORE DELETE ON category_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_registry_immutable BEFORE UPDATE ON operation_registry BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_registry_keep BEFORE DELETE ON operation_registry BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_immutable BEFORE UPDATE ON draft_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_keep BEFORE DELETE ON draft_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_immutable BEFORE UPDATE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_keep BEFORE DELETE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_immutable BEFORE UPDATE ON draft_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_keep BEFORE DELETE ON draft_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_immutable BEFORE UPDATE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_keep BEFORE DELETE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
)SQL";
app::BatchInfo batch(sqlite3* db,std::string_view id) {
    Statement s(db,(scalar(db,"PRAGMA user_version")=="3" || scalar(db,"PRAGMA user_version")=="4" || scalar(db,"PRAGMA user_version")=="5")?"SELECT id,profile,state,revision,count,blocked,materialized_revision IS NULL FROM batches WHERE id=?":"SELECT id,profile,state,revision,count,blocked,0 FROM batches WHERE id=?"); s.text(1,id);
    if (!s.row()) fail(SQLITE_NOTFOUND,"Import does not exist");
    return {s.bytes(0),static_cast<qif::DateOrder>(s.integer(1)),static_cast<app::BatchState>(s.integer(2)),s.integer(3),
        static_cast<std::size_t>(s.integer(4)),s.integer(5)!=0,s.integer(6)!=0};
}
void expect(sqlite3* db,std::string_view id,std::int64_t revision) {
    const auto b=batch(db,id);
    if (b.state!=app::BatchState::pending || b.revision!=revision) fail(SQLITE_CONSTRAINT,"Import changed; refresh before retrying");
}
void advance(sqlite3* db,std::string_view id,std::int64_t revision,const char* kind,int state=0,int blocked=-1) {
    const auto current=batch(db,id);
    if(current.revision!=revision || (current.state!=app::BatchState::pending &&
       !(current.state==app::BatchState::captured && (std::string_view(kind)=="opening" || std::string_view(kind)=="discard"))))
        fail(SQLITE_CONSTRAINT,"Import changed; refresh before retrying");
    if (revision==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    Statement update(db,"UPDATE batches SET revision=revision+1,state=?,blocked=CASE WHEN ?<0 THEN blocked ELSE ? END WHERE id=?"); update.integer(1,state); update.integer(2,blocked); update.integer(3,blocked); update.text(4,id); update.done();
    Statement event(db,"INSERT INTO events(batch,revision,kind) VALUES(?,?,?)"); event.text(1,id); event.integer(2,revision+1); event.text(3,kind); event.done();
}
void save_metadata(sqlite3*,const imports::Occurrence&,std::int64_t,const imports::DraftRow&,bool);
void version(sqlite3* db,const imports::Occurrence& id,std::int64_t rev,const imports::DraftRow& row,const imports::Decision& decision,bool fresh_review=true) {
    Statement s(db,"INSERT INTO versions VALUES(?,?,?,?,?,?,?,?,?,?)");
    s.text(1,id.batch); s.integer(2,integer(id.ordinal)); s.integer(3,rev); s.text(4,row.date); s.text(5,row.payee); s.text(6,row.memo);
    s.integer(7,row.transaction.effective_total().units()); s.integer(8,static_cast<int>(decision.disposition));
    s.text(9,decision.duplicate_of ? decision.duplicate_of->batch : "");
    s.integer(10,decision.duplicate_of ? integer(decision.duplicate_of->ordinal) : -1); s.done();
    if((scalar(db,"PRAGMA user_version")=="4" || scalar(db,"PRAGMA user_version")=="5") && decision.duplicate_of) {
        std::int64_t target_revision=0,target_active=0,review_revision=rev;
        if(fresh_review) {
            Statement target(db,"SELECT h.revision,e.active FROM entry_heads h JOIN entry_revisions e USING(batch,ordinal,revision) WHERE h.batch=? AND h.ordinal=?");
            target.text(1,decision.duplicate_of->batch);target.integer(2,integer(decision.duplicate_of->ordinal));if(!target.row())fail(SQLITE_CONSTRAINT);
            target_revision=target.integer(0);target_active=target.integer(1);
        } else {
            Statement prior(db,"SELECT target_revision,target_active,review_revision FROM review_stamps WHERE batch=? AND ordinal=? AND revision<? ORDER BY revision DESC LIMIT 1");
            prior.text(1,id.batch);prior.integer(2,integer(id.ordinal));prior.integer(3,rev);if(!prior.row())fail(SQLITE_CORRUPT);
            target_revision=prior.integer(0);target_active=prior.integer(1);review_revision=prior.integer(2);
        }
        Statement stamp(db,"INSERT INTO review_stamps VALUES(?,?,?,?,?,?)");stamp.text(1,id.batch);stamp.integer(2,integer(id.ordinal));stamp.integer(3,rev);stamp.integer(4,target_revision);stamp.integer(5,target_active);stamp.integer(6,review_revision);stamp.done();
    }
    std::int64_t pos=0;
    for (const auto& a:row.transaction.allocations()) {
        Statement x(db,"INSERT INTO allocations VALUES(?,?,?,?,?,?,?)"); x.text(1,id.batch); x.integer(2,integer(id.ordinal)); x.integer(3,rev);
        x.integer(4,pos++); x.text(5,a.category); x.text(6,a.memo); x.integer(7,a.amount.units()); x.done();
    }    save_metadata(db,id,rev,row,false);
}

void save_metadata(sqlite3*,const imports::Occurrence&,std::int64_t,const imports::DraftRow&,bool);
void load_metadata(sqlite3*,const imports::Occurrence&,std::int64_t,imports::DraftRow&,bool);
bool categories_format(sqlite3* db){return scalar(db,"PRAGMA user_version")=="5";}

void store_entry(sqlite3*,const imports::Occurrence&,const app::EntrySnapshot&);
void append_financial(sqlite3*,std::string_view,app::ChangeKind,const imports::Occurrence&,std::int64_t,std::int64_t,const Money&,const Money&,const Money&,std::string_view,std::int64_t,std::int64_t=0);

}
struct ImportStore::Impl {
    sqlite3* db=nullptr;
    int lock=-1;
    ImportObserver observer;
    ~Impl() { if (db) sqlite3_close_v2(db); if(lock>=0) ::close(lock); }
    void finish(Atomic& a) {
        if (observer) observer(ImportPhase::before_commit);
        a.commit();
        if (observer) observer(ImportPhase::after_commit);
    }
};
ImportStore::ImportStore(std::unique_ptr<Impl> p):impl_(std::move(p)){}
ImportStore::~ImportStore()=default;
ImportStore::ImportStore(ImportStore&&) noexcept=default;
ImportStore& ImportStore::operator=(ImportStore&&) noexcept=default;
void ImportStore::observer(ImportObserver f) { impl_->observer=std::move(f); }
ImportStore ImportStore::connect(const std::filesystem::path& path, std::string_view password, bool create, const app::OperationContext* context) {
    if(context) context->checkpoint("Opening dataset");
    if (password.empty() || password.size() > 1024) fail(SQLITE_MISUSE, "Password length is invalid");
    if (!create) {
        struct stat input{};
        if (::lstat(path.c_str(), &input)!=0 || !S_ISREG(input.st_mode) || input.st_nlink!=1)
            fail(SQLITE_CANTOPEN, "Dataset is missing, inaccessible, or not a supported regular file");
    }
    if (create) exclusive_file(path);
    auto impl = std::make_unique<Impl>();
    // Use a separate lock inode: on some macOS filesystems SQLite itself uses
    // flock on the database. Never delete the lock file or steal a live lock.
    // An empty leftover file is harmless; kernel ownership ends on process exit.
    auto lock_path=path; lock_path+=".lock";
    impl->lock=::open(lock_path.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW,0600);
    if (impl->lock<0 || ::flock(impl->lock,LOCK_EX|LOCK_NB)!=0) fail(SQLITE_BUSY,"Document is already open or unavailable");
    struct stat st{}, lock_st{};
    if (::stat(path.c_str(),&st)!=0 || !S_ISREG(st.st_mode) || st.st_nlink!=1 ||
        ::fstat(impl->lock,&lock_st)!=0 || !S_ISREG(lock_st.st_mode) || lock_st.st_nlink!=1 || lock_st.st_uid!=::geteuid())
        fail(SQLITE_CANTOPEN,"Document or lock is not a supported regular file");
    ok(sqlite3_open_v2(path.c_str(), &impl->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, nullptr));
    sqlite3* db = impl->db;
    sqlite3_limit(db, SQLITE_LIMIT_LENGTH, static_cast<int>(qif::max_file + 1024 * 1024));
    sqlite3_limit(db, SQLITE_LIMIT_SQL_LENGTH, 64 * 1024);
    sqlite3_limit(db, SQLITE_LIMIT_ATTACHED, 0);
    ok(sqlite3_extended_result_codes(db, 1));
    ok(sqlite3_busy_timeout(db, 1000));
    if (scalar(db, "PRAGMA cipher_version") != "4.19.0 community") fail(SQLITE_ERROR, "Unexpected SQLCipher version");
    ok(sqlite3_key(db, password.data(), static_cast<int>(password.size())));
    exec(db, "PRAGMA cipher_compatibility=4; PRAGMA cipher_memory_security=ON");
    try { (void)scalar(db, "SELECT count(*) FROM sqlite_master"); } // Authenticate BEFORE mutations.
    catch (const ImportError& error) {
        if ((error.code() & 255)==SQLITE_NOTADB) throw ImportAuthenticationError(SQLITE_NOTADB);
        throw;
    }
    if (!create && (scalar(db, "PRAGMA application_id") != "1397770577" || (scalar(db, "PRAGMA user_version") != "1" && scalar(db, "PRAGMA user_version") != "2" && scalar(db, "PRAGMA user_version") != "3" && scalar(db, "PRAGMA user_version") != "4" && scalar(db, "PRAGMA user_version") != "5")))
        fail(SQLITE_SCHEMA, "Unsupported document schema; no migration was attempted");
    if(!create) {
        const auto format=scalar(db,"PRAGMA user_version");
        const bool retained_column=scalar(db,"SELECT count(*) FROM pragma_table_info('batches') WHERE name='materialized_revision'")=="1";
        const auto committed_tables=scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('entry_revisions','entry_allocations','entry_heads','ledger_operations','change_dependencies','history_order','review_stamps')");
        const auto category_tables=scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('category_state','category_versions','category_heads','category_operations','operation_registry','draft_fields','entry_fields','draft_allocation_meta','entry_allocation_meta')");
        if((format!="5" && category_tables!="0") || (format=="5" && category_tables!="9"))fail(SQLITE_SCHEMA,"Category structure does not match format version");
        if(((format=="1" || format=="2") && (retained_column || committed_tables!="0")) ||
           (format=="3" && (!retained_column || committed_tables!="0")) || ((format=="4" || format=="5") && (!retained_column || committed_tables!="7")))
            fail(SQLITE_SCHEMA,"Document structure does not match its format version; no reinterpretation was attempted");
    }
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
    ImportStore store(std::move(impl));
    if (create) {
        Atomic transaction(db);
        exec(db, schema);
        transaction.commit();
        sync_directory(path);
    }
    if (!create) store.verify(context);
    return store;
}
ImportStore ImportStore::create(const std::filesystem::path& path,std::string_view password,std::string_view name,const Currency& currency,std::string_view today) {
    safe_text(name); if(name.empty()) fail(SQLITE_MISUSE,"Account name is required"); iso_date(today);
    // New accounts must use current selection; persisted definitions remain readable.
    if (Currency(currency.code())!=currency) fail(SQLITE_MISUSE,"Currency cannot be selected for a new account");
    auto store=connect(path,password,true);
    Atomic a(store.impl_->db);
    Statement s(store.impl_->db,"INSERT INTO account VALUES(1,?,?,?,0,0,0,0)"); s.text(1,name); s.text(2,currency.definition_id()); s.text(3,today); s.done();
    a.commit(); store.verify(); return store;
}
ImportStore ImportStore::open(const std::filesystem::path& path,std::string_view password,const app::OperationContext* context) { return connect(path,password,false,context); }
app::Account ImportStore::account() const {
    Statement s(impl_->db,"SELECT name,definition,opening_date,opening,frozen,revision,confirmed FROM account WHERE singleton=1");
    if (!s.row()) fail(SQLITE_CORRUPT);
    const auto currency=Currency::from_definition(s.bytes(1));
    return {s.bytes(0),currency,s.bytes(2),Money(s.integer(3),currency),s.integer(4)!=0,s.integer(5),s.integer(6)!=0};
}
void ImportStore::opening(std::int64_t expected,std::string_view date,const Money& amount) {
    iso_date(date); Atomic a(impl_->db); const auto acct=account(); acct.opening.require_compatible(amount);
    if(acct.frozen || acct.revision!=expected || expected==INT64_MAX) fail(SQLITE_CONSTRAINT,"Opening balance is frozen or changed");
    Statement s(impl_->db,"UPDATE account SET opening_date=?,opening=?,revision=revision+1,confirmed=1 WHERE singleton=1");
    s.text(1,date); s.integer(2,amount.units()); s.done();
    for(const auto& b:batches()) if(b.state==app::BatchState::pending || b.state==app::BatchState::captured) advance(impl_->db,b.id,b.revision,"opening",static_cast<int>(b.state));
    impl_->finish(a);
}
std::vector<app::BatchInfo> ImportStore::batches() const {
    Statement s(impl_->db,"SELECT id FROM batches ORDER BY rowid"); std::vector<app::BatchInfo> result;
    while(s.row()) result.push_back(batch(impl_->db,s.bytes(0))); return result;
}
std::string ImportStore::evidence(std::string_view id) const {
    Statement s(impl_->db,"SELECT bytes FROM sources WHERE id=?"); s.text(1,id); if(!s.row()) fail(SQLITE_NOTFOUND); return s.bytes(0);
}
app::RowView ImportStore::row(const imports::Occurrence& id) const { return load_row(id,true); }
app::RowView ImportStore::load_row(const imports::Occurrence& id,bool candidates) const {
    const auto b=batch(impl_->db,id.batch); const auto acct=account();
    Statement src(impl_->db,"SELECT raw FROM records WHERE batch=? AND ordinal=?"); src.text(1,id.batch); src.integer(2,integer(id.ordinal));
    if(!src.row()) {
        auto parsed=qif::parse(evidence(id.batch),b.profile,acct.currency);
        if(id.ordinal>=parsed.records.size()) fail(SQLITE_NOTFOUND);
        auto source=std::move(parsed.records[id.ordinal]);
        if(scalar(impl_->db,"PRAGMA user_version")=="1") source.repairable=false;
        return {id,std::move(source),{}, {imports::Disposition::unresolved,{}},{}};
    }
    auto parsed=qif::parse(src.bytes(0),b.profile,acct.currency);
    if(parsed.diagnostic || parsed.records.size()!=1 || (!parsed.records[0].transaction && !parsed.records[0].repairable)) fail(SQLITE_CORRUPT);
    app::RowView out{id,std::move(parsed.records[0]),{}, {},{}};
    Statement v(impl_->db,"SELECT revision,date,payee,memo,total,decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? ORDER BY revision DESC LIMIT 1");
    v.text(1,id.batch); v.integer(2,integer(id.ordinal)); if(!v.row()) fail(SQLITE_CORRUPT);
    Statement allocations(impl_->db,"SELECT position,category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");
    allocations.text(1,id.batch); allocations.integer(2,integer(id.ordinal)); allocations.integer(3,v.integer(0));
    std::vector<Allocation> aa;
    while(allocations.row()) {
        if (allocations.integer(0)!=integer(aa.size()) || aa.size()>=qif::max_allocations) fail(SQLITE_CORRUPT);
        aa.push_back({allocations.bytes(1),allocations.bytes(2),Money(allocations.integer(3),acct.currency)});
    }
    out.draft=imports::DraftRow{v.bytes(1),v.bytes(2),v.bytes(3),Transaction::corrected_import(*out.source.original_amount,Money(v.integer(4),acct.currency),std::move(aa))};
    load_metadata(impl_->db,id,v.integer(0),*out.draft,false);
    imports::validate_draft(out.source,*out.draft);
    out.decision.disposition=static_cast<imports::Disposition>(v.integer(5));
    if(out.decision.disposition==imports::Disposition::duplicate) out.decision.duplicate_of=imports::Occurrence{v.bytes(6),static_cast<std::size_t>(v.integer(7))};
    if(committed_changes_supported() && out.decision.duplicate_of) {
        Statement target(impl_->db,"SELECT s.target_revision,s.target_active,h.revision,e.active FROM review_stamps s JOIN entry_heads h ON h.batch=? AND h.ordinal=? JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE s.batch=? AND s.ordinal=? AND s.revision=?");
        target.text(1,out.decision.duplicate_of->batch);target.integer(2,integer(out.decision.duplicate_of->ordinal));target.text(3,id.batch);target.integer(4,integer(id.ordinal));target.integer(5,v.integer(0));
        if(!target.row())fail(SQLITE_CORRUPT);out.review_stale=target.integer(0)!=target.integer(2) || target.integer(1)!=target.integer(3);
    }
    if (!candidates) return out;
    Statement c(impl_->db,"SELECT r.batch,r.ordinal FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal "
        "WHERE b.state=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=1 "
        "AND (r.source_date=? OR r.source_date='' OR ?='') AND r.original=? ORDER BY r.batch,r.ordinal");
    c.text(1,out.source.date); c.text(2,out.source.date); c.integer(3,out.source.original_amount->units());
    while(c.row()) out.candidates.push_back({c.bytes(0),static_cast<std::size_t>(c.integer(1))});
    return out;
}
app::BatchInfo ImportStore::ingest(std::string_view id,std::string_view bytes,qif::DateOrder profile) {
    identifier(id); const auto acct=account(); auto parsed=qif::parse(bytes,profile,acct.currency);
    if(!parsed.admissible) fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    Atomic a(impl_->db);
    Statement retry(impl_->db,"SELECT profile,bytes FROM batches JOIN sources USING(id) WHERE id=?"); retry.text(1,id);
    if(retry.row()) {
        if(retry.integer(0)!=static_cast<int>(profile) || retry.bytes(1)!=bytes) fail(SQLITE_CONSTRAINT,"Import identity was reused with different content");
        auto result=batch(impl_->db,id); a.commit(); return result;
    }
    Statement pending(impl_->db,"SELECT id,profile,bytes FROM batches JOIN sources USING(id) WHERE state IN(0,3)");
    if(pending.row()) {
        if(pending.integer(1)==static_cast<int>(profile) && pending.bytes(2)==bytes) {
            auto result=batch(impl_->db,pending.bytes(0)); a.commit(); return result;
        }
        fail(SQLITE_CONSTRAINT,"Finish or discard the pending import first");
    }
    Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)"); evidence.text(1,id); evidence.blob(2,bytes); evidence.done();
    Statement insert(impl_->db,(scalar(impl_->db,"PRAGMA user_version")=="3" || scalar(impl_->db,"PRAGMA user_version")=="4" || scalar(impl_->db,"PRAGMA user_version")=="5")?"INSERT INTO batches VALUES(?,?,?,0,0,?,?,0)":"INSERT INTO batches VALUES(?,?,?,0,0,?,?)");
    insert.text(1,id); insert.integer(2,static_cast<int>(profile)); insert.text(3,qif::parser_version);
    insert.integer(4,integer(parsed.records.size())); insert.integer(5,parsed.valid()?0:1); insert.done();
    populate(id,bytes,parsed,0,nullptr);
    Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'ingest')"); event.text(1,id); event.done();
    impl_->finish(a); return batch(impl_->db,id);
}
app::BatchInfo ImportStore::ingest_controlled(std::string_view id,std::string_view bytes,qif::DateOrder profile,app::OperationContext& context) {
    if((scalar(impl_->db,"PRAGMA user_version")!="3" && scalar(impl_->db,"PRAGMA user_version")!="4" && scalar(impl_->db,"PRAGMA user_version")!="5")) {
        context.seal();context.checkpoint("Saving import evidence and drafts");return ingest(id,bytes,profile);
    }
    identifier(id);context.checkpoint("Checking QIF resource limits and format");
    auto parsed=qif::parse(bytes,profile,account().currency,[&]{context.checkpoint();});
    if(!parsed.admissible) fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    // Existing evidence is navigated to, never implicitly resumed.
    Statement prior(impl_->db,"SELECT id,profile,bytes FROM batches JOIN sources USING(id) WHERE id=? OR state IN(0,3) ORDER BY id=? DESC");
    prior.text(1,id);prior.text(2,id);
    if(prior.row()) {
        if(prior.integer(1)!=static_cast<int>(profile) || prior.bytes(2)!=bytes)
            fail(SQLITE_CONSTRAINT,"Import identity conflicts or another import needs review");
        context.seal();return batch(impl_->db,prior.bytes(0));
    }
    context.checkpoint("Retaining source evidence");
    try {
        Atomic transaction(impl_->db);
        Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)");evidence.text(1,id);evidence.blob(2,bytes);evidence.done();
        Statement insert(impl_->db,"INSERT INTO batches VALUES(?,?,?,3,0,?,1,NULL)");
        insert.text(1,id);insert.integer(2,static_cast<int>(profile));insert.text(3,qif::parser_version);insert.integer(4,integer(parsed.records.size()));insert.done();
        Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'capture')");event.text(1,id);event.done();
        context.seal();context.checkpoint("Finishing source evidence retention");impl_->finish(transaction);
    } catch (...) {
        const auto failure=std::current_exception();
        if(!sqlite3_get_autocommit(impl_->db)){context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
        try {
            Statement saved(impl_->db,"SELECT id FROM batches WHERE id=?");saved.text(1,id);
            if(saved.row()) {context.state.store(app::OperationContext::sealed);return batch(impl_->db,id);}
        } catch (...) {context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
        std::rethrow_exception(failure);
    }
    // Reopen only a sealed gate after known durable capture. No accepted request
    // can be lost here, because request() cannot change sealed to requested.
    auto sealed=app::OperationContext::sealed;
    if(!context.state.compare_exchange_strong(sealed,app::OperationContext::available)) fail(SQLITE_MISUSE);
    parsed={};
    try {
        context.checkpoint("Source evidence retained");
        return resume(id,0,context);
    } catch(const app::OperationCancelled&) {
        throw app::OperationCancelled("Processing cancelled. Source evidence is retained; resume or discard the incomplete import.");
    }
}
app::BatchInfo ImportStore::resume(std::string_view id,std::int64_t expected,app::OperationContext& context) {
    if((scalar(impl_->db,"PRAGMA user_version")!="3" && scalar(impl_->db,"PRAGMA user_version")!="4" && scalar(impl_->db,"PRAGMA user_version")!="5")) fail(SQLITE_SCHEMA,"Resuming evidence requires a new experimental dataset");
    if(expected<0)fail(SQLITE_CONSTRAINT,"Invalid import revision");
    context.checkpoint("Preparing draft interpretations");
    const auto current=batch(impl_->db,id);
    Statement metadata(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");metadata.text(1,id);if(!metadata.row())fail(SQLITE_NOTFOUND);
    if(!metadata.null(0) && expected<INT64_MAX && metadata.integer(0)==expected+1) {context.seal();return current;}
    if(current.state!=app::BatchState::captured || current.revision!=expected || expected==INT64_MAX)
        fail(SQLITE_CONSTRAINT,"Import changed; refresh before resuming");
    try {
        const auto bytes=evidence(id);
        const auto parsed=qif::parse(bytes,current.profile,account().currency,[&]{context.checkpoint();});
        if(!parsed.admissible || parsed.records.size()!=current.count) fail(SQLITE_CORRUPT);
        try {
            Atomic transaction(impl_->db);
            Statement update(impl_->db,"UPDATE batches SET state=0,revision=revision+1,blocked=?,materialized_revision=revision+1 WHERE id=?");
            update.integer(1,parsed.valid()?0:1);update.text(2,id);update.done();
            Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,?,'materialize')");event.text(1,id);event.integer(2,expected+1);event.done();
            populate(id,bytes,parsed,expected+1,&context);
            context.seal();context.checkpoint("Finishing draft interpretations");impl_->finish(transaction);
        } catch (...) {
            const auto failure=std::current_exception();
            if(!sqlite3_get_autocommit(impl_->db)){context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
            try {
                Statement saved(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");saved.text(1,id);
                if(!saved.row()) fail(SQLITE_CORRUPT);
                if(!saved.null(0) && saved.integer(0)==expected+1) {context.state.store(app::OperationContext::sealed);return batch(impl_->db,id);}
            } catch (...) {context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
            std::rethrow_exception(failure);
        }
        return batch(impl_->db,id);
    } catch(const app::OperationCancelled&) {
        throw app::OperationCancelled("Processing cancelled. Source evidence is retained; resume or discard the incomplete import.");
    }
}
void ImportStore::populate(std::string_view id,std::string_view bytes,const qif::Result& parsed,std::int64_t revision,const app::OperationContext* context) {
    const auto profile=batch(impl_->db,id).profile;
    std::string identical;
    Statement prior(impl_->db,"SELECT id FROM batches JOIN sources USING(id) WHERE state=1 AND profile=? AND bytes=? ORDER BY batches.rowid DESC LIMIT 1");
    prior.integer(1,static_cast<int>(profile)); prior.blob(2,bytes); if(prior.row()) identical=prior.bytes(0);
    Statement sequence(impl_->db,"SELECT coalesce(max(sequence),0) FROM records"); if(!sequence.row()) fail(SQLITE_CORRUPT);
    auto seq=sequence.integer(0);
    std::map<std::pair<std::string,std::int64_t>,bool> candidate_groups;
    for(std::size_t i=0;i<parsed.records.size();++i) {
        if(context) context->checkpoint();
        const auto& r=parsed.records[i]; if(r.diagnostic || !r.transaction) continue;
        if(seq==INT64_MAX) fail(SQLITE_FULL,"Entry identity capacity exhausted");
        Statement record(impl_->db,"INSERT INTO records VALUES(?,?,?,?,?,?,?,?)");
        record.text(1,id); record.integer(2,integer(i)); record.integer(3,++seq); record.blob(4,bytes.substr(r.begin,r.end-r.begin));
        record.text(5,r.date); record.integer(6,r.transaction->original_imported_amount()->units()); record.text(7,r.payee); record.text(8,r.reference); record.done();
        imports::Decision decision;
        if(!identical.empty()) {
            const auto original=load_row({identical,i},false);
            decision.disposition=imports::Disposition::duplicate;
            decision.duplicate_of=original.decision.duplicate_of ? original.decision.duplicate_of : std::optional(imports::Occurrence{identical,i});
        } else {
            const auto key=std::pair{r.date,r.transaction->original_imported_amount()->units()};
            const auto found=candidate_groups.find(key);
            if(found!=candidate_groups.end()) {
                if(found->second) decision.disposition=imports::Disposition::unresolved;
            } else {
            Statement candidate(impl_->db,"SELECT 1 FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal "
                "WHERE b.state=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=1 "
                "AND (r.source_date=? OR r.source_date='') AND r.original=? LIMIT 1");
            candidate.text(1,r.date); candidate.integer(2,r.transaction->original_imported_amount()->units());
            const bool has_candidate=candidate.row();
            candidate_groups.emplace(key,has_candidate);
            if(has_candidate) decision.disposition=imports::Disposition::unresolved;
            }
        }
        if(committed_changes_supported() && decision.duplicate_of && !committed_entry(*decision.duplicate_of).current.active)decision={imports::Disposition::unresolved,{}};
        version(impl_->db,{std::string(id),i},revision,{r.date,r.payee,r.memo,*r.transaction,FieldOwnership::source,FieldOwnership::source},decision);
        if(context && impl_->observer)impl_->observer(ImportPhase::materialize_row);
    }
}
void ImportStore::save(const imports::Occurrence& id,std::int64_t expected,const imports::DraftRow& edit) {
    Atomic a(impl_->db); expect(impl_->db,id.batch,expected); const auto old=load_row(id,false);
    const auto canonical=canonical_draft(id,edit,old.draft,expected+1,false);
    imports::validate_draft(old.source,canonical);
    if(expected==INT64_MAX) fail(SQLITE_FULL);
    if(!old.draft) {
        if(scalar(impl_->db,"PRAGMA user_version")=="1" || !old.source.repairable)
            fail(SQLITE_CONSTRAINT,"Record repair requires a new experimental dataset");
        const auto bytes=evidence(id.batch);
        Statement sequence(impl_->db,"SELECT coalesce(max(sequence),0) FROM records");
        if(!sequence.row() || sequence.integer(0)==INT64_MAX) fail(SQLITE_FULL);
        Statement record(impl_->db,"INSERT INTO records VALUES(?,?,?,?,?,?,?,?)");
        record.text(1,id.batch); record.integer(2,integer(id.ordinal)); record.integer(3,sequence.integer(0)+1);
        record.blob(4,std::string_view(bytes).substr(old.source.begin,old.source.end-old.source.begin));
        record.text(5,old.source.date); record.integer(6,old.source.original_amount->units());
        record.text(7,old.source.payee); record.text(8,old.source.reference); record.done();
    }
    int blocked=-1;
    if(!old.draft) {
        const auto b=batch(impl_->db,id.batch);
        const auto parsed=qif::parse(evidence(id.batch),b.profile,account().currency);
        Statement count(impl_->db,"SELECT count(*) FROM records WHERE batch=?"); count.text(1,id.batch); if(!count.row()) fail(SQLITE_CORRUPT);
        blocked=(parsed.diagnostic || count.integer(0)!=integer(b.count))?1:0;
    }
    // A first repair requires an explicit subsequent duplicate disposition.
    version(impl_->db,id,expected+1,canonical,old.decision,false); advance(impl_->db,id.batch,expected,"save",0,blocked);
    impl_->finish(a);
}
void ImportStore::decide(const imports::Occurrence& id,std::int64_t expected,const imports::Decision& decision) {
    Atomic a(impl_->db); expect(impl_->db,id.batch,expected); const auto old=row(id);
    if(!old.draft) fail(SQLITE_CONSTRAINT,"Save the record repair before duplicate review");
    imports::validate_decisions({old.candidates},std::span(&decision,1),false);
    if(decision.duplicate_of) {
        Statement used(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND ordinal!=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) "
            "AND decision=2 AND duplicate_batch=? AND duplicate_ordinal=? LIMIT 1");
        used.text(1,id.batch); used.integer(2,integer(id.ordinal)); used.text(3,decision.duplicate_of->batch); used.integer(4,integer(decision.duplicate_of->ordinal));
        if(used.row()) fail(SQLITE_CONSTRAINT,"That prior occurrence is already matched in this import");
    }
    if(expected==INT64_MAX) fail(SQLITE_FULL);
    version(impl_->db,id,expected+1,*old.draft,decision); advance(impl_->db,id.batch,expected,"decide"); impl_->finish(a);
}
void ImportStore::discard(std::string_view id,std::int64_t expected) {
    Atomic a(impl_->db); const auto b=batch(impl_->db,id);
    if(b.state==app::BatchState::discarded && expected<INT64_MAX && b.revision==expected+1) { a.commit(); return; }
    advance(impl_->db,id,expected,"discard",2); impl_->finish(a);
}
std::optional<app::Receipt> ImportStore::durable_outcome(std::string_view operation) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    return outcome(operation);
}
std::optional<app::Receipt> ImportStore::outcome(std::string_view operation) const {
    const auto c=account().currency;
    Statement s(impl_->db,"SELECT batch,revision,included,excluded,before,change,after FROM operations WHERE id=?"); s.text(1,operation);
    if(!s.row()) return {};
    return app::Receipt{std::string(operation),s.bytes(0),s.integer(1),static_cast<std::size_t>(s.integer(2)),static_cast<std::size_t>(s.integer(3)),
        Money(s.integer(4),c),Money(s.integer(5),c),Money(s.integer(6),c)};
}
app::Receipt ImportStore::commit(std::string_view id,std::int64_t expected,std::string_view operation) {
    identifier(operation); Atomic a(impl_->db);
    if(committed_changes_supported()) {Statement collision(impl_->db,"SELECT kind FROM ledger_operations WHERE id=?");collision.text(1,operation);if(collision.row() && collision.integer(0)!=0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");}
    if(auto prior=outcome(operation)) {
        if(prior->batch!=id || prior->revision!=expected) fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        a.commit(); return *prior;
    }
    expect(impl_->db,id,expected); const auto b=batch(impl_->db,id);
    if(b.blocked) fail(SQLITE_CONSTRAINT,"Resolve the blocked import before committing");
    const auto acct=account();
    if(!acct.opening_confirmed) fail(SQLITE_CONSTRAINT,"Confirm the opening balance before the first commit");
    const auto before=committed_balance();
    std::size_t included=0,excluded=0; std::vector<Money> amounts; std::set<imports::Occurrence> matched;
    for(std::size_t i=0;i<b.count;++i) {
        auto r=load_row({std::string(id),i},false);
        if(r.decision.disposition==imports::Disposition::unresolved) fail(SQLITE_CONSTRAINT,"Review possible duplicates before committing");
        if(r.decision.disposition==imports::Disposition::include) { ++included; amounts.push_back(r.draft->transaction.effective_total()); }
        else if(r.decision.disposition==imports::Disposition::duplicate && r.decision.duplicate_of) {
            const auto& target=*r.decision.duplicate_of;
            const auto other=load_row(target,false);
            if(committed_changes_supported()) {
                Statement stamp(impl_->db,"SELECT s.target_revision,s.target_active FROM review_stamps s WHERE s.batch=? AND s.ordinal=? ORDER BY s.revision DESC LIMIT 1");
                stamp.text(1,id);stamp.integer(2,integer(i));const auto current=committed_entry(target);
                if(!stamp.row() || stamp.integer(0)!=current.current.revision || stamp.integer(1)!=(current.current.active?1:0))fail(SQLITE_CONSTRAINT,"Duplicate target changed; reconfirm the review decision");
            }
            if(batch(impl_->db,target.batch).state!=app::BatchState::committed || other.decision.disposition!=imports::Disposition::include ||
                !imports::candidate_basis(r.source,other.source) || !matched.insert(target).second) fail(SQLITE_CONSTRAINT,"Invalid duplicate relationship");
            ++excluded;
        } else fail(SQLITE_CORRUPT);
    }
    const auto change=imports::sum(acct.currency,amounts); const auto after=before+change;
    Statement op(impl_->db,"INSERT INTO operations VALUES(?,?,?,?,?,?,?,?)"); op.text(1,operation); op.text(2,id); op.integer(3,expected);
    op.integer(4,integer(included)); op.integer(5,integer(excluded)); op.integer(6,before.units()); op.integer(7,change.units()); op.integer(8,after.units()); op.done();
    advance(impl_->db,id,expected,"commit",1);
    exec(impl_->db,"UPDATE account SET frozen=1 WHERE singleton=1");
    if(committed_changes_supported()) {
        for(std::size_t ordinal=0;ordinal<b.count;++ordinal){const auto row=load_row({std::string(id),ordinal},false);if(row.decision.disposition==imports::Disposition::include)store_entry(impl_->db,{std::string(id),ordinal},{0,true,*row.draft});}
        append_financial(impl_->db,operation,app::ChangeKind::publication,{std::string(id),0},-1,0,before,change,after,"",0);
    }
    impl_->finish(a); return {std::string(operation),std::string(id),expected,included,excluded,before,change,after};
}
std::vector<app::LedgerRow> ImportStore::ledger(bool projected) const {
    if(committed_changes_supported()) {
        if(projected && !projected_balance())fail(SQLITE_CONSTRAINT,"Projection is incomplete; review the pending import");
        std::vector<app::LedgerRow> result;
        Statement entries(impl_->db,"SELECT h.batch,h.ordinal,h.revision,r.sequence FROM entry_heads h JOIN entry_revisions e USING(batch,ordinal,revision) JOIN records r ON r.batch=h.batch AND r.ordinal=h.ordinal WHERE e.active=1 ORDER BY e.date,r.sequence");
        while(entries.row()){imports::Occurrence identity{entries.bytes(0),static_cast<std::size_t>(entries.integer(1))};result.push_back({identity,entries.integer(3),entry_snapshot(identity,entries.integer(2)).draft,true});}
        if(projected){Statement pending(impl_->db,"SELECT r.batch,r.ordinal,r.sequence FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE b.state=0 AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal)");while(pending.row()){imports::Occurrence identity{pending.bytes(0),static_cast<std::size_t>(pending.integer(1))};result.push_back({identity,pending.integer(2),*load_row(identity,false).draft,false});}}
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.draft.date,a.sequence)<std::tie(b.draft.date,b.sequence);});return result;
    }
    if(projected && !projected_balance()) fail(SQLITE_CONSTRAINT,"Projection is incomplete; resolve the pending import first");
    Statement s(impl_->db,"SELECT r.batch,r.ordinal,r.sequence,b.state FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal "
        "WHERE (b.state=1 OR (b.state=0 AND ?=1 AND b.blocked=0)) AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) ORDER BY v.date,r.sequence");
    s.integer(1,projected?1:0); std::vector<app::LedgerRow> result;
    while(s.row()) {
        imports::Occurrence id{s.bytes(0),static_cast<std::size_t>(s.integer(1))}; auto r=load_row(id,false);
        result.push_back({id,s.integer(2),std::move(*r.draft),s.integer(3)==1});
    }
    return result;
}
Money ImportStore::committed_balance() const {
    if(committed_changes_supported()) {const auto currency=account().currency;std::vector<Money> values{account().opening};Statement rows(impl_->db,"SELECT e.total FROM entry_heads h JOIN entry_revisions e USING(batch,ordinal,revision) WHERE e.active=1");while(rows.row())values.emplace_back(rows.integer(0),currency);return imports::sum(currency,values);}
    const auto a=account(); std::vector<Money> amounts{a.opening};
    Statement s(impl_->db,"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.state=1 AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    while(s.row()) amounts.emplace_back(s.integer(0),a.currency); return imports::sum(a.currency,amounts);
}
std::optional<Money> ImportStore::projected_balance() const {
    for(const auto& b:batches()) if(b.state==app::BatchState::pending || b.state==app::BatchState::captured) {
        if(b.blocked) return {};
        Statement s(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=0 AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) LIMIT 1");
        s.text(1,b.id); if(s.row()) return {};
    }
    if(committed_changes_supported()) {
        Statement stale(impl_->db,"SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch JOIN review_stamps s USING(batch,ordinal,revision) JOIN entry_heads h ON h.batch=v.duplicate_batch AND h.ordinal=v.duplicate_ordinal JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE b.state=0 AND v.decision=2 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) AND (s.target_revision!=h.revision OR s.target_active!=e.active) LIMIT 1");if(stale.row())return {};
    }
    const auto a=account(); std::vector<Money> amounts{committed_changes_supported()?committed_balance():a.opening};
    Statement s(impl_->db,committed_changes_supported()?"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.state=0 AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)":"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.state IN(0,1) AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    while(s.row()) amounts.emplace_back(s.integer(0),a.currency); return imports::sum(a.currency,amounts);
}
void ImportStore::verify(const app::OperationContext* context) const {
    if(context) context->checkpoint("Verifying dataset integrity");
    if(scalar(impl_->db,"PRAGMA integrity_check")!="ok") fail(SQLITE_CORRUPT);
    for(const char* sql:{"PRAGMA cipher_integrity_check","PRAGMA foreign_key_check"}) { Statement s(impl_->db,sql); if(s.row()) fail(SQLITE_CORRUPT); }
    const auto acct=account(); safe_text(acct.name); iso_date(acct.opening_date);
    if(acct.frozen && !acct.opening_confirmed) fail(SQLITE_CORRUPT);
    Statement invalid(impl_->db,
        "SELECT 1 FROM batches b WHERE parser!='bank-v1' OR (state=1)!=(EXISTS(SELECT 1 FROM operations o WHERE o.batch=b.id)) "
        "OR revision!=(SELECT max(revision) FROM events WHERE batch=b.id) OR revision+1!=(SELECT count(*) FROM events WHERE batch=b.id) "
        "UNION ALL SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch WHERE v.revision>b.revision "
        "UNION ALL SELECT 1 FROM sources s WHERE NOT EXISTS(SELECT 1 FROM batches b WHERE b.id=s.id) "
        "UNION ALL SELECT 1 FROM account WHERE frozen!=(EXISTS(SELECT 1 FROM operations))");
    if(invalid.row()) fail(SQLITE_CORRUPT,"Invalid import history relationships");
    for(const auto& b:batches()) {
        if(context) context->checkpoint("Verifying import evidence");
        const auto bytes=evidence(b.id); const auto parsed=qif::parse(bytes,b.profile,acct.currency,[&]{if(context)context->checkpoint();});
        if(!parsed.admissible || parsed.records.size()!=b.count) fail(SQLITE_CORRUPT);
        Statement count(impl_->db,"SELECT count(*) FROM records WHERE batch=?"); count.text(1,b.id); if(!count.row()) fail(SQLITE_CORRUPT);
        std::int64_t initial_revision=0;
        if((scalar(impl_->db,"PRAGMA user_version")=="3" || scalar(impl_->db,"PRAGMA user_version")=="4" || scalar(impl_->db,"PRAGMA user_version")=="5")) {
            Statement materialized(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");materialized.text(1,b.id);if(!materialized.row())fail(SQLITE_CORRUPT);
            Statement first(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=0");first.text(1,b.id);if(!first.row())fail(SQLITE_CORRUPT);
            if(materialized.null(0)) {
                if(first.bytes(0)!="capture" || (b.state!=app::BatchState::captured && b.state!=app::BatchState::discarded) || !b.blocked || count.integer(0)!=0) fail(SQLITE_CORRUPT);
                Statement bad(impl_->db,"SELECT 1 FROM events WHERE batch=? AND ((revision=0 AND kind!='capture') OR (revision>0 AND kind NOT IN('opening','discard')) OR (kind='discard' AND (revision!=? OR ?!=2)))");
                bad.text(1,b.id);bad.integer(2,b.revision);bad.integer(3,static_cast<int>(b.state));if(bad.row())fail(SQLITE_CORRUPT);
                Statement last(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");last.text(1,b.id);last.integer(2,b.revision);if(!last.row() || (b.state==app::BatchState::discarded)!=(last.bytes(0)=="discard"))fail(SQLITE_CORRUPT);
                continue;
            }
            initial_revision=materialized.integer(0);
            if(initial_revision<0 || initial_revision>b.revision || b.state==app::BatchState::captured || first.bytes(0)!=(initial_revision==0?"ingest":"capture"))fail(SQLITE_CORRUPT);
            Statement events(impl_->db,"SELECT revision,kind FROM events WHERE batch=? AND (kind IN('capture','ingest','materialize') OR revision<=?) ORDER BY revision");
            events.text(1,b.id);events.integer(2,initial_revision);
            bool saw_materialized=initial_revision==0;
            while(events.row()) {
                const auto revision=events.integer(0);const auto kind=events.bytes(1);
                if(revision==0) continue;
                if(revision<initial_revision && kind=="opening")continue;
                if(initial_revision>0 && revision==initial_revision && kind=="materialize" && !saw_materialized){saw_materialized=true;continue;}
                fail(SQLITE_CORRUPT);
            }
            if(!saw_materialized)fail(SQLITE_CORRUPT);
        }
        std::size_t valid_count=0; bool blocked=parsed.diagnostic.has_value();
        for(std::size_t i=0;i<parsed.records.size();++i) {
            if(context) context->checkpoint();
            const auto& source=parsed.records[i];
            Statement exists(impl_->db,"SELECT 1 FROM records WHERE batch=? AND ordinal=?"); exists.text(1,b.id); exists.integer(2,integer(i));
            const bool stored=exists.row();
            if(source.diagnostic || !source.transaction) {
                if(!stored) {blocked=true; continue;}
                if(!source.repairable || scalar(impl_->db,"PRAGMA user_version")=="1") fail(SQLITE_CORRUPT);
            }
            ++valid_count;
            Statement rec(impl_->db,"SELECT raw,source_date,original,payee,reference FROM records WHERE batch=? AND ordinal=?"); rec.text(1,b.id); rec.integer(2,integer(i));
            if(!rec.row() || rec.bytes(0)!=std::string_view(bytes).substr(source.begin,source.end-source.begin) || rec.bytes(1)!=source.date ||
                rec.integer(2)!=source.original_amount->units() || rec.bytes(3)!=source.payee || rec.bytes(4)!=source.reference) fail(SQLITE_CORRUPT);
            (void)load_row({b.id,i},false);
            Statement vv(impl_->db,"SELECT revision,date,payee,memo,total,decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? ORDER BY revision");
            vv.text(1,b.id); vv.integer(2,integer(i)); bool initial=true;
            while(vv.row()) {
                if(context) context->checkpoint();
                if(initial) {
                    if(source.diagnostic) {
                        if(vv.integer(0)<=initial_revision || vv.integer(5)!=0) fail(SQLITE_CORRUPT);
                        Statement event(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");
                        event.text(1,b.id); event.integer(2,vv.integer(0));
                        if(!event.row() || event.bytes(0)!="save") fail(SQLITE_CORRUPT);
                    } else if(vv.integer(0)!=initial_revision) fail(SQLITE_CORRUPT);
                } initial=false;
                Statement aa(impl_->db,"SELECT position,category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");
                aa.text(1,b.id); aa.integer(2,integer(i)); aa.integer(3,vv.integer(0)); std::vector<Allocation> allocations;
                while(aa.row()) {
                    if(aa.integer(0)!=integer(allocations.size()) || allocations.size()>=qif::max_allocations) fail(SQLITE_CORRUPT);
                    allocations.push_back({aa.bytes(1),aa.bytes(2),Money(aa.integer(3),acct.currency)});
                }
                imports::validate_draft(source,{vv.bytes(1),vv.bytes(2),vv.bytes(3),Transaction::corrected_import(*source.original_amount,Money(vv.integer(4),acct.currency),std::move(allocations))});
                if(vv.integer(5)==2) {
                    imports::Occurrence target{vv.bytes(6),static_cast<std::size_t>(vv.integer(7))};
                    if(target.batch==b.id || batch(impl_->db,target.batch).state!=app::BatchState::committed) fail(SQLITE_CORRUPT);
                    const auto other=load_row(target,false);
                    if(other.decision.disposition!=imports::Disposition::include || !imports::candidate_basis(source,other.source)) fail(SQLITE_CORRUPT);
                }
            }
        }
        if(count.integer(0)!=integer(valid_count) || blocked!=b.blocked) fail(SQLITE_CORRUPT);
        Statement duplicates(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=2 AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) "
            "GROUP BY duplicate_batch,duplicate_ordinal HAVING count(*)>1"); duplicates.text(1,b.id); if(duplicates.row()) fail(SQLITE_CORRUPT);
    }
    if(categories_supported())verify_categories(context);
    if(committed_changes_supported()){verify_committed(context);return;}
    Money balance=acct.opening;
    Statement ops(impl_->db,"SELECT id FROM operations ORDER BY rowid");
    while(ops.row()) {
        if(context) context->checkpoint();
        const auto receipt=*outcome(ops.bytes(0)); const auto b=batch(impl_->db,receipt.batch);
        if(b.blocked || receipt.revision==INT64_MAX || receipt.revision+1!=b.revision || receipt.before!=balance) fail(SQLITE_CORRUPT);
        std::size_t included=0,excluded=0; std::vector<Money> values;
        Statement rows(impl_->db,"SELECT total,decision FROM versions v WHERE batch=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
        rows.text(1,b.id);
        while(rows.row()) {
            if(rows.integer(1)==1) { ++included; values.emplace_back(rows.integer(0),acct.currency); }
            else if(rows.integer(1)==2) ++excluded; else fail(SQLITE_CORRUPT);
        }
        if(receipt.included!=included || receipt.excluded!=excluded || included+excluded!=b.count || receipt.change!=imports::sum(acct.currency,values)) fail(SQLITE_CORRUPT);
        balance=balance+receipt.change; if(balance!=receipt.after) fail(SQLITE_CORRUPT);
    }
    if(balance!=committed_balance()) fail(SQLITE_CORRUPT);
}
void ImportStore::backup_to(const std::filesystem::path& destination,std::string_view password,
                          const BackupObserver& observer) const {
    auto candidate=destination; candidate+=".partial";
    {
        auto target=connect(candidate,password,true);
        sqlite3_backup* backup=sqlite3_backup_init(target.impl_->db,"main",impl_->db,"main");
        if(!backup) fail(sqlite3_errcode(target.impl_->db));
        const int step=sqlite3_backup_step(backup,-1), finish=sqlite3_backup_finish(backup);
        if(step!=SQLITE_DONE) fail(step); ok(finish); target.verify();
    }
    { auto restored=open(candidate,password); restored.verify(); }
    if(observer) observer(BackupPhase::before_publish);
    if(::link(candidate.c_str(),destination.c_str())!=0) fail(SQLITE_CANTOPEN,"Backup publication failed; encrypted candidate retained");
    if(observer) observer(BackupPhase::before_publish_sync);
    sync_directory(destination);
    if(observer) observer(BackupPhase::before_cleanup);
    if(::unlink(candidate.c_str())!=0) fail(SQLITE_IOERR_DELETE,"Backup published; candidate cleanup failed");
    if(observer) observer(BackupPhase::before_cleanup_sync);
    sync_directory(destination);
    if(observer) observer(BackupPhase::complete);
}
#include "category_store.inc"
#include "committed_store.inc"
} // namespace spentit::storage
