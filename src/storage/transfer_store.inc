namespace {
bool local_side(const transfers::Side& side){return (side.manually_entered || !side.independent_evidence) && !side.attached_evidence;}
Money local_total(const std::vector<app::TransferView>& values,std::int64_t account,const Currency& currency,bool projected){
    std::vector<Money> amounts;
    for(const auto& value:values)for(const auto* side:{&value.link.origin,&value.link.destination})
        if(side->endpoint.account==account && local_side(*side) && side->state!=transfers::State::removed &&
           (projected || side->state==transfers::State::committed) && !(projected && side->proposed_evidence))amounts.push_back(side->amount);
    return imports::sum(currency,amounts);
}
std::vector<app::TransferView> import_transition(std::vector<app::TransferView> values,std::int64_t account,std::string_view batch,bool publish){
    std::set<imports::Occurrence> protected_origins;
    for(const auto& value:values)if(value.link.origin.endpoint.account==account && value.link.origin.endpoint.entry.batch==batch && transfers::protected_draft(value.link,account))protected_origins.insert(value.link.origin.endpoint.entry);
    for(auto& value:values){
        for(const auto* side:{&value.link.origin,&value.link.destination})if(side->endpoint.account==account && side->proposed_evidence && side->proposed_evidence->endpoint.entry.batch==batch){
            const auto evidence=*side->proposed_evidence;
            value.link=publish?transfers::commit_import_match(value.link,account,evidence):transfers::discard_import_match(value.link,account,batch);
            break;
        }
        if(value.link.origin.endpoint.account==account && value.link.origin.endpoint.entry.batch==batch && value.link.origin.state==transfers::State::draft){
            if(publish)value.link=transfers::commit(value.link,account);
            else if(!protected_origins.contains(value.link.origin.endpoint.entry))value.link=transfers::clear_workspace(value.link,account);
        }else if(value.link.destination.independent_evidence && value.link.destination.endpoint.account==account && value.link.destination.endpoint.entry.batch==batch && value.link.destination.state==transfers::State::draft){
            if(publish){if(!value.link.unresolved)value.link=transfers::commit(value.link,account);}
            else if(!value.link.unresolved)fail(SQLITE_CONSTRAINT,"This import is linked as a transfer destination. Change the origin before discarding it");
        }
    }
    return values;
}
void store_transfer(sqlite3* db,const app::TransferView& value,std::int64_t sequence,bool initial){
    Statement row(db,"INSERT INTO transfer_versions VALUES(?,?,?)");row.text(1,value.link.identity);row.integer(2,sequence);row.blob(3,transfer_codec::encode(value));row.done();
    Statement head(db,initial?"INSERT INTO transfer_heads VALUES(?,?)":"UPDATE transfer_heads SET sequence=?2 WHERE identity=?1");head.text(1,value.link.identity);head.integer(2,sequence);head.done();
}
void validate_transfer_set(const std::vector<app::TransferView>& values){std::vector<transfers::Link> links;links.reserve(values.size());for(const auto& value:values)links.push_back(value.link);transfers::validate_set(links);}
app::TransferView transition_transfer(app::TransferView value,const app::TransferRequest& request,std::int64_t account){
    const auto& current=value.link.origin.endpoint.account==account?value.link.origin:value.link.destination;
    if(current.endpoint.account!=account)fail(SQLITE_CONSTRAINT,"Transfer belongs to another account");
    switch(request.action){
    case app::TransferAction::accept:value.link=transfers::accept_unmatched(value.link,account);break;
    case app::TransferAction::commit_local:
        if(!local_side(current))fail(SQLITE_CONSTRAINT,"Commit imported entries through their import");
        value.link=transfers::commit(value.link,account);break;
    case app::TransferAction::propose_match:
        if(!request.evidence)fail(SQLITE_CONSTRAINT,"Choose bank evidence explicitly");
        value.link=transfers::propose_match(value.link,account,*request.evidence);break;
    case app::TransferAction::discard_match:
        if(!request.evidence || current.proposed_evidence!=request.evidence)fail(SQLITE_CONSTRAINT,"Match proposal changed");
        value.link=transfers::discard_import_match(value.link,account,request.evidence->endpoint.entry.batch);break;
    case app::TransferAction::clear_local:
        if(!local_side(current))fail(SQLITE_CONSTRAINT,"Clear imported entries through their import");
        value.link=transfers::clear_workspace(value.link,account);break;
    default:fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    }
    return value;
}
const char* transfer_action_name(const app::TransferRequest& request) {
    switch(request.action) {
    case app::TransferAction::manual: return "Create";
    case app::TransferAction::from_draft: return "Generate";
    case app::TransferAction::accept: return "Accept";
    case app::TransferAction::commit_local: return "Commit";
    case app::TransferAction::propose_match: return "Propose match";
    case app::TransferAction::discard_match: return "Drop proposal";
    case app::TransferAction::clear_local: return "Clear";
    case app::TransferAction::import_commit: return "Import commit";
    case app::TransferAction::import_discard: return "Import discard";
    case app::TransferAction::merge: return "Merge";
    case app::TransferAction::retarget: return request.mode==0?"Change category":"Change destination";
    }
    return "Transfer";
}
void require_management_shape(const app::TransferRequest& request) {
    identifier(request.kept);
    const bool category_fields=!request.category_id.empty() || !request.category_path.empty();
    if(request.action==app::TransferAction::merge) {
        if(request.mode!=0 || category_fields || request.generated || !request.generated_currency.empty() || !request.left || !request.right)
            fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    } else if(request.action==app::TransferAction::retarget) {
        if(!request.left || request.right) fail(SQLITE_CONSTRAINT,"Invalid transfer command");
        if(request.mode==0) { if(request.generated || !request.generated_currency.empty()) fail(SQLITE_CONSTRAINT,"Invalid transfer command"); }
        else if(request.mode!=1 || !request.generated || request.generated_currency.empty() || category_fields) fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    } else fail(SQLITE_CONSTRAINT,"Invalid transfer command");
}
std::vector<app::TransferView> manage_views(const std::vector<app::TransferView>& values,const app::TransferRequest& request,std::int64_t sequence) {
    std::vector<transfers::Link> links;links.reserve(values.size());
    for(const auto& value:values) links.push_back(value.link);
    std::optional<Currency> generated_currency;
    if(!request.generated_currency.empty()) {
        try { generated_currency=Currency::from_definition(request.generated_currency); }
        catch(const std::exception&) { fail(SQLITE_CORRUPT,"Transfer history does not replay"); }
    }
    const auto edit=request.action==app::TransferAction::merge
        ? transfers::merge(links,*request.left,*request.right,request.link,request.kept)
        : transfers::retarget(links,*request.left,request.link,request.kept,request.generated,generated_currency);
    const app::TransferView* prior=nullptr;
    for(const auto& value:values) if(value.link.identity==edit.link.identity) prior=&value;
    std::vector<app::TransferView> out;out.reserve(values.size()+1);
    for(const auto& value:values) {
        if(value.link.identity==edit.link.identity) continue;
        if(edit.kept && value.link.identity==edit.kept->identity) fail(SQLITE_CONSTRAINT,"Transfer identity already exists");
        out.push_back(value);
    }
    if(prior) out.push_back({edit.link,prior->payee,prior->memo,prior->created_sequence});
    else { safe_text(request.payee);safe_text(request.memo);out.push_back({edit.link,request.payee,request.memo,sequence}); }
    if(edit.kept) {
        if(!prior) fail(SQLITE_CONSTRAINT,"Invalid transfer command");
        out.push_back({*edit.kept,prior->payee,prior->memo,prior->created_sequence});
    }
    for(const auto& value:values) if(std::none_of(out.begin(),out.end(),[&](const auto& next){return next.link.identity==value.link.identity;}))
        fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    validate_transfer_set(out);return out;
}
std::vector<app::TransferView> changed_views(const std::vector<app::TransferView>& before,const std::vector<app::TransferView>& after) {
    std::map<std::string,std::string> prior;
    for(const auto& value:before) prior.emplace(value.link.identity,transfer_codec::encode(value));
    std::vector<app::TransferView> out;
    for(const auto& value:after) {
        const auto found=prior.find(value.link.identity);
        if(found==prior.end() || found->second!=transfer_codec::encode(value)) out.push_back(value);
    }
    return out;
}
template<class AccountOf>
void require_zero_committed_delta(const std::vector<app::TransferView>& before,const std::vector<app::TransferView>& after,const std::set<std::int64_t>& accounts,AccountOf account_of,int code,const char* message) {
    for(const auto id:accounts) {
        const auto currency=account_of(id).currency;
        Money delta(0,currency);
        try { delta=local_total(after,id,currency,false)-local_total(before,id,currency,false); }
        catch(const FinancialError&) { fail(code,message); }
        if(delta.units()!=0) fail(code,message);
    }
}
}
std::int64_t ImportStore::transfer_sequence() const{return financial_sequence(impl_->db);}
std::vector<app::TransferView> ImportStore::all_transfers() const {
    Statement rows(impl_->db,"SELECT v.identity,v.payload FROM transfer_heads h JOIN transfer_versions v USING(identity,sequence) ORDER BY v.identity");std::vector<app::TransferView> out;
    while(rows.row()){auto value=transfer_codec::view(rows.bytes(1));if(value.link.identity!=rows.bytes(0))fail(SQLITE_CORRUPT);out.push_back(std::move(value));if(out.size()>qif::max_records)fail(SQLITE_TOOBIG);}
    validate_transfer_set(out);return out;
}
std::vector<app::TransferView> ImportStore::transfers() const {
    auto out=all_transfers();std::erase_if(out,[&](const auto& value){return value.link.origin.endpoint.account!=impl_->selected_account && value.link.destination.endpoint.account!=impl_->selected_account;});return out;
}
Money ImportStore::local_transfer_total(std::int64_t account,bool projected) const {return local_total(all_transfers(),account,account_by_id(account).currency,projected);}
std::vector<app::LedgerRow> ImportStore::local_transfer_ledger(bool projected) const {
    std::vector<app::LedgerRow> out;
    for(const auto& value:transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(side->endpoint.account!=impl_->selected_account || !local_side(*side) || side->state==transfers::State::removed || (!projected && side->state!=transfers::State::committed) || (projected && side->proposed_evidence))continue;
        Allocation allocation{"Transfer",value.memo,side->amount,{},side->endpoint.allocation};allocation.category_owner=allocation.memo_owner=FieldOwnership::manual;
        out.push_back({side->endpoint.entry,-value.created_sequence,{side->date,value.payee,value.memo,Transaction::manual(side->amount,{allocation}),FieldOwnership::manual,FieldOwnership::manual},side->state==transfers::State::committed});
    }
    return out;
}
void ImportStore::require_unlinked(const imports::Occurrence& id) const {
    for(const auto& value:all_transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(side->state==transfers::State::removed)continue;
        const bool reserve=!value.link.unresolved || local_side(*side);
        if(reserve && side->endpoint.entry==id)
            fail(SQLITE_CONSTRAINT,"Transfer-linked entries require transfer-aware editing; this command is unavailable");
        if(!value.link.unresolved && ((side->attached_evidence && side->attached_evidence->endpoint.entry==id) || (side->proposed_evidence && side->proposed_evidence->endpoint.entry==id)))
            fail(SQLITE_CONSTRAINT,"Transfer-linked entries require transfer-aware editing; this command is unavailable");
    }
}
std::optional<app::TransferReceipt> ImportStore::transfer_outcome(std::string_view operation) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT t.account,o.sequence,o.before,o.change,o.after FROM transfer_events t JOIN ledger_operations o ON o.id=t.operation WHERE t.operation=?");row.text(1,operation);
    if(!row.row())return {};
    if(!impl_->verifying && row.integer(0)!=impl_->selected_account)fail(SQLITE_CONSTRAINT,"Transfer operation belongs to another account");
    Statement order(impl_->db,"SELECT sequence FROM history_order WHERE operation=?");order.text(1,operation);if(!order.row())fail(SQLITE_CORRUPT);
    const auto currency=currency_as_of(impl_->db,row.integer(0),order.integer(0));
    return app::TransferReceipt{std::string(operation),row.integer(0),row.integer(1),Money(row.integer(2),currency),Money(row.integer(3),currency),Money(row.integer(4),currency)};
}
std::vector<app::TransferJournalEntry> ImportStore::transfer_journal() const {
    Statement rows(impl_->db,"SELECT e.operation,e.request,o.sequence,v.payload FROM transfer_events e JOIN ledger_operations o ON o.id=e.operation JOIN transfer_versions v ON v.sequence=o.sequence ORDER BY o.sequence,v.identity");
    std::vector<app::TransferJournalEntry> out;
    while(rows.row()) {
        if(out.size()>=qif::max_records) fail(SQLITE_TOOBIG);
        const auto request=transfer_codec::request(rows.bytes(1));
        const auto value=transfer_codec::view(rows.bytes(3));
        out.push_back({rows.bytes(0),transfer_action_name(request),value.link.identity,rows.integer(2),value.link.origin.endpoint.account,value.link.destination.endpoint.account,value.link.origin.amount,value.link.destination.amount,value.link.origin.date,value.link.destination.date,value.link.unresolved});
    }
    return out;
}
app::TransferReceipt ImportStore::apply_transfer(std::string_view operation,const app::TransferRequest& request){
    identifier(operation);identifier(request.link);const auto encoded=transfer_codec::encode(request);
    if(const auto prior=transfer_outcome(operation)){Statement saved(impl_->db,"SELECT request FROM transfer_events WHERE operation=?");saved.text(1,operation);if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");return *prior;}
    Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");collision.text(1,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    require_writable_account(impl_->db,impl_->selected_account);
    if(request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft)require_writable_account(impl_->db,request.destination);
    if(request.expected_sequence!=transfer_sequence() || request.expected_sequence==INT64_MAX)fail(SQLITE_CONSTRAINT,"Transfer preview changed; refresh before applying");
    Atomic transaction(impl_->db);
    if(request.action==app::TransferAction::commit_local){start_import_groups(financial_group_batches(impl_->selected_account),operation);publish_group_opening(impl_->selected_account);}
    const auto account=this->account();auto values=all_transfers();auto found=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==request.link;});
    const bool create=request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft;
    const bool managed=request.action==app::TransferAction::merge || request.action==app::TransferAction::retarget;
    const auto before=committed_balance();const auto local_before=local_total(values,account.identity,account.currency,false);const auto sequence=request.expected_sequence+1;
    if(managed){
        require_management_shape(request);
        std::optional<Currency> generated_currency;
        if(!request.generated_currency.empty()){try{generated_currency=Currency::from_definition(request.generated_currency);}catch(const std::exception&){fail(SQLITE_CONSTRAINT,"Invalid transfer command");}}
        if(request.generated){
            const auto destination=account_by_id(request.generated->account);require_writable_account(impl_->db,destination.identity);
            if(request.mode==1 && (!generated_currency || destination.currency!=*generated_currency))fail(SQLITE_CONSTRAINT,"Cross-currency transfers are unsupported");
        }
        struct ReadScope{bool& slot;bool prior;explicit ReadScope(bool& value):slot(value),prior(value){slot=true;}~ReadScope(){slot=prior;}} reading(impl_->verifying);
        auto confirm_fact=[&](const transfers::AllocationFact& fact,std::int64_t revision){
            require_writable_account(impl_->db,fact.endpoint.account);(void)account_by_id(fact.endpoint.account);
            if(fact.generated || fact.manually_entered){if(fact.imported)fail(SQLITE_CONSTRAINT,"Invalid transfer command");return;}
            if(!fact.imported)fail(SQLITE_CONSTRAINT,"Invalid transfer command");
            if(account_for_batch(fact.endpoint.entry.batch).identity!=fact.endpoint.account)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
            const auto info=batch(impl_->db,fact.endpoint.entry.batch);
            if(info.revision!=revision)fail(SQLITE_CONSTRAINT,"Import changed; refresh before retrying");
            const bool committed=info.state==app::BatchState::committed;
            const bool editable=info.state==app::BatchState::pending || info.state==app::BatchState::protected_drafts;
            if(fact.state==transfers::State::committed){if(!committed)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");}
            else if(fact.state!=transfers::State::draft || !editable)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
            std::optional<imports::DraftRow> row;
            if(committed){const auto entry=committed_entry(fact.endpoint.entry);if(!entry.current.active)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");row=entry.current.draft;}
            else{const auto loaded=load_row(fact.endpoint.entry,false);if(loaded.excluded_by_clear || !loaded.draft || loaded.decision.disposition!=imports::Disposition::include)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");row=loaded.draft;}
            const auto& allocations=row->transaction.allocations();
            const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==fact.endpoint.allocation;});
            if(allocation==allocations.end() || allocation->amount!=fact.amount || row->date!=fact.date)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
        };
        if(request.left)confirm_fact(*request.left,request.batch_revision);
        if(request.right)confirm_fact(*request.right,request.right_revision);
        const auto managed_values=manage_views(values,request,sequence);
        const auto changed=changed_views(values,managed_values);
        if(changed.empty())fail(SQLITE_CONSTRAINT,"Invalid transfer command");
        std::set<std::int64_t> accounts{account.identity};
        auto note=[&](std::int64_t id){if(id>0)accounts.insert(id);};
        if(request.left)note(request.left->endpoint.account);if(request.right)note(request.right->endpoint.account);if(request.generated)note(request.generated->account);
        std::map<std::string,const app::TransferView*> indexed;for(const auto& value:values)indexed.emplace(value.link.identity,&value);
        for(const auto& value:changed){
            note(value.link.origin.endpoint.account);note(value.link.destination.endpoint.account);
            if(const auto prior=indexed.find(value.link.identity);prior!=indexed.end()){note(prior->second->link.origin.endpoint.account);note(prior->second->link.destination.endpoint.account);}
        }
        require_zero_committed_delta(values,managed_values,accounts,[&](std::int64_t id){return account_by_id(id);},SQLITE_CONSTRAINT,"This transfer command would change committed money");
        if(request.action==app::TransferAction::retarget && request.mode==0){
            const auto& fact=*request.left;
            if(!fact.imported || fact.state!=transfers::State::draft)fail(SQLITE_CONSTRAINT,"Invalid transfer command");
            safe_text(request.category_path);
            if(!request.category_id.empty()){identifier(request.category_id);const auto display=resolve_category(request.category_id);if(!display.assignable || display.path!=request.category_path)fail(SQLITE_CONSTRAINT,"Category choice changed or is archived; choose an active leaf again");}
            const auto info=batch(impl_->db,fact.endpoint.entry.batch);
            if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");
            const auto loaded=load_row(fact.endpoint.entry,false);if(!loaded.draft)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");
            auto edit=*loaded.draft;auto allocations=edit.transaction.allocations();bool matched=false;
            for(auto& allocation:allocations)if(allocation.identity==fact.endpoint.allocation){
                matched=true;allocation.category=request.category_path;allocation.category_owner=FieldOwnership::manual;allocation.category_rule.reset();
                if(request.category_id.empty())allocation.category_binding.reset();else allocation.category_binding=CategoryBinding{request.category_id,catalog_sequence(impl_->db)};
            }
            if(!matched)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
            edit.transaction=Transaction::repaired_import(edit.transaction.original_imported_amount(),edit.transaction.effective_total(),std::move(allocations),edit.transaction.reassigned_currency());
            const auto canonical=canonical_draft(fact.endpoint.entry,edit,loaded.draft,info.revision+1,false,true);
            const bool reassigned=loaded.source.source_currency && canonical.transaction.effective_total().currency()!=*loaded.source.source_currency;
            imports::validate_draft(loaded.source,canonical,reassigned);
            auto write_category=[&]{version(impl_->db,fact.endpoint.entry,info.revision+1,canonical,loaded.decision,false);advance(impl_->db,fact.endpoint.entry.batch,info.revision,"save",static_cast<int>(info.state));};
            // Retained protected drafts accept a version only while an account command is marked. This write does not touch account tables.
            if(info.state==app::BatchState::protected_drafts){struct CommandScope{int& slot;explicit CommandScope(int& value):slot(value){slot=1;}~CommandScope(){slot=0;}} scope(impl_->account_command);write_category();}
            else write_category();
        }
        std::set<std::string> prior_ids;for(const auto& value:values)prior_ids.insert(value.link.identity);
        const Money zero(0,account.currency);
        Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,account.identity);event.blob(3,encoded);event.integer(4,0);event.integer(5,0);event.done();
        for(const auto& value:changed)store_transfer(impl_->db,value,sequence,!prior_ids.contains(value.link.identity));
        append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,before,zero,before,"",request.expected_sequence,catalog_sequence(impl_->db));
        impl_->finish(transaction);return {std::string(operation),account.identity,sequence,before,zero,before};
    }
    if(create){
        if(found!=values.end())fail(SQLITE_CONSTRAINT,"Transfer identity already exists");
        const auto destination=account_by_id(request.destination);safe_text(request.payee);safe_text(request.memo);
        std::optional<imports::DraftRow> draft;transfers::Endpoint origin{account.identity,{"~transfer-"+request.link+"-origin",0},"manual"};
        if(request.action==app::TransferAction::manual){
            if(!request.amount || request.origin || !request.allocation.empty() || request.evidence)fail(SQLITE_CONSTRAINT,"Invalid manual transfer request");
            request.amount->require_compatible(account.opening);iso_date(request.date);
            draft=imports::DraftRow{request.date,request.payee,request.memo,Transaction::manual(*request.amount,{{"",request.memo,*request.amount,{},"manual"}})};
        }else{
            if(!request.origin || request.amount || request.evidence)fail(SQLITE_CONSTRAINT,"Choose a draft allocation");
            require_account(request.origin->batch);expect(impl_->db,request.origin->batch,request.batch_revision);const auto row=load_row(*request.origin,false);
            if(row.excluded_by_clear || !row.draft || row.decision.disposition!=imports::Disposition::include)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");
            draft=*row.draft;origin.entry=*request.origin;origin.allocation=request.allocation;
        }
        auto link=transfers::generate(request.link,origin,*draft,{destination.identity,{"~transfer-"+request.link+"-destination",0},"generated"},destination.currency);
        values.push_back({link,draft->payee,draft->memo,sequence});found=std::prev(values.end());
    }else{
        if(found==values.end())fail(SQLITE_NOTFOUND,"Transfer does not exist");
        if(request.action==app::TransferAction::propose_match){
            if(!request.evidence)fail(SQLITE_CONSTRAINT,"Choose bank evidence");
            require_account(request.evidence->endpoint.entry.batch);if(batch(impl_->db,request.evidence->endpoint.entry.batch).state!=app::BatchState::pending)fail(SQLITE_CONSTRAINT,"Match evidence must belong to a pending import");expect(impl_->db,request.evidence->endpoint.entry.batch,request.batch_revision);
            const auto row=load_row(request.evidence->endpoint.entry,false);
            if(!row.draft || row.decision.disposition!=imports::Disposition::include)fail(SQLITE_CONSTRAINT,"Resolve the bank entry before matching");
            const auto& allocations=row.draft->transaction.allocations();const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return a.identity==request.evidence->endpoint.allocation;});
            if(allocation==allocations.end() || allocation->amount!=request.evidence->amount || row.draft->date!=request.evidence->posting_date || request.evidence->endpoint.account!=account.identity)fail(SQLITE_CONSTRAINT,"Bank evidence changed; refresh the proposal");
        }
        *found=transition_transfer(*found,request,account.identity);
    }
    validate_transfer_set(values);
    const bool did_commit=request.action==app::TransferAction::commit_local;
    if(did_commit && !account.opening_confirmed)fail(SQLITE_CONSTRAINT,"Confirm the opening balance before committing");
    const auto change=local_total(values,account.identity,account.currency,false)-local_before;const auto after=before+change;
    Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,account.identity);event.blob(3,encoded);event.integer(4,did_commit?1:0);event.integer(5,change.units());event.done();
    store_transfer(impl_->db,*found,sequence,create);
    append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,before,change,after,"",request.expected_sequence,catalog_sequence(impl_->db));
    if(did_commit)exec(impl_->db,"UPDATE account SET frozen=1 WHERE singleton=spentit_account()");
    impl_->finish(transaction);return {std::string(operation),account.identity,sequence,before,change,after};
}
std::set<std::size_t> ImportStore::protected_import_rows(std::string_view batch) const {
    std::set<std::size_t> result;
    for(const auto& value:all_transfers())if(value.link.origin.endpoint.entry.batch==batch && transfers::protected_draft(value.link,value.link.origin.endpoint.account))result.insert(value.link.origin.endpoint.entry.ordinal);
    return result;
}
Money ImportStore::transfer_import_adjustment(std::string_view operation) const {
    Statement event(impl_->db,"SELECT account,local_change FROM transfer_events WHERE operation=?");event.text(1,operation);
    Statement order(impl_->db,"SELECT sequence FROM history_order WHERE operation=?");order.text(1,operation);
    if(!event.row()){Statement op(impl_->db,"SELECT batch FROM operations WHERE id=?");op.text(1,operation);if(!op.row())fail(SQLITE_CORRUPT);const auto owner=account_for_batch(op.bytes(0));return Money(0,order.row()?currency_as_of(impl_->db,owner.identity,order.integer(0)):owner.currency);}
    if(!order.row())fail(SQLITE_CORRUPT);return Money(event.integer(1),currency_as_of(impl_->db,event.integer(0),order.integer(0)));
}
Money ImportStore::transfer_import_transition(std::string_view batch_id,std::string_view operation,bool publish){
    const auto account=this->account();const auto before=all_transfers();const auto after=import_transition(before,account.identity,batch_id,publish);validate_transfer_set(after);
    if(financial_sequence(impl_->db)==INT64_MAX)fail(SQLITE_FULL,"Financial order capacity exhausted");
    const auto sequence=financial_sequence(impl_->db)+1;std::vector<std::size_t> changed;
    for(std::size_t i=0;i<before.size();++i)if(transfer_codec::encode(before[i])!=transfer_codec::encode(after[i]))changed.push_back(i);
    if(changed.empty() && (publish || protected_import_rows(batch_id).empty()))return Money(0,account.currency);
    const auto delta=local_total(after,account.identity,account.currency,false)-local_total(before,account.identity,account.currency,false);
    app::TransferRequest request{};request.action=publish?app::TransferAction::import_commit:app::TransferAction::import_discard;request.origin=imports::Occurrence{std::string(batch_id),0};request.expected_sequence=sequence-1;
    Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,account.identity);event.blob(3,transfer_codec::encode(request));event.integer(4,publish?1:0);event.integer(5,delta.units());event.done();
    for(const auto index:changed)store_transfer(impl_->db,after[index],sequence,false);
    if(!publish){const auto balance=committed_balance();append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,balance,Money(0,account.currency),balance,"",sequence-1,catalog_sequence(impl_->db));}
    return delta;
}
void ImportStore::verify_transfers(const app::OperationContext* context) const {
    std::map<std::string,app::TransferView> previous;std::size_t count=0;std::set<std::string> verified_retained;
    struct Scale {std::int64_t sequence; std::int64_t account; std::string operation;};
    std::vector<Scale> scales;Statement scale_rows(impl_->db,"SELECT sequence,ordinal,undo_of FROM ledger_operations WHERE kind=5 ORDER BY sequence");
    while(scale_rows.row())scales.push_back({scale_rows.integer(0),scale_rows.integer(1),scale_rows.bytes(2)});
    std::size_t scale_index=0;
    auto apply_scales=[&](std::int64_t before){
        while(scale_index<scales.size() && scales[scale_index].sequence<before){
            const auto& scale=scales[scale_index++];
            Statement command(impl_->db,"SELECT definition FROM account_operations WHERE id=?");command.text(1,scale.operation);if(!command.row())fail(SQLITE_CORRUPT);
            const auto target=Currency::from_definition(command.bytes(0));
            Statement stored(impl_->db,"SELECT identity,payload FROM transfer_versions WHERE sequence=?");stored.integer(1,scale.sequence);
            while(stored.row()){
                auto saved=transfer_codec::view(stored.bytes(1));auto found=previous.find(saved.link.identity);if(found==previous.end())fail(SQLITE_CORRUPT);
                auto expected=found->second;try{rescale_link_account(expected.link,scale.account,target);}catch(const FinancialError&){fail(SQLITE_CORRUPT);}catch(const transfers::TransferError&){fail(SQLITE_CORRUPT);}
                if(transfer_codec::encode(expected)!=stored.bytes(1))fail(SQLITE_CORRUPT,"Transfer history does not replay");found->second=std::move(expected);++count;
            }
        }
    };
    Statement events(impl_->db,"SELECT e.operation,e.account,e.request,e.did_commit,o.sequence,o.kind,e.local_change,o.preview_sequence,o.change,h.sequence FROM transfer_events e JOIN ledger_operations o ON o.id=e.operation JOIN history_order h ON h.operation=o.id ORDER BY o.sequence");
    while(events.row()){
        if(context)context->checkpoint();const auto request=transfer_codec::request(events.bytes(2));const auto account=account_by_id(events.integer(1));const auto sequence=events.integer(4);apply_scales(sequence);const auto currency=currency_as_of(impl_->db,account.identity,events.integer(9));
        if(events.integer(5)==4 && events.integer(6)!=events.integer(8))fail(SQLITE_CORRUPT,"Transfer receipt differs from its financial effect");
        const bool imported=request.action==app::TransferAction::import_commit || request.action==app::TransferAction::import_discard;
        if(events.integer(5)!=(request.action==app::TransferAction::import_commit?0:4) || request.expected_sequence!=sequence-1 || (request.action!=app::TransferAction::import_commit && events.integer(7)!=sequence-1) || (events.integer(3)!=0)!=(request.action==app::TransferAction::commit_local || request.action==app::TransferAction::import_commit))fail(SQLITE_CORRUPT);
        if(imported){
            if(!request.origin || account_for_batch(request.origin->batch).identity!=account.identity)fail(SQLITE_CORRUPT);
            std::vector<app::TransferView> before;for(const auto& [id,value]:previous){(void)id;before.push_back(value);}
            const auto after=import_transition(before,account.identity,request.origin->batch,request.action==app::TransferAction::import_commit);validate_transfer_set(after);
            std::size_t changed=0;
            for(std::size_t i=0;i<after.size();++i)if(transfer_codec::encode(before[i])!=transfer_codec::encode(after[i])){
                Statement stored(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");stored.text(1,after[i].link.identity);stored.integer(2,sequence);
                if(!stored.row() || stored.bytes(0)!=transfer_codec::encode(after[i]))fail(SQLITE_CORRUPT);previous.at(after[i].link.identity)=after[i];++changed;
            }
            std::set<std::size_t> protected_rows;
            if(request.action==app::TransferAction::import_discard){
                for(const auto& value:before)if(value.link.origin.endpoint.entry.batch==request.origin->batch && transfers::protected_draft(value.link,account.identity))protected_rows.insert(value.link.origin.endpoint.entry.ordinal);
                std::set<std::size_t> saved_rows;Statement kept(impl_->db,"SELECT ordinal FROM retained_transfer_rows WHERE batch=?");kept.text(1,request.origin->batch);while(kept.row())saved_rows.insert(static_cast<std::size_t>(kept.integer(0)));
                if(saved_rows!=protected_rows)fail(SQLITE_CORRUPT,"Protected import membership does not replay");if(!protected_rows.empty())verified_retained.insert(request.origin->batch);
            }
            if((changed==0 && protected_rows.empty()) || (local_total(after,account.identity,currency,false)-local_total(before,account.identity,currency,false)).units()!=events.integer(6))fail(SQLITE_CORRUPT);
            count+=changed;continue;
        }
        if(request.action==app::TransferAction::merge || request.action==app::TransferAction::retarget){
            if(request.kept.empty() || !request.left || (request.action==app::TransferAction::merge && !request.right) || (request.action==app::TransferAction::retarget && request.right))fail(SQLITE_CORRUPT,"Transfer history does not replay");
            std::vector<app::TransferView> before;for(const auto& [id,value]:previous){(void)id;before.push_back(value);}const auto balance_before=local_total(before,account.identity,currency,false);
            std::vector<app::TransferView> after;
            try{after=manage_views(before,request,sequence);}catch(const transfers::TransferError&){fail(SQLITE_CORRUPT,"Transfer history does not replay");}
            const auto changed=changed_views(before,after);if(changed.empty() || events.integer(6)!=0)fail(SQLITE_CORRUPT,"Transfer history does not replay");
            std::set<std::int64_t> accounts{account.identity};
            for(const auto& value:changed){accounts.insert(value.link.origin.endpoint.account);accounts.insert(value.link.destination.endpoint.account);}
            if(request.left)accounts.insert(request.left->endpoint.account);if(request.right)accounts.insert(request.right->endpoint.account);if(request.generated)accounts.insert(request.generated->account);
            require_zero_committed_delta(before,after,accounts,[&](std::int64_t id){return account_by_id(id);},SQLITE_CORRUPT,"Transfer amount does not reconcile");
            if((local_total(after,account.identity,currency,false)-balance_before).units()!=0)fail(SQLITE_CORRUPT,"Transfer amount does not reconcile");
            for(const auto& value:changed){
                Statement stored(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");stored.text(1,value.link.identity);stored.integer(2,sequence);
                if(!stored.row() || stored.bytes(0)!=transfer_codec::encode(value))fail(SQLITE_CORRUPT,"Transfer history does not replay");
                previous.insert_or_assign(value.link.identity,value);++count;
            }
            if(request.action==app::TransferAction::retarget && request.mode==0){
                if(!request.left || !request.left->imported || request.left->state!=transfers::State::draft)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                const auto& fact=*request.left;const auto revision=request.batch_revision+1;
                Statement kind(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");kind.text(1,fact.endpoint.entry.batch);kind.integer(2,revision);
                if(!kind.row() || kind.bytes(0)!="save")fail(SQLITE_CORRUPT,"Transfer history does not replay");
                Statement saved(impl_->db,"SELECT a.category,a.units,m.category_id,v.total,p.total,prior.units FROM allocations a JOIN draft_allocation_meta m USING(batch,ordinal,revision,position) JOIN versions v ON v.batch=a.batch AND v.ordinal=a.ordinal AND v.revision=a.revision JOIN versions p ON p.batch=a.batch AND p.ordinal=a.ordinal AND p.revision=(SELECT max(revision) FROM versions WHERE batch=a.batch AND ordinal=a.ordinal AND revision<a.revision) JOIN draft_allocation_meta pm ON pm.batch=p.batch AND pm.ordinal=p.ordinal AND pm.revision=p.revision AND pm.identity=m.identity JOIN allocations prior ON prior.batch=pm.batch AND prior.ordinal=pm.ordinal AND prior.revision=pm.revision AND prior.position=pm.position WHERE a.batch=? AND a.ordinal=? AND a.revision=? AND m.identity=?");
                saved.text(1,fact.endpoint.entry.batch);saved.integer(2,integer(fact.endpoint.entry.ordinal));saved.integer(3,revision);saved.text(4,fact.endpoint.allocation);
                if(!saved.row() || saved.bytes(0)!=request.category_path || saved.integer(3)!=saved.integer(4) || saved.integer(1)!=saved.integer(5) || Money(saved.integer(1),version_currency(impl_->db,fact.endpoint.entry.batch,integer(fact.endpoint.entry.ordinal),revision))!=fact.amount)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                if(request.category_id.empty()){if(!saved.null(2))fail(SQLITE_CORRUPT,"Transfer history does not replay");}
                else if(saved.null(2) || saved.bytes(2)!=request.category_id)fail(SQLITE_CORRUPT,"Transfer history does not replay");
            }
            std::vector<app::TransferView> replayed;for(const auto& [id,value]:previous){(void)id;replayed.push_back(value);}validate_transfer_set(replayed);continue;
        }
        Statement row(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");row.text(1,request.link);row.integer(2,sequence);if(!row.row())fail(SQLITE_CORRUPT);
        const auto saved=transfer_codec::view(row.bytes(0));if(saved.link.identity!=request.link)fail(SQLITE_CORRUPT);
        std::vector<app::TransferView> before;for(const auto& [id,value]:previous){(void)id;before.push_back(value);}const auto balance_before=local_total(before,account.identity,currency,false);
        auto found=previous.find(request.link);
        if(request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft){
            if(saved.link.destination.endpoint.entry!=imports::Occurrence{"~transfer-"+request.link+"-destination",0} || saved.link.destination.endpoint.allocation!="generated" || found!=previous.end() || saved.created_sequence!=sequence || saved.link.origin.endpoint.account!=account.identity || saved.link.destination.endpoint.account!=request.destination || saved.link.origin.state!=transfers::State::draft || saved.link.destination.state!=transfers::State::draft || saved.link.origin.accepted_unmatched || saved.link.destination.accepted_unmatched || saved.link.origin.proposed_evidence || saved.link.destination.proposed_evidence || saved.link.origin.attached_evidence || saved.link.destination.attached_evidence)fail(SQLITE_CORRUPT);
            if(saved.link.destination.amount.currency()!=currency_as_of(impl_->db,request.destination,events.integer(9)))fail(SQLITE_CORRUPT);
            if(request.action==app::TransferAction::manual){if(saved.link.origin.endpoint.entry!=imports::Occurrence{"~transfer-"+request.link+"-origin",0} || saved.link.origin.endpoint.allocation!="manual" || !request.amount || request.origin || saved.link.origin.amount!=*request.amount || !saved.link.origin.manually_entered || saved.link.generated_date!=request.date || saved.payee!=request.payee || saved.memo!=request.memo)fail(SQLITE_CORRUPT);}
            else {
                if(!request.origin || saved.link.origin.endpoint.entry!=*request.origin || saved.link.origin.endpoint.allocation!=request.allocation || saved.link.origin.manually_entered || account_for_batch(request.origin->batch).identity!=account.identity)fail(SQLITE_CORRUPT);
                Statement source(impl_->db,"SELECT a.units,v.date,v.payee,v.memo FROM versions v JOIN allocations a USING(batch,ordinal,revision) JOIN draft_allocation_meta m USING(batch,ordinal,revision,position) WHERE v.batch=? AND v.ordinal=? AND m.identity=? AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal AND revision<=?)");source.text(1,request.origin->batch);source.integer(2,integer(request.origin->ordinal));source.text(3,request.allocation);source.integer(4,request.batch_revision);
                if(!source.row() || source.integer(0)!=saved.link.origin.amount.units() || source.bytes(1)!=saved.link.generated_date || source.bytes(2)!=saved.payee || source.bytes(3)!=saved.memo)fail(SQLITE_CORRUPT);
            }
            previous.emplace(request.link,saved);
        }else{
            if(request.action==app::TransferAction::propose_match){
                if(!request.evidence || request.evidence->endpoint.account!=account.identity || account_for_batch(request.evidence->endpoint.entry.batch).identity!=account.identity)fail(SQLITE_CORRUPT);
                const auto& evidence=*request.evidence;
                Statement bank(impl_->db,"SELECT a.units,v.date,v.decision FROM versions v JOIN allocations a USING(batch,ordinal,revision) JOIN draft_allocation_meta m USING(batch,ordinal,revision,position) WHERE v.batch=? AND v.ordinal=? AND m.identity=? AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal AND revision<=?)");bank.text(1,evidence.endpoint.entry.batch);bank.integer(2,integer(evidence.endpoint.entry.ordinal));bank.text(3,evidence.endpoint.allocation);bank.integer(4,request.batch_revision);
                if(!bank.row() || bank.integer(0)!=evidence.amount.units() || bank.bytes(1)!=evidence.posting_date || bank.integer(2)!=1)fail(SQLITE_CORRUPT,"Transfer evidence does not match the reviewed bank allocation");
            }
            if(found==previous.end())fail(SQLITE_CORRUPT);const auto expected=transition_transfer(found->second,request,account.identity);
            if(transfer_codec::encode(expected)!=transfer_codec::encode(saved))fail(SQLITE_CORRUPT,"Transfer history does not replay");found->second=saved;
        }
        std::vector<app::TransferView> after;for(const auto& [id,value]:previous){(void)id;after.push_back(value);}validate_transfer_set(after);
        if((local_total(after,account.identity,currency,false)-balance_before).units()!=events.integer(6))fail(SQLITE_CORRUPT,"Transfer amount does not reconcile");++count;
    }
    apply_scales(INT64_MAX);
    Statement retained(impl_->db,"SELECT DISTINCT batch FROM retained_transfer_rows");while(retained.row())if(!verified_retained.contains(retained.bytes(0)))fail(SQLITE_CORRUPT);
    Statement rows(impl_->db,"SELECT count(*) FROM transfer_versions");if(!rows.row() || rows.integer(0)!=integer(count))fail(SQLITE_CORRUPT);
    auto current=all_transfers();if(current.size()!=previous.size())fail(SQLITE_CORRUPT);
    for(const auto& value:current){const auto found=previous.find(value.link.identity);if(found==previous.end() || transfer_codec::encode(found->second)!=transfer_codec::encode(value))fail(SQLITE_CORRUPT);}
    const auto check_evidence=[&](const transfers::Endpoint& endpoint,const Money& amount,std::string_view date){
        const auto source=load_row(endpoint.entry,false);
        if(source.excluded_by_clear || source.decision.disposition!=imports::Disposition::include || !source.draft || account_for_batch(endpoint.entry.batch).identity!=endpoint.account)fail(SQLITE_CORRUPT);
        auto draft=*source.draft;
        if(batch(impl_->db,endpoint.entry.batch).state==app::BatchState::committed){
            Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,endpoint.entry.batch);head.integer(2,integer(endpoint.entry.ordinal));if(!head.row())fail(SQLITE_CORRUPT);
            const auto snapshot=entry_snapshot(endpoint.entry,head.integer(0));if(!snapshot.active)fail(SQLITE_CORRUPT);draft=snapshot.draft;
        }
        const auto& allocations=draft.transaction.allocations();const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return a.identity==endpoint.allocation;});
        if(allocation==allocations.end() || allocation->amount!=amount || draft.date!=date)fail(SQLITE_CORRUPT,"Transfer relationship disagrees with its evidence");
    };
    for(const auto& value:current){
        const auto& origin=value.link.origin;
        if(!origin.manually_entered && origin.state!=transfers::State::removed)check_evidence(origin.endpoint,origin.amount,origin.date);
        if(!origin.manually_entered && origin.state!=transfers::State::removed){const auto source=batch(impl_->db,origin.endpoint.entry.batch);if((source.state==app::BatchState::committed)!=(origin.state==transfers::State::committed) || (source.state!=app::BatchState::committed && source.state!=app::BatchState::pending && source.state!=app::BatchState::protected_drafts))fail(SQLITE_CORRUPT);}
        for(const auto* side:{&value.link.origin,&value.link.destination}){
            for(const auto* evidence:{&side->attached_evidence,&side->proposed_evidence})if(*evidence)check_evidence((*evidence)->endpoint,(*evidence)->amount,(*evidence)->posting_date);
            if(side->attached_evidence && batch(impl_->db,side->attached_evidence->endpoint.entry.batch).state!=app::BatchState::committed)fail(SQLITE_CORRUPT);
            if(side->proposed_evidence && batch(impl_->db,side->proposed_evidence->endpoint.entry.batch).state!=app::BatchState::pending)fail(SQLITE_CORRUPT);
        }
    }
    const auto resolved=resolved_transfers(current);
    Statement committed(impl_->db,"SELECT v.batch,v.ordinal FROM versions v JOIN batches b ON b.id=v.batch WHERE b.state=1 AND v.decision=1 AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
    while(committed.row()) {
        const imports::Occurrence id{committed.bytes(0),static_cast<std::size_t>(committed.integer(1))};
        if(retained_member(impl_->db,id.batch,id.ordinal))require_qif_transfers(id,*load_row(id,false).draft,resolved);
    }
    Statement extra(impl_->db,"SELECT 1 FROM ledger_operations o WHERE kind=4 AND NOT EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=o.id)");if(extra.row())fail(SQLITE_CORRUPT);
}
