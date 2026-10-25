#include <spentit/money.hpp>
#include <limits>
#include <utility>

namespace spentit {
namespace {
constexpr auto low = std::numeric_limits<std::int64_t>::min();
constexpr auto high = std::numeric_limits<std::int64_t>::max();
[[noreturn]] void overflow() { throw FinancialError(ErrorCode::overflow, "Money overflow"); }
constexpr CurrencyDefinition catalog[] = {
#include "currency_catalog.inc"
};
constexpr AmountCurrencyDefinition definitions[] = {
#include "amount_currency_definitions.inc"
};
struct Selection { std::string_view code; std::string_view id; };
constexpr Selection selections[] = {
#include "currency_selection.inc"
};
std::string current_definition(std::string_view code) {
    for (const auto& selection : selections)
        if (selection.code == code) return std::string(selection.id);
    throw FinancialError(ErrorCode::unresolved_currency_definition, "Current currency definition is unresolved");
}
unsigned currency_scale(std::string_view code) {
    if (code.size() != 3 || code.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string_view::npos)
        throw FinancialError(ErrorCode::invalid_currency, "Currency code requires three uppercase ASCII letters");
    for (const auto& entry : catalog) if (entry.code == code) return entry.scale;
    throw FinancialError(ErrorCode::unsupported_currency, "Currency is not in the supported catalog");
}
}

std::span<const CurrencyDefinition> supported_currencies() noexcept { return catalog; }
std::span<const AmountCurrencyDefinition> amount_currency_definitions() noexcept { return definitions; }
Currency::Currency(const AmountCurrencyDefinition& definition)
    : code_(definition.code), scale_(definition.scale), definition_id_(definition.id) {}
Currency Currency::from_definition(std::string_view id) {
    for (const auto& definition : definitions)
        if (definition.id == id) return Currency(definition);
    throw FinancialError(ErrorCode::unresolved_currency_definition, "Unknown or unresolved currency definition");
}
Currency::Currency(std::string code)
    : code_(std::move(code)), scale_(currency_scale(code_)), definition_id_(current_definition(code_)) {}
Currency::Currency(std::string code, unsigned scale) : code_(std::move(code)), scale_(scale) {
    if (code_.size() != 3 || code_.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
        throw FinancialError(ErrorCode::invalid_currency, "Currency code requires three uppercase ASCII letters");
    if (scale_ > 9)
        throw FinancialError(ErrorCode::unsupported_scale, "Supported scales are 0 through 9");
    if (scale_ != currency_scale(code_))
        throw FinancialError(ErrorCode::scale_mismatch, "Scale differs from supported currency definition");
    definition_id_ = current_definition(code_);
}

Money Money::parse(std::string_view text, Currency currency) {
    // Strict ASCII decimal grammar: [+-]?[0-9]+(.[0-9]+)?; no exponent or locale inference.
    bool negative = false;
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    const auto dot = text.find('.');
    const auto whole = text.substr(0, dot);
    const auto fraction = dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);
    const auto digits = [](std::string_view s) { return s.find_first_not_of("0123456789") == std::string_view::npos; };
    if (whole.empty() || !digits(whole) || !digits(fraction) || (dot != std::string_view::npos && fraction.empty()))
        throw FinancialError(ErrorCode::invalid_decimal, "Invalid decimal syntax");
    if (fraction.size() > currency.scale())
        throw FinancialError(ErrorCode::precision, "Decimal exceeds currency scale");
    // Accumulate negative values to represent INT64_MIN without negating it.
    std::int64_t value = 0;
    const auto append = [&](int digit) {
        if (negative) {
            if (value < (low + digit) / 10) overflow();
            value = value * 10 - digit;
        } else {
            if (value > (high - digit) / 10) overflow();
            value = value * 10 + digit;
        }
    };
    for (char c : whole) append(c - '0');
    for (char c : fraction) append(c - '0');
    for (auto i = fraction.size(); i < currency.scale(); ++i) append(0);
    return Money(value, std::move(currency));
}

void Money::require_compatible(const Money& other) const {
    if (currency_.code() != other.currency_.code())
        throw FinancialError(ErrorCode::currency_mismatch, "Currency mismatch");
    if (currency_.scale() != other.currency_.scale())
        throw FinancialError(ErrorCode::scale_mismatch, "Currency scale mismatch");
    if (currency_.definition_id() != other.currency_.definition_id())
        throw FinancialError(ErrorCode::currency_definition_mismatch, "Currency definitions differ");
}
Money Money::operator+(const Money& other) const {
    require_compatible(other);
    const auto b = other.units_;
    if ((b > 0 && units_ > high - b) || (b < 0 && units_ < low - b)) overflow();
    return Money(units_ + b, currency_);
}
Money Money::operator-(const Money& other) const {
    require_compatible(other);
    const auto b = other.units_;
    if ((b > 0 && units_ < low + b) || (b < 0 && units_ > high + b)) overflow();
    return Money(units_ - b, currency_);
}
Money Money::operator-() const {
    if (units_ == low) overflow();
    return Money(-units_, currency_);
}
} // namespace spentit
