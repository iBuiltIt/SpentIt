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
    std::optional<RuleMark> category_rule={}, memo_rule={};
};

// Validated in-memory value, not a persisted transaction or an import evidence record.
class Transaction {
public:
    static Transaction imported(Money original, std::vector<Allocation> allocations);
    static Transaction corrected_import(Money original, Money total, std::vector<Allocation> allocations);
    // reassigned means the effective currency differs from the source currency
    // under a recorded reassignment. Decimals are not required to stay equal
    // when an earlier correction changed the effective total.
    static Transaction repaired_import(std::optional<Money> original, Money total, std::vector<Allocation> allocations, bool reassigned=false);
    static Transaction manual(Money total, std::vector<Allocation> allocations);
    Transaction corrected(Money total, std::vector<Allocation> allocations) const;
    const Money& effective_total() const noexcept { return total_; }
    const std::optional<Money>& original_imported_amount() const noexcept { return original_; }
    const std::vector<Allocation>& allocations() const noexcept { return allocations_; }
    bool reassigned_currency() const noexcept { return reassigned_; }
    bool has_corrected_imported_total() const {
        if(!original_) return false;
        if(original_->currency()==total_.currency()) return *original_!=total_;
        return !original_->same_decimal(total_);
    }
private:
    Transaction(Money total, std::optional<Money> original, std::vector<Allocation> allocations, bool reassigned);
    Money total_;
    std::optional<Money> original_;
    std::vector<Allocation> allocations_;
    bool reassigned_=false;
};
} // namespace spentit
