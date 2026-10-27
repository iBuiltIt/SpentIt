#include <spentit/import_review.hpp>
#include "exact_sum.hpp"
#include <algorithm>
#include <set>

namespace spentit::imports {
namespace {
[[noreturn]] void invalid() { throw std::invalid_argument("Invalid import review operation"); }
void text(std::string_view value) {
    if (value.size()>qif::max_field || !qif::valid_utf8(value)) invalid();
    for (const char ch:value) { const auto c=static_cast<unsigned char>(ch); if ((c<32 && c!='\t') || c==127) invalid(); }
}
}
EvidenceKey evidence_key(const qif::Record& source) {
    if ((!source.transaction && !source.repairable) || !source.source_currency) invalid();
    return {source.date,source.source_currency->definition_id(),source.original_amount?std::optional(source.original_amount->units()):std::nullopt,source.payee,source.reference};
}
std::optional<MatchBasis> candidate_basis(const qif::Record& a,const qif::Record& b) {
    const auto x=evidence_key(a),y=evidence_key(b);
    if(x.definition!=y.definition) return {};
    if(!x.original_units || !y.original_units) {
        if(!x.date.empty() && !y.date.empty() && x.date!=y.date)return {};
        return MatchBasis::unknown_original_amount;
    }
    if(x.original_units!=y.original_units)return {};
    if(x.date.empty() || y.date.empty()) return MatchBasis::same_amount_unknown_date;
    if(x.date!=y.date) return {};
    return x==y ? MatchBasis::same_source_fields : MatchBasis::same_date_amount;
}
void validate_draft(const qif::Record& source,const DraftRow& edit) {
    if ((!source.transaction && !source.repairable) || !source.source_currency) invalid();
    if (edit.transaction.original_imported_amount()!=source.original_amount) invalid();
    if(edit.transaction.effective_total().currency()!=*source.source_currency)invalid();
    if (edit.date.size()!=10 || edit.date[4]!='-' || edit.date[7]!='-') invalid();
    if (qif::date(edit.date.substr(8,2)+"/"+edit.date.substr(5,2)+"/"+edit.date.substr(0,4),qif::DateOrder::dmy)!=edit.date) invalid();
    text(edit.payee); text(edit.memo);
    if (edit.transaction.allocations().size()>qif::max_allocations) invalid();
    // Source transfer intent cannot disappear through an ordinary category/split edit.
    for(std::size_t ordinal=0;ordinal<source.source_allocation_fields.size();++ordinal){const auto& original=source.source_allocation_fields[ordinal];if(!qif::transfer_destination(original.category))continue;
        const auto& allocations=edit.transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& a){return a.identity=="source-"+std::to_string(ordinal);});
        if(found==allocations.end() || found->source_ordinal!=ordinal || found->category!=original.category)throw std::invalid_argument("Preserve imported transfer allocations; resolve their destinations in Transfers");
    }
    std::size_t size=edit.payee.size()+edit.memo.size();
    for (const auto& allocation:edit.transaction.allocations()) {
        text(allocation.category); text(allocation.memo);
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
