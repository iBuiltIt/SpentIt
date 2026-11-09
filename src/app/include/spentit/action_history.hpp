#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace spentit::app {

// A payload is a reference to its owning domain receipt, never a collection of
// SQL row images. Import members are determined by source ownership.
enum class ActionKind { transaction, review, rule_application, manual_creation,
                        opening, currency, acceptance, import };
enum class ActionState { preparing, applied, undone, cancelled };
enum class ImportStage { captured, materialized, awaiting_decision };

struct ActionToken {
    std::int64_t session = 0;
    std::int64_t epoch = 0;
    std::string head;
    bool operator==(const ActionToken&) const = default;
};

struct ActionSummary {
    std::string id;
    std::string parent;
    std::int64_t sequence = 0;
    ActionKind kind = ActionKind::transaction;
    ActionState state = ActionState::applied;
    std::string payload;
    std::string label;
    std::int64_t count = 0;
};

struct UndoPreview {
    ActionToken token;
    ActionSummary action;
    std::vector<std::int64_t> accounts;
    std::string refusal;
};

struct ActionReceipt {
    std::string operation;
    std::string action;
};

} // namespace spentit::app
