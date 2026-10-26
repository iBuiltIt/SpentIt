#pragma once
#include <spentit/money.hpp>
#include <spentit/category.hpp>
#include <optional>
#include <vector>

namespace spentit {
struct Allocation {
    std::string category;
    std::string memo;
    Money amount;
    std::optional<CategoryBinding> category_binding={};
    std::string identity={};
    std::optional<std::size_t> source_ordinal={};
    FieldOwnership category_owner=FieldOwnership::untracked,memo_owner=FieldOwnership::untracked;
};

// Validated in-memory value, not a persisted transaction or an import evidence record.
class Transaction {
public:
    static Transaction imported(Money original, std::vector<Allocation> allocations);
    static Transaction corrected_import(Money original, Money total, std::vector<Allocation> allocations);
    static Transaction manual(Money total, std::vector<Allocation> allocations);
    Transaction corrected(Money total, std::vector<Allocation> allocations) const;
    const Money& effective_total() const noexcept { return total_; }
    const std::optional<Money>& original_imported_amount() const noexcept { return original_; }
    const std::vector<Allocation>& allocations() const noexcept { return allocations_; }
    bool has_corrected_imported_total() const { return original_ && *original_ != total_; }
private:
    Transaction(Money total, std::optional<Money> original, std::vector<Allocation> allocations);
    Money total_;
    std::optional<Money> original_;
    std::vector<Allocation> allocations_;
};
} // namespace spentit
