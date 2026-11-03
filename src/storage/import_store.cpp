#include <spentit/diagnostics.hpp>
namespace diag = spentit::diagnostics;
#include <spentit/import_store.hpp>
#include <spentit/statements.hpp>
#include <spentit/date_format.hpp>
#include <spentit/rule_regex.hpp>
#include <sys/file.h>
#include <sys/stat.h>
#include <set>
#include <tuple>
#include <map>
#include <unordered_set>
#include <sqlite3.h>
#include <algorithm>
#include <climits>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <unistd.h>
#include "verification_cache.hpp"
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
// Reuse prepared plans only within one bounded repository operation. Results are
// never cached: each use binds afresh and steps the database again. Finalizing
// the scope releases every plan before a document can close or a command return.
class ScopedStatements {
public:
    struct Slot { sqlite3_stmt* statement=nullptr; std::size_t bytes=0; };
    explicit ScopedStatements(sqlite3* db):db_(db),previous_(active_) {
        active_=previous_ && previous_->db_==db ? previous_ : this;
    }
    ~ScopedStatements(){active_=previous_;for(auto& [sql,slot]:plans_)sqlite3_finalize(slot.statement);}
    ScopedStatements(const ScopedStatements&)=delete;
    ScopedStatements& operator=(const ScopedStatements&)=delete;
    std::optional<std::string> format_version;
    std::string alias_signature;
    std::optional<std::vector<app::AccountAlias>> aliases;
    std::map<std::string,std::optional<std::int64_t>,std::less<>> alias_resolutions;
    static ScopedStatements* current(sqlite3* db){return active_ && active_->db_==db?active_:nullptr;}
    Slot* slot(const char* sql){
        const auto found=plans_.find(sql);if(found!=plans_.end())return &found->second;
        if(plans_.size()>=256 || std::string_view(sql).size()>4096)return nullptr;
        return &plans_.try_emplace(sql).first->second;
    }
    sqlite3_stmt* take(Slot* slot){if(!slot)return nullptr;auto* result=slot->statement;cached_bytes_-=slot->bytes;slot->statement=nullptr;slot->bytes=0;return result;}
    bool put(Slot* slot,sqlite3_stmt* statement){
        if(!slot || slot->statement)return false;
        if(sqlite3_reset(statement)!=SQLITE_OK || sqlite3_clear_bindings(statement)!=SQLITE_OK)return false;
        const auto size=sqlite3_stmt_status(statement,SQLITE_STMTSTATUS_MEMUSED,0);
        if(size<0 || static_cast<std::size_t>(size)>4*1024*1024-cached_bytes_)return false;
        slot->statement=statement;slot->bytes=static_cast<std::size_t>(size);cached_bytes_+=slot->bytes;return true;
    }
private:
    sqlite3* db_;
    ScopedStatements* previous_;
    std::map<std::string,Slot,std::less<>> plans_;
    std::size_t cached_bytes_=0;
    inline static thread_local ScopedStatements* active_=nullptr;
};
class Statement {
public:
    Statement(sqlite3* db, const char* sql) {
        cache_=ScopedStatements::current(db);if(cache_){slot_=cache_->slot(sql);stmt_=cache_->take(slot_);}
        diag::Collector::instance().statement(stmt_!=nullptr);
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
    ScopedStatements* cache_=nullptr;
    ScopedStatements::Slot* slot_=nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};
std::string scalar(sqlite3* db, const char* sql) {
    // Format is immutable during repository operations. This value lives only
    // as long as their statement scope; a new open/verification reads it again.
    auto* scope=std::string_view(sql)=="PRAGMA user_version"?ScopedStatements::current(db):nullptr;
    if(scope && scope->format_version)return *scope->format_version;
    Statement query(db, sql);
    if (!query.row()) fail(SQLITE_ERROR);
    const auto value = query.bytes(0);
    if (query.row()) fail(SQLITE_ERROR);
    if(scope)scope->format_version=value;
    return value;
}
class Atomic {
public:
    explicit Atomic(sqlite3* db) : db_(db) { exec(db_, "BEGIN IMMEDIATE"); }
    ~Atomic() { if (active_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
    void commit() { diag::Span commitSpan(diag::Code::transaction);exec(db_, "COMMIT"); active_ = false; }
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

#include "transfer_schema.inc"
#include "review_sheet_schema.inc"
#include "transfer_codec.inc"
#ifndef SPENTIT_RULES_STORAGE
constexpr const char* schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=45;
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
CREATE TABLE import_group_accounts(group_id TEXT NOT NULL REFERENCES import_groups(id),source_account INTEGER NOT NULL CHECK(source_account>=0),account_id INTEGER NOT NULL REFERENCES account(singleton),batch TEXT NOT NULL UNIQUE REFERENCES batches(id),created INTEGER NOT NULL CHECK(created IN(0,1)),balance_choice INTEGER NOT NULL DEFAULT 0 CHECK(balance_choice BETWEEN 0 AND 6),balance_revision INTEGER,balance_sequence INTEGER,reviewed_change INTEGER,opening INTEGER,opening_date TEXT,PRIMARY KEY(group_id,source_account),UNIQUE(group_id,account_id)) STRICT;
CREATE TABLE retired_import_accounts(account_id INTEGER PRIMARY KEY REFERENCES account(singleton),group_id TEXT NOT NULL REFERENCES import_groups(id)) STRICT;
CREATE TABLE import_category_decisions(source TEXT PRIMARY KEY REFERENCES sources(id) ON DELETE CASCADE,resolved INTEGER NOT NULL CHECK(resolved IN(0,1))) STRICT;
CREATE TABLE import_category_choices(source TEXT NOT NULL REFERENCES import_category_decisions(source) ON DELETE CASCADE,name TEXT NOT NULL,chosen INTEGER CHECK(chosen IN(0,1)),PRIMARY KEY(source,name)) STRICT;
CREATE TRIGGER category_choice_final BEFORE UPDATE ON import_category_choices WHEN OLD.chosen IS NOT NULL BEGIN SELECT RAISE(ABORT,'Import category decision is final'); END;
CREATE TRIGGER category_decision_final BEFORE UPDATE ON import_category_decisions WHEN OLD.resolved=1 BEGIN SELECT RAISE(ABORT,'Import category decision is final'); END;
CREATE TABLE import_group_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL REFERENCES import_groups(id),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_balance_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL,source_account INTEGER NOT NULL,balance_choice INTEGER NOT NULL CHECK(balance_choice BETWEEN 1 AND 6),balance_revision INTEGER NOT NULL,balance_sequence INTEGER NOT NULL,reviewed_change INTEGER NOT NULL,opening INTEGER NOT NULL,opening_date TEXT NOT NULL,FOREIGN KEY(group_id,source_account) REFERENCES import_group_accounts(group_id,source_account)) STRICT;
CREATE TRIGGER group_creation AFTER INSERT ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER group_transition BEFORE UPDATE ON import_groups WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NOT ((OLD.state=0 AND NEW.state IN(1,3)) OR (OLD.state=1 AND NEW.state IN(2,3))) BEGIN SELECT RAISE(ABORT,'Invalid group transition'); END;
CREATE TRIGGER group_transition_history AFTER UPDATE ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER balance_history AFTER UPDATE OF balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date ON import_group_accounts BEGIN INSERT INTO import_balance_events(group_id,source_account,balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date) VALUES(NEW.group_id,NEW.source_account,NEW.balance_choice,NEW.balance_revision,NEW.balance_sequence,NEW.reviewed_change,NEW.opening,NEW.opening_date); END;
CREATE TRIGGER group_events_immutable BEFORE UPDATE ON import_group_events BEGIN SELECT RAISE(ABORT,'Immutable group history'); END;
CREATE TRIGGER group_events_keep BEFORE DELETE ON import_group_events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Retained group history'); END;
CREATE TRIGGER balance_events_immutable BEFORE UPDATE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Immutable balance history'); END;
CREATE TRIGGER balance_events_keep BEFORE DELETE ON import_balance_events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Retained balance history'); END;
CREATE TRIGGER retired_account_immutable BEFORE UPDATE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account identity is retained'); END;
CREATE TRIGGER retired_account_keep BEFORE DELETE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account cannot be resurrected'); END;
CREATE TRIGGER group_keep BEFORE DELETE ON import_groups WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Import group is retained'); END;
CREATE TRIGGER group_member_keep BEFORE DELETE ON import_group_accounts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Import group member is retained'); END;
CREATE TRIGGER group_mapping_immutable BEFORE UPDATE ON import_group_accounts WHEN NEW.group_id!=OLD.group_id OR NEW.source_account!=OLD.source_account OR NEW.account_id!=OLD.account_id OR NEW.batch!=OLD.batch OR NEW.created!=OLD.created BEGIN SELECT RAISE(ABORT,'Import mapping is immutable'); END;
CREATE TRIGGER batch_account_immutable BEFORE UPDATE OF account_id ON batches WHEN OLD.account_id!=NEW.account_id BEGIN SELECT RAISE(ABORT,'Account ownership is immutable'); END;
CREATE TRIGGER account_creation_immutable BEFORE UPDATE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is immutable'); END;
CREATE TRIGGER account_creation_retained BEFORE DELETE ON account_creations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Account creation is retained'); END;
CREATE INDEX pending_accounts ON batches(account_id,state);
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
CREATE TABLE operations(id TEXT PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,
 included INTEGER NOT NULL,excluded INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL) STRICT;
CREATE TABLE events(sequence INTEGER PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN('capture','materialize','ingest','save','decide','discard','retain','commit','accept','rollback','opening','currency')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.source_id!=OLD.source_id OR NEW.source_account IS NOT OLD.source_account OR NEW.count!=OLD.count OR OLD.state NOT IN(0,3,4) OR NEW.revision!=OLD.revision+1 OR
 (NEW.materialized_revision IS NOT OLD.materialized_revision AND NOT
 (OLD.state=3 AND NEW.state=0 AND OLD.materialized_revision IS NULL AND NEW.materialized_revision=NEW.revision))
 BEGIN SELECT RAISE(ABORT,'invalid batch transition'); END;
CREATE TRIGGER evidence_keep BEFORE DELETE ON sources WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER evidence_immutable BEFORE UPDATE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_keep BEFORE DELETE ON batches WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_immutable BEFORE UPDATE ON records BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_keep BEFORE DELETE ON records WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_immutable BEFORE UPDATE ON versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_keep BEFORE DELETE ON versions WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_immutable BEFORE UPDATE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_keep BEFORE DELETE ON allocations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_immutable BEFORE UPDATE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_keep BEFORE DELETE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_immutable BEFORE UPDATE ON events BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_keep BEFORE DELETE ON events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER frozen_opening BEFORE UPDATE ON account WHEN OLD.frozen=1 AND spentit_account_command()=0 AND
 (NEW.opening!=OLD.opening OR NEW.opening_date!=OLD.opening_date OR NEW.definition!=OLD.definition OR NEW.frozen!=1)
 BEGIN SELECT RAISE(ABORT,'frozen opening'); END;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=NEW.batch AND a.ordinal=NEW.ordinal) OR EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=NEW.batch AND g.ordinal=NEW.ordinal) OR (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER record_pending BEFORE INSERT ON records WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TABLE entry_revisions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),active INTEGER NOT NULL CHECK(active IN(0,1)),date TEXT,payee TEXT,memo TEXT,total INTEGER,draft_revision INTEGER,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal),FOREIGN KEY(batch,ordinal,draft_revision) REFERENCES versions(batch,ordinal,revision),CHECK((revision=0 AND draft_revision IS NOT NULL AND date IS NULL AND payee IS NULL AND memo IS NULL AND total IS NULL) OR (revision>0 AND draft_revision IS NULL AND date IS NOT NULL AND payee IS NOT NULL AND memo IS NOT NULL AND total IS NOT NULL))) STRICT;
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
CREATE TRIGGER ledger_operations_keep BEFORE DELETE ON ledger_operations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_immutable BEFORE UPDATE ON change_dependencies BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_keep BEFORE DELETE ON change_dependencies WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_immutable BEFORE UPDATE ON history_order BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_keep BEFORE DELETE ON history_order WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_immutable BEFORE UPDATE ON review_stamps BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_keep BEFORE DELETE ON review_stamps WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_heads_keep BEFORE DELETE ON entry_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE category_state(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision INTEGER NOT NULL CHECK(revision>=0)) STRICT;
INSERT INTO category_state VALUES(1,0);
CREATE TABLE category_versions(id TEXT NOT NULL,revision INTEGER NOT NULL,catalog_revision INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL CHECK(kind IN(0,1)),archived INTEGER NOT NULL CHECK(archived IN(0,1)),deleted INTEGER NOT NULL CHECK(deleted IN(0,1)),PRIMARY KEY(id,revision)) STRICT;
CREATE TABLE category_heads(id TEXT PRIMARY KEY,revision INTEGER NOT NULL,FOREIGN KEY(id,revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE category_operations(id TEXT PRIMARY KEY,action INTEGER NOT NULL,category TEXT NOT NULL,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL,catalog_before INTEGER NOT NULL,catalog_after INTEGER NOT NULL UNIQUE,before_revision INTEGER,after_revision INTEGER NOT NULL,FOREIGN KEY(category,after_revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL CHECK(domain IN(0,1,4,5,6))) STRICT;
CREATE TABLE draft_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,manual_split_amounts INTEGER NOT NULL CHECK(manual_split_amounts IN(0,1)),remainder_allocation TEXT NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES versions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,manual_split_amounts INTEGER NOT NULL CHECK(manual_split_amounts IN(0,1)),remainder_allocation TEXT NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE draft_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES allocations(batch,ordinal,revision,position),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
CREATE TABLE entry_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
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
CREATE TRIGGER operation_registry_keep BEFORE DELETE ON operation_registry WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_immutable BEFORE UPDATE ON draft_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_keep BEFORE DELETE ON draft_fields WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_immutable BEFORE UPDATE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_keep BEFORE DELETE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_immutable BEFORE UPDATE ON draft_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_keep BEFORE DELETE ON draft_allocation_meta WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_immutable BEFORE UPDATE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_keep BEFORE DELETE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
)SQL";
#else
constexpr const char* schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=46;
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
CREATE TABLE import_group_accounts(group_id TEXT NOT NULL REFERENCES import_groups(id),source_account INTEGER NOT NULL CHECK(source_account>=0),account_id INTEGER NOT NULL REFERENCES account(singleton),batch TEXT NOT NULL UNIQUE REFERENCES batches(id),created INTEGER NOT NULL CHECK(created IN(0,1)),balance_choice INTEGER NOT NULL DEFAULT 0 CHECK(balance_choice BETWEEN 0 AND 6),balance_revision INTEGER,balance_sequence INTEGER,reviewed_change INTEGER,opening INTEGER,opening_date TEXT,PRIMARY KEY(group_id,source_account),UNIQUE(group_id,account_id)) STRICT;
CREATE TABLE retired_import_accounts(account_id INTEGER PRIMARY KEY REFERENCES account(singleton),group_id TEXT NOT NULL REFERENCES import_groups(id)) STRICT;
CREATE TABLE import_category_decisions(source TEXT PRIMARY KEY REFERENCES sources(id) ON DELETE CASCADE,resolved INTEGER NOT NULL CHECK(resolved IN(0,1))) STRICT;
CREATE TABLE import_category_choices(source TEXT NOT NULL REFERENCES import_category_decisions(source) ON DELETE CASCADE,name TEXT NOT NULL,chosen INTEGER CHECK(chosen IN(0,1)),PRIMARY KEY(source,name)) STRICT;
CREATE TRIGGER category_choice_final BEFORE UPDATE ON import_category_choices WHEN OLD.chosen IS NOT NULL BEGIN SELECT RAISE(ABORT,'Import category decision is final'); END;
CREATE TRIGGER category_decision_final BEFORE UPDATE ON import_category_decisions WHEN OLD.resolved=1 BEGIN SELECT RAISE(ABORT,'Import category decision is final'); END;
CREATE TABLE import_group_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL REFERENCES import_groups(id),state INTEGER NOT NULL CHECK(state IN(0,1,2,3)),publication TEXT REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,CHECK((state=2)=(publication IS NOT NULL))) STRICT;
CREATE TABLE import_balance_events(sequence INTEGER PRIMARY KEY,group_id TEXT NOT NULL,source_account INTEGER NOT NULL,balance_choice INTEGER NOT NULL CHECK(balance_choice BETWEEN 1 AND 6),balance_revision INTEGER NOT NULL,balance_sequence INTEGER NOT NULL,reviewed_change INTEGER NOT NULL,opening INTEGER NOT NULL,opening_date TEXT NOT NULL,FOREIGN KEY(group_id,source_account) REFERENCES import_group_accounts(group_id,source_account)) STRICT;
CREATE TRIGGER group_creation AFTER INSERT ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER group_transition BEFORE UPDATE ON import_groups WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NOT ((OLD.state=0 AND NEW.state IN(1,3)) OR (OLD.state=1 AND NEW.state IN(2,3))) BEGIN SELECT RAISE(ABORT,'Invalid group transition'); END;
CREATE TRIGGER group_transition_history AFTER UPDATE ON import_groups BEGIN INSERT INTO import_group_events(group_id,state,publication) VALUES(NEW.id,NEW.state,NEW.publication); END;
CREATE TRIGGER balance_history AFTER UPDATE OF balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date ON import_group_accounts BEGIN INSERT INTO import_balance_events(group_id,source_account,balance_choice,balance_revision,balance_sequence,reviewed_change,opening,opening_date) VALUES(NEW.group_id,NEW.source_account,NEW.balance_choice,NEW.balance_revision,NEW.balance_sequence,NEW.reviewed_change,NEW.opening,NEW.opening_date); END;
CREATE TRIGGER group_events_immutable BEFORE UPDATE ON import_group_events BEGIN SELECT RAISE(ABORT,'Immutable group history'); END;
CREATE TRIGGER group_events_keep BEFORE DELETE ON import_group_events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Retained group history'); END;
CREATE TRIGGER balance_events_immutable BEFORE UPDATE ON import_balance_events BEGIN SELECT RAISE(ABORT,'Immutable balance history'); END;
CREATE TRIGGER balance_events_keep BEFORE DELETE ON import_balance_events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Retained balance history'); END;
CREATE TRIGGER retired_account_immutable BEFORE UPDATE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account identity is retained'); END;
CREATE TRIGGER retired_account_keep BEFORE DELETE ON retired_import_accounts BEGIN SELECT RAISE(ABORT,'Removed account cannot be resurrected'); END;
CREATE TRIGGER group_keep BEFORE DELETE ON import_groups WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Import group is retained'); END;
CREATE TRIGGER group_member_keep BEFORE DELETE ON import_group_accounts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Import group member is retained'); END;
CREATE TRIGGER group_mapping_immutable BEFORE UPDATE ON import_group_accounts WHEN NEW.group_id!=OLD.group_id OR NEW.source_account!=OLD.source_account OR NEW.account_id!=OLD.account_id OR NEW.batch!=OLD.batch OR NEW.created!=OLD.created BEGIN SELECT RAISE(ABORT,'Import mapping is immutable'); END;
CREATE TRIGGER batch_account_immutable BEFORE UPDATE OF account_id ON batches WHEN OLD.account_id!=NEW.account_id BEGIN SELECT RAISE(ABORT,'Account ownership is immutable'); END;
CREATE TRIGGER account_creation_immutable BEFORE UPDATE ON account_creations BEGIN SELECT RAISE(ABORT,'Account creation is immutable'); END;
CREATE TRIGGER account_creation_retained BEFORE DELETE ON account_creations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Account creation is retained'); END;
CREATE INDEX pending_accounts ON batches(account_id,state);
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
CREATE TABLE operations(id TEXT PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,
 included INTEGER NOT NULL,excluded INTEGER NOT NULL,before INTEGER NOT NULL,change INTEGER NOT NULL,after INTEGER NOT NULL) STRICT;
CREATE TABLE events(sequence INTEGER PRIMARY KEY,batch TEXT NOT NULL REFERENCES batches(id),revision INTEGER NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN('capture','materialize','ingest','save','decide','discard','retain','commit','accept','rollback','opening','apply_rules','currency')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.source_id!=OLD.source_id OR NEW.source_account IS NOT OLD.source_account OR NEW.count!=OLD.count OR OLD.state NOT IN(0,3,4) OR NEW.revision!=OLD.revision+1 OR
 (NEW.materialized_revision IS NOT OLD.materialized_revision AND NOT
 (OLD.state=3 AND NEW.state=0 AND OLD.materialized_revision IS NULL AND NEW.materialized_revision=NEW.revision))
 BEGIN SELECT RAISE(ABORT,'invalid batch transition'); END;
CREATE TRIGGER evidence_keep BEFORE DELETE ON sources WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER evidence_immutable BEFORE UPDATE ON sources BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_keep BEFORE DELETE ON batches WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_immutable BEFORE UPDATE ON records BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER record_keep BEFORE DELETE ON records WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_immutable BEFORE UPDATE ON versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER version_keep BEFORE DELETE ON versions WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_immutable BEFORE UPDATE ON allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER allocation_keep BEFORE DELETE ON allocations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_immutable BEFORE UPDATE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER operation_keep BEFORE DELETE ON operations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_immutable BEFORE UPDATE ON events BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER event_keep BEFORE DELETE ON events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER frozen_opening BEFORE UPDATE ON account WHEN OLD.frozen=1 AND spentit_account_command()=0 AND
 (NEW.opening!=OLD.opening OR NEW.opening_date!=OLD.opening_date OR NEW.definition!=OLD.definition OR NEW.frozen!=1)
 BEGIN SELECT RAISE(ABORT,'frozen opening'); END;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=NEW.batch AND a.ordinal=NEW.ordinal) OR EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=NEW.batch AND g.ordinal=NEW.ordinal) OR (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER record_pending BEFORE INSERT ON records WHEN (SELECT state FROM batches WHERE id=NEW.batch)!=0
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN (SELECT state FROM batches WHERE id=NEW.batch) NOT IN(0,4) OR ((SELECT state FROM batches WHERE id=NEW.batch)=4 AND spentit_account_command()=0)
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
CREATE TABLE entry_revisions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL CHECK(revision>=0),active INTEGER NOT NULL CHECK(active IN(0,1)),date TEXT,payee TEXT,memo TEXT,total INTEGER,draft_revision INTEGER,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal),FOREIGN KEY(batch,ordinal,draft_revision) REFERENCES versions(batch,ordinal,revision),CHECK((revision=0 AND draft_revision IS NOT NULL AND date IS NULL AND payee IS NULL AND memo IS NULL AND total IS NULL) OR (revision>0 AND draft_revision IS NULL AND date IS NOT NULL AND payee IS NOT NULL AND memo IS NOT NULL AND total IS NOT NULL))) STRICT;
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
CREATE TRIGGER rule_applications_immutable BEFORE UPDATE ON rule_applications WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_applications_keep BEFORE DELETE ON rule_applications WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
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
CREATE TRIGGER ledger_operations_keep BEFORE DELETE ON ledger_operations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_immutable BEFORE UPDATE ON change_dependencies BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER change_dependencies_keep BEFORE DELETE ON change_dependencies WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_immutable BEFORE UPDATE ON history_order BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER history_order_keep BEFORE DELETE ON history_order WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_immutable BEFORE UPDATE ON review_stamps BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER review_stamps_keep BEFORE DELETE ON review_stamps WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_heads_keep BEFORE DELETE ON entry_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE category_state(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision INTEGER NOT NULL CHECK(revision>=0)) STRICT;
INSERT INTO category_state VALUES(1,0);
CREATE TABLE category_versions(id TEXT NOT NULL,revision INTEGER NOT NULL,catalog_revision INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL CHECK(kind IN(0,1)),archived INTEGER NOT NULL CHECK(archived IN(0,1)),deleted INTEGER NOT NULL CHECK(deleted IN(0,1)),PRIMARY KEY(id,revision)) STRICT;
CREATE TABLE category_heads(id TEXT PRIMARY KEY,revision INTEGER NOT NULL,FOREIGN KEY(id,revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE category_operations(id TEXT PRIMARY KEY,action INTEGER NOT NULL,category TEXT NOT NULL,name TEXT NOT NULL,parent TEXT NOT NULL,kind INTEGER NOT NULL,catalog_before INTEGER NOT NULL,catalog_after INTEGER NOT NULL UNIQUE,before_revision INTEGER,after_revision INTEGER NOT NULL,FOREIGN KEY(category,after_revision) REFERENCES category_versions(id,revision)) STRICT;
CREATE TABLE operation_registry(id TEXT PRIMARY KEY,domain INTEGER NOT NULL CHECK(domain IN(0,1,2,3,4,5,6))) STRICT;
CREATE TABLE draft_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,manual_split_amounts INTEGER NOT NULL CHECK(manual_split_amounts IN(0,1)),remainder_allocation TEXT NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES versions(batch,ordinal,revision)) STRICT;
CREATE TABLE entry_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,manual_split_amounts INTEGER NOT NULL CHECK(manual_split_amounts IN(0,1)),remainder_allocation TEXT NOT NULL,PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision)) STRICT;
CREATE TABLE draft_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES allocations(batch,ordinal,revision,position),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
CREATE TABLE entry_allocation_meta(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,identity TEXT NOT NULL,source_ordinal INTEGER,category_id TEXT,catalog_revision INTEGER,category_owner INTEGER NOT NULL,memo_owner INTEGER NOT NULL,PRIMARY KEY(batch,ordinal,revision,position),UNIQUE(batch,ordinal,revision,identity),FOREIGN KEY(batch,ordinal,revision) REFERENCES entry_revisions(batch,ordinal,revision),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK((category_id IS NULL AND catalog_revision IS NULL) OR (category_id IS NOT NULL AND catalog_revision IS NOT NULL))) STRICT;
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
CREATE TRIGGER operation_registry_keep BEFORE DELETE ON operation_registry WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_immutable BEFORE UPDATE ON draft_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_fields_keep BEFORE DELETE ON draft_fields WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_immutable BEFORE UPDATE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_fields_keep BEFORE DELETE ON entry_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_immutable BEFORE UPDATE ON draft_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_allocation_meta_keep BEFORE DELETE ON draft_allocation_meta WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_immutable BEFORE UPDATE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER entry_allocation_meta_keep BEFORE DELETE ON entry_allocation_meta BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_definitions(id TEXT NOT NULL,version INTEGER NOT NULL CHECK(version>=0),label TEXT NOT NULL CHECK(length(label)<=256),scope INTEGER NOT NULL CHECK(scope IN(0,1)),input_field INTEGER NOT NULL CHECK(input_field IN(0,1,2)),expression TEXT NOT NULL CHECK(length(expression)<=4096),payee_template TEXT CHECK(payee_template IS NULL OR length(payee_template)<=65536),memo_template TEXT CHECK(memo_template IS NULL OR length(memo_template)<=65536),category_id TEXT CHECK(category_id IS NULL OR (length(category_id) BETWEEN 1 AND 256)),inherit_parent INTEGER NOT NULL CHECK(inherit_parent IN(0,1)),PRIMARY KEY(id,version),FOREIGN KEY(category_id) REFERENCES category_heads(id),CHECK(category_id IS NOT NULL),CHECK(inherit_parent=0),CHECK(scope<>1 OR payee_template IS NULL)) STRICT;
CREATE TRIGGER rule_definitions_immutable BEFORE UPDATE ON rule_definitions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_definitions_keep BEFORE DELETE ON rule_definitions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_heads(id TEXT PRIMARY KEY,version INTEGER NOT NULL,position INTEGER,enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),deleted INTEGER NOT NULL CHECK(deleted IN(0,1)),FOREIGN KEY(id,version) REFERENCES rule_definitions(id,version),CHECK((deleted=1 AND position IS NULL AND enabled=0) OR (deleted=0 AND position IS NOT NULL AND position>=0))) STRICT;
CREATE UNIQUE INDEX rule_head_position ON rule_heads(position) WHERE deleted=0;
CREATE TRIGGER rule_heads_update BEFORE UPDATE ON rule_heads WHEN NEW.id<>OLD.id OR OLD.deleted=1 OR NEW.version<>OLD.version AND NEW.version<>OLD.version+1 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_heads_keep BEFORE DELETE ON rule_heads BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_operation_requests(operation TEXT NOT NULL REFERENCES rule_operations(id),position INTEGER NOT NULL,rule TEXT NOT NULL,label TEXT NOT NULL CHECK(length(label)<=256),scope INTEGER NOT NULL CHECK(scope IN(0,1)),input_field INTEGER NOT NULL CHECK(input_field IN(0,1,2)),expression TEXT NOT NULL CHECK(length(expression)<=4096),payee_template TEXT CHECK(payee_template IS NULL OR length(payee_template)<=65536),memo_template TEXT CHECK(memo_template IS NULL OR length(memo_template)<=65536),category_id TEXT CHECK(category_id IS NULL OR (length(category_id) BETWEEN 1 AND 256)),inherit_parent INTEGER NOT NULL CHECK(inherit_parent IN(0,1)),enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),PRIMARY KEY(operation,position),UNIQUE(operation,rule),CHECK(category_id IS NOT NULL),CHECK(inherit_parent=0),CHECK(scope<>1 OR payee_template IS NULL)) STRICT;
CREATE TRIGGER rule_operation_requests_immutable BEFORE UPDATE ON rule_operation_requests BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_operation_requests_keep BEFORE DELETE ON rule_operation_requests BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE rule_snapshots(ruleset_revision INTEGER NOT NULL REFERENCES rule_operations(ruleset_after),position INTEGER NOT NULL,rule TEXT NOT NULL,version INTEGER NOT NULL,enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),PRIMARY KEY(ruleset_revision,position),UNIQUE(ruleset_revision,rule),FOREIGN KEY(rule,version) REFERENCES rule_definitions(id,version)) STRICT;
CREATE TRIGGER rule_snapshots_immutable BEFORE UPDATE ON rule_snapshots BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER rule_snapshots_keep BEFORE DELETE ON rule_snapshots BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE draft_rule_fields(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,payee_rule TEXT,payee_version INTEGER,memo_rule TEXT,memo_version INTEGER,ruleset_revision INTEGER NOT NULL CHECK(ruleset_revision>=1),PRIMARY KEY(batch,ordinal,revision),FOREIGN KEY(batch,ordinal,revision) REFERENCES draft_fields(batch,ordinal,revision),FOREIGN KEY(payee_rule,payee_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(ruleset_revision) REFERENCES rule_operations(ruleset_after),CHECK((payee_rule IS NULL)=(payee_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (payee_version IS NULL OR payee_version>=0) AND (memo_version IS NULL OR memo_version>=0))) STRICT;
CREATE TRIGGER draft_rule_fields_immutable BEFORE UPDATE ON draft_rule_fields BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_rule_fields_keep BEFORE DELETE ON draft_rule_fields WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE draft_rule_allocations(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,revision INTEGER NOT NULL,position INTEGER NOT NULL,category_rule TEXT,category_version INTEGER,memo_rule TEXT,memo_version INTEGER,PRIMARY KEY(batch,ordinal,revision,position),FOREIGN KEY(batch,ordinal,revision,position) REFERENCES draft_allocation_meta(batch,ordinal,revision,position),FOREIGN KEY(category_rule,category_version) REFERENCES rule_definitions(id,version),FOREIGN KEY(memo_rule,memo_version) REFERENCES rule_definitions(id,version),CHECK((category_rule IS NULL)=(category_version IS NULL) AND (memo_rule IS NULL)=(memo_version IS NULL) AND (category_version IS NULL OR category_version>=0) AND (memo_version IS NULL OR memo_version>=0) AND (category_rule IS NOT NULL OR memo_rule IS NOT NULL))) STRICT;
CREATE TRIGGER draft_rule_allocations_immutable BEFORE UPDATE ON draft_rule_allocations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER draft_rule_allocations_keep BEFORE DELETE ON draft_rule_allocations WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
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
CREATE VIEW entry_values AS SELECT e.batch,e.ordinal,e.revision,e.active,coalesce(e.date,v.date) AS date,coalesce(e.payee,v.payee) AS payee,coalesce(e.memo,v.memo) AS memo,coalesce(e.total,v.total) AS total FROM entry_revisions e LEFT JOIN versions v ON v.batch=e.batch AND v.ordinal=e.ordinal AND v.revision=e.draft_revision;
CREATE VIEW entry_allocation_values AS SELECT batch,ordinal,revision,position,category,memo,units FROM entry_allocations UNION ALL SELECT a.batch,a.ordinal,e.revision,a.position,CASE WHEN m.category_id IS NULL THEN a.category ELSE (SELECT c.name FROM category_versions c WHERE c.id=m.category_id AND c.catalog_revision<=m.catalog_revision ORDER BY c.revision DESC LIMIT 1) END,a.memo,a.units FROM entry_revisions e JOIN allocations a ON a.batch=e.batch AND a.ordinal=e.ordinal AND a.revision=e.draft_revision LEFT JOIN entry_allocation_meta m ON m.batch=e.batch AND m.ordinal=e.ordinal AND m.revision=e.revision AND m.position=a.position WHERE e.draft_revision IS NOT NULL;
CREATE TABLE accepted_transactions(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,operation TEXT NOT NULL REFERENCES operations(id),PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TABLE acceptance_members(operation TEXT NOT NULL REFERENCES operations(id),ordinal INTEGER NOT NULL,decision INTEGER NOT NULL CHECK(decision IN(1,2)),draft_revision INTEGER NOT NULL,target_batch TEXT,target_ordinal INTEGER,PRIMARY KEY(operation,ordinal)) STRICT;
CREATE TABLE dataset_acceptances(id TEXT PRIMARY KEY,transactions INTEGER NOT NULL,failed_records INTEGER NOT NULL,ignored INTEGER NOT NULL CHECK(ignored IN(0,1))) STRICT;
CREATE TABLE acceptance_operations(parent TEXT NOT NULL REFERENCES dataset_acceptances(id),operation TEXT NOT NULL UNIQUE REFERENCES ledger_operations(id),PRIMARY KEY(parent,operation)) STRICT;
CREATE TRIGGER register_acceptance BEFORE INSERT ON dataset_acceptances BEGIN INSERT INTO operation_registry VALUES(NEW.id,5); END;
CREATE TRIGGER acceptance_immutable BEFORE UPDATE ON dataset_acceptances BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER acceptance_keep BEFORE DELETE ON dataset_acceptances BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER accepted_transactions_immutable BEFORE UPDATE ON accepted_transactions BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER accepted_transactions_keep BEFORE DELETE ON accepted_transactions BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER acceptance_members_immutable BEFORE UPDATE ON acceptance_members BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER acceptance_members_keep BEFORE DELETE ON acceptance_members BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER acceptance_operations_immutable BEFORE UPDATE ON acceptance_operations BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TRIGGER acceptance_operations_keep BEFORE DELETE ON acceptance_operations BEGIN SELECT RAISE(ABORT,'immutable acceptance'); END;
CREATE TABLE mutation_generation(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision INTEGER NOT NULL CHECK(revision>=0)) STRICT;
INSERT INTO mutation_generation VALUES(1,0);
CREATE TABLE rollback_receipt(singleton INTEGER PRIMARY KEY CHECK(singleton=1),operation TEXT NOT NULL,account INTEGER NOT NULL,transactions INTEGER NOT NULL,failures INTEGER NOT NULL) STRICT;
CREATE TRIGGER rollback_operation_collision BEFORE INSERT ON operation_registry WHEN EXISTS(SELECT 1 FROM rollback_receipt WHERE operation=NEW.id) BEGIN SELECT RAISE(ABORT,'Operation id belongs to rollback'); END;
CREATE TABLE retained_opening_reviews(account INTEGER PRIMARY KEY REFERENCES account(singleton),opening INTEGER NOT NULL,opening_date TEXT NOT NULL) STRICT;
CREATE TABLE retained_category_names(name TEXT PRIMARY KEY) STRICT;
CREATE TABLE retained_account_aliases(account INTEGER NOT NULL REFERENCES account(singleton),name TEXT NOT NULL,PRIMARY KEY(account,name)) STRICT;
CREATE TABLE generated_source_matches(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,link TEXT NOT NULL REFERENCES transfer_heads(identity),account INTEGER NOT NULL REFERENCES account(singleton),sequence INTEGER NOT NULL REFERENCES ledger_operations(sequence),active INTEGER NOT NULL CHECK(active IN(0,1)),PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TRIGGER generated_match_identity BEFORE UPDATE OF batch,ordinal,link,account,sequence ON generated_source_matches BEGIN SELECT RAISE(ABORT,'immutable match identity'); END;
CREATE TRIGGER generated_match_keep BEFORE DELETE ON generated_source_matches WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Ownership cleanup required'); END;
CREATE TABLE retained_group_counts(group_id TEXT PRIMARY KEY REFERENCES import_groups(id),count INTEGER NOT NULL CHECK(count>0)) STRICT;
CREATE TRIGGER retained_group_update BEFORE UPDATE ON retained_group_counts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Cleanup required'); END;
CREATE TRIGGER retained_group_delete BEFORE DELETE ON retained_group_counts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Cleanup required'); END;
CREATE TABLE retained_source_counts(batch TEXT PRIMARY KEY REFERENCES batches(id),count INTEGER NOT NULL CHECK(count>=0)) STRICT;
CREATE TRIGGER retained_count_update BEFORE UPDATE ON retained_source_counts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Cleanup required'); END;
CREATE TRIGGER retained_count_delete BEFORE DELETE ON retained_source_counts WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'Cleanup required'); END;
CREATE TABLE import_file_errors(batch TEXT PRIMARY KEY REFERENCES batches(id)) STRICT;
CREATE TABLE import_completions(batch TEXT PRIMARY KEY REFERENCES batches(id)) STRICT;
CREATE VIEW review_targets AS SELECT r.batch,r.ordinal,
 CASE WHEN h.revision>0 THEN h.revision ELSE -1-coalesce(e.draft_revision,v.revision) END AS revision,
 CASE WHEN g.link IS NOT NULL THEN g.active WHEN h.revision IS NOT NULL THEN e.active WHEN b.state IN(0,1) OR (b.state=4 AND EXISTS(SELECT 1 FROM retained_transfer_rows k WHERE k.batch=r.batch AND k.ordinal=r.ordinal)) THEN 1 ELSE 0 END AS active
 FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal
 LEFT JOIN entry_heads h ON h.batch=r.batch AND h.ordinal=r.ordinal
 LEFT JOIN generated_source_matches g ON g.batch=r.batch AND g.ordinal=r.ordinal
 LEFT JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision
 WHERE v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=1
;
CREATE TABLE source_headers(source_id TEXT PRIMARY KEY REFERENCES sources(id),format TEXT NOT NULL CHECK(format IN('qif','ofx','qfx','qbo','aso')),grammar TEXT NOT NULL CHECK(grammar IN('qif','ofx-sgml','ofx-xml')),header TEXT NOT NULL) STRICT;
CREATE TABLE source_occurrences(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,format TEXT NOT NULL CHECK(format IN('qif','ofx','qfx','qbo','aso')),grammar TEXT NOT NULL CHECK(grammar IN('qif','ofx-sgml','ofx-xml')),begin_offset INTEGER NOT NULL,end_offset INTEGER NOT NULL,fitid TEXT NOT NULL,source_account_key TEXT NOT NULL,time_evidence TEXT NOT NULL,trntype TEXT NOT NULL,currency TEXT NOT NULL,original_units INTEGER,original_date TEXT NOT NULL,payee TEXT NOT NULL,memo TEXT NOT NULL,reference TEXT NOT NULL,uninterpreted TEXT NOT NULL,diagnostic INTEGER,blocked INTEGER NOT NULL CHECK(blocked IN(0,1)),PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch) REFERENCES batches(id)) STRICT;
CREATE TABLE source_associations(ledger_batch TEXT NOT NULL,ledger_ordinal INTEGER NOT NULL,source_batch TEXT NOT NULL,source_ordinal INTEGER NOT NULL,PRIMARY KEY(ledger_batch,ledger_ordinal,source_batch,source_ordinal),FOREIGN KEY(ledger_batch,ledger_ordinal) REFERENCES records(batch,ordinal),FOREIGN KEY(source_batch,source_ordinal) REFERENCES source_occurrences(batch,ordinal)) STRICT;
CREATE TRIGGER source_headers_immutable BEFORE UPDATE ON source_headers BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_headers_keep BEFORE DELETE ON source_headers WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_occurrences_immutable BEFORE UPDATE ON source_occurrences BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_occurrences_keep BEFORE DELETE ON source_occurrences WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE UNIQUE INDEX source_membership ON source_associations(source_batch,source_ordinal);
CREATE TRIGGER version_membership AFTER INSERT ON versions BEGIN
 DELETE FROM source_associations WHERE source_batch=NEW.batch AND source_ordinal=NEW.ordinal;
 INSERT INTO source_associations SELECT CASE WHEN NEW.decision=2 THEN NEW.duplicate_batch ELSE NEW.batch END,CASE WHEN NEW.decision=2 THEN NEW.duplicate_ordinal ELSE NEW.ordinal END,NEW.batch,NEW.ordinal WHERE NEW.decision IN(1,2);
END;
CREATE TRIGGER source_associations_immutable BEFORE UPDATE ON source_associations BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER source_associations_keep BEFORE DELETE ON source_associations WHEN spentit_cleanup()=0 AND (SELECT state FROM batches WHERE id=OLD.source_batch)=1 BEGIN SELECT RAISE(ABORT,'immutable'); END;
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
CREATE TRIGGER account_amount_keep BEFORE DELETE ON account_amount_revisions WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER batch_source_currency_immutable BEFORE UPDATE ON batch_source_currency BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER batch_source_currency_keep BEFORE DELETE ON batch_source_currency WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER account_name_command BEFORE UPDATE OF name ON account WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account name requires an explicit command'); END;
CREATE TRIGGER account_definition_command BEFORE UPDATE OF definition ON account WHEN spentit_account_command()=0 BEGIN SELECT RAISE(ABORT,'Account currency requires an explicit command'); END;
CREATE TRIGGER register_account BEFORE INSERT ON account_operations BEGIN INSERT INTO operation_registry VALUES(NEW.id,4); END;
CREATE TRIGGER order_account AFTER INSERT ON account_operations BEGIN INSERT INTO history_order(account_operation) VALUES(NEW.id); END;
)SQL";
bool schema3_batch(sqlite3* db) {
    const auto version=scalar(db,"PRAGMA user_version");
    return version=="3" || version=="4" || version=="17" || version=="19" || version=="45"
#ifdef SPENTIT_RULES_STORAGE
        || version=="6" || version=="16" || version=="18" || version=="20" || version=="46"
#endif
        ;
}
bool review_stamp_schema(sqlite3* db) {
    const auto version=scalar(db,"PRAGMA user_version");
    return version=="4" || version=="17" || version=="19" || version=="45"
#ifdef SPENTIT_RULES_STORAGE
        || version=="6" || version=="16" || version=="18" || version=="20" || version=="46"
#endif
        ;
}
app::BatchInfo batch(sqlite3* db,std::string_view id) {
    Statement s(db,schema3_batch(db)?"SELECT id,profile,state,revision,count,blocked,materialized_revision IS NULL FROM batches WHERE id=?":"SELECT id,profile,state,revision,count,blocked,0 FROM batches WHERE id=?"); s.text(1,id);
    if (!s.row()) fail(SQLITE_NOTFOUND,"Import does not exist");
    Statement completed(db,"SELECT 1 FROM import_completions WHERE batch=?");completed.text(1,id);
    return {s.bytes(0),static_cast<qif::DateOrder>(s.integer(1)),static_cast<app::BatchState>(s.integer(2)),s.integer(3),
        static_cast<std::size_t>(s.integer(4)),s.integer(5)!=0,s.integer(6)!=0,completed.row()};
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
bool generated_match(sqlite3* db,const imports::Occurrence& id){Statement row(db,"SELECT 1 FROM generated_source_matches WHERE batch=? AND ordinal=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));return row.row();}
std::size_t accepted_row_count(sqlite3* db,std::string_view id) {
    Statement row(db,"SELECT (SELECT count(*) FROM accepted_transactions WHERE batch=?1)+(SELECT count(*) FROM generated_source_matches WHERE batch=?1)");row.text(1,id);if(!row.row())fail(SQLITE_CORRUPT);return static_cast<std::size_t>(row.integer(0));
}
std::pair<std::int64_t,bool> review_target(sqlite3* db,const imports::Occurrence& id) {
    Statement row(db,"SELECT revision,active FROM review_targets WHERE batch=? AND ordinal=?");
    row.text(1,id.batch);row.integer(2,integer(id.ordinal));if(!row.row())fail(SQLITE_CONSTRAINT,"Duplicate target is no longer available");
    return {row.integer(0),row.integer(1)!=0};
}
bool review_target_active(sqlite3* db,const imports::Occurrence& id) {return review_target(db,id).second;}
void version(sqlite3* db,const imports::Occurrence& id,std::int64_t rev,const imports::DraftRow& row,const imports::Decision& decision,bool fresh_review=true,std::optional<std::int64_t> ruleset=std::nullopt) {
    Statement s(db,"INSERT INTO versions VALUES(?,?,?,?,?,?,?,?,?,?)");
    s.text(1,id.batch); s.integer(2,integer(id.ordinal)); s.integer(3,rev); s.text(4,row.date); s.text(5,row.payee); s.text(6,row.memo);
    s.integer(7,row.transaction.effective_total().units()); s.integer(8,static_cast<int>(decision.disposition));
    s.text(9,decision.duplicate_of ? decision.duplicate_of->batch : "");
    s.integer(10,decision.duplicate_of ? integer(decision.duplicate_of->ordinal) : -1); s.done();
    if(review_stamp_schema(db) && decision.duplicate_of) {
        std::int64_t target_revision=0,target_active=0,review_revision=rev;
        if(fresh_review) {
            Statement target(db,"SELECT revision,active FROM review_targets WHERE batch=? AND ordinal=?");
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
bool categories_format(sqlite3* db){const auto version=scalar(db,"PRAGMA user_version");return version=="17" || version=="19" || version=="45"
#ifdef SPENTIT_RULES_STORAGE
    || version=="6" || version=="16" || version=="18" || version=="20" || version=="46"
#endif
    ;}
bool rules_format(sqlite3* db){
#ifdef SPENTIT_RULES_STORAGE
    const auto version=scalar(db,"PRAGMA user_version");return version=="6" || version=="16" || version=="18" || version=="20" || version=="46";
#else
    (void)db;return false;
#endif
}
void retain_source_header(sqlite3* db,std::string_view id,std::string_view format,std::string_view grammar,std::string_view header) {
    Statement row(db,"INSERT INTO source_headers VALUES(?,?,?,?)");row.text(1,id);row.text(2,format);row.text(3,grammar);row.text(4,header);row.done();
}
// SQL retrieves a conservative superset. Only the engine decides whether the
// original facts make a duplicate candidate; format labels never enter policy.
void bind_candidate(Statement& statement,const qif::Record& record) {
    statement.text(1,record.date);
    if(record.original_amount)statement.integer(2,record.original_amount->units());
    const auto& primary=record.fitid.empty()?record.associated_fitid:record.fitid;
    statement.text(3,primary);
    statement.integer(5,std::any_of(record.associated_fitids.begin(),record.associated_fitids.end(),[&](const auto& id){return id!=primary;}));
}
constexpr const char* candidate_where =
    "b.account_id=spentit_account() AND b.state IN(0,1,4) AND r.batch<>?4 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=1 "
    "AND (?5=1 OR (?3<>'' AND EXISTS(SELECT 1 FROM source_associations a JOIN source_occurrences o ON o.batch=a.source_batch AND o.ordinal=a.source_ordinal WHERE a.ledger_batch=r.batch AND a.ledger_ordinal=r.ordinal AND o.fitid=?3)) "
    "OR ((r.original=?2 OR r.original IS NULL OR ?2 IS NULL) AND (r.source_date=?1 OR r.source_date='' OR ?1='')))";
void load_associated_ids(sqlite3* db,const imports::Occurrence& id,qif::Record& source) {
    source.associated_fitid.clear();source.associated_fitids.clear();
    Statement fits(db,"SELECT DISTINCT fitid FROM source_associations a JOIN source_occurrences o ON o.batch=a.source_batch AND o.ordinal=a.source_ordinal WHERE a.ledger_batch=? AND a.ledger_ordinal=? AND fitid<>'' ORDER BY fitid");
    fits.text(1,id.batch);fits.integer(2,integer(id.ordinal));
    while(fits.row()) {
        if(source.associated_fitids.size()==4096)fail(SQLITE_TOOBIG,"More than 4096 retained bank IDs on one transaction");
        source.associated_fitids.push_back(fits.bytes(0));
    }
    if(source.fitid.empty() && source.associated_fitids.size()==1)source.associated_fitid=source.associated_fitids.front();
}
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
imports::Occurrence review_survivor(sqlite3* db,imports::Occurrence id) {
    std::set<imports::Occurrence> seen;
    while(seen.insert(id).second){Statement m(db,"SELECT target_batch,target_ordinal FROM review_merges WHERE batch=? AND ordinal=?");m.text(1,id.batch);m.integer(2,integer(id.ordinal));if(!m.row())return id;id={m.bytes(0),static_cast<std::size_t>(m.integer(1))};}
    fail(SQLITE_CORRUPT,"Cyclic Review merge");
}
bool review_merged_source(sqlite3* db,const imports::Occurrence& id) {
    Statement s(db,"SELECT 1 FROM review_merges WHERE batch=?1 AND ordinal=?2 UNION ALL SELECT 1 FROM review_sources s WHERE source_batch=?1 AND source_ordinal=?2 AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=s.answer)");s.text(1,id.batch);s.integer(2,integer(id.ordinal));return s.row();
}
std::optional<imports::Occurrence> review_association(sqlite3* db,const imports::Occurrence& id){Statement s(db,"SELECT ledger_batch,ledger_ordinal FROM source_associations WHERE source_batch=? AND source_ordinal=?");s.text(1,id.batch);s.integer(2,integer(id.ordinal));if(!s.row())return {};return imports::Occurrence{s.bytes(0),static_cast<std::size_t>(s.integer(1))};}
bool confirmed_duplicate(sqlite3* db,const imports::Occurrence& id,const imports::Occurrence& target,std::int64_t revision) {
    // Suggestion policy may tighten; a recorded human decision is not thereby
    // corrupt. Use the most recent explicit decision for this exact occurrence.
    Statement s(db,"SELECT v.decision,v.duplicate_batch,v.duplicate_ordinal FROM versions v JOIN events e ON e.batch=v.batch AND e.revision=v.revision WHERE v.batch=? AND v.ordinal=? AND v.revision<=? AND e.kind='decide' ORDER BY v.revision DESC LIMIT 1");
    s.text(1,id.batch);s.integer(2,integer(id.ordinal));s.integer(3,revision);
    return s.row() && s.integer(0)==2 && s.bytes(1)==target.batch && s.integer(2)==integer(target.ordinal);
}
bool explicit_review_merge(sqlite3* db,const imports::Occurrence& id,std::int64_t revision) {
    Statement m(db,"SELECT 1 FROM review_members m JOIN review_answers a ON a.id=m.answer WHERE m.batch=? AND m.ordinal=? AND m.result=? AND m.committed=0 AND a.action=1");m.text(1,id.batch);m.integer(2,integer(id.ordinal));m.integer(3,revision);return m.row();
}
void verify_source_facts(sqlite3* db,const app::BatchInfo& b,const qif::Result& parsed,bool materialized) {
    Statement occurrences(db,"SELECT count(*) FROM source_occurrences WHERE batch=?");occurrences.text(1,b.id);if(!occurrences.row()) fail(SQLITE_CORRUPT);
    Statement associations(db,"SELECT count(*) FROM source_associations WHERE source_batch=?");associations.text(1,b.id);if(!associations.row()) fail(SQLITE_CORRUPT);
    if(!materialized) {
        if(occurrences.integer(0)!=0 || associations.integer(0)!=0) fail(SQLITE_CORRUPT,"Source facts exist before interpretation");
        return;
    }
    Statement retained_count(db,"SELECT count FROM retained_source_counts WHERE batch=?");retained_count.text(1,b.id);
    if(!retained_count.row() || occurrences.integer(0)!=retained_count.integer(0) || occurrences.integer(0)>integer(parsed.records.size())) fail(SQLITE_CORRUPT,"Source occurrence does not match its record");
    // The source side is not the leading key of source_associations. Read its
    // complete mapping once, preserving multiplicity checks, rather than scan
    // the association table again for every committed occurrence.
    std::map<std::size_t,imports::Occurrence> associated;
    {
        Statement links(db,"SELECT source_ordinal,ledger_batch,ledger_ordinal FROM source_associations WHERE source_batch=?");links.text(1,b.id);
        while(links.row()) {
            const auto ordinal=links.integer(0);
            if(ordinal<0 || static_cast<std::size_t>(ordinal)>=parsed.records.size() ||
               !associated.emplace(static_cast<std::size_t>(ordinal),imports::Occurrence{links.bytes(1),static_cast<std::size_t>(links.integer(2))}).second)
                fail(SQLITE_CORRUPT,"Committed source has an invalid or repeated evidence association");
        }
    }
    for(std::size_t i=0;i<parsed.records.size();++i) {
        const auto& source=parsed.records[i];
        Statement present(db,"SELECT 1 FROM source_occurrences WHERE batch=? AND ordinal=?");present.text(1,b.id);present.integer(2,integer(i));if(!present.row())continue;
        Statement row(db,"SELECT format,grammar,begin_offset,end_offset,fitid,source_account_key,time_evidence,trntype,currency,original_units,original_date,payee,memo,reference,uninterpreted,diagnostic,blocked FROM source_occurrences WHERE batch=? AND ordinal=?");
        row.text(1,b.id);row.integer(2,integer(i));if(!row.row() || !source.source_currency) fail(SQLITE_CORRUPT,"Source occurrence is missing");
        const auto format=source.format.empty()?"qif":source.format;const auto grammar=source.grammar.empty()?"qif":source.grammar;
        const bool same=row.bytes(0)==format && row.bytes(1)==grammar && row.integer(2)==integer(source.begin) && row.integer(3)==integer(source.end) && row.bytes(4)==source.fitid && row.bytes(5)==source.source_account_key && row.bytes(6)==source.time_evidence && row.bytes(7)==source.trntype && row.bytes(8)==source.source_currency->definition_id() && row.null(9)==!source.original_amount && (!source.original_amount || row.integer(9)==source.original_amount->units()) && row.bytes(10)==source.date && row.bytes(11)==source.payee && row.bytes(12)==source.memo && row.bytes(13)==source.reference && row.bytes(14)==source.uninterpreted && row.null(15)==!source.diagnostic && (!source.diagnostic || row.integer(15)==static_cast<int>(source.diagnostic->issue)) && row.integer(16)==(source.diagnostic && !source.repairable?1:0);
        if(!same) fail(SQLITE_CORRUPT,"Source occurrence does not match its record");
        if(b.state!=app::BatchState::committed) {
            const auto link=associated.find(i);
            if(b.state==app::BatchState::discarded || !retained_member(db,b.id,i)){if(link!=associated.end())fail(SQLITE_CORRUPT,"Cleared source has a membership");continue;}
            Statement v(db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? ORDER BY revision DESC LIMIT 1");v.text(1,b.id);v.integer(2,integer(i));
            if(!v.row() || v.integer(0)==0){if(link!=associated.end())fail(SQLITE_CORRUPT,"Unresolved source has a membership");continue;}
            if(link==associated.end() && (generated_match(db,{b.id,i}) || [&]{
                Statement cleared(db,"SELECT 1 FROM events WHERE batch=? AND kind='rollback'");cleared.text(1,b.id);return cleared.row();
            }())) {
                Statement owner(db,"SELECT 1 FROM source_associations WHERE ledger_batch=? AND ledger_ordinal=?");owner.text(1,b.id);owner.integer(2,integer(i));
                if(owner.row())continue;
            }
            if(link==associated.end() || link->second!=review_survivor(db,(v.integer(0)==1?imports::Occurrence{b.id,i}:imports::Occurrence{v.bytes(1),static_cast<std::size_t>(v.integer(2))})))fail(SQLITE_CORRUPT,"Invalid source membership");
            continue;
        }
        const auto link=associated.find(i);
        const bool kept=retained_member(db,b.id,i);
        Statement decision(db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=?1 AND ordinal=?2 AND revision=(SELECT max(revision) FROM versions WHERE batch=?1 AND ordinal=?2)");
        decision.text(1,b.id);decision.integer(2,integer(i));if(!decision.row() || decision.integer(0)==0) fail(SQLITE_CORRUPT,"Committed source has no review decision");
        if(!kept) {if(link!=associated.end()) fail(SQLITE_CORRUPT,"Cleared source is associated with a ledger transaction");continue;}
        if(link==associated.end()) fail(SQLITE_CORRUPT,"Committed source has no evidence association");
        const auto expected=review_survivor(db,decision.integer(0)==1?imports::Occurrence{b.id,i}:imports::Occurrence{decision.bytes(1),static_cast<std::size_t>(decision.integer(2))});
        if(link->second!=expected)fail(SQLITE_CORRUPT,"Source is not associated with its reviewed survivor");
    }
}

void store_entry(sqlite3*,const imports::Occurrence&,const app::EntrySnapshot&,std::optional<std::int64_t> = {});
void append_financial(sqlite3*,std::string_view,app::ChangeKind,const imports::Occurrence&,std::int64_t,std::int64_t,const Money&,const Money&,const Money&,std::string_view,std::int64_t,std::int64_t=0);
#include "account_money.inc"
}
// Bounded validated row heads for read-only access. Verification starts fresh;
// only its successful result can seed the connection cache. Transactional reads
// bypass it, and any connection write (including rollback) invalidates it.
struct VerificationRows {
    std::map<imports::Occurrence,app::RowView> rows;
    std::size_t bytes=0;
    static std::size_t text_size(const std::string& s){return s.capacity()+1;}
    static std::size_t currency_size(const Currency& c){return text_size(c.code())+text_size(c.definition_id());}
    static std::size_t allocations_size(const std::vector<Allocation>& values){
        std::size_t size=values.capacity()*sizeof(Allocation);
        for(const auto& a:values){
            size+=text_size(a.category)+text_size(a.memo)+text_size(a.identity)+currency_size(a.amount.currency());
            if(a.category_binding)size+=text_size(a.category_binding->identity);
            if(a.category_rule)size+=text_size(a.category_rule->identity);
            if(a.memo_rule)size+=text_size(a.memo_rule->identity);
        }
        return size;
    }
    static std::size_t transaction_size(const Transaction& t){
        return allocations_size(t.allocations())+currency_size(t.effective_total().currency())+
            (t.original_imported_amount()?currency_size(t.original_imported_amount()->currency()):0);
    }
    static std::size_t row_size(const app::RowView& row){
        // Includes container/node allowance and retained dynamic capacities.
        std::size_t size=sizeof(app::RowView)+sizeof(imports::Occurrence)+128+2*text_size(row.identity.batch);
        const auto& s=row.source;
        for(const auto* value:{&s.date,&s.payee,&s.memo,&s.reference,&s.cleared,&s.source_category,&s.source_class,
            &s.format,&s.grammar,&s.fitid,&s.source_account_key,&s.time_evidence,&s.trntype,&s.uninterpreted,&s.associated_fitid})size+=text_size(*value);
        for(const auto& id:s.associated_fitids)size+=sizeof(std::string)+text_size(id);
        size+=s.source_allocation_classes.capacity()*sizeof(std::string);
        for(const auto& value:s.source_allocation_classes)size+=text_size(value);
        size+=s.source_allocation_fields.capacity()*sizeof(qif::SourceAllocation);
        for(const auto& a:s.source_allocation_fields){size+=text_size(a.category)+text_size(a.memo)+text_size(a.class_label);if(a.amount)size+=currency_size(a.amount->currency());}
        size+=allocations_size(s.source_allocations);
        if(s.source_currency)size+=currency_size(*s.source_currency);
        if(s.original_amount)size+=currency_size(s.original_amount->currency());
        if(s.transaction)size+=transaction_size(*s.transaction);
        if(row.draft){const auto& d=*row.draft;size+=text_size(d.date)+text_size(d.payee)+text_size(d.memo)+transaction_size(d.transaction);if(d.payee_rule)size+=text_size(d.payee_rule->identity);if(d.memo_rule)size+=text_size(d.memo_rule->identity);}
        if(row.decision.duplicate_of)size+=text_size(row.decision.duplicate_of->batch);
        return size;
    }
    void remember(const app::RowView& row){
        constexpr std::size_t limit=128*1024*1024;
        if(rows.size()>=65536)return;
        const auto size=row_size(row);
        if(bytes>=limit || size>limit-bytes)return;
        if(rows.emplace(row.identity,row).second)bytes+=size;
    }
};

struct PreparedImportRow {
    std::string stamp;
    app::RowView row;
    std::string warning;
    std::string transfer_stamp,transfer_warning;
};
struct PreparedImport {
    std::int64_t revision=0;
    std::size_t included=0,excluded=0;
    std::vector<Money> amounts;
    std::map<std::size_t,PreparedImportRow> rows;
    std::string warning;
};
struct ImportStore::Impl {
    sqlite3* db=nullptr;
    int lock=-1;
    std::filesystem::path path;
    std::int64_t selected_account=1;
    int account_command=0;
    bool verifying=false;
    bool startup_cleanup=false;
    // An opening audit is reusable only in this process, with no intervening
    // writes/rollback and an identical full-file fingerprint under a read lock.
    mutable std::string opening_audit;
    sqlite3_int64 opening_audit_changes=-1;
    mutable VerificationRows* verification_rows=nullptr;
    struct AuditEntries {
        std::map<std::pair<imports::Occurrence,std::int64_t>,app::EntrySnapshot> values;
        std::size_t bytes=0;
    };
    mutable AuditEntries* audit_entries=nullptr;
    mutable VerificationRows read_rows;
    mutable sqlite3_int64 read_rows_changes=-1;
    mutable std::map<imports::Occurrence,std::string> read_stamps;
    mutable sqlite3_int64 alias_changes=-1;
    mutable std::vector<app::AccountAlias> alias_cache;
    mutable std::map<std::string,std::optional<std::int64_t>,std::less<>> alias_resolutions;
    mutable std::string category_source, binding_group;
    mutable std::vector<qif::CategoryDefinition> source_category_cache;
    mutable std::int64_t binding_epoch=-1;
    mutable std::map<std::string,std::pair<std::string,CategoryBinding>> imported_bindings;
    // One bounded, account-independent group snapshot. Connection change counts
    // include trigger writes and rolled-back writes; transactional reads and
    // verification always bypass it. The document owns an exclusive file lock.
    mutable std::optional<app::ImportGroup> group_snapshot;
    mutable sqlite3_int64 group_snapshot_changes=-1;
    std::map<std::string,PreparedImport,std::less<>> reviews;
    std::map<std::string,app::ImportGroup,std::less<>> reviewed_groups;
    std::map<std::string,std::pair<std::string,qif::Result>,std::less<>> reviewed_sources;
    mutable std::string reviewed_transfer_stamp;
    mutable std::optional<std::vector<app::TransferView>> reviewed_transfers;
    bool preparing_review=false;
    bool publishing_review=false;
    bool accepting_dataset=false;
    bool cleaning=false;
    bool composing_review=false;
    mutable std::optional<app::ReviewSheet> sheet_cache;
    mutable sqlite3_int64 sheet_changes=-1;
    mutable std::map<imports::Occurrence,app::ReviewTransaction> link_rows;
    mutable std::map<imports::Occurrence,std::map<imports::Occurrence,bool>> link_edges;
    mutable std::string link_alias_stamp;
    mutable std::map<imports::Occurrence,std::set<std::string>> link_occupied;
    sqlite3_int64 review_changes=-1;
    ImportObserver observer;
    ~Impl() { if (db) sqlite3_close_v2(db); if(lock>=0) ::close(lock); }

};
ImportStore::ImportStore(std::unique_ptr<Impl> p):impl_(std::move(p)){}
ImportStore::~ImportStore()=default;
ImportStore::ImportStore(ImportStore&&) noexcept=default;
ImportStore& ImportStore::operator=(ImportStore&&) noexcept=default;
void ImportStore::observer(ImportObserver f) { impl_->observer=std::move(f); }
ImportStore ImportStore::connect(const std::filesystem::path& path, std::string_view password, bool create, const app::OperationContext* context,bool overwrite,const std::filesystem::path& cache) {
    if(context) context->checkpoint("Opening file");
    if (password.empty() || password.size() > 1024) fail(SQLITE_MISUSE, "Password length is invalid");
    if (!create) {
        struct stat input{};
        if (::lstat(path.c_str(), &input)!=0 || !S_ISREG(input.st_mode) || input.st_nlink!=1)
            fail(SQLITE_CANTOPEN, "Dataset file not found, inaccessible, or not a supported regular file");
    }
    auto impl = std::make_unique<Impl>();
    impl->path=path;
    // Use a separate lock inode: on some macOS filesystems SQLite itself uses
    // flock on the database. Never delete the lock file or steal a live lock.
    // An empty leftover file is harmless; kernel ownership ends on process exit.
    auto lock_path=path; lock_path+=".lock";
    impl->lock=::open(lock_path.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW,0600);
    if (impl->lock<0 || ::flock(impl->lock,LOCK_EX|LOCK_NB)!=0) fail(SQLITE_BUSY,"Document is already open or unavailable");
    struct stat owned_lock{};
    if(::fstat(impl->lock,&owned_lock)!=0 || !S_ISREG(owned_lock.st_mode) || owned_lock.st_nlink!=1 || owned_lock.st_uid!=::geteuid())
        fail(SQLITE_CANTOPEN,"Document lock is not a supported regular file");
    if(create) {
        struct stat previous{};
        if(overwrite && ::lstat(path.c_str(),&previous)==0) {
            if(!S_ISREG(previous.st_mode) || previous.st_nlink!=1)fail(SQLITE_CANTOPEN,"Dataset is not a supported regular file");
            // Explicit creation-time replacement discards the old database and its journals.
            // Keep the document lock throughout so another SpentIt instance cannot use it.
            for(const auto* suffix:{"-journal","-wal","-shm"}) {
                const auto sidecar=path.string()+suffix;
                if(::unlink(sidecar.c_str())!=0 && errno!=ENOENT)fail(SQLITE_IOERR_DELETE,"Cannot remove the old dataset journal");
            }
            const int fd=::open(path.c_str(),O_WRONLY|O_NOFOLLOW);
            if(fd<0)fail(SQLITE_CANTOPEN,"Cannot overwrite dataset");
            struct stat opened{};
            const bool same=::fstat(fd,&opened)==0 && opened.st_dev==previous.st_dev && opened.st_ino==previous.st_ino && opened.st_nlink==1;
            if(!same || ::fchmod(fd,0600)!=0 || ::ftruncate(fd,0)!=0) {::close(fd);fail(SQLITE_IOERR,"Cannot overwrite dataset");}
            const int synced=::fsync(fd);const int closed=::close(fd);
            if(synced!=0 || closed!=0)fail(SQLITE_IOERR,"Dataset replacement could not be completed");
        } else exclusive_file(path);
    }
    struct stat st{}, lock_st{};
    if (::stat(path.c_str(),&st)!=0 || !S_ISREG(st.st_mode) || st.st_nlink!=1 ||
        ::fstat(impl->lock,&lock_st)!=0 || !S_ISREG(lock_st.st_mode) || lock_st.st_nlink!=1 || lock_st.st_uid!=::geteuid())
        fail(SQLITE_CANTOPEN,"Document or lock is not a supported regular file");
    const bool clean_input=verification_cache::no_journals(path);
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
    ok(sqlite3_create_function_v2(db,"spentit_cleanup",0,SQLITE_UTF8,impl.get(),
        [](sqlite3_context* ctx,int,sqlite3_value**){sqlite3_result_int(ctx,static_cast<Impl*>(sqlite3_user_data(ctx))->cleaning?1:0);},nullptr,nullptr,nullptr));
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
        constexpr const char* expected_format="46";
#else
        constexpr const char* expected_format="45";
#endif
        if(scalar(db,"PRAGMA application_id")!="1397770577" || scalar(db,"PRAGMA user_version")!=expected_format)
            fail(SQLITE_SCHEMA,"This dataset format is not supported by this version of SpentIt; the file has not been changed");
        if(scalar(db,"SELECT count(*) FROM pragma_table_info('batches') WHERE name='account_id'")!="1" ||
           scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='account_creations'")!="1")
            fail(SQLITE_SCHEMA,"Account structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('transfer_events','transfer_versions','transfer_heads','retained_transfer_rows')")!="4")fail(SQLITE_SCHEMA,"Transfer structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('source_headers','source_occurrences','source_associations')")!="3")fail(SQLITE_SCHEMA,"Document structure does not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='dataset_preferences'")!="1")fail(SQLITE_SCHEMA,"Dataset preferences do not match format version");
        if(scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('review_answers','review_undos','review_undo_members','review_stack','review_members','review_choices','review_sources','review_merges','review_link_history','link_exclusion_events')")!="10")fail(SQLITE_SCHEMA,"Review structure does not match format version");
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
    // Bound the connection page cache at a 32 MiB target to avoid repeatedly
    // decrypting the same working set. Spill, HMAC, secure memory and durability
    // settings remain enabled; this is not a persistent format change.
    exec(db,"PRAGMA cache_size=-32768");
    exec(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=EXTRA; PRAGMA fullfsync=ON;"
             "PRAGMA foreign_keys=ON; PRAGMA temp_store=MEMORY; PRAGMA secure_delete=ON");
    for (const auto& setting : {std::pair{"PRAGMA journal_mode", "delete"},
         {"PRAGMA cache_size", "-32768"}, {"PRAGMA synchronous", "3"}, {"PRAGMA fullfsync", "1"}, {"PRAGMA foreign_keys", "1"},
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
        exec(db, review_sheet_schema);
        exec(db,"CREATE TABLE dataset_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton=1),date_format TEXT NOT NULL,default_categories INTEGER NOT NULL CHECK(default_categories IN(0,1))) STRICT; CREATE TRIGGER dataset_preferences_immutable BEFORE UPDATE ON dataset_preferences BEGIN SELECT RAISE(ABORT,'Dataset setup is immutable'); END; CREATE TRIGGER dataset_preferences_keep BEFORE DELETE ON dataset_preferences BEGIN SELECT RAISE(ABORT,'Dataset setup is immutable'); END;");
        transaction.commit();
        sync_directory(path);
    }
    sqlite3_rollback_hook(db,[](void* pointer){
        auto& state=*static_cast<Impl*>(pointer);
        state.opening_audit.clear();
        state.reviews.clear();state.reviewed_groups.clear();state.reviewed_sources.clear();state.review_changes=-1;
        state.read_rows=VerificationRows{};state.read_stamps.clear();
        state.reviewed_transfers.reset();state.reviewed_transfer_stamp.clear();
    },store.impl_.get());
    if (!create) {store.impl_->selected_account=smallest_active_account(store.impl_->db);
        bool approved=false;
        if(clean_input && !cache.empty()) {
            if(context)context->checkpoint("Checking saved verification");
            exec(db,"BEGIN");
            try {
                (void)scalar(db,"SELECT count(*) FROM account"); // retain SQLite read lock during fingerprinting
                approved=verification_cache::matches(path,cache,context);
                exec(db,"COMMIT");
            } catch(...) {sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
        }
        if(!approved) {
            if(cache.empty())store.verify(context);
            else {
                exec(db,"BEGIN");
                try {
                    (void)scalar(db,"SELECT count(*) FROM account");
                    const auto before=verification_cache::fingerprint(path,context);
                    store.verify(context);
                    const auto after=verification_cache::fingerprint(path,context);
                    if(!before.empty() && before==after) {
                        store.impl_->opening_audit=after;
                        store.impl_->opening_audit_changes=sqlite3_total_changes64(db);
                    }
                    exec(db,"COMMIT");
                } catch(...) {sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
            }
        }
        if(context)context->checkpoint("Removing unfinished import");
        store.impl_->startup_cleanup=store.cleanup_incomplete_imports();
        store.prepare_review();}
    return store;
}
ImportStore ImportStore::create(const std::filesystem::path& path,std::string_view password,std::string_view name,const Currency& currency,std::string_view today,bool overwrite,const app::DatasetOptions& options) {
    validate_date_format(options.date_format);
    safe_text(name); if(name.empty()) fail(SQLITE_MISUSE,"Account name is required"); iso_date(today);
    // New accounts must use current selection; persisted definitions remain readable.
    if (Currency(currency.code())!=currency) fail(SQLITE_MISUSE,"Currency cannot be selected for a new account");
    auto store=connect(path,password,true,nullptr,overwrite);
    Atomic a(store.impl_->db);
    Statement s(store.impl_->db,"INSERT INTO account VALUES(1,?,?,?,0,0,0,0)"); s.text(1,name); s.text(2,currency.definition_id()); s.text(3,today); s.done();
    Statement receipt(store.impl_->db,"INSERT INTO account_creations VALUES(1,?,?,?)");
    receipt.text(1,name);receipt.text(2,currency.definition_id());receipt.text(3,today);receipt.done();
    Statement preferences(store.impl_->db,"INSERT INTO dataset_preferences VALUES(1,?,?)");preferences.text(1,options.date_format);preferences.integer(2,options.default_categories?1:0);preferences.done();
    if(options.default_categories)store.initialize_categories();
    store.finish([&]{a.commit();}); store.verify(); return store;
}
std::string ImportStore::date_format() const {
    Statement row(impl_->db,"SELECT date_format,default_categories FROM dataset_preferences WHERE singleton=1");
    if(!row.row() || (row.integer(1)!=0 && row.integer(1)!=1))fail(SQLITE_CORRUPT,"Dataset preferences are missing or invalid");
    auto format=row.bytes(0);validate_date_format(format);return format;
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
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
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
    require_account_name(name,id);
    Statement row(impl_->db,"INSERT INTO account VALUES(?,?,?,?,0,0,0,0)");
    row.integer(1,id);row.text(2,name);row.text(3,currency.definition_id());row.text(4,today);row.done();
    Statement receipt(impl_->db,"INSERT INTO account_creations VALUES(?,?,?,?)");
    receipt.integer(1,id);receipt.text(2,name);receipt.text(3,currency.definition_id());receipt.text(4,today);receipt.done();
    finish([&]{transaction.commit();});return account_by_id(id);
}
ImportStore ImportStore::open_cached(const std::filesystem::path& path,std::string_view password,const std::filesystem::path& cache,const app::OperationContext* context) {
    return connect(path,password,false,context,false,cache);
}
void ImportStore::certify(const std::filesystem::path& cache,const app::OperationContext* context) {
    if(cache.empty())return;
    // Keep the document and SQLite read locks until audit and publication finish.
    exec(impl_->db,"BEGIN");
    try {
        (void)scalar(impl_->db,"SELECT count(*) FROM account");
        // If no bytes changed since a previous successful audit, it remains valid.
        if(!verification_cache::matches(impl_->path,cache,context)) {
            const auto audited=verification_cache::fingerprint(impl_->path,context);
            const bool reuse=!audited.empty() && audited==impl_->opening_audit &&
                impl_->opening_audit_changes==sqlite3_total_changes64(impl_->db);
            impl_->opening_audit.clear();
            if(!reuse)verify(context);
            verification_cache::publish(impl_->path,cache,audited,context);
        }
        exec(impl_->db,"COMMIT");
    } catch(...) {sqlite3_exec(impl_->db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
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
    finish([&]{a.commit();});
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
    if(!candidates && impl_->verification_rows) {
        const auto found=impl_->verification_rows->rows.find(id);
        if(found!=impl_->verification_rows->rows.end())return found->second;
    }
    require_account(id.batch);
    if(!candidates && !impl_->verifying && (impl_->preparing_review || impl_->publishing_review || impl_->review_changes==sqlite3_total_changes64(impl_->db))) {
        const auto batch=impl_->reviews.find(id.batch);
        if(batch!=impl_->reviews.end()) {
            const auto row=batch->second.rows.find(id.ordinal);
            if(row!=batch->second.rows.end())return row->second.row;
        }
    }
    auto* rows=impl_->verification_rows;
    std::string stamp;
    if(!candidates) {
        Statement state(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");
        state.text(1,id.batch);state.integer(2,integer(id.ordinal));
        if(state.row() && !state.null(0))stamp=std::to_string(state.integer(0));
        const auto old=impl_->read_stamps.find(id);
        const auto cached=impl_->read_rows.rows.find(id);
        if(!rows && cached!=impl_->read_rows.rows.end() && (old==impl_->read_stamps.end() || old->second!=stamp)) {
            impl_->read_rows.bytes-=VerificationRows::row_size(cached->second);
            impl_->read_rows.rows.erase(cached);
        }
        if(!rows)rows=&impl_->read_rows;
    }
    if(!candidates && rows) {
        const auto found=rows->rows.find(id);
        if(found!=rows->rows.end()) {
            auto& cached=found->second;
            cached.matched_generated=generated_match(impl_->db,id);
            const auto prior_size=VerificationRows::row_size(cached);
            if(!impl_->verification_rows) {
                cached.excluded_by_clear=!retained_member(impl_->db,id.batch,id.ordinal);
                load_associated_ids(impl_->db,id,cached.source);
                if(cached.decision.duplicate_of && review_merged_source(impl_->db,id)){cached.decision.duplicate_of=review_association(impl_->db,id);cached.review_stale=false;}
                else if(cached.decision.duplicate_of) {
                    const auto& target=*cached.decision.duplicate_of;
                    Statement stale(impl_->db,"SELECT s.target_revision!=h.revision OR s.target_active!=h.active FROM review_stamps s JOIN review_targets h ON h.batch=? AND h.ordinal=? WHERE s.batch=? AND s.ordinal=? ORDER BY s.revision DESC LIMIT 1");
                    stale.text(1,target.batch);stale.integer(2,integer(target.ordinal));stale.text(3,id.batch);stale.integer(4,integer(id.ordinal));
                    if(!stale.row())fail(SQLITE_CORRUPT);cached.review_stale=stale.integer(0)!=0;
                }
            }
            rows->bytes=rows->bytes-prior_size+VerificationRows::row_size(cached);
            return cached;
        }
    }
    const auto b=batch(impl_->db,id.batch); const auto source_currency=pinned_currency(impl_->db,id.batch);
    Statement src(impl_->db,"SELECT raw FROM records WHERE batch=? AND ordinal=?"); src.text(1,id.batch); src.integer(2,integer(id.ordinal));
    if(!src.row()) {
        auto parsed=parsed_source(id.batch);
        if(id.ordinal>=parsed.records.size()) fail(SQLITE_NOTFOUND);
        auto source=std::move(parsed.records[id.ordinal]);
        Statement present(impl_->db,"SELECT 1 FROM source_occurrences WHERE batch=? AND ordinal=?");present.text(1,id.batch);present.integer(2,integer(id.ordinal));
        if(b.state!=app::BatchState::captured && !present.row()){source.repairable=false;return {id,std::move(source),{},{imports::Disposition::unresolved,{}},{},false,true};}
        if(scalar(impl_->db,"PRAGMA user_version")=="1") source.repairable=false;
        return {id,std::move(source),{}, {imports::Disposition::unresolved,{}},{}};
    }
    Statement parser(impl_->db,"SELECT parser FROM batches WHERE id=?");parser.text(1,id.batch);if(!parser.row())fail(SQLITE_CORRUPT);
    const auto parsed=parser.bytes(0)==qif::parser_version ? qif::parse(src.bytes(0),b.profile,source_currency,{},source_categories(id.batch)) : statements::parse_occurrence(src.bytes(0),parser.bytes(0),source_currency,b.profile);
    if(parsed.diagnostic || parsed.records.size()!=1 || (!parsed.records[0].transaction && !parsed.records[0].repairable)) fail(SQLITE_CORRUPT);
    auto source=parsed.records[0]; overlay_occurrence(impl_->db,id,source);
    load_associated_ids(impl_->db,id,source);
    app::RowView out{id,std::move(source),{}, {},{}};out.matched_generated=generated_match(impl_->db,id);
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
    if(impl_->observer)impl_->observer(ImportPhase::validate_row);
    out.decision.disposition=static_cast<imports::Disposition>(v.integer(5));
    if(out.decision.disposition==imports::Disposition::duplicate) out.decision.duplicate_of=imports::Occurrence{v.bytes(6),static_cast<std::size_t>(v.integer(7))};
    const bool merged=review_merged_source(impl_->db,id);
    if(merged && out.decision.disposition==imports::Disposition::duplicate)out.decision.duplicate_of=review_association(impl_->db,id);
    if(committed_changes_supported() && out.decision.duplicate_of && !merged) {
        Statement target(impl_->db,"SELECT s.target_revision,s.target_active,h.revision,h.active FROM review_stamps s JOIN review_targets h ON h.batch=? AND h.ordinal=? WHERE s.batch=? AND s.ordinal=? AND s.revision=?");
        target.text(1,out.decision.duplicate_of->batch);target.integer(2,integer(out.decision.duplicate_of->ordinal));target.text(3,id.batch);target.integer(4,integer(id.ordinal));target.integer(5,v.integer(0));
        if(!target.row())fail(SQLITE_CORRUPT);out.review_stale=target.integer(0)!=target.integer(2) || target.integer(1)!=target.integer(3);
    }
    if (!candidates) {if(rows){rows->remember(out);if(rows->rows.contains(id))impl_->read_stamps[id]=std::move(stamp);}return out;}
    out.candidates=duplicate_candidates(out.source,id.batch,false);
    if(!impl_->verifying && impl_->review_changes==sqlite3_total_changes64(impl_->db)) {
        const auto reviewed=impl_->reviews.find(id.batch);
        if(reviewed!=impl_->reviews.end()) {
            const auto row=reviewed->second.rows.find(id.ordinal);
            if(row!=reviewed->second.rows.end() && row->second.row.review_stale)out.review_stale=true;
        }
    }
    return out;
}
std::vector<imports::Occurrence> ImportStore::duplicate_candidates(const qif::Record& source,std::string_view batch_id,bool first_only) const {
    const auto sql=std::string("SELECT r.batch,r.ordinal FROM batches b CROSS JOIN records r ON r.batch=b.id CROSS JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE ")+candidate_where+" ORDER BY r.batch,r.ordinal";
    Statement candidates(impl_->db,sql.c_str());bind_candidate(candidates,source);candidates.text(4,batch_id);
    std::vector<imports::Occurrence> result;
    std::size_t scanned=0;
    while(candidates.row()) {
        if(++scanned>1000000)fail(SQLITE_TOOBIG,"Duplicate review exceeds one million candidate comparisons");
        const imports::Occurrence target{candidates.bytes(0),static_cast<std::size_t>(candidates.integer(1))};
        if(!imports::candidate_basis(source,load_row(target,false).source))continue;
        if(result.size()==4096)fail(SQLITE_TOOBIG,"More than 4096 possible duplicate occurrences; review requires a narrower source import");
        result.push_back(target);if(first_only)break;
    }
    return result;
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
    Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)"); evidence.text(1,id); evidence.blob(2,bytes); evidence.done();
    retain_source_header(impl_->db,id,"qif","qif","");
    Statement insert(impl_->db,schema3_batch(impl_->db)?"INSERT INTO batches VALUES(?,?,?,0,0,?,?,0,spentit_account(),?1,NULL)":"INSERT INTO batches VALUES(?,?,?,0,0,?,?)");
    insert.text(1,id); insert.integer(2,static_cast<int>(profile)); insert.text(3,qif::parser_version);
    insert.integer(4,integer(parsed.records.size())); insert.integer(5,parsed.valid()?0:1); insert.done();
    pin_source_currency(impl_->db,id,acct.currency);
    populate(id,bytes,parsed,0,nullptr);
    Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'ingest')"); event.text(1,id); event.done();
    generate_explicit_transfers();
    finish([&]{a.commit();}); return batch(impl_->db,id);
}
app::BatchInfo ImportStore::ingest_controlled(std::string_view id,std::string_view bytes,qif::DateOrder profile,app::OperationContext& context) {
    if(!schema3_batch(impl_->db)) {
        context.seal();context.checkpoint("Saving imported transactions");return ingest(id,bytes,profile);
    }
    identifier(id);context.checkpoint("Checking QIF resource limits and format");
    const auto acct=account();require_writable_account(impl_->db,acct.identity);
    auto parsed=qif::parse(bytes,profile,acct.currency,[&]{context.checkpoint();});
    if(!parsed.admissible) fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    // Existing evidence is navigated to, never implicitly resumed.
    Statement prior(impl_->db,"SELECT batches.id,profile,bytes FROM batches JOIN sources ON sources.id=batches.source_id WHERE batches.id=?");
    prior.text(1,id);
    if(prior.row()) {
        require_account(prior.bytes(0));
        if(prior.integer(1)!=static_cast<int>(profile) || prior.bytes(2)!=bytes)
            fail(SQLITE_CONSTRAINT,"Import identity conflicts or another import needs review");
        context.seal();return batch(impl_->db,prior.bytes(0));
    }
    context.checkpoint("Reading imported transactions");
    try {
        Atomic transaction(impl_->db);
        Statement evidence(impl_->db,"INSERT INTO sources VALUES(?,?)");evidence.text(1,id);evidence.blob(2,bytes);evidence.done();
        retain_source_header(impl_->db,id,"qif","qif","");
        Statement insert(impl_->db,"INSERT INTO batches VALUES(?,?,?,3,0,?,1,NULL,spentit_account(),?1,NULL)");
        insert.text(1,id);insert.integer(2,static_cast<int>(profile));insert.text(3,qif::parser_version);insert.integer(4,integer(parsed.records.size()));insert.done();
        pin_source_currency(impl_->db,id,acct.currency);
        Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'capture')");event.text(1,id);event.done();
        context.seal();context.checkpoint("Finishing import");finish([&]{transaction.commit();});diag::durable(diag::Code::capture);
    } catch (...) {
        const auto failure=std::current_exception();
        if(!sqlite3_get_autocommit(impl_->db)){context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
        try {
            Statement saved(impl_->db,"SELECT id FROM batches WHERE id=?");saved.text(1,id);
            if(saved.row()) {diag::durable(diag::Code::capture);context.state.store(app::OperationContext::sealed);return batch(impl_->db,id);}
        } catch (...) {context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
        std::rethrow_exception(failure);
    }
    // Reopen only a sealed gate after known durable capture. No accepted request
    // can be lost here, because request() cannot change sealed to requested.
    auto sealed=app::OperationContext::sealed;
    if(!context.state.compare_exchange_strong(sealed,app::OperationContext::available)) fail(SQLITE_MISUSE);
    context.observeState();
    parsed={};
    try {
        context.checkpoint("Import saved");
        return resume(id,0,context);
    } catch(const app::OperationCancelled&) {
        cleanup_incomplete_imports(id);
        throw app::OperationCancelled("Import cancelled. You can import the file again.");
    }
}
app::BatchInfo ImportStore::resume(std::string_view id,std::int64_t expected,app::OperationContext& context) {
    if(!schema3_batch(impl_->db)) fail(SQLITE_SCHEMA,"Resuming import review is not supported by this dataset");
    require_account(id);require_writable_account(impl_->db,account_for_batch(id).identity);
    Statement grouped(impl_->db,"SELECT 1 FROM import_group_accounts WHERE batch=?");grouped.text(1,id);if(grouped.row())fail(SQLITE_CONSTRAINT,"Resume all source accounts together");
    if(expected<0)fail(SQLITE_CONSTRAINT,"Invalid import revision");
    context.checkpoint("Importing transactions");
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
            generate_explicit_transfers(&context);
            context.seal();context.checkpoint("Finishing import");finish([&]{transaction.commit();});diag::durable(diag::Code::prepare);
        } catch (...) {
            const auto failure=std::current_exception();
            if(!sqlite3_get_autocommit(impl_->db)){context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
            try {
                Statement saved(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");saved.text(1,id);
                if(!saved.row()) fail(SQLITE_CORRUPT);
                if(!saved.null(0) && saved.integer(0)==expected+1) {diag::durable(diag::Code::prepare);context.state.store(app::OperationContext::sealed);return batch(impl_->db,id);}
            } catch (...) {context.state.store(app::OperationContext::sealed);throw app::OperationOutcomeUnknown();}
            std::rethrow_exception(failure);
        }
        return batch(impl_->db,id);
    } catch(const app::OperationCancelled&) {
        cleanup_incomplete_imports(id);
        throw app::OperationCancelled("Import cancelled. You can import the file again.");
    }
}
void ImportStore::populate(std::string_view id,std::string_view bytes,const qif::Result& parsed,std::int64_t revision,const app::OperationContext* context) {
    diag::Span phaseSpan(diag::Code::import_populate);
    ScopedStatements plans(impl_->db);
    const auto profile=batch(impl_->db,id).profile;
    Statement retained_count(impl_->db,"INSERT INTO retained_source_counts VALUES(?,?)");retained_count.text(1,id);retained_count.integer(2,integer(parsed.records.size()));retained_count.done();
    if(parsed.diagnostic){Statement error(impl_->db,"INSERT OR IGNORE INTO import_file_errors VALUES(?)");error.text(1,id);error.done();}
    std::string identical;
    Statement prior(impl_->db,"SELECT batches.id FROM batches JOIN sources ON sources.id=batches.source_id WHERE account_id=spentit_account() AND state IN(0,1) AND batches.id!=?3 AND profile=?1 AND bytes=?2 AND source_account IS (SELECT source_account FROM batches WHERE id=?3) ORDER BY batches.rowid DESC LIMIT 1");
    prior.integer(1,static_cast<int>(profile)); prior.blob(2,bytes); prior.text(3,id); if(prior.row()) identical=prior.bytes(0);
    Statement sequence(impl_->db,"SELECT coalesce(max(sequence),0) FROM records"); if(!sequence.row()) fail(SQLITE_CORRUPT);
    auto seq=sequence.integer(0);
    std::map<std::pair<std::string,std::int64_t>,bool> candidate_groups;
    std::unordered_map<std::string,std::size_t> fitid_counts;
    for(const auto& record:parsed.records)if(!record.fitid.empty())++fitid_counts[record.fitid];
    struct BankMatch { const qif::Record* source=nullptr;std::optional<imports::Occurrence> target;bool conflict=false; };
    std::unordered_map<std::string,BankMatch> bank_matches;
    for(const auto& record:parsed.records)if(!record.fitid.empty() && fitid_counts.at(record.fitid)==1)
        bank_matches.emplace(record.fitid,BankMatch{&record,{},false});
    // Walk retained bank associations once, not once per imported row. Several
    // exports can support one ledger identity. Any conflicting occurrence or
    // second identity with the same ID prevents automatic matching.
    if(!bank_matches.empty()) {
        Statement matches(impl_->db,R"SQL(SELECT a.ledger_batch,a.ledger_ordinal,a.source_batch,a.source_ordinal,o.fitid
          FROM source_associations a JOIN source_occurrences o
          ON o.batch=a.source_batch AND o.ordinal=a.source_ordinal
          JOIN batches b ON b.id=a.ledger_batch
          WHERE b.account_id=spentit_account() AND b.state IN(0,1,4) AND o.fitid<>'')SQL");
        while(matches.row()) {
            if(context)context->checkpoint();
            const auto found=bank_matches.find(matches.bytes(4));
            if(found==bank_matches.end() || found->second.conflict)continue;
            auto& match=found->second;
            const imports::Occurrence candidate{matches.bytes(0),static_cast<std::size_t>(matches.integer(1))};
            if(match.target && *match.target!=candidate){match.conflict=true;continue;}
            match.target=candidate;
            const auto evidence=load_row({matches.bytes(2),static_cast<std::size_t>(matches.integer(3))},false);
            if(!imports::same_bank_transaction(*match.source,evidence.source))match.conflict=true;
        }
    }
    std::set<imports::Occurrence> automatic_targets;
    struct ExactMatch {
        std::size_t incoming=0;
        std::optional<imports::Occurrence> target;
        bool ambiguous=false,has_bank_id=false,conflicting_ids=false;
    };
    std::map<imports::ReimportKey,ExactMatch> exact_matches;
    for(const auto& record:parsed.records)
        if(const auto key=imports::reimport_key(record))++exact_matches[*key].incoming;
    // Scan original ledger source facts once. Several attached exports still
    // represent one target; their IDs must not be bypassed by the ID-less rule.
    Statement exact(impl_->db,R"SQL(SELECT r.batch,r.ordinal,o.original_date,o.currency,o.original_units,o.payee,
        (SELECT count(DISTINCT s.fitid) FROM source_associations a JOIN source_occurrences s
          ON s.batch=a.source_batch AND s.ordinal=a.source_ordinal
          WHERE a.ledger_batch=r.batch AND a.ledger_ordinal=r.ordinal AND s.fitid<>'')
      FROM batches b JOIN records r ON r.batch=b.id
      JOIN source_associations attached ON attached.ledger_batch=r.batch AND attached.ledger_ordinal=r.ordinal
      JOIN source_occurrences o ON o.batch=attached.source_batch AND o.ordinal=attached.source_ordinal
      JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal
      WHERE b.account_id=spentit_account() AND b.state IN(0,1,4) AND b.id<>?
        AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal)
        AND v.decision=1 AND o.diagnostic IS NULL AND o.original_units IS NOT NULL
        AND o.original_date<>'' AND o.payee<>'')SQL");
    exact.text(1,id);
    while(exact.row()) {
        if(context)context->checkpoint();
        const auto found=exact_matches.find({exact.bytes(2),exact.bytes(3),exact.integer(4),exact.bytes(5)});
        if(found==exact_matches.end())continue;
        auto& match=found->second;
        const imports::Occurrence target{exact.bytes(0),static_cast<std::size_t>(exact.integer(1))};
        if(match.target && *match.target!=target)match.ambiguous=true;
        match.target=target;
        match.has_bank_id=match.has_bank_id || exact.integer(6)!=0;
        match.conflicting_ids=match.conflicting_ids || exact.integer(6)>1;
    }
    const auto exact_target=[&](const qif::Record& source)->std::optional<imports::Occurrence> {
        const auto key=imports::reimport_key(source);
        if(!key)return {};
        const auto& match=exact_matches.at(*key);
        if(match.incoming!=1 || match.ambiguous || match.conflicting_ids || !match.target)return {};
        if(!source.fitid.empty()) {
            // Both sides have IDs: only the strict bank-ID path may decide.
            // A conflicting occurrence elsewhere with this ID also blocks fallback.
            const auto bank=bank_matches.find(source.fitid);
            if(match.has_bank_id || (bank!=bank_matches.end() &&
               (bank->second.target || bank->second.conflict)))return {};
        }
        if(automatic_targets.contains(*match.target) || !review_target_active(impl_->db,*match.target))return {};
        return match.target;
    };
    const auto automatic_target=[&](const qif::Record& source)->std::optional<imports::Occurrence> {
        const auto found=bank_matches.find(source.fitid);
        if(found==bank_matches.end() || found->second.conflict)return {};
        const auto& target=found->second.target;
        if(target && (automatic_targets.contains(*target) ||
           !review_target_active(impl_->db,*target)))return {};
        return target;
    };
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
        if(!r.fitid.empty() && fitid_counts.at(r.fitid)>1) {
            // Even the first occurrence needs review when the incoming source
            // repeats an ID; do not silently consume one and leave the rest.
            decision.disposition=imports::Disposition::unresolved;
        } else if(!identical.empty() && r.fitid.empty()) {
            const auto original=load_row({identical,i},false);
            if(original.decision.disposition==imports::Disposition::unresolved || original.review_stale)decision.disposition=imports::Disposition::unresolved;
            else {
                decision.disposition=imports::Disposition::duplicate;
                decision.duplicate_of=original.decision.duplicate_of ? original.decision.duplicate_of : std::optional(imports::Occurrence{identical,i});
            }
        } else if(!r.fitid.empty()) {
            if(const auto target=automatic_target(r)) {
                decision={imports::Disposition::duplicate,target};
            } else if(const auto target=exact_target(r)) {
                decision={imports::Disposition::duplicate,target};
            } else { if(!duplicate_candidates(r,id,true).empty()) decision.disposition=imports::Disposition::unresolved; }
        } else if(const auto target=exact_target(r)) {
            decision={imports::Disposition::duplicate,target};
        } else {
            const auto key=std::pair{r.date,r.transaction->original_imported_amount()->units()};
            const auto found=candidate_groups.find(key);
            if(found!=candidate_groups.end()) {
                if(found->second) decision.disposition=imports::Disposition::unresolved;
            } else {
            const bool has_candidate=!duplicate_candidates(r,id,true).empty();
            candidate_groups.emplace(key,has_candidate);
            if(has_candidate) decision.disposition=imports::Disposition::unresolved;
            }
        }
        if(decision.duplicate_of && !review_target_active(impl_->db,*decision.duplicate_of))decision={imports::Disposition::unresolved,{}};
        if(decision.duplicate_of && !automatic_targets.insert(*decision.duplicate_of).second)
            decision={imports::Disposition::unresolved,{}};
        auto transaction=*r.transaction;
        const auto account_currency=account_for_batch(id).currency;
        if(transaction.effective_total().currency()!=account_currency) transaction=rescale_draft(imports::DraftRow{r.date,r.payee,r.memo,transaction},account_currency).transaction;
        version(impl_->db,{std::string(id),i},revision,{r.date,r.payee,r.memo,transaction,FieldOwnership::source,FieldOwnership::source},decision);
        if(context && impl_->observer)impl_->observer(ImportPhase::materialize_row);
    }
}
void ImportStore::save(const imports::Occurrence& id,std::int64_t expected,const imports::DraftRow& edit) {
    if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Use Review Undo to restore a merged transaction");
    ScopedStatements plans(impl_->db);
    require_unlinked(id);require_writable_account(impl_->db,account_for_batch(id.batch).identity);
    Atomic a(impl_->db); expect(impl_->db,id.batch,expected); if(generated_match(impl_->db,id))fail(SQLITE_CONSTRAINT,"Edit the existing transfer rather than its matched source");const auto old=load_row(id,false);
    const auto canonical=canonical_draft(id,edit,old.draft,expected+1,false);
    const bool reassigned=old.source.source_currency && canonical.transaction.effective_total().currency()!=*old.source.source_currency;
    imports::validate_draft(old.source,canonical,reassigned);
    if(expected==INT64_MAX) fail(SQLITE_FULL);
    if(!old.draft) {
        if(scalar(impl_->db,"PRAGMA user_version")=="1" || !old.source.repairable)
            fail(SQLITE_CONSTRAINT,"Record repair is not supported by this dataset");
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
    finish([&]{a.commit();});
}
void ImportStore::decide(const imports::Occurrence& id,std::int64_t expected,const imports::Decision& decision) {
    if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Use Review Undo to restore a merged transaction");
    require_unlinked(id);require_writable_account(impl_->db,account_for_batch(id.batch).identity);
    Atomic a(impl_->db); expect(impl_->db,id.batch,expected);if(generated_match(impl_->db,id))fail(SQLITE_CONSTRAINT,"This source already matches an accepted transfer"); const auto old=row(id);
    if(!old.draft) fail(SQLITE_CONSTRAINT,"Save the record repair before duplicate review");
    imports::validate_decisions({old.candidates},std::span(&decision,1),false);
    if(decision.duplicate_of) {
        Statement used(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND ordinal!=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) "
            "AND decision=2 AND duplicate_batch=? AND duplicate_ordinal=? LIMIT 1");
        used.text(1,id.batch); used.integer(2,integer(id.ordinal)); used.text(3,decision.duplicate_of->batch); used.integer(4,integer(decision.duplicate_of->ordinal));
        if(used.row()) fail(SQLITE_CONSTRAINT,"That prior occurrence is already matched in this import");
    }
    if(expected==INT64_MAX) fail(SQLITE_FULL);
    version(impl_->db,id,expected+1,*old.draft,decision); advance(impl_->db,id.batch,expected,"decide"); finish([&]{a.commit();});
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
    finish([&]{a.commit();});
}
std::optional<app::Receipt> ImportStore::durable_outcome(std::string_view operation) const {
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
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
    ScopedStatements plans(impl_->db);
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
    if(auto prior=outcome(operation)) {
        if(prior->batch!=id || prior->revision!=expected)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        return *prior;
    }
    // Normally prepared by the preceding mutation/open. Rebuild only after a
    // failed command rolled back and discarded its speculative review state.
    if(impl_->review_changes!=sqlite3_total_changes64(impl_->db))prepare_review();
    const auto reviewed=impl_->reviews.find(std::string(id));
    if(reviewed==impl_->reviews.end())fail(SQLITE_CONSTRAINT,"No pending import review");
    const auto& ready=reviewed->second;
    if(ready.revision!=expected)fail(SQLITE_CONSTRAINT,"Import changed since review");
    if(!ready.warning.empty())fail(SQLITE_CONSTRAINT,ready.warning.c_str());
    struct PublicationGuard {bool& flag;~PublicationGuard(){flag=false;}} publication{impl_->publishing_review};
    impl_->publishing_review=true;
    Atomic a(impl_->db);
    if(committed_changes_supported() && !rules_format(impl_->db)) {Statement collision(impl_->db,"SELECT kind FROM ledger_operations WHERE id=?");collision.text(1,operation);if(collision.row() && collision.integer(0)!=0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");}
    if(auto prior=outcome(operation)) {
        if(prior->batch!=id || prior->revision!=expected) fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        a.commit(); return *prior;
    }
    const auto result=publish_review(id,expected,operation);
    finish([&]{a.commit();});return result;
}
app::Receipt ImportStore::publish_review(std::string_view id,std::int64_t expected,std::string_view operation) {
    const auto ready=impl_->reviews.at(std::string(id));
    expect(impl_->db,id,expected); start_import_groups({std::string(id)},operation);publish_group_opening(impl_->selected_account); const auto b=batch(impl_->db,id);
    const auto acct=account();
    const auto before=committed_balance();
    const auto included=ready.included,excluded=ready.excluded;
    for(const auto& [ordinal,item]:ready.rows) {
        const auto& row=item.row;
        if(row.excluded_by_clear)continue;
        const auto target=row.decision.duplicate_of.value_or(imports::Occurrence{std::string(id),ordinal});
        Statement link(impl_->db,"INSERT OR IGNORE INTO source_associations VALUES(?,?,?,?)");
        link.text(1,target.batch);link.integer(2,integer(target.ordinal));link.text(3,id);link.integer(4,integer(ordinal));link.done();
    }
    const auto adjustment=transfer_import_transition(id,operation,true);
    const auto change=imports::sum(acct.currency,ready.amounts)+adjustment; const auto after=before+change;
    Statement op(impl_->db,"INSERT INTO operations VALUES(?,?,?,?,?,?,?,?)"); op.text(1,operation); op.text(2,id); op.integer(3,expected);
    op.integer(4,integer(included)); op.integer(5,integer(excluded)); op.integer(6,before.units()); op.integer(7,change.units()); op.integer(8,after.units()); op.done();
    advance(impl_->db,id,expected,b.blocked?"accept":"commit",b.blocked?0:1);
    exec(impl_->db,"UPDATE account SET frozen=1 WHERE singleton=spentit_account()");
    if(committed_changes_supported()) {
        for(const auto& [ordinal,item]:ready.rows)if(!item.row.excluded_by_clear) {
            const auto& row=item.row;
            Statement member(impl_->db,"INSERT INTO acceptance_members SELECT ?1,?2,?3,max(revision),?4,?5 FROM versions WHERE batch=?6 AND ordinal=?2");
            member.text(1,operation);member.integer(2,integer(ordinal));member.integer(3,static_cast<int>(row.decision.disposition));
            if(row.decision.duplicate_of){member.text(4,row.decision.duplicate_of->batch);member.integer(5,integer(row.decision.duplicate_of->ordinal));}member.text(6,id);member.done();
            if(row.decision.disposition==imports::Disposition::include) {
                store_entry(impl_->db,{std::string(id),ordinal},{0,true,bind_import_categories(id,*row.draft)});
                Statement accepted(impl_->db,"INSERT INTO accepted_transactions VALUES(?,?,?)");accepted.text(1,id);accepted.integer(2,integer(ordinal));accepted.text(3,operation);accepted.done();
            }
        }
        append_financial(impl_->db,operation,app::ChangeKind::publication,{std::string(id),0},-1,0,before,change,after,"",0);
    }
    return {std::string(operation),std::string(id),expected,included,excluded,before,change,after};
}
std::vector<app::LedgerRow> ImportStore::ledger(bool projected) const { return ledger_rows(projected,false); }
std::vector<app::LedgerRow> ImportStore::current_ledger() const { return ledger_rows(true,true); }
std::vector<app::LedgerRow> ImportStore::ledger_rows(bool projected,bool current) const {
    ScopedStatements read_plans(impl_->db);
    if(committed_changes_supported()) {
        if(projected && !current && !projected_balance())fail(SQLITE_CONSTRAINT,"Projection is incomplete; review the pending import");
        std::vector<app::LedgerRow> result;
        Statement entries(impl_->db,"SELECT h.batch,h.ordinal,h.revision,r.sequence FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) JOIN records r ON r.batch=h.batch AND r.ordinal=h.ordinal WHERE h.batch IN(SELECT id FROM batches WHERE account_id=spentit_account()) AND e.active=1 ORDER BY e.date,r.sequence");
        while(entries.row()){imports::Occurrence identity{entries.bytes(0),static_cast<std::size_t>(entries.integer(1))};result.push_back({identity,entries.integer(3),entry_snapshot(identity,entries.integer(2)).draft,true});}
        if(projected){Statement pending(impl_->db,"SELECT r.batch,r.ordinal,r.sequence FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE b.account_id=spentit_account() AND b.state IN(0,4) AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=v.batch AND t.ordinal=v.ordinal) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=v.batch AND g.ordinal=v.ordinal) AND (v.decision=1 OR (?1=1 AND v.decision=0)) AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal)");pending.integer(1,current?1:0);while(pending.row()){imports::Occurrence identity{pending.bytes(0),static_cast<std::size_t>(pending.integer(1))};result.push_back({identity,pending.integer(2),*load_row(identity,false).draft,false});}}
        const auto local=local_transfer_ledger(projected);result.insert(result.end(),local.begin(),local.end());
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.draft.date,a.sequence)<std::tie(b.draft.date,b.sequence);});return result;
    }
    if(projected && !current && !projected_balance()) fail(SQLITE_CONSTRAINT,"Projection is incomplete; resolve the pending import first");
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
Money ImportStore::balance_for(std::int64_t id) const {
    const auto acct=account_by_id(id);
    std::vector<Money> amounts{acct.opening,local_transfer_total(id,true)};
    Statement rows(impl_->db,
        "SELECT e.total FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) "
        "WHERE h.batch IN(SELECT id FROM batches WHERE account_id=?1) AND e.active=1 "
        "UNION ALL SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch "
        "WHERE b.account_id=?1 AND b.state IN(0,4) "
        "AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) "
        "AND NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=v.batch AND t.ordinal=v.ordinal) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=v.batch AND g.ordinal=v.ordinal) AND v.decision IN(0,1) AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    rows.integer(1,id);
    while(rows.row())amounts.emplace_back(rows.integer(0),acct.currency);
    return imports::sum(acct.currency,amounts);
}
Money ImportStore::balance() const { return balance_for(impl_->selected_account); }
std::vector<std::pair<std::int64_t,Money>> ImportStore::balances() const {
    ScopedStatements plans(impl_->db);
    Statement ids(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM retired_import_accounts r WHERE r.account_id=singleton) AND NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=singleton) ORDER BY singleton");
    std::vector<std::pair<std::int64_t,Money>> result;
    while(ids.row())result.emplace_back(ids.integer(0),balance_for(ids.integer(0)));
    return result;
}
Money ImportStore::committed_for(std::int64_t id) const {
    const auto acct=account_by_id(id);std::vector<Money> values{acct.opening,local_transfer_total(id,false)};
    Statement rows(impl_->db,"SELECT e.total FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) WHERE h.batch IN(SELECT id FROM batches WHERE account_id=?) AND e.active=1");
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
        if(!impl_->verifying && impl_->review_changes==sqlite3_total_changes64(impl_->db)) {
            const auto reviewed=impl_->reviews.find(b.id);
            if(reviewed!=impl_->reviews.end() && !reviewed->second.warning.empty())return {};
        }
        Statement s(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=0 AND (NOT EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch) OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) LIMIT 1");
        s.text(1,b.id); if(s.row()) return {};
    }
    if(committed_changes_supported()) {
        Statement stale(impl_->db,"SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch JOIN review_stamps s USING(batch,ordinal,revision) JOIN review_targets h ON h.batch=v.duplicate_batch AND h.ordinal=v.duplicate_ordinal WHERE b.account_id=? AND b.state=0 AND v.decision=2 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) AND (s.target_revision!=h.revision OR s.target_active!=h.active) LIMIT 1");
        stale.integer(1,id);if(stale.row())return {};
    }
    const auto a=account_by_id(id);std::vector<Money> amounts{committed_changes_supported()?committed_for(id):a.opening,local_transfer_total(id,true)-local_transfer_total(id,false)};
    Statement s(impl_->db,committed_changes_supported()?"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.account_id=? AND b.state IN(0,4) AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=v.batch AND t.ordinal=v.ordinal) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=v.batch AND g.ordinal=v.ordinal) AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)":"SELECT v.total FROM versions v JOIN batches b ON b.id=v.batch WHERE b.account_id=? AND b.state IN(0,1) AND v.decision=1 "
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
    impl_->opening_audit.clear();
    diag::Span verificationSpan(diag::Code::verify);
    (void)date_format();
    ScopedStatements read_plans(impl_->db);
    struct VerificationGuard { bool& value; bool prior; ~VerificationGuard(){value=prior;} } guard{impl_->verifying,impl_->verifying};
    impl_->verifying=true;
    impl_->read_rows=VerificationRows{};impl_->read_rows_changes=-1;impl_->read_stamps.clear();
    VerificationRows verified_rows;
    Impl::AuditEntries entries;
    struct EntriesGuard {
        Impl& state;Impl::AuditEntries* prior;
        ~EntriesGuard(){state.audit_entries=prior;}
    } entries_guard{*impl_,impl_->audit_entries};
    impl_->audit_entries=&entries;
    struct RowsGuard {
        Impl& state;VerificationRows& rows;VerificationRows* prior;
        int exceptions=std::uncaught_exceptions();
        ~RowsGuard(){
            state.verification_rows=prior;
            if(std::uncaught_exceptions()!=exceptions){state.reviews.clear();state.reviewed_groups.clear();state.reviewed_sources.clear();state.reviewed_transfers.reset();state.review_changes=-1;state.read_stamps.clear();}
            if(!prior && std::uncaught_exceptions()==exceptions && sqlite3_get_autocommit(state.db)){
                state.read_rows=std::move(rows);state.read_rows_changes=sqlite3_total_changes64(state.db);
            }
        }
    } rows_guard{*impl_,verified_rows,impl_->verification_rows};
    impl_->verification_rows=&verified_rows;
    if(context) context->checkpoint("Checking file");
    {diag::Span integritySpan(diag::Code::integrity);
    if(scalar(impl_->db,"PRAGMA integrity_check")!="ok") fail(SQLITE_CORRUPT);
    for(const char* sql:{"PRAGMA cipher_integrity_check","PRAGMA foreign_key_check"}) { Statement s(impl_->db,sql); if(s.row()) fail(SQLITE_CORRUPT); }
    }
    std::vector<app::Account> all_accounts;Statement account_ids(impl_->db,"SELECT singleton FROM account ORDER BY singleton");while(account_ids.row())all_accounts.push_back(account_by_id(account_ids.integer(0)));if(all_accounts.empty() || all_accounts.front().identity!=1)fail(SQLITE_CORRUPT);
    for(const auto& acct:all_accounts){safe_text(acct.name);iso_date(acct.opening_date);if(acct.name.empty())fail(SQLITE_CORRUPT);}
    Statement creation(impl_->db,"SELECT 1 FROM account a LEFT JOIN account_creations c ON c.identity=a.singleton WHERE c.identity IS NULL");if(creation.row())fail(SQLITE_CORRUPT);
    for(const auto& acct:all_accounts){
        Statement original(impl_->db,"SELECT opening_date FROM account_creations WHERE identity=?");original.integer(1,acct.identity);if(!original.row())fail(SQLITE_CORRUPT);iso_date(original.bytes(0));
        if(acct.opening_confirmed!=(acct.revision>0) || (acct.revision==0 && (acct.opening.units()!=0 || acct.opening_date!=original.bytes(0))))fail(SQLITE_CORRUPT,"Account opening does not match its creation");
    }
    {diag::Span phaseSpan(diag::Code::accounts);verify_account_commands(context);}
    {diag::Span phaseSpan(diag::Code::groups);verify_import_groups(context);}
    Statement invalid(impl_->db,
        "SELECT 1 FROM batches b WHERE parser NOT IN('bank-v6','ofx-sgml-1','ofx-xml-2') OR (state=1 AND NOT EXISTS(SELECT 1 FROM operations o WHERE o.batch=b.id)) "
        "OR revision!=(SELECT max(revision) FROM events WHERE batch=b.id) OR revision+1!=(SELECT count(*) FROM events WHERE batch=b.id) "
        "UNION ALL SELECT 1 FROM versions v JOIN batches b ON b.id=v.batch WHERE v.revision>b.revision "
        "UNION ALL SELECT 1 FROM sources s WHERE NOT EXISTS(SELECT 1 FROM batches b WHERE b.source_id=s.id) "
        "UNION ALL SELECT 1 FROM account a WHERE frozen!=(EXISTS(SELECT 1 FROM operations o JOIN batches b ON b.id=o.batch WHERE b.account_id=a.singleton) OR EXISTS(SELECT 1 FROM transfer_events t WHERE t.account=a.singleton AND t.did_commit=1))");
    if(invalid.row()) fail(SQLITE_CORRUPT,"Invalid import history relationships");
    Statement headers(impl_->db,"SELECT 1 FROM sources s WHERE NOT EXISTS(SELECT 1 FROM source_headers h WHERE h.source_id=s.id) UNION ALL SELECT 1 FROM batches b JOIN source_headers h ON h.source_id=b.source_id WHERE NOT ((b.parser='bank-v6' AND h.format='qif' AND h.grammar='qif') OR (b.parser='ofx-sgml-1' AND h.grammar='ofx-sgml' AND h.format IN('ofx','qfx','qbo','aso')) OR (b.parser='ofx-xml-2' AND h.grammar='ofx-xml' AND h.format IN('ofx','qfx','qbo','aso')))");
    if(headers.row()) fail(SQLITE_CORRUPT,"Source evidence does not match its import");
    Statement all_batches(impl_->db,"SELECT id FROM batches ORDER BY rowid");
    while(all_batches.row()) {
        diag::Span evidenceSpan(diag::Code::evidence);
        const auto b=batch(impl_->db,all_batches.bytes(0));const auto acct=account_for_batch(b.id);
        if(context) context->checkpoint("Checking imported transactions");
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
        std::size_t valid_count=0; Statement file_error(impl_->db,"SELECT 1 FROM import_file_errors WHERE batch=?");file_error.text(1,b.id);bool blocked=file_error.row();
        for(std::size_t i=0;i<parsed.records.size();++i) {
            if(context) context->checkpoint();
            const auto& source=parsed.records[i];
            Statement retained(impl_->db,"SELECT 1 FROM source_occurrences WHERE batch=? AND ordinal=?");retained.text(1,b.id);retained.integer(2,integer(i));if(!retained.row())continue;
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
                    const bool reviewed=explicit_review_merge(impl_->db,{b.id,i},vv.integer(0));
                    if(account_for_batch(target.batch).identity!=acct.identity || (target.batch==b.id && !reviewed)) fail(SQLITE_CORRUPT);
                    const auto other=load_row(target,false);
                    if(!reviewed && !review_merged_source(impl_->db,{b.id,i}) && ((other.decision.disposition!=imports::Disposition::include && review_survivor(impl_->db,target)==target) || !imports::candidate_basis(source,other.source,confirmed_duplicate(impl_->db,{b.id,i},target,vv.integer(0))?imports::MatchContext::confirmed_decision:imports::MatchContext::suggestion))) fail(SQLITE_CORRUPT);
                }
            }
        }
        verify_source_facts(impl_->db,b,parsed,true);
        if(count.integer(0)!=integer(valid_count) || (retained_batch(impl_->db,b.id)?b.blocked:blocked!=b.blocked)) fail(SQLITE_CORRUPT);
        Statement duplicates(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=2 AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) "
            "GROUP BY duplicate_batch,duplicate_ordinal HAVING count(*)>1"); duplicates.text(1,b.id); if(duplicates.row()) fail(SQLITE_CORRUPT);
    }
    verify_link_exclusions();
    if(categories_supported()){diag::Span phaseSpan(diag::Code::categories);verify_categories(context);}
    if(rules_format(impl_->db)){diag::Span phaseSpan(diag::Code::rules);verify_rules(context);}
    if(committed_changes_supported()){{diag::Span phaseSpan(diag::Code::transfers);verify_transfers(context);}{diag::Span phaseSpan(diag::Code::committed);verify_committed(context);}if(bulk_supported()){diag::Span phaseSpan(diag::Code::bulk);verify_bulk(context);}if(rules_format(impl_->db)){diag::Span phaseSpan(diag::Code::reapply);verify_rule_reapply(context);}return;}
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
    if(context) context->checkpoint("Preparing backup");
    auto candidate=destination; candidate+=".partial";
    {
        auto target=connect(candidate,password,true);
        sqlite3_backup* backup=sqlite3_backup_init(target.impl_->db,"main",impl_->db,"main");
        if(!backup) fail(sqlite3_errcode(target.impl_->db));
        int step=SQLITE_OK;
        try {
            do {
                if(context) context->checkpoint("Copying backup");
                step=sqlite3_backup_step(backup,128);
            } while(step==SQLITE_OK);
        } catch (...) { sqlite3_backup_finish(backup); throw; }
        const int finish=sqlite3_backup_finish(backup);
        if(step!=SQLITE_DONE) fail(step); ok(finish);
        if(context) context->checkpoint("Checking backup");
        target.verify(context);
    }
    if(context) context->checkpoint("Checking backup");
    { auto restored=open(candidate,password,context); restored.verify(context); }
    if(observer) observer(BackupPhase::before_publish);
    if(context) { context->checkpoint("Finishing backup"); context->seal(); }
    if(::link(candidate.c_str(),destination.c_str())!=0) fail(SQLITE_CANTOPEN,"Backup publication failed; encrypted candidate retained");
    if(observer) observer(BackupPhase::before_publish_sync);
    sync_directory(destination);
    if(observer) observer(BackupPhase::before_cleanup);
    if(::unlink(candidate.c_str())!=0) fail(SQLITE_IOERR_DELETE,"Backup published; candidate cleanup failed");
    if(observer) observer(BackupPhase::before_cleanup_sync);
    sync_directory(destination);
    if(observer) observer(BackupPhase::complete);
}
#include "account_aliases.inc"
#include "category_store.inc"
#include "committed_store.inc"
#include "bulk_store.inc"
namespace { std::int64_t review_generation(sqlite3*); }
#include "rule_store.inc"
#include "rule_reapply_store.inc"
#include "transfer_store.inc"
#include "import_category_decisions.inc"
#include "import_group_store.inc"
#include "account_lifecycle.inc"
#include "import_review.inc"
#include "dataset_acceptance.inc"
#include "account_rollback.inc"
#include "incomplete_import_cleanup.inc"
#include "review_sheet_store.inc"
#include "ledger_selection_store.inc"
#include "rule_account_store.inc"
} // namespace spentit::storage
