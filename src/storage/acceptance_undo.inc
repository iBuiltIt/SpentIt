void ImportStore::record_acceptance_action(std::string_view operation,std::string_view recipe,const std::vector<app::TransferView>& prior,std::string_view digest,std::size_t count) {
    Statement saved(impl_->db,"INSERT INTO acceptance_states VALUES(?,?,NULL)");saved.text(1,operation);saved.blob(2,recipe);saved.done();
    Statement root(impl_->db,"INSERT INTO review_answers VALUES(?1,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'acceptance',hex(?2),?3)");root.text(1,operation);root.blob(2,digest);root.integer(3,ledger_edit_generation());root.done();
    for(const auto& current:all_transfers()) {
        const auto old=std::find_if(prior.begin(),prior.end(),[&](const auto& value){return value.link.identity==current.link.identity;});
        if(old==prior.end())fail(SQLITE_CORRUPT,"Acceptance created an unexpected relationship");
        if(transfer_codec::encode(*old)==transfer_codec::encode(current))continue;
        Statement history(impl_->db,"INSERT INTO review_link_history SELECT ?1,?2,?3,?4,l.id,?6 FROM ledger_operations l JOIN transfer_heads h ON h.sequence=l.sequence WHERE h.identity=?5");history.text(1,operation);history.text(2,current.link.identity);history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(current));history.text(5,current.link.identity);history.integer(6,old->sequence);history.done();
    }
    transfer_codec::Reader input{recipe};if(input.number()!=1)fail(SQLITE_CORRUPT);
    auto accounts=input.number();while(accounts-->0){(void)input.number();(void)input.boolean();}
    auto batches=input.number();std::size_t serial=0;
    while(batches-->0){const auto batch_id=input.text();(void)input.number();const auto prior_revision=input.number();
        Statement decisions(impl_->db,"SELECT m.ordinal,m.draft_revision,(SELECT max(v.revision) FROM versions v WHERE v.batch=o.batch AND v.ordinal=m.ordinal AND v.revision<=?3) FROM acceptance_members m JOIN operations o ON o.id=m.operation WHERE o.batch=?2 AND (o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1)) AND m.draft_revision>?3");decisions.text(1,operation);decisions.text(2,batch_id);decisions.integer(3,prior_revision);
        while(decisions.row()){if(decisions.null(2))fail(SQLITE_CORRUPT);Statement member(impl_->db,"INSERT INTO review_members VALUES(?,?,?,0,?,?,?)");member.text(1,operation);member.text(2,batch_id);member.integer(3,decisions.integer(0));member.integer(4,decisions.integer(2));member.integer(5,decisions.integer(1));member.text(6,std::string(operation)+"-decision-"+std::to_string(++serial));member.done();}
    }
    app::ActionSummary action;action.id=operation;action.payload=operation;action.kind=app::ActionKind::acceptance;action.label="Accept transactions";action.count=integer(count);
    impl_->pending_action=std::move(action);impl_->pending_action_digest=digest;
}

void ImportStore::undo_acceptance(std::string_view operation,std::string_view original) {
    if(!impl_->undo_token)fail(SQLITE_CONSTRAINT,"Undo the latest action first.");
    if(!impl_->undo_target.empty()?impl_->undo_target!=original:(!action_head() || action_head()->id!=original))fail(SQLITE_CONSTRAINT,"Undo the latest action first.");
    std::string payload;
    {Statement saved(impl_->db,"SELECT payload FROM acceptance_states WHERE id=? AND undo_id IS NULL");saved.text(1,original);if(!saved.row())fail(SQLITE_CONSTRAINT,"Acceptance cannot be undone");payload=saved.bytes(0);}
    transfer_codec::Reader in{payload};if(in.number()!=1)fail(SQLITE_CORRUPT);
    const auto count=[&]{const auto n=in.number();if(n<0 || n>static_cast<std::int64_t>(qif::max_records))fail(SQLITE_CORRUPT);return n;};
    AcceptanceBaseline before;
    for(auto n=count();n>0;--n){const auto id=in.number();before.accounts.push_back({id,in.boolean()});}
    for(auto n=count();n>0;--n){const auto id=in.text();const auto state=in.number();before.batches.push_back({id,state,in.number()});}
    for(auto n=count();n>0;--n){const auto id=in.text();const auto state=in.number();before.groups.push_back({id,state,in.text()});}in.finish();
    std::vector<std::string> publications;
    {Statement rows(impl_->db,"SELECT id FROM ledger_operations WHERE kind=0 AND (id=?1 OR id IN(SELECT operation FROM acceptance_operations WHERE parent=?1)) ORDER BY sequence DESC");rows.text(1,original);while(rows.row())publications.push_back(rows.bytes(0));}
    struct Scope {Impl& state;std::int64_t selected;bool cleaning;~Scope(){state.selected_account=selected;state.cleaning=cleaning;}} scope{*impl_,impl_->selected_account,impl_->cleaning};
    Atomic transaction(impl_->db,impl_->undo_joined?&impl_->undo_depth:nullptr);CommandFlag command(impl_->account_command);impl_->cleaning=true;
    Statement reversal(impl_->db,"UPDATE acceptance_states SET undo_id=? WHERE id=? AND undo_id IS NULL");reversal.text(1,operation);reversal.text(2,original);reversal.done();
    std::size_t serial=0;
    for(const auto& publication:publications) {
        {Statement owner(impl_->db,"SELECT b.account_id FROM operations o JOIN batches b ON b.id=o.batch WHERE o.id=?");owner.text(1,publication);if(!owner.row())fail(SQLITE_CORRUPT);impl_->selected_account=owner.integer(0);}
        const auto receipt=outcome(publication);if(!receipt)fail(SQLITE_CORRUPT);
        const auto balance_before=committed_balance();
        Statement members(impl_->db,"SELECT ordinal,publication_revision FROM acceptance_members WHERE operation=? AND decision=1");members.text(1,publication);
        while(members.row()) {
            const imports::Occurrence id{receipt->batch,static_cast<std::size_t>(members.integer(0))};
            const auto current=committed_entry(id);const auto published=entry_snapshot(id,members.integer(1));
            if(!current.current.active || !review_restores_revision(impl_->db,id,true,current.current.revision,members.integer(1)) || !same_draft(current.current.draft,published.draft))fail(SQLITE_CONSTRAINT,"Undo later transaction changes first.");
            Statement owner(impl_->db,"SELECT 1 FROM accepted_transactions WHERE batch=? AND ordinal=? AND operation=?");owner.text(1,id.batch);owner.integer(2,integer(id.ordinal));owner.text(3,publication);if(!owner.row())fail(SQLITE_CORRUPT);
            Statement head(impl_->db,"DELETE FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,id.batch);head.integer(2,integer(id.ordinal));head.done();
            Statement accepted(impl_->db,"DELETE FROM accepted_transactions WHERE batch=? AND ordinal=?");accepted.text(1,id.batch);accepted.integer(2,integer(id.ordinal));accepted.done();
        }
        const auto balance_after=committed_balance();
        // Kind 6 is membership revocation, not a balancing transaction. A zero
        // stored delta avoids negating INT64_MIN; verification derives the exact
        // transition from the existing publication membership.
        append_financial(impl_->db,std::string(operation)+"-accept-"+std::to_string(++serial),static_cast<app::ChangeKind>(6),{receipt->batch,0},-1,0,balance_before,Money(0,balance_before.currency()),balance_after,publication,financial_sequence(impl_->db),catalog_sequence(impl_->db));
        Statement associations(impl_->db,"DELETE FROM source_associations WHERE source_batch=?1 AND source_ordinal IN(SELECT ordinal FROM acceptance_members WHERE operation=?2 AND association_created=1)");associations.text(1,receipt->batch);associations.text(2,publication);associations.done();
    }
    for(const auto& old:before.batches) {
        const auto current=batch(impl_->db,old.id);if(current.revision==INT64_MAX)fail(SQLITE_FULL);
        Statement restore(impl_->db,"UPDATE batches SET state=?,revision=revision+1 WHERE id=?");restore.integer(1,old.state);restore.text(2,old.id);restore.done();
        Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,?,'unaccept')");event.text(1,old.id);event.integer(2,current.revision+1);event.done();
    }
    restore_review_answer(operation,original);
    for(const auto& old:before.groups){Statement group(impl_->db,"UPDATE import_groups SET state=?,publication=NULL WHERE id=?");group.integer(1,old.state);group.text(2,old.id);group.done();}
    // Restore the flag after finish() drops associations. A later acceptance on
    // the same account stored no frozen change of its own, so writing this
    // action's earlier flag would thaw the account while that acceptance remains.
    finish([&]{sync_acceptance_frozen(impl_->db);transaction.commit();});
}
