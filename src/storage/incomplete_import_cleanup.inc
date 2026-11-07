// Capture is durable before preparation. Preparation (including population,
// matching, categories and generated transfers) is a single atomic transaction.
// Thus a captured batch has no published rows or relationships to reverse.
// Never infer incompleteness from pending transactions or an unclean shutdown.
bool ImportStore::startup_import_cleanup() const { return impl_->startup_cleanup; }
bool ImportStore::cleanup_incomplete_imports(std::string_view source) {
    {Statement pending(impl_->db,"SELECT 1 FROM batches WHERE state=3 AND (?1='' OR source_id=?1) LIMIT 1");pending.text(1,source);
    if(!pending.row())return false;}
    Atomic transaction(impl_->db);
    struct Cleanup {bool& flag;~Cleanup(){flag=false;}} cleanup{impl_->cleaning};impl_->cleaning=true;
    exec(impl_->db,"CREATE TEMP TABLE IF NOT EXISTS incomplete_sources(id TEXT PRIMARY KEY); DELETE FROM incomplete_sources");
    Statement roots(impl_->db,"INSERT INTO incomplete_sources SELECT DISTINCT source_id FROM batches WHERE state=3 AND (?1='' OR source_id=?1)");roots.text(1,source);roots.done();
    // Fail closed if the durable stage and its ownership roots disagree.
    Statement invalid(impl_->db,"SELECT 1 FROM batches b WHERE b.state=3 AND (b.materialized_revision IS NOT NULL OR EXISTS(SELECT 1 FROM records r WHERE r.batch=b.id) OR EXISTS(SELECT 1 FROM source_occurrences o WHERE o.batch=b.id) OR EXISTS(SELECT 1 FROM import_group_accounts m JOIN import_groups g ON g.id=m.group_id WHERE m.batch=b.id AND g.state!=0))");
    if(invalid.row())fail(SQLITE_CORRUPT,"Incomplete import has unexpected published material; cleanup refused");
    Statement mixed(impl_->db,"SELECT 1 FROM import_groups g JOIN import_group_accounts m ON m.group_id=g.id JOIN batches b ON b.id=m.batch WHERE g.state=0 AND b.state!=3");
    if(mixed.row())fail(SQLITE_CORRUPT,"Incomplete import has inconsistent preparation stages; cleanup refused");
    exec(impl_->db,"CREATE TEMP TABLE IF NOT EXISTS incomplete_accounts(id INTEGER PRIMARY KEY); DELETE FROM incomplete_accounts; INSERT INTO incomplete_accounts SELECT DISTINCT m.account_id FROM import_group_accounts m JOIN import_groups g ON g.id=m.group_id WHERE g.id IN(SELECT id FROM incomplete_sources) AND m.created=1");
    // Keep accounts referenced by other imports, manual history or transfers.
    // Foreign keys and the full audit below provide an additional fail-closed gate.
    exec(impl_->db,"DELETE FROM incomplete_accounts WHERE EXISTS(SELECT 1 FROM batches b WHERE b.account_id=incomplete_accounts.id AND b.state!=3) OR EXISTS(SELECT 1 FROM account_operations o WHERE o.account=incomplete_accounts.id)");
    exec(impl_->db,"DELETE FROM incomplete_accounts WHERE id IN(SELECT identity FROM account_status) OR id IN(SELECT account_id FROM account_retirements) OR id IN(SELECT account FROM retained_account_aliases) OR id IN(SELECT account FROM retained_opening_reviews) OR id IN(SELECT account_id FROM retired_import_accounts)");
    exec(impl_->db,"DELETE FROM incomplete_accounts WHERE EXISTS(SELECT 1 FROM account a JOIN account_creations c ON c.identity=a.singleton JOIN import_group_accounts m ON m.account_id=a.singleton AND m.created=1 WHERE a.singleton=incomplete_accounts.id AND (a.frozen!=0 OR a.revision!=CASE WHEN m.balance_choice>=5 THEN 1 ELSE 0 END OR a.opening!=coalesce(m.opening,0) OR a.opening_date!=c.opening_date))");
    for(const auto& transfer:all_transfers())for(const auto* side:{&transfer.link.origin,&transfer.link.destination}) {
        Statement keep(impl_->db,"DELETE FROM incomplete_accounts WHERE id=?");keep.integer(1,side->endpoint.account);keep.done();
    }
    exec(impl_->db,"DELETE FROM import_balance_events WHERE group_id IN(SELECT id FROM import_groups WHERE id IN(SELECT id FROM incomplete_sources)); DELETE FROM import_group_accounts WHERE group_id IN(SELECT id FROM import_groups WHERE id IN(SELECT id FROM incomplete_sources)); DELETE FROM import_group_events WHERE group_id IN(SELECT id FROM import_groups WHERE id IN(SELECT id FROM incomplete_sources)); DELETE FROM retained_group_counts WHERE group_id IN(SELECT id FROM import_groups WHERE id IN(SELECT id FROM incomplete_sources)); DELETE FROM import_groups WHERE id IN(SELECT id FROM incomplete_sources)");
    exec(impl_->db,"DELETE FROM history_order WHERE batch IN(SELECT id FROM batches WHERE source_id IN(SELECT id FROM incomplete_sources)); DELETE FROM events WHERE batch IN(SELECT id FROM batches WHERE source_id IN(SELECT id FROM incomplete_sources)); DELETE FROM batch_source_currency WHERE batch IN(SELECT id FROM batches WHERE source_id IN(SELECT id FROM incomplete_sources)); DELETE FROM batches WHERE source_id IN(SELECT id FROM incomplete_sources); DELETE FROM source_headers WHERE source_id IN(SELECT id FROM incomplete_sources) AND source_id NOT IN(SELECT source_id FROM batches); DELETE FROM sources WHERE id IN(SELECT id FROM incomplete_sources) AND id NOT IN(SELECT source_id FROM batches)");
    exec(impl_->db,"DELETE FROM incomplete_accounts WHERE EXISTS(SELECT 1 FROM batches b WHERE b.account_id=incomplete_accounts.id); DELETE FROM account_creations WHERE identity IN(SELECT id FROM incomplete_accounts); DELETE FROM account WHERE singleton IN(SELECT id FROM incomplete_accounts); DELETE FROM incomplete_accounts; DELETE FROM incomplete_sources");
    const auto previous=impl_->selected_account;
    {Statement retained(impl_->db,"SELECT 1 FROM account WHERE singleton=?");retained.integer(1,previous);
     if(!retained.row())impl_->selected_account=smallest_active_account(impl_->db);}
    // The checked roots above contain no published financial rows. Foreign-key
    // constraints and the ownership/stage checks protect this bounded deletion;
    // do not turn normal startup cleanup into another full-document audit.
    try { finish([&]{transaction.commit();}); }
    catch(...) {
        const auto failure=std::current_exception();
        if(!sqlite3_get_autocommit(impl_->db)){impl_->selected_account=previous;throw app::OperationOutcomeUnknown();}
        // A lost acknowledgement after COMMIT is success only when the captured
        // roots are durably absent. Otherwise the transaction rolled back intact.
        try {
            Statement remaining(impl_->db,"SELECT 1 FROM batches WHERE state=3 AND (?1='' OR source_id=?1)");remaining.text(1,source);
            if(!remaining.row())return true;
        } catch(...) {impl_->selected_account=previous;throw app::OperationOutcomeUnknown();}
        impl_->selected_account=previous;std::rethrow_exception(failure);
    }
    return true;
}
