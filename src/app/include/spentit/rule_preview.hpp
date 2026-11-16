#pragma once
#include <spentit/category_catalog.hpp>
#include <spentit/import_review.hpp>
#include <spentit/rules.hpp>
#include <map>
#include <set>

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
// Immutable, validated catalog for a streamed preview. Each transaction still
// counts these bytes against its existing input limit and validates its fields.
class RulePreviewCatalog {
public:
    explicit RulePreviewCatalog(const CategoryCatalog&,const std::function<bool()>& cancelled={});
private:
    std::int64_t revision_;
    std::size_t bytes_=0;
    std::map<std::string,CategoryDefinition> definitions_;
    std::map<std::string,std::string> paths_;
    std::set<std::string> active_;
    friend std::vector<RuleProposal> preview_rules(const rules::RuleSet&,const RulePreviewCatalog&,const std::vector<RuleInput>&,bool,const std::function<bool()>&,bool);
};
std::vector<RuleProposal> preview_rules(const rules::RuleSet&,const RulePreviewCatalog&,
                                      const std::vector<RuleInput>&,bool replace_manual=false,
                                      const std::function<bool()>& cancelled={},bool preserve_untouched=false);
// Verified source/current snapshots and catalog enter this pure adapter. No
// repository write occurs; durable Apply must check their revisions again.
std::vector<RuleProposal> preview_rules(const rules::RuleSet&, const CategoryCatalog&,
                                      const std::vector<RuleInput>&,
                                      bool replace_manual=false,
                                      const std::function<bool()>& cancelled={},
                                      bool preserve_untouched=false);
}
