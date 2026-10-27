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
        if(side->endpoint.entry==id || (side->attached_evidence && side->attached_evidence->endpoint.entry==id) || (side->proposed_evidence && side->proposed_evidence->endpoint.entry==id))
            fail(SQLITE_CONSTRAINT,"Transfer-linked entries require transfer-aware editing; this command is unavailable");
    }
}
std::optional<app::TransferReceipt> ImportStore::transfer_outcome(std::string_view operation) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT t.account,o.sequence,o.before,o.change,o.after FROM transfer_events t JOIN ledger_operations o ON o.id=t.operation WHERE t.operation=?");row.text(1,operation);
    if(!row.row())return {};
    if(!impl_->verifying && row.integer(0)!=impl_->selected_account)fail(SQLITE_CONSTRAINT,"Transfer operation belongs to another account");
    const auto currency=account_by_id(row.integer(0)).currency;
    return app::TransferReceipt{std::string(operation),row.integer(0),row.integer(1),Money(row.integer(2),currency),Money(row.integer(3),currency),Money(row.integer(4),currency)};
}
app::TransferReceipt ImportStore::apply_transfer(std::string_view operation,const app::TransferRequest& request){
    identifier(operation);identifier(request.link);const auto encoded=transfer_codec::encode(request);
    if(const auto prior=transfer_outcome(operation)){Statement saved(impl_->db,"SELECT request FROM transfer_events WHERE operation=?");saved.text(1,operation);if(!saved.row() || saved.bytes(0)!=encoded)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");return *prior;}
    Statement collision(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");collision.text(1,operation);if(collision.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    if(request.expected_sequence!=transfer_sequence() || request.expected_sequence==INT64_MAX)fail(SQLITE_CONSTRAINT,"Transfer preview changed; refresh before applying");
    const auto account=this->account();auto values=all_transfers();auto found=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.link.identity==request.link;});
    const bool create=request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft;
    const auto before=committed_balance();const auto local_before=local_total(values,account.identity,account.currency,false);const auto sequence=request.expected_sequence+1;
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
    Atomic transaction(impl_->db);
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
    Statement event(impl_->db,"SELECT account,local_change FROM transfer_events WHERE operation=?");event.text(1,operation);if(!event.row()){Statement op(impl_->db,"SELECT batch FROM operations WHERE id=?");op.text(1,operation);if(!op.row())fail(SQLITE_CORRUPT);return Money(0,account_for_batch(op.bytes(0)).currency);}
    return Money(event.integer(1),account_by_id(event.integer(0)).currency);
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
    Statement events(impl_->db,"SELECT e.operation,e.account,e.request,e.did_commit,o.sequence,o.kind,e.local_change,o.preview_sequence,o.change FROM transfer_events e JOIN ledger_operations o ON o.id=e.operation ORDER BY o.sequence");
    while(events.row()){
        if(context)context->checkpoint();const auto request=transfer_codec::request(events.bytes(2));const auto account=account_by_id(events.integer(1));const auto sequence=events.integer(4);
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
            if((changed==0 && protected_rows.empty()) || (local_total(after,account.identity,account.currency,false)-local_total(before,account.identity,account.currency,false)).units()!=events.integer(6))fail(SQLITE_CORRUPT);
            count+=changed;continue;
        }
        Statement row(impl_->db,"SELECT payload FROM transfer_versions WHERE identity=? AND sequence=?");row.text(1,request.link);row.integer(2,sequence);if(!row.row())fail(SQLITE_CORRUPT);
        const auto saved=transfer_codec::view(row.bytes(0));if(saved.link.identity!=request.link)fail(SQLITE_CORRUPT);
        std::vector<app::TransferView> before;for(const auto& [id,value]:previous){(void)id;before.push_back(value);}const auto balance_before=local_total(before,account.identity,account.currency,false);
        auto found=previous.find(request.link);
        if(request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft){
            if(saved.link.destination.endpoint.entry!=imports::Occurrence{"~transfer-"+request.link+"-destination",0} || saved.link.destination.endpoint.allocation!="generated" || found!=previous.end() || saved.created_sequence!=sequence || saved.link.origin.endpoint.account!=account.identity || saved.link.destination.endpoint.account!=request.destination || saved.link.origin.state!=transfers::State::draft || saved.link.destination.state!=transfers::State::draft || saved.link.origin.accepted_unmatched || saved.link.destination.accepted_unmatched || saved.link.origin.proposed_evidence || saved.link.destination.proposed_evidence || saved.link.origin.attached_evidence || saved.link.destination.attached_evidence)fail(SQLITE_CORRUPT);
            if(saved.link.destination.amount.currency()!=account_by_id(request.destination).currency)fail(SQLITE_CORRUPT);
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
        if((local_total(after,account.identity,account.currency,false)-balance_before).units()!=events.integer(6))fail(SQLITE_CORRUPT,"Transfer amount does not reconcile");++count;
    }
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
    Statement extra(impl_->db,"SELECT 1 FROM ledger_operations o WHERE kind=4 AND NOT EXISTS(SELECT 1 FROM transfer_events e WHERE e.operation=o.id)");if(extra.row())fail(SQLITE_CORRUPT);
}
