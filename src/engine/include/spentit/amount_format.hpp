#pragma once
#include <spentit/money.hpp>
namespace spentit {
// Stable persisted style identifiers; disabled retains the chosen style.
struct AmountFormat {
    bool enabled = true;
    std::string style = "comma-dot";
    bool operator==(const AmountFormat&) const = default;
};
void validate_amount_format(const AmountFormat&);
AmountFormat suggested_amount_format(std::string_view currency);
std::string display_amount(const Money&, const AmountFormat&);
std::string normalize_amount_expression(std::string_view, const AmountFormat&);
Money evaluate_amount(std::string_view, Currency, const AmountFormat&);
}
