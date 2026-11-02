#pragma once
#include <spentit/category_catalog.hpp>
#include <spentit/import_review.hpp>
#include <spentit/rules.hpp>

namespace spentit::app {
enum class RuleField { payee, memo, allocation_memo, allocation_category };
struct RuleProvenance {
    RuleField field;
    std::string allocation; // Empty for transaction fields; stable local ID otherwise.
    rules::Contribution contribution;
    bool operator==(const RuleProvenance&) const = default;
};
struct RuleInput {
    imports::Occurrence identity;
    qif::Record source;
    imports::DraftRow draft;
    std::vector<RuleProvenance> provenance;
    std::vector<std::string> transfer_allocations{};
};
struct RuleProposal {
    imports::Occurrence identity;
    imports::DraftRow after;
    std::vector<RuleProvenance> provenance;
    std::vector<rules::Explanation> explanations;
    rules::MatchSummary match{};
};
// Verified source/current snapshots and catalog enter this pure adapter. No
// repository write occurs; durable Apply must check their revisions again.
std::vector<RuleProposal> preview_rules(const rules::RuleSet&, const CategoryCatalog&,
                                      const std::vector<RuleInput>&,
                                      bool replace_manual=false,
                                      const std::function<bool()>& cancelled={},
                                      bool preserve_untouched=false);
}
