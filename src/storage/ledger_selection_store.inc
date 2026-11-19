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
    if(request.keep_counterpart && request.delete_counterparts)fail(SQLITE_CONSTRAINT,"Choose whether to keep or delete the other entry.");
    DeletionPlan plan;
    const bool single=selected.size()==1;
    const auto relationships=all_transfers();
    const auto live=[](const transfers::Link& link) {
        return !link.unresolved && !link.destination_released &&
               link.origin.state!=transfers::State::removed && link.destination.state!=transfers::State::removed;
    };
    std::set<imports::Occurrence> linked;
    const app::TransferView* pair=nullptr;
    std::map<imports::Occurrence,std::vector<const app::TransferView*>> detached;
    for(const auto& value:relationships){
        context.checkpoint("Checking linked transactions");
        for(const auto* side:{&value.link.origin,&value.link.destination}){
            if(side->state==transfers::State::removed || !selected.contains(side->endpoint.entry))continue;
            if(side->endpoint.account!=request.account)fail(SQLITE_CONSTRAINT,"Select transactions in this account");
            if(live(value.link)){
                linked.insert(side->endpoint.entry);
                if(single && pair)fail(SQLITE_CONSTRAINT,"This transaction has more than one transfer link. Change the linked splits individually before deleting it.");
                pair=&value;
            }else if(value.link.unresolved || value.link.destination_released)
                detached[side->endpoint.entry].push_back(&value);
        }
    }
    auto impact=[&](std::int64_t id)->app::DeletionImpact& {
        auto it=std::find_if(plan.preview.accounts.begin(),plan.preview.accounts.end(),[&](const auto& a){return a.account==id;});
        if(it==plan.preview.accounts.end()){const auto a=account_by_id(id);plan.preview.accounts.push_back({id,a.name,Money(0,a.currency)});return plan.preview.accounts.back();}return *it;
    };
    auto load=[&](const imports::Occurrence& id,std::int64_t owner) {
        impl_->selected_account=owner;
        require_writable_account(impl_->db,owner);
        require_account(id.batch);
        if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Select the retained transaction instead of a merged transaction.");
        auto row=load_row(id,false);
        if(!row.draft || row.excluded_by_clear || row.matched_generated ||
           (row.decision.disposition!=imports::Disposition::include && row.decision.disposition!=imports::Disposition::unresolved))
            fail(SQLITE_CONSTRAINT,"Resolve the selected transactions in Review before deleting them.");
        if(transaction_accepted(id)){
            auto entry=committed_entry(id);
            if(!entry.current.active)fail(SQLITE_CONSTRAINT,"This transaction is already deleted.");
            row.draft=std::move(entry.current.draft);
        }
        return row;
    };
    auto remove=[&](const imports::Occurrence& id,std::int64_t owner) {
        auto row=load(id,owner);
        auto& out=impact(owner);
        out.change=out.change-row.draft->transaction.effective_total();
        ++out.removed;
        plan.rows.push_back({id,owner,std::move(*row.draft),row.decision,true});
    };
    auto check_source_match=[&](const app::TransferView& value) {
        Statement match(impl_->db,"SELECT 1 FROM generated_source_matches WHERE link=? AND active=1 LIMIT 1");
        match.text(1,value.link.identity);
        if(match.row())fail(SQLITE_CONSTRAINT,"This transfer has a bank match. Resolve the match in Review before deleting it.");
    };
    if(single && pair){
        plan.preview.links=1;
        const auto& link=pair->link;
        check_source_match(*pair);
        const bool from_origin=link.origin.endpoint.entry==*selected.begin();
        const auto& chosen=from_origin?link.origin:link.destination;
        const auto& other=from_origin?link.destination:link.origin;
        const auto other_account=other.endpoint.account;
        require_writable_account(impl_->db,request.account);
        require_writable_account(impl_->db,other_account);
        if(std::any_of(relationships.begin(),relationships.end(),[&](const auto& value){
            return &value!=pair && live(value.link) &&
                   (value.link.origin.endpoint.entry==other.endpoint.entry || value.link.destination.endpoint.entry==other.endpoint.entry);
        }))fail(SQLITE_CONSTRAINT,"The other entry has more than one transfer link. Change the linked splits individually before deleting it.");

        // Validate pending/attached evidence before looking up the other posting.
        const int selected_mask=from_origin?delete_transfer_origin:delete_transfer_destination;
        auto middle=unlink_transfer_for_deletion(*pair,selected_mask);
        auto detail=app::DeletionCounterpart{account_by_id(other_account).name,pair->payee,other.date,{},other.amount,false};
        bool retain_label=false;
        if(!local_side(other)){
            auto row=load(other.endpoint.entry,other_account);
            auto& draft=*row.draft;
            auto allocations=draft.transaction.allocations();
            if(allocations.size()!=1 || allocations.front().identity!=other.endpoint.allocation)
                fail(SQLITE_CONSTRAINT,"The other entry has splits. Change its transfer split before deleting this transaction.");
            auto& allocation=allocations.front();
            detail.payee=draft.payee;
            detail.date=draft.date;
            detail.amount=draft.transaction.effective_total();
            detail.imported=row.source.format!="manual";
            if(allocation.source_ordinal){
                const auto& original=row.source.source_allocation_fields.at(*allocation.source_ordinal).category;
                if(transfer_name(original))detail.category=original;
            }
            retain_label=!detail.category.empty();
            if(!request.delete_counterparts){
                if(allocation.category!=detail.category || allocation.category_binding || allocation.category_rule){
                    allocation.category=detail.category;
                    allocation.category_binding.reset();
                    allocation.category_rule.reset();
                    allocation.category_owner=FieldOwnership::manual;
                    draft.transaction=draft.transaction.corrected(detail.amount,std::move(allocations));
                    imports::validate_draft(row.source,draft,row.source.source_currency && *row.source.source_currency!=detail.amount.currency());
                    plan.rows.push_back({row.identity,other_account,std::move(draft),row.decision,false});
                }
                ++impact(other_account).changed;
            }
        }
        plan.preview.counterpart=std::move(detail);
        const int first_mask=!request.delete_counterparts && (retain_label || local_side(other))
            ?selected_mask|keep_transfer_posting:selected_mask;
        if(first_mask!=selected_mask)middle=unlink_transfer_for_deletion(*pair,first_mask);
        // Separate events record the balance effect in each account. Both
        // events and the corresponding rows commit inside one outer action.
        constexpr int both_mask=delete_transfer_origin|delete_transfer_destination;
        const auto after=request.delete_counterparts?unlink_transfer_for_deletion(middle,both_mask):middle;
        const bool second=transfer_codec::encode(middle)!=transfer_codec::encode(after);
        plan.links.push_back({*pair,middle,request.account,first_mask,second});
        if(second)plan.links.push_back({middle,after,other_account,both_mask,false});
        const auto remove_side=[&](const transfers::Side& side){
            if(!local_side(side)){remove(side.endpoint.entry,side.endpoint.account);return;}
            auto& out=impact(side.endpoint.account);
            out.change=out.change-side.amount;
            ++out.removed;
        };
        remove_side(chosen);
        if(request.delete_counterparts)remove_side(other);
        else if(local_side(other))++impact(other_account).changed;
        return plan;
    }

    // Bulk deletion leaves live linked rows and their counterparts in place.
    plan.preview.skipped=linked.size();
    for(const auto& id:selected){
        if(linked.contains(id))continue;
        bool local=false;
        const auto related=detached.find(id);
        if(related!=detached.end())for(const auto* value:related->second){
            const auto& link=value->link;
            const bool origin=link.origin.endpoint.entry==id;
            const auto& side=origin?link.origin:link.destination;
            check_source_match(*value);
            const int mask=origin?delete_transfer_origin:delete_transfer_destination;
            plan.links.push_back({*value,unlink_transfer_for_deletion(*value,mask),request.account,mask,false});
            if(local_side(side)){
                local=true;
                require_writable_account(impl_->db,request.account);
                auto& out=impact(request.account);
                out.change=out.change-side.amount;
                ++out.removed;
            }
        }
        if(!local){
            if(id.batch.starts_with("~transfer-"))fail(SQLITE_CONSTRAINT,"This generated transaction is unavailable.");
            remove(id,request.account);
        }
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
    if(request.keep_counterpart)encoded.text("keep-counterpart");
    const auto request_hash=session_snapshot::digest(encoded.bytes);
    session_snapshot::retry(impl_->db,operation,"selection",request_hash);
    if(const auto outcome=ActionChain(impl_->db).durable_outcome(operation,request_hash);outcome && *outcome==ActionChain::Outcome::no_change)return {std::string(operation)};
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='ledger-delete'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded.bytes)fail(SQLITE_CONSTRAINT,"Retry changed the deletion choice or selection");return *prior;
    }
    const auto plan=plan_deletion(request,context);
    if((plan.preview.counterpart && !request.keep_counterpart && !request.delete_counterparts) ||
       (plan.rows.empty() && plan.links.empty()))
        fail(SQLITE_CONSTRAINT,"These transactions are linked to other accounts. They stay in this account.");
    if(request.keep_counterpart && !plan.preview.counterpart)
        fail(SQLITE_CONSTRAINT,"This transfer changed. Select it again before deleting it.");
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
        Statement prior_link(impl_->db,"SELECT sequence FROM transfer_heads WHERE identity=?");prior_link.text(1,action.link);if(!prior_link.row())fail(SQLITE_CORRUPT);const auto prior_sequence=prior_link.integer(0);
        const auto child=std::string(operation)+"-link-"+std::to_string(++serial);apply_transfer(child,action);impl_->composing_review=false;
        Statement h(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");h.text(1,link.first?unlink:std::string(operation));h.text(2,action.link);h.blob(3,transfer_codec::encode(link.before));h.blob(4,transfer_codec::encode(link.after));h.text(5,child);h.integer(6,prior_sequence);h.done();
    }
    std::map<std::int64_t,Money> balances;
    for(const auto& row:plan.rows){
        context.checkpoint("Deleting selected transactions");impl_->selected_account=row.account;
        auto balance=balances.find(row.account);if(balance==balances.end())balance=balances.emplace(row.account,committed_balance()).first;
        const auto child=std::string(operation)+"-edit-"+std::to_string(++serial);
        write_review_row(row.remove?std::string(operation):kept,child,row.id,row.draft,row.decision,!row.remove,false,false,{},{},&balance->second);
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();impl_->pending_review_action=operation;
    session_snapshot::completed(impl_->db,operation,"selection",request_hash);
    context.checkpoint("Finishing deletion");finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}

// Selection commands derive their changes from retained rows, never UI snapshots.
std::int64_t ImportStore::ledger_edit_generation() const {return review_generation(impl_->db);}
app::ReviewReceipt ImportStore::edit_transactions(std::string_view operation,const app::LedgerSelectionEdit& request,const app::OperationContext& context) {
    if(request.action==app::LedgerSelectionAction::remove)return delete_transactions(operation,request,context);
    ScopedStatements statements(impl_->db,&context);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(request.selected.empty())fail(SQLITE_CONSTRAINT,"Select at least one transaction");
    if(request.action!=app::LedgerSelectionAction::rename_payee && request.action!=app::LedgerSelectionAction::restore_imported && request.action!=app::LedgerSelectionAction::remove && request.action!=app::LedgerSelectionAction::set_memo)fail(SQLITE_CONSTRAINT,"Choose Rename payee, Set memo, Restore imported values, or Delete");
    if(request.action!=app::LedgerSelectionAction::rename_payee && !request.payee.empty())fail(SQLITE_CONSTRAINT,"This action does not accept a replacement payee");
    if(request.payee.size()>65536)fail(SQLITE_CONSTRAINT,"The payee name is too long");
    if(request.action!=app::LedgerSelectionAction::set_memo && (!request.memo.empty() || request.overwrite_memo))fail(SQLITE_CONSTRAINT,"This action does not accept a replacement memo");
    if(request.action==app::LedgerSelectionAction::set_memo && request.delete_counterparts)fail(SQLITE_CONSTRAINT,"This action does not delete transactions");
    if(request.memo.size()>65536)fail(SQLITE_CONSTRAINT,"The memo is too long");
    const bool removing=request.action==app::LedgerSelectionAction::remove;
    const std::string question=removing?"ledger-delete":"ledger-edit-batch";
    transfer_codec::Writer encoded;encoded.number(static_cast<int>(request.action));encoded.number(request.account);encoded.number(request.generation);encoded.text(request.payee);
    std::set<imports::Occurrence> identities;
    for(const auto& id:request.selected){context.checkpoint("Checking selected transactions");if(!identities.insert(id).second)fail(SQLITE_CONSTRAINT,"Select each transaction only once");encoded.text(review_key(id));}
    if(request.action==app::LedgerSelectionAction::set_memo){encoded.text(request.memo);encoded.number(request.overwrite_memo?1:0);}
    const auto request_hash=session_snapshot::digest(encoded.bytes);
    session_snapshot::retry(impl_->db,operation,"selection",request_hash);
    if(const auto outcome=ActionChain(impl_->db).durable_outcome(operation,request_hash);outcome && *outcome==ActionChain::Outcome::no_change)return {std::string(operation)};
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question=?");saved.text(1,operation);saved.text(2,question);
        if(!saved.row() || saved.bytes(0)!=encoded.bytes)fail(SQLITE_CONSTRAINT,"Retry changed the selected transactions or values");
        return *prior;
    }
    const auto current=[&]{return request.account==impl_->selected_account && request.generation==review_generation(impl_->db);};
    if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    require_writable_account(impl_->db,request.account);
    if(request.generation==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
    struct Plan {imports::Occurrence id;std::int64_t account=0;imports::DraftRow draft;imports::Decision decision;std::optional<EntryLinkPlan> links;};
    std::vector<Plan> plans;const auto before_links=all_transfers();std::set<std::string> changed_links;
    const auto catalog=category_catalog();
    const auto open_account=impl_->selected_account;
    struct OpenAccount {Impl& state;std::int64_t account;~OpenAccount(){state.selected_account=account;}} open{*impl_,open_account};
    // One running balance per account. A selection inside the open account still
    // reads that balance once, then updates it as each row is written.
    std::map<std::int64_t,Money> balances;
    auto account_balance=[&](std::int64_t account)->Money&{
        auto found=balances.find(account);
        if(found==balances.end()){impl_->selected_account=account;found=balances.emplace(account,committed_balance()).first;}
        return found->second;
    };
    const auto memo_has_text=[](std::string_view value){return std::any_of(value.begin(),value.end(),[](unsigned char c){return c!=' ' && c!='\t' && c!='\n' && c!='\r' && c!='\v' && c!='\f';});};
    const bool across=request.action==app::LedgerSelectionAction::rename_payee || request.action==app::LedgerSelectionAction::set_memo;
    for(const auto& id:request.selected){
        context.checkpoint("Checking selected transactions");
        if(id.batch.starts_with("~transfer-"))fail(SQLITE_CONSTRAINT,"Select imported transactions. Generated transfer entries cannot use this action.");
        const auto owner=account_for_batch(id.batch).identity;
        if(owner!=request.account && !across)fail(SQLITE_CONSTRAINT,"Select transactions from one account for a group edit.");
        impl_->selected_account=owner;
        require_account(id.batch);
        if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Select the retained transaction instead of a merged transaction.");
        const auto info=batch(impl_->db,id.batch);const auto row=load_row(id,false);
        if(!row.draft || row.excluded_by_clear || row.matched_generated || (row.decision.disposition!=imports::Disposition::include && row.decision.disposition!=imports::Disposition::unresolved))fail(SQLITE_CONSTRAINT,"Resolve the selected transactions in Review before editing them.");
        const bool committed=transaction_accepted(id);
        const auto before=committed?committed_entry(id).current.draft:*row.draft;
        auto draft=before;
        if(request.action==app::LedgerSelectionAction::rename_payee){draft.payee=request.payee;draft.payee_owner=FieldOwnership::manual;draft.payee_rule.reset();}
        else if(request.action==app::LedgerSelectionAction::set_memo){
            const bool existing=memo_has_text(draft.memo);
            if(!existing || request.overwrite_memo){
                const auto next=memo_has_text(request.memo)?request.memo:std::string{};
                if(existing || !next.empty()){draft.memo=next;draft.memo_owner=FieldOwnership::manual;draft.memo_rule.reset();}
            }
        }
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
        // An unchanged memo must not be canonicalized into a new revision. The row
        // is still rejected when this command cannot edit it.
        if(request.action==app::LedgerSelectionAction::set_memo && same_draft(before,draft)){
            if(committed){if(plan_entry_links(before_links,id,before,draft,false).changed.empty())continue;}
            else{
                require_unlinked(id);
                if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts && !restored_draft(impl_->db,id))fail(SQLITE_CONSTRAINT,"This transaction cannot be edited.");
                continue;
            }
        }
        std::optional<EntryLinkPlan> links;
        if(committed){
            const auto preview=preview_change({removing?app::ChangeKind::remove:app::ChangeKind::correct,id,removing?std::optional<imports::DraftRow>{}:std::optional<imports::DraftRow>{draft},{}},true,account_balance(owner));draft=preview.after_entry.draft;
            links=plan_entry_links(before_links,id,before,draft,removing);
            for(const auto& link:links->changed)if(!changed_links.insert(link.link.identity).second)fail(SQLITE_CONSTRAINT,"These transactions belong to the same transfer. Change one transaction at a time.");
        }else{
            if(removing && transfer_linked(id))fail(SQLITE_CONSTRAINT,"Unlink this transfer before deleting it.");
            require_unlinked(id);
            if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts && !restored_draft(impl_->db,id))fail(SQLITE_CONSTRAINT,"This transaction cannot be edited.");
            if(info.revision==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");
            draft=canonical_draft(id,draft,before,info.revision+1,false);
            imports::validate_draft(row.source,draft,row.source.source_currency && *row.source.source_currency!=draft.transaction.effective_total().currency());
        }
        if(same_draft(before,draft) && (!links || links->changed.empty()))continue;
        plans.push_back({id,owner,std::move(draft),row.decision,std::move(links)});
    }
    impl_->selected_account=open_account;
    Atomic transaction(impl_->db);if(!current())fail(SQLITE_CONSTRAINT,"Transactions changed. Select them again before editing.");
    if(plans.empty()){ActionChain(impl_->db).no_change(action_token(),operation,request_hash);session_snapshot::completed(impl_->db,operation,"selection",request_hash);context.seal();transaction.commit();return {std::string(operation)};}
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,?,?,?)");receipt.text(1,operation);receipt.text(2,question);receipt.text(3,encoded.bytes);receipt.integer(4,request.generation);receipt.done();
    std::size_t serial=0;
    for(const auto& plan:plans){
        context.checkpoint("Updating selected transactions");const auto child=std::string(operation)+"-edit-"+std::to_string(++serial);
        auto& running=account_balance(plan.account);
        write_review_row(operation,child,plan.id,plan.draft,plan.decision,!removing,false,false,{},{},&running);
        if(plan.links && !plan.links->changed.empty()){
            write_entry_links(child,plan.account,app::TransferAction::relink,financial_sequence(impl_->db),plan.links->changed);
            for(const auto& value:plan.links->changed){
                const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
                Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
                if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));
                history.blob(4,transfer_codec::encode(value));history.text(5,child);history.integer(6,old==before_links.end()?0:old->sequence);history.done();
            }
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,request.generation+1);stack.done();impl_->pending_review_action=operation;
    session_snapshot::completed(impl_->db,operation,"selection",request_hash);
    context.checkpoint("Finishing selected transaction changes");finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}
