// Selection commands derive their changes from retained rows, never UI snapshots.
std::int64_t ImportStore::ledger_edit_generation() const {return review_generation(impl_->db);}
app::ReviewReceipt ImportStore::edit_transactions(std::string_view operation,const app::LedgerSelectionEdit& request,const app::OperationContext& context) {
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(request.selected.empty() || request.selected.size()>4096)fail(SQLITE_CONSTRAINT,"Select between 1 and 4096 transactions");
    if(request.action!=app::LedgerSelectionAction::rename_payee && request.action!=app::LedgerSelectionAction::restore_imported)fail(SQLITE_CONSTRAINT,"Choose Rename payee or Restore imported values");
    if(request.action==app::LedgerSelectionAction::restore_imported && !request.payee.empty())fail(SQLITE_CONSTRAINT,"Restore does not accept a replacement payee");
    if(request.payee.size()>65536)fail(SQLITE_CONSTRAINT,"The payee name is too long");
    transfer_codec::Writer encoded;encoded.number(static_cast<int>(request.action));encoded.number(request.account);encoded.number(request.generation);encoded.text(request.payee);
    std::set<imports::Occurrence> identities;
    for(const auto& id:request.selected){if(!identities.insert(id).second)fail(SQLITE_CONSTRAINT,"Select each transaction only once");encoded.text(review_key(id));}
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='ledger-edit-batch'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded.bytes)fail(SQLITE_CONSTRAINT,"Retry changed the selected transactions or values");return *prior;
    }
    const auto current=[&]{return request.account==impl_->selected_account && request.generation==review_generation(impl_->db);};
    if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    require_writable_account(impl_->db,request.account);
    if(request.generation==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
    struct Plan {imports::Occurrence id;imports::DraftRow draft;imports::Decision decision;std::optional<EntryLinkPlan> links;};
    std::vector<Plan> plans;const auto before_links=all_transfers();std::set<std::string> changed_links;
    const auto catalog=category_catalog();
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
        else {
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
            const auto preview=preview_committed_change({app::ChangeKind::correct,id,draft,{}});draft=preview.after_entry.draft;
            links=plan_entry_links(before_links,id,before,draft,false);
            for(const auto& link:links->changed)if(!changed_links.insert(link.link.identity).second)fail(SQLITE_CONSTRAINT,"Selected transactions change the same transfer. Restore them individually.");
        }else{
            require_unlinked(id);
            if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"This transaction cannot be edited.");
            if(info.revision==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
            draft=canonical_draft(id,draft,before,info.revision+1,false);
            imports::validate_draft(row.source,draft,row.source.source_currency && *row.source.source_currency!=draft.transaction.effective_total().currency());
        }
        plans.push_back({id,std::move(draft),row.decision,std::move(links)});
    }
    Atomic transaction(impl_->db);if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'ledger-edit-batch',?,?)");receipt.text(1,operation);receipt.text(2,encoded.bytes);receipt.integer(3,request.generation);receipt.done();
    std::size_t serial=0;
    for(const auto& plan:plans){
        context.checkpoint("Updating selected transactions");const auto child=std::string(operation)+"-edit-"+std::to_string(++serial);
        write_review_row(operation,child,plan.id,plan.draft,plan.decision,true,false,false);
        if(plan.links && !plan.links->changed.empty()){
            write_entry_links(child,request.account,app::TransferAction::relink,financial_sequence(impl_->db),plan.links->changed);
            for(const auto& value:plan.links->changed){
                const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
                Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
                if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(value));history.text(5,child);history.done();
            }
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();
    context.checkpoint("Finishing selected transaction changes");finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}
