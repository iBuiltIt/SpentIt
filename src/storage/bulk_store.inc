bool ImportStore::bulk_supported() const {return scalar(impl_->db,"PRAGMA user_version")=="10";}
std::string ImportStore::bulk_group(std::string_view operation) const {if(!bulk_supported())return {};Statement group(impl_->db,"SELECT operation FROM bulk_members WHERE entry_operation=?");group.text(1,operation);return group.row()?group.bytes(0):std::string{};}
namespace {
constexpr const char* bulk_refresh="Bulk preview changed; refresh before applying";
void bulk_bound(const app::BulkRequest& request) {
    if(request.identities.empty() || request.identities.size()>4096)fail(SQLITE_CONSTRAINT,"Choose between 1 and 4096 committed transactions");
    std::set<imports::Occurrence> ids;
    for(const auto& id:request.identities){identifier(id.batch);if(!ids.insert(id).second)fail(SQLITE_CONSTRAINT,"Duplicate bulk selection");}
    if(!std::is_sorted(request.identities.begin(),request.identities.end()))fail(SQLITE_CONSTRAINT,"Bulk identities must be in canonical order");
}
bool same_preview(const app::BulkPreview& a,const app::BulkPreview& b){
    if(a.request!=b.request || a.preview_sequence!=b.preview_sequence || a.catalog_revision!=b.catalog_revision || a.before!=b.before || a.change!=b.change || a.after!=b.after || a.entries.size()!=b.entries.size())return false;
    for(std::size_t i=0;i<a.entries.size();++i){const auto& x=a.entries[i];const auto& y=b.entries[i];if(x.expected!=y.expected || x.before_entry.active!=y.before_entry.active || x.after_entry.active!=y.after_entry.active || !same_draft(x.before_entry.draft,y.before_entry.draft) || !same_draft(x.after_entry.draft,y.after_entry.draft))return false;}
    return true;
}
}
std::optional<app::BulkReceipt> ImportStore::bulk_outcome(std::string_view operation) const {
    if(!bulk_supported())fail(SQLITE_SCHEMA,"Bulk changes require a new schema-8 dataset");
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement group(impl_->db,"SELECT kind,category,undo_of,preview_sequence,catalog_revision,count FROM bulk_operations WHERE id=?");group.text(1,operation);
    if(!group.row()){Statement used(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");used.text(1,operation);if(used.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");return {};}
    Statement owner(impl_->db,"SELECT o.batch FROM bulk_members m JOIN ledger_operations o ON o.id=m.entry_operation WHERE m.operation=? ORDER BY m.position LIMIT 1");owner.text(1,operation);if(!owner.row())fail(SQLITE_CORRUPT);require_account(owner.bytes(0));
    const auto currency=account_for_batch(owner.bytes(0)).currency;const Money zero(0,currency);
    app::BulkReceipt receipt{std::string(operation),{static_cast<app::ChangeKind>(group.integer(0)),{},group.bytes(1),group.bytes(2)},{},zero,zero,zero,group.integer(3),group.integer(4)};
    Statement members(impl_->db,"SELECT position,entry_operation FROM bulk_members WHERE operation=? ORDER BY position");members.text(1,operation);
    while(members.row()){
        if(members.integer(0)!=integer(receipt.entries.size()) || receipt.entries.size()>=4096)fail(SQLITE_CORRUPT,"Invalid bulk membership");
        const auto entry=committed_change_outcome(members.bytes(1));if(!entry)fail(SQLITE_CORRUPT,"Missing bulk member history");
        if(account_for_batch(entry->identity.batch).identity!=account_for_batch(owner.bytes(0)).identity)fail(SQLITE_CORRUPT,"Bulk members span accounts");
        if(entry->kind!=receipt.request.kind || entry->sequence!=receipt.preview_sequence+integer(receipt.entries.size())+1)fail(SQLITE_CORRUPT,"Bulk history is not contiguous");
        if(receipt.entries.empty()){if(entry->operation!=operation)fail(SQLITE_CORRUPT);receipt.before=entry->before;}else if(entry->before!=receipt.after)fail(SQLITE_CORRUPT);
        receipt.after=entry->after;receipt.request.identities.push_back(entry->identity);receipt.entries.push_back(*entry);
    }
    if(integer(receipt.entries.size())!=group.integer(5))fail(SQLITE_CORRUPT,"Incomplete bulk operation");
    bulk_bound(receipt.request);std::vector<Money> deltas;for(const auto& entry:receipt.entries)deltas.push_back(entry.change);
    receipt.change=imports::sum(currency,deltas);if(receipt.before+receipt.change!=receipt.after)fail(SQLITE_CORRUPT,"Bulk totals do not reconcile");
    return receipt;
}
app::BulkPreview ImportStore::preview_bulk(const app::BulkRequest& request) const {
    if(!bulk_supported())fail(SQLITE_SCHEMA,"Bulk changes require a new schema-8 dataset");bulk_bound(request);
    if(request.kind!=app::ChangeKind::correct && request.kind!=app::ChangeKind::remove && request.kind!=app::ChangeKind::undo)fail(SQLITE_CONSTRAINT,"Invalid bulk action");
    if((request.kind!=app::ChangeKind::correct && !request.category.empty()) || (request.kind!=app::ChangeKind::undo && !request.undo_of.empty()))fail(SQLITE_CONSTRAINT,"Invalid bulk fields");
    const auto epoch=catalog_sequence(impl_->db),sequence=financial_sequence(impl_->db);const auto before=committed_balance();
    if(sequence>INT64_MAX-integer(request.identities.size()))fail(SQLITE_FULL,"History capacity exhausted");
    app::BulkPreview result{request,{},before,Money(0,before.currency()),before,sequence,epoch};
    std::optional<app::BulkReceipt> undo;
    if(request.kind==app::ChangeKind::undo){undo=bulk_outcome(request.undo_of);if(!undo || undo->request.identities!=request.identities)fail(SQLITE_CONSTRAINT,"Choose the complete bulk operation to undo");}
    std::string path;
    if(request.kind==app::ChangeKind::correct && !request.category.empty()){const auto display=resolve_category(request.category);if(!display.assignable)fail(SQLITE_CONSTRAINT,"Choose an available category");path=display.path;}
    std::size_t allocations=0,text_bytes=0;std::vector<Money> deltas;
    for(std::size_t i=0;i<request.identities.size();++i){
        const auto& id=request.identities[i];const auto current=committed_entry(id);
        allocations+=current.current.draft.transaction.allocations().size();if(allocations>16384)fail(SQLITE_TOOBIG,"Bulk preview exceeds 16384 allocations");
        text_bytes+=current.current.draft.payee.size()+current.current.draft.memo.size();for(const auto& allocation:current.current.draft.transaction.allocations())text_bytes+=allocation.category.size()+allocation.memo.size()+path.size();if(text_bytes>16*1024*1024)fail(SQLITE_TOOBIG,"Bulk preview text exceeds 16 MiB");
        app::ChangeRequest change{request.kind,id,{}, {}};
        if(request.kind==app::ChangeKind::correct){
            auto draft=current.current.draft;auto values=draft.transaction.allocations();
            for(auto& value:values){value.category=path;value.category_binding=request.category.empty()?std::optional<CategoryBinding>{}:CategoryBinding{request.category,epoch};value.category_owner=FieldOwnership::manual;value.category_rule.reset();}
            draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),std::move(values));change.correction=std::move(draft);
        }else if(undo)change.undo_of=undo->entries[i].operation;
        auto preview=preview_committed_change(change);preview.before=result.after;preview.after=preview.before+preview.change;preview.preview_sequence=sequence+integer(i);
        result.after=preview.after;deltas.push_back(preview.change);result.entries.push_back(std::move(preview));
    }
    result.change=imports::sum(before.currency(),deltas);return result;
}
app::BulkReceipt ImportStore::apply_bulk(std::string_view operation,const app::BulkPreview& supplied) {
    identifier(operation);if(operation.size()>96)fail(SQLITE_CONSTRAINT,"Bulk operation identity is too long");
    if(const auto prior=bulk_outcome(operation)){
        if(prior->request!=supplied.request || prior->preview_sequence!=supplied.preview_sequence || prior->catalog_revision!=supplied.catalog_revision || prior->entries.size()!=supplied.entries.size())fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        for(std::size_t i=0;i<prior->entries.size();++i){auto expected=prior->entries[i].result_token;expected.revision=prior->entries[i].before_entry.revision;if(expected!=supplied.entries[i].expected || !same_draft(prior->entries[i].after_entry.draft,supplied.entries[i].after_entry.draft) || prior->entries[i].after_entry.active!=supplied.entries[i].after_entry.active)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");}
        return *prior;
    }
    const auto preview=preview_bulk(supplied.request);if(!same_preview(preview,supplied))fail(SQLITE_CONSTRAINT,bulk_refresh);
    std::vector<std::string> operations;std::vector<std::optional<std::int64_t>> rulesets;
    for(std::size_t i=0;i<preview.entries.size();++i){
        operations.push_back(i==0?std::string(operation):std::string(operation)+"-"+std::to_string(i));
        Statement used(impl_->db,"SELECT 1 FROM operation_registry WHERE id=? UNION ALL SELECT 1 FROM operations WHERE id=?");used.text(1,operations.back());used.text(2,operations.back());if(used.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
        std::optional<std::int64_t> ruleset;
        if(preview.request.kind==app::ChangeKind::undo){
            const auto original=committed_change_outcome(preview.entries[i].request.undo_of);
            Statement marks(impl_->db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");marks.text(1,original->identity.batch);marks.integer(2,integer(original->identity.ordinal));marks.integer(3,original->before_entry.revision);if(marks.row())ruleset=marks.integer(0);
        }
        rulesets.push_back(ruleset);
    }
    Atomic transaction(impl_->db);
    if(financial_sequence(impl_->db)!=preview.preview_sequence || catalog_sequence(impl_->db)!=preview.catalog_revision)fail(SQLITE_CONSTRAINT,bulk_refresh);
    for(const auto& entry:preview.entries)if(committed_entry(entry.request.identity).token!=entry.expected)fail(SQLITE_CONSTRAINT,bulk_refresh);
    for(std::size_t i=0;i<preview.entries.size();++i){
        const auto& entry=preview.entries[i];store_entry(impl_->db,entry.request.identity,entry.after_entry,rulesets[i]);
        append_financial(impl_->db,operations[i],entry.request.kind,entry.request.identity,entry.before_entry.revision,entry.after_entry.revision,entry.before,entry.change,entry.after,entry.request.undo_of,entry.preview_sequence,preview.catalog_revision);
        for(const auto& dependency:entry.expected.dependencies){Statement d(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");d.text(1,operations[i]);d.text(2,dependency.batch);d.integer(3,dependency.revision);d.integer(4,static_cast<int>(dependency.state));d.done();}
    }
    Statement group(impl_->db,"INSERT INTO bulk_operations VALUES(?,?,?,?,?,?,?)");group.text(1,operation);group.integer(2,static_cast<int>(preview.request.kind));if(preview.request.kind==app::ChangeKind::correct)group.text(3,preview.request.category);if(preview.request.kind==app::ChangeKind::undo)group.text(4,preview.request.undo_of);group.integer(5,preview.preview_sequence);group.integer(6,preview.catalog_revision);group.integer(7,integer(preview.entries.size()));group.done();
    for(std::size_t i=0;i<operations.size();++i){Statement member(impl_->db,"INSERT INTO bulk_members VALUES(?,?,?)");member.text(1,operation);member.integer(2,integer(i));member.text(3,operations[i]);member.done();}
    impl_->finish(transaction);return *bulk_outcome(operation);
}
void ImportStore::verify_bulk(const app::OperationContext* context) const {
    Statement groups(impl_->db,"SELECT id FROM bulk_operations ORDER BY preview_sequence");
    while(groups.row()){
        if(context)context->checkpoint("Verifying bulk change history");const auto receipt=*bulk_outcome(groups.bytes(0));
        std::optional<app::BulkReceipt> original;
        if(receipt.request.kind==app::ChangeKind::undo){original=bulk_outcome(receipt.request.undo_of);if(!original || original->entries.size()!=receipt.entries.size() || original->request.identities!=receipt.request.identities)fail(SQLITE_CORRUPT,"Invalid bulk Undo membership");}
        for(std::size_t i=0;i<receipt.entries.size();++i){const auto& entry=receipt.entries[i];
            if(context)context->checkpoint();
            Statement epoch(impl_->db,"SELECT catalog_revision FROM ledger_operations WHERE id=?");epoch.text(1,entry.operation);if(!epoch.row() || epoch.integer(0)!=receipt.catalog_revision)fail(SQLITE_CORRUPT);
            if(receipt.request.kind==app::ChangeKind::correct){
                auto expected=entry.before_entry.draft;auto values=expected.transaction.allocations();
                for(std::size_t n=0;n<values.size();++n){const auto& after=entry.after_entry.draft.transaction.allocations().at(n);values[n].category=after.category;values[n].category_binding=after.category_binding;values[n].category_owner=FieldOwnership::manual;values[n].category_rule.reset();if(receipt.request.category.empty()?after.category_binding.has_value() || !after.category.empty():!after.category_binding || after.category_binding->identity!=receipt.request.category || after.category_binding->catalog_revision!=receipt.catalog_revision)fail(SQLITE_CORRUPT,"Bulk category receipt does not match allocations");}
                expected.transaction=expected.transaction.corrected(expected.transaction.effective_total(),std::move(values));if(!same_draft(expected,entry.after_entry.draft))fail(SQLITE_CORRUPT,"Bulk category change modified other fields");
            }else if(original && entry.undo_of!=original->entries[i].operation)fail(SQLITE_CORRUPT,"Invalid bulk Undo membership");
        }
    }
}
