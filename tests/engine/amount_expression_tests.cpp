#include <spentit/money.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace spentit;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("Expression check failed"); }
void fails(std::string_view input, ErrorCode expected) {
    try { (void)Money::evaluate(input, Currency("CAD")); }
    catch (const FinancialError& e) { check(e.code() == expected); return; }
    throw std::runtime_error("Expected expression rejection");
}
}
int main() {
    try {
        const Currency cad("CAD");
        const auto value = [&](std::string_view s) { return Money::evaluate(s, cad).units(); };
        check(value("-9.34+(12*0.25)") == -634);
        check(value(" (10 / 3) * 3 ") == 1000);
        check(value("1/3+1/6+1/2") == 100);
        check(value("1+2*3") == 700);
        check(value("(1+2)*3") == 900);
        check(value("8/4/2") == 100);
        check(value("8-4-2") == 200);
        check(value("-(9.34+3)") == -1234);
        check(value("1--2") == 300);
        check(value("+.25 * -4") == -100);
        check(value("0.001*1000") == 100);
        check(value("-0/3") == 0);
        check(value("1.000") == 100);
        check(value("92233720368547758.07") == std::numeric_limits<std::int64_t>::max());
        check(value("-92233720368547758.08") == std::numeric_limits<std::int64_t>::min());
        check(value("-92233720368547758.08+92233720368547758.07") == -1);
        check(value("18446744073709551615/18446744073709551615") == 100);
        check(Money::evaluate("1/4",Currency("KWD")).units() == 250);
        check(Money::evaluate("1/10000",Currency("CLF")).units() == 1);
        check(Money::evaluate("(10/3)*3",Currency("JPY")).units() == 10);
        check(Money::evaluate("1/4",Currency::from_definition(cad.definition_id())).units() == 25);
        for (auto s : {"", " ", "1+", "()", "(1", "1)", "1 2", "2(3)", "2**3", "1e2", "a", "1,000", "1..2", ".", "1/0", "0/0", "1/(2-2)"})
            fails(s, ErrorCode::invalid_decimal);
        for (auto s : {"10/3", "0.001", "1/8"}) fails(s, ErrorCode::precision);
        for (auto s : {"92233720368547758.08", "-92233720368547758.09", "18446744073709551616", "18446744073709551615*2", "1/18446744073709551615/2"})
            fails(s, ErrorCode::overflow);
        fails(std::string(4097, '1'), ErrorCode::invalid_decimal);
        fails(std::string(64, '(')+"1"+std::string(64, ')'), ErrorCode::invalid_decimal);
        fails(std::string(64, '-')+"1", ErrorCode::invalid_decimal);
        // Broad exact cancellation coverage, including negative intermediate fractions.
        for (int n=-100;n<=100;++n) for (int d=1;d<=31;++d) {
            check(value("("+std::to_string(n)+"/"+std::to_string(d)+")*"+std::to_string(d)) == n*100);
        }
        // Import decimal grammar must not acquire expression semantics.
        try { (void)Money::parse("1+2",cad); throw std::runtime_error("Import accepted expression"); }
        catch (const FinancialError& e) { check(e.code()==ErrorCode::invalid_decimal); }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
