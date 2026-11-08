#ifdef SPENTIT_RULES_STORAGE
namespace {
void prune_rule_saves(sqlite3* db) {
    // A single baseline includes deleted identities to prevent resurrection.
    exec(db,R"SQL(
INSERT INTO rule_snapshots(ruleset_revision,position,rule,version,enabled,deleted)
 SELECT (SELECT revision FROM rule_state),-row_number() OVER(ORDER BY id),id,version,0,1 FROM rule_heads WHERE deleted=1
 AND NOT EXISTS(SELECT 1 FROM rule_snapshots s WHERE s.ruleset_revision=(SELECT revision FROM rule_state) AND s.rule=rule_heads.id);
DELETE FROM rule_operation_requests;
DELETE FROM rule_snapshots WHERE ruleset_revision!=(SELECT revision FROM rule_state);
DELETE FROM rule_definitions AS d
 WHERE NOT EXISTS(SELECT 1 FROM rule_heads h WHERE h.id=d.id AND h.version=d.version)
 AND NOT EXISTS(SELECT 1 FROM rule_snapshots s WHERE s.rule=d.id AND s.version=d.version)
 AND NOT EXISTS(SELECT 1 FROM draft_rule_fields f WHERE (f.payee_rule=d.id AND f.payee_version=d.version) OR (f.memo_rule=d.id AND f.memo_version=d.version))
 AND NOT EXISTS(SELECT 1 FROM entry_rule_fields f WHERE (f.payee_rule=d.id AND f.payee_version=d.version) OR (f.memo_rule=d.id AND f.memo_version=d.version))
 AND NOT EXISTS(SELECT 1 FROM draft_rule_allocations a WHERE (a.category_rule=d.id AND a.category_version=d.version) OR (a.memo_rule=d.id AND a.memo_version=d.version))
 AND NOT EXISTS(SELECT 1 FROM entry_rule_allocations a WHERE (a.category_rule=d.id AND a.category_version=d.version) OR (a.memo_rule=d.id AND a.memo_version=d.version));
)SQL");
}
}
#endif
// Called inside the close transaction, with cleanup permission. This is a
// reference-closure calculation, not a financial audit or a full-file rewrite.
void ImportStore::prune_session(const app::OperationContext* context) {
    if(context)context->checkpoint("Finishing change history");
    {Statement children(impl_->db,"SELECT id FROM review_answers WHERE question LIKE 'ledger-delete-%' AND id NOT IN(SELECT id FROM command_completions)");while(children.row())session_snapshot::completed(impl_->db,children.bytes(0),"member",session_snapshot::digest(children.bytes(0)));}
    // Selection requests can contain superseded payee text. Exact request
    // identity survives as a fingerprint; expired Undo never needs this text.
    exec(impl_->db,"UPDATE review_answers SET request='' WHERE (question IN('ledger-edit-batch','ledger-delete','transfer-assignment','rules-account') OR question LIKE 'ledger-delete-%') AND request!='' AND id IN(SELECT id FROM command_completions WHERE expired=0)");
    // No changed snapshot means no payload scan/rewrite for an unchanged close.
    Statement pending(impl_->db,"SELECT 1 FROM draft_fields WHERE baseline=0 UNION ALL SELECT 1 FROM entry_fields WHERE baseline=0 UNION ALL SELECT 1 FROM ledger_operations WHERE sequence>(SELECT financial_cut FROM document_session) LIMIT 1");
    if(!pending.row()){
#ifdef SPENTIT_RULES_STORAGE
        Statement saves(impl_->db,"SELECT 1 FROM command_completions WHERE domain='rule-save' AND expired=0 LIMIT 1");if(saves.row())prune_rule_saves(impl_->db);
#endif
        exec(impl_->db,"UPDATE command_completions SET expired=1 WHERE expired=0");return;
    }
    // Ordinary editor restoration is not a permanent link-history archive.
    // Keep only the latest live restoration input for each current relationship;
    // Review matching decisions remain dependencies for source-owned rollback.
    exec(impl_->db,R"SQL(
DELETE FROM review_link_history AS h WHERE answer IN(SELECT id FROM review_answers WHERE question IN('ledger-edit','ledger-edit-batch','rules-account','ledger-delete','transfer-assignment') OR question LIKE 'ledger-delete-%')
AND (EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=h.answer)
 OR NOT EXISTS(SELECT 1 FROM transfer_heads t JOIN transfer_versions v USING(identity,sequence) WHERE t.identity=h.identity AND v.payload=h.result_payload)
 OR EXISTS(SELECT 1 FROM review_link_history n JOIN review_answers newer ON newer.id=n.answer JOIN review_answers older ON older.id=h.answer WHERE n.identity=h.identity AND newer.sequence>older.sequence AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=n.answer)));
)SQL");
    exec(impl_->db,"CREATE TEMP TABLE IF NOT EXISTS session_transfer_restore(identity TEXT,sequence INTEGER,PRIMARY KEY(identity,sequence)) WITHOUT ROWID; DELETE FROM session_transfer_restore");
    {Statement heads(impl_->db,"SELECT v.identity,v.sequence,v.payload FROM transfer_heads h JOIN transfer_versions v USING(identity,sequence)");
     while(heads.row()){const auto value=transfer_codec::view(heads.bytes(2));
        if(!value.link.origin.proposed_evidence && !value.link.destination.proposed_evidence)continue;
        Statement prior(impl_->db,"INSERT OR IGNORE INTO session_transfer_restore SELECT identity,max(sequence) FROM transfer_versions WHERE identity=? AND sequence<? HAVING max(sequence) IS NOT NULL");prior.text(1,heads.bytes(0));prior.integer(2,heads.integer(1));prior.done();}}
    exec(impl_->db,R"SQL(
DELETE FROM transfer_versions AS v WHERE NOT EXISTS(SELECT 1 FROM session_transfer_restore r WHERE r.identity=v.identity AND r.sequence=v.sequence)
AND NOT EXISTS(SELECT 1 FROM transfer_heads h WHERE h.identity=v.identity AND h.sequence=v.sequence)
AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.link=v.identity AND g.sequence=v.sequence)
AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.link=v.identity AND v.sequence=(SELECT max(p.sequence) FROM transfer_versions p WHERE p.identity=g.link AND p.sequence<g.sequence));
DELETE FROM transfer_events WHERE NOT EXISTS(SELECT 1 FROM ledger_operations l JOIN transfer_versions v ON v.sequence=l.sequence WHERE l.id=transfer_events.operation)
AND NOT EXISTS(SELECT 1 FROM acceptance_operations a WHERE a.operation=transfer_events.operation)
AND NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.operation=transfer_events.operation);
)SQL");
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
-- Duplicate recognition retains the recorded decision and target, not every
-- ordinary edit that happened to carry that decision.
-- Pending duplicate targets retain their reviewed original state even if a
-- later edit has advanced the target before it was accepted.
INSERT OR IGNORE INTO session_keep SELECT 0,v.duplicate_batch,v.duplicate_ordinal,-1-s.target_revision FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) JOIN session_keep k ON k.kind=0 AND k.batch=v.batch AND k.ordinal=v.ordinal AND k.revision=v.revision WHERE s.target_revision<0;
INSERT OR IGNORE INTO session_keep SELECT 1,v.duplicate_batch,v.duplicate_ordinal,s.target_revision FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) JOIN session_keep k ON k.kind=0 AND k.batch=v.batch AND k.ordinal=v.ordinal AND k.revision=v.revision WHERE s.target_revision>=0;
-- Transfer replay and import reversal have explicit snapshot dependencies.
INSERT OR IGNORE INTO session_keep SELECT m.committed,m.batch,m.ordinal,m.prior FROM review_members m WHERE EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=m.answer);
INSERT OR IGNORE INTO session_keep SELECT m.committed,m.batch,m.ordinal,m.result FROM review_members m WHERE EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=m.answer);
)SQL");
    for(;;){const auto before=sqlite3_total_changes64(impl_->db);
        exec(impl_->db,"INSERT OR IGNORE INTO session_keep SELECT 0,s.batch,s.ordinal,s.review_revision FROM review_stamps s JOIN session_keep k ON k.kind=0 AND k.batch=s.batch AND k.ordinal=s.ordinal AND k.revision=s.revision");
        exec(impl_->db,R"SQL(INSERT OR IGNORE INTO session_keep SELECT 0,v.duplicate_batch,v.duplicate_ordinal,-1-s.target_revision FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) JOIN session_keep k ON k.kind=0 AND k.batch=v.batch AND k.ordinal=v.ordinal AND k.revision=v.revision WHERE s.target_revision<0;
INSERT OR IGNORE INTO session_keep SELECT 1,v.duplicate_batch,v.duplicate_ordinal,s.target_revision FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) JOIN session_keep k ON k.kind=0 AND k.batch=v.batch AND k.ordinal=v.ordinal AND k.revision=v.revision WHERE s.target_revision>=0;
)SQL");
        if(before==sqlite3_total_changes64(impl_->db))break;
    }
    for(bool entry:{false,true}){
        Statement rows(impl_->db,entry?"SELECT batch,ordinal,revision FROM entry_fields WHERE baseline=0":"SELECT batch,ordinal,revision FROM draft_fields WHERE baseline=0");
        while(rows.row()){
            if(context)context->checkpoint();
            const imports::Occurrence id{rows.bytes(0),static_cast<std::size_t>(rows.integer(1))};const auto revision=rows.integer(2);
            std::int64_t order=0;
            if(entry){const auto history=revision_history(impl_->db,id,revision);if(history)order=history->order;
                else {Statement scale(impl_->db,"SELECT h.sequence FROM account_amount_revisions a JOIN ledger_operations l ON l.undo_of=a.operation AND l.kind=5 JOIN history_order h ON h.operation=l.id WHERE a.kind=2 AND a.batch=? AND a.ordinal=? AND a.revision=? LIMIT 1");scale.text(1,id.batch);scale.integer(2,integer(id.ordinal));scale.integer(3,revision);if(!scale.row())fail(SQLITE_CORRUPT,"Snapshot ordering is missing");order=scale.integer(0);}
            }else{Statement event(impl_->db,"SELECT sequence FROM history_order WHERE batch=? AND event_revision=?");event.text(1,id.batch);event.integer(2,revision);if(!event.row())fail(SQLITE_CORRUPT);order=event.integer(0);}
            Statement update(impl_->db,entry?"UPDATE entry_fields SET baseline=1,baseline_order=?4 WHERE batch=?1 AND ordinal=?2 AND revision=?3":"UPDATE draft_fields SET baseline=1,baseline_order=?4 WHERE batch=?1 AND ordinal=?2 AND revision=?3");
            update.text(1,id.batch);update.integer(2,integer(id.ordinal));update.integer(3,revision);update.integer(4,order);update.done();
        }
    }
    exec(impl_->db,"DELETE FROM account_amount_revisions WHERE kind IN(0,1,2,3) AND NOT EXISTS(SELECT 1 FROM session_keep k WHERE k.kind=CASE WHEN account_amount_revisions.kind<2 THEN 0 ELSE 1 END AND k.batch=account_amount_revisions.batch AND k.ordinal=account_amount_revisions.ordinal AND k.revision=account_amount_revisions.revision)");
    {Statement undos(impl_->db,"SELECT u.id FROM review_undos u JOIN review_answers a ON a.id=u.answer WHERE (a.question IN('ledger-edit','ledger-edit-batch','rules-account','ledger-delete','transfer-assignment') OR a.question LIKE 'ledger-delete-%') AND NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=a.id) AND NOT EXISTS(SELECT 1 FROM command_completions c WHERE c.id=u.id)");
     while(undos.row())session_snapshot::completed(impl_->db,undos.bytes(0),"review-undo",session_snapshot::digest(undos.bytes(0)));}
    // Operations referenced by retained relationship restoration remain inputs.
    // Their expired ordinary predecessors do not form a replay chain.
    exec(impl_->db,R"SQL(
CREATE TEMP TABLE IF NOT EXISTS session_operations(id TEXT PRIMARY KEY) WITHOUT ROWID;
DELETE FROM session_operations;
INSERT OR IGNORE INTO session_operations SELECT l.id FROM ledger_operations l WHERE l.kind IN(0,5) OR EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=l.id) OR EXISTS(SELECT 1 FROM transfer_versions v WHERE v.sequence=l.sequence);
INSERT OR IGNORE INTO session_operations SELECT m.child FROM review_members m JOIN review_link_history h ON h.answer=m.answer WHERE m.committed=1;
)SQL");
#ifdef SPENTIT_RULES_STORAGE
    // Group commands retain only their request digest after close. Child IDs
    // remain reserved as compact identities so they cannot be reused directly.
    {Statement children(impl_->db,"SELECT entry_operation FROM bulk_members UNION SELECT entry_operation FROM rule_reapplication_members");
     while(children.row()){Statement existing(impl_->db,"SELECT 1 FROM command_completions WHERE id=?");existing.text(1,children.bytes(0));if(!existing.row())session_snapshot::completed(impl_->db,children.bytes(0),"group-child",session_snapshot::digest(children.bytes(0)));}}
    exec(impl_->db,"DELETE FROM bulk_members; DELETE FROM bulk_operations; DELETE FROM rule_reapplication_members; DELETE FROM rule_reapplications; DELETE FROM history_order WHERE rule_application IS NOT NULL; DELETE FROM operation_registry WHERE domain=3; DELETE FROM rule_applications");
#endif
    {Statement removed(impl_->db,"SELECT id FROM ledger_operations WHERE id NOT IN(SELECT id FROM session_operations) AND id NOT IN(SELECT id FROM command_completions)");
     while(removed.row())session_snapshot::completed(impl_->db,removed.bytes(0),"member",session_snapshot::digest(removed.bytes(0)));}
    for(const auto* table:{"entry_rule_allocations","entry_rule_fields","entry_allocation_meta","entry_fields","entry_allocations","entry_revisions","draft_rule_allocations","draft_rule_fields","draft_allocation_meta","draft_fields","allocations","review_stamps","versions"}){
#ifndef SPENTIT_RULES_STORAGE
        if(std::string_view(table).find("rule_")!=std::string_view::npos)continue;
#endif
        if(context)context->checkpoint();
        const bool entry=std::string_view(table).starts_with("entry_");
        const auto sql="DELETE FROM "+std::string(table)+" WHERE NOT EXISTS(SELECT 1 FROM session_keep k WHERE k.kind="+(entry?"1":"0")+" AND k.batch="+table+".batch AND k.ordinal="+table+".ordinal AND k.revision="+table+".revision)";
        exec(impl_->db,sql.c_str());
    }
#ifdef SPENTIT_RULES_STORAGE
    prune_rule_saves(impl_->db);
#endif
    exec(impl_->db,R"SQL(
DELETE FROM review_stack;
DELETE FROM review_undo_members WHERE child NOT IN(SELECT id FROM session_operations) OR original_child NOT IN(SELECT id FROM session_operations);
DELETE FROM review_members WHERE (committed=1 OR answer NOT IN(SELECT id FROM review_answers WHERE question='ledger-delete') OR EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=review_members.answer)) AND answer IN(SELECT id FROM review_answers WHERE question IN('ledger-edit','ledger-edit-batch','rules-account','ledger-delete','transfer-assignment') OR question LIKE 'ledger-delete-%') AND NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=review_members.answer);
DELETE FROM review_undos WHERE answer IN(SELECT a.id FROM review_answers a WHERE (a.question IN('ledger-edit','ledger-edit-batch','rules-account','ledger-delete','transfer-assignment') OR a.question LIKE 'ledger-delete-%') AND NOT EXISTS(SELECT 1 FROM review_members m WHERE m.answer=a.id) AND NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=a.id));
DELETE FROM review_answers WHERE (question IN('ledger-edit','ledger-edit-batch','rules-account','ledger-delete','transfer-assignment') OR question LIKE 'ledger-delete-%') AND NOT EXISTS(SELECT 1 FROM review_members m WHERE m.answer=review_answers.id) AND NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=review_answers.id) AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=review_answers.id);
DELETE FROM change_dependencies WHERE operation NOT IN(SELECT id FROM session_operations);
DELETE FROM history_order WHERE operation IS NOT NULL AND operation NOT IN(SELECT id FROM session_operations);
DELETE FROM operation_registry WHERE domain=0 AND id NOT IN(SELECT id FROM session_operations);
DELETE FROM ledger_operations WHERE id NOT IN(SELECT id FROM session_operations);
CREATE TEMP TABLE IF NOT EXISTS session_events(batch TEXT,revision INTEGER,PRIMARY KEY(batch,revision)) WITHOUT ROWID;
DELETE FROM session_events;
INSERT INTO session_events SELECT e.batch,e.revision FROM events e JOIN batches b ON b.id=e.batch WHERE e.kind IN('save','decide','apply_rules') AND e.revision!=b.revision AND NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=e.batch AND v.revision=e.revision) AND NOT EXISTS(SELECT 1 FROM review_stamps r WHERE r.batch=e.batch AND r.review_revision=e.revision);
DELETE FROM history_order WHERE (batch,event_revision) IN(SELECT batch,revision FROM session_events);
DELETE FROM events WHERE (batch,revision) IN(SELECT batch,revision FROM session_events);
DELETE FROM session_events;
UPDATE command_completions SET expired=1 WHERE expired=0;
DELETE FROM session_keep;
DELETE FROM session_operations;
)SQL");
}
