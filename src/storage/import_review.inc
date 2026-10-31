// Session review state belongs to the exclusively locked connection. Mutations
// publish it before their durable boundary; the rollback hook drops speculative
// results. It is reconstructed after verified open, never persisted as authority.
void ImportStore::finish(const std::function<void()>& commit) {
    if(impl_->composing_review){commit();return;}
    exec(impl_->db,"UPDATE mutation_generation SET revision=revision+1 WHERE singleton=1");
    exec(impl_->db,"DELETE FROM source_associations WHERE source_batch IN(SELECT id FROM batches WHERE state=2) OR (source_batch IN(SELECT id FROM batches WHERE state=4) AND NOT EXISTS(SELECT 1 FROM retained_transfer_rows r WHERE r.batch=source_batch AND r.ordinal=source_ordinal))");
    for(const auto& value:all_transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
        Statement active(impl_->db,"UPDATE generated_source_matches SET active=? WHERE link=? AND account=? AND active!=?");const auto live=side->state!=transfers::State::removed?1:0;active.integer(1,live);active.text(2,value.link.identity);active.integer(3,side->endpoint.account);active.integer(4,live);active.done();
    }
    prepare_review();
    exec(impl_->db,"DELETE FROM import_completions");
    for(const auto& [id,review]:impl_->reviews)if(review.included==0 && review.warning.empty()) {
        Statement complete(impl_->db,"INSERT INTO import_completions SELECT id FROM batches WHERE id=? AND state=0 AND blocked=0 AND NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.batch=batches.id AND m.created=1)");complete.text(1,id);complete.done();
    }
    impl_->review_changes=sqlite3_total_changes64(impl_->db);
    if(impl_->observer)impl_->observer(ImportPhase::before_commit);
    commit();
    if(impl_->observer)impl_->observer(ImportPhase::after_commit);
}
void ImportStore::prepare_review() {
    ScopedStatements plans(impl_->db);
    struct Guard {
        Impl& state;std::int64_t account;bool prior;
        ~Guard(){state.selected_account=account;state.preparing_review=prior;}
    } guard{*impl_,impl_->selected_account,impl_->preparing_review};
    impl_->preparing_review=true;
    const auto resolved=resolved_transfers(all_transfers());
    std::string names;
    for(const auto& alias:account_aliases())names+=std::to_string(alias.account)+":"+std::to_string(alias.name.size())+":"+alias.name;
    std::set<std::string> pending;
    Statement batches(impl_->db,"SELECT id,revision,(SELECT count(*) FROM source_occurrences o WHERE o.batch=batches.id),blocked FROM batches WHERE state IN(0,4) ORDER BY rowid");
    while(batches.row()) {
        const auto id=batches.bytes(0);pending.insert(id);
        impl_->selected_account=account_for_batch(id).identity;
        auto& review=impl_->reviews[id];review.revision=batches.integer(1);
        review.included=0;review.excluded=0;review.amounts.clear();review.warning.clear();
        if(batches.integer(3))review.warning="Resolve the blocked import before committing";
        std::set<imports::Occurrence> matched;
        Statement rows(impl_->db,R"SQL(SELECT v.ordinal,v.revision,
          coalesce(h.revision,-1),coalesce(h.active,-1),
          (SELECT count(*) FROM source_associations WHERE ledger_batch=v.batch AND ledger_ordinal=v.ordinal),
          (SELECT count(*) FROM source_associations WHERE ledger_batch=v.duplicate_batch AND ledger_ordinal=v.duplicate_ordinal)
          FROM versions v LEFT JOIN review_targets h ON h.batch=v.duplicate_batch AND h.ordinal=v.duplicate_ordinal
          WHERE v.batch=? AND NOT EXISTS(SELECT 1 FROM accepted_transactions accepted WHERE accepted.batch=v.batch AND accepted.ordinal=v.ordinal) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=v.batch AND g.ordinal=v.ordinal) AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)
          ORDER BY v.ordinal)SQL");rows.text(1,id);
        std::set<std::size_t> present;
        while(rows.row()) {
            const auto ordinal=static_cast<std::size_t>(rows.integer(0));present.insert(ordinal);
            const imports::Occurrence identity{id,ordinal};
            const bool retained=retained_member(impl_->db,id,ordinal);
            std::string stamp=retained?"kept":"excluded";
            for(int column=1;column<6;++column)stamp+=":"+std::to_string(rows.integer(column));
            auto found=review.rows.find(ordinal);
            if(found==review.rows.end() || found->second.stamp!=stamp) {
                // Do not let load_row see an older speculative result.
                review.rows.erase(ordinal);
                auto row=load_row(identity,false);
                std::string warning;
                if(!row.draft || row.decision.disposition==imports::Disposition::unresolved)
                    warning="Review possible duplicates before committing";
                else if(row.decision.disposition==imports::Disposition::duplicate) {
                    if(!row.decision.duplicate_of || row.review_stale)warning="Duplicate target changed; reconfirm the review decision";
                    else {
                        const auto& target=*row.decision.duplicate_of;
                        const auto other=load_row(target,false);
                        if(!review_merged_source(impl_->db,identity) && (other.decision.disposition!=imports::Disposition::include || !imports::candidate_basis(row.source,other.source,confirmed_duplicate(impl_->db,identity,target,rows.integer(1))?imports::MatchContext::confirmed_decision:imports::MatchContext::suggestion)))
                            warning="Invalid duplicate relationship";
                    }
                }
                if(row.decision.disposition==imports::Disposition::duplicate && !warning.empty())row.review_stale=true;
                found=review.rows.emplace(ordinal,PreparedImportRow{std::move(stamp),std::move(row),std::move(warning),{},{}}).first;
                if(impl_->observer)impl_->observer(ImportPhase::review_row);
            }
            auto& item=found->second;const auto& row=item.row;
            if(!retained){++review.excluded;continue;}
            if(!item.warning.empty())review.warning=item.warning;
            if(row.draft && (row.decision.disposition!=imports::Disposition::duplicate || !item.warning.empty())) {
                auto transfer_stamp=names;
                for(const auto& allocation:row.draft->transaction.allocations())transfer_stamp+=resolved.contains({id,ordinal,allocation.identity})?"1":"0";
                if(item.transfer_stamp!=transfer_stamp) {
                    item.transfer_stamp=std::move(transfer_stamp);item.transfer_warning.clear();
                    try {require_qif_transfers(identity,*row.draft,resolved);}
                    catch(const ImportError& error){if(error.code()!=SQLITE_CONSTRAINT)throw;item.transfer_warning=error.what();}
                }
                if(!item.transfer_warning.empty())review.warning=item.transfer_warning;
            }
            if(row.decision.disposition==imports::Disposition::include && row.draft) {
                ++review.included;review.amounts.push_back(row.draft->transaction.effective_total());
            } else {
                ++review.excluded;
                if(row.decision.duplicate_of && !matched.insert(*row.decision.duplicate_of).second)review.warning="Invalid duplicate relationship";
            }
        }
        for(auto i=review.rows.begin();i!=review.rows.end();)if(!present.contains(i->first))i=review.rows.erase(i);else ++i;
        if(present.size()+accepted_row_count(impl_->db,id)!=static_cast<std::size_t>(batches.integer(2)) && batch(impl_->db,id).state!=app::BatchState::protected_drafts)
            review.warning="Resolve the blocked import before committing";
    }
    // Group opening/repair review has cross-account dependencies. Reuse the
    // validated row images above while refreshing its aggregate warning state.
    impl_->reviewed_groups.clear();
    Statement groups(impl_->db,"SELECT id FROM import_groups WHERE state IN(1,2)");
    while(groups.row()) {
        const auto id=groups.bytes(0);
        impl_->reviewed_groups.emplace(id,import_group(id));
    }
    for(auto i=impl_->reviews.begin();i!=impl_->reviews.end();)if(!pending.contains(i->first))i=impl_->reviews.erase(i);else ++i;
    impl_->review_changes=sqlite3_total_changes64(impl_->db);
}
