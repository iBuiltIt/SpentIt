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
                                 const std::string& source,const TransferFileSnapshot& before,std::int64_t removed,
                                 std::size_t affected=0) {
    const auto joined=transfer_snapshot(store);
    const auto preview=store.preview_undo(source,store.action_token());
    if(!preview.refusal.empty() || preview.accounts.size()!=(affected?affected:before.size()) || preview.action.count!=removed)
        throw std::runtime_error("Preview must include all affected accounts: "+source+" "+preview.refusal);
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
void provisional_import_link_orders(const std::filesystem::path& dir) {
    const Currency cad("CAD");
    const std::array<std::string,3> batches{"a","b","c"};
    const std::array<std::string,3> postings{
        "!Type:Bank\nD01/01/2026\nT-10\nPPayment\n^\n",
        "!Type:Bank\nD01/01/2026\nT10\nPUnrelated deposit\n^\n",
        "!Type:Bank\nD01/01/2026\nT10\nPTransfer receipt\n^\n"};
    const auto create=[&](const auto& path){
        auto store=ImportStore::create(path,"synthetic","A",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));
        add_transfer_account(store,2,"B");
        add_transfer_account(store,3,"C");
        return store;
    };
    const auto ingest=[&](ImportStore& store,std::size_t index){
        store.select_account(static_cast<std::int64_t>(index)+1);
        store.ingest(batches[index],postings[index],qif::DateOrder::dmy);
    };
    const auto match=[&](ImportStore& store,std::size_t index){
        OperationContext context;
        return store.review_completed_import("match-"+batches[index],{batches[index]},true,true,context);
    };
    const auto review=[&](ImportStore& store){
        const auto sheet=store.review_sheet();
        check(sheet.questions.size()==1,"All orders show one Review question");
        const auto& question=sheet.questions.front();
        check(question.kind==ReviewKind::link && question.rows.size()==3 &&
              question.rows.front().identity==imports::Occurrence{"a",0},"Review starts with A's payment");
        std::set<imports::Occurrence> alternatives;
        for(std::size_t i=1;i<question.rows.size();++i)alternatives.insert(question.rows[i].identity);
        check(alternatives==std::set<imports::Occurrence>{{"b",0},{"c",0}},"Review offers both B and C");
        check(store.pending_review_links().empty(),"The provisional link no longer appears as matched");
        for(const auto& [account,amount]:store.balances())
            check(amount==Money(account==1?-1000:1000,cad),"Reopening preserves each bank posting and balance");
        return sheet;
    };
    std::array<std::size_t,3> order{0,1,2};
    int serial=0;
    do {
        for(bool close:{false,true}){
            const auto path=dir/("provisional-"+std::to_string(++serial));
            auto store=create(path);
            ingest(store,order[0]);
            check(match(store,order[0]).linked==0,"First import has no partner");
            ingest(store,order[1]);
            const bool linked=order[2]!=0;
            check(match(store,order[1]).linked==std::size_t(linked),"First two imports link only when A is present");
            const auto before=transfer_snapshot(store);
            ingest(store,order[2]);
            const auto result=match(store,order[2]);
            check(result.reopened==std::size_t(linked) && result.linked==0,"Third import restores the choice in every order");
            review(store);
            store.verify();
            if(close){
                test_session::close_reopen(store,path,"synthetic");
                review(store);
                const auto preview=store.review_import_undo(batches[order[2]]);
                check(preview.affected_accounts.size()==(linked?3u:1u),"Import Undo previews all affected accounts");
                store.undo_import("undo-third",batches[order[2]],preview.token);
                check_transfer_snapshot(store,before);
                check(store.pending_review_links().size()==std::size_t(linked),"Import Undo restores the original link mark");
                store.verify();
            }else{
                verify_import_transfer_undo(path,store,batches[order[2]],before,1,linked?3u:1u);
            }
        }
    }while(std::next_permutation(order.begin(),order.end()));

    for(bool accept:{false,true}){
        auto store=create(dir/(accept?"accepted-link":"explicit-link"));
        ingest(store,0);ingest(store,1);
        if(accept){
            check(match(store,1).linked==1,"Auto Link creates the link to accept");
            store.accept_all("accept-link");
        }else{
            const auto sheet=store.review_sheet();
            app::ReviewAnswer answer;
            answer.generation=sheet.generation;
            answer.question=sheet.questions.front().key;
            answer.action=ReviewAction::link;
            answer.selected={{"a",0},{"b",0}};
            store.answer_review("choose-b",answer);
        }
        const auto before=store.transfers();
        ingest(store,2);
        const auto result=match(store,2);
        check(result.reopened==0 && result.linked==0,"A later candidate preserves accepted and explicitly chosen links");
        store.select_account(2);
        check(store.transfers().size()==before.size(),"The chosen link still connects to B");
        store.verify();
    }

    {
        auto store=create(dir/"provisional-retry");
        ingest(store,0);ingest(store,1);
        check(match(store,1).linked==1,"Fault fixture starts with an automatic link");
        ingest(store,2);
        const auto before=transfer_snapshot(store);
        OperationContext cancel;
        cancel.progress=[&](std::string_view phase){if(phase=="Saving the import matches")cancel.request();};
        bool cancelled=false;
        try{store.review_completed_import("match-c",{"c"},true,true,cancel);}
        catch(const OperationCancelled&){cancelled=true;}
        check(cancelled && !store.review_outcome("match-c-reopen-1"),"Cancel leaves the earlier link and no reopening receipt");
        check_transfer_snapshot(store,before);
        for(const int error:{SQLITE_FULL,SQLITE_IOERR_WRITE}){
            fault_vfs::arm(1,error,false);
            bool failed=false;
            try{match(store,2);}catch(const storage::ImportError&){failed=true;}
            fault_vfs::disarm();
            check(failed && fault_vfs::fired && !store.review_outcome("match-c-reopen-1"),"Write failure leaves no reopening receipt");
            check_transfer_snapshot(store,before);
            store.verify();
        }
        store.observer([](storage::ImportPhase phase){
            if(phase==storage::ImportPhase::after_commit)throw std::runtime_error("lost matching acknowledgement");
        });
        check(match(store,2).reopened==1,"A lost acknowledgement returns the durable result");
        store.observer({});
        const auto token=store.action_token();
        check(match(store,2).reopened==1 && store.action_token()==token,"Exact retry returns the original result without another decision");
        OperationContext changed;
        bool rejected=false;
        try{store.review_completed_import("match-c",{"c"},false,true,changed);}
        catch(const storage::ImportError& e){rejected=e.code()==SQLITE_CONSTRAINT;}
        check(rejected && store.action_token()==token,"Retry refuses a changed request");
        review(store);
        store.verify();
    }
    {
        auto store=create(dir/"provisional-adjacent-date");
        ingest(store,0);ingest(store,1);
        check(match(store,1).linked==1,"Adjacent-date fixture has a same-day first match");
        store.select_account(3);
        store.ingest("c","!Type:Bank\nD02/01/2026\nT10\nPTransfer receipt\n^\n",qif::DateOrder::dmy);
        check(match(store,2).reopened==1,"A later eligible date can reopen a same-day link");
        review(store);
        add_transfer_account(store,4,"D");
        store.ingest("d","!Type:Bank\nD01/01/2026\nT10\nPAnother deposit\n^\n",qif::DateOrder::dmy);
        OperationContext context;
        const auto next=store.review_completed_import("match-d",{"d"},true,true,context);
        check(next.linked==0 && store.review_sheet().questions.front().rows.size()==4,
              "A future import keeps the reopened choice in Review");
        store.verify();
    }
    for(const auto phase:{storage::ImportPhase::before_commit,storage::ImportPhase::after_commit}){
        const bool committed=phase==storage::ImportPhase::after_commit;
        const auto path=dir/(committed?"provisional-crash-after":"provisional-crash-before");
        {
            auto store=create(path);
            ingest(store,0);ingest(store,1);
            check(match(store,1).linked==1,"Crash fixture starts with an automatic link");
            ingest(store,2);
        }
        const auto child=fork();
        check(child>=0,"fork failed");
        if(child==0){
            try{
                auto store=ImportStore::open(path,"synthetic");
                store.observer([&](auto at){if(at==phase)_exit(71);});
                match(store,2);
                _exit(72);
            }catch(...){_exit(73);}
        }
        int status=0;
        check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71,"Crash reached the matching commit boundary");
        auto store=ImportStore::open(path,"synthetic");
        check(store.review_outcome("match-c-reopen-1").has_value()==committed &&
              store.pending_review_links().size()==(committed?0u:1u),"Recovered link and receipt agree with the matching commit boundary");
        if(!committed)check(match(store,2).reopened==1,"Reopening can be retried after recovery");
        review(store);
        store.verify();
    }
    {
        auto store=create(dir/"provisional-other-split");
        store.ingest("a","!Type:Bank\nD01/01/2026\nT-15\nPPayment\nSUncategorized\n$-10\nSUncategorized\n$-5\n^\n",qif::DateOrder::dmy);
        ingest(store,1);
        check(match(store,1).linked==1,"Only A's first split matches B");
        store.select_account(3);
        store.ingest("c","!Type:Bank\nD01/01/2026\nT5\nPOther receipt\n^\n",qif::DateOrder::dmy);
        OperationContext context;
        const auto result=store.review_completed_import("match-c",{"c"},false,false,context);
        check(result.reopened==0 && store.pending_review_links().size()==1,"A candidate for another split leaves the first link in place");
        store.verify();
    }
}

void import_transfer_undo_cases(const std::filesystem::path& dir){
    fault_vfs::install();
    provisional_import_link_orders(dir);
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
