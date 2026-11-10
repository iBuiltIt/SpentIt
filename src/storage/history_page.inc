// Change history reads the live applied action chain. It does not create a journal.
namespace {
constexpr const char* member_where="(m.answer=?1 OR m.answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1))";
struct HistorySnap {
    bool present=false,active=true,has_active=false;
    std::string date,payee,memo,definition;
    std::int64_t total=0;
    struct Split {std::string category,memo;std::int64_t units=0;};
    std::vector<Split> splits;
};
bool same_field(const app::HistoryFieldChange& left,const app::HistoryFieldChange& right) {
    return left.field==right.field && left.kind==right.kind && left.named_only==right.named_only && left.before_text==right.before_text && left.after_text==right.after_text && left.before_money.units==right.before_money.units && left.before_money.definition==right.before_money.definition && left.after_money.units==right.after_money.units && left.after_money.definition==right.after_money.definition;
}
bool same_fields(const std::vector<app::HistoryFieldChange>& left,const std::vector<app::HistoryFieldChange>& right) {
    if(left.size()!=right.size())return false;
    for(std::size_t i=0;i<left.size();++i)if(!same_field(left[i],right[i]))return false;
    return true;
}
void add_text(std::vector<app::HistoryFieldChange>& out,std::string field,app::HistoryFieldKind kind,std::string before,std::string after) {
    if(before==after)return;
    app::HistoryFieldChange change;change.field=std::move(field);change.kind=kind;change.before_text=std::move(before);change.after_text=std::move(after);out.push_back(std::move(change));
}
void add_money(std::vector<app::HistoryFieldChange>& out,std::string field,std::int64_t before,std::string before_definition,std::int64_t after,std::string after_definition) {
    if(before==after && before_definition==after_definition)return;
    app::HistoryFieldChange change;change.field=std::move(field);change.kind=app::HistoryFieldKind::money;
    change.before_money={before,std::move(before_definition)};change.after_money={after,std::move(after_definition)};out.push_back(std::move(change));
}
std::vector<std::int64_t> sorted_unique(std::vector<std::int64_t> ids) {
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());return ids;
}
}
std::string ImportStore::action_display_label(const app::ActionSummary& action) const {
    Statement saved(impl_->db,"SELECT question FROM review_answers WHERE id=?");saved.text(1,action.id);
    if(!saved.row() || saved.bytes(0)!="ledger-bulk")return action.label;
    Statement bulk(impl_->db,"SELECT kind FROM bulk_operations WHERE id=?");bulk.text(1,action.id);
    if(!bulk.row())return action.label;
    if(bulk.integer(0)==static_cast<int>(app::ChangeKind::correct))return "Change category";
    if(bulk.integer(0)==static_cast<int>(app::ChangeKind::remove))return "Delete transactions";
    return action.label;
}
std::vector<app::HistoryFieldChange> ImportStore::history_named_fields(const app::ActionSummary& action) const {
    auto differs=[&](const char* sql){Statement query(impl_->db,sql);query.text(1,action.id);return query.row() && query.integer(0)!=0;};
    const std::string where=std::string("SELECT 1 FROM review_members m WHERE ")+member_where+" AND ";
    std::vector<app::HistoryFieldChange> names;
    auto add=[&](const char* field,const char* predicate){if(!differs((where+predicate+" LIMIT 1").c_str()))return;app::HistoryFieldChange change;change.field=field;change.named_only=true;names.push_back(std::move(change));};
    add("Date","((m.committed=0 AND EXISTS(SELECT 1 FROM versions a JOIN versions b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.date!=b.date)) OR (m.committed=1 AND EXISTS(SELECT 1 FROM entry_values a JOIN entry_values b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.date!=b.date)))");
    add("Payee","((m.committed=0 AND EXISTS(SELECT 1 FROM versions a JOIN versions b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.payee!=b.payee)) OR (m.committed=1 AND EXISTS(SELECT 1 FROM entry_values a JOIN entry_values b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.payee!=b.payee)))");
    add("Memo","((m.committed=0 AND EXISTS(SELECT 1 FROM versions a JOIN versions b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.memo!=b.memo)) OR (m.committed=1 AND EXISTS(SELECT 1 FROM entry_values a JOIN entry_values b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.memo!=b.memo)))");
    add("Amount","((m.committed=0 AND EXISTS(SELECT 1 FROM versions a JOIN versions b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.total!=b.total)) OR (m.committed=1 AND EXISTS(SELECT 1 FROM entry_values a JOIN entry_values b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.total!=b.total)))");
    add("Status","(m.committed=1 AND EXISTS(SELECT 1 FROM entry_values a JOIN entry_values b ON a.batch=m.batch AND b.batch=m.batch AND a.ordinal=m.ordinal AND b.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.active!=b.active))");
    const char* split="((m.committed=0 AND ((SELECT count(*) FROM allocations WHERE batch=m.batch AND ordinal=m.ordinal AND revision=m.prior)!=(SELECT count(*) FROM allocations WHERE batch=m.batch AND ordinal=m.ordinal AND revision=m.result) OR EXISTS(SELECT 1 FROM allocations a JOIN allocations b ON a.batch=b.batch AND a.ordinal=b.ordinal AND a.position=b.position WHERE a.batch=m.batch AND a.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.COLUMN!=b.COLUMN))) OR (m.committed=1 AND ((SELECT count(*) FROM entry_allocation_values WHERE batch=m.batch AND ordinal=m.ordinal AND revision=m.prior)!=(SELECT count(*) FROM entry_allocation_values WHERE batch=m.batch AND ordinal=m.ordinal AND revision=m.result) OR EXISTS(SELECT 1 FROM entry_allocation_values a JOIN entry_allocation_values b ON a.batch=b.batch AND a.ordinal=b.ordinal AND a.position=b.position WHERE a.batch=m.batch AND a.ordinal=m.ordinal AND a.revision=m.prior AND b.revision=m.result AND a.COLUMN!=b.COLUMN))))";
    auto split_sql=[&](const char* column){std::string sql=split;for(std::string::size_type at=0;(at=sql.find("COLUMN",at))!=std::string::npos;at+=std::char_traits<char>::length(column))sql.replace(at,6,column);return sql;};
    const auto category=split_sql("category"),memo=split_sql("memo"),units=split_sql("units");
    add("Category",category.c_str());add("Split memo",memo.c_str());add("Split amount",units.c_str());
    return names;
}
bool ImportStore::history_touches(const app::ActionSummary& action,std::int64_t account) const {
    auto hit=[&](const char* sql,std::string_view id){Statement query(impl_->db,sql);query.text(1,id);query.integer(2,account);return query.row();};
    switch(action.kind) {
    case app::ActionKind::opening: return hit("SELECT 1 FROM account_operations WHERE id=? AND account=? AND kind=4",action.payload);
    case app::ActionKind::currency: return hit("SELECT 1 FROM account_operations WHERE id=? AND account=? AND kind=5",action.payload);
    case app::ActionKind::import: return hit("SELECT 1 FROM batches WHERE source_id=? AND account_id=?",action.payload);
    case app::ActionKind::manual_creation: return hit("SELECT 1 FROM batches WHERE id=? AND account_id=?",action.payload);
    case app::ActionKind::acceptance: return hit("SELECT 1 FROM operations o JOIN batches b ON b.id=o.batch WHERE (o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1)) AND b.account_id=?2",action.payload);
    default: return hit((std::string("SELECT 1 FROM review_members m JOIN batches b ON b.id=m.batch WHERE ")+member_where+" AND b.account_id=?2 UNION SELECT 1 FROM review_repairs r JOIN batches b ON b.id=r.batch WHERE r.answer=?1 AND b.account_id=?2").c_str(),action.id);
    }
}
bool ImportStore::history_includes(const app::ActionSummary& action,const imports::Occurrence& id) const {
    switch(action.kind) {
    case app::ActionKind::opening: return false;
    case app::ActionKind::manual_creation: return action.payload==id.batch && id.ordinal==0;
    case app::ActionKind::import: {
        Statement query(impl_->db,"SELECT 1 FROM batches b JOIN records r ON r.batch=b.id AND r.ordinal=?3 WHERE b.source_id=?1 AND b.id=?2");
        query.text(1,action.payload);query.text(2,id.batch);query.integer(3,integer(id.ordinal));return query.row();
    }
    case app::ActionKind::acceptance: {
        Statement query(impl_->db,"SELECT 1 FROM acceptance_members m JOIN operations o ON o.id=m.operation WHERE m.ordinal=?2 AND m.decision=1 AND o.batch=?3 AND (o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1))");
        query.text(1,action.payload);query.integer(2,integer(id.ordinal));query.text(3,id.batch);return query.row();
    }
    case app::ActionKind::currency: {
        Statement query(impl_->db,"SELECT 1 FROM account_amount_revisions WHERE operation=? AND kind IN(0,2) AND position=0 AND batch=? AND ordinal=?");
        query.text(1,action.payload);query.text(2,id.batch);query.integer(3,integer(id.ordinal));return query.row();
    }
    default: {
        Statement query(impl_->db,(std::string("SELECT 1 FROM review_members m WHERE ")+member_where+" AND m.batch=?2 AND m.ordinal=?3 UNION SELECT 1 FROM review_repairs r WHERE r.answer=?1 AND r.batch=?2 AND r.ordinal=?3").c_str());
        query.text(1,action.id);query.text(2,id.batch);query.integer(3,integer(id.ordinal));return query.row();
    }
    }
}
std::set<imports::Occurrence> ImportStore::history_occurrences(const app::ActionSummary& action) const {
    std::set<imports::Occurrence> ids;
    auto add=[&](std::string batch,std::int64_t ordinal){if(batch.empty() || ordinal<0)return;ids.insert({std::move(batch),static_cast<std::size_t>(ordinal)});};
    auto link_ends=[&](std::string_view answer){
        Statement rows(impl_->db,"SELECT prior_payload,result_payload FROM review_link_history WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");rows.text(1,answer);
        while(rows.row())for(int column:{0,1})if(!rows.null(column)){const auto value=transfer_codec::view(rows.bytes(column));add(value.link.origin.endpoint.entry.batch,integer(value.link.origin.endpoint.entry.ordinal));add(value.link.destination.endpoint.entry.batch,integer(value.link.destination.endpoint.entry.ordinal));}
    };
    switch(action.kind) {
    case app::ActionKind::opening: return {};
    case app::ActionKind::manual_creation: add(action.payload,0);return ids;
    case app::ActionKind::import: {
        Statement query(impl_->db,"SELECT b.id,r.ordinal FROM batches b JOIN records r ON r.batch=b.id WHERE b.source_id=?");query.text(1,action.payload);while(query.row())add(query.bytes(0),query.integer(1));return ids;
    }
    case app::ActionKind::acceptance: {
        Statement query(impl_->db,"SELECT o.batch,m.ordinal FROM acceptance_members m JOIN operations o ON o.id=m.operation WHERE m.decision=1 AND (o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1))");query.text(1,action.payload);while(query.row())add(query.bytes(0),query.integer(1));
        link_ends(action.id);return ids;
    }
    case app::ActionKind::currency: {
        Statement query(impl_->db,"SELECT batch,ordinal FROM account_amount_revisions WHERE operation=? AND kind IN(0,2) AND position=0");query.text(1,action.payload);while(query.row())add(query.bytes(0),query.integer(1));
        Statement sides(impl_->db,"SELECT v.payload FROM transfer_versions v JOIN ledger_operations l ON l.sequence=v.sequence WHERE l.kind=5 AND l.undo_of=?");sides.text(1,action.payload);
        while(sides.row()){const auto value=transfer_codec::view(sides.bytes(0));add(value.link.origin.endpoint.entry.batch,integer(value.link.origin.endpoint.entry.ordinal));add(value.link.destination.endpoint.entry.batch,integer(value.link.destination.endpoint.entry.ordinal));}
        return ids;
    }
    default: {
        Statement query(impl_->db,(std::string("SELECT m.batch,m.ordinal FROM review_members m WHERE ")+member_where+" UNION SELECT r.batch,r.ordinal FROM review_repairs r WHERE r.answer=?1").c_str());query.text(1,action.id);while(query.row())add(query.bytes(0),query.integer(1));
        link_ends(action.id);return ids;
    }
    }
}
app::HistoryActionView ImportStore::history_view(const app::ActionSummary& action,std::int64_t account,bool with_balance,bool named_only) const {
    app::HistoryActionView view;view.id=action.id;view.sequence=action.sequence;view.kind=action.kind;view.label=action_display_label(action);view.count=action.count;
    if(named_only && (action.kind==app::ActionKind::transaction || action.kind==app::ActionKind::rule_application || action.kind==app::ActionKind::review)){view.changes=history_named_fields(action);return view;}
    Statement question_row(impl_->db,"SELECT question FROM review_answers WHERE id=?");question_row.text(1,action.id);
    const auto question=question_row.row()?question_row.bytes(0):std::string{};
    auto names_of=[&](const std::vector<std::int64_t>& ids){std::vector<std::string> names;for(const auto id:ids)names.push_back(account_by_id(id).name);return names;};
    auto link_accounts=[&](std::vector<std::int64_t>& ids){
        Statement links(impl_->db,"SELECT prior_payload,result_payload FROM review_link_history WHERE answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)");links.text(1,action.id);
        while(links.row())for(int column:{0,1})if(!links.null(column)){const auto value=transfer_codec::view(links.bytes(column));ids.push_back(value.link.origin.endpoint.account);ids.push_back(value.link.destination.endpoint.account);}
    };
    auto member_accounts=[&]{
        std::vector<std::int64_t> ids;Statement accounts(impl_->db,(std::string("SELECT DISTINCT account_id FROM(SELECT b.account_id AS account_id FROM review_members m JOIN batches b ON b.id=m.batch WHERE ")+member_where+" UNION SELECT b.account_id FROM review_repairs r JOIN batches b ON b.id=r.batch WHERE r.answer=?1) ORDER BY account_id").c_str());accounts.text(1,action.id);while(accounts.row())ids.push_back(accounts.integer(0));link_accounts(ids);return sorted_unique(std::move(ids));
    };
    const bool balance_wanted=with_balance && account!=0;
    auto read_version=[&](const imports::Occurrence& id,std::int64_t revision){
        HistorySnap snap;Statement row(impl_->db,"SELECT date,payee,memo,total FROM versions WHERE batch=? AND ordinal=? AND revision=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,revision);if(!row.row())return snap;
        snap.present=true;snap.date=row.bytes(0);snap.payee=row.bytes(1);snap.memo=row.bytes(2);snap.total=row.integer(3);
        Statement definition(impl_->db,"SELECT definition FROM draft_fields WHERE batch=? AND ordinal=? AND revision=?");definition.text(1,id.batch);definition.integer(2,integer(id.ordinal));definition.integer(3,revision);
        if(definition.row())snap.definition=definition.bytes(0);else {Statement pin(impl_->db,"SELECT definition FROM batch_source_currency WHERE batch=?");pin.text(1,id.batch);if(pin.row())snap.definition=pin.bytes(0);}
        Statement splits(impl_->db,"SELECT category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");splits.text(1,id.batch);splits.integer(2,integer(id.ordinal));splits.integer(3,revision);
        while(splits.row()){if(snap.splits.size()>=qif::max_allocations)fail(SQLITE_CORRUPT,"This change's transactions could not be read.");snap.splits.push_back({splits.bytes(0),splits.bytes(1),splits.integer(2)});}return snap;
    };
    auto read_entry=[&](const imports::Occurrence& id,std::int64_t revision){
        HistorySnap snap;Statement row(impl_->db,"SELECT active,date,payee,memo,total FROM entry_values WHERE batch=? AND ordinal=? AND revision=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,revision);if(!row.row())return snap;
        snap.present=true;snap.has_active=true;snap.active=row.integer(0)!=0;snap.date=row.bytes(1);snap.payee=row.bytes(2);snap.memo=row.bytes(3);snap.total=row.integer(4);
        Statement definition(impl_->db,"SELECT definition FROM entry_fields WHERE batch=? AND ordinal=? AND revision=?");definition.text(1,id.batch);definition.integer(2,integer(id.ordinal));definition.integer(3,revision);
        if(definition.row())snap.definition=definition.bytes(0);else {Statement pin(impl_->db,"SELECT definition FROM batch_source_currency WHERE batch=?");pin.text(1,id.batch);if(pin.row())snap.definition=pin.bytes(0);}
        Statement splits(impl_->db,"SELECT category,memo,units FROM entry_allocation_values WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");splits.text(1,id.batch);splits.integer(2,integer(id.ordinal));splits.integer(3,revision);
        while(splits.row()){if(snap.splits.size()>=qif::max_allocations)fail(SQLITE_CORRUPT,"This change's transactions could not be read.");snap.splits.push_back({splits.bytes(0),splits.bytes(1),splits.integer(2)});}return snap;
    };
    auto diffs=[&](const HistorySnap& prior,const HistorySnap& result){
        std::vector<app::HistoryFieldChange> changes;
        if(!prior.present || !result.present)return changes;
        add_text(changes,"Date",app::HistoryFieldKind::date,prior.date,result.date);
        add_text(changes,"Payee",app::HistoryFieldKind::text,prior.payee,result.payee);
        add_text(changes,"Memo",app::HistoryFieldKind::text,prior.memo,result.memo);
        add_money(changes,"Amount",prior.total,prior.definition,result.total,result.definition);
        if(prior.has_active && result.has_active && prior.active!=result.active)add_text(changes,"Status",app::HistoryFieldKind::text,prior.active?"Accepted":"Removed",result.active?"Accepted":"Removed");
        const auto count=std::max(prior.splits.size(),result.splits.size());const bool multi=count>1;const auto definition=result.definition.empty()?prior.definition:result.definition;
        for(std::size_t i=0;i<count;++i){
            const auto* left=i<prior.splits.size()?&prior.splits[i]:nullptr;const auto* right=i<result.splits.size()?&result.splits[i]:nullptr;
            const auto number=std::to_string(i+1);
            add_text(changes,multi?"Split "+number+" category":"Category",app::HistoryFieldKind::text,left?left->category:"",right?right->category:"");
            add_text(changes,multi?"Split "+number+" memo":"Split memo",app::HistoryFieldKind::text,left?left->memo:"",right?right->memo:"");
            add_money(changes,multi?"Split "+number+" amount":"Split amount",left?left->units:0,definition,right?right->units:0,definition);
        }
        return changes;
    };
    auto fill_current=[&](app::HistoryMemberView& member,const imports::Occurrence& id,const HistorySnap& fallback){
        const bool accepted=transaction_accepted(id);
        HistorySnap current;
        if(accepted){
            Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,id.batch);head.integer(2,integer(id.ordinal));
            if(head.row())current=read_entry(id,head.integer(0));
        } else {
            Statement latest(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");latest.text(1,id.batch);latest.integer(2,integer(id.ordinal));
            if(latest.row() && !latest.null(0))current=read_version(id,latest.integer(0));
            if(current.present)current.definition=account_for_batch(id.batch).currency.definition_id();
        }
        if(!current.present){current=fallback;member.state="Removed";member.in_ledger=false;}
        else if(accepted){
            member.state=current.active?"Accepted":"Removed";
            member.in_ledger=current.active && retained_member(impl_->db,id.batch,id.ordinal) && !generated_match(impl_->db,id);
        } else {
            const bool removed=pending_removed(impl_->db,id) || !retained_member(impl_->db,id.batch,id.ordinal);
            member.state=removed?"Removed":"New";
            Statement batch_state(impl_->db,"SELECT state FROM batches WHERE id=?");batch_state.text(1,id.batch);
            Statement decision(impl_->db,"SELECT decision FROM versions WHERE batch=? AND ordinal=? ORDER BY revision DESC LIMIT 1");decision.text(1,id.batch);decision.integer(2,integer(id.ordinal));
            const auto state=batch_state.row()?batch_state.integer(0):-1;const auto chosen=decision.row()?decision.integer(0):-1;
            member.in_ledger=!removed && !generated_match(impl_->db,id) && (chosen==0 || chosen==1) && (state==static_cast<int>(app::BatchState::pending) || state==static_cast<int>(app::BatchState::protected_drafts));
        }
        member.date=current.date;member.payee=current.payee;member.memo=current.memo;member.amount={current.total,current.definition};
        for(const auto& split:current.splits)member.splits.push_back({split.category,split.memo,{split.units,current.definition}});
    };
    if(action.kind==app::ActionKind::opening || action.kind==app::ActionKind::currency) {
        Statement owner(impl_->db,"SELECT account,opening,prior_opening,opening_date,prior_opening_date,definition,prior_definition,kind FROM account_operations WHERE id=?");owner.text(1,action.payload);
        if(!owner.row())fail(SQLITE_CORRUPT,"Account Undo details are missing");
        const auto kind=owner.integer(7);if(kind!=(action.kind==app::ActionKind::opening?4:5))fail(SQLITE_CORRUPT,"Account Undo details are missing");
        std::vector<std::int64_t> ids{owner.integer(0)};
        if(action.kind==app::ActionKind::currency){
            Statement links(impl_->db,"SELECT v.payload FROM transfer_versions v JOIN ledger_operations l ON l.sequence=v.sequence WHERE l.kind=5 AND l.undo_of=?");links.text(1,action.payload);
            while(links.row()){const auto value=transfer_codec::view(links.bytes(0));ids.push_back(value.link.origin.endpoint.account);ids.push_back(value.link.destination.endpoint.account);}
            std::set<std::int64_t> seen;std::erase_if(ids,[&](auto id){return !seen.insert(id).second;});
            Statement counted(impl_->db,"SELECT count(*) FROM account_amount_revisions WHERE operation=? AND kind IN(0,2) AND position=0");counted.text(1,action.payload);view.count=counted.row()?counted.integer(0):0;
            Statement scale(impl_->db,"SELECT before,change,after FROM ledger_operations WHERE kind=5 AND undo_of=?");scale.text(1,action.payload);
            if(!scale.row())fail(SQLITE_CORRUPT,"Account Undo details are missing");
            const auto before=scale.integer(0),change=scale.integer(1),after=scale.integer(2);if(scale.row())fail(SQLITE_CORRUPT,"Account Undo details are missing");
            if(balance_wanted){view.balance.kind=app::HistoryBalanceKind::rescale;view.balance.before={before,owner.bytes(6)};view.balance.change={change,owner.bytes(5)};view.balance.after={after,owner.bytes(5)};}
            if(Currency::from_definition(owner.bytes(6)).code()!=Currency::from_definition(owner.bytes(5)).code())add_text(view.changes,"Currency",app::HistoryFieldKind::text,Currency::from_definition(owner.bytes(6)).code(),Currency::from_definition(owner.bytes(5)).code());
        } else {
            view.count=0;view.lists_transactions=false;
            if(balance_wanted){const auto prior=owner.integer(2),next=owner.integer(1);const auto definition=owner.bytes(5);const auto prior_definition=owner.bytes(6);const auto delta=definition==prior_definition?(Money(next,Currency::from_definition(definition))-Money(prior,Currency::from_definition(prior_definition))).units():0;view.balance.kind=app::HistoryBalanceKind::absolute;view.balance.before={prior,prior_definition};view.balance.change={delta,definition};view.balance.after={next,definition};}
            add_money(view.changes,"Opening balance",owner.integer(2),owner.bytes(6),owner.integer(1),owner.bytes(5));
            add_text(view.changes,"Opening date",app::HistoryFieldKind::date,owner.bytes(4),owner.bytes(3));
        }
        view.accounts=names_of(ids);view.lists_transactions=false;return view;
    }
    if(action.kind==app::ActionKind::import) {
        std::vector<std::int64_t> ids;Statement accounts(impl_->db,"SELECT DISTINCT account_id FROM batches WHERE source_id=? ORDER BY account_id");accounts.text(1,action.payload);while(accounts.row())ids.push_back(accounts.integer(0));
        view.accounts=names_of(ids);view.lists_transactions=false;
        Statement counted(impl_->db,"SELECT count(*) FROM source_occurrences o JOIN records r ON r.batch=o.batch AND r.ordinal=o.ordinal JOIN batches b ON b.id=o.batch WHERE b.source_id=?1 AND EXISTS(SELECT 1 FROM versions v WHERE v.batch=o.batch AND v.ordinal=o.ordinal AND v.revision=(SELECT min(revision) FROM versions WHERE batch=o.batch AND ordinal=o.ordinal) AND v.decision!=2)");counted.text(1,action.payload);view.count=counted.row()?counted.integer(0):0;
        if(balance_wanted){
            std::optional<Money> sum;bool mixed=false;
            Statement totals(impl_->db,"SELECT b.id,v.total FROM batches b JOIN versions v ON v.batch=b.id WHERE b.source_id=?1 AND b.account_id=?2 AND v.revision=(SELECT min(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal) AND v.decision!=2");totals.text(1,action.payload);totals.integer(2,account);
            while(totals.row()){const auto part=Money(totals.integer(1),pinned_currency(impl_->db,totals.bytes(0)));if(!sum)sum=part;else if(sum->currency().definition_id()==part.currency().definition_id())sum=*sum+part;else mixed=true;}
            if(!mixed){const auto change=sum?sum->units():0;const auto definition=sum?sum->currency().definition_id():account_by_id(account).currency.definition_id();view.balance.kind=app::HistoryBalanceKind::delta;view.balance.change={change,definition};}
        }
        return view;
    }
    if(action.kind==app::ActionKind::acceptance) {
        std::vector<std::int64_t> ids;Statement accounts(impl_->db,"SELECT DISTINCT b.account_id FROM operations o JOIN batches b ON b.id=o.batch WHERE o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1) UNION SELECT t.account FROM transfer_events t JOIN acceptance_operations m ON m.operation=t.operation WHERE m.parent=?1");accounts.text(1,action.payload);while(accounts.row())ids.push_back(accounts.integer(0));
        link_accounts(ids);view.accounts=names_of(sorted_unique(std::move(ids)));view.lists_transactions=false;view.count=action.count;
        if(balance_wanted){view.balance.kind=app::HistoryBalanceKind::delta;view.balance.change={0,account_by_id(account).currency.definition_id()};}
        return view;
    }
    if(action.kind==app::ActionKind::manual_creation) {
        const imports::Occurrence id{action.payload,0};view.accounts=names_of({account_for_batch(id.batch).identity});view.lists_transactions=true;view.count=1;
        Statement first(impl_->db,"SELECT min(revision) FROM versions WHERE batch=? AND ordinal=0");first.text(1,id.batch);
        const auto created=first.row() && !first.null(0)?read_version(id,first.integer(0)):HistorySnap{};
        if(balance_wanted && created.present && !created.definition.empty()){view.balance.kind=app::HistoryBalanceKind::delta;view.balance.change={created.total,created.definition};}
        app::HistoryMemberView member;member.batch=id.batch;member.ordinal=0;fill_current(member,id,created);view.members.push_back(std::move(member));return view;
    }
    view.accounts=names_of(member_accounts());
    if(question=="review-exclusion"){view.lists_transactions=false;view.count=action.count;return view;}
    const bool require_members=action.kind==app::ActionKind::transaction || action.kind==app::ActionKind::rule_application;
    struct StoredMember {imports::Occurrence id;bool committed=false;std::int64_t prior=0,result=0;std::string answer,child;std::int64_t account=0;};
    std::vector<StoredMember> stored;
    Statement members(impl_->db,(std::string("SELECT m.batch,m.ordinal,m.committed,m.prior,m.result,m.answer,m.child,b.account_id FROM review_members m JOIN batches b ON b.id=m.batch WHERE ")+member_where+" ORDER BY m.batch,m.ordinal").c_str());members.text(1,action.id);
    while(members.row()){if(stored.size()>=4096)fail(SQLITE_TOOBIG,"This change's transactions could not be read.");if(members.integer(1)<0)fail(SQLITE_CORRUPT,"This change's transactions could not be read.");stored.push_back({{members.bytes(0),static_cast<std::size_t>(members.integer(1))},members.integer(2)!=0,members.integer(3),members.integer(4),members.bytes(5),members.bytes(6),members.integer(7)});}
    Statement repairs(impl_->db,"SELECT r.batch,r.ordinal,r.result,b.account_id FROM review_repairs r JOIN batches b ON b.id=r.batch WHERE r.answer=?");repairs.text(1,action.id);
    while(repairs.row()){if(stored.size()>=4096)fail(SQLITE_TOOBIG,"This change's transactions could not be read.");if(repairs.integer(1)<0)fail(SQLITE_CORRUPT,"This change's transactions could not be read.");stored.push_back({{repairs.bytes(0),static_cast<std::size_t>(repairs.integer(1))},false,-1,repairs.integer(2),std::string(action.id),{},repairs.integer(3)});}
    if(require_members && action.count>0 && stored.empty())fail(SQLITE_CORRUPT,"This change's transactions could not be read.");
    struct AccountEffect {bool ledger=false,ok=false;std::int64_t sequence=0,before=0,change=0,after=0;std::string definition;};
    std::vector<AccountEffect> effects;
    for(const auto& item:stored){
        const bool repair=!item.committed && item.prior<0;
        const auto prior=repair?HistorySnap{}:item.committed?read_entry(item.id,item.prior):read_version(item.id,item.prior);
        const auto result=item.committed?read_entry(item.id,item.result):read_version(item.id,item.result);
        if(require_members && (repair?!result.present:(!prior.present || !result.present)))fail(SQLITE_CORRUPT,"This change's transactions could not be read.");
        app::HistoryMemberView member;member.batch=item.id.batch;member.ordinal=item.id.ordinal;member.changes=diffs(prior,result);fill_current(member,item.id,result.present?result:prior);view.members.push_back(std::move(member));
        if(!balance_wanted || item.account!=account)continue;
        AccountEffect effect;
        if(item.committed){
            Statement ledger(impl_->db,"SELECT l.before,l.change,l.after,h.sequence FROM ledger_operations l JOIN history_order h ON h.operation=l.id WHERE l.id=? AND l.kind IN(1,2,3)");ledger.text(1,item.child);
            if(ledger.row()){effect.ledger=true;effect.ok=true;effect.before=ledger.integer(0);effect.change=ledger.integer(1);effect.after=ledger.integer(2);effect.sequence=ledger.integer(3);effect.definition=currency_as_of(impl_->db,account,effect.sequence).definition_id();}
            else if(prior.present && result.present && prior.definition==result.definition && !prior.definition.empty()){const auto before_units=prior.has_active && !prior.active?0:prior.total;const auto after_units=result.has_active && !result.active?0:result.total;effect.ok=true;effect.change=after_units-before_units;effect.definition=prior.definition;}
        } else if(repair && result.present && !result.definition.empty()){effect.ok=true;effect.definition=result.definition;effect.change=result.total;}
        else if(prior.present && !prior.definition.empty() && ((question=="ledger-delete" && item.answer==action.id) || (result.present && prior.definition==result.definition))){
            effect.ok=true;effect.definition=prior.definition;effect.change=question=="ledger-delete" && item.answer==action.id?-prior.total:result.total-prior.total;
        }
        effects.push_back(std::move(effect));
    }
    view.lists_transactions=!view.members.empty();
    if(view.lists_transactions)view.count=integer(view.members.size());
    if(!view.members.empty() && std::all_of(view.members.begin(),view.members.end(),[&](const auto& member){return same_fields(member.changes,view.members.front().changes);})){view.changes=view.members.front().changes;for(auto& member:view.members)member.changes.clear();}
    if(balance_wanted && !effects.empty() && std::all_of(effects.begin(),effects.end(),[](const auto& effect){return effect.ok;})){
        const auto& definition=effects.front().definition;const bool one_currency=std::all_of(effects.begin(),effects.end(),[&](const auto& effect){return effect.definition==definition;});
        const bool all_ledger=std::all_of(effects.begin(),effects.end(),[](const auto& effect){return effect.ledger;});
        if(one_currency && all_ledger){
            const auto ends=std::minmax_element(effects.begin(),effects.end(),[](const auto& left,const auto& right){return left.sequence<right.sequence;});
            Statement between(impl_->db,"SELECT count(*) FROM ledger_operations l JOIN history_order h ON h.operation=l.id JOIN batches b ON b.id=l.batch WHERE b.account_id=?1 AND l.kind IN(1,2,3) AND h.sequence>?2 AND h.sequence<?3");
            between.integer(1,account);between.integer(2,ends.first->sequence);between.integer(3,ends.second->sequence);
            const auto between_count=between.row()?between.integer(0):1;
            const auto ours_between=std::count_if(effects.begin(),effects.end(),[&](const auto& effect){return effect.sequence>ends.first->sequence && effect.sequence<ends.second->sequence;});
            std::int64_t sum=0;bool overflow=false;for(const auto& effect:effects){if((effect.change>0 && sum>INT64_MAX-effect.change) || (effect.change<0 && sum<INT64_MIN-effect.change))overflow=true;else sum+=effect.change;}
            const auto same_ends=currency_as_of(impl_->db,account,ends.first->sequence).definition_id()==currency_as_of(impl_->db,account,ends.second->sequence).definition_id();
            const bool sums=!overflow && !((sum>0 && ends.first->before>INT64_MAX-sum) || (sum<0 && ends.first->before<INT64_MIN-sum)) && ends.first->before+sum==ends.second->after;
            if(between_count==ours_between && same_ends && sums){view.balance.kind=app::HistoryBalanceKind::absolute;view.balance.before={ends.first->before,definition};view.balance.change={sum,definition};view.balance.after={ends.second->after,definition};return view;}
        }
        if(one_currency){std::int64_t sum=0;bool overflow=false;for(const auto& effect:effects){if((effect.change>0 && sum>INT64_MAX-effect.change) || (effect.change<0 && sum<INT64_MIN-effect.change))overflow=true;else sum+=effect.change;}if(!overflow){view.balance.kind=app::HistoryBalanceKind::delta;view.balance.change={sum,definition};}}
    }
    return view;
}
std::vector<app::HistoryFieldChange> ImportStore::action_field_changes(const app::ActionSummary& action) const {
    if(action.count>64 && (action.kind==app::ActionKind::transaction || action.kind==app::ActionKind::rule_application || action.kind==app::ActionKind::review))return history_named_fields(action);
    const auto view=history_view(action,0,false,false);
    if(!view.changes.empty() || view.members.empty())return view.changes;
    std::vector<app::HistoryFieldChange> names;std::set<std::string> seen;
    auto take=[&](const std::string& field){if(!seen.insert(field).second)return;app::HistoryFieldChange change;change.field=field;change.named_only=true;names.push_back(std::move(change));};
    for(const auto* field:{"Date","Payee","Memo","Amount","Status","Category","Split memo","Split amount","Opening balance","Opening date","Currency"})for(const auto& member:view.members)for(const auto& change:member.changes)if(change.field==field)take(field);
    for(const auto& member:view.members)for(const auto& change:member.changes)take(change.field);
    return names;
}
app::AccountHistoryPage ImportStore::account_change_history(std::int64_t account,std::int64_t before,std::size_t limit,const std::optional<imports::Occurrence>& focus) const {
    if(before<0 || limit==0 || limit>50)fail(SQLITE_MISUSE,"Invalid Undo history page");
    Statement present(impl_->db,"SELECT 1 FROM account WHERE singleton=?");present.integer(1,account);if(!present.row())fail(SQLITE_NOTFOUND,"This account is not in the file.");
    app::AccountHistoryPage page;const auto head=ActionChain(impl_->db).head();if(!head)return page;
    auto header=[&](std::string_view id)->std::optional<app::ActionSummary>{
        if(id.empty())return {};
        Statement row(impl_->db,"SELECT id,parent,sequence,kind,state,payload,label,count FROM action_headers WHERE id=?");row.text(1,id);if(!row.row())return {};
        const auto kind=row.integer(3),state=row.integer(4);if(kind<0 || kind>7 || state<0 || state>3)fail(SQLITE_CORRUPT,"Undo head is inconsistent");
        return app::ActionSummary{row.bytes(0),row.bytes(1),row.integer(2),static_cast<app::ActionKind>(kind),static_cast<app::ActionState>(state),row.bytes(5),row.bytes(6),row.integer(7)};
    };
    std::set<std::string> seen;auto current=head;
    for(int walked=0;current && walked<100000;++walked){
        if(!seen.insert(current->id).second)break;
        const bool in_page=before==0 || current->sequence<before;
        if(in_page && history_touches(*current,account)){
            if(page.actions.size()<limit)page.actions.push_back(history_view(*current,account,true,false));
            else page.more=true;
        }
        if(focus && page.focus.empty() && history_includes(*current,*focus))page.focus=current->id;
        if(page.more && (!focus || !page.focus.empty()))break;
        if(current->parent.empty())break;
        auto parent=header(current->parent);if(!parent)fail(SQLITE_CORRUPT,"Undo head is inconsistent");
        if(parent->state!=app::ActionState::applied)break;
        current=std::move(parent);
    }
    return page;
}
