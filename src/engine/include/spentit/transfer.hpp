#pragma once
#include <spentit/import_review.hpp>

namespace spentit::transfers {
// Allocation identity, never its position, identifies a transfer endpoint.
struct Endpoint {
    std::int64_t account;
    imports::Occurrence entry;
    std::string allocation;
    bool operator==(const Endpoint&) const = default;
};
enum class State { draft, committed, removed };
struct BankEvidence {
    Endpoint endpoint;
    Money amount;
    std::string posting_date;
    bool operator==(const BankEvidence&) const = default;
};
struct Side {
    Endpoint endpoint;
    Money amount;
    std::string date;
    State state=State::draft;
    bool independent_evidence=false;
    bool provisional_date=false;
    // Creation provenance survives later attachment of bank evidence.
    bool manually_entered=false;
    bool accepted_unmatched=false;
    std::optional<BankEvidence> attached_evidence={};
    std::optional<BankEvidence> proposed_evidence={};
};
struct Link {
    std::string identity;
    Side origin;
    Side destination;
    std::string generated_date;
    // Set only when a currency reassignment preserved each side's decimal amount.
    // New matches stay false and still reject a cross-currency link.
    bool preserved_amounts=false;
    // The origin no longer participates. A kept counterpart stays, with its amount.
    bool unresolved=false;
    // A committed correction changed the origin posting date. The counterpart date stays.
    bool origin_date_corrected=false;
    // The destination allocation was corrected or removed. The origin entry stays.
    bool destination_released=false;
    // A committed manual origin released to an ordinary category. Its amount stays.
    std::string origin_category;
};
// One named allocation. Identity is the endpoint, never its position in a row.
struct AllocationFact {
    Endpoint endpoint;
    Money amount;
    std::string date;
    State state=State::draft;
    bool imported=false;
    bool generated=false;
    bool manually_entered=false;
};
struct LinkEdit {
    Link link;
    std::optional<Link> kept;
    // A purely generated committed allocation that left the active ledger.
    std::optional<Side> retired_committed;
};
enum class LinkFate { unchanged, dated, released, removed_generated };
struct EntryLinkEdit {
    Link link;
    LinkFate fate=LinkFate::unchanged;
};
enum class Error { invalid_relationship, cross_currency, amount_mismatch,
                   unresolved_policy, acceptance_required, origin_controls_draft,
                   committed_reversal, ambiguous_endpoint };
class TransferError final : public std::runtime_error {
public:
    TransferError(Error code,const char* message):std::runtime_error(message),code_(code){}
    Error code() const noexcept { return code_; }
private:
    Error code_;
};
// These pure transitions return a complete proposal. Persistence must validate
// current revisions and write both sides and history in one storage transaction.
void validate(const Link&);
// Source provenance is explicit: a repaired import can lack an original amount.
Link generate(std::string identity,const Endpoint& origin,const imports::DraftRow&,
              const Endpoint& destination,const Currency& destination_currency,bool imported_origin=true);
bool protected_draft(const Link&,std::int64_t account);
Link commit(const Link&,std::int64_t account);
// Acceptance is a review decision, never a commit or a bank match.
// Adjust only a purely generated counterpart after an explicit allocation split.
Link release_generated(const Link&);
Link resize_generated(const Link&,const Money& origin_amount);
Link accept_unmatched(const Link&,std::int64_t account);
// A proposed attachment does not alter posting date, provenance, or commit
// state. Only commit_import_match publishes it; discard retains the old side.
Link propose_match(const Link&,std::int64_t account,const BankEvidence&);
Link discard_import_match(const Link&,std::int64_t account,std::string_view batch);
Link commit_import_match(const Link&,std::int64_t account,const BankEvidence& expected);
// Origin discard removes a purely generated uncommitted pair. A protected draft
// is left in place. Deleting only the generated destination is refused.
// An independently evidenced draft counterpart is unlinked and left unchanged.
Link clear_workspace(const Link&,std::int64_t account);
// Explicit Merge of two named allocations. A new link uses identity. An update
// keeps the existing link identity and may return a kept unresolved counterpart.
LinkEdit merge(std::span<const Link>,const AllocationFact&,const AllocationFact&,std::string identity,std::string kept_identity);
// Category release passes no generated endpoint. An account choice passes a new
// synthetic endpoint and that account's currency. This does not search for an
// existing independent row and does not auto-link one.
LinkEdit retarget(std::span<const Link>,const AllocationFact& origin,std::string identity,std::string kept_identity,std::optional<Endpoint> generated,std::optional<Currency> generated_currency,std::string_view category={});
// One linked allocation of a committed entry. keep_transfer is true only when
// that allocation's amount and category are unchanged. A date-only change keeps
// the link and does not change the other side. Removing the entry, changing the
// amount, or leaving the transfer category ends the link. A committed
// counterpart stays. A purely generated uncommitted counterpart is removed.
EntryLinkEdit edit_linked_allocation(std::span<const Link>,const AllocationFact& before,bool remove,bool keep_transfer,const Money& after_amount,std::string_view after_date);
// Pass the complete relationship set for the operation. A protected allocation
// preserves its entire source transaction, including sibling transfer links.
// Duplicate endpoints and inconsistent states of a shared entry are rejected.
void validate_set(std::span<const Link>);
std::vector<Link> commit_account(std::span<const Link>,std::int64_t account);
std::vector<Link> clear_account(std::span<const Link>,std::int64_t account);
std::vector<Link> accept_entry(std::span<const Link>,std::int64_t account,const imports::Occurrence&);
// Whole-import proposals preserve unrelated links and act only on explicitly
// proposed evidence from this account and batch. No candidate is auto-matched.
std::vector<Link> commit_import_matches(std::span<const Link>,std::int64_t account,std::string_view batch);
std::vector<Link> discard_import_matches(std::span<const Link>,std::int64_t account,std::string_view batch);
// Exact opposite amounts are necessary, never sufficient to confirm a match.
// Posting dates remain independent. This checks compatibility, not confidence.
void validate_match(const Link&,const Endpoint& candidate,const Money& amount,
                    std::string_view posting_date);
} // namespace spentit::transfers
