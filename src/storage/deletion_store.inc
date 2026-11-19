// Deletion retires catalog identities; historical snapshots keep their bindings.
// Only an inverse/replay that needs the retired identity expires the linear stack.
bool ImportStore::deletion_affects_action(const app::ActionSummary& action,std::int64_t account,std::string_view category,bool redo) const {
    if(account) {
        if(history_touches(action,account))return true;
        Statement links(impl_->db,"SELECT prior_payload,result_payload FROM review_link_history WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");links.text(1,action.payload);
        while(links.row())for(int col:{0,1})if(!links.null(col) && link_mentions(transfer_codec::view(links.bytes(col)).link,account))return true;
        return false;
    }
    return action_categories(action,redo).contains(std::string(category));
}
std::set<std::string> ImportStore::action_categories(const app::ActionSummary& action,bool redo) const {
    std::set<std::string> result;
    auto bound=[&](std::string_view batch,std::int64_t ordinal,std::int64_t revision,bool entry) {
        Statement rows(impl_->db,entry?R"SQL(
SELECT m.category_id FROM entry_revisions e JOIN entry_allocation_meta m USING(batch,ordinal,revision)
 WHERE e.batch=?1 AND e.ordinal=?2 AND e.revision=?3 AND e.active=1 AND m.category_id IS NOT NULL
UNION SELECT m.category_id FROM entry_revisions e JOIN draft_allocation_meta m ON m.batch=e.batch AND m.ordinal=e.ordinal AND m.revision=e.draft_revision
 WHERE e.batch=?1 AND e.ordinal=?2 AND e.revision=?3 AND e.active=1 AND m.category_id IS NOT NULL
)SQL":R"SQL(
SELECT m.category_id FROM draft_allocation_meta m JOIN versions v USING(batch,ordinal,revision)
 WHERE m.batch=?1 AND m.ordinal=?2 AND m.revision=?3 AND m.category_id IS NOT NULL AND v.decision!=2
)SQL");
        rows.text(1,batch);rows.integer(2,ordinal);rows.integer(3,revision);
        while(rows.row())result.insert(rows.bytes(0));
    };
    Statement members(impl_->db,"SELECT batch,ordinal,committed,prior,result FROM review_members WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");members.text(1,action.payload);
    while(members.row())bound(members.bytes(0),members.integer(1),members.integer(redo?4:3),members.integer(2)!=0);
    if(action.kind==app::ActionKind::acceptance) {
        Statement rows(impl_->db,"SELECT o.batch,m.ordinal,m.draft_revision FROM acceptance_members m JOIN operations o ON o.id=m.operation WHERE o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1)");rows.text(1,action.payload);
        while(rows.row())bound(rows.bytes(0),rows.integer(1),rows.integer(2),false);
    }
    if(redo) {
        Statement repair(impl_->db,"SELECT batch,ordinal,result FROM review_repairs WHERE answer=?");repair.text(1,action.payload);
        if(repair.row())bound(repair.bytes(0),repair.integer(1),repair.integer(2),false);
    }
    if(redo && action.kind==app::ActionKind::manual_creation) {
        Statement rows(impl_->db,"SELECT ordinal,max(revision) FROM versions WHERE batch=? GROUP BY ordinal");rows.text(1,action.payload);
        while(rows.row())bound(action.payload,rows.integer(0),rows.integer(1),false);
    }
    return result;
}
bool ImportStore::deletion_clears_history(std::int64_t account,std::string_view category) const {
    // Page through the complete stack, not just the screen's latest actions.
    std::int64_t before=0;
    for(;;) {
        const auto actions=action_history(before,200);if(actions.empty())break;
        for(const auto& action:actions)if(action.state==app::ActionState::applied && deletion_affects_action(action,account,category,false))return true;
        before=actions.back().sequence;
    }
    (void)redo_history();
    for(const auto& frame:impl_->redo)
        if(account?frame.accounts.contains(account):frame.categories.contains(std::string(category)))return true;
    return false;
}
void ImportStore::expire_deletion_history() {
    // Exactly the close baseline transition, but leave the file open and dirty.
    struct Cleanup {bool& flag;bool prior;~Cleanup(){flag=prior;}} cleanup{impl_->cleaning,impl_->cleaning};
    impl_->cleaning=true;
    ActionChain(impl_->db).expire(action_token());
    prune_session(nullptr);
    exec(impl_->db,"DELETE FROM review_stack; DELETE FROM session_history; UPDATE document_session SET history_cut=max(history_cut,(SELECT coalesce(max(seq),0) FROM sqlite_sequence WHERE name='history_order')),financial_cut=max(financial_cut,(SELECT coalesce(max(seq),0) FROM sqlite_sequence WHERE name='ledger_operations')) WHERE singleton=1");
}
void ImportStore::require_unused_category(std::string_view id) const {
    // Use the same visible membership as the ledger, including unresolved rows.
    const auto definition=category_at(impl_->db,id);
    struct Restore {std::int64_t& slot;std::int64_t prior;~Restore(){slot=prior;}} restore{impl_->selected_account,impl_->selected_account};
    auto available=accounts();const auto archived=archived_accounts();available.insert(available.end(),archived.begin(),archived.end());
    for(const auto& account:available) {
        impl_->selected_account=account.identity;
        for(const auto& row:current_ledger())for(const auto& allocation:row.draft.transaction.allocations())
            // An unbound QIF transfer destination is outside the ordinary category namespace.
            if(allocation.category_binding?allocation.category_binding->identity==id:
               (!allocation.category.empty() && !qif::transfer_destination(allocation.category) &&
                app::category_name_matches(allocation.category,definition.name)))
                fail(SQLITE_CONSTRAINT,"Transactions use this category. Change their category before deleting it.");
    }
    if(rules_format(impl_->db)) {
        Statement rules(impl_->db,"SELECT 1 FROM rule_heads h JOIN rule_definitions d ON d.id=h.id AND d.version=h.version WHERE h.deleted=0 AND d.category_id=? LIMIT 1");rules.text(1,id);
        if(rules.row())fail(SQLITE_CONSTRAINT,"A rule uses this category. Change or delete the rule before deleting the category.");
    }
    Statement reports(impl_->db,"SELECT 1 FROM report_categories WHERE category=? LIMIT 1");reports.text(1,id);
    if(reports.row())fail(SQLITE_CONSTRAINT,"A saved report uses this category. Remove it from the report before deleting it.");
}
