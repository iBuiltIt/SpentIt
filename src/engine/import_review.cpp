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
    if (source.diagnostic || !source.transaction || !source.transaction->original_imported_amount()) invalid();
    const auto& amount=*source.transaction->original_imported_amount();
    return {source.date,amount.currency().definition_id(),amount.units(),source.payee,source.reference};
}
std::optional<MatchBasis> candidate_basis(const qif::Record& a,const qif::Record& b) {
    const auto x=evidence_key(a),y=evidence_key(b);
    if(x.date!=y.date || x.definition!=y.definition || x.original_units!=y.original_units) return {};
    return x==y ? MatchBasis::same_source_fields : MatchBasis::same_date_amount;
}
void validate_draft(const qif::Record& source,const DraftRow& edit) {
    if (source.diagnostic || !source.transaction) invalid();
    if (edit.transaction.original_imported_amount()!=source.transaction->original_imported_amount()) invalid();
    source.transaction->effective_total().require_compatible(edit.transaction.effective_total());
    if (edit.date.size()!=10 || edit.date[4]!='-' || edit.date[7]!='-') invalid();
    if (qif::date(edit.date.substr(8,2)+"/"+edit.date.substr(5,2)+"/"+edit.date.substr(0,4),qif::DateOrder::dmy)!=edit.date) invalid();
    text(edit.payee); text(edit.memo);
    if (edit.transaction.allocations().size()>qif::max_allocations) invalid();
    std::size_t size=edit.payee.size()+edit.memo.size();
    for (const auto& allocation:edit.transaction.allocations()) {
        text(allocation.category); text(allocation.memo);
        if (!qif::supported_category(allocation.category)) invalid();
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
