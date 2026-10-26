#pragma once
#include <spentit/rule_preview.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace spentit::app {
struct RuleFieldDiff {
    RuleField field;
    std::string allocation; // Empty for transaction payee and memo.
    std::string before_text;
    std::string after_text;
    FieldOwnership before_owner;
    FieldOwnership after_owner;
    std::optional<RuleMark> before_mark;
    std::optional<RuleMark> after_mark;
    bool removed = false;  // Before mark present, after mark absent or different.
    bool assigned = false; // After owner is rule and the mark, owner, or text differs.
};
std::vector<RuleFieldDiff> rule_field_diffs(const imports::DraftRow& before,const RuleProposal& proposal);

struct RuleDefinitionImage {
    std::string identity;
    std::string label;
    bool enabled = true;
    rules::Scope scope = rules::Scope::transaction;
    rules::InputField input = rules::InputField::payee;
    std::string expression;
    rules::Actions actions;
};
struct RulePreviewRequest {
    std::string batch;
    std::int64_t batch_revision = -1;
    std::optional<std::vector<RuleDefinitionImage>> editor;
    bool replace_manual = false;
};
struct RulePreviewRecord {
    imports::Occurrence identity;
    imports::DraftRow before;
    RuleProposal proposal;
    std::vector<RuleFieldDiff> diffs;
};
struct RulePreviewResult {
    std::string batch;
    std::int64_t batch_revision = 0;
    std::int64_t ruleset_revision = 0;
    std::int64_t catalog_revision = 0;
    bool replace_manual = false;
    bool unsaved_rules = false;
    std::size_t evaluated = 0;
    std::size_t changed = 0;
    std::vector<RulePreviewRecord> records;
};
struct RuleApplyRequest {
    std::string batch;
    std::int64_t batch_revision = 0;
    std::int64_t ruleset_revision = 0;
    std::int64_t catalog_revision = 0;
    bool replace_manual = false;
};
struct RuleApplyReceipt {
    std::string operation;
    std::string batch;
    std::int64_t batch_before = 0;
    std::int64_t batch_after = 0;
    std::int64_t ruleset_revision = 0;
    std::int64_t catalog_revision = 0;
    bool replace_manual = false;
    std::size_t evaluated = 0;
    std::size_t changed = 0;
};
struct RuleSaveRequest {
    std::vector<RuleDefinitionImage> ordered;
};
struct RuleSaveReceipt {
    std::string operation;
    std::int64_t ruleset_before = 0;
    std::int64_t ruleset_after = 0;
    bool unchanged = false;
    std::vector<std::pair<std::string, std::int64_t>> versions;
};
struct RuleImage {
    std::int64_t revision = 0;
    std::vector<RuleDefinitionImage> ordered;
};
struct RuleTombstone {
    std::string identity;
    std::int64_t version = 0;
    std::string label;
    rules::Scope scope = rules::Scope::transaction;
    rules::InputField input = rules::InputField::payee;
    std::string expression;
    rules::Actions actions;
};
struct RuleMatch {
    std::vector<rules::Explanation> explanations;
    std::vector<std::string> winner_lines;
    std::string error;
};
// Quote a source field as one literal. A source that contains \E closes the
// quote, emits \\E, and reopens it. Empty source is an empty whole field.
inline std::string quoted_field_pattern(std::string_view text) {
    std::string pattern="^(?:";
    std::size_t index=0;
    while(index<text.size()) {
        pattern+="\\Q";
        const auto end=text.find("\\E",index);
        if(end==std::string_view::npos) {pattern.append(text.substr(index));pattern+="\\E";break;}
        pattern.append(text.substr(index,end-index));
        pattern+="\\E\\\\E";
        index=end+2;
    }
    pattern+=")$";
    return pattern;
}
}
