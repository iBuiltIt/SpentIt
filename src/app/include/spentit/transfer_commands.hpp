#pragma once
#include <spentit/transfer.hpp>

namespace spentit::app {
enum class TransferAction { manual, from_draft, accept, commit_local, propose_match, discard_match, clear_local, import_commit, import_discard, merge, retarget, relink, restore, review_undo, resize_generated, release_generated };
struct TransferRequest {
    TransferAction action;
    std::string link;
    std::int64_t expected_sequence=0;
    std::optional<imports::Occurrence> origin;
    std::string allocation;
    std::int64_t batch_revision=0;
    std::int64_t destination=0;
    std::optional<Money> amount;
    std::string date,payee,memo;
    std::optional<transfers::BankEvidence> evidence;
    // Encoded only for merge and retarget. mode 0 releases to a category; mode 1
    // generates or replaces a destination. Facts are the caller's complete request.
    std::string kept;
    int mode=0;
    std::string category_id,category_path,generated_currency;
    std::optional<transfers::Endpoint> generated;
    std::optional<transfers::AllocationFact> left,right;
    std::int64_t right_revision=0;
    // Identity-based category commands retain their selection revision for retry.
    std::int64_t category_generation=-1;
};
struct TransferJournalEntry {
    std::string operation,action,link;
    std::int64_t sequence=0,origin_account=0,destination_account=0;
    Money origin_amount,destination_amount;
    std::string origin_date,destination_date;
    bool unresolved=false;
};
struct TransferView {
    transfers::Link link;
    std::string payee,memo;
    std::int64_t created_sequence;
    // Derived source ownership for the selected account; not part of link history.
    bool source_matched=false;
};
struct TransferReceipt {
    std::string operation;
    std::int64_t account,sequence;
    Money before,change,after;
};
struct TransferPreview {
    Money before,change,after;
    std::string note;
};
} // namespace spentit::app
