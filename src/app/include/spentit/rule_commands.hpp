#pragma once
#include <spentit/rule_preview.hpp>

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
}
