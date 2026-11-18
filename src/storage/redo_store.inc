// Redo is connection-local. The durable format remains the ordinary v1 action
// chain: replay always creates a fresh action and fresh completion receipts.
namespace {
std::string redo_mapped(const std::map<std::string,std::string>& names,std::string_view kind,std::string name) {
    std::set<std::string> seen;
    for(;;){const auto found=names.find(std::string(kind)+name);if(found==names.end())return name;if(!seen.insert(name).second)fail(SQLITE_CORRUPT,"Redo identity mapping is inconsistent");name=found->second;}
}
imports::Occurrence redo_key(std::string_view bytes) {
    transfer_codec::Reader in{bytes};auto name=in.text();
    if(in.bytes.empty() || in.bytes.front()!=':')fail(SQLITE_CORRUPT,"Invalid saved transaction identity");
    in.bytes.remove_prefix(1);std::size_t ordinal=0;
    const auto end=in.bytes.data()+in.bytes.size();const auto parsed=std::from_chars(in.bytes.data(),end,ordinal);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end)fail(SQLITE_CORRUPT,"Invalid saved transaction identity");
    return {std::move(name),ordinal};
}
app::ReviewAnswer redo_answer(std::string_view bytes) {
    const bool automatic=bytes.ends_with("|auto");if(automatic)bytes.remove_suffix(5);
    auto number=[&]{const auto end=bytes.find(':');if(end==bytes.npos)fail(SQLITE_CORRUPT);int n=0;auto result=std::from_chars(bytes.data(),bytes.data()+end,n);if(result.ec!=std::errc{} || result.ptr!=bytes.data()+end)fail(SQLITE_CORRUPT);bytes.remove_prefix(end+1);return n;};
    (void)number();const auto kind=number();if(kind<0 || kind>4)fail(SQLITE_CORRUPT);
    transfer_codec::Reader in{bytes};app::ReviewAnswer answer;answer.automatic=automatic;answer.action=static_cast<app::ReviewAction>(kind);answer.question=in.text();
    std::vector<std::string> fields;while(in.bytes.size()>1)fields.push_back(in.text());
    if(fields.size()<4 || (in.bytes!="0" && in.bytes!="1"))fail(SQLITE_CORRUPT);
    answer.split_difference=in.bytes=="1";const auto selected=fields.size()-4;
    for(std::size_t i=0;i<selected;++i)answer.selected.push_back(redo_key(fields[i]));
    if(!fields[selected].empty())answer.survivor=redo_key(fields[selected]);
    answer.left_allocation=fields[selected+1];answer.right_allocation=fields[selected+2];answer.remainder_category=fields[selected+3];return answer;
}
}
std::vector<app::ActionSummary> ImportStore::redo_history() const {
    if(impl_->finalized || impl_->redo_epoch!=ledger_edit_generation()) {
        impl_->redo.clear();impl_->redo_names.clear();impl_->redo_epoch=ledger_edit_generation();
    }
    std::vector<app::ActionSummary> result;result.reserve(impl_->redo.size());
    for(auto it=impl_->redo.rbegin();it!=impl_->redo.rend();++it)result.push_back(it->action);
    return result;
}
imports::Occurrence ImportStore::redo_occurrence(imports::Occurrence id) const {
    id.batch=redo_mapped(impl_->redo_names,"batch:",id.batch);return id;
}
app::ReviewAnswer ImportStore::redo_review_answer(app::ReviewAnswer answer) const {
    answer.generation=ledger_edit_generation();
    answer.left_allocation=redo_mapped(impl_->redo_names,"allocation:",answer.left_allocation);
    answer.right_allocation=redo_mapped(impl_->redo_names,"allocation:",answer.right_allocation);
    for(auto& id:answer.selected)id=redo_occurrence(id);
    if(answer.survivor)answer.survivor=redo_occurrence(*answer.survivor);
    for(const auto prefix:{"same:","link:","repair:"})if(answer.question.starts_with(prefix))answer.question=std::string(prefix)+review_key(redo_occurrence(redo_key(std::string_view(answer.question).substr(std::string_view(prefix).size()))));
    if(answer.question.starts_with("waiting:"))answer.question="waiting:"+redo_mapped(impl_->redo_names,"link:",answer.question.substr(8));
    return answer;
}
app::TransferRequest ImportStore::redo_transfer(app::TransferRequest request,std::string_view operation) {
    auto mapped=[&](std::string name,std::string_view kind="link:"){return redo_mapped(impl_->redo_names,kind,std::move(name));};
    request.expected_sequence=transfer_sequence();
    if(request.category_generation>=0)request.category_generation=ledger_edit_generation();
    if(request.origin){request.origin=redo_occurrence(*request.origin);request.batch_revision=batch(impl_->db,request.origin->batch).revision;}
    for(auto* fact:{&request.left,&request.right})if(*fact){(*fact)->endpoint.entry=redo_occurrence((*fact)->endpoint.entry);(*fact)->endpoint.allocation=mapped((*fact)->endpoint.allocation,"allocation:");if(!(*fact)->endpoint.entry.batch.starts_with("~transfer-"))(*fact)->state=transaction_accepted((*fact)->endpoint.entry)?transfers::State::committed:transfers::State::draft;}
    if(request.left && !request.left->endpoint.entry.batch.starts_with("~transfer-"))request.batch_revision=batch(impl_->db,request.left->endpoint.entry.batch).revision;
    if(request.right && !request.right->endpoint.entry.batch.starts_with("~transfer-"))request.right_revision=batch(impl_->db,request.right->endpoint.entry.batch).revision;
    if(request.generated){request.generated->entry=redo_occurrence(request.generated->entry);request.generated->allocation=mapped(request.generated->allocation,"allocation:");}
    if(request.evidence){request.evidence->endpoint.entry=redo_occurrence(request.evidence->endpoint.entry);request.batch_revision=batch(impl_->db,request.evidence->endpoint.entry.batch).revision;}
    request.allocation=mapped(request.allocation,"allocation:");
    request.link=mapped(request.link);request.kept=mapped(request.kept);
    // Reuse an existing relationship identity when Undo left its removed head.
    // Otherwise a generated identity belongs to this new command.
    Statement link(impl_->db,"SELECT 1 FROM transfer_heads WHERE identity=?");link.text(1,request.link);
    if(!request.link.empty() && (request.action==app::TransferAction::manual || request.action==app::TransferAction::from_draft || !link.row())){const auto old=request.link;request.link=std::string(operation)+"-link";impl_->redo_names["link:"+old]=request.link;impl_->redo_names["batch:~transfer-"+old+"-origin"]="~transfer-"+request.link+"-origin";impl_->redo_names["batch:~transfer-"+old+"-destination"]="~transfer-"+request.link+"-destination";}
    if(!request.kept.empty()){const auto old=request.kept;request.kept=std::string(operation)+"-kept";impl_->redo_names["link:"+old]=request.kept;}
    return request;
}
ImportStore::RedoFrame ImportStore::capture_redo(const app::ActionSummary& action) const {
    // Recorded actions can span accounts; source reads must use each member's owner.
    struct Selection {std::int64_t& value;std::int64_t prior;~Selection(){value=prior;}} selection{impl_->selected_account,impl_->selected_account};
    RedoFrame frame;frame.action=action;frame.action.label=action_display_label(action);
    if(action.kind==app::ActionKind::import)return frame;
    auto available=accounts();const auto archived=archived_accounts();available.insert(available.end(),archived.begin(),archived.end());
    for(const auto& account:available)if(deletion_affects_action(action,account.identity,{},true))frame.accounts.insert(account.identity);
    frame.categories=action_categories(action,true);
    if(action.kind==app::ActionKind::manual_creation) {
        const auto id=action.payload;const auto owner=account_for_batch(id).identity;const auto draft=*load_row({id,0},false).draft;
        frame.replay=[id,owner,draft](ImportStore& store,std::string_view operation){store.impl_->selected_account=owner;store.impl_->redo_names["batch:"+id]=std::string(operation);store.create_manual(operation,draft);};return frame;
    }
    if(action.kind==app::ActionKind::opening || action.kind==app::ActionKind::currency) {
        Statement saved(impl_->db,"SELECT account,definition,opening,opening_date FROM account_operations WHERE id=?");saved.text(1,action.payload);if(!saved.row())fail(SQLITE_CORRUPT);
        const auto owner=saved.integer(0);const auto currency=Currency::from_definition(saved.bytes(1));const auto amount=Money(saved.integer(2),currency);const auto date=saved.bytes(3);const auto kind=action.kind;
        frame.replay=[owner,currency,amount,date,kind](ImportStore& store,std::string_view operation){store.impl_->selected_account=owner;if(kind==app::ActionKind::opening)store.correct_opening(operation,date,amount);else store.reassign_currency(operation,currency);};return frame;
    }
    if(action.kind==app::ActionKind::acceptance) {
        std::set<imports::Occurrence> membership;
        Statement rows(impl_->db,"SELECT o.batch,m.ordinal FROM acceptance_members m JOIN operations o ON o.id=m.operation WHERE m.decision=1 AND (o.id=?1 OR o.id IN(SELECT operation FROM acceptance_operations WHERE parent=?1))");rows.text(1,action.payload);while(rows.row())membership.insert({rows.bytes(0),static_cast<std::size_t>(rows.integer(1))});
        frame.replay=[membership](ImportStore& store,std::string_view operation){
            // Linear replay permits no intervening saved mutation. Reacceptance
            // uses the same domain command; validate its exact membership before commit.
            store.impl_->redo_acceptance_members.emplace();
            for(const auto& id:membership)store.impl_->redo_acceptance_members->insert(store.redo_occurrence(id));
            store.accept_all(operation,true);
        };return frame;
    }
    Statement root(impl_->db,"SELECT question,request FROM review_answers WHERE id=?");root.text(1,action.payload);if(!root.row())fail(SQLITE_CORRUPT,"Redo details are missing");
    const auto question=root.bytes(0),request=root.bytes(1);auto owner=impl_->selected_account;
    if(question=="review-exclusion") {
        Statement saved(impl_->db,"SELECT identity,field,expression,account,regex,removed FROM link_exclusion_events WHERE operation=?");saved.text(1,action.payload);if(!saved.row())fail(SQLITE_CORRUPT);
        app::LinkExclusion rule{saved.bytes(0),saved.bytes(1),saved.bytes(2),saved.integer(3),saved.integer(4)!=0,saved.integer(5)!=0};
        frame.replay=[rule](ImportStore& store,std::string_view operation){store.save_link_exclusion(operation,rule,store.ledger_edit_generation());};return frame;
    }
    if(question=="batch" || question=="selected" || question.starts_with("same:") || question.starts_with("link:") || question.starts_with("waiting:")) {
        std::vector<app::ReviewAnswer> answers;
        if(question=="batch"){transfer_codec::Reader in{std::string_view(request).substr(6)};while(!in.bytes.empty())answers.push_back(redo_answer(in.text()));}else answers.push_back(redo_answer(request));
        std::vector<std::string> created_links;
        Statement created(impl_->db,"SELECT identity FROM review_link_history WHERE (answer=?1 OR answer IN(SELECT id FROM review_answers WHERE question='batch-child:'||?1)) AND prior_payload IS NULL");created.text(1,action.payload);while(created.row())created_links.push_back(created.bytes(0));
        frame.replay=[answers,owner,grouped=question=="batch",original=action.payload,created_links](ImportStore& store,std::string_view operation){
            for(const auto& link:created_links)if(link.starts_with(original+"-"))store.impl_->redo_names["link:"+link]=std::string(operation)+link.substr(original.size());
            store.impl_->redo_names["allocation:"+original+"-remainder"]=std::string(operation)+"-remainder";
            for(std::size_t i=0;i<answers.size();++i)store.impl_->redo_names["allocation:"+original+"-group-"+std::to_string(i+1)+"-remainder"]=std::string(operation)+"-group-"+std::to_string(i+1)+"-remainder";
            auto fresh=answers;for(auto& answer:fresh)answer=store.redo_review_answer(answer);store.impl_->selected_account=fresh.front().selected.empty()?owner:store.account_for_batch(fresh.front().selected.front().batch).identity;if(!grouped)store.answer_review(operation,fresh.front());else {app::OperationContext context;store.write_review_batch(operation,fresh,context);}};return frame;
    }
    if(question=="unmerge" || question=="unmerge-link") {
        imports::Occurrence selected;
        if(question=="unmerge"){auto rest=std::string_view(request);const auto first=rest.find('\n');if(first==rest.npos)fail(SQLITE_CORRUPT);rest.remove_prefix(first+1);const auto end=rest.find('\n');if(end==rest.npos)fail(SQLITE_CORRUPT);selected.batch=rest.substr(0,end);rest.remove_prefix(end+1);const auto n=rest.find('\n');if(n==rest.npos)fail(SQLITE_CORRUPT);const auto result=std::from_chars(rest.data(),rest.data()+n,selected.ordinal);if(result.ec!=std::errc{} || result.ptr!=rest.data()+n)fail(SQLITE_CORRUPT);}
        else if(request.starts_with("selection:"))selected=redo_key(std::string_view(request).substr(10));
        else {Statement link(impl_->db,"SELECT prior_payload FROM review_link_history WHERE answer=? LIMIT 1");link.text(1,action.payload);if(!link.row())fail(SQLITE_CORRUPT);selected=transfer_codec::view(link.bytes(0)).link.origin.endpoint.entry;}
        frame.replay=[selected](ImportStore& store,std::string_view operation){const auto id=store.redo_occurrence(selected);store.impl_->selected_account=store.account_for_batch(id.batch).identity;store.unmerge(operation,{id});};return frame;
    }
    if(question=="ledger-transfer" || question=="transfer-assignment") {
        std::vector<app::TransferRequest> requests;
        if(question=="ledger-transfer"){Statement saved(impl_->db,"SELECT request,account FROM transfer_events WHERE operation=?");saved.text(1,action.payload);if(!saved.row())fail(SQLITE_CORRUPT);requests.push_back(transfer_codec::request(saved.bytes(0)));owner=saved.integer(1);}
        else {transfer_codec::Reader in{std::string_view(request).substr(std::string_view("transfer-assignment:").size())};while(!in.bytes.empty())requests.push_back(transfer_codec::request(in.text()));}
        frame.replay=[requests,owner,question](ImportStore& store,std::string_view operation){auto fresh=requests;std::size_t i=0;for(auto& value:fresh)value=store.redo_transfer(value,std::string(operation)+"-"+std::to_string(++i));store.impl_->selected_account=fresh.front().left?fresh.front().left->endpoint.account:owner;if(question=="ledger-transfer")store.apply_transfer(operation,fresh.front());else{app::OperationContext context;store.assign_transfer_batch(operation,fresh,context);}};return frame;
    }
    if(question=="ledger-delete") {
        transfer_codec::Reader in{request};app::LedgerSelectionEdit choice;choice.action=static_cast<app::LedgerSelectionAction>(in.number());choice.account=in.number();(void)in.number();choice.payee=in.text();while(!in.bytes.empty()){const auto field=in.text();if(field=="delete-counterparts")choice.delete_counterparts=true;else choice.selected.push_back(redo_key(field));}
        frame.replay=[choice](ImportStore& store,std::string_view operation){auto fresh=choice;fresh.generation=store.ledger_edit_generation();for(auto& id:fresh.selected)id=store.redo_occurrence(id);store.impl_->selected_account=fresh.account;app::OperationContext context;store.delete_transactions(operation,fresh,context);};return frame;
    }
    // Ordinary edits and rule application replay recorded results. They do not
    // re-evaluate expressions, import matching, or the current rule definitions.
    struct Member {imports::Occurrence id;imports::DraftRow draft;imports::Decision decision;bool active;std::optional<std::int64_t> ruleset;};
    std::vector<Member> members;
    Statement rows(impl_->db,"SELECT batch,ordinal,committed,result FROM review_members WHERE answer=? ORDER BY rowid");rows.text(1,action.payload);
    while(rows.row()){
        const imports::Occurrence id{rows.bytes(0),static_cast<std::size_t>(rows.integer(1))};const auto committed=rows.integer(2)!=0;const auto revision=rows.integer(3);
        impl_->selected_account=account_for_batch(id.batch).identity;
        auto draft=committed?entry_snapshot(id,revision).draft:review_draft(id,revision);imports::Decision decision{imports::Disposition::include,{}};bool active=true;
        if(committed)active=entry_snapshot(id,revision).active;
        else {Statement saved(impl_->db,"SELECT decision,duplicate_batch,duplicate_ordinal FROM versions WHERE batch=? AND ordinal=? AND revision=?");saved.text(1,id.batch);saved.integer(2,integer(id.ordinal));saved.integer(3,revision);if(!saved.row())fail(SQLITE_CORRUPT);decision.disposition=static_cast<imports::Disposition>(saved.integer(0));if(decision.disposition==imports::Disposition::duplicate)decision.duplicate_of=imports::Occurrence{saved.bytes(1),static_cast<std::size_t>(saved.integer(2))};}
        std::optional<std::int64_t> ruleset;
#ifdef SPENTIT_RULES_STORAGE
        Statement marks(impl_->db,committed?"SELECT ruleset_revision FROM entry_rule_fields WHERE batch=? AND ordinal=? AND revision=?":"SELECT ruleset_revision FROM draft_rule_fields WHERE batch=? AND ordinal=? AND revision=?");marks.text(1,id.batch);marks.integer(2,integer(id.ordinal));marks.integer(3,revision);if(marks.row())ruleset=marks.integer(0);
#endif
        members.push_back({id,std::move(draft),decision,active,ruleset});
    }
    Statement repair(impl_->db,"SELECT batch,ordinal,result FROM review_repairs WHERE answer=?");repair.text(1,action.payload);
    if(repair.row()) {
        const imports::Occurrence id{repair.bytes(0),static_cast<std::size_t>(repair.integer(1))};const auto draft=review_draft(id,repair.integer(2));
        frame.replay=[id,draft](ImportStore& store,std::string_view){const auto target=store.redo_occurrence(id);store.impl_->selected_account=store.account_for_batch(target.batch).identity;store.save(target,batch(store.impl_->db,target.batch).revision,draft);};return frame;
    }
    if(members.empty())fail(SQLITE_CONSTRAINT,"This action has no Redo details");
    Statement linked(impl_->db,"SELECT 1 FROM review_link_history WHERE answer=? LIMIT 1");linked.text(1,action.payload);const bool has_links=linked.row();
    frame.replay=[members,has_links,label=frame.action.label,kind=action.kind](ImportStore& store,std::string_view operation){

        Atomic transaction(store.impl_->db);const auto generation=store.ledger_edit_generation();
        std::string question="ledger-edit",encoded(operation);
#ifdef SPENTIT_RULES_STORAGE
        if(kind==app::ActionKind::rule_application){
            app::AccountRuleRequest request;request.all_accounts=true;request.generation=generation;request.catalog_revision=catalog_sequence(store.impl_->db);
            for(const auto& member:members)if(member.ruleset)request.ruleset_revision=std::max(request.ruleset_revision,*member.ruleset);
            question="rules-account";encoded=account_rule_request(request);
        }
#endif
        Statement root(store.impl_->db,"INSERT INTO review_answers VALUES(?,(SELECT coalesce(max(sequence),0)+1 FROM review_answers),2,?,?,?)");root.text(1,operation);root.text(2,question);root.text(3,encoded);root.integer(4,generation);root.done();
        std::size_t serial=0;
        for(const auto& member:members){
            auto id=store.redo_occurrence(member.id);auto draft=member.draft;
            auto allocation_name=[&](std::string name){return redo_mapped(store.impl_->redo_names,"allocation:",std::move(name));};
            auto allocations=draft.transaction.allocations();for(auto& allocation:allocations)allocation.identity=allocation_name(allocation.identity);
            draft.remainder_allocation=allocation_name(draft.remainder_allocation);draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),std::move(allocations));
            auto decision=member.decision;if(decision.duplicate_of)decision.duplicate_of=store.redo_occurrence(*decision.duplicate_of);
            const auto child=std::string(operation)+"-"+std::to_string(++serial);
            const auto before_links=has_links?store.all_transfers():std::vector<app::TransferView>{};
            std::optional<EntryLinkPlan> links;
            if(has_links && store.transaction_accepted(id))links=store.plan_entry_links(before_links,id,store.committed_entry(id).current.draft,draft,!member.active);
            store.write_review_row(operation,child,id,draft,decision,member.active,false,false,member.ruleset);
            Statement result(store.impl_->db,"SELECT committed,result FROM review_members WHERE answer=? AND child=?");result.text(1,operation);result.text(2,child);if(!result.row())fail(SQLITE_CORRUPT);
            retain_redo_image(store.impl_->db,id,result.integer(1),result.integer(0)!=0,draft);
            if(links && !links->changed.empty()){
                store.write_entry_links(child,store.impl_->selected_account,app::TransferAction::relink,financial_sequence(store.impl_->db),links->changed);
                for(const auto& value:links->changed){
                    const auto old=std::find_if(before_links.begin(),before_links.end(),[&](const auto& v){return v.link.identity==value.link.identity;});
                    Statement history(store.impl_->db,"INSERT INTO review_link_history VALUES(?,?,?,?,?,?)");history.text(1,operation);history.text(2,value.link.identity);
                    if(old!=before_links.end())history.blob(3,transfer_codec::encode(*old));history.blob(4,transfer_codec::encode(value));history.text(5,child);history.integer(6,old==before_links.end()?0:old->sequence);history.done();
                }
            }
        }
        Statement stack(store.impl_->db,"INSERT INTO review_stack VALUES(?,?)");stack.text(1,operation);stack.integer(2,generation+1);stack.done();
        app::ActionSummary summary;summary.id=operation;summary.payload=operation;summary.label=label;summary.kind=kind;summary.count=integer(members.size());store.impl_->pending_action=summary;store.impl_->pending_action_digest=session_snapshot::digest(std::string(operation));
        store.finish([&]{transaction.commit();});
    };return frame;
}
std::optional<app::ActionReceipt> ImportStore::redo_action_outcome(std::string_view operation,const app::UndoPreview& preview) const {
    if(!sqlite3_get_autocommit(impl_->db))throw app::OperationOutcomeUnknown();
    const auto found=impl_->redo_requests.find(std::string(operation));if(found==impl_->redo_requests.end())return {};
    if(found->second.preview.token!=preview.token || found->second.preview.action.id!=preview.action.id)fail(SQLITE_CONSTRAINT,"Redo retry changed its request");
    if(!found->second.completed)return {};
    return app::ActionReceipt{std::string(operation),std::string(operation)};
}
app::ActionReceipt ImportStore::redo_action(std::string_view operation,const app::UndoPreview& preview) {
    identifier(operation);if(operation.size()>64)fail(SQLITE_MISUSE);
    if(const auto prior=redo_action_outcome(operation,preview))return *prior;
    const auto available=redo_history();ActionChain(impl_->db).require_current(preview.token);ActionChain(impl_->db).require_idle();
    if(available.empty() || available.front().id!=preview.action.id)fail(SQLITE_CONSTRAINT,"There is no action to redo");
    Statement used(impl_->db,"SELECT 1 FROM action_receipts WHERE id=?");used.text(1,operation);
    if(used.row())fail(SQLITE_CONSTRAINT,"Redo requires a new operation identity");
    auto& request=impl_->redo_requests.try_emplace(std::string(operation),Impl::RedoRequest{preview,false}).first->second;
    auto names=impl_->redo_names;const auto selected=impl_->selected_account;
    struct Selection{Impl& state;std::int64_t selected;~Selection(){state.selected_account=selected;state.redo_operation.clear();state.redo_acceptance_members.reset();}} selection{*impl_,selected};
    impl_->redo_operation=operation;
    try {impl_->redo.back().replay(*this,operation);}
    catch(...) {
        if(!sqlite3_get_autocommit(impl_->db)){impl_->redo_names.swap(names);throw;}
        Statement saved(impl_->db,"SELECT 1 FROM action_headers WHERE id=? AND state=1");saved.text(1,operation);
        if(!saved.row()){impl_->redo_names.swap(names);throw;}
        // The transaction committed; the observer/refresh failed afterward.
        request.completed=true;impl_->redo.pop_back();impl_->redo_epoch=ledger_edit_generation();throw;
    }
    request.completed=true;impl_->redo.pop_back();impl_->redo_epoch=ledger_edit_generation();return {std::string(operation),std::string(operation)};
}
