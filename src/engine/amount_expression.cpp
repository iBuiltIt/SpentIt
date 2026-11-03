#include <spentit/money.hpp>
#include <limits>
#include <numeric>

namespace spentit {
namespace {
using U = std::uint64_t;
constexpr U maximum = std::numeric_limits<U>::max();
[[noreturn]] void too_large() {
    throw FinancialError(ErrorCode::overflow, "This calculation is too large. Use smaller amounts.");
}
[[noreturn]] void syntax() {
    throw FinancialError(ErrorCode::invalid_decimal, "Enter an amount or a calculation using +, -, *, / and parentheses.");
}
U add(U a, U b) { if (a > maximum - b) too_large(); return a + b; }
U multiply(U a, U b) { if (b && a > maximum / b) too_large(); return a * b; }
// Reduced signed rational numbers keep intermediate fractions exact. All
// arithmetic is checked before execution; no floating point or code evaluation.
struct Rational {
    U n = 0, d = 1;
    bool negative = false;
    Rational(U numerator, U denominator = 1, bool sign = false)
        : n(numerator), d(denominator), negative(sign && numerator != 0) {
        const auto g = std::gcd(n, d); n /= g; d /= g;
    }
};
Rational product(Rational a, Rational b, bool divide) {
    if (divide) {
        if (!b.n) throw FinancialError(ErrorCode::invalid_decimal, "Cannot divide by zero. Change the calculation.");
        std::swap(b.n, b.d);
    }
    const auto g = std::gcd(a.n, b.d), h = std::gcd(b.n, a.d);
    return {multiply(a.n / g, b.n / h), multiply(a.d / h, b.d / g), a.negative != b.negative};
}
Rational sum(Rational a, Rational b, bool subtract) {
    if (subtract && b.n) b.negative = !b.negative;
    const auto g = std::gcd(a.d, b.d);
    const auto left = multiply(a.n, b.d / g), right = multiply(b.n, a.d / g);
    const auto denominator = multiply(a.d, b.d / g);
    if (a.negative == b.negative) return {add(left, right), denominator, a.negative};
    if (left >= right) return {left - right, denominator, a.negative};
    return {right - left, denominator, b.negative};
}
class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {
        if (input.size() > 4096)
            throw FinancialError(ErrorCode::invalid_decimal, "Calculation is too long. Use at most 4096 characters.");
    }
    Rational parse() {
        auto result = expression(0);
        whitespace();
        if (position_ != input_.size()) syntax();
        return result;
    }
private:
    std::string_view input_;
    std::size_t position_ = 0;
    void whitespace() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' || input_[position_] == '\n' || input_[position_] == '\r')) ++position_;
    }
    bool take(char c) {
        whitespace();
        if (position_ == input_.size() || input_[position_] != c) return false;
        ++position_; return true;
    }
    Rational expression(unsigned depth) {
        auto result = term(depth);
        for (;;) {
            if (take('+')) result = sum(result, term(depth), false);
            else if (take('-')) result = sum(result, term(depth), true);
            else return result;
        }
    }
    Rational term(unsigned depth) {
        auto result = factor(depth);
        for (;;) {
            if (take('*')) result = product(result, factor(depth), false);
            else if (take('/')) result = product(result, factor(depth), true);
            else return result;
        }
    }
    Rational factor(unsigned depth) {
        if (depth >= 64)
            throw FinancialError(ErrorCode::invalid_decimal, "Calculation is nested too deeply. Use fewer parentheses or signs.");
        if (take('+')) return factor(depth + 1);
        if (take('-')) { auto value = factor(depth + 1); if (value.n) value.negative = !value.negative; return value; }
        if (take('(')) { auto value = expression(depth + 1); if (!take(')')) syntax(); return value; }
        whitespace();
        U numerator = 0, denominator = 1;
        bool digit = false, dot = false;
        while (position_ < input_.size()) {
            const char c = input_[position_];
            if (c == '.' && !dot) { dot = true; ++position_; continue; }
            if (c < '0' || c > '9') break;
            digit = true; ++position_;
            numerator = add(multiply(numerator, 10), static_cast<U>(c - '0'));
            if (dot) denominator = multiply(denominator, 10);
        }
        if (!digit) syntax();
        return {numerator, denominator};
    }
};
}
Money Money::evaluate(std::string_view expression, Currency currency) {
    auto result = Parser(expression).parse();
    U scale = 1;
    for (unsigned i = 0; i < currency.scale(); ++i) scale *= 10;
    const auto common = std::gcd(result.d, scale);
    if (result.d / common != 1)
        throw FinancialError(ErrorCode::precision, "The result has more decimal places than this currency allows. Change the calculation.");
    const auto units = multiply(result.n, scale / common);
    constexpr auto high = static_cast<U>(std::numeric_limits<std::int64_t>::max());
    if (units > high + (result.negative ? U{1} : U{0})) too_large();
    const auto signed_units = result.negative
        ? (units == high + 1 ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(units))
        : static_cast<std::int64_t>(units);
    return Money(signed_units, std::move(currency));
}
} // namespace spentit
