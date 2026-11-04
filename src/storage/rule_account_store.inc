#ifndef SPENTIT_RULES_STORAGE
app::ReviewReceipt ImportStore::apply_account_rules(std::string_view,const app::AccountRuleRequest&,const app::OperationContext&) {fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset");}
bool ImportStore::can_undo_account_rules() const {return false;}
app::ReviewReceipt ImportStore::undo_account_rules(std::string_view) {fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset");}
#else
namespace {
std::string account_rule_request(const app::AccountRuleRequest& r) {
    return std::string(r.replace_manual?"1|":"0|")+std::to_string(r.account)+"|"+std::to_string(r.generation)+"|"+std::to_string(r.ruleset_revision)+"|"+std::to_string(r.catalog_revision)+"|"+(r.new_only?"new|":"all|")+r.rule;
}
}
app::ReviewReceipt ImportStore::apply_account_rules(std::string_view operation,const app::AccountRuleRequest& request,const app::OperationContext& context) {
    if(!rules_supported())fail(SQLITE_SCHEMA,rules_dataset);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    const auto encoded=account_rule_request(request);
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='rules-account'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,reused_command);
        if(account().identity!=request.account)fail(SQLITE_CONSTRAINT,refresh_rules);
        return *prior;
    }
    auto current=[&]{return account().identity==request.account && review_generation(impl_->db)==request.generation && ruleset_revision(impl_->db)==request.ruleset_revision && catalog_sequence(impl_->db)==request.catalog_revision;};
    if(!current())fail(SQLITE_CONSTRAINT,refresh_rules);
    require_writable_account(impl_->db,request.account);
    if(request.generation==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
    app::RulePreviewRequest query;query.account_scope=true;query.new_only=request.new_only;query.replace_manual=request.replace_manual;query.saved_rule=request.rule;query.changed_only=true;
    const auto preview=preview_pending_rules(query,context);
    if(!current())fail(SQLITE_CONSTRAINT,refresh_rules);
    if(preview.records.empty())return {};
    // A single snapshot journal covers new and committed rows across all imports.
    // No child commits independently. The generation rejects changed previews.
    const auto unchanged_balance=committed_balance();
    ScopedStatements plans(impl_->db);
    Atomic transaction(impl_->db);
    if(!current())fail(SQLITE_CONSTRAINT,refresh_rules);
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'rules-account',?,?)");
    receipt.text(1,operation);receipt.text(2,encoded);receipt.integer(3,request.generation);receipt.done();
    std::size_t serial=0;
    for(const auto& record:preview.records){
        context.checkpoint("Applying matching rules");
        const auto row=load_row(record.identity,false);
        const auto before=transaction_accepted(record.identity)?committed_entry(record.identity).current.draft:*row.draft;
        if(!same_draft(before,record.before))fail(SQLITE_CONSTRAINT,refresh_rules);
        reapply_keep(impl_->db,before,record.proposal.after,request.catalog_revision);
        check_rule_transition(before,record.proposal.after,true,request.replace_manual);
        write_review_row(operation,std::string(operation)+"-rule-"+std::to_string(++serial),record.identity,record.proposal.after,row.decision,true,false,false,request.ruleset_revision,unchanged_balance);
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();
    finish([&]{context.seal();transaction.commit();});
    return {std::string(operation)};
}
bool ImportStore::can_undo_account_rules() const {
    if(!rules_supported())return false;
    Statement top(impl_->db,"SELECT s.generation,a.question,substr(a.request,3,instr(substr(a.request,3),'|')-1) FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    return top.row() && top.bytes(1)=="rules-account" && top.integer(0)==review_generation(impl_->db) && top.bytes(2)==std::to_string(account().identity);
}
app::ReviewReceipt ImportStore::undo_account_rules(std::string_view operation) {
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT 1 FROM review_undos u JOIN review_answers a ON a.id=u.answer WHERE u.id=? AND a.question='rules-account'");saved.text(1,operation);
        if(!saved.row())fail(SQLITE_CONSTRAINT,reused_command);
        return *prior;
    }
    if(!can_undo_account_rules())fail(SQLITE_CONSTRAINT,"Later changes prevent Undo. Check Change history.");
    return undo_review(operation);
}
#endif
