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
        if(!local_side(current) && !(current.independent_evidence && current.state==transfers::State::draft && (account==value.link.origin.endpoint.account || (value.link.unresolved && !current.attached_evidence && !current.proposed_evidence))))fail(SQLITE_CONSTRAINT,"Clear imported entries through their import");
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
    case app::TransferAction::relink: return "Relink";
    case app::TransferAction::restore: return "Restore";
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
        : transfers::retarget(links,*request.left,request.link,request.kept,request.generated,generated_currency,request.category_path);
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
    std::set<std::string> retained_ids;for(const auto& value:out)retained_ids.insert(value.link.identity);
    for(const auto& value:values) if(!retained_ids.contains(value.link.identity))
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
bool retired_generated(const transfers::Side& side){return side.state==transfers::State::committed && !side.independent_evidence && !side.manually_entered && !side.attached_evidence;}
bool endpoint_active(const std::vector<app::TransferView>& values,const transfers::Endpoint& endpoint){for(const auto& value:values)for(const auto* side:{&value.link.origin,&value.link.destination})if(side->endpoint==endpoint && side->state!=transfers::State::removed)return true;return false;}
std::string signed_decimal(const Money& value){
    const bool negative=value.units()<0;unsigned long long magnitude=0;
    if(!negative)magnitude=static_cast<unsigned long long>(value.units());
    else if(value.units()==std::numeric_limits<long long>::min())magnitude=1ULL<<63;
    else magnitude=static_cast<unsigned long long>(-value.units());
    auto digits=std::to_string(magnitude);const auto scale=value.currency().scale();
    if(scale>0){if(digits.size()<=scale)digits.insert(0,scale+1-digits.size(),'0');digits.insert(digits.size()-scale,1,'.');}
    return (negative?"-":"+")+digits;
}
std::string stamp_views(std::vector<app::TransferView> changed){
    std::sort(changed.begin(),changed.end(),[](const auto& a,const auto& b){return a.link.identity<b.link.identity;});
    std::string out;for(const auto& value:changed){out+=value.link.identity;out.push_back('\n');out+=transfer_codec::encode(value);out.push_back('\n');}return out;
}
template<class CurrencyOf>
Money managed_local_change(const std::vector<app::TransferView>& before,const std::vector<app::TransferView>& after,std::int64_t selected,CurrencyOf currency_of,int code,bool allow_retirement){
    std::map<std::int64_t,std::vector<Money>> retired;
    for(const auto& value:before)for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(!retired_generated(*side) || endpoint_active(after,side->endpoint))continue;
        try{retired[side->endpoint.account].push_back(-side->amount);}catch(const FinancialError&){fail(code,"This transfer command would change committed money");}
    }
    std::set<std::int64_t> accounts{selected};
    for(const auto& value:before){accounts.insert(value.link.origin.endpoint.account);accounts.insert(value.link.destination.endpoint.account);}
    for(const auto& value:after){accounts.insert(value.link.origin.endpoint.account);accounts.insert(value.link.destination.endpoint.account);}
    for(const auto& [id,amounts]:retired){(void)amounts;accounts.insert(id);}
    int nonzero=0;std::int64_t nonzero_account=0;Money selected_change(0,currency_of(selected));
    for(const auto id:accounts){
        const auto currency=currency_of(id);Money actual(0,currency),allowed(0,currency);
        try{actual=local_total(after,id,currency,false)-local_total(before,id,currency,false);if(const auto found=retired.find(id);found!=retired.end())allowed=imports::sum(currency,found->second);}
        catch(const FinancialError&){fail(code,"This transfer command would change committed money");}
        if(actual!=allowed)fail(code,"This transfer command would change committed money");
        if(allowed.units()!=0){++nonzero;nonzero_account=id;}
        if(id==selected)selected_change=actual;
    }
    if(nonzero>1 || (!allow_retirement && nonzero!=0))fail(code,"This transfer command would change committed money");
    if(nonzero==1 && nonzero_account!=selected)fail(code,"Open the account that holds the generated duplicate before merging it.");
    return selected_change;
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
        const std::string category=side==&value.link.origin && !value.link.origin_category.empty()?value.link.origin_category:"Transfer";
        Allocation allocation{category,value.memo,side->amount,{},side->endpoint.allocation};allocation.category_owner=allocation.memo_owner=FieldOwnership::manual;
        out.push_back({side->endpoint.entry,-value.created_sequence,{side->date,value.payee,value.memo,Transaction::manual(side->amount,{allocation}),FieldOwnership::manual,FieldOwnership::manual},side->state==transfers::State::committed});
    }
    return out;
}
bool ImportStore::transfer_linked(const imports::Occurrence& id) const {
    for(const auto& value:all_transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(side->state==transfers::State::removed)continue;
        const bool reserve=!value.link.unresolved || local_side(*side);
        if(reserve && side->endpoint.entry==id)return true;
        if(!value.link.unresolved && ((side->attached_evidence && side->attached_evidence->endpoint.entry==id) || (side->proposed_evidence && side->proposed_evidence->endpoint.entry==id)))return true;
    }
    return false;
}
void ImportStore::require_unlinked(const imports::Occurrence& id) const {
    if(transfer_linked(id))fail(SQLITE_CONSTRAINT,"Transfer-linked entries require transfer-aware editing; this command is unavailable");
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
        const bool merging=request.action==app::TransferAction::merge;
        const auto change=managed_local_change(values,managed_values,account.identity,[&](std::int64_t id){return account_by_id(id).currency;},SQLITE_CONSTRAINT,merging);
        bool entry_category=false;std::optional<app::EntrySnapshot> entry_after;imports::Occurrence entry_identity{};std::vector<app::DependencyStamp> entry_dependencies;
        if(request.action==app::TransferAction::retarget && request.mode==0){
            const auto& fact=*request.left;
            safe_text(request.category_path);
            if(!request.category_id.empty()){identifier(request.category_id);const auto display=resolve_category(request.category_id);if(!display.assignable || display.path!=request.category_path)fail(SQLITE_CONSTRAINT,"Category choice changed or is archived; choose an active leaf again");}
            auto rewrite=[&](imports::DraftRow edit,const std::optional<imports::DraftRow>& previous,const qif::Record& source,std::int64_t revision,bool entry){
                auto allocations=edit.transaction.allocations();bool matched=false;
                for(auto& allocation:allocations)if(allocation.identity==fact.endpoint.allocation){
                    matched=true;allocation.category=request.category_path;allocation.category_owner=FieldOwnership::manual;allocation.category_rule.reset();
                    if(request.category_id.empty())allocation.category_binding.reset();else allocation.category_binding=CategoryBinding{request.category_id,catalog_sequence(impl_->db)};
                }
                if(!matched)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
                edit.transaction=Transaction::repaired_import(edit.transaction.original_imported_amount(),edit.transaction.effective_total(),std::move(allocations),edit.transaction.reassigned_currency());
                const auto canonical=canonical_draft(fact.endpoint.entry,edit,previous,revision,entry,true);
                const bool reassigned=source.source_currency && canonical.transaction.effective_total().currency()!=*source.source_currency;
                imports::validate_draft(source,canonical,reassigned);return canonical;
            };
            if(fact.manually_entered){
                if(fact.state!=transfers::State::committed || fact.imported)fail(SQLITE_CONSTRAINT,"Invalid transfer command");
                bool recorded=false;for(const auto& value:managed_values)if(value.link.identity==request.link && value.link.origin_category==request.category_path && value.link.unresolved && value.link.origin.state==transfers::State::committed)recorded=true;
                if(!recorded)fail(SQLITE_CONSTRAINT,"Invalid transfer command");
            }else if(fact.imported && fact.state==transfers::State::draft){
                const auto info=batch(impl_->db,fact.endpoint.entry.batch);
                if(info.state!=app::BatchState::pending && info.state!=app::BatchState::protected_drafts)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");
                const auto loaded=load_row(fact.endpoint.entry,false);if(!loaded.draft)fail(SQLITE_CONSTRAINT,"Resolve the draft before assigning a transfer");
                const auto canonical=rewrite(*loaded.draft,loaded.draft,loaded.source,info.revision+1,false);
                auto write_category=[&]{version(impl_->db,fact.endpoint.entry,info.revision+1,canonical,loaded.decision,false);advance(impl_->db,fact.endpoint.entry.batch,info.revision,"save",static_cast<int>(info.state));};
                if(info.state==app::BatchState::protected_drafts){struct CommandScope{int& slot;explicit CommandScope(int& value):slot(value){slot=1;}~CommandScope(){slot=0;}} scope(impl_->account_command);write_category();}
                else write_category();
            }else if(fact.imported && fact.state==transfers::State::committed){
                if(fact.endpoint.account!=account.identity)fail(SQLITE_CONSTRAINT,"Open the account that holds this transaction before changing its category");
                const auto current=committed_entry(fact.endpoint.entry);if(!current.current.active)fail(SQLITE_CONSTRAINT,"Transfer allocation changed; refresh before applying");
                const auto canonical=rewrite(current.current.draft,current.current.draft,current.source,current.current.revision+1,true);
                if(canonical.transaction.effective_total()!=current.current.draft.transaction.effective_total())fail(SQLITE_CONSTRAINT,"This transfer command would change committed money");
                entry_after=current.current;entry_after->revision++;entry_after->draft=canonical;entry_after->active=true;entry_identity=fact.endpoint.entry;entry_dependencies=current.token.dependencies;entry_category=true;
                store_entry(impl_->db,entry_identity,*entry_after,std::nullopt);
            }else fail(SQLITE_CONSTRAINT,"Invalid transfer command");
        }
        std::set<std::string> prior_ids;for(const auto& value:values)prior_ids.insert(value.link.identity);
        const Money zero(0,account.currency);const bool did_commit=change.units()!=0;
        Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");event.text(1,operation);event.integer(2,account.identity);event.blob(3,encoded);event.integer(4,did_commit?1:0);event.integer(5,change.units());event.done();
        for(const auto& value:changed)store_transfer(impl_->db,value,sequence,!prior_ids.contains(value.link.identity));
        if(entry_category){
            append_financial(impl_->db,operation,app::ChangeKind::correct,entry_identity,entry_after->revision-1,entry_after->revision,before,zero,before,"",request.expected_sequence,catalog_sequence(impl_->db));
            for(const auto& dependency:entry_dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,operation);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
        }else append_financial(impl_->db,operation,app::ChangeKind::transfer,{"",0},-1,0,before,change,before+change,"",request.expected_sequence,catalog_sequence(impl_->db));
        impl_->finish(transaction);return {std::string(operation),account.identity,sequence,before,entry_category?zero:change,before+(entry_category?zero:change)};
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
        const int kind=static_cast<int>(events.integer(5));
        const bool committed_category=request.action==app::TransferAction::retarget && request.mode==0 && request.left && request.left->imported && !request.left->manually_entered && request.left->state==transfers::State::committed;
        int expected_kind=4;
        if(request.action==app::TransferAction::import_commit)expected_kind=0;
        else if(request.action==app::TransferAction::relink){if(kind!=static_cast<int>(app::ChangeKind::correct) && kind!=static_cast<int>(app::ChangeKind::remove))fail(SQLITE_CORRUPT,"Transfer history does not replay");expected_kind=kind;}
        else if(request.action==app::TransferAction::restore)expected_kind=static_cast<int>(app::ChangeKind::undo);
        else if(committed_category)expected_kind=static_cast<int>(app::ChangeKind::correct);
        const bool posts_money=request.action==app::TransferAction::commit_local || request.action==app::TransferAction::import_commit || (request.action==app::TransferAction::merge && events.integer(6)!=0);
        if(kind!=expected_kind || request.expected_sequence!=sequence-1 || (request.action!=app::TransferAction::import_commit && events.integer(7)!=sequence-1) || (events.integer(3)!=0)!=posts_money)fail(SQLITE_CORRUPT);
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
        if(request.action==app::TransferAction::relink || request.action==app::TransferAction::restore){
            if(events.integer(6)!=0)fail(SQLITE_CORRUPT,"Transfer history does not replay");
            EntryLinkPlan expected;
            try{
                if(request.action==app::TransferAction::restore){
                    Statement undone(impl_->db,"SELECT undo_of FROM ledger_operations WHERE sequence=?");undone.integer(1,sequence);if(!undone.row() || undone.bytes(0).empty())fail(SQLITE_CORRUPT);
                    expected=plan_restore(undone.bytes(0));
                }else{
                    Statement op(impl_->db,"SELECT batch,ordinal,prior_revision,result_revision,kind FROM ledger_operations WHERE sequence=?");op.integer(1,sequence);if(!op.row())fail(SQLITE_CORRUPT);
                    const imports::Occurrence id{op.bytes(0),static_cast<std::size_t>(op.integer(1))};
                    std::vector<app::TransferView> before;for(const auto& [identity,value]:previous){(void)identity;before.push_back(value);}
                    expected=plan_entry_links(before,id,entry_snapshot(id,op.integer(2)).draft,entry_snapshot(id,op.integer(3)).draft,op.integer(4)==static_cast<int>(app::ChangeKind::remove));
                }
            }catch(const ImportError&){fail(SQLITE_CORRUPT,"Transfer history does not replay");}
            catch(const transfers::TransferError&){fail(SQLITE_CORRUPT,"Transfer history does not replay");}
            catch(const FinancialError&){fail(SQLITE_CORRUPT,"Transfer history does not replay");}
            if(expected.changed.empty())fail(SQLITE_CORRUPT,"Transfer history does not replay");
            std::set<std::string> ids;
            for(const auto& value:expected.changed){
                ids.insert(value.link.identity);
                Statement stored(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");stored.text(1,value.link.identity);stored.integer(2,sequence);
                if(!stored.row() || stored.bytes(0)!=transfer_codec::encode(value))fail(SQLITE_CORRUPT,"Transfer history does not replay");
                previous.insert_or_assign(value.link.identity,value);++count;
            }
            Statement extras(impl_->db,"SELECT identity FROM transfer_versions WHERE sequence=?");extras.integer(1,sequence);
            while(extras.row())if(!ids.contains(extras.bytes(0)))fail(SQLITE_CORRUPT,"Transfer history does not replay");
            std::vector<app::TransferView> replayed;for(const auto& [id,value]:previous){(void)id;replayed.push_back(value);}validate_transfer_set(replayed);continue;
        }
        if(request.action==app::TransferAction::merge || request.action==app::TransferAction::retarget){
            if(request.kept.empty() || !request.left || (request.action==app::TransferAction::merge && !request.right) || (request.action==app::TransferAction::retarget && request.right))fail(SQLITE_CORRUPT,"Transfer history does not replay");
            std::vector<app::TransferView> before;for(const auto& [id,value]:previous){(void)id;before.push_back(value);}
            std::vector<app::TransferView> after;
            try{after=manage_views(before,request,sequence);}catch(const transfers::TransferError&){fail(SQLITE_CORRUPT,"Transfer history does not replay");}
            const auto changed=changed_views(before,after);if(changed.empty())fail(SQLITE_CORRUPT,"Transfer history does not replay");
            const auto recorded=managed_local_change(before,after,account.identity,[&](std::int64_t id){return currency_as_of(impl_->db,id,events.integer(9));},SQLITE_CORRUPT,request.action==app::TransferAction::merge);
            if(recorded.units()!=events.integer(6))fail(SQLITE_CORRUPT,"Transfer amount does not reconcile");
            for(const auto& value:changed){
                Statement stored(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");stored.text(1,value.link.identity);stored.integer(2,sequence);
                if(!stored.row() || stored.bytes(0)!=transfer_codec::encode(value))fail(SQLITE_CORRUPT,"Transfer history does not replay");
                previous.insert_or_assign(value.link.identity,value);++count;
            }
            if(request.action==app::TransferAction::retarget && request.mode==0){
                if(!request.left)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                const auto& fact=*request.left;
                if(fact.manually_entered){
                    if(fact.state!=transfers::State::committed)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                    bool recorded_category=false;
                    for(const auto& value:changed)if(value.link.origin.endpoint==fact.endpoint && value.link.origin_category==request.category_path && value.link.unresolved && value.link.origin.state==transfers::State::committed)recorded_category=true;
                    if(!recorded_category)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                }else if(fact.imported && fact.state==transfers::State::committed){
                    Statement op(impl_->db,"SELECT result_revision,change FROM ledger_operations WHERE sequence=? AND kind=1");op.integer(1,sequence);
                    if(!op.row() || op.integer(1)!=0)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                    const auto after_entry=entry_snapshot(fact.endpoint.entry,op.integer(0));
                    const auto& allocations=after_entry.draft.transaction.allocations();
                    const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==fact.endpoint.allocation;});
                    if(!after_entry.active || allocation==allocations.end() || allocation->category!=request.category_path || allocation->amount!=fact.amount)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                    if(request.category_id.empty()){if(allocation->category_binding)fail(SQLITE_CORRUPT,"Transfer history does not replay");}
                    else if(!allocation->category_binding || allocation->category_binding->identity!=request.category_id)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                    bool released=false;
                    for(const auto& value:after)if(value.link.identity==request.link){
                        const bool both=value.link.origin.state==transfers::State::removed && value.link.destination.state==transfers::State::removed;
                        const bool unresolved=value.link.unresolved && value.link.origin.state==transfers::State::removed && value.link.destination.state==transfers::State::committed;
                        if(both || unresolved)released=true;
                    }
                    if(!released)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                }else if(!fact.imported || fact.state!=transfers::State::draft)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                else{const auto revision=request.batch_revision+1;
                Statement kind(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");kind.text(1,fact.endpoint.entry.batch);kind.integer(2,revision);
                if(!kind.row() || kind.bytes(0)!="save")fail(SQLITE_CORRUPT,"Transfer history does not replay");
                Statement saved(impl_->db,"SELECT a.category,a.units,m.category_id,v.total,p.total,prior.units FROM allocations a JOIN draft_allocation_meta m USING(batch,ordinal,revision,position) JOIN versions v ON v.batch=a.batch AND v.ordinal=a.ordinal AND v.revision=a.revision JOIN versions p ON p.batch=a.batch AND p.ordinal=a.ordinal AND p.revision=(SELECT max(revision) FROM versions WHERE batch=a.batch AND ordinal=a.ordinal AND revision<a.revision) JOIN draft_allocation_meta pm ON pm.batch=p.batch AND pm.ordinal=p.ordinal AND pm.revision=p.revision AND pm.identity=m.identity JOIN allocations prior ON prior.batch=pm.batch AND prior.ordinal=pm.ordinal AND prior.revision=pm.revision AND prior.position=pm.position WHERE a.batch=? AND a.ordinal=? AND a.revision=? AND m.identity=?");
                saved.text(1,fact.endpoint.entry.batch);saved.integer(2,integer(fact.endpoint.entry.ordinal));saved.integer(3,revision);saved.text(4,fact.endpoint.allocation);
                if(!saved.row() || saved.bytes(0)!=request.category_path || saved.integer(3)!=saved.integer(4) || saved.integer(1)!=saved.integer(5) || Money(saved.integer(1),version_currency(impl_->db,fact.endpoint.entry.batch,integer(fact.endpoint.entry.ordinal),revision))!=fact.amount)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                if(request.category_id.empty()){if(!saved.null(2))fail(SQLITE_CORRUPT,"Transfer history does not replay");}
                else if(saved.null(2) || saved.bytes(2)!=request.category_id)fail(SQLITE_CORRUPT,"Transfer history does not replay");
                }
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
namespace {
bool mentions_entry(const app::TransferView& value,const imports::Occurrence& id){
    auto hit=[&](const transfers::Side& side){
        if(side.endpoint.entry==id)return true;
        if(side.attached_evidence && side.attached_evidence->endpoint.entry==id)return true;
        if(side.proposed_evidence && side.proposed_evidence->endpoint.entry==id)return true;
        return false;
    };
    return hit(value.link.origin) || hit(value.link.destination);
}
const Allocation* allocation_for(const imports::DraftRow& draft,std::string_view identity){
    const auto& values=draft.transaction.allocations();
    const auto found=std::find_if(values.begin(),values.end(),[&](const Allocation& value){return value.identity==identity;});
    return found==values.end()?nullptr:&*found;
}
transfers::AllocationFact fact_from(const transfers::Side& side){
    const bool generated=!side.independent_evidence && !side.manually_entered && !side.attached_evidence;
    return {side.endpoint,side.amount,side.date,side.state,side.independent_evidence,generated,side.manually_entered};
}
}
ImportStore::EntryLinkPlan ImportStore::plan_entry_links(const std::vector<app::TransferView>& before,const imports::Occurrence& id,const imports::DraftRow& before_draft,const imports::DraftRow& after_draft,bool remove) const {
    for(const auto& value:before)for(const auto* side:{&value.link.origin,&value.link.destination}){
        const bool attached=side->attached_evidence && side->attached_evidence->endpoint.entry==id;
        const bool proposed=side->proposed_evidence && side->proposed_evidence->endpoint.entry==id;
        if(attached || proposed)fail(SQLITE_CONSTRAINT,"Replacing transfer evidence requires the repair policy");
    }
    auto working=before;
    struct Hit { std::size_t index; bool origin; };
    std::vector<Hit> hits;
    for(std::size_t i=0;i<working.size();++i){
        const auto& link=working[i].link;
        if(link.unresolved)continue;
        if(link.origin.state!=transfers::State::removed && link.origin.endpoint.entry==id)hits.push_back({i,true});
        if(link.destination.state!=transfers::State::removed && link.destination.endpoint.entry==id)hits.push_back({i,false});
    }
    std::vector<std::string> notes;
    for(const auto& hit:hits){
        auto& view=working[hit.index];
        if(view.link.unresolved)continue;
        const auto& side=hit.origin?view.link.origin:view.link.destination;
        if(side.state==transfers::State::removed || side.endpoint.entry!=id)continue;
        const transfers::Link* host=nullptr;
        for(const auto& value:working){
            if(value.link.unresolved)continue;
            for(const auto* candidate:{&value.link.origin,&value.link.destination})if(candidate->state!=transfers::State::removed && candidate->endpoint==side.endpoint){
                if(host)fail(SQLITE_CONSTRAINT,"Transfer relationship disagrees with its evidence");
                host=&value.link;
            }
        }
        if(!host)fail(SQLITE_CONSTRAINT,"Transfer relationship disagrees with its evidence");
        const auto* prior=allocation_for(before_draft,side.endpoint.allocation);
        if(!prior || prior->amount!=side.amount || before_draft.date!=side.date)fail(SQLITE_CONSTRAINT,"Transfer relationship disagrees with its evidence");
        const auto* next=allocation_for(after_draft,side.endpoint.allocation);
        const bool removing=remove || next==nullptr;
        const bool keep=!removing && next->category==prior->category && next->category_binding==prior->category_binding;
        const Money after_amount=removing?side.amount:next->amount;
        const std::string after_date=removing?side.date:after_draft.date;
        const auto counterpart=hit.origin?view.link.destination:view.link.origin;
        const auto fact=fact_from(side);
        std::vector<transfers::Link> links;links.reserve(working.size());
        for(const auto& value:working)links.push_back(value.link);
        const auto edit=transfers::edit_linked_allocation(links,fact,removing,keep,after_amount,after_date);
        view.link=edit.link;
        if(edit.fate==transfers::LinkFate::unchanged)continue;
        const auto name=account_by_id(counterpart.endpoint.account).name;
        if(edit.fate==transfers::LinkFate::dated)notes.push_back(name+"'s date and amount stay. The link remains.");
        else if(edit.fate==transfers::LinkFate::removed_generated)notes.push_back("The generated "+name+" draft is removed.");
        else notes.push_back(name+" "+signed_decimal(counterpart.amount)+" stays. The link ends.");
    }
    validate_transfer_set(working);
    std::map<std::int64_t,Currency> currencies;
    auto collect=[&](const std::vector<app::TransferView>& values){
        for(const auto& value:values)for(const auto* side:{&value.link.origin,&value.link.destination})currencies.emplace(side->endpoint.account,side->amount.currency());
    };
    collect(before);collect(working);
    for(const auto& [account,currency]:currencies){
        Money delta(0,currency);
        try{delta=local_total(working,account,currency,false)-local_total(before,account,currency,false);}
        catch(const FinancialError&){fail(SQLITE_CONSTRAINT,"This correction would change a linked transfer balance");}
        if(delta.units()!=0)fail(SQLITE_CONSTRAINT,"This correction would change a linked transfer balance");
    }
    auto changed=changed_views(before,working);
    std::sort(changed.begin(),changed.end(),[](const auto& left,const auto& right){return left.link.identity<right.link.identity;});
    EntryLinkPlan plan;plan.changed=std::move(changed);
    for(const auto& note:notes){if(!plan.relationship.empty())plan.relationship.push_back(' ');plan.relationship+=note;}
    plan.stamp=stamp_views(plan.changed);
    return plan;
}
ImportStore::EntryLinkPlan ImportStore::plan_restore(std::string_view operation) const {
    Statement seq(impl_->db,"SELECT sequence FROM ledger_operations WHERE id=?");seq.text(1,operation);
    if(!seq.row())fail(SQLITE_CORRUPT,"Transfer history does not replay");
    const auto sequence=seq.integer(0);
    Statement rows(impl_->db,"SELECT identity,payload FROM transfer_versions WHERE sequence=? ORDER BY identity");rows.integer(1,sequence);
    EntryLinkPlan plan;
    while(rows.row()){
        const auto identity=rows.bytes(0);
        Statement prior(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence<? ORDER BY sequence DESC LIMIT 1");
        prior.text(1,identity);prior.integer(2,sequence);
        if(!prior.row())fail(SQLITE_CORRUPT,"Transfer history does not replay");
        const auto bytes=prior.bytes(0);
        const auto value=transfer_codec::view(bytes);
        if(value.link.identity!=identity || transfer_codec::encode(value)!=bytes)fail(SQLITE_CORRUPT,"Transfer history does not replay");
        plan.changed.push_back(value);
    }
    plan.stamp=stamp_views(plan.changed);
    return plan;
}
bool ImportStore::later_relationship_blocks(const imports::Occurrence& id,std::string_view operation) const {
    Statement place(impl_->db,"SELECT o.sequence,h.sequence FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.id=?");place.text(1,operation);
    if(!place.row())fail(SQLITE_CORRUPT,"Transfer history does not replay");
    const auto ledger=place.integer(0),history=place.integer(1);
    Statement versions(impl_->db,"SELECT count(*) FROM transfer_versions WHERE sequence=?");versions.integer(1,ledger);
    if(!versions.row())fail(SQLITE_CORRUPT,"Transfer history does not replay");
    const auto version_count=versions.integer(0);
    bool mentioned=false;
    Statement heads(impl_->db,"SELECT h.sequence,v.payload FROM transfer_heads h JOIN transfer_versions v USING(identity,sequence)");
    while(heads.row()){
        const auto head_sequence=heads.integer(0);
        const auto value=transfer_codec::view(heads.bytes(1));
        if(!mentions_entry(value,id))continue;
        mentioned=true;
        if(head_sequence>ledger)return true;
    }
    if(version_count==0 && !mentioned)return false;
    Statement written(impl_->db,"SELECT h.sequence FROM transfer_versions v JOIN transfer_heads h ON h.identity=v.identity WHERE v.sequence=?");written.integer(1,ledger);
    std::int64_t seen=0;
    while(written.row()){if(written.integer(0)!=ledger)return true;++seen;}
    if(seen!=version_count)return true;
    std::vector<imports::Occurrence> watched;
    {
        Statement payloads(impl_->db,"SELECT payload FROM transfer_versions WHERE sequence=?");payloads.integer(1,ledger);
        std::vector<std::string> blobs;while(payloads.row())blobs.push_back(payloads.bytes(0));
        auto watch=[&](const imports::Occurrence& entry){
            if(entry.batch.starts_with("~transfer-"))return;
            for(const auto& existing:watched)if(existing==entry)return;
            watched.push_back(entry);
        };
        for(const auto& blob:blobs){
            const auto value=transfer_codec::view(blob);
            for(const auto* side:{&value.link.origin,&value.link.destination}){
                watch(side->endpoint.entry);
                if(side->attached_evidence)watch(side->attached_evidence->endpoint.entry);
                if(side->proposed_evidence)watch(side->proposed_evidence->endpoint.entry);
            }
        }
    }
    for(const auto& entry:watched){
        Statement later(impl_->db,"SELECT 1 FROM ledger_operations WHERE batch=? AND ordinal=? AND kind IN(1,2,3) AND sequence>? LIMIT 1");
        later.text(1,entry.batch);later.integer(2,integer(entry.ordinal));later.integer(3,ledger);
        if(later.row())return true;
    }
    Statement newer(impl_->db,"SELECT 1 FROM source_associations a JOIN ledger_operations pub ON pub.kind=0 AND pub.batch=a.source_batch JOIN history_order h ON h.operation=pub.id WHERE a.ledger_batch=? AND a.ledger_ordinal=? AND h.sequence>? LIMIT 1");
    newer.text(1,id.batch);newer.integer(2,integer(id.ordinal));newer.integer(3,history);
    return newer.row();
}
void ImportStore::write_entry_links(std::string_view operation,std::int64_t account,app::TransferAction action,std::int64_t sequence,const std::vector<app::TransferView>& changed) {
    if(changed.empty() || sequence<=0 || (action!=app::TransferAction::relink && action!=app::TransferAction::restore))fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    app::TransferRequest request{};
    request.action=action;request.link=changed.front().link.identity;request.expected_sequence=sequence-1;
    const auto encoded=transfer_codec::encode(request);
    if(transfer_codec::request(encoded).action!=action || transfer_codec::request(encoded).link!=request.link)fail(SQLITE_CORRUPT,"Transfer history does not replay");
    Statement event(impl_->db,"INSERT INTO transfer_events VALUES(?,?,?,?,?)");
    event.text(1,operation);event.integer(2,account);event.blob(3,encoded);event.integer(4,0);event.integer(5,0);event.done();
    for(const auto& value:changed)store_transfer(impl_->db,value,sequence,false);
}
app::TransferPreview ImportStore::preview_transfer(const app::TransferRequest& request) const {
    identifier(request.link);
    if(request.expected_sequence!=transfer_sequence() || request.expected_sequence==INT64_MAX)fail(SQLITE_CONSTRAINT,"Transfer preview changed; refresh before applying");
    if(request.action!=app::TransferAction::merge && request.action!=app::TransferAction::retarget)fail(SQLITE_CONSTRAINT,"Invalid transfer command");
    struct ReadScope{bool& slot;bool prior;explicit ReadScope(bool& value):slot(value),prior(value){slot=true;}~ReadScope(){slot=prior;}} reading(impl_->verifying);
    require_writable_account(impl_->db,impl_->selected_account);
    require_management_shape(request);
    std::optional<Currency> generated_currency;
    if(!request.generated_currency.empty()){try{generated_currency=Currency::from_definition(request.generated_currency);}catch(const std::exception&){fail(SQLITE_CONSTRAINT,"Invalid transfer command");}}
    if(request.generated){
        const auto destination=account_by_id(request.generated->account);require_writable_account(impl_->db,destination.identity);
        if(request.mode==1 && (!generated_currency || destination.currency!=*generated_currency))fail(SQLITE_CONSTRAINT,"Cross-currency transfers are unsupported");
    }
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
    const auto account=this->account();
    const auto values=all_transfers();
    std::vector<app::TransferView> managed;
    try{managed=manage_views(values,request,request.expected_sequence+1);}
    catch(const transfers::TransferError& error){fail(SQLITE_CONSTRAINT,error.what());}
    const auto change=managed_local_change(values,managed,account.identity,[&](std::int64_t id){return account_by_id(id).currency;},SQLITE_CONSTRAINT,request.action==app::TransferAction::merge);
    const auto before=committed_balance();
    std::string note;
    if(request.action==app::TransferAction::merge && change.units()!=0){
        std::string other;
        for(const auto& value:values)for(const auto* side:{&value.link.origin,&value.link.destination}){
            if(!retired_generated(*side) || endpoint_active(managed,side->endpoint))continue;
            const auto* counterpart=side==&value.link.origin?&value.link.destination:&value.link.origin;
            other=account_by_id(counterpart->endpoint.account).name;
        }
        note=account.name+" changes by "+signed_decimal(change)+". "+other+" is unchanged.";
    }
    return {before,change,before+change,std::move(note)};
}
