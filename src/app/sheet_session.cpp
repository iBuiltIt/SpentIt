#include <spentit/sheet_session.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <limits>
#include <numeric>

namespace spentit::sheet {
std::string decimal(const Money& money) {
    const auto units = money.units();
    const auto magnitude = units < 0 ? std::uint64_t(-(units + 1)) + 1 : std::uint64_t(units);
    auto digits = std::to_string(magnitude);
    const auto scale = money.currency().scale();
    if (scale) {
        if (digits.size() <= scale) digits.insert(0, scale + 1 - digits.size(), '0');
        digits.insert(digits.size() - scale, 1, '.');
    }
    return (units < 0 ? "-" : "") + digits;
}
static std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
Snapshot synthetic(int count) {
    if (count < 1 || count > std::numeric_limits<int>::max() / 2) throw std::invalid_argument("Fixture count exceeds integer identity range");
    auto rows = std::make_shared<Rows>();
    rows->reserve(static_cast<std::size_t>(count) * 2);
    const Currency cad("CAD");
    const char* names[] = {"Cedar Market", "Northline Transit", "Harbour Coffee", "Payday", "Maple Books", "City Utilities"};
    for (int account = 0; account < 2; ++account) {
        for (int i = 0; i < count; ++i) {
            const auto units = i % 6 == 3 ? 220000 : -(1200 + (i % 137) * 37);
            Money total(units, cad);
            std::vector<Allocation> splits{{units > 0 ? "Income" : "Everyday", "Synthetic remainder", total}};
            if (i % 7 == 0) {
                splits = {{"Everyday", "Synthetic remainder", Money(units + 500, cad)},
                          {"Transport", "Synthetic ticket", Money(-500, cad)}};
            }
            char date[16];
            const std::chrono::year_month_day day(std::chrono::sys_days(std::chrono::year(2006)/1/1) + std::chrono::days(i % 7300));
            std::snprintf(date, sizeof(date), "%04d-%02u-%02u", int(day.year()), unsigned(day.month()), unsigned(day.day()));
            rows->push_back({account * count + i + 1, account, date, names[i % 6],
                "Synthetic fixture — no bank source", Transaction::imported(total, std::move(splits)), i >= count - count / 10});
        }
    }
    return rows;
}
const Row& by_id(const Rows& rows, int id) {
    // Fixture IDs are contiguous and permanent, unrelated to view positions.
    if (id <= 0 || static_cast<std::size_t>(id) > rows.size() || rows[static_cast<std::size_t>(id - 1)].id != id)
        throw std::invalid_argument("Unknown transaction identity");
    return rows[static_cast<std::size_t>(id - 1)];
}
View query(const Rows& rows, int account, const std::string& literal, int column, bool descending) {
    if (column < 0 || column > 5) throw std::invalid_argument("Unknown sort column");
    if (literal.size() > 256) throw std::invalid_argument("Prototype filter is limited to 256 bytes");
    const Currency cad("CAD");
    Money committed(0, cad), projected(0, cad), withdrawal(0, cad), deposit(0, cad);
    View view;
    view.balances.resize(rows.size());
    std::vector<int> chronological;
    for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].account == account) chronological.push_back(static_cast<int>(i));
    std::sort(chronological.begin(), chronological.end(), [&](int a, int b) {
        const auto& x = rows[static_cast<std::size_t>(a)]; const auto& y = rows[static_cast<std::size_t>(b)];
        return x.date == y.date ? x.id < y.id : x.date < y.date;
    });
    std::vector<std::int64_t> balanceUnits(rows.size());
    const auto needle = lower(literal);
    for (int index : chronological) {
        const auto i = static_cast<std::size_t>(index); const auto& row = rows[i];
        const auto& amount = row.transaction.effective_total();
        projected = projected + amount;
        if (!row.draft) committed = committed + amount;
        view.balances[i] = decimal(projected); balanceUnits[i] = projected.units();
        std::string searchable = row.date + " " + row.payee + " " + row.note;
        for (const auto& allocation : row.transaction.allocations()) searchable += " " + allocation.category + " " + allocation.memo;
        if (lower(searchable).find(needle) == std::string::npos) continue;
        view.indices.push_back(index);
        if (amount.units() < 0) withdrawal = withdrawal - amount; else deposit = deposit + amount;
    }
    auto category = [](const Row& row) { return row.transaction.allocations().size() > 1 ? std::string("Split") : row.transaction.allocations()[0].category; };
    std::sort(view.indices.begin(), view.indices.end(), [&](int a, int b) {
        const auto x = static_cast<std::size_t>(a), y = static_cast<std::size_t>(b);
        const auto& left = rows[x]; const auto& right = rows[y];
        int comparison = 0;
        if (column < 3) {
            const auto l = column == 0 ? left.date : column == 1 ? left.payee : category(left);
            const auto r = column == 0 ? right.date : column == 1 ? right.payee : category(right);
            comparison = l.compare(r);
        } else {
            const auto l = column == 3 ? std::min<std::int64_t>(0, left.transaction.effective_total().units()) : column == 4 ? std::max<std::int64_t>(0, left.transaction.effective_total().units()) : balanceUnits[x];
            const auto r = column == 3 ? std::min<std::int64_t>(0, right.transaction.effective_total().units()) : column == 4 ? std::max<std::int64_t>(0, right.transaction.effective_total().units()) : balanceUnits[y];
            comparison = l < r ? -1 : l > r ? 1 : 0;
            if (column == 3) comparison = -comparison; // Displayed withdrawal magnitudes.
        }
        if (comparison == 0) return left.id < right.id;
        return descending ? comparison > 0 : comparison < 0;
    });
    view.withdrawals = decimal(withdrawal); view.deposits = decimal(deposit);
    view.committed = decimal(committed); view.projected = decimal(projected);
    return view;
}
Edit begin_edit(const Rows& rows, int id) {
    const auto& row = by_id(rows, id);
    const auto& allocations = row.transaction.allocations();
    const bool split = allocations.size() > 1;
    return {id, row.payee, row.note, decimal(row.transaction.effective_total()),
            split ? allocations[1].category : "Transport", split ? allocations[1].memo : "",
            split ? decimal(allocations[1].amount) : "0.00", SplitEditor(row.transaction, 0).mixed_signs_enabled(), split};
}
Snapshot save_edit(const Rows& rows, const Edit& edit) {
    if (edit.payee.empty() || edit.payee.size() > 200 || edit.note.size() > 2000 || edit.category.size() > 200 || edit.memo.size() > 500)
        throw std::invalid_argument("Payee is required; field length limit exceeded if applicable");
    const auto& original = by_id(rows, edit.id);
    auto editor = SplitEditor(original.transaction, 0);
    // Validate the combined field edit, not an intermediate total/split combination.
    editor = editor.allow_mixed_signs();
    const auto currency = original.transaction.effective_total().currency();
    editor = editor.correct_total(Money::evaluate(edit.total, currency));
    if (original.transaction.allocations().size() > 1 || edit.addAllocation) {
        const Allocation allocation{edit.category, edit.memo, Money::evaluate(edit.allocation, currency)};
        if (original.transaction.allocations().size() > 1) editor = editor.edit(1, allocation);
        else editor = editor.append(allocation);
    }
    if (!edit.mixed) {
        bool positive = false, negative = false;
        for (const auto& part : editor.transaction().allocations()) {
            positive |= part.amount.units() > 0; negative |= part.amount.units() < 0;
        }
        if (positive && negative) throw FinancialError(ErrorCode::mixed_sign_choice_required, "Explicitly allow mixed signs to save this allocation");
    }
    auto updated = std::make_shared<Rows>(rows);
    auto& row = (*updated)[static_cast<std::size_t>(edit.id - 1)];
    row.transaction = editor.transaction(); row.payee = edit.payee; row.note = edit.note;
    return updated;
}
} // namespace spentit::sheet
