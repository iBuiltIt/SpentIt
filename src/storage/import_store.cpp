#include <spentit/import_store.hpp>
#include <spentit/statements.hpp>
#include <sys/file.h>
#include <sys/stat.h>
#include <set>
#include <tuple>
#include <map>
#include <unordered_set>
#include <sqlite3.h>
#include <algorithm>
#include <climits>
#include <charconv>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <unistd.h>
#ifdef SPENTIT_RULES_STORAGE
#include <spentit/rule_preview.hpp>
#endif

namespace spentit::storage {
namespace {
[[noreturn]] void fail(int code, const char* message = "Encrypted storage operation failed") {
    throw ImportError(code, message); // Never expose SQL, keys, paths or record values.
}
void ok(int rc) { if (rc != SQLITE_OK) fail(rc); }
void exec(sqlite3* db, const char* sql) { ok(sqlite3_exec(db, sql, nullptr, nullptr, nullptr)); }
// Reuse prepared read plans only within one bounded read operation. Results are
// never cached: each use binds afresh and steps the database again. Finalizing
// the scope releases every plan before a document can close or a command return.
class ReadStatements {
public:
    struct Slot { sqlite3_stmt* statement=nullptr; std::size_t bytes=0; };
    explicit ReadStatements(sqlite3* db):db_(db),previous_(active_) {
        active_=previous_ && previous_->db_==db ? previous_ : this;
    }
    ~ReadStatements(){active_=previous_;for(auto& [sql,slot]:plans_)sqlite3_finalize(slot.statement);}
    ReadStatements(const ReadStatements&)=delete;
    ReadStatements& operator=(const ReadStatements&)=delete;
    static ReadStatements* current(sqlite3* db){return active_ && active_->db_==db?active_:nullptr;}
    Slot* slot(const char* sql){
        const auto found=plans_.find(sql);if(found!=plans_.end())return &found->second;
        if(plans_.size()>=64 || std::string_view(sql).size()>4096)return nullptr;
        return &plans_.try_emplace(sql).first->second;
    }
    sqlite3_stmt* take(Slot* slot){if(!slot)return nullptr;auto* result=slot->statement;cached_bytes_-=slot->bytes;slot->statement=nullptr;slot->bytes=0;return result;}
    bool put(Slot* slot,sqlite3_stmt* statement){
        if(!slot || slot->statement || !sqlite3_stmt_readonly(statement))return false;
        if(sqlite3_reset(statement)!=SQLITE_OK || sqlite3_clear_bindings(statement)!=SQLITE_OK)return false;
        const auto size=sqlite3_stmt_status(statement,SQLITE_STMTSTATUS_MEMUSED,0);
        if(size<0 || static_cast<std::size_t>(size)>4*1024*1024-cached_bytes_)return false;
        slot->statement=statement;slot->bytes=static_cast<std::size_t>(size);cached_bytes_+=slot->bytes;return true;
    }
private:
    sqlite3* db_;
    ReadStatements* previous_;
    std::map<std::string,Slot,std::less<>> plans_;
    std::size_t cached_bytes_=0;
    inline static thread_local ReadStatements* active_=nullptr;
};
class Statement {
public:
    Statement(sqlite3* db, const char* sql) {
        cache_=ReadStatements::current(db);if(cache_){slot_=cache_->slot(sql);stmt_=cache_->take(slot_);}
        if(!stmt_){const auto rc=sqlite3_prepare_v2(db,sql,-1,&stmt_,nullptr);if(rc!=SQLITE_OK){sqlite3_finalize(stmt_);fail(rc);}}
    }
    ~Statement() { if(!cache_ || !cache_->put(slot_,stmt_))sqlite3_finalize(stmt_); }
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
    ReadStatements* cache_=nullptr;
    ReadStatements::Slot* slot_=nullptr;
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
using ResolvedTransfers=std::set<std::tuple<std::string,std::size_t,std::string>>;
ResolvedTransfers resolved_transfers(const std::vector<app::TransferView>& values) {
    ResolvedTransfers result;
    auto add=[&](const transfers::Endpoint& e){result.emplace(e.entry.batch,e.entry.ordinal,e.allocation);};
    for(const auto& v:values)for(const auto* side:{&v.link.origin,&v.link.destination}) {
        if(side->state==transfers::State::removed)continue;
        add(side->endpoint);
        if(side->proposed_evidence)add(side->proposed_evidence->endpoint);
        if(side->attached_evidence)add(side->attached_evidence->endpoint);
    }
    return result;
}
void require_qif_transfers(const imports::Occurrence& id,const imports::DraftRow& draft,const ResolvedTransfers& resolved) {
    for(const auto& a:draft.transaction.allocations())if(qif::transfer_destination(a.category) && !resolved.contains({id.batch,id.ordinal,a.identity}))
        fail(SQLITE_CONSTRAINT,"Resolve every QIF transfer destination in Transfers before committing: choose an account or explicitly match an existing transfer");
}

#include "transfer_schema.inc"
#include "transfer_codec.inc"
#ifndef SPENTIT_RULES_STORAGE
constexpr const char* schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=21;
CREATE TABLE account(singleton INTEGER PRIMARY KEY CHECK(singleton>0),name TEXT NOT NULL,definition TEXT NOT NULL,
 opening_date TEXT NOT NULL,opening INTEGER NOT NULL,frozen INTEGER NOT NULL CHECK(frozen IN(0,1)),revision INTEGER NOT NULL CHECK(revision>=0),confirmed INTEGER NOT NULL CHECK(confirmed IN(0,1))) STRICT;
CREATE TABLE sources(id TEXT PRIMARY KEY,bytes BLOB NOT NULL) STRICT;
CREATE TABLE account_creations(identity INTEGER PRIMARY KEY REFERENCES account(singleton),name TEXT NOT NULL,definition TEXT NOT NULL,opening_date TEXT NOT NULL) STRICT;
CREATE TABLE batches(id TEXT PRIMARY KEY,profile INTEGER NOT NULL CHECK(profile IN(0,1)),parser TEXT NOT NULL,
 state INTEGER NOT NULL CHECK(state IN(0,1,2,3,4)),revision INTEGER NOT NULL CHECK(revision>=0),
 count INTEGER NOT NULL CHECK(count BETWEEN 0 AND 200000),blocked INTEGER NOT NULL CHECK(blocked IN(0,1)),materialized_revision INTEGER,account_id INTEGER NOT NULL REFERENCES account(singleton),source_id TEXT NOT NULL REFERENCES sources(id),source_account INTEGER,
 CHECK((materialized_revision IS NULL AND state IN(2,3) AND blocked=1) OR
 (materialized_revision IS NOT NULL AND materialized_revision BETWEEN 0 AND revision AND state IN(0,1,2,4)))) STRICT;
CREATE TABLE import_groups(id TEXT PRIMARY KEY REFERENCES sources(id),profile INTEGER NOT NULL CHECK(profile IN(0,1)),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_group_accounts(group_id TEXT NOT NULL REFERENCES import_groups(id),source_account INTEGER NOT NULL CHECK(source_account>=0),account_id INTEGER NOT NULL REFERENCES account(singleton),batch TEXT NOT NULL UNIQUE REFERENCES batches(id),created INTEGER NOT NULL CHECK(created IN(0,1)),balance_choice INTEGER NOT NULL DEFAULT 0 CHECK(balance_choice BETWEEN 0 AND 4),balance_revision INTEGER,balance_sequence INTEGER,reviewed_change INTEGER,opening INTEGER,opening_date TEXT,PRIMARY KEY(group_id,source_account),UNIQUE(group_id,account_id)) STRICT;
CREATE TABLE retired_import_accounts(account_id INTEGER PRIMARY KEY REFERENCES account(singleton),group_id TEXT NOT NULL REFERENCES import_groups(id)) STRICT;
CREATE TABLE import_group_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL REFERENCES import_groups(id),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_balance_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL,source_account INTEGER NOT NULL,balance_choice INTEGER NOT NULL CHECK(balance_choice BETWEEN 1 AND 4),balance_revision INTEGER NOT NULL,balance_sequence INTEGER NOT NULL,reviewed_change INTEGER NOT NULL,opening INTEGER NOT NULL,opening_date TEXT NOT NULL,FOREIGN KEY(group_id,source_account) REFERENCES import_group_accounts(group_id,source_account)) STRICT;
CREATE TRIGGER group_creation AFTER INSERT ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER group_transition BEFORE UPDATE ON import_groups WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NOT ((OLD.state=0 AND NEW.state IN(1,3)) OR (OLD.state=1 AND NEW.state IN(2,3))) BEGIN SELECT RAISE(ABORT,'Invalid group transition'); END;
CREATE TRIGGER group_transition_history AFTER UPDATE ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER balance_history AFTER UPDATE OF balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date ON import_group_accounts BEGIN INSERT INTO import_balance_events(group_id,source_account,balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date) VALUES(NEW.group_id,NEW.source_account,NEW.balance_choice,NEW.balance_revision,NEW.balance_sequence,NEW.reviewed_change,NEW.opening,NEW.opening_date); END;
CREATE TRIGGER group_events_immutable BEFORE UPDATE ON import_group_events BEGIN SELECT RAISE(ABORT,'Immutable group history'); END;
CREATE TRIGGER group_events_keep BEFORE DELETE ON import_group_events BEGIN SELECT RAISE(ABORT,'Retained group history'); END;
CREATE TRIGGER balance_events_immutable BEFORE UPDATE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Immutable balance history'); END;
CREATE TRIGGER balance_events_keep BEFORE DELETE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Retained balance history'); END;
CREATE TRIGGER retired_account_immutable BEFORE UPDATE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account identity is retained'); END;
CREATE TRIGGER retired_account_keep BEFORE DELETE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account cannot be resurrected'); END;
CREATE TRIGGER group_keep BEFORE DELETE ON import_groups BEGIN SELECT RAISE(ABORT,'Import group is retained'); END;
CREATE TRIGGER group_member_keep BEFORE DELETE ON import_group_accounts BEGIN SELECT RAISE(ABORT,'Import group member is retained'); END;
CREATE TRIGGER group_mapping_immutable BEFORE UPDATE ON import_group_accounts WHEN NEW.group_id!=OLD.group_id OR NEW.source_account!=OLD.source_account OR NEW.account_id!=OLD.account_id OR NEW.batch!=OLD.batch OR NEW.created!=OLD.created BEGIN SELECT RAISE(ABORT,'Import mapping is immutable'); END;
CREATE TRIGGER batch_account_immutable BEFORE UPDATE OF account_id ON batches WHEN OLD.account_id!=NEW.account_id BEGIN SELECT RAISE(ABORT,'Account ownership is immutable'); END;
CREATE TRIGGER account_creation_immutable BEFORE UPDATE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is immutable'); END;
CREATE TRIGGER account_creation_retained BEFORE DELETE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is retained'); END;
CREATE UNIQUE INDEX single_pending ON batches(account_id) WHERE state IN(0,3);
CREATE TABLE records(batch TEXT NOT NULL REFERENCES batches(id),ordinal INTEGER NOT NULL,sequence INTEGER NOT NULL UNIQUE CHECK(sequence>0),
 raw BLOB NOT NULL,source_date TEXT NOT NULL,original INTEGER,payee TEXT NOT NULL,reference TEXT NOT NULL,
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
 kind TEXT NOT NULL CHECK(kind IN('capture','materialize','ingest','save','decide','discard','retain','commit','opening','currency')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.source_id!=OLD.source_id OR NEW.source_account IS NOT OLD.source_account OR NEW.count!=OLD.count OR OLD.state NOT IN(0,3,4) OR NEW.revision!=OLD.revision+1 OR
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
CREATE TRIGGER frozen_opening BEFORE UPDATE ON account WHEN OLD.frozen=1 AND spentit_account_command()=0 AND
 (NEW.opening!=OLD.opening OR NEW.opening_date!=OLD.opening_date OR NEW.definition!=OLD.definition OR NEW.frozen!=1)
 BEGIN SELECT RAISE(ABORT,'frozen opening'); END;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER record_pending BEFORE INSERT ON records WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TABLE entry_revisions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),active INTEGER NOT NULL CHECK(active IN(0,1)),date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,total INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TABLE entry_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category TEXT NOT NULL,memo TEXT NOT NULL,units INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_heads(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE ledger_operations(sequence INTEGER PRIMARY KEY,id TEXT NOT NULL UNIQUE,kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 5),batch TEXT NOT NULL,ordinal INTEGER NOT NULL,prior_revision INTEGER NOT NULL,result_revision INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL,undo_of TEXT NOT NULL,preview_sequence INTEGER NOT NULL,catalog_revision INTEGER NOT NULL DEFAULT 0) STRICT;
CREATE TABLE change_dependencies(operation TEXT NOT NULL REFERENCES ledger_operations(id),batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,state INTEGER NOT NULL,PRIMARY KEY(operation,batch)) STRICT;
CREATE TABLE history_order(sequence INTEGER PRIMARY KEY,batch TEXT,event_revision INTEGER,operation TEXT,category_operation TEXT,account_operation TEXT,UNIQUE(batch,event_revision),UNIQUE(operation),UNIQUE(category_operation),UNIQUE(account_operation),FOREIGN KEY(batch,event_revision) REFERENCES events(batch,revision),FOREIGN KEY(operation) REFERENCES ledger_operations(id),FOREIGN KEY(category_operation) REFERENCES category_operations(id),FOREIGN KEY(account_operation) REFERENCES account_operations(id),CHECK((batch IS NOT NULL AND event_revision IS NOT NULL AND operation IS NULL AND category_operation IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NOT NULL AND category_operation IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NOT NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NULL AND account_operation IS NOT NULL))) STRICT;
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
CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL CHECK(domain IN(0,1,4))) STRICT;
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
#else
constexpr const char* schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=22;
CREATE TABLE account(singleton INTEGER PRIMARY KEY CHECK(singleton>0),name TEXT NOT NULL,definition TEXT NOT NULL,
 opening_date TEXT NOT NULL,opening INTEGER NOT NULL,frozen INTEGER NOT NULL CHECK(frozen IN(0,1)),revision INTEGER NOT NULL CHECK(revision>=0),confirmed INTEGER NOT NULL CHECK(confirmed IN(0,1))) STRICT;
CREATE TABLE sources(id TEXT PRIMARY KEY,bytes BLOB NOT NULL) STRICT;
CREATE TABLE account_creations(identity INTEGER PRIMARY KEY REFERENCES account(singleton),name TEXT NOT NULL,definition TEXT NOT NULL,opening_date TEXT NOT NULL) STRICT;
CREATE TABLE batches(id TEXT PRIMARY KEY,profile INTEGER NOT NULL CHECK(profile IN(0,1)),parser TEXT NOT NULL,
 state INTEGER NOT NULL CHECK(state IN(0,1,2,3,4)),revision INTEGER NOT NULL CHECK(revision>=0),
 count INTEGER NOT NULL CHECK(count BETWEEN 0 AND 200000),blocked INTEGER NOT NULL CHECK(blocked IN(0,1)),materialized_revision INTEGER,account_id INTEGER NOT NULL REFERENCES account(singleton),source_id TEXT NOT NULL REFERENCES sources(id),source_account INTEGER,
 CHECK((materialized_revision IS NULL AND state IN(2,3) AND blocked=1) OR
 (materialized_revision IS NOT NULL AND materialized_revision BETWEEN 0 AND revision AND state IN(0,1,2,4)))) STRICT;
CREATE TABLE import_groups(id TEXT PRIMARY KEY REFERENCES sources(id),profile INTEGER NOT NULL CHECK(profile IN(0,1)),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_group_accounts(group_id TEXT NOT NULL REFERENCES import_groups(id),source_account INTEGER NOT NULL CHECK(source_account>=0),account_id INTEGER NOT NULL REFERENCES account(singleton),batch TEXT NOT NULL UNIQUE REFERENCES batches(id),created INTEGER NOT NULL CHECK(created IN(0,1)),balance_choice INTEGER NOT NULL DEFAULT 0 CHECK(balance_choice BETWEEN 0 AND 4),balance_revision INTEGER,balance_sequence INTEGER,reviewed_change INTEGER,opening INTEGER,opening_date TEXT,PRIMARY KEY(group_id,source_account),UNIQUE(group_id,account_id)) STRICT;
CREATE TABLE retired_import_accounts(account_id INTEGER PRIMARY KEY REFERENCES account(singleton),group_id TEXT NOT NULL REFERENCES import_groups(id)) STRICT;
CREATE TABLE import_group_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL REFERENCES import_groups(id),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_balance_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL,source_account INTEGER NOT NULL,balance_choice INTEGER NOT NULL CHECK(balance_choice BETWEEN 1 AND 4),balance_revision INTEGER NOT NULL,balance_sequence INTEGER NOT NULL,reviewed_change INTEGER NOT NULL,opening INTEGER NOT NULL,opening_date TEXT NOT NULL,FOREIGN KEY(group_id,source_account) REFERENCES import_group_accounts(group_id,source_account)) STRICT;
CREATE TRIGGER group_creation AFTER INSERT ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER group_transition BEFORE UPDATE ON import_groups WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NOT ((OLD.state=0 AND NEW.state IN(1,3)) OR (OLD.state=1 AND NEW.state IN(2,3))) BEGIN SELECT RAISE(ABORT,'Invalid group transition'); END;
CREATE TRIGGER group_transition_history AFTER UPDATE ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER balance_history AFTER UPDATE OF balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date ON import_group_accounts BEGIN INSERT INTO import_balance_events(group_id,source_account,balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date) VALUES(NEW.group_id,NEW.source_account,NEW.balance_choice,NEW.balance_revision,NEW.balance_sequence,NEW.reviewed_change,NEW.opening,NEW.opening_date); END;
CREATE TRIGGER group_events_immutable BEFORE UPDATE ON import_group_events BEGIN SELECT RAISE(ABORT,'Immutable group history'); END;
CREATE TRIGGER group_events_keep BEFORE DELETE ON import_group_events BEGIN SELECT RAISE(ABORT,'Retained group history'); END;
CREATE TRIGGER balance_events_immutable BEFORE UPDATE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Immutable balance history'); END;
CREATE TRIGGER balance_events_keep BEFORE DELETE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Retained balance history'); END;
CREATE TRIGGER retired_account_immutable BEFORE UPDATE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account identity is retained'); END;
CREATE TRIGGER retired_account_keep BEFORE DELETE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account cannot be resurrected'); END;
CREATE TRIGGER group_keep BEFORE DELETE ON import_groups BEGIN SELECT RAISE(ABORT,'Import group is retained'); END;
CREATE TRIGGER group_member_keep BEFORE DELETE ON import_group_accounts BEGIN SELECT RAISE(ABORT,'Import group member is retained'); END;
CREATE TRIGGER group_mapping_immutable BEFORE UPDATE ON import_group_accounts WHEN NEW.group_id!=OLD.group_id OR NEW.source_account!=OLD.source_account OR NEW.account_id!=OLD.account_id OR NEW.batch!=OLD.batch OR NEW.created!=OLD.created BEGIN SELECT RAISE(ABORT,'Import mapping is immutable'); END;
CREATE TRIGGER batch_account_immutable BEFORE UPDATE OF account_id ON batches WHEN OLD.account_id!=NEW.account_id BEGIN SELECT RAISE(ABORT,'Account ownership is immutable'); END;
CREATE TRIGGER account_creation_immutable BEFORE UPDATE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is immutable'); END;
CREATE TRIGGER account_creation_retained BEFORE DELETE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is retained'); END;
CREATE UNIQUE INDEX single_pending ON batches(account_id) WHERE state IN(0,3);
CREATE TABLE records(batch TEXT NOT NULL REFERENCES batches(id),ordinal INTEGER NOT NULL,sequence INTEGER NOT NULL UNIQUE CHECK(sequence>0),
 raw BLOB NOT NULL,source_date TEXT NOT NULL,original INTEGER,payee TEXT NOT NULL,reference TEXT NOT NULL,
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
 kind TEXT NOT NULL CHECK(kind IN('capture','materialize','ingest','save','decide','discard','retain','commit','opening','apply_rules','currency')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.source_id!=OLD.source_id OR NEW.source_account IS NOT OLD.source_account OR NEW.count!=OLD.count OR OLD.state NOT IN(0,3,4) OR NEW.revision!=OLD.revision+1 OR
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
CREATE TRIGGER frozen_opening BEFORE UPDATE ON account WHEN OLD.frozen=1 AND spentit_account_command()=0 AND
 (NEW.opening!=OLD.opening OR NEW.opening_date!=OLD.opening_date OR NEW.definition!=OLD.definition OR NEW.frozen!=1)
 BEGIN SELECT RAISE(ABORT,'frozen opening'); END;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER record_pending BEFORE INSERT ON records WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TABLE entry_revisions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),active INTEGER NOT NULL CHECK(active IN(0,1)),date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,total INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TABLE entry_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category TEXT NOT NULL,memo TEXT NOT NULL,units INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_heads(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE ledger_operations(sequence INTEGER PRIMARY KEY,id TEXT NOT NULL UNIQUE,kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 5),batch TEXT NOT NULL,ordinal INTEGER NOT NULL,prior_revision INTEGER NOT NULL,result_revision INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL,undo_of TEXT NOT NULL,preview_sequence INTEGER NOT NULL,catalog_revision INTEGER NOT NULL DEFAULT 0) STRICT;
CREATE TABLE change_dependencies(operation TEXT NOT NULL REFERENCES ledger_operations(id),batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,state INTEGER NOT NULL,PRIMARY KEY(operation,batch)) STRICT;
CREATE TABLE rule_state(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision INTEGER NOT NULL CHECK(revision>=0)) STRICT;
INSERT INTO rule_state VALUES(1,0);
CREATE TRIGGER rule_state_step BEFORE UPDATE ON rule_state WHEN NEW.singleton<>1 OR NEW.revision<>OLD.revision+1 BEGIN SELECT RAISE(ABORT,'invalid ruleset revision'); END;
CREATE TRIGGER rule_state_keep BEFORE DELETE ON rule_state BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_operations(id TEXT PRIMARY KEY,ruleset_before INTEGER NOT NULL,ruleset_after INTEGER NOT NULL UNIQUE,CHECK(ruleset_after=ruleset_before+1)) STRICT;
CREATE TRIGGER rule_operations_immutable BEFORE UPDATE ON rule_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_operations_keep BEFORE DELETE ON rule_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_applications(id TEXT PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),batch_before INTEGER NOT NULL,batch_after INTEGER NOT NULL,ruleset_revision INTEGER NOT NULL CHECK(ruleset_revision>=0),catalog_revision INTEGER NOT NULL CHECK(catalog_revision>=0),replace_manual INTEGER NOT NULL CHECK(replace_manual IN(0,1)),evaluated INTEGER NOT NULL CHECK(evaluated>=0),changed_count INTEGER NOT NULL CHECK(changed_count>=0 AND changed_count<=evaluated),CHECK((changed_count=0 AND batch_after=batch_before) OR (changed_count>0 AND batch_after=batch_before+1))) STRICT;
CREATE TRIGGER rule_applications_immutable BEFORE UPDATE ON rule_applications BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_applications_keep BEFORE DELETE ON rule_applications BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_applications_revision BEFORE INSERT ON rule_applications WHEN (NEW.ruleset_revision>0 AND NOT EXISTS(SELECT 1 FROM rule_operations WHERE ruleset_after=NEW.ruleset_revision)) OR (NEW.ruleset_revision=0 AND (SELECT revision FROM rule_state WHERE singleton=1)!=0) BEGIN SELECT RAISE(ABORT,'invalid ruleset revision'); END;
CREATE TABLE history_order(sequence INTEGER PRIMARY KEY,batch TEXT,event_revision INTEGER,operation TEXT,category_operation TEXT,rule_operation TEXT,rule_application TEXT,account_operation TEXT,UNIQUE(batch,event_revision),UNIQUE(operation),UNIQUE(category_operation),UNIQUE(rule_operation),UNIQUE(rule_application),UNIQUE(account_operation),FOREIGN KEY(batch,event_revision) REFERENCES events(batch,revision),FOREIGN KEY(operation) REFERENCES ledger_operations(id),FOREIGN KEY(category_operation) REFERENCES category_operations(id),FOREIGN KEY(rule_operation) REFERENCES rule_operations(id),FOREIGN KEY(rule_application) REFERENCES rule_applications(id),FOREIGN KEY(account_operation) REFERENCES account_operations(id),CHECK((batch IS NOT NULL AND event_revision IS NOT NULL AND operation IS NULL AND category_operation IS NULL AND rule_operation IS NULL AND rule_application IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NOT NULL AND category_operation IS NULL AND rule_operation IS NULL AND rule_application IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NOT NULL AND rule_operation IS NULL AND rule_application IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NULL AND rule_operation IS NOT NULL AND rule_application IS NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NULL AND rule_operation IS NULL AND rule_application IS NOT NULL AND account_operation IS NULL) OR (batch IS NULL AND event_revision IS NULL AND operation IS NULL AND category_operation IS NULL AND rule_operation IS NULL AND rule_application IS NULL AND account_operation IS NOT NULL))) STRICT;
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
CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL CHECK(domain IN(0,1,2,3,4))) STRICT;
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
CREATE TABLE rule_definitions(id TEXT NOT NULL,version INTEGER NOT NULL CHECK(version>=0),label TEXT NOT NULL CHECK(length(label)<=256),scope INTEGER NOT NULL CHECK(scope IN(0,1)),input_field INTEGER NOT NULL CHECK(input_field IN(0,1,2)),expression TEXT NOT NULL CHECK(length(expression)<=4096),payee_template TEXT CHECK(payee_template IS NULL OR length(payee_template)<=65536),memo_template TEXT CHECK(memo_template IS NULL OR length(memo_template)<=65536),category_id TEXT CHECK(category_id IS NULL OR (length(category_id) BETWEEN 1 AND 256)),inherit_parent INTEGER NOT NULL CHECK(inherit_parent IN(0,1)),PRIMARY KEY(id,version),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((payee_template IS NOT NULL) OR (memo_template IS NOT NULL) OR (category_id IS NOT NULL) OR (inherit_parent=1)),CHECK(inherit_parent=0 OR (scope=1 AND category_id IS NULL AND payee_template IS NULL)),CHECK(scope<>1 OR payee_template IS NULL)) STRICT;
CREATE TRIGGER rule_definitions_immutable BEFORE UPDATE ON rule_definitions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_definitions_keep BEFORE DELETE ON rule_definitions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_heads(id TEXT PRIMARY KEY,version INTEGER NOT NULL,position INTEGER,enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),deleted INTEGER NOT NULL CHECK(deleted IN(0,1)),FOREIGN KEY(id,version) REFERENCES rule_definitions(id,version),CHECK((deleted=1 AND position IS NULL AND enabled=0) OR (deleted=0 AND position IS NOT NULL AND position>=0))) STRICT;
CREATE UNIQUE INDEX rule_head_position ON rule_heads(position) WHERE deleted=0;
CREATE TRIGGER rule_heads_update BEFORE UPDATE ON rule_heads WHEN NEW.id<>OLD.id OR OLD.deleted=1 OR NEW.version<>OLD.version AND NEW.version<>OLD.version+1 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_heads_keep BEFORE DELETE ON rule_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_operation_requests(operation TEXT NOT NULL REFERENCES rule_operations(id),position INTEGER NOT NULL,rule TEXT NOT NULL,label TEXT NOT NULL CHECK(length(label)<=256),scope INTEGER NOT NULL CHECK(scope IN(0,1)),input_field INTEGER NOT NULL CHECK(input_field IN(0,1,2)),expression TEXT NOT NULL CHECK(length(expression)<=4096),payee_template TEXT CHECK(payee_template IS NULL OR length(payee_template)<=65536),memo_template TEXT CHECK(memo_template IS NULL OR length(memo_template)<=65536),category_id TEXT CHECK(category_id IS NULL OR (length(category_id) BETWEEN 1 AND 256)),inherit_parent INTEGER NOT NULL CHECK(inherit_parent IN(0,1)),enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),PRIMARY KEY(operation,position),UNIQUE(operation,rule),CHECK((payee_template IS NOT NULL) OR (memo_template IS NOT NULL) OR (category_id IS NOT NULL) OR (inherit_parent=1)),CHECK(inherit_parent=0 OR (scope=1 AND category_id IS NULL AND payee_template IS NULL)),CHECK(scope<>1 OR payee_template IS NULL)) STRICT;
CREATE TRIGGER rule_operation_requests_immutable BEFORE UPDATE ON rule_operation_requests BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_operation_requests_keep BEFORE DELETE ON rule_operation_requests BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_snapshots(ruleset_revision INTEGER NOT NULL REFERENCES rule_operations(ruleset_after),position INTEGER NOT NULL,rule TEXT NOT NULL,version INTEGER NOT NULL,enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),PRIMARY KEY(ruleset_revision,position),UNIQUE(ruleset_revision,rule),FOREIGN KEY(rule,version) REFERENCES rule_definitions(id,version)) STRICT;
CREATE TRIGGER rule_snapshots_immutable BEFORE UPDATE ON rule_snapshots BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_snapshots_keep BEFORE DELETE ON rule_snapshots BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE draft_rule_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_rule TEXT,payee_version INTEGER,memo_rule TEXT,memo_version INTEGER,ruleset_revision INTEGER NOT NULL CHECK(ruleset_revision>=1),PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES draft_fields(batch,ordinal,revision),FOREIGN KEY(payee_rule,payee_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(ruleset_revision) REFERENCES rule_operations(ruleset_after),CHECK((payee_rule IS NULL)=(payee_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (payee_version IS NULL OR payee_version>=0) AND (memo_version IS NULL OR memo_version>=0))) STRICT;
CREATE TRIGGER draft_rule_fields_immutable BEFORE UPDATE ON draft_rule_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_rule_fields_keep BEFORE DELETE ON draft_rule_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE draft_rule_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category_rule TEXT,category_version INTEGER,memo_rule TEXT,memo_version INTEGER,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES draft_allocation_meta(batch,ordinal,revision,position),FOREIGN KEY(category_rule,category_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),CHECK((category_rule IS NULL)=(category_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (category_version IS NULL OR category_version>=0) AND (memo_version IS NULL OR memo_version>=0) AND (category_rule IS NOT NULL OR memo_rule IS NOT NULL))) STRICT;
CREATE TRIGGER draft_rule_allocations_immutable BEFORE UPDATE ON draft_rule_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_rule_allocations_keep BEFORE DELETE ON draft_rule_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE entry_rule_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_rule TEXT,payee_version INTEGER,memo_rule TEXT,memo_version INTEGER,ruleset_revision INTEGER NOT NULL CHECK(ruleset_revision>=1),PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_fields(batch,ordinal,revision),FOREIGN KEY(payee_rule,payee_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(ruleset_revision) REFERENCES rule_operations(ruleset_after),CHECK((payee_rule IS NULL)=(payee_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (payee_version IS NULL OR payee_version>=0) AND (memo_version IS NULL OR memo_version>=0))) STRICT;
CREATE TRIGGER entry_rule_fields_immutable BEFORE UPDATE ON entry_rule_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_rule_fields_keep BEFORE DELETE ON entry_rule_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE entry_rule_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category_rule TEXT,category_version INTEGER,memo_rule TEXT,memo_version INTEGER,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES entry_allocation_meta(batch,ordinal,revision,position),FOREIGN KEY(category_rule,category_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),CHECK((category_rule IS NULL)=(category_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (category_version IS NULL OR category_version>=0) AND (memo_version IS NULL OR memo_version>=0) AND (category_rule IS NOT NULL OR memo_rule IS NOT NULL))) STRICT;
CREATE TRIGGER entry_rule_allocations_immutable BEFORE UPDATE ON entry_rule_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_rule_allocations_keep BEFORE DELETE ON entry_rule_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER register_rule_save BEFORE INSERT ON rule_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,2); END;
CREATE TRIGGER order_rule_save AFTER INSERT ON rule_operations BEGIN INSERT INTO history_order(rule_operation) VALUES(NEW.id); END;
CREATE TRIGGER register_rule_apply BEFORE INSERT ON rule_applications BEGIN INSERT INTO operation_registry VALUES(NEW.id,3); END;
CREATE TRIGGER order_rule_apply AFTER INSERT ON rule_applications BEGIN INSERT INTO history_order(rule_application) VALUES(NEW.id); END;
CREATE TABLE bulk_operations(id TEXT PRIMARY KEY REFERENCES ledger_operations(id),kind INTEGER NOT NULL CHECK(kind IN(1,2,3)),category TEXT,undo_of TEXT REFERENCES bulk_operations(id),preview_sequence INTEGER NOT NULL CHECK(preview_sequence>=0),catalog_revision INTEGER NOT NULL CHECK(catalog_revision>=0),count INTEGER NOT NULL CHECK(count BETWEEN 1 AND 4096),CHECK((kind=1 AND category IS NOT NULL AND undo_of IS NULL) OR (kind=2 AND category IS NULL AND undo_of IS NULL) OR (kind=3 AND category IS NULL AND undo_of IS NOT NULL))) STRICT;
CREATE TABLE bulk_members(operation TEXT NOT NULL REFERENCES bulk_operations(id),position INTEGER NOT NULL CHECK(position>=0),entry_operation TEXT NOT NULL UNIQUE REFERENCES ledger_operations(id),PRIMARY KEY(operation,position)) STRICT;
CREATE TRIGGER bulk_operations_immutable BEFORE UPDATE ON bulk_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER bulk_operations_keep BEFORE DELETE ON bulk_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER bulk_members_immutable BEFORE UPDATE ON bulk_members BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER bulk_members_keep BEFORE DELETE ON bulk_members BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_reapplications(id TEXT PRIMARY KEY REFERENCES ledger_operations(id),account INTEGER NOT NULL REFERENCES account(singleton),ruleset_revision INTEGER NOT NULL CHECK(ruleset_revision>=0),catalog_revision INTEGER NOT NULL CHECK(catalog_revision>=0),replace_manual INTEGER NOT NULL CHECK(replace_manual IN(0,1)),evaluated INTEGER NOT NULL CHECK(evaluated BETWEEN 0 AND 4096),changed_count INTEGER NOT NULL CHECK(changed_count BETWEEN 1 AND 4096 AND changed_count<=evaluated),preview_sequence INTEGER NOT NULL CHECK(preview_sequence>=0),undo_of TEXT REFERENCES rule_reapplications(id)) STRICT;
CREATE TRIGGER rule_reapplications_immutable BEFORE UPDATE ON rule_reapplications BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_reapplications_keep BEFORE DELETE ON rule_reapplications BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_reapplication_members(operation TEXT NOT NULL REFERENCES rule_reapplications(id),position INTEGER NOT NULL CHECK(position>=0),entry_operation TEXT NOT NULL UNIQUE REFERENCES ledger_operations(id),PRIMARY KEY(operation,position)) STRICT;
CREATE TRIGGER rule_reapplication_members_immutable BEFORE UPDATE ON rule_reapplication_members BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_reapplication_members_keep BEFORE DELETE ON rule_reapplication_members BEGIN SELECT RAISE(ABORT,'immutable'); END;
)SQL";
#endif
constexpr const char* source_fact_schema = R"SQL(
CREATE TABLE source_headers(source_id TEXT PRIMARY KEY REFERENCES sources(id),format TEXT NOT NULL CHECK(format IN('qif','ofx','qfx','qbo','aso')),grammar TEXT NOT NULL CHECK(grammar IN('qif','ofx-sgml','ofx-xml')),header TEXT NOT NULL) STRICT;
CREATE TABLE source_occurrences(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,format TEXT NOT NULL CHECK(format IN('qif','ofx','qfx','qbo','aso')),grammar TEXT NOT NULL CHECK(grammar IN('qif','ofx-sgml','ofx-xml')),begin_offset INTEGER NOT NULL,end_offset INTEGER NOT NULL,fitid TEXT NOT NULL,source_account_key TEXT NOT NULL,time_evidence TEXT NOT NULL,trntype TEXT NOT NULL,currency TEXT NOT NULL,original_units INTEGER,original_date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,reference TEXT NOT NULL,uninterpreted TEXT NOT NULL,diagnostic INTEGER,blocked INTEGER NOT NULL CHECK(blocked IN(0,1)),PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch) REFERENCES batches(id)) STRICT;
CREATE TABLE source_associations(ledger_batch TEXT NOT NULL,ledger_ordinal INTEGER NOT NULL,source_batch TEXT NOT NULL,source_ordinal INTEGER NOT NULL,PRIMARY KEY(ledger_batch,ledger_ordinal,source_batch,source_ordinal),FOREIGN KEY(ledger_batch,ledger_ordinal) REFERENCES records(batch,ordinal),FOREIGN KEY(source_batch,source_ordinal) REFERENCES source_occurrences(batch,ordinal)) STRICT;
CREATE TRIGGER source_headers_immutable BEFORE UPDATE ON source_headers BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_headers_keep BEFORE DELETE ON source_headers BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_occurrences_immutable BEFORE UPDATE ON source_occurrences BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_occurrences_keep BEFORE DELETE ON source_occurrences BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_associations_immutable BEFORE UPDATE ON source_associations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_associations_keep BEFORE DELETE ON source_associations BEGIN SELECT RAISE(ABORT,'immutable'); END;
)SQL";
constexpr const char* account_command_schema = R"SQL(
CREATE TABLE account_status(identity INTEGER PRIMARY KEY REFERENCES account(singleton),archived INTEGER NOT NULL CHECK(archived IN(0,1))) STRICT;
CREATE TABLE account_retirements(account_id INTEGER PRIMARY KEY REFERENCES account(singleton),operation TEXT NOT NULL) STRICT;
CREATE TABLE account_operations(id TEXT PRIMARY KEY,account INTEGER NOT NULL REFERENCES account(singleton),kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 6),name TEXT NOT NULL,prior_name TEXT NOT NULL,definition TEXT NOT NULL,prior_definition TEXT NOT NULL,opening INTEGER NOT NULL,prior_opening INTEGER NOT NULL,opening_date TEXT NOT NULL,prior_opening_date TEXT NOT NULL,archived INTEGER NOT NULL CHECK(archived IN(0,1)),prior_archived INTEGER NOT NULL CHECK(prior_archived IN(0,1)),retired INTEGER NOT NULL CHECK(retired IN(0,1)),undo_of TEXT NOT NULL,content TEXT NOT NULL,revision INTEGER NOT NULL,frozen INTEGER NOT NULL CHECK(frozen IN(0,1)),confirmed INTEGER NOT NULL CHECK(confirmed IN(0,1))) STRICT;
CREATE TABLE account_amount_revisions(operation TEXT NOT NULL REFERENCES account_operations(id),kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 4),batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,prior_units INTEGER NOT NULL,units INTEGER NOT NULL,PRIMARY KEY(operation,kind,batch,ordinal,revision,position)) STRICT;
CREATE TABLE batch_source_currency(batch TEXT PRIMARY KEY,definition TEXT NOT NULL) STRICT;
CREATE TRIGGER account_status_command BEFORE INSERT ON account_status WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account command required'); END;
CREATE TRIGGER account_status_update BEFORE UPDATE ON account_status WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account command required'); END;
CREATE TRIGGER account_status_keep BEFORE DELETE ON account_status BEGIN SELECT RAISE(ABORT,'Account status is retained'); END;
CREATE TRIGGER account_retirement_command BEFORE INSERT ON account_retirements WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account command required'); END;
CREATE TRIGGER account_retirement_immutable BEFORE UPDATE ON account_retirements BEGIN SELECT RAISE(ABORT,'Account retirement is permanent'); END;
CREATE TRIGGER account_retirement_keep BEFORE DELETE ON account_retirements BEGIN SELECT RAISE(ABORT,'Account retirement is permanent'); END;
CREATE TRIGGER account_operations_command BEFORE INSERT ON account_operations WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account command required'); END;
CREATE TRIGGER account_operations_immutable BEFORE UPDATE ON account_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER account_operations_keep BEFORE DELETE ON account_operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER account_amount_command BEFORE INSERT ON account_amount_revisions WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account command required'); END;
CREATE TRIGGER account_amount_immutable BEFORE UPDATE ON account_amount_revisions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER account_amount_keep BEFORE DELETE ON account_amount_revisions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER batch_source_currency_immutable BEFORE UPDATE ON batch_source_currency BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER batch_source_currency_keep BEFORE DELETE ON batch_source_currency BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER account_name_command BEFORE UPDATE OF name ON account WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account name requires an explicit command'); END;
CREATE TRIGGER account_definition_command BEFORE UPDATE OF definition ON account WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account currency requires an explicit command'); END;
CREATE TRIGGER register_account BEFORE INSERT ON account_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,4); END;
CREATE TRIGGER order_account AFTER INSERT ON account_operations BEGIN INSERT INTO history_order(account_operation) VALUES(NEW.id); END;
)SQL";
bool schema3_batch(sqlite3* db) {
    const auto version=scalar(db,"PRAGMA user_version");
    return version=="3" || version=="4" || version=="17" || version=="19" || version=="21"
#ifdef SPENTIT_RULES_STORAGE
        || version=="6" || version=="16" || version=="18" || version=="20" || version=="22"
#endif
        ;
}
bool review_stamp_schema(sqlite3* db) {
    const auto version=scalar(db,"PRAGMA user_version");
    return version=="4" || version=="17" || version=="19" || version=="21"
#ifdef SPENTIT_RULES_STORAGE
        || version=="6" || version=="16" || version=="18" || version=="20" || version=="22"
#endif
        ;
}
app::BatchInfo batch(sqlite3* db,std::string_view id) {
    Statement s(db,schema3_batch(db)?"SELECT id,profile,state,revision,count,blocked,materialized_revision IS NULL FROM batches WHERE id=?":"SELECT id,profile,state,revision,count,blocked,0 FROM batches WHERE id=?"); s.text(1,id);
    if (!s.row()) fail(SQLITE_NOTFOUND,"Import does not exist");
    return {s.bytes(0),static_cast<qif::DateOrder>(s.integer(1)),static_cast<app::BatchState>(s.integer(2)),s.integer(3),
        static_cast<std::size_t>(s.integer(4)),s.integer(5)!=0,s.integer(6)!=0};
}
bool retained_batch(sqlite3* db,std::string_view id){Statement q(db,"SELECT 1 FROM retained_transfer_rows WHERE batch=? LIMIT 1");q.text(1,id);return q.row();}
bool retained_member(sqlite3* db,std::string_view id,std::size_t ordinal){if(!retained_batch(db,id))return true;Statement q(db,"SELECT 1 FROM retained_transfer_rows WHERE batch=? AND ordinal=?");q.text(1,id);q.integer(2,integer(ordinal));return q.row();}
void expect(sqlite3* db,std::string_view id,std::int64_t revision) {
    const auto b=batch(db,id);
    if ((b.state!=app::BatchState::pending && b.state!=app::BatchState::protected_drafts) || b.revision!=revision) fail(SQLITE_CONSTRAINT,"Import changed; refresh before retrying");
}
void advance(sqlite3* db,std::string_view id,std::int64_t revision,const char* kind,int state=0,int blocked=-1) {
    const auto current=batch(db,id);
    if(current.revision!=revision || (current.state!=app::BatchState::pending && current.state!=app::BatchState::protected_drafts &&
       !(current.state==app::BatchState::captured && (std::string_view(kind)=="opening" || std::string_view(kind)=="discard"))))
        fail(SQLITE_CONSTRAINT,"Import changed; refresh before retrying");
    if (revision==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    Statement update(db,"UPDATE batches SET revision=revision+1,state=?,blocked=CASE WHEN ?<0 THEN blocked ELSE ? END WHERE id=?"); update.integer(1,state); update.integer(2,blocked); update.integer(3,blocked); update.text(4,id); update.done();
    Statement event(db,"INSERT INTO events(batch,revision,kind) VALUES(?,?,?)"); event.text(1,id); event.integer(2,revision+1); event.text(3,kind); event.done();
}
void save_metadata(sqlite3*,const imports::Occurrence&,std::int64_t,const imports::DraftRow&,bool,std::optional<std::int64_t> = {});
void version(sqlite3* db,const imports::Occurrence& id,std::int64_t rev,const imports::DraftRow& row,const imports::Decision& decision,bool fresh_review=true,std::optional<std::int64_t> ruleset=std::nullopt) {
    Statement s(db,"INSERT INTO versions VALUES(?,?,?,?,?,?,?,?,?,?)");
    s.text(1,id.batch); s.integer(2,integer(id.ordinal)); s.integer(3,rev); s.text(4,row.date); s.text(5,row.payee); s.text(6,row.memo);
    s.integer(7,row.transaction.effective_total().units()); s.integer(8,static_cast<int>(decision.disposition));
    s.text(9,decision.duplicate_of ? decision.duplicate_of->batch : "");
    s.integer(10,decision.duplicate_of ? integer(decision.duplicate_of->ordinal) : -1); s.done();
    if(review_stamp_schema(db) && decision.duplicate_of) {
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
    }    save_metadata(db,id,rev,row,false,ruleset);
}

void load_metadata(sqlite3*,const imports::Occurrence&,std::int64_t,imports::DraftRow&,bool);
bool categories_format(sqlite3* db){const auto version=scalar(db,"PRAGMA user_version");return version=="17" || version=="19" || version=="21"
#ifdef SPENTIT_RULES_STORAGE
    || version=="6" || version=="16" || version=="18" || version=="20" || version=="22"
#endif
    ;}
bool rules_format(sqlite3* db){
#ifdef SPENTIT_RULES_STORAGE
    const auto version=scalar(db,"PRAGMA user_version");return version=="6" || version=="16" || version=="18" || version=="20" || version=="22";
#else
    (void)db;return false;
#endif
}
void retain_source_header(sqlite3* db,std::string_view id,std::string_view format,std::string_view grammar,std::string_view header) {
    Statement row(db,"INSERT INTO source_headers VALUES(?,?,?,?)");row.text(1,id);row.text(2,format);row.text(3,grammar);row.text(4,header);row.done();
}
void bind_candidate(Statement& statement,const qif::Record& record) {
    statement.text(1,record.date);
    if(record.original_amount) statement.integer(3,record.original_amount->units());
    statement.text(4,record.fitid);
    statement.text(5,record.format.empty()?"qif":record.format);
}
constexpr const char* candidate_where =
    "b.account_id=spentit_account() AND b.state=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=1 "
    "AND ((?4<>'' AND EXISTS(SELECT 1 FROM source_associations fa JOIN source_occurrences fo ON fo.batch=fa.source_batch AND fo.ordinal=fa.source_ordinal WHERE fa.ledger_batch=r.batch AND fa.ledger_ordinal=r.ordinal AND ifnull(fo.fitid,'')=?4)) "
    "OR (NOT (?4<>'' AND EXISTS(SELECT 1 FROM source_associations fa JOIN source_occurrences fo ON fo.batch=fa.source_batch AND fo.ordinal=fa.source_ordinal WHERE fa.ledger_batch=r.batch AND fa.ledger_ordinal=r.ordinal AND ifnull(fo.fitid,'')<>'' AND ifnull(fo.fitid,'')<>?4)) "
    "AND (r.original=?3 OR r.original IS NULL OR ?3 IS NULL) AND ((r.source_date=?1 OR r.source_date='' OR ?1='') "
    "OR (?3 IS NOT NULL AND r.original=?3 AND r.source_date<>'' AND ?1<>'' AND r.source_date<>?1 AND (?5<>'qif' OR ifnull(o.format,'qif')<>'qif')))))";
void write_occurrence(sqlite3* db,std::string_view batch,std::size_t ordinal,const qif::Record& record) {
    if(!record.source_currency) fail(SQLITE_CORRUPT,"Source currency is missing");
    Statement row(db,"INSERT INTO source_occurrences VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    row.text(1,batch);row.integer(2,integer(ordinal));row.text(3,record.format.empty()?"qif":record.format);row.text(4,record.grammar.empty()?"qif":record.grammar);
    row.integer(5,integer(record.begin));row.integer(6,integer(record.end));row.text(7,record.fitid);row.text(8,record.source_account_key);
    row.text(9,record.time_evidence);row.text(10,record.trntype);row.text(11,record.source_currency->definition_id());
    if(record.original_amount) row.integer(12,record.original_amount->units());
    row.text(13,record.date);row.text(14,record.payee);row.text(15,record.memo);row.text(16,record.reference);row.text(17,record.uninterpreted);
    if(record.diagnostic) row.integer(18,static_cast<int>(record.diagnostic->issue));
    row.integer(19,record.diagnostic && !record.repairable?1:0);row.done();
}
void overlay_occurrence(sqlite3* db,const imports::Occurrence& id,qif::Record& source) {
    Statement row(db,"SELECT format,grammar,fitid,source_account_key,time_evidence,trntype,uninterpreted FROM source_occurrences WHERE batch=? AND ordinal=?");
    row.text(1,id.batch);row.integer(2,integer(id.ordinal));
    if(!row.row() || row.bytes(2)!=source.fitid) fail(SQLITE_CORRUPT,"Source occurrence does not match its record");
    source.format=row.bytes(0);source.grammar=row.bytes(1);source.fitid=row.bytes(2);source.source_account_key=row.bytes(3);
    source.time_evidence=row.bytes(4);source.trntype=row.bytes(5);source.uninterpreted=row.bytes(6);
}
statements::Index indexed_source(sqlite3* db,std::string_view source_id,std::string_view bytes,const std::function<void()>& checkpoint={}) {
    Statement header(db,"SELECT format FROM source_headers WHERE source_id=?");header.text(1,source_id);
    if(!header.row()) fail(SQLITE_CORRUPT,"Source header is missing");
    auto index=statements::inspect(bytes,header.bytes(0),checkpoint);
    if(!index.admissible || !index.refusal.empty() || index.diagnostic) fail(SQLITE_CORRUPT,"Invalid grouped source structure");
    return index;
}
std::string source_header_text(const statements::Index& index) {
    std::string header=index.header;
    if(!index.notes.empty()) {
        if(!header.empty() && header.back()!='\n') header.push_back('\n');
        header+=index.notes;
    }
    return header;
}
void verify_source_facts(sqlite3* db,const app::BatchInfo& b,const qif::Result& parsed,bool materialized) {
    Statement occurrences(db,"SELECT count(*) FROM source_occurrences WHERE batch=?");occurrences.text(1,b.id);if(!occurrences.row()) fail(SQLITE_CORRUPT);
    Statement associations(db,"SELECT count(*) FROM source_associations WHERE source_batch=?");associations.text(1,b.id);if(!associations.row()) fail(SQLITE_CORRUPT);
    if(!materialized) {
        if(occurrences.integer(0)!=0 || associations.integer(0)!=0) fail(SQLITE_CORRUPT,"Source facts exist before interpretation");
        return;
    }
    if(occurrences.integer(0)!=integer(parsed.records.size())) fail(SQLITE_CORRUPT,"Source occurrence does not match its record");
    if(b.state!=app::BatchState::committed && associations.integer(0)!=0) fail(SQLITE_CORRUPT,"Source association exists before commit");
    for(std::size_t i=0;i<parsed.records.size();++i) {
        const auto& source=parsed.records[i];
        Statement row(db,"SELECT format,grammar,begin_offset,end_offset,fitid,source_account_key,time_evidence,trntype,currency,original_units,original_date,payee,memo,reference,uninterpreted,diagnostic,blocked FROM source_occurrences WHERE batch=? AND ordinal=?");
        row.text(1,b.id);row.integer(2,integer(i));if(!row.row() || !source.source_currency) fail(SQLITE_CORRUPT,"Source occurrence is missing");
        const auto format=source.format.empty()?"qif":source.format;const auto grammar=source.grammar.empty()?"qif":source.grammar;
        const bool same=row.bytes(0)==format && row.bytes(1)==grammar && row.integer(2)==integer(source.begin) && row.integer(3)==integer(source.end) && row.bytes(4)==source.fitid && row.bytes(5)==source.source_account_key && row.bytes(6)==source.time_evidence && row.bytes(7)==source.trntype && row.bytes(8)==source.source_currency->definition_id() && row.null(9)==!source.original_amount && (!source.original_amount || row.integer(9)==source.original_amount->units()) && row.bytes(10)==source.date && row.bytes(11)==source.payee && row.bytes(12)==source.memo && row.bytes(13)==source.reference && row.bytes(14)==source.uninterpreted && row.null(15)==!source.diagnostic && (!source.diagnostic || row.integer(15)==static_cast<int>(source.diagnostic->issue)) && row.integer(16)==(source.diagnostic && !source.repairable?1:0);
        if(!same) fail(SQLITE_CORRUPT,"Source occurrence does not match its record");
        if(b.state!=app::BatchState::committed) continue;
        Statement links(db,"SELECT ledger_batch,ledger_ordinal FROM source_associations WHERE source_batch=? AND source_ordinal=?");links.text(1,b.id);links.integer(2,integer(i));
        const bool kept=retained_member(db,b.id,i);
        Statement decision(db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=?1 AND ordinal=?2 AND revision=(SELECT max(revision) FROM versions WHERE batch=?1 AND ordinal=?2)");
        decision.text(1,b.id);decision.integer(2,integer(i));if(!decision.row() || decision.integer(0)==0) fail(SQLITE_CORRUPT,"Committed source has no review decision");
        if(!kept) {if(links.row()) fail(SQLITE_CORRUPT,"Cleared source is associated with a ledger transaction");continue;}
        if(!links.row()) fail(SQLITE_CORRUPT,"Committed source has no evidence association");
        const auto ledger_batch=links.bytes(0);const auto ledger_ordinal=links.integer(1);
        if(links.row()) fail(SQLITE_CORRUPT,"Committed source has more than one evidence association");
        if(decision.integer(0)==1) {if(ledger_batch!=b.id || ledger_ordinal!=integer(i)) fail(SQLITE_CORRUPT,"Included source is not associated with its ledger transaction");}
        else if(decision.integer(0)==2) {if(ledger_batch!=decision.bytes(1) || ledger_ordinal!=decision.integer(2)) fail(SQLITE_CORRUPT,"Duplicate source is not associated with its ledger transaction");}
        else fail(SQLITE_CORRUPT,"Committed source has no evidence association");
    }
}

void store_entry(sqlite3*,const imports::Occurrence&,const app::EntrySnapshot&,std::optional<std::int64_t> = {});
void append_financial(sqlite3*,std::string_view,app::ChangeKind,const imports::Occurrence&,std::int64_t,std::int64_t,const Money&,const Money&,const Money&,std::string_view,std::int64_t,std::int64_t=0);
#include "account_money.inc"
}
struct ImportStore::Impl {
    sqlite3* db=nullptr;
    int lock=-1;
    std::int64_t selected_account=1;
    int account_command=0;
    bool verifying=false;
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
    ok(sqlite3_create_function_v2(db,"spentit_account",0,SQLITE_UTF8,impl.get(),
        [](sqlite3_context* ctx,int,sqlite3_value**){
            sqlite3_result_int64(ctx,static_cast<Impl*>(sqlite3_user_data(ctx))->selected_account);
        },nullptr,nullptr,nullptr));
    ok(sqlite3_create_function_v2(db,"spentit_account_command",0,SQLITE_UTF8,impl.get(),
        [](sqlite3_context* ctx,int,sqlite3_value**){
            sqlite3_result_int(ctx,static_cast<Impl*>(sqlite3_user_data(ctx))->account_command);
        },nullptr,nullptr,nullptr));
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
    if (!create) {
#ifdef SPENTIT_RULES_STORAGE
        constexpr const char* expected_format="22";
#else
        constexpr const char* expected_format="21";
#endif
        if(scalar(db,"PRAGMA application_id")!="1397770577" || scalar(db,"PRAGMA user_version")!=expected_format)
            fail(SQLITE_SCHEMA,"Unsupported experimental format; no migration was attempted");
        if(scalar(db,"SELECT count(*) FROM pragma_table_info('batches') WHERE name='account_id'")!="1" ||
           scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='account_creations'")!="1")
            fail(SQLITE_SCHEMA,"Account structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('transfer_events','transfer_versions','transfer_heads','retained_transfer_rows')")!="4")fail(SQLITE_SCHEMA,"Transfer structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('source_headers','source_occurrences','source_associations')")!="3")fail(SQLITE_SCHEMA,"Document structure does not match format version");
        const auto common=scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('account','account_creations','sources','batches','records','versions','allocations','operations','events','entry_revisions','entry_allocations','entry_heads','ledger_operations','change_dependencies','history_order','review_stamps','category_state','category_versions','category_heads','category_operations','operation_registry','draft_fields','entry_fields','draft_allocation_meta','entry_allocation_meta')");
        const auto rule_tables=scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('rule_state','rule_definitions','rule_heads','rule_operations','rule_operation_requests','rule_snapshots','rule_applications','draft_rule_fields','draft_rule_allocations','entry_rule_fields','entry_rule_allocations','rule_reapplications','rule_reapplication_members')");
        const auto bulk_tables=scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('bulk_operations','bulk_members')");
#ifdef SPENTIT_RULES_STORAGE
        if(common!="25" || rule_tables!="13" || bulk_tables!="2")
#else
        if(common!="25" || rule_tables!="0" || bulk_tables!="0")
#endif
            fail(SQLITE_SCHEMA,"Document structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('account_status','account_retirements','account_operations','account_amount_revisions','batch_source_currency')")!="5"
           || scalar(db,"SELECT count(*) FROM pragma_table_info('history_order') WHERE name='account_operation'")!="1")
            fail(SQLITE_SCHEMA,"Account structure does not match format version");
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
        exec(db, transfer_schema);
        exec(db, source_fact_schema);
        exec(db, account_command_schema);
        transaction.commit();
        sync_directory(path);
    }
    if (!create) {store.impl_->selected_account=smallest_active_account(store.impl_->db);store.verify(context);}
    return store;
}
ImportStore ImportStore::create(const std::filesystem::path& path,std::string_view password,std::string_view name,const Currency& currency,std::string_view today) {
    safe_text(name); if(name.empty()) fail(SQLITE_MISUSE,"Account name is required"); iso_date(today);
    // New accounts must use current selection; persisted definitions remain readable.
    if (Currency(currency.code())!=currency) fail(SQLITE_MISUSE,"Currency cannot be selected for a new account");
    auto store=connect(path,password,true);
    Atomic a(store.impl_->db);
    Statement s(store.impl_->db,"INSERT INTO account VALUES(1,?,?,?,0,0,0,0)"); s.text(1,name); s.text(2,currency.definition_id()); s.text(3,today); s.done();
    Statement receipt(store.impl_->db,"INSERT INTO account_creations VALUES(1,?,?,?)");
    receipt.text(1,name);receipt.text(2,currency.definition_id());receipt.text(3,today);receipt.done();
    a.commit(); store.verify(); return store;
}
ImportStore ImportStore::open(const std::filesystem::path& path,std::string_view password,const app::OperationContext* context) { return connect(path,password,false,context); }
app::Account ImportStore::account_by_id(std::int64_t id) const {
    Statement s(impl_->db,"SELECT name,definition,opening_date,opening,frozen,revision,confirmed FROM account WHERE singleton=?");
    s.integer(1,id);if (!s.row()) fail(SQLITE_NOTFOUND,"Account does not exist");
    const auto currency=Currency::from_definition(s.bytes(1));
    return {s.bytes(0),currency,s.bytes(2),Money(s.integer(3),currency),s.integer(4)!=0,s.integer(5),s.integer(6)!=0,id,account_row_archived(impl_->db,id)};
}
app::Account ImportStore::account() const { return account_by_id(impl_->selected_account); }
std::vector<app::Account> ImportStore::accounts() const {
    Statement ids(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM retired_import_accounts r WHERE r.account_id=singleton) AND NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=singleton) AND NOT EXISTS(SELECT 1 FROM account_status s WHERE s.identity=singleton AND s.archived=1) ORDER BY singleton");
    std::vector<app::Account> result;while(ids.row())result.push_back(account_by_id(ids.integer(0)));return result;
}
std::int64_t ImportStore::next_account_identity() const {
    Statement row(impl_->db,"SELECT max(singleton) FROM account");if(!row.row() || row.integer(0)==INT64_MAX)fail(SQLITE_FULL,"Account identities exhausted");return row.integer(0)+1;
}
void ImportStore::select_account(std::int64_t id) {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement retired(impl_->db,"SELECT 1 FROM retired_import_accounts WHERE account_id=?");retired.integer(1,id);if(retired.row())fail(SQLITE_CONSTRAINT,"Account was removed when its import was aborted");
    if(account_lifecycle_retired(impl_->db,id))fail(SQLITE_CONSTRAINT,"Account identity was retired");
    (void)account_by_id(id);impl_->selected_account=id;
}
app::Account ImportStore::account_for_batch(std::string_view id) const {
    Statement s(impl_->db,"SELECT account_id FROM batches WHERE id=?");s.text(1,id);
    if(!s.row())fail(SQLITE_NOTFOUND,"Import does not exist");return account_by_id(s.integer(0));
}
void ImportStore::require_account(std::string_view id) const {
    if(!impl_->verifying && account_for_batch(id).identity!=impl_->selected_account)
        fail(SQLITE_CONSTRAINT,"Import belongs to another account; refresh the account view");
}
std::optional<app::AccountCreation> ImportStore::account_creation_outcome(std::int64_t id) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT name,definition,opening_date FROM account_creations WHERE identity=?");row.integer(1,id);
    if(!row.row())return {};return app::AccountCreation{id,row.bytes(0),Currency::from_definition(row.bytes(1)),row.bytes(2)};
}
app::Account ImportStore::create_account(std::int64_t id,std::string_view name,const Currency& currency,std::string_view today) {
    if(id<=0)fail(SQLITE_MISUSE,"Invalid account identity");safe_text(name);iso_date(today);
    if(name.empty() || Currency(currency.code())!=currency)fail(SQLITE_MISUSE,"Invalid new account definition");
    Atomic transaction(impl_->db);
    Statement prior(impl_->db,"SELECT name,definition,opening_date FROM account_creations WHERE identity=?");prior.integer(1,id);
    if(prior.row()) {
        if(prior.bytes(0)!=name || prior.bytes(1)!=currency.definition_id() || prior.bytes(2)!=today)
            fail(SQLITE_CONSTRAINT,"Account identity was reused with different content");
        Statement retired(impl_->db,"SELECT 1 FROM retired_import_accounts WHERE account_id=?");retired.integer(1,id);if(retired.row())fail(SQLITE_CONSTRAINT,"Account creation belongs to an aborted import");
        if(account_lifecycle_retired(impl_->db,id))fail(SQLITE_CONSTRAINT,"Account identity was retired");
        transaction.commit();return account_by_id(id);
    }
    Statement row(impl_->db,"INSERT INTO account VALUES(?,?,?,?,0,0,0,0)");
    row.integer(1,id);row.text(2,name);row.text(3,currency.definition_id());row.text(4,today);row.done();
    Statement receipt(impl_->db,"INSERT INTO account_creations VALUES(?,?,?,?)");
    receipt.integer(1,id);receipt.text(2,name);receipt.text(3,currency.definition_id());receipt.text(4,today);receipt.done();
    impl_->finish(transaction);return account_by_id(id);
}
void ImportStore::opening(std::int64_t expected,std::string_view date,const Money& amount) {
    Statement grouped(impl_->db,"SELECT 1 FROM import_group_accounts m JOIN import_groups g ON g.id=m.group_id WHERE m.account_id=spentit_account() AND m.created=1 AND g.state IN(0,1,2)");if(grouped.row())fail(SQLITE_CONSTRAINT,"Use the import account balance review for this account");
    iso_date(date); const auto acct=account(); require_writable_account(impl_->db,acct.identity);
    if(explicit_account_money(impl_->db,acct.identity))fail(SQLITE_CONSTRAINT,"Use the explicit opening correction after an account command");
    Atomic a(impl_->db); acct.opening.require_compatible(amount);
    if(acct.frozen || acct.revision!=expected || expected==INT64_MAX) fail(SQLITE_CONSTRAINT,"Opening balance is frozen or changed");
    Statement s(impl_->db,"UPDATE account SET opening_date=?,opening=?,revision=revision+1,confirmed=1 WHERE singleton=spentit_account()");
    s.text(1,date); s.integer(2,amount.units()); s.done();
    for(const auto& b:batches()) if(b.state==app::BatchState::pending || b.state==app::BatchState::captured || b.state==app::BatchState::protected_drafts) advance(impl_->db,b.id,b.revision,"opening",static_cast<int>(b.state));
    impl_->finish(a);
}
std::vector<app::BatchInfo> ImportStore::batches_for(std::int64_t id) const {
    Statement s(impl_->db,"SELECT id FROM batches WHERE account_id=? ORDER BY rowid");s.integer(1,id);std::vector<app::BatchInfo> result;
    while(s.row()) result.push_back(batch(impl_->db,s.bytes(0)));return result;
}
std::vector<app::BatchInfo> ImportStore::batches() const {return batches_for(impl_->selected_account);}
std::string ImportStore::evidence(std::string_view id) const {
    require_account(id);
    Statement s(impl_->db,"SELECT bytes FROM sources JOIN batches ON sources.id=batches.source_id WHERE batches.id=?"); s.text(1,id); if(!s.row()) fail(SQLITE_NOTFOUND); return s.bytes(0);
}
qif::Result ImportStore::parsed_source(std::string_view id,const std::function<void()>& checkpoint) const {
    require_account(id);const auto b=batch(impl_->db,id);const auto currency=pinned_currency(impl_->db,id);
    const auto bytes=evidence(id);
    Statement association(impl_->db,"SELECT b.source_account,b.parser,h.format,h.grammar FROM batches b JOIN source_headers h ON h.source_id=b.source_id WHERE b.id=?");association.text(1,id);if(!association.row())fail(SQLITE_CORRUPT,"Source header is missing");
    if(association.bytes(1)==qif::parser_version || association.bytes(2)=="qif") {
        if(association.null(0))return qif::parse(bytes,b.profile,currency,checkpoint);
        const auto document=qif::index_document(bytes,checkpoint);
        return qif::parse_account(bytes,document,static_cast<std::size_t>(association.integer(0)),b.profile,currency,checkpoint);
    }
    if(association.null(0))fail(SQLITE_CORRUPT,"Statement import is missing its account mapping");
    statements::Index index;index.parser=association.bytes(1);index.format=association.bytes(2);index.grammar=association.bytes(3);index.explicit_accounts=true;
    return statements::parse_mapped(bytes,index,static_cast<std::size_t>(association.integer(0)),b.profile,currency,checkpoint);
}
app::ImportRepairProgress ImportStore::repair_progress(std::string_view id) const {
    const auto parsed=parsed_source(id);app::ImportRepairProgress result;if(parsed.diagnostic)++result.blocked;
    for(std::size_t i=0;i<parsed.records.size();++i){const auto& r=parsed.records[i];if(r.repairable){++result.total;if(r.amount_repair)++result.amounts;Statement saved(impl_->db,"SELECT 1 FROM records WHERE batch=? AND ordinal=?");saved.text(1,id);saved.integer(2,integer(i));if(saved.row())++result.done;}else if(r.diagnostic)++result.blocked;}
    return result;
}
std::string ImportStore::record_evidence(const imports::Occurrence& id) const {
    require_account(id.batch);
    Statement raw(impl_->db,"SELECT raw FROM records WHERE batch=? AND ordinal=?");
    raw.text(1,id.batch);raw.integer(2,integer(id.ordinal));if(raw.row())return raw.bytes(0);
    // Unsupported or not-yet-repaired records have no materialized row. Keep the
    // whole-file indexing fallback for those retained source occurrences.
    return app::ImportRepository::record_evidence(id);
}
app::RowView ImportStore::row(const imports::Occurrence& id) const { return load_row(id,true); }
app::RowView ImportStore::load_row(const imports::Occurrence& id,bool candidates) const {
    require_account(id.batch);const auto b=batch(impl_->db,id.batch); const auto source_currency=pinned_currency(impl_->db,id.batch);
    Statement src(impl_->db,"SELECT raw FROM records WHERE batch=? AND ordinal=?"); src.text(1,id.batch); src.integer(2,integer(id.ordinal));
    if(!src.row()) {
        auto parsed=parsed_source(id.batch);
        if(id.ordinal>=parsed.records.size()) fail(SQLITE_NOTFOUND);
        auto source=std::move(parsed.records[id.ordinal]);
        if(scalar(impl_->db,"PRAGMA user_version")=="1") source.repairable=false;
        return {id,std::move(source),{}, {imports::Disposition::unresolved,{}},{}};
    }
    Statement parser(impl_->db,"SELECT parser FROM batches WHERE id=?");parser.text(1,id.batch);if(!parser.row())fail(SQLITE_CORRUPT);
    const auto parsed=parser.bytes(0)==qif::parser_version ? qif::parse(src.bytes(0),b.profile,source_currency) : statements::parse_occurrence(src.bytes(0),parser.bytes(0),source_currency,b.profile);
    if(parsed.diagnostic || parsed.records.size()!=1 || (!parsed.records[0].transaction && !parsed.records[0].repairable)) fail(SQLITE_CORRUPT);
    auto source=parsed.records[0]; overlay_occurrence(impl_->db,id,source);
    if(source.fitid.empty()) {
        Statement fits(impl_->db,"SELECT DISTINCT fitid FROM source_associations a JOIN source_occurrences o ON o.batch=a.source_batch AND o.ordinal=a.source_ordinal WHERE a.ledger_batch=? AND a.ledger_ordinal=? AND ifnull(fitid,'')<>''");
        fits.text(1,id.batch);fits.integer(2,integer(id.ordinal));
        if(fits.row()) { source.associated_fitid=fits.bytes(0); if(fits.row()) source.associated_fitid.clear(); }
    }
    app::RowView out{id,std::move(source),{}, {},{}};
    out.excluded_by_clear=!retained_member(impl_->db,id.batch,id.ordinal);
    Statement v(impl_->db,"SELECT revision,date,payee,memo,total,decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? ORDER BY revision DESC LIMIT 1");
    v.text(1,id.batch); v.integer(2,integer(id.ordinal)); if(!v.row()) fail(SQLITE_CORRUPT);
    Statement allocations(impl_->db,"SELECT position,category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");
    allocations.text(1,id.batch); allocations.integer(2,integer(id.ordinal)); allocations.integer(3,v.integer(0));
    const auto draft_currency=version_currency(impl_->db,id.batch,integer(id.ordinal),v.integer(0));
    const bool reassigned=out.source.source_currency && *out.source.source_currency!=draft_currency;
    std::vector<Allocation> aa;
    while(allocations.row()) {
        if (allocations.integer(0)!=integer(aa.size()) || aa.size()>=qif::max_allocations) fail(SQLITE_CORRUPT);
        aa.push_back({allocations.bytes(1),allocations.bytes(2),Money(allocations.integer(3),draft_currency)});
    }
    out.draft=imports::DraftRow{v.bytes(1),v.bytes(2),v.bytes(3),Transaction::repaired_import(out.source.original_amount,Money(v.integer(4),draft_currency),std::move(aa),reassigned)};
    load_metadata(impl_->db,id,v.integer(0),*out.draft,false);
    imports::validate_draft(out.source,*out.draft,reassigned);
    out.decision.disposition=static_cast<imports::Disposition>(v.integer(5));
    if(out.decision.disposition==imports::Disposition::duplicate) out.decision.duplicate_of=imports::Occurrence{v.bytes(6),static_cast<std::size_t>(v.integer(7))};
    if(committed_changes_supported() && out.decision.duplicate_of) {
        Statement target(impl_->db,"SELECT s.target_revision,s.target_active,h.revision,e.active FROM review_stamps s JOIN entry_heads h ON h.batch=? AND h.ordinal=? JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE s.batch=? AND s.ordinal=? AND s.revision=?");
        target.text(1,out.decision.duplicate_of->batch);target.integer(2,integer(out.decision.duplicate_of->ordinal));target.text(3,id.batch);target.integer(4,integer(id.ordinal));target.integer(5,v.integer(0));
        if(!target.row())fail(SQLITE_CORRUPT);out.review_stale=target.integer(0)!=target.integer(2) || target.integer(1)!=target.integer(3);
    }
    if (!candidates) return out;
    const auto candidate_sql=std::string("SELECT r.batch,r.ordinal FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal LEFT JOIN source_occurrences o ON o.batch=r.batch AND o.ordinal=r.ordinal WHERE ")+candidate_where+" ORDER BY r.batch,r.ordinal LIMIT 4097";
    Statement c(impl_->db,candidate_sql.c_str());
    bind_candidate(c,out.source);
    while(c.row()) {if(out.candidates.size()==4096)fail(SQLITE_TOOBIG,"More than 4096 possible duplicate occurrences; review requires a narrower source import");out.candidates.push_back({c.bytes(0),static_cast<std::size_t>(c.integer(1))});}
    return out;
}
app::BatchInfo ImportStore::ingest(std::string_view id,std::string_view bytes,qif::DateOrder profile) {
    identifier(id); const auto acct=account(); require_writable_account(impl_->db,acct.identity); auto parsed=qif::parse(bytes,profile,acct.currency);
    if(!parsed.admissible) fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    Atomic a(impl_->db);
    Statement retry(impl_->db,"SELECT profile,bytes FROM batches JOIN sources ON sources.id=batches.source_id WHERE batches.id=?"); retry.text(1,id);
    if(retry.row()) {
        require_account(id);
        if(retry.integer(0)!=static_cast<int>(profile) || retry.bytes(1)!=bytes) fail(SQLITE_CONSTRAINT,"Import identity was reused with different content");
        auto result=batch(impl_->db,id); a.commit(); return result;
    }
    Statement pending(impl_->db,"SELECT batches.id,profile,bytes FROM batches JOIN sources ON sources.id=batches.source_id WHERE account_id=spentit_account() AND state IN(0,3)");
    if(pending.row()) {
        if(pending.integer(1)==static_cast<int>(profile) && pending.bytes(2)==bytes) {
            auto result=batch(impl_->db,pending.bytes(0)); a.commit(); return result;
        }
        fail(SQLITE_CONSTRAINT,"Finish or discard the pending import first");
    }
    Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)"); evidence.text(1,id); evidence.blob(2,bytes); evidence.done();
    retain_source_header(impl_->db,id,"qif","qif","");
    Statement insert(impl_->db,schema3_batch(impl_->db)?"INSERT INTO batches VALUES(?,?,?,0,0,?,?,0,spentit_account(),?1,NULL)":"INSERT INTO batches VALUES(?,?,?,0,0,?,?)");
    insert.text(1,id); insert.integer(2,static_cast<int>(profile)); insert.text(3,qif::parser_version);
    insert.integer(4,integer(parsed.records.size())); insert.integer(5,parsed.valid()?0:1); insert.done();
    pin_source_currency(impl_->db,id,acct.currency);
    populate(id,bytes,parsed,0,nullptr);
    Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'ingest')"); event.text(1,id); event.done();
    impl_->finish(a); return batch(impl_->db,id);
}
app::BatchInfo ImportStore::ingest_controlled(std::string_view id,std::string_view bytes,qif::DateOrder profile,app::OperationContext& context) {
    if(!schema3_batch(impl_->db)) {
        context.seal();context.checkpoint("Saving import evidence and drafts");return ingest(id,bytes,profile);
    }
    identifier(id);context.checkpoint("Checking QIF resource limits and format");
    const auto acct=account();require_writable_account(impl_->db,acct.identity);
    auto parsed=qif::parse(bytes,profile,acct.currency,[&]{context.checkpoint();});
    if(!parsed.admissible) fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    // Existing evidence is navigated to, never implicitly resumed.
    Statement prior(impl_->db,"SELECT batches.id,profile,bytes FROM batches JOIN sources ON sources.id=batches.source_id WHERE batches.id=? OR (account_id=spentit_account() AND state IN(0,3)) ORDER BY batches.id=? DESC");
    prior.text(1,id);prior.text(2,id);
    if(prior.row()) {
        require_account(prior.bytes(0));
        if(prior.integer(1)!=static_cast<int>(profile) || prior.bytes(2)!=bytes)
            fail(SQLITE_CONSTRAINT,"Import identity conflicts or another import needs review");
        context.seal();return batch(impl_->db,prior.bytes(0));
    }
    context.checkpoint("Retaining source evidence");
    try {
        Atomic transaction(impl_->db);
        Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)");evidence.text(1,id);evidence.blob(2,bytes);evidence.done();
        retain_source_header(impl_->db,id,"qif","qif","");
        Statement insert(impl_->db,"INSERT INTO batches VALUES(?,?,?,3,0,?,1,NULL,spentit_account(),?1,NULL)");
        insert.text(1,id);insert.integer(2,static_cast<int>(profile));insert.text(3,qif::parser_version);insert.integer(4,integer(parsed.records.size()));insert.done();
        pin_source_currency(impl_->db,id,acct.currency);
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
    if(!schema3_batch(impl_->db)) fail(SQLITE_SCHEMA,"Resuming evidence requires a new experimental dataset");
    require_account(id);require_writable_account(impl_->db,account_for_batch(id).identity);
    Statement grouped(impl_->db,"SELECT 1 FROM import_group_accounts WHERE batch=?");grouped.text(1,id);if(grouped.row())fail(SQLITE_CONSTRAINT,"Resume all source accounts together");
    if(expected<0)fail(SQLITE_CONSTRAINT,"Invalid import revision");
    context.checkpoint("Preparing draft interpretations");
    const auto current=batch(impl_->db,id);
    Statement metadata(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");metadata.text(1,id);if(!metadata.row())fail(SQLITE_NOTFOUND);
    if(!metadata.null(0) && expected<INT64_MAX && metadata.integer(0)==expected+1) {context.seal();return current;}
    if(current.state!=app::BatchState::captured || current.revision!=expected || expected==INT64_MAX)
        fail(SQLITE_CONSTRAINT,"Import changed; refresh before resuming");
    try {
        const auto bytes=evidence(id);
        const auto parsed=parsed_source(id,[&]{context.checkpoint();});
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
    Statement prior(impl_->db,"SELECT batches.id FROM batches JOIN sources ON sources.id=batches.source_id WHERE account_id=spentit_account() AND state=1 AND profile=? AND bytes=? AND source_account IS (SELECT source_account FROM batches WHERE id=?3) ORDER BY batches.rowid DESC LIMIT 1");
    prior.integer(1,static_cast<int>(profile)); prior.blob(2,bytes); prior.text(3,id); if(prior.row()) identical=prior.bytes(0);
    Statement sequence(impl_->db,"SELECT coalesce(max(sequence),0) FROM records"); if(!sequence.row()) fail(SQLITE_CORRUPT);
    auto seq=sequence.integer(0);
    std::map<std::pair<std::string,std::int64_t>,bool> candidate_groups;
    std::unordered_set<std::string> seen_fitids;
    const auto probe_sql=std::string("SELECT 1 FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal LEFT JOIN source_occurrences o ON o.batch=r.batch AND o.ordinal=r.ordinal WHERE ")+candidate_where+" LIMIT 1";
    for(std::size_t i=0;i<parsed.records.size();++i) {
        if(context) context->checkpoint();
        const auto& r=parsed.records[i];
        write_occurrence(impl_->db,id,i,r);
        if(r.diagnostic || !r.transaction) continue;
        if(seq==INT64_MAX) fail(SQLITE_FULL,"Entry identity capacity exhausted");
        Statement record(impl_->db,"INSERT INTO records VALUES(?,?,?,?,?,?,?,?)");
        record.text(1,id); record.integer(2,integer(i)); record.integer(3,++seq); record.blob(4,bytes.substr(r.begin,r.end-r.begin));
        record.text(5,r.date); record.integer(6,r.transaction->original_imported_amount()->units()); record.text(7,r.payee); record.text(8,r.reference); record.done();
        imports::Decision decision;
        if(!identical.empty()) {
            const auto original=load_row({identical,i},false);
            decision.disposition=imports::Disposition::duplicate;
            decision.duplicate_of=original.decision.duplicate_of ? original.decision.duplicate_of : std::optional(imports::Occurrence{identical,i});
        } else if(!r.fitid.empty()) {
            if(!seen_fitids.insert(r.fitid).second) decision.disposition=imports::Disposition::unresolved;
            else { Statement candidate(impl_->db,probe_sql.c_str()); bind_candidate(candidate,r); if(candidate.row()) decision.disposition=imports::Disposition::unresolved; }
        } else {
            const auto key=std::pair{r.date,r.transaction->original_imported_amount()->units()};
            const auto found=candidate_groups.find(key);
            if(found!=candidate_groups.end()) {
                if(found->second) decision.disposition=imports::Disposition::unresolved;
            } else {
            Statement candidate(impl_->db,probe_sql.c_str()); bind_candidate(candidate,r);
            const bool has_candidate=candidate.row();
            candidate_groups.emplace(key,has_candidate);
            if(has_candidate) decision.disposition=imports::Disposition::unresolved;
            }
        }
        if(committed_changes_supported() && decision.duplicate_of && !committed_entry(*decision.duplicate_of).current.active)decision={imports::Disposition::unresolved,{}};
        auto transaction=*r.transaction;
        const auto account_currency=account_for_batch(id).currency;
        if(transaction.effective_total().currency()!=account_currency) transaction=rescale_draft(imports::DraftRow{r.date,r.payee,r.memo,transaction},account_currency).transaction;
        version(impl_->db,{std::string(id),i},revision,{r.date,r.payee,r.memo,transaction,FieldOwnership::source,FieldOwnership::source},decision);
        if(context && impl_->observer)impl_->observer(ImportPhase::materialize_row);
    }
}
void ImportStore::save(const imports::Occurrence& id,std::int64_t expected,const imports::DraftRow& edit) {
    require_unlinked(id);require_writable_account(impl_->db,account_for_batch(id.batch).identity);
    Atomic a(impl_->db); expect(impl_->db,id.batch,expected); const auto old=load_row(id,false);
    const auto canonical=canonical_draft(id,edit,old.draft,expected+1,false);
    const bool reassigned=old.source.source_currency && canonical.transaction.effective_total().currency()!=*old.source.source_currency;
    imports::validate_draft(old.source,canonical,reassigned);
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
        record.text(5,old.source.date); if(old.source.original_amount)record.integer(6,old.source.original_amount->units());
        record.text(7,old.source.payee); record.text(8,old.source.reference); record.done();
    }
    int blocked=-1;
    if(!old.draft) {
        const auto b=batch(impl_->db,id.batch);
        const auto parsed=parsed_source(id.batch);
        Statement count(impl_->db,"SELECT count(*) FROM records WHERE batch=?"); count.text(1,id.batch); if(!count.row()) fail(SQLITE_CORRUPT);
        blocked=(parsed.diagnostic || count.integer(0)!=integer(b.count))?1:0;
    }
    // A first repair requires an explicit subsequent duplicate disposition.
    version(impl_->db,id,expected+1,canonical,old.decision,false); advance(impl_->db,id.batch,expected,"save",0,blocked);
    impl_->finish(a);
}
void ImportStore::decide(const imports::Occurrence& id,std::int64_t expected,const imports::Decision& decision) {
    require_unlinked(id);require_writable_account(impl_->db,account_for_batch(id.batch).identity);
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
    require_account(id);require_writable_account(impl_->db,account_for_batch(id).identity);
    Statement grouped(impl_->db,"SELECT g.state FROM import_group_accounts m JOIN import_groups g ON g.id=m.group_id WHERE m.batch=?");grouped.text(1,id);if(grouped.row() && grouped.integer(0)!=2)fail(SQLITE_CONSTRAINT,"Use whole-import Abort for this grouped source");
    Atomic a(impl_->db); const auto b=batch(impl_->db,id);
    if((b.state==app::BatchState::discarded || b.state==app::BatchState::protected_drafts) && expected<INT64_MAX && b.revision==expected+1) { a.commit(); return; }
    const auto protected_rows=protected_import_rows(id);
    const auto operation="transfer-discard-"+std::string(id)+"-"+std::to_string(expected);
    (void)transfer_import_transition(id,operation,false);
    if(protected_rows.empty())advance(impl_->db,id,expected,"discard",2);
    else {
        for(const auto ordinal:protected_rows){Statement keep(impl_->db,"INSERT OR IGNORE INTO retained_transfer_rows VALUES(?,?)");keep.text(1,id);keep.integer(2,integer(ordinal));keep.done();}
        advance(impl_->db,id,expected,"retain",4,0);
    }
    impl_->finish(a);
}
std::optional<app::Receipt> ImportStore::durable_outcome(std::string_view operation) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    return outcome(operation);
}
std::optional<app::Receipt> ImportStore::outcome(std::string_view operation) const {
    Statement s(impl_->db,"SELECT batch,revision,included,excluded,before,change,after FROM operations WHERE id=?"); s.text(1,operation);
    if(!s.row()) return {};
    require_account(s.bytes(0));
    Statement order(impl_->db,"SELECT sequence FROM history_order WHERE operation=?");order.text(1,operation);
    const auto c=order.row()?currency_as_of(impl_->db,account_for_batch(s.bytes(0)).identity,order.integer(0)):account_for_batch(s.bytes(0)).currency;
    return app::Receipt{std::string(operation),s.bytes(0),s.integer(1),static_cast<std::size_t>(s.integer(2)),static_cast<std::size_t>(s.integer(3)),
        Money(s.integer(4),c),Money(s.integer(5),c),Money(s.integer(6),c)};
}
app::Receipt ImportStore::commit(std::string_view id,std::int64_t expected,std::string_view operation) {
    require_account(id);require_writable_account(impl_->db,account_for_batch(id).identity);identifier(operation);
    if(rules_format(impl_->db)) {
        if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
        if(auto prior=outcome(operation)) {
            if(prior->batch!=id || prior->revision!=expected) fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
            return *prior;
        }
        Statement collision(impl_->db,"SELECT kind FROM ledger_operations WHERE id=?");collision.text(1,operation);
        if(collision.row()) {if(collision.integer(0)!=0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");fail(SQLITE_CORRUPT);}
        Statement other(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");other.text(1,operation);
        if(other.row()) fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    }
    Atomic a(impl_->db);
    if(committed_changes_supported() && !rules_format(impl_->db)) {Statement collision(impl_->db,"SELECT kind FROM ledger_operations WHERE id=?");collision.text(1,operation);if(collision.row() && collision.integer(0)!=0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");}
    if(auto prior=outcome(operation)) {
        if(prior->batch!=id || prior->revision!=expected) fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        a.commit(); return *prior;
    }
    expect(impl_->db,id,expected); start_import_groups(financial_group_batches(impl_->selected_account),operation);publish_group_opening(impl_->selected_account); const auto b=batch(impl_->db,id);
    if(b.blocked) fail(SQLITE_CONSTRAINT,"Resolve the blocked import before committing");
    const auto acct=account();
    if(!acct.opening_confirmed) fail(SQLITE_CONSTRAINT,"Confirm the opening balance before the first commit");
    const auto before=committed_balance();
    const auto resolved=resolved_transfers(all_transfers());
    std::size_t included=0,excluded=0; std::vector<Money> amounts; std::set<imports::Occurrence> matched;
    for(std::size_t i=0;i<b.count;++i) {
        if(!retained_member(impl_->db,id,i)){++excluded;continue;}
        auto r=load_row({std::string(id),i},false);
        if(r.decision.disposition==imports::Disposition::unresolved) fail(SQLITE_CONSTRAINT,"Review possible duplicates before committing");
        if(r.decision.disposition==imports::Disposition::include) {
            require_qif_transfers({std::string(id),i},*r.draft,resolved); ++included; amounts.push_back(r.draft->transaction.effective_total());
            Statement link(impl_->db,"INSERT INTO source_associations VALUES(?,?,?,?)");link.text(1,id);link.integer(2,integer(i));link.text(3,id);link.integer(4,integer(i));link.done();
        }
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
            Statement link(impl_->db,"INSERT INTO source_associations VALUES(?,?,?,?)");link.text(1,target.batch);link.integer(2,integer(target.ordinal));link.text(3,id);link.integer(4,integer(i));link.done();
        } else fail(SQLITE_CORRUPT);
    }
    const auto adjustment=transfer_import_transition(id,operation,true);
    const auto change=imports::sum(acct.currency,amounts)+adjustment; const auto after=before+change;
    Statement op(impl_->db,"INSERT INTO operations VALUES(?,?,?,?,?,?,?,?)"); op.text(1,operation); op.text(2,id); op.integer(3,expected);
    op.integer(4,integer(included)); op.integer(5,integer(excluded)); op.integer(6,before.units()); op.integer(7,change.units()); op.integer(8,after.units()); op.done();
    advance(impl_->db,id,expected,"commit",1);
    exec(impl_->db,"UPDATE account SET frozen=1 WHERE singleton=spentit_account()");
    if(committed_changes_supported()) {
        for(std::size_t ordinal=0;ordinal<b.count;++ordinal){if(!retained_member(impl_->db,id,ordinal))continue;const auto row=load_row({std::string(id),ordinal},false);if(row.decision.disposition==imports::Disposition::include)store_entry(impl_->db,{std::string(id),ordinal},{0,true,*row.draft});}
        append_financial(impl_->db,operation,app::ChangeKind::publication,{std::string(id),0},-1,0,before,change,after,"",0);
    }
    impl_->finish(a); return {std::string(operation),std::string(id),expected,included,excluded,before,change,after};
}
std::vector<app::LedgerRow> ImportStore::ledger(bool projected) const {
    ReadStatements read_plans(impl_->db);
    if(committed_changes_supported()) {
        if(projected && !projected_balance())fail(SQLITE_CONSTRAINT,"Projection is incomplete; review the pending import");
        std::vector<app::LedgerRow> result;
        Statement entries(impl_->db,"SELECT h.batch,h.ordinal,h.revision,r.sequence FROM entry_heads h JOIN entry_revisions e USING(batch,ordinal,revision) JOIN records r ON r.batch=h.batch AND r.ordinal=h.ordinal WHERE h.batch IN(SELECT id FROM batches WHERE account_id=spentit_account()) AND e.active=1 ORDER BY e.date,r.sequence");
        while(entries.row()){imports::Occurrence identity{entries.bytes(0),static_cast<std::size_t>(entries.integer(1))};result.push_back({identity,entries.integer(3),entry_snapshot(identity,entries.integer(2)).draft,true});}
        if(projected){Statement pending(impl_->db,"SELECT r.batch,r.ordinal,r.sequence FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE b.account_id=spentit_account() AND b.state IN(0,4) AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal)");while(pending.row()){imports::Occurrence identity{pending.bytes(0),static_cast<std::size_t>(pending.integer(1))};result.push_back({identity,pending.integer(2),*load_row(identity,false).draft,false});}}
        const auto local=local_transfer_ledger(projected);result.insert(result.end(),local.begin(),local.end());
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.draft.date,a.sequence)<std::tie(b.draft.date,b.sequence);});return result;
    }
    if(projected && !projected_balance()) fail(SQLITE_CONSTRAINT,"Projection is incomplete; resolve the pending import first");
    Statement s(impl_->db,"SELECT r.batch,r.ordinal,r.sequence,b.state FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal "
        "WHERE b.account_id=spentit_account() AND (b.state=1 OR (b.state=0 AND ?=1 AND b.blocked=0)) AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) ORDER BY v.date,r.sequence");
    s.integer(1,projected?1:0); std::vector<app::LedgerRow> result;
    while(s.row()) {
        imports::Occurrence id{s.bytes(0),static_cast<std::size_t>(s.integer(1))}; auto r=load_row(id,false);
        result.push_back({id,s.integer(2),std::move(*r.draft),s.integer(3)==1});
    }
    return result;
}
Money ImportStore::committed_for(std::int64_t id) const {
    const auto acct=account_by_id(id);std::vector<Money> values{acct.opening,local_transfer_total(id,false)};
    Statement rows(impl_->db,"SELECT e.total FROM entry_heads h JOIN entry_revisions e USING(batch,ordinal,revision) WHERE h.batch IN(SELECT id FROM batches WHERE account_id=?) AND e.active=1");
    rows.integer(1,id);while(rows.row())values.emplace_back(rows.integer(0),acct.currency);return imports::sum(acct.currency,values);
}
Money ImportStore::committed_balance() const {
    if(committed_changes_supported()) return committed_for(impl_->selected_account);
    const auto a=account(); std::vector<Money> amounts{a.opening};
    Statement s(impl_->db,"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.state=1 AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    while(s.row()) amounts.emplace_back(s.integer(0),a.currency); return imports::sum(a.currency,amounts);
}
std::optional<Money> ImportStore::projected_for(std::int64_t id) const {
    for(const auto& b:batches_for(id)) if(b.state==app::BatchState::pending || b.state==app::BatchState::captured || b.state==app::BatchState::protected_drafts) {
        if(b.blocked) return {};
        Statement s(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=0 AND (NOT EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch) OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) LIMIT 1");
        s.text(1,b.id); if(s.row()) return {};
    }
    if(committed_changes_supported()) {
        Statement stale(impl_->db,"SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch JOIN review_stamps s USING(batch,ordinal,revision) JOIN entry_heads h ON h.batch=v.duplicate_batch AND h.ordinal=v.duplicate_ordinal JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE b.account_id=? AND b.state=0 AND v.decision=2 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) AND (s.target_revision!=h.revision OR s.target_active!=e.active) LIMIT 1");
        stale.integer(1,id);if(stale.row())return {};
    }
    const auto a=account_by_id(id);std::vector<Money> amounts{committed_changes_supported()?committed_for(id):a.opening,local_transfer_total(id,true)-local_transfer_total(id,false)};
    Statement s(impl_->db,committed_changes_supported()?"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.account_id=? AND b.state IN(0,4) AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)":"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.account_id=? AND b.state IN(0,1) AND v.decision=1 "
        "AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    s.integer(1,id);while(s.row()) amounts.emplace_back(s.integer(0),a.currency);return imports::sum(a.currency,amounts);
}
std::optional<Money> ImportStore::projected_balance() const {return projected_for(impl_->selected_account);}
std::vector<app::AccountBalance> ImportStore::account_balances() const {
    Statement ids(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM retired_import_accounts r WHERE r.account_id=singleton) AND NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=singleton) ORDER BY singleton");
    std::vector<app::AccountBalance> result;
    while(ids.row()) {
        const auto id=ids.integer(0);
        if(committed_changes_supported()) result.push_back(app::AccountBalance{id,committed_for(id),projected_for(id)});
        else if(id==impl_->selected_account && account_selected()) result.push_back(app::AccountBalance{id,committed_balance(),projected_balance()});
    }
    return result;
}
void ImportStore::verify(const app::OperationContext* context) const {
    ReadStatements read_plans(impl_->db);
    struct VerificationGuard { bool& value; bool prior; ~VerificationGuard(){value=prior;} } guard{impl_->verifying,impl_->verifying};
    impl_->verifying=true;
    if(context) context->checkpoint("Verifying dataset integrity");
    if(scalar(impl_->db,"PRAGMA integrity_check")!="ok") fail(SQLITE_CORRUPT);
    for(const char* sql:{"PRAGMA cipher_integrity_check","PRAGMA foreign_key_check"}) { Statement s(impl_->db,sql); if(s.row()) fail(SQLITE_CORRUPT); }
    std::vector<app::Account> all_accounts;Statement account_ids(impl_->db,"SELECT singleton FROM account ORDER BY singleton");while(account_ids.row())all_accounts.push_back(account_by_id(account_ids.integer(0)));if(all_accounts.empty() || all_accounts.front().identity!=1)fail(SQLITE_CORRUPT);
    for(const auto& acct:all_accounts){safe_text(acct.name);iso_date(acct.opening_date);if(acct.name.empty() || (acct.frozen && !acct.opening_confirmed))fail(SQLITE_CORRUPT);}
    Statement creation(impl_->db,"SELECT 1 FROM account a LEFT JOIN account_creations c ON c.identity=a.singleton WHERE c.identity IS NULL");if(creation.row())fail(SQLITE_CORRUPT);
    for(const auto& acct:all_accounts){
        Statement original(impl_->db,"SELECT opening_date FROM account_creations WHERE identity=?");original.integer(1,acct.identity);if(!original.row())fail(SQLITE_CORRUPT);iso_date(original.bytes(0));
        if(acct.opening_confirmed!=(acct.revision>0) || (acct.revision==0 && (acct.opening.units()!=0 || acct.opening_date!=original.bytes(0))))fail(SQLITE_CORRUPT,"Account opening does not match its creation");
    }
    verify_account_commands(context);
    verify_import_groups(context);
    Statement invalid(impl_->db,
        "SELECT 1 FROM batches b WHERE parser NOT IN('bank-v3','ofx-sgml-1','ofx-xml-2') OR (state=1)!=(EXISTS(SELECT 1 FROM operations o WHERE o.batch=b.id)) "
        "OR revision!=(SELECT max(revision) FROM events WHERE batch=b.id) OR revision+1!=(SELECT count(*) FROM events WHERE batch=b.id) "
        "UNION ALL SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch WHERE v.revision>b.revision "
        "UNION ALL SELECT 1 FROM sources s WHERE NOT EXISTS(SELECT 1 FROM batches b WHERE b.source_id=s.id) "
        "UNION ALL SELECT 1 FROM account a WHERE frozen!=(EXISTS(SELECT 1 FROM operations o JOIN batches b ON b.id=o.batch WHERE b.account_id=a.singleton) OR EXISTS(SELECT 1 FROM transfer_events t WHERE t.account=a.singleton AND t.did_commit=1))");
    if(invalid.row()) fail(SQLITE_CORRUPT,"Invalid import history relationships");
    Statement headers(impl_->db,"SELECT 1 FROM sources s WHERE NOT EXISTS(SELECT 1 FROM source_headers h WHERE h.source_id=s.id) UNION ALL SELECT 1 FROM batches b JOIN source_headers h ON h.source_id=b.source_id WHERE NOT ((b.parser='bank-v3' AND h.format='qif' AND h.grammar='qif') OR (b.parser='ofx-sgml-1' AND h.grammar='ofx-sgml' AND h.format IN('ofx','qfx','qbo','aso')) OR (b.parser='ofx-xml-2' AND h.grammar='ofx-xml' AND h.format IN('ofx','qfx','qbo','aso'))) UNION ALL SELECT 1 FROM source_associations a JOIN batches b ON b.id=a.source_batch WHERE b.state!=1 UNION ALL SELECT 1 FROM source_associations a JOIN batches b ON b.id=a.ledger_batch WHERE b.state!=1");
    if(headers.row()) fail(SQLITE_CORRUPT,"Source evidence does not match its import");
    Statement all_batches(impl_->db,"SELECT id FROM batches ORDER BY rowid");
    while(all_batches.row()) {
        const auto b=batch(impl_->db,all_batches.bytes(0));const auto acct=account_for_batch(b.id);
        if(context) context->checkpoint("Verifying import evidence");
        const auto bytes=evidence(b.id); const auto parsed=parsed_source(b.id,[&]{if(context)context->checkpoint();});
        if(!parsed.admissible || parsed.records.size()!=b.count) fail(SQLITE_CORRUPT);
        Statement count(impl_->db,"SELECT count(*) FROM records WHERE batch=?"); count.text(1,b.id); if(!count.row()) fail(SQLITE_CORRUPT);
        std::int64_t initial_revision=0;
        if(schema3_batch(impl_->db)) {
            Statement materialized(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");materialized.text(1,b.id);if(!materialized.row())fail(SQLITE_CORRUPT);
            Statement first(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=0");first.text(1,b.id);if(!first.row())fail(SQLITE_CORRUPT);
            if(materialized.null(0)) {
                if(first.bytes(0)!="capture" || (b.state!=app::BatchState::captured && b.state!=app::BatchState::discarded) || !b.blocked || count.integer(0)!=0) fail(SQLITE_CORRUPT);
                Statement bad(impl_->db,"SELECT 1 FROM events WHERE batch=? AND ((revision=0 AND kind!='capture') OR (revision>0 AND kind NOT IN('opening','discard')) OR (kind='discard' AND (revision!=? OR ?!=2)))");
                bad.text(1,b.id);bad.integer(2,b.revision);bad.integer(3,static_cast<int>(b.state));if(bad.row())fail(SQLITE_CORRUPT);
                Statement last(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");last.text(1,b.id);last.integer(2,b.revision);if(!last.row() || (b.state==app::BatchState::discarded)!=(last.bytes(0)=="discard"))fail(SQLITE_CORRUPT);
                verify_source_facts(impl_->db,b,parsed,false);continue;
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
                rec.null(2)!=!source.original_amount || (source.original_amount && rec.integer(2)!=source.original_amount->units()) || rec.bytes(3)!=source.payee || rec.bytes(4)!=source.reference) fail(SQLITE_CORRUPT);
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
                const auto historical_currency=version_currency(impl_->db,b.id,integer(i),vv.integer(0));
                const bool reassigned=source.source_currency && *source.source_currency!=historical_currency;
                while(aa.row()) {
                    if(aa.integer(0)!=integer(allocations.size()) || allocations.size()>=qif::max_allocations) fail(SQLITE_CORRUPT);
                    allocations.push_back({aa.bytes(1),aa.bytes(2),Money(aa.integer(3),historical_currency)});
                }
                imports::DraftRow historical{vv.bytes(1),vv.bytes(2),vv.bytes(3),Transaction::repaired_import(source.original_amount,Money(vv.integer(4),historical_currency),std::move(allocations),reassigned)};
                load_metadata(impl_->db,{b.id,i},vv.integer(0),historical,false);
                imports::validate_draft(source,historical,reassigned);
                if(vv.integer(5)==2) {
                    imports::Occurrence target{vv.bytes(6),static_cast<std::size_t>(vv.integer(7))};
                    if(account_for_batch(target.batch).identity!=acct.identity || target.batch==b.id || batch(impl_->db,target.batch).state!=app::BatchState::committed) fail(SQLITE_CORRUPT);
                    const auto other=load_row(target,false);
                    if(other.decision.disposition!=imports::Disposition::include || !imports::candidate_basis(source,other.source)) fail(SQLITE_CORRUPT);
                }
            }
        }
        verify_source_facts(impl_->db,b,parsed,true);
        if(count.integer(0)!=integer(valid_count) || (retained_batch(impl_->db,b.id)?b.blocked:blocked!=b.blocked)) fail(SQLITE_CORRUPT);
        Statement duplicates(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=2 AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) "
            "GROUP BY duplicate_batch,duplicate_ordinal HAVING count(*)>1"); duplicates.text(1,b.id); if(duplicates.row()) fail(SQLITE_CORRUPT);
    }
    if(categories_supported())verify_categories(context);
    if(rules_format(impl_->db))verify_rules(context);
    if(committed_changes_supported()){verify_transfers(context);verify_committed(context);if(bulk_supported())verify_bulk(context);if(rules_format(impl_->db))verify_rule_reapply(context);return;}
    const auto acct=account();
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
                          const BackupObserver& observer, const app::OperationContext* context) const {
    if(context) context->checkpoint("Preparing encrypted backup");
    auto candidate=destination; candidate+=".partial";
    {
        auto target=connect(candidate,password,true);
        sqlite3_backup* backup=sqlite3_backup_init(target.impl_->db,"main",impl_->db,"main");
        if(!backup) fail(sqlite3_errcode(target.impl_->db));
        int step=SQLITE_OK;
        try {
            do {
                if(context) context->checkpoint("Copying encrypted backup pages");
                step=sqlite3_backup_step(backup,128);
            } while(step==SQLITE_OK);
        } catch (...) { sqlite3_backup_finish(backup); throw; }
        const int finish=sqlite3_backup_finish(backup);
        if(step!=SQLITE_DONE) fail(step); ok(finish);
        if(context) context->checkpoint("Verifying encrypted backup");
        target.verify(context);
    }
    if(context) context->checkpoint("Reopening encrypted backup");
    { auto restored=open(candidate,password,context); restored.verify(context); }
    if(observer) observer(BackupPhase::before_publish);
    if(context) { context->checkpoint("Publishing verified backup"); context->seal(); }
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
#include "bulk_store.inc"
#include "rule_store.inc"
#include "rule_reapply_store.inc"
#include "transfer_store.inc"
#include "import_group_store.inc"
#include "account_lifecycle.inc"
} // namespace spentit::storage
