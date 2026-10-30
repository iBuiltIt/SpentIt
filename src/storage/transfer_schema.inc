constexpr const char* transfer_schema=R"SQL(
CREATE TABLE retained_transfer_rows(batch TEXT NOT NULL,ordinal INTEGER NOT NULL,PRIMARY KEY(batch,ordinal),FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal)) STRICT;
CREATE TRIGGER retained_transfer_rows_keep BEFORE DELETE ON retained_transfer_rows WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER retained_transfer_rows_immutable BEFORE UPDATE ON retained_transfer_rows BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TABLE transfer_events(operation TEXT PRIMARY KEY REFERENCES ledger_operations(id) DEFERRABLE INITIALLY DEFERRED,account INTEGER NOT NULL REFERENCES account(singleton),request BLOB NOT NULL,did_commit INTEGER NOT NULL CHECK(did_commit IN(0,1)),local_change INTEGER NOT NULL) STRICT;
CREATE TABLE transfer_versions(identity TEXT NOT NULL,sequence INTEGER NOT NULL REFERENCES ledger_operations(sequence) DEFERRABLE INITIALLY DEFERRED,payload BLOB NOT NULL,PRIMARY KEY(identity,sequence)) STRICT;
CREATE TABLE transfer_heads(identity TEXT PRIMARY KEY,sequence INTEGER NOT NULL,FOREIGN KEY(identity,sequence) REFERENCES transfer_versions(identity,sequence)) STRICT;
CREATE TRIGGER transfer_events_immutable BEFORE UPDATE ON transfer_events BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER transfer_events_keep BEFORE DELETE ON transfer_events WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER transfer_versions_immutable BEFORE UPDATE ON transfer_versions BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER transfer_versions_keep BEFORE DELETE ON transfer_versions WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER transfer_heads_keep BEFORE DELETE ON transfer_heads WHEN spentit_cleanup()=0 BEGIN SELECT RAISE(ABORT,'immutable'); END;
CREATE TRIGGER transfer_head_step BEFORE UPDATE ON transfer_heads WHEN spentit_cleanup()=0 AND (NEW.identity!=OLD.identity OR NEW.sequence<=OLD.sequence) BEGIN SELECT RAISE(ABORT,'invalid transfer head'); END;
)SQL";
