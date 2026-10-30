// Names are derived from immutable creation, rename and confirmed mapping
// evidence. No second editable catalog or installation-local alias state exists.
std::vector<app::AccountAlias> ImportStore::account_aliases() const {
    const auto changes=sqlite3_total_changes64(impl_->db);
    const bool cacheable=sqlite3_get_autocommit(impl_->db) && !impl_->verifying;
    if(cacheable && impl_->alias_changes==changes)return impl_->alias_cache;
    auto* scope=ScopedStatements::current(impl_->db);
    std::string signature;
    if(scope){
        signature=scalar(impl_->db,"SELECT (SELECT count(*) FROM retained_account_aliases)||':'||(SELECT count(*) FROM account)||':'||(SELECT count(*) FROM account_operations)||':'||(SELECT count(*) FROM import_group_accounts)||':'||(SELECT count(*) FROM retired_import_accounts)||':'||(SELECT count(*) FROM account_retirements)||':'||(SELECT revision FROM category_state WHERE singleton=1)");
        if(scope->aliases && scope->alias_signature==signature)return *scope->aliases;
        scope->aliases.reset();scope->alias_resolutions.clear();scope->alias_signature=signature;
    }
    impl_->alias_changes=-1;impl_->alias_resolutions.clear();
    std::set<std::int64_t> live;
    Statement accounts(impl_->db,"SELECT singleton FROM account WHERE singleton NOT IN(SELECT account_id FROM retired_import_accounts) AND singleton NOT IN(SELECT account_id FROM account_retirements)");
    while(accounts.row())live.insert(accounts.integer(0));
    std::vector<app::AccountAlias> result;
    std::set<std::pair<std::int64_t,std::string>> unique;std::size_t bytes=0;
    const auto add=[&](std::int64_t id,std::string name){if(live.contains(id) && !name.empty() && unique.emplace(id,name).second){if(result.size()>=100000 || name.size()>64*1024*1024-bytes)fail(SQLITE_TOOBIG,"Account alias limit exceeded");bytes+=name.size();result.push_back({id,std::move(name)});}};
    Statement names(impl_->db,"SELECT singleton,name FROM account UNION SELECT identity,name FROM account_creations UNION SELECT account,name FROM account_operations WHERE kind=0 UNION SELECT account,prior_name FROM account_operations WHERE kind=0 UNION SELECT account,name FROM retained_account_aliases");
    while(names.row())add(names.integer(0),names.bytes(1));
    Statement sources(impl_->db,"SELECT DISTINCT s.id,s.bytes FROM sources s JOIN source_headers h ON h.source_id=s.id JOIN import_group_accounts m ON m.group_id=s.id WHERE h.format='qif'");
    while(sources.row()) {
        const auto doc=indexed_source(impl_->db,sources.bytes(0),sources.bytes(1));
        if(!doc.explicit_accounts)continue;
        Statement mapping(impl_->db,"SELECT source_account,account_id FROM import_group_accounts WHERE group_id=?");mapping.text(1,sources.bytes(0));
        while(mapping.row()){const auto n=static_cast<std::size_t>(mapping.integer(0));if(n>=doc.accounts.size())fail(SQLITE_CORRUPT);add(mapping.integer(1),doc.accounts[n].name);}
    }
    if(scope)scope->aliases=result;
    if(cacheable){impl_->alias_cache=result;impl_->alias_changes=changes;}
    return result;
}
std::optional<std::int64_t> ImportStore::resolve_account_name(std::string_view name) const {
    if(name.empty())return {};
    // Refresh the cache before looking at name results; writes and rollbacks
    // invalidate it, including mappings made inside the current transaction.
    const auto names=account_aliases();
    const bool cacheable=sqlite3_get_autocommit(impl_->db) && !impl_->verifying;
    auto* scope=ScopedStatements::current(impl_->db);
    auto* resolutions=cacheable?&impl_->alias_resolutions:scope?&scope->alias_resolutions:nullptr;
    if(resolutions){const auto at=resolutions->find(name);if(at!=resolutions->end())return at->second;}
    std::optional<std::int64_t> found;
    for(const auto& alias:names)if(app::account_name_matches(alias.name,name)) {
        if(found && *found!=alias.account)fail(SQLITE_CONSTRAINT,"Account name or alias identifies multiple accounts; review the confirmed mappings");
        found=alias.account;
    }
    if(resolutions && resolutions->size()<100000)resolutions->emplace(name,found);
    return found;
}
void ImportStore::require_account_name(std::string_view name,std::int64_t account_id) const {
    for(const auto& alias:account_aliases())if(alias.account!=account_id && app::account_name_matches(alias.name,name))
        fail(SQLITE_CONSTRAINT,"That name is already an account name or alias; choose a distinct account name or map to the existing account");
    for(const auto& c:category_catalog().categories)if(!c.deleted && app::account_name_matches(c.name,name))
        fail(SQLITE_CONSTRAINT,"That name belongs to an ordinary category. Rename the category before using this account name or alias.");
}
std::optional<std::string> ImportStore::transfer_name(std::string_view category) const {
    if(const auto bracketed=qif::transfer_destination(category))return std::string(*bracketed);
    try {if(resolve_account_name(category))return std::string(category);}
    catch(const ImportError& error){if(error.code()!=SQLITE_CONSTRAINT)throw;return std::string(category);} // Ambiguity is a reviewable transfer blocker.

    return {};
}
void ImportStore::require_qif_transfers(const imports::Occurrence& id,const imports::DraftRow& draft,const ResolvedTransfers& resolved) const {
    for(const auto& a:draft.transaction.allocations())if(transfer_name(a.category) && !resolved.contains({id.batch,id.ordinal,a.identity}))
        fail(SQLITE_CONSTRAINT,"Resolve every account allocation before committing: choose its destination or link an existing transfer");
}
