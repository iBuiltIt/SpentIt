// The Change history dialog reads actions that are already on the chain.
// Yes restores those saved before-values, newest first, in one transaction.
namespace {
std::optional<app::ActionSummary> connected_header(sqlite3* db,std::string_view id) {
    if(id.empty())return {};
    Statement row(db,"SELECT id,parent,sequence,kind,state,payload,label,count FROM action_headers WHERE id=?");row.text(1,id);
    if(!row.row())return {};
    const auto kind=row.integer(3),state=row.integer(4);
    if(kind<0 || kind>7 || state<0 || state>3)fail(SQLITE_CORRUPT,"Undo head is inconsistent");
    return app::ActionSummary{row.bytes(0),row.bytes(1),row.integer(2),static_cast<app::ActionKind>(kind),static_cast<app::ActionState>(state),row.bytes(5),row.bytes(6),row.integer(7)};
}
}
std::vector<app::ActionSummary> ImportStore::connected_actions(std::string_view id) const {
    std::vector<app::ActionSummary> applied;
    auto current=ActionChain(impl_->db).head();
    std::set<std::string> seen;
    for(int walked=0;current && walked<100000;++walked) {
        if(!seen.insert(current->id).second)fail(SQLITE_CORRUPT,"Undo history chain is inconsistent");
        applied.push_back(*current);
        if(current->parent.empty())break;
        auto parent=connected_header(impl_->db,current->parent);
        if(!parent || parent->state!=app::ActionState::applied)break;
        current=std::move(parent);
    }
    const auto target=std::find_if(applied.begin(),applied.end(),[&](const auto& action){return action.id==id;});
    if(target==applied.end())fail(SQLITE_CONSTRAINT,"This change is no longer available.");
    std::map<std::string,std::set<imports::Occurrence>> occurrences;
    auto occ_of=[&](const app::ActionSummary& action){
        auto found=occurrences.find(action.id);
        if(found==occurrences.end())found=occurrences.emplace(action.id,history_occurrences(action)).first;
        return found->second;
    };
    std::set<std::string> chosen{target->id};
    auto touched=occ_of(*target);
    bool grew=true;
    while(grew) {
        grew=false;
        for(const auto& action:applied) {
            if(action.sequence<target->sequence || chosen.contains(action.id))continue;
            const auto mine=occ_of(action);
            if(!std::any_of(mine.begin(),mine.end(),[&](const auto& item){return touched.contains(item);}))continue;
            chosen.insert(action.id);touched.insert(mine.begin(),mine.end());grew=true;
        }
    }
    std::vector<app::ActionSummary> result;
    for(const auto& action:applied)if(chosen.contains(action.id))result.push_back(action);
    return result;
}
app::ConnectedUndoLine ImportStore::connected_line(const app::ActionSummary& action) const {
    app::ConnectedUndoLine line;line.id=action.id;line.payload=action.payload;line.kind=action.kind;line.label=action_display_label(action);
    const auto view=history_view(action,0,false,false);line.count=view.count;
    if(!view.changes.empty())line.changes=view.changes;
    else for(const auto& member:view.members)for(const auto& change:member.changes){line.changes.push_back(change);if(line.changes.size()>=8)return line;}
    if(line.changes.size()>8)line.changes.resize(8);
    return line;
}
app::ConnectedUndoPreview ImportStore::preview_connected_undo(std::string_view id,const app::ActionToken& expected) const {
    ActionChain chain(impl_->db);chain.require_current(expected);chain.require_idle();
    app::ConnectedUndoPreview result;result.token=chain.token();result.target=id;
    const auto actions=connected_actions(id);
    for(const auto& action:actions) {
        auto line=connected_line(action);result.lines.push_back(line);
        if(action.id!=id)continue;
        const auto view=history_view(action,0,false,false);
        result.label=line.label;result.count=line.count;result.kind=line.kind;result.accounts=view.accounts;result.changes=line.changes;
    }
    if(result.lines.empty() || result.lines.back().id!=id)fail(SQLITE_CONSTRAINT,"This change is no longer available.");
    return result;
}
std::optional<app::ActionReceipt> ImportStore::undo_connected_outcome(std::string_view operation,const app::ConnectedUndoPreview& preview) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    transfer_codec::Writer request;request.text("undo-connected");request.text(preview.target);
    request.number(preview.token.session);request.number(preview.token.epoch);request.text(preview.token.head);
    const auto done=ActionChain(impl_->db).durable_outcome(operation,session_snapshot::digest(request.bytes));
    if(!done)return {};
    if(*done==ActionChain::Outcome::expired)throw app::OperationAlreadyCompleted(std::string(operation));
    return app::ActionReceipt{std::string(operation),preview.target};
}
app::ActionReceipt ImportStore::undo_connected(std::string_view operation,const app::ConnectedUndoPreview& preview) {
    identifier(operation);
    if(operation.size()>80)fail(SQLITE_MISUSE,"Undo operation identifier is too long");
    transfer_codec::Writer request;request.text("undo-connected");request.text(preview.target);
    request.number(preview.token.session);request.number(preview.token.epoch);request.text(preview.token.head);
    const auto digest=session_snapshot::digest(request.bytes);
    if(const auto done=ActionChain(impl_->db).durable_outcome(operation,digest)) {
        if(*done==ActionChain::Outcome::expired)throw app::OperationAlreadyCompleted(std::string(operation));
        return {std::string(operation),preview.target};
    }
    const auto checked=preview_connected_undo(preview.target,preview.token);
    if(!checked.refusal.empty())fail(SQLITE_CONSTRAINT,checked.refusal.c_str());
    if(checked.lines.empty() || checked.token!=preview.token)fail(SQLITE_CONSTRAINT,"Changes were saved. Refresh the action and try again.");
    if(impl_->undo_token)fail(SQLITE_MISUSE,"Undo cannot be nested");
    struct Guard {
        Impl& state;
        ~Guard(){state.undo_token.reset();state.undo_operation.clear();state.undo_digest.clear();state.undo_target.clear();state.undo_joined=false;state.undo_depth=0;}
    } guard{*impl_};
    Atomic transaction(impl_->db,&impl_->undo_depth);
    impl_->undo_token=preview.token;impl_->undo_digest=digest;impl_->undo_joined=true;
    for(std::size_t i=0;i<checked.lines.size();++i) {
        const auto& line=checked.lines[i];
        const auto header=connected_header(impl_->db,line.id);
        if(!header || header->state!=app::ActionState::applied || header->kind!=line.kind || header->payload!=line.payload)fail(SQLITE_CONSTRAINT,"Changes were saved. Refresh the action and try again.");
        // Restoring an accepted change records each transaction as {operation}-N
        // and registers that id. A later action in this same Undo cannot reuse it.
        impl_->undo_operation=i==0?std::string(operation):std::string(operation)+"-u"+std::to_string(i);
        impl_->undo_target=line.id;
        const auto selected=impl_->selected_account;
        struct Selection {std::int64_t& value;std::int64_t old;~Selection(){value=old;}} selection{impl_->selected_account,selected};
        switch(line.kind) {
        case app::ActionKind::transaction:
        case app::ActionKind::review:
        case app::ActionKind::rule_application: undo_review(impl_->undo_operation);break;
        case app::ActionKind::import: undo_import(impl_->undo_operation,line.payload,ledger_edit_generation());break;
        case app::ActionKind::acceptance: undo_acceptance(impl_->undo_operation,line.payload);break;
        case app::ActionKind::manual_creation: {
            Statement owner(impl_->db,"SELECT account_id FROM batches WHERE id=?");owner.text(1,line.payload);if(!owner.row())fail(SQLITE_CONSTRAINT,"This action could not be undone.");
            impl_->selected_account=owner.integer(0);undo_manual(line.payload,ledger_edit_generation());break;
        }
        case app::ActionKind::opening:
        case app::ActionKind::currency: {
            Statement owner(impl_->db,"SELECT account FROM account_operations WHERE id=?");owner.text(1,line.payload);if(!owner.row())fail(SQLITE_CONSTRAINT,"This action could not be undone.");
            impl_->selected_account=owner.integer(0);undo_account_operation(impl_->undo_operation,line.payload);break;
        }
        default:fail(SQLITE_CONSTRAINT,"This action cannot yet be undone.");
        }
        Statement undone(impl_->db,"SELECT state FROM action_headers WHERE id=?");undone.text(1,line.id);
        if(!undone.row() || undone.integer(0)!=static_cast<int>(app::ActionState::undone))fail(SQLITE_CONSTRAINT,"This action could not be undone.");
    }
    verify();
    if(impl_->observer)impl_->observer(ImportPhase::before_commit);
    transaction.commit();
    if(impl_->observer)impl_->observer(ImportPhase::after_commit);
    return {std::string(operation),preview.target};
}
