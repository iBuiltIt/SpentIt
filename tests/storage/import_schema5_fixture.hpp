#pragma once
// Exact synthetic schema-5 definition captured before rule schema changes.
inline constexpr const char* schema5_import_fixture=R"SQL(
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
