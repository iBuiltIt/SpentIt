// Sources already provide durable import identity. No new format or per-row tag
// is needed; rowid supplies retained-source order, never transaction ownership.
// Import removal collects transfer versions. A link Undo that already restored
// one of those versions would otherwise fail the next check. Drop only the
// restoration rows that check rejects, and leave a row whose version remains.
namespace {
void release_collected_link_restorations(sqlite3* db) {
    exec(db,R"SQL(
DELETE FROM review_link_restorations WHERE (identity,restored) IN(
 SELECT r.identity,r.restored FROM review_link_restorations r JOIN review_undos u ON u.id=r.undo_id
 WHERE NOT EXISTS(SELECT 1 FROM transfer_versions v WHERE v.identity=r.identity AND v.sequence=r.restored)
 OR (NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=u.answer AND h.identity=r.identity)
     AND NOT EXISTS(SELECT 1 FROM action_headers a JOIN transfer_versions p ON p.identity=r.identity AND p.sequence=r.prior JOIN transfer_versions v ON v.identity=r.identity AND v.sequence=r.restored WHERE a.id=u.answer AND a.state=2 AND p.payload=v.payload AND r.prior<r.consumed AND r.consumed<r.restored)))
)SQL");
}
}
std::vector<std::string> ImportStore::import_undo_holders(std::string_view source) const {
    std::vector<std::string> holders;
    Statement retained(impl_->db,"SELECT 1 FROM entry_revisions e JOIN batches b ON b.id=e.batch WHERE b.source_id=? LIMIT 1");retained.text(1,source);
    if(!retained.row())return holders;
    Statement shared(impl_->db,R"SQL(
        SELECT DISTINCT owner.source_id FROM source_associations a
        JOIN batches b ON b.id=a.ledger_batch JOIN batches owner ON owner.id=a.source_batch
        WHERE b.source_id=?1 AND owner.source_id!=?1
    )SQL");shared.text(1,source);
    while(shared.row())holders.push_back(shared.bytes(0));
    return holders;
}
bool ImportStore::import_undo_held(std::string_view source) const { return !import_undo_holders(source).empty(); }
std::vector<app::UndoableImport> ImportStore::undoable_imports() const {
    std::vector<app::UndoableImport> result;
    Statement sources(impl_->db,R"SQL(
      SELECT s.id,h.format,s.rowid FROM sources s JOIN source_headers h ON h.source_id=s.id
      WHERE h.format!='manual' AND EXISTS(SELECT 1 FROM batches b WHERE b.source_id=s.id AND b.state IN(0,4))
      AND NOT EXISTS(SELECT 1 FROM batches b WHERE b.source_id=s.id AND (b.state NOT IN(0,4)
          OR EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=b.id)))
      AND EXISTS(SELECT 1 FROM batches b WHERE b.source_id=s.id AND (
          EXISTS(SELECT 1 FROM source_associations a WHERE a.source_batch=b.id)
          OR EXISTS(SELECT 1 FROM source_occurrences o WHERE o.batch=b.id AND NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=o.batch AND r.ordinal=o.ordinal))
          OR EXISTS(SELECT 1 FROM records r JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal WHERE r.batch=b.id AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal) AND v.decision=0)
          OR EXISTS(SELECT 1 FROM import_file_errors e WHERE e.batch=b.id)))
      ORDER BY s.rowid DESC
    )SQL");
    while(sources.row()) {
        app::UndoableImport item;item.source=sources.bytes(0);item.format=sources.bytes(1);item.order=sources.integer(2);
        if(import_undo_held(item.source))continue;
        Statement accounts(impl_->db,"SELECT DISTINCT account_id FROM batches WHERE source_id=? ORDER BY account_id");accounts.text(1,item.source);
        while(accounts.row())item.accounts.push_back(accounts.integer(0));
        Statement facts(impl_->db,"SELECT count(*),min(v.date),max(v.date) FROM source_occurrences o JOIN batches b ON b.id=o.batch LEFT JOIN versions v ON v.batch=o.batch AND v.ordinal=o.ordinal AND v.revision=(SELECT min(revision) FROM versions WHERE batch=o.batch AND ordinal=o.ordinal) WHERE b.source_id=?");facts.text(1,item.source);
        if(facts.row()){item.records=static_cast<std::size_t>(facts.integer(0));item.first_date=facts.bytes(1);item.last_date=facts.bytes(2);}
        result.push_back(std::move(item));
    }
    return result;
}
app::RollbackReview ImportStore::review_import_undo(std::string_view source) const {
    identifier(source);auto imports=undoable_imports();
    const auto head=action_head();
    if(head && head->kind==app::ActionKind::manual_creation && head->payload==source) {
        Statement manual(impl_->db,"SELECT account_id FROM batches WHERE id=? AND parser='manual-v1' AND state=0 AND NOT EXISTS(SELECT 1 FROM accepted_transactions WHERE batch=?1)");manual.text(1,source);
        if(manual.row()){app::UndoableImport item;item.source=source;item.format="manual";item.accounts.push_back(manual.integer(0));item.records=1;imports.push_back(std::move(item));}
    }
    const auto found=std::find_if(imports.begin(),imports.end(),[&](const auto& item){return item.source==source;});
    if(found==imports.end())fail(SQLITE_CONSTRAINT,"This import cannot be undone. Refresh the import list.");
    app::RollbackReview result;Statement generation(impl_->db,"SELECT revision FROM mutation_generation WHERE singleton=1");if(!generation.row())fail(SQLITE_CORRUPT);result.token=generation.integer(0);
    std::set<std::int64_t> affected(found->accounts.begin(),found->accounts.end());
    for(const auto account:affected)require_writable_account(impl_->db,account);
    std::set<std::pair<std::string,std::size_t>> removed;
    Statement rows(impl_->db,R"SQL(
        SELECT o.batch,o.ordinal,EXISTS(SELECT 1 FROM records r WHERE r.batch=o.batch AND r.ordinal=o.ordinal),
          (SELECT v.decision FROM versions v WHERE v.batch=o.batch AND v.ordinal=o.ordinal ORDER BY v.revision DESC LIMIT 1),
          EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=o.batch AND g.ordinal=o.ordinal)
        FROM source_occurrences o JOIN batches b ON b.id=o.batch WHERE b.source_id=?1
        AND NOT EXISTS(SELECT 1 FROM source_associations a JOIN batches owner ON owner.id=a.source_batch
          WHERE a.ledger_batch=o.batch AND a.ledger_ordinal=o.ordinal AND owner.source_id!=?1)
    )SQL");rows.text(1,source);
    while(rows.row()) {
        removed.emplace(rows.bytes(0),static_cast<std::size_t>(rows.integer(1)));
        if(!rows.integer(2))++result.failures;
        else if(rows.integer(3)!=2 && !rows.integer(4))++result.transactions;
    }
    Statement lastOwners(impl_->db,R"SQL(
        SELECT DISTINCT a.ledger_batch,a.ledger_ordinal,b.account_id FROM source_associations a
        JOIN batches origin ON origin.id=a.source_batch JOIN batches b ON b.id=a.ledger_batch
        WHERE origin.source_id=?1 AND b.source_id!=?1
        AND NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=a.ledger_batch AND t.ordinal=a.ledger_ordinal)
        AND NOT EXISTS(SELECT 1 FROM source_associations other JOIN batches ob ON ob.id=other.source_batch
            WHERE other.ledger_batch=a.ledger_batch AND other.ledger_ordinal=a.ledger_ordinal AND ob.source_id!=?1)
    )SQL");lastOwners.text(1,source);
    while(lastOwners.row())if(removed.emplace(lastOwners.bytes(0),static_cast<std::size_t>(lastOwners.integer(1))).second){++result.transactions;affected.insert(lastOwners.integer(2));}
    Statement errors(impl_->db,"SELECT count(*) FROM import_file_errors e JOIN batches b ON b.id=e.batch WHERE b.source_id=?");errors.text(1,source);if(errors.row())result.failures+=static_cast<std::size_t>(errors.integer(0));
    const auto restorable=import_link_undos(source);
    const auto removes=[&](const transfers::Endpoint& e){return removed.contains({e.entry.batch,e.entry.ordinal});};
    for(const auto& value:all_transfers()) {
        const auto& link=value.link;
        const bool ends=removes(link.origin.endpoint)||removes(link.destination.endpoint);
        bool proposal=false;
        for(const auto* side:{&link.origin,&link.destination})if(side->proposed_evidence && removes(side->proposed_evidence->endpoint))proposal=true;
        if(!ends && !proposal)continue;
        affected.insert(link.origin.endpoint.account);affected.insert(link.destination.endpoint.account);
        if(ends) {
            if((link.origin.state==transfers::State::committed || link.destination.state==transfers::State::committed) && restorable.empty())
                fail(SQLITE_CONSTRAINT,"Undo the later transfer link before undoing this import.");
            for(const auto* side:{&link.origin,&link.destination})if(local_side(*side) && side->state==transfers::State::draft && !side->proposed_evidence)++result.transactions;
        }
    }
    result.affected_accounts.assign(affected.begin(),affected.end());return result;
}
std::optional<app::RollbackReceipt> ImportStore::import_undo_outcome(std::string_view operation,std::string_view source) const {
    identifier(operation);identifier(source);
    // A separate, unambiguous operation namespace binds retries to the source,
    // including after that source was deleted. Existing account receipts remain unchanged.
    Statement prior(impl_->db,"SELECT operation FROM rollback_receipt WHERE substr(operation,1,length(?1)+1)=?1||'.'");prior.text(1,operation);
    if(prior.row() && prior.bytes(0)!=std::string(operation)+"."+std::string(source))fail(SQLITE_CONSTRAINT,"Import undo retry changed its source");
    auto result=rollback_outcome(std::string(operation)+"."+std::string(source));
    if(result)result->operation=operation;
    return result;
}
app::RollbackReceipt ImportStore::undo_import(std::string_view operation,std::string_view source,std::int64_t expected) {
    identifier(operation);identifier(source);
    if(auto prior=import_undo_outcome(operation,source)){
        Statement saved(impl_->db,"SELECT epoch FROM rollback_receipt WHERE operation=?");saved.text(1,std::string(operation)+"."+std::string(source));if(!saved.row() || saved.integer(0)!=expected)fail(SQLITE_CONSTRAINT,"Import Undo retry changed its request");return *prior;
    }
    Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?1 UNION ALL SELECT 1 FROM rollback_receipt WHERE operation=?1");collision.text(1,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Operation identifier is already used");
    const auto head=action_head();
    if(!impl_->undo_token) {
        if(head && head->kind==app::ActionKind::import && head->payload==source) {
            if(expected!=ledger_edit_generation())fail(SQLITE_CONSTRAINT,"Work changed; review this import again.");
            const auto review=review_import_undo(source);
            undo_action(operation,preview_undo(head->id,action_token()));
            return {std::string(operation),account().identity,review.transactions,review.failures};
        }
        ActionChain(impl_->db).require_empty();
    }
    const auto linked=import_link_undos(source);
    if(!linked.empty()) {
        const auto review=review_import_undo(source);
        if(review.token!=expected)fail(SQLITE_CONSTRAINT,"Work changed; review this import again.");
        return undo_linked_import(operation,source,review,linked);
    }
    auto result=rollback_scope(std::string(operation)+"."+std::string(source),expected,source);
    result.operation=operation;return result;
}

// Rollback deletes transient ownership roots, then collects exclusively owned
// rows. Compact per-operation receipts resolve retries without retaining row payloads.
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
    if(!impl_->verifying && !impl_->undo_joined && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement receipt(impl_->db,"SELECT account,transactions,failures FROM rollback_receipt WHERE operation=?");receipt.text(1,operation);if(!receipt.row())return {};
    return app::RollbackReceipt{std::string(operation),receipt.integer(0),static_cast<std::size_t>(receipt.integer(1)),static_cast<std::size_t>(receipt.integer(2))};
}
app::RollbackReceipt ImportStore::rollback_account(std::string_view operation,std::int64_t expected) {
    ActionChain(impl_->db).require_empty();
    identifier(operation);return rollback_scope(operation,expected,{});
}
app::RollbackReceipt ImportStore::rollback_scope(std::string_view operation,std::int64_t expected,std::string_view source,bool collecting) {
    if(!collecting)if(auto previous=rollback_outcome(operation)){Statement saved(impl_->db,"SELECT source,epoch FROM rollback_receipt WHERE operation=?");saved.text(1,operation);if(!saved.row() || saved.bytes(0)!=source || saved.integer(1)!=expected || (source.empty() && previous->account!=impl_->selected_account))fail(SQLITE_CONSTRAINT,"Rollback retry changed its request");return *previous;}
    Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");collision.text(1,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Operation id is already used");
    if(source.empty())require_writable_account(impl_->db,impl_->selected_account);
    const auto review=collecting?app::RollbackReview{}:source.empty()?rollback_review():review_import_undo(source);
    if(!collecting && review.token!=expected)fail(SQLITE_CONSTRAINT,"Work changed; review rollback again");
    const auto owner=impl_->selected_account;const auto aliases=account_aliases();
    if(collecting && (sqlite3_get_autocommit(impl_->db) || !impl_->cleaning || source.empty()))fail(SQLITE_MISUSE);
    std::unique_ptr<Atomic> transaction;if(!collecting)transaction=std::make_unique<Atomic>(impl_->db,impl_->undo_joined?&impl_->undo_depth:nullptr);
    struct Cleanup {bool& flag;bool prior;~Cleanup(){flag=prior;}} cleanup{impl_->cleaning,impl_->cleaning};impl_->cleaning=true;
    if(!source.empty()) {
        bool financial_history=false;
        {Statement retained(impl_->db,"SELECT 1 FROM entry_revisions e JOIN batches b ON b.id=e.batch WHERE b.source_id=? LIMIT 1");retained.text(1,source);financial_history=retained.row();}
        if(financial_history) {
            if(collecting)fail(SQLITE_CORRUPT,"Retired source still has financial dependencies");
            if(import_undo_held(source))fail(SQLITE_CONSTRAINT,"A retained import still owns transactions from this source.");
            // Revoked acceptance still supplies this session's financial replay.
            // Retire visibility now; close collects the no-longer-needed proof
            // after publishing the current baseline. No deleted-row copies are made.
            std::vector<std::pair<std::string,std::int64_t>> batches;
            {Statement rows(impl_->db,"SELECT id,revision FROM batches WHERE source_id=? AND state IN(0,4)");rows.text(1,source);while(rows.row())batches.emplace_back(rows.bytes(0),rows.integer(1));}
            for(const auto& [id,revision]:batches) {
                impl_->selected_account=account_for_batch(id).identity;
                (void)transfer_import_transition(id,std::string(operation)+"-retire-"+id,false);
                advance(impl_->db,id,revision,"undo-import",2);
            }
            impl_->selected_account=owner;
            // A reviewing group must still have pending batches. Discarded
            // batches are valid only after the group is aborted.
            Statement group(impl_->db,"UPDATE import_groups SET state=3 WHERE id=? AND state=1");group.text(1,source);group.done();
            Statement detach(impl_->db,"DELETE FROM source_associations WHERE source_batch IN(SELECT id FROM batches WHERE source_id=?)");detach.text(1,source);detach.done();
            Statement receipt(impl_->db,"INSERT INTO rollback_receipt VALUES(?,?,?,?,?,?)");receipt.text(1,operation);receipt.integer(2,owner);receipt.integer(3,integer(review.transactions));receipt.integer(4,integer(review.failures));receipt.text(5,source);receipt.integer(6,expected);receipt.done();
            release_collected_link_restorations(impl_->db);
            if(!collecting){finish([&]{verify();transaction->commit();},true);}return {std::string(operation),owner,review.transactions,review.failures};
        }
    }
    exec(impl_->db,"CREATE TEMP TABLE rollback_batches(id TEXT PRIMARY KEY); CREATE TEMP TABLE rollback_rows(batch TEXT,ordinal INTEGER,PRIMARY KEY(batch,ordinal)); CREATE TEMP TABLE rollback_operations(id TEXT PRIMARY KEY); CREATE TEMP TABLE rollback_links(id TEXT PRIMARY KEY)");
    if(source.empty())exec(impl_->db,"INSERT INTO rollback_batches SELECT id FROM batches WHERE account_id=spentit_account() AND state IN(0,3,4)");
    else {Statement scope(impl_->db,"INSERT INTO rollback_batches SELECT id FROM batches WHERE source_id=?");scope.text(1,source);scope.done();}
    exec(impl_->db,"INSERT INTO rollback_rows SELECT o.batch,o.ordinal FROM source_occurrences o JOIN rollback_batches b ON b.id=o.batch WHERE NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=o.batch AND a.ordinal=o.ordinal)");
    if(!source.empty()) {
        // Collect a ledger row retained after an earlier source undo when this
        // import now relinquishes its final ownership. Its original source may
        // differ from this import; never collect an accepted row.
        exec(impl_->db,"INSERT OR IGNORE INTO rollback_rows SELECT a.ledger_batch,a.ledger_ordinal FROM source_associations a JOIN rollback_rows r ON r.batch=a.source_batch AND r.ordinal=a.source_ordinal WHERE NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=a.ledger_batch AND t.ordinal=a.ledger_ordinal) AND NOT EXISTS(SELECT 1 FROM source_associations owner WHERE owner.ledger_batch=a.ledger_batch AND owner.ledger_ordinal=a.ledger_ordinal AND (owner.source_batch,owner.source_ordinal) NOT IN(SELECT batch,ordinal FROM rollback_rows))");
        exec(impl_->db,"INSERT OR IGNORE INTO rollback_batches SELECT DISTINCT batch FROM rollback_rows");
    }
    // Remove this account's uncommitted source ownership first. A source-only
    // transfer match may still support a retained import; keep its facts and
    // relationship history until its final owner disappears.
    exec(impl_->db,"DELETE FROM source_associations WHERE (source_batch,source_ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");
    exec(impl_->db,"DELETE FROM rollback_rows WHERE EXISTS(SELECT 1 FROM source_associations a WHERE a.ledger_batch=rollback_rows.batch AND a.ledger_ordinal=rollback_rows.ordinal)");
    // A bank proposal is an independent ownership root for its imported row.
    // Removing the originating link leaves that other account's row intact.
    const auto removing=[&](const transfers::Endpoint& endpoint){Statement row(impl_->db,"SELECT 1 FROM rollback_rows WHERE batch=? AND ordinal=?");row.text(1,endpoint.entry.batch);row.integer(2,integer(endpoint.entry.ordinal));return row.row();};
    auto values=all_transfers();std::set<std::string> doomed;
    for(const auto& value:values) {
        const auto& l=value.link;
        if(source.empty()) {
            if((l.origin.endpoint.account==owner && l.origin.state==transfers::State::draft) ||
               (l.destination.endpoint.account==owner && l.destination.state==transfers::State::draft && (l.destination.independent_evidence || l.destination.ledger_entry)))doomed.insert(l.identity);
        } else if(removing(l.origin.endpoint) || removing(l.destination.endpoint))doomed.insert(l.identity);
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
            for(const auto& id:links) {
                if(!source.empty() && !doomed.contains(id))fail(SQLITE_CONSTRAINT,"Undo the later transfer change before undoing this import.");
                changed|=doomed.insert(id).second;
            }
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
        if((request.action!=app::TransferAction::propose_match && request.action!=app::TransferAction::discard_match) || !request.evidence || (source.empty() && request.evidence->endpoint.account!=owner))continue;
        Statement evidence_row(impl_->db,"SELECT 1 FROM rollback_rows WHERE batch=? AND ordinal=?");evidence_row.text(1,request.evidence->endpoint.entry.batch);evidence_row.integer(2,integer(request.evidence->endpoint.entry.ordinal));if(!evidence_row.row())continue;
        Statement op(impl_->db,"INSERT OR IGNORE INTO rollback_operations VALUES(?)");op.text(1,proposals.bytes(0));op.done();
    }
    Statement unsafe(impl_->db,"SELECT 1 FROM rollback_operations r JOIN ledger_operations l ON l.id=r.id WHERE l.kind!=4 OR l.change!=0 OR EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=l.id AND e.did_commit=1)");if(unsafe.row())fail(SQLITE_CONSTRAINT,"Rollback would affect accepted history");
    exec(impl_->db,"DELETE FROM generated_source_matches WHERE (batch,ordinal) IN(SELECT batch,ordinal FROM rollback_rows)");
    exec(impl_->db,"DELETE FROM transfer_heads WHERE identity IN(SELECT id FROM rollback_links)");
    // A proposal head reverts to its latest retained revision. The head-step
    // trigger permits this only within the repository's cleanup boundary.
    exec(impl_->db,"UPDATE transfer_heads SET sequence=(SELECT max(v.sequence) FROM transfer_versions v WHERE v.identity=transfer_heads.identity AND v.sequence NOT IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id)) WHERE sequence IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id)");
    exec(impl_->db,"DELETE FROM transfer_versions WHERE sequence IN(SELECT l.sequence FROM ledger_operations l JOIN rollback_operations r ON r.id=l.id); DELETE FROM transfer_events WHERE operation IN(SELECT id FROM rollback_operations); DELETE FROM history_order WHERE operation IN(SELECT id FROM rollback_operations); DELETE FROM operation_registry WHERE id IN(SELECT id FROM rollback_operations); DELETE FROM ledger_operations WHERE id IN(SELECT id FROM rollback_operations)");
    if(auto* scope=ScopedStatements::current(impl_->db))scope->clear_revisions();
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
    exec(impl_->db,"DELETE FROM change_dependencies WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM history_order WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM events WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM import_balance_events WHERE (group_id,source_account) IN(SELECT group_id,source_account FROM import_group_accounts WHERE batch IN(SELECT id FROM rollback_batches)); DELETE FROM import_group_accounts WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM batch_source_currency WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM account_amount_revisions WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM retained_source_counts WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM manual_creations WHERE batch IN(SELECT id FROM rollback_batches); DELETE FROM batches WHERE id IN(SELECT id FROM rollback_batches)");
    exec(impl_->db,"DELETE FROM import_group_events WHERE group_id IN(SELECT id FROM import_groups g WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=g.id)); DELETE FROM retained_group_counts WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=retained_group_counts.group_id); UPDATE retained_group_counts SET count=(SELECT count(*) FROM import_group_accounts m WHERE m.group_id=retained_group_counts.group_id); DELETE FROM import_groups WHERE NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=import_groups.id); DELETE FROM source_headers WHERE source_id NOT IN(SELECT source_id FROM batches); DELETE FROM sources WHERE id NOT IN(SELECT source_id FROM batches)");
    std::vector<std::pair<std::string,std::int64_t>> partial;
    Statement remaining(impl_->db,"SELECT id,revision FROM batches WHERE state IN(0,4) AND ((?1='' AND account_id=spentit_account()) OR source_id=?1)");remaining.text(1,source);while(remaining.row())partial.emplace_back(remaining.bytes(0),remaining.integer(1));
    for(const auto& [id,revision]:partial){Statement count(impl_->db,"UPDATE retained_source_counts SET count=(SELECT count(*) FROM source_occurrences WHERE batch=?1) WHERE batch=?1");count.text(1,id);count.done();advance(impl_->db,id,revision,"rollback",0,0);}
    if(!collecting){Statement receipt(impl_->db,"INSERT INTO rollback_receipt VALUES(?,?,?,?,?,?)");receipt.text(1,operation);receipt.integer(2,owner);receipt.integer(3,integer(review.transactions));receipt.integer(4,integer(review.failures));receipt.text(5,source);receipt.integer(6,expected);receipt.done();}
    exec(impl_->db,"DROP TABLE rollback_links; DROP TABLE rollback_operations; DROP TABLE rollback_rows; DROP TABLE rollback_batches");
    impl_->reviews.clear();impl_->reviewed_groups.clear();impl_->reviewed_sources.clear();impl_->read_rows=VerificationRows{};impl_->read_stamps.clear();impl_->reviewed_transfers.reset();impl_->reviewed_transfer_stamp.clear();impl_->review_changes=-1;
    // Full verification is appropriate for destructive collection, and runs
    // before durability so any broken retained relationship rolls back intact.
    exec(impl_->db,"DELETE FROM review_merges WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_merges.batch AND r.ordinal=review_merges.ordinal) OR NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_merges.target_batch AND r.ordinal=review_merges.target_ordinal); DELETE FROM review_choices WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_choices.batch AND r.ordinal=review_choices.ordinal); DELETE FROM review_sources WHERE NOT EXISTS(SELECT 1 FROM source_occurrences o WHERE o.batch=source_batch AND o.ordinal=source_ordinal); DELETE FROM review_undo_members WHERE original_child IN(SELECT child FROM review_members m WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=m.batch AND r.ordinal=m.ordinal)); DELETE FROM review_members WHERE NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=review_members.batch AND r.ordinal=review_members.ordinal); DELETE FROM review_link_restorations WHERE prior=0 AND undo_id IN(SELECT u.id FROM review_undos u JOIN review_link_history h ON h.answer=u.answer WHERE NOT EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=h.operation)); DELETE FROM review_link_history WHERE NOT EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=review_link_history.operation); DELETE FROM review_stack WHERE NOT EXISTS(SELECT 1 FROM action_headers h WHERE h.id=review_stack.answer AND h.state=1)");
    release_collected_link_restorations(impl_->db);
    if(!collecting){finish([&]{verify();transaction->commit();},true);}return {std::string(operation),owner,review.transactions,review.failures};
}

// A Review Link may have changed a committed relationship. Reverse its recorded
// snapshots with ordinary persistent history rather than erasing that history.
std::vector<std::string> ImportStore::import_link_undos(std::string_view source) const {
    std::set<std::string> answers;
    const auto values=all_transfers();
    const auto belongs=[&](const transfers::Endpoint& e){Statement b(impl_->db,"SELECT 1 FROM batches WHERE id=? AND source_id=?");b.text(1,e.entry.batch);b.text(2,source);return b.row();};
    for(const auto& value:values) {
        const auto& l=value.link;
        if(l.origin.state==transfers::State::removed && l.destination.state==transfers::State::removed)continue;
        if(!belongs(l.origin.endpoint) && !belongs(l.destination.endpoint))continue;
        if(l.origin.state!=transfers::State::committed && l.destination.state!=transfers::State::committed)continue;
        Statement h(impl_->db,"SELECT h.answer,h.result_payload FROM review_link_history h JOIN review_answers a ON a.id=h.answer WHERE h.identity=? AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=h.answer) ORDER BY a.sequence DESC LIMIT 1");h.text(1,l.identity);
        if(!h.row() || h.bytes(1)!=transfer_codec::encode(value))fail(SQLITE_CONSTRAINT,"Undo the later transfer change before undoing this import.");
        answers.insert(h.bytes(0));
    }
    std::vector<std::string> result;
    Statement ordered(impl_->db,"SELECT id FROM review_answers ORDER BY sequence DESC");
    while(ordered.row())if(answers.contains(ordered.bytes(0))) {
        const auto answer=ordered.bytes(0);
        Statement links(impl_->db,"SELECT identity,result_payload FROM review_link_history WHERE answer=?");links.text(1,answer);
        while(links.row()) {
            const auto current=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==links.bytes(0);});
            if(current==values.end() || transfer_codec::encode(*current)!=links.bytes(1))fail(SQLITE_CONSTRAINT,"Undo the later transfer change before undoing this import.");
        }
        Statement members(impl_->db,"SELECT batch,ordinal,committed,result FROM review_members WHERE answer=?");members.text(1,answer);
        while(members.row()) {
            Statement current(impl_->db,members.integer(2)?"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?":"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");current.text(1,members.bytes(0));current.integer(2,members.integer(1));
            if(!current.row() || current.integer(0)!=members.integer(3))fail(SQLITE_CONSTRAINT,"Undo the later transaction edit before undoing this import.");
        }
        result.push_back(answer);
    }
    return result;
}
app::RollbackReceipt ImportStore::undo_linked_import(std::string_view operation,std::string_view source,const app::RollbackReview& review,const std::vector<std::string>& answers) {
    if(operation.size()>60)fail(SQLITE_MISUSE,"Import undo operation identifier is too long");
    const auto key=std::string(operation)+"."+std::string(source);
    Statement shared(impl_->db,"SELECT 1 FROM source_associations a JOIN batches b ON b.id=a.ledger_batch JOIN batches owner ON owner.id=a.source_batch WHERE b.source_id=?1 AND owner.source_id!=?1");shared.text(1,source);
    if(shared.row())fail(SQLITE_CONSTRAINT,"Another import now uses these linked transactions. Undo that import first.");
    const auto selected=impl_->selected_account;
    struct Scope {Impl& impl;std::int64_t selected;bool cleaning;~Scope(){impl.selected_account=selected;impl.cleaning=cleaning;}} scope{*impl_,selected,impl_->cleaning};
    Atomic transaction(impl_->db,impl_->undo_joined?&impl_->undo_depth:nullptr);impl_->cleaning=true;
    std::size_t serial=0;
    for(const auto& answer:answers)restore_review_answer(std::string(operation)+"-restore-"+std::to_string(++serial),answer);
    Statement batches(impl_->db,"SELECT id,account_id FROM batches WHERE source_id=? ORDER BY rowid");batches.text(1,source);
    std::vector<std::pair<std::string,std::int64_t>> retained;
    while(batches.row())retained.emplace_back(batches.bytes(0),batches.integer(1));
    for(const auto& [id,account]:retained) {
        impl_->selected_account=account;
        if(!protected_import_rows(id).empty())fail(SQLITE_CONSTRAINT,"Another accepted transfer still needs this import.");
        (void)transfer_import_transition(id,std::string(operation)+"-discard-"+std::to_string(++serial),false);
        advance(impl_->db,id,batch(impl_->db,id).revision,"discard",2);
        Statement associations(impl_->db,"DELETE FROM source_associations WHERE source_batch=?");associations.text(1,id);associations.done();
        Statement errors(impl_->db,"DELETE FROM import_file_errors WHERE batch=?");errors.text(1,id);errors.done();
        Statement completions(impl_->db,"DELETE FROM import_completions WHERE batch=?");completions.text(1,id);completions.done();
    }
    // Settings created during the import survive transaction undo, just as in
    // account rollback. Capture them before aborting the retained group.
    for(const auto& alias:account_aliases()){Statement keep(impl_->db,"INSERT OR IGNORE INTO retained_account_aliases VALUES(?,?)");keep.integer(1,alias.account);keep.text(2,alias.name);keep.done();}
    for(const auto& [id,account]:retained){(void)account;for(const auto& category:source_categories(id)){Statement keep(impl_->db,"INSERT OR IGNORE INTO retained_category_names VALUES(?)");keep.text(1,category.name);keep.done();}}
    Statement opening(impl_->db,"INSERT OR REPLACE INTO retained_opening_reviews SELECT m.account_id,m.opening,m.opening_date FROM import_group_accounts m JOIN account a ON a.singleton=m.account_id WHERE m.group_id=? AND m.created=1 AND m.opening IS NOT NULL AND a.confirmed=0");opening.text(1,source);opening.done();
    Statement group(impl_->db,"UPDATE import_groups SET state=3 WHERE id=? AND state=1");group.text(1,source);group.done();
    exec(impl_->db,"DELETE FROM review_stack WHERE NOT EXISTS(SELECT 1 FROM action_headers h WHERE h.id=review_stack.answer AND h.state=1)");
    Statement receipt(impl_->db,"INSERT INTO rollback_receipt VALUES(?,?,?,?,?,?)");receipt.text(1,key);receipt.integer(2,selected);receipt.integer(3,integer(review.transactions));receipt.integer(4,integer(review.failures));receipt.text(5,source);receipt.integer(6,review.token);receipt.done();
    impl_->selected_account=selected;impl_->reviews.clear();impl_->reviewed_groups.clear();impl_->reviewed_sources.clear();impl_->read_rows=VerificationRows{};impl_->read_stamps.clear();impl_->reviewed_transfers.reset();impl_->reviewed_transfer_stamp.clear();impl_->review_changes=-1;
    release_collected_link_restorations(impl_->db);
    finish([&]{verify();transaction.commit();},true);return {std::string(operation),selected,review.transactions,review.failures};
}
