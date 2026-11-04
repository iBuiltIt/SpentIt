#pragma once
#include <spentit/import_review.hpp>
#include <optional>

namespace spentit::app {
struct LinkExclusion {
    std::string identity, field="payee", expression;
    std::int64_t account=0; // zero means dataset-wide
    bool regex=false, removed=false;
    bool operator==(const LinkExclusion&) const = default;
};
struct LinkExclusionPreview {
    std::int64_t generation=0;
    std::vector<std::string> comparisons;
};
enum class ReviewKind { same, link, correction };
struct ReviewTransaction {
    imports::Occurrence identity;
    std::int64_t account;
    std::string account_name;
    imports::DraftRow draft;
    bool committed=false;
    std::string note;
    // Presentation defaults relative to the question’s source; never confirm a link.
    std::string suggested_left_allocation={}, suggested_right_allocation={};
};
struct ReviewQuestion {
    std::string key;
    ReviewKind kind=ReviewKind::correction;
    std::int64_t account=0;
    std::string account_name, reason, batch;
    std::optional<std::size_t> ordinal;
    std::vector<ReviewTransaction> rows;
    std::string waiting_link={};
};
struct ReviewSheet {
    std::int64_t generation=0;
    std::vector<ReviewQuestion> questions;
    bool can_undo=false;
};
enum class ReviewAction { separate, merge, link, not_linked, accept_missing };
struct ReviewAnswer {
    std::int64_t generation=0;
    std::string question;
    ReviewAction action=ReviewAction::separate;
    std::vector<imports::Occurrence> selected;
    std::optional<imports::Occurrence> survivor;
    std::string left_allocation,right_allocation,remainder_category;
    bool split_difference=false;
};
struct ReviewReceipt {std::string operation;};
}
