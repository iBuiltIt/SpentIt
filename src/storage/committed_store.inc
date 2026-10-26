namespace {
bool same_draft(const imports::DraftRow& a,const imports::DraftRow& b) {
    if(a.payee_owner!=b.payee_owner || a.memo_owner!=b.memo_owner || a.date!=b.date || a.payee!=b.payee || a.memo!=b.memo || a.transaction.effective_total()!=b.transaction.effective_total() || a.transaction.original_imported_amount()!=b.transaction.original_imported_amount())return false;
    const auto& aa=a.transaction.allocations();const auto& bb=b.transaction.allocations();if(aa.size()!=bb.size())return false;
    for(std::size_t i=0;i<aa.size();++i)if(aa[i].category_binding!=bb[i].category_binding || aa[i].identity!=bb[i].identity || aa[i].source_ordinal!=bb[i].source_ordinal || aa[i].category_owner!=bb[i].category_owner || aa[i].memo_owner!=bb[i].memo_owner || aa[i].category!=bb[i].category || aa[i].memo!=bb[i].memo || aa[i].amount!=bb[i].amount)return false;
    return true;
}
void store_entry(sqlite3* db,const imports::Occurrence& id,const app::EntrySnapshot& snapshot) {
    Statement row(db,"INSERT INTO entry_revisions VALUES(?,?,?,?,?,?,?,?)");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,snapshot.revision);row.integer(4,snapshot.active?1:0);row.text(5,snapshot.draft.date);row.text(6,snapshot.draft.payee);row.text(7,snapshot.draft.memo);row.integer(8,snapshot.draft.transaction.effective_total().units());row.done();
    std::int64_t position=0;for(const auto& a:snapshot.draft.transaction.allocations()){Statement allocation(db,"INSERT INTO entry_allocations VALUES(?,?,?,?,?,?,?)");allocation.text(1,id.batch);allocation.integer(2,integer(id.ordinal));allocation.integer(3,snapshot.revision);allocation.integer(4,position++);allocation.text(5,a.category);allocation.text(6,a.memo);allocation.integer(7,a.amount.units());allocation.done();}
    save_metadata(db,id,snapshot.revision,snapshot.draft,true);
    Statement head(db,snapshot.revision==0?"INSERT INTO entry_heads VALUES(?,?,?)":"UPDATE entry_heads SET revision=?3 WHERE batch=?1 AND ordinal=?2");head.text(1,id.batch);head.integer(2,integer(id.ordinal));head.integer(3,snapshot.revision);head.done();
}
void append_financial(sqlite3* db,std::string_view operation,app::ChangeKind kind,const imports::Occurrence& id,std::int64_t prior,std::int64_t result,const Money& before,const Money& change,const Money& after,std::string_view undo,std::int64_t preview,std::int64_t catalog) {
    Statement row(db,categories_format(db)?"INSERT INTO ledger_operations(id,kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence,catalog_revision) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)":"INSERT INTO ledger_operations(id,kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence) VALUES(?,?,?,?,?,?,?,?,?,?,?)");row.text(1,operation);row.integer(2,static_cast<int>(kind));row.text(3,id.batch);row.integer(4,kind==app::ChangeKind::publication?-1:integer(id.ordinal));row.integer(5,prior);row.integer(6,result);row.integer(7,before.units());row.integer(8,change.units());row.integer(9,after.units());row.text(10,undo);row.integer(11,preview);if(categories_format(db))row.integer(12,kind==app::ChangeKind::publication?catalog_sequence(db):catalog);row.done();
}
std::int64_t financial_sequence(sqlite3* db){Statement s(db,"SELECT coalesce(max(sequence),0) FROM ledger_operations");if(!s.row())fail(SQLITE_CORRUPT);return s.integer(0);}
}
bool ImportStore::committed_changes_supported() const {return scalar(impl_->db,"PRAGMA user_version")=="4" || categories_supported();}
app::EntrySnapshot ImportStore::entry_snapshot(const imports::Occurrence& id,std::int64_t revision) const {
    Statement row(impl_->db,"SELECT active,date,payee,memo,total FROM entry_revisions WHERE batch=? AND ordinal=? AND revision=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,revision);if(!row.row())fail(SQLITE_NOTFOUND,"Committed revision does not exist");
    const auto original=load_row(id,false).source;std::vector<Allocation> allocations;
    Statement aa(impl_->db,"SELECT position,category,memo,units FROM entry_allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");aa.text(1,id.batch);aa.integer(2,integer(id.ordinal));aa.integer(3,revision);
    while(aa.row()){if(aa.integer(0)!=integer(allocations.size()) || allocations.size()>=qif::max_allocations)fail(SQLITE_CORRUPT);allocations.push_back({aa.bytes(1),aa.bytes(2),Money(aa.integer(3),account().currency)});}
    imports::DraftRow draft{row.bytes(1),row.bytes(2),row.bytes(3),Transaction::corrected_import(*original.original_amount,Money(row.integer(4),account().currency),std::move(allocations))};load_metadata(impl_->db,id,revision,draft,true);imports::validate_draft(original,draft);
    return {revision,row.integer(0)!=0,std::move(draft)};
}
std::vector<app::DependencyStamp> ImportStore::dependencies(const imports::Occurrence& id,std::int64_t before) const {
    std::vector<app::DependencyStamp> result;
    Statement refs(impl_->db,"SELECT DISTINCT v.batch FROM versions v JOIN history_order h ON h.batch=v.batch AND h.event_revision=v.revision WHERE v.duplicate_batch=? AND v.duplicate_ordinal=? AND (?=0 OR h.sequence<?) ORDER BY v.batch");refs.text(1,id.batch);refs.integer(2,integer(id.ordinal));refs.integer(3,before);refs.integer(4,before);
    while(refs.row()){
        const auto name=refs.bytes(0);if(before==0){const auto b=batch(impl_->db,name);result.push_back({name,b.revision,b.state});continue;}
        Statement event(impl_->db,"SELECT h.event_revision FROM history_order h WHERE h.batch=? AND h.sequence<? ORDER BY h.sequence DESC LIMIT 1");event.text(1,name);event.integer(2,before);if(!event.row())fail(SQLITE_CORRUPT);
        Statement state(impl_->db,"SELECT e.kind FROM events e JOIN history_order h ON h.batch=e.batch AND h.event_revision=e.revision WHERE e.batch=? AND h.sequence<? AND e.kind IN('capture','ingest','materialize','commit','discard') ORDER BY h.sequence DESC LIMIT 1");state.text(1,name);state.integer(2,before);if(!state.row())fail(SQLITE_CORRUPT);
        const auto kind=state.bytes(0);result.push_back({name,event.integer(0),kind=="capture"?app::BatchState::captured:kind=="commit"?app::BatchState::committed:kind=="discard"?app::BatchState::discarded:app::BatchState::pending});
    }return result;
}
app::CommittedEntry ImportStore::committed_entry(const imports::Occurrence& id) const {
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Committed changes require a new experimental dataset");
    Statement head(impl_->db,"SELECT h.revision,r.sequence FROM entry_heads h JOIN records r ON r.batch=h.batch AND r.ordinal=h.ordinal WHERE h.batch=? AND h.ordinal=?");head.text(1,id.batch);head.integer(2,integer(id.ordinal));if(!head.row())fail(SQLITE_CONSTRAINT,"Choose an included committed transaction");
    const auto revision=head.integer(0);return {id,head.integer(1),entry_snapshot(id,revision),load_row(id,false).source,{id,revision,dependencies(id)}};
}
std::optional<app::ChangeReceipt> ImportStore::committed_change_outcome(std::string_view operation) const {
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Committed changes require a new experimental dataset");
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,sequence FROM ledger_operations WHERE id=?");row.text(1,operation);if(!row.row()){if(categories_supported()){Statement other(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");other.text(1,operation);if(other.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");}return {};}
    if(row.integer(0)==0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to an import commit");
    imports::Occurrence id{row.bytes(1),static_cast<std::size_t>(row.integer(2))};const auto currency=account().currency;std::vector<app::DependencyStamp> deps;
    Statement dd(impl_->db,"SELECT batch,revision,state FROM change_dependencies WHERE operation=? ORDER BY batch");dd.text(1,operation);while(dd.row())deps.push_back({dd.bytes(0),dd.integer(1),static_cast<app::BatchState>(dd.integer(2))});
    return app::ChangeReceipt{std::string(operation),static_cast<app::ChangeKind>(row.integer(0)),id,entry_snapshot(id,row.integer(3)),entry_snapshot(id,row.integer(4)),Money(row.integer(5),currency),Money(row.integer(6),currency),Money(row.integer(7),currency),row.bytes(8),{id,row.integer(4),std::move(deps)},row.integer(9)};
}
app::ChangePreview ImportStore::preview_committed_change(const app::ChangeRequest& request) const {
    const auto current=committed_entry(request.identity);auto after=current.current;
    if(current.current.revision==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");after.revision++;
    if(request.kind==app::ChangeKind::correct){if(!current.current.active || !request.correction || !request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Invalid correction request");after.draft=canonical_draft(request.identity,*request.correction,current.current.draft,after.revision,true);imports::validate_draft(current.source,after.draft);}
    else if(request.kind==app::ChangeKind::remove){if(!current.current.active || request.correction || !request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Invalid removal request");after.active=false;}
    else if(request.kind==app::ChangeKind::undo){if(request.correction || request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Choose a committed change to undo");const auto original=committed_change_outcome(request.undo_of);if(!original || original->identity!=request.identity || original->result_token!=current.token)fail(SQLITE_CONSTRAINT,"Newer transaction or dependent review prevents Undo");after.draft=original->before_entry.draft;after.active=original->before_entry.active;}
    else fail(SQLITE_CONSTRAINT,"Import publication cannot be undone by this command");
    const auto currency=account().currency;const Money zero(0,currency);const auto delta=(after.active?after.draft.transaction.effective_total():zero)-(current.current.active?current.current.draft.transaction.effective_total():zero);
    const auto before=committed_balance();auto canonical=request;if(canonical.correction)canonical.correction=after.draft;return {canonical,current.current,std::move(after),before,delta,before+delta,current.token,financial_sequence(impl_->db),categories_supported()?catalog_sequence(impl_->db):0};
}
app::ChangeReceipt ImportStore::apply_committed_change(std::string_view operation,const app::ChangeRequest& request,const app::EntryToken& expected,std::int64_t preview_sequence) {
    return apply_committed_change_with_catalog(operation,request,expected,preview_sequence,0);
}
app::ChangeReceipt ImportStore::apply_committed_change_with_catalog(std::string_view operation,const app::ChangeRequest& request,const app::EntryToken& expected,std::int64_t preview_sequence,std::int64_t catalog_revision) {
    identifier(operation);
    if(const auto prior=committed_change_outcome(operation)){
        Statement stored(impl_->db,categories_supported()?"SELECT preview_sequence,catalog_revision FROM ledger_operations WHERE id=?":"SELECT preview_sequence,0 FROM ledger_operations WHERE id=?");stored.text(1,operation);if(!stored.row())fail(SQLITE_CORRUPT);
        auto prior_token=prior->result_token;prior_token.revision=prior->before_entry.revision;
        if(prior->kind!=request.kind || prior->identity!=request.identity || prior->undo_of!=request.undo_of || prior_token!=expected || stored.integer(0)!=preview_sequence || stored.integer(1)!=catalog_revision ||
           (request.kind==app::ChangeKind::correct?(!request.correction || !same_draft(canonical_draft(request.identity,*request.correction,prior->before_entry.draft,prior->after_entry.revision,true,false),prior->after_entry.draft)):request.correction.has_value()))fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        return *prior;
    }
    // Preview before BEGIN so durable lookup used by Undo cannot inspect an open
    // write transaction. Exclusive document ownership serializes all commands.
    const auto preview=preview_committed_change(request);
    if(preview.expected!=expected || preview.preview_sequence!=preview_sequence || preview.catalog_revision!=catalog_revision)fail(SQLITE_CONSTRAINT,"Preview changed; refresh before applying");
    Atomic transaction(impl_->db);
    if(committed_entry(request.identity).token!=expected || financial_sequence(impl_->db)!=preview_sequence || (categories_supported()?catalog_sequence(impl_->db):0)!=catalog_revision)fail(SQLITE_CONSTRAINT,"Preview changed; refresh before applying");
    store_entry(impl_->db,request.identity,preview.after_entry);
    append_financial(impl_->db,operation,request.kind,request.identity,preview.before_entry.revision,preview.after_entry.revision,preview.before,preview.change,preview.after,request.undo_of,preview_sequence,catalog_revision);
    for(const auto& dependency:expected.dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,operation);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
    impl_->finish(transaction);return *committed_change_outcome(operation);
}
std::vector<app::CommittedHistoryItem> ImportStore::committed_history(const imports::Occurrence& id) const {
    const auto current=committed_entry(id);std::vector<app::CommittedHistoryItem> result;
    Statement rows(impl_->db,"SELECT id,kind,prior_revision,result_revision,before,change,after,sequence FROM ledger_operations WHERE batch=? AND (kind=0 OR ordinal=?) ORDER BY sequence");rows.text(1,id.batch);rows.integer(2,integer(id.ordinal));const auto currency=account().currency;
    while(rows.row()){const auto kind=static_cast<app::ChangeKind>(rows.integer(1));std::optional<app::EntrySnapshot> before;bool eligible=false;std::string reason="Import publication is not an undoable correction";
        if(kind!=app::ChangeKind::publication){before=entry_snapshot(id,rows.integer(2));const auto receipt=committed_change_outcome(rows.bytes(0));eligible=receipt->result_token==current.token;reason=eligible?"":"Newer transaction or dependent review prevents Undo";}
        result.push_back({rows.bytes(0),kind,std::move(before),entry_snapshot(id,kind==app::ChangeKind::publication?0:rows.integer(3)),Money(rows.integer(4),currency),Money(rows.integer(5),currency),Money(rows.integer(6),currency),eligible,reason,rows.integer(7)});
    }return result;
}
std::vector<app::CommittedChangeSummary> ImportStore::committed_changes(std::int64_t before,std::size_t limit) const {
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Committed changes require a new experimental dataset");if(before<0 || limit==0 || limit>100)fail(SQLITE_CONSTRAINT,"History page limit is invalid");
    Statement rows(impl_->db,"SELECT o.id,o.kind,o.batch,o.ordinal,o.sequence,e.active,e.date,e.payee,e.total FROM ledger_operations o JOIN entry_heads h ON h.batch=o.batch AND h.ordinal=o.ordinal JOIN entry_revisions e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE o.kind!=0 AND (?=0 OR o.sequence<?) ORDER BY o.sequence DESC LIMIT ?");rows.integer(1,before);rows.integer(2,before);rows.integer(3,integer(limit));std::vector<app::CommittedChangeSummary> result;const auto currency=account().currency;
    while(rows.row())result.push_back({rows.bytes(0),static_cast<app::ChangeKind>(rows.integer(1)),{rows.bytes(2),static_cast<std::size_t>(rows.integer(3))},rows.integer(4),rows.integer(5)!=0,rows.bytes(6),rows.bytes(7),Money(rows.integer(8),currency)});return result;
}
void ImportStore::verify_committed(const app::OperationContext* context) const {
    if(context)context->checkpoint("Verifying committed change history");
    Statement order_check(impl_->db,"SELECT 1 FROM events e WHERE NOT EXISTS(SELECT 1 FROM history_order h WHERE h.batch=e.batch AND h.event_revision=e.revision) UNION ALL SELECT 1 FROM ledger_operations o WHERE NOT EXISTS(SELECT 1 FROM history_order h WHERE h.operation=o.id)");if(order_check.row())fail(SQLITE_CORRUPT,"Missing history ordering");
    Statement common_order(impl_->db,"SELECT count(*),coalesce(min(sequence),1),coalesce(max(sequence),0) FROM history_order");if(!common_order.row() || common_order.integer(1)!=1 || common_order.integer(0)!=common_order.integer(2))fail(SQLITE_CORRUPT);
    Statement batch_order(impl_->db,"SELECT 1 FROM history_order h JOIN history_order prior ON prior.batch=h.batch AND prior.event_revision=h.event_revision-1 WHERE h.sequence<=prior.sequence");if(batch_order.row())fail(SQLITE_CORRUPT);
    Statement event_order(impl_->db,"SELECT 1 FROM events e JOIN events prior ON prior.sequence=e.sequence-1 JOIN history_order h ON h.batch=e.batch AND h.event_revision=e.revision JOIN history_order p ON p.batch=prior.batch AND p.event_revision=prior.revision WHERE h.sequence<=p.sequence");if(event_order.row())fail(SQLITE_CORRUPT);
    const auto currency=account().currency;
    // Each exclusion records the target state at its original review event.
    // A later Save carries that decision; it never silently refreshes the stamp.
    Statement review_count(impl_->db,"SELECT (SELECT count(*) FROM versions WHERE decision=2),(SELECT count(*) FROM review_stamps)");if(!review_count.row() || review_count.integer(0)!=review_count.integer(1))fail(SQLITE_CORRUPT);
    Statement reviews(impl_->db,"SELECT s.batch,s.ordinal,s.revision,s.target_revision,s.target_active,s.review_revision,v.duplicate_batch,v.duplicate_ordinal,h.sequence,e.kind FROM review_stamps s JOIN versions v USING(batch,ordinal,revision) JOIN history_order h ON h.batch=s.batch AND h.event_revision=s.review_revision JOIN events e ON e.batch=s.batch AND e.revision=s.revision WHERE v.decision=2");
    while(reviews.row()) {
        if(context)context->checkpoint();
        const auto batch_id=reviews.bytes(0);const auto ordinal=reviews.integer(1),revision=reviews.integer(2),origin=reviews.integer(5);imports::Occurrence target{reviews.bytes(6),static_cast<std::size_t>(reviews.integer(7))};
        if(origin>revision)fail(SQLITE_CORRUPT);
        Statement original(impl_->db,"SELECT v.duplicate_batch,v.duplicate_ordinal,e.kind,s.target_revision,s.target_active,s.review_revision FROM versions v JOIN events e ON e.batch=v.batch AND e.revision=v.revision JOIN review_stamps s USING(batch,ordinal,revision) WHERE v.batch=? AND v.ordinal=? AND v.revision=? AND v.decision=2");original.text(1,batch_id);original.integer(2,ordinal);original.integer(3,origin);
        if(!original.row() || original.bytes(0)!=target.batch || original.integer(1)!=integer(target.ordinal) || (original.bytes(2)!="ingest" && original.bytes(2)!="materialize" && original.bytes(2)!="decide") || original.integer(3)!=reviews.integer(3) || original.integer(4)!=reviews.integer(4) || original.integer(5)!=origin)fail(SQLITE_CORRUPT);
        if(origin!=revision){if(reviews.bytes(9)!="save")fail(SQLITE_CORRUPT);Statement previous(impl_->db,"SELECT s.target_revision,s.target_active,s.review_revision,v.duplicate_batch,v.duplicate_ordinal FROM versions v LEFT JOIN review_stamps s USING(batch,ordinal,revision) WHERE v.batch=? AND v.ordinal=? AND v.revision<? ORDER BY v.revision DESC LIMIT 1");previous.text(1,batch_id);previous.integer(2,ordinal);previous.integer(3,revision);if(!previous.row() || previous.null(0) || previous.integer(0)!=reviews.integer(3) || previous.integer(1)!=reviews.integer(4) || previous.integer(2)!=origin || previous.bytes(3)!=target.batch || previous.integer(4)!=integer(target.ordinal))fail(SQLITE_CORRUPT);}
        Statement then(impl_->db,"SELECT o.result_revision FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.batch=? AND (o.kind=0 OR o.ordinal=?) AND h.sequence<? ORDER BY h.sequence DESC LIMIT 1");then.text(1,target.batch);then.integer(2,integer(target.ordinal));then.integer(3,reviews.integer(8));if(!then.row() || then.integer(0)!=reviews.integer(3) || entry_snapshot(target,then.integer(0)).active!=(reviews.integer(4)!=0))fail(SQLITE_CORRUPT,"Invalid review target history");
    }
    std::map<imports::Occurrence,std::int64_t> heads;Money balance=account().opening;std::int64_t prior_sequence=0,prior_order=0;std::size_t snapshot_count=0;
    Statement operations(impl_->db,"SELECT o.id,o.kind,o.batch,o.ordinal,o.prior_revision,o.result_revision,o.before,o.change,o.after,o.undo_of,o.preview_sequence,o.sequence,h.sequence FROM ledger_operations o JOIN history_order h ON h.operation=o.id ORDER BY o.sequence");
    while(operations.row()) {
        if(context)context->checkpoint();
        const auto operation=operations.bytes(0);const auto kind=static_cast<app::ChangeKind>(operations.integer(1));const auto sequence=operations.integer(11),order=operations.integer(12);
        if(categories_supported()){Statement epoch(impl_->db,"SELECT o.catalog_revision,(SELECT coalesce(max(c.catalog_after),0) FROM category_operations c JOIN history_order h ON h.category_operation=c.id WHERE h.sequence<?) FROM ledger_operations o WHERE o.id=?");epoch.integer(1,order);epoch.text(2,operation);if(!epoch.row() || epoch.integer(0)!=epoch.integer(1))fail(SQLITE_CORRUPT,"Financial catalog preview does not match history");}
        if(order<=prior_order || sequence!=prior_sequence+1 || Money(operations.integer(6),currency)!=balance || balance+Money(operations.integer(7),currency)!=Money(operations.integer(8),currency))fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");
        if(kind==app::ChangeKind::publication){
            const auto receipt=outcome(operation);if(!receipt || receipt->batch!=operations.bytes(2) || receipt->before!=balance || receipt->change.units()!=operations.integer(7) || receipt->after.units()!=operations.integer(8) || operations.integer(3)!=-1 || operations.integer(4)!=-1 || operations.integer(5)!=0 || !operations.bytes(9).empty() || operations.integer(10)!=0)fail(SQLITE_CORRUPT);
            Statement event(impl_->db,"SELECT h.sequence FROM history_order h JOIN events e ON e.batch=h.batch AND e.revision=h.event_revision WHERE e.batch=? AND e.kind='commit' AND e.revision=?");event.text(1,receipt->batch);event.integer(2,receipt->revision+1);if(!event.row() || event.integer(0)+1!=order)fail(SQLITE_CORRUPT);
            const auto b=batch(impl_->db,receipt->batch);std::vector<Money> values;std::size_t included=0,excluded=0;
            for(std::size_t ordinal=0;ordinal<b.count;++ordinal){if(context)context->checkpoint();imports::Occurrence identity{b.id,ordinal};const auto row=load_row(identity,false);
                if(row.decision.disposition==imports::Disposition::include){const auto initial=entry_snapshot(identity,0);if(!initial.active || !same_draft(initial.draft,*row.draft) || heads.contains(identity))fail(SQLITE_CORRUPT);heads.emplace(identity,0);++snapshot_count;++included;values.push_back(initial.draft.transaction.effective_total());}
                else if(row.decision.disposition==imports::Disposition::duplicate){++excluded;const auto target=*row.decision.duplicate_of;const auto head=heads.find(target);if(head==heads.end())fail(SQLITE_CORRUPT);Statement stamp(impl_->db,"SELECT target_revision,target_active FROM review_stamps WHERE batch=? AND ordinal=? ORDER BY revision DESC LIMIT 1");stamp.text(1,b.id);stamp.integer(2,integer(ordinal));if(!stamp.row() || stamp.integer(0)!=head->second || (stamp.integer(1)!=0)!=entry_snapshot(target,head->second).active)fail(SQLITE_CORRUPT,"Stale exclusion was committed");}
                else fail(SQLITE_CORRUPT);
            }
            if(included!=receipt->included || excluded!=receipt->excluded || imports::sum(currency,values)!=receipt->change)fail(SQLITE_CORRUPT);
        } else {
            const auto receipt=committed_change_outcome(operation);if(!receipt)fail(SQLITE_CORRUPT);const auto head=heads.find(receipt->identity);
            if(head==heads.end() || receipt->before_entry.revision!=head->second || receipt->after_entry.revision!=head->second+1 || operations.integer(10)!=prior_sequence || receipt->result_token.dependencies!=dependencies(receipt->identity,order))fail(SQLITE_CORRUPT,"Invalid committed change dependencies");
            const auto& before=receipt->before_entry;const auto& after=receipt->after_entry;
            const Money zero(0,currency);const auto delta=(after.active?after.draft.transaction.effective_total():zero)-(before.active?before.draft.transaction.effective_total():zero);if(delta!=receipt->change)fail(SQLITE_CORRUPT);
            if(kind==app::ChangeKind::correct){if(!before.active || !after.active || !receipt->undo_of.empty())fail(SQLITE_CORRUPT);}
            else if(kind==app::ChangeKind::remove){if(!before.active || after.active || !same_draft(before.draft,after.draft) || !receipt->undo_of.empty())fail(SQLITE_CORRUPT);}
            else if(kind==app::ChangeKind::undo){const auto original=committed_change_outcome(receipt->undo_of);if(!original || original->sequence>=sequence || original->identity!=receipt->identity || original->after_entry.revision!=before.revision || original->result_token.dependencies!=receipt->result_token.dependencies || original->before_entry.active!=after.active || !same_draft(original->before_entry.draft,after.draft))fail(SQLITE_CORRUPT,"Invalid strict Undo history");}
            else fail(SQLITE_CORRUPT);
            head->second=after.revision;++snapshot_count;
        }
        balance=Money(operations.integer(8),currency);prior_sequence=sequence;prior_order=order;
    }
    Statement counts(impl_->db,"SELECT (SELECT count(*) FROM entry_heads),(SELECT count(*) FROM entry_revisions),(SELECT count(*) FROM operations),(SELECT count(*) FROM ledger_operations WHERE kind=0)");if(!counts.row() || counts.integer(0)!=integer(heads.size()) || counts.integer(1)!=integer(snapshot_count) || counts.integer(2)!=counts.integer(3))fail(SQLITE_CORRUPT);
    for(const auto& [identity,revision]:heads){if(context)context->checkpoint();Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,identity.batch);head.integer(2,integer(identity.ordinal));if(!head.row() || head.integer(0)!=revision)fail(SQLITE_CORRUPT);}
    Statement extra_dependencies(impl_->db,"SELECT 1 FROM change_dependencies d JOIN ledger_operations o ON o.id=d.operation WHERE o.kind=0");if(extra_dependencies.row() || balance!=committed_balance())fail(SQLITE_CORRUPT,"Current heads do not reconcile");
}
