#ifdef SPENTIT_RULES_STORAGE
namespace {
constexpr const char* refresh_rules="Rules, categories, or the import changed; refresh before applying";
constexpr const char* rules_dataset="Rules are not enabled for this dataset";
constexpr const char* other_command="Operation identity belongs to another command";
constexpr const char* reused_command="Operation identity was reused with different content";
constexpr const char* rules_drift="Rule result changed money, dates, allocations, or the decision";

void validate_label(std::string_view label) {
    if(label.size()>256 || !qif::valid_utf8(label)) fail(SQLITE_CONSTRAINT,"Invalid rule label");
    for(std::size_t i=0;i<label.size();) {
        const auto lead=static_cast<unsigned char>(label[i]);
        char32_t code=lead; std::size_t width=1;
        if(lead>=0x80U) {
            if((lead & 0xE0U)==0xC0U){code=lead & 0x1FU;width=2U;}
            else if((lead & 0xF0U)==0xE0U){code=lead & 0x0FU;width=3U;}
            else {code=lead & 0x07U;width=4U;}
            for(std::size_t extra=1;extra<width;++extra) code=(code<<6) | static_cast<char32_t>(static_cast<unsigned char>(label[i+extra]) & 0x3FU);
        }
        if(code<=0x1F || code==0x7F || (code>=0x80 && code<=0x9F)) fail(SQLITE_CONSTRAINT,"Invalid rule label");
        i+=width;
    }
}
void require_rule_category(sqlite3* db,const std::optional<std::string>& category,bool enabled) {
    if(!category) return;
    identifier(*category);
    CategoryDefinition definition;
    try { definition=category_at(db,*category); }
    catch(const ImportError& error) { if(error.code()==SQLITE_NOTFOUND) fail(SQLITE_CONSTRAINT,"Rule category must identify an assignable category"); throw; }
    if(definition.deleted || definition.kind!=CategoryKind::leaf) fail(SQLITE_CONSTRAINT,"Rule category must identify an assignable category");
    if(enabled && !category_display(db,*category).assignable) throw rules::RegexError(rules::RegexFailure::invalid_input,"Enabled rule targets an unavailable category");
}
bool command_taken(sqlite3* db,std::string_view operation) {
    Statement other(db,"SELECT 1 FROM operation_registry WHERE id=? UNION ALL SELECT 1 FROM operations WHERE id=? UNION ALL SELECT 1 FROM ledger_operations WHERE id=? UNION ALL SELECT 1 FROM category_operations WHERE id=? UNION ALL SELECT 1 FROM rule_operations WHERE id=? UNION ALL SELECT 1 FROM rule_applications WHERE id=?");
    for(int index=1;index<=6;++index) other.text(index,operation);
    return other.row();
}
std::int64_t ruleset_revision(sqlite3* db){Statement state(db,"SELECT revision FROM rule_state WHERE singleton=1");if(!state.row())fail(SQLITE_CORRUPT);return state.integer(0);}
std::optional<std::string> optional_text(Statement& row,int column){if(row.null(column))return {};return row.bytes(column);}
bool same_optional(Statement& row,int column,const std::optional<std::string>& value){if(row.null(column))return !value;return value && row.bytes(column)==*value;}
struct RuleBody {
    std::string label,expression;
    rules::Scope scope=rules::Scope::transaction;
    rules::InputField input=rules::InputField::payee;
    std::optional<std::string> payee,memo,category;
    bool inherit=false;
    bool operator==(const RuleBody&) const=default;
};
RuleBody body_of(const app::RuleDefinitionImage& image){return {image.label,image.expression,image.scope,image.input,image.actions.payee_template,image.actions.memo_template,image.actions.category,image.actions.inherit_parent};}
bool same_content(const RuleBody& left,const RuleBody& right){return left==right;}
rules::Rule rule_from(const app::RuleDefinitionImage& image,std::int64_t version){rules::Rule rule;rule.identity=image.identity;rule.version=version;rule.enabled=image.enabled;rule.scope=image.scope;rule.input=image.input;rule.expression=image.expression;rule.actions=image.actions;return rule;}
rules::Rule rule_from_row(Statement& row,int id_column,std::int64_t version,bool enabled){
    rules::Rule rule;rule.identity=row.bytes(id_column);rule.version=version;rule.enabled=enabled;rule.scope=static_cast<rules::Scope>(row.integer(id_column+2));rule.input=static_cast<rules::InputField>(row.integer(id_column+3));rule.expression=row.bytes(id_column+4);rule.actions.payee_template=optional_text(row,id_column+5);rule.actions.memo_template=optional_text(row,id_column+6);rule.actions.category=optional_text(row,id_column+7);rule.actions.inherit_parent=row.integer(id_column+8)!=0;return rule;
}
std::vector<rules::Rule> rules_at(sqlite3* db,std::int64_t revision){
    if(revision==0) return {};
    Statement present(db,"SELECT 1 FROM rule_operations WHERE ruleset_after=?");present.integer(1,revision);if(!present.row()) return {};
    Statement rows(db,"SELECT s.rule,s.version,s.enabled,d.scope,d.input_field,d.expression,d.payee_template,d.memo_template,d.category_id,d.inherit_parent FROM rule_snapshots s JOIN rule_definitions d ON d.id=s.rule AND d.version=s.version WHERE s.ruleset_revision=? AND s.deleted=0 ORDER BY s.position");rows.integer(1,revision);
    std::vector<rules::Rule> rules;
    while(rows.row()){
        rules::Rule rule;rule.identity=rows.bytes(0);rule.version=rows.integer(1);rule.enabled=rows.integer(2)!=0;rule.scope=static_cast<rules::Scope>(rows.integer(3));rule.input=static_cast<rules::InputField>(rows.integer(4));rule.expression=rows.bytes(5);rule.actions.payee_template=optional_text(rows,6);rule.actions.memo_template=optional_text(rows,7);rule.actions.category=optional_text(rows,8);rule.actions.inherit_parent=rows.integer(9)!=0;rules.push_back(std::move(rule));
    }
    return rules;
}
std::vector<rules::Rule> compile_images(sqlite3* db,const std::vector<app::RuleDefinitionImage>& images,bool saved){
    std::vector<rules::Rule> rules;
    for(const auto& image:images){
        identifier(image.identity);validate_label(image.label);
        if(saved){Statement dead(db,"SELECT deleted FROM rule_heads WHERE id=?");dead.text(1,image.identity);if(dead.row() && dead.integer(0)!=0)fail(SQLITE_CONSTRAINT,"Deleted rule identity cannot return");}
        require_rule_category(db,image.actions.category,image.enabled);
        rules.push_back(rule_from(image,0));
    }
    return rules;
}
struct PreparedScope { bool present=false; std::string batch; std::int64_t batch_revision=0; std::size_t count=0; };
PreparedScope pending_scope(const ImportStore& store,std::string_view requested){
    std::string id(requested);
    if(id.empty()){for(const auto& info:store.batches())if(info.state==app::BatchState::pending){id=info.id;break;}if(id.empty())return {};}
    const auto all=store.batches();const auto found=std::find_if(all.begin(),all.end(),[&](const auto& info){return info.id==id;});
    if(found==all.end()) fail(SQLITE_NOTFOUND,"Import does not exist");
    if(found->state==app::BatchState::committed) fail(SQLITE_CONSTRAINT,"Committed records are not changed by rules");
    if(found->state!=app::BatchState::pending) fail(SQLITE_CONSTRAINT,"Rules can only be applied after the import finishes.");
    PreparedScope scope;scope.present=true;scope.batch=found->id;scope.batch_revision=found->revision;scope.count=found->count;
    return scope;
}
std::vector<app::RuleInput> inputs_from(const std::vector<app::RowView>& rows,const app::ImportRepository& repo){
    std::vector<app::RuleInput> inputs;
    for(const auto& viewed:rows){
        app::RuleInput input{viewed.identity,viewed.source,*viewed.draft,{}};
        auto add=[&](app::RuleField field,const std::string& allocation,const std::optional<RuleMark>& mark){if(mark)input.provenance.push_back({field,allocation,{mark->identity,mark->version}});};
        add(app::RuleField::payee,"",input.draft.payee_rule);add(app::RuleField::memo,"",input.draft.memo_rule);
        for(const auto& allocation:input.draft.transaction.allocations()){add(app::RuleField::allocation_memo,allocation.identity,allocation.memo_rule);add(app::RuleField::allocation_category,allocation.identity,allocation.category_rule);}
        for(const auto& allocation:input.draft.transaction.allocations())if(repo.resolve_account_name(allocation.category))input.transfer_allocations.push_back(allocation.identity);
        inputs.push_back(std::move(input));
    }
    return inputs;
}
// Bounds for retained output/preparation, separate from the evaluator's
// per-transaction limits. These are conservative payload budgets, not RSS claims.
constexpr std::size_t match_preview_budget=128U*1024U*1024U;
constexpr std::size_t rule_apply_budget=512U*1024U*1024U;
std::size_t rule_snapshot_bytes(const imports::DraftRow& row) {
    std::size_t bytes=1024+row.date.size()+row.payee.size()+row.memo.size();
    for(const auto& allocation:row.transaction.allocations())bytes+=512+allocation.category.size()+allocation.memo.size()+allocation.identity.size();
    return bytes;
}
bool any_mark(const imports::DraftRow& row){if(row.payee_rule || row.memo_rule)return true;for(const auto& allocation:row.transaction.allocations())if(allocation.category_rule || allocation.memo_rule)return true;return false;}
void refuse_drift(sqlite3* db,const imports::DraftRow& before,const imports::DraftRow& after,std::int64_t catalog){
    if((before.remainder_allocation!=after.remainder_allocation || before.manual_split_amounts!=after.manual_split_amounts) || before.date!=after.date || before.transaction.effective_total()!=after.transaction.effective_total() || before.transaction.original_imported_amount()!=after.transaction.original_imported_amount()) fail(SQLITE_CONSTRAINT,rules_drift);
    const auto& left=before.transaction.allocations();const auto& right=after.transaction.allocations();if(left.size()!=right.size())fail(SQLITE_CONSTRAINT,rules_drift);
    for(std::size_t index=0;index<left.size();++index){
        if(left[index].identity!=right[index].identity || left[index].source_ordinal!=right[index].source_ordinal || left[index].amount.units()!=right[index].amount.units()) fail(SQLITE_CONSTRAINT,rules_drift);
        if(right[index].category_owner==FieldOwnership::rule && right[index].category_binding && right[index].category_binding->catalog_revision!=catalog) fail(SQLITE_CONSTRAINT,rules_drift);
        if(right[index].category_binding) validate_binding(db,right[index]);
    }
}
bool save_request_matches(sqlite3* db,std::string_view operation,const app::RuleSaveRequest& request){
    Statement rows(db,"SELECT rule,label,scope,input_field,expression,payee_template,memo_template,category_id,inherit_parent,enabled FROM rule_operation_requests WHERE operation=? ORDER BY position");rows.text(1,operation);std::size_t index=0;
    while(rows.row()){
        if(index>=request.ordered.size()) return false;
        const auto& image=request.ordered[index++];
        if(rows.bytes(0)!=image.identity || rows.bytes(1)!=image.label || rows.integer(2)!=static_cast<int>(image.scope) || rows.integer(3)!=static_cast<int>(image.input) || rows.bytes(4)!=image.expression || !same_optional(rows,5,image.actions.payee_template) || !same_optional(rows,6,image.actions.memo_template) || !same_optional(rows,7,image.actions.category) || (rows.integer(8)!=0)!=image.actions.inherit_parent || (rows.integer(9)!=0)!=image.enabled) return false;
    }
    return index==request.ordered.size();
}
struct StoredHead { std::string id; std::int64_t version=0; std::int64_t position=0; bool enabled=false; bool deleted=false; RuleBody body; };
std::vector<StoredHead> stored_heads(sqlite3* db){
    Statement rows(db,"SELECT h.id,h.version,h.position,h.enabled,h.deleted,d.label,d.scope,d.input_field,d.expression,d.payee_template,d.memo_template,d.category_id,d.inherit_parent FROM rule_heads h JOIN rule_definitions d ON d.id=h.id AND d.version=h.version");
    std::vector<StoredHead> heads;
    while(rows.row()){StoredHead head;head.id=rows.bytes(0);head.version=rows.integer(1);head.deleted=rows.integer(4)!=0;head.enabled=rows.integer(3)!=0;if(!head.deleted){if(rows.null(2))fail(SQLITE_CORRUPT);head.position=rows.integer(2);}head.body={rows.bytes(5),rows.bytes(8),static_cast<rules::Scope>(rows.integer(6)),static_cast<rules::InputField>(rows.integer(7)),optional_text(rows,9),optional_text(rows,10),optional_text(rows,11),rows.integer(12)!=0};heads.push_back(std::move(head));}
    return heads;
}
void bind_definition(Statement& row,const app::RuleDefinitionImage& image,std::int64_t version,int first){
    row.text(first,image.identity);row.integer(first+1,version);row.text(first+2,image.label);row.integer(first+3,static_cast<int>(image.scope));row.integer(first+4,static_cast<int>(image.input));row.text(first+5,image.expression);
    if(image.actions.payee_template)row.text(first+6,*image.actions.payee_template);
    if(image.actions.memo_template)row.text(first+7,*image.actions.memo_template);
    if(image.actions.category)row.text(first+8,*image.actions.category);
    row.integer(first+9,image.actions.inherit_parent?1:0);
}
std::string rule_save_digest(const app::RuleSaveRequest& request,std::int64_t expected) {
    transfer_codec::Writer out;out.number(expected);out.number(integer(request.ordered.size()));
    auto optional=[&](const std::optional<std::string>& value){out.number(value.has_value());if(value)out.text(*value);};
    for(const auto& r:request.ordered){out.text(r.identity);out.text(r.label);out.number(r.enabled);out.number(static_cast<int>(r.scope));out.number(static_cast<int>(r.input));out.text(r.expression);optional(r.actions.payee_template);optional(r.actions.memo_template);optional(r.actions.category);out.number(r.actions.inherit_parent);}
    return session_snapshot::digest(out.bytes);
}
void bind_request(Statement& row,const app::RuleDefinitionImage& image,int first){
    row.text(first,image.identity);row.text(first+1,image.label);row.integer(first+2,static_cast<int>(image.scope));row.integer(first+3,static_cast<int>(image.input));row.text(first+4,image.expression);
    if(image.actions.payee_template)row.text(first+5,*image.actions.payee_template);
    if(image.actions.memo_template)row.text(first+6,*image.actions.memo_template);
    if(image.actions.category)row.text(first+7,*image.actions.category);
    row.integer(first+8,image.actions.inherit_parent?1:0);
}
}
#endif

bool ImportStore::rules_supported() const { return rules_format(impl_->db); }

#ifndef SPENTIT_RULES_STORAGE
app::RuleImage ImportStore::rule_image() const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
std::vector<app::RuleTombstone> ImportStore::rule_tombstones() const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
app::RuleSaveReceipt ImportStore::save_rules(std::string_view,const app::RuleSaveRequest&,std::int64_t,const app::OperationContext&) { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
std::optional<app::RuleSaveReceipt> ImportStore::rule_save_outcome(std::string_view) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
app::RulePreviewResult ImportStore::preview_pending_rules(const app::RulePreviewRequest&,const app::OperationContext&) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
app::RuleApplyReceipt ImportStore::apply_pending_rules(std::string_view,const app::RuleApplyRequest&,const app::OperationContext&) { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
std::optional<app::RuleApplyReceipt> ImportStore::rule_apply_outcome(std::string_view) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
app::RuleMatch ImportStore::match_saved_rules(const imports::Occurrence&,const app::OperationContext&) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
void ImportStore::verify_rules(const app::OperationContext*) const {}
#else
app::RuleImage ImportStore::rule_image() const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    app::RuleImage image;image.revision=ruleset_revision(impl_->db);
    Statement rows(impl_->db,"SELECT h.id,h.enabled,d.label,d.scope,d.input_field,d.expression,d.payee_template,d.memo_template,d.category_id,d.inherit_parent FROM rule_heads h JOIN rule_definitions d ON d.id=h.id AND d.version=h.version WHERE h.deleted=0 ORDER BY h.position");
    while(rows.row()){app::RuleDefinitionImage rule;rule.identity=rows.bytes(0);rule.enabled=rows.integer(1)!=0;rule.label=rows.bytes(2);rule.scope=static_cast<rules::Scope>(rows.integer(3));rule.input=static_cast<rules::InputField>(rows.integer(4));rule.expression=rows.bytes(5);rule.actions.payee_template=optional_text(rows,6);rule.actions.memo_template=optional_text(rows,7);rule.actions.category=optional_text(rows,8);rule.actions.inherit_parent=rows.integer(9)!=0;image.ordered.push_back(std::move(rule));}
    return image;
}
std::vector<app::RuleTombstone> ImportStore::rule_tombstones() const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    std::vector<app::RuleTombstone> result;
    Statement rows(impl_->db,"SELECT h.id,h.version,d.label,d.scope,d.input_field,d.expression,d.payee_template,d.memo_template,d.category_id,d.inherit_parent FROM rule_heads h JOIN rule_definitions d ON d.id=h.id AND d.version=h.version WHERE h.deleted=1 ORDER BY h.id");
    while(rows.row()){app::RuleTombstone tombstone;tombstone.identity=rows.bytes(0);tombstone.version=rows.integer(1);tombstone.label=rows.bytes(2);tombstone.scope=static_cast<rules::Scope>(rows.integer(3));tombstone.input=static_cast<rules::InputField>(rows.integer(4));tombstone.expression=rows.bytes(5);tombstone.actions.payee_template=optional_text(rows,6);tombstone.actions.memo_template=optional_text(rows,7);tombstone.actions.category=optional_text(rows,8);tombstone.actions.inherit_parent=rows.integer(9)!=0;result.push_back(std::move(tombstone));}
    return result;
}
std::optional<app::RuleSaveReceipt> ImportStore::rule_save_outcome(std::string_view operation) const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    session_snapshot::expired(impl_->db,operation,"rule-save");
    Statement row(impl_->db,"SELECT ruleset_before,ruleset_after FROM rule_operations WHERE id=?");row.text(1,operation);
    if(!row.row()){if(command_taken(impl_->db,operation))fail(SQLITE_CONSTRAINT,other_command);return {};}
    app::RuleSaveReceipt receipt{std::string(operation),row.integer(0),row.integer(1),false,{}};
    Statement versions(impl_->db,"SELECT rule,version FROM rule_snapshots WHERE ruleset_revision=? AND deleted=0 ORDER BY position");versions.integer(1,receipt.ruleset_after);
    while(versions.row()) receipt.versions.emplace_back(versions.bytes(0),versions.integer(1));
    Statement requests(impl_->db,"SELECT count(*) FROM rule_operation_requests WHERE operation=?");requests.text(1,operation);
    if(!requests.row() || requests.integer(0)!=integer(receipt.versions.size())) fail(SQLITE_CORRUPT);
    return receipt;
}
app::RuleSaveReceipt ImportStore::save_rules(std::string_view operation,const app::RuleSaveRequest& request,std::int64_t expected,const app::OperationContext& context) {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    identifier(operation);
    const auto digest=rule_save_digest(request,expected);
    session_snapshot::retry(impl_->db,operation,"rule-save",digest);
    if(const auto prior=rule_save_outcome(operation)){if(prior->ruleset_before!=expected || !save_request_matches(impl_->db,operation,request))fail(SQLITE_CONSTRAINT,reused_command);return *prior;}
    context.checkpoint("Saving rules");
    auto compiled=compile_images(impl_->db,request.ordered,true);
    { rules::RuleSet compiled_set(std::move(compiled)); }
    context.checkpoint("Saving rules");
    if(!impl_->composing_rule_save)context.seal();
    std::unique_ptr<Atomic> transaction;
    if(!impl_->composing_rule_save)transaction=std::make_unique<Atomic>(impl_->db);
    const auto current=ruleset_revision(impl_->db);
    if(current!=expected) fail(SQLITE_CONSTRAINT,refresh_rules);
    auto heads=stored_heads(impl_->db);
    std::vector<StoredHead> live;for(const auto& head:heads)if(!head.deleted)live.push_back(head);
    std::sort(live.begin(),live.end(),[](const auto& left,const auto& right){return left.position<right.position;});
    bool unchanged=live.size()==request.ordered.size();
    if(unchanged)for(std::size_t index=0;index<live.size();++index)if(live[index].id!=request.ordered[index].identity || live[index].enabled!=request.ordered[index].enabled || !same_content(live[index].body,body_of(request.ordered[index]))) unchanged=false;
    if(unchanged){app::RuleSaveReceipt receipt{"",current,current,true,{}};for(const auto& head:live)receipt.versions.emplace_back(head.id,head.version);return receipt;}
    if(current==INT64_MAX) fail(SQLITE_FULL,"Ruleset revision capacity exhausted");
    std::map<std::string,StoredHead> by_id;for(const auto& head:heads)by_id.emplace(head.id,head);
    struct Planned { app::RuleDefinitionImage image; std::int64_t version=0; bool define=false; bool created=false; };
    std::vector<Planned> planned;std::set<std::string> retained;
    for(const auto& image:request.ordered){
        Planned item;item.image=image;auto found=by_id.find(image.identity);
        if(found==by_id.end()){item.version=0;item.define=true;item.created=true;}
        else {if(found->second.deleted)fail(SQLITE_CONSTRAINT,"Deleted rule identity cannot return");item.version=found->second.version;if(!same_content(found->second.body,body_of(image))){if(item.version==INT64_MAX)fail(SQLITE_FULL);++item.version;item.define=true;}}
        if(!retained.insert(image.identity).second) fail(SQLITE_CONSTRAINT,reused_command);
        planned.push_back(std::move(item));
    }
    for(const auto& item:planned)if(item.define){Statement definition(impl_->db,"INSERT INTO rule_definitions VALUES(?,?,?,?,?,?,?,?,?,?)");bind_definition(definition,item.image,item.version,1);definition.done();}
    for(const auto& head:live)if(!retained.contains(head.id)){Statement tombstone(impl_->db,"UPDATE rule_heads SET deleted=1,position=NULL,enabled=0 WHERE id=?");tombstone.text(1,head.id);tombstone.done();}
    // Partial unique index is checked per row, so finals stay clear of the shifted range.
    Statement shift(impl_->db,"UPDATE rule_heads SET position=position+100000 WHERE deleted=0");shift.done();
    for(std::size_t index=0;index<planned.size();++index){
        const auto& item=planned[index];
        if(item.created){Statement head(impl_->db,"INSERT INTO rule_heads VALUES(?,?,?,?,0)");head.text(1,item.image.identity);head.integer(2,item.version);head.integer(3,integer(index));head.integer(4,item.image.enabled?1:0);head.done();}
        else {Statement head(impl_->db,"UPDATE rule_heads SET version=?,position=?,enabled=? WHERE id=?");head.integer(1,item.version);head.integer(2,integer(index));head.integer(3,item.image.enabled?1:0);head.text(4,item.image.identity);head.done();}
    }
    Statement saved(impl_->db,"INSERT INTO rule_operations VALUES(?,?,?)");saved.text(1,operation);saved.integer(2,current);saved.integer(3,current+1);saved.done();
    for(std::size_t index=0;index<planned.size();++index){
        const auto& item=planned[index];
        Statement stored(impl_->db,"INSERT INTO rule_operation_requests VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");stored.text(1,operation);stored.integer(2,integer(index));bind_request(stored,item.image,3);stored.integer(12,item.image.enabled?1:0);stored.done();
        Statement snapshot(impl_->db,"INSERT INTO rule_snapshots(ruleset_revision,position,rule,version,enabled) VALUES(?,?,?,?,?)");snapshot.integer(1,current+1);snapshot.integer(2,integer(index));snapshot.text(3,item.image.identity);snapshot.integer(4,item.version);snapshot.integer(5,item.image.enabled?1:0);snapshot.done();
    }
    session_snapshot::completed(impl_->db,operation,"rule-save",digest);
    Statement bump(impl_->db,"UPDATE rule_state SET revision=? WHERE singleton=1 AND revision=?");bump.integer(1,current+1);bump.integer(2,current);bump.done();
    finish([&]{if(transaction)transaction->commit();},true);
    return *rule_save_outcome(operation);
}
std::optional<app::RuleApplyReceipt> ImportStore::rule_apply_outcome(std::string_view operation) const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    if(!impl_->verifying)session_snapshot::expired(impl_->db,operation,"rule-apply");
    Statement row(impl_->db,"SELECT batch,batch_before,batch_after,ruleset_revision,catalog_revision,replace_manual,evaluated,changed_count FROM rule_applications WHERE id=?");row.text(1,operation);
    if(!row.row()){if(command_taken(impl_->db,operation))fail(SQLITE_CONSTRAINT,other_command);return {};}
    require_account(row.bytes(0));
    return app::RuleApplyReceipt{std::string(operation),row.bytes(0),row.integer(1),row.integer(2),row.integer(3),row.integer(4),row.integer(5)!=0,static_cast<std::size_t>(row.integer(6)),static_cast<std::size_t>(row.integer(7))};
}
app::RulePreviewResult ImportStore::preview_pending_rules(const app::RulePreviewRequest& request,const app::OperationContext& context) const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    if(request.replace_manual)fail(SQLITE_CONSTRAINT,"Clear categories in the ledger before running rules again");
    ScopedStatements read_plans(impl_->db);
    context.checkpoint("Loading rules");
    if(request.single_rule && request.saved_rule.empty() && (!request.editor || request.editor->size()!=1))fail(SQLITE_CONSTRAINT,"Choose one rule to preview");
    const auto scope=(request.single_rule || request.account_scope)?PreparedScope{}:pending_scope(*this,request.batch);
    if(scope.present && request.batch_revision!=-1 && request.batch_revision!=scope.batch_revision) fail(SQLITE_CONSTRAINT,refresh_rules);
    app::RulePreviewResult result;result.batch=scope.batch;result.batch_revision=scope.batch_revision;result.catalog_revision=catalog_sequence(impl_->db);result.replace_manual=request.replace_manual;result.unsaved_rules=request.editor.has_value();result.ruleset_revision=request.editor?-1:ruleset_revision(impl_->db);
    if(!impl_->composing_rule_save && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    result.account_request={account().identity,review_generation(impl_->db),ruleset_revision(impl_->db),result.catalog_revision,request.new_only,request.replace_manual,request.saved_rule};
    auto definitions=rules_at(impl_->db,ruleset_revision(impl_->db));
    std::string selected=request.saved_rule;
    if(request.editor) {
        auto edited=compile_images(impl_->db,*request.editor,false);
        if(request.single_rule) {
            if(edited.size()!=1)fail(SQLITE_CONSTRAINT,"Choose one rule to preview");
            selected=edited.front().identity;
            const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& r){return r.identity==selected;});
            if(found==definitions.end())definitions.push_back(edited.front());else *found=edited.front();
        } else definitions=std::move(edited);
    }
    rules::RuleSet ruleset(definitions,selected);
    const auto catalog=category_catalog();
    std::map<imports::Occurrence,std::vector<std::string>> protected_allocations;
    if(request.account_scope)for(const auto& transfer:all_transfers())if(!transfer.link.unresolved)
        for(const auto* side:{&transfer.link.origin,&transfer.link.destination})
            if(side->state!=transfers::State::removed && side->endpoint.account==account().identity)
                protected_allocations[side->endpoint.entry].push_back(side->endpoint.allocation);
    std::size_t result_bytes=0;
    auto evaluate=[&](app::RowView viewed) {
        if(viewed.excluded_by_clear)return;
        Statement awaiting(impl_->db,"SELECT 1 FROM import_category_decisions d JOIN batches b ON b.source_id=d.source WHERE b.id=? AND d.resolved=0");awaiting.text(1,viewed.identity.batch);if(awaiting.row())return;
        context.checkpoint(result.evaluated%256==0?"Finding matching transactions":"");
        // Each record retains the existing allocation, text and regex execution
        // bounds. Cancellation/failure discards the entire locally-built result.
        std::optional<app::RuleProposal> evaluated;
        auto inputs=inputs_from({viewed},*this);
        if(const auto found=protected_allocations.find(viewed.identity);found!=protected_allocations.end())inputs.front().transfer_allocations.insert(inputs.front().transfer_allocations.end(),found->second.begin(),found->second.end());
        try {evaluated=app::preview_rules(ruleset,catalog,inputs,request.replace_manual,[&]{return context.state.load()==app::OperationContext::requested;},true).front();}
        catch(const rules::RegexError& error){if(error.kind()==rules::RegexFailure::cancelled)throw app::OperationCancelled();throw;}
        catch(const std::invalid_argument&){fail(SQLITE_CONSTRAINT,rules_drift);}
        auto proposal=std::move(*evaluated);
        ++result.evaluated;
        result.raw_matches+=proposal.match.raw;result.categorized_matches+=proposal.match.categorized;
        result.earlier_matches+=proposal.match.earlier;result.eligible_matches+=proposal.match.eligible;
        auto diffs=app::rule_field_diffs(*viewed.draft,proposal);
        if(!diffs.empty())++result.changed;
        if(!request.changed_only && proposal.match.eligible) {
            const auto bytes=512+viewed.identity.batch.size()+viewed.draft->date.size()+viewed.source.payee.size();
            if(bytes>match_preview_budget-result_bytes)throw rules::RegexError(rules::RegexFailure::resource_limit,"This preview has too many matching details to display. Preview fewer rules or narrow their patterns.");
            result_bytes+=bytes;
            result.matches.push_back({viewed.identity,viewed.draft->date,viewed.source.payee,viewed.draft->transaction.effective_total()});
        }
        if(!request.matches_only && (!request.single_rule || proposal.match.eligible) && (!request.changed_only || !diffs.empty())) {
            auto bytes=rule_snapshot_bytes(*viewed.draft)+rule_snapshot_bytes(proposal.after);
            for(const auto& diff:diffs)bytes+=512+diff.before_text.size()+diff.after_text.size();
            if(bytes>(request.changed_only?rule_apply_budget:match_preview_budget)-result_bytes)throw rules::RegexError(rules::RegexFailure::resource_limit,request.changed_only?"These rule changes exceed the Apply memory budget. Narrow the rules and preview again.":"Detailed rule preview exceeds its memory budget. Use a compact matching preview.");
            result_bytes+=bytes;
            result.records.push_back({viewed.identity,std::move(*viewed.draft),std::move(proposal),std::move(diffs)});
        }
    };
    if(request.single_rule || request.account_scope) {
        // Same membership as current_ledger(), without materializing all drafts
        // or generated counterpart rows (which have no imported source).
        Statement accepted(impl_->db,"SELECT h.batch,h.ordinal,h.revision FROM entry_heads h JOIN entry_values e USING(batch,ordinal,revision) WHERE h.batch IN(SELECT id FROM batches WHERE account_id=spentit_account()) AND e.active=1 ORDER BY e.date,h.batch,h.ordinal");
        while(!request.new_only && accepted.row()) {
            context.checkpoint();
            imports::Occurrence id{accepted.bytes(0),static_cast<std::size_t>(accepted.integer(1))};
            auto viewed=load_row(id,false);viewed.draft=entry_snapshot(id,accepted.integer(2)).draft;evaluate(std::move(viewed));
        }
        Statement pending(impl_->db,"SELECT v.batch,v.ordinal FROM versions v JOIN batches b ON b.id=v.batch WHERE b.account_id=spentit_account() AND b.state IN(0,4) AND (b.state!=4 OR EXISTS(SELECT 1 FROM retained_transfer_rows keep WHERE keep.batch=v.batch AND keep.ordinal=v.ordinal)) AND NOT EXISTS(SELECT 1 FROM accepted_transactions t WHERE t.batch=v.batch AND t.ordinal=v.ordinal) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g WHERE g.batch=v.batch AND g.ordinal=v.ordinal) AND v.decision IN(0,1) AND v.revision=(SELECT max(revision) FROM versions WHERE batch=v.batch AND ordinal=v.ordinal)");
        while(pending.row()){context.checkpoint();evaluate(load_row({pending.bytes(0),static_cast<std::size_t>(pending.integer(1))},false));}
    } else {
        for(std::size_t ordinal=0;ordinal<scope.count;++ordinal) {
            context.checkpoint();
            const imports::Occurrence id{scope.batch,ordinal};auto viewed=load_row(id,false);
            if(viewed.draft && !viewed.matched_generated && !transaction_accepted(id))evaluate(std::move(viewed));
        }
    }
    std::sort(result.matches.begin(),result.matches.end(),[](const auto& a,const auto& b){return std::tie(a.date,a.identity.batch,a.identity.ordinal)<std::tie(b.date,b.identity.batch,b.identity.ordinal);});
    context.checkpoint();
    return result;
}
app::RuleApplyReceipt ImportStore::apply_pending_rules(std::string_view operation,const app::RuleApplyRequest& request,const app::OperationContext& context) {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    identifier(operation);
    transfer_codec::Writer encoding;encoding.text(request.batch);encoding.number(request.batch_revision);encoding.number(request.ruleset_revision);encoding.number(request.catalog_revision);encoding.number(request.replace_manual);
    const auto request_hash=session_snapshot::digest(encoding.bytes);session_snapshot::retry(impl_->db,operation,"rule-apply",request_hash);
    if(const auto prior=rule_apply_outcome(operation)){if(prior->batch!=request.batch || prior->batch_before!=request.batch_revision || prior->ruleset_revision!=request.ruleset_revision || prior->catalog_revision!=request.catalog_revision || prior->replace_manual!=request.replace_manual)fail(SQLITE_CONSTRAINT,reused_command);return *prior;}
    ScopedStatements read_plans(impl_->db);
    context.checkpoint("Loading rules");
    const auto scope=pending_scope(*this,request.batch);
    Statement awaiting(impl_->db,"SELECT 1 FROM import_category_decisions d JOIN batches b ON b.source_id=d.source WHERE b.id=? AND d.resolved=0");awaiting.text(1,scope.batch);if(awaiting.row())fail(SQLITE_CONSTRAINT,"Choose the imported categories before running rules");
    const auto catalog=catalog_sequence(impl_->db);
    if(!scope.present || scope.count==0){
        if(scope.present && request.batch_revision!=scope.batch_revision) fail(SQLITE_CONSTRAINT,refresh_rules);
        return {"",scope.batch,scope.batch_revision,scope.batch_revision,ruleset_revision(impl_->db),catalog,request.replace_manual,0,0};
    }
    if(scope.batch_revision!=request.batch_revision || ruleset_revision(impl_->db)!=request.ruleset_revision || catalog!=request.catalog_revision)fail(SQLITE_CONSTRAINT,refresh_rules);
    if(!impl_->composing_rule_save && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
    struct Prepared { imports::Occurrence identity; imports::DraftRow before; imports::Decision decision; app::RuleProposal proposal; std::vector<app::RuleFieldDiff> diffs; };
    std::vector<Prepared> prepared;
    std::size_t evaluated=0,prepared_bytes=0;
    {
        rules::RuleSet ruleset(rules_at(impl_->db,request.ruleset_revision));
        const auto categories=category_catalog();
        for(std::size_t ordinal=0;ordinal<scope.count;++ordinal) {
            context.checkpoint(ordinal%256==0?"Preparing matching transactions":"");
            const imports::Occurrence id{scope.batch,ordinal};auto viewed=load_row(id,false);
            if(!viewed.draft || viewed.matched_generated || transaction_accepted(id))continue;
            std::vector<app::RuleProposal> proposals;
            try {proposals=app::preview_rules(ruleset,categories,inputs_from({viewed},*this),request.replace_manual,[&]{return context.state.load()==app::OperationContext::requested;},true);}
            catch(const rules::RegexError& error){if(error.kind()==rules::RegexFailure::cancelled) throw app::OperationCancelled(); throw;}
            catch(const std::invalid_argument&){fail(SQLITE_CONSTRAINT,rules_drift);}
            ++evaluated;
            auto diffs=app::rule_field_diffs(*viewed.draft,proposals.front());
            if(!diffs.empty()) {
                auto bytes=rule_snapshot_bytes(*viewed.draft)+rule_snapshot_bytes(proposals.front().after);
                for(const auto& diff:diffs)bytes+=512+diff.before_text.size()+diff.after_text.size();
                if(bytes>rule_apply_budget-prepared_bytes)throw rules::RegexError(rules::RegexFailure::resource_limit,"These rule changes exceed the Apply memory budget. Narrow the rules and preview again.");
                prepared_bytes+=bytes;
                prepared.push_back({id,std::move(*viewed.draft),viewed.decision,std::move(proposals.front()),std::move(diffs)});
            }
        }
    }
    if(evaluated==0)return {"",scope.batch,scope.batch_revision,scope.batch_revision,request.ruleset_revision,catalog,request.replace_manual,0,0};
    context.checkpoint("Applying rules");
    context.seal();
    Atomic transaction(impl_->db);
    Statement batch_row(impl_->db,"SELECT revision,state FROM batches WHERE id=?");batch_row.text(1,scope.batch);if(!batch_row.row())fail(SQLITE_CORRUPT);
    if(batch_row.integer(0)!=request.batch_revision || batch_row.integer(1)!=static_cast<int>(app::BatchState::pending) || ruleset_revision(impl_->db)!=request.ruleset_revision || catalog_sequence(impl_->db)!=request.catalog_revision) fail(SQLITE_CONSTRAINT,refresh_rules);
    if(request.ruleset_revision>0){Statement present(impl_->db,"SELECT 1 FROM rule_operations WHERE ruleset_after=?");present.integer(1,request.ruleset_revision);if(!present.row())fail(SQLITE_CORRUPT);}
    std::size_t changed=0;
    for(const auto& item:prepared){refuse_drift(impl_->db,item.before,item.proposal.after,request.catalog_revision);if(!item.diffs.empty())++changed;}
    const auto after_revision=changed==0?request.batch_revision:request.batch_revision+1;
    if(changed!=0){
        if(request.batch_revision==INT64_MAX) fail(SQLITE_FULL,"Revision capacity exhausted");
        for(const auto& item:prepared)if(!item.diffs.empty()){
            std::optional<std::int64_t> ruleset;if(any_mark(item.proposal.after)){if(request.ruleset_revision<1)fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");ruleset=request.ruleset_revision;}
            version(impl_->db,item.identity,after_revision,item.proposal.after,item.decision,false,ruleset);
        }
        advance(impl_->db,scope.batch,request.batch_revision,"apply_rules");
    }
    Statement application(impl_->db,"INSERT INTO rule_applications VALUES(?,?,?,?,?,?,?,?,?)");application.text(1,operation);application.text(2,scope.batch);application.integer(3,request.batch_revision);application.integer(4,after_revision);application.integer(5,request.ruleset_revision);application.integer(6,request.catalog_revision);application.integer(7,request.replace_manual?1:0);application.integer(8,integer(evaluated));application.integer(9,integer(changed));application.done();
    if(changed && impl_->import_action.empty() && !impl_->composing_review) {
        record_transaction_action(operation,"rules-pending",request_hash,{});
        Statement members(impl_->db,"INSERT INTO review_members SELECT ?1,v.batch,v.ordinal,0,(SELECT max(p.revision) FROM versions p WHERE p.batch=v.batch AND p.ordinal=v.ordinal AND p.revision<v.revision),v.revision,?1||'-pending-'||v.ordinal FROM versions v WHERE v.batch=?2 AND v.revision=?3");members.text(1,operation);members.text(2,scope.batch);members.integer(3,after_revision);members.done();
    }
    session_snapshot::completed(impl_->db,operation,"rule-apply",request_hash);
    finish([&]{transaction.commit();},changed==0);
    return *rule_apply_outcome(operation);
}
app::RuleMatch ImportStore::match_saved_rules(const imports::Occurrence& id,const app::OperationContext& context) const {
    if(!rules_supported()) fail(SQLITE_SCHEMA,rules_dataset);
    context.checkpoint("Loading rules");
    const auto viewed=load_row(id,false);
    std::optional<imports::DraftRow> draft;
    Statement head(impl_->db,"SELECT revision FROM entry_heads WHERE batch=? AND ordinal=?");head.text(1,id.batch);head.integer(2,integer(id.ordinal));
    if(head.row()) draft=entry_snapshot(id,head.integer(0)).draft;
    else if(viewed.draft) draft=*viewed.draft;
    else return {};
    app::RuleMatch match;
    try {
        context.checkpoint("Evaluating rules");
        if(!impl_->composing_rule_save && !sqlite3_get_autocommit(impl_->db)) throw app::OperationOutcomeUnknown();
        app::RowView row;row.identity=id;row.source=viewed.source;row.draft=draft;
        rules::RuleSet ruleset(rules_at(impl_->db,ruleset_revision(impl_->db)));
        auto proposals=app::preview_rules(ruleset,category_catalog(),inputs_from({row},*this),false,[&]{return context.state.load()==app::OperationContext::requested;});
        if(proposals.size()==1) match.explanations=std::move(proposals.front().explanations);
    } catch(const rules::RegexError& error) { match.error=error.what(); }
    auto winner=[&](const std::optional<RuleMark>& mark,const char* field,std::optional<std::size_t> allocation={}){if(!mark)return;Statement definition(impl_->db,"SELECT label,scope FROM rule_definitions WHERE id=? AND version=?");definition.text(1,mark->identity);definition.integer(2,mark->version);if(!definition.row())fail(SQLITE_CORRUPT);match.assignments.push_back({*mark,field,allocation,static_cast<rules::Scope>(definition.integer(1))});match.winner_lines.push_back("Assigned by "+definition.bytes(0)+" v"+std::to_string(mark->version));};
    winner(draft->payee_rule,"Payee");winner(draft->memo_rule,"Memo");
    for(std::size_t i=0;i<draft->transaction.allocations().size();++i){const auto& allocation=draft->transaction.allocations()[i];winner(allocation.category_rule,"Category",i);winner(allocation.memo_rule,"Split memo",i);}
    return match;
}
void ImportStore::verify_rules(const app::OperationContext* context) const {
    if(context) context->checkpoint("Verifying rules");
    struct Sim { std::int64_t version=0; bool enabled=false; bool deleted=false; RuleBody body; };
    std::map<std::string,Sim> reconstructed;std::int64_t revision=0,previous_order=0;
    Statement cut(impl_->db,"SELECT coalesce(max(o.ruleset_after),0) FROM rule_operations o JOIN history_order h ON h.rule_operation=o.id WHERE h.sequence<=(SELECT history_cut FROM document_session)");
    if(!cut.row())fail(SQLITE_CORRUPT);const auto baseline=cut.integer(0);
    Statement shape(impl_->db,"SELECT 1 FROM rule_snapshots WHERE ruleset_revision<?1 OR (deleted=1 AND ruleset_revision!=?1) UNION ALL SELECT 1 FROM rule_operation_requests r JOIN rule_operations o ON o.id=r.operation WHERE o.ruleset_after<=?1 UNION ALL SELECT 1 FROM (SELECT ruleset_revision,count(*) AS n,min(position) AS lo,max(position) AS hi FROM rule_snapshots WHERE deleted=0 GROUP BY ruleset_revision) WHERE lo!=0 OR hi!=n-1 UNION ALL SELECT 1 FROM (SELECT operation,count(*) AS n,min(position) AS lo,max(position) AS hi FROM rule_operation_requests GROUP BY operation) WHERE lo!=0 OR hi!=n-1");shape.integer(1,baseline);if(shape.row())fail(SQLITE_CORRUPT,"Invalid retained ruleset baseline");

    if(baseline){
        Statement saved(impl_->db,"SELECT s.rule,s.version,s.enabled,s.deleted,d.label,d.scope,d.input_field,d.expression,d.payee_template,d.memo_template,d.category_id,d.inherit_parent FROM rule_snapshots s JOIN rule_definitions d ON d.id=s.rule AND d.version=s.version WHERE s.ruleset_revision=?");saved.integer(1,baseline);
        while(saved.row()) reconstructed.emplace(saved.bytes(0),Sim{saved.integer(1),saved.integer(2)!=0,saved.integer(3)!=0,{saved.bytes(4),saved.bytes(7),static_cast<rules::Scope>(saved.integer(5)),static_cast<rules::InputField>(saved.integer(6)),optional_text(saved,8),optional_text(saved,9),optional_text(saved,10),saved.integer(11)!=0}});
    }
    Statement operations(impl_->db,"SELECT id,ruleset_before,ruleset_after FROM rule_operations ORDER BY ruleset_after");
    while(operations.row()){
        if(context) context->checkpoint();
        const auto operation=operations.bytes(0);const auto before=operations.integer(1),after=operations.integer(2);
        if(before!=revision || after!=revision+1) fail(SQLITE_CORRUPT);
        Statement ordering(impl_->db,"SELECT sequence FROM history_order WHERE rule_operation=?");ordering.text(1,operation);if(!ordering.row() || ordering.integer(0)<=previous_order)fail(SQLITE_CORRUPT);
        const auto sequence=ordering.integer(0);previous_order=sequence;
        Statement completion(impl_->db,"SELECT domain,length(digest),expired FROM command_completions WHERE id=?");completion.text(1,operation);
        if(!completion.row() || completion.bytes(0)!="rule-save" || completion.integer(1)!=32 || (completion.integer(2)!=0)!=(after<=baseline))fail(SQLITE_CORRUPT,"Invalid rule save completion");
        if(after<=baseline){revision=after;continue;}

        Statement epoch(impl_->db,"SELECT coalesce(max(o.catalog_after),0) FROM category_operations o CROSS JOIN history_order h ON h.category_operation=o.id WHERE h.sequence<?");epoch.integer(1,sequence);if(!epoch.row())fail(SQLITE_CORRUPT);
        const auto catalog=epoch.integer(0);
        struct Member { std::string rule; RuleBody body; std::int64_t version=0; bool enabled=false; };
        std::vector<Member> requests,snapshots;
        Statement request_rows(impl_->db,"SELECT rule,label,scope,input_field,expression,payee_template,memo_template,category_id,inherit_parent,enabled FROM rule_operation_requests WHERE operation=? ORDER BY position");request_rows.text(1,operation);
        while(request_rows.row()) requests.push_back({request_rows.bytes(0),{request_rows.bytes(1),request_rows.bytes(4),static_cast<rules::Scope>(request_rows.integer(2)),static_cast<rules::InputField>(request_rows.integer(3)),optional_text(request_rows,5),optional_text(request_rows,6),optional_text(request_rows,7),request_rows.integer(8)!=0},0,request_rows.integer(9)!=0});
        Statement snapshot_rows(impl_->db,"SELECT rule,version,enabled FROM rule_snapshots WHERE ruleset_revision=? AND deleted=0 ORDER BY position");snapshot_rows.integer(1,after);
        while(snapshot_rows.row()) snapshots.push_back({snapshot_rows.bytes(0),{},snapshot_rows.integer(1),snapshot_rows.integer(2)!=0});
        if(requests.size()!=snapshots.size()) fail(SQLITE_CORRUPT);
        app::RuleSaveRequest request;
        for(const auto& r:requests){app::RuleDefinitionImage image;image.identity=r.rule;image.label=r.body.label;image.enabled=r.enabled;image.scope=r.body.scope;image.input=r.body.input;image.expression=r.body.expression;image.actions={r.body.payee,r.body.memo,r.body.category,r.body.inherit};request.ordered.push_back(std::move(image));}
        Statement digest(impl_->db,"SELECT digest FROM command_completions WHERE id=?");digest.text(1,operation);if(!digest.row() || digest.bytes(0)!=rule_save_digest(request,before))fail(SQLITE_CORRUPT,"Rule request differs from completion");

        std::set<std::string> seen;
        for(std::size_t index=0;index<requests.size();++index){
            if(requests[index].rule!=snapshots[index].rule || requests[index].enabled!=snapshots[index].enabled || !seen.insert(requests[index].rule).second) fail(SQLITE_CORRUPT);
            auto found=reconstructed.find(requests[index].rule);
            const bool live=found!=reconstructed.end() && !found->second.deleted;
            if(found!=reconstructed.end() && found->second.deleted) fail(SQLITE_CORRUPT);
            const auto expected_version=live?(same_content(found->second.body,requests[index].body)?found->second.version:found->second.version+1):0;
            if(snapshots[index].version!=expected_version) fail(SQLITE_CORRUPT);
            Statement definition(impl_->db,"SELECT label,scope,input_field,expression,payee_template,memo_template,category_id,inherit_parent FROM rule_definitions WHERE id=? AND version=?");definition.text(1,requests[index].rule);definition.integer(2,expected_version);if(!definition.row())fail(SQLITE_CORRUPT);
            RuleBody stored{definition.bytes(0),definition.bytes(3),static_cast<rules::Scope>(definition.integer(1)),static_cast<rules::InputField>(definition.integer(2)),optional_text(definition,4),optional_text(definition,5),optional_text(definition,6),definition.integer(7)!=0};
            if(stored!=requests[index].body) fail(SQLITE_CORRUPT);
            if(requests[index].body.category){
                bool unavailable=false;
                try { const auto category=category_at(impl_->db,*requests[index].body.category,catalog); unavailable=category.deleted || category.kind!=CategoryKind::leaf || (requests[index].enabled && !category_display(impl_->db,*requests[index].body.category,catalog).assignable); }
                catch(const ImportError&){ unavailable=true; }
                if(unavailable) fail(SQLITE_CORRUPT);
            }
            reconstructed[requests[index].rule]=Sim{expected_version,requests[index].enabled,false,requests[index].body};
        }
        for(auto& [identity,sim]:reconstructed) if(!sim.deleted && !seen.contains(identity)){sim.deleted=true;sim.enabled=false;}
        revision=after;
    }
    if(ruleset_revision(impl_->db)!=revision) fail(SQLITE_CORRUPT);
    const auto heads=stored_heads(impl_->db);
    if(heads.size()!=reconstructed.size()) fail(SQLITE_CORRUPT);
    std::vector<StoredHead> live;
    for(const auto& head:heads){
        auto found=reconstructed.find(head.id);if(found==reconstructed.end() || found->second.version!=head.version || found->second.enabled!=head.enabled || found->second.deleted!=head.deleted || found->second.body!=head.body) fail(SQLITE_CORRUPT);
        if(!head.deleted) live.push_back(head);
    }
    std::sort(live.begin(),live.end(),[](const auto& left,const auto& right){return left.position<right.position;});
    for(std::size_t index=0;index<live.size();++index) if(live[index].position!=integer(index)) fail(SQLITE_CORRUPT);
    if(revision>0){Statement snapshot(impl_->db,"SELECT rule,version,enabled FROM rule_snapshots WHERE ruleset_revision=? AND deleted=0 ORDER BY position");snapshot.integer(1,revision);std::size_t index=0;while(snapshot.row()){if(index>=live.size() || snapshot.bytes(0)!=live[index].id || snapshot.integer(1)!=live[index].version || (snapshot.integer(2)!=0)!=live[index].enabled)fail(SQLITE_CORRUPT);++index;}if(index!=live.size())fail(SQLITE_CORRUPT);}
    try {
        std::vector<rules::Rule> current;for(const auto& head:live){app::RuleDefinitionImage image;image.identity=head.id;image.label=head.body.label;image.enabled=head.enabled;image.scope=head.body.scope;image.input=head.body.input;image.expression=head.body.expression;image.actions={head.body.payee,head.body.memo,head.body.category,head.body.inherit};current.push_back(rule_from(image,head.version));}
        if(context) context->checkpoint();
        rules::RuleSet current_set(std::move(current));
        Statement definitions(impl_->db,"SELECT id,version,scope,input_field,expression,payee_template,memo_template,category_id,inherit_parent FROM rule_definitions ORDER BY id,version");
        while(definitions.row()){if(context)context->checkpoint();rules::Rule historical=rule_from_row(definitions,0,definitions.integer(1),false);rules::RuleSet one({std::move(historical)});}
    } catch(const rules::RegexError&) { fail(SQLITE_CORRUPT); }
    Statement batches_with_rules(impl_->db,"SELECT DISTINCT batch,ordinal FROM versions ORDER BY batch,ordinal");
    while(batches_with_rules.row()){
        const imports::Occurrence id{batches_with_rules.bytes(0),static_cast<std::size_t>(batches_with_rules.integer(1))};
        Statement versions(impl_->db,"SELECT revision,date,payee,memo,total,decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? ORDER BY revision");versions.text(1,id.batch);versions.integer(2,integer(id.ordinal));
        std::optional<imports::DraftRow> previous;std::int64_t previous_decision=0;std::string previous_duplicate;std::int64_t previous_ordinal=-1;
        while(versions.row()){
            if(context) context->checkpoint();
            const auto revision_at=versions.integer(0);
            const auto source=load_row(id,false).source;
            const auto currency=version_currency(impl_->db,id.batch,integer(id.ordinal),revision_at);const bool reassigned=source.source_currency && *source.source_currency!=currency;
            std::vector<Allocation> allocations;Statement values(impl_->db,"SELECT category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");values.text(1,id.batch);values.integer(2,integer(id.ordinal));values.integer(3,revision_at);
            while(values.row()) allocations.push_back({values.bytes(0),values.bytes(1),Money(values.integer(2),currency)});
            imports::DraftRow draft{versions.bytes(1),versions.bytes(2),versions.bytes(3),Transaction::repaired_import(source.original_amount,Money(versions.integer(4),currency),std::move(allocations),reassigned)};
            load_metadata(impl_->db,id,revision_at,draft,false);
            const bool marked=any_mark(draft);
            Statement provenance(impl_->db,"SELECT 1 FROM draft_rule_fields WHERE batch=? AND ordinal=? AND revision=?");provenance.text(1,id.batch);provenance.integer(2,integer(id.ordinal));provenance.integer(3,revision_at);
            if(provenance.row()!=marked) fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");
            std::set<std::int64_t> marked_positions;Statement allocation_marks(impl_->db,"SELECT position FROM draft_rule_allocations WHERE batch=? AND ordinal=? AND revision=?");allocation_marks.text(1,id.batch);allocation_marks.integer(2,integer(id.ordinal));allocation_marks.integer(3,revision_at);
            while(allocation_marks.row()) if(!marked_positions.insert(allocation_marks.integer(0)).second) fail(SQLITE_CORRUPT);
            const auto& stored_allocations=draft.transaction.allocations();
            for(std::size_t index=0;index<stored_allocations.size();++index){const bool allocation_marked=stored_allocations[index].category_rule.has_value() || stored_allocations[index].memo_rule.has_value();if(marked_positions.contains(integer(index))!=allocation_marked)fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");}
            std::size_t marked_allocations=0;for(const auto& allocation:stored_allocations)if(allocation.category_rule||allocation.memo_rule)++marked_allocations;if(marked_positions.size()!=marked_allocations)fail(SQLITE_CORRUPT);
            Statement event(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");event.text(1,id.batch);event.integer(2,revision_at);if(!event.row())fail(SQLITE_CORRUPT);
            const auto kind=event.bytes(0);
            if(session_snapshot::baseline(impl_->db,false,id,revision_at)){
                // Checked baseline: provenance is validated above; expired
                // intermediate field transitions are not reconstructed.
            } else if(!previous){
                const auto initial_owner=source.format=="manual"?FieldOwnership::manual:FieldOwnership::source;
                if(draft.payee_owner==FieldOwnership::rule || draft.memo_owner==FieldOwnership::rule || marked) fail(SQLITE_CORRUPT);
                if((kind=="ingest" || kind=="materialize") && (draft.payee_owner!=initial_owner || draft.memo_owner!=initial_owner)) fail(SQLITE_CORRUPT);
                for(const auto& allocation:stored_allocations) if(allocation.category_owner==FieldOwnership::rule || allocation.memo_owner==FieldOwnership::rule || ((kind=="ingest" || kind=="materialize") && (allocation.category_owner!=initial_owner || allocation.memo_owner!=initial_owner))) fail(SQLITE_CORRUPT);
            } else if(kind=="save") {if(!review_draft_undo(id,revision_at,draft)){
                Statement applied(impl_->db,"SELECT substr(a.request,1,1) FROM review_members m JOIN review_answers a ON a.id=m.answer WHERE m.batch=? AND m.ordinal=? AND m.result=? AND m.committed=0 AND a.question='rules-account'");
                applied.text(1,id.batch);applied.integer(2,integer(id.ordinal));applied.integer(3,revision_at);const bool account_apply=applied.row();
                check_rule_transition(*previous,draft,account_apply,account_apply && applied.bytes(0)=="1");
            }}
            else if(kind=="apply_rules"){
                if(versions.integer(5)!=previous_decision || versions.bytes(6)!=previous_duplicate || versions.integer(7)!=previous_ordinal) fail(SQLITE_CORRUPT);
                Statement applied(impl_->db,"SELECT replace_manual FROM rule_applications WHERE batch=? AND batch_after=?");applied.text(1,id.batch);applied.integer(2,revision_at);if(!applied.row())fail(SQLITE_CORRUPT);
                check_rule_transition(*previous,draft,true,applied.integer(0)!=0);
            } else if(kind=="decide") { if(!same_draft(*previous,draft)) fail(SQLITE_CORRUPT); }
            else if(kind=="currency") {
                if(versions.integer(5)!=previous_decision || versions.bytes(6)!=previous_duplicate || versions.integer(7)!=previous_ordinal || draft.date!=previous->date || draft.payee!=previous->payee || draft.memo!=previous->memo || draft.transaction.original_imported_amount()!=previous->transaction.original_imported_amount() || !previous->transaction.effective_total().same_decimal(draft.transaction.effective_total())) fail(SQLITE_CORRUPT);
                const auto& left=previous->transaction.allocations();const auto& right=draft.transaction.allocations();if(left.size()!=right.size())fail(SQLITE_CORRUPT);
                for(std::size_t index=0;index<left.size();++index)if(left[index].identity!=right[index].identity || left[index].category!=right[index].category || left[index].memo!=right[index].memo || !left[index].amount.same_decimal(right[index].amount))fail(SQLITE_CORRUPT);
            }
            else fail(SQLITE_CORRUPT);
            previous_decision=versions.integer(5);previous_duplicate=versions.bytes(6);previous_ordinal=versions.integer(7);previous=std::move(draft);
        }
    }
    Statement applications(impl_->db,"SELECT id,batch,batch_before,batch_after,ruleset_revision,changed_count,evaluated FROM rule_applications WHERE id IN(SELECT rule_application FROM history_order WHERE sequence>(SELECT history_cut FROM document_session))");
    while(applications.row()){
        if(context) context->checkpoint();
        Statement history(impl_->db,"SELECT sequence FROM history_order WHERE rule_application=?");history.text(1,applications.bytes(0));if(!history.row())fail(SQLITE_CORRUPT);
        const auto before=applications.integer(2),after=applications.integer(3),ruleset=applications.integer(4),changed=applications.integer(5),evaluated=applications.integer(6);
        if(ruleset>0){Statement present(impl_->db,"SELECT 1 FROM rule_operations WHERE ruleset_after=?");present.integer(1,ruleset);if(!present.row())fail(SQLITE_CORRUPT);}
        // An unchanged Apply keeps the revision, so the event there belongs to the earlier command.
        if(changed==0){if(after!=before)fail(SQLITE_CORRUPT);}
        else {Statement event(impl_->db,"SELECT kind FROM events WHERE batch=? AND revision=?");event.text(1,applications.bytes(1));event.integer(2,after);if(!event.row() || event.bytes(0)!="apply_rules" || after!=before+1)fail(SQLITE_CORRUPT);Statement count(impl_->db,"SELECT count(*) FROM versions WHERE batch=? AND revision=?");count.text(1,applications.bytes(1));count.integer(2,after);if(!count.row() || count.integer(0)!=changed)fail(SQLITE_CORRUPT);}
        Statement scope_count(impl_->db,"SELECT count(*) FROM records r WHERE r.batch=? AND EXISTS(SELECT 1 FROM versions v WHERE v.batch=r.batch AND v.ordinal=r.ordinal AND v.revision<=?2) AND NOT EXISTS(SELECT 1 FROM accepted_transactions a JOIN operations o ON o.id=a.operation WHERE a.batch=r.batch AND a.ordinal=r.ordinal AND o.revision<?2) AND NOT EXISTS(SELECT 1 FROM generated_source_matches g JOIN ledger_operations l ON l.sequence=g.sequence JOIN history_order h ON h.operation=l.id WHERE g.batch=r.batch AND g.ordinal=r.ordinal AND h.sequence<?3)");scope_count.text(1,applications.bytes(1));scope_count.integer(2,before);scope_count.integer(3,history.integer(0));if(!scope_count.row() || scope_count.integer(0)!=evaluated)fail(SQLITE_CORRUPT);
    }
}
#endif
