// A restored row stays in its original import. Publication still owns its
// historical membership, while the restoration authorizes a current draft.
constexpr const char* release_v2 = R"SQL(
CREATE TABLE restored_import_rows(
  answer TEXT NOT NULL REFERENCES review_answers(id) ON DELETE CASCADE,
  merge_answer TEXT NOT NULL REFERENCES review_answers(id),
  batch TEXT NOT NULL,
  ordinal INTEGER NOT NULL,
  PRIMARY KEY(batch,ordinal,answer),
  FOREIGN KEY(batch,ordinal) REFERENCES records(batch,ordinal) ON DELETE CASCADE
) STRICT, WITHOUT ROWID;
CREATE TRIGGER restored_import_rows_immutable BEFORE UPDATE ON restored_import_rows
BEGIN SELECT RAISE(ABORT,'Unmerge history is immutable'); END;
CREATE TRIGGER restored_import_rows_keep BEFORE DELETE ON restored_import_rows WHEN spentit_cleanup()=0
BEGIN SELECT RAISE(ABORT,'Unmerge history is retained'); END;
CREATE VIEW restored_import_drafts AS
 SELECT DISTINCT r.batch,r.ordinal FROM restored_import_rows r
 WHERE NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=r.answer)
 AND NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE (a.batch,a.ordinal)=(r.batch,r.ordinal));

DROP TRIGGER source_immutable;
CREATE TRIGGER source_immutable BEFORE UPDATE ON batches WHEN
 NEW.id!=OLD.id OR NEW.profile!=OLD.profile OR NEW.parser!=OLD.parser OR
 NEW.source_id!=OLD.source_id OR NEW.source_account IS NOT OLD.source_account OR NEW.count!=OLD.count OR
 (OLD.state NOT IN(0,3,4) AND NOT (
    OLD.state=1 AND (
     (NEW.state IN(0,4) AND spentit_cleanup()!=0 AND EXISTS(SELECT 1 FROM ledger_operations l WHERE l.kind=6 AND l.batch=OLD.id))
     OR (NEW.state=1 AND NEW.blocked=OLD.blocked AND EXISTS(SELECT 1 FROM restored_import_rows r WHERE r.batch=OLD.id))
    ))) OR NEW.revision!=OLD.revision+1 OR
 (NEW.materialized_revision IS NOT OLD.materialized_revision AND NOT
 (OLD.state=3 AND NEW.state=0 AND OLD.materialized_revision IS NULL AND NEW.materialized_revision=NEW.revision))
 BEGIN SELECT RAISE(ABORT,'invalid batch transition'); END;
DROP TRIGGER version_pending;
CREATE TRIGGER version_pending BEFORE INSERT ON versions WHEN
 EXISTS(SELECT 1 FROM accepted_transactions a WHERE (a.batch,a.ordinal)=(NEW.batch,NEW.ordinal)) OR
 EXISTS(SELECT 1 FROM generated_source_matches g WHERE (g.batch,g.ordinal)=(NEW.batch,NEW.ordinal)) OR
 NOT EXISTS(SELECT 1 FROM batches b WHERE b.id=NEW.batch AND (
 b.state=0 OR (b.state=4 AND spentit_account_command()!=0) OR (b.state=1 AND
 (EXISTS(SELECT 1 FROM restored_import_drafts r WHERE (r.batch,r.ordinal)=(NEW.batch,NEW.ordinal)) OR
 (spentit_cleanup()!=0 AND EXISTS(SELECT 1 FROM restored_import_rows r WHERE (r.batch,r.ordinal)=(NEW.batch,NEW.ordinal)))))))
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
DROP TRIGGER allocation_pending;
CREATE TRIGGER allocation_pending BEFORE INSERT ON allocations WHEN NOT EXISTS(
 SELECT 1 FROM batches b WHERE b.id=NEW.batch AND (
 b.state=0 OR (b.state=4 AND spentit_account_command()!=0) OR (b.state=1 AND
 (EXISTS(SELECT 1 FROM restored_import_drafts r WHERE (r.batch,r.ordinal)=(NEW.batch,NEW.ordinal)) OR
 (spentit_cleanup()!=0 AND EXISTS(SELECT 1 FROM restored_import_rows r WHERE (r.batch,r.ordinal)=(NEW.batch,NEW.ordinal)))))))
 BEGIN SELECT RAISE(ABORT,'committed history'); END;
DROP TRIGGER source_associations_keep;
CREATE TRIGGER source_associations_keep BEFORE DELETE ON source_associations WHEN spentit_cleanup()=0 AND
 (SELECT state FROM batches WHERE id=OLD.source_batch)=1 AND
 NOT EXISTS(SELECT 1 FROM restored_import_drafts r WHERE (r.batch,r.ordinal)=(OLD.source_batch,OLD.source_ordinal))
 BEGIN SELECT RAISE(ABORT,'immutable'); END;
PRAGMA user_version=2;
)SQL";
