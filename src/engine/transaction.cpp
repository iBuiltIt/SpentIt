#include <spentit/transaction.hpp>
#include <utility>
#include "exact_sum.hpp"

namespace spentit {
Transaction::Transaction(Money total, std::optional<Money> original, std::vector<Allocation> allocations)
    : total_(std::move(total)), original_(std::move(original)), allocations_(std::move(allocations)) {
    if (original_) total_.require_compatible(*original_);
    if (allocations_.empty())
        throw FinancialError(ErrorCode::empty_allocations, "At least one allocation is required");
    for (const auto& allocation : allocations_) total_.require_compatible(allocation.amount);

    std::vector<detail::Term> terms;
    terms.reserve(allocations_.size());
    for (const auto& allocation : allocations_) terms.push_back({allocation.amount.units()});
    const Money sum(detail::exact_sum(terms), total_.currency());
    if (sum != total_) throw FinancialError(ErrorCode::split_mismatch, "Allocations do not equal effective total");
}
Transaction Transaction::imported(Money original, std::vector<Allocation> allocations) {
    return Transaction(original, original, std::move(allocations));
}
Transaction Transaction::manual(Money total, std::vector<Allocation> allocations) {
    return Transaction(std::move(total), std::nullopt, std::move(allocations));
}
Transaction Transaction::corrected(Money total, std::vector<Allocation> allocations) const {
    total_.require_compatible(total);
    return Transaction(std::move(total), original_, std::move(allocations));
}
} // namespace spentit
