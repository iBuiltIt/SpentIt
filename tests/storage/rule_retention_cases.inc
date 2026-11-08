// Synthetic close-time rules baseline, retained provenance, and retry coverage.
app::RuleDefinitionImage retention_rule(std::string label="Original") {
    return rule_of("kept-rule",std::move(label),rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled payee","Ruled memo","rule-default",false});
}
void retention_setup(const fs::path& path) {
    auto s=create(path);save_rules(s,"initial",{retention_rule()},0);
    s.ingest("accepted",plain_source,qif::DateOrder::dmy);
    {app::OperationContext c;auto p=s.preview_pending_rules({"accepted",0,std::nullopt,false},c);app::OperationContext apply;s.apply_pending_rules("apply-accepted",{p.batch,p.batch_revision,p.ruleset_revision,p.catalog_revision,p.replace_manual},apply);}
    s.accept_all("accept",true);
    auto pending_source=plain_source;pending_source.replace(0,11,"D12/08/2026");
    s.ingest("pending",pending_source+"D11/08/2026\nT-2.00\nPUnmatched\n^\n",qif::DateOrder::dmy);
    {app::OperationContext c;auto p=s.preview_pending_rules({"pending",0,std::nullopt,false},c);app::OperationContext apply;s.apply_pending_rules("apply-pending",{p.batch,p.batch_revision,p.ruleset_revision,p.catalog_revision,p.replace_manual},apply);}
    save_rules(s,"edit-one",{retention_rule("Intermediate")},1);
    save_rules(s,"edit-two",{retention_rule("Current")},2);
    s.verify();
}
void retention_values(ImportStore& s) {
    const auto accepted=s.committed_entry({"accepted",0}).current.draft;
    const auto pending=*s.row({"pending",0}).draft;
    for(const auto* row:{&accepted,&pending}){
        check(row->payee=="Ruled payee" && row->memo=="Ruled memo");
        check(row->payee_rule && row->payee_rule->version==0 && row->memo_rule && row->memo_rule->version==0);
        check(row->transaction.allocations()[0].category_rule && row->transaction.allocations()[0].category_rule->version==0);
    }
    check(s.balance()==Money(-400,cad));s.verify();
}
void retention_completed(ImportStore& s) {
    bool completed=false;try{save_rules(s,"edit-two",{retention_rule("Current")},2);}catch(const app::OperationAlreadyCompleted&){completed=true;}check(completed);
    rejects_text([&]{save_rules(s,"edit-two",{retention_rule("Different")},2);},"different content");
    rejects_text([&]{save_rules(s,"edit-two",{retention_rule("Current")},1);},"different content");
    completed=false;try{(void)s.rule_save_outcome("edit-two");}catch(const app::OperationAlreadyCompleted&){completed=true;}check(completed);
}
void rule_retention() {
    fault_vfs::install();Directory dir("spentit-rule-retention-XXXXXX");const auto path=dir.path/"doc";retention_setup(path);
    {auto s=ImportStore::open(path,password);retention_values(s);check(s.rule_save_outcome("edit-two")->versions[0].second==2);
        // Cancellation after pruning must roll back requests and the baseline.
        app::OperationContext cancel;int finishing=0;cancel.progress=[&](std::string_view phase){if(phase=="Finishing file session" && ++finishing==2)cancel.request();};
        rejects_text([&]{s.certify({},&cancel);},"cancel");check(finishing==2);check(s.rule_save_outcome("edit-one").has_value());retention_values(s);
        s.backup_to(dir.path/"open-backup",password);s.certify({});retention_values(s);retention_completed(s);
    }
    check(query(path,"SELECT count(*) FROM rule_operation_requests")=="0");
    check(query(path,"SELECT count(*) FROM rule_snapshots")=="1");
    check(query(path,"SELECT group_concat(version) FROM (SELECT version FROM rule_definitions ORDER BY version)")=="0,2");
    {auto s=ImportStore::open(dir.path/"open-backup",password);retention_values(s);check(s.rule_save_outcome("edit-one").has_value());s.certify({});retention_completed(s);}
    {auto s=ImportStore::open(path,password);retention_values(s);retention_completed(s);s.backup_to(dir.path/"closed-backup",password);}
    {auto s=ImportStore::open(dir.path/"closed-backup",password);retention_values(s);retention_completed(s);const auto p=s.review_import_undo("pending");s.undo_import("undo-pending","pending",p.token);check(s.balance()==Money(-100,cad));s.verify();s.certify({});}
    // A referenced old definition outlives newer edits and deletion; tombstones
    // seed the next session and cannot resurrect even with an empty live set.
    {auto s=ImportStore::open(path,password);save_rules(s,"delete-rule",{},3);s.verify();s.certify({});s.verify();}
    {auto s=ImportStore::open(path,password);retention_values(s);check(s.rule_image().ordered.empty());check(s.rule_tombstones().size()==1);
        rejects_text([&]{save_rules(s,"revive",{retention_rule()},4);},"Deleted rule identity");
        auto added=retention_rule("New");added.identity="new-rule";save_rules(s,"new-rule",{added},4);s.verify();s.certify({});s.verify();}
    check(query(path,"SELECT count(*) FROM rule_snapshots")=="2");
    // Explicit checks must detect corruption of retained baseline/dependencies.
    for(const auto* sql:{"DROP TRIGGER rule_snapshots_immutable; UPDATE rule_snapshots SET position=20 WHERE deleted=0", "DROP TRIGGER rule_definitions_keep; DELETE FROM rule_definitions WHERE id='kept-rule' AND version=0", "DROP TRIGGER completion_identity; UPDATE command_completions SET domain='other' WHERE id='edit-two'"}){
        const auto damaged=dir.path/"damaged";fs::copy_file(path,damaged,fs::copy_options::overwrite_existing);
        sqlite3* db=nullptr;check(sqlite3_open(damaged.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        if(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(std::string("Corruption fixture: ")+sqlite3_errmsg(db));check(sqlite3_close(db)==SQLITE_OK);
        bool refused=false;try{auto s=ImportStore::open(damaged,password);s.verify();}catch(const ImportError&){refused=true;}check(refused);
    }
    // Deleted baseline members must not consume the live evaluator's 128-rule limit.
    const auto capacity=dir.path/"capacity";
    {auto s=create(capacity);std::vector<app::RuleDefinitionImage> rules;
        for(int i=0;i<128;++i){auto r=retention_rule();r.identity="deleted-"+std::to_string(i);rules.push_back(r);}
        save_rules(s,"many",rules,0);save_rules(s,"remove-all",{},1);s.certify({});}
    {auto s=ImportStore::open(capacity,password);save_rules(s,"new-live",{retention_rule()},2);s.certify({});}
    {auto s=ImportStore::open(capacity,password);check(s.rule_tombstones().size()==128);s.ingest("new",plain_source,qif::DateOrder::dmy);
        app::OperationContext c;auto p=s.preview_pending_rules({"new",0,std::nullopt,false},c);check(p.changed==1);s.verify();s.certify({});}
    no_leaks(dir.path);
}
void rule_retention_faults() {
    fault_vfs::install();Directory dir("spentit-rule-retention-faults-XXXXXX");const auto base=dir.path/"base";retention_setup(base);
    int positions=0;
    {const auto probe=dir.path/"probe";fs::copy_file(base,probe);auto s=ImportStore::open(probe,password);fault_vfs::arm(-1);s.certify({});positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    for(int nth=1;nth<=positions;++nth){const auto trial=dir.path/("trial-"+std::to_string(nth));fs::copy_file(base,trial);
        {auto s=ImportStore::open(trial,password);fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.certify({});}catch(const std::exception&){}check(fault_vfs::fired);fault_vfs::disarm();}
        {auto s=ImportStore::open(trial,password);retention_values(s);const bool closed=query(trial,"SELECT count(*) FROM rule_operation_requests")=="0";
            if(closed)retention_completed(s);else check(s.rule_save_outcome("edit-two")->versions[0].second==2);
            s.certify({});retention_values(s);retention_completed(s);}
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){const auto trial=dir.path/(phase==ImportPhase::before_commit?"before":"after");fs::copy_file(base,trial);const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(trial,password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});s.certify({});_exit(73);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        auto s=ImportStore::open(trial,password);retention_values(s);check((query(trial,"SELECT count(*) FROM rule_operation_requests")=="0")== (phase==ImportPhase::after_commit));
        if(phase==ImportPhase::before_commit)check(s.rule_save_outcome("edit-two").has_value());else retention_completed(s);
        s.certify({});retention_completed(s);
    }
    std::cout<<"Rule retention close: "<<positions<<" write/sync failure positions and both crash boundaries passed\n";
}
