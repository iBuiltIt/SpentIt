#include <spentit/import_review.hpp>
#include "exact_sum.hpp"
#include <algorithm>
#include <set>

namespace spentit::imports {
namespace {
[[noreturn]] void invalid() { throw std::invalid_argument("Invalid import review operation"); }
void text(std::string_view value,bool embedded_breaks=false) {
    if (value.size()>qif::max_field || !qif::valid_utf8(value)) invalid();
    for (const char ch:value) { const auto c=static_cast<unsigned char>(ch); if ((c<32 && c!='\t' && !(embedded_breaks && c=='\r')) || c==127) invalid(); }
}
}
EvidenceKey evidence_key(const qif::Record& source) {
    if ((!source.transaction && !source.repairable) || !source.source_currency) invalid();
    return {source.date,source.source_currency->definition_id(),source.original_amount?std::optional(source.original_amount->units()):std::nullopt,source.payee,source.reference};
}
std::optional<ReimportKey> reimport_key(const qif::Record& source) {
    if(source.diagnostic || !source.source_currency || !source.original_amount ||
       source.date.empty() || source.payee.empty())return {};
    if(source.original_amount->currency()!=*source.source_currency)return {};
    return ReimportKey{source.date,source.source_currency->definition_id(),
        source.original_amount->units(),source.payee};
}
namespace {
bool ofx_family(const qif::Record& record) {
    return record.format=="ofx" || record.format=="qfx" || record.format=="qbo" || record.format=="aso";
}
}
std::string basis_label(MatchBasis basis) {
    switch(basis) {
    case MatchBasis::same_source_fields: return "same source fields";
    case MatchBasis::same_date_amount: return "same date and amount";
    case MatchBasis::same_amount_unknown_date: return "same original amount; source date unavailable";
    case MatchBasis::unknown_original_amount: return "original amount unavailable; review source evidence";
    case MatchBasis::same_fitid: return "same FITID, currency and original amount";
    case MatchBasis::uncertain_fitid: return "same FITID with uncertain amount or currency";
    case MatchBasis::uncertain_date: return "same original amount; dates differ across exports";
    }
    return "review source evidence";
}
std::optional<MatchBasis> candidate_basis(const qif::Record& a,const qif::Record& b) {
    const auto x=evidence_key(a),y=evidence_key(b);
    const auto fit_of=[](const qif::Record& record){return record.fitid.empty()?record.associated_fitid:record.fitid;};
    const auto fit_a_id=fit_of(a), fit_b_id=fit_of(b);
    const bool fit_a=!fit_a_id.empty(), fit_b=!fit_b_id.empty();
    // Different bank ids are legitimate repeats, even on the same day and amount.
    if(fit_a && fit_b && fit_a_id!=fit_b_id) return {};
    if(fit_a && fit_b) {
        if(x.definition!=y.definition || !x.original_units || !y.original_units || x.original_units!=y.original_units)
            return MatchBasis::uncertain_fitid;
        return MatchBasis::same_fitid;
    }
    if(x.definition!=y.definition) return {};
    if(!x.original_units || !y.original_units) {
        if(!x.date.empty() && !y.date.empty() && x.date!=y.date)return {};
        return MatchBasis::unknown_original_amount;
    }
    if(x.original_units!=y.original_units)return {};
    if(x.date.empty() || y.date.empty()) return MatchBasis::same_amount_unknown_date;
    if(x.date!=y.date) return (ofx_family(a) || ofx_family(b)) ? std::optional(MatchBasis::uncertain_date) : std::nullopt;
    return x==y ? MatchBasis::same_source_fields : MatchBasis::same_date_amount;
}
bool same_bank_transaction(const qif::Record& a,const qif::Record& b) {
    // A manually associated ID alone cannot supply the original date/type of
    // its bank occurrence. Compare that occurrence itself, not the QIF row.
    return ofx_family(a) && ofx_family(b) && !a.diagnostic && !b.diagnostic &&
        !a.fitid.empty() && a.fitid==b.fitid &&
        a.source_currency && b.source_currency && *a.source_currency==*b.source_currency &&
        a.original_amount && b.original_amount && *a.original_amount==*b.original_amount &&
        !a.date.empty() && a.date==b.date && !a.trntype.empty() && a.trntype==b.trntype;
}
void validate_draft(const qif::Record& source,const DraftRow& edit,bool reassigned) {
    if ((!source.transaction && !source.repairable) || !source.source_currency) invalid();
    if (edit.transaction.original_imported_amount()!=source.original_amount) invalid();
    if(!reassigned && edit.transaction.effective_total().currency()!=*source.source_currency)invalid();
    if(reassigned && edit.transaction.reassigned_currency()==false && edit.transaction.effective_total().currency()!=*source.source_currency)invalid();
    if (edit.date.size()!=10 || edit.date[4]!='-' || edit.date[7]!='-') invalid();
    if (qif::date(edit.date.substr(8,2)+"/"+edit.date.substr(5,2)+"/"+edit.date.substr(0,4),qif::DateOrder::dmy)!=edit.date) invalid();
    text(edit.payee,true); text(edit.memo,true);
    if (edit.transaction.allocations().size()>qif::max_allocations) invalid();
    // Source transfer intent cannot disappear through an ordinary category/split edit.
    for(std::size_t ordinal=0;ordinal<source.source_allocation_fields.size();++ordinal){const auto& original=source.source_allocation_fields[ordinal];if(!qif::transfer_destination(original.category))continue;
        const auto& allocations=edit.transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return a.identity=="source-"+std::to_string(ordinal);});
        if(found==allocations.end() || found->source_ordinal!=ordinal || found->category!=original.category)throw std::invalid_argument("Preserve imported transfer allocations; resolve their destinations in Transfers");
    }
    std::size_t size=edit.payee.size()+edit.memo.size();
    for (const auto& allocation:edit.transaction.allocations()) {
        text(allocation.category); text(allocation.memo,true);
        if (!qif::supported_category(allocation.category) && !qif::transfer_destination(allocation.category)) invalid();
        if(qif::transfer_destination(allocation.category)) {
            if(allocation.category_binding || allocation.category_rule)invalid();
            const auto found=std::find_if(source.source_allocation_fields.begin(),source.source_allocation_fields.end(),[&](const auto& a){return a.category==allocation.category;});
            if(found==source.source_allocation_fields.end() || !allocation.source_ordinal || *allocation.source_ordinal>=source.source_allocation_fields.size() || allocation.identity!="source-"+std::to_string(*allocation.source_ordinal) || source.source_allocation_fields[*allocation.source_ordinal].category!=allocation.category)invalid();
        }
        size+=allocation.category.size()+allocation.memo.size();
        if (size>qif::max_record) invalid();
    }
}
void validate_decisions(const std::vector<std::vector<Occurrence>>& candidates,std::span<const Decision> decisions,bool require_resolved) {
    if (candidates.size()!=decisions.size()) invalid();
    std::set<Occurrence> used;
    for (std::size_t i=0;i<decisions.size();++i) {
        const auto& d=decisions[i];
        switch (d.disposition) {
        case Disposition::unresolved: if (require_resolved || d.duplicate_of || candidates[i].empty()) invalid(); break;
        case Disposition::include: if (d.duplicate_of) invalid(); break;
        case Disposition::duplicate:
            if (!d.duplicate_of || std::find(candidates[i].begin(),candidates[i].end(),*d.duplicate_of)==candidates[i].end() ||
                !used.insert(*d.duplicate_of).second) invalid();
            break;
        default: invalid();
        }
    }
}
Money sum(const Currency& currency,std::span<const Money> amounts) {
    std::vector<detail::Term> terms; terms.reserve(amounts.size());
    const Money zero(0,currency);
    for (const auto& amount:amounts) { zero.require_compatible(amount); terms.push_back({amount.units(),false}); }
    return Money(detail::exact_sum(terms),currency);
}
} // namespace spentit::imports
