namespace {
std::string definition_import_digest(const app::DefinitionImport& request) {
    std::string bytes;
    const auto add=[&](std::string_view v){bytes+=std::to_string(v.size())+":"+std::string(v);};
    const auto number=[&](auto v){add(std::to_string(v));};
    number(request.token.session);number(request.token.epoch);add(request.token.head);
    number(request.categories.size());
    for(const auto& c:request.categories){number(static_cast<int>(c.action));add(c.identity);add(c.name);add(c.parent);number(static_cast<int>(c.kind));number(c.income);}
    number(request.rules.has_value());number(request.rules_revision);
    if(request.rules){number(request.rules->ordered.size());for(const auto& r:request.rules->ordered){add(r.identity);add(r.label);number(r.enabled);number(static_cast<int>(r.scope));number(static_cast<int>(r.input));add(r.expression);for(const auto* v:{&r.actions.payee_template,&r.actions.memo_template,&r.actions.category}){number(v->has_value());if(*v)add(**v);}number(r.actions.inherit_parent);}}
    number(request.reports.has_value());
    if(request.reports){number(request.reports->revision);number(request.reports->definitions.size());for(const auto& r:request.reports->definitions){add(r.id);add(r.name);add(r.graph);number(r.accounts.size());for(auto a:r.accounts)number(a);number(r.categories.size());for(const auto& c:r.categories)add(c);number(r.periods.size());for(const auto& p:r.periods){add(p.preset);add(p.start);add(p.end);}}}
    return session_snapshot::digest(bytes);
}
}
void ImportStore::import_definitions(std::string_view operation,const app::DefinitionImport& request,const app::OperationContext& context) {
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    const auto digest=definition_import_digest(request);
    session_snapshot::retry(impl_->db,operation,"definition-import",digest);
    {Statement done(impl_->db,"SELECT 1 FROM command_completions WHERE id=?");done.text(1,operation);if(done.row())return;}
    if(request.categories.size()>8192 || (!request.rules && !request.reports && request.categories.empty()))throw std::invalid_argument("Select definitions to import.");
    context.checkpoint();
    Atomic transaction(impl_->db);
    if(action_token()!=request.token || action_preparing())throw std::invalid_argument("The file changed. Open the import again before saving.");
    if(request.reports){const auto current=report_catalog();if(request.reports->revision!=current.revision || request.reports->definitions.size()<=current.definitions.size() || !std::equal(current.definitions.begin(),current.definitions.end(),request.reports->definitions.begin()))throw std::invalid_argument("Definition import must retain existing reports unchanged.");}
#ifdef SPENTIT_RULES_STORAGE
    if(request.rules){const auto current=rule_image();if(request.rules_revision!=current.revision || request.rules->ordered.size()<=current.ordered.size())throw std::invalid_argument("Definition import must append new rules.");for(std::size_t i=0;i<current.ordered.size();++i){const auto& before=current.ordered[i];const auto& after=request.rules->ordered[i];if(before.identity!=after.identity || before.enabled!=after.enabled || body_of(before)!=body_of(after))throw std::invalid_argument("Definition import must retain existing rules unchanged.");}}
#endif
    struct Scope {Impl& state;bool verifying;~Scope(){state.verifying=verifying;state.composing_rule_save=false;state.composing_definitions=false;}} scope{*impl_,impl_->verifying};
    impl_->verifying=true;impl_->composing_rule_save=true;impl_->composing_definitions=true;
    std::size_t index=0;
    for(const auto& category:request.categories){
        context.checkpoint();
        if(category.action!=app::CategoryAction::create && category.action!=app::CategoryAction::set_income)throw std::invalid_argument("Invalid imported category action.");
        const auto preview=preview_category_change(category);
        write_category_change(std::string(operation)+"-c"+std::to_string(index++),preview);
    }
    if(request.rules)save_rules(std::string(operation)+"-r",*request.rules,request.rules_revision,context);
    if(request.reports){context.checkpoint();save_reports(*request.reports);}
    session_snapshot::completed(impl_->db,operation,"definition-import",digest);
    impl_->composing_rule_save=false;impl_->composing_definitions=false;
    finish([&]{context.seal();transaction.commit();},true);
}

bool ImportStore::definition_import_completed(std::string_view operation,const app::DefinitionImport& request) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    session_snapshot::retry(impl_->db,operation,"definition-import",definition_import_digest(request));
    Statement done(impl_->db,"SELECT 1 FROM command_completions WHERE id=?");done.text(1,operation);return done.row();
}
