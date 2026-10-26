app::BulkRequest bulk_request(app::ChangeKind kind){return {kind,{{"batch",0},{"batch",1}},kind==app::ChangeKind::correct?"bulk":"",kind==app::ChangeKind::undo?"original":""};}
app::BulkPreview bulk_setup(const fs::path& path,app::ChangeKind kind){
    auto s=create(path);apply_category(s,"category",make("bulk","Bulk"));s.ingest("batch",split_source+plain_source,qif::DateOrder::dmy);s.commit("batch",0,"publish");
    if(kind==app::ChangeKind::undo)s.apply_bulk("original",s.preview_bulk(bulk_request(app::ChangeKind::remove)));
    return s.preview_bulk(bulk_request(kind));
}
void bulk_basics(){
    Directory dir("spentit-bulk-XXXXXX");const auto path=dir.path/"doc";
    const auto preview=bulk_setup(path,app::ChangeKind::correct);
    {auto s=ImportStore::open(path,password);const auto source=s.evidence("batch");check(s.bulk_supported());check(preview.entries.size()==2 && preview.change.units()==0);
        auto receipt=s.apply_bulk("bulk",preview);check(receipt.entries.size()==2 && receipt.before==receipt.after);
        check(s.apply_bulk("bulk",preview).entries[1].sequence==receipt.entries[1].sequence);
        auto changed=preview;changed.request.identities.pop_back();rejects_text([&]{s.apply_bulk("bulk",changed);},"different content");
        for(const auto& id:preview.request.identities){const auto entry=s.committed_entry(id);for(const auto& allocation:entry.current.draft.transaction.allocations())check(allocation.category_binding->identity=="bulk" && allocation.category_owner==FieldOwnership::manual);}
        check(s.evidence("batch")==source);s.verify();s.backup_to(dir.path/"backup",password);
        auto undo=bulk_request(app::ChangeKind::undo);undo.undo_of="bulk";auto undone=s.apply_bulk("undo",s.preview_bulk(undo));check(undone.before==undone.after);s.verify();
        auto remove=s.preview_bulk(bulk_request(app::ChangeKind::remove));s.apply_bulk("remove",remove);check(s.committed_balance().units()==0);s.verify();
        undo.undo_of="remove";s.apply_bulk("restore",s.preview_bulk(undo));check(s.committed_balance().units()==-3031);s.verify();
        auto stale=s.preview_bulk(bulk_request(app::ChangeKind::remove));apply_category(s,"rename",change(app::CategoryAction::rename,"bulk","Changed"));rejects_text([&]{s.apply_bulk("stale",stale);},"preview changed");check(!s.bulk_outcome("stale"));
        auto correction=s.committed_entry({"batch",0}).current.draft;correction.memo="Later";correct(s,"later",{"batch",0},correction);undo.undo_of="restore";rejects_text([&]{s.preview_bulk(undo);},"Newer transaction");
        auto request=bulk_request(app::ChangeKind::remove);request.identities.push_back(request.identities.front());rejects_text([&]{s.preview_bulk(request);},"Duplicate");request.identities={{"",0}};rejects_text([&]{s.preview_bulk(request);},"identifier");
        s.ingest("pending",plain_source,qif::DateOrder::dmy);request.identities={{"pending",0}};rejects_text([&]{s.preview_bulk(request);},"committed");
        auto clear=bulk_request(app::ChangeKind::correct);clear.category.clear();s.apply_bulk("uncategorize",s.preview_bulk(clear));s.verify();
    }
    {auto s=ImportStore::open(dir.path/"backup",password);check(s.bulk_outcome("bulk")->entries.size()==2);s.verify();}
    no_leaks(dir.path);
    Directory provenance("spentit-bulk-provenance-XXXXXX");
    {
        auto s=create(provenance.path/"doc");apply_category(s,"cat",make("bulk","Bulk"));
        save_rules(s,"rule",{rule_of("r","Synthetic rule",rules::Scope::transaction,rules::InputField::payee,".*",{"Rule payee",{},"bulk",false})},0);
        s.ingest("batch",split_source+plain_source,qif::DateOrder::dmy);app::OperationContext context;apply_rules(s,"rules",request_from(s.preview_pending_rules({"batch",0,{},false},context)));s.commit("batch",batch_revision(s,"batch"),"publish");
        const auto original=s.committed_entry({"batch",1}).current.draft;check(original.payee_rule.has_value() && original.transaction.allocations()[0].category_rule.has_value());
        auto request=bulk_request(app::ChangeKind::correct);request.category.clear();s.apply_bulk("clear",s.preview_bulk(request));
        const auto changed=s.committed_entry({"batch",1}).current.draft;check(changed.payee_rule==original.payee_rule && !changed.transaction.allocations()[0].category_rule);
        auto undo=bulk_request(app::ChangeKind::undo);undo.undo_of="clear";s.apply_bulk("restore",s.preview_bulk(undo));
        check(same_fields(s.committed_entry({"batch",1}).current.draft,original));s.verify();
    }
    {auto s=ImportStore::open(provenance.path/"doc",password);s.verify();check(s.committed_entry({"batch",1}).current.draft.transaction.allocations()[0].category_rule.has_value());}
    no_leaks(provenance.path);
}
void bulk_recover(const fs::path& path,const app::BulkPreview& preview,bool strict){
    auto s=ImportStore::open(path,password);const auto prior=s.bulk_outcome("fault");if(strict)check(!prior);
    for(const auto& entry:preview.entries){const auto current=s.committed_entry(entry.request.identity);check(current.current.revision==(prior?entry.after_entry.revision:entry.before_entry.revision));check(current.current.active==(prior?entry.after_entry.active:entry.before_entry.active));}
    const auto result=s.apply_bulk("fault",preview);check(result.entries.size()==2);check(s.apply_bulk("fault",preview).after==result.after);s.verify();
}
void bulk_sweep(bool crash){fault_vfs::install();for(const auto kind:{app::ChangeKind::correct,app::ChangeKind::remove,app::ChangeKind::undo}){
    int positions=0;{Directory dir("spentit-bulk-probe-XXXXXX");const auto preview=bulk_setup(dir.path/"doc",kind);auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(-1);s.apply_bulk("fault",preview);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    for(int position=1;position<=positions;++position){Directory dir("spentit-bulk-fault-XXXXXX");const auto path=dir.path/"doc";const auto preview=bulk_setup(path,kind);
        if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);s.apply_bulk("fault",preview);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));}
        else{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.apply_bulk("fault",preview);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}
        bulk_recover(path,preview,!crash);no_leaks(dir.path);
    }
    if(crash)for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory dir("spentit-bulk-boundary-XXXXXX");const auto path=dir.path/"doc";const auto preview=bulk_setup(path,kind);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](auto current){if(current==phase)_exit(72);});s.apply_bulk("fault",preview);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);{auto s=ImportStore::open(path,password);check(s.bulk_outcome("fault").has_value()==(phase==ImportPhase::after_commit));}bulk_recover(path,preview,false);}
    std::cout<<"bulk "<<static_cast<int>(kind)<<(crash?" crash ":" io ")<<positions<<" positions\n";
}}
