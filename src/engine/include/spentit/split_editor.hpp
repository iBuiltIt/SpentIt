#pragma once
#include <spentit/transaction.hpp>

namespace spentit {
// An immutable editing session over a valid transaction. Indices refer to the
// current allocation order. The designated remainder cannot be directly edited
// or removed; designate another row first. No persistent history is implied.
class SplitEditor {
public:
    SplitEditor(Transaction transaction, std::size_t remainder);
    const Transaction& transaction() const noexcept { return transaction_; }
    std::size_t remainder_index() const noexcept { return remainder_; }
    bool mixed_signs_enabled() const noexcept { return mixed_; }
    SplitEditor allow_mixed_signs() const;
    SplitEditor designate_remainder(std::size_t index) const;
    SplitEditor edit(std::size_t index, Allocation allocation) const;
    SplitEditor append(Allocation allocation) const;
    SplitEditor remove(std::size_t index) const;
    SplitEditor correct_total(Money total) const;
private:
    SplitEditor recalculate(Money total, std::vector<Allocation> allocations, std::size_t remainder) const;
    void require_index(std::size_t index) const;
    Transaction transaction_;
    std::size_t remainder_;
    bool mixed_;
};
} // namespace spentit
