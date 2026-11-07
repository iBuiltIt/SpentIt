#ifndef SPENTIT_RULES_STORAGE
std::string ImportStore::rule_reapply_group(std::string_view) const { return {}; }
app::RuleReapplyPreview ImportStore::preview_rule_reapply(bool,std::string_view,const app::OperationContext&) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
app::RuleReapplyReceipt ImportStore::apply_rule_reapply(std::string_view,const app::RuleReapplyRequest&,const app::OperationContext&) { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
std::optional<app::RuleReapplyReceipt> ImportStore::rule_reapply_outcome(std::string_view) const { fail(SQLITE_SCHEMA,"Rules are not enabled for this dataset"); }
void ImportStore::verify_rule_reapply(const app::OperationContext*) const {}
#else
namespace {
constexpr const char* reapply_undo="Newer transaction, transfer relationship, or source association prevents Undo";
constexpr const char* reapply_refresh="Preview changed; refresh before applying";
constexpr const char* reapply_money="This reapply would change committed money";
constexpr const char* reapply_missing="Rule reapply was not found";
constexpr const char* reapply_limit="Too many records in one rule preview";
constexpr const char* reapply_membership="Invalid rule reapply Undo membership";
constexpr const char* head_where=
    "FROM entry_heads h JOIN entry_values e ON e.batch=h.batch AND e.ordinal=h.ordinal AND e.revision=h.revision "
    "JOIN batches b ON b.id=h.batch WHERE e.active=1 AND b.account_id=spentit_account() AND b.state=1 AND substr(h.batch,1,10)<>'~transfer-'";
bool reapply_schema(sqlite3* db){return scalar(db,"PRAGMA user_version")=="64";}
std::string protection_stamp(const std::vector<app::TransferView>& links,std::int64_t account){
    std::set<std::pair<imports::Occurrence,std::string>> keys;
    for(const auto& value:links){
        if(value.link.unresolved)continue;
        for(const auto* side:{&value.link.origin,&value.link.destination}){
            if(side->state==transfers::State::removed || side->endpoint.account!=account || side->endpoint.entry.batch.starts_with("~transfer-"))continue;
            keys.emplace(side->endpoint.entry,side->endpoint.allocation);
        }
    }
    std::string stamp;
    for(const auto& [entry,allocation]:keys){stamp+=entry.batch;stamp.push_back('\n');stamp+=std::to_string(entry.ordinal);stamp.push_back('\n');stamp+=allocation;stamp.push_back('\n');}
    return stamp;
}
void reapply_keep(sqlite3* db,const imports::DraftRow& before,const imports::DraftRow& after,std::int64_t catalog){
    if(before.transaction.effective_total()!=after.transaction.effective_total() || before.transaction.original_imported_amount()!=after.transaction.original_imported_amount())fail(SQLITE_CONSTRAINT,reapply_money);
    if((before.remainder_allocation!=after.remainder_allocation || before.manual_split_amounts!=after.manual_split_amounts) || before.date!=after.date)fail(SQLITE_CONSTRAINT,rules_drift);
    const auto& left=before.transaction.allocations();const auto& right=after.transaction.allocations();
    if(left.size()!=right.size())fail(SQLITE_CONSTRAINT,rules_drift);
    for(std::size_t index=0;index<left.size();++index){
        if(left[index].identity!=right[index].identity || left[index].source_ordinal!=right[index].source_ordinal || left[index].amount!=right[index].amount)fail(SQLITE_CONSTRAINT,rules_drift);
        const bool same_category=left[index].category==right[index].category && left[index].category_binding==right[index].category_binding;
        if(!same_category && right[index].category_owner==FieldOwnership::rule && right[index].category_binding && right[index].category_binding->catalog_revision!=catalog)fail(SQLITE_CONSTRAINT,rules_drift);
        if(right[index].category_binding)validate_binding(db,right[index]);
    }
}
std::string skip_note(const std::string& category,const std::string& payee,const std::string& allocation){
    if(category.empty())return "Left the blank category on "+payee+" ("+allocation+"). That category stays because it is a transfer link.";
    return "Left "+category+" on "+payee+" ("+allocation+"). That category stays because it is a transfer link.";
}
}
std::string ImportStore::rule_reapply_group(std::string_view operation) const {
    if(!reapply_schema(impl_->db))return {};
    Statement group(impl_->db,"SELECT operation FROM rule_reapplication_members WHERE entry_operation=?");group.text(1,operation);
    return group.row()?group.bytes(0):std::string{};
}
std::optional<app::RuleReapplyReceipt> ImportStore::rule_reapply_outcome(std::string_view operation) const {
    if(!rules_supported())fail(SQLITE_SCHEMA,rules_dataset);
    if(!impl_->verifying && !sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    if(!impl_->verifying)session_snapshot::expired(impl_->db,operation,"reapply");
    Statement group(impl_->db,"SELECT account,ruleset_revision,catalog_revision,replace_manual,evaluated,changed_count,preview_sequence,undo_of FROM rule_reapplications WHERE id=?");group.text(1,operation);
    if(!group.row()){if(command_taken(impl_->db,operation))fail(SQLITE_CONSTRAINT,other_command);return {};}
    const auto account_id=group.integer(0);const auto ruleset=group.integer(1);const auto catalog=group.integer(2);const auto replace=group.integer(3)!=0;const auto evaluated=group.integer(4);const auto changed=group.integer(5);const auto sequence=group.integer(6);const auto undo=group.null(7)?std::string{}:group.bytes(7);
    if(evaluated<0 || changed<1 || changed>evaluated || changed>4096)fail(SQLITE_CORRUPT,reapply_membership);
    Statement owner(impl_->db,"SELECT o.batch FROM rule_reapplication_members m JOIN ledger_operations o ON o.id=m.entry_operation WHERE m.operation=? ORDER BY m.position LIMIT 1");owner.text(1,operation);
    if(!owner.row())fail(SQLITE_CORRUPT,reapply_membership);
    require_account(owner.bytes(0));
    if(account_for_batch(owner.bytes(0)).identity!=account_id)fail(SQLITE_CORRUPT,"Rule reapply account does not match its members");
    const auto currency=account_for_batch(owner.bytes(0)).currency;const Money zero(0,currency);
    app::RuleReapplyReceipt receipt{std::string(operation),{ruleset,catalog,replace,undo,sequence,static_cast<std::size_t>(evaluated),static_cast<std::size_t>(changed)},{},zero,zero,zero};
    Statement members(impl_->db,"SELECT position,entry_operation FROM rule_reapplication_members WHERE operation=? ORDER BY position");members.text(1,operation);
    while(members.row()){
        if(members.integer(0)!=integer(receipt.entries.size()) || receipt.entries.size()>=4096)fail(SQLITE_CORRUPT,reapply_membership);
        const auto entry=committed_change_outcome(members.bytes(1));if(!entry)fail(SQLITE_CORRUPT,"Missing rule reapply member history");
        if(account_for_batch(entry->identity.batch).identity!=account_id)fail(SQLITE_CORRUPT,"Rule reapply members span accounts");
        Statement stored(impl_->db,"SELECT preview_sequence,catalog_revision,kind FROM ledger_operations WHERE id=?");stored.text(1,entry->operation);
        if(!stored.row() || stored.integer(0)!=sequence+integer(receipt.entries.size()) || stored.integer(1)!=catalog)fail(SQLITE_CORRUPT,"Rule reapply history is not contiguous");
        if(entry->sequence!=sequence+integer(receipt.entries.size())+1)fail(SQLITE_CORRUPT,"Rule reapply history is not contiguous");
        if(entry->change.units()!=0 || entry->before!=entry->after)fail(SQLITE_CORRUPT,reapply_money);
        const bool forward=undo.empty();
        if(forward){if(entry->kind!=app::ChangeKind::correct || !entry->undo_of.empty() || stored.integer(2)!=static_cast<int>(app::ChangeKind::correct))fail(SQLITE_CORRUPT,"Rule reapply Undo used a correction");}
        else if(entry->kind!=app::ChangeKind::undo || entry->undo_of.empty() || stored.integer(2)!=static_cast<int>(app::ChangeKind::undo))fail(SQLITE_CORRUPT,reapply_membership);
        if(receipt.entries.empty()){if(entry->operation!=operation)fail(SQLITE_CORRUPT);receipt.before=entry->before;}
        else if(entry->before!=receipt.after)fail(SQLITE_CORRUPT,"Rule reapply history is not contiguous");
        receipt.after=entry->after;receipt.entries.push_back(*entry);
    }
    if(integer(receipt.entries.size())!=changed)fail(SQLITE_CORRUPT,reapply_membership);
    if(receipt.before!=receipt.after)fail(SQLITE_CORRUPT,reapply_money);
    receipt.change=zero;
    if(!undo.empty()){
        if(undo==operation)fail(SQLITE_CORRUPT,reapply_membership);
        Statement prior(impl_->db,"SELECT preview_sequence FROM rule_reapplications WHERE id=?");prior.text(1,undo);
        if(!prior.row() || prior.integer(0)>=sequence)fail(SQLITE_CORRUPT,reapply_membership);
        const auto original=rule_reapply_outcome(undo);if(!original)fail(SQLITE_CORRUPT,reapply_membership);
        if(original->entries.size()!=receipt.entries.size() || original->request.ruleset_revision!=ruleset || original->request.replace_manual!=replace || original->request.evaluated!=receipt.request.evaluated || original->request.changed!=receipt.request.changed)fail(SQLITE_CORRUPT,reapply_membership);
        for(std::size_t index=0;index<receipt.entries.size();++index){
            if(receipt.entries[index].undo_of!=original->entries[index].operation || receipt.entries[index].identity!=original->entries[index].identity || !same_draft(receipt.entries[index].after_entry.draft,original->entries[index].before_entry.draft))fail(SQLITE_CORRUPT,reapply_membership);
        }
    }
    return receipt;
}
app::RuleReapplyPreview ImportStore::preview_rule_reapply(bool replace_manual,std::string_view undo_of,const app::OperationContext& context) const {
    if(!rules_supported())fail(SQLITE_SCHEMA,rules_dataset);
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    context.checkpoint("Loading committed rows");
    const auto before=committed_balance();const Money zero(0,before.currency());
    const auto sequence=financial_sequence(impl_->db);const auto catalog=catalog_sequence(impl_->db);
    const auto selected=account().identity;
    if(!undo_of.empty()){
        require_session_history(undo_of);
        const auto original=rule_reapply_outcome(undo_of);if(!original)fail(SQLITE_CONSTRAINT,reapply_missing);
        if(original->request.changed!=original->entries.size())fail(SQLITE_CORRUPT,reapply_membership);
        if(sequence>INT64_MAX-integer(original->entries.size()))fail(SQLITE_FULL,"History capacity exhausted");
        app::RuleReapplyPreview preview{{original->request.ruleset_revision,catalog,original->request.replace_manual,std::string(undo_of),sequence,original->request.evaluated,original->request.changed},before,zero,before,{},{},{}};
        for(const auto& member:original->entries){
            context.checkpoint("Loading committed rows");
            const auto current=committed_entry(member.identity);
            if(!current.current.active || current.current.revision!=member.after_entry.revision || current.token.dependencies!=member.result_token.dependencies)fail(SQLITE_CONSTRAINT,reapply_undo);
            if(later_relationship_blocks(member.identity,member.operation))fail(SQLITE_CONSTRAINT,reapply_undo);
            if(current.current.revision==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
            if(!member.before_entry.active || member.before_entry.draft.transaction.effective_total()!=current.current.draft.transaction.effective_total())fail(SQLITE_CONSTRAINT,reapply_money);
            app::RuleProposal proposal{member.identity,member.before_entry.draft,{},{}};
            auto diffs=app::rule_field_diffs(current.current.draft,proposal);
            preview.records.push_back({member.identity,current.current.draft,member.before_entry.draft,std::move(diffs),{},current.token,member.operation});
        }
        if(preview.records.size()!=original->entries.size())fail(SQLITE_CORRUPT,reapply_membership);
        return preview;
    }
    Statement counted(impl_->db,std::string("SELECT count(*) ").append(head_where).c_str());
    if(!counted.row())fail(SQLITE_CORRUPT);
    if(counted.integer(0)<0 || counted.integer(0)>4096)fail(SQLITE_TOOBIG,reapply_limit);
    std::vector<app::CommittedEntry> entries;
    Statement heads(impl_->db,std::string("SELECT h.batch,h.ordinal ").append(head_where).append(" ORDER BY h.batch,h.ordinal").c_str());
    while(heads.row()){
        context.checkpoint("Loading committed rows");
        imports::Occurrence id{heads.bytes(0),static_cast<std::size_t>(heads.integer(1))};
        entries.push_back(committed_entry(id));
        if(entries.size()>4096)fail(SQLITE_TOOBIG,reapply_limit);
    }
    if(integer(entries.size())!=counted.integer(0))fail(SQLITE_CORRUPT);
    std::vector<app::RowView> rows;
    for(const auto& entry:entries)rows.push_back({entry.identity,entry.source,entry.current.draft,{},{},false,false});
    context.checkpoint("Evaluating rules");
    std::vector<app::RuleProposal> proposals;
    try{
        rules::RuleSet compiled(rules_at(impl_->db,ruleset_revision(impl_->db)));
        auto inputs=inputs_from(rows,*this);
        const auto links=all_transfers();
        for(auto& input:inputs)for(const auto& link:links)if(!link.link.unresolved)
            for(const auto* side:{&link.link.origin,&link.link.destination})if(side->state!=transfers::State::removed && side->endpoint.account==selected && side->endpoint.entry==input.identity)
                input.transfer_allocations.push_back(side->endpoint.allocation);
        for(auto& input:inputs){std::sort(input.transfer_allocations.begin(),input.transfer_allocations.end());input.transfer_allocations.erase(std::unique(input.transfer_allocations.begin(),input.transfer_allocations.end()),input.transfer_allocations.end());}
        proposals=app::preview_rules(compiled,category_catalog(),inputs,replace_manual,[&]{return context.state.load()==app::OperationContext::requested;},true);
    }catch(const rules::RegexError& error){
        if(error.kind()==rules::RegexFailure::cancelled)throw app::OperationCancelled(error.what());
        throw;
    }catch(const std::invalid_argument&){fail(SQLITE_CONSTRAINT,rules_drift);}
    if(proposals.size()!=entries.size())fail(SQLITE_CORRUPT);
    const auto live=protection_stamp(all_transfers(),selected);
    std::set<std::pair<imports::Occurrence,std::string>> linked;
    for(const auto& value:all_transfers()){
        if(value.link.unresolved)continue;
        for(const auto* side:{&value.link.origin,&value.link.destination}){
            if(side->state==transfers::State::removed || side->endpoint.account!=selected || side->endpoint.entry.batch.starts_with("~transfer-"))continue;
            linked.emplace(side->endpoint.entry,side->endpoint.allocation);
        }
    }
    app::RuleReapplyPreview preview{{ruleset_revision(impl_->db),catalog,replace_manual,{},sequence,entries.size(),0},before,zero,before,{},{},live};
    for(std::size_t index=0;index<entries.size();++index){
        const auto& entry=entries[index];auto after=proposals[index].after;auto explanations=proposals[index].explanations;
        auto allocations=after.transaction.allocations();const auto& original=entry.current.draft.transaction.allocations();
        if(allocations.size()!=original.size())fail(SQLITE_CONSTRAINT,rules_drift);
        std::set<std::size_t> skipped;
        for(std::size_t position=0;position<allocations.size();++position){
            const bool category_changed=allocations[position].category!=original[position].category || allocations[position].category_binding!=original[position].category_binding;
            const bool protected_match=std::any_of(explanations.begin(),explanations.end(),[&](const auto& e){return e.allocation==position && e.effect==rules::Effect::transfer_protected;});
            if((!category_changed && !protected_match) || !linked.contains({entry.identity,original[position].identity}))continue;
            allocations[position].category=original[position].category;
            allocations[position].category_binding=original[position].category_binding;
            allocations[position].category_owner=original[position].category_owner;
            allocations[position].category_rule=original[position].category_rule;
            skipped.insert(position);
            preview.skips.push_back({entry.identity,original[position].identity,entry.current.draft.payee,original[position].category,skip_note(original[position].category,entry.current.draft.payee,original[position].identity)});
        }
        if(!skipped.empty()){
            std::vector<rules::Explanation> kept;
            for(const auto& item:explanations)if(!(item.field=="category" && item.allocation && skipped.contains(*item.allocation)))kept.push_back(item);
            explanations=std::move(kept);
        }
        after.transaction=after.transaction.corrected(after.transaction.effective_total(),std::move(allocations));
        reapply_keep(impl_->db,entry.current.draft,after,catalog);
        imports::validate_draft(entry.source,after);
        app::RuleProposal proposal{entry.identity,after,{},explanations};
        auto diffs=app::rule_field_diffs(entry.current.draft,proposal);
        if(diffs.empty())continue;
        if(entry.current.revision==INT64_MAX)fail(SQLITE_FULL,"Revision capacity exhausted");
        check_rule_transition(entry.current.draft,after,true,replace_manual);
        preview.records.push_back({entry.identity,entry.current.draft,std::move(after),std::move(diffs),std::move(explanations),entry.token,{}});
    }
    if(!preview.records.empty() && sequence>INT64_MAX-integer(preview.records.size()))fail(SQLITE_FULL,"History capacity exhausted");
    preview.request.changed=preview.records.size();
    return preview;
}
app::RuleReapplyReceipt ImportStore::apply_rule_reapply(std::string_view operation,const app::RuleReapplyRequest& request,const app::OperationContext& context) {
    if(!rules_supported())fail(SQLITE_SCHEMA,rules_dataset);
    identifier(operation);
    if(operation.size()>96)fail(SQLITE_CONSTRAINT,"Rule reapply operation identity is too long");
    transfer_codec::Writer encoding;encoding.number(request.ruleset_revision);encoding.number(request.catalog_revision);encoding.number(request.replace_manual);encoding.text(request.undo_of);encoding.number(request.preview_sequence);encoding.number(integer(request.evaluated));encoding.number(integer(request.changed));
    const auto request_hash=session_snapshot::digest(encoding.bytes);session_snapshot::retry(impl_->db,operation,"reapply",request_hash);
    if(const auto prior=rule_reapply_outcome(operation)){if(prior->request!=request)fail(SQLITE_CONSTRAINT,reused_command);return *prior;}
    if(command_taken(impl_->db,operation))fail(SQLITE_CONSTRAINT,other_command);
    const auto preview=preview_rule_reapply(request.replace_manual,request.undo_of,context);
    if(preview.request!=request)fail(SQLITE_CONSTRAINT,refresh_rules);
    if(preview.records.empty()){
        if(!request.undo_of.empty())fail(SQLITE_CORRUPT,reapply_membership);
        return {"",request,{},preview.before,Money(0,preview.before.currency()),preview.after};
    }
    const bool forward=request.undo_of.empty();
    std::vector<std::string> operations;std::vector<std::optional<std::int64_t>> rulesets;
    for(std::size_t index=0;index<preview.records.size();++index){
        auto id=index==0?std::string(operation):std::string(operation)+"-"+std::to_string(index);
        identifier(id);
        if(command_taken(impl_->db,id))fail(SQLITE_CONSTRAINT,other_command);
        const auto& record=preview.records[index];
        std::optional<std::int64_t> ruleset;
        if(!forward){
            if(record.prior_operation.empty())fail(SQLITE_CORRUPT,reapply_membership);
            if(any_mark(record.after)){
                const auto original=committed_change_outcome(record.prior_operation);if(!original)fail(SQLITE_CORRUPT,reapply_membership);
                Statement mark(impl_->db,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?");
                mark.text(1,record.identity.batch);mark.integer(2,integer(record.identity.ordinal));mark.integer(3,original->before_entry.revision);
                if(!mark.row())fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");
                ruleset=mark.integer(0);
            }
        }else if(any_mark(record.after)){
            if(request.ruleset_revision<1)fail(SQLITE_CONSTRAINT,"Ordinary save cannot introduce rule ownership");
            ruleset=request.ruleset_revision;
        }
        operations.push_back(std::move(id));rulesets.push_back(ruleset);
    }
    Atomic transaction(impl_->db);
    if(financial_sequence(impl_->db)!=request.preview_sequence || catalog_sequence(impl_->db)!=request.catalog_revision || committed_balance()!=preview.before)fail(SQLITE_CONSTRAINT,reapply_refresh);
    if(forward){if(protection_stamp(all_transfers(),account().identity)!=preview.protection)fail(SQLITE_CONSTRAINT,reapply_refresh);}
    const Money zero(0,preview.before.currency());
    for(std::size_t index=0;index<preview.records.size();++index){
        const auto& record=preview.records[index];
        if(committed_entry(record.identity).token!=record.expected)fail(SQLITE_CONSTRAINT,reapply_refresh);
        if(!forward && later_relationship_blocks(record.identity,record.prior_operation))fail(SQLITE_CONSTRAINT,reapply_refresh);
        app::EntrySnapshot snapshot{record.expected.revision+1,true,record.after};
        store_entry(impl_->db,record.identity,snapshot,rulesets[index]);
        append_financial(impl_->db,operations[index],forward?app::ChangeKind::correct:app::ChangeKind::undo,record.identity,record.expected.revision,snapshot.revision,preview.before,zero,preview.before,forward?std::string_view{}:std::string_view{record.prior_operation},request.preview_sequence+integer(index),request.catalog_revision);
        for(const auto& dependency:record.expected.dependencies){Statement row(impl_->db,"INSERT INTO change_dependencies VALUES(?,?,?,?)");row.text(1,operations[index]);row.text(2,dependency.batch);row.integer(3,dependency.revision);row.integer(4,static_cast<int>(dependency.state));row.done();}
    }
    Statement group(impl_->db,"INSERT INTO rule_reapplications VALUES(?,?,?,?,?,?,?,?,?)");
    group.text(1,operation);group.integer(2,account().identity);group.integer(3,request.ruleset_revision);group.integer(4,request.catalog_revision);group.integer(5,request.replace_manual?1:0);group.integer(6,integer(request.evaluated));group.integer(7,integer(request.changed));group.integer(8,request.preview_sequence);
    if(!request.undo_of.empty())group.text(9,request.undo_of);
    group.done();
    for(std::size_t index=0;index<operations.size();++index){Statement member(impl_->db,"INSERT INTO rule_reapplication_members VALUES(?,?,?)");member.text(1,operation);member.integer(2,integer(index));member.text(3,operations[index]);member.done();}
    session_snapshot::completed(impl_->db,operation,"reapply",request_hash);
    finish([&]{transaction.commit();});
    return *rule_reapply_outcome(operation);
}
void ImportStore::verify_rule_reapply(const app::OperationContext* context) const {
    if(!reapply_schema(impl_->db))return;
    Statement groups(impl_->db,"SELECT id FROM rule_reapplications ORDER BY preview_sequence");
    while(groups.row()){
        if(context)context->checkpoint("Verifying rule reapply history");
        const auto receipt=*rule_reapply_outcome(groups.bytes(0));
        for(const auto& entry:receipt.entries){
            if(context)context->checkpoint();
            const auto& before=entry.before_entry;const auto& after=entry.after_entry;
            if(!before.active || !after.active || before.draft.date!=after.draft.date)fail(SQLITE_CORRUPT);
            if(entry.change.units()!=0 || before.draft.transaction.effective_total()!=after.draft.transaction.effective_total() || before.draft.transaction.original_imported_amount()!=after.draft.transaction.original_imported_amount())fail(SQLITE_CORRUPT,reapply_money);
            const auto& left=before.draft.transaction.allocations();const auto& right=after.draft.transaction.allocations();
            if(left.size()!=right.size())fail(SQLITE_CORRUPT);
            for(std::size_t index=0;index<left.size();++index){
                if(left[index].identity!=right[index].identity || left[index].source_ordinal!=right[index].source_ordinal || left[index].amount!=right[index].amount)fail(SQLITE_CORRUPT);
                if(left[index].category==right[index].category && left[index].category_binding==right[index].category_binding)continue;
                Statement versions(impl_->db,"SELECT payload FROM transfer_versions v WHERE v.sequence<?1 AND v.sequence=(SELECT max(sequence) FROM transfer_versions WHERE identity=v.identity AND sequence<?1)");
                versions.integer(1,entry.sequence);
                while(versions.row()){
                    const auto value=transfer_codec::view(versions.bytes(0));
                    if(value.link.unresolved)continue;
                    for(const auto* side:{&value.link.origin,&value.link.destination}){
                        if(side->state==transfers::State::removed)continue;
                        if(side->endpoint.entry==entry.identity && side->endpoint.allocation==left[index].identity)fail(SQLITE_CORRUPT,"Rule reapply changed a transfer category");
                    }
                }
            }
        }
    }
}
#endif
