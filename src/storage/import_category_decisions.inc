// A completed preparation may wait for this import-wide decision indefinitely.
// It remains distinct from captured/incomplete preparation and blocks acceptance
// and rule evaluation until the decision and resulting assignments are durable.
std::vector<app::MissingImportCategories> ImportStore::missing_import_categories() const {
    std::vector<app::MissingImportCategories> result;
    Statement groups(impl_->db,"SELECT d.source FROM import_category_decisions d JOIN import_groups g ON g.id=d.source WHERE d.resolved=0 AND g.state=1 ORDER BY d.source");
    while(groups.row()) {
        app::MissingImportCategories item{groups.bytes(0),{}};
        Statement names(impl_->db,"SELECT name FROM import_category_choices WHERE source=? ORDER BY name COLLATE NOCASE,name");names.text(1,item.source);
        while(names.row())item.names.push_back(names.bytes(0));
        result.push_back(std::move(item));
    }
    return result;
}
void ImportStore::stage_import_categories(std::string_view source,const app::OperationContext& context) {
    Statement insert(impl_->db,"INSERT INTO import_category_decisions VALUES(?,0)");insert.text(1,source);insert.done();
    const auto original_account=impl_->selected_account;
    struct Restore {std::int64_t& target;std::int64_t value;~Restore(){target=value;}} restore{impl_->selected_account,original_account};
    const auto catalog=category_catalog();std::vector<std::string> names;std::set<std::string> seen;
    auto add=[&](const std::string& name) {
        context.checkpoint();
        if(!seen.insert(name).second)return;
        if(seen.size()>16384)fail(SQLITE_TOOBIG,"This import has too many distinct category names; split it into smaller files");
        if(name.empty() || qif::transfer_destination(name) || resolve_account_name(name))return;
        const auto same=[&](std::string_view other){try{return app::category_name_matches(other,name);}catch(const std::invalid_argument&){return other==name;}};
        if(std::any_of(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && same(c.name);}))return;
        if(std::none_of(names.begin(),names.end(),same))names.push_back(name);
    };
    Statement members(impl_->db,"SELECT batch,account_id,source_account FROM import_group_accounts WHERE group_id=? ORDER BY source_account");members.text(1,source);
    while(members.row()) {
        impl_->selected_account=members.integer(1);
        for(const auto& category:source_categories(members.bytes(0)))add(category.name);
        const auto parsed=parsed_source(members.bytes(0),[&]{context.checkpoint();});
        for(const auto& row:parsed.records)for(const auto& allocation:row.source_allocation_fields)add(allocation.category);
    }
    for(const auto& name:names){Statement choice(impl_->db,"INSERT INTO import_category_choices VALUES(?,?,NULL)");choice.text(1,source);choice.text(2,name);choice.done();}
    if(names.empty())finish_import_categories(source,context);
}
void ImportStore::resolve_import_categories(std::string_view source,const std::vector<std::string>& selected,app::OperationContext& context) {
    diag::Span phase(diag::Code::import_category_resolution);
    ScopedStatements plans(impl_->db);
    Atomic transaction(impl_->db);
    Statement state(impl_->db,"SELECT d.resolved,g.state FROM import_category_decisions d JOIN import_groups g ON g.id=d.source WHERE d.source=?");state.text(1,source);
    if(!state.row() || state.integer(1)==3)fail(SQLITE_CONSTRAINT,"This import is no longer awaiting categories");
    std::set<std::string> chosen(selected.begin(),selected.end());if(chosen.size()!=selected.size())fail(SQLITE_CONSTRAINT,"Select each category once");
    Statement rows(impl_->db,"SELECT name,chosen FROM import_category_choices WHERE source=?");rows.text(1,source);
    std::size_t found=0;while(rows.row()) {
        const bool include=chosen.contains(rows.bytes(0));found+=include;
        if(state.integer(0)==1 && (rows.null(1) || (rows.integer(1)!=0)!=include))fail(SQLITE_CONSTRAINT,"The import category decision changed on retry");
    }
    if(found!=chosen.size())fail(SQLITE_CONSTRAINT,"The category list changed. Open it again.");
    if(state.integer(0)==1){context.seal();transaction.commit();return;}
    Statement names(impl_->db,"SELECT name FROM import_category_choices WHERE source=? ORDER BY name");names.text(1,source);
    std::size_t ordinal=0;
    while(names.row()) {
        context.checkpoint("Creating selected categories");const auto name=names.bytes(0);const bool include=chosen.contains(name);
        if(include) {
            const auto catalog=category_catalog();
            const auto existing=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && app::category_name_matches(c.name,name);});
            if(existing==catalog.categories.end()) {
                const auto id="import-cat-"+std::string(source)+"-"+std::to_string(ordinal);
                write_category_change(id,preview_category_change({app::CategoryAction::create,id,name,{},CategoryKind::leaf}));
            } else if(existing->archived)fail(SQLITE_CONSTRAINT,"Reactivate the selected category before using it");
        }
        Statement choice(impl_->db,"UPDATE import_category_choices SET chosen=? WHERE source=? AND name=?");choice.integer(1,include?1:0);choice.text(2,source);choice.text(3,name);choice.done();++ordinal;
    }
    finish_import_categories(source,context);
    context.seal();finish([&]{transaction.commit();});
}
void ImportStore::finish_import_categories(std::string_view source,const app::OperationContext& context) {
    // Also cover automatic resolution when preparation finds no missing names.
    ScopedStatements plans(impl_->db);
    const auto original_account=impl_->selected_account;
    struct Restore {std::int64_t& target;std::int64_t value;~Restore(){target=value;}} restore{impl_->selected_account,original_account};
    const auto catalog=category_catalog();
    std::vector<std::string> cleared;
    Statement choices(impl_->db,"SELECT name,chosen FROM import_category_choices WHERE source=?");choices.text(1,source);
    while(choices.row()){if(choices.null(1))fail(SQLITE_CONSTRAINT,"Choose the imported categories first");if(choices.integer(1)==0)cleared.push_back(choices.bytes(0));}
    Statement members(impl_->db,"SELECT batch,account_id,source_account FROM import_group_accounts WHERE group_id=? ORDER BY source_account");members.text(1,source);
    while(members.row()) {
        const auto batch_id=members.bytes(0);impl_->selected_account=members.integer(1);
        const auto before=batch(impl_->db,batch_id);bool changed=false;
        {
            diag::Span assignment(diag::Code::import_category_assign);
            for(std::size_t i=0;i<before.count;++i) {
                context.checkpoint("Assigning imported categories");
                const imports::Occurrence id{batch_id,i};const auto row=load_row(id,false);if(!row.draft || transaction_accepted(id) || row.matched_generated)continue;
                auto draft=*row.draft;auto allocations=draft.transaction.allocations();bool row_changed=false;
                for(auto& a:allocations) {
                    if(a.category_owner!=FieldOwnership::source || a.category_binding || a.category.empty() || qif::transfer_destination(a.category) || resolve_account_name(a.category))continue;
                    const auto same=[&](std::string_view other){try{return app::category_name_matches(other,a.category);}catch(const std::invalid_argument&){return other==a.category;}};
                    if(std::any_of(cleared.begin(),cleared.end(),same)) {a.category.clear();a.category_owner=FieldOwnership::manual;a.category_rule.reset();row_changed=true;continue;}
                    const auto found=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && same(c.name);});
                    if(found==catalog.categories.end())fail(SQLITE_CONSTRAINT,"Choose how to use every imported category");
                    if(found->archived)fail(SQLITE_CONSTRAINT,"Reactivate the imported category before using it");
                    a.category=found->name;a.category_binding=CategoryBinding{found->identity,catalog.revision};row_changed=true;
                }
                if(row_changed) {
                    if(before.revision==INT64_MAX)fail(SQLITE_FULL);
                    draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),std::move(allocations));
                    version(impl_->db,id,before.revision+1,draft,row.decision,false);changed=true;
                }
            }
            if(changed)advance(impl_->db,batch_id,before.revision,"save");
        }
#ifdef SPENTIT_RULES_STORAGE
        if(rules_supported()) {
            diag::Span evaluation(diag::Code::import_category_rules);
            const auto revision=ruleset_revision(impl_->db);rules::RuleSet ruleset(rules_at(impl_->db,revision));
            const auto prepared=batch(impl_->db,batch_id);std::size_t evaluated=0,modified=0;
            for(std::size_t i=0;i<prepared.count;++i) {
                context.checkpoint("Categorizing imported transactions");const imports::Occurrence id{batch_id,i};const auto row=load_row(id,false);
                if(!row.draft || row.decision.disposition==imports::Disposition::duplicate || row.matched_generated || transaction_accepted(id))continue;
                auto proposal=[&]{try{return app::preview_rules(ruleset,catalog,inputs_from({row},*this),false,[&]{return context.state.load()==app::OperationContext::requested;},true).front();}catch(const rules::RegexError& error){if(error.kind()==rules::RegexFailure::cancelled)throw app::OperationCancelled();throw;}}();++evaluated;
                if(app::rule_field_diffs(*row.draft,proposal).empty())continue;
                if(prepared.revision==INT64_MAX)fail(SQLITE_FULL);
                check_rule_transition(*row.draft,proposal.after,true,false);
                version(impl_->db,id,prepared.revision+1,proposal.after,row.decision,false,revision);++modified;
            }
            if(modified) {
                advance(impl_->db,batch_id,prepared.revision,"apply_rules");
                Statement receipt(impl_->db,"INSERT INTO rule_applications VALUES(?,?,?,?,?,?,?,?,?)");receipt.text(1,"auto-rules-"+std::string(source)+"-"+std::to_string(members.integer(2)));receipt.text(2,batch_id);receipt.integer(3,prepared.revision);receipt.integer(4,prepared.revision+1);receipt.integer(5,revision);receipt.integer(6,catalog.revision);receipt.integer(7,0);receipt.integer(8,integer(evaluated));receipt.integer(9,integer(modified));receipt.done();
            }
        }
#endif
    }
    Statement done(impl_->db,"UPDATE import_category_decisions SET resolved=1 WHERE source=?");done.text(1,source);done.done();
}

imports::DraftRow ImportStore::resolve_repaired_categories(std::string_view batch_id,const imports::DraftRow& input) const {
    Statement decision(impl_->db,"SELECT d.source FROM import_category_decisions d JOIN batches b ON b.source_id=d.source WHERE b.id=? AND d.resolved=1");decision.text(1,batch_id);if(!decision.row())return input;
    std::vector<std::string> cleared;Statement names(impl_->db,"SELECT name FROM import_category_choices WHERE source=? AND chosen=0");names.text(1,decision.bytes(0));while(names.row())cleared.push_back(names.bytes(0));
    const auto catalog=category_catalog();auto allocations=input.transaction.allocations();
    for(auto& a:allocations) {
        if(a.category_owner!=FieldOwnership::source || a.category_binding || a.category.empty() || qif::transfer_destination(a.category) || resolve_account_name(a.category))continue;
        const auto same=[&](std::string_view other){try{return app::category_name_matches(other,a.category);}catch(const std::invalid_argument&){return other==a.category;}};
        if(std::any_of(cleared.begin(),cleared.end(),same)){a.category.clear();a.category_owner=FieldOwnership::manual;continue;}
        const auto found=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& c){return !c.deleted && !c.archived && same(c.name);});
        if(found==catalog.categories.end())fail(SQLITE_CONSTRAINT,"Choose an available category for the repaired transaction");
        a.category=found->name;a.category_binding=CategoryBinding{found->identity,catalog.revision};
    }
    auto result=input;result.transaction=input.transaction.corrected(input.transaction.effective_total(),std::move(allocations));return result;
}
