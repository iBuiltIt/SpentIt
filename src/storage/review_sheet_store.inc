namespace {
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
    Statement undo(impl_->db,"SELECT s.generation,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    result.can_undo=undo.row() && undo.integer(0)==result.generation && undo.bytes(1)!="ledger-edit" && undo.bytes(1)!="ledger-edit-batch" && undo.bytes(1)!="ledger-delete" && undo.bytes(1)!="rules-account";
    std::map<imports::Occurrence,app::ReviewTransaction> rows;
    std::set<imports::Occurrence> unresolved;
    auto all_accounts=accounts();const auto archived=archived_accounts();all_accounts.insert(all_accounts.end(),archived.begin(),archived.end());
    for(const auto& account:all_accounts){
        impl_->selected_account=account.identity;
        for(const auto& row:current_ledger()) {
            if(row.identity.batch.starts_with("~transfer-"))continue;
            rows.emplace(row.identity,app::ReviewTransaction{row.identity,account.identity,account.name,row.draft,row.committed,batch(impl_->db,row.identity.batch).manual?"Manually entered":""});
        }
        for(const auto& b:batches())if(b.state==app::BatchState::pending || b.state==app::BatchState::captured || b.state==app::BatchState::protected_drafts){
            if(b.processing_incomplete){result.questions.push_back({"file:"+b.id,app::ReviewKind::correction,account.identity,account.name,"Resume this import before reviewing its transactions",b.id,{},{}});continue;}
            const auto parsed=parsed_source(b.id);
            if(parsed.diagnostic)result.questions.push_back({"file:"+b.id,app::ReviewKind::correction,account.identity,account.name,"Needs correction: import could not be fully read",b.id,{},{}});
            for(std::size_t i=0;i<parsed.records.size();++i){
                const imports::Occurrence id{b.id,i};const auto row=load_row(id,false);
                if(row.excluded_by_clear || transaction_accepted(id))continue;
                if(!row.draft){result.questions.push_back({"repair:"+review_key(id),app::ReviewKind::correction,account.identity,account.name,"Needs correction: repair this imported record",b.id,i,{}});continue;}
                if(row.decision.disposition==imports::Disposition::unresolved || row.review_stale)unresolved.insert(id);
            }
        }
    }
    // A partial decision leaves the unselected comparison outstanding even if
    // those rows were clean in their original import. A later explicit answer
    // retires that pending intent; undoing it exposes the remainder again.
    std::map<std::string,std::vector<imports::Occurrence>> remainders;
    Statement pending_choices(impl_->db,R"SQL(SELECT p.answer,p.batch,p.ordinal
      FROM review_choices p JOIN review_answers a ON a.id=p.answer
      WHERE p.kind=0 AND p.resolved=0
      AND NOT EXISTS(SELECT 1 FROM review_undos u WHERE u.answer=p.answer)
      AND NOT EXISTS(SELECT 1 FROM review_choices n JOIN review_answers na ON na.id=n.answer
        WHERE n.kind=0 AND n.resolved=1 AND n.batch=p.batch AND n.ordinal=p.ordinal
        AND na.sequence>a.sequence AND NOT EXISTS(SELECT 1 FROM review_undos nu WHERE nu.answer=n.answer)))SQL");
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
    if(!unresolved.empty())for(const auto& [id,view]:rows){impl_->selected_account=view.account;comparison_sources.emplace(id,load_row(id,false).source);}
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
    std::set<imports::Occurrence> visited,duplicate_rows;
    for(const auto& id:unresolved){
        if(!rows.contains(id) || visited.contains(id))continue;
        std::vector<imports::Occurrence> pending{id};visited.insert(id);std::vector<app::ReviewTransaction> members;
        while(!pending.empty()) {const auto next=pending.back();pending.pop_back();members.push_back(rows.at(next));duplicate_rows.insert(next);for(const auto& neighbor:graph[next])if(visited.insert(neighbor).second)pending.push_back(neighbor);}
        std::sort(members.begin(),members.end(),[](const auto& a,const auto& b){return a.identity<b.identity;});
        const auto& first=members.front();
        result.questions.push_back({"same:"+review_key(first.identity),members.size()>1?app::ReviewKind::same:app::ReviewKind::correction,first.account,first.account_name,members.size()>1?"Are these the same?":"Needs correction: review this transaction's duplicate decision",first.identity.batch,first.identity.ordinal,std::move(members)});
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
            candidates.push_back(std::move(candidate));
        }
        if(candidates.size()>1){link_sources.insert(id);result.questions.push_back({"link:"+review_key(id),app::ReviewKind::link,left.account,left.account_name,"Does this link with any of these?",id.batch,id.ordinal,std::move(candidates)});}
    }
    for(const auto& value:all_transfers()){
        const auto& link=value.link;
        if(link.unresolved || link.destination_released || link.destination.state==transfers::State::removed || link.destination.independent_evidence || link.destination.ledger_entry || link.destination.attached_evidence || link.destination.proposed_evidence || link.destination.accepted_unmatched)continue;
        const auto row=rows.find(link.origin.endpoint.entry);if(row==rows.end())continue;
        const auto message="Transfer recorded in "+account_by_id(link.destination.endpoint.account).name+"; bank transaction not yet matched";
        auto question=std::find_if(result.questions.begin(),result.questions.end(),[&](const auto& q){return q.kind==app::ReviewKind::link && !q.rows.empty() && q.rows.front().identity==row->first && q.waiting_link.empty();});
        if(question!=result.questions.end()){question->reason=message;question->waiting_link=link.identity;}
        else result.questions.push_back({"waiting:"+link.identity,app::ReviewKind::link,row->second.account,row->second.account_name,message,row->first.batch,row->first.ordinal,{row->second},link.identity});
    }
    const auto acceptance=acceptance_review();
    for(const auto& warning:acceptance.warnings){
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
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement s(impl_->db,"SELECT id FROM review_answers WHERE id=?1 UNION ALL SELECT id FROM review_undos WHERE id=?1 UNION ALL SELECT operation FROM link_exclusion_events WHERE operation=?1");s.text(1,operation);if(!s.row())return {};return app::ReviewReceipt{std::string(operation)};
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
void ImportStore::write_review_row(std::string_view answer,std::string_view child,const imports::Occurrence& id,const imports::DraftRow& draft,const imports::Decision& decision,bool active,bool undoing,bool fresh_review,std::optional<std::int64_t> applied_ruleset,std::optional<Money> unchanged_balance) {
    impl_->selected_account=account_for_batch(id.batch).identity;require_writable_account(impl_->db,impl_->selected_account);
    const auto source=load_row(id,false).source;imports::validate_draft(source,draft,source.source_currency && *source.source_currency!=draft.transaction.effective_total().currency());
    const bool committed=transaction_accepted(id);std::int64_t prior,result;
    if(committed){
        const auto current=committed_entry(id);prior=current.current.revision;if(prior==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");result=prior+1;
        if(undoing){Statement saved(impl_->db,"SELECT result FROM review_members WHERE answer=? AND batch=? AND ordinal=?");saved.text(1,answer);saved.text(2,id.batch);saved.integer(3,integer(id.ordinal));if(!saved.row())fail(SQLITE_CORRUPT);const auto expected=entry_snapshot(id,saved.integer(0));if(expected.active!=current.current.active || !same_draft(expected.draft,current.current.draft))fail(SQLITE_CONSTRAINT,"Later transaction changes prevent Review Undo");}
        auto after=current.current;after.revision=result;after.active=active;after.draft=draft;
        const auto before=unchanged_balance?*unchanged_balance:committed_balance();const Money zero(0,before.currency());
        const auto delta=(active?draft.transaction.effective_total():zero)-(current.current.active?current.current.draft.transaction.effective_total():zero);
        if(unchanged_balance && delta.units()!=0)fail(SQLITE_CONSTRAINT,"Rule application cannot change the account balance");
        std::optional<std::int64_t> ruleset;
#ifdef SPENTIT_RULES_STORAGE
        std::int64_t rule_revision=prior;
        if(undoing){Statement original(impl_->db,"SELECT prior FROM review_members WHERE answer=? AND batch=? AND ordinal=?");original.text(1,answer);original.text(2,id.batch);original.integer(3,integer(id.ordinal));if(!original.row())fail(SQLITE_CORRUPT);rule_revision=original.integer(0);}
        Statement mark(impl_->db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");mark.text(1,id.batch);mark.integer(2,integer(id.ordinal));mark.integer(3,rule_revision);if(mark.row())ruleset=mark.integer(0);
#endif
        if(applied_ruleset)ruleset=applied_ruleset;
        store_entry(impl_->db,id,after,ruleset);
        std::string undo_of;
        if(undoing){Statement old(impl_->db,"SELECT child FROM review_members WHERE answer=? AND batch=? AND ordinal=?");old.text(1,answer);old.text(2,id.batch);old.integer(3,integer(id.ordinal));if(!old.row())fail(SQLITE_CORRUPT);undo_of=old.bytes(0);}
        append_financial(impl_->db,child,undoing?app::ChangeKind::undo:active?app::ChangeKind::correct:app::ChangeKind::remove,id,prior,result,before,delta,before+delta,undo_of,financial_sequence(impl_->db),catalog_sequence(impl_->db));
        if(undoing){Statement member(impl_->db,"INSERT INTO review_undo_members SELECT id,?1,?2 FROM review_undos WHERE answer=?3");member.text(1,child);member.text(2,undo_of);member.text(3,answer);member.done();}
        for(const auto& dependency:current.token.dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,child);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
    }else{
        Statement v(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");v.text(1,id.batch);v.integer(2,integer(id.ordinal));if(!v.row() || v.null(0))fail(SQLITE_CORRUPT);prior=v.integer(0);
        const auto info=batch(impl_->db,id.batch);if(info.revision==INT64_MAX)fail(SQLITE_FULL);result=info.revision+1;
        const auto old=load_row(id,false);const auto kind=fresh_review && old.draft && same_draft(*old.draft,draft)?"decide":"save";
        std::optional<std::int64_t> ruleset;
#ifdef SPENTIT_RULES_STORAGE
        if(undoing){Statement original(impl_->db,"SELECT f.ruleset_revision FROM review_members m JOIN draft_rule_fields f ON f.batch=m.batch AND f.ordinal=m.ordinal AND f.revision=m.prior WHERE m.answer=? AND m.batch=? AND m.ordinal=?");original.text(1,answer);original.text(2,id.batch);original.integer(3,integer(id.ordinal));if(original.row())ruleset=original.integer(0);}
#endif
        if(applied_ruleset)ruleset=applied_ruleset;
        version(impl_->db,id,result,draft,decision,fresh_review,ruleset);advance(impl_->db,id.batch,info.revision,kind,static_cast<int>(info.state));
    }
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
        Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");history.text(1,operation);history.text(2,request.link);history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(*next));history.text(5,child);history.done();
        Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,answer.generation+1);stack.done();
        finish([&]{transaction.commit();});return {std::string(operation)};
    }
    const auto question=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& q){return q.key==answer.question;});if(question==sheet.questions.end())fail(SQLITE_CONSTRAINT,"This question has already changed");
    if(question->kind==app::ReviewKind::correction)fail(SQLITE_CONSTRAINT,"Correct the record before resolving it");
    const bool same=question->kind==app::ReviewKind::same;
    if(same!=(answer.action==app::ReviewAction::merge || answer.action==app::ReviewAction::separate))fail(SQLITE_CONSTRAINT,"Invalid Review answer");
    std::set<imports::Occurrence> selected(answer.selected.begin(),answer.selected.end());
    if(selected.size()!=answer.selected.size())fail(SQLITE_CONSTRAINT,"Select each transaction only once");
    if(answer.action==app::ReviewAction::separate || answer.action==app::ReviewAction::not_linked)for(const auto& row:question->rows)selected.insert(row.identity);
    if(selected.size()>4096)fail(SQLITE_TOOBIG,"Resolve at most 4096 transactions in one Review decision");
    if(selected.size()<2)fail(SQLITE_CONSTRAINT,"Select at least two transactions");
    std::vector<app::ReviewTransaction> chosen;
    for(const auto& id:selected){const auto row=std::find_if(question->rows.begin(),question->rows.end(),[&](const auto& r){return r.identity==id;});if(row==question->rows.end())fail(SQLITE_CONSTRAINT,"Select transactions in this question");chosen.push_back(*row);}
    std::optional<imports::Occurrence> survivor=answer.action==app::ReviewAction::merge?answer.survivor:std::nullopt;
    if(answer.action==app::ReviewAction::merge){
        const auto committed=std::count_if(chosen.begin(),chosen.end(),[](const auto& r){return r.committed;});
        const auto manual=std::count_if(chosen.begin(),chosen.end(),[&](const auto& r){return batch(impl_->db,r.identity.batch).manual;});
        if(manual==1)survivor=std::find_if(chosen.begin(),chosen.end(),[&](const auto& r){return batch(impl_->db,r.identity.batch).manual;})->identity;
        else if(manual>1){if(!survivor || !selected.contains(*survivor) || !batch(impl_->db,survivor->batch).manual)fail(SQLITE_CONSTRAINT,"Choose the manually entered transaction to keep");}
        else if(committed==1)survivor=std::find_if(chosen.begin(),chosen.end(),[](const auto& r){return r.committed;})->identity;
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
        for(const auto& row:question->rows)choice(0,row.identity,selected.contains(row.identity));
        if(survivor)std::stable_sort(chosen.begin(),chosen.end(),[&](const auto& a,const auto& b){return a.identity==*survivor && b.identity!=*survivor;});
        for(const auto& row:chosen){
            impl_->selected_account=row.account;
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
                Statement h(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");h.text(1,operation);h.text(2,value.link.identity);if(old!=prior.end())h.blob(3,transfer_codec::encode(*old));h.blob(4,payload);h.text(5,link_operation);h.done();
            }
        }
    }
    if(parent.empty()){Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,answer.generation+1);stack.done();finish([&]{transaction->commit();});}
    return {std::string(operation)};
}
app::ReviewReceipt ImportStore::undo_review(std::string_view operation) {
    identifier(operation);if(operation.size()>80)fail(SQLITE_MISUSE);if(const auto prior=review_outcome(operation)){Statement saved(impl_->db,"SELECT 1 FROM review_undos WHERE id=?");saved.text(1,operation);if(!saved.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another Review command");return *prior;}
    Statement top(impl_->db,"SELECT answer,generation FROM review_stack ORDER BY (SELECT sequence FROM review_answers WHERE id=answer) DESC LIMIT 1");
    if(!top.row())fail(SQLITE_CONSTRAINT,"There is no Review decision to undo");
    const auto answer=top.bytes(0);const auto generation=top.integer(1);if(generation==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
    if(generation!=review_generation(impl_->db))fail(SQLITE_CONSTRAINT,"Later changes prevent Review Undo");
    struct Scope{Impl& state;std::int64_t selected;bool cleaning;~Scope(){state.selected_account=selected;state.cleaning=cleaning;}} scope{*impl_,impl_->selected_account,impl_->cleaning};
    Atomic transaction(impl_->db);
    Statement parent(impl_->db,"SELECT question,request FROM review_answers WHERE id=?");parent.text(1,answer);if(!parent.row())fail(SQLITE_CORRUPT);
    Statement children(impl_->db,"SELECT id,request FROM review_answers WHERE question=? ORDER BY sequence");children.text(1,"batch-child:"+answer);
    std::vector<std::string> ids;std::string encoded="batch:";
    while(children.row()){
        ids.push_back(children.bytes(0));const auto request=children.bytes(1);encoded+=std::to_string(request.size())+":"+request;
        if(ids.size()>2048 || ids.back()!=answer+"-group-"+std::to_string(ids.size()))fail(SQLITE_CORRUPT,"Batch Review history is inconsistent");
    }
    if(parent.bytes(0)=="batch"?(ids.empty() || encoded!=parent.bytes(1)):!ids.empty())fail(SQLITE_CORRUPT,"Batch Review history is incomplete");
    std::reverse(ids.begin(),ids.end());
    restore_review_answer(operation,answer);
    std::size_t child=0;for(const auto& id:ids)restore_review_answer(std::string(operation)+"-group-"+std::to_string(++child),id);
    Statement pop(impl_->db,"DELETE FROM review_stack WHERE answer=?");pop.text(1,answer);pop.done();
    // Reversing the top decision makes the preceding stack entry current again.
    Statement prior(impl_->db,"UPDATE review_stack SET generation=? WHERE answer=(SELECT answer FROM review_stack ORDER BY (SELECT sequence FROM review_answers WHERE id=answer) DESC LIMIT 1) AND generation=(SELECT generation FROM review_answers WHERE id=?2)");prior.integer(1,generation+1);prior.text(2,answer);prior.done();
    finish([&]{transaction.commit();});return {std::string(operation)};
}

void ImportStore::restore_review_answer(std::string_view operation,std::string_view answer) {
    const auto prior_cleaning=impl_->cleaning;
    Statement receipt(impl_->db,"INSERT INTO review_undos VALUES(?,?)");receipt.text(1,operation);receipt.text(2,answer);receipt.done();
    // Restore transfer relationships first, then allocation snapshots. All writes
    // remain inside this single transaction, so no intermediate state is visible.
    Statement links(impl_->db,"SELECT identity,prior_payload,result_payload FROM review_link_history WHERE answer=?");links.text(1,answer);
    std::vector<app::TransferView> restored;
    while(links.row()){
        auto view=transfer_codec::view(links.bytes(2));
        if(!links.null(1))view=transfer_codec::view(links.bytes(1));
        else {undo_new_relationship(view);}
        restored.push_back(std::move(view));
    }
    if(!restored.empty()){
        const auto current=all_transfers();
        const auto active_count=[&](const app::TransferView& value){const auto found=std::find_if(current.begin(),current.end(),[&](const auto& v){return v.link.identity==value.link.identity;});if(found==current.end())fail(SQLITE_CORRUPT);return int(found->link.origin.state!=transfers::State::removed)+int(found->link.destination.state!=transfers::State::removed);};
        std::stable_sort(restored.begin(),restored.end(),[&](const auto& a,const auto& b){return active_count(a)>active_count(b);});
        std::size_t link_serial=0;
        for(const auto& view:restored){
            impl_->selected_account=view.link.destination.endpoint.account;
            const auto before=committed_balance();const auto old=all_transfers();auto after=old;
            for(auto& value:after)if(value.link.identity==view.link.identity)value=view;
            validate_transfer_set(after);
            const auto delta=local_total(after,impl_->selected_account,before.currency(),false)-local_total(old,impl_->selected_account,before.currency(),false);
            const auto previous_sequence=financial_sequence(impl_->db);if(previous_sequence==INT64_MAX)fail(SQLITE_FULL,"History capacity exhausted");const auto sequence=previous_sequence+1;
            const auto child=std::string(operation)+"-links-"+std::to_string(++link_serial);
            app::TransferRequest request{};request.action=app::TransferAction::review_undo;request.link=answer;request.allocation=view.link.identity;request.expected_sequence=sequence-1;
            Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,child);event.integer(2,impl_->selected_account);event.blob(3,transfer_codec::encode(request));event.integer(4,delta.units()!=0?1:0);event.integer(5,delta.units());event.done();
            store_transfer(impl_->db,view,sequence,false);
            append_financial(impl_->db,child,app::ChangeKind::transfer,{"",0},-1,0,before,delta,before+delta,"",sequence-1,catalog_sequence(impl_->db));
        }
    }
    Statement edit(impl_->db,"SELECT 1 FROM review_answers WHERE id=? AND question IN('ledger-edit','ledger-edit-batch','ledger-delete','rules-account')");edit.text(1,answer);const bool editor_undo=edit.row();
    Statement rules_group(impl_->db,"SELECT 1 FROM review_answers WHERE id=? AND question='rules-account'");rules_group.text(1,answer);const bool rules_undo=rules_group.row();
    std::map<std::int64_t,Money> rule_balances;
    std::size_t serial=0;
    Statement members(impl_->db,"SELECT batch,ordinal,committed,prior,result FROM review_members WHERE answer=? ORDER BY rowid DESC");members.text(1,answer);
    while(members.row()){
        const imports::Occurrence id{members.bytes(0),static_cast<std::size_t>(members.integer(1))};impl_->selected_account=account_for_batch(id.batch).identity;
        if(members.integer(2)){
            const auto before=entry_snapshot(id,members.integer(3));
            std::optional<Money> unchanged;
            if(rules_undo){auto it=rule_balances.find(impl_->selected_account);if(it==rule_balances.end())it=rule_balances.emplace(impl_->selected_account,committed_balance()).first;unchanged=it->second;}
            write_review_row(answer,std::string(operation)+"-"+std::to_string(++serial),id,before.draft,{imports::Disposition::include,{}},before.active,true,true,{},unchanged);
        }else{
            Statement decision(impl_->db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? AND revision=?");decision.text(1,id.batch);decision.integer(2,integer(id.ordinal));decision.integer(3,members.integer(3));if(!decision.row())fail(SQLITE_CORRUPT);
            imports::Decision value{static_cast<imports::Disposition>(decision.integer(0)),{}};if(value.disposition==imports::Disposition::duplicate)value.duplicate_of=imports::Occurrence{decision.bytes(1),static_cast<std::size_t>(decision.integer(2))};
            write_review_row(answer,std::string(operation)+"-"+std::to_string(++serial),id,review_draft(id,members.integer(3)),value,true,true,!editor_undo);
        }
    }
    impl_->cleaning=true;
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
    if(answers.empty() || answers.size()>2048)fail(SQLITE_CONSTRAINT,"Select between 1 and 2048 match groups");
    std::string encoded="batch:";
    std::set<std::string> questions;std::set<imports::Occurrence> selected;
    for(const auto& answer:answers){
        if(answer.action!=app::ReviewAction::merge || answer.question=="selected")fail(SQLITE_CONSTRAINT,"Batch Merge requires duplicate match groups");
        if(!questions.insert(answer.question).second)fail(SQLITE_CONSTRAINT,"Select each match group only once");
        for(const auto& id:answer.selected)if(!selected.insert(id).second)fail(SQLITE_CONSTRAINT,"Select each transaction only once in a batch");
        const auto request=review_request(answer);encoded+=std::to_string(request.size())+":"+request;
    }
    if(selected.size()>4096)fail(SQLITE_TOOBIG,"Merge at most 4096 transactions in one batch");
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
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,sheet.generation+1);stack.done();
    context.checkpoint("Validating merged matches");
    finish([&]{context.seal();transaction.commit();});
    return {std::string(operation)};
}

// Category assignment uses the same durable relationship snapshots and mutation
// barrier as Review Link. Child transfers never commit outside this transaction.
bool ImportStore::can_undo_transfer_assignment() const {
    Statement top(impl_->db,"SELECT s.generation,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    return top.row() && top.bytes(1)=="transfer-assignment" && top.integer(0)==review_generation(impl_->db);
}
app::ReviewReceipt ImportStore::assign_categories(std::string_view operation,const app::CategorySelection& selection,const app::OperationContext& context) {
    identifier(operation);
    if(selection.account!=impl_->selected_account || selection.selected.empty() || selection.selected.size()>4096)
        fail(SQLITE_CONSTRAINT,"Select between 1 and 4096 transactions in this account");
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
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(requests.empty() || requests.size()>4096)fail(SQLITE_CONSTRAINT,"Select between 1 and 4096 transactions");
    std::string encoded="transfer-assignment:";
    std::set<imports::Occurrence> entries;
    std::set<std::string> link_ids;
    const auto sequence=requests.front().expected_sequence;
    for(const auto& request:requests){
        if(request.action!=app::TransferAction::retarget || (request.mode!=0 && request.mode!=1) || !request.left || (request.mode==1 && !request.generated) || request.mode!=requests.front().mode || request.left->endpoint.account!=impl_->selected_account || request.expected_sequence!=sequence)
            fail(SQLITE_CONSTRAINT,"Invalid transfer assignment batch");
        if(!entries.insert(request.left->endpoint.entry).second || !link_ids.insert(request.link).second)fail(SQLITE_CONSTRAINT,"Select each transaction and relationship only once");
        const auto bytes=transfer_codec::encode(request);encoded+=std::to_string(bytes.size())+":"+bytes;
    }
    if(const auto prior=review_outcome(operation)){
        Statement saved(impl_->db,"SELECT request FROM review_answers WHERE id=? AND question='transfer-assignment'");saved.text(1,operation);
        if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Transfer assignment retry changed its selections");return *prior;
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
            Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
            if(old!=before.end())history.blob(3,transfer_codec::encode(*old));history.blob(4,payload);history.text(5,child);history.done();
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,generation+1);stack.done();
    context.checkpoint("Validating transfer assignments");
    finish([&]{context.seal();transaction.commit();});return {std::string(operation)};
}

// The existing snapshot journal also records editor acceptances. No pending
// buffer is persisted: only a complete, validated transaction reaches this API.
app::ReviewReceipt ImportStore::edit_transaction(std::string_view operation,const imports::Occurrence& id,std::int64_t revision,const std::optional<app::EntryToken>& token,const imports::DraftRow& input) {
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    require_account(id.batch);require_writable_account(impl_->db,impl_->selected_account);
    transfer_codec::Writer request;request.text(review_key(id));request.number(revision);request.number(token?1:0);
    if(token){request.text(review_key(token->identity));request.number(token->revision);request.number(integer(token->dependencies.size()));for(const auto& dependency:token->dependencies){request.text(dependency.batch);request.number(dependency.revision);request.number(static_cast<int>(dependency.state));}}
    const auto encoded=request.bytes;
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
    auto canonical=*row.draft;
    std::optional<EntryLinkPlan> links;
    const auto before_links=all_transfers();
    if(committed){
        const auto current=committed_entry(id);if(current.token!=*token)fail(SQLITE_CONSTRAINT,"Transaction changed; refresh before editing");
        const auto preview=preview_committed_change({app::ChangeKind::correct,id,input,{}});
        canonical=preview.after_entry.draft;
        links=plan_entry_links(before_links,id,current.current.draft,canonical,false);
    }else{
        require_unlinked(id);if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"This draft is not editable");
        if(info.revision==INT64_MAX)fail(SQLITE_FULL);
        canonical=canonical_draft(id,input,row.draft,info.revision+1,false);
        imports::validate_draft(row.source,canonical,row.source.source_currency && *row.source.source_currency!=canonical.transaction.effective_total().currency());
    }
    const auto generation=review_generation(impl_->db);if(generation==INT64_MAX)fail(SQLITE_FULL);
    Atomic transaction(impl_->db);
    Statement receipt(impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,'ledger-edit',?,?)");
    receipt.text(1,operation);receipt.text(2,encoded);receipt.integer(3,generation);receipt.done();
    const auto child=std::string(operation)+"-edit";
    write_review_row(operation,child,id,canonical,row.decision,true,false,false);
    if(links && !links->changed.empty()){
        write_entry_links(child,impl_->selected_account,app::TransferAction::relink,financial_sequence(impl_->db),links->changed);
        for(const auto& value:links->changed){
            const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
            Statement history(impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
            if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(value));history.text(5,child);history.done();
        }
    }
    Statement stack(impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,generation+1);stack.done();
    finish([&]{transaction.commit();});return {std::string(operation)};
}
bool ImportStore::can_undo_transaction_edit(std::string_view edit) const {
    Statement top(impl_->db,"SELECT s.answer,s.generation,a.question FROM review_stack s JOIN review_answers a ON a.id=s.answer ORDER BY a.sequence DESC LIMIT 1");
    return top.row() && top.bytes(0)==edit && (top.bytes(2)=="ledger-edit" || top.bytes(2)=="ledger-edit-batch" || top.bytes(2)=="ledger-delete") && top.integer(1)==review_generation(impl_->db);
}
app::ReviewReceipt ImportStore::undo_transaction_edit(std::string_view operation,std::string_view edit) {
    if(const auto receipt=review_outcome(operation)){
        Statement prior(impl_->db,"SELECT answer FROM review_undos WHERE id=?");prior.text(1,operation);
        if(!prior.row() || prior.bytes(0)!=edit)fail(SQLITE_CONSTRAINT,"Undo retry changed its edit");return *receipt;
    }
    if(!can_undo_transaction_edit(edit))fail(SQLITE_CONSTRAINT,"Later changes prevent transaction edit Undo");
    return undo_review(operation);
}
