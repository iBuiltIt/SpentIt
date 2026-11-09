imports::Occurrence ImportStore::create_manual(std::string_view id,const imports::DraftRow& input) {
    identifier(id);const auto acct=account();require_writable_account(impl_->db,acct.identity);
    if(input.transaction.original_imported_amount())fail(SQLITE_CONSTRAINT,"A manual transaction cannot have an imported amount");
    input.transaction.effective_total().require_compatible(acct.opening);
    auto row=input;row.payee_owner=row.memo_owner=FieldOwnership::manual;row.payee_rule.reset();row.memo_rule.reset();
    auto allocations=row.transaction.allocations();std::set<std::string> identities;
    for(std::size_t i=0;i<allocations.size();++i){auto& a=allocations[i];
        if(a.source_ordinal || a.category_rule || a.memo_rule)fail(SQLITE_CONSTRAINT,"Manual splits cannot have imported or rule assignments");
        if(a.identity.empty())a.identity="manual-d-0-"+std::to_string(i);
        identifier(a.identity);if(a.identity.starts_with("source-") || !identities.insert(a.identity).second)fail(SQLITE_CONSTRAINT,"Invalid split identity");
        a.category_owner=a.memo_owner=FieldOwnership::manual;
    }
    row.transaction=Transaction::manual(row.transaction.effective_total(),std::move(allocations));
    if(!row.remainder_allocation.empty() && !identities.contains(row.remainder_allocation))fail(SQLITE_CONSTRAINT,"Choose a balancing split");
    const auto bytes=manual_codec::encode(row);const auto parsed=manual_codec::parse(bytes,acct.currency);
    const auto digest=session_snapshot::digest("manual:"+std::to_string(acct.identity)+":"+bytes);
    if(const auto outcome=ActionChain(impl_->db).durable_outcome(id,digest)) {
        if(*outcome==ActionChain::Outcome::applied)return {std::string(id),0};
        throw app::OperationAlreadyCompleted(std::string(id));
    }
    Atomic transaction(impl_->db);
    Statement retry(impl_->db,"SELECT b.account_id,b.parser,s.bytes FROM batches b JOIN sources s ON s.id=b.source_id WHERE b.id=?");retry.text(1,id);
    if(retry.row()){
        if(retry.integer(0)!=acct.identity || retry.bytes(1)!="manual-v1" || retry.bytes(2)!=bytes)fail(SQLITE_CONSTRAINT,"Transaction identity was reused with different content");
        transaction.commit();return {std::string(id),0};
    }
    for(const auto& a:row.transaction.allocations()) {
        if(resolve_account_name(a.category) || qif::transfer_destination(a.category))fail(SQLITE_CONSTRAINT,"Save the transaction before assigning a transfer account");
        if(a.category_binding){validate_binding(impl_->db,a);const auto current=resolve_category(a.category_binding->identity);
            if(!current.assignable || current.path!=a.category || a.category_binding->catalog_revision!=catalog_sequence(impl_->db))fail(SQLITE_CONSTRAINT,"Choose an available category");}
        else if(!a.category.empty())fail(SQLITE_CONSTRAINT,"Choose a category from the list");
    }
    Statement source(impl_->db,"INSERT INTO sources(id,bytes) VALUES(?,?)");source.text(1,id);source.blob(2,bytes);source.done();
    retain_source_header(impl_->db,id,"manual","manual","");
    Statement insert(impl_->db,"INSERT INTO batches VALUES(?,0,'manual-v1',0,0,1,0,0,spentit_account(),?1,NULL)");insert.text(1,id);insert.done();
    Statement count(impl_->db,"INSERT INTO retained_source_counts VALUES(?,1)");count.text(1,id);count.done();
    pin_source_currency(impl_->db,id,acct.currency);write_occurrence(impl_->db,id,0,parsed.records.front());
    Statement record(impl_->db,"INSERT INTO records(batch,ordinal,sequence,raw,source_date,original,payee,reference) VALUES(?,0,(SELECT coalesce(max(seq),0)+1 FROM sqlite_sequence WHERE name='records'),?,?,NULL,?,'')");
    record.text(1,id);record.blob(2,bytes);record.text(3,row.date);record.text(4,row.payee);record.done();
    version(impl_->db,{std::string(id),0},0,row,{imports::Disposition::include,{}});
    Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'ingest')");event.text(1,id);event.done();
    Statement creation(impl_->db,"INSERT INTO manual_creations VALUES(?,(SELECT revision+1 FROM mutation_generation WHERE singleton=1))");creation.text(1,id);creation.done();
    app::ActionSummary action;action.id=id;action.payload=id;action.kind=app::ActionKind::manual_creation;action.label="Add transaction";action.count=1;
    impl_->pending_action=std::move(action);impl_->pending_action_digest=digest;
    finish([&]{transaction.commit();});return {std::string(id),0};
}
void ImportStore::undo_manual(std::string_view id,std::int64_t generation) {
    if(!impl_->undo_token){Statement completed(impl_->db,"SELECT source,epoch,account FROM rollback_receipt WHERE operation=?");completed.text(1,"undo-manual-"+std::to_string(generation));if(completed.row()){if(completed.bytes(0)!=id || completed.integer(1)!=generation || completed.integer(2)!=account().identity)fail(SQLITE_CONSTRAINT,"Undo retry changed its request");return;}}
    const auto head=action_head();
    if(!impl_->undo_token) {
        if(!head || head->kind!=app::ActionKind::manual_creation || head->id!=id || generation!=ledger_edit_generation())
            fail(SQLITE_CONSTRAINT,"Undo the latest action first.");
        undo_action("undo-manual-"+std::to_string(action_token().epoch),preview_undo(id,action_token()));return;
    }
    {
    require_account(id);Statement kind(impl_->db,"SELECT parser,state FROM batches WHERE id=?");kind.text(1,id);
    if(!kind.row() || kind.bytes(0)!="manual-v1")fail(SQLITE_CONSTRAINT,"Choose a manually entered transaction");
    if(kind.integer(1)==2)return;
    Statement created(impl_->db,"SELECT generation FROM manual_creations WHERE batch=? AND batch IN(SELECT id FROM session_history WHERE domain='manual')");created.text(1,id);
    if(!created.row() || !head || head->id!=id || transaction_accepted({std::string(id),0}))fail(SQLITE_CONSTRAINT,"Another change ended Undo for this transaction");
    }
    rollback_scope(impl_->undo_operation,ledger_edit_generation(),id);
}
std::optional<imports::Occurrence> ImportStore::imported_original(const imports::Occurrence& id) const {
    require_account(id.batch);
    Statement source(impl_->db,"SELECT a.source_batch,a.source_ordinal FROM source_associations a JOIN batches b ON b.id=a.source_batch WHERE a.ledger_batch=? AND a.ledger_ordinal=? AND b.parser!='manual-v1' ORDER BY CASE WHEN a.source_batch=? THEN 0 ELSE 1 END,a.source_batch,a.source_ordinal LIMIT 1");
    source.text(1,id.batch);source.integer(2,integer(id.ordinal));source.text(3,id.batch);
    if(source.row())return imports::Occurrence{source.bytes(0),static_cast<std::size_t>(source.integer(1))};
    Statement kind(impl_->db,"SELECT parser FROM batches WHERE id=?");kind.text(1,id.batch);if(!kind.row())fail(SQLITE_NOTFOUND);
    if(kind.bytes(0)!="manual-v1")return id;
    return {};
}

bool ImportStore::manual_outcome(std::string_view id) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement result(impl_->db,"SELECT account_id FROM batches WHERE id=? AND parser='manual-v1'");result.text(1,id);
    return result.row() && result.integer(0)==account().identity;
}
