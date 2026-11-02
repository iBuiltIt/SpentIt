transfers::AllocationFact reapply_fact(std::int64_t account,std::string batch,Money amount,std::string date){return {{account,{std::move(batch),0},"source-0"},amount,std::move(date),transfers::State::committed,true,false,false};}
app::TransferView reapply_link(ImportStore& store,const std::string& id){for(const auto& value:store.transfers())if(value.link.identity==id)return value;throw std::runtime_error("Missing transfer "+id);}
app::RuleReapplyPreview reapply_preview(ImportStore& store,bool replace=false,std::string undo={}){app::OperationContext context;return store.preview_rule_reapply(replace,undo,context);}
app::RuleReapplyReceipt reapply_apply(ImportStore& store,std::string_view operation,const app::RuleReapplyRequest& request){app::OperationContext context;return store.apply_rule_reapply(operation,request,context);}
app::TransferRequest reapply_merge(ImportStore& store){app::TransferRequest request{};request.action=app::TransferAction::merge;request.link="merged";request.kept="merged-kept";request.expected_sequence=store.transfer_sequence();request.left=reapply_fact(1,"left",Money(-10000,cad),"2026-10-26");request.right=reapply_fact(2,"shop",Money(10000,cad),"2026-10-26");request.batch_revision=1;request.right_revision=1;request.payee="Left";return request;}
void reapply_basics(){
    Directory dir("spentit-reapply-XXXXXX");const auto path=dir.path/"doc";
    {
        auto s=create(path);s.create_account(2,"Visa",cad,"2026-08-01");s.select_account(2);s.opening(0,"2026-08-01",Money(0,cad));s.select_account(1);
        save_rules(s,"save-payee",{rule_of("payee-rule","Payee rule",rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false})},0);
        s.ingest("rows",plain_source+plain_source+plain_source+plain_source,qif::DateOrder::dmy);s.commit("rows",0,"publish-rows");
        app::ChangeRequest removal{app::ChangeKind::remove,{"rows",2},{},{}};auto removed=s.preview_committed_change(removal);s.apply_committed_change_with_catalog("remove-one",removed.request,removed.expected,removed.preview_sequence,removed.catalog_revision);
        auto kept=s.committed_entry({"rows",3}).current.draft;kept.payee="Kept";kept.payee_owner=FieldOwnership::manual;kept.payee_rule.reset();bind(s,kept,0,"rule-default");correct(s,"keep-payee",{"rows",3},kept);
        s.ingest("draft",plain_source,qif::DateOrder::dmy);
        s.select_account(2);s.ingest("other",plain_source,qif::DateOrder::dmy);s.commit("other",0,"publish-other");const auto other_balance=s.committed_balance();s.select_account(1);
        const auto balance=s.committed_balance();const auto evidence=s.evidence("rows");
        auto looked=reapply_preview(s);check(looked.request.evaluated==3 && looked.request.changed==2 && looked.records.size()==2 && looked.change.units()==0 && looked.before==balance && looked.after==balance);
        check(looked.records[0].identity==imports::Occurrence{"rows",0} && looked.records[1].identity==imports::Occurrence{"rows",1});
        for(const auto& record:looked.records)check(record.before.payee==marker && record.after.payee=="Ruled Payee" && record.after.payee_owner==FieldOwnership::rule);
        rejects_text([&]{reapply_preview(s,true);},"Clear categories");
        app::OperationContext context;rejects_text([&]{s.apply_rule_reapply("publish-rows",looked.request,context);},"Operation identity belongs to another command");
        rejects_text([&]{s.apply_rule_reapply(std::string(97,'a'),looked.request,context);},"Rule reapply operation identity is too long");
        auto applied=reapply_apply(s,"reapply",looked.request);check(applied.operation=="reapply" && applied.entries.size()==2 && applied.change.units()==0 && applied.before==applied.after && applied.entries[0].operation=="reapply" && applied.entries[1].operation=="reapply-1");
        check(s.apply_rule_reapply("reapply",looked.request,context).entries[1].sequence==applied.entries[1].sequence);
        auto flipped=looked.request;flipped.replace_manual=true;rejects_text([&]{s.apply_rule_reapply("reapply",flipped,context);},"Operation identity was reused with different content");
        for(const auto ordinal:std::array<std::size_t,2>{0,1}){const auto row=s.committed_entry({"rows",ordinal}).current.draft;check(row.payee=="Ruled Payee" && row.payee_owner==FieldOwnership::rule && row.payee_rule==RuleMark{"payee-rule",0} && row.transaction.effective_total().units()==-100);}
        check(s.committed_entry({"rows",3}).current.draft.payee=="Kept" && s.committed_entry({"rows",3}).current.draft.payee_owner==FieldOwnership::manual && !s.committed_entry({"rows",2}).current.active && s.committed_entry({"rows",2}).current.draft.payee==marker);
        bool draft_pending=false;for(const auto& batch:s.batches())if(batch.id=="draft")draft_pending=batch.state==app::BatchState::pending;check(draft_pending && s.row({"draft",0}).draft->payee==marker && s.committed_balance()==balance && s.evidence("rows")==evidence);
        s.select_account(2);check(s.committed_entry({"other",0}).current.draft.payee==marker && s.committed_balance()==other_balance);s.select_account(1);
        check(s.rule_reapply_group("reapply")=="reapply" && s.rule_reapply_group("reapply-1")=="reapply" && s.rule_reapply_group("remove-one").empty());
        const auto token=s.committed_entry({"rows",0}).token;app::ChangeRequest misuse{app::ChangeKind::correct,{"rows",0},std::nullopt,{}};
        rejects_text([&]{s.apply_committed_change_with_catalog("reapply-1",misuse,token,0,0);},"Operation identity belongs to a rule reapply");
        app::ChangeRequest single{app::ChangeKind::undo,{"rows",0},{},"reapply"};rejects_text([&]{s.apply_committed_change_with_catalog("plain-undo",single,token,0,0);},"Operation identity belongs to a rule reapply");
        rejects_text([&]{preview(s,"rows",batch_revision(s,"rows"));},"Committed records are not changed by rules");
        auto undo=reapply_preview(s,false,"reapply");check(undo.request.undo_of=="reapply" && undo.request.changed==2 && undo.records[0].after.payee==marker && undo.records[0].after.payee_owner==FieldOwnership::source);
        auto undone=reapply_apply(s,"undo-reapply",undo.request);check(undone.operation=="undo-reapply" && s.committed_entry({"rows",0}).current.draft.payee==marker && !s.committed_entry({"rows",0}).current.draft.payee_rule && s.committed_balance()==balance);
        auto redo=reapply_preview(s,false,"undo-reapply");auto redone=reapply_apply(s,"redo-reapply",redo.request);check(redone.operation=="redo-reapply" && s.committed_entry({"rows",1}).current.draft.payee=="Ruled Payee" && s.committed_entry({"rows",1}).current.draft.payee_rule==RuleMark{"payee-rule",0});
        auto later=s.committed_entry({"rows",0}).current.draft;later.memo="Later";later.memo_owner=FieldOwnership::manual;later.memo_rule.reset();correct(s,"later-memo",{"rows",0},later);
        rejects_text([&]{reapply_preview(s,false,"redo-reapply");},"Newer transaction, transfer relationship, or source association prevents Undo");
        s.verify();
    }
    check(query(path,"SELECT ruleset_revision FROM entry_rule_fields WHERE batch='rows' AND ordinal=0 AND revision=1")=="1");no_leaks(dir.path);
    Directory unused("spentit-reapply-unused-XXXXXX");const auto spare=unused.path/"doc";
    {
        auto s=create(spare);save_rules(s,"save-miss",{rule_of("miss","Miss",rules::Scope::transaction,rules::InputField::payee,"NoSuchPayee",{"Nope",{},"rule-default",false})},0);
        s.ingest("batch",plain_source,qif::DateOrder::dmy);s.commit("batch",0,"publish");
        auto looked=reapply_preview(s);check(looked.request.evaluated==1 && looked.request.changed==0 && looked.records.empty() && looked.change.units()==0);
        app::OperationContext context;rejects_text([&]{s.apply_rule_reapply("publish",looked.request,context);},"Operation identity belongs to another command");
        auto none=reapply_apply(s,"unused",looked.request);check(none.operation.empty() && !s.rule_reapply_outcome("unused") && s.committed_entry({"batch",0}).current.draft.payee==marker);
        save_rules(s,"save-hit",{rule_of("payee-rule","Payee rule",rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false})},1);
        auto next=reapply_preview(s);check(next.request.changed==1);auto wrote=reapply_apply(s,"unused",next.request);check(wrote.operation=="unused" && s.committed_entry({"batch",0}).current.draft.payee=="Ruled Payee");s.verify();
    }
    no_leaks(unused.path);
    Directory linked("spentit-reapply-link-XXXXXX");const auto link_path=linked.path/"doc";
    {
        auto s=create(link_path);s.create_account(2,"Visa",cad,"2026-08-01");s.select_account(2);s.opening(0,"2026-08-01",Money(0,cad));s.select_account(1);
        apply_category(s,"fees",make("fees","Fees"));
        s.ingest("left","D26/10/2026\nT-100\nPLeft\n^\n",qif::DateOrder::dmy);s.commit("left",0,"left-commit");
        s.select_account(2);s.ingest("shop","D26/10/2026\nT100\nPShop\n^\n",qif::DateOrder::dmy);s.commit("shop",0,"shop-commit");s.select_account(1);
        const auto request=reapply_merge(s);s.apply_transfer("merge-both",request);
        const auto linked_view=reapply_link(s,"merged");check(!linked_view.link.unresolved && linked_view.link.origin.state==transfers::State::committed && linked_view.link.destination.state==transfers::State::committed && linked_view.link.origin.amount==Money(-10000,cad) && linked_view.link.destination.amount==Money(10000,cad));
        const auto left_evidence=s.record_evidence({"left",0});s.select_account(2);const auto shop_evidence=s.record_evidence({"shop",0});const auto shop_balance=s.committed_balance();const auto before_category=s.committed_entry({"shop",0}).current.draft.transaction.allocations()[0].category;check(before_category.empty() && shop_balance==Money(10000,cad));
        save_rules(s,"save-category",{rule_of("shop-category","Category",rules::Scope::transaction,rules::InputField::payee,"Shop",{{},{},"fees",false})},0);
        auto skipped=reapply_preview(s);check(skipped.request.evaluated==1 && skipped.request.changed==0 && skipped.records.empty() && skipped.skips.size()==1 && skipped.skips[0].note=="Left the blank category on Shop (source-0). That category stays because it is a transfer link." && skipped.skips[0].category.empty());
        auto untouched=reapply_apply(s,"spare",skipped.request);check(untouched.operation.empty() && !s.rule_reapply_outcome("spare") && s.committed_entry({"shop",0}).current.draft.payee=="Shop" && s.committed_entry({"shop",0}).current.draft.transaction.allocations()[0].category.empty());
        save_rules(s,"save-shop",{rule_of("shop-rule","Shop rule",rules::Scope::transaction,rules::InputField::payee,"Shop",{"Ruled Shop",{},"fees",false})},1);
        auto named=reapply_preview(s);check(named.request.changed==0 && named.records.empty() && named.skips.size()==1);
        app::RulePreviewRequest accountQuery;accountQuery.account_scope=true;accountQuery.matches_only=true;
        app::OperationContext accountContext;const auto accountPreview=s.preview_pending_rules(accountQuery,accountContext);check(accountPreview.changed==0 && accountPreview.matches.size()==1);
        app::OperationContext accountApply;check(s.apply_account_rules("account-linked",accountPreview.account_request,accountApply).operation.empty());
        auto wrote=reapply_apply(s,"spare",named.request);check(wrote.operation.empty());
        const auto shop=s.committed_entry({"shop",0}).current.draft;check(shop.payee=="Shop" && shop.transaction.allocations()[0].category.empty() && shop.transaction.effective_total()==Money(10000,cad));
        check(s.record_evidence({"shop",0})==shop_evidence && s.committed_balance()==shop_balance);const auto still=reapply_link(s,"merged");check(!still.link.unresolved && still.link.origin.amount==Money(-10000,cad) && still.link.destination.amount==Money(10000,cad) && still.link.destination.endpoint.entry.batch=="shop" && still.link.destination.endpoint.allocation=="source-0");
        s.select_account(1);check(s.committed_entry({"left",0}).current.draft.payee=="Left" && s.committed_balance()==Money(-10000,cad) && s.record_evidence({"left",0})==left_evidence);s.verify();
    }
    check(query(link_path,"SELECT count(*) FROM entry_rule_fields WHERE batch='shop' AND ordinal=0 AND revision>0")=="0");no_leaks(linked.path);
}
app::RuleReapplyPreview reapply_fault_setup(const fs::path& path){auto s=create(path);save_rules(s,"save-payee",{rule_of("payee-rule","Payee rule",rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false})},0);s.ingest("batch",plain_source,qif::DateOrder::dmy);s.commit("batch",0,"publish");return reapply_preview(s);}
void reapply_recover(const fs::path& path,const app::RuleReapplyPreview& preview,bool strict){
    auto s=ImportStore::open(path,password);const auto prior=s.rule_reapply_outcome("fault");if(strict)check(!prior);
    const auto current=s.committed_entry({"batch",0}).current;check(current.revision==(prior?preview.records.at(0).expected.revision+1:preview.records.at(0).expected.revision));check(current.draft.payee==(prior?"Ruled Payee":marker));
    auto result=reapply_apply(s,"fault",preview.request);check(result.operation=="fault" && result.change.units()==0 && s.committed_entry({"batch",0}).current.draft.payee=="Ruled Payee");
    check(reapply_apply(s,"fault",preview.request).entries.size()==result.entries.size());s.verify();
}
void reapply_faults(){
    fault_vfs::install();int positions=0;
    {Directory dir("spentit-reapply-probe-XXXXXX");const auto preview=reapply_fault_setup(dir.path/"doc");auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(-1);reapply_apply(s,"fault",preview.request);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    auto one_disk_full=[&](int position){Directory dir("spentit-reapply-full-XXXXXX");const auto path=dir.path/"doc";const auto preview=reapply_fault_setup(path);{auto s=ImportStore::open(path,password);fault_vfs::arm(position,SQLITE_FULL);try{reapply_apply(s,"fault",preview.request);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}reapply_recover(path,preview,true);no_leaks(dir.path);};
    if(positions<=80){for(int position=1;position<=positions;++position){Directory dir("spentit-reapply-fault-XXXXXX");const auto path=dir.path/"doc";const auto preview=reapply_fault_setup(path);{auto s=ImportStore::open(path,password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{reapply_apply(s,"fault",preview.request);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}reapply_recover(path,preview,true);no_leaks(dir.path);}}
    else one_disk_full(1);
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory dir("spentit-reapply-observer-XXXXXX");const auto path=dir.path/"doc";const auto preview=reapply_fault_setup(path);const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase current){if(current==phase)_exit(72);});reapply_apply(s,"fault",preview.request);_exit(0);}int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);{auto s=ImportStore::open(path,password);check(s.rule_reapply_outcome("fault").has_value()==(phase==ImportPhase::after_commit));}reapply_recover(path,preview,false);no_leaks(dir.path);}
    std::cout<<"reapply faults: "<<positions<<" fault positions"<<(positions<=80?" swept":" observer and one disk-full")<<"\n";
}
