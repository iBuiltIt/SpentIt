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

// Presentation of one saved action. Dates are ISO. Money keeps its definition
// so the screen can apply the file's amount format. This is not a new journal.
enum class HistoryFieldKind { text, date, money };
enum class HistoryBalanceKind { none, delta, absolute, rescale };
struct HistoryMoney {
    std::int64_t units = 0;
    std::string definition;
};
struct HistoryFieldChange {
    std::string field;
    HistoryFieldKind kind = HistoryFieldKind::text;
    bool named_only = false;
    std::string before_text, after_text;
    HistoryMoney before_money, after_money;
};
struct HistorySplitView {
    std::string category, memo;
    HistoryMoney amount;
};
struct HistoryMemberView {
    std::string batch;
    std::size_t ordinal = 0;
    std::string date, payee, memo, state;
    HistoryMoney amount;
    bool in_ledger = false;
    std::vector<HistorySplitView> splits;
    std::vector<HistoryFieldChange> changes;
};
struct HistoryBalance {
    HistoryBalanceKind kind = HistoryBalanceKind::none;
    HistoryMoney before, change, after;
};
struct HistoryActionView {
    std::string id;
    std::int64_t sequence = 0;
    std::string label;
    std::int64_t count = 0;
    std::vector<std::string> accounts;
    HistoryBalance balance;
    std::vector<HistoryFieldChange> changes;
    std::vector<HistoryMemberView> members;
    bool lists_transactions = false;
    ActionKind kind = ActionKind::transaction;
};
struct AccountHistoryPage {
    std::vector<HistoryActionView> actions;
    bool more = false;
    std::string focus;
    std::string notice;
};

// One row of the Change history Undo dialog. The text is read from the action
// already stored with that change. It is not a second journal.
struct ConnectedUndoLine {
    std::string id;
    std::string payload;
    std::string label;
    std::int64_t count = 0;
    ActionKind kind = ActionKind::transaction;
    std::vector<HistoryFieldChange> changes;
};
struct ConnectedUndoPreview {
    ActionToken token;
    std::string target;
    std::vector<ConnectedUndoLine> lines;
    std::string refusal;
    std::string label;
    std::int64_t count = 0;
    ActionKind kind = ActionKind::transaction;
    std::vector<std::string> accounts;
    std::vector<HistoryFieldChange> changes;
};

} // namespace spentit::app
