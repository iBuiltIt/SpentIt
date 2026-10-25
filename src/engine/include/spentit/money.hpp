#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <span>

namespace spentit {
enum class ErrorCode { invalid_currency, unsupported_scale, invalid_decimal,
                       precision, currency_mismatch, scale_mismatch, overflow,
                       empty_allocations, split_mismatch, unsupported_currency,
                       invalid_allocation_index, remainder_edit, mixed_sign_choice_required,
                       unresolved_currency_definition, currency_definition_mismatch };

class FinancialError : public std::runtime_error {
public:
    FinancialError(ErrorCode code, const char* message) : std::runtime_error(message), code_(code) {}
    ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};

struct CurrencyDefinition { std::string_view code; unsigned scale; };
std::span<const CurrencyDefinition> supported_currencies() noexcept;

// Retained, reviewed definitions are independent of new-account eligibility.
struct AmountCurrencyDefinition {
    std::string_view id;
    std::string_view code;
    unsigned scale;
};
std::span<const AmountCurrencyDefinition> amount_currency_definitions() noexcept;

// Constructors select the current catalog. Historical amounts require an explicit
// retained definition, never a guessed scale or a code-only historical lookup.
class Currency {
public:
    explicit Currency(std::string code);
    Currency(std::string code, unsigned scale);
    static Currency from_definition(std::string_view id);
    const std::string& code() const noexcept { return code_; }
    unsigned scale() const noexcept { return scale_; }
    const std::string& definition_id() const noexcept { return definition_id_; }
    bool operator==(const Currency&) const = default;
private:
    explicit Currency(const AmountCurrencyDefinition& definition);
    std::string code_;
    unsigned scale_;
    std::string definition_id_;
};

class Money {
public:
    Money(std::int64_t units, Currency currency) : units_(units), currency_(std::move(currency)) {}
    static Money parse(std::string_view decimal, Currency currency);
    std::int64_t units() const noexcept { return units_; }
    const Currency& currency() const noexcept { return currency_; }
    void require_compatible(const Money& other) const;
    Money operator+(const Money& other) const;
    Money operator-(const Money& other) const;
    Money operator-() const;
    bool operator==(const Money&) const = default;
private:
    std::int64_t units_;
    Currency currency_;
};
} // namespace spentit
