// A deletion is planned entirely from retained current rows. Counterparts are
// restricted to allocations on links touched by the original selection.
struct ImportStore::DeletionPlan {
    struct Row {imports::Occurrence id;std::int64_t account;imports::DraftRow draft;imports::Decision decision;bool remove;};
    struct Link {app::TransferView before,after;std::int64_t account;int mask;bool first;};
    std::vector<Row> rows;
    std::vector<Link> links;
    app::DeletionPreview preview;
};
ImportStore::DeletionPlan ImportStore::plan_deletion(const app::LedgerSelectionEdit& request,const app::OperationContext& context) const {
    if(request.action!=app::LedgerSelectionAction::remove || request.selected.empty() || !request.payee.empty())fail(SQLITE_CONSTRAINT,"Select transactions to delete");
    if(request.account!=impl_->selected_account || request.generation!=review_generation(impl_->db))fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again.");
    struct Scope {Impl& state;std::int64_t account;~Scope(){state.selected_account=account;}} scope{*impl_,impl_->selected_account};
    const std::set<imports::Occurrence> selected(request.selected.begin(),request.selected.end());
    if(selected.size()!=request.selected.size())fail(SQLITE_CONSTRAINT,"Select each transaction only once");
    DeletionPlan plan;
    std::map<imports::Occurrence,std::set<std::string>> affected;
    std::map<imports::Occurrence,std::int64_t> accounts;
    std::set<imports::Occurrence> generated;
    for(const auto& id:selected){accounts[id]=request.account;if(!id.batch.starts_with("~transfer-"))affected[id];}
    auto impact=[&](std::int64_t id)->app::DeletionImpact& {
        auto it=std::find_if(plan.preview.accounts.begin(),plan.preview.accounts.end(),[&](const auto& a){return a.account==id;});
        if(it==plan.preview.accounts.end()){const auto a=account_by_id(id);plan.preview.accounts.push_back({id,a.name,Money(0,a.currency)});return plan.preview.accounts.back();}return *it;
    };
    for(const auto& value:all_transfers()){
        context.checkpoint("Checking linked transactions");
        const auto& link=value.link;
        const bool origin=selected.contains(link.origin.endpoint.entry),destination=selected.contains(link.destination.endpoint.entry);
        if(!origin && !destination)continue;
        if((!origin || link.origin.state==transfers::State::removed) && (!destination || link.destination.state==transfers::State::removed))continue;
        if(!link.unresolved && !link.destination_released)++plan.preview.links;
        if((origin && link.origin.endpoint.account!=request.account) || (destination && link.destination.endpoint.account!=request.account))fail(SQLITE_CONSTRAINT,"Select transactions in this account");
        const bool remove_origin=origin || request.delete_counterparts;
        const bool remove_destination=destination || request.delete_counterparts || (origin && local_side(link.destination) && link.destination.state==transfers::State::draft);
        auto middle=value;middle.link=transfers::unlink_for_deletion(link,origin,destination);
        auto after=middle;after.link=transfers::unlink_for_deletion(middle.link,remove_origin,remove_destination);
        const bool second=transfer_codec::encode(middle)!=transfer_codec::encode(after);
        plan.links.push_back({value,middle,request.account,(origin?1:0)|(destination?2:0),second});
        if(second)plan.links.push_back({middle,after,origin?link.destination.endpoint.account:link.origin.endpoint.account,(remove_origin?1:0)|(remove_destination?2:0),false});
        for(const auto* side:{&link.origin,&link.destination}){
            if(side->state==transfers::State::removed)continue;
            require_writable_account(impl_->db,side->endpoint.account);
            const auto& id=side->endpoint.entry;
            if(local_side(*side)){
                generated.insert(id);
                const auto& next=side==&link.origin?after.link.origin:after.link.destination;
                auto& out=impact(side->endpoint.account);
                if(next.state==transfers::State::removed){out.change=out.change-side->amount;++out.removed;}else ++out.changed;
            }else {accounts[id]=side->endpoint.account;affected[id].insert(side->endpoint.allocation);}
        }
    }
    for(const auto& id:selected)if(id.batch.starts_with("~transfer-") && !generated.contains(id))fail(SQLITE_CONSTRAINT,"This generated transaction is no longer linked. Change it individually.");
    for(const auto& [id,allocations]:affected){
        context.checkpoint("Checking transactions to delete");
        impl_->selected_account=accounts.at(id);require_writable_account(impl_->db,impl_->selected_account);require_account(id.batch);
        if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Select the retained transaction instead of a merged transaction.");
        const auto row=load_row(id,false);const auto info=batch(impl_->db,id.batch);
        if(!row.draft || row.excluded_by_clear || row.matched_generated || (row.decision.disposition!=imports::Disposition::include && row.decision.disposition!=imports::Disposition::unresolved))fail(SQLITE_CONSTRAINT,"Resolve the selected transactions in Review before deleting them.");
        const bool accepted=transaction_accepted(id);
        const auto before=accepted?committed_entry(id).current.draft:*row.draft;
        auto draft=before;auto splits=before.transaction.allocations();
        bool remove=selected.contains(id);
        if(!remove){
            for(const auto& allocation:allocations)if(std::none_of(splits.begin(),splits.end(),[&](const auto& a){return a.identity==allocation;}))fail(SQLITE_CONSTRAINT,"A linked split changed. Select the transactions again.");
            if(request.delete_counterparts){
                auto total=before.transaction.effective_total();
                std::erase_if(splits,[&](const auto& a){if(!allocations.contains(a.identity))return false;total=total-a.amount;return true;});
                remove=splits.empty();
                if(!remove){draft.transaction=before.transaction.corrected(total,std::move(splits));if(allocations.contains(draft.remainder_allocation))draft.remainder_allocation.clear();}
            }else {
                for(auto& a:splits)if(allocations.contains(a.identity)){a.category.clear();a.category_binding.reset();a.category_owner=FieldOwnership::manual;a.category_rule.reset();}
                draft.transaction=before.transaction.corrected(before.transaction.effective_total(),std::move(splits));
            }
        }
        if(!remove){
            const auto prior=accepted?committed_entry(id).current.revision:info.revision;
            if(prior==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
            const auto revision=prior+1;
            draft=canonical_draft(id,draft,before,revision,accepted);
            imports::validate_draft(row.source,draft,row.source.source_currency && *row.source.source_currency!=draft.transaction.effective_total().currency());
        }
        auto& out=impact(impl_->selected_account);
        out.change=out.change+(remove?Money(0,before.transaction.effective_total().currency()):draft.transaction.effective_total())-before.transaction.effective_total();
        if(remove)++out.removed;else ++out.changed;
        plan.rows.push_back({id,impl_->selected_account,std::move(draft),row.decision,remove});
    }
    return plan;
}
app::DeletionPreview ImportStore::preview_deletion(const app::LedgerSelectionEdit& request,const app::OperationContext& context){ScopedStatements statements(impl_->db,&context);return plan_deletion(request,context).preview;}
app::ReviewReceipt ImportStore::delete_transactions(std::string_view operation,const app::LedgerSelectionEdit& request,const app::OperationContext& context){
    ScopedStatements statements(impl_->db,&context);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    transfer_codec::Writer encoded;encoded.number(static_cast<int>(request.action));encoded.number(request.account);encoded.number(request.generation);encoded.text(request.payee);
    for(const auto& id:request.selected)encoded.text(review_key(id));
    if(request.delete_counterparts)encoded.text("delete-counterparts");
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='ledger-delete'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded.bytes)fail(SQLITE_CONSTRAINT,"Retry changed the deletion choice or selection");return *prior;
    }
    const auto plan=plan_deletion(request,context);
    struct Scope {Impl& state;std::int64_t account;bool composing;~Scope(){state.selected_account=account;state.composing_review=composing;}} scope{*impl_,impl_->selected_account,impl_->composing_review};
    Atomic transaction(impl_->db);
    if(request.generation!=review_generation(impl_->db) || request.generation==INT64_MAX)fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again.");
    auto receipt=[&](std::string_view id,std::string_view question){Statement row(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,?,?,?)");row.text(1,id);row.text(2,question);row.text(3,encoded.bytes);row.integer(4,request.generation);row.done();};
    receipt(operation,"ledger-delete");
    const auto kept=std::string(operation)+"-kept";
    if(std::any_of(plan.rows.begin(),plan.rows.end(),[](const auto& r){return !r.remove;}))receipt(kept,"ledger-delete-kept:"+std::string(operation));
    const auto unlink=std::string(operation)+"-unlink";
    if(std::any_of(plan.links.begin(),plan.links.end(),[](const auto& l){return l.first;}))receipt(unlink,"ledger-delete-links:"+std::string(operation));
    std::size_t serial=0;
    for(const auto& link:plan.links){
        context.checkpoint("Unlinking selected transactions");
        impl_->selected_account=link.account;impl_->composing_review=true;
        app::TransferRequest action{};action.action=app::TransferAction::delete_selection;action.link=link.before.link.identity;action.expected_sequence=transfer_sequence();action.destination=link.mask;
        const auto child=std::string(operation)+"-link-"+std::to_string(++serial);apply_transfer(child,action);impl_->composing_review=false;
        Statement h(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");h.text(1,link.first?unlink:std::string(operation));h.text(2,action.link);h.blob(3,transfer_codec::encode(link.before));h.blob(4,transfer_codec::encode(link.after));h.text(5,child);h.done();
    }
    std::map<std::int64_t,Money> balances;
    for(const auto& row:plan.rows){
        context.checkpoint("Deleting selected transactions");impl_->selected_account=row.account;
        auto balance=balances.find(row.account);if(balance==balances.end())balance=balances.emplace(row.account,committed_balance()).first;
        const auto child=std::string(operation)+"-edit-"+std::to_string(++serial);
        write_review_row(row.remove?std::string(operation):kept,child,row.id,row.draft,row.decision,!row.remove,false,false,{},{},&balance->second);
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();
    context.checkpoint("Finishing deletion");finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}

// Selection commands derive their changes from retained rows, never UI snapshots.
std::int64_t ImportStore::ledger_edit_generation() const {return review_generation(impl_->db);}
app::ReviewReceipt ImportStore::edit_transactions(std::string_view operation,const app::LedgerSelectionEdit& request,const app::OperationContext& context) {
    if(request.action==app::LedgerSelectionAction::remove)return delete_transactions(operation,request,context);
    ScopedStatements statements(impl_->db,&context);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(request.selected.empty())fail(SQLITE_CONSTRAINT,"Select at least one transaction");
    if(request.action!=app::LedgerSelectionAction::rename_payee && request.action!=app::LedgerSelectionAction::restore_imported && request.action!=app::LedgerSelectionAction::remove)fail(SQLITE_CONSTRAINT,"Choose Rename payee, Restore imported values, or Delete");
    if(request.action!=app::LedgerSelectionAction::rename_payee && !request.payee.empty())fail(SQLITE_CONSTRAINT,"This action does not accept a replacement payee");
    if(request.payee.size()>65536)fail(SQLITE_CONSTRAINT,"The payee name is too long");
    const bool removing=request.action==app::LedgerSelectionAction::remove;
    const std::string question=removing?"ledger-delete":"ledger-edit-batch";
    transfer_codec::Writer encoded;encoded.number(static_cast<int>(request.action));encoded.number(request.account);encoded.number(request.generation);encoded.text(request.payee);
    std::set<imports::Occurrence> identities;
    for(const auto& id:request.selected){context.checkpoint("Checking selected transactions");if(!identities.insert(id).second)fail(SQLITE_CONSTRAINT,"Select each transaction only once");encoded.text(review_key(id));}
    const auto request_hash=session_snapshot::digest(encoded.bytes);
    session_snapshot::retry(impl_->db,operation,"selection",request_hash);
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question=?");saved.text(1,operation);saved.text(2,question);
        if(!saved.row() || saved.bytes(0)!=encoded.bytes)fail(SQLITE_CONSTRAINT,"Retry changed the selected transactions or values");
        return *prior;
    }
    const auto current=[&]{return request.account==impl_->selected_account && request.generation==review_generation(impl_->db);};
    if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    require_writable_account(impl_->db,request.account);
    if(request.generation==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
    struct Plan {imports::Occurrence id;imports::DraftRow draft;imports::Decision decision;std::optional<EntryLinkPlan> links;};
    std::vector<Plan> plans;const auto before_links=all_transfers();std::set<std::string> changed_links;
    const auto catalog=category_catalog();
    auto balance=committed_balance();
    for(const auto& id:request.selected){
        context.checkpoint("Checking selected transactions");
        if(id.batch.starts_with("~transfer-"))fail(SQLITE_CONSTRAINT,"Select imported transactions. Generated transfer entries cannot use this action.");
        require_account(id.batch);
        if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Select the retained transaction instead of a merged transaction.");
        const auto info=batch(impl_->db,id.batch);const auto row=load_row(id,false);
        if(!row.draft || row.excluded_by_clear || row.matched_generated || (row.decision.disposition!=imports::Disposition::include && row.decision.disposition!=imports::Disposition::unresolved))fail(SQLITE_CONSTRAINT,"Resolve the selected transactions in Review before editing them.");
        const bool committed=transaction_accepted(id);
        const auto before=committed?committed_entry(id).current.draft:*row.draft;
        auto draft=before;
        if(request.action==app::LedgerSelectionAction::rename_payee){draft.payee=request.payee;draft.payee_owner=FieldOwnership::manual;draft.payee_rule.reset();}
        else if(!removing) {
            const auto& source=row.source;
            if(!source.transaction || !source.original_amount || source.date.empty())fail(SQLITE_CONSTRAINT,"An original date or amount is unavailable. Restore that transaction in its editor and complete the missing fields.");
            if(source.original_amount->currency()!=before.transaction.effective_total().currency())fail(SQLITE_CONSTRAINT,"The original currency differs. Restore this transaction individually.");
            auto allocations=source.source_allocations;
            if(allocations.empty())fail(SQLITE_CONSTRAINT,"Original splits are unavailable. Restore this transaction individually.");
            for(auto& a:allocations){
                const auto old=std::find_if(before.transaction.allocations().begin(),before.transaction.allocations().end(),[&](const auto& v){return v.source_ordinal==a.source_ordinal && a.source_ordinal.has_value();});
                if(old==before.transaction.allocations().end())fail(SQLITE_CONSTRAINT,"An original split was removed. Restore this transaction individually.");
                a.identity=old->identity;a.category_owner=FieldOwnership::manual;a.category_rule.reset();a.category_binding.reset();a.memo_owner=FieldOwnership::source;a.memo_rule.reset();
                if(!a.category.empty() && !qif::transfer_destination(a.category) && !resolve_account_name(a.category)){
                    const auto found=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && !c.archived && app::category_name_matches(c.name,a.category);});
                    if(found==catalog.categories.end())fail(SQLITE_CONSTRAINT,"An imported category is unavailable. Restore this transaction individually and choose a category.");
                    a.category=found->name;a.category_binding=CategoryBinding{found->identity,catalog.revision};
                }
            }
            draft={source.date,source.payee,source.memo,Transaction::repaired_import(source.original_amount,*source.original_amount,std::move(allocations)),FieldOwnership::source,FieldOwnership::source};
            draft.manual_split_amounts=before.manual_split_amounts;draft.remainder_allocation=before.remainder_allocation;
        }
        std::optional<EntryLinkPlan> links;
        if(committed){
            const auto preview=preview_change({removing?app::ChangeKind::remove:app::ChangeKind::correct,id,removing?std::optional<imports::DraftRow>{}:std::optional<imports::DraftRow>{draft},{}},true,balance);draft=preview.after_entry.draft;
            links=plan_entry_links(before_links,id,before,draft,removing);
            for(const auto& link:links->changed)if(!changed_links.insert(link.link.identity).second)fail(SQLITE_CONSTRAINT,"These transactions belong to the same transfer. Change one transaction at a time.");
        }else{
            if(removing && transfer_linked(id))fail(SQLITE_CONSTRAINT,"Unlink this transfer before deleting it.");
            require_unlinked(id);
            if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"This transaction cannot be edited.");
            if(info.revision==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
            draft=canonical_draft(id,draft,before,info.revision+1,false);
            imports::validate_draft(row.source,draft,row.source.source_currency && *row.source.source_currency!=draft.transaction.effective_total().currency());
        }
        plans.push_back({id,std::move(draft),row.decision,std::move(links)});
    }
    Atomic transaction(impl_->db);if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,?,?,?)");receipt.text(1,operation);receipt.text(2,question);receipt.text(3,encoded.bytes);receipt.integer(4,request.generation);receipt.done();
    std::size_t serial=0;
    for(const auto& plan:plans){
        context.checkpoint("Updating selected transactions");const auto child=std::string(operation)+"-edit-"+std::to_string(++serial);
        write_review_row(operation,child,plan.id,plan.draft,plan.decision,!removing,false,false,{},{},&balance);
        if(plan.links && !plan.links->changed.empty()){
            write_entry_links(child,request.account,app::TransferAction::relink,financial_sequence(impl_->db),plan.links->changed);
            for(const auto& value:plan.links->changed){
                const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
                Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
                if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));
                history.blob(4,transfer_codec::encode(value));history.text(5,child);history.done();
            }
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();
    session_snapshot::completed(impl_->db,operation,"selection",request_hash);
    context.checkpoint("Finishing selected transaction changes");finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}
