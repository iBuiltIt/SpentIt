#include <spentit/transfer.hpp>
#include <algorithm>
#include <set>
#include <map>

namespace spentit::transfers {
namespace {
[[noreturn]] void invalid() { throw TransferError(Error::invalid_relationship,"Invalid transfer relationship"); }
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
    if(link.origin.amount.currency()!=link.destination.amount.currency())
        throw TransferError(Error::cross_currency,"Cross-currency transfers are unsupported");
    if(-link.origin.amount!=link.destination.amount)
        throw TransferError(Error::amount_mismatch,"Transfer amounts disagree; explicit resolution is required");
    if((!link.origin.independent_evidence && !link.origin.manually_entered) || link.origin.provisional_date) invalid();
    if(!link.origin.manually_entered && (link.origin.attached_evidence || link.origin.proposed_evidence))invalid();
    if(!link.origin.attached_evidence && link.origin.date!=link.generated_date)invalid();
    if(link.destination.manually_entered)invalid();
    if(link.origin.manually_entered && link.origin.independent_evidence!=link.origin.attached_evidence.has_value())invalid();
    if(link.destination.independent_evidence!=link.destination.attached_evidence.has_value())invalid();
    if((link.origin.state==State::removed)!=(link.destination.state==State::removed)) invalid();
    if(link.origin.state==State::removed && link.destination.independent_evidence) invalid();
    if(!link.destination.independent_evidence && (!link.destination.provisional_date || link.destination.date!=link.generated_date)) invalid();
}
Link generate(std::string id,const Endpoint& origin,const imports::DraftRow& draft,
              const Endpoint& destination,const Currency& currency) {
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
    const bool imported=draft.transaction.original_imported_amount().has_value();
    Link link{std::move(id),{origin,found->amount,draft.date,State::draft,imported,false,!imported},
        {destination,-found->amount,draft.date,State::draft,false,true},draft.date};
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
    if(!current.independent_evidence && !current.accepted_unmatched)
        throw TransferError(Error::acceptance_required,"Accept this entry as valid without a bank match before committing");
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).state=State::committed;
    validate(result);return result;
}
Link accept_unmatched(const Link& link,std::int64_t account) {
    validate(link);const auto& current=chosen(link,account);
    if(current.state==State::removed || current.independent_evidence || current.proposed_evidence)invalid();
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).accepted_unmatched=true;
    validate(result);return result;
}
Link propose_match(const Link& link,std::int64_t account,const BankEvidence& evidence) {
    validate(link);const auto& current=chosen(link,account);
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
    validate(link);identity(batch);const auto& current=chosen(link,account);
    if(!current.proposed_evidence || current.proposed_evidence->endpoint.entry.batch!=batch)invalid();
    auto result=link;
    (account==result.origin.endpoint.account?result.origin:result.destination).proposed_evidence.reset();
    validate(result);return result;
}
Link commit_import_match(const Link& link,std::int64_t account,const BankEvidence& expected) {
    validate(link);const auto& current=chosen(link,account);
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
    if(account==link.destination.endpoint.account)
        throw TransferError(Error::unresolved_policy,"Destination-only draft cancellation requires an explicit policy");
    if(link.destination.independent_evidence)
        throw TransferError(Error::unresolved_policy,"Removing an independently evidenced transfer requires an explicit policy");
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
            const auto& endpoint=side->endpoint;
            if(!endpoints.emplace(endpoint.account,endpoint.entry,endpoint.allocation).second)invalid();
            const auto metadata=std::tuple{endpoint.account,side->state,side->independent_evidence,side->manually_entered,side->accepted_unmatched,side->date};
            const auto [entry,inserted]=entries.emplace(endpoint.entry,metadata);
            if(!inserted && (entry->second!=metadata || (!side->independent_evidence && !side->manually_entered)))invalid();
            for(const auto* evidence:{&side->attached_evidence,&side->proposed_evidence})if(*evidence) {
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
} // namespace spentit::transfers
