#include <spentit/transfer.hpp>
#include <algorithm>
#include <set>
#include <map>
#include <utility>

namespace spentit::transfers {
namespace {
[[noreturn]] void invalid() { throw TransferError(Error::invalid_relationship,"Invalid transfer relationship"); }
void identity(std::string_view text);
[[noreturn]] void mismatch() { throw TransferError(Error::amount_mismatch,"Transfer amounts disagree; explicit resolution is required"); }
[[noreturn]] void currencies() { throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported"); }
void preserved(const Link& link) {
    if(link.preserved_amounts) throw TransferError(Error::invalid_relationship,"This link preserves amounts in two currencies. It is not a match and this command does not repair it.");
}
bool counts_locally(const Side& side) { return (side.manually_entered || !side.independent_evidence) && !side.attached_evidence; }
bool disposable(const Side& side) {
    return side.state==State::draft && !side.independent_evidence && !side.attached_evidence && !side.manually_entered;
}
bool committed_generated(const Side& side) {
    return side.state==State::committed && !side.independent_evidence && !side.manually_entered && !side.attached_evidence;
}
void retire_origin(Link& link) {
    link.origin.state=State::removed;link.origin.proposed_evidence.reset();link.origin.attached_evidence.reset();link.origin.accepted_unmatched=false;
    if(link.origin.manually_entered) link.origin.independent_evidence=false;
    link.origin.provisional_date=false;link.origin.date=link.generated_date;link.origin_date_corrected=false;
}
void fact_ok(const AllocationFact& fact);
Side direct_side(const AllocationFact& fact,bool origin_side) {
    if((!origin_side && fact.manually_entered) || fact.generated) invalid();
    Side side{fact.endpoint,fact.amount,fact.date,fact.state,fact.imported,false,origin_side && fact.manually_entered};
    if(!side.independent_evidence && !side.manually_entered) invalid();
    return side;
}
const Link* owner_of(std::span<const Link> links,const Endpoint& endpoint) {
    const Link* found=nullptr;
    for(const auto& link:links) {
        if(link.unresolved) continue;
        for(const auto* side:{&link.origin,&link.destination}) if(side->state!=State::removed && side->endpoint==endpoint) {
            if(found) invalid();
            found=&link;
        }
    }
    return found;
}
void agree(const Side& side,const AllocationFact& fact) {
    const bool generated=!side.independent_evidence && !side.manually_entered && !side.attached_evidence;
    if(side.endpoint!=fact.endpoint || side.amount!=fact.amount || side.date!=fact.date || side.state!=fact.state || side.manually_entered!=fact.manually_entered || side.independent_evidence!=fact.imported || generated!=fact.generated) invalid();
}
LinkEdit release_link(Link link) {
    preserved(link);
    if(link.unresolved || link.origin.state==State::removed || link.destination.state==State::removed) invalid();
    if(disposable(link.destination)) {
        link.origin.proposed_evidence.reset();link.destination.proposed_evidence.reset();
        link.origin.state=link.destination.state=State::removed;
        validate(link);return {std::move(link),std::nullopt,std::nullopt};
    }
    // Keeping this side would drop its proposal while its committed amount remains.
    if(link.destination.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    link.destination.proposed_evidence.reset();link.unresolved=true;retire_origin(link);validate(link);return {std::move(link),std::nullopt,std::nullopt};
}
Link leftover(const Link& link,std::string identity) {
    if(identity.empty() || identity==link.identity) invalid();
    if(link.destination.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    Link kept=link;kept.identity=std::move(identity);
    kept.destination.proposed_evidence.reset();kept.unresolved=true;retire_origin(kept);validate(kept);return kept;
}
LinkEdit replace_destination(Link link,const AllocationFact& next,std::string kept_identity) {
    preserved(link);
    if(link.unresolved || next.generated || next.manually_entered || next.endpoint.account==link.origin.endpoint.account || next.endpoint==link.destination.endpoint) invalid();
    if(next.amount.currency()!=link.origin.amount.currency() || next.amount.currency()!=link.destination.amount.currency()) currencies();
    if(-link.origin.amount!=next.amount || link.destination.amount!=next.amount) mismatch();
    if(link.destination.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    LinkEdit edit{link,std::nullopt,std::nullopt};
    if(committed_generated(link.destination)) edit.retired_committed=link.destination;
    else if(!disposable(link.destination)) edit.kept=leftover(link,std::move(kept_identity));
    link.destination.endpoint=next.endpoint;link.destination.date=next.date;link.destination.state=next.state;
    link.destination.independent_evidence=true;link.destination.provisional_date=false;link.destination.manually_entered=false;
    link.destination.accepted_unmatched=false;link.destination.attached_evidence.reset();link.destination.proposed_evidence.reset();
    link.destination.amount=next.amount;validate(link);edit.link=std::move(link);return edit;
}
void evidenced_generated(const Link* host,const AllocationFact& fact) {
    if(!host || !fact.generated) return;
    const auto& side=host->origin.endpoint==fact.endpoint?host->origin:host->destination;
    if(side.endpoint==fact.endpoint && (side.independent_evidence || side.attached_evidence))
        throw TransferError(Error::unresolved_policy,"A generated row that has its own import evidence is not removed.");
}
LinkEdit release_manual(Link link,std::string_view category) {
    preserved(link);
    if(link.unresolved || link.destination_released || link.origin.state!=State::committed || !link.origin.manually_entered) invalid();
    identity(category);
    if(link.destination.proposed_evidence || link.origin.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    if(link.destination.attached_evidence || link.origin.attached_evidence)
        throw TransferError(Error::unresolved_policy,"Replacing transfer evidence requires the repair policy");
    if(disposable(link.destination)) {link.destination.proposed_evidence.reset();link.destination.state=State::removed;}
    link.unresolved=true;link.origin_category=std::string(category);validate(link);return {std::move(link),std::nullopt,std::nullopt};
}
void identity(std::string_view text) {
    if(text.empty() || text.size()>qif::max_field || !qif::valid_utf8(text)) invalid();
    for(char ch:text) {const auto c=static_cast<unsigned char>(ch);if(c<32 || c==127) invalid();}
}
void endpoint(const Endpoint& value) {
    if(value.account<=0 || value.entry.ordinal>=qif::max_records) invalid();
    identity(value.entry.batch);identity(value.allocation);
}
void date(std::string_view value) {
    if(value.size()!=10 || value[4]!='-' || value[7]!='-') invalid();
    try {
        if(qif::date(std::string(value.substr(8,2))+"/"+std::string(value.substr(5,2))+"/"+std::string(value.substr(0,4)),qif::DateOrder::dmy)!=value) invalid();
    } catch(const std::invalid_argument&) { invalid(); }
}
void fact_ok(const AllocationFact& fact) {
    if(fact.endpoint.allocation.empty()) throw TransferError(Error::ambiguous_endpoint,"Choose the allocation explicitly. This transaction has more than one possible transfer endpoint.");
    endpoint(fact.endpoint);date(fact.date);
    if(fact.state!=State::draft && fact.state!=State::committed) invalid();
    if(fact.generated && (fact.imported || fact.manually_entered)) invalid();
    if(fact.imported && fact.manually_entered) invalid();
}
void side(const Side& value) {
    endpoint(value.endpoint);date(value.date);
    if(value.state!=State::draft && value.state!=State::committed && value.state!=State::removed) invalid();
    if(value.independent_evidence && value.provisional_date) invalid();
    if(value.manually_entered && value.provisional_date)invalid();
    for(const auto* evidence:{&value.attached_evidence,&value.proposed_evidence})if(*evidence) {
        endpoint((*evidence)->endpoint);date((*evidence)->posting_date);
        if((*evidence)->endpoint.account!=value.endpoint.account || (*evidence)->endpoint.entry==value.endpoint.entry ||
           (*evidence)->amount!=value.amount || value.state==State::removed)invalid();
    }
    if(value.attached_evidence && (!value.independent_evidence || value.date!=value.attached_evidence->posting_date))invalid();
    if(value.proposed_evidence && value.independent_evidence)invalid();
    if(value.state==State::committed && !value.independent_evidence && !value.accepted_unmatched)invalid();
}
const Side& chosen(const Link& link,std::int64_t account) {
    if(link.origin.endpoint.account==account)return link.origin;
    if(link.destination.endpoint.account==account)return link.destination;
    invalid();
}
const Side& opposite(const Link& link,std::int64_t account) {
    (void)chosen(link,account);
    return link.origin.endpoint.account==account?link.destination:link.origin;
}
}
void validate(const Link& link) {
    identity(link.identity);date(link.generated_date);side(link.origin);side(link.destination);
    if(link.origin.endpoint.account==link.destination.endpoint.account ||
       link.origin.endpoint.entry==link.destination.endpoint.entry) invalid();
    const bool same_currency=link.origin.amount.currency()==link.destination.amount.currency();
    const bool opposite=same_currency?-link.origin.amount==link.destination.amount:(-link.origin.amount).same_decimal(link.destination.amount);
    if(!opposite) throw TransferError(Error::amount_mismatch,"Transfer amounts disagree; explicit resolution is required");
    if(!same_currency && !link.preserved_amounts)
        throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported");
    if((!link.origin.independent_evidence && !link.origin.manually_entered) || link.origin.provisional_date) invalid();
    if(!link.origin.manually_entered && (link.origin.attached_evidence || link.origin.proposed_evidence))invalid();
    if(link.origin.attached_evidence) { if(link.origin_date_corrected) invalid(); }
    else if((link.origin.date!=link.generated_date)!=link.origin_date_corrected) invalid();
    if(link.destination.manually_entered)invalid();
    if(link.origin.manually_entered && link.origin.independent_evidence!=link.origin.attached_evidence.has_value())invalid();
    if(link.destination.attached_evidence && !link.destination.independent_evidence)invalid();
    if(link.destination_released) {
        if(link.unresolved || !link.origin_category.empty() || link.destination.state!=State::removed || link.origin.state==State::removed) invalid();
    } else if(!link.origin_category.empty()) {
        if(!link.unresolved || !link.origin.manually_entered || link.origin.state!=State::committed) invalid();
        if(link.destination.state==State::removed && link.destination.independent_evidence) invalid();
    } else if(link.unresolved) {
        if(link.origin.state!=State::removed) invalid();
    } else {
        if((link.origin.state==State::removed)!=(link.destination.state==State::removed)) invalid();
        if(link.origin.state==State::removed && link.destination.independent_evidence) invalid();
    }
    if(!link.destination.independent_evidence && (!link.destination.provisional_date || link.destination.date!=link.generated_date)) invalid();
}
Link generate(std::string id,const Endpoint& origin,const imports::DraftRow& draft,
              const Endpoint& destination,const Currency& currency,bool imported) {
    endpoint(origin);endpoint(destination);date(draft.date);
    if(draft.transaction.effective_total().currency()!=currency)
        throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported");
    const auto& allocations=draft.transaction.allocations();
    if(allocations.size()>qif::max_allocations)invalid();
    std::set<std::string> identities;
    for(const auto& allocation:allocations) {
        identity(allocation.identity);
        if(!identities.insert(allocation.identity).second)invalid();
    }
    const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return a.identity==origin.allocation;});
    if(found==allocations.end())invalid();
    Link link{std::move(id),{origin,found->amount,draft.date,State::draft,imported,false,!imported},
        {destination,-found->amount,draft.date,State::draft,false,true},draft.date,false,false,false,false,{}};
    validate(link);return link;
}
bool protected_draft(const Link& link,std::int64_t account) {
    validate(link);
    return chosen(link,account).state==State::draft && opposite(link,account).state==State::committed;
}
Link commit(const Link& link,std::int64_t account) {
    validate(link);
    if(chosen(link,account).state==State::removed)invalid();
    const auto& current=chosen(link,account);
    if(current.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    if(current.state==State::committed) return link;
    if(link.unresolved) invalid();
    if(!current.independent_evidence && !current.accepted_unmatched)
        throw TransferError(Error::acceptance_required,"Accept this entry as valid without a bank match before committing");
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).state=State::committed;
    validate(result);return result;
}
Link release_generated(const Link& link) {
    validate(link);
    if(link.unresolved || link.destination_released || !link.origin.independent_evidence || link.origin.state==State::removed || link.destination.state==State::removed || link.destination.independent_evidence || link.destination.manually_entered || link.destination.attached_evidence || link.destination.proposed_evidence)invalid();
    auto result=link;result.origin.state=State::removed;result.destination.state=State::removed;validate(result);return result;
}
Link resize_generated(const Link& link,const Money& amount) {
    validate(link);
    if(link.unresolved || link.destination_released || !link.origin.independent_evidence || link.origin.state==State::removed || link.destination.state==State::removed || link.destination.independent_evidence || link.destination.manually_entered || link.destination.attached_evidence || link.destination.proposed_evidence)invalid();
    if(amount.currency()!=link.origin.amount.currency() || amount.units()==0 || amount.units()==INT64_MIN || (amount.units()<0)!=(link.origin.amount.units()<0))invalid();
    auto result=link;result.origin.amount=amount;result.destination.amount=Money(-amount.units(),amount.currency());validate(result);return result;
}
Link accept_unmatched(const Link& link,std::int64_t account) {
    validate(link);if(link.unresolved)invalid();const auto& current=chosen(link,account);
    if(current.state==State::removed || current.independent_evidence || current.proposed_evidence)invalid();
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).accepted_unmatched=true;
    validate(result);return result;
}
Link propose_match(const Link& link,std::int64_t account,const BankEvidence& evidence) {
    validate(link);if(link.unresolved)invalid();const auto& current=chosen(link,account);
    endpoint(evidence.endpoint);date(evidence.posting_date);
    if(current.state==State::removed || evidence.endpoint.account!=account ||
       evidence.endpoint.entry==link.origin.endpoint.entry || evidence.endpoint.entry==link.destination.endpoint.entry)invalid();
    if(evidence.amount.currency()!=current.amount.currency())
        throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported");
    if(evidence.amount!=current.amount)
        throw TransferError(Error::amount_mismatch,"Transfer amounts disagree; explicit resolution is required");
    if(current.independent_evidence)
        throw TransferError(Error::unresolved_policy,"Replacing transfer evidence requires the repair policy");
    if(current.proposed_evidence && *current.proposed_evidence!=evidence)
        throw TransferError(Error::unresolved_policy,"Discard the previous match proposal before choosing another");
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).proposed_evidence=evidence;
    validate(result);return result;
}
Link discard_import_match(const Link& link,std::int64_t account,std::string_view batch) {
    validate(link);if(link.unresolved)invalid();identity(batch);const auto& current=chosen(link,account);
    if(!current.proposed_evidence || current.proposed_evidence->endpoint.entry.batch!=batch)invalid();
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).proposed_evidence.reset();
    validate(result);return result;
}
Link commit_import_match(const Link& link,std::int64_t account,const BankEvidence& expected) {
    validate(link);if(link.unresolved)invalid();const auto& current=chosen(link,account);
    if(!current.proposed_evidence || *current.proposed_evidence!=expected)invalid();
    auto result=link;auto& target=account==result.origin.endpoint.account?result.origin:result.destination;
    target.attached_evidence=target.proposed_evidence;target.proposed_evidence.reset();
    target.independent_evidence=true;target.provisional_date=false;target.date=expected.posting_date;
    target.state=State::committed;
    validate(result);return result;
}
Link clear_workspace(const Link& link,std::int64_t account) {
    validate(link);
    if(chosen(link,account).state!=State::draft || protected_draft(link,account))return link;
    if(link.unresolved && account==link.destination.endpoint.account && link.destination.independent_evidence && !link.destination.attached_evidence && !link.destination.proposed_evidence){auto result=link;result.destination.state=State::removed;validate(result);return result;}
    if(account==link.destination.endpoint.account)
        throw TransferError(Error::origin_controls_draft,"The origin controls this generated draft. Change or discard the origin instead of deleting the destination.");
    if(link.unresolved)return link;
    if(link.destination.independent_evidence){auto result=link;result.unresolved=true;retire_origin(result);validate(result);return result;}
    auto result=link;result.origin.state=result.destination.state=State::removed;
    result.origin.proposed_evidence.reset();result.destination.proposed_evidence.reset();
    validate(result);return result;
}
void validate_match(const Link& link,const Endpoint& candidate,const Money& amount,std::string_view posting_date) {
    validate(link);endpoint(candidate);date(posting_date);
    if(link.origin.state==State::removed || candidate.account!=link.destination.endpoint.account ||
       candidate.entry==link.origin.endpoint.entry || candidate.entry==link.destination.endpoint.entry)invalid();
    if(amount.currency()!=link.destination.amount.currency())
        throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported");
    if(amount!=link.destination.amount)
        throw TransferError(Error::amount_mismatch,"Transfer amounts disagree; explicit resolution is required");
    if(link.destination.independent_evidence)
        throw TransferError(Error::unresolved_policy,"Replacing transfer evidence requires the repair policy");
}
void validate_set(std::span<const Link> links) {
    if(links.size()>qif::max_records)invalid();
    std::set<std::string> ids;
    std::set<std::tuple<std::int64_t,imports::Occurrence,std::string>> endpoints;
    std::map<imports::Occurrence,std::tuple<std::int64_t,State,bool,bool,bool,std::string>> entries;
    std::set<std::tuple<std::int64_t,imports::Occurrence,std::string>> evidence_endpoints;
    std::map<std::int64_t,Currency> currencies;
    for(const auto& link:links) {
        validate(link);
        if(!ids.insert(link.identity).second)invalid();
        for(const auto* side:{&link.origin,&link.destination}) {
            if(side->state==State::removed || (link.unresolved && !counts_locally(*side))) continue;
            const auto& endpoint=side->endpoint;
            if(!endpoints.emplace(endpoint.account,endpoint.entry,endpoint.allocation).second)invalid();
            const auto metadata=std::tuple{endpoint.account,side->state,side->independent_evidence,side->manually_entered,side->accepted_unmatched,side->date};
            const auto [entry,inserted]=entries.emplace(endpoint.entry,metadata);
            if(!inserted && (entry->second!=metadata || (!side->independent_evidence && !side->manually_entered)))invalid();
            if(!link.unresolved) for(const auto* evidence:{&side->attached_evidence,&side->proposed_evidence})if(*evidence) {
                const auto& e=(*evidence)->endpoint;
                if(!evidence_endpoints.emplace(e.account,e.entry,e.allocation).second)invalid();
            }
            const auto [currency,new_account]=currencies.emplace(endpoint.account,side->amount.currency());
            if(!new_account && currency->second!=side->amount.currency())invalid();
        }
    }
    for(const auto& e:evidence_endpoints)if(endpoints.contains(e))invalid();
}
std::vector<Link> commit_account(std::span<const Link> links,std::int64_t account) {
    if(account<=0)invalid();validate_set(links);
    std::vector<Link> result;result.reserve(links.size());
    for(const auto& link:links) {
        const bool participates=link.origin.endpoint.account==account || link.destination.endpoint.account==account;
        result.push_back(participates?commit(link,account):link);
    }
    validate_set(result);return result;
}
std::vector<Link> clear_account(std::span<const Link> links,std::int64_t account) {
    if(account<=0)invalid();validate_set(links);
    std::set<imports::Occurrence> preserved;
    for(const auto& link:links) {
        if((link.origin.endpoint.account==account || link.destination.endpoint.account==account) && protected_draft(link,account))
            preserved.insert(chosen(link,account).endpoint.entry);
    }
    std::vector<Link> result;result.reserve(links.size());
    for(const auto& link:links) {
        const bool participates=link.origin.endpoint.account==account || link.destination.endpoint.account==account;
        result.push_back(participates && !preserved.contains(chosen(link,account).endpoint.entry)?clear_workspace(link,account):link);
    }
    validate_set(result);return result;
}
std::vector<Link> accept_entry(std::span<const Link> links,std::int64_t account,const imports::Occurrence& entry) {
    if(account<=0)invalid();validate_set(links);
    std::vector<Link> result;result.reserve(links.size());bool found=false;
    for(const auto& link:links) {
        const bool participates=link.origin.endpoint.account==account || link.destination.endpoint.account==account;
        const bool matches=participates && chosen(link,account).endpoint.entry==entry;
        result.push_back(matches?accept_unmatched(link,account):link);found=found || matches;
    }
    if(!found)invalid();validate_set(result);return result;
}
namespace {
std::vector<Link> import_matches(std::span<const Link> links,std::int64_t account,std::string_view batch,bool publish) {
    if(account<=0)invalid();identity(batch);validate_set(links);
    std::vector<Link> result;result.reserve(links.size());
    for(const auto& link:links) {
        const bool participates=link.origin.endpoint.account==account || link.destination.endpoint.account==account;
        if(participates) {
            const auto& proposal=chosen(link,account).proposed_evidence;
            if(proposal && proposal->endpoint.entry.batch==batch) {
                result.push_back(publish?commit_import_match(link,account,*proposal):discard_import_match(link,account,batch));
                continue;
            }
        }
        result.push_back(link);
    }
    validate_set(result);return result;
}
}
std::vector<Link> commit_import_matches(std::span<const Link> links,std::int64_t account,std::string_view batch) {
    return import_matches(links,account,batch,true);
}
std::vector<Link> discard_import_matches(std::span<const Link> links,std::int64_t account,std::string_view batch) {
    return import_matches(links,account,batch,false);
}
namespace {
std::vector<Link> edited(std::span<const Link> links,const LinkEdit& edit) {
    std::vector<Link> out;out.reserve(links.size()+1);
    for(const auto& link:links) {
        if(link.identity==edit.link.identity) continue;
        if(edit.kept && link.identity==edit.kept->identity) invalid();
        out.push_back(link);
    }
    out.push_back(edit.link);if(edit.kept) out.push_back(*edit.kept);validate_set(out);return out;
}
AllocationFact ordered_origin(AllocationFact left,AllocationFact right) {
    if(right.manually_entered && !left.manually_entered) std::swap(left,right);
    else if(!left.manually_entered && !right.manually_entered && right.endpoint.account<left.endpoint.account) std::swap(left,right);
    if(right.manually_entered || (!left.imported && !left.manually_entered) || !right.imported) invalid();
    return left;
}
}
LinkEdit merge(std::span<const Link> links,const AllocationFact& left,const AllocationFact& right,std::string identity,std::string kept_identity) {
    if(links.size()>qif::max_records) invalid();
    validate_set(links);fact_ok(left);fact_ok(right);
    if(left.endpoint==right.endpoint || identity.empty()) invalid();
    if(left.amount.currency()!=right.amount.currency()) currencies();
    const auto* left_link=owner_of(links,left.endpoint);const auto* right_link=owner_of(links,right.endpoint);
    if(left_link && right_link && left_link!=right_link) throw TransferError(Error::ambiguous_endpoint,"Choose one transfer relationship. These allocations belong to different links.");
    if(left_link && right_link) invalid();
    evidenced_generated(left_link,left);evidenced_generated(right_link,right);
    if(left_link) agree(left_link->origin.endpoint==left.endpoint?left_link->origin:left_link->destination,left);
    if(right_link) agree(right_link->origin.endpoint==right.endpoint?right_link->origin:right_link->destination,right);
    if(left.endpoint.account==right.endpoint.account) {
        const AllocationFact* generated=nullptr;const AllocationFact* imported=nullptr;const Link* host=nullptr;
        if(left.generated && right.imported) {generated=&left;imported=&right;host=left_link;}
        else if(right.generated && left.imported) {generated=&right;imported=&left;host=right_link;}
        else invalid();
        if(!host || host->identity!=identity || host->destination.endpoint!=generated->endpoint) invalid();
        if(host->destination.proposed_evidence)
            throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
        const bool retiring=committed_generated(host->destination);
        if(!retiring && !disposable(host->destination)) {
            if(host->destination.attached_evidence || host->destination.independent_evidence)
                throw TransferError(Error::unresolved_policy,"A generated row that has its own import evidence is not removed.");
            invalid();
        }
        if((generated==&left && right_link) || (generated==&right && left_link)) invalid();
        preserved(*host);
        if(host->destination.amount!=imported->amount) mismatch();
        LinkEdit edit{*host,std::nullopt,std::nullopt};
        if(retiring) edit.retired_committed=host->destination;
        edit.link.destination.endpoint=imported->endpoint;edit.link.destination.date=imported->date;edit.link.destination.state=imported->state;
        edit.link.destination.independent_evidence=true;edit.link.destination.provisional_date=false;edit.link.destination.manually_entered=false;edit.link.destination.accepted_unmatched=false;
        edit.link.destination.attached_evidence.reset();edit.link.destination.proposed_evidence.reset();validate(edit.link);edited(links,edit);return edit;
    }
    if(left.generated || right.generated || -left.amount!=right.amount) {
        if(left.generated || right.generated) invalid();
        mismatch();
    }
    if(!left_link && !right_link) {
        const auto origin=ordered_origin(left,right);const auto destination=origin.endpoint==left.endpoint?right:left;
        LinkEdit edit{Link{std::move(identity),direct_side(origin,true),direct_side(destination,false),origin.date,false,false,false,false,{}},std::nullopt,std::nullopt};
        validate(edit.link);edited(links,edit);return edit;
    }
    const auto* existing=left_link?left_link:right_link;const auto& other=left_link?right:left;const auto& on=left_link?left:right;
    if(existing->identity!=identity || existing->origin.endpoint!=on.endpoint) invalid();
    auto edit=replace_destination(*existing,other,std::move(kept_identity));edited(links,edit);return edit;
}
LinkEdit retarget(std::span<const Link> links,const AllocationFact& origin,std::string identity,std::string kept_identity,std::optional<Endpoint> generated,std::optional<Currency> generated_currency,std::string_view category) {
    if(links.size()>qif::max_records) invalid();
    validate_set(links);fact_ok(origin);
    if(origin.generated) throw TransferError(Error::origin_controls_draft,"The origin controls this generated draft. Change or discard the origin instead of deleting the destination.");
    const auto* host=owner_of(links,origin.endpoint);
    if(host && host->destination.endpoint==origin.endpoint) throw TransferError(Error::origin_controls_draft,"The origin controls this generated draft. Change or discard the origin instead of deleting the destination.");
    if(host && host->origin.endpoint!=origin.endpoint) invalid();
    if(host) {if(host->identity!=identity) invalid();agree(host->origin,origin);preserved(*host);}
    else if(identity.empty()) invalid();
    if(!generated) {
        if(!host) invalid();
        if(origin.manually_entered && origin.state!=State::committed)
            throw TransferError(Error::invalid_relationship,"Changing a manual transfer into a category is not this command. Discard the manual origin while it is uncommitted.");
        auto edit=origin.manually_entered?release_manual(*host,category):release_link(*host);
        edited(links,edit);return edit;
    }
    if(!generated_currency) currencies();
    endpoint(*generated);
    if(generated->account==origin.endpoint.account) invalid();
    if(origin.amount.currency()!=*generated_currency) currencies();
    if(!host) {
        const Allocation only{"","",origin.amount,{},origin.endpoint.allocation};
        const auto draft=origin.manually_entered
            ? imports::DraftRow{origin.date,"","",Transaction::manual(origin.amount,{only})}
            : imports::DraftRow{origin.date,"","",Transaction::imported(origin.amount,{only})};
        LinkEdit edit{generate(std::move(identity),origin.endpoint,draft,*generated,*generated_currency),std::nullopt,std::nullopt};
        if(edit.link.origin.state!=origin.state || edit.link.origin.amount!=origin.amount || edit.link.origin.date!=origin.date) invalid();
        edited(links,edit);return edit;
    }
    Side fresh{*generated,-origin.amount,host->generated_date,State::draft,false,true};
    LinkEdit edit{*host,std::nullopt,std::nullopt};
    if(!disposable(host->destination)) edit.kept=leftover(*host,std::move(kept_identity));
    edit.link=*host;edit.link.destination=fresh;validate(edit.link);edited(links,edit);return edit;
}
EntryLinkEdit edit_linked_allocation(std::span<const Link> links,const AllocationFact& before,bool remove,bool keep_transfer,const Money& after_amount,std::string_view after_date) {
    if(links.size()>qif::max_records) invalid();
    validate_set(links);fact_ok(before);date(after_date);
    if(after_amount.currency()!=before.amount.currency()) currencies();
    const auto* host=owner_of(links,before.endpoint);
    if(!host) invalid();
    const bool origin_side=host->origin.endpoint==before.endpoint;
    if(!origin_side && host->destination.endpoint!=before.endpoint) invalid();
    const auto& current=origin_side?host->origin:host->destination;
    agree(current,before);preserved(*host);
    if(host->origin.proposed_evidence || host->destination.proposed_evidence)
        throw TransferError(Error::unresolved_policy,"Commit or discard the pending import match explicitly");
    if(host->origin.attached_evidence || host->destination.attached_evidence)
        throw TransferError(Error::unresolved_policy,"Replacing transfer evidence requires the repair policy");
    const bool amount_same=after_amount==before.amount;
    const bool date_same=after_date==before.date;
    if(!remove && keep_transfer && amount_same && date_same) return {*host,LinkFate::unchanged};
    if(!remove && keep_transfer && amount_same) {
        Link link=*host;
        if(origin_side) {
            link.origin.date=std::string(after_date);
            link.origin_date_corrected=link.origin.date!=link.generated_date;
        } else {
            if(!link.destination.independent_evidence && after_date!=link.generated_date) invalid();
            link.destination.date=std::string(after_date);
        }
        validate(link);edited(links,{link,std::nullopt,std::nullopt});return {std::move(link),LinkFate::dated};
    }
    if(origin_side) {
        const bool generated_draft=disposable(host->destination);
        auto edit=release_link(*host);edited(links,edit);
        return {std::move(edit.link),generated_draft?LinkFate::removed_generated:LinkFate::released};
    }
    Link link=*host;
    if(link.unresolved || link.destination_released || link.destination.state==State::removed || link.origin.state==State::removed) invalid();
    link.destination.proposed_evidence.reset();link.destination.state=State::removed;link.destination_released=true;
    validate(link);edited(links,{link,std::nullopt,std::nullopt});
    return {std::move(link),LinkFate::released};
}
} // namespace spentit::transfers
