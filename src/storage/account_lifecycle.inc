struct CommandFlag {
    int& slot;
    explicit CommandFlag(int& value):slot(value){slot=1;}
    ~CommandFlag(){slot=0;}
};
app::Account ImportStore::command_account(bool money_edit) const {
    if(impl_->selected_account<=0) fail(SQLITE_CONSTRAINT,"No active account is selected. Archived accounts remain available.");
    if(account_lifecycle_retired(impl_->db,impl_->selected_account) || account_import_retired(impl_->db,impl_->selected_account)) fail(SQLITE_CONSTRAINT,"Account identity was retired");
    auto acct=account();
    if(money_edit && acct.archived) fail(SQLITE_CONSTRAINT,reactivate_first);
    return acct;
}
bool ImportStore::account_selected() const {
    if(impl_->selected_account<=0 || account_lifecycle_retired(impl_->db,impl_->selected_account) || account_import_retired(impl_->db,impl_->selected_account)) return false;
    Statement row(impl_->db,"SELECT 1 FROM account WHERE singleton=?");row.integer(1,impl_->selected_account);return row.row();
}
void ImportStore::release_account_selection() {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    impl_->selected_account=0;
}
std::vector<app::Account> ImportStore::archived_accounts() const {
    Statement ids(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM retired_import_accounts r WHERE r.account_id=singleton) AND NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=singleton) AND EXISTS(SELECT 1 FROM account_status s WHERE s.identity=singleton AND s.archived=1) ORDER BY singleton");
    std::vector<app::Account> result;while(ids.row()) result.push_back(account_by_id(ids.integer(0)));return result;
}
std::optional<app::AccountCommandReceipt> ImportStore::account_command_outcome(std::string_view operation) const {
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    if(auto found=account_command_row(impl_->db,operation)) return found;
    Statement reg(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");reg.text(1,operation);
    if(reg.row()) fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    return {};
}
std::optional<std::string> ImportStore::undoable_account_operation() const {
    if(impl_->selected_account<=0) return {};
    Statement row(impl_->db,"SELECT o.id,o.kind FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND o.kind IN(4,5,6) ORDER BY h.sequence DESC LIMIT 1");
    row.integer(1,impl_->selected_account);if(!row.row() || row.integer(1)==6) return {};
    const auto id=row.bytes(0);const auto kind=static_cast<int>(row.integer(1));
    if(!command_image_current(impl_->db,id) || later_financial(impl_->db,impl_->selected_account,account_anchor(impl_->db,id,kind))) return {};
    return id;
}
bool ImportStore::account_currency_warning() const {
    if(impl_->selected_account<=0) return false;
    const auto acct=account();if(acct.archived) return false;
    if(acct.opening_confirmed || acct.frozen || acct.opening.units()!=0) return true;
    Statement batches(impl_->db,"SELECT 1 FROM batches WHERE account_id=? LIMIT 1");batches.integer(1,acct.identity);if(batches.row()) return true;
    return transfer_participation(impl_->db,acct.identity);
}
app::AccountCommandReceipt ImportStore::rename_account(std::string_view operation,std::string_view name) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);safe_text(name);if(blank_name(name)) fail(SQLITE_MISUSE,"Account name is required");
    const auto content="0\n"+std::string(name);
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    const auto acct=command_account(false);
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    require_account_name(name,acct.identity);
    insert_account_op(impl_->db,id,acct.identity,0,name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),acct.opening.units(),acct.opening.units(),acct.opening_date,acct.opening_date,acct.archived?1:0,acct.archived?1:0,0,"",content,acct.revision,acct.frozen?1:0,acct.opening_confirmed?1:0);
    Statement update(impl_->db,"UPDATE account SET name=? WHERE singleton=?");update.text(1,name);update.integer(2,acct.identity);update.done();
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::archive_account(std::string_view operation) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);const auto content="1";
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    const auto acct=command_account(false);
    if(acct.archived) fail(SQLITE_CONSTRAINT,"Account is already archived");
    Statement pending(impl_->db,"SELECT 1 FROM batches WHERE account_id=? AND state IN(0,3,4) LIMIT 1");pending.integer(1,acct.identity);
    if(pending.row()) fail(SQLITE_CONSTRAINT,"Finish or discard the pending import before archiving");
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,1,acct.name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),acct.opening.units(),acct.opening.units(),acct.opening_date,acct.opening_date,1,0,0,"",content,acct.revision,acct.frozen?1:0,acct.opening_confirmed?1:0);
    Statement existing(impl_->db,"SELECT 1 FROM account_status WHERE identity=?");existing.integer(1,acct.identity);
    if(existing.row()) {Statement update(impl_->db,"UPDATE account_status SET archived=1 WHERE identity=?");update.integer(1,acct.identity);update.done();}
    else {Statement insert(impl_->db,"INSERT INTO account_status VALUES(?,1)");insert.integer(1,acct.identity);insert.done();}
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::reactivate_account(std::string_view operation) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);const auto content="2";
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    const auto acct=command_account(false);
    if(!acct.archived) fail(SQLITE_CONSTRAINT,"Account is already active");
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,2,acct.name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),acct.opening.units(),acct.opening.units(),acct.opening_date,acct.opening_date,0,1,0,"",content,acct.revision,acct.frozen?1:0,acct.opening_confirmed?1:0);
    Statement update(impl_->db,"UPDATE account_status SET archived=0 WHERE identity=?");update.integer(1,acct.identity);update.done();
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::delete_account(std::string_view operation) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);const auto content="3";
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    if(impl_->selected_account<=0) fail(SQLITE_CONSTRAINT,"No active account is selected. Archived accounts remain available.");
    const auto acct=account();
    auto refuse=[&]{fail(SQLITE_CONSTRAINT,empty_account_deletion);};
    if(account_lifecycle_retired(impl_->db,acct.identity) || account_import_retired(impl_->db,acct.identity)) refuse();
    Statement batches(impl_->db,"SELECT 1 FROM batches WHERE account_id=? LIMIT 1");batches.integer(1,acct.identity);if(batches.row()) refuse();
    if(transfer_participation(impl_->db,acct.identity)) refuse();
    if(explicit_account_money(impl_->db,acct.identity)) refuse();
    Statement created(impl_->db,"SELECT opening_date FROM account_creations WHERE identity=?");created.integer(1,acct.identity);if(!created.row()) fail(SQLITE_CORRUPT);
    if(acct.opening.units()!=0 || acct.opening_date!=created.bytes(0)) refuse();
    Statement remaining(impl_->db,"SELECT count(*) FROM account WHERE NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=account.singleton)");
    if(!remaining.row() || remaining.integer(0)<=1) refuse();
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,3,acct.name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),acct.opening.units(),acct.opening.units(),acct.opening_date,acct.opening_date,acct.archived?1:0,acct.archived?1:0,1,"",content,acct.revision,acct.frozen?1:0,acct.opening_confirmed?1:0);
    Statement retirement(impl_->db,"INSERT INTO account_retirements VALUES(?,?)");retirement.integer(1,acct.identity);retirement.text(2,id);retirement.done();
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::correct_opening(std::string_view operation,std::string_view date,const Money& amount) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);iso_date(date);
    const auto content="4\n"+std::string(date)+"\n"+std::to_string(amount.units());
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    const auto acct=command_account(true);
    amount.require_compatible(acct.opening);
    std::optional<std::string> earliest;
    auto consider=[&](const std::string& value){if(!earliest || value<*earliest) earliest=value;};
    Statement dates(impl_->db,"SELECT e.date FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) JOIN batches b ON b.id=h.batch WHERE b.account_id=?");dates.integer(1,acct.identity);
    while(dates.row()) consider(dates.bytes(0));
    for(const auto& value:all_transfers()) for(const auto* side:{&value.link.origin,&value.link.destination}) if(side->endpoint.account==acct.identity && side->state==transfers::State::committed) consider(side->date);
    if(earliest && date>*earliest) fail(SQLITE_CONSTRAINT,"The opening date must be on or before the earliest accepted transaction.");
    if(acct.revision==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    guard_money([&]{(void)(committed_balance()-acct.opening+amount);});
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,4,acct.name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),amount.units(),acct.opening.units(),date,acct.opening_date,acct.archived?1:0,acct.archived?1:0,0,"",content,acct.revision+1,acct.frozen?1:0,1);
    Statement update(impl_->db,"UPDATE account SET opening_date=?,opening=?,revision=revision+1,confirmed=1 WHERE singleton=?");update.text(1,date);update.integer(2,amount.units());update.integer(3,acct.identity);update.done();
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::reassign_currency(std::string_view operation,const Currency& currency) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);
    if(Currency(currency.code())!=currency) fail(SQLITE_MISUSE,"Currency cannot be selected for a new account");
    const auto content="5\n"+currency.definition_id();
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    const auto ledger_id=scale_ledger_id(id);
    Statement taken(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");taken.text(1,ledger_id);if(taken.row()) fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    const auto acct=command_account(true);
    if(acct.currency==currency) fail(SQLITE_CONSTRAINT,"Account currency is unchanged");
    if(acct.revision==INT64_MAX || financial_sequence(impl_->db)==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    struct DraftPlan {imports::Occurrence id;imports::DraftRow row;imports::Decision decision;std::int64_t prior_total;std::vector<std::int64_t> prior_alloc;};
    struct EntryPlan {imports::Occurrence id;app::EntrySnapshot row;std::int64_t prior_total;std::vector<std::int64_t> prior_alloc;};
    struct LinkPlan {app::TransferView view;bool origin;bool destination;std::int64_t origin_prior;std::int64_t destination_prior;};
    std::map<std::string,std::vector<DraftPlan>> drafts;std::map<std::string,std::pair<std::int64_t,int>> batch_state;
    std::vector<EntryPlan> entries;std::vector<LinkPlan> links;
    auto consider_draft=[&](const app::BatchInfo& info){
        if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts) return;
        Statement heads(impl_->db,"SELECT ordinal,revision,decision,duplicate_batch,duplicate_ordinal,total FROM versions v WHERE batch=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) ORDER BY ordinal");
        heads.text(1,info.id);
        while(heads.row()) {
            imports::Occurrence occ{info.id,static_cast<std::size_t>(heads.integer(0))};
            const auto source=load_row(occ,false).source;
            auto draft=guard_money([&]{return rescale_draft(draft_at(impl_->db,occ,heads.integer(1),source),currency);});
            imports::Decision decision{static_cast<imports::Disposition>(heads.integer(2)),{}};
            if(decision.disposition==imports::Disposition::duplicate) decision.duplicate_of=imports::Occurrence{heads.bytes(3),static_cast<std::size_t>(heads.integer(4))};
            std::vector<std::int64_t> prior;Statement alloc(impl_->db,"SELECT units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");
            alloc.text(1,info.id);alloc.integer(2,heads.integer(0));alloc.integer(3,heads.integer(1));while(alloc.row()) prior.push_back(alloc.integer(0));
            drafts[info.id].push_back({occ,std::move(draft),std::move(decision),heads.integer(5),std::move(prior)});
        }
        batch_state[info.id]={info.revision,static_cast<int>(info.state)};
    };
    for(const auto& info:batches()) {
        if(info.state==app::BatchState::captured) {const auto parsed=parsed_source(info.id);for(const auto& record:parsed.records) guard_money([&]{require_record_rescale(record,currency);});}
        consider_draft(info);
    }
    Statement entry_rows(impl_->db,"SELECT h.batch,h.ordinal,h.revision FROM entry_heads h JOIN batches b ON b.id=h.batch WHERE b.account_id=? ORDER BY h.batch,h.ordinal");entry_rows.integer(1,acct.identity);
    while(entry_rows.row()) {
        imports::Occurrence occ{entry_rows.bytes(0),static_cast<std::size_t>(entry_rows.integer(1))};
        auto current=entry_snapshot(occ,entry_rows.integer(2));
        const auto prior_total=current.draft.transaction.effective_total().units();
        std::vector<std::int64_t> prior;for(const auto& allocation:current.draft.transaction.allocations()) prior.push_back(allocation.amount.units());
        current.draft=guard_money([&]{return rescale_draft(current.draft,currency);});current.revision++;
        entries.push_back({occ,std::move(current),prior_total,std::move(prior)});
    }
    for(const auto& value:all_transfers()) if(link_mentions(value.link,acct.identity)) {
        auto copy=value;guard_money([&]{rescale_link_account(copy.link,acct.identity,currency);});
        links.push_back({std::move(copy),value.link.origin.endpoint.account==acct.identity,value.link.destination.endpoint.account==acct.identity,value.link.origin.amount.units(),value.link.destination.amount.units()});
    }
    const auto before=committed_balance();const auto opening=guard_money([&]{return acct.opening.rescale(currency);});
    const auto after=guard_money([&]{return before.rescale(currency);});
    Money recomputed(0,currency);recomputed=opening;
    for(const auto& entry:entries) if(entry.row.active) recomputed=recomputed+entry.row.draft.transaction.effective_total();
    for(const auto& link:links) for(const auto* side:{&link.view.link.origin,&link.view.link.destination}) if(side->endpoint.account==acct.identity && local_side(*side) && side->state==transfers::State::committed && !side->proposed_evidence) recomputed=recomputed+side->amount;
    if(recomputed.units()!=after.units()) fail(SQLITE_CONSTRAINT,"Currency reassignment does not reconcile");
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,5,acct.name,acct.name,currency.definition_id(),acct.currency.definition_id(),opening.units(),acct.opening.units(),acct.opening_date,acct.opening_date,acct.archived?1:0,acct.archived?1:0,0,"",content,acct.revision+1,acct.frozen?1:0,1);
    Statement update(impl_->db,"UPDATE account SET definition=?,opening=?,revision=revision+1,confirmed=1 WHERE singleton=?");update.text(1,currency.definition_id());update.integer(2,opening.units());update.integer(3,acct.identity);update.done();
    for(auto& [batch_id,plans]:drafts) {
        const auto revision=batch_state.at(batch_id).first;
        for(const auto& plan:plans) {
            version(impl_->db,plan.id,revision+1,plan.row,plan.decision,false);
            record_amount(impl_->db,id,0,plan.id.batch,integer(plan.id.ordinal),revision+1,0,plan.prior_total,plan.row.transaction.effective_total().units());
            const auto& allocations=plan.row.transaction.allocations();
            for(std::size_t i=0;i<allocations.size();++i) record_amount(impl_->db,id,1,plan.id.batch,integer(plan.id.ordinal),revision+1,integer(i),plan.prior_alloc.at(i),allocations[i].amount.units());
        }
        advance(impl_->db,batch_id,revision,"currency",batch_state.at(batch_id).second);
    }
    for(const auto& plan:entries) {
        store_entry(impl_->db,plan.id,plan.row);
        record_amount(impl_->db,id,2,plan.id.batch,integer(plan.id.ordinal),plan.row.revision,0,plan.prior_total,plan.row.draft.transaction.effective_total().units());
        const auto& allocations=plan.row.draft.transaction.allocations();
        for(std::size_t i=0;i<allocations.size();++i) record_amount(impl_->db,id,3,plan.id.batch,integer(plan.id.ordinal),plan.row.revision,integer(i),plan.prior_alloc.at(i),allocations[i].amount.units());
    }
    for(const auto& plan:links) {
        if(plan.origin) record_amount(impl_->db,id,4,plan.view.link.identity,0,0,0,plan.origin_prior,plan.view.link.origin.amount.units());
        if(plan.destination) record_amount(impl_->db,id,4,plan.view.link.identity,0,0,1,plan.destination_prior,plan.view.link.destination.amount.units());
    }
    const auto sequence=financial_sequence(impl_->db);
    append_financial(impl_->db,ledger_id,static_cast<app::ChangeKind>(5),{"",static_cast<std::size_t>(acct.identity)},-1,0,before,Money(0,before.currency()),after,id,sequence,catalog_sequence(impl_->db));
    const auto written=financial_sequence(impl_->db);
    for(const auto& plan:links) store_transfer(impl_->db,plan.view,written,false);
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
app::AccountCommandReceipt ImportStore::undo_account_operation(std::string_view operation,std::string_view undo_of) {
    if(!sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    const auto id=command_id(operation);command_id(undo_of);const auto content="6\n"+std::string(undo_of);
    if(auto prior=matching_account_command(impl_->db,id,content)) return *prior;
    Statement target(impl_->db,"SELECT account,kind,name,definition,prior_definition,opening,prior_opening,opening_date,prior_opening_date,archived,revision,frozen,confirmed FROM account_operations WHERE id=?");
    target.text(1,undo_of);if(!target.row() || (target.integer(1)!=4 && target.integer(1)!=5)) fail(SQLITE_CONSTRAINT,"Choose an opening balance or currency change to undo.");
    const auto acct=command_account(false);
    if(target.integer(0)!=acct.identity) fail(SQLITE_CONSTRAINT,"Account command belongs to another account");
    const auto kind=static_cast<int>(target.integer(1));
    if(!command_image_current(impl_->db,undo_of) || later_financial(impl_->db,acct.identity,account_anchor(impl_->db,undo_of,kind))) fail(SQLITE_CONSTRAINT,later_undo_block);
    if(acct.revision==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    if(kind==4) {
        const auto restored_units=target.integer(6);const auto restored_date=target.bytes(8);const auto restored=Money(restored_units,acct.currency);
        guard_money([&]{(void)(committed_balance()-acct.opening+restored);});
        Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
        insert_account_op(impl_->db,id,acct.identity,6,acct.name,acct.name,acct.currency.definition_id(),acct.currency.definition_id(),restored_units,acct.opening.units(),restored_date,acct.opening_date,acct.archived?1:0,acct.archived?1:0,0,undo_of,content,acct.revision+1,acct.frozen?1:0,1);
        Statement update(impl_->db,"UPDATE account SET opening_date=?,opening=?,revision=revision+1,confirmed=1 WHERE singleton=?");update.text(1,restored_date);update.integer(2,restored_units);update.integer(3,acct.identity);update.done();
        finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
    }
    const auto restored_currency=Currency::from_definition(target.bytes(4));
    const auto ledger_id=scale_ledger_id(id);
    Statement taken(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");taken.text(1,ledger_id);if(taken.row()) fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    if(financial_sequence(impl_->db)==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
    struct DraftPlan {imports::Occurrence id;imports::DraftRow row;imports::Decision decision;std::int64_t prior_total;std::vector<std::int64_t> prior_alloc;};
    struct EntryPlan {imports::Occurrence id;app::EntrySnapshot row;std::int64_t prior_total;std::vector<std::int64_t> prior_alloc;};
    struct LinkPlan {app::TransferView view;bool origin;bool destination;std::int64_t origin_prior;std::int64_t destination_prior;};
    std::map<std::string,std::vector<DraftPlan>> drafts;std::map<std::string,std::pair<std::int64_t,int>> batch_state;
    Statement marked(impl_->db,"SELECT batch,ordinal,revision,prior_units,units FROM account_amount_revisions WHERE operation=? AND kind=0 AND position=0 ORDER BY batch,ordinal");marked.text(1,undo_of);
    while(marked.row()) {
        const auto batch_id=marked.bytes(0);imports::Occurrence occ{batch_id,static_cast<std::size_t>(marked.integer(1))};
        const auto info=batch(impl_->db,batch_id);if(info.revision!=marked.integer(2)) fail(SQLITE_CONSTRAINT,later_undo_block);
        Statement prior(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=? AND revision<?");prior.text(1,batch_id);prior.integer(2,marked.integer(1));prior.integer(3,marked.integer(2));
        if(!prior.row() || prior.null(0)) fail(SQLITE_CORRUPT);
        const auto source=load_row(occ,false).source;auto draft=draft_at(impl_->db,occ,prior.integer(0),source);
        if(draft.transaction.effective_total().units()!=marked.integer(3)) fail(SQLITE_CONSTRAINT,later_undo_block);
        Statement head(impl_->db,"SELECT decision,duplicate_batch,duplicate_ordinal,total FROM versions WHERE batch=? AND ordinal=? AND revision=?");head.text(1,batch_id);head.integer(2,marked.integer(1));head.integer(3,marked.integer(2));if(!head.row() || head.integer(3)!=marked.integer(4)) fail(SQLITE_CONSTRAINT,later_undo_block);
        imports::Decision decision{static_cast<imports::Disposition>(head.integer(0)),{}};
        if(decision.disposition==imports::Disposition::duplicate) decision.duplicate_of=imports::Occurrence{head.bytes(1),static_cast<std::size_t>(head.integer(2))};
        std::vector<std::int64_t> prior_alloc;Statement alloc(impl_->db,"SELECT units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");alloc.text(1,batch_id);alloc.integer(2,marked.integer(1));alloc.integer(3,marked.integer(2));while(alloc.row()) prior_alloc.push_back(alloc.integer(0));
        drafts[batch_id].push_back({occ,std::move(draft),std::move(decision),marked.integer(4),std::move(prior_alloc)});
        batch_state[batch_id]={info.revision,static_cast<int>(info.state)};
    }
    std::vector<EntryPlan> entries;
    Statement entry_marks(impl_->db,"SELECT batch,ordinal,revision,prior_units,units FROM account_amount_revisions WHERE operation=? AND kind=2 AND position=0 ORDER BY batch,ordinal");entry_marks.text(1,undo_of);
    while(entry_marks.row()) {
        imports::Occurrence occ{entry_marks.bytes(0),static_cast<std::size_t>(entry_marks.integer(1))};
        Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,occ.batch);head.integer(2,integer(occ.ordinal));
        if(!head.row() || head.integer(0)!=entry_marks.integer(2) || entry_marks.integer(2)<=0) fail(SQLITE_CONSTRAINT,later_undo_block);
        auto prior_snapshot=entry_snapshot(occ,entry_marks.integer(2)-1);auto current=entry_snapshot(occ,entry_marks.integer(2));
        if(current.draft.transaction.effective_total().units()!=entry_marks.integer(4) || prior_snapshot.draft.transaction.effective_total().units()!=entry_marks.integer(3)) fail(SQLITE_CONSTRAINT,later_undo_block);
        std::vector<std::int64_t> prior_alloc;for(const auto& allocation:current.draft.transaction.allocations()) prior_alloc.push_back(allocation.amount.units());
        prior_snapshot.revision=current.revision+1;entries.push_back({occ,std::move(prior_snapshot),entry_marks.integer(4),std::move(prior_alloc)});
    }
    std::vector<LinkPlan> links;
    for(const auto& value:all_transfers()) {
        Statement sides(impl_->db,"SELECT position,prior_units,units FROM account_amount_revisions WHERE operation=? AND kind=4 AND batch=? ORDER BY position");sides.text(1,undo_of);sides.text(2,value.link.identity);
        bool origin=false,destination=false;std::int64_t origin_prior=0,destination_prior=0;bool any=false;
        while(sides.row()) {
            any=true;const auto position=sides.integer(0);const auto current_units=position==0?value.link.origin.amount.units():value.link.destination.amount.units();
            if(current_units!=sides.integer(2)) fail(SQLITE_CONSTRAINT,later_undo_block);
            if(position==0){origin=true;origin_prior=value.link.origin.amount.units();}
            else if(position==1){destination=true;destination_prior=value.link.destination.amount.units();}
            else fail(SQLITE_CORRUPT);
        }
        if(!any) continue;
        auto copy=value;guard_money([&]{rescale_link_account(copy.link,acct.identity,restored_currency);});
        links.push_back({std::move(copy),origin,destination,origin_prior,destination_prior});
    }
    const auto before=committed_balance();const auto opening=Money(target.integer(6),restored_currency);
    if(acct.opening.units()!=target.integer(5) || acct.opening_date!=target.bytes(7)) fail(SQLITE_CONSTRAINT,later_undo_block);
    const auto after=guard_money([&]{return before.rescale(restored_currency);});
    if(opening.units()!=after.units() && entries.empty() && links.empty()) {if(opening.units()!=acct.opening.rescale(restored_currency).units()) fail(SQLITE_CONSTRAINT,later_undo_block);} 
    Atomic transaction(impl_->db);CommandFlag flag(impl_->account_command);
    insert_account_op(impl_->db,id,acct.identity,6,acct.name,acct.name,restored_currency.definition_id(),acct.currency.definition_id(),opening.units(),acct.opening.units(),target.bytes(8),acct.opening_date,acct.archived?1:0,acct.archived?1:0,0,undo_of,content,acct.revision+1,acct.frozen?1:0,1);
    Statement update(impl_->db,"UPDATE account SET definition=?,opening=?,opening_date=?,revision=revision+1,confirmed=1 WHERE singleton=?");
    update.text(1,restored_currency.definition_id());update.integer(2,opening.units());update.text(3,target.bytes(8));update.integer(4,acct.identity);update.done();
    for(auto& [batch_id,plans]:drafts) {
        const auto revision=batch_state.at(batch_id).first;
        for(const auto& plan:plans) {
            version(impl_->db,plan.id,revision+1,plan.row,plan.decision,false);
            record_amount(impl_->db,id,0,plan.id.batch,integer(plan.id.ordinal),revision+1,0,plan.prior_total,plan.row.transaction.effective_total().units());
            const auto& allocations=plan.row.transaction.allocations();
            for(std::size_t i=0;i<allocations.size();++i) record_amount(impl_->db,id,1,plan.id.batch,integer(plan.id.ordinal),revision+1,integer(i),plan.prior_alloc.at(i),allocations[i].amount.units());
        }
        advance(impl_->db,batch_id,revision,"currency",batch_state.at(batch_id).second);
    }
    for(const auto& plan:entries) {
        store_entry(impl_->db,plan.id,plan.row);
        record_amount(impl_->db,id,2,plan.id.batch,integer(plan.id.ordinal),plan.row.revision,0,plan.prior_total,plan.row.draft.transaction.effective_total().units());
        const auto& allocations=plan.row.draft.transaction.allocations();
        for(std::size_t i=0;i<allocations.size();++i) record_amount(impl_->db,id,3,plan.id.batch,integer(plan.id.ordinal),plan.row.revision,integer(i),plan.prior_alloc.at(i),allocations[i].amount.units());
    }
    for(const auto& plan:links) {
        if(plan.origin) record_amount(impl_->db,id,4,plan.view.link.identity,0,0,0,plan.origin_prior,plan.view.link.origin.amount.units());
        if(plan.destination) record_amount(impl_->db,id,4,plan.view.link.identity,0,0,1,plan.destination_prior,plan.view.link.destination.amount.units());
    }
    const auto sequence=financial_sequence(impl_->db);
    append_financial(impl_->db,ledger_id,static_cast<app::ChangeKind>(5),{"",static_cast<std::size_t>(acct.identity)},-1,0,before,Money(0,before.currency()),after,id,sequence,catalog_sequence(impl_->db));
    const auto written=financial_sequence(impl_->db);
    for(const auto& plan:links) store_transfer(impl_->db,plan.view,written,false);
    finish([&]{transaction.commit();});return *account_command_row(impl_->db,id);
}
void ImportStore::verify_account_commands(const app::OperationContext* context) const {
    if(context) context->checkpoint("Verifying account commands");
    Statement accounts(impl_->db,"SELECT singleton FROM account ORDER BY singleton");
    while(accounts.row()) {
        const auto id=accounts.integer(0);
        Statement created(impl_->db,"SELECT name,definition FROM account_creations WHERE identity=?");created.integer(1,id);if(!created.row()) fail(SQLITE_CORRUPT);
        std::string name=created.bytes(0),definition=created.bytes(1);int archived=0,retired=0;bool saw_retirement=false;
        std::optional<std::tuple<std::int64_t,std::string,std::string>> last_money;
        Statement ops(impl_->db,"SELECT id,kind,name,prior_name,definition,prior_definition,opening,prior_opening,opening_date,prior_opening_date,archived,prior_archived,retired,undo_of,content,revision,frozen,confirmed FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? ORDER BY h.sequence");
        ops.integer(1,id);
        while(ops.row()) {
            if(context) context->checkpoint();
            const auto op=ops.bytes(0);const auto kind=static_cast<int>(ops.integer(1));
            if(ops.bytes(3)!=name || ops.bytes(5)!=definition || ops.integer(11)!=archived || (kind!=3 && ops.integer(12)!=retired)) fail(SQLITE_CORRUPT,"Account command does not replay");
            const auto content=ops.bytes(14);
            if(kind==0) {if(content!="0\n"+ops.bytes(2) || ops.bytes(4)!=definition) fail(SQLITE_CORRUPT);name=ops.bytes(2);}
            else if(kind==1) {if(content!="1" || archived!=0 || ops.integer(10)!=1) fail(SQLITE_CORRUPT);archived=1;}
            else if(kind==2) {if(content!="2" || archived!=1 || ops.integer(10)!=0) fail(SQLITE_CORRUPT);archived=0;}
            else if(kind==3) {if(content!="3" || saw_retirement || ops.integer(12)!=1) fail(SQLITE_CORRUPT);retired=1;saw_retirement=true;}
            else if(kind==4) {if(content!="4\n"+ops.bytes(8)+"\n"+std::to_string(ops.integer(6)) || ops.bytes(4)!=definition) fail(SQLITE_CORRUPT);last_money=std::tuple{ops.integer(6),ops.bytes(8),definition};}
            else if(kind==5) {if(content!="5\n"+ops.bytes(4)) fail(SQLITE_CORRUPT);definition=ops.bytes(4);last_money=std::tuple{ops.integer(6),ops.bytes(8),definition};
                Statement ledger(impl_->db,"SELECT ordinal,change,undo_of,batch FROM ledger_operations WHERE id=? AND kind=5");ledger.text(1,scale_ledger_id(op));if(!ledger.row() || ledger.integer(0)!=id || ledger.integer(1)!=0 || ledger.bytes(2)!=op || !ledger.bytes(3).empty()) fail(SQLITE_CORRUPT,"Currency command is missing its financial anchor");}
            else if(kind==6) {
                if(content!="6\n"+ops.bytes(13)) fail(SQLITE_CORRUPT);
                Statement undone(impl_->db,"SELECT account,kind FROM account_operations WHERE id=?");undone.text(1,ops.bytes(13));if(!undone.row() || undone.integer(0)!=id || (undone.integer(1)!=4 && undone.integer(1)!=5)) fail(SQLITE_CORRUPT);
                if(undone.integer(1)==5) {definition=ops.bytes(4);Statement ledger(impl_->db,"SELECT ordinal,change,undo_of FROM ledger_operations WHERE id=? AND kind=5");ledger.text(1,scale_ledger_id(op));if(!ledger.row() || ledger.integer(0)!=id || ledger.integer(1)!=0 || ledger.bytes(2)!=op) fail(SQLITE_CORRUPT);} 
                last_money=std::tuple{ops.integer(6),ops.bytes(8),definition};
            } else fail(SQLITE_CORRUPT);
            Statement amounts(impl_->db,"SELECT prior_units,units FROM account_amount_revisions WHERE operation=?");amounts.text(1,op);
            while(amounts.row()) if(guard_money([&]{return Money(amounts.integer(0),Currency::from_definition(ops.bytes(5))).rescale(Currency::from_definition(ops.bytes(4))).units();})!=amounts.integer(1)) fail(SQLITE_CORRUPT,"Rescaled amount history does not replay");
        }
        const auto live=account_by_id(id);
        if(live.name!=name || live.currency.definition_id()!=definition || live.archived!=static_cast<bool>(archived) || account_lifecycle_retired(impl_->db,id)!=static_cast<bool>(retired)) fail(SQLITE_CORRUPT,"Account command does not replay");
        if(last_money && (live.opening.units()!=std::get<0>(*last_money) || live.opening_date!=std::get<1>(*last_money) || live.currency.definition_id()!=std::get<2>(*last_money))) fail(SQLITE_CORRUPT,"Explicit opening does not match its latest command");
    }
    Statement pins(impl_->db,"SELECT 1 FROM batches b WHERE NOT EXISTS(SELECT 1 FROM batch_source_currency p WHERE p.batch=b.id) UNION ALL SELECT 1 FROM source_occurrences o JOIN batches b ON b.id=o.batch JOIN batch_source_currency p ON p.batch=o.batch WHERE b.materialized_revision IS NOT NULL AND o.currency!=p.definition");
    if(pins.row()) fail(SQLITE_CORRUPT,"Source currency pin does not match its occurrence");
}
