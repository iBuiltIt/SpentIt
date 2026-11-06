namespace { bool same_draft(const imports::DraftRow&,const imports::DraftRow&); }
namespace {
std::int64_t catalog_sequence(sqlite3* db){Statement state(db,"SELECT revision FROM category_state WHERE singleton=1");if(!state.row())fail(SQLITE_CORRUPT);return state.integer(0);}
CategoryDefinition category_at(sqlite3* db,std::string_view id,std::int64_t catalog=INT64_MAX) {
    Statement row(db,"SELECT id,name,parent,kind,revision,archived,deleted,income FROM category_versions WHERE id=? AND catalog_revision<=? ORDER BY revision DESC LIMIT 1");row.text(1,id);row.integer(2,catalog);if(!row.row())fail(SQLITE_NOTFOUND,"Category does not exist");return {row.bytes(0),row.bytes(1),row.bytes(2),static_cast<CategoryKind>(row.integer(3)),row.integer(4),row.integer(5)!=0,row.integer(6)!=0,row.integer(7)!=0};
}
app::CategoryDisplay category_display(sqlite3* db,std::string_view id,std::int64_t catalog=INT64_MAX){const auto category=category_at(db,id,catalog);if(category.deleted)fail(SQLITE_CONSTRAINT,"Category was deleted");if(category.kind!=CategoryKind::leaf || !category.parent.empty())fail(SQLITE_CORRUPT);return {category.identity,category.name,!category.archived,category.archived};}
void validate_binding(sqlite3* db,const Allocation& allocation){if(!allocation.category_binding)return;const auto& binding=*allocation.category_binding;if(binding.catalog_revision<1 || binding.catalog_revision>catalog_sequence(db))fail(SQLITE_CONSTRAINT,"Category assignment revision is invalid");const auto at=category_display(db,binding.identity,binding.catalog_revision);if(!at.assignable || at.path!=allocation.category)fail(SQLITE_CONSTRAINT,"Category assignment does not match its retained definition");}
bool category_referenced(sqlite3* db,std::string_view id){Statement used(db,"SELECT 1 FROM (SELECT category_id FROM draft_allocation_meta WHERE category_id IS NOT NULL UNION ALL SELECT category_id FROM entry_allocation_meta WHERE category_id IS NOT NULL) WHERE category_id=? LIMIT 1");used.text(1,id);if(used.row())return true;
    // Publication pins identities for other accounts whose source assignments
    // remain drafts. These references also prevent deletion/name reuse.
    Statement groups(db,"SELECT g.id,l.catalog_revision,s.bytes FROM import_groups g JOIN ledger_operations l ON l.id=g.publication JOIN sources s ON s.id=g.id JOIN source_headers h ON h.source_id=g.id WHERE g.state=2 AND h.format='qif'");
    while(groups.row()) {
        Statement existed(db,"SELECT 1 FROM category_versions WHERE id=? AND catalog_revision<=?");existed.text(1,id);existed.integer(2,groups.integer(1));if(!existed.row())continue;
        const auto definition=category_at(db,id,groups.integer(1));if(definition.deleted)continue;
        const auto catalog=qif::index_document(groups.bytes(2)).categories;
        if(std::none_of(catalog.begin(),catalog.end(),[&](const auto& c){return app::category_name_matches(c.name,definition.name);}))continue;
        Statement labels(db,"SELECT DISTINCT a.category FROM allocations a JOIN draft_allocation_meta m USING(batch,ordinal,revision,position) JOIN import_group_accounts g ON g.batch=a.batch WHERE g.group_id=? AND m.category_owner=1 AND m.category_id IS NULL");labels.text(1,groups.bytes(0));
        while(labels.row()) {try {if(app::category_name_matches(labels.bytes(0),definition.name))return true;}catch(const std::invalid_argument&) {}}
    }
    if(!rules_format(db))return false;
    Statement rules(db,"SELECT 1 FROM rule_definitions WHERE category_id=? LIMIT 1");rules.text(1,id);return rules.row();}
bool source_category_equal(const Allocation& allocation,const qif::SourceAllocation* original) {
    if(!original)return false;
    if(!allocation.category_binding)return allocation.category==original->category;
    try {return app::category_name_matches(allocation.category,original->category);}
    catch(const std::invalid_argument&){return false;}
}
void validate_owner(FieldOwnership owner){if(owner!=FieldOwnership::source && owner!=FieldOwnership::manual)fail(SQLITE_CONSTRAINT,"Unknown or unverified field ownership");}
void accept_rule_owner(FieldOwnership owner,const std::optional<RuleMark>& mark,bool reading){const int code=reading?SQLITE_CORRUPT:SQLITE_CONSTRAINT;if((owner==FieldOwnership::rule)!=mark.has_value())fail(code,"Rule provenance does not match its ownership");if(owner==FieldOwnership::rule){if(!reading)identifier(mark->identity);if(mark->version<0)fail(code,"Rule provenance does not match its ownership");}else validate_owner(owner);}
std::int64_t carried_ruleset(sqlite3* db,const imports::Occurrence& id,std::int64_t revision,bool entry){
    if(entry && revision>0){Statement prior(db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");prior.text(1,id.batch);prior.integer(2,integer(id.ordinal));prior.integer(3,revision-1);if(!prior.row())fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");return prior.integer(0);}
    Statement previous(db,entry?"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?":"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=? AND revision<?");previous.text(1,id.batch);previous.integer(2,integer(id.ordinal));if(!entry)previous.integer(3,revision);if(!previous.row() || previous.null(0))fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");
    Statement mark(db,"SELECT ruleset_revision FROM draft_rule_fields WHERE batch=? AND ordinal=? AND revision=?");mark.text(1,id.batch);mark.integer(2,integer(id.ordinal));mark.integer(3,previous.integer(0));if(!mark.row())fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");return mark.integer(0);
}
void save_metadata(sqlite3* db,const imports::Occurrence& id,std::int64_t revision,const imports::DraftRow& row,bool entry,std::optional<std::int64_t> ruleset) {
    if(!categories_format(db)){for(const auto& allocation:row.transaction.allocations())if(allocation.category_binding)fail(SQLITE_SCHEMA,"Category assignment requires a new dataset");return;}
    if(rules_format(db)){accept_rule_owner(row.payee_owner,row.payee_rule,false);accept_rule_owner(row.memo_owner,row.memo_rule,false);}
    else {validate_owner(row.payee_owner);validate_owner(row.memo_owner);}
    Statement fields(db,entry?"INSERT INTO entry_fields VALUES(?,?,?,?,?,?,?)":"INSERT INTO draft_fields VALUES(?,?,?,?,?,?,?)");fields.text(1,id.batch);fields.integer(2,integer(id.ordinal));fields.integer(3,revision);fields.integer(4,static_cast<int>(row.payee_owner));fields.integer(5,static_cast<int>(row.memo_owner));fields.integer(6,row.manual_split_amounts);fields.text(7,row.remainder_allocation);fields.done();
    std::int64_t position=0;std::set<std::string> identities;std::set<std::size_t> sources;
    for(const auto& allocation:row.transaction.allocations()) {
        identifier(allocation.identity);if(!allocation.source_ordinal && allocation.identity.starts_with("source-"))fail(SQLITE_CONSTRAINT,"Source identity requires its retained association");if(!identities.insert(allocation.identity).second || (allocation.source_ordinal && !sources.insert(*allocation.source_ordinal).second))fail(SQLITE_CONSTRAINT,"Duplicate allocation identity or source association");
        if(rules_format(db)){accept_rule_owner(allocation.category_owner,allocation.category_rule,false);accept_rule_owner(allocation.memo_owner,allocation.memo_rule,false);}
        else {validate_owner(allocation.category_owner);validate_owner(allocation.memo_owner);}
        validate_binding(db,allocation);
        Statement meta(db,entry?"INSERT INTO entry_allocation_meta VALUES(?,?,?,?,?,?,?,?,?,?)":"INSERT INTO draft_allocation_meta VALUES(?,?,?,?,?,?,?,?,?,?)");meta.text(1,id.batch);meta.integer(2,integer(id.ordinal));meta.integer(3,revision);meta.integer(4,position++);meta.text(5,allocation.identity);
        if(allocation.source_ordinal)meta.integer(6,integer(*allocation.source_ordinal));
        if(allocation.category_binding){meta.text(7,allocation.category_binding->identity);meta.integer(8,allocation.category_binding->catalog_revision);}
        meta.integer(9,static_cast<int>(allocation.category_owner));meta.integer(10,static_cast<int>(allocation.memo_owner));meta.done();
    }
    if(!rules_format(db))return;
    bool any=row.payee_rule.has_value()||row.memo_rule.has_value();
    for(const auto& allocation:row.transaction.allocations())if(allocation.category_rule||allocation.memo_rule)any=true;
    if(!any)return;
    std::int64_t ruleset_revision;
    if(ruleset){if(*ruleset<1)fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");ruleset_revision=*ruleset;}
    else ruleset_revision=carried_ruleset(db,id,revision,entry);
    Statement provenance(db,entry?"INSERT INTO entry_rule_fields VALUES(?,?,?,?,?,?,?,?)":"INSERT INTO draft_rule_fields VALUES(?,?,?,?,?,?,?,?)");provenance.text(1,id.batch);provenance.integer(2,integer(id.ordinal));provenance.integer(3,revision);
    if(row.payee_rule){provenance.text(4,row.payee_rule->identity);provenance.integer(5,row.payee_rule->version);}
    if(row.memo_rule){provenance.text(6,row.memo_rule->identity);provenance.integer(7,row.memo_rule->version);}
    provenance.integer(8,ruleset_revision);provenance.done();
    std::int64_t marked=0;for(const auto& allocation:row.transaction.allocations()){
        if(allocation.category_rule||allocation.memo_rule){Statement mark(db,entry?"INSERT INTO entry_rule_allocations VALUES(?,?,?,?,?,?,?,?)":"INSERT INTO draft_rule_allocations VALUES(?,?,?,?,?,?,?,?)");mark.text(1,id.batch);mark.integer(2,integer(id.ordinal));mark.integer(3,revision);mark.integer(4,marked);if(allocation.category_rule){mark.text(5,allocation.category_rule->identity);mark.integer(6,allocation.category_rule->version);}if(allocation.memo_rule){mark.text(7,allocation.memo_rule->identity);mark.integer(8,allocation.memo_rule->version);}mark.done();}
        ++marked;
    }
}
void load_metadata(sqlite3* db,const imports::Occurrence& id,std::int64_t revision,imports::DraftRow& row,bool entry){
    if(!categories_format(db))return;
    Statement fields(db,entry?"SELECT payee_owner,memo_owner,manual_split_amounts,remainder_allocation FROM entry_fields WHERE batch=? AND ordinal=? AND revision=?":"SELECT payee_owner,memo_owner,manual_split_amounts,remainder_allocation FROM draft_fields WHERE batch=? AND ordinal=? AND revision=?");fields.text(1,id.batch);fields.integer(2,integer(id.ordinal));fields.integer(3,revision);if(!fields.row())fail(SQLITE_CORRUPT);row.payee_owner=static_cast<FieldOwnership>(fields.integer(0));row.memo_owner=static_cast<FieldOwnership>(fields.integer(1));if(fields.integer(2)!=0 && fields.integer(2)!=1)fail(SQLITE_CORRUPT);row.manual_split_amounts=fields.integer(2)!=0;row.remainder_allocation=fields.bytes(3);if(!row.remainder_allocation.empty())identifier(row.remainder_allocation);
    if(rules_format(db)){Statement marks(db,entry?"SELECT payee_rule,payee_version,memo_rule,memo_version FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?":"SELECT payee_rule,payee_version,memo_rule,memo_version FROM draft_rule_fields WHERE batch=? AND ordinal=? AND revision=?");marks.text(1,id.batch);marks.integer(2,integer(id.ordinal));marks.integer(3,revision);if(marks.row()){auto take=[&](int id_col,int ver_col)->std::optional<RuleMark>{if(marks.null(id_col)!=marks.null(ver_col))fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");if(marks.null(id_col))return {};return RuleMark{marks.bytes(id_col),marks.integer(ver_col)};};row.payee_rule=take(0,1);row.memo_rule=take(2,3);}accept_rule_owner(row.payee_owner,row.payee_rule,true);accept_rule_owner(row.memo_owner,row.memo_rule,true);}
    else {validate_owner(row.payee_owner);validate_owner(row.memo_owner);}
    auto allocations=row.transaction.allocations();Statement meta(db,entry?"SELECT position,identity,source_ordinal,category_id,catalog_revision,category_owner,memo_owner FROM entry_allocation_meta WHERE batch=? AND ordinal=? AND revision=? ORDER BY position":"SELECT position,identity,source_ordinal,category_id,catalog_revision,category_owner,memo_owner FROM draft_allocation_meta WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");meta.text(1,id.batch);meta.integer(2,integer(id.ordinal));meta.integer(3,revision);std::size_t count=0;
    while(meta.row()){if(count>=allocations.size() || meta.integer(0)!=integer(count))fail(SQLITE_CORRUPT);auto& allocation=allocations[count++];allocation.identity=meta.bytes(1);if(!meta.null(2))allocation.source_ordinal=static_cast<std::size_t>(meta.integer(2));if(!meta.null(3))allocation.category_binding=CategoryBinding{meta.bytes(3),meta.integer(4)};allocation.category_owner=static_cast<FieldOwnership>(meta.integer(5));allocation.memo_owner=static_cast<FieldOwnership>(meta.integer(6));if(!rules_format(db)){validate_owner(allocation.category_owner);validate_owner(allocation.memo_owner);}validate_binding(db,allocation);}
    if(count!=allocations.size())fail(SQLITE_CORRUPT);
    if(rules_format(db)){Statement marks(db,entry?"SELECT position,category_rule,category_version,memo_rule,memo_version FROM entry_rule_allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position":"SELECT position,category_rule,category_version,memo_rule,memo_version FROM draft_rule_allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");marks.text(1,id.batch);marks.integer(2,integer(id.ordinal));marks.integer(3,revision);while(marks.row()){const auto position=static_cast<std::size_t>(marks.integer(0));if(position>=allocations.size())fail(SQLITE_CORRUPT);auto& allocation=allocations[position];auto take=[&](int id_col,int ver_col)->std::optional<RuleMark>{if(marks.null(id_col)!=marks.null(ver_col))fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");if(marks.null(id_col))return {};return RuleMark{marks.bytes(id_col),marks.integer(ver_col)};};allocation.category_rule=take(1,2);allocation.memo_rule=take(3,4);}for(auto& allocation:allocations){accept_rule_owner(allocation.category_owner,allocation.category_rule,true);accept_rule_owner(allocation.memo_owner,allocation.memo_rule,true);}}
    row.transaction=row.transaction.corrected(row.transaction.effective_total(),std::move(allocations));
}
}
bool ImportStore::review_draft_undo(const imports::Occurrence& id,std::int64_t revision,const imports::DraftRow& draft) const {
    Statement predecessor(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=? AND revision<?");predecessor.text(1,id.batch);predecessor.integer(2,integer(id.ordinal));predecessor.integer(3,revision);
    if(!predecessor.row() || predecessor.null(0))return false;
    const auto previous=review_draft(id,predecessor.integer(0));
    if(auto* cache=impl_->audit_restores) {
        if(!cache->loaded){
            cache->loaded=true;cache->complete=true;std::size_t count=0,bytes=0;
            Statement all(impl_->db,"SELECT m.batch,m.ordinal,m.prior,m.result FROM review_members m JOIN review_undos u ON u.answer=m.answer WHERE m.committed=0");
            while(all.row()){
                auto batch=all.bytes(0);bytes+=batch.size()+128;
                if(++count>65536 || bytes>16*1024*1024){cache->rows.clear();cache->complete=false;break;}
                cache->rows[{std::move(batch),static_cast<std::size_t>(all.integer(1))}].emplace_back(all.integer(2),all.integer(3));
            }
        }
        if(cache->complete){
            const auto found=cache->rows.find(id);if(found==cache->rows.end())return false;
            for(const auto& [prior,result]:found->second)if(result<revision && same_draft(draft,review_draft(id,prior)) && same_draft(previous,review_draft(id,result)))return true;
            return false;
        }
    }
    Statement restored(impl_->db,"SELECT m.prior,m.result FROM review_members m JOIN review_undos u ON u.answer=m.answer WHERE m.batch=? AND m.ordinal=? AND m.committed=0 AND m.result<?");
    restored.text(1,id.batch);restored.integer(2,integer(id.ordinal));restored.integer(3,revision);
    // Like committed Review Undo, compare exact snapshots: a later group Undo
    // can restore the same result at a newer revision before this group is undone.
    while(restored.row())if(same_draft(draft,review_draft(id,restored.integer(0))) && same_draft(previous,review_draft(id,restored.integer(1))))return true;
    return false;
}
bool ImportStore::categories_supported() const{return categories_format(impl_->db);}
app::CategoryCatalog ImportStore::category_catalog() const {
    if(!categories_supported())fail(SQLITE_SCHEMA,"Categories are not supported by this dataset");
    app::CategoryCatalog result{catalog_sequence(impl_->db),{}};Statement ids(impl_->db,"SELECT h.id,v.catalog_revision FROM category_heads h LEFT JOIN category_versions v ON v.id=h.id AND v.revision=0 ORDER BY h.id");
    while(ids.row()) {
        if(ids.null(1))fail(SQLITE_CORRUPT,"Category creation history is missing");
        const auto id=ids.bytes(0);result.categories.push_back(category_at(impl_->db,id));result.creation_order.emplace(id,ids.integer(1));
    }
    return result;
}
app::CategoryDisplay ImportStore::resolve_category(std::string_view id) const {if(!categories_supported())fail(SQLITE_SCHEMA,"Categories are not supported by this dataset");return category_display(impl_->db,id);}
app::CategoryPreview ImportStore::preview_category_change(const app::CategoryRequest& request) const {
    const auto catalog=category_catalog();identifier(request.identity);std::optional<CategoryDefinition> before;
    if(request.income && request.action!=app::CategoryAction::set_income)fail(SQLITE_CONSTRAINT,"Income can only be set with the Income category control");
    auto found=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& definition){return definition.identity==request.identity;});
    if(request.action==app::CategoryAction::create){if(found!=catalog.categories.end())fail(SQLITE_CONSTRAINT,"Category identity already exists");}
    else {if(found==catalog.categories.end() || found->deleted)fail(SQLITE_CONSTRAINT,"Choose an existing category");before=*found;if(before->revision==INT64_MAX)fail(SQLITE_FULL);}
    CategoryDefinition after=before?*before:CategoryDefinition{request.identity,request.name,request.parent,request.kind,-1,false,false};++after.revision;
    switch(request.action){
    case app::CategoryAction::create:break;
    case app::CategoryAction::set_income:after.income=request.income;break;
    case app::CategoryAction::rename:after.name=request.name;break;
    case app::CategoryAction::move:fail(SQLITE_CONSTRAINT,"Rename the complete category name to change its stem");
    case app::CategoryAction::archive:after.archived=true;break;
    case app::CategoryAction::unarchive:after.archived=false;break;
    case app::CategoryAction::erase:{
        for(const auto& category:catalog.categories)if(category.parent==request.identity && !category.deleted)fail(SQLITE_CONSTRAINT,"Delete requires an empty category");
        if(category_referenced(impl_->db,request.identity))fail(SQLITE_CONSTRAINT,"Transactions use this category. Archive it instead.");
        after.deleted=true;break;}
    default:fail(SQLITE_CONSTRAINT,"Unknown category action");}
    after.name=canonical_category_name(after.name);if(after.kind!=CategoryKind::leaf || !after.parent.empty())fail(SQLITE_CONSTRAINT,"Create an assignable category using one optional colon, without a parent object");
    if(request.action==app::CategoryAction::create || request.action==app::CategoryAction::rename || request.action==app::CategoryAction::unarchive)
        for(const auto& alias:account_aliases())if(app::account_name_matches(alias.name,after.name))fail(SQLITE_CONSTRAINT,"That name belongs to an account or account alias. Select the account as a transfer destination instead.");
    if(!after.deleted)for(const auto& category:catalog.categories)if(!category.deleted && category.identity!=after.identity && app::category_name_matches(category.name,after.name))fail(SQLITE_CONSTRAINT,"That category name already exists (case-insensitive)");
    if(catalog.revision==INT64_MAX)fail(SQLITE_FULL);
    return {request,catalog.revision,before,after};
}
std::optional<app::CategoryReceipt> ImportStore::category_change_outcome(std::string_view operation) const {
    if(!categories_supported())fail(SQLITE_SCHEMA,"Categories are not supported by this dataset");
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    Statement row(impl_->db,"SELECT action,category,name,parent,kind,catalog_before,catalog_after,before_revision,after_revision,income FROM category_operations WHERE id=?");row.text(1,operation);if(!row.row()){Statement existing(impl_->db,"SELECT 1 FROM operation_registry WHERE id=?");existing.text(1,operation);if(existing.row())fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");return {};}
    std::optional<CategoryDefinition> before;if(!row.null(7)){before=category_at(impl_->db,row.bytes(1),row.integer(5));if(before->revision!=row.integer(7))fail(SQLITE_CORRUPT);}if(category_at(impl_->db,row.bytes(1),row.integer(6)).revision!=row.integer(8))fail(SQLITE_CORRUPT);return app::CategoryReceipt{std::string(operation),{static_cast<app::CategoryAction>(row.integer(0)),row.bytes(1),row.bytes(2),row.bytes(3),static_cast<CategoryKind>(row.integer(4)),row.integer(9)!=0},row.integer(5),row.integer(6),before,category_at(impl_->db,row.bytes(1),row.integer(6))};
}
app::CategoryReceipt ImportStore::apply_category_change(std::string_view operation,const app::CategoryRequest& request,std::int64_t expected) {
    identifier(operation);if(const auto prior=category_change_outcome(operation)){if(prior->request!=request || prior->catalog_before!=expected)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");return *prior;}
    const auto preview=preview_category_change(request);if(preview.catalog_revision!=expected)fail(SQLITE_CONSTRAINT,"Catalog changed; refresh before applying");Atomic transaction(impl_->db);if(catalog_sequence(impl_->db)!=expected)fail(SQLITE_CONSTRAINT,"Catalog changed; refresh before applying");
    const auto changes=sqlite3_total_changes64(impl_->db);
    const bool reviewsCurrent=impl_->review_changes==changes;
    const bool sheetCurrent=impl_->sheet_cache && impl_->sheet_changes==changes;
    write_category_change(operation,preview);
    if(request.action==app::CategoryAction::set_income) {
        // This flag cannot change import completion, matching, transfers or amounts.
        // Retain already-current Review facts, but advance mutation/Undo guards.
        exec(impl_->db,"UPDATE mutation_generation SET revision=revision+1 WHERE singleton=1");
        if(impl_->observer)impl_->observer(ImportPhase::before_commit);
        transaction.commit();
        const auto savedChanges=sqlite3_total_changes64(impl_->db);
        if(reviewsCurrent)impl_->review_changes=savedChanges;
        if(sheetCurrent) {
            impl_->sheet_cache->generation=ledger_edit_generation();impl_->sheet_cache->can_undo=false;
            impl_->sheet_changes=savedChanges;
        }
        if(impl_->observer)impl_->observer(ImportPhase::after_commit);
    } else finish([&]{transaction.commit();});
    return *category_change_outcome(operation);
}
void ImportStore::write_category_change(std::string_view operation,const app::CategoryPreview& preview) {
    const auto& request=preview.request;const auto expected=preview.catalog_revision;
    const auto& value=preview.after;Statement revision(impl_->db,"INSERT INTO category_versions VALUES(?,?,?,?,?,?,?,?,?)");revision.text(1,value.identity);revision.integer(2,value.revision);revision.integer(3,expected+1);revision.text(4,value.name);revision.text(5,value.parent);revision.integer(6,static_cast<int>(value.kind));revision.integer(7,value.archived?1:0);revision.integer(8,value.deleted?1:0);revision.integer(9,value.income);revision.done();
    Statement head(impl_->db,preview.before?"UPDATE category_heads SET revision=?2 WHERE id=?1":"INSERT INTO category_heads VALUES(?,?)");head.text(1,value.identity);head.integer(2,value.revision);head.done();
    Statement event(impl_->db,"INSERT INTO category_operations VALUES(?,?,?,?,?,?,?,?,?,?,?)");event.text(1,operation);event.integer(2,static_cast<int>(request.action));event.text(3,request.identity);event.text(4,request.name);event.text(5,request.parent);event.integer(6,static_cast<int>(request.kind));event.integer(7,expected);event.integer(8,expected+1);if(preview.before)event.integer(9,preview.before->revision);event.integer(10,value.revision);event.integer(11,request.income);event.done();
    Statement state(impl_->db,"UPDATE category_state SET revision=? WHERE singleton=1");state.integer(1,expected+1);state.done();
}
std::vector<app::CategoryReceipt> ImportStore::category_history(std::string_view id) const {if(!categories_supported())fail(SQLITE_SCHEMA,"Categories are not supported by this dataset");std::vector<app::CategoryReceipt> result;Statement operations(impl_->db,"SELECT id FROM category_operations WHERE category=? ORDER BY catalog_after");operations.text(1,id);while(operations.row())result.push_back(*category_change_outcome(operations.bytes(0)));return result;}
imports::DraftRow ImportStore::canonical_draft(const imports::Occurrence& id,const imports::DraftRow& input,const std::optional<imports::DraftRow>& prior,std::int64_t revision,bool entry,bool check_assignment) const {
    if(!categories_supported())return input;
    auto result=input;if(prior && !prior->remainder_allocation.empty())result.remainder_allocation=prior->remainder_allocation;if(prior && prior->manual_split_amounts)result.manual_split_amounts=true;const auto source=load_row(id,false).source;const bool rules=rules_format(impl_->db);
    struct Settled { FieldOwnership owner; std::optional<RuleMark> mark; };
    auto settle=[&](FieldOwnership supplied,const std::optional<RuleMark>& supplied_mark,FieldOwnership previous,const std::optional<RuleMark>& previous_mark,bool value_equal,bool source_equal,bool text_field=true)->Settled{
        // Explicit source ownership restores text only when it exactly matches retained import facts.
        if(text_field && supplied==FieldOwnership::source && (source_equal || previous==FieldOwnership::manual || previous==FieldOwnership::rule)){
            if(!source_equal || supplied_mark)fail(SQLITE_CONSTRAINT,"Restored text must match the imported value");
            return {FieldOwnership::source,{}};
        }
        if(!rules){if(supplied==FieldOwnership::rule)fail(SQLITE_CONSTRAINT,"Rule provenance is not supported yet");if(supplied!=FieldOwnership::untracked && supplied!=FieldOwnership::source && supplied!=FieldOwnership::manual)fail(SQLITE_CONSTRAINT);return {supplied==FieldOwnership::source && previous!=FieldOwnership::manual && source_equal?FieldOwnership::source:FieldOwnership::manual,{}};}
        if(supplied_mark && supplied!=FieldOwnership::rule)fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");
        if(supplied==FieldOwnership::rule){if(previous!=FieldOwnership::rule || !previous_mark || supplied_mark!=previous_mark || !value_equal)fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");return {FieldOwnership::rule,previous_mark};}
        if(previous==FieldOwnership::rule && supplied==FieldOwnership::source)fail(SQLITE_CONSTRAINT,"Ordinary save cannot replace rule ownership with source");
        if(supplied!=FieldOwnership::untracked && supplied!=FieldOwnership::source && supplied!=FieldOwnership::manual)fail(SQLITE_CONSTRAINT);
        const bool keep_source=supplied==FieldOwnership::source && previous!=FieldOwnership::manual && previous!=FieldOwnership::rule && source_equal;
        return {keep_source?FieldOwnership::source:FieldOwnership::manual,{}};
    };
    if(source.format=="manual" && (input.payee_owner==FieldOwnership::source || input.memo_owner==FieldOwnership::source))fail(SQLITE_CONSTRAINT,"This transaction was entered manually");
    const auto payee=settle(input.payee_owner,rules?input.payee_rule:std::optional<RuleMark>{},prior?prior->payee_owner:FieldOwnership::source,prior?prior->payee_rule:std::optional<RuleMark>{},prior && input.payee==prior->payee,input.payee==source.payee);
    const auto memo=settle(input.memo_owner,rules?input.memo_rule:std::optional<RuleMark>{},prior?prior->memo_owner:FieldOwnership::source,prior?prior->memo_rule:std::optional<RuleMark>{},prior && input.memo==prior->memo,input.memo==source.memo);
    result.payee_owner=payee.owner;result.memo_owner=memo.owner;if(rules){result.payee_rule=payee.mark;result.memo_rule=memo.mark;}
    auto allocations=input.transaction.allocations();std::set<std::string> identities;std::set<std::size_t> sources;
    for(std::size_t position=0;position<allocations.size();++position){auto& allocation=allocations[position];if(allocation.identity.empty()){if(allocation.source_ordinal)fail(SQLITE_CONSTRAINT,"Source association requires its retained identity");allocation.identity=std::string(entry?"manual-e-":"manual-d-")+std::to_string(revision)+"-"+std::to_string(position);allocation.source_ordinal.reset();}
        identifier(allocation.identity);if(!allocation.source_ordinal && allocation.identity.starts_with("source-"))fail(SQLITE_CONSTRAINT,"Source identity requires its retained association");if(!identities.insert(allocation.identity).second || (allocation.source_ordinal && !sources.insert(*allocation.source_ordinal).second))fail(SQLITE_CONSTRAINT,"Duplicate allocation/source identity");
        const Allocation* previous=nullptr;if(prior)for(const auto& candidate:prior->transaction.allocations())if(candidate.identity==allocation.identity)previous=&candidate;
        if(allocation.source_ordinal){const auto ordinal=*allocation.source_ordinal;if(ordinal>=source.source_allocation_fields.size() || allocation.identity!="source-"+std::to_string(ordinal) || (prior && (!previous || previous->source_ordinal!=allocation.source_ordinal)))fail(SQLITE_CONSTRAINT,"Source association cannot be invented or rebound");}
        if(previous && previous->source_ordinal!=allocation.source_ordinal)fail(SQLITE_CONSTRAINT,"Source association cannot be removed or rebound");
        Statement old(impl_->db,"SELECT source_ordinal FROM draft_allocation_meta WHERE batch=?1 AND ordinal=?2 AND identity=?3 UNION ALL SELECT source_ordinal FROM entry_allocation_meta WHERE batch=?1 AND ordinal=?2 AND identity=?3");old.text(1,id.batch);old.integer(2,integer(id.ordinal));old.text(3,allocation.identity);while(old.row())if(old.null(0)!=!allocation.source_ordinal || (!old.null(0) && old.integer(0)!=integer(*allocation.source_ordinal)))fail(SQLITE_CONSTRAINT,"Allocation identity was rebound");
        const qif::SourceAllocation* original=allocation.source_ordinal?&source.source_allocation_fields[*allocation.source_ordinal]:nullptr;
        if(check_assignment && resolve_account_name(allocation.category) &&
           !(previous && previous->category==allocation.category) && !(original && original->category==allocation.category))
            fail(SQLITE_CONSTRAINT,"An account name is a transfer destination. Use the account transfer command rather than saving ordinary category text.");
        const auto memo_owner=settle(allocation.memo_owner,rules?allocation.memo_rule:std::optional<RuleMark>{},previous?previous->memo_owner:FieldOwnership::source,previous?previous->memo_rule:std::optional<RuleMark>{},previous && allocation.memo==previous->memo,original && allocation.memo==original->memo);
        const auto category_owner=settle(allocation.category_owner,rules?allocation.category_rule:std::optional<RuleMark>{},previous?previous->category_owner:FieldOwnership::source,previous?previous->category_rule:std::optional<RuleMark>{},previous && allocation.category==previous->category && allocation.category_binding==previous->category_binding,source_category_equal(allocation,original),false);
        allocation.memo_owner=memo_owner.owner;allocation.category_owner=category_owner.owner;if(rules){allocation.memo_rule=memo_owner.mark;allocation.category_rule=category_owner.mark;}
        if(allocation.category_binding){validate_binding(impl_->db,allocation);const bool unchanged=previous && previous->category_binding==allocation.category_binding && previous->category==allocation.category;
            if(check_assignment && !unchanged){const auto display=resolve_category(allocation.category_binding->identity);if(allocation.category_binding->catalog_revision!=catalog_sequence(impl_->db) || !display.assignable || display.path!=allocation.category)fail(SQLITE_CONSTRAINT,"This category changed or was archived. Choose an available category.");}
        }
    }
    if(!result.remainder_allocation.empty()){
        identifier(result.remainder_allocation);
        const bool was_present=prior && std::any_of(prior->transaction.allocations().begin(),prior->transaction.allocations().end(),[&](const auto& a){return a.identity==result.remainder_allocation;});
        if((!prior || prior->remainder_allocation.empty()) && !identities.contains(result.remainder_allocation) && !was_present)fail(SQLITE_CONSTRAINT,"Balancing split is missing");
    }
    const auto effective=input.transaction.effective_total();bool reassigned=input.transaction.reassigned_currency();
    if(source.source_currency && effective.currency()!=*source.source_currency){if(!reassignment_authorizes(impl_->db,account_for_batch(id.batch).identity,effective.currency(),*source.source_currency))fail(SQLITE_CONSTRAINT,"Currency reassignment is not in effect");reassigned=true;}
    result.transaction=Transaction::repaired_import(input.transaction.original_imported_amount(),effective,std::move(allocations),reassigned);return prior?result:resolve_repaired_categories(id.batch,result);
}
void ImportStore::verify_categories(const app::OperationContext* context) const {
    if(context)context->checkpoint("Verifying category history");
    const auto catalog=category_catalog();std::map<std::string,CategoryDefinition> reconstructed;std::int64_t previous=0,previous_order=0;
    Statement operations(impl_->db,"SELECT id FROM category_operations ORDER BY catalog_after");
    while(operations.row()){
        if(context)context->checkpoint();
        const auto receipt=*category_change_outcome(operations.bytes(0));if(receipt.catalog_before!=previous || receipt.catalog_after!=previous+1)fail(SQLITE_CORRUPT);
        Statement ordering(impl_->db,"SELECT sequence FROM history_order WHERE category_operation=?");ordering.text(1,receipt.operation);if(!ordering.row() || ordering.integer(0)<=previous_order)fail(SQLITE_CORRUPT);previous_order=ordering.integer(0);
        const auto& request=receipt.request;if(request.income && request.action!=app::CategoryAction::set_income)fail(SQLITE_CORRUPT);auto found=reconstructed.find(request.identity);CategoryDefinition expected=receipt.after;
        if(request.action==app::CategoryAction::create){if(found!=reconstructed.end() || receipt.before || receipt.after.revision!=0)fail(SQLITE_CORRUPT);expected={request.identity,request.name,request.parent,request.kind,0,false,false};}
        else {if(found==reconstructed.end() || found->second.deleted || !receipt.before || *receipt.before!=found->second)fail(SQLITE_CORRUPT);expected=found->second;++expected.revision;
            switch(request.action){case app::CategoryAction::set_income:expected.income=request.income;break;case app::CategoryAction::rename:expected.name=request.name;break;case app::CategoryAction::move:fail(SQLITE_CORRUPT);case app::CategoryAction::archive:expected.archived=true;break;case app::CategoryAction::unarchive:expected.archived=false;break;case app::CategoryAction::erase:expected.deleted=true;break;default:fail(SQLITE_CORRUPT);}
        }
        expected.name=canonical_category_name(expected.name);
        if(expected!=receipt.after)fail(SQLITE_CORRUPT);
        identifier(expected.identity);validate_category_name(expected.name);
        if(expected.kind!=CategoryKind::leaf || !expected.parent.empty())fail(SQLITE_CORRUPT);
        if(!expected.parent.empty()){const auto parent=reconstructed.find(expected.parent);if(expected.kind!=CategoryKind::leaf || parent==reconstructed.end() || parent->second.deleted || parent->second.kind!=CategoryKind::group)fail(SQLITE_CORRUPT);}
        for(const auto& [id,definition]:reconstructed){if(expected.deleted && !definition.deleted && definition.parent==expected.identity)fail(SQLITE_CORRUPT);if(!expected.deleted && !definition.deleted && id!=expected.identity && app::category_name_matches(definition.name,expected.name))fail(SQLITE_CORRUPT);}
        if(expected.deleted && category_referenced(impl_->db,expected.identity))fail(SQLITE_CORRUPT);
        reconstructed.insert_or_assign(expected.identity,expected);previous=receipt.catalog_after;
    }
    if(previous!=catalog.revision || reconstructed.size()!=catalog.categories.size())fail(SQLITE_CORRUPT);
    for(const auto& definition:catalog.categories){const auto found=reconstructed.find(definition.identity);if(found==reconstructed.end() || found->second!=definition)fail(SQLITE_CORRUPT);Statement head(impl_->db,"SELECT revision FROM category_heads WHERE id=?");head.text(1,definition.identity);if(!head.row() || head.integer(0)!=definition.revision)fail(SQLITE_CORRUPT);}
    if(rules_format(impl_->db)){
        Statement count(impl_->db,"SELECT (SELECT count(*) FROM category_versions),(SELECT count(*) FROM operation_registry),(SELECT count(*) FROM ledger_operations)+(SELECT count(*) FROM category_operations)+(SELECT count(*) FROM rule_operations)+(SELECT count(*) FROM rule_applications)+(SELECT count(*) FROM account_operations)+(SELECT count(*) FROM dataset_acceptances)+(SELECT count(*) FROM link_exclusion_events),(SELECT count(*) FROM draft_fields),(SELECT count(*) FROM versions),(SELECT count(*) FROM entry_fields),(SELECT count(*) FROM entry_values),(SELECT count(*) FROM draft_allocation_meta),(SELECT count(*) FROM allocations),(SELECT count(*) FROM entry_allocation_meta),(SELECT count(*) FROM entry_allocation_values)");
        if(!count.row() || count.integer(0)!=catalog.revision || count.integer(1)!=count.integer(2) || count.integer(3)!=count.integer(4) || count.integer(5)!=count.integer(6) || count.integer(7)!=count.integer(8) || count.integer(9)!=count.integer(10))fail(SQLITE_CORRUPT);
        Statement registry(impl_->db,"SELECT 1 FROM operation_registry r WHERE (domain=0 AND NOT EXISTS(SELECT 1 FROM ledger_operations o WHERE o.id=r.id)) OR (domain=1 AND NOT EXISTS(SELECT 1 FROM category_operations o WHERE o.id=r.id)) OR (domain=2 AND NOT EXISTS(SELECT 1 FROM rule_operations o WHERE o.id=r.id)) OR (domain=3 AND NOT EXISTS(SELECT 1 FROM rule_applications o WHERE o.id=r.id)) OR (domain=4 AND NOT EXISTS(SELECT 1 FROM account_operations o WHERE o.id=r.id)) OR (domain=5 AND NOT EXISTS(SELECT 1 FROM dataset_acceptances o WHERE o.id=r.id)) OR (domain=6 AND NOT EXISTS(SELECT 1 FROM link_exclusion_events o WHERE o.operation=r.id))");if(registry.row())fail(SQLITE_CORRUPT);
    }else{
        Statement count(impl_->db,"SELECT (SELECT count(*) FROM category_versions),(SELECT count(*) FROM operation_registry),(SELECT count(*) FROM ledger_operations)+(SELECT count(*) FROM category_operations)+(SELECT count(*) FROM account_operations)+(SELECT count(*) FROM dataset_acceptances)+(SELECT count(*) FROM link_exclusion_events),(SELECT count(*) FROM draft_fields),(SELECT count(*) FROM versions),(SELECT count(*) FROM entry_fields),(SELECT count(*) FROM entry_values),(SELECT count(*) FROM draft_allocation_meta),(SELECT count(*) FROM allocations),(SELECT count(*) FROM entry_allocation_meta),(SELECT count(*) FROM entry_allocation_values)");
        if(!count.row() || count.integer(0)!=catalog.revision || count.integer(1)!=count.integer(2) || count.integer(3)!=count.integer(4) || count.integer(5)!=count.integer(6) || count.integer(7)!=count.integer(8) || count.integer(9)!=count.integer(10))fail(SQLITE_CORRUPT);
        Statement registry(impl_->db,"SELECT 1 FROM operation_registry r WHERE (domain=0 AND NOT EXISTS(SELECT 1 FROM ledger_operations o WHERE o.id=r.id)) OR (domain=1 AND NOT EXISTS(SELECT 1 FROM category_operations o WHERE o.id=r.id)) OR (domain=4 AND NOT EXISTS(SELECT 1 FROM account_operations o WHERE o.id=r.id)) OR (domain=5 AND NOT EXISTS(SELECT 1 FROM dataset_acceptances o WHERE o.id=r.id)) OR (domain=6 AND NOT EXISTS(SELECT 1 FROM link_exclusion_events o WHERE o.operation=r.id))");if(registry.row())fail(SQLITE_CORRUPT);
    }
    for(bool entry:{false,true}){
        Statement rows(impl_->db,entry?"SELECT batch,ordinal,revision FROM entry_values ORDER BY batch,ordinal,revision":"SELECT batch,ordinal,revision FROM versions ORDER BY batch,ordinal,revision");
        while(rows.row()){
            if(context)context->checkpoint();
            const imports::Occurrence id{rows.bytes(0),static_cast<std::size_t>(rows.integer(1))};const auto revision=rows.integer(2);
            const auto head=load_row(id,false);const auto& source=head.source;
            // Reuse the validated head; the historical version below still gets
            // its own fields, allocations, ownership and provenance checks.
            imports::DraftRow draft=entry?entry_snapshot(id,revision).draft:*head.draft;
            if(!entry){ // Load an arbitrary historical draft revision, not only its head.
                Statement fields(impl_->db,"SELECT date,payee,memo,total FROM versions WHERE batch=? AND ordinal=? AND revision=?");fields.text(1,id.batch);fields.integer(2,integer(id.ordinal));fields.integer(3,revision);if(!fields.row())fail(SQLITE_CORRUPT);
                const auto currency=version_currency(impl_->db,id.batch,integer(id.ordinal),revision);const bool reassigned=source.source_currency && *source.source_currency!=currency;
                std::vector<Allocation> aa;Statement values(impl_->db,"SELECT category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");values.text(1,id.batch);values.integer(2,integer(id.ordinal));values.integer(3,revision);while(values.row())aa.push_back({values.bytes(0),values.bytes(1),Money(values.integer(2),currency)});
                draft={fields.bytes(0),fields.bytes(1),fields.bytes(2),Transaction::repaired_import(source.original_amount,Money(fields.integer(3),currency),std::move(aa),reassigned)};load_metadata(impl_->db,id,revision,draft,false);
            }
            if((draft.payee_owner==FieldOwnership::source && draft.payee!=source.payee) || (draft.memo_owner==FieldOwnership::source && draft.memo!=source.memo))fail(SQLITE_CORRUPT);
            std::int64_t order=0;bool undo=false;std::optional<std::int64_t> prior_revision;
            if(entry){Statement operation(impl_->db,"SELECT h.sequence,o.kind FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.batch=? AND ((?=0 AND o.kind=0) OR (o.ordinal=? AND o.result_revision=? AND o.kind!=0))");operation.text(1,id.batch);operation.integer(2,revision);operation.integer(3,integer(id.ordinal));operation.integer(4,revision);if(operation.row()){order=operation.integer(0);undo=operation.integer(1)==static_cast<int>(app::ChangeKind::undo);}else{Statement mark(impl_->db,"SELECT operation FROM account_amount_revisions WHERE kind=2 AND batch=? AND ordinal=? AND revision=? AND position=0");mark.text(1,id.batch);mark.integer(2,integer(id.ordinal));mark.integer(3,revision);if(!mark.row())fail(SQLITE_CORRUPT);Statement ledger(impl_->db,"SELECT h.sequence FROM ledger_operations l JOIN history_order h ON h.operation=l.id WHERE l.kind=5 AND l.undo_of=?");ledger.text(1,mark.bytes(0));if(!ledger.row())fail(SQLITE_CORRUPT);order=ledger.integer(0);undo=false;}if(revision>0)prior_revision=revision-1;}
            else {Statement event(impl_->db,"SELECT sequence FROM history_order WHERE batch=? AND event_revision=?");event.text(1,id.batch);event.integer(2,revision);if(!event.row())fail(SQLITE_CORRUPT);order=event.integer(0);Statement prior(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=? AND revision<?");prior.text(1,id.batch);prior.integer(2,integer(id.ordinal));prior.integer(3,revision);if(!prior.row())fail(SQLITE_CORRUPT);if(!prior.null(0))prior_revision=prior.integer(0);}
            // Pending Review Undo restores the exact saved draft, including
            // field ownership. Require its receipt, exact predecessor snapshot and
            // original snapshot; source-owned text must exactly match imported facts.
            if(!entry && prior_revision)undo=review_draft_undo(id,revision,draft);
            if(prior_revision && !undo){
                Statement prior_mode(impl_->db,entry?"SELECT manual_split_amounts,remainder_allocation FROM entry_fields WHERE batch=? AND ordinal=? AND revision=?":"SELECT manual_split_amounts,remainder_allocation FROM draft_fields WHERE batch=? AND ordinal=? AND revision=?");
                prior_mode.text(1,id.batch);prior_mode.integer(2,integer(id.ordinal));prior_mode.integer(3,*prior_revision);
                if(!prior_mode.row() || (prior_mode.integer(0)!=0 && !draft.manual_split_amounts))fail(SQLITE_CORRUPT,"Manual split amount editing mode was removed without Undo");
                if(!prior_mode.bytes(1).empty() && prior_mode.bytes(1)!=draft.remainder_allocation)fail(SQLITE_CORRUPT,"Balancing split changed without Undo");
            }
            Statement epoch(impl_->db,"SELECT coalesce(max(o.catalog_after),0) FROM category_operations o CROSS JOIN history_order h ON h.category_operation=o.id WHERE h.sequence<?");epoch.integer(1,order);if(!epoch.row())fail(SQLITE_CORRUPT);const auto current_catalog=epoch.integer(0);
            if(source.format=="manual") {
                if(draft.payee_owner==FieldOwnership::source || draft.memo_owner==FieldOwnership::source)fail(SQLITE_CORRUPT,"Manual text has imported ownership");
                for(const auto& a:draft.transaction.allocations())if(a.source_ordinal || a.category_owner==FieldOwnership::source || a.memo_owner==FieldOwnership::source)fail(SQLITE_CORRUPT,"Manual split has imported ownership");
                if(!entry && !prior_revision && manual_codec::encode(draft)!=evidence(id.batch))fail(SQLITE_CORRUPT,"Manual creation does not match its journal");
            }
            if(!entry && !prior_revision && source.transaction && source.format!="manual"){Statement initial(impl_->db,"SELECT materialized_revision FROM batches WHERE id=?");initial.text(1,id.batch);if(!initial.row())fail(SQLITE_CORRUPT);if(!initial.null(0) && revision==initial.integer(0)){if(draft.payee_owner!=FieldOwnership::source || draft.memo_owner!=FieldOwnership::source || draft.transaction.allocations().size()!=source.source_allocations.size())fail(SQLITE_CORRUPT,"Initial source ownership is inconsistent");for(std::size_t i=0;i<draft.transaction.allocations().size();++i){const auto& a=draft.transaction.allocations()[i];if(a.identity!=source.source_allocations[i].identity || a.source_ordinal!=source.source_allocations[i].source_ordinal || a.category_owner!=FieldOwnership::source || a.memo_owner!=FieldOwnership::source || a.category_binding)fail(SQLITE_CORRUPT,"Initial source association is inconsistent");}}}
            std::set<std::string> identities;std::set<std::size_t> sources;
            for(const auto& allocation:draft.transaction.allocations()){
                identifier(allocation.identity);if(!identities.insert(allocation.identity).second || (allocation.source_ordinal && !sources.insert(*allocation.source_ordinal).second))fail(SQLITE_CORRUPT);
                const qif::SourceAllocation* original=nullptr;if(allocation.source_ordinal){if(*allocation.source_ordinal>=source.source_allocation_fields.size() || allocation.identity!="source-"+std::to_string(*allocation.source_ordinal))fail(SQLITE_CORRUPT);original=&source.source_allocation_fields[*allocation.source_ordinal];}
                else if(allocation.identity.starts_with("source-"))fail(SQLITE_CORRUPT);
                if((allocation.memo_owner==FieldOwnership::source && (!original || allocation.memo!=original->memo)) || (allocation.category_owner==FieldOwnership::source && !source_category_equal(allocation,original)))fail(SQLITE_CORRUPT);
                Statement associations(impl_->db,"SELECT source_ordinal FROM draft_allocation_meta WHERE batch=?1 AND ordinal=?2 AND identity=?3 UNION ALL SELECT source_ordinal FROM entry_allocation_meta WHERE batch=?1 AND ordinal=?2 AND identity=?3");associations.text(1,id.batch);associations.integer(2,integer(id.ordinal));associations.text(3,allocation.identity);while(associations.row())if(associations.null(0)!=!allocation.source_ordinal || (!associations.null(0) && associations.integer(0)!=integer(*allocation.source_ordinal)))fail(SQLITE_CORRUPT);
                bool carried=false,previous_identity=false;
                const bool prior_entry=entry && revision>0;
                std::optional<std::int64_t> previous_revision=prior_revision;
                if(entry && revision==0){Statement prior(impl_->db,"SELECT max(revision) FROM versions WHERE batch=? AND ordinal=?");prior.text(1,id.batch);prior.integer(2,integer(id.ordinal));if(!prior.row() || prior.null(0))fail(SQLITE_CORRUPT);previous_revision=prior.integer(0);}
                if(previous_revision){Statement prior_allocation(impl_->db,prior_entry?"SELECT m.category_id,m.catalog_revision,a.category,m.category_owner,m.memo_owner FROM entry_allocation_meta m JOIN entry_allocation_values a USING(batch,ordinal,revision,position) WHERE m.batch=? AND m.ordinal=? AND m.revision=? AND m.identity=?":"SELECT m.category_id,m.catalog_revision,a.category,m.category_owner,m.memo_owner FROM draft_allocation_meta m JOIN allocations a USING(batch,ordinal,revision,position) WHERE m.batch=? AND m.ordinal=? AND m.revision=? AND m.identity=?");prior_allocation.text(1,id.batch);prior_allocation.integer(2,integer(id.ordinal));prior_allocation.integer(3,*previous_revision);prior_allocation.text(4,allocation.identity);if(prior_allocation.row()){if(!undo && prior_allocation.integer(3)==static_cast<int>(FieldOwnership::manual) && allocation.category_owner==FieldOwnership::source)fail(SQLITE_CORRUPT,"Manual allocation ownership was silently removed");previous_identity=true;carried=allocation.category_binding && !prior_allocation.null(0) && prior_allocation.bytes(0)==allocation.category_binding->identity && prior_allocation.integer(1)==allocation.category_binding->catalog_revision && prior_allocation.bytes(2)==allocation.category;}}
                if(allocation.source_ordinal && previous_revision && !previous_identity && !undo)fail(SQLITE_CORRUPT,"Source association was recreated without Undo");
                if(allocation.category_binding){validate_binding(impl_->db,allocation);if(allocation.category_binding->catalog_revision>current_catalog || (!carried && !undo && allocation.category_owner!=FieldOwnership::source && allocation.category_binding->catalog_revision!=current_catalog))fail(SQLITE_CORRUPT,"Category assignment bypassed catalog freshness");}
            }
        }
    }
}

void ImportStore::initialize_categories() {
    const std::vector<std::string> names{
        "Amazon",
        "Auto: Gas",
        "Auto: Insurance",
        "Auto: Maintenance",
        "Auto: Other",
        "Auto: Parking",
        "Child Expenses",
        "Childcare",
        "Dining & Restaurants",
        "Education",
        "Electronics",
        "Entertainment",
        "Fast Food & Coffee",
        "Fees, Interest & Bank Charges",
        "Gifts & Donations",
        "Groceries",
        "Gym & Fitness",
        "Hobbies",
        "Home: Furniture & Appliances",
        "Home: Improvement",
        "Home: Insurance",
        "Home: Maintenance",
        "Home: Mortgage",
        "Home: Rent",
        "Income: Investment Income",
        "Income: Pension",
        "Income: Salaries & Wages",
        "Medical: Doctor & Hospital",
        "Medical: Pharmacy",
        "Misc Expenses",
        "Personal Care",
        "Pets",
        "Shoes & Clothing",
        "Taxes: Housing",
        "Taxes: Income",
        "Transport: Rideshare & Taxi",
        "Transport: Transit",
        "Travel: Flights",
        "Travel: Hotels & Lodging",
        "Travel: Insurance",
        "Travel: Other",
        "Utils: Electricity, Gas & Heating",
        "Utils: Internet",
        "Utils: Phones",
        "Utils: TV & Streaming",
        "Utils: Water, Sewer & Garbage"
    };
    for(std::size_t i=0;i<names.size();++i){
        const auto id="starter-"+std::to_string(i+1);
        const auto preview=preview_category_change({app::CategoryAction::create,id,names[i],{},CategoryKind::leaf});
        write_category_change("setup-"+id,preview);
    }
}

// The immutable encrypted whole source is the category staging store. It never
// enters category_heads until the group's first financial publication.
const std::vector<qif::CategoryDefinition>& ImportStore::source_categories(std::string_view batch_id) const {
    Statement source(impl_->db,"SELECT b.source_id,h.format FROM batches b JOIN source_headers h ON h.source_id=b.source_id WHERE b.id=?");source.text(1,batch_id);if(!source.row())fail(SQLITE_CORRUPT);
    const auto id=source.bytes(0);
    if(impl_->category_source!=id) {
        std::vector<qif::CategoryDefinition> categories;
        if(source.bytes(1)=="qif") {
            Statement bytes(impl_->db,"SELECT bytes FROM sources WHERE id=?");bytes.text(1,id);if(!bytes.row())fail(SQLITE_CORRUPT);
            categories=qif::index_document(bytes.bytes(0)).categories;
        }
        impl_->source_category_cache=std::move(categories);impl_->category_source=id;
    }
    return impl_->source_category_cache;
}
void ImportStore::publish_import_categories(std::string_view batch_id) {
    Statement resolved(impl_->db,"SELECT 1 FROM import_category_decisions d JOIN batches b ON b.source_id=d.source WHERE b.id=? AND d.resolved=1");resolved.text(1,batch_id);if(resolved.row())return;
    const auto definitions=source_categories(batch_id);
    for(std::size_t i=0;i<definitions.size();++i) {
        if(resolve_account_name(definitions[i].name))continue; // Existing account; no ordinary category is created.
        const auto name=canonical_category_name(definitions[i].name);
        const auto catalog=category_catalog();
        const auto found=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && app::category_name_matches(c.name,name);});
        if(found!=catalog.categories.end()) {
            if(found->archived)fail(SQLITE_CONSTRAINT,"An imported category matches an archived category. Reactivate that category before committing the import.");
            continue;
        }
        const auto id="qif-cat-"+impl_->category_source+"-"+std::to_string(i);
        identifier(id);
        write_category_change(id,preview_category_change({app::CategoryAction::create,id,name,{},CategoryKind::leaf}));
    }
}
imports::DraftRow ImportStore::bind_import_categories(std::string_view batch_id,const imports::DraftRow& input) const {
    Statement resolved(impl_->db,"SELECT 1 FROM import_category_decisions d JOIN batches b ON b.source_id=d.source WHERE b.id=? AND d.resolved=1");resolved.text(1,batch_id);if(resolved.row())return input;
    const auto& definitions=source_categories(batch_id);
    if(definitions.empty())return input;
    Statement group(impl_->db,"SELECT g.id,g.state,l.catalog_revision FROM import_groups g JOIN import_group_accounts m ON m.group_id=g.id LEFT JOIN ledger_operations l ON l.id=g.publication WHERE m.batch=?");group.text(1,batch_id);
    if(!group.row() || group.integer(1)!=2) return input;
    // During publication the financial receipt follows entry creation inside the
    // same transaction; after it, its immutable epoch pins category identities.
    if(group.null(2) && sqlite3_get_autocommit(impl_->db))fail(SQLITE_CORRUPT,"Missing category publication receipt");
    const auto epoch=group.null(2)?catalog_sequence(impl_->db):group.integer(2);
    if(impl_->binding_group!=group.bytes(0) || impl_->binding_epoch!=epoch) {
        std::vector<CategoryDefinition> catalog;
        Statement ids(impl_->db,"SELECT DISTINCT id FROM category_versions WHERE catalog_revision<=?");ids.integer(1,epoch);
        while(ids.row())catalog.push_back(category_at(impl_->db,ids.bytes(0),epoch));
        std::map<std::string,std::pair<std::string,CategoryBinding>> bindings;
        for(const auto& definition:definitions) {
            const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& c){return !c.deleted && !c.archived && app::category_name_matches(c.name,definition.name);});
            if(found==catalog.end() && resolve_account_name(definition.name))continue;
            if(found==catalog.end())fail(SQLITE_CORRUPT,"Imported category catalog was not published completely");
            bindings.emplace(definition.name,std::make_pair(found->name,CategoryBinding{found->identity,epoch}));
        }
        impl_->imported_bindings=std::move(bindings);impl_->binding_group=group.bytes(0);impl_->binding_epoch=epoch;
    }
    auto result=input;auto allocations=input.transaction.allocations();
    for(auto& allocation:allocations) {
        if(allocation.category_owner!=FieldOwnership::source || allocation.category_binding || allocation.category.empty() || qif::transfer_destination(allocation.category))continue;
        auto found=impl_->imported_bindings.find(allocation.category);
        if(found==impl_->imported_bindings.end()) {
            // Undeclared source labels may not satisfy editable-name syntax.
            // Preserve them as unmapped evidence rather than invent a category.
            try {(void)canonical_category_name(allocation.category);}catch(const std::invalid_argument&){continue;}
            // Match only catalog identity, never similar spelling or a prefix.
            found=std::find_if(impl_->imported_bindings.begin(),impl_->imported_bindings.end(),[&](const auto& pair){return app::category_name_matches(pair.first,allocation.category);});
        }
        if(found!=impl_->imported_bindings.end()){allocation.category=found->second.first;allocation.category_binding=found->second.second;}
    }
    result.transaction=input.transaction.corrected(input.transaction.effective_total(),std::move(allocations));return result;
}
