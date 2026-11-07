// Called inside the close transaction, with cleanup permission. This is a
// reference-closure calculation, not a financial audit or a full-file rewrite.
void ImportStore::prune_session(const app::OperationContext* context) {
    if(context)context->checkpoint("Finishing change history");
    // Selection requests can contain superseded payee text. Exact request
    // identity survives as a fingerprint; expired Undo never needs this text.
    exec(impl_->db,"UPDATE review_answers SET request='' WHERE question='ledger-edit-batch' AND request!='' AND id IN(SELECT id FROM command_completions WHERE domain='selection' AND expired=0)");
    // No changed snapshot means no payload scan/rewrite for an unchanged close.
    Statement pending(impl_->db,"SELECT 1 FROM snapshot_facts WHERE baseline=0 LIMIT 1");
    if(!pending.row()){exec(impl_->db,"UPDATE command_completions SET expired=1 WHERE expired=0");return;}
    exec(impl_->db,R"SQL(
CREATE TEMP TABLE IF NOT EXISTS session_keep(kind INTEGER,batch TEXT,ordinal INTEGER,revision INTEGER,PRIMARY KEY(kind,batch,ordinal,revision)) WITHOUT ROWID;
DELETE FROM session_keep;
-- Current state, immutable acceptance references, and original/manual origins.
INSERT OR IGNORE INTO session_keep SELECT 1,batch,ordinal,revision FROM entry_heads;
INSERT OR IGNORE INTO session_keep SELECT 1,batch,ordinal,0 FROM entry_revisions WHERE revision=0;
INSERT OR IGNORE INTO session_keep SELECT 0,batch,ordinal,min(revision) FROM versions GROUP BY batch,ordinal;
INSERT OR IGNORE INTO session_keep SELECT 0,batch,ordinal,max(revision) FROM versions GROUP BY batch,ordinal;
INSERT OR IGNORE INTO session_keep SELECT 0,batch,ordinal,draft_revision FROM entry_revisions WHERE draft_revision IS NOT NULL;
INSERT OR IGNORE INTO session_keep SELECT 0,o.batch,m.ordinal,m.draft_revision FROM acceptance_members m JOIN operations o ON o.id=m.operation;
-- Historical duplicate/conflict stamps remain recognition data, not Undo.
INSERT OR IGNORE INTO session_keep SELECT 0,batch,ordinal,revision FROM versions WHERE decision=2;
-- Pending duplicate targets retain their reviewed original state even if a
-- later edit has advanced the target before it was accepted.
INSERT OR IGNORE INTO session_keep SELECT 0,v.duplicate_batch,v.duplicate_ordinal,-1-s.target_revision FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) WHERE s.target_revision<0;
-- Transfer replay and import reversal have explicit snapshot dependencies.
INSERT OR IGNORE INTO session_keep SELECT 1,l.batch,l.ordinal,l.prior_revision FROM ledger_operations l WHERE l.kind IN(1,2,3) AND EXISTS(SELECT 1 FROM transfer_versions v WHERE v.sequence=l.sequence);
INSERT OR IGNORE INTO session_keep SELECT 1,l.batch,l.ordinal,l.result_revision FROM ledger_operations l WHERE l.kind IN(1,2,3) AND EXISTS(SELECT 1 FROM transfer_versions v WHERE v.sequence=l.sequence);
INSERT OR IGNORE INTO session_keep SELECT m.committed,m.batch,m.ordinal,m.prior FROM review_members m WHERE EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=m.answer);
INSERT OR IGNORE INTO session_keep SELECT m.committed,m.batch,m.ordinal,m.result FROM review_members m WHERE EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=m.answer);
)SQL");
    // Follow Undo links used by retained transfer/review dependencies. A later
    // import can require the result of an earlier restore across many sessions.
    for(;;){
        const auto before=sqlite3_total_changes64(impl_->db);
        exec(impl_->db,R"SQL(
INSERT OR IGNORE INTO session_keep
SELECT 1,u.batch,u.ordinal,u.prior_revision FROM ledger_operations l JOIN ledger_operations u ON u.id=l.undo_of
WHERE EXISTS(SELECT 1 FROM session_keep k WHERE k.kind=1 AND k.batch=l.batch AND k.ordinal=l.ordinal AND k.revision=l.result_revision) AND l.kind=3;
INSERT OR IGNORE INTO session_keep
SELECT 1,u.batch,u.ordinal,u.result_revision FROM ledger_operations l JOIN ledger_operations u ON u.id=l.undo_of
WHERE EXISTS(SELECT 1 FROM session_keep k WHERE k.kind=1 AND k.batch=l.batch AND k.ordinal=l.ordinal AND k.revision=l.result_revision) AND l.kind=3;
)SQL");
        if(before==sqlite3_total_changes64(impl_->db))break;
        if(context)context->checkpoint();
    }
    for(const auto* table:{"entry_rule_allocations","entry_rule_fields","entry_allocation_meta","entry_fields","entry_allocations","entry_revisions","draft_rule_allocations","draft_rule_fields","draft_allocation_meta","draft_fields","allocations","review_stamps","versions"}){
#ifndef SPENTIT_RULES_STORAGE
        if(std::string_view(table).find("rule_")!=std::string_view::npos)continue;
#endif
        if(context)context->checkpoint();
        const bool entry=std::string_view(table).starts_with("entry_");
        const auto sql="DELETE FROM "+std::string(table)+" WHERE NOT EXISTS(SELECT 1 FROM session_keep k WHERE k.kind="+(entry?"1":"0")+" AND k.batch="+table+".batch AND k.ordinal="+table+".ordinal AND k.revision="+table+".revision)";
        exec(impl_->db,sql.c_str());
    }
    exec(impl_->db,"UPDATE snapshot_facts SET baseline=1 WHERE baseline=0; UPDATE command_completions SET expired=1 WHERE expired=0; DELETE FROM session_keep");
}
