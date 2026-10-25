#pragma once
#include <spentit/split_editor.hpp>
#include <memory>
#include <string>
#include <vector>

namespace spentit::sheet {
// Synthetic, session-only use cases. No document or persistence API.
struct Row {
    int id;
    int account;
    std::string date, payee, note;
    Transaction transaction;
    bool draft;
};
using Rows = std::vector<Row>;
using Snapshot = std::shared_ptr<const Rows>;
struct View {
    std::vector<int> indices;
    std::vector<std::string> balances; // Indexed by original record, never visible position.
    std::string withdrawals, deposits, committed, projected;
};
struct Edit {
    int id;
    std::string payee, note, total, category, memo, allocation;
    bool mixed = false;
    bool addAllocation = false;
};
std::string decimal(const Money& money);
Snapshot synthetic(int per_account);
View query(const Rows&, int account, const std::string& literal, int column, bool descending);
Edit begin_edit(const Rows&, int id);
Snapshot save_edit(const Rows&, const Edit&);
const Row& by_id(const Rows&, int id);
} // namespace spentit::sheet
