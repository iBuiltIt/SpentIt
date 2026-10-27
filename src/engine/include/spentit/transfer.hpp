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
};
enum class Error { invalid_relationship, cross_currency, amount_mismatch,
                   unresolved_policy, acceptance_required };
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
Link generate(std::string identity,const Endpoint& origin,const imports::DraftRow&,
              const Endpoint& destination,const Currency& destination_currency);
bool protected_draft(const Link&,std::int64_t account);
Link commit(const Link&,std::int64_t account);
// Acceptance is a review decision, never a commit or a bank match.
Link accept_unmatched(const Link&,std::int64_t account);
// A proposed attachment does not alter posting date, provenance, or commit
// state. Only commit_import_match publishes it; discard retains the old side.
Link propose_match(const Link&,std::int64_t account,const BankEvidence&);
Link discard_import_match(const Link&,std::int64_t account,std::string_view batch);
Link commit_import_match(const Link&,std::int64_t account,const BankEvidence& expected);
// Origin discard removes a purely generated pair only while both are drafts.
// Protected drafts survive workspace clearing. Destination-only cancellation
// and independently evidenced removal await an explicit product policy.
Link clear_workspace(const Link&,std::int64_t account);
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
