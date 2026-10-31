// Rollback deletes transient ownership roots, then collects exclusively owned
// rows. The one-slot receipt resolves retries without archiving forgotten imports.
app::RollbackReview ImportStore::rollback_review() const {
    app::RollbackReview result;Statement generation(impl_->db,"SELECT revision FROM mutation_generation WHERE singleton=1");if(!generation.row())fail(SQLITE_CORRUPT,"Missing mutation generation");result.token=generation.integer(0);
    std::set<std::int64_t> affected;
    Statement rows(impl_->db,"SELECT count(*) FROM records r JOIN batches b ON b.id=r.batch JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE b.account_id=spentit_account() AND b.state IN(0,3,4) AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision!=2 AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=r.batch AND g.ordinal=r.ordinal) AND NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=r.batch AND a.ordinal=r.ordinal)");
    if(rows.row())result.transactions=static_cast<std::size_t>(rows.integer(0));
    Statement errors(impl_->db,"SELECT (SELECT count(*) FROM source_occurrences o JOIN batches b ON b.id=o.batch WHERE b.account_id=spentit_account() AND b.state IN(0,3,4) AND NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=o.batch AND r.ordinal=o.ordinal))+(SELECT count(*) FROM batches b WHERE account_id=spentit_account() AND (state=3 OR EXISTS(SELECT 1 FROM import_file_errors e WHERE e.batch=b.id)))");
    if(errors.row())result.failures=static_cast<std::size_t>(errors.integer(0));
    if(result.transactions || result.failures)affected.insert(impl_->selected_account);
    for(const auto& value:all_transfers()) {
        const auto& l=value.link;
        const bool own=l.origin.endpoint.account==impl_->selected_account && l.origin.state==transfers::State::draft;
        if(own)for(const auto* side:{&l.origin,&l.destination})if(local_side(*side) && side->state==transfers::State::draft && !side->proposed_evidence){++result.transactions;affected.insert(side->endpoint.account);}
        if(own || (l.destination.endpoint.account==impl_->selected_account && l.destination.state==transfers::State::draft)) {affected.insert(l.origin.endpoint.account);affected.insert(l.destination.endpoint.account);}
    }
    result.affected_accounts.assign(affected.begin(),affected.end());return result;
}
std::optional<app::RollbackReceipt> ImportStore::rollback_outcome(std::string_view operation) const {
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement receipt(impl_->db,"SELECT account,transactions,failures FROM rollback_receipt WHERE operation=?");receipt.text(1,operation);if(!receipt.row())return {};
    return app::RollbackReceipt{std::string(operation),receipt.integer(0),static_cast<std::size_t>(receipt.integer(1)),static_cast<std::size_t>(receipt.integer(2))};
}
app::RollbackReceipt ImportStore::rollback_account(std::string_view operation,std::int64_t expected) {
    identifier(operation);if(auto previous=rollback_outcome(operation)){if(previous->account!=impl_->selected_account)fail(SQLITE_CONSTRAINT,"Rollback retry changed account");return *previous;}
    Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");collision.text(1,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Operation id is already used");
    require_writable_account(impl_->db,impl_->selected_account);const auto review=rollback_review();
    if(review.token!=expected)fail(SQLITE_CONSTRAINT,"Work changed; review rollback again");
    const auto owner=impl_->selected_account;const auto aliases=account_aliases();
    Atomic transaction(impl_->db);
    struct Cleanup {bool& flag;~Cleanup(){flag=false;}} cleanup{impl_->cleaning};impl_->cleaning=true;
    exec(impl_->db,"CREATE TEMP TABLE rollback_batches(id TEXT PRIMARY KEY); CREATE TEMP TABLE rollback_rows(batch TEXT,ordinal INTEGER,PRIMARY KEY(batch,ordinal)); CREATE TEMP TABLE rollback_operations(id TEXT PRIMARY KEY); CREATE TEMP TABLE rollback_links(id TEXT PRIMARY KEY)");
    exec(impl_->db,"INSERT INTO rollback_batches SELECT id FROM batches WHERE account_id=spentit_account() AND state IN(0,3,4); INSERT INTO rollback_rows SELECT o.batch,o.ordinal FROM source_occurrences o JOIN rollback_batches b ON b.id=o.batch WHERE NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=o.batch AND a.ordinal=o.ordinal)");
    // Remove this account's uncommitted source ownership first. A source-only
    // transfer match may still support a retained import; keep its facts and
    // relationship history until its final owner disappears.
    exec(impl_->db,"DELETE FROM source_associations WHERE (source_batch,source_ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");
    exec(impl_->db,"DELETE FROM rollback_rows WHERE EXISTS(SELECT 1 FROM source_associations a WHERE a.ledger_batch=rollback_rows.batch AND a.ledger_ordinal=rollback_rows.ordinal)");
    // A bank proposal is an independent ownership root for its imported row.
    // Removing the originating link leaves that other account's row intact.
    auto values=all_transfers();std::set<std::string> doomed;
    for(const auto& value:values) {
        const auto& l=value.link;
        if((l.origin.endpoint.account==owner && l.origin.state==transfers::State::draft) ||
           (l.destination.endpoint.account==owner && l.destination.state==transfers::State::draft && l.destination.independent_evidence))doomed.insert(l.identity);
    }
    // Collect the transient relationship history as a connected component. A
    // multi-link edit cannot leave a dangling fragment of its replay inputs.
    bool changed=true;
    while(changed) {
        changed=false;
        Statement versions(impl_->db,"SELECT identity,sequence,payload FROM transfer_versions ORDER BY sequence");
        std::map<std::int64_t,std::set<std::string>> by_operation;
        while(versions.row())by_operation[versions.integer(1)].insert(versions.bytes(0));
        for(const auto& [sequence,links]:by_operation)if(std::any_of(links.begin(),links.end(),[&](const auto& id){return doomed.contains(id);})) {
            for(const auto& id:links)changed|=doomed.insert(id).second;
            Statement op(impl_->db,"INSERT OR IGNORE INTO rollback_operations SELECT id FROM ledger_operations WHERE sequence=?");op.integer(1,sequence);op.done();
        }
    }
    for(const auto& id:doomed) {
        const auto found=std::find_if(values.begin(),values.end(),[&](const auto& value){return value.link.identity==id;});
        if(found!=values.end() && (found->link.origin.state==transfers::State::committed || found->link.destination.state==transfers::State::committed))fail(SQLITE_CONSTRAINT,"Rollback cannot remove an accepted transfer relationship");
        Statement link(impl_->db,"INSERT INTO rollback_links VALUES(?)");link.text(1,id);link.done();
    }
    // Proposals on previously accepted generated transactions have no financial
    // effect; retain the accepted relationship and remove only that proposal.
    Statement proposals(impl_->db,"SELECT e.operation,e.request FROM transfer_events e JOIN ledger_operations l ON l.id=e.operation WHERE l.kind=4 AND e.did_commit=0 AND e.local_change=0");
    while(proposals.row()) {
        const auto request=transfer_codec::request(proposals.bytes(1));
        if((request.action!=app::TransferAction::propose_match && request.action!=app::TransferAction::discard_match) || !request.evidence || request.evidence->endpoint.account!=owner)continue;
        Statement source(impl_->db,"SELECT 1 FROM rollback_rows WHERE batch=? AND ordinal=?");source.text(1,request.evidence->endpoint.entry.batch);source.integer(2,integer(request.evidence->endpoint.entry.ordinal));if(!source.row())continue;
        Statement op(impl_->db,"INSERT OR IGNORE INTO rollback_operations VALUES(?)");op.text(1,proposals.bytes(0));op.done();
    }
    Statement unsafe(impl_->db,"SELECT 1 FROM rollback_operations r JOIN ledger_operations l ON l.id=r.id WHERE l.kind!=4 OR l.change!=0 OR EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=l.id AND e.did_commit=1)");if(unsafe.row())fail(SQLITE_CONSTRAINT,"Rollback would affect accepted history");
    exec(impl_->db,"DELETE FROM generated_source_matches WHERE (batch,ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");
    exec(impl_->db,"DELETE FROM transfer_heads WHERE identity IN(SELECT id FROM rollback_links)");
    // A proposal head reverts to its latest retained revision. The head-step
    // trigger permits this only within the repository's cleanup boundary.
    exec(impl_->db,"UPDATE transfer_heads SET sequence=(SELECT max(v.sequence) FROM transfer_versions v WHERE v.identity=transfer_heads.identity AND v.sequence NOT IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id)) WHERE sequence IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id)");
    exec(impl_->db,"DELETE FROM transfer_versions WHERE sequence IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id); DELETE FROM transfer_events WHERE operation IN(SELECT id FROM rollback_operations); DELETE FROM history_order WHERE operation IN(SELECT id FROM rollback_operations); DELETE FROM operation_registry WHERE id IN(SELECT id FROM rollback_operations); DELETE FROM ledger_operations WHERE id IN(SELECT id FROM rollback_operations)");
    // Account creation, confirmed names, staged category creation and reviewed
    // opening amounts are independent of the transaction ownership being removed.
    Statement staged(impl_->db,"INSERT OR REPLACE INTO retained_opening_reviews SELECT m.account_id,m.opening,m.opening_date FROM import_group_accounts m JOIN rollback_batches r ON r.id=m.batch JOIN account a ON a.singleton=m.account_id WHERE m.created=1 AND m.opening IS NOT NULL AND a.confirmed=0");staged.done();
    Statement catalogs(impl_->db,"SELECT id FROM rollback_batches");while(catalogs.row())for(const auto& category:source_categories(catalogs.bytes(0))){Statement keep(impl_->db,"INSERT OR IGNORE INTO retained_category_names VALUES(?)");keep.text(1,category.name);keep.done();}
    for(const auto& alias:aliases){Statement keep(impl_->db,"INSERT OR IGNORE INTO retained_account_aliases VALUES(?,?)");keep.integer(1,alias.account);keep.text(2,alias.name);keep.done();}
    exec(impl_->db,"DELETE FROM source_associations WHERE (source_batch,source_ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");
    Statement shared(impl_->db,"SELECT 1 FROM source_associations WHERE (ledger_batch,ledger_ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");if(shared.row())fail(SQLITE_CONSTRAINT,"A retained source still owns this transaction");
    const auto erase_rows=[&](const char* table){const auto sql=std::string("DELETE FROM ")+table+" WHERE (batch,ordinal) IN(SELECT batch,ordinal FROM rollback_rows)";exec(impl_->db,sql.c_str());};
    if(rules_format(impl_->db)){erase_rows("draft_rule_allocations");erase_rows("draft_rule_fields");}
    for(const auto* table:{"draft_allocation_meta","draft_fields","review_stamps","allocations","versions","retained_transfer_rows","records","source_occurrences"})erase_rows(table);
    if(rules_format(impl_->db)) {
        exec(impl_->db,"DELETE FROM history_order WHERE rule_application IN(SELECT id FROM rule_applications a WHERE a.batch_after>a.batch_before AND NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=a.batch AND v.revision=a.batch_after)); DELETE FROM operation_registry WHERE id IN(SELECT id FROM rule_applications a WHERE a.batch_after>a.batch_before AND NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=a.batch AND v.revision=a.batch_after)); DELETE FROM rule_applications WHERE batch_after>batch_before AND NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=rule_applications.batch AND v.revision=rule_applications.batch_after)");
        exec(impl_->db,"UPDATE rule_applications SET changed_count=(SELECT count(*) FROM versions v WHERE v.batch=rule_applications.batch AND v.revision=rule_applications.batch_after),evaluated=(SELECT count(*) FROM records r WHERE r.batch=rule_applications.batch AND EXISTS(SELECT 1 FROM versions v WHERE v.batch=r.batch AND v.ordinal=r.ordinal AND v.revision<=rule_applications.batch_before) AND NOT EXISTS(SELECT 1 FROM accepted_transactions a JOIN operations o ON o.id=a.operation WHERE a.batch=r.batch AND a.ordinal=r.ordinal AND o.revision<rule_applications.batch_before)) WHERE batch_after>batch_before AND batch IN(SELECT id FROM rollback_batches)");
    }
    exec(impl_->db,"DELETE FROM account_amount_revisions WHERE (batch,ordinal) IN(SELECT batch,ordinal FROM rollback_rows) AND kind IN(0,1)");
    exec(impl_->db,"DELETE FROM import_file_errors WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM import_completions WHERE batch IN(SELECT id FROM rollback_batches)");
    // Entirely unowned sources are physically removed. Shared source files stay
    // only while another account or an accepted transaction needs their facts.
    exec(impl_->db,"DELETE FROM rollback_batches WHERE EXISTS(SELECT 1 FROM records r WHERE r.batch=rollback_batches.id)");
    if(rules_format(impl_->db))exec(impl_->db,"DELETE FROM history_order WHERE rule_application IN(SELECT id FROM rule_applications WHERE batch IN(SELECT id FROM rollback_batches)); DELETE FROM operation_registry WHERE id IN(SELECT id FROM rule_applications WHERE batch IN(SELECT id FROM rollback_batches)); DELETE FROM rule_applications WHERE batch IN(SELECT id FROM rollback_batches)");
    exec(impl_->db,"DELETE FROM change_dependencies WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM history_order WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM events WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM import_balance_events WHERE (group_id,source_account) IN(SELECT group_id,source_account FROM import_group_accounts WHERE batch IN(SELECT id FROM rollback_batches)); DELETE FROM import_group_accounts WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM batch_source_currency WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM account_amount_revisions WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM retained_source_counts WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM batches WHERE id IN(SELECT id FROM rollback_batches)");
    exec(impl_->db,"DELETE FROM import_group_events WHERE group_id IN(SELECT id FROM import_groups g WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=g.id)); DELETE FROM retained_group_counts WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=retained_group_counts.group_id); UPDATE retained_group_counts SET count=(SELECT count(*) FROM import_group_accounts m WHERE m.group_id=retained_group_counts.group_id); DELETE FROM import_groups WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=import_groups.id); DELETE FROM source_headers WHERE source_id NOT IN(SELECT source_id FROM batches); DELETE FROM sources WHERE id NOT IN(SELECT source_id FROM batches)");
    std::vector<std::pair<std::string,std::int64_t>> partial;
    Statement remaining(impl_->db,"SELECT id,revision FROM batches WHERE account_id=spentit_account() AND state IN(0,4)");while(remaining.row())partial.emplace_back(remaining.bytes(0),remaining.integer(1));
    for(const auto& [id,revision]:partial){Statement count(impl_->db,"UPDATE retained_source_counts SET count=(SELECT count(*) FROM source_occurrences WHERE batch=?1) WHERE batch=?1");count.text(1,id);count.done();advance(impl_->db,id,revision,"rollback",0,0);}
    exec(impl_->db,"DELETE FROM rollback_receipt");Statement receipt(impl_->db,"INSERT INTO rollback_receipt VALUES(1,?,?,?,?)");receipt.text(1,operation);receipt.integer(2,owner);receipt.integer(3,integer(review.transactions));receipt.integer(4,integer(review.failures));receipt.done();
    exec(impl_->db,"DROP TABLE rollback_links; DROP TABLE rollback_operations; DROP TABLE rollback_rows; DROP TABLE rollback_batches");
    impl_->reviews.clear();impl_->reviewed_groups.clear();impl_->reviewed_sources.clear();impl_->read_rows=VerificationRows{};impl_->read_stamps.clear();impl_->reviewed_transfers.reset();impl_->reviewed_transfer_stamp.clear();impl_->review_changes=-1;
    // Full verification is appropriate for destructive collection, and runs
    // before durability so any broken retained relationship rolls back intact.
    exec(impl_->db,"DELETE FROM review_merges WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_merges.batch AND r.ordinal=review_merges.ordinal) OR NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_merges.target_batch AND r.ordinal=review_merges.target_ordinal); DELETE FROM review_choices WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_choices.batch AND r.ordinal=review_choices.ordinal); DELETE FROM review_sources WHERE NOT EXISTS(SELECT 1 FROM source_occurrences o WHERE o.batch=source_batch AND o.ordinal=source_ordinal); DELETE FROM review_members WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_members.batch AND r.ordinal=review_members.ordinal); DELETE FROM review_link_history WHERE NOT EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=review_link_history.operation); DELETE FROM review_stack");
    verify();finish([&]{transaction.commit();});return {std::string(operation),owner,review.transactions,review.failures};
}
