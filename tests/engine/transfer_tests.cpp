#include <spentit/transfer.hpp>
#include <iostream>
#include <limits>
#include <source_location>
using namespace spentit;
using namespace spentit::transfers;
namespace {
const Currency cad("CAD");
void check(bool value,const std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error("Transfer assertion at line "+std::to_string(at.line()));
}
template<class F> void rejected(Error expected,F command) {
    try { command(); } catch(const TransferError& error) {check(error.code()==expected);return;}
    throw std::runtime_error("Expected transfer rejection");
}
imports::DraftRow source() {
    return {"2026-10-26","Synthetic payee","",Transaction::imported(Money(-1000,cad),{
        {"","",Money(-600,cad),{},"payment-a"},
        {"","",Money(-600,cad),{},"payment-b"},
        {"Refund","",Money(200,cad),{},"refund"}})};
}
Link generated() {return generate("link-a",{1,{"checking",0},"payment-a"},source(),{2,{"generated-a",0},"counterpart"},cad);}
void lifecycle() {
    const auto draft=generated();
    check(draft.origin.amount==Money(-600,cad) && draft.destination.amount==Money(600,cad));
    check(draft.destination.provisional_date && !draft.destination.independent_evidence);
    check(!protected_draft(draft,1) && !protected_draft(draft,2));
    for(const auto first:{1,2}) {
        const auto second=3-first;
        const auto published=commit(accept_unmatched(draft,2),first);
        check(protected_draft(published,second) && !protected_draft(published,first));
        const auto cleared=clear_workspace(published,second);
        check(cleared.origin.state==published.origin.state && cleared.destination.state==published.destination.state);
        const auto both=commit(published,second);
        check(both.origin.state==State::committed && both.destination.state==State::committed);
        check(!protected_draft(both,1) && !protected_draft(both,2));
        check(commit(both,first).destination.state==State::committed);
    }
    check(draft.origin.state==State::draft && draft.destination.state==State::draft);
    const auto removed=clear_workspace(draft,1);
    check(removed.origin.state==State::removed && removed.destination.state==State::removed);
    rejected(Error::invalid_relationship,[&]{commit(removed,2);});
    rejected(Error::origin_controls_draft,[&]{clear_workspace(draft,2);});
    rejected(Error::invalid_relationship,[&]{commit(draft,3);});
    const auto other=generate("link-b",{1,{"checking",0},"payment-b"},source(),{3,{"generated-b",0},"counterpart"},cad);
    check(other.origin.amount==draft.origin.amount && other.origin.endpoint!=draft.origin.endpoint);
    auto reordered=source();auto allocations=reordered.transaction.allocations();std::swap(allocations[0],allocations[2]);
    reordered.transaction=reordered.transaction.corrected(reordered.transaction.effective_total(),allocations);
    check(generate("link",draft.origin.endpoint,reordered,draft.destination.endpoint,cad).origin.amount==draft.origin.amount);
    const auto positive=generate("refund-link",{1,{"checking",0},"refund"},source(),{3,{"generated-refund",0},"counterpart"},cad);
    check(positive.destination.amount==Money(-200,cad));
}
void matching() {
    const auto draft=generated();const Endpoint candidate{2,{"visa-bank",0},"source-0"};
    validate_match(draft,candidate,Money(600,cad),"2026-10-28");
    validate_match(commit(draft,1),candidate,Money(600,cad),"2026-10-28");
    validate_match(commit(commit(accept_unmatched(draft,2),1),2),candidate,Money(600,cad),"2026-10-28");
    check(draft.destination.date=="2026-10-26" && draft.destination.provisional_date);
    rejected(Error::amount_mismatch,[&]{validate_match(draft,candidate,Money(599,cad),"2026-10-28");});
    rejected(Error::cross_currency,[&]{validate_match(draft,candidate,Money(600,Currency("USD")),"2026-10-28");});
    rejected(Error::invalid_relationship,[&]{validate_match(draft,{3,{"other",0},"source-0"},Money(600,cad),"2026-10-28");});
    rejected(Error::invalid_relationship,[&]{validate_match(draft,candidate,Money(600,cad),"2026-02-30");});
    auto attached=commit_import_match(propose_match(draft,2,{candidate,Money(600,cad),"2026-10-28"}),2,{candidate,Money(600,cad),"2026-10-28"});
    validate(attached);
    rejected(Error::unresolved_policy,[&]{validate_match(attached,candidate,Money(600,cad),"2026-10-28");});
    check(clear_workspace(attached,1).origin.state==State::draft);
    auto evidenced_draft=attached;evidenced_draft.destination.state=State::draft;
    const auto unlinked=clear_workspace(evidenced_draft,1);
    check(unlinked.unresolved && unlinked.origin.state==State::removed);
    check(unlinked.destination.amount==evidenced_draft.destination.amount && unlinked.destination.date==evidenced_draft.destination.date);
    check(unlinked.destination.state==State::draft && unlinked.destination.endpoint==evidenced_draft.destination.endpoint);
    check(unlinked.destination.attached_evidence==evidenced_draft.destination.attached_evidence);
    rejected(Error::origin_controls_draft,[&]{clear_workspace(unlinked,2);});
    auto direct=unlinked;direct.destination.attached_evidence.reset();
    const auto discarded=clear_workspace(direct,2);check(discarded.origin.state==State::removed && discarded.destination.state==State::removed);validate(discarded);
}
void boundaries() {
    const auto original=generated();
    rejected(Error::cross_currency,[&]{generate("x",original.origin.endpoint,source(),original.destination.endpoint,Currency("USD"));});
    rejected(Error::invalid_relationship,[&]{generate("x",{1,{"checking",0},"missing"},source(),original.destination.endpoint,cad);});
    rejected(Error::invalid_relationship,[&]{generate("x",original.origin.endpoint,source(),{1,{"other",0},"counterpart"},cad);});
    auto invalid=original;invalid.destination.amount=Money(-600,cad);
    rejected(Error::amount_mismatch,[&]{validate(invalid);});
    invalid=original;invalid.destination.state=State::removed;
    rejected(Error::invalid_relationship,[&]{validate(invalid);});
    invalid=original;invalid.destination.state=static_cast<State>(99);
    rejected(Error::invalid_relationship,[&]{validate(invalid);});
    invalid=original;invalid.destination.date="2026-10-27";
    rejected(Error::invalid_relationship,[&]{validate(invalid);});
    invalid=original;invalid.origin.date="2026-10-27";
    rejected(Error::invalid_relationship,[&]{validate(invalid);});
    invalid=original;invalid.origin.attached_evidence=BankEvidence{{1,{"invented",0},"source-0"},Money(-600,cad),"2026-10-26"};
    rejected(Error::invalid_relationship,[&]{validate(invalid);});
    auto extreme=source();const auto minimum=Money(std::numeric_limits<std::int64_t>::min(),cad);
    extreme.transaction=Transaction::imported(minimum,{{"","",minimum,{},"payment-a"}});
    bool overflow=false;
    try {generate("x",original.origin.endpoint,extreme,original.destination.endpoint,cad);}
    catch(const FinancialError& error) {overflow=error.code()==ErrorCode::overflow;}
    check(overflow && extreme.transaction.effective_total()==minimum);
    auto duplicate=source();auto allocations=duplicate.transaction.allocations();allocations[1].identity=allocations[0].identity;
    duplicate.transaction=duplicate.transaction.corrected(duplicate.transaction.effective_total(),allocations);
    rejected(Error::invalid_relationship,[&]{generate("x",original.origin.endpoint,duplicate,original.destination.endpoint,cad);});
}
void groups() {
    const auto first=generated();
    const auto second=generate("link-b",{1,{"checking",0},"payment-b"},source(),{3,{"generated-b",0},"counterpart"},cad);
    const std::vector<Link> links{first,second};validate_set(links);
    const auto committed=commit_account(links,1);
    check(committed[0].origin.state==State::committed && committed[1].origin.state==State::committed);
    check(protected_draft(committed[0],2) && protected_draft(committed[1],3));
    const auto destination_first=commit_account(std::vector<Link>{accept_unmatched(first,2),second},2);
    const auto cleared=clear_account(destination_first,1);
    // A second transfer allocation must not disappear from a protected origin.
    check(cleared[0].origin.state==State::draft && cleared[1].origin.state==State::draft);
    check(cleared[1].destination.state==State::draft);
    const auto discarded=clear_account(links,1);
    check(discarded[0].origin.state==State::removed && discarded[1].destination.state==State::removed);
    const std::vector<Link> duplicate{first,first};
    rejected(Error::invalid_relationship,[&]{commit_account(duplicate,1);});
    auto same_endpoint=second;same_endpoint.origin.endpoint=first.origin.endpoint;
    rejected(Error::invalid_relationship,[&]{validate_set(std::vector<Link>{first,same_endpoint});});
    rejected(Error::invalid_relationship,[&]{validate_set(std::vector<Link>{commit(first,1),second});});
    rejected(Error::origin_controls_draft,[&]{clear_account(links,2);});
    check(links[0].origin.state==State::draft && links[1].destination.state==State::draft);
}
void acceptance() {
    const auto original=generated();
    rejected(Error::acceptance_required,[&]{commit(original,2);});
    const auto accepted=accept_unmatched(original,2);
    check(accepted.destination.accepted_unmatched && !accepted.destination.independent_evidence);
    check(accepted.destination.state==State::draft && accepted.origin.state==State::draft);
    check(accepted.destination.amount==original.destination.amount && accepted.destination.date==original.destination.date);
    const auto committed=commit(accepted,2);
    check(committed.destination.state==State::committed && protected_draft(committed,1));
    check(!committed.destination.attached_evidence && committed.destination.provisional_date);
    check(accept_unmatched(accepted,2).destination.accepted_unmatched);
    rejected(Error::invalid_relationship,[&]{accept_unmatched(original,1);});
    rejected(Error::invalid_relationship,[&]{accept_unmatched(clear_workspace(original,1),2);});
    const BankEvidence mismatch{{2,{"visa-95",0},"source-0"},Money(595,cad),"2026-10-28"};
    rejected(Error::amount_mismatch,[&]{propose_match(committed,2,mismatch);});
    check(committed.destination.amount==Money(600,cad));
    auto corrupt=accepted;corrupt.destination.amount=Money(595,cad);
    rejected(Error::amount_mismatch,[&]{commit(corrupt,2);});
}
void proposals() {
    const BankEvidence evidence{{2,{"visa-bank",0},"source-0"},Money(600,cad),"2026-10-28"};
    for(int stage=0;stage<3;++stage) {
        auto before=generated();
        if(stage>=1)before=commit(before,1);
        if(stage==2)before=commit(accept_unmatched(before,2),2);
        const auto proposed=propose_match(before,2,evidence);
        check(proposed.destination.proposed_evidence==evidence && !proposed.destination.attached_evidence);
        check(proposed.destination.date==before.destination.date && proposed.destination.state==before.destination.state);
        check(proposed.destination.amount==before.destination.amount && proposed.origin.state==before.origin.state);
        check(protected_draft(proposed,1)==protected_draft(before,1) && protected_draft(proposed,2)==protected_draft(before,2));
        rejected(Error::unresolved_policy,[&]{commit(proposed,2);});
        rejected(Error::invalid_relationship,[&]{accept_unmatched(proposed,2);});
        const auto discarded=discard_import_match(proposed,2,"visa-bank");
        check(!discarded.destination.proposed_evidence && !discarded.destination.independent_evidence);
        check(discarded.destination.date==before.destination.date && discarded.destination.state==before.destination.state);
        check(discarded.destination.accepted_unmatched==before.destination.accepted_unmatched);
        const auto matched=commit_import_match(proposed,2,evidence);
        check(matched.destination.endpoint==before.destination.endpoint && matched.destination.attached_evidence==evidence);
        check(matched.destination.independent_evidence && !matched.destination.provisional_date);
        check(matched.destination.date=="2026-10-28" && matched.generated_date=="2026-10-26");
        check(matched.destination.state==State::committed && matched.origin.state==before.origin.state);
        check(matched.destination.amount==before.destination.amount && !matched.destination.proposed_evidence);
        auto stale=evidence;stale.posting_date="2026-10-29";
        rejected(Error::invalid_relationship,[&]{commit_import_match(proposed,2,stale);});
        rejected(Error::invalid_relationship,[&]{discard_import_match(proposed,2,"another-import");});
        rejected(Error::invalid_relationship,[&]{commit_import_match(proposed,1,evidence);});
        rejected(Error::unresolved_policy,[&]{propose_match(proposed,2,stale);});
    }
    auto proposed=propose_match(generated(),2,evidence);
    const auto cleared=clear_workspace(proposed,1);
    check(cleared.origin.state==State::removed && !cleared.destination.proposed_evidence);
    // One bank allocation cannot provide evidence for two equal movements.
    auto other=generate("link-b",{1,{"checking",0},"payment-b"},source(),{2,{"generated-b",0},"counterpart"},cad);
    other=propose_match(other,2,evidence);
    rejected(Error::invalid_relationship,[&]{validate_set(std::vector<Link>{proposed,other});});
    const BankEvidence second{{2,{"visa-bank",1},"source-0"},Money(600,cad),"2026-10-29"};
    other=propose_match(discard_import_match(other,2,"visa-bank"),2,second);
    const std::vector<Link> proposals{proposed,other};
    const auto published=commit_import_matches(proposals,2,"visa-bank");
    check(published[0].destination.attached_evidence==evidence && published[1].destination.attached_evidence==second);
    check(published[0].origin.state==State::draft && published[1].origin.state==State::draft);
    const auto cancelled=discard_import_matches(proposals,2,"visa-bank");
    check(!cancelled[0].destination.proposed_evidence && !cancelled[1].destination.proposed_evidence);
    check(cancelled[0].destination.state==State::draft && cancelled[1].destination.state==State::draft);
    check(commit_import_matches(proposals,1,"visa-bank")[0].destination.proposed_evidence==evidence);
    check(discard_import_matches(proposals,2,"unrelated")[1].destination.proposed_evidence==second);
}
void manual() {
    auto repaired=source();repaired.transaction=Transaction::repaired_import({},repaired.transaction.effective_total(),repaired.transaction.allocations());
    const auto imported=generate("repaired-import",{1,{"repaired",0},"payment-a"},repaired,{2,{"generated-repaired",0},"counterpart"},cad,true);
    check(imported.origin.independent_evidence && !imported.origin.manually_entered && !repaired.transaction.original_imported_amount());
    auto draft=source();draft.transaction=Transaction::manual(draft.transaction.effective_total(),draft.transaction.allocations());
    const auto link=generate("manual-link",{1,{"manual",0},"payment-a"},draft,{2,{"generated",0},"counterpart"},cad,false);
    check(link.origin.manually_entered && !link.origin.independent_evidence && !draft.transaction.original_imported_amount());
    rejected(Error::acceptance_required,[&]{commit(link,1);});
    auto accepted=accept_unmatched(accept_unmatched(link,1),2);
    const auto committed=commit(commit(accepted,1),2);
    check(committed.origin.state==State::committed && committed.destination.state==State::committed);
    check(!committed.origin.independent_evidence && !committed.destination.independent_evidence);
    const BankEvidence evidence{{1,{"checking-bank",0},"source-0"},Money(-600,cad),"2026-10-27"};
    const auto matched=commit_import_match(propose_match(committed,1,evidence),1,evidence);
    check(matched.origin.manually_entered && matched.origin.independent_evidence);
    check(matched.origin.date=="2026-10-27" && matched.destination.date=="2026-10-26");
    check(matched.destination.provisional_date && matched.generated_date=="2026-10-26");
    auto other=generate("manual-b",{1,{"manual",0},"payment-b"},draft,{3,{"generated-b",0},"counterpart"},cad,false);
    validate_set(std::vector<Link>{link,other});
    rejected(Error::invalid_relationship,[&]{validate_set(std::vector<Link>{accept_unmatched(link,1),other});});
    const auto accepted_entry=accept_entry(std::vector<Link>{link,other},1,{"manual",0});
    check(accepted_entry[0].origin.accepted_unmatched && accepted_entry[1].origin.accepted_unmatched);
    check(commit_account(accepted_entry,1)[1].origin.state==State::committed);
    rejected(Error::invalid_relationship,[&]{accept_entry(std::vector<Link>{link,other},1,{"missing",0});});
}
AllocationFact fact(Endpoint endpoint,Money amount,std::string date,State state,bool imported,bool generated=false,bool manual=false) {
    return {std::move(endpoint),amount,std::move(date),state,imported,generated,manual};
}
void management() {
    const Currency usd("USD");
    const AllocationFact visa=fact({2,{"visa",0},"source-0"},Money(9500,cad),"2026-10-28",State::committed,true);
    const AllocationFact whole=fact({1,{"checking",0},"whole"},Money(-10000,cad),"2026-10-26",State::committed,true);
    const AllocationFact fee=fact({1,{"checking",0},"fee"},Money(-500,cad),"2026-10-26",State::draft,true);
    const AllocationFact part=fact({1,{"checking",0},"transfer"},Money(-9500,cad),"2026-10-26",State::draft,true);
    rejected(Error::amount_mismatch,[&]{merge({},whole,visa,"new","kept");});
    rejected(Error::amount_mismatch,[&]{merge({},fee,visa,"new","kept");});
    const auto linked=merge({},part,visa,"pay-95","kept");
    check(!linked.kept && linked.link.origin.endpoint.allocation=="transfer" && linked.link.destination.endpoint==visa.endpoint);
    check(linked.link.origin.amount==Money(-9500,cad) && linked.link.destination.amount==Money(9500,cad));
    const auto repeated=merge({},fact({1,{"checking",1},"transfer"},Money(-9500,cad),"2026-10-26",State::draft,true),fact({2,{"visa",1},"source-0"},Money(9500,cad),"2026-10-28",State::draft,true),"pay-95-b","kept");
    check(repeated.link.origin.endpoint!=linked.link.origin.endpoint && repeated.link.destination.endpoint!=linked.link.destination.endpoint);
    validate_set(std::vector<Link>{linked.link,repeated.link});
    const auto draft=generated();
    const auto generated_fact=fact(draft.destination.endpoint,draft.destination.amount,draft.destination.date,State::draft,false,true);
    const auto independent=fact({2,{"visa-bank",0},"source-0"},Money(600,cad),"2026-10-28",State::draft,true);
    const std::vector<Link> one{draft};
    const auto merged=merge(one,generated_fact,independent,draft.identity,"kept");
    check(!merged.kept && merged.link.destination.endpoint==independent.endpoint && merged.link.destination.amount==draft.destination.amount);
    check(merged.link.origin.endpoint==draft.origin.endpoint && merged.link.origin.amount==draft.origin.amount && merged.link.origin.date==draft.origin.date);
    check(merged.link.destination.date=="2026-10-28" && merged.link.generated_date==draft.generated_date);
    const auto protected_link=commit(draft,1);
    const auto protected_merge=merge(std::vector<Link>{protected_link},generated_fact,independent,protected_link.identity,"kept");
    check(protected_merge.link.origin.state==State::committed && protected_merge.link.destination.endpoint==independent.endpoint);
    check(clear_workspace(protected_link,2).destination.endpoint==protected_link.destination.endpoint && clear_workspace(protected_link,2).destination.state==State::draft);
    const auto committed_dest=commit(accept_unmatched(draft,2),2);
    const auto committed_generated=fact(committed_dest.destination.endpoint,committed_dest.destination.amount,committed_dest.destination.date,State::committed,false,true);
    const auto retired=merge(std::vector<Link>{committed_dest},committed_generated,independent,committed_dest.identity,"kept");
    check(retired.retired_committed && retired.retired_committed->endpoint==committed_generated.endpoint && !retired.kept);
    check(retired.link.destination.endpoint==independent.endpoint && retired.link.destination.amount==committed_dest.destination.amount && retired.link.destination.date==independent.date);
    check(retired.link.origin.endpoint==committed_dest.origin.endpoint && retired.link.origin.amount==committed_dest.origin.amount && retired.link.origin.date==committed_dest.origin.date);
    auto evidenced=committed_dest;evidenced.destination.date=independent.date;evidenced.destination.provisional_date=false;evidenced.destination.independent_evidence=true;evidenced.destination.attached_evidence=BankEvidence{independent.endpoint,evidenced.destination.amount,independent.date};
    validate(evidenced);
    rejected(Error::unresolved_policy,[&]{merge(std::vector<Link>{evidenced},committed_generated,independent,evidenced.identity,"kept");});
    const auto origin=fact({1,{"checking",0},"source-0"},Money(-600,cad),"2026-10-26",State::committed,true);
    const auto other=fact({2,{"visa",0},"source-0"},Money(600,cad),"2026-10-27",State::committed,true);
    const auto both=merge({},origin,other,"both","kept");
    check(!both.kept && both.link.origin.state==State::committed && both.link.destination.state==State::committed);
    check(both.link.origin.amount==origin.amount && both.link.destination.amount==other.amount && both.link.origin.date==origin.date && both.link.destination.date==other.date);
    rejected(Error::amount_mismatch,[&]{merge({},origin,fact({2,{"visa",0},"source-1"},Money(599,cad),"2026-10-27",State::committed,true),"bad","kept");});
    rejected(Error::cross_currency,[&]{merge({},origin,fact({2,{"visa",0},"source-2"},Money(600,usd),"2026-10-27",State::committed,true),"fx","kept");});
    auto nameless=origin;nameless.endpoint.allocation.clear();
    rejected(Error::ambiguous_endpoint,[&]{merge({},nameless,other,"x","kept");});
    rejected(Error::origin_controls_draft,[&]{clear_workspace(draft,2);});
    const auto removed=clear_workspace(draft,1);
    check(removed.origin.state==State::removed && removed.destination.state==State::removed && !removed.unresolved);
    const auto origin_fact=fact(draft.origin.endpoint,draft.origin.amount,draft.origin.date,State::draft,true);
    const auto released=retarget(one,origin_fact,draft.identity,"kept",{},{});
    check(!released.kept && released.link.origin.state==State::removed && released.link.destination.state==State::removed);
    auto draft_link=both.link;draft_link.origin.state=State::draft;
    const auto unlinked=retarget(std::vector<Link>{draft_link},fact(draft_link.origin.endpoint,draft_link.origin.amount,draft_link.origin.date,State::draft,true),draft_link.identity,"kept",{},{});
    check(unlinked.link.unresolved && unlinked.link.origin.state==State::removed && unlinked.link.destination.state==State::committed);
    check(unlinked.link.destination.amount==draft_link.destination.amount && unlinked.link.destination.date==draft_link.destination.date && unlinked.link.destination.endpoint==draft_link.destination.endpoint);
    const auto rewritten=retarget(std::vector<Link>{both.link},origin,both.link.identity,"kept",{},{},"Fees");
    check(!rewritten.kept && rewritten.link.unresolved && rewritten.link.origin.state==State::removed && rewritten.link.origin_category.empty());
    check(rewritten.link.destination.state==State::committed && rewritten.link.destination.amount==both.link.destination.amount && rewritten.link.destination.date==both.link.destination.date && rewritten.link.destination.endpoint==both.link.destination.endpoint);
    auto dated=both.link;dated.origin.date="2026-10-27";dated.origin_date_corrected=true;validate(dated);
    const auto dated_edit=edit_linked_allocation(std::vector<Link>{both.link},origin,false,true,origin.amount,"2026-10-27");
    check(dated_edit.fate==LinkFate::dated && dated_edit.link.origin.date=="2026-10-27" && dated_edit.link.origin_date_corrected && dated_edit.link.destination.date==both.link.destination.date && !dated_edit.link.unresolved);
    const auto released_edit=edit_linked_allocation(std::vector<Link>{both.link},origin,false,false,Money(-5000,cad),origin.date);
    check(released_edit.fate==LinkFate::released && released_edit.link.unresolved && released_edit.link.origin.state==State::removed && released_edit.link.destination.amount==both.link.destination.amount);
    const auto destination_edit=edit_linked_allocation(std::vector<Link>{both.link},other,true,false,other.amount,other.date);
    check(destination_edit.fate==LinkFate::released && destination_edit.link.destination_released && destination_edit.link.destination.state==State::removed && destination_edit.link.origin.state==State::committed && destination_edit.link.origin.amount==both.link.origin.amount);
    auto manual_draft=source();manual_draft.transaction=Transaction::manual(manual_draft.transaction.effective_total(),manual_draft.transaction.allocations());
    auto manual_link=commit(commit(accept_unmatched(accept_unmatched(generate("manual-release",{1,{"manual",1},"payment-a"},manual_draft,{2,{"generated-m",0},"counterpart"},cad,false),1),2),1),2);
    const auto manual_origin=fact(manual_link.origin.endpoint,manual_link.origin.amount,manual_link.origin.date,State::committed,false,false,true);
    const auto manual_category=retarget(std::vector<Link>{manual_link},manual_origin,manual_link.identity,"kept",{},{},"Fees");
    check(manual_category.link.unresolved && manual_category.link.origin_category=="Fees" && manual_category.link.origin.state==State::committed && manual_category.link.destination.state==State::committed && manual_category.link.destination.amount==manual_link.destination.amount);
    rejected(Error::invalid_relationship,[&]{retarget(std::vector<Link>{accept_unmatched(generate("manual-open",{1,{"manual",2},"payment-b"},manual_draft,{3,{"generated-n",0},"counterpart"},cad,false),1)},fact({1,{"manual",2},"payment-b"},Money(-600,cad),"2026-10-26",State::draft,false,false,true),"manual-open","kept",{},{},"Fees");});
    const Endpoint created{3,{"~transfer-moved-destination",0},"generated"};
    const auto moved=retarget(one,origin_fact,draft.identity,"kept",created,cad);
    check(!moved.kept && moved.link.destination.endpoint==created && moved.link.destination.amount==draft.destination.amount && moved.link.origin.amount==draft.origin.amount);
    const auto moved_committed=retarget(std::vector<Link>{committed_dest},origin_fact,committed_dest.identity,"kept-row",created,cad);
    check(moved_committed.kept && moved_committed.kept->unresolved && moved_committed.kept->destination.state==State::committed);
    check(moved_committed.kept->destination.amount==committed_dest.destination.amount && moved_committed.link.destination.endpoint==created);
    auto pending=committed_dest;
    pending.destination.proposed_evidence=BankEvidence{{2,{"bank-proposal",0},"source-0"},pending.destination.amount,"2026-10-28"};
    validate(pending);
    rejected(Error::unresolved_policy,[&]{retarget(std::vector<Link>{pending},origin_fact,pending.identity,"kept-proposal",{},{});});
    rejected(Error::unresolved_policy,[&]{retarget(std::vector<Link>{pending},origin_fact,pending.identity,"kept-proposal",created,cad);});
    auto preserved=draft;preserved.destination.amount=Money(600,usd);preserved.preserved_amounts=true;validate(preserved);
    rejected(Error::invalid_relationship,[&]{retarget(std::vector<Link>{preserved},origin_fact,preserved.identity,"kept",{},{});});
    const auto pair=merge({},fact({1,{"c",0},"source-0"},Money(-600,cad),"2026-10-26",State::draft,true),fact({2,{"v",0},"source-0"},Money(600,cad),"2026-10-28",State::draft,true),"pair","kept");
    const auto cleared=clear_workspace(pair.link,1);
    check(cleared.unresolved && cleared.origin.state==State::removed && cleared.destination.amount==Money(600,cad) && cleared.destination.date=="2026-10-28" && cleared.destination.state==State::draft);
}
}
int main(int argc,char** argv) {
    try {
        const std::string name=argc>1?argv[1]:"";
        if(name=="lifecycle")lifecycle();else if(name=="matching")matching();else if(name=="boundaries")boundaries();else if(name=="groups")groups();else if(name=="acceptance")acceptance();else if(name=="proposals")proposals();else if(name=="manual")manual();else if(name=="management")management();else return 2;
        std::cout<<"Transfer "<<name<<" passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
