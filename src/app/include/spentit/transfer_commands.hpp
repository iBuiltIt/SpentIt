#pragma once
#include <spentit/transfer.hpp>

namespace spentit::app {
enum class TransferAction { manual, from_draft, accept, commit_local, propose_match, discard_match, clear_local, import_commit, import_discard };
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
};
struct TransferView {
    transfers::Link link;
    std::string payee,memo;
    std::int64_t created_sequence;
};
struct TransferReceipt {
    std::string operation;
    std::int64_t account,sequence;
    Money before,change,after;
};
} // namespace spentit::app
