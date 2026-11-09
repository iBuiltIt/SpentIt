bool ImportStore::action_preparing() const {return ActionChain(impl_->db).preparing().has_value();}
app::ActionToken ImportStore::action_token() const { return ActionChain(impl_->db).token(); }
void ImportStore::begin_import_action(std::string_view source,std::string_view request) {
    const auto digest=session_snapshot::digest(request);
    ActionChain chain(impl_->db);
    if(chain.outcome(source,digest))throw app::OperationAlreadyCompleted(std::string(source));
    app::ActionSummary action;action.id=source;action.payload=source;
    action.kind=app::ActionKind::import;action.label="Import file";
    chain.begin_import(action_token(),action,digest);
}
void ImportStore::complete_import_action(std::string_view source,bool awaiting_decision) {
    ActionChain chain(impl_->db);
    chain.import_stage(source,awaiting_decision?app::ImportStage::awaiting_decision:app::ImportStage::materialized);
    if(!awaiting_decision)chain.finish_import(source,false);
}
std::optional<app::ActionSummary> ImportStore::action_head() const { return ActionChain(impl_->db).head(); }
std::vector<app::ActionSummary> ImportStore::action_history(std::int64_t before,std::size_t limit) const {
    return ActionChain(impl_->db).summaries(before,limit);
}
void ImportStore::publish_review_action(std::string_view operation) {
    Statement saved(impl_->db,"SELECT question,request FROM review_answers WHERE id=?");saved.text(1,operation);
    if(!saved.row())fail(SQLITE_CORRUPT,"Undo operation is missing");
    const auto question=saved.bytes(0);
    app::ActionSummary action;action.id=operation;action.payload=operation;
    action.kind=question.starts_with("rules-")?app::ActionKind::rule_application:
        question.starts_with("ledger-")?app::ActionKind::transaction:app::ActionKind::review;
    action.label=question=="ledger-transfer"?"Change transfer":question=="review-exclusion"?"Change transfer exclusions":question.starts_with("rules-")?"Apply rules":question=="ledger-delete"?"Delete transactions":
        question=="transfer-assignment"?"Change category":question.starts_with("ledger-")?"Edit transactions":"Review transactions";
    if(question=="ledger-edit-batch"){const auto encoded=saved.bytes(1);transfer_codec::Reader request{encoded};const auto kind=static_cast<app::LedgerSelectionAction>(request.number());action.label=kind==app::LedgerSelectionAction::rename_payee?"Rename payee":kind==app::LedgerSelectionAction::set_memo?"Set memo":"Restore imported values";}
    Statement count(impl_->db,"SELECT count(*) FROM review_members WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");count.text(1,operation);
    if(!count.row())fail(SQLITE_CORRUPT);action.count=question=="review-exclusion"?1:count.integer(0);
    auto digest=session_snapshot::digest(saved.bytes(1));
    Statement canonical(impl_->db,"SELECT digest FROM command_completions WHERE id=?");canonical.text(1,operation);
    if(canonical.row())digest=canonical.bytes(0);
    ActionChain(impl_->db).applied(action_token(),action,digest);
}
app::UndoPreview ImportStore::preview_undo(std::string_view id,const app::ActionToken& expected) const {
    ActionChain chain(impl_->db);chain.require_current(expected);chain.require_idle();
    const auto current=chain.head();
    if(!current || current->id!=id)fail(SQLITE_CONSTRAINT,"Undo the latest action first.");
    app::UndoPreview result;result.token=expected;result.action=*current;
    const auto relationship_scope=[&]{
        std::set<imports::Occurrence> transactions;
        Statement rows(impl_->db,"SELECT batch,ordinal FROM review_members WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1) UNION SELECT batch,ordinal FROM review_repairs WHERE answer=?1");rows.text(1,current->payload);while(rows.row())transactions.insert({rows.bytes(0),static_cast<std::size_t>(rows.integer(1))});
        Statement links(impl_->db,"SELECT prior_payload,result_payload FROM review_link_history WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");links.text(1,current->payload);
        while(links.row())for(int column:{0,1})if(!links.null(column)){const auto value=transfer_codec::view(links.bytes(column));for(const auto* side:{&value.link.origin,&value.link.destination}){result.accounts.push_back(side->endpoint.account);transactions.insert(side->endpoint.entry);}}
        std::sort(result.accounts.begin(),result.accounts.end());result.accounts.erase(std::unique(result.accounts.begin(),result.accounts.end()),result.accounts.end());
        result.action.count=std::max(result.action.count,integer(transactions.size()));
    };
    if(current->kind==app::ActionKind::opening || current->kind==app::ActionKind::currency) {
        Statement owner(impl_->db,"SELECT account FROM account_operations WHERE id=?");owner.text(1,current->payload);
        if(!owner.row())fail(SQLITE_CORRUPT,"Account Undo details are missing");
        result.accounts.push_back(owner.integer(0));result.action.count=0;
        if(current->kind==app::ActionKind::currency){
            std::set<imports::Occurrence> transactions;
            Statement rows(impl_->db,"SELECT batch,ordinal FROM account_amount_revisions WHERE operation=? AND kind IN(0,2) AND position=0");rows.text(1,current->payload);while(rows.row())transactions.insert({rows.bytes(0),static_cast<std::size_t>(rows.integer(1))});
            Statement links(impl_->db,"SELECT v.payload FROM transfer_versions v JOIN ledger_operations l ON l.sequence=v.sequence WHERE l.kind=5 AND l.undo_of=?");links.text(1,current->payload);
            while(links.row()){const auto value=transfer_codec::view(links.bytes(0));for(const auto* side:{&value.link.origin,&value.link.destination}){result.accounts.push_back(side->endpoint.account);transactions.insert(side->endpoint.entry);}}
            // The command's account remains first for inverse dispatch.
            std::set<std::int64_t> seen;std::erase_if(result.accounts,[&](auto account){return !seen.insert(account).second;});result.action.count=integer(transactions.size());
        }
        return result;
    }
    if(current->kind==app::ActionKind::acceptance) {
        Statement accounts(impl_->db,"SELECT DISTINCT b.account_id FROM operations o JOIN batches b ON b.id=o.batch WHERE o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1) UNION SELECT t.account FROM transfer_events t JOIN acceptance_operations m ON m.operation=t.operation WHERE m.parent=?1 ORDER BY 1");accounts.text(1,current->payload);while(accounts.row())result.accounts.push_back(accounts.integer(0));relationship_scope();return result;
    }
    if(current->kind==app::ActionKind::import || current->kind==app::ActionKind::manual_creation) {
        const auto scope=review_import_undo(current->payload);
        result.accounts=scope.affected_accounts;result.action.count=integer(scope.transactions);
        return result;
    }
    Statement accounts(impl_->db,"SELECT DISTINCT b.account_id FROM review_members m JOIN batches b ON b.id=m.batch WHERE m.answer=?1 OR m.answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1) ORDER BY b.account_id");accounts.text(1,id);
    while(accounts.row())result.accounts.push_back(accounts.integer(0));
    relationship_scope();return result;
}
std::optional<app::ActionReceipt> ImportStore::undo_action_outcome(std::string_view operation,const app::UndoPreview& preview) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    transfer_codec::Writer request;request.text("undo-action");request.text(preview.action.id);
    request.number(preview.token.session);request.number(preview.token.epoch);request.text(preview.token.head);
    const auto done=ActionChain(impl_->db).durable_outcome(operation,session_snapshot::digest(request.bytes));
    if(!done)return {};
    if(*done==ActionChain::Outcome::expired)throw app::OperationAlreadyCompleted(std::string(operation));
    return app::ActionReceipt{std::string(operation),preview.action.id};
}
app::ActionReceipt ImportStore::undo_action(std::string_view operation,const app::UndoPreview& preview) {
    identifier(operation);
    transfer_codec::Writer request;request.text("undo-action");request.text(preview.action.id);
    request.number(preview.token.session);request.number(preview.token.epoch);request.text(preview.token.head);
    const auto digest=session_snapshot::digest(request.bytes);
    ActionChain chain(impl_->db);
    if(const auto done=chain.durable_outcome(operation,digest)) {
        if(*done==ActionChain::Outcome::expired)throw app::OperationAlreadyCompleted(std::string(operation));
        return {std::string(operation),preview.action.id};
    }
    const auto checked=preview_undo(preview.action.id,preview.token);
    if(!checked.refusal.empty())fail(SQLITE_CONSTRAINT,checked.refusal.c_str());
    if(impl_->undo_token)fail(SQLITE_MISUSE,"Undo cannot be nested");
    struct Guard {
        Impl& state;
        ~Guard(){state.undo_token.reset();state.undo_operation.clear();state.undo_digest.clear();}
    } guard{*impl_};
    impl_->undo_token=preview.token;impl_->undo_operation=operation;impl_->undo_digest=digest;
    switch(checked.action.kind) {
    case app::ActionKind::transaction:
    case app::ActionKind::review:
    case app::ActionKind::rule_application: undo_review(operation);break;
    case app::ActionKind::import: undo_import(operation,checked.action.payload,ledger_edit_generation());break;
    case app::ActionKind::acceptance: undo_acceptance(operation,checked.action.payload);break;
    case app::ActionKind::manual_creation: {
        struct Selection {std::int64_t& value;std::int64_t old;~Selection(){value=old;}} selection{impl_->selected_account,impl_->selected_account};
        impl_->selected_account=checked.accounts.at(0);undo_manual(checked.action.payload,ledger_edit_generation());break;
    }
    case app::ActionKind::opening:
    case app::ActionKind::currency: {
        const auto selected=impl_->selected_account;
        struct Selection {std::int64_t& value;std::int64_t old;~Selection(){value=old;}} selection{impl_->selected_account,selected};
        impl_->selected_account=checked.accounts.at(0);
        undo_account_operation(operation,checked.action.payload);break;
    }
    default:fail(SQLITE_CONSTRAINT,"This action cannot yet be undone.");
    }
    return {std::string(operation),checked.action.id};
}

void ImportStore::verify_action_restorations() const {
    Statement payloads(impl_->db,R"SQL(
SELECT 1 FROM action_headers h WHERE h.state IN(0,1) AND (
 (h.kind IN(0,1,2) AND (NOT EXISTS(SELECT 1 FROM review_answers a WHERE a.id=h.payload) OR NOT EXISTS(SELECT 1 FROM review_stack s WHERE s.answer=h.payload)))
 OR (h.kind=3 AND NOT EXISTS(SELECT 1 FROM manual_creations m WHERE m.batch=h.payload))
 OR (h.kind IN(4,5) AND NOT EXISTS(SELECT 1 FROM account_operations o WHERE o.id=h.payload AND o.kind=h.kind))
 OR (h.kind=6 AND NOT EXISTS(SELECT 1 FROM acceptance_states s WHERE s.id=h.payload AND s.undo_id IS NULL))
 OR (h.kind=7 AND NOT EXISTS(SELECT 1 FROM sources s WHERE s.id=h.payload)))
UNION ALL
SELECT 1 FROM review_members m JOIN action_headers h ON h.payload=m.answer WHERE h.state=1 AND
 ((m.committed=0 AND (NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=m.batch AND v.ordinal=m.ordinal AND v.revision=m.prior) OR NOT EXISTS(SELECT 1 FROM versions v WHERE v.batch=m.batch AND v.ordinal=m.ordinal AND v.revision=m.result)))
 OR (m.committed=1 AND (NOT EXISTS(SELECT 1 FROM entry_revisions e WHERE e.batch=m.batch AND e.ordinal=m.ordinal AND e.revision=m.prior) OR NOT EXISTS(SELECT 1 FROM entry_revisions e WHERE e.batch=m.batch AND e.ordinal=m.ordinal AND e.revision=m.result))))
)SQL");
    if(payloads.row())fail(SQLITE_CORRUPT,"Active Undo action has missing restoration details");
    Statement link_restorations(impl_->db,"SELECT r.identity,r.consumed,r.restored,r.prior,l.sequence,h.prior_sequence FROM review_link_restorations r JOIN review_undos u ON u.id=r.undo_id JOIN review_link_history h ON h.answer=u.answer AND h.identity=r.identity JOIN ledger_operations l ON l.id=h.operation");
    while(link_restorations.row())if(link_restorations.integer(3)!=link_restorations.integer(5) || !review_restores_link(impl_->db,link_restorations.bytes(0),link_restorations.integer(1),link_restorations.integer(4)))fail(SQLITE_CORRUPT,"Relationship restoration lineage is inconsistent");
    // Source removal may collect an undone source proposal. Its compact bridge
    // remains while an earlier action needs the restored relationship identity.
    Statement missing_links(impl_->db,"SELECT 1 FROM review_link_restorations r JOIN review_undos u ON u.id=r.undo_id WHERE NOT EXISTS(SELECT 1 FROM transfer_versions v WHERE v.identity=r.identity AND v.sequence=r.restored) OR (NOT EXISTS(SELECT 1 FROM review_link_history h WHERE h.answer=u.answer AND h.identity=r.identity) AND NOT EXISTS(SELECT 1 FROM action_headers a JOIN transfer_versions p ON p.identity=r.identity AND p.sequence=r.prior JOIN transfer_versions v ON v.identity=r.identity AND v.sequence=r.restored WHERE a.id=u.answer AND a.state=2 AND p.payload=v.payload AND r.prior<r.consumed AND r.consumed<r.restored))");if(missing_links.row())fail(SQLITE_CORRUPT,"Relationship restoration details are missing");
    Statement dangling(impl_->db,"SELECT 1 FROM review_undo_members u JOIN review_undos r ON r.id=u.undo_id WHERE NOT EXISTS(SELECT 1 FROM review_members m WHERE m.answer=r.answer AND m.child=u.original_child)");
    if(dangling.row())fail(SQLITE_CORRUPT,"Transaction restoration is missing its original operation");
    Statement members(impl_->db,"SELECT m.batch,m.ordinal,m.committed,m.prior,m.result,u.consumed,u.restored,u.child FROM review_undo_members u JOIN review_undos r ON r.id=u.undo_id JOIN review_members m ON m.answer=r.answer AND m.child=u.original_child");
    while(members.row()) {
        const imports::Occurrence id{members.bytes(0),static_cast<std::size_t>(members.integer(1))};
        const bool committed=members.integer(2)!=0;
        if(!review_restores_revision(impl_->db,id,committed,members.integer(5),members.integer(4)))fail(SQLITE_CORRUPT,"Transaction restoration lineage is inconsistent");
        if(committed) {
            const auto original=entry_snapshot(id,members.integer(3)),restored=entry_snapshot(id,members.integer(6));
            if(original.active!=restored.active || !same_draft(original.draft,restored.draft))fail(SQLITE_CORRUPT,"Transaction restoration does not match its original state");
            Statement ledger(impl_->db,"SELECT 1 FROM ledger_operations WHERE id=? AND kind=3 AND batch=? AND ordinal=? AND prior_revision=? AND result_revision=?");
            ledger.text(1,members.bytes(7));ledger.text(2,id.batch);ledger.integer(3,integer(id.ordinal));ledger.integer(4,members.integer(5));ledger.integer(5,members.integer(6));
            if(!ledger.row())fail(SQLITE_CORRUPT,"Transaction restoration has no financial transition");
        } else {
            if(!same_draft(review_draft(id,members.integer(3)),review_draft(id,members.integer(6))))fail(SQLITE_CORRUPT,"Draft restoration does not match its original state");
            Statement decisions(impl_->db,"SELECT 1 FROM versions a JOIN versions b USING(batch,ordinal) WHERE a.batch=? AND a.ordinal=? AND a.revision=? AND b.revision=? AND a.decision=b.decision AND a.duplicate_batch=b.duplicate_batch AND a.duplicate_ordinal=b.duplicate_ordinal");
            decisions.text(1,id.batch);decisions.integer(2,integer(id.ordinal));decisions.integer(3,members.integer(3));decisions.integer(4,members.integer(6));
            if(!decisions.row())fail(SQLITE_CORRUPT,"Draft restoration changed its decision");
        }
    }
}

void ImportStore::record_transaction_action(std::string_view operation,std::string_view question,std::string_view digest,const std::vector<std::string>& children) {
    if(!impl_->import_action.empty() || impl_->composing_review)return;
    Statement root(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,?2,hex(?3),?4)");
    root.text(1,operation);root.text(2,question);root.blob(3,digest);root.integer(4,ledger_edit_generation());root.done();
    std::map<std::string,std::pair<std::int64_t,std::int64_t>> links_changed;
    for(const auto& child:children) {
        Statement member(impl_->db,"INSERT INTO review_members SELECT ?1,batch,ordinal,1,prior_revision,result_revision,id FROM ledger_operations WHERE id=?2 AND kind IN(1,2)");member.text(1,operation);member.text(2,child);member.done();
        Statement links(impl_->db,"SELECT v.identity,v.sequence FROM transfer_versions v JOIN ledger_operations l ON l.sequence=v.sequence WHERE l.id=?");links.text(1,child);
        while(links.row()){auto [it,inserted]=links_changed.try_emplace(links.bytes(0),links.integer(1),links.integer(1));if(!inserted){it->second.first=std::min(it->second.first,links.integer(1));it->second.second=std::max(it->second.second,links.integer(1));}}
    }
    for(const auto& [identity,range]:links_changed) {
        Statement links(impl_->db,"INSERT INTO review_link_history SELECT ?1,v.identity,p.payload,v.payload,l.id,coalesce(p.sequence,0) FROM transfer_versions v JOIN ledger_operations l ON l.sequence=v.sequence LEFT JOIN transfer_versions p ON p.identity=v.identity AND p.sequence=(SELECT max(sequence) FROM transfer_versions WHERE identity=v.identity AND sequence<?3) WHERE v.identity=?2 AND v.sequence=?4");links.text(1,operation);links.text(2,identity);links.integer(3,range.first);links.integer(4,range.second);links.done();
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,ledger_edit_generation()+1);stack.done();impl_->pending_review_action=operation;
}
