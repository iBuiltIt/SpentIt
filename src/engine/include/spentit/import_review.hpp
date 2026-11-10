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
    std::optional<std::int64_t> original_units;
    std::string payee, reference;
    auto operator<=>(const EvidenceKey&) const = default;
};
EvidenceKey evidence_key(const qif::Record&);
// Exact original facts for ID-less re-import recognition. Account scoping,
// uniqueness on both sides, retained bank IDs and target liveness are caller obligations.
struct ReimportKey {
    std::string date, definition;
    std::int64_t units;
    std::string payee;
    auto operator<=>(const ReimportKey&) const = default;
};
std::optional<ReimportKey> reimport_key(const qif::Record&);
enum class MatchBasis { same_source_fields, same_date_amount, same_amount_unknown_date, unknown_original_amount,
    same_fitid, uncertain_fitid, uncertain_date };
// One format-independent policy on original facts. Ordinary amount candidates
// require the same known date; unknown facts remain reviewable. Shared original
// or retained bank IDs can flag contradictory facts for review, never exclusion.
enum class MatchContext { suggestion, confirmed_decision };
// confirmed_decision is only for validating an already recorded explicit choice.
std::optional<MatchBasis> candidate_basis(const qif::Record&,const qif::Record&,
                                        MatchContext = MatchContext::suggestion);
// Strict bank-ID evidence agreement. The caller must additionally establish
// the same resolved account, a unique live target and one-to-one occurrences.
// Never pass edited ledger fields in place of the original source records.
bool same_bank_transaction(const qif::Record&,const qif::Record&);
std::string basis_label(MatchBasis basis);
// Manual transfer suggestion policy; never an automatic association decision.
// Dates must be valid ISO calendar dates. Returns their absolute calendar distance.
int posting_distance(std::string_view left, std::string_view right);
// One calendar day earlier or later. Empty when the date is invalid or the
// result is outside year 0001–9999.
std::optional<std::string> shift_calendar_date(std::string_view date, int days);
// Same-account review only. Ordinary same-day matching is candidate_basis.
// This is the one-day window for a new row that missed that match: same
// currency and exact original amount, known dates exactly one day apart.
// Shared bank IDs stay on candidate_basis. Disjoint bank IDs and manual rows
// do not widen. Automatic recognition does not call this.
std::optional<MatchBasis> adjacent_day_basis(const qif::Record&, const qif::Record&);
bool link_amount_candidate(const Money& left, const Money& right,
                           bool explicit_outgoing_destination);
enum class Disposition { unresolved, include, duplicate };
struct Decision {
    Disposition disposition = Disposition::include;
    std::optional<Occurrence> duplicate_of;
    bool operator==(const Decision&) const = default;
};
struct DraftRow {
    std::string date, payee, memo;
    Transaction transaction;
    FieldOwnership payee_owner=FieldOwnership::untracked,memo_owner=FieldOwnership::untracked;
    std::optional<RuleMark> payee_rule={}, memo_rule={};
    bool manual_split_amounts=false; // Explicit balancing-split amount disables automatic edits.
    std::string remainder_allocation={}; // Stable allocation identity; empty until first editor save.
};
// reassigned is true when a recorded currency reassignment authorizes a
// different effective currency. The effective decimal need not equal the
// source decimal if that total was corrected before reassignment.
void validate_draft(const qif::Record& source, const DraftRow& edited, bool reassigned=false);
// Explicitly selecting Include is always allowed. Duplicate choices must refer to
// a supplied candidate, with no reuse of the same occurrence within this batch.
void validate_decisions(const std::vector<std::vector<Occurrence>>& candidates,
                        std::span<const Decision> decisions, bool require_resolved);
Money sum(const Currency&, std::span<const Money> amounts);
} // namespace spentit::imports
