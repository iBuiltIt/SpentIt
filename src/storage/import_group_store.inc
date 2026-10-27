namespace {
void append_group_local_amounts(std::vector<Money>& amounts,const std::vector<app::TransferView>& values,std::int64_t account) {
    for(const auto& value:values)for(const auto* side:{&value.link.origin,&value.link.destination})
        if(side->endpoint.account==account && local_side(*side) && side->state!=transfers::State::removed && !side->proposed_evidence)amounts.push_back(side->amount);
}
struct GroupSelection {
    std::int64_t& selected; std::int64_t prior;
    explicit GroupSelection(std::int64_t& value):selected(value),prior(value){}
    ~GroupSelection(){selected=prior;}
};
}
app::ImportGroup ImportStore::import_group(std::string_view id) const {
    Statement group(impl_->db,"SELECT profile,state FROM import_groups WHERE id=?");group.text(1,id);if(!group.row())fail(SQLITE_NOTFOUND,"Import group does not exist");
    app::ImportGroup result{std::string(id),static_cast<app::GroupState>(group.integer(1)),static_cast<qif::DateOrder>(group.integer(0)),{}};
    Statement source(impl_->db,"SELECT bytes FROM sources WHERE id=?");source.text(1,id);if(!source.row())fail(SQLITE_CORRUPT);
    const auto bytes=source.bytes(0);const auto document=indexed_source(impl_->db,id,bytes);
    GroupSelection selection(impl_->selected_account);
    Statement members(impl_->db,"SELECT source_account,account_id,batch,created,balance_choice,balance_revision,opening,opening_date,balance_sequence,reviewed_change FROM import_group_accounts WHERE group_id=? ORDER BY source_account");members.text(1,id);
    while(members.row()) {
        const auto source_account=static_cast<std::size_t>(members.integer(0));const auto account=account_by_id(members.integer(1));impl_->selected_account=account.identity;
        const auto batch_id=members.bytes(2);const auto b=batch(impl_->db,batch_id);
        app::ImportGroupAccount member{source_account,account.identity,batch_id,members.integer(3)!=0,static_cast<app::ReportedBalanceChoice>(members.integer(4)),{},{},members.null(7)?account.opening_date:members.bytes(7)};
        const auto parsed=statements::parse_mapped(bytes,document,source_account,result.profile,account.currency);
        bool ready=!parsed.diagnostic && b.state!=app::BatchState::captured && b.state!=app::BatchState::discarded;
        const bool active_review=b.state!=app::BatchState::discarded;
        std::vector<Money> amounts;
        for(std::size_t i=0;i<parsed.records.size();++i) {
            if(b.state==app::BatchState::protected_drafts && !retained_member(impl_->db,batch_id,i))continue;
            const auto& record=parsed.records[i];
            if(record.diagnostic && record.repairable) {
                ++result.repairs_total;if(record.amount_repair)++result.amount_repairs;
            } else if(record.diagnostic)++result.blocked_records;
            Statement stored(impl_->db,"SELECT 1 FROM records WHERE batch=? AND ordinal=?");stored.text(1,batch_id);stored.integer(2,integer(i));
            if(!stored.row()){if(active_review)++result.unresolved;ready=false;continue;}
            const auto row=load_row({batch_id,i},false);
            if(record.diagnostic && record.repairable)++result.repairs_done;
            if(!row.draft || row.decision.disposition==imports::Disposition::unresolved || (row.review_stale && b.state!=app::BatchState::committed)){if(active_review)++result.unresolved;ready=false;continue;}
            if(row.decision.disposition==imports::Disposition::include)amounts.push_back(row.draft->transaction.effective_total());
        }
        if(parsed.diagnostic){if(active_review)++result.unresolved;++result.blocked_records;}
        append_group_local_amounts(amounts,all_transfers(),account.identity);
        if(ready)member.reviewed_change=imports::sum(account.currency,amounts);
        if(member.created && !account.frozen && b.state!=app::BatchState::discarded) {
            if(members.null(5) || members.null(9) || !member.reviewed_change || members.integer(9)!=member.reviewed_change->units())member.balance_choice=app::ReportedBalanceChoice::unreviewed;
            else if(!members.null(6))member.proposed_opening=Money(members.integer(6),account.currency);
            if(member.balance_choice==app::ReportedBalanceChoice::unreviewed)++result.unresolved;
        }
        result.accounts.push_back(std::move(member));
    }
    if(result.accounts.size()!=document.accounts.size())fail(SQLITE_CORRUPT,"Incomplete source account mapping");
    return result;
}
std::optional<app::ImportGroup> ImportStore::import_group_outcome(std::string_view id) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement found(impl_->db,"SELECT 1 FROM import_groups WHERE id=?");found.text(1,id);if(!found.row())return {};return import_group(id);
}
std::vector<app::ImportGroup> ImportStore::import_groups() const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement ids(impl_->db,"SELECT id FROM import_groups ORDER BY rowid");std::vector<app::ImportGroup> result;
    while(ids.row())result.push_back(import_group(ids.bytes(0)));return result;
}
app::ImportGroup ImportStore::prepare_import_group(std::string_view id,std::string_view bytes,qif::DateOrder profile,
    const std::vector<app::ImportAccountMapping>& mappings,app::OperationContext& context,std::string_view label) {
    identifier(id);if(id.size()>108)fail(SQLITE_MISUSE,"Import group identity is too long");context.checkpoint("Indexing source account sections");
    const auto document=statements::inspect(bytes,label,[&]{context.checkpoint();});
    if(!document.admissible)fail(SQLITE_TOOBIG,"Import exceeds resource limits");
    if(!document.refusal.empty() || document.diagnostic)fail(SQLITE_CONSTRAINT,document.refusal.empty()?"Unsupported account section structure; source file remains unchanged":document.refusal.c_str());
    if(document.format=="ofx" || document.format=="qfx" || document.format=="qbo" || document.format=="aso")profile=qif::DateOrder::dmy;
    if(mappings.size()!=document.accounts.size() || mappings.empty())fail(SQLITE_CONSTRAINT,"Every source account requires an explicit mapping");
    std::set<std::size_t> source_ids;std::set<std::int64_t> target_ids;
    for(const auto& mapping:mappings) {
        if(mapping.source_account>=document.accounts.size() || !source_ids.insert(mapping.source_account).second || mapping.account<=0 || !target_ids.insert(mapping.account).second)
            fail(SQLITE_CONSTRAINT,"Map each source account once to a distinct account");
        if(mapping.creation) {
            const auto& c=*mapping.creation;safe_text(c.name);iso_date(c.date);
            if(c.identity!=mapping.account || c.name.empty() || Currency(c.currency.code())!=c.currency ||
               (document.explicit_accounts && c.name!=document.accounts[mapping.source_account].name))fail(SQLITE_CONSTRAINT,"Invalid explicit account creation");
        }
    }
    Atomic transaction(impl_->db);
    Statement prior(impl_->db,"SELECT g.profile,s.bytes,h.format FROM import_groups g JOIN sources s ON s.id=g.id JOIN source_headers h ON h.source_id=g.id WHERE g.id=?");prior.text(1,id);
    if(prior.row()) {
        if(prior.integer(0)!=static_cast<int>(profile) || prior.bytes(1)!=bytes || prior.bytes(2)!=document.format)fail(SQLITE_CONSTRAINT,"Import identity was reused with different content");
        const auto saved=import_group(id);
        for(const auto& mapping:mappings) {
            const auto found=std::find_if(saved.accounts.begin(),saved.accounts.end(),[&](const auto& a){return a.source_account==mapping.source_account;});
            if(found==saved.accounts.end() || found->account!=mapping.account || found->created!=mapping.creation.has_value())fail(SQLITE_CONSTRAINT,"Import mapping changed on retry");
            if(mapping.creation) {const auto a=account_by_id(mapping.account);Statement creation(impl_->db,"SELECT opening_date FROM account_creations WHERE identity=?");creation.integer(1,mapping.account);if(!creation.row())fail(SQLITE_CORRUPT);if(a.name!=mapping.creation->name || a.currency!=mapping.creation->currency || creation.bytes(0)!=mapping.creation->date)fail(SQLITE_CONSTRAINT,"Import account changed on retry");}
        }
        context.seal();transaction.commit();return saved;
    }
    if(std::none_of(mappings.begin(),mappings.end(),[](const auto& m){return m.creation.has_value();})) {
        Statement pending(impl_->db,"SELECT g.id FROM import_groups g JOIN sources s ON s.id=g.id JOIN source_headers h ON h.source_id=g.id WHERE g.profile=? AND s.bytes=? AND h.format=? AND g.state!=3 AND EXISTS(SELECT 1 FROM import_group_accounts m JOIN batches b ON b.id=m.batch WHERE m.group_id=g.id AND b.state IN(0,3)) ORDER BY g.rowid");pending.integer(1,static_cast<int>(profile));pending.blob(2,bytes);pending.text(3,document.format);
        while(pending.row()) {
            const auto existing=import_group(pending.bytes(0));if(existing.accounts.size()!=mappings.size())continue;
            bool matches=true;for(const auto& mapping:mappings)if(std::none_of(existing.accounts.begin(),existing.accounts.end(),[&](const auto& m){return m.source_account==mapping.source_account && m.account==mapping.account;})){matches=false;break;}
            if(matches){context.seal();transaction.commit();return existing;}
        }
    }
    if(document.format!="qif")for(const auto& mapping:mappings) {
        const auto& code=document.accounts[mapping.source_account].currency_code;
        const auto& target=mapping.creation?mapping.creation->currency:account_by_id(mapping.account).currency;
        if(code!=target.code()) {const auto message="Statement currency "+code+" does not match the target account; no conversion was applied";fail(SQLITE_CONSTRAINT,message.c_str());}
    }
    Statement source(impl_->db,"INSERT INTO sources VALUES(?,?)");source.text(1,id);source.blob(2,bytes);source.done();
    retain_source_header(impl_->db,id,document.format,document.grammar,source_header_text(document));
    Statement group(impl_->db,"INSERT INTO import_groups(id,profile,state) VALUES(?,?,0)");group.text(1,id);group.integer(2,static_cast<int>(profile));group.done();
    GroupSelection selection(impl_->selected_account);
    for(const auto& mapping:mappings) {
        context.checkpoint("Retaining mapped source accounts");
        if(mapping.creation) {
            const auto& c=*mapping.creation;
            Statement a(impl_->db,"INSERT INTO account VALUES(?,?,?,?,0,0,0,0)");a.integer(1,c.identity);a.text(2,c.name);a.text(3,c.currency.definition_id());a.text(4,c.date);a.done();
            Statement receipt(impl_->db,"INSERT INTO account_creations VALUES(?,?,?,?)");receipt.integer(1,c.identity);receipt.text(2,c.name);receipt.text(3,c.currency.definition_id());receipt.text(4,c.date);receipt.done();
        }
        const auto account=account_by_id(mapping.account);impl_->selected_account=account.identity;
        Statement retired(impl_->db,"SELECT 1 FROM retired_import_accounts WHERE account_id=?");retired.integer(1,account.identity);if(retired.row())fail(SQLITE_CONSTRAINT,"Removed account cannot receive imports");
        const auto parsed=statements::parse_mapped(bytes,document,mapping.source_account,profile,account.currency,[&]{context.checkpoint();});
        if(!parsed.admissible)fail(SQLITE_TOOBIG,"Import exceeds resource limits");
        if(parsed.diagnostic && parsed.diagnostic->issue==qif::Issue::currency_mismatch && parsed.records.empty()) {const auto message="Statement currency "+document.accounts[mapping.source_account].currency_code+" does not match the target account; no conversion was applied";fail(SQLITE_CONSTRAINT,message.c_str());}
        const auto batch_id=std::string(id)+"-account-"+std::to_string(mapping.source_account);identifier(batch_id);
        Statement b(impl_->db,"INSERT INTO batches VALUES(?,?,?,3,0,?,1,NULL,?,?,?)");b.text(1,batch_id);b.integer(2,static_cast<int>(profile));b.text(3,document.parser);b.integer(4,integer(parsed.records.size()));b.integer(5,account.identity);b.text(6,id);b.integer(7,integer(mapping.source_account));b.done();
        Statement member(impl_->db,"INSERT INTO import_group_accounts(group_id,source_account,account_id,batch,created) VALUES(?,?,?,?,?)");member.text(1,id);member.integer(2,integer(mapping.source_account));member.integer(3,account.identity);member.text(4,batch_id);member.integer(5,mapping.creation?1:0);member.done();
        Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,0,'capture')");event.text(1,batch_id);event.done();
    }
    context.seal();impl_->finish(transaction);return import_group(id);
}
app::ImportGroup ImportStore::resume_import_group(std::string_view id,app::OperationContext& context) {
    auto group=import_group(id);if(group.state!=app::GroupState::captured){context.seal();return group;}
    GroupSelection selection(impl_->selected_account);Atomic transaction(impl_->db);
    for(const auto& member:group.accounts) {
        impl_->selected_account=member.account;const auto b=batch(impl_->db,member.batch);
        if(b.state!=app::BatchState::captured || b.revision==INT64_MAX)fail(SQLITE_CONSTRAINT,"Import group changed before resuming");
        context.checkpoint("Preparing all source account drafts");
        const auto bytes=evidence(member.batch);const auto parsed=parsed_source(member.batch,[&]{context.checkpoint();});
        if(!parsed.admissible || parsed.records.size()!=b.count)fail(SQLITE_CORRUPT);
        Statement update(impl_->db,"UPDATE batches SET state=0,revision=revision+1,blocked=?,materialized_revision=revision+1 WHERE id=?");update.integer(1,parsed.diagnostic?1:(!parsed.records.empty() && !parsed.valid()?1:0));update.text(2,member.batch);update.done();
        Statement event(impl_->db,"INSERT INTO events(batch,revision,kind) VALUES(?,?,'materialize')");event.text(1,member.batch);event.integer(2,b.revision+1);event.done();
        populate(member.batch,bytes,parsed,b.revision+1,&context);
    }
    Statement update(impl_->db,"UPDATE import_groups SET state=1 WHERE id=? AND state=0");update.text(1,id);update.done();
    context.seal();impl_->finish(transaction);return import_group(id);
}
void ImportStore::review_import_balance(std::string_view id,std::size_t source_account,std::int64_t expected,app::ReportedBalanceChoice choice,std::string_view date) {
    iso_date(date);auto group=import_group(id);
    if(group.state!=app::GroupState::reviewing && group.state!=app::GroupState::started)fail(SQLITE_CONSTRAINT,"Balance review requires an uncommitted group");
    const auto found=std::find_if(group.accounts.begin(),group.accounts.end(),[&](const auto& a){return a.source_account==source_account;});
    if(found==group.accounts.end() || !found->created || !found->reviewed_change)fail(SQLITE_CONSTRAINT,"Finish this new account's repairs and duplicate review first");
    GroupSelection selection(impl_->selected_account);impl_->selected_account=found->account;
    const auto a=account();const auto b=batch(impl_->db,found->batch);if(b.revision!=expected || a.frozen)fail(SQLITE_CONSTRAINT,"Balance preview changed");
    const auto doc=indexed_source(impl_->db,id,evidence(found->batch));if(source_account>=doc.accounts.size())fail(SQLITE_CORRUPT,"Incomplete source account mapping");const auto& reported=doc.accounts[source_account].reported_balance;
    Money opening(0,a.currency);
    if(choice==app::ReportedBalanceChoice::opening || choice==app::ReportedBalanceChoice::closing) {
        if(!reported)fail(SQLITE_CONSTRAINT,"No reported balance is present");
        opening=Money::parse(*reported,a.currency);if(choice==app::ReportedBalanceChoice::closing)opening=opening-*found->reviewed_change;
    } else if(choice==app::ReportedBalanceChoice::zero) {
        if(reported && Money::parse(*reported,a.currency)!=*found->reviewed_change)fail(SQLITE_CONSTRAINT,"Reported balance differs; choose opening, closing, or ignore explicitly");
    } else if(choice==app::ReportedBalanceChoice::ignore)opening=a.opening;
    else fail(SQLITE_CONSTRAINT,"Choose how to interpret the reported balance");
    Statement same(impl_->db,"SELECT 1 FROM import_group_accounts WHERE group_id=? AND source_account=? AND balance_choice=? AND balance_revision=? AND opening=? AND opening_date=? AND reviewed_change=?");same.text(1,id);same.integer(2,integer(source_account));same.integer(3,static_cast<int>(choice));same.integer(4,expected);same.integer(5,opening.units());same.text(6,date);same.integer(7,found->reviewed_change->units());if(same.row())return;
    Atomic transaction(impl_->db);
    Statement update(impl_->db,"UPDATE import_group_accounts SET balance_choice=?,balance_revision=?,opening=?,opening_date=?,balance_sequence=?7,reviewed_change=?8 WHERE group_id=?5 AND source_account=?6");update.integer(1,static_cast<int>(choice));update.integer(2,expected);update.integer(3,opening.units());update.text(4,date);update.text(5,id);update.integer(6,integer(source_account));update.integer(7,transfer_sequence());update.integer(8,found->reviewed_change->units());update.done();impl_->finish(transaction);
}
void ImportStore::abort_import_group(std::string_view id) {
    const auto group=import_group(id);if(group.state==app::GroupState::aborted){Statement retired(impl_->db,"SELECT 1 FROM retired_import_accounts WHERE account_id=spentit_account()");if(retired.row())impl_->selected_account=1;return;}
    if(group.state==app::GroupState::started)fail(SQLITE_CONSTRAINT,"This import has committed effects; whole-import Abort is no longer available");
    GroupSelection selection(impl_->selected_account);Atomic transaction(impl_->db);
    for(const auto& member:group.accounts) {
        impl_->selected_account=member.account;const auto b=batch(impl_->db,member.batch);
        if(b.state!=app::BatchState::pending && b.state!=app::BatchState::captured)fail(SQLITE_CONSTRAINT,"Import group has committed or protected effects");
        if(!protected_import_rows(member.batch).empty())fail(SQLITE_CONSTRAINT,"Import group has protected effects");
        (void)transfer_import_transition(member.batch,"group-abort-"+member.batch+"-"+std::to_string(b.revision),false);
        advance(impl_->db,member.batch,b.revision,"discard",2);
    }
    for(const auto& member:group.accounts)if(member.created) {
        Statement used(impl_->db,"SELECT 1 FROM batches WHERE account_id=? AND source_id!=? AND state!=2");used.integer(1,member.account);used.text(2,id);
        bool unused=!used.row() && !account_by_id(member.account).frozen;
        for(const auto& transfer:all_transfers())if((transfer.link.origin.endpoint.account==member.account || transfer.link.destination.endpoint.account==member.account) && (transfer.link.origin.state!=transfers::State::removed || transfer.link.destination.state!=transfers::State::removed))unused=false;
        if(unused){Statement retired(impl_->db,"INSERT INTO retired_import_accounts VALUES(?,?)");retired.integer(1,member.account);retired.text(2,id);retired.done();}
    }
    Statement update(impl_->db,"UPDATE import_groups SET state=3 WHERE id=?");update.text(1,id);update.done();impl_->finish(transaction);
    Statement retired(impl_->db,"SELECT 1 FROM retired_import_accounts WHERE account_id=?");retired.integer(1,selection.prior);if(retired.row())selection.prior=1;
}
void ImportStore::require_group_ready(std::string_view batch_id) const {
    Statement member(impl_->db,"SELECT group_id FROM import_group_accounts WHERE batch=?");member.text(1,batch_id);if(!member.row()) {
        Statement source(impl_->db,"SELECT state,blocked FROM batches WHERE id=?");source.text(1,batch_id);if(!source.row() || source.integer(0)==1 || source.integer(0)==2)return;
        if(source.integer(0)==3 || source.integer(1)!=0)fail(SQLITE_CONSTRAINT,"Finish all repairs in the source import before committing or protecting any of its records");
        Statement unresolved(impl_->db,"SELECT 1 FROM versions v WHERE batch=? AND decision=0 AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) LIMIT 1");unresolved.text(1,batch_id);if(unresolved.row())fail(SQLITE_CONSTRAINT,"Finish duplicate review for the source import before protecting its records");
        return;
    }
    const auto group=import_group(member.bytes(0));
    if((group.state!=app::GroupState::reviewing && group.state!=app::GroupState::started) || group.unresolved!=0)
        fail(SQLITE_CONSTRAINT,"Finish all source-account repairs, duplicate review and new-account balance decisions before committing or protecting this import");
}
void ImportStore::start_import_groups(const std::set<std::string>& batches,std::string_view operation) {
    for(const auto& batch_id:batches) {
        require_group_ready(batch_id);
        Statement group(impl_->db,"UPDATE import_groups SET state=2,publication=?2 WHERE state=1 AND id IN(SELECT group_id FROM import_group_accounts WHERE batch=?1)");group.text(1,batch_id);group.text(2,operation);group.done();
    }
}
void ImportStore::publish_group_opening(std::int64_t account_id) {
    const auto a=account_by_id(account_id);if(a.frozen)return;
    Statement staged(impl_->db,"SELECT m.batch,m.balance_revision,m.opening,m.opening_date FROM import_group_accounts m JOIN import_groups g ON g.id=m.group_id WHERE m.account_id=? AND m.created=1 AND g.state IN(1,2)");staged.integer(1,account_id);if(!staged.row())return;
    require_group_ready(staged.bytes(0));
    if(staged.null(1) || staged.null(2) || a.revision==INT64_MAX)fail(SQLITE_CONSTRAINT,"Confirm the current opening balance proposal");
    Statement update(impl_->db,"UPDATE account SET opening=?,opening_date=?,revision=revision+1,confirmed=1 WHERE singleton=?");update.integer(1,staged.integer(2));update.text(2,staged.bytes(3));update.integer(3,account_id);update.done();
}
void ImportStore::verify_import_groups(const app::OperationContext* context) const {
    Statement invalid(impl_->db,"SELECT 1 FROM batches b LEFT JOIN import_group_accounts m ON m.batch=b.id WHERE (b.source_account IS NULL)!=(m.batch IS NULL) OR (m.batch IS NOT NULL AND (b.source_id!=m.group_id OR b.source_account!=m.source_account OR b.account_id!=m.account_id)) UNION ALL SELECT 1 FROM retired_import_accounts r JOIN import_groups g ON g.id=r.group_id JOIN account a ON a.singleton=r.account_id WHERE g.state!=3 OR a.frozen!=0 OR NOT EXISTS(SELECT 1 FROM import_group_accounts m WHERE m.group_id=r.group_id AND m.account_id=r.account_id AND m.created=1)");
    if(invalid.row())fail(SQLITE_CORRUPT,"Invalid grouped import relationships");
    for(const auto& group:import_groups()) {
        if(context)context->checkpoint("Verifying grouped source review");
        Statement states(impl_->db,"SELECT state FROM import_group_events WHERE group_id=? ORDER BY sequence");states.text(1,group.id);
        int prior=-1;while(states.row()){const auto next=static_cast<int>(states.integer(0));if(!((prior==-1 && next==0) || (prior==0 && (next==1 || next==3)) || (prior==1 && (next==2 || next==3))))fail(SQLITE_CORRUPT,"Invalid group transition history");prior=next;}
        if(prior!=static_cast<int>(group.state))fail(SQLITE_CORRUPT,"Group history does not replay");

        for(const auto& member:group.accounts) {
            const auto b=batch(impl_->db,member.batch);
            const auto account=account_by_id(member.account);
            Statement source(impl_->db,"SELECT bytes FROM sources WHERE id=?");source.text(1,group.id);if(!source.row())fail(SQLITE_CORRUPT);
            const auto doc=indexed_source(impl_->db,group.id,source.bytes(0));if(member.source_account>=doc.accounts.size())fail(SQLITE_CORRUPT,"Incomplete source account mapping");const auto& reported=doc.accounts[member.source_account].reported_balance;
            Statement decisions(impl_->db,"SELECT balance_choice,balance_revision,opening,opening_date,balance_sequence,reviewed_change FROM import_balance_events WHERE group_id=? AND source_account=? ORDER BY sequence");decisions.text(1,group.id);decisions.integer(2,integer(member.source_account));
            std::int64_t last_revision=-1,last_choice=0,last_opening=0,last_sequence=-1,last_change=0;std::string last_date;
            while(decisions.row()) {
                if(!member.created || decisions.integer(1)>b.revision || decisions.integer(1)<0)fail(SQLITE_CORRUPT,"Invalid reported balance decision");
                iso_date(decisions.bytes(3));std::vector<Money> amounts;std::size_t count=0;
                Statement rows(impl_->db,"SELECT total,decision FROM versions v WHERE batch=? AND revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal AND revision<=?)");rows.text(1,member.batch);rows.integer(2,decisions.integer(1));
                while(rows.row()){++count;if(rows.integer(1)==0)fail(SQLITE_CORRUPT,"Unreviewed balance history");if(rows.integer(1)==1)amounts.emplace_back(rows.integer(0),account.currency);}
                if(count!=b.count)fail(SQLITE_CORRUPT,"Incomplete balance history");
                if(decisions.integer(4)<0 || decisions.integer(4)>transfer_sequence())fail(SQLITE_CORRUPT);
                std::vector<app::TransferView> views;Statement transfers(impl_->db,"SELECT payload FROM transfer_versions v WHERE sequence=(SELECT max(sequence) FROM transfer_versions WHERE identity=v.identity AND sequence<=?)");transfers.integer(1,decisions.integer(4));while(transfers.row())views.push_back(transfer_codec::view(transfers.bytes(0)));
                append_group_local_amounts(amounts,views,account.identity);
                const auto change=imports::sum(account.currency,amounts);if(change.units()!=decisions.integer(5))fail(SQLITE_CORRUPT,"Reviewed balance change does not replay");Money expected(0,account.currency);
                const auto choice=static_cast<app::ReportedBalanceChoice>(decisions.integer(0));
                if(choice==app::ReportedBalanceChoice::opening || choice==app::ReportedBalanceChoice::closing) {
                    if(!reported)fail(SQLITE_CORRUPT);expected=Money::parse(*reported,account.currency);if(choice==app::ReportedBalanceChoice::closing)expected=expected-change;
                } else if(choice==app::ReportedBalanceChoice::zero) {
                    if(reported && Money::parse(*reported,account.currency)!=change)fail(SQLITE_CORRUPT,"Invalid zero-opening comparison");
                } else if(choice!=app::ReportedBalanceChoice::ignore)fail(SQLITE_CORRUPT);
                if(expected.units()!=decisions.integer(2))fail(SQLITE_CORRUPT,"Reported balance decision does not replay");
                last_choice=decisions.integer(0);last_revision=decisions.integer(1);last_opening=decisions.integer(2);last_date=decisions.bytes(3);last_sequence=decisions.integer(4);last_change=decisions.integer(5);
            }
            Statement chosen(impl_->db,"SELECT balance_choice,balance_revision,opening,opening_date,balance_sequence,reviewed_change FROM import_group_accounts WHERE batch=?");chosen.text(1,member.batch);if(!chosen.row())fail(SQLITE_CORRUPT);
            if(last_revision<0){if(chosen.integer(0)!=0 || !chosen.null(1) || !chosen.null(2) || !chosen.null(3) || !chosen.null(4) || !chosen.null(5))fail(SQLITE_CORRUPT);}
            else if(chosen.integer(0)!=last_choice || chosen.integer(1)!=last_revision || chosen.integer(2)!=last_opening || chosen.bytes(3)!=last_date || chosen.integer(4)!=last_sequence || chosen.integer(5)!=last_change)fail(SQLITE_CORRUPT,"Balance choice history differs from current proposal");
            if(member.created && account.frozen && (account.opening.units()!=last_opening || account.opening_date!=last_date))fail(SQLITE_CORRUPT,"Published opening differs from reviewed proposal");
            if((group.state==app::GroupState::captured && b.state!=app::BatchState::captured) ||
               (group.state==app::GroupState::aborted && b.state!=app::BatchState::discarded) ||
               (group.state==app::GroupState::reviewing && b.state!=app::BatchState::pending))fail(SQLITE_CORRUPT,"Invalid grouped import state");
        }
    }
}
std::set<std::string> ImportStore::financial_group_batches(std::int64_t account_id) const {
    std::set<std::string> result;
    Statement own(impl_->db,"SELECT m.batch FROM import_group_accounts m JOIN batches b ON b.id=m.batch WHERE m.account_id=? AND b.state IN(0,3,4)");own.integer(1,account_id);while(own.row())result.insert(own.bytes(0));
    for(const auto& value:all_transfers()) {
        const auto& link=value.link;
        if(link.origin.endpoint.account!=account_id && link.destination.endpoint.account!=account_id)continue;
        for(const auto* side:{&link.origin,&link.destination}) {
            if(side->state==transfers::State::draft)result.insert(side->endpoint.entry.batch);
            if(side->proposed_evidence)result.insert(side->proposed_evidence->endpoint.entry.batch);
        }
    }
    return result;
}
