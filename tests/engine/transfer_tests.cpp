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
    rejected(Error::unresolved_policy,[&]{clear_workspace(draft,2);});
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
    rejected(Error::unresolved_policy,[&]{clear_workspace(evidenced_draft,1);});
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
    rejected(Error::unresolved_policy,[&]{clear_account(links,2);});
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
    auto draft=source();draft.transaction=Transaction::manual(draft.transaction.effective_total(),draft.transaction.allocations());
    const auto link=generate("manual-link",{1,{"manual",0},"payment-a"},draft,{2,{"generated",0},"counterpart"},cad);
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
    auto other=generate("manual-b",{1,{"manual",0},"payment-b"},draft,{3,{"generated-b",0},"counterpart"},cad);
    validate_set(std::vector<Link>{link,other});
    rejected(Error::invalid_relationship,[&]{validate_set(std::vector<Link>{accept_unmatched(link,1),other});});
    const auto accepted_entry=accept_entry(std::vector<Link>{link,other},1,{"manual",0});
    check(accepted_entry[0].origin.accepted_unmatched && accepted_entry[1].origin.accepted_unmatched);
    check(commit_account(accepted_entry,1)[1].origin.state==State::committed);
    rejected(Error::invalid_relationship,[&]{accept_entry(std::vector<Link>{link,other},1,{"missing",0});});
}
}
int main(int argc,char** argv) {
    try {
        const std::string name=argc>1?argv[1]:"";
        if(name=="lifecycle")lifecycle();else if(name=="matching")matching();else if(name=="boundaries")boundaries();else if(name=="groups")groups();else if(name=="acceptance")acceptance();else if(name=="proposals")proposals();else if(name=="manual")manual();else return 2;
        std::cout<<"Transfer "<<name<<" passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
