#pragma once
#include <spentit/money.hpp>
#include <limits>
#include <vector>

namespace spentit::detail {
struct Term { std::int64_t units; bool subtract = false; };

// Include the total and subtracted allocations in one cancellation pass. Neither
// a subtotal nor negated INT64_MIN needs to fit in signed 64 bits independently.
inline std::int64_t exact_sum(const std::vector<Term>& terms) {
    const auto negative = [](Term t) { return t.units != 0 && ((t.units < 0) != t.subtract); };
    const auto magnitude = [](Term t) {
        const auto value = static_cast<std::uint64_t>(t.units);
        return t.units < 0 ? std::uint64_t{0} - value : value;
    };
    std::size_t positive = 0, negative_index = 0;
    std::uint64_t value = 0;
    bool sign = false;
    for (;;) {
        while (positive < terms.size() && negative(terms[positive])) ++positive;
        while (negative_index < terms.size() && !negative(terms[negative_index])) ++negative_index;
        if (positive == terms.size() && negative_index == terms.size()) break;
        const bool take_negative = negative_index < terms.size() && (!sign || positive == terms.size());
        const auto term = terms[take_negative ? negative_index++ : positive++];
        const auto amount = magnitude(term);
        if (sign == take_negative) {
            if (amount > std::numeric_limits<std::uint64_t>::max() - value)
                throw FinancialError(ErrorCode::overflow, "Allocation sum overflow");
            value += amount;
        } else if (value >= amount) {
            value -= amount;
        } else {
            value = amount - value;
            sign = take_negative;
        }
        if (value == 0) sign = false;
    }
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (value > maximum + (sign ? 1U : 0U))
        throw FinancialError(ErrorCode::overflow, "Allocation sum overflow");
    if (sign && value == maximum + 1) return std::numeric_limits<std::int64_t>::min();
    const auto result = static_cast<std::int64_t>(value);
    return sign ? -result : result;
}
} // namespace spentit::detail
