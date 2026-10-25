#pragma once
#include <spentit/qif.hpp>
#include <span>
#include <tuple>

namespace spentit::imports {
struct Occurrence {
    std::string batch;
    std::size_t ordinal;
    bool operator==(const Occurrence&) const = default;
    auto operator<=>(const Occurrence&) const = default;
};
struct EvidenceKey {
    std::string date, definition;
    std::int64_t original_units;
    std::string payee, reference;
    auto operator<=>(const EvidenceKey&) const = default;
};
EvidenceKey evidence_key(const qif::Record&);
enum class MatchBasis { same_source_fields, same_date_amount };
// A candidate is evidence for review, never permission for automatic exclusion.
std::optional<MatchBasis> candidate_basis(const qif::Record&,const qif::Record&);
enum class Disposition { unresolved, include, duplicate };
struct Decision {
    Disposition disposition = Disposition::include;
    std::optional<Occurrence> duplicate_of;
    bool operator==(const Decision&) const = default;
};
struct DraftRow {
    std::string date, payee, memo;
    Transaction transaction;
};
void validate_draft(const qif::Record& source, const DraftRow& edited);
// Explicitly selecting Include is always allowed. Duplicate choices must refer to
// a supplied candidate, with no reuse of the same occurrence within this batch.
void validate_decisions(const std::vector<std::vector<Occurrence>>& candidates,
                        std::span<const Decision> decisions, bool require_resolved);
Money sum(const Currency&, std::span<const Money> amounts);
} // namespace spentit::imports
