namespace {
#include "audit_snapshot.inc"

bool same_draft(const imports::DraftRow& a,const imports::DraftRow& b) {
    if(a.remainder_allocation!=b.remainder_allocation || a.manual_split_amounts!=b.manual_split_amounts || a.payee_owner!=b.payee_owner || a.memo_owner!=b.memo_owner || a.payee_rule!=b.payee_rule || a.memo_rule!=b.memo_rule || a.date!=b.date || a.payee!=b.payee || a.memo!=b.memo || a.transaction.effective_total()!=b.transaction.effective_total() || a.transaction.original_imported_amount()!=b.transaction.original_imported_amount())return false;
    const auto& aa=a.transaction.allocations();const auto& bb=b.transaction.allocations();if(aa.size()!=bb.size())return false;
    for(std::size_t i=0;i<aa.size();++i)if(aa[i].category_binding!=bb[i].category_binding || aa[i].identity!=bb[i].identity || aa[i].source_ordinal!=bb[i].source_ordinal || aa[i].category_owner!=bb[i].category_owner || aa[i].memo_owner!=bb[i].memo_owner || aa[i].category_rule!=bb[i].category_rule || aa[i].memo_rule!=bb[i].memo_rule || aa[i].category!=bb[i].category || aa[i].memo!=bb[i].memo || aa[i].amount!=bb[i].amount)return false;
    return true;
}
#ifdef SPENTIT_RULES_STORAGE
void check_rule_transition(const imports::DraftRow& before,const imports::DraftRow& after,bool apply,bool replace_manual) {
    if(apply && (before.remainder_allocation!=after.remainder_allocation || before.manual_split_amounts!=after.manual_split_amounts))fail(SQLITE_CORRUPT,"Rule changed split amount editing mode");
    if(apply && replace_manual)fail(SQLITE_CORRUPT,"Rule application cannot replace manual fields");
    if(apply) {
        const auto& allocations=before.transaction.allocations();
        const bool categorized=std::all_of(allocations.begin(),allocations.end(),[](const auto& a){return !a.category.empty() || a.category_binding.has_value();});
        if((categorized || allocations.size()>1) && (before.payee!=after.payee || before.memo!=after.memo || before.payee_owner!=after.payee_owner || before.memo_owner!=after.memo_owner || before.payee_rule!=after.payee_rule || before.memo_rule!=after.memo_rule))fail(SQLITE_CORRUPT,"Rule changed protected transaction text");
    }
    auto mark_ok=[&](FieldOwnership owner,const std::optional<RuleMark>& mark){if(owner!=FieldOwnership::source && owner!=FieldOwnership::manual && owner!=FieldOwnership::rule)fail(SQLITE_CORRUPT);if((owner==FieldOwnership::rule)!=mark.has_value())fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");};
    auto field=[&](FieldOwnership before_owner,const std::optional<RuleMark>& before_mark,const std::string& before_text,const std::optional<CategoryBinding>& before_binding,FieldOwnership after_owner,const std::optional<RuleMark>& after_mark,const std::string& after_text,const std::optional<CategoryBinding>& after_binding,bool category){
        mark_ok(before_owner,before_mark);mark_ok(after_owner,after_mark);
        const bool same_value=before_text==after_text && (!category || before_binding==after_binding);
        if(!apply){
            if(before_owner==FieldOwnership::rule){if(after_owner==FieldOwnership::rule){if(!same_value || before_mark!=after_mark)fail(SQLITE_CORRUPT);}else if(after_owner==FieldOwnership::manual || (!category && after_owner==FieldOwnership::source)){if(after_mark)fail(SQLITE_CORRUPT);}else fail(SQLITE_CORRUPT);}
            else if(after_owner==FieldOwnership::rule)fail(SQLITE_CORRUPT);
            else if(before_owner==FieldOwnership::manual && after_owner!=FieldOwnership::manual && (category || after_owner!=FieldOwnership::source))fail(SQLITE_CORRUPT,"Manual ownership was silently removed");
        }else{
            if(category && (!before_text.empty() || before_binding) && (!same_value || before_owner!=after_owner || before_mark!=after_mark))fail(SQLITE_CORRUPT,"Rule changed an assigned category");
            if(before_owner==FieldOwnership::manual && after_owner==FieldOwnership::rule){if(!replace_manual && !(category && before_text.empty() && !before_binding))fail(SQLITE_CORRUPT);}
            else if(before_owner==FieldOwnership::manual && after_owner!=FieldOwnership::manual)fail(SQLITE_CORRUPT,"Manual ownership was silently removed");
            else if(before_owner==FieldOwnership::rule && after_owner==FieldOwnership::manual)fail(SQLITE_CORRUPT);
            else if(before_owner==FieldOwnership::source && after_owner==FieldOwnership::manual)fail(SQLITE_CORRUPT);
            if((before_owner==after_owner && (before_owner==FieldOwnership::source || before_owner==FieldOwnership::manual) && !same_value) || (after_owner==FieldOwnership::rule && !after_mark) || (before_owner==FieldOwnership::rule && after_owner==FieldOwnership::source && after_mark))fail(SQLITE_CORRUPT);
        }
    };
    field(before.payee_owner,before.payee_rule,before.payee,{},after.payee_owner,after.payee_rule,after.payee,{},false);
    field(before.memo_owner,before.memo_rule,before.memo,{},after.memo_owner,after.memo_rule,after.memo,{},false);
    const auto& left=before.transaction.allocations();const auto& right=after.transaction.allocations();
    if(apply){
        if(before.date!=after.date || before.transaction.effective_total()!=after.transaction.effective_total() || before.transaction.original_imported_amount()!=after.transaction.original_imported_amount() || left.size()!=right.size())fail(SQLITE_CORRUPT);
        for(std::size_t i=0;i<left.size();++i){if(left[i].identity!=right[i].identity || left[i].source_ordinal!=right[i].source_ordinal || left[i].amount!=right[i].amount)fail(SQLITE_CORRUPT);field(left[i].memo_owner,left[i].memo_rule,left[i].memo,{},right[i].memo_owner,right[i].memo_rule,right[i].memo,{},false);field(left[i].category_owner,left[i].category_rule,left[i].category,left[i].category_binding,right[i].category_owner,right[i].category_rule,right[i].category,right[i].category_binding,true);}
        return;
    }
    std::map<std::string,const Allocation*> previous;
    for(const auto& allocation:left)if(!previous.emplace(allocation.identity,&allocation).second)fail(SQLITE_CORRUPT);
    for(const auto& allocation:right){auto found=previous.find(allocation.identity);if(found==previous.end()){if(allocation.category_owner==FieldOwnership::rule || allocation.memo_owner==FieldOwnership::rule || allocation.category_rule || allocation.memo_rule)fail(SQLITE_CORRUPT);continue;}field(found->second->memo_owner,found->second->memo_rule,found->second->memo,{},allocation.memo_owner,allocation.memo_rule,allocation.memo,{},false);field(found->second->category_owner,found->second->category_rule,found->second->category,found->second->category_binding,allocation.category_owner,allocation.category_rule,allocation.category,allocation.category_binding,true);}
}
#endif
void store_entry(sqlite3* db,const imports::Occurrence& id,const app::EntrySnapshot& snapshot,std::optional<std::int64_t> ruleset) {
    if(snapshot.revision==0) {
        Statement row(db,"INSERT INTO entry_revisions(batch,ordinal,revision,active,draft_revision) SELECT ?1,?2,0,1,max(revision) FROM versions WHERE batch=?1 AND ordinal=?2");
        row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.done();
    } else {
        Statement row(db,"INSERT INTO entry_revisions(batch,ordinal,revision,active,date,payee,memo,total) VALUES(?,?,?,?,?,?,?,?)");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,snapshot.revision);row.integer(4,snapshot.active?1:0);row.text(5,snapshot.draft.date);row.text(6,snapshot.draft.payee);row.text(7,snapshot.draft.memo);row.integer(8,snapshot.draft.transaction.effective_total().units());row.done();
        std::int64_t position=0;for(const auto& a:snapshot.draft.transaction.allocations()){Statement allocation(db,"INSERT INTO entry_allocations VALUES(?,?,?,?,?,?,?)");allocation.text(1,id.batch);allocation.integer(2,integer(id.ordinal));allocation.integer(3,snapshot.revision);allocation.integer(4,position++);allocation.text(5,a.category);allocation.text(6,a.memo);allocation.integer(7,a.amount.units());allocation.done();}
    }
    save_metadata(db,id,snapshot.revision,snapshot.draft,true,ruleset);
    Statement head(db,snapshot.revision==0?"INSERT INTO entry_heads VALUES(?,?,?)":"UPDATE entry_heads SET revision=?3 WHERE batch=?1 AND ordinal=?2");head.text(1,id.batch);head.integer(2,integer(id.ordinal));head.integer(3,snapshot.revision);head.done();
}
void append_financial(sqlite3* db,std::string_view operation,app::ChangeKind kind,const imports::Occurrence& id,std::int64_t prior,std::int64_t result,const Money& before,const Money& change,const Money& after,std::string_view undo,std::int64_t preview,std::int64_t catalog) {
    Statement row(db,categories_format(db)?"INSERT INTO ledger_operations(id,kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence,catalog_revision) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)":"INSERT INTO ledger_operations(id,kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,preview_sequence) VALUES(?,?,?,?,?,?,?,?,?,?,?)");row.text(1,operation);row.integer(2,static_cast<int>(kind));row.text(3,id.batch);row.integer(4,kind==app::ChangeKind::publication?-1:integer(id.ordinal));row.integer(5,prior);row.integer(6,result);row.integer(7,before.units());row.integer(8,change.units());row.integer(9,after.units());row.text(10,undo);row.integer(11,preview);if(categories_format(db))row.integer(12,kind==app::ChangeKind::publication?catalog_sequence(db):catalog);row.done();
}
std::int64_t financial_sequence(sqlite3* db){Statement s(db,"SELECT coalesce(max(sequence),0) FROM ledger_operations");if(!s.row())fail(SQLITE_CORRUPT);return s.integer(0);}
}
bool ImportStore::committed_changes_supported() const {return scalar(impl_->db,"PRAGMA user_version")=="4" || categories_supported();}
app::EntrySnapshot ImportStore::entry_snapshot(const imports::Occurrence& id,std::int64_t revision,const qif::Record* known_source) const {
    // Only a single read-only verification owns this bounded cache. History
    // reconciliation still checks every reference, ordering and balance below.
    const auto key=std::make_pair(id,revision);
    auto* cache=impl_->audit_entries;
    if(cache){const auto found=cache->values.find(key);if(found!=cache->values.end()){cache->touch(key);return found->second.snapshot;}}
    const auto original=known_source?*known_source:load_row(id,false).source;
    const auto currency=entry_currency(impl_->db,id.batch,integer(id.ordinal),revision);
    const auto result=[&]{
        if(cache && categories_supported())return audit_snapshot(impl_->db,id,revision,original,currency,ScopedStatements::current(impl_->db)->revision_context);
        Statement row(impl_->db,"SELECT active,date,payee,memo,total FROM entry_values WHERE batch=? AND ordinal=? AND revision=?");row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,revision);if(!row.row())fail(SQLITE_NOTFOUND,"Committed revision does not exist");
        const bool reassigned=original.source_currency && *original.source_currency!=currency;std::vector<Allocation> allocations;
        Statement aa(impl_->db,"SELECT position,category,memo,units FROM entry_allocation_values WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");aa.text(1,id.batch);aa.integer(2,integer(id.ordinal));aa.integer(3,revision);
        while(aa.row()){if(aa.integer(0)!=integer(allocations.size()) || allocations.size()>=qif::max_allocations)fail(SQLITE_CORRUPT);allocations.push_back({aa.bytes(1),aa.bytes(2),Money(aa.integer(3),currency)});}
        imports::DraftRow draft{row.bytes(1),row.bytes(2),row.bytes(3),Transaction::repaired_import(original.original_amount,Money(row.integer(4),currency),std::move(allocations),reassigned)};load_metadata(impl_->db,id,revision,draft,true);imports::validate_draft(original,draft,reassigned);
        return app::EntrySnapshot{revision,row.integer(0)!=0,std::move(draft)};
    }();
    if(cache) {
        const auto& d=result.draft;
        const auto size=sizeof(result)+sizeof(key)+192+VerificationRows::text_size(id.batch)+
            VerificationRows::text_size(d.date)+VerificationRows::text_size(d.payee)+VerificationRows::text_size(d.memo)+
            VerificationRows::transaction_size(d.transaction)+
            (d.payee_rule?VerificationRows::text_size(d.payee_rule->identity):0)+
            (d.memo_rule?VerificationRows::text_size(d.memo_rule->identity):0);
        cache->remember(key,result,size);
    }
    return result;
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
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Editing committed transactions is not supported by this dataset");
    Statement head(impl_->db,"SELECT h.revision,r.sequence FROM entry_heads h JOIN records r ON r.batch=h.batch AND r.ordinal=h.ordinal WHERE h.batch=? AND h.ordinal=?");head.text(1,id.batch);head.integer(2,integer(id.ordinal));if(!head.row())fail(SQLITE_CONSTRAINT,"Select an accepted transaction that has not been deleted.");
    const auto revision=head.integer(0);auto source=load_row(id,false).source;auto snapshot=entry_snapshot(id,revision,&source);return {id,head.integer(1),std::move(snapshot),std::move(source),{id,revision,dependencies(id)}};
}
std::optional<app::ChangeReceipt> ImportStore::committed_change_outcome(std::string_view operation) const {
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Editing committed transactions is not supported by this dataset");
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT kind,batch,ordinal,prior_revision,result_revision,before,change,after,undo_of,sequence FROM ledger_operations WHERE id=?");row.text(1,operation);if(!row.row()){if(categories_supported()){Statement other(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");other.text(1,operation);if(other.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");}return {};}
    if(row.integer(0)==5)fail(SQLITE_CONSTRAINT,"Operation identity belongs to an account command");
    if(row.integer(0)==0)fail(SQLITE_CONSTRAINT,"Operation identity belongs to an import commit");
    imports::Occurrence id{row.bytes(1),static_cast<std::size_t>(row.integer(2))};require_account(id.batch);Statement history(impl_->db,"SELECT sequence FROM history_order WHERE operation=?");history.text(1,operation);if(!history.row())fail(SQLITE_CORRUPT);const auto currency=currency_as_of(impl_->db,account_for_batch(id.batch).identity,history.integer(0));std::vector<app::DependencyStamp> deps;
    Statement dd(impl_->db,"SELECT batch,revision,state FROM change_dependencies WHERE operation=? ORDER BY batch");dd.text(1,operation);while(dd.row())deps.push_back({dd.bytes(0),dd.integer(1),static_cast<app::BatchState>(dd.integer(2))});
    return app::ChangeReceipt{std::string(operation),static_cast<app::ChangeKind>(row.integer(0)),id,entry_snapshot(id,row.integer(3)),entry_snapshot(id,row.integer(4)),Money(row.integer(5),currency),Money(row.integer(6),currency),Money(row.integer(7),currency),row.bytes(8),{id,row.integer(4),std::move(deps)},row.integer(9)};
}
app::ChangePreview ImportStore::preview_change(const app::ChangeRequest& request,bool links,std::optional<Money> known_balance) const {
    if(request.kind==app::ChangeKind::undo){Statement rules(impl_->db,"SELECT 1 FROM review_members m JOIN review_answers a ON a.id=m.answer WHERE m.child=? AND a.question='rules-account'");rules.text(1,request.undo_of);if(rules.row())fail(SQLITE_CONSTRAINT,"Use Undo rule application on the Rules screen to undo the whole operation.");}
    if(request.kind==app::ChangeKind::undo){Statement group(impl_->db,"SELECT 1 FROM review_members m JOIN review_answers a ON a.id=m.answer WHERE m.child=?1 AND a.question IN('ledger-edit-batch','ledger-delete') UNION ALL SELECT 1 FROM review_undo_members m JOIN review_undos u ON u.id=m.undo_id JOIN review_answers a ON a.id=u.answer WHERE m.child=?1 AND a.question IN('ledger-edit-batch','ledger-delete')");group.text(1,request.undo_of);if(group.row())fail(SQLITE_CONSTRAINT,"This change belongs to a selection edit and cannot be undone individually.");}
    if(request.kind==app::ChangeKind::undo){Statement member(impl_->db,"SELECT 1 FROM review_members m JOIN review_answers a ON a.id=m.answer WHERE m.child=?1 AND a.question!='ledger-edit' UNION ALL SELECT 1 FROM review_undo_members m JOIN review_undos u ON u.id=m.undo_id JOIN review_answers a ON a.id=u.answer WHERE m.child=?1 AND a.question!='ledger-edit'");member.text(1,request.undo_of);if(member.row())fail(SQLITE_CONSTRAINT,"Use Review Undo to reverse this entire decision");}
    if(!links && transfer_linked(request.identity))fail(SQLITE_CONSTRAINT,"Transfer-linked rows are corrected one at a time.");
    const auto current=committed_entry(request.identity);auto after=current.current;
    if(current.current.revision==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
    after.revision++;
    if(request.kind==app::ChangeKind::correct){if(!current.current.active || !request.correction || !request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Invalid correction request");after.draft=canonical_draft(request.identity,*request.correction,current.current.draft,after.revision,true);const bool reassigned=current.source.source_currency && after.draft.transaction.effective_total().currency()!=*current.source.source_currency;imports::validate_draft(current.source,after.draft,reassigned);}
    else if(request.kind==app::ChangeKind::remove){if(!current.current.active || request.correction || !request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Invalid removal request");after.active=false;}
    else if(request.kind==app::ChangeKind::undo){if(request.correction || request.undo_of.empty())fail(SQLITE_CONSTRAINT,"Choose a committed change to undo");const auto original=committed_change_outcome(request.undo_of);if(!original || original->identity!=request.identity || original->result_token!=current.token)fail(SQLITE_CONSTRAINT,"Newer transaction or dependent review prevents Undo");if(later_relationship_blocks(request.identity,request.undo_of))fail(SQLITE_CONSTRAINT,"Newer transaction, transfer relationship, or source association prevents Undo");after.draft=original->before_entry.draft;after.active=original->before_entry.active;}
    else fail(SQLITE_CONSTRAINT,"Import publication cannot be undone by this command");
    std::string relationship,link_stamp;
    if(links){
        try{
            const auto plan=request.kind==app::ChangeKind::undo?plan_restore(request.undo_of):plan_entry_links(all_transfers(),request.identity,current.current.draft,after.draft,request.kind==app::ChangeKind::remove);
            relationship=plan.relationship;link_stamp=plan.stamp;
        }catch(const transfers::TransferError& error){fail(SQLITE_CONSTRAINT,error.what());}
    }
    const auto currency=account().currency;const Money zero(0,currency);const auto delta=(after.active?after.draft.transaction.effective_total():zero)-(current.current.active?current.current.draft.transaction.effective_total():zero);
    const auto before=known_balance?*known_balance:committed_balance();auto canonical=request;if(canonical.correction)canonical.correction=after.draft;return {canonical,current.current,std::move(after),before,delta,before+delta,current.token,financial_sequence(impl_->db),categories_supported()?catalog_sequence(impl_->db):0,std::move(relationship),std::move(link_stamp)};
}
app::ChangePreview ImportStore::preview_committed_change(const app::ChangeRequest& request) const {
    return preview_change(request,true);
}
app::ChangeReceipt ImportStore::apply_committed_change(std::string_view operation,const app::ChangeRequest& request,const app::EntryToken& expected,std::int64_t preview_sequence) {
    return apply_committed_change_with_catalog(operation,request,expected,preview_sequence,0);
}
app::ChangeReceipt ImportStore::apply_committed_change_with_catalog(std::string_view operation,const app::ChangeRequest& request,const app::EntryToken& expected,std::int64_t preview_sequence,std::int64_t catalog_revision) {
    identifier(operation);
    if(bulk_supported()){Statement member(impl_->db,"SELECT 1 FROM bulk_members WHERE entry_operation=?");member.text(1,operation);if(member.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to a bulk command");}
#ifdef SPENTIT_RULES_STORAGE
    if(scalar(impl_->db,"PRAGMA user_version")=="52"){
        Statement member(impl_->db,"SELECT 1 FROM rule_reapplication_members WHERE entry_operation=?");member.text(1,operation);if(member.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to a rule reapply");
        if(request.kind==app::ChangeKind::undo){Statement target(impl_->db,"SELECT 1 FROM rule_reapplication_members WHERE entry_operation=?");target.text(1,request.undo_of);if(target.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to a rule reapply");}
    }
#endif
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
    std::optional<std::int64_t> restored_ruleset;
#ifdef SPENTIT_RULES_STORAGE
    // A clearing correction has no provenance row, so Undo must name the snapshot it restores.
    if(rules_format(impl_->db) && request.kind==app::ChangeKind::undo){
        const auto& draft=preview.after_entry.draft;bool any=draft.payee_rule.has_value() || draft.memo_rule.has_value();
        if(!any)for(const auto& allocation:draft.transaction.allocations())if(allocation.category_rule || allocation.memo_rule)any=true;
        if(any){const auto original=committed_change_outcome(request.undo_of);Statement mark(impl_->db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");mark.text(1,request.identity.batch);mark.integer(2,integer(request.identity.ordinal));mark.integer(3,original->before_entry.revision);if(!mark.row())fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");restored_ruleset=mark.integer(0);}
    }
#endif
    Atomic transaction(impl_->db);
    if(committed_entry(request.identity).token!=expected || financial_sequence(impl_->db)!=preview_sequence || (categories_supported()?catalog_sequence(impl_->db):0)!=catalog_revision)fail(SQLITE_CONSTRAINT,"Preview changed; refresh before applying");
    EntryLinkPlan fresh;
    try{
        if(request.kind==app::ChangeKind::undo){
            if(later_relationship_blocks(request.identity,request.undo_of))fail(SQLITE_CONSTRAINT,"Preview changed; refresh before applying");
            fresh=plan_restore(request.undo_of);
        }else fresh=plan_entry_links(all_transfers(),request.identity,preview.before_entry.draft,preview.after_entry.draft,request.kind==app::ChangeKind::remove);
    }catch(const transfers::TransferError& error){fail(SQLITE_CONSTRAINT,error.what());}
    if(fresh.stamp!=preview.link_stamp)fail(SQLITE_CONSTRAINT,"Preview changed; refresh before applying");
    store_entry(impl_->db,request.identity,preview.after_entry,restored_ruleset);
    append_financial(impl_->db,operation,request.kind,request.identity,preview.before_entry.revision,preview.after_entry.revision,preview.before,preview.change,preview.after,request.undo_of,preview_sequence,catalog_revision);
    if(!fresh.stamp.empty())write_entry_links(operation,account().identity,request.kind==app::ChangeKind::undo?app::TransferAction::restore:app::TransferAction::relink,preview_sequence+1,fresh.changed);
    for(const auto& dependency:expected.dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,operation);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
    finish([&]{transaction.commit();});return *committed_change_outcome(operation);
}
std::vector<app::CommittedHistoryItem> ImportStore::committed_history(const imports::Occurrence& id) const {
    const auto current=committed_entry(id);std::vector<app::CommittedHistoryItem> result;
    Statement rows(impl_->db,"SELECT o.id,o.kind,o.prior_revision,o.result_revision,o.before,o.change,o.after,o.sequence,h.sequence FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.batch=?1 AND ((o.kind=0 AND EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.operation=o.id AND a.batch=o.batch AND a.ordinal=?2)) OR o.ordinal=?2) ORDER BY o.sequence");rows.text(1,id.batch);rows.integer(2,integer(id.ordinal));
    while(rows.row()){const auto currency=currency_as_of(impl_->db,account_for_batch(id.batch).identity,rows.integer(8));const auto kind=static_cast<app::ChangeKind>(rows.integer(1));std::optional<app::EntrySnapshot> before;bool eligible=false;std::string reason="Import publication is not an undoable correction";
        if(kind!=app::ChangeKind::publication){before=entry_snapshot(id,rows.integer(2));const auto operation=rows.bytes(0);const auto receipt=committed_change_outcome(operation);eligible=receipt->result_token==current.token;reason=eligible?"":"Newer transaction or dependent review prevents Undo";if(eligible && later_relationship_blocks(id,operation)){eligible=false;reason="Newer transaction, transfer relationship, or source association prevents Undo";}}
        result.push_back({rows.bytes(0),kind,std::move(before),entry_snapshot(id,kind==app::ChangeKind::publication?0:rows.integer(3)),Money(rows.integer(4),currency),Money(rows.integer(5),currency),Money(rows.integer(6),currency),eligible,reason,rows.integer(7)});
    }return result;
}
std::vector<app::CommittedChangeSummary> ImportStore::committed_changes(std::int64_t before,std::size_t limit) const {
    if(!committed_changes_supported())fail(SQLITE_SCHEMA,"Editing committed transactions is not supported by this dataset");
    if(before<0 || limit==0 || limit>100)fail(SQLITE_CONSTRAINT,"History page limit is invalid");
    Statement rows(impl_->db,"SELECT o.id,o.kind,o.batch,o.ordinal,o.sequence,e.active,e.date,e.payee,e.total,h.revision FROM ledger_operations o JOIN entry_heads h ON h.batch=o.batch AND h.ordinal=o.ordinal JOIN entry_values e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision WHERE o.batch IN(SELECT id FROM batches WHERE account_id=spentit_account()) AND o.kind!=0 AND (?=0 OR o.sequence<?) ORDER BY o.sequence DESC LIMIT ?");rows.integer(1,before);rows.integer(2,before);rows.integer(3,integer(limit));std::vector<app::CommittedChangeSummary> result;
    while(rows.row())result.push_back({rows.bytes(0),static_cast<app::ChangeKind>(rows.integer(1)),{rows.bytes(2),static_cast<std::size_t>(rows.integer(3))},rows.integer(4),rows.integer(5)!=0,rows.bytes(6),rows.bytes(7),Money(rows.integer(8),entry_currency(impl_->db,rows.bytes(2),rows.integer(3),rows.integer(9)))});
    return result;
}
void ImportStore::verify_committed(const app::OperationContext* context) const {
    if(context)context->checkpoint("Verifying committed change history");
    Statement order_check(impl_->db,"SELECT 1 FROM events e WHERE NOT EXISTS(SELECT 1 FROM history_order h WHERE h.batch=e.batch AND h.event_revision=e.revision) UNION ALL SELECT 1 FROM ledger_operations o WHERE NOT EXISTS(SELECT 1 FROM history_order h WHERE h.operation=o.id)");if(order_check.row())fail(SQLITE_CORRUPT,"Missing history ordering");
    Statement common_order(impl_->db,"SELECT count(*),coalesce(min(sequence),1),coalesce(max(sequence),0) FROM history_order");if(!common_order.row() || common_order.integer(1)<1 || common_order.integer(0)>common_order.integer(2))fail(SQLITE_CORRUPT);
    Statement batch_order(impl_->db,"SELECT 1 FROM history_order h JOIN history_order prior ON prior.batch=h.batch AND prior.event_revision=h.event_revision-1 WHERE h.sequence<=prior.sequence");if(batch_order.row())fail(SQLITE_CORRUPT);
    Statement event_order(impl_->db,"SELECT 1 FROM events e JOIN events prior ON prior.sequence=e.sequence-1 JOIN history_order h ON h.batch=e.batch AND h.event_revision=e.revision JOIN history_order p ON p.batch=prior.batch AND p.event_revision=prior.revision WHERE h.sequence<=p.sequence");if(event_order.row())fail(SQLITE_CORRUPT);
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
        if(origin!=revision){if(reviews.bytes(9)!="save" && reviews.bytes(9)!="apply_rules" && reviews.bytes(9)!="currency")fail(SQLITE_CORRUPT);Statement previous(impl_->db,"SELECT s.target_revision,s.target_active,s.review_revision,v.duplicate_batch,v.duplicate_ordinal FROM versions v LEFT JOIN review_stamps s USING(batch,ordinal,revision) WHERE v.batch=? AND v.ordinal=? AND v.revision<? ORDER BY v.revision DESC LIMIT 1");previous.text(1,batch_id);previous.integer(2,ordinal);previous.integer(3,revision);if(!previous.row() || previous.null(0) || previous.integer(0)!=reviews.integer(3) || previous.integer(1)!=reviews.integer(4) || previous.integer(2)!=origin || previous.bytes(3)!=target.batch || previous.integer(4)!=integer(target.ordinal))fail(SQLITE_CORRUPT);}
        Statement then(impl_->db,"SELECT o.result_revision FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.batch=?1 AND ((o.kind=0 AND EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=o.batch AND a.ordinal=?2 AND a.operation=o.id)) OR o.ordinal=?2) AND h.sequence<?3 ORDER BY h.sequence DESC LIMIT 1");then.text(1,target.batch);then.integer(2,integer(target.ordinal));then.integer(3,reviews.integer(8));
        std::int64_t expected_stamp;bool expected_active=true;
        if(then.row()) {
            const auto entry_revision=then.integer(0);expected_active=entry_snapshot(target,entry_revision).active;
            if(entry_revision>0)expected_stamp=entry_revision;
            else {Statement ref(impl_->db,"SELECT draft_revision FROM entry_revisions WHERE batch=? AND ordinal=? AND revision=0");ref.text(1,target.batch);ref.integer(2,integer(target.ordinal));if(!ref.row() || ref.null(0))fail(SQLITE_CORRUPT);expected_stamp=-1-ref.integer(0);}
        } else {
            Statement draft(impl_->db,"SELECT v.revision,v.decision FROM versions v JOIN history_order h ON h.batch=v.batch AND h.event_revision=v.revision WHERE v.batch=? AND v.ordinal=? AND h.sequence<? ORDER BY v.revision DESC LIMIT 1");draft.text(1,target.batch);draft.integer(2,integer(target.ordinal));draft.integer(3,reviews.integer(8));
            if(!draft.row() || draft.integer(1)!=1)fail(SQLITE_CORRUPT,"Invalid pending review target history");
            expected_stamp=-1-draft.integer(0);
        }
        Statement match_history(impl_->db,"SELECT v.payload,g.account FROM generated_source_matches g JOIN transfer_versions v ON v.identity=g.link JOIN ledger_operations l ON l.sequence=v.sequence JOIN history_order h ON h.operation=l.id WHERE g.batch=? AND g.ordinal=? AND h.sequence<? ORDER BY v.sequence DESC LIMIT 1");match_history.text(1,target.batch);match_history.integer(2,integer(target.ordinal));match_history.integer(3,reviews.integer(8));
        if(match_history.row()){const auto value=transfer_codec::view(match_history.bytes(0));const auto& side=value.link.origin.endpoint.account==match_history.integer(1)?value.link.origin:value.link.destination;expected_active=side.state!=transfers::State::removed;}
        if(expected_stamp!=reviews.integer(3) || expected_active!=(reviews.integer(4)!=0))fail(SQLITE_CORRUPT,"Invalid review target history");
    }
    std::map<imports::Occurrence,std::int64_t> heads;std::map<std::int64_t,Money> balances;std::map<std::int64_t,std::vector<OpeningDelta>> deltas;std::map<std::int64_t,std::size_t> applied;
    Statement live_accounts(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=account.singleton) ORDER BY singleton");
    while(live_accounts.row()){const auto id=live_accounts.integer(0);balances.emplace(id,opening_at_start(impl_->db,id));deltas.emplace(id,opening_deltas(impl_->db,id));}
    auto apply_openings=[&](std::int64_t account,std::int64_t before){auto found=balances.find(account);if(found==balances.end())fail(SQLITE_CORRUPT,"Financial timeline names a retired account");auto& list=deltas[account];auto& index=applied[account];while(index<list.size() && list[index].sequence<before){try{found->second=found->second-list[index].prior+list[index].next;}catch(const FinancialError&){fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");}++index;}};
    std::int64_t prior_sequence=0,prior_order=0;std::size_t snapshot_count=0;
    Statement operations(impl_->db,"SELECT o.id,o.kind,o.batch,o.ordinal,o.prior_revision,o.result_revision,o.before,o.change,o.after,o.undo_of,o.preview_sequence,o.sequence,h.sequence FROM ledger_operations o JOIN history_order h ON h.operation=o.id ORDER BY o.sequence");
    while(operations.row()) {
        if(context)context->checkpoint();
        const auto operation=operations.bytes(0);const auto sequence=operations.integer(11),order=operations.integer(12);
        if(operations.integer(1)==5){
            const auto account=operations.integer(3);apply_openings(account,order);auto found=balances.find(account);if(found==balances.end())fail(SQLITE_CORRUPT,"Financial timeline names a retired account");
            if(categories_supported()){Statement epoch(impl_->db,"SELECT o.catalog_revision,(SELECT coalesce(max(c.catalog_after),0) FROM category_operations c CROSS JOIN history_order h ON h.category_operation=c.id WHERE h.sequence<?) FROM ledger_operations o WHERE o.id=?");epoch.integer(1,order);epoch.text(2,operation);if(!epoch.row() || epoch.integer(0)!=epoch.integer(1))fail(SQLITE_CORRUPT,"Financial catalog preview does not match history");}
            Statement command(impl_->db,"SELECT prior_definition,definition FROM account_operations WHERE id=?");command.text(1,operations.bytes(9));if(!command.row() || operations.integer(7)!=0 || !operations.bytes(2).empty())fail(SQLITE_CORRUPT,"Currency command is missing its financial anchor");
            const auto before_currency=Currency::from_definition(command.bytes(0));const auto after_currency=Currency::from_definition(command.bytes(1));
            if(order<=prior_order || sequence<=prior_sequence || found->second.currency()!=before_currency || found->second.units()!=operations.integer(6))fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");
            Money after_money(operations.integer(8),after_currency);try{if(found->second.rescale(after_currency).units()!=after_money.units())fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");}catch(const FinancialError&){fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");}
            std::size_t bumped=0;for(auto& [identity,revision]:heads)if(account_for_batch(identity.batch).identity==account){++revision;++bumped;}snapshot_count+=bumped;found->second=after_money;prior_sequence=sequence;prior_order=order;continue;
        }
        std::optional<app::Account> transfer_owner;bool transfer_nonfinancial=false;
        if(operations.integer(1)==4){Statement t(impl_->db,"SELECT account,did_commit FROM transfer_events WHERE operation=?");t.text(1,operations.bytes(0));if(!t.row())fail(SQLITE_CORRUPT);transfer_owner=account_by_id(t.integer(0));transfer_nonfinancial=t.integer(1)==0;}
        const auto owner=transfer_owner?*transfer_owner:account_for_batch(operations.bytes(2));apply_openings(owner.identity,order);auto balance_found=balances.find(owner.identity);if(balance_found==balances.end())fail(SQLITE_CORRUPT,"Financial timeline names a retired account");auto& balance=balance_found->second;const auto currency=currency_as_of(impl_->db,owner.identity,order);
        const auto kind=static_cast<app::ChangeKind>(operations.integer(1));
        if(categories_supported()){Statement epoch(impl_->db,"SELECT o.catalog_revision,(SELECT coalesce(max(c.catalog_after),0) FROM category_operations c CROSS JOIN history_order h ON h.category_operation=c.id WHERE h.sequence<?) FROM ledger_operations o WHERE o.id=?");epoch.integer(1,order);epoch.text(2,operation);if(!epoch.row() || epoch.integer(0)!=epoch.integer(1))fail(SQLITE_CORRUPT,"Financial catalog preview does not match history");}
        if(order<=prior_order || sequence<=prior_sequence || (!transfer_nonfinancial && (Money(operations.integer(6),currency)!=balance || balance+Money(operations.integer(7),currency)!=Money(operations.integer(8),currency))))fail(SQLITE_CORRUPT,"Financial timeline does not reconcile");
        if(kind==app::ChangeKind::transfer){
            if(operations.bytes(2)!="" || operations.integer(3)!=0 || operations.integer(4)!=-1 || operations.integer(5)!=0 || !operations.bytes(9).empty() || (operations.integer(10)<prior_sequence || operations.integer(10)>=sequence) || (transfer_nonfinancial && (operations.integer(7)!=0 || operations.integer(6)!=operations.integer(8))))fail(SQLITE_CORRUPT);
        } else if(kind==app::ChangeKind::publication){
            const auto receipt=outcome(operation);if(!receipt || receipt->batch!=operations.bytes(2) || receipt->before!=balance || receipt->change.units()!=operations.integer(7) || receipt->after.units()!=operations.integer(8) || operations.integer(3)!=-1 || operations.integer(4)!=-1 || operations.integer(5)!=0 || !operations.bytes(9).empty() || operations.integer(10)!=0)fail(SQLITE_CORRUPT);
            Statement event(impl_->db,"SELECT h.sequence FROM history_order h JOIN events e ON e.batch=h.batch AND e.revision=h.event_revision WHERE e.batch=? AND e.kind IN('commit','accept') AND e.revision=?");event.text(1,receipt->batch);event.integer(2,receipt->revision+1);if(!event.row() || event.integer(0)+1!=order)fail(SQLITE_CORRUPT);
            const auto b=batch(impl_->db,receipt->batch);std::vector<Money> values;std::size_t included=0,excluded=0;
            if(retained_batch(impl_->db,b.id)) {
                Statement cleared(impl_->db,"SELECT count(*) FROM records r WHERE batch=? AND NOT EXISTS(SELECT 1 FROM retained_transfer_rows k WHERE k.batch=r.batch AND k.ordinal=r.ordinal)");cleared.text(1,b.id);if(!cleared.row())fail(SQLITE_CORRUPT);excluded=static_cast<std::size_t>(cleared.integer(0));
            }
            Statement members(impl_->db,"SELECT ordinal,decision,draft_revision,target_batch,target_ordinal FROM acceptance_members WHERE operation=? ORDER BY ordinal");members.text(1,operation);
            while(members.row()) {
                if(context)context->checkpoint();
                const auto ordinal=static_cast<std::size_t>(members.integer(0));const imports::Occurrence identity{b.id,ordinal};
                if(members.integer(1)==1) {
                    const auto initial=entry_snapshot(identity,0);
                    Statement reference(impl_->db,"SELECT e.draft_revision,a.operation FROM entry_revisions e JOIN accepted_transactions a USING(batch,ordinal) WHERE e.batch=? AND e.ordinal=? AND e.revision=0");reference.text(1,b.id);reference.integer(2,integer(ordinal));
                    if(!reference.row() || reference.integer(0)!=members.integer(2) || reference.bytes(1)!=operation || !initial.active || heads.contains(identity))fail(SQLITE_CORRUPT,"Acceptance reference does not replay");
                    heads.emplace(identity,0);++snapshot_count;++included;values.push_back(initial.draft.transaction.effective_total());
                } else {
                    ++excluded;
                    const imports::Occurrence target{members.bytes(3),static_cast<std::size_t>(members.integer(4))};
                    if(!heads.contains(target)) {
                        Statement match(impl_->db,"SELECT 1 FROM generated_source_matches g JOIN ledger_operations l ON l.sequence=g.sequence JOIN history_order h ON h.operation=l.id WHERE g.batch=? AND g.ordinal=? AND h.sequence<?");match.text(1,target.batch);match.integer(2,integer(target.ordinal));match.integer(3,order);
                        if(!match.row()) {
                            // A publication and a dataset acceptance are atomic. A
                            // duplicate may precede its included target in row/batch
                            // replay order within that boundary, but never in a
                            // separate later acceptance. The target still receives
                            // its normal initial-snapshot/reference checks on replay.
                            Statement together(impl_->db,R"SQL(
SELECT 1 FROM accepted_transactions a
JOIN operations o ON o.id=a.operation AND o.batch=a.batch
JOIN acceptance_members m ON m.operation=a.operation AND m.ordinal=a.ordinal
WHERE a.batch=?1 AND a.ordinal=?2 AND m.decision=1
AND (a.operation=?3 OR EXISTS(
    SELECT 1 FROM acceptance_operations target
    JOIN acceptance_operations current ON current.parent=target.parent
    WHERE target.operation=a.operation AND current.operation=?3))
)SQL");
                            together.text(1,target.batch);together.integer(2,integer(target.ordinal));together.text(3,operation);
                            if(!together.row())fail(SQLITE_CORRUPT,"Acceptance precedes its duplicate target");
                        }
                    }
                }
            }
            if(included!=receipt->included || excluded!=receipt->excluded || imports::sum(currency,values)+transfer_import_adjustment(operation)!=receipt->change)fail(SQLITE_CORRUPT);
        } else {
            // The cursor already supplies the operation, ordering and money facts.
            // Retain every receipt/dependency/snapshot comparison below.
            const imports::Occurrence id{operations.bytes(2),static_cast<std::size_t>(operations.integer(3))};require_account(id.batch);
            std::vector<app::DependencyStamp> deps;
            {Statement saved_dependencies(impl_->db,"SELECT batch,revision,state FROM change_dependencies WHERE operation=? ORDER BY batch");saved_dependencies.text(1,operation);
            while(saved_dependencies.row())deps.push_back({saved_dependencies.bytes(0),saved_dependencies.integer(1),static_cast<app::BatchState>(saved_dependencies.integer(2))});}
            // Release the plan before validating an Undo's original receipt,
            // which reads its own dependencies with the same SQL.
            const std::optional<app::ChangeReceipt> receipt=app::ChangeReceipt{operation,kind,id,entry_snapshot(id,operations.integer(4)),entry_snapshot(id,operations.integer(5)),Money(operations.integer(6),currency),Money(operations.integer(7),currency),Money(operations.integer(8),currency),operations.bytes(9),{id,operations.integer(5),std::move(deps)},sequence};
            const auto head=heads.find(receipt->identity);
            if(head==heads.end() || receipt->before_entry.revision!=head->second || receipt->after_entry.revision!=head->second+1 || (operations.integer(10)<prior_sequence || operations.integer(10)>=sequence) || receipt->result_token.dependencies!=dependencies(receipt->identity,order))fail(SQLITE_CORRUPT,"Invalid committed change dependencies");
            const auto& before=receipt->before_entry;const auto& after=receipt->after_entry;
            const Money zero(0,currency);const auto delta=(after.active?after.draft.transaction.effective_total():zero)-(before.active?before.draft.transaction.effective_total():zero);if(delta!=receipt->change)fail(SQLITE_CORRUPT);
            if(kind==app::ChangeKind::correct){if(!before.active || !after.active || !receipt->undo_of.empty())fail(SQLITE_CORRUPT);
#ifdef SPENTIT_RULES_STORAGE
                if(rules_format(impl_->db)){
                    bool reapply=false,replace_manual=false;
                    if(scalar(impl_->db,"PRAGMA user_version")=="52"){
                        Statement member(impl_->db,"SELECT g.replace_manual,g.undo_of IS NULL FROM rule_reapplication_members m JOIN rule_reapplications g ON g.id=m.operation WHERE m.entry_operation=?");
                        member.text(1,operation);
                        if(member.row()){if(member.integer(1)==0)fail(SQLITE_CORRUPT,"Rule reapply Undo used a correction");reapply=true;replace_manual=member.integer(0)!=0;}
                    }
                    Statement account_apply(impl_->db,"SELECT substr(a.request,1,1) FROM review_answers a CROSS JOIN review_members m ON m.answer=a.id WHERE m.child=? AND m.committed=1 AND a.question='rules-account'");account_apply.text(1,operation);
                    if(account_apply.row()){reapply=true;replace_manual=account_apply.bytes(0)=="1";}
                    check_rule_transition(before.draft,after.draft,reapply,replace_manual);
                }
#endif
            }
            else if(kind==app::ChangeKind::remove){if(!before.active || after.active || !same_draft(before.draft,after.draft) || !receipt->undo_of.empty())fail(SQLITE_CORRUPT);}
            else if(kind==app::ChangeKind::undo){
                const auto original=committed_change_outcome(receipt->undo_of);
                Statement grouped(impl_->db,"SELECT 1 FROM review_undo_members m JOIN review_undos u ON u.id=m.undo_id JOIN review_members a ON a.answer=u.answer AND a.child=m.original_child WHERE m.child=? AND m.original_child=? AND a.batch=? AND a.ordinal=?");grouped.text(1,operation);grouped.text(2,receipt->undo_of);grouped.text(3,receipt->identity.batch);grouped.integer(4,integer(receipt->identity.ordinal));const bool review_undo=grouped.row();
                if(!original || original->sequence>=sequence || original->identity!=receipt->identity || original->before_entry.active!=after.active || !same_draft(original->before_entry.draft,after.draft) ||
                   (review_undo?(original->after_entry.active!=before.active || !same_draft(original->after_entry.draft,before.draft)):(original->after_entry.revision!=before.revision || original->result_token.dependencies!=receipt->result_token.dependencies)))fail(SQLITE_CORRUPT,"Invalid strict Undo history");
            }
            else fail(SQLITE_CORRUPT);
            head->second=after.revision;++snapshot_count;
        }
        if(!transfer_nonfinancial)balance=Money(operations.integer(8),currency);
        prior_sequence=sequence;prior_order=order;
    }
    Statement counts(impl_->db,"SELECT (SELECT count(*) FROM entry_heads),(SELECT count(*) FROM entry_values),(SELECT count(*) FROM operations),(SELECT count(*) FROM ledger_operations WHERE kind=0)");if(!counts.row() || counts.integer(0)!=integer(heads.size()) || counts.integer(1)!=integer(snapshot_count) || counts.integer(2)!=counts.integer(3))fail(SQLITE_CORRUPT);
    for(const auto& [identity,revision]:heads){if(context)context->checkpoint();Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,identity.batch);head.integer(2,integer(identity.ordinal));if(!head.row() || head.integer(0)!=revision)fail(SQLITE_CORRUPT);}
    Statement acceptance(impl_->db,"SELECT d.id,d.transactions,coalesce((SELECT sum(o.included) FROM acceptance_operations m JOIN operations o ON o.id=m.operation WHERE m.parent=d.id),0)+(SELECT count(*) FROM acceptance_operations m JOIN transfer_events t ON t.operation=m.operation JOIN ledger_operations l ON l.id=m.operation WHERE m.parent=d.id AND l.kind=4 AND t.did_commit=1) FROM dataset_acceptances d");
    while(acceptance.row())if(acceptance.integer(1)!=acceptance.integer(2))fail(SQLITE_CORRUPT,"Dataset acceptance count does not reconcile");
    Statement accepted(impl_->db,"SELECT 1 FROM accepted_transactions a LEFT JOIN acceptance_members m ON m.operation=a.operation AND m.ordinal=a.ordinal LEFT JOIN operations o ON o.id=a.operation LEFT JOIN entry_revisions e ON e.batch=a.batch AND e.ordinal=a.ordinal AND e.revision=0 WHERE m.decision!=1 OR m.draft_revision!=e.draft_revision OR o.batch!=a.batch OR e.batch IS NULL OR m.operation IS NULL UNION ALL SELECT 1 FROM entry_heads h WHERE NOT EXISTS(SELECT 1 FROM accepted_transactions a WHERE a.batch=h.batch AND a.ordinal=h.ordinal)");
    if(accepted.row())fail(SQLITE_CORRUPT,"Transaction acceptance does not match its financial reference");
    Statement extra_dependencies(impl_->db,"SELECT 1 FROM change_dependencies d JOIN ledger_operations o ON o.id=d.operation WHERE o.kind=0");if(extra_dependencies.row())fail(SQLITE_CORRUPT);
    for(const auto& [account,balance]:balances)apply_openings(account,INT64_MAX);
    Statement end_accounts(impl_->db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=account.singleton)");
    while(end_accounts.row()){
        const auto acct=account_by_id(end_accounts.integer(0));
        std::vector<Money> values{acct.opening,local_transfer_total(acct.identity,false)};
        Statement totals(impl_->db,"SELECT h.batch,h.ordinal,h.revision,e.total FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) JOIN batches b ON b.id=h.batch WHERE b.account_id=? AND e.active=1");totals.integer(1,acct.identity);
        while(totals.row())values.emplace_back(totals.integer(3),entry_currency(impl_->db,totals.bytes(0),totals.integer(1),totals.integer(2)));
        if(imports::sum(acct.currency,values)!=balances.at(acct.identity))fail(SQLITE_CORRUPT,"Current heads do not reconcile");
    }
}
