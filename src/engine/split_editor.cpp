#include <spentit/split_editor.hpp>
#include "exact_sum.hpp"
#include <utility>

namespace spentit {
namespace {
bool has_mixed_signs(const std::vector<Allocation>& allocations) {
    bool positive = false, negative = false;
    for (const auto& allocation : allocations) {
        positive |= allocation.amount.units() > 0;
        negative |= allocation.amount.units() < 0;
    }
    return positive && negative;
}
}
SplitEditor::SplitEditor(Transaction transaction, std::size_t remainder)
    : transaction_(std::move(transaction)), remainder_(remainder),
      mixed_(has_mixed_signs(transaction_.allocations())) { require_index(remainder); }

void SplitEditor::require_index(std::size_t index) const {
    if (index >= transaction_.allocations().size())
        throw FinancialError(ErrorCode::invalid_allocation_index, "Invalid allocation index");
}
SplitEditor SplitEditor::allow_mixed_signs() const {
    auto next = *this;
    next.mixed_ = true;
    return next;
}
SplitEditor SplitEditor::designate_remainder(std::size_t index) const {
    require_index(index);
    auto next = *this;
    next.remainder_ = index;
    return next;
}
SplitEditor SplitEditor::recalculate(Money total, std::vector<Allocation> allocations, std::size_t remainder) const {
    transaction_.effective_total().require_compatible(total);
    std::vector<detail::Term> terms{{total.units()}};
    for (std::size_t i = 0; i < allocations.size(); ++i) {
        total.require_compatible(allocations[i].amount);
        if (i != remainder) terms.push_back({allocations[i].amount.units(), true});
    }
    allocations[remainder].amount = Money(detail::exact_sum(terms), total.currency());
    if (!mixed_ && has_mixed_signs(allocations))
        throw FinancialError(ErrorCode::mixed_sign_choice_required, "Explicit mixed-sign choice required");
    auto next = *this;
    next.transaction_ = transaction_.corrected(std::move(total), std::move(allocations));
    next.remainder_ = remainder;
    return next;
}
SplitEditor SplitEditor::edit(std::size_t index, Allocation allocation) const {
    require_index(index);
    if (index == remainder_) throw FinancialError(ErrorCode::remainder_edit, "Designate another remainder before editing this row");
    auto allocations = transaction_.allocations();
    allocations[index] = std::move(allocation);
    return recalculate(transaction_.effective_total(), std::move(allocations), remainder_);
}
SplitEditor SplitEditor::append(Allocation allocation) const {
    auto allocations = transaction_.allocations();
    allocations.push_back(std::move(allocation));
    return recalculate(transaction_.effective_total(), std::move(allocations), remainder_);
}
SplitEditor SplitEditor::remove(std::size_t index) const {
    require_index(index);
    if (index == remainder_) throw FinancialError(ErrorCode::remainder_edit, "Designate another remainder before removing this row");
    auto allocations = transaction_.allocations();
    allocations.erase(allocations.begin() + static_cast<std::vector<Allocation>::difference_type>(index));
    return recalculate(transaction_.effective_total(), std::move(allocations), remainder_ - (index < remainder_ ? 1U : 0U));
}
SplitEditor SplitEditor::correct_total(Money total) const {
    return recalculate(std::move(total), transaction_.allocations(), remainder_);
}
} // namespace spentit
