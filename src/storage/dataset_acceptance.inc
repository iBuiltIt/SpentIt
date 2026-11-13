app::ImportResult ImportStore::import_result(std::string_view id) const {
    require_account(id);app::ImportResult result;
    Statement rows(impl_->db,"SELECT v.decision,count(*) FROM versions v WHERE batch=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) GROUP BY v.decision");rows.text(1,id);
    while(rows.row()){const auto count=static_cast<std::size_t>(rows.integer(1));if(rows.integer(0)==1)result.added=count;else if(rows.integer(0)==2)result.already_present=count;else result.unresolved=count;}
    Statement matches(impl_->db,"SELECT count(*) FROM generated_source_matches WHERE batch=?");matches.text(1,id);if(matches.row()){const auto count=static_cast<std::size_t>(matches.integer(0));result.added-=count;result.already_present+=count;}
    Statement failures(impl_->db,"SELECT (SELECT count(*) FROM source_occurrences o WHERE batch=?1 AND NOT EXISTS(SELECT 1 FROM records r WHERE r.batch=o.batch AND r.ordinal=o.ordinal))+(SELECT count(*) FROM import_file_errors WHERE batch=?1)");failures.text(1,id);if(failures.row())result.failed=static_cast<std::size_t>(failures.integer(0));return result;
}
bool ImportStore::transaction_accepted(const imports::Occurrence& id) const {
    Statement row(impl_->db,"SELECT 1 FROM accepted_transactions WHERE batch=? AND ordinal=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));return row.row();
}
std::vector<imports::Occurrence> ImportStore::merge_survivors() const {
    std::vector<imports::Occurrence> out;
    Statement rows(impl_->db,R"SQL(SELECT DISTINCT target_batch,target_ordinal FROM review_merges m
        WHERE EXISTS(SELECT 1 FROM records r WHERE r.batch=m.target_batch AND r.ordinal=m.target_ordinal)
          AND NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=m.target_batch AND a.ordinal=m.target_ordinal))SQL");
    while(rows.row())out.push_back({rows.bytes(0),static_cast<std::size_t>(rows.integer(1))});
    return out;
}
// Dataset acceptance consumes prepared mutation review. Financial revisions are
// referenced by entry_revisions.draft_revision, never copied at acceptance.
app::AcceptanceReview ImportStore::acceptance_review() const {
    if(impl_->review_changes!=sqlite3_total_changes64(impl_->db))const_cast<ImportStore*>(this)->prepare_review();
    app::AcceptanceReview result;
    for(const auto& pending:missing_import_categories()) {
        Statement member(impl_->db,"SELECT account_id,batch FROM import_group_accounts WHERE group_id=? ORDER BY source_account LIMIT 1");member.text(1,pending.source);
        if(!member.row())fail(SQLITE_CORRUPT,"Import category decision has no accounts");
        result.warnings.push_back({member.integer(0),member.bytes(1),{},"","Choose the imported categories before accepting transactions"});
    }
    Statement incomplete(impl_->db,"SELECT count(*) FROM batches WHERE state=3");if(incomplete.row())result.failed_records=static_cast<std::size_t>(incomplete.integer(0));
    const auto resolved=resolved_transfers(all_transfers());
    for(const auto& [id,review]:impl_->reviews) {
        const auto info=batch(impl_->db,id);const auto owner=account_for_batch(id);
        const auto accepted=accepted_row_count(impl_->db,id);
        Statement count(impl_->db,"SELECT count(*) FROM source_occurrences WHERE batch=?");count.text(1,id);if(!count.row())fail(SQLITE_CORRUPT);const auto remaining=static_cast<std::size_t>(count.integer(0));
        if(remaining>=review.rows.size()+accepted)result.failed_records+=remaining-review.rows.size()-accepted;
        if(info.blocked && remaining==review.rows.size()+accepted)++result.failed_records;
        for(const auto& [ordinal,item]:review.rows) {
            if(item.row.excluded_by_clear || !item.row.draft)continue;
            if(item.row.decision.disposition!=imports::Disposition::duplicate || !item.warning.empty())++result.transactions;
            if(!item.transfer_warning.empty()) {
                bool other=false;
                std::vector<std::string> missing;
                for(const auto& allocation:item.row.draft->transaction.allocations()) {
                    if(resolved.contains({id,ordinal,allocation.identity}))continue;
                    const auto named=transfer_name(allocation.category);
                    if(!named)continue;
                    bool ambiguous=false;
                    std::optional<std::int64_t> destination;
                    try {destination=resolve_account_name(*named);}
                    catch(const ImportError& error){if(error.code()!=SQLITE_CONSTRAINT)throw;ambiguous=true;}
                    if(!destination && !ambiguous)missing.push_back(*named);
                    else other=true;
                }
                // Keep the generic refusal when some other transfer on this row is unresolved,
                // or when the stored warning did not classify. Missing names are separate lines.
                if(other || missing.empty()) {
                    app::ImportBlocker warning{owner.identity,id,ordinal,"",item.transfer_warning};
                    result.warnings.push_back(std::move(warning));
                }
                std::string shown_date;
                try {shown_date=display_date(item.row.draft->date,date_format());}
                catch(const std::exception&){shown_date=item.row.draft->date;}
                for(const auto& destination:missing) {
                    std::string reason=owner.name;
                    if(!shown_date.empty())reason+=" · "+shown_date;
                    if(!item.row.draft->payee.empty())reason+=" · "+item.row.draft->payee;
                    reason+=" · "+destination+" is not an account";
                    app::ImportBlocker warning{owner.identity,id,ordinal,"",std::move(reason)};
                    warning.missing_destination=true;
                    result.warnings.push_back(std::move(warning));
                }
            } else if(!item.warning.empty()) {
                app::ImportBlocker warning{owner.identity,id,ordinal,"",item.warning};
                warning.overridable=item.warning!=duplicate_decision_warning;
                result.warnings.push_back(std::move(warning));
            }
        }
    }
    for(const auto& value:all_transfers())for(const auto* side:{&value.link.origin,&value.link.destination}) {
        if(side->state!=transfers::State::draft || !local_side(*side) || side->proposed_evidence)continue;
        ++result.transactions;
        if(value.link.unresolved)
            result.warnings.push_back({side->endpoint.account,"",{},"","Resolve this transfer before accepting transactions"});
        else if(!side->accepted_unmatched) {
            app::ImportBlocker warning{side->endpoint.account,"",{},"","This transfer has not been matched to an imported transaction."};warning.overridable=true;result.warnings.push_back(std::move(warning));
        }
    }
    return result;
}
std::optional<app::AcceptanceReceipt> ImportStore::acceptance_outcome(std::string_view operation) const {
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT transactions,failed_records,ignored FROM dataset_acceptances WHERE id=?");row.text(1,operation);
    if(!row.row())return {};
    return app::AcceptanceReceipt{std::string(operation),static_cast<std::size_t>(row.integer(0)),static_cast<std::size_t>(row.integer(1)),row.integer(2)!=0};
}
void ImportStore::accept_local(std::string_view operation,const app::TransferRequest& request) {
    const auto owner=account();const auto before=committed_balance();auto values=all_transfers();
    const auto local_before=local_total(values,owner.identity,owner.currency,false);
    auto found=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==request.link;});
    if(found==values.end() || request.expected_sequence!=transfer_sequence())fail(SQLITE_CONSTRAINT,"Transfer changed before acceptance");
    *found=transition_transfer(*found,request,owner.identity);validate_transfer_set(values);
    const auto change=local_total(values,owner.identity,owner.currency,false)-local_before;
    const auto sequence=request.expected_sequence+1;const bool accepting=request.action==app::TransferAction::commit_local;
    Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,owner.identity);event.blob(3,transfer_codec::encode(request));event.integer(4,accepting?1:0);event.integer(5,change.units());event.done();
    store_transfer(impl_->db,*found,sequence,false);
    append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,before,change,before+change,"",request.expected_sequence,catalog_sequence(impl_->db));
    if(accepting)exec(impl_->db,"UPDATE account SET frozen=1 WHERE singleton=spentit_account()");
}
app::AcceptanceReceipt ImportStore::accept_all(std::string_view operation,bool ignore_warnings) {
    identifier(operation);if(operation.size()>80)fail(SQLITE_MISUSE,"Acceptance operation identifier is too long");
    const auto digest=session_snapshot::digest(ignore_warnings?"accept:1":"accept:0");
    if(const auto saved=ActionChain(impl_->db).durable_outcome(operation,digest);saved && *saved!=ActionChain::Outcome::applied && *saved!=ActionChain::Outcome::no_change)throw app::OperationAlreadyCompleted(std::string(operation));
    if(const auto prior=acceptance_outcome(operation)) {
        if(prior->ignored_warnings!=ignore_warnings)fail(SQLITE_CONSTRAINT,"Acceptance retry changed its warning decision");
        return *prior;
    }
    ActionChain(impl_->db).require_idle();
    const auto baseline=acceptance_baseline(impl_->db,all_transfers());
    const auto review=acceptance_review();
    for(const auto& warning:review.warnings)if(!ignore_warnings || !warning.overridable)fail(SQLITE_CONSTRAINT,warning.reason.c_str());
    struct Scope {Impl& state;std::int64_t selected;~Scope(){state.selected_account=selected;state.accepting_dataset=false;state.publishing_review=false;}} scope{*impl_,impl_->selected_account};
    Atomic transaction(impl_->db);impl_->accepting_dataset=true;impl_->publishing_review=true;
    Statement receipt(impl_->db,"INSERT INTO dataset_acceptances VALUES(?,?,?,?)");receipt.text(1,operation);receipt.integer(2,integer(review.transactions));receipt.integer(3,integer(review.failed_records));receipt.integer(4,ignore_warnings?1:0);receipt.done();
    if(review.transactions>0){
        Statement names(impl_->db,"SELECT name FROM retained_category_names ORDER BY name");std::size_t ordinal=0;
        while(names.row()){
            if(resolve_account_name(names.bytes(0)))continue;
            const auto name=canonical_category_name(names.bytes(0));const auto catalog=category_catalog();
            if(std::any_of(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && app::category_name_matches(c.name,name);}))continue;
            const auto id=std::string(operation)+"-category-"+std::to_string(++ordinal);
            write_category_change(id,preview_category_change({app::CategoryAction::create,id,name,{},CategoryKind::leaf}));
        }
        exec(impl_->db,"DELETE FROM retained_category_names");
    }
    std::vector<std::string> batches;
    Statement pending(impl_->db,"SELECT id FROM batches WHERE state IN(0,4) ORDER BY rowid");while(pending.row())batches.push_back(pending.bytes(0));
    std::size_t serial=0;
    const auto child=[&]{return std::string(operation)+"-"+std::to_string(++serial);};
    const auto member=[&](std::string_view id){Statement m(impl_->db,"INSERT INTO acceptance_operations VALUES(?,?)");m.text(1,operation);m.text(2,id);m.done();};
    for(const auto& id:batches) {
        auto& prepared=impl_->reviews.at(id);impl_->selected_account=account_for_batch(id).identity;
        // Ignore is an explicit Include decision on already-valid rows. It
        // changes review metadata and consumes the previously validated image.
        if(ignore_warnings)for(auto& [ordinal,item]:prepared.rows)if(!item.row.excluded_by_clear && !item.warning.empty() && item.row.draft) {
            const auto revision=batch(impl_->db,id).revision;
            item.row.decision={imports::Disposition::include,{}};item.row.review_stale=false;item.warning.clear();
            version(impl_->db,{id,ordinal},revision+1,*item.row.draft,item.row.decision);
            advance(impl_->db,id,revision,"decide");prepared.revision=revision+1;
        }
        prepared.included=0;prepared.excluded=0;prepared.amounts.clear();
        for(const auto& [ordinal,item]:prepared.rows) {
            if(item.row.excluded_by_clear){if(!pending_removed(impl_->db,{id,ordinal}))++prepared.excluded;continue;}
            if(item.row.decision.disposition==imports::Disposition::include){++prepared.included;prepared.amounts.push_back(item.row.draft->transaction.effective_total());}
            else ++prepared.excluded;
        }
        // A duplicate-only source already completed durably during import. It
        // neither accepts older work nor introduces a financial history event.
        if(prepared.included==0)continue;
        require_writable_account(impl_->db,impl_->selected_account);
        const auto id_operation=child();publish_review(id,prepared.revision,id_operation);member(id_operation);
    }
    // Accept every remaining generated/manual counterpart in the same boundary.
    for(const auto& initial:all_transfers())for(const auto account:{initial.link.origin.endpoint.account,initial.link.destination.endpoint.account}) {
        auto values=all_transfers();const auto found=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==initial.link.identity;});
        const auto& side=found->link.origin.endpoint.account==account?found->link.origin:found->link.destination;
        if(side.state!=transfers::State::draft || !local_side(side) || side.proposed_evidence)continue;
        impl_->selected_account=account;require_writable_account(impl_->db,account);
        app::TransferRequest request{};request.link=initial.link.identity;
        if(!side.accepted_unmatched){request.action=app::TransferAction::accept;request.expected_sequence=transfer_sequence();const auto op=child();accept_local(op,request);member(op);}
        request.action=app::TransferAction::commit_local;request.expected_sequence=transfer_sequence();const auto op=child();accept_local(op,request);member(op);
    }
    if(serial)record_acceptance_action(operation,acceptance_recipe(impl_->db,baseline),baseline.links,digest,review.transactions);
    else ActionChain(impl_->db).no_change(action_token(),operation,digest);
    finish([&]{transaction.commit();},serial==0);
    return {std::string(operation),review.transactions,review.failed_records,ignore_warnings};
}
