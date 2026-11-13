namespace {
template<class State> void retain_review_ledger_entries(State& state,sqlite3_int64 before,const std::set<imports::Occurrence>& changed){
    if(state.ledger_entry_changes!=before)return;
    for(const auto& id:changed)state.ledger_entries.erase(id);
    // Bytes remain a conservative upper bound until the next full invalidation.
    state.ledger_entry_changes=sqlite3_total_changes64(state.db);
}
std::string review_key(const imports::Occurrence& id) {return std::to_string(id.batch.size())+":"+id.batch+":"+std::to_string(id.ordinal);}
std::int64_t review_generation(sqlite3* db) {Statement s(db,"SELECT revision FROM mutation_generation WHERE singleton=1");if(!s.row())fail(SQLITE_CORRUPT);return s.integer(0);}
bool review_pair(sqlite3* db,int kind,const imports::Occurrence& a,const imports::Occurrence& b) {
    Statement s(db,R"SQL(SELECT 1 FROM review_choices p JOIN review_choices q ON q.answer=p.answer AND q.kind=p.kind
      WHERE p.kind=?1 AND p.batch=?2 AND p.ordinal=?3 AND q.batch=?4 AND q.ordinal=?5 AND (p.resolved=1 OR q.resolved=1)
      AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=p.answer))SQL");
    s.integer(1,kind);s.text(2,a.batch);s.integer(3,integer(a.ordinal));s.text(4,b.batch);s.integer(5,integer(b.ordinal));return s.row();
}
std::string review_request(const app::ReviewAnswer& a) {
    std::string out=std::to_string(a.generation)+":"+std::to_string(static_cast<int>(a.action))+":";
    const auto field=[&](std::string_view v){out+=std::to_string(v.size())+":"+std::string(v);};
    field(a.question);for(const auto& id:a.selected)field(review_key(id));field(a.survivor?review_key(*a.survivor):"");field(a.left_allocation);field(a.right_allocation);field(a.remainder_category);out+=a.split_difference?"1":"0";return out;
}
}
namespace {
rules::Pattern exclusion_pattern(const app::LinkExclusion& rule) {
    if(rule.field!="payee" && rule.field!="memo")fail(SQLITE_CONSTRAINT,"Choose Payee or Memo");
    if(rule.expression.empty())fail(SQLITE_CONSTRAINT,"Enter a nonempty exclusion condition");
    if(rule.regex)return rules::Pattern(rule.expression);
    std::string pattern="\\A";
    for(char c:rule.expression){if(std::string_view(R"(\.^$|()[]{}*+?)").find(c)!=std::string_view::npos)pattern+='\\';pattern+=c;}
    return rules::Pattern(pattern+"\\z");
}
bool excluded_endpoint(const app::LinkExclusion& rule,const rules::Pattern& pattern,const app::ReviewTransaction& row,std::size_t& steps) {
    if(rule.removed || (rule.account && rule.account!=row.account))return false;
    const auto matches=[&](std::string_view field){
        try{return pattern.match(field,[&]{return ++steps>1000000;}).has_value();}
        catch(const rules::RegexError& error){if(error.kind()==rules::RegexFailure::cancelled && steps>1000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Link exclusion evaluation exhausted its aggregate execution budget");throw;}
    };
    if(rule.field=="payee")return matches(row.draft.payee);
    if(matches(row.draft.memo))return true;
    for(const auto& part:row.draft.transaction.allocations())if(matches(part.memo))return true;
    return false;
}
}
namespace {
bool review_opening_balance(const imports::DraftRow& row) {
    return app::account_name_matches(row.payee,"Opening Balance") || app::account_name_matches(row.payee,"Starting Balance");
}
}
bool ImportStore::review_link_category(std::string_view category,std::int64_t counterpart_account) const {
    if(const auto name=transfer_name(category))return resolve_account_name(*name)==counterpart_account;
    return category.empty() || app::account_name_matches(category,"Uncategorized") || app::account_name_matches(category,"Uncategorised");
}
void ImportStore::generate_explicit_transfers(app::OperationContext* context) {
    diag::Span phaseSpan(diag::Code::import_generate_transfers);
    // This helper only appends draft links inside its caller's atomic operation.
    // Reuse the working set and accepted balances; no appended draft can change
    // those balances. Full relationship validation still precedes durable commit.
    if(sqlite3_get_autocommit(impl_->db))fail(SQLITE_MISUSE,"Transfer generation requires an atomic import");
    ScopedStatements plans(impl_->db);
    GroupSelection selection(impl_->selected_account);
    auto values=all_transfers();
    auto resolved=resolved_transfers(values);
    std::map<std::int64_t,Money> starting_balances;
    Statement pending(impl_->db,"SELECT id,account_id FROM batches WHERE state=0 ORDER BY rowid");
    while(pending.row()){
        const auto batch_id=pending.bytes(0);impl_->selected_account=pending.integer(1);
        if(account().archived)continue;
        const auto info=batch(impl_->db,batch_id);
        for(std::size_t ordinal=0;ordinal<info.count;++ordinal){
            if(context)context->checkpoint("Recording explicit transfer destinations");
            const imports::Occurrence id{batch_id,ordinal};
            if(transaction_accepted(id))continue;
            const auto row=load_row(id,false);
            if(!row.draft || row.excluded_by_clear || row.matched_generated || row.review_stale || row.decision.disposition!=imports::Disposition::include)continue;
            for(const auto& part:row.draft->transaction.allocations()){
                const auto name=transfer_name(part.category);
                if(!name || part.amount.units()==0 || part.amount.units()==INT64_MIN || resolved.contains({batch_id,ordinal,part.identity}))continue;
                const auto target=resolve_account_name(*name);
                if(!target || *target==impl_->selected_account)continue;
                const auto destination=account_by_id(*target);
                if(destination.archived || destination.currency!=part.amount.currency())continue;
                const auto sequence=transfer_sequence();if(sequence==INT64_MAX)fail(SQLITE_FULL);
                const auto operation="explicit-transfer-"+std::to_string(sequence+1);
                app::TransferRequest request{};request.action=app::TransferAction::from_draft;request.link=operation;request.origin=id;request.allocation=part.identity;request.destination=*target;request.batch_revision=info.revision;request.expected_sequence=sequence;
                identifier(operation);
                require_writable_account(impl_->db,impl_->selected_account);
                require_writable_account(impl_->db,*target);
                Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=? UNION ALL SELECT 1 FROM transfer_heads WHERE identity=?");collision.text(1,operation);collision.text(2,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Transfer operation identity collision");
                auto balance=starting_balances.find(impl_->selected_account);
                if(balance==starting_balances.end())balance=starting_balances.emplace(impl_->selected_account,committed_balance()).first;
                const auto before=balance->second;const Money zero(0,before.currency());
                const auto link=transfers::generate(operation,{impl_->selected_account,id,part.identity},*row.draft,
                    {*target,{"~transfer-"+operation+"-destination",0},"generated"},destination.currency,true);
                if(link.origin.state!=transfers::State::draft || link.destination.state!=transfers::State::draft)fail(SQLITE_CONSTRAINT,"Generated transfer must remain unaccepted");
                const app::TransferView generated{link,row.draft->payee,row.draft->memo,sequence+1};
                Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,impl_->selected_account);event.blob(3,transfer_codec::encode(request));event.integer(4,0);event.integer(5,0);event.done();
                store_transfer(impl_->db,generated,sequence+1,true);
                append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,before,zero,before,"",sequence,catalog_sequence(impl_->db));
                values.push_back(generated);resolved.insert({batch_id,ordinal,part.identity});
                if(context)context->checkpoint("Generated transfer staged");
            }
        }
    }
    validate_transfer_set(values);
}
std::vector<app::LinkExclusion> ImportStore::link_exclusions() const {
    std::vector<app::LinkExclusion> result;
    Statement rows(impl_->db,"SELECT identity,field,expression,account,regex,removed FROM link_exclusion_events e WHERE sequence=(SELECT max(sequence) FROM link_exclusion_events WHERE identity=e.identity) ORDER BY sequence");
    while(rows.row())result.push_back({rows.bytes(0),rows.bytes(1),rows.bytes(2),rows.integer(3),rows.integer(4)!=0,rows.integer(5)!=0});
    return result;
}
void ImportStore::verify_link_exclusions() const {
    Statement events(impl_->db,"SELECT operation,identity,field,expression,account,regex,removed,generation,sequence FROM link_exclusion_events ORDER BY sequence");
    std::int64_t sequence=0;const auto generation=review_generation(impl_->db);
    while(events.row()){
        identifier(events.bytes(0));identifier(events.bytes(1));
        if(events.integer(8)!=++sequence || events.integer(7)<0 || events.integer(7)>generation)fail(SQLITE_CORRUPT,"Link exclusion history does not reconcile");
        const app::LinkExclusion rule{events.bytes(1),events.bytes(2),events.bytes(3),events.integer(4),events.integer(5)!=0,events.integer(6)!=0};
        if(rule.account)account_by_id(rule.account);
        try{const auto pattern=exclusion_pattern(rule);(void)pattern;}catch(const rules::RegexError&){fail(SQLITE_CORRUPT,"Invalid retained link exclusion");}
        Statement registered(impl_->db,"SELECT 1 FROM operation_registry WHERE id=? AND domain=6");registered.text(1,events.bytes(0));if(!registered.row())fail(SQLITE_CORRUPT);
    }
    Statement inverses(impl_->db,R"SQL(
SELECT 1 FROM review_undos u JOIN review_answers a ON a.id=u.answer
JOIN link_exclusion_events f ON f.operation=a.id
LEFT JOIN link_exclusion_events e ON e.operation=u.id||'-exclusion'
LEFT JOIN link_exclusion_events p ON p.identity=f.identity AND p.sequence=(SELECT max(sequence) FROM link_exclusion_events WHERE identity=f.identity AND sequence<f.sequence)
WHERE a.question='review-exclusion' AND (e.operation IS NULL OR e.sequence<=f.sequence OR e.identity!=f.identity
 OR e.field!=coalesce(p.field,f.field) OR e.expression!=coalesce(p.expression,f.expression)
 OR e.account!=coalesce(p.account,f.account) OR e.regex!=coalesce(p.regex,f.regex) OR e.removed!=coalesce(p.removed,1))
)SQL");
    if(inverses.row())fail(SQLITE_CORRUPT,"Transfer exclusion restoration is inconsistent");
    const auto current=link_exclusions();if(std::count_if(current.begin(),current.end(),[](const auto& rule){return !rule.removed;})>128)fail(SQLITE_CORRUPT);
}
app::LinkExclusionPreview ImportStore::preview_link_exclusion(const app::LinkExclusion& rule) const {
    identifier(rule.identity);auto pattern=exclusion_pattern(rule);(void)pattern;
    if(rule.account)account_by_id(rule.account);
    auto proposed=link_exclusions();std::erase_if(proposed,[&](const auto& r){return r.identity==rule.identity;});proposed.push_back(rule);
    if(std::count_if(proposed.begin(),proposed.end(),[](const auto& r){return !r.removed;})>128)fail(SQLITE_TOOBIG,"At most 128 active link exclusions are supported");
    const auto before=review_sheet(),after=build_review_sheet(proposed);
    const auto edges=[](const app::ReviewSheet& sheet){std::map<std::string,std::string> out;for(const auto& q:sheet.questions)if(q.kind==app::ReviewKind::link)for(std::size_t i=1;i<q.rows.size();++i){const auto& a=q.rows.front();const auto& b=q.rows[i];out.emplace(review_key(a.identity)+"/"+review_key(b.identity),a.account_name+": "+a.draft.date+" "+a.draft.payee+" ↔ "+b.account_name+": "+b.draft.date+" "+b.draft.payee);}return out;};
    const auto old=edges(before),next=edges(after);app::LinkExclusionPreview preview{before.generation,{}};
    for(const auto& [id,label]:old)if(!next.contains(id))preview.comparisons.push_back("Suppress: "+label);
    for(const auto& [id,label]:next)if(!old.contains(id))preview.comparisons.push_back("Restore: "+label);
    return preview;
}
app::ReviewReceipt ImportStore::save_link_exclusion(std::string_view operation,const app::LinkExclusion& rule,std::int64_t generation) {
    identifier(operation);identifier(rule.identity);
    Statement saved(impl_->db,"SELECT identity,field,expression,account,regex,removed,generation FROM link_exclusion_events WHERE operation=?");saved.text(1,operation);
    if(saved.row()){
        if(app::LinkExclusion{saved.bytes(0),saved.bytes(1),saved.bytes(2),saved.integer(3),saved.integer(4)!=0,saved.integer(5)!=0}!=rule || saved.integer(6)!=generation)fail(SQLITE_CONSTRAINT,"Exclusion retry changed its request");
        return {std::string(operation)};
    }
    if(generation!=review_generation(impl_->db))fail(SQLITE_CONSTRAINT,"Transactions changed; preview this exclusion again");
    preview_link_exclusion(rule);
    Atomic transaction(impl_->db);
    Statement event(impl_->db,"INSERT INTO link_exclusion_events VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM link_exclusion_events),?,?,?,?,?,?,?)");
    event.text(1,operation);event.text(2,rule.identity);event.text(3,rule.field);event.text(4,rule.expression);event.integer(5,rule.account);event.integer(6,rule.regex);event.integer(7,rule.removed);event.integer(8,generation);event.done();
    transfer_codec::Writer request;request.text(rule.identity);request.text(rule.field);request.text(rule.expression);request.number(rule.account);request.number(rule.regex);request.number(rule.removed);request.number(generation);
    const auto digest=session_snapshot::digest(request.bytes);
    record_transaction_action(operation,"review-exclusion",digest,{});
    session_snapshot::completed(impl_->db,operation,"exclusion",digest);
    finish([&]{transaction.commit();});return {std::string(operation)};
}
app::ReviewSheet ImportStore::review_sheet() const {
    diag::Span span(diag::Code::review_build);
    if(impl_->sheet_cache && impl_->sheet_changes==sqlite3_total_changes64(impl_->db))return *impl_->sheet_cache;
    auto result=build_review_sheet(link_exclusions());
    impl_->sheet_cache=result;impl_->sheet_changes=sqlite3_total_changes64(impl_->db);return result;
}
app::ReviewSheet ImportStore::build_review_sheet(const std::vector<app::LinkExclusion>& exclusions) const {
    ScopedStatements plans(impl_->db);
    struct Scope{Impl& state;std::int64_t selected;~Scope(){state.selected_account=selected;}} scope{*impl_,impl_->selected_account};
    app::ReviewSheet result;result.generation=review_generation(impl_->db);
    Statement undo(impl_->db,"SELECT a.id,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    result.can_undo=undo.row() && action_head() && action_head()->id==undo.bytes(0) && undo.bytes(1)!="ledger-edit" && undo.bytes(1)!="ledger-edit-batch" && undo.bytes(1)!="ledger-delete" && undo.bytes(1)!="rules-account";
    std::map<imports::Occurrence,app::ReviewTransaction> rows;
    std::set<imports::Occurrence> unresolved;
    auto all_accounts=accounts();const auto archived=archived_accounts();all_accounts.insert(all_accounts.end(),archived.begin(),archived.end());
    for(const auto& account:all_accounts){
        impl_->selected_account=account.identity;
        const auto account_batches=batches();std::map<std::string,bool> manual_batches;
        for(const auto& b:account_batches)manual_batches.emplace(b.id,b.manual);
        for(const auto& row:current_ledger()) {
            if(row.identity.batch.starts_with("~transfer-"))continue;
            rows.emplace(row.identity,app::ReviewTransaction{row.identity,account.identity,account.name,row.draft,row.committed,manual_batches.at(row.identity.batch)?"Manually entered":""});
        }
        for(const auto& b:account_batches)if(b.state==app::BatchState::pending || b.state==app::BatchState::captured || b.state==app::BatchState::protected_drafts){
            if(b.processing_incomplete){result.questions.push_back({"file:"+b.id,app::ReviewKind::correction,account.identity,account.name,"Resume this import before reviewing its transactions",b.id,{},{}});continue;}
            const auto parsed=parsed_source(b.id);
            if(parsed.diagnostic)result.questions.push_back({"file:"+b.id,app::ReviewKind::correction,account.identity,account.name,"Needs correction: import could not be fully read",b.id,{},{}});
            std::set<std::size_t> accepted;
            Statement accepted_rows(impl_->db,"SELECT ordinal FROM accepted_transactions WHERE batch=?");accepted_rows.text(1,b.id);
            while(accepted_rows.row())accepted.insert(static_cast<std::size_t>(accepted_rows.integer(0)));
            for(std::size_t i=0;i<parsed.records.size();++i){
                if(accepted.contains(i))continue;
                const imports::Occurrence id{b.id,i};const auto row=load_row(id,false);
                if(row.excluded_by_clear)continue;
                if(!row.draft){result.questions.push_back({"repair:"+review_key(id),app::ReviewKind::correction,account.identity,account.name,"Needs correction: repair this imported record",b.id,i,{}});continue;}
                if(row.decision.disposition==imports::Disposition::unresolved || row.review_stale){
                    unresolved.insert(id);
                    // A duplicate is hidden from the ledger, so the question has to carry the transaction itself.
                    if(!rows.contains(id))rows.emplace(id,app::ReviewTransaction{id,account.identity,account.name,*row.draft,false,manual_batches.at(b.id)?"Manually entered":""});
                }
            }
        }
    }
    // A partial decision leaves the unselected comparison outstanding even if
    // those rows were clean in their original import. A later explicit answer
    // retires that pending intent; undoing it exposes the remainder again.
    std::map<std::string,std::vector<imports::Occurrence>> remainders;
    Statement pending_choices(impl_->db,R"SQL(SELECT p.answer,p.batch,p.ordinal
      FROM review_choices p JOIN review_answers a ON a.id=p.answer
      WHERE p.kind=0 AND p.resolved=0 AND a.action<>0
      AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=p.answer)
      AND NOT EXISTS(SELECT 1 FROM review_choices n JOIN review_answers na ON na.id=n.answer
        WHERE n.kind=0 AND n.resolved=1 AND n.batch=p.batch AND n.ordinal=p.ordinal
        AND na.action<>0 AND na.sequence>a.sequence AND NOT EXISTS(SELECT 1 FROM review_undos nu WHERE nu.answer=n.answer)))SQL");
    while(pending_choices.row()){
        imports::Occurrence id{pending_choices.bytes(1),static_cast<std::size_t>(pending_choices.integer(2))};
        if(rows.contains(id))remainders[pending_choices.bytes(0)].push_back(std::move(id));
    }
    for(const auto& [answer,remaining]:remainders)if(remaining.size()>1)unresolved.insert(remaining.begin(),remaining.end());
    // Candidate edges use the existing source comparison rule. Connected groups
    // expose ambiguity without claiming that transitive candidates are identical.
    std::map<imports::Occurrence,std::set<imports::Occurrence>> graph;
    // Load immutable comparison facts once, not once per possible pair. Check
    // the engine predicate before querying persisted human decisions.
    std::map<imports::Occurrence,qif::Record> comparison_sources;
    std::set<std::string> unresolved_batches;for(const auto& id:unresolved)unresolved_batches.insert(id.batch);
    for(const auto& [id,view]:rows)if(unresolved_batches.contains(id.batch)){impl_->selected_account=view.account;comparison_sources.emplace(id,load_row(id,false).source);}
    for(const auto& id:unresolved){
        const auto it=rows.find(id);if(it==rows.end())continue;
        impl_->selected_account=it->second.account;
        const auto row=load_row(id,true);
        for(const auto& other:row.candidates)if(rows.contains(other) && !review_pair(impl_->db,0,id,other)){
            graph[id].insert(other);graph[other].insert(id);
        }
        // Candidate lookup historically omits same-import peers. Include them
        // only for a record already flagged as uncertain by the importer.
        for(const auto& [other,view]:rows)if(other!=id && (other.batch==id.batch || unresolved.contains(other)) && view.account==it->second.account){
            if(imports::candidate_basis(row.source,comparison_sources.at(other)) && !review_pair(impl_->db,0,id,other)){graph[id].insert(other);graph[other].insert(id);}
        }
    }
    // One-day review pass. Same-day groups stay sealed unless a new row missed
    // them, is older than the newest accepted original date in the account, and
    // sits one day from that group. The group then takes other ungrouped
    // same-amount rows within one day of its members, once. A chain of
    // consecutive days does not become one question. Automatic recognition is
    // unchanged.
    auto source_for=[&](const imports::Occurrence& id){
        auto found=comparison_sources.find(id);
        if(found==comparison_sources.end()){impl_->selected_account=rows.at(id).account;found=comparison_sources.emplace(id,load_row(id,false).source).first;}
        return found->second;
    };
    std::map<std::int64_t,std::string> newest_accepted;
    {Statement frontier(impl_->db,R"SQL(SELECT b.account_id,max(o.original_date) FROM accepted_transactions a
        JOIN batches b ON b.id=a.batch JOIN source_occurrences o ON o.batch=a.batch AND o.ordinal=a.ordinal
        WHERE o.original_date<>'' GROUP BY b.account_id)SQL");
    while(frontier.row())newest_accepted.emplace(frontier.integer(0),frontier.bytes(1));}
    auto neighboring=[&](const qif::Record& source,std::int64_t account,const imports::Occurrence& self){
        std::vector<imports::Occurrence> hits;
        if(!source.original_amount || source.date.empty() || source.format=="manual")return hits;
        const std::optional<std::string> days[]={imports::shift_calendar_date(source.date,-1),imports::shift_calendar_date(source.date,1)};
        for(const auto& day:days){
            if(!day)continue;
            Statement query(impl_->db,R"SQL(SELECT r.batch,r.ordinal FROM batches b
              JOIN records r ON r.batch=b.id JOIN versions v ON v.batch=r.batch AND v.ordinal=r.ordinal
              JOIN source_occurrences o ON o.batch=r.batch AND o.ordinal=r.ordinal
              WHERE b.account_id=? AND b.state IN(0,1,4)
                AND v.revision=(SELECT max(revision) FROM versions WHERE batch=r.batch AND ordinal=r.ordinal)
                AND v.decision IN(0,1) AND o.diagnostic IS NULL AND o.original_units=? AND o.original_date=?)SQL");
            query.integer(1,account);query.integer(2,source.original_amount->units());query.text(3,*day);
            while(query.row()){
                imports::Occurrence other{query.bytes(0),static_cast<std::size_t>(query.integer(1))};
                if(other==self || !rows.contains(other) || review_pair(impl_->db,0,self,other))continue;
                const auto candidate=source_for(other);
                if(imports::adjacent_day_basis(source,candidate))hits.push_back(other);
            }
        }
        return hits;
    };
    auto link=[&](const imports::Occurrence& left,const imports::Occurrence& right){if(left!=right && rows.contains(left) && rows.contains(right) && !review_pair(impl_->db,0,left,right)){graph[left].insert(right);graph[right].insert(left);}};
    std::map<imports::Occurrence,std::size_t> same_day_component;
    std::vector<std::vector<imports::Occurrence>> same_day_groups;
    for(const auto& [id,neighbors]:graph){
        if(neighbors.empty() || same_day_component.contains(id))continue;
        const auto index=same_day_groups.size();
        std::vector<imports::Occurrence> pending{id},members;same_day_component.emplace(id,index);
        while(!pending.empty()){const auto next=pending.back();pending.pop_back();members.push_back(next);for(const auto& other:graph[next])if(!same_day_component.contains(other)){same_day_component.emplace(other,index);pending.push_back(other);}}
        same_day_groups.push_back(std::move(members));
    }
    std::map<imports::Occurrence,std::size_t> attached;
    std::vector<imports::Occurrence> free_orphans;
    for(const auto& id:unresolved){
        if(!rows.contains(id) || same_day_component.contains(id))continue;
        const auto source=source_for(id);
        const auto frontier=newest_accepted.find(rows.at(id).account);
        if(frontier==newest_accepted.end() || source.date.empty() || source.date>=frontier->second)continue;
        const auto hits=neighboring(source,rows.at(id).account,id);
        std::set<std::size_t> groups;
        for(const auto& hit:hits){const auto found=same_day_component.find(hit);if(found!=same_day_component.end())groups.insert(found->second);}
        if(groups.empty()){free_orphans.push_back(id);continue;}
        const auto keep=*groups.begin();
        for(const auto group:groups)if(group!=keep){
            for(const auto& member:same_day_groups[group])same_day_component[member]=keep;
            same_day_groups[keep].insert(same_day_groups[keep].end(),same_day_groups[group].begin(),same_day_groups[group].end());
            same_day_groups[group].clear();
            for(auto& [orphan,assigned]:attached)if(assigned==group)assigned=keep;
        }
        attached[id]=keep;
    }
    std::set<imports::Occurrence> free_orphans_only(free_orphans.begin(),free_orphans.end());
    for(const auto& id:free_orphans)for(const auto& hit:neighboring(source_for(id),rows.at(id).account,id))if(free_orphans_only.contains(hit) || (!same_day_component.contains(hit) && !attached.contains(hit)))link(id,hit);
    std::set<std::size_t> widened;for(const auto& [id,group]:attached)widened.insert(group);
    for(const auto group:widened){
        if(same_day_groups[group].empty())continue;
        std::vector<imports::Occurrence> centers=same_day_groups[group];
        for(const auto& [id,assigned]:attached)if(assigned==group)centers.push_back(id);
        std::set<imports::Occurrence> extras;
        for(const auto& center:centers)for(const auto& hit:neighboring(source_for(center),rows.at(center).account,center))if(!same_day_component.contains(hit) && !attached.contains(hit))extras.insert(hit);
        for(const auto& hit:extras)for(const auto& center:centers)if(imports::adjacent_day_basis(source_for(center),source_for(hit))){link(center,hit);break;}
        for(const auto& [id,assigned]:attached)if(assigned==group)link(id,same_day_groups[group].front());
    }
    for(const auto& [answer,remaining]:remainders){
        (void)answer;
        for(std::size_t i=0;i<remaining.size();++i)for(std::size_t j=i+1;j<remaining.size();++j){
            if(!rows.contains(remaining[i]) || !rows.contains(remaining[j]) || review_pair(impl_->db,0,remaining[i],remaining[j]))continue;
            const auto basis=imports::candidate_basis(source_for(remaining[i]),source_for(remaining[j]),imports::MatchContext::confirmed_decision);
            if(basis==imports::MatchBasis::uncertain_date || basis==imports::MatchBasis::same_date_amount || basis==imports::MatchBasis::same_source_fields || basis==imports::MatchBasis::same_fitid)link(remaining[i],remaining[j]);
        }
    }
    std::map<std::string,std::int64_t> import_orders;Statement orders(impl_->db,"SELECT batch,min(sequence) FROM records GROUP BY batch");while(orders.row())import_orders.emplace(orders.bytes(0),orders.integer(1));
    std::set<imports::Occurrence> protected_rows;
    for(const auto& value:all_transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(side->state==transfers::State::removed)continue;
        if(!value.link.unresolved || local_side(*side))protected_rows.insert(side->endpoint.entry);
        if(!value.link.unresolved){if(side->attached_evidence)protected_rows.insert(side->attached_evidence->endpoint.entry);if(side->proposed_evidence)protected_rows.insert(side->proposed_evidence->endpoint.entry);}
    }
    Statement generated_rows(impl_->db,"SELECT batch,ordinal FROM generated_source_matches");
    while(generated_rows.row())protected_rows.insert({generated_rows.bytes(0),static_cast<std::size_t>(generated_rows.integer(1))});
    for(auto& [id,row]:rows)if(graph.contains(id) && !graph.at(id).empty()){
        row.duplicate_candidates.assign(graph[id].begin(),graph[id].end());
        row.transfer_linked=protected_rows.contains(id);
        row.import_order=import_orders.at(id.batch);
    }
    std::set<imports::Occurrence> visited,duplicate_rows;
    for(const auto& id:unresolved){
        if(!rows.contains(id) || visited.contains(id))continue;
        if(graph[id].empty()){
            const auto loaded=load_row(id,false);
            if(loaded.decision.disposition==imports::Disposition::include)continue;
            // A one-day row that this pass deliberately left outside a question
            // stays an ordinary transaction. A same-day decision with no live
            // partner still needs correction.
            if(!loaded.decision.duplicate_of && loaded.source.fitid.empty() && !neighboring(loaded.source,rows.at(id).account,id).empty())continue;
        }
        std::vector<imports::Occurrence> pending{id};visited.insert(id);std::vector<app::ReviewTransaction> members;
        while(!pending.empty()) {const auto next=pending.back();pending.pop_back();members.push_back(rows.at(next));duplicate_rows.insert(next);for(const auto& neighbor:graph[next])if(visited.insert(neighbor).second)pending.push_back(neighbor);}
        std::sort(members.begin(),members.end(),[](const auto& a,const auto& b){return a.identity<b.identity;});
        const auto& first=members.front();
        const bool dates_differ=members.size()>1 && std::any_of(members.begin(),members.end(),[&](const auto& row){return row.draft.date!=first.draft.date;});
        result.questions.push_back({"same:"+review_key(first.identity),members.size()>1?app::ReviewKind::same:app::ReviewKind::correction,first.account,first.account_name,members.size()>1?(dates_differ?"Are these the same? Dates differ.":"Are these the same?"):duplicate_decision_warning,first.identity.batch,first.identity.ordinal,std::move(members)});
    }
    std::set<imports::Occurrence> suppressed;
    std::size_t exclusion_comparisons=0,compiled_bytes=0,exclusion_steps=0;
    for(const auto& rule:exclusions)if(!rule.removed){
        const auto pattern=exclusion_pattern(rule);compiled_bytes+=pattern.compiled_bytes();
        if(compiled_bytes>16*1024*1024)fail(SQLITE_TOOBIG,"Link exclusions exceed the compiled pattern memory budget");
        for(const auto& [id,row]:rows)if(!suppressed.contains(id) && (!rule.account || rule.account==row.account)){
            exclusion_comparisons+=rule.field=="payee"?1:1+row.draft.transaction.allocations().size();
            if(exclusion_comparisons>1000000)fail(SQLITE_TOOBIG,"Link exclusion evaluation exceeds one million field comparisons");
            if(excluded_endpoint(rule,pattern,row,exclusion_steps))suppressed.insert(id);
        }
    }
    // Candidate dates are indexed so equal amounts years apart are never visited.
    // Financial linking still requires the existing explicit command.
    std::set<imports::Occurrence> linked;
    std::map<imports::Occurrence,std::set<std::string>> occupied;
    std::map<imports::Occurrence,std::string> repair_origins;
    const auto occupy=[&](const transfers::Endpoint& endpoint){occupied[endpoint.entry].insert(endpoint.allocation);};
    for(const auto& v:all_transfers())if(!v.link.unresolved && !v.link.destination_released && v.link.origin.state!=transfers::State::removed && v.link.destination.state!=transfers::State::removed){
        if((v.link.origin.independent_evidence || v.link.origin.ledger_entry) && !v.link.destination.independent_evidence && !v.link.destination.ledger_entry && !v.link.destination.attached_evidence && !v.link.destination.proposed_evidence)repair_origins.emplace(v.link.origin.endpoint.entry,"Replaces the generated counterpart; the selected transaction stays.");
        else {
            for(const auto* side:{&v.link.origin,&v.link.destination}){
                occupy(side->endpoint);if(side->attached_evidence)occupy(side->attached_evidence->endpoint);if(side->proposed_evidence)occupy(side->proposed_evidence->endpoint);
            }
        }
    }
    for(const auto& [id,row]:rows)if(std::all_of(row.draft.transaction.allocations().begin(),row.draft.transaction.allocations().end(),[&](const auto& allocation){return occupied[id].contains(allocation.identity);}))linked.insert(id);
    using DateIndex=std::pair<std::string,int>;
    std::map<DateIndex,std::vector<imports::Occurrence>> dated;
    std::map<imports::Occurrence,int> days;
    for(const auto& [id,row]:rows){
        const auto day=imports::posting_distance("0001-01-01",row.draft.date);days.emplace(id,day);
        dated[{row.draft.transaction.effective_total().currency().definition_id(),day}].push_back(id);
    }
    // Raw eligibility depends only on current financial fields and resolved aliases.
    // Re-evaluate both the old and new date neighborhoods after a mutation. Saved
    // decisions, relationships and exclusions are presentation filters on these
    // edges, so changing those cannot make a stale financial pair eligible.
    std::string alias_stamp;
    for(const auto& alias:account_aliases())alias_stamp+=std::to_string(alias.account)+":"+std::to_string(alias.name.size())+":"+alias.name;
    std::set<imports::Occurrence> affected;
    const auto neighborhood=[&](const app::ReviewTransaction& row){
        const auto day=imports::posting_distance("0001-01-01",row.draft.date);const auto currency=row.draft.transaction.effective_total().currency().definition_id();
        for(auto bucket=dated.lower_bound({currency,day-7});bucket!=dated.end() && bucket->first.first==currency && bucket->first.second<=day+7;++bucket)affected.insert(bucket->second.begin(),bucket->second.end());
    };
    if(alias_stamp!=impl_->link_alias_stamp){for(const auto& [id,row]:rows){(void)row;affected.insert(id);}}
    else {
        for(const auto& [id,row]:rows){const auto old=impl_->link_rows.find(id);if(old==impl_->link_rows.end() || old->second.account!=row.account || !same_draft(old->second.draft,row.draft) || impl_->link_occupied[id]!=occupied[id]){neighborhood(row);if(old!=impl_->link_rows.end())neighborhood(old->second);}}
        for(const auto& [id,row]:impl_->link_rows)if(!rows.contains(id))neighborhood(row);
    }
    auto edges=impl_->link_edges;
    std::erase_if(edges,[&](const auto& entry){return !rows.contains(entry.first);});
    for(const auto& id:affected){
        const auto& left=rows.at(id);auto& matches=edges[id];matches.clear();
        if(review_opening_balance(left.draft))continue;
        const auto day=days.at(id);const auto currency=left.draft.transaction.effective_total().currency().definition_id();
        for(auto bucket=dated.lower_bound({currency,day-7});bucket!=dated.end() && bucket->first.first==currency && bucket->first.second<=day+7;++bucket)for(const auto& other:bucket->second){
            if(matches.size()>4096)break;
            const auto& right=rows.at(other);if(right.account==left.account || review_opening_balance(right.draft))continue;
            bool eligible=false,difference=false;
            for(const auto& a:left.draft.transaction.allocations())for(const auto& b:right.draft.transaction.allocations()){
                if(occupied[id].contains(a.identity) || occupied[other].contains(b.identity) || !review_link_category(a.category,right.account) || !review_link_category(b.category,left.account))continue;
                const auto& outgoing=a.amount.units()<0?a:b;const auto target=a.amount.units()<0?right.account:left.account;
                const auto name=transfer_name(outgoing.category);const auto destination=name?resolve_account_name(*name):std::optional<std::int64_t>{};
                if(destination && *destination!=target)continue;
                if(imports::link_amount_candidate(a.amount,b.amount,destination==target)){
                    eligible=true;difference|=a.amount.units()==INT64_MIN || b.amount.units()==INT64_MIN || a.amount.units()!=-b.amount.units();
                }
            }
            if(eligible)matches.emplace(other,difference);
        }
    }
    impl_->link_rows=rows;impl_->link_edges=std::move(edges);impl_->link_alias_stamp=alias_stamp;impl_->link_occupied=occupied;
    std::set<imports::Occurrence> link_sources;
    for(const auto& [id,left]:rows){
        if(suppressed.contains(id) || linked.contains(id) || duplicate_rows.contains(id) || (left.committed && !repair_origins.contains(id)))continue;
        auto source=left;if(repair_origins.contains(id))source.note=repair_origins.at(id);
        std::vector<app::ReviewTransaction> candidates{source};
        const auto found=impl_->link_edges.find(id);
        if(found!=impl_->link_edges.end() && found->second.size()>4096){result.questions.push_back({"many-links:"+review_key(id),app::ReviewKind::correction,left.account,left.account_name,"More than 4096 eligible counterparts; select this transfer manually",id.batch,id.ordinal,{left}});continue;}
        if(found!=impl_->link_edges.end())for(const auto& [other,difference]:found->second){
            const auto target=rows.find(other);if(target==rows.end())continue;const auto& right=target->second;
            if(suppressed.contains(other) || linked.contains(other) || duplicate_rows.contains(other) || review_pair(impl_->db,1,id,other))continue;
            if(repair_origins.contains(other) && !repair_origins.contains(id))continue;
            if(!right.committed && !repair_origins.contains(id)){
                const auto outgoing=[](const app::ReviewTransaction& row){return std::any_of(row.draft.transaction.allocations().begin(),row.draft.transaction.allocations().end(),[](const auto& part){return part.amount.units()<0;});};
                const bool left_outgoing=outgoing(left),right_outgoing=outgoing(right);
                if(left_outgoing!=right_outgoing?!left_outgoing:other<id)continue;
            }
            auto candidate=right;
            candidate.note=(difference?"Explicit transfer destination; outgoing difference at most 4%; ":"Equal opposite allocation amounts; ")+std::to_string(std::abs(days.at(id)-days.at(other)))+" calendar day(s) apart";
            if(repair_origins.contains(other))candidate.note+=". "+repair_origins.at(other);
            std::size_t exact_pairs=0;
            for(const auto& a:left.draft.transaction.allocations())for(const auto& b:right.draft.transaction.allocations()){
                if(occupied[id].contains(a.identity) || occupied[other].contains(b.identity) || !review_link_category(a.category,right.account) || !review_link_category(b.category,left.account))continue;
                if(a.amount.units()==INT64_MIN || b.amount.units()==INT64_MIN || a.amount.units()!=-b.amount.units() || !imports::link_amount_candidate(a.amount,b.amount,false))continue;
                ++exact_pairs;
                candidate.suggested_left_allocation=a.identity;
                candidate.suggested_right_allocation=b.identity;
            }
            if(exact_pairs!=1){candidate.suggested_left_allocation.clear();candidate.suggested_right_allocation.clear();}
            candidates.push_back(std::move(candidate));
        }
        if(candidates.size()>1){link_sources.insert(id);result.questions.push_back({"link:"+review_key(id),app::ReviewKind::link,left.account,left.account_name,"Does this link with any of these?",id.batch,id.ordinal,std::move(candidates)});}
    }
    // Two questions that name the same two imported lines are one transfer.
    // Keep the outgoing line. When neither line is outgoing, keep the earlier
    // occurrence, matching the candidate filter above.
    {
        const auto outgoing=[](const app::ReviewTransaction& row){return std::any_of(row.draft.transaction.allocations().begin(),row.draft.transaction.allocations().end(),[](const auto& part){return part.amount.units()<0;});};
        std::set<std::string> dropped;
        for(std::size_t i=0;i<result.questions.size();++i)for(std::size_t j=i+1;j<result.questions.size();++j){
            const auto& a=result.questions[i];const auto& b=result.questions[j];
            if(a.kind!=app::ReviewKind::link || b.kind!=app::ReviewKind::link || a.rows.size()!=2 || b.rows.size()!=2)continue;
            const bool mirrored=a.rows[0].identity==b.rows[1].identity && a.rows[1].identity==b.rows[0].identity;
            const bool identical=a.rows[0].identity==b.rows[0].identity && a.rows[1].identity==b.rows[1].identity;
            if(!mirrored && !identical)continue;
            const bool a_out=outgoing(a.rows[0]),b_out=outgoing(b.rows[0]);
            if(a_out!=b_out)dropped.insert(a_out?b.key:a.key);
            else if(a.rows[0].identity!=b.rows[0].identity)dropped.insert(a.rows[0].identity<b.rows[0].identity?b.key:a.key);
            else dropped.insert(b.key);
        }
        if(!dropped.empty())std::erase_if(result.questions,[&](const auto& q){return dropped.contains(q.key);});
    }
    for(const auto& value:all_transfers()){
        const auto& link=value.link;
        if(link.unresolved || link.destination_released || link.destination.state==transfers::State::removed || link.destination.independent_evidence || link.destination.ledger_entry || link.destination.attached_evidence || link.destination.proposed_evidence || link.destination.accepted_unmatched)continue;
        const auto row=rows.find(link.origin.endpoint.entry);if(row==rows.end())continue;
        const auto message="Transfer recorded in "+account_by_id(link.destination.endpoint.account).name+"; bank transaction not yet matched";
        auto question=std::find_if(result.questions.begin(),result.questions.end(),[&](const auto& q){return q.kind==app::ReviewKind::link && !q.rows.empty() && q.rows.front().identity==row->first && q.waiting_link.empty();});
        if(question!=result.questions.end()){question->reason=message;question->waiting_link=link.identity;}
        else if(std::any_of(result.questions.begin(),result.questions.end(),[&](const auto& q){return q.kind==app::ReviewKind::link && q.rows.size()>1 && std::any_of(q.rows.begin(),q.rows.end(),[&](const auto& candidate){return candidate.identity==row->first;});}))continue;
        else result.questions.push_back({"waiting:"+link.identity,app::ReviewKind::link,row->second.account,row->second.account_name,message,row->first.batch,row->first.ordinal,{row->second},link.identity});
    }
    const auto acceptance=acceptance_review();
    for(const auto& warning:acceptance.warnings){
        if(warning.missing_destination)continue;
        if(warning.reason=="This transfer has not been matched to an imported transaction.")continue;
        if(warning.ordinal && (duplicate_rows.contains({warning.batch,*warning.ordinal}) || link_sources.contains({warning.batch,*warning.ordinal})))continue;
        if(warning.reason=="Review possible duplicates before committing")continue;
        const auto account=account_by_id(warning.account);
        const auto key="issue:"+std::to_string(warning.account)+":"+warning.batch+":"+(warning.ordinal?std::to_string(*warning.ordinal):"opening");
        if(std::any_of(result.questions.begin(),result.questions.end(),[&](const auto& q){return q.key==key;}))continue;
        result.questions.push_back({key,app::ReviewKind::correction,account.identity,account.name,warning.reason,warning.batch,warning.ordinal,{}});
    }
    std::stable_sort(result.questions.begin(),result.questions.end(),[](const auto& a,const auto& b){return a.account<b.account;});
    return result;
}
std::optional<app::ReviewReceipt> ImportStore::review_outcome(std::string_view operation) const {
    if(!impl_->composing_rule_save && !impl_->undo_joined && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement s(impl_->db,"SELECT id FROM review_answers WHERE id=?1 UNION ALL SELECT id FROM review_undos WHERE id=?1 UNION ALL SELECT operation FROM link_exclusion_events WHERE operation=?1 UNION ALL SELECT id FROM command_completions WHERE id=?1 AND domain IN('editor','selection','save-apply')");s.text(1,operation);if(!s.row())return {};return app::ReviewReceipt{std::string(operation)};
}
imports::DraftRow ImportStore::review_draft(const imports::Occurrence& id,std::int64_t revision) const {
    const auto original=load_row(id,false).source;const auto currency=account_for_batch(id.batch).currency;
    Statement s(impl_->db,"SELECT date,payee,memo,total FROM versions WHERE batch=? AND ordinal=? AND revision=?");s.text(1,id.batch);s.integer(2,integer(id.ordinal));s.integer(3,revision);if(!s.row())fail(SQLITE_CORRUPT);
    std::vector<Allocation> allocations;
    Statement aa(impl_->db,"SELECT category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");aa.text(1,id.batch);aa.integer(2,integer(id.ordinal));aa.integer(3,revision);
    while(aa.row())allocations.push_back({aa.bytes(0),aa.bytes(1),Money(aa.integer(2),currency)});
    imports::DraftRow draft{s.bytes(0),s.bytes(1),s.bytes(2),Transaction::repaired_import(original.original_amount,Money(s.integer(3),currency),std::move(allocations),original.source_currency && *original.source_currency!=currency)};
    load_metadata(impl_->db,id,revision,draft,false);return draft;
}
void ImportStore::write_review_row(std::string_view answer,std::string_view child,const imports::Occurrence& id,const imports::DraftRow& draft,const imports::Decision& decision,bool active,bool undoing,bool fresh_review,std::optional<std::int64_t> applied_ruleset,std::optional<Money> unchanged_balance,Money* running_balance,const app::CommittedEntry* known_current,const ReviewRestore* known_restore) {
    impl_->selected_account=account_for_batch(id.batch).identity;require_writable_account(impl_->db,impl_->selected_account);
    const bool committed=transaction_accepted(id);std::int64_t prior,result;
    const auto loaded_current=committed && !known_current?std::optional<app::CommittedEntry>{committed_entry(id)}:std::nullopt;
    const auto* current_entry=known_current?known_current:loaded_current?&*loaded_current:nullptr;
    const auto source=committed?current_entry->source:load_row(id,false).source;
    imports::validate_draft(source,draft,source.source_currency && *source.source_currency!=draft.transaction.effective_total().currency());
    std::optional<ReviewRestore> loaded_restore;
    if(undoing && !known_restore){
        Statement saved(impl_->db,"SELECT prior,result,child FROM review_members WHERE answer=? AND batch=? AND ordinal=?");saved.text(1,answer);saved.text(2,id.batch);saved.integer(3,integer(id.ordinal));if(!saved.row())fail(SQLITE_CORRUPT);
        loaded_restore=ReviewRestore{saved.integer(0),saved.integer(1),saved.bytes(2)};known_restore=&*loaded_restore;
    }
    if(committed){
        const auto& current=*current_entry;prior=current.current.revision;if(prior==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");result=prior+1;
        if(undoing){const auto expected=known_restore->result==current.current.revision?current.current:entry_snapshot(id,known_restore->result,&source);if(!review_restores_revision(impl_->db,id,true,current.current.revision,known_restore->result) || expected.active!=current.current.active || !same_draft(expected.draft,current.current.draft))fail(SQLITE_CONSTRAINT,"Later transaction changes prevent Review Undo");}
        auto after=current.current;after.revision=result;after.active=active;after.draft=draft;
        const auto before=running_balance?*running_balance:unchanged_balance?*unchanged_balance:committed_balance();const Money zero(0,before.currency());
        const auto delta=(active?draft.transaction.effective_total():zero)-(current.current.active?current.current.draft.transaction.effective_total():zero);
        if(unchanged_balance && delta.units()!=0)fail(SQLITE_CONSTRAINT,"Rule application cannot change the account balance");
        std::optional<std::int64_t> ruleset;
#ifdef SPENTIT_RULES_STORAGE
        std::int64_t rule_revision=prior;
        if(undoing)rule_revision=known_restore->prior;
        Statement mark(impl_->db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");mark.text(1,id.batch);mark.integer(2,integer(id.ordinal));mark.integer(3,rule_revision);if(mark.row())ruleset=mark.integer(0);
#endif
        if(applied_ruleset)ruleset=applied_ruleset;
        store_entry(impl_->db,id,after,ruleset);
        std::string undo_of;
        if(undoing)undo_of=known_restore->child;
        append_financial(impl_->db,child,undoing?app::ChangeKind::undo:active?app::ChangeKind::correct:app::ChangeKind::remove,id,prior,result,before,delta,before+delta,undo_of,financial_sequence(impl_->db),catalog_sequence(impl_->db));
        if(running_balance)*running_balance=before+delta;
        for(const auto& dependency:current.token.dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,child);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
    }else{
        Statement v(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");v.text(1,id.batch);v.integer(2,integer(id.ordinal));if(!v.row() || v.null(0))fail(SQLITE_CORRUPT);prior=v.integer(0);
        if(undoing && !review_restores_revision(impl_->db,id,false,prior,known_restore->result))fail(SQLITE_CONSTRAINT,"Later transaction changes prevent Undo");
        const auto info=batch(impl_->db,id.batch);if(info.revision==INT64_MAX)fail(SQLITE_FULL);result=info.revision+1;
        const auto old=load_row(id,false);const auto kind=fresh_review && old.draft && same_draft(*old.draft,draft)?"decide":"save";
        std::optional<std::int64_t> ruleset;
#ifdef SPENTIT_RULES_STORAGE
        if(undoing){Statement original(impl_->db,"SELECT f.ruleset_revision FROM review_members m JOIN draft_rule_fields f ON f.batch=m.batch AND f.ordinal=m.ordinal AND f.revision=m.prior WHERE m.answer=? AND m.batch=? AND m.ordinal=?");original.text(1,answer);original.text(2,id.batch);original.integer(3,integer(id.ordinal));if(original.row())ruleset=original.integer(0);}
#endif
        if(applied_ruleset)ruleset=applied_ruleset;
        version(impl_->db,id,result,draft,decision,fresh_review,ruleset);advance(impl_->db,id.batch,info.revision,kind,static_cast<int>(info.state));
    }
    if(undoing){Statement member(impl_->db,"INSERT INTO review_undo_members SELECT id,?1,?2,?4,?5 FROM review_undos WHERE answer=?3");member.text(1,child);member.text(2,known_restore->child);member.text(3,answer);member.integer(4,prior);member.integer(5,result);member.done();}
    if(!undoing){Statement member(impl_->db,"INSERT INTO review_members VALUES(?,?,?,?,?,?,?)");member.text(1,answer);member.text(2,id.batch);member.integer(3,integer(id.ordinal));member.integer(4,committed?1:0);member.integer(5,prior);member.integer(6,result);member.text(7,child);member.done();}
}
app::ReviewReceipt ImportStore::answer_review(std::string_view operation,const app::ReviewAnswer& answer) {
    diag::Span span(diag::Code::review_write);
    identifier(operation);if(operation.size()>80)fail(SQLITE_MISUSE);const auto encoded=review_request(answer);
    if(const auto prior=review_outcome(operation)){Statement s(impl_->db,"SELECT request FROM review_answers WHERE id=?");s.text(1,operation);if(!s.row() || s.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Review retry changed its answer");return *prior;}
    auto sheet=review_sheet();if(sheet.generation==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");if(sheet.generation!=answer.generation)fail(SQLITE_CONSTRAINT,"The transactions changed. Review them again.");
    if(answer.question=="selected" && answer.action==app::ReviewAction::merge){
        const auto owner=account();app::ReviewQuestion manual{"selected",app::ReviewKind::same,owner.identity,owner.name,"Are these the same?","",{}, {}};
        const std::set<imports::Occurrence> wanted(answer.selected.begin(),answer.selected.end());
        for(const auto& row:current_ledger())if(wanted.contains(row.identity) && !row.identity.batch.starts_with("~transfer-"))manual.rows.push_back({row.identity,owner.identity,owner.name,row.draft,row.committed,batch(impl_->db,row.identity.batch).manual?"Manually entered":""});
        sheet.questions.push_back(std::move(manual));
    }
    return write_review_answer(operation,answer,sheet);
}
app::ReviewReceipt ImportStore::write_review_answer(std::string_view operation,const app::ReviewAnswer& answer,const app::ReviewSheet& sheet,std::string_view parent) {
    if(!parent.empty() && (sqlite3_get_autocommit(impl_->db) || answer.action!=app::ReviewAction::merge))fail(SQLITE_MISUSE,"A batch child requires its enclosing Merge transaction");
    const auto cache_changes=sqlite3_total_changes64(impl_->db);
    const auto encoded=review_request(answer);
    if(answer.action==app::ReviewAction::accept_missing){
        const auto question=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& q){return q.key==answer.question && !q.waiting_link.empty();});
        if(question==sheet.questions.end())fail(SQLITE_CONSTRAINT,"This waiting transfer has changed");
        const auto values=all_transfers();const auto old=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==question->waiting_link;});if(old==values.end())fail(SQLITE_CONSTRAINT);
        struct Guard {Impl& state;std::int64_t selected;~Guard(){state.selected_account=selected;state.composing_review=false;}} guard{*impl_,impl_->selected_account};
        Atomic transaction(impl_->db);
        Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),?,?,?,?)");receipt.text(1,operation);receipt.integer(2,4);receipt.text(3,answer.question);receipt.text(4,encoded);receipt.integer(5,answer.generation);receipt.done();
        impl_->selected_account=old->link.destination.endpoint.account;impl_->composing_review=true;
        app::TransferRequest request{};request.action=app::TransferAction::accept;request.link=old->link.identity;request.expected_sequence=transfer_sequence();
        const auto child=std::string(operation)+"-accept";apply_transfer(child,request);impl_->composing_review=false;
        const auto updated=all_transfers();const auto next=std::find_if(updated.begin(),updated.end(),[&](const auto& v){return v.link.identity==request.link;});
        Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");history.text(1,operation);history.text(2,request.link);history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(*next));history.text(5,child);history.integer(6,old->sequence);history.done();
        Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,answer.generation+1);stack.done();impl_->pending_review_action=operation;
        finish([&]{transaction.commit();});return {std::string(operation)};
    }
    const auto question=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& q){return q.key==answer.question;});if(question==sheet.questions.end())fail(SQLITE_CONSTRAINT,"This question has already changed");
    if(question->kind==app::ReviewKind::correction)fail(SQLITE_CONSTRAINT,"Correct the record before resolving it");
    const bool same=question->kind==app::ReviewKind::same;
    if(same!=(answer.action==app::ReviewAction::merge || answer.action==app::ReviewAction::separate))fail(SQLITE_CONSTRAINT,"Invalid Review answer");
    std::set<imports::Occurrence> selected(answer.selected.begin(),answer.selected.end());
    if(selected.size()!=answer.selected.size())fail(SQLITE_CONSTRAINT,"Select each transaction only once");
    if((answer.action==app::ReviewAction::separate && selected.empty()) || answer.action==app::ReviewAction::not_linked)for(const auto& row:question->rows)selected.insert(row.identity);
    const bool focused_separate=answer.action==app::ReviewAction::separate && !answer.selected.empty();
    if(focused_separate && answer.selected.size()>2)fail(SQLITE_CONSTRAINT,"Choose a pair or one transaction and all its matches");
    const auto separate_origin=focused_separate?answer.selected.front():imports::Occurrence{};
    if(focused_separate && selected.size()==1){
        const auto origin=std::find_if(question->rows.begin(),question->rows.end(),[&](const auto& r){return r.identity==separate_origin;});
        if(origin==question->rows.end())fail(SQLITE_CONSTRAINT,"Select a transaction in this question");
        selected.insert(origin->duplicate_candidates.begin(),origin->duplicate_candidates.end());
    }
    if(selected.size()<2)fail(SQLITE_CONSTRAINT,"Select at least two transactions");
    std::vector<app::ReviewTransaction> chosen;
    for(const auto& id:selected){const auto row=std::find_if(question->rows.begin(),question->rows.end(),[&](const auto& r){return r.identity==id;});if(row==question->rows.end())fail(SQLITE_CONSTRAINT,"Select transactions in this question");chosen.push_back(*row);}
    std::optional<imports::Occurrence> survivor=answer.action==app::ReviewAction::merge?answer.survivor:std::nullopt;
    if(answer.action==app::ReviewAction::merge){
        const auto committed=std::count_if(chosen.begin(),chosen.end(),[](const auto& r){return r.committed;});
        // Revalidate the survivor using current relationships and import order.
        for(auto& row:chosen){row.transfer_linked=transfer_linked(row.identity);Statement order(impl_->db,"SELECT min(sequence) FROM records WHERE batch=?");order.text(1,row.identity.batch);if(order.row())row.import_order=order.integer(0);}
        const auto linked=std::count_if(chosen.begin(),chosen.end(),[](const auto& r){return r.transfer_linked;});
        const auto manual=std::count_if(chosen.begin(),chosen.end(),[&](const auto& r){return batch(impl_->db,r.identity.batch).manual;});
        const auto protected_count=std::count_if(chosen.begin(),chosen.end(),[](const auto& r){return r.transfer_linked || r.committed || r.note=="Manually entered";});
        if(protected_count==1){
            const auto keep=std::find_if(chosen.begin(),chosen.end(),[](const auto& r){return r.transfer_linked || r.committed || r.note=="Manually entered";})->identity;
            if(survivor && *survivor!=keep)fail(SQLITE_CONSTRAINT,"Keep the established transaction or resolve it separately");
            survivor=keep;
        }
        if(manual && survivor && !batch(impl_->db,survivor->batch).manual)fail(SQLITE_CONSTRAINT,"Keep the manually entered transaction or resolve it separately");
        if(!survivor && !linked && !manual && !committed){
            const auto first=std::min_element(chosen.begin(),chosen.end(),[](const auto& a,const auto& b){return a.import_order<b.import_order;});
            if(first->import_order>0 && std::count_if(chosen.begin(),chosen.end(),[&](const auto& r){return r.import_order==first->import_order;})==1)survivor=first->identity;
        }
        if(!survivor || !selected.contains(*survivor))fail(SQLITE_CONSTRAINT,"Choose the transaction to keep");
        for(const auto& r:chosen){if(r.account!=chosen.front().account)fail(SQLITE_CONSTRAINT,"Merge transactions within the same account");if(r.draft.transaction.effective_total()!=chosen.front().draft.transaction.effective_total())fail(SQLITE_CONSTRAINT,"Amounts differ. Edit the transactions before merging.");if(r.identity!=*survivor && transfer_linked(r.identity))fail(SQLITE_CONSTRAINT,"Resolve the existing links before merging this transaction");}
    }
    struct Scope{Impl& state;std::int64_t selected;bool cleaning;~Scope(){state.selected_account=selected;state.cleaning=cleaning;state.composing_review=false;}} scope{*impl_,impl_->selected_account,impl_->cleaning};
    std::unique_ptr<Atomic> transaction;if(parent.empty())transaction=std::make_unique<Atomic>(impl_->db);
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),?,?,?,?)");receipt.text(1,operation);receipt.integer(2,static_cast<int>(answer.action));receipt.text(3,parent.empty()?answer.question:"batch-child:"+std::string(parent));receipt.text(4,encoded);receipt.integer(5,answer.generation);receipt.done();
    const auto choice=[&](int kind,const imports::Occurrence& id,bool resolved){Statement p(impl_->db,"INSERT INTO review_choices VALUES(?,?,?,?,?)");p.text(1,operation);p.integer(2,kind);p.text(3,id.batch);p.integer(4,integer(id.ordinal));p.integer(5,resolved?1:0);p.done();};
    std::size_t child=0;const auto next=[&]{return std::string(operation)+"-"+std::to_string(++child);};
    if(same){
        // Selected rows leave this question. Remaining rows retain their mutual
        // comparisons, including exactly two remaining candidates.
        if(focused_separate){
            const auto& origin=*std::find_if(chosen.begin(),chosen.end(),[&](const auto& r){return r.identity==separate_origin;});
            for(const auto& row:chosen)if(row.identity!=separate_origin && std::find(origin.duplicate_candidates.begin(),origin.duplicate_candidates.end(),row.identity)==origin.duplicate_candidates.end())fail(SQLITE_CONSTRAINT,"This pair is no longer a suggested match");
            for(const auto& row:chosen)choice(0,row.identity,row.identity==separate_origin);
        }else for(const auto& row:question->rows)choice(0,row.identity,selected.contains(row.identity));
        if(survivor)std::stable_sort(chosen.begin(),chosen.end(),[&](const auto& a,const auto& b){return a.identity==*survivor && b.identity!=*survivor;});
        for(const auto& row:chosen){
            impl_->selected_account=row.account;
            // A pair rejection must leave other comparisons and their unresolved
            // import decisions intact. Only resolved endpoints become included.
            if(focused_separate && std::any_of(row.duplicate_candidates.begin(),row.duplicate_candidates.end(),[&](const auto& other){return row.identity==separate_origin?!selected.contains(other):other!=separate_origin;}))continue;
            const bool loser=survivor && row.identity!=*survivor;
            if(loser){Statement mapping(impl_->db,"INSERT INTO review_merges VALUES(?,?,?,?,?)");mapping.text(1,row.identity.batch);mapping.integer(2,integer(row.identity.ordinal));mapping.text(3,survivor->batch);mapping.integer(4,integer(survivor->ordinal));mapping.text(5,operation);mapping.done();}
            // Capture sources before version triggers reassociate the row itself.
            if(loser){Statement sources(impl_->db,"INSERT INTO review_sources SELECT ?1,source_batch,source_ordinal,ledger_batch,ledger_ordinal,?2,?3 FROM source_associations WHERE ledger_batch=?4 AND ledger_ordinal=?5");sources.text(1,operation);sources.text(2,survivor->batch);sources.integer(3,integer(survivor->ordinal));sources.text(4,row.identity.batch);sources.integer(5,integer(row.identity.ordinal));sources.done();}
            if(!row.committed || loser)write_review_row(operation,next(),row.identity,row.draft,{loser?imports::Disposition::duplicate:imports::Disposition::include,loser?survivor:std::nullopt},!loser);
        }
        impl_->cleaning=true;
        Statement ss(impl_->db,"SELECT source_batch,source_ordinal,result_batch,result_ordinal FROM review_sources WHERE answer=?");ss.text(1,operation);
        while(ss.row()){Statement del(impl_->db,"DELETE FROM source_associations WHERE source_batch=? AND source_ordinal=?");del.text(1,ss.bytes(0));del.integer(2,ss.integer(1));del.done();Statement add(impl_->db,"INSERT INTO source_associations VALUES(?,?,?,?)");add.text(1,ss.bytes(2));add.integer(2,ss.integer(3));add.text(3,ss.bytes(0));add.integer(4,ss.integer(1));add.done();}
        impl_->cleaning=scope.cleaning;
    }else{
        const auto& origin=question->rows.front();
        for(const auto& row:question->rows)choice(1,row.identity,row.identity==origin.identity);
        if(answer.action==app::ReviewAction::link){
            if(selected.size()!=2 || !selected.contains(origin.identity))fail(SQLITE_CONSTRAINT,"Select one counterpart");
            auto left=origin;auto right=*std::find_if(chosen.begin(),chosen.end(),[&](const auto& r){return r.identity!=origin.identity;});
            const auto allocation=[](const app::ReviewTransaction& row,const std::string& name)->Allocation {
                const auto& aa=row.draft.transaction.allocations();if(name.empty() && aa.size()==1)return aa.front();
                const auto it=std::find_if(aa.begin(),aa.end(),[&](const auto& a){return a.identity==name;});if(it==aa.end())fail(SQLITE_CONSTRAINT,"Choose the split to link");return *it;
            };
            const auto prior=all_transfers();
            auto a=allocation(left,answer.left_allocation),b=allocation(right,answer.right_allocation);
            if(a.amount.currency()!=b.amount.currency() || a.amount.units()==0 || b.amount.units()==0 || (a.amount.units()<0)==(b.amount.units()<0))fail(SQLITE_CONSTRAINT,"Choose opposite amounts in the same currency");
            const auto& outgoing=a.amount.units()<0?a:b;const auto destination_account=a.amount.units()<0?right.account:left.account;
            const auto destination_name=transfer_name(outgoing.category);
            if(review_opening_balance(left.draft) || review_opening_balance(right.draft) || !review_link_category(a.category,right.account) || !review_link_category(b.category,left.account) || imports::posting_distance(left.draft.date,right.draft.date)>7 || !imports::link_amount_candidate(a.amount,b.amount,destination_name && resolve_account_name(*destination_name)==destination_account))fail(SQLITE_CONSTRAINT,"These allocations do not meet this suggestion's matching policy; use explicit manual Link instead");
            const auto magnitude=[](std::int64_t n)->std::uint64_t{return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);};
            if(magnitude(a.amount.units())!=magnitude(b.amount.units())){
                if(!answer.split_difference)fail(SQLITE_CONSTRAINT,"Confirm how to categorise the difference");
                auto& large=magnitude(a.amount.units())>magnitude(b.amount.units())?left:right;
                auto& part=magnitude(a.amount.units())>magnitude(b.amount.units())?a:b;
                const auto& small=magnitude(a.amount.units())>magnitude(b.amount.units())?b:a;
                const auto common=Money(-small.amount.units(),part.amount.currency());const auto remainder=part.amount-common;
                std::optional<app::TransferView> generated;
                for(const auto& v:prior)if(!v.link.unresolved && v.link.origin.endpoint.entry==large.identity && v.link.origin.endpoint.allocation==part.identity && !v.link.destination.independent_evidence && !v.link.destination.ledger_entry && !v.link.destination.attached_evidence && !v.link.destination.proposed_evidence)generated=v;
                if(transfer_linked(large.identity) && !generated)fail(SQLITE_CONSTRAINT,"Resolve the existing link before changing its allocation");
                std::string category;std::optional<CategoryBinding> binding;
                if(!answer.remainder_category.empty()){const auto display=resolve_category(answer.remainder_category);if(!display.assignable)fail(SQLITE_CONSTRAINT,"Choose an available category");category=display.path;binding=CategoryBinding{answer.remainder_category,catalog_sequence(impl_->db)};}
                auto aa=large.draft.transaction.allocations();for(auto& item:aa)if(item.identity==part.identity)item.amount=common;
                Allocation extra{category,"",remainder,binding,std::string(operation)+"-remainder"};extra.category_owner=FieldOwnership::manual;extra.memo_owner=FieldOwnership::manual;aa.push_back(extra);
                large.draft.transaction=large.draft.transaction.corrected(large.draft.transaction.effective_total(),std::move(aa));
                impl_->selected_account=large.account;write_review_row(operation,next(),large.identity,large.draft,{imports::Disposition::include,{}},true);part.amount=common;
                if(generated){
                    app::TransferRequest resize{};resize.action=app::TransferAction::resize_generated;resize.link=generated->link.identity;resize.amount=common;resize.expected_sequence=transfer_sequence();
                    // The local generated amount belongs to the destination account.
                    impl_->selected_account=generated->link.destination.endpoint.account;impl_->composing_review=true;apply_transfer(next(),resize);impl_->composing_review=false;
                }
            }
            // If both imported sides independently generated placeholders, retire
            // the second relationship before joining the two real postings.
            std::optional<app::TransferView> first_generated,second_generated;
            for(const auto& value:all_transfers())if(!value.link.unresolved && value.link.origin.state!=transfers::State::removed && !value.link.destination.independent_evidence && !value.link.destination.ledger_entry && !value.link.destination.attached_evidence && !value.link.destination.proposed_evidence){
                if(value.link.origin.endpoint.entry==left.identity && value.link.origin.endpoint.allocation==a.identity)first_generated=value;
                if(value.link.origin.endpoint.entry==right.identity && value.link.origin.endpoint.allocation==b.identity)second_generated=value;
            }
            if(first_generated && second_generated && first_generated->link.identity!=second_generated->link.identity){
                app::TransferRequest release{};release.action=app::TransferAction::release_generated;release.link=second_generated->link.identity;release.expected_sequence=transfer_sequence();
                impl_->selected_account=second_generated->link.destination.endpoint.account;impl_->composing_review=true;apply_transfer(next(),release);impl_->composing_review=false;
            }
            app::TransferRequest request{};request.action=app::TransferAction::merge;request.link=std::string(operation)+"-link";request.kept=std::string(operation)+"-kept";request.expected_sequence=transfer_sequence();
            request.left=transfers::AllocationFact{{left.account,left.identity,a.identity},a.amount,left.draft.date,left.committed?transfers::State::committed:transfers::State::draft,true,false,false};
            request.right=transfers::AllocationFact{{right.account,right.identity,b.identity},b.amount,right.draft.date,right.committed?transfers::State::committed:transfers::State::draft,true,false,false};
            for(auto* fact:{&*request.left,&*request.right}){const bool manual=batch(impl_->db,fact->endpoint.entry.batch).manual;fact->imported=!manual;fact->manually_entered=manual;fact->ledger_entry=manual;}
            request.batch_revision=batch(impl_->db,left.identity.batch).revision;request.right_revision=batch(impl_->db,right.identity.batch).revision;
            impl_->selected_account=left.account;
            for(const auto& v:all_transfers())if(!v.link.unresolved && v.link.origin.state!=transfers::State::removed && (v.link.origin.endpoint==request.left->endpoint || v.link.origin.endpoint==request.right->endpoint)){request.link=v.link.identity;if(v.link.destination.state==transfers::State::committed && !v.link.destination.independent_evidence && !v.link.destination.ledger_entry)impl_->selected_account=v.link.destination.endpoint.account;}
            const auto link_operation=next();impl_->composing_review=true;apply_transfer(link_operation,request);impl_->composing_review=false;
            for(const auto& value:all_transfers()){
                const auto old=std::find_if(prior.begin(),prior.end(),[&](const auto& v){return v.link.identity==value.link.identity;});const auto payload=transfer_codec::encode(value);
                if(old!=prior.end() && payload==transfer_codec::encode(*old))continue;
                Statement h(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");h.text(1,operation);h.text(2,value.link.identity);if(old!=prior.end())h.blob(3,transfer_codec::encode(*old));h.blob(4,payload);Statement saved_link(impl_->db,"SELECT l.id FROM ledger_operations l JOIN transfer_heads t ON t.sequence=l.sequence WHERE t.identity=?");saved_link.text(1,value.link.identity);if(!saved_link.row())fail(SQLITE_CORRUPT);h.text(5,saved_link.bytes(0));h.integer(6,old==prior.end()?0:old->sequence);h.done();
            }
        }
    }
    if(parent.empty()){Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,answer.generation+1);stack.done();impl_->pending_review_action=operation;finish([&]{transaction->commit();});if(same)retain_review_ledger_entries(*impl_,cache_changes,selected);}
    return {std::string(operation)};
}
app::ReviewReceipt ImportStore::undo_review(std::string_view operation) {
    ScopedStatements statements(impl_->db);
    session_snapshot::retry(impl_->db,operation,"review-undo",session_snapshot::digest(operation));
    identifier(operation);if(operation.size()>80)fail(SQLITE_MISUSE);if(const auto prior=review_outcome(operation)){Statement saved(impl_->db,"SELECT 1 FROM review_undos WHERE id=?");saved.text(1,operation);if(!saved.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another Review command");return *prior;}
    std::string answer;std::int64_t generation=0;bool top_undo=false;
    if(!impl_->undo_target.empty()) {
        Statement chosen(impl_->db,"SELECT generation FROM review_stack WHERE answer=?");chosen.text(1,impl_->undo_target);
        if(!chosen.row())fail(SQLITE_CONSTRAINT,"There is no Review decision to undo");
        answer=impl_->undo_target;generation=chosen.integer(0);
        Statement top(impl_->db,"SELECT answer FROM review_stack ORDER BY (SELECT sequence FROM review_answers WHERE id=answer) DESC LIMIT 1");
        top_undo=top.row() && top.bytes(0)==answer;
    } else {
        Statement top(impl_->db,"SELECT answer,generation FROM review_stack ORDER BY (SELECT sequence FROM review_answers WHERE id=answer) DESC LIMIT 1");
        if(!top.row())fail(SQLITE_CONSTRAINT,"There is no Review decision to undo");
        answer=top.bytes(0);generation=top.integer(1);top_undo=true;
        const auto common_head=action_head();
        if(!common_head || common_head->id!=answer)fail(SQLITE_CONSTRAINT,"Undo the latest action first.");
        if(!impl_->undo_token) {
            const auto result=undo_action(operation,preview_undo(answer,action_token()));
            return {result.operation};
        }
    }
    if(generation==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
    struct Scope{Impl& state;std::int64_t selected;bool cleaning;~Scope(){state.selected_account=selected;state.cleaning=cleaning;}} scope{*impl_,impl_->selected_account,impl_->cleaning};
    const auto cache_changes=sqlite3_total_changes64(impl_->db);
    std::set<imports::Occurrence> cache_changed;
    Statement cache_rows(impl_->db,"SELECT batch,ordinal FROM review_choices WHERE answer=? AND kind=0");cache_rows.text(1,answer);
    while(cache_rows.row())cache_changed.insert({cache_rows.bytes(0),static_cast<std::size_t>(cache_rows.integer(1))});
    Atomic transaction(impl_->db,impl_->undo_joined?&impl_->undo_depth:nullptr);
    Statement parent(impl_->db,"SELECT question,request,action FROM review_answers WHERE id=?");parent.text(1,answer);if(!parent.row())fail(SQLITE_CORRUPT);
    Statement children(impl_->db,"SELECT id,request FROM review_answers WHERE question=? ORDER BY sequence");children.text(1,"batch-child:"+answer);
    std::vector<std::string> ids;std::string encoded="batch:";
    while(children.row()){
        ids.push_back(children.bytes(0));const auto request=children.bytes(1);encoded+=std::to_string(request.size())+":"+request;
        if(ids.back()!=answer+"-group-"+std::to_string(ids.size()))fail(SQLITE_CORRUPT,"Batch Review history is inconsistent");
    }
    if(parent.bytes(0)=="batch"?(ids.empty() || encoded!=parent.bytes(1)):!ids.empty())fail(SQLITE_CORRUPT,"Batch Review history is incomplete");
    std::reverse(ids.begin(),ids.end());
    restore_review_answer(operation,answer);
    if(parent.bytes(0)=="ledger-delete"){
        Statement unlink(impl_->db,"SELECT id,request FROM review_answers WHERE question=?");unlink.text(1,"ledger-delete-links:"+answer);
        if(unlink.row()){
            if(unlink.bytes(0)!=answer+"-unlink" || unlink.bytes(1)!=parent.bytes(1))fail(SQLITE_CORRUPT,"Deletion history is inconsistent");
            restore_review_answer(std::string(operation)+"-unlink",unlink.bytes(0));
            if(unlink.row())fail(SQLITE_CORRUPT,"Deletion history is inconsistent");
        }
        Statement kept(impl_->db,"SELECT id,request FROM review_answers WHERE question=?");kept.text(1,"ledger-delete-kept:"+answer);
        if(kept.row()){
            if(kept.bytes(0)!=answer+"-kept" || kept.bytes(1)!=parent.bytes(1))fail(SQLITE_CORRUPT,"Deletion history is inconsistent");
            restore_review_answer(std::string(operation)+"-kept",kept.bytes(0));
            if(kept.row())fail(SQLITE_CORRUPT,"Deletion history is inconsistent");
        }
    }
    std::size_t child=0;for(const auto& id:ids)restore_review_answer(std::string(operation)+"-group-"+std::to_string(++child),id);
    if(impl_->legacy_undo_finalize)impl_->legacy_undo_finalize();
    Statement pop(impl_->db,"DELETE FROM review_stack WHERE answer=?");pop.text(1,answer);pop.done();
    // Reversing the top decision makes the preceding stack entry current again.
    if(top_undo){Statement prior(impl_->db,"UPDATE review_stack SET generation=? WHERE answer=(SELECT answer FROM review_stack ORDER BY (SELECT sequence FROM review_answers WHERE id=answer) DESC LIMIT 1) AND generation=(SELECT generation FROM review_answers WHERE id=?2)");prior.integer(1,generation+1);prior.text(2,answer);prior.done();}
    finish([&]{transaction.commit();});
    if((parent.integer(2)==0 || parent.integer(2)==1) && (parent.bytes(0).starts_with("same:") || parent.bytes(0)=="selected"))retain_review_ledger_entries(*impl_,cache_changes,cache_changed);
    return {std::string(operation)};
}

void ImportStore::restore_review_answer(std::string_view operation,std::string_view answer) {
    const auto prior_cleaning=impl_->cleaning;
    Statement receipt(impl_->db,"INSERT INTO review_undos VALUES(?,?)");receipt.text(1,operation);receipt.text(2,answer);receipt.done();
    {Statement exclusion(impl_->db,"SELECT e.identity,e.field,e.expression,e.account,e.regex,e.removed,e.sequence FROM link_exclusion_events e JOIN review_answers a ON a.id=e.operation WHERE e.operation=? AND a.question='review-exclusion'");exclusion.text(1,answer);
     if(exclusion.row()){
        const auto identity=exclusion.bytes(0);auto field=exclusion.bytes(1),expression=exclusion.bytes(2);auto account=exclusion.integer(3),regex=exclusion.integer(4),removed=std::int64_t{1};
        Statement previous(impl_->db,"SELECT field,expression,account,regex,removed FROM link_exclusion_events WHERE identity=? AND sequence<? ORDER BY sequence DESC LIMIT 1");previous.text(1,identity);previous.integer(2,exclusion.integer(6));
        if(previous.row()){field=previous.bytes(0);expression=previous.bytes(1);account=previous.integer(2);regex=previous.integer(3);removed=previous.integer(4);}
        Statement inverse(impl_->db,"INSERT INTO link_exclusion_events VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM link_exclusion_events),?,?,?,?,?,?,?)");inverse.text(1,std::string(operation)+"-exclusion");inverse.text(2,identity);inverse.text(3,field);inverse.text(4,expression);inverse.integer(5,account);inverse.integer(6,regex);inverse.integer(7,removed);inverse.integer(8,ledger_edit_generation());inverse.done();
     }}
    // Restore transfer relationships first, then allocation snapshots. All writes
    // remain inside this single transaction, so no intermediate state is visible.
    Statement links(impl_->db,"SELECT identity,prior_payload,result_payload,operation,prior_sequence FROM review_link_history WHERE answer=?");links.text(1,answer);
    std::vector<app::TransferView> restored;
    std::map<std::string,std::pair<std::int64_t,std::int64_t>> link_lineage;
    while(links.row()){
        Statement original(impl_->db,"SELECT l.sequence,h.sequence,(SELECT coalesce(max(v.sequence),0) FROM transfer_versions v WHERE v.identity=h.identity AND v.sequence<l.sequence) FROM ledger_operations l JOIN transfer_heads h ON h.identity=?2 WHERE l.id=?1");original.text(1,links.bytes(3));original.text(2,links.bytes(0));
        if(!original.row() || !review_restores_link(impl_->db,links.bytes(0),original.integer(1),original.integer(0)))fail(SQLITE_CONSTRAINT,"Undo later transfer changes first.");
        link_lineage.emplace(links.bytes(0),std::pair{original.integer(1),links.integer(4)});
        auto view=transfer_codec::view(links.bytes(2));
        if(!links.null(1))view=transfer_codec::view(links.bytes(1));
        else {undo_new_relationship(view);}
        restored.push_back(std::move(view));
    }
    if(!restored.empty()){
        const auto current=all_transfers();
        auto working=current;std::map<std::int64_t,Money> link_balances;
        const auto active_count=[&](const app::TransferView& value){const auto found=std::find_if(current.begin(),current.end(),[&](const auto& v){return v.link.identity==value.link.identity;});if(found==current.end())fail(SQLITE_CORRUPT);return int(found->link.origin.state!=transfers::State::removed)+int(found->link.destination.state!=transfers::State::removed);};
        std::stable_sort(restored.begin(),restored.end(),[&](const auto& a,const auto& b){return active_count(a)>active_count(b);});
        std::size_t link_serial=0;
        for(const auto& target:restored){
            const auto initial=std::find_if(working.begin(),working.end(),[&](const auto& v){return v.link.identity==target.link.identity;});if(initial==working.end())fail(SQLITE_CORRUPT);
            const auto origin_account=target.link.origin.endpoint.account,destination_account=target.link.destination.endpoint.account;
            const bool two_accounts=(local_total({target},origin_account,account_by_id(origin_account).currency,false)-local_total({*initial},origin_account,account_by_id(origin_account).currency,false)).units()!=0 && (local_total({target},destination_account,account_by_id(destination_account).currency,false)-local_total({*initial},destination_account,account_by_id(destination_account).currency,false)).units()!=0;
            auto intermediate=target;intermediate.link.destination=initial->link.destination;
            std::int64_t intermediate_sequence=0;
            for(int stage=two_accounts?1:0;stage<=(two_accounts?2:0);++stage){
            const auto& view=stage==1?intermediate:target;
            const auto old=std::find_if(working.begin(),working.end(),[&](const auto& v){return v.link.identity==view.link.identity;});if(old==working.end())fail(SQLITE_CORRUPT);
            const auto origin=view.link.origin.endpoint.account,destination=view.link.destination.endpoint.account;
            const auto origin_currency=account_by_id(origin).currency;
            const auto origin_delta=local_total({view},origin,origin_currency,false)-local_total({*old},origin,origin_currency,false);
            impl_->selected_account=origin_delta.units()!=0?origin:destination;
            if(origin_delta.units()!=0 && (local_total({view},destination,account_by_id(destination).currency,false)-local_total({*old},destination,account_by_id(destination).currency,false)).units()!=0)fail(SQLITE_CORRUPT,"A transfer Undo changes two account balances at once");
            auto balance=link_balances.find(impl_->selected_account);if(balance==link_balances.end())balance=link_balances.emplace(impl_->selected_account,committed_balance()).first;
            const auto before=balance->second;
            const auto prior_local=local_total(working,impl_->selected_account,before.currency(),false);
            for(auto& value:working)if(value.link.identity==view.link.identity)value=view;
            validate_transfer_set(working);
            const auto delta=local_total(working,impl_->selected_account,before.currency(),false)-prior_local;
            balance->second=before+delta;
            const auto previous_sequence=financial_sequence(impl_->db);if(previous_sequence==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");const auto sequence=previous_sequence+1;
            const auto child=std::string(operation)+"-links-"+std::to_string(++link_serial);
            app::TransferRequest request{};request.action=app::TransferAction::review_undo;request.link=answer;request.allocation=view.link.identity;request.expected_sequence=sequence-1;request.mode=stage;
            Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,child);event.integer(2,impl_->selected_account);event.blob(3,transfer_codec::encode(request));event.integer(4,delta.units()!=0?1:0);event.integer(5,delta.units());event.done();
            store_transfer(impl_->db,view,sequence,false);
            const auto lineage=link_lineage.at(view.link.identity);
            if(stage==1)intermediate_sequence=sequence;
            else { Statement restoration(impl_->db,"INSERT INTO review_link_restorations VALUES(?,?,?,?,?,?)");restoration.text(1,operation);restoration.text(2,view.link.identity);restoration.integer(3,lineage.first);restoration.integer(4,sequence);restoration.integer(5,lineage.second);restoration.integer(6,intermediate_sequence);restoration.done();}
            append_financial(impl_->db,child,app::ChangeKind::transfer,{"",0},-1,0,before,delta,before+delta,"",sequence-1,catalog_sequence(impl_->db));
            }
        }
    }
    Statement edit(impl_->db,"SELECT 1 FROM review_answers WHERE id=? AND (question IN('ledger-edit','ledger-edit-batch','ledger-delete','rules-account') OR question LIKE 'ledger-delete-kept:%')");edit.text(1,answer);const bool editor_undo=edit.row();
    Statement rules_group(impl_->db,"SELECT 1 FROM review_answers WHERE id=? AND question='rules-account'");rules_group.text(1,answer);const bool rules_undo=rules_group.row();
    std::map<std::int64_t,Money> rule_balances, balances;
    std::size_t serial=0;
    Statement root_kind(impl_->db,"SELECT question FROM review_answers WHERE id=?");root_kind.text(1,answer);if(!root_kind.row())fail(SQLITE_CORRUPT);
    const bool direct=root_kind.bytes(0)=="ledger-committed" || (root_kind.bytes(0)=="ledger-edit" && impl_->legacy_undo_domain=="committed");
    const bool grouped=root_kind.bytes(0)=="ledger-bulk" || root_kind.bytes(0)=="rules-reapply";
    const auto next_child=[&]{if(direct)return std::string(operation);if(grouped){const auto index=serial++;return index==0?std::string(operation):std::string(operation)+"-"+std::to_string(index);}return std::string(operation)+"-"+std::to_string(++serial);};
    Statement members(impl_->db,grouped?"SELECT batch,ordinal,committed,prior,result,child FROM review_members WHERE answer=? ORDER BY rowid":"SELECT batch,ordinal,committed,prior,result,child FROM review_members WHERE answer=? ORDER BY rowid DESC");members.text(1,answer);
    while(members.row()){
        const imports::Occurrence id{members.bytes(0),static_cast<std::size_t>(members.integer(1))};impl_->selected_account=account_for_batch(id.batch).identity;
        if(members.integer(2)){
            const auto current=committed_entry(id);
            const ReviewRestore restore{members.integer(3),members.integer(4),members.bytes(5)};
            const auto before=entry_snapshot(id,restore.prior,&current.source);
            std::optional<Money> unchanged;
            if(rules_undo){auto it=rule_balances.find(impl_->selected_account);if(it==rule_balances.end())it=rule_balances.emplace(impl_->selected_account,committed_balance()).first;unchanged=it->second;}
            auto balance=balances.find(impl_->selected_account);
            if(balance==balances.end())balance=balances.emplace(impl_->selected_account,committed_balance()).first;
            write_review_row(answer,next_child(),id,before.draft,{imports::Disposition::include,{}},before.active,true,true,{},unchanged,&balance->second,&current,&restore);
        }else{
            Statement decision(impl_->db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? AND revision=?");decision.text(1,id.batch);decision.integer(2,integer(id.ordinal));decision.integer(3,members.integer(3));if(!decision.row())fail(SQLITE_CORRUPT);
            imports::Decision value{static_cast<imports::Disposition>(decision.integer(0)),{}};if(value.disposition==imports::Disposition::duplicate)value.duplicate_of=imports::Occurrence{decision.bytes(1),static_cast<std::size_t>(decision.integer(2))};
            write_review_row(answer,next_child(),id,review_draft(id,members.integer(3)),value,true,true,!editor_undo);
        }
    }
    impl_->cleaning=true;
    // A direct bank-source match owns only the association it created. The
    // source row remains and becomes visible again when that action is undone.
    {Statement matches(impl_->db,"DELETE FROM generated_source_matches AS g WHERE EXISTS(SELECT 1 FROM review_link_history h JOIN ledger_operations l ON l.id=h.operation WHERE h.answer=? AND h.identity=g.link AND g.sequence>h.prior_sequence AND g.sequence<=l.sequence)");matches.text(1,answer);matches.done();}
    {Statement repair(impl_->db,"SELECT batch,ordinal,blocked,result FROM review_repairs WHERE answer=?");repair.text(1,answer);
     if(repair.row()){
        const imports::Occurrence id{repair.bytes(0),static_cast<std::size_t>(repair.integer(1))};const auto state=batch(impl_->db,id.batch);
        Statement latest(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");latest.text(1,id.batch);latest.integer(2,integer(id.ordinal));if(!latest.row() || !review_restores_revision(impl_->db,id,false,latest.integer(0),repair.integer(3)))fail(SQLITE_CONSTRAINT,"Undo later transaction changes first.");
        {Statement inverse(impl_->db,"DELETE FROM review_undo_members WHERE original_child IN(SELECT child FROM review_members WHERE batch=? AND ordinal=?)");inverse.text(1,id.batch);inverse.integer(2,integer(id.ordinal));inverse.done();Statement members(impl_->db,"DELETE FROM review_members WHERE batch=? AND ordinal=?");members.text(1,id.batch);members.integer(2,integer(id.ordinal));members.done();}
        for(const auto* table:{"draft_rule_allocations","draft_rule_fields","draft_allocation_meta","draft_fields","allocations","review_stamps","versions","records"}){
#ifndef SPENTIT_RULES_STORAGE
            if(std::string_view(table).starts_with("draft_rule"))continue;
#endif
            const auto sql="DELETE FROM "+std::string(table)+" WHERE batch=? AND ordinal=?";Statement remove(impl_->db,sql.c_str());remove.text(1,id.batch);remove.integer(2,integer(id.ordinal));remove.done();
        }
        advance(impl_->db,id.batch,state.revision,"unrepair",static_cast<int>(state.state),static_cast<int>(repair.integer(2)));
     }}
    Statement sources(impl_->db,"SELECT source_batch,source_ordinal,prior_batch,prior_ordinal FROM review_sources WHERE answer=?");sources.text(1,answer);
    while(sources.row()){
        Statement del(impl_->db,"DELETE FROM source_associations WHERE source_batch=? AND source_ordinal=?");del.text(1,sources.bytes(0));del.integer(2,sources.integer(1));del.done();
        Statement add(impl_->db,"INSERT INTO source_associations VALUES(?,?,?,?)");add.text(1,sources.bytes(2));add.integer(2,sources.integer(3));add.text(3,sources.bytes(0));add.integer(4,sources.integer(1));add.done();
    }
    impl_->cleaning=prior_cleaning;
    Statement merged(impl_->db,"DELETE FROM review_merges WHERE answer=?");merged.text(1,answer);merged.done();
}

app::ReviewReceipt ImportStore::merge_review_batch(std::string_view operation,const std::vector<app::ReviewAnswer>& answers,const app::OperationContext& context) {
    diag::Span span(diag::Code::review_write);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(answers.empty())fail(SQLITE_CONSTRAINT,"Select at least one match group");
    std::string encoded="batch:";
    std::set<std::string> questions;std::set<imports::Occurrence> selected;
    for(const auto& answer:answers){
        context.checkpoint("Checking selected matches");
        if(answer.action!=app::ReviewAction::merge || answer.question=="selected")fail(SQLITE_CONSTRAINT,"Batch Merge requires duplicate match groups");
        if(!questions.insert(answer.question).second)fail(SQLITE_CONSTRAINT,"Select each match group only once");
        for(const auto& id:answer.selected)if(!selected.insert(id).second)fail(SQLITE_CONSTRAINT,"Select each transaction only once in a batch");
        const auto request=review_request(answer);encoded+=std::to_string(request.size())+":"+request;
    }
    if(const auto prior=review_outcome(operation)){Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=?");saved.text(1,operation);if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Batch retry changed its selections");return *prior;}
    context.checkpoint("Checking selected matches");
    const auto sheet=review_sheet();if(sheet.generation==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
    for(std::size_t i=0;i<answers.size();++i)if(answers[i].generation!=sheet.generation)fail(SQLITE_CONSTRAINT,("Selected match "+std::to_string(i+1)+" is stale. Review the current groups again.").c_str());
    Atomic transaction(impl_->db);
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),1,'batch',?,?)");receipt.text(1,operation);receipt.text(2,encoded);receipt.integer(3,sheet.generation);receipt.done();
    for(std::size_t i=0;i<answers.size();++i){
        context.checkpoint("Merging selected matches");
        try{write_review_answer(std::string(operation)+"-group-"+std::to_string(i+1),answers[i],sheet,operation);}
        catch(const storage::ImportError& error){fail(error.code(),("Selected match "+std::to_string(i+1)+": "+error.what()).c_str());}
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,sheet.generation+1);stack.done();impl_->pending_review_action=operation;
    context.checkpoint("Validating merged matches");
    finish([&]{context.seal();transaction.commit();});
    return {std::string(operation)};
}

// Category assignment uses the same durable relationship snapshots and mutation
// barrier as Review Link. Child transfers never commit outside this transaction.
bool ImportStore::can_undo_transfer_assignment() const {
    Statement top(impl_->db,"SELECT a.id,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    return top.row() && top.bytes(1)=="transfer-assignment" && action_head() && action_head()->kind==app::ActionKind::review && action_head()->id==top.bytes(0);
}
app::ReviewReceipt ImportStore::assign_categories(std::string_view operation,const app::CategorySelection& selection,const app::OperationContext& context) {
    identifier(operation);
    if(selection.account!=impl_->selected_account || selection.selected.empty())
        fail(SQLITE_CONSTRAINT,"Select at least one transaction in this account");
    // Compare retries with the saved command, without rereading now-changed rows.
    if(const auto receipt=review_outcome(operation)) {
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='transfer-assignment'");saved.text(1,operation);
        if(!saved.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
        const auto bytes=saved.bytes(0);constexpr std::string_view prefix="transfer-assignment:";
        if(!bytes.starts_with(prefix))fail(SQLITE_CORRUPT);
        transfer_codec::Reader in{std::string_view(bytes).substr(prefix.size())};
        for(const auto& target:selection.selected) {
            const auto request=transfer_codec::request(in.text());
            if(request.mode!=0 || !request.left || request.left->endpoint.account!=selection.account || request.left->endpoint.entry!=target.identity || request.allocation!=target.allocation || request.category_id!=selection.category || request.category_generation!=selection.generation)
                fail(SQLITE_CONSTRAINT,"Category assignment retry changed its selections");
        }
        if(!in.bytes.empty())fail(SQLITE_CONSTRAINT,"Category assignment retry changed its selections");
        return *receipt;
    }
    if(selection.generation!=ledger_edit_generation())fail(SQLITE_CONSTRAINT,"Transactions changed; select them again");
    std::string category_path;
    if(!selection.category.empty()){const auto category=resolve_category(selection.category);if(!category.assignable)fail(SQLITE_CONSTRAINT,"Choose an available category");category_path=category.path;}
    const auto links=all_transfers();std::vector<app::TransferRequest> requests;std::size_t serial=0;
    for(const auto& target:selection.selected) {
        context.checkpoint("Reading selected transactions");require_account(target.identity.batch);
        const auto info=batch(impl_->db,target.identity.batch);const bool accepted=transaction_accepted(target.identity);
        const auto row=load_row(target.identity,false);if(!row.draft || row.excluded_by_clear)fail(SQLITE_CONSTRAINT,"Transaction is unavailable");
        const auto draft=accepted?committed_entry(target.identity).current.draft:*row.draft;
        const auto& allocations=draft.transaction.allocations();
        if(target.allocation.empty() && allocations.size()!=1)fail(SQLITE_CONSTRAINT,"Choose which split to change");
        const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return target.allocation.empty() || a.identity==target.allocation;});
        if(allocation==allocations.end())fail(SQLITE_CONSTRAINT,"Choose which split to change");
        const bool imported=!info.manual;
        app::TransferRequest request{};request.action=app::TransferAction::retarget;request.mode=0;
        request.link=std::string(operation)+"-"+std::to_string(++serial);request.kept=request.link+"-kept";
        request.left=transfers::AllocationFact{{selection.account,target.identity,allocation->identity},allocation->amount,draft.date,accepted?transfers::State::committed:transfers::State::draft,imported,false,!imported,!imported};
        for(const auto& value:links)if(!value.link.unresolved)for(const auto* side:{&value.link.origin,&value.link.destination})if(side->state!=transfers::State::removed && side->endpoint==request.left->endpoint){request.link=value.link.identity;request.left=fact_from(*side);}
        request.expected_sequence=transfer_sequence();request.batch_revision=info.revision;
        request.allocation=target.allocation;request.category_generation=selection.generation;request.category_id=selection.category;request.category_path=category_path;
        requests.push_back(std::move(request));
    }
    return assign_transfer_batch(operation,requests,context);
}

app::ReviewReceipt ImportStore::assign_transfer_batch(std::string_view operation,const std::vector<app::TransferRequest>& requests,const app::OperationContext& context) {
    ScopedStatements statements(impl_->db);
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(requests.empty())fail(SQLITE_CONSTRAINT,"Select at least one transaction");
    std::string encoded="transfer-assignment:";
    std::set<imports::Occurrence> entries;
    std::set<std::string> link_ids;
    const auto sequence=requests.front().expected_sequence;
    for(const auto& request:requests){
        context.checkpoint("Checking transfer assignments");
        if(request.action!=app::TransferAction::retarget || (request.mode!=0 && request.mode!=1) || !request.left || (request.mode==1 && !request.generated) || request.mode!=requests.front().mode || request.left->endpoint.account!=impl_->selected_account || request.expected_sequence!=sequence)
            fail(SQLITE_CONSTRAINT,"Invalid transfer assignment batch");
        if(!entries.insert(request.left->endpoint.entry).second || !link_ids.insert(request.link).second)fail(SQLITE_CONSTRAINT,"Select each transaction and relationship only once");
        const auto bytes=transfer_codec::encode(request);encoded+=std::to_string(bytes.size())+":"+bytes;
    }
    const auto request_hash=session_snapshot::digest(encoded);
    session_snapshot::retry(impl_->db,operation,"transfer-assignment",request_hash);
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='transfer-assignment'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Transfer assignment retry changed its selections");
        return *prior;
    }
    const auto generation=review_generation(impl_->db);if(generation==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
    for(const auto& request:requests)if(request.category_generation>=0 && request.category_generation!=generation)fail(SQLITE_CONSTRAINT,"Transactions changed; select them again");
    Atomic transaction(impl_->db);
    // Validate every caller snapshot before any sibling advances a batch revision.
    const auto initial=all_transfers();
    const auto linked=[&](const app::TransferRequest& request){return std::any_of(initial.begin(),initial.end(),[&](const auto& v){return !v.link.unresolved && v.link.origin.state!=transfers::State::removed && (v.link.origin.endpoint==request.left->endpoint || v.link.destination.endpoint==request.left->endpoint);});};
    const auto category_draft=[&](const app::TransferRequest& request){
        const auto& fact=*request.left;const auto id=fact.endpoint.entry;
        if(request.expected_sequence!=sequence || sequence!=transfer_sequence() || batch(impl_->db,id.batch).revision!=request.batch_revision || account_for_batch(id.batch).identity!=impl_->selected_account)
            fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before changing category");
        require_writable_account(impl_->db,impl_->selected_account);require_management_shape(request);
        const bool committed=transaction_accepted(id);
        if(committed!=(fact.state==transfers::State::committed) || fact.generated)fail(SQLITE_CONSTRAINT,"Invalid category assignment");
        auto row=load_row(id,false);if(!row.draft || row.excluded_by_clear || row.matched_generated || (row.decision.disposition!=imports::Disposition::include && row.decision.disposition!=imports::Disposition::unresolved))fail(SQLITE_CONSTRAINT,"Resolve the transaction before changing category");
        auto draft=committed?committed_entry(id).current.draft:*row.draft;
        if(committed && !committed_entry(id).current.active)fail(SQLITE_CONSTRAINT,"Transaction was removed");
        // Manual ledger entries have no imported original. Validate the supplied
        // kind against the stored transaction rather than requiring import evidence.
        const bool imported=!batch(impl_->db,id.batch).manual;
        if(fact.imported!=imported || fact.manually_entered==imported)
            fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before changing category");
        const auto previous=draft;auto aa=draft.transaction.allocations();auto a=std::find_if(aa.begin(),aa.end(),[&](const auto& value){return value.identity==fact.endpoint.allocation;});
        if(a==aa.end() || a->amount!=fact.amount || draft.date!=fact.date)fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before changing category");
        if(!request.category_id.empty()){const auto display=resolve_category(request.category_id);if(!display.assignable || display.path!=request.category_path)fail(SQLITE_CONSTRAINT,"Choose an available category");}
        else if(!request.category_path.empty())fail(SQLITE_CONSTRAINT,"Choose an ordinary category or Uncategorized");
        a->category=request.category_path;a->category_binding=request.category_id.empty()?std::optional<CategoryBinding>{}:CategoryBinding{request.category_id,catalog_sequence(impl_->db)};a->category_owner=FieldOwnership::manual;a->category_rule.reset();
        draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),std::move(aa));return canonical_draft(id,draft,previous,committed?committed_entry(id).current.revision+1:request.batch_revision+1,committed);
    };
    std::vector<std::optional<imports::DraftRow>> ordinary;
    for(const auto& request:requests){context.checkpoint("Checking transfer assignments");
        if(request.mode==0 && !linked(request))ordinary.push_back(category_draft(request));
        else {(void)preview_transfer(request);ordinary.push_back({});}
    }
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'transfer-assignment',?,?)");
    receipt.text(1,operation);receipt.text(2,encoded);receipt.integer(3,generation);receipt.done();
    struct Scope {Impl& state;~Scope(){state.composing_review=false;}} scope{*impl_};
    std::size_t serial=0;std::set<std::string> changed;
    for(auto request:requests){
        context.checkpoint("Assigning transfers");
        const auto before=all_transfers();const auto child=std::string(operation)+"-transfer-"+std::to_string(++serial);
        const auto id=request.left->endpoint.entry;
        const bool record_category=request.mode==0 && (request.left->imported || request.left->ledger_entry);
        const bool committed=record_category && transaction_accepted(id);
        std::int64_t prior=0;
        if(record_category){if(committed)prior=committed_entry(id).current.revision;else{Statement v(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");v.text(1,id.batch);v.integer(2,integer(id.ordinal));if(!v.row() || v.null(0))fail(SQLITE_CORRUPT);prior=v.integer(0);}}
        request.expected_sequence=transfer_sequence();
        if(request.left->imported || request.left->ledger_entry)request.batch_revision=batch(impl_->db,id.batch).revision;
        impl_->composing_review=true;
        if(ordinary[serial-1]){
            write_review_row(operation,child,id,*ordinary[serial-1],load_row(id,false).decision,true,false,false);
        }else apply_transfer(child,request);
        impl_->composing_review=false;
        if(record_category && !ordinary[serial-1]){
            std::int64_t result=0;if(committed)result=committed_entry(id).current.revision;else{Statement v(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");v.text(1,id.batch);v.integer(2,integer(id.ordinal));if(!v.row() || v.null(0))fail(SQLITE_CORRUPT);result=v.integer(0);}
            Statement member(impl_->db,"INSERT INTO review_members VALUES(?,?,?,?,?,?,?)");member.text(1,operation);member.text(2,id.batch);member.integer(3,integer(id.ordinal));member.integer(4,committed?1:0);member.integer(5,prior);member.integer(6,result);member.text(7,child);member.done();
        }
        for(const auto& value:all_transfers()){
            const auto old=std::find_if(before.begin(),before.end(),[&](const auto& v){return v.link.identity==value.link.identity;});const auto payload=transfer_codec::encode(value);
            if(old!=before.end() && payload==transfer_codec::encode(*old))continue;
            if(!changed.insert(value.link.identity).second)fail(SQLITE_CONSTRAINT,"Selected transfers affect the same relationship");
            Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
            if(old!=before.end())history.blob(3,transfer_codec::encode(*old));
            history.blob(4,payload);history.text(5,child);history.integer(6,old==before.end()?0:old->sequence);history.done();
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,generation+1);stack.done();impl_->pending_review_action=operation;
    session_snapshot::completed(impl_->db,operation,"transfer-assignment",request_hash);
    context.checkpoint("Validating transfer assignments");
    finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}

// The existing snapshot journal also records editor acceptances. No pending
// buffer is persisted: only a complete, validated transaction reaches this API.
app::ReviewReceipt ImportStore::edit_transaction(std::string_view operation,const imports::Occurrence& id,std::int64_t revision,const std::optional<app::EntryToken>& token,const imports::DraftRow& input) {
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    transfer_codec::Writer request;request.text(review_key(id));request.number(revision);request.number(token?1:0);
    if(token){request.text(review_key(token->identity));request.number(token->revision);request.number(integer(token->dependencies.size()));for(const auto& dependency:token->dependencies){request.text(dependency.batch);request.number(dependency.revision);request.number(static_cast<int>(dependency.state));}}
    const auto encoded=request.bytes;
    session_snapshot::request_draft(request,input);const auto request_hash=session_snapshot::digest(request.bytes);
    session_snapshot::retry(impl_->db,operation,"editor",request_hash);
    if(const auto outcome=ActionChain(impl_->db).durable_outcome(operation,request_hash);outcome && *outcome==ActionChain::Outcome::no_change)return {std::string(operation)};
    require_account(id.batch);require_writable_account(impl_->db,impl_->selected_account);
    if(const auto receipt=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT a.request,m.committed,m.prior,m.result FROM review_answers a JOIN review_members m ON m.answer=a.id WHERE a.id=? AND a.question='ledger-edit' AND m.batch=? AND m.ordinal=?");
        saved.text(1,operation);saved.text(2,id.batch);saved.integer(3,integer(id.ordinal));
        if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Edit retry changed its transaction");
        const auto before=saved.integer(1)?entry_snapshot(id,saved.integer(2)).draft:review_draft(id,saved.integer(2));
        const auto after=saved.integer(1)?entry_snapshot(id,saved.integer(3)).draft:review_draft(id,saved.integer(3));
        if(!same_draft(canonical_draft(id,input,before,saved.integer(3),saved.integer(1),false),after))fail(SQLITE_CONSTRAINT,"Edit retry changed its values");
        return *receipt;
    }
    if(review_survivor(impl_->db,id)!=id)fail(SQLITE_CONSTRAINT,"Edit the retained transaction");
    const auto info=batch(impl_->db,id.batch);if(info.revision!=revision)fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before editing");
    const auto row=load_row(id,false);
    if(!row.draft || row.excluded_by_clear || row.matched_generated)fail(SQLITE_CONSTRAINT,"Resolve this record before editing");
    const bool committed=transaction_accepted(id);
    if(committed!=token.has_value())fail(SQLITE_CONSTRAINT,"Transaction acceptance changed; refresh before editing");
    auto canonical=*row.draft;auto original=canonical;
    std::optional<EntryLinkPlan> links;
    const auto before_links=all_transfers();
    if(committed){
        const auto current=committed_entry(id);if(current.token!=*token)fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before editing");
        const auto preview=preview_committed_change({app::ChangeKind::correct,id,input,{}});
        original=current.current.draft;canonical=preview.after_entry.draft;
        links=plan_entry_links(before_links,id,current.current.draft,canonical,false);
    }else{
        require_unlinked(id);if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"This draft is not editable");
        if(info.revision==INT64_MAX)fail(SQLITE_FULL);
        canonical=canonical_draft(id,input,row.draft,info.revision+1,false);
        imports::validate_draft(row.source,canonical,row.source.source_currency && *row.source.source_currency!=canonical.transaction.effective_total().currency());
    }
    const auto generation=review_generation(impl_->db);if(generation==INT64_MAX)fail(SQLITE_FULL);
    Atomic transaction(impl_->db);
    if(same_draft(original,canonical) && (!links || links->changed.empty())){
        ActionChain(impl_->db).no_change(action_token(),operation,request_hash);
        session_snapshot::completed(impl_->db,operation,"editor",request_hash);
        transaction.commit();return {std::string(operation)};
    }
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'ledger-edit',?,?)");
    receipt.text(1,operation);receipt.text(2,encoded);receipt.integer(3,generation);receipt.done();
    const auto child=std::string(operation)+"-edit";
    write_review_row(operation,child,id,canonical,row.decision,true,false,false);
    if(links && !links->changed.empty()){
        write_entry_links(child,impl_->selected_account,app::TransferAction::relink,financial_sequence(impl_->db),links->changed);
        for(const auto& value:links->changed){
            const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
            Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
            if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));
            history.blob(4,transfer_codec::encode(value));history.text(5,child);history.integer(6,old==before_links.end()?0:old->sequence);history.done();
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,generation+1);stack.done();impl_->pending_review_action=operation;
    session_snapshot::completed(impl_->db,operation,"editor",request_hash);
    finish([&]{transaction.commit();});return {std::string(operation)};
}
bool ImportStore::can_undo_transaction_edit(std::string_view edit) const {
    Statement top(impl_->db,"SELECT s.answer,s.generation,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    return top.row() && top.bytes(0)==edit && (top.bytes(2)=="ledger-edit" || top.bytes(2)=="ledger-edit-batch" || top.bytes(2)=="ledger-delete") && action_head() && action_head()->id==edit;
}
app::ReviewReceipt ImportStore::undo_transaction_edit(std::string_view operation,std::string_view edit) {
    if(const auto receipt=review_outcome(operation)){
        Statement prior(impl_->db,"SELECT answer FROM review_undos WHERE id=?");prior.text(1,operation);
        if(!prior.row() || prior.bytes(0)!=edit)fail(SQLITE_CONSTRAINT,"Undo retry changed its edit");
        return *receipt;
    }
    if(!can_undo_transaction_edit(edit))fail(SQLITE_CONSTRAINT,"Later changes prevent transaction edit Undo");
    return undo_review(operation);
}
