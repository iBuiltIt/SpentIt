// Compare financial state through the repository. Physical restoration revisions
// advance on Undo, so compare the identity and contents of each live posting and link.
struct TransferAccountSnapshot {
    std::vector<LedgerRow> postings;
    std::vector<TransferView> links;
    Money balance,committed;
};
using TransferFileSnapshot=std::map<std::int64_t,TransferAccountSnapshot>;

TransferFileSnapshot transfer_snapshot(ImportStore& store) {
    const auto selected=store.account().identity;
    TransferFileSnapshot state;
    for(const auto& account:store.accounts()){
        store.select_account(account.identity);
        auto postings=store.current_ledger();
        std::sort(postings.begin(),postings.end(),[](const auto& a,const auto& b){return a.identity<b.identity;});
        auto links=store.transfers();
        std::erase_if(links,[](const auto& v){return v.link.unresolved ||
            (v.link.origin.state==transfers::State::removed && v.link.destination.state==transfers::State::removed);});
        state.emplace(account.identity,TransferAccountSnapshot{std::move(postings),std::move(links),store.balance(),store.committed_balance()});
    }
    store.select_account(selected);
    return state;
}
void check_transfer_snapshot(ImportStore& store,const TransferFileSnapshot& expected) {
    const auto actual=transfer_snapshot(store);
    check(actual.size()==expected.size(),"Account count changed");
    const auto allocation=[](const auto& a){return std::tie(a.identity,a.amount,a.category,a.category_binding,a.memo,
        a.source_ordinal,a.category_owner,a.memo_owner,a.category_rule,a.memo_rule);};
    const auto posting=[](const LedgerRow& r){const auto& d=r.draft;
        return std::tie(r.identity,r.sequence,r.committed,d.date,d.payee,d.memo,d.payee_owner,d.memo_owner,
            d.payee_rule,d.memo_rule,d.manual_split_amounts,d.remainder_allocation,
            d.transaction.effective_total(),d.transaction.original_imported_amount());};
    const auto side=[](const transfers::Side& s){return std::tie(s.endpoint,s.amount,s.date,s.state,s.independent_evidence,
        s.provisional_date,s.manually_entered,s.accepted_unmatched,s.attached_evidence,s.proposed_evidence,s.ledger_entry);};
    const auto link=[](const TransferView& v){const auto& l=v.link;
        return std::tie(l.identity,l.generated_date,l.preserved_amounts,l.unresolved,l.origin_date_corrected,
            l.destination_released,l.origin_category,l.manual_cross_currency,v.payee,v.memo,v.created_sequence,
            v.source_matched,v.link_has_bank_match);};
    for(const auto& [id,before]:expected){
        const auto& after=actual.at(id);
        check(before.balance==after.balance && before.committed==after.committed,"Account balances did not return to the snapshot");
        check(before.postings.size()==after.postings.size(),"Ledger posting count changed");
        for(std::size_t i=0;i<before.postings.size();++i){
            const auto& a=before.postings[i];const auto& b=after.postings[i];
            check(posting(a)==posting(b) && a.draft.transaction.reassigned_currency()==b.draft.transaction.reassigned_currency(),"Ledger posting changed");
            const auto& left=a.draft.transaction.allocations();const auto& right=b.draft.transaction.allocations();
            check(std::equal(left.begin(),left.end(),right.begin(),right.end(),[&](const auto& x,const auto& y){return allocation(x)==allocation(y);}),"Posting allocations changed");
        }
        check(before.links.size()==after.links.size(),"Live transfer count changed");
        for(std::size_t i=0;i<before.links.size();++i){
            const auto& a=before.links[i];const auto& b=after.links[i];
            check(link(a)==link(b) && side(a.link.origin)==side(b.link.origin) && side(a.link.destination)==side(b.link.destination),"Transfer endpoints or bank evidence changed");
        }
    }
}
void add_transfer_account(ImportStore& store,std::int64_t id,const char* name){
    const Currency cad("CAD");
    store.create_account(id,name,cad,"2026-01-01");
    store.select_account(id);store.opening(0,"2026-01-01",Money(0,cad));
}
void save_one_import_link(ImportStore& store,const std::vector<std::string>& batches){
    auto plan=store.automatic_review_plan(batches,false,true);
    check(plan.size()==1 && plan.front().action==ReviewAction::link,"Fixture needs one automatic link");
    OperationContext context;store.save_automatic_import_review("matches",plan,context);
}
void verify_import_transfer_undo(const std::filesystem::path& path,ImportStore& store,
                                 const std::string& source,const TransferFileSnapshot& before,std::int64_t removed) {
    const auto joined=transfer_snapshot(store);
    const auto preview=store.preview_undo(source,store.action_token());
    check(preview.refusal.empty() && preview.accounts.size()==before.size() && preview.action.count==removed,"Preview must include all transfer accounts");
    store.observer([](storage::ImportPhase phase){if(phase==storage::ImportPhase::before_commit)throw std::runtime_error("before transfer Undo commit");});
    bool refused=false;
    try{store.undo_action("undo-import",preview);}catch(const std::runtime_error& e){check(std::string_view(e.what())=="before transfer Undo commit",e.what());refused=true;}
    store.observer({});
    check(refused && store.action_token()==preview.token && !store.undo_action_outcome("undo-import",preview),"Failed Undo must roll back its receipt and head");
    check_transfer_snapshot(store,joined);
    store.observer([](storage::ImportPhase phase){if(phase==storage::ImportPhase::after_commit)throw std::runtime_error("after transfer Undo commit");});
    try{store.undo_action("undo-import",preview);}catch(const std::runtime_error& e){check(std::string_view(e.what())=="after transfer Undo commit",e.what());}
    store.observer({});
    check(store.undo_action_outcome("undo-import",preview).has_value(),"Committed Undo must have a receipt");
    store.undo_action("undo-import",preview);
    check(store.redo_history().empty(),"Import Undo clears Redo");
    check_transfer_snapshot(store,before);store.verify();
    // Recover the unclosed session, then exercise close-time pruning as well.
    std::optional<ImportStore> open{std::move(store)};open.reset();
    store=ImportStore::open(path,"synthetic");check_transfer_snapshot(store,before);store.verify();
    test_session::close_reopen(store,path,"synthetic");check_transfer_snapshot(store,before);store.verify();
}
void import_transfer_undo_cases(const std::filesystem::path& dir){
    fault_vfs::install();
    const Currency cad("CAD");
    const auto input=[](bool outgoing,bool generated){
        return std::string("!Type:Bank\nD01/01/2026\nT")+(outgoing?"-10":"10")+"\nP"+(outgoing?"Payment":"Receipt")+
            "\nMStatement memo\n"+(generated?(outgoing?"L[Savings]\n":"L[Checking]\n"):"")+"^\n";};
    int serial=0;
    for(bool reverse:{false,true})for(bool earlier_generated:{false,true})for(bool later_generated:{false,true})for(bool accept:{false,true}){
        const auto path=dir/("automatic-"+std::to_string(++serial));
        auto store=ImportStore::create(path,"synthetic","Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));add_transfer_account(store,2,"Savings");
        store.select_account(reverse?2:1);store.ingest("earlier",input(!reverse,earlier_generated),qif::DateOrder::dmy);
        const auto before=transfer_snapshot(store);
        store.select_account(reverse?1:2);store.ingest("later",input(reverse,later_generated),qif::DateOrder::dmy);
        save_one_import_link(store,{"later"});
        bool older_blocked=false;
        try{(void)store.review_import_undo("earlier");}catch(const storage::ImportError& e){older_blocked=e.code()==SQLITE_CONSTRAINT;}
        check(older_blocked,"The picker must refuse an older import while the current Undo chain is active");
        if(accept){
            store.accept_all("accept",true);undo(store,"undo-accept","accept");
            store.redo_action("accept-again",redo_preview(store));undo(store,"undo-again","accept-again");
        }
        verify_import_transfer_undo(path,store,"later",before,1);
    }
    // Two automatic links to an earlier source. A later import and acceptance
    // are undone first, as in the reported history. Closing here expires ordinary
    // Undo, leaving the persistent import picker and its saved match snapshots.
    for(bool close_before:{false,true}){
        const auto path=dir/("two-links-"+std::to_string(++serial));
        auto store=ImportStore::create(path,"synthetic","Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));add_transfer_account(store,2,"Savings");
        store.select_account(1);
        store.ingest("old-pair",input(true,true)+"D02/01/2026\nT-25\nPSecond payment\nL[Savings]\n^\n",qif::DateOrder::dmy);
        const auto before=transfer_snapshot(store);
        store.select_account(2);
        store.ingest("new-pair",input(false,true)+"D02/01/2026\nT25\nPSecond receipt\nL[Checking]\n^\n",qif::DateOrder::dmy);
        auto pairs=store.automatic_review_plan({"new-pair"},false,true);
        check(pairs.size()==2,"Both automatic links must be present");
        OperationContext context;store.save_automatic_import_review("match-pairs",pairs,context);
        store.accept_all("accept-pairs",true);
        store.ingest("unrelated",source("Another payment"),qif::DateOrder::dmy);
        undo(store,"undo-unrelated","unrelated");undo(store,"undo-accept-pairs","accept-pairs");
        if(!close_before){
            verify_import_transfer_undo(path,store,"new-pair",before,2);
            continue;
        }
        test_session::close_reopen(store,path,"synthetic");
        check(!store.action_head(),"A closed session uses import Undo from Review");
        const auto review=store.review_import_undo("new-pair");
        check(review.transactions==2 && review.affected_accounts==std::vector<std::int64_t>{1,2},"Older-import preview covers both accounts");
        const auto joined=transfer_snapshot(store);
        // An independent later change to one of the two matches must block
        // the entire prior preview and leave the other pair intact.
        store.unmerge("separate-one",{{"new-pair",0}});
        const auto changed=transfer_snapshot(store);
        bool refused=false;
        try{(void)store.review_import_undo("new-pair");}catch(const storage::ImportError& e){refused=e.code()==SQLITE_CONSTRAINT;}
        check(refused,"A conflicting link change is refused during preview");
        refused=false;
        try{store.undo_import("stale","new-pair",review.token);}catch(const storage::ImportError& e){refused=e.code()==SQLITE_CONSTRAINT;}
        check(refused && !store.import_undo_outcome("stale","new-pair"),"A stale preview cannot save");
        check_transfer_snapshot(store,changed);
        undo(store,"undo-separate","separate-one");
        check_transfer_snapshot(store,joined);
        const auto refreshed=store.review_import_undo("new-pair");
        for(const int error:{SQLITE_FULL,SQLITE_IOERR_WRITE}){
            fault_vfs::arm(1,error,false);
            bool failed=false;
            try{store.undo_import("undo-pair","new-pair",refreshed.token);}
            catch(const storage::ImportError&){failed=true;}
            fault_vfs::disarm();
            check(failed && fault_vfs::fired && !store.import_undo_outcome("undo-pair","new-pair"),
                  "A failed write must keep both links and leave no success receipt");
            check(!store.redo_history().empty(),"A failed import Undo must preserve the pending Redo");
            store.verify();check_transfer_snapshot(store,joined);
        }
        store.undo_import("undo-pair","new-pair",refreshed.token);
        check(store.redo_history().empty(),"Older-import Undo must clear pending Redo");
        store.verify();check_transfer_snapshot(store,before);
        test_session::close_reopen(store,path,"synthetic");store.verify();check_transfer_snapshot(store,before);
    }
    for(bool destination_first:{false,true}){
        const auto path=dir/("mixed-"+std::to_string(++serial));
        auto store=ImportStore::create(path,"synthetic","Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));add_transfer_account(store,2,"Savings");add_transfer_account(store,3,"Card");
        store.ingest("earlier","!Type:Bank\nD10/01/2026\nT-23\nPOlder payment\n^\n",qif::DateOrder::dmy);store.accept_all("accept",true);
        const auto before=transfer_snapshot(store);
        const std::string saving="!Account\nNSavings\nTBank\n^\n!Type:Bank\nD2026-01-02\nT10\nPReceipt\nL[Checking]\n^\nD2026-01-10\nT23\nPCard receipt\n^\n";
        const std::string checking="!Account\nNChecking\nTBank\n^\n!Type:Bank\nD2026-01-02\nT-10\nPPayment\nL[Savings]\n^\n";
        OperationContext context;
        store.prepare_import_group("mapped",destination_first?saving+checking:checking+saving,qif::DateOrder::dmy,
            {{0,destination_first?2:1,{}},{1,destination_first?1:2,{}}},context);
        store.resume_import_group("mapped",context);
        save_one_import_link(store,{"mapped-account-0","mapped-account-1"});
        verify_import_transfer_undo(path,store,"mapped",before,3);
    }
}
