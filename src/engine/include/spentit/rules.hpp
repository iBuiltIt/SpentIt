#pragma once
#include <spentit/rule_regex.hpp>
#include <cstdint>
#include <set>

namespace spentit::rules {
enum class Scope { transaction, split };
enum class InputField { payee, memo, category };
enum class Ownership { source, manual, rule };
struct Contribution { std::string rule; std::int64_t version; bool operator==(const Contribution&) const = default; };
template<class T> struct Field {
    T value;
    Ownership ownership=Ownership::source;
    std::optional<Contribution> contribution=std::nullopt;
    bool operator==(const Field&) const = default;
};
struct CategoryValue {
    std::string identity; // Empty: unmapped text or Uncategorized.
    std::string unmapped; // Empty with no identity: Uncategorized.
    bool operator==(const CategoryValue&) const = default;
};
struct Actions {
    std::optional<std::string> payee_template, memo_template, category;
    bool inherit_parent=false;
};
struct Rule {
    std::string identity;
    std::int64_t version=0;
    bool enabled=true;
    Scope scope=Scope::transaction;
    InputField input=InputField::payee;
    std::string expression;
    Actions actions;
};
struct SourceSplit { std::string memo, category; };
struct Source {
    std::string payee,memo,category;
    bool split=false;
    // One element for an unsplit record, one per original allocation otherwise.
    std::vector<SourceSplit> allocations;
};
struct AllocationFields {
    // Persisted source association, never inferred from current vector position.
    std::optional<std::size_t> source_ordinal;
    Field<std::string> memo;
    Field<CategoryValue> category;
};
struct Record {
    Source source;
    Field<std::string> payee,memo;
    std::vector<AllocationFields> allocations;
};
enum class Effect { applied, compatible, competing, manual_protected, missing_parent };
struct Explanation {
    Contribution contribution;
    std::optional<std::size_t> allocation; // Current result position, display only.
    std::string field;
    Effect effect;
    // Compatible/competing assignments are shadowed by the earlier field winner.
};
struct Result { Record record; std::vector<Explanation> explanations; };
struct EvaluationLimits {
    static constexpr std::size_t rules=128, records=4096, allocations=4096;
    static constexpr std::size_t scope_allocations=16384;
    static constexpr std::size_t input_bytes=16U*1024U*1024U;
    static constexpr std::size_t comparisons=100000, callouts=1000000, explanations=100000;
};
class RuleSet {
public:
    explicit RuleSet(std::vector<Rule> ordered_rules);
    ~RuleSet();
    RuleSet(RuleSet&&) noexcept;
    RuleSet& operator=(RuleSet&&) noexcept;
    RuleSet(const RuleSet&)=delete;
    RuleSet& operator=(const RuleSet&)=delete;
    // Pure preview: no mutation of input, no money/date/structure fields exist in
    // this interface. Active leaf identities come from a verified catalog.
    std::vector<Result> evaluate(const std::vector<Record>& records,
                                const std::set<std::string>& active_categories,
                                bool replace_manual=false,
                                const std::function<bool()>& cancelled={}) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace spentit::rules
