#pragma once
// Synthetic schema-2 fixture definition preserved from the pre-schema-3 implementation.
// Schema 1 has the same table shape; its verifier disallows first repairs.
inline constexpr const char* legacy_import_schema=R"SQL(
PRAGMA application_id=1397770577;
PRAGMA user_version=2;
CREATE TABLE account(singleton INTEGER PRIMARY KEY CHECK(singleton=1),name TEXT NOT NULL,definition TEXT NOT NULL,
 opening_date TEXT NOT NULL,opening INTEGER NOT NULL,frozen INTEGER NOT NULL CHECK(frozen IN(0,1)),revision INTEGER NOT NULL CHECK(revision>=0),confirmed INTEGER NOT NULL CHECK(confirmed IN(0,1))) STRICT;
CREATE TABLE sources(id TEXT PRIMARY KEY,bytes BLOB NOT NULL) STRICT;
CREATE TABLE batches(id TEXT PRIMARY KEY REFERENCES sources(id),profile INTEGER NOT NULL CHECK(profile IN(0,1)),parser TEXT NOT NULL,
 state INTEGER NOT NULL CHECK(state IN(0,1,2)),revision INTEGER NOT NULL CHECK(revision>=0),
 count INTEGER NOT NULL CHECK(count BETWEEN 0 AND 200000),blocked INTEGER NOT NULL CHECK(blocked IN(0,1))) STRICT;
CREATE UNIQUE INDEX single_pending ON batches(state) WHERE state=0;
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
 kind TEXT NOT NULL CHECK(kind IN('ingest','save','decide','discard','commit','opening')),UNIQUE(batch,revision)) STRICT;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.count!=OLD.count OR OLD.state!=0 OR NEW.revision!=OLD.revision+1
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
)SQL";
