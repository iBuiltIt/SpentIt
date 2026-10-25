#include <spentit/transaction.hpp>
#include <spentit/split_editor.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace spentit;
namespace {
const Currency cad("CAD", 2);
const Currency usd("USD", 2);
constexpr auto low = std::numeric_limits<std::int64_t>::min();
constexpr auto high = std::numeric_limits<std::int64_t>::max();
void check(bool value) { if (!value) throw std::runtime_error("Check failed"); }
template<class F> void fails(ErrorCode code, F f) {
    try { f(); } catch (const FinancialError& e) { check(e.code() == code); return; }
    throw std::runtime_error("Expected FinancialError");
}
Money m(std::int64_t units) { return Money(units, cad); }
Allocation a(std::int64_t units) { return {"category", "synthetic memo", m(units)}; }
void decimal() {
    check(Money::parse("-29.31", cad) == m(-2931));
    check(Money::parse("+12.3", cad) == m(1230));
    check(Money::parse("00012", cad) == m(1200));
    check(Money::parse("-0.00", cad) == m(0));
    check(Money::parse("9", Currency("JPY", 0)).units() == 9);
    check(Money::parse("0.0001", Currency("CLF", 4)).units() == 1);
    for (const auto s : {"", "+", ".1", "1.", "1e2", " 1", "1 ", "1,00", "1.2.3", "--1", "NaN"})
        fails(ErrorCode::invalid_decimal, [&] { Money::parse(s, cad); });
    fails(ErrorCode::precision, [] { Money::parse("1.001", cad); });
    fails(ErrorCode::precision, [] { Money::parse("1.000", cad); });
    fails(ErrorCode::precision, [] { Money::parse("1.0", Currency("JPY", 0)); });
    fails(ErrorCode::unsupported_scale, [] { Currency("CAD", 10); });
    for (const auto s : {"", "CA", "CADX", "cad", "C1D"})
        fails(ErrorCode::invalid_currency, [&] { Currency(s, 2); });
}
void arithmetic() {
    check(m(100) + m(-25) == m(75));
    check(m(100) - m(25) == m(75));
    check(-m(-25) == m(25));
    check(m(low) - m(low) == m(0));
    check(m(high) + m(low) == m(-1));
    fails(ErrorCode::overflow, [] { (void)(m(high) + m(1)); });
    fails(ErrorCode::overflow, [] { (void)(m(low) + m(-1)); });
    fails(ErrorCode::overflow, [] { (void)(m(low) - m(1)); });
    fails(ErrorCode::overflow, [] { (void)(m(high) - m(-1)); });
    fails(ErrorCode::overflow, [] { (void)-m(low); });
    fails(ErrorCode::currency_mismatch, [] { (void)(m(1) + Money(1, usd)); });
    fails(ErrorCode::currency_mismatch, [] { (void)(m(1) - Money(1, usd)); });
    fails(ErrorCode::scale_mismatch, [] { (void)(m(1) + Money(1, Currency("CAD", 3))); });
    fails(ErrorCode::scale_mismatch, [] { (void)(m(1) - Money(1, Currency("CAD", 3))); });
}
void splits() {
    auto ordinary = Transaction::imported(m(-2931), {a(-14), a(-2917)});
    check(ordinary.allocations()[0].memo == "synthetic memo");
    check(ordinary.allocations()[1].amount == m(-2917));
    check(Transaction::manual(m(220000), {a(300000), a(-70000), a(-10000)}).effective_total() == m(220000));
    check(Transaction::manual(m(100), {a(100), a(0)}).allocations().size() == 2);
    check(Transaction::manual(m(0), {a(100), a(-100), a(0)}).effective_total() == m(0));
    check(Transaction::manual(m(0), {a(0)}).effective_total() == m(0));
    fails(ErrorCode::empty_allocations, [] { Transaction::manual(m(0), {}); });
    fails(ErrorCode::split_mismatch, [] { Transaction::manual(m(100), {a(99)}); });
    fails(ErrorCode::currency_mismatch, [] { Transaction::manual(m(0), {{"", "", Money(0, usd)}}); });
    fails(ErrorCode::scale_mismatch, [] { Transaction::manual(m(0), {{"", "", Money(0, Currency("CAD", 3))}}); });
}
void corrections() {
    const auto original = Transaction::imported(m(100), {a(100)});
    const auto corrected = original.corrected(m(110), {a(100), a(10)});
    const auto again = corrected.corrected(m(90), {a(90)});
    check(original.effective_total() == m(100));
    check(!original.has_corrected_imported_total());
    check(corrected.has_corrected_imported_total());
    check(again.original_imported_amount() == m(100));
    check(again.effective_total() == m(90));
    check(!again.corrected(m(100), {a(100)}).has_corrected_imported_total());
    fails(ErrorCode::split_mismatch, [&] { original.corrected(m(110), {a(100)}); });
    check(original.original_imported_amount() == m(100));
    fails(ErrorCode::currency_mismatch, [&] { original.corrected(Money(100, usd), {{"", "", Money(100, usd)}}); });
    fails(ErrorCode::scale_mismatch, [&] { original.corrected(Money(100, Currency("CAD", 3)), {}); });
    check(!Transaction::manual(m(100), {a(100)}).corrected(m(90), {a(90)}).original_imported_amount());
}
void catalog() {
    check(supported_currencies().size() == 165);
    std::string previous;
    for (auto entry : supported_currencies()) {
        check(previous < entry.code);
        previous = entry.code;
        const Currency currency(std::string(entry.code));
        check(currency.scale() == entry.scale);
        check(currency == Currency(std::string(entry.code), entry.scale));
        std::string unit = entry.scale == 0 ? "1" : "0." + std::string(entry.scale - 1, '0') + "1";
        check(Money::parse(unit, currency).units() == 1);
        fails(ErrorCode::scale_mismatch, [&] { Currency(std::string(entry.code), entry.scale + 1); });
        fails(ErrorCode::precision, [&] { Money::parse(entry.scale == 0 ? "1.0" : unit + "0", currency); });
    }
    check(Currency("CAD").scale() == 2);
    check(Currency("JPY").scale() == 0);
    check(Currency("KWD").scale() == 3);
    check(Currency("CLF").scale() == 4);
    check(Currency("XCG").scale() == 2);
    check(Currency("XAD").scale() == 2);
    for (const auto code : {"BGN", "HRK", "ZZZ", "TST", "XAU", "XDR", "XXX"}) {
        fails(ErrorCode::unsupported_currency, [&] { Currency currency(code); });
        fails(ErrorCode::unsupported_currency, [&] { Currency currency(code, 2); });
    }
    fails(ErrorCode::invalid_currency, [] { Currency currency("cad"); });
}
void historical() {
    check(amount_currency_definitions().size() == 167);
    for (const auto& d : amount_currency_definitions()) {
        const auto currency = Currency::from_definition(d.id);
        check(currency.code() == d.code && currency.scale() == d.scale);
        check(currency.definition_id() == d.id);
        check(Money(low, currency).units() == low);
    }
    check(Currency::from_definition("six-2026-09-17:CAD") == cad);
    const auto bgn = Currency::from_definition("six-amendment-180:BGN");
    const auto hrk = Currency::from_definition("six-amendment-174:HRK");
    check(bgn.code() == "BGN" && hrk.code() == "HRK");
    for (const auto& currency : {bgn, hrk}) {
        fails(ErrorCode::unsupported_currency, [&] { Currency current(currency.code()); });
        const auto original = Money::parse("-29.31", currency);
        const auto transaction = Transaction::imported(original,
            {{"fees", "", Money(-14, currency)}, {"travel", "", Money(-2917, currency)}});
        const auto corrected = transaction.corrected(Money(-3000, currency),
            {{"fees", "", Money(-14, currency)}, {"travel", "", Money(-2986, currency)}});
        check(corrected.original_imported_amount() == original);
        check((original + Money(1, currency)).units() == -2930);
        check(Currency::from_definition(currency.definition_id()) == currency);
        fails(ErrorCode::precision, [&] { Money::parse("1.001", currency); });
        fails(ErrorCode::currency_mismatch, [&] { (void)(original + m(0)); });
    }
    for (const auto id : {"", "BGN", "six-amendment-180:ZZZ", "six-2026-09-17:HRK", "unknown:CAD:2"})
        fails(ErrorCode::unresolved_currency_definition, [&] { Currency::from_definition(id); });
}
void editing() {
    const SplitEditor start(Transaction::imported(m(100), {{"remainder", "keep", m(100)}}), 0);
    const auto first = start.append({"entered", "memo", m(40)});
    check(first.transaction().allocations()[0].amount == m(60));
    check(first.transaction().allocations()[0].category == "remainder");
    check(first.transaction().allocations()[0].memo == "keep");
    check(first.transaction().allocations()[1].amount == m(40));
    check(start.transaction().allocations().size() == 1);
    const auto zero = first.edit(1, a(100));
    check(zero.transaction().allocations()[0].amount == m(0));
    fails(ErrorCode::mixed_sign_choice_required, [&] { zero.edit(1, a(101)); });
    fails(ErrorCode::mixed_sign_choice_required, [&] { first.edit(1, a(-1)); });
    fails(ErrorCode::mixed_sign_choice_required, [&] { first.append(a(61)); });
    check(zero.transaction().allocations()[0].amount == m(0));
    const auto mixed = zero.allow_mixed_signs().edit(1, a(101));
    check(mixed.transaction().allocations()[0].amount == m(-1));
    check(mixed.transaction().allocations()[1].amount == m(101));
    check(!zero.mixed_signs_enabled());
    check(mixed.mixed_signs_enabled());
    const auto removed = first.remove(1);
    check(removed.transaction().allocations()[0].amount == m(100));
    const auto designated = first.designate_remainder(1).edit(0, a(25));
    check(designated.transaction().allocations()[1].amount == m(75));
    const auto shifted = designated.remove(0);
    check(shifted.remainder_index() == 0);
    check(shifted.transaction().allocations()[0].amount == m(100));
    fails(ErrorCode::remainder_edit, [&] { first.remove(0); });
    fails(ErrorCode::remainder_edit, [&] { first.edit(0, a(99)); });
    fails(ErrorCode::invalid_allocation_index, [&] { SplitEditor(start.transaction(), 1); });
    fails(ErrorCode::invalid_allocation_index, [&] { first.edit(99, a(0)); });
    fails(ErrorCode::invalid_allocation_index, [&] { first.remove(99); });
    fails(ErrorCode::invalid_allocation_index, [&] { first.designate_remainder(99); });
    fails(ErrorCode::currency_mismatch, [&] { first.append({"", "", Money(1, usd)}); });
    fails(ErrorCode::currency_mismatch, [&] { first.edit(1, {"", "", Money(1, usd)}); });
    fails(ErrorCode::currency_mismatch, [&] { first.correct_total(Money(100, usd)); });
    const auto corrected = first.correct_total(m(120)).correct_total(m(130));
    check(corrected.transaction().original_imported_amount() == m(100));
    check(corrected.transaction().allocations()[1].amount == m(40));
    check(corrected.transaction().allocations()[0].amount == m(90));
    check(corrected.transaction().has_corrected_imported_total());
    fails(ErrorCode::mixed_sign_choice_required, [&] { first.correct_total(m(30)); });
    check(first.allow_mixed_signs().correct_total(m(30)).transaction().allocations()[0].amount == m(-10));
    const SplitEditor withdrawal(Transaction::manual(m(-100), {a(-100)}), 0);
    check(withdrawal.append(a(-100)).transaction().allocations()[0].amount == m(0));
    fails(ErrorCode::mixed_sign_choice_required, [&] { withdrawal.append(a(-101)); });
    check(withdrawal.allow_mixed_signs().append(a(-101)).transaction().allocations()[0].amount == m(1));
    const SplitEditor empty_total(Transaction::manual(m(0), {a(0)}), 0);
    fails(ErrorCode::mixed_sign_choice_required, [&] { empty_total.append(a(1)); });
    check(empty_total.allow_mixed_signs().append(a(1)).transaction().allocations()[0].amount == m(-1));
    const SplitEditor imported(Transaction::imported(m(220000), {a(300000), a(-70000), a(-10000)}), 2);
    check(imported.mixed_signs_enabled());
    check(imported.transaction().allocations()[2].amount == m(-10000));
    check(imported.edit(0, a(310000)).transaction().allocations()[2].amount == m(-20000));
    check(imported.transaction().allocations()[0].amount == m(300000));
}
void editing_boundaries() {
    const SplitEditor minimum(Transaction::manual(m(low), {a(low)}), 0);
    check(minimum.append(a(low)).transaction().allocations()[0].amount == m(0));
    const SplitEditor maximum(Transaction::manual(m(high), {a(high)}), 0);
    fails(ErrorCode::overflow, [&] { maximum.allow_mixed_signs().append(a(-1)); });
    fails(ErrorCode::overflow, [&] { minimum.allow_mixed_signs().append(a(1)); });
    // The explicit subtotal exceeds INT64_MAX; the final remainder still fits.
    const SplitEditor large(Transaction::manual(m(high), {a(-high), a(high), a(high)}), 0);
    check(large.edit(1, a(high)).transaction().allocations()[0].amount == m(-high));
    // Subtracted INT64_MIN has magnitude 2^63; cancellation must precede range checking.
    const SplitEditor low_terms(Transaction::manual(m(low), {a(high), a(low), a(-high)}), 0);
    check(low_terms.edit(1, a(low)).transaction().allocations()[0].amount == m(high));
    // A small exhaustive reference checks both sign gating and remainder calculation.
    for (std::int64_t total = -12; total <= 12; ++total) {
        const SplitEditor initial(Transaction::manual(m(total), {a(total)}), 0);
        for (std::int64_t entered = -15; entered <= 15; ++entered) {
            const auto expected = total - entered;
            const bool mixed = (entered < 0 && expected > 0) || (entered > 0 && expected < 0);
            if (mixed) fails(ErrorCode::mixed_sign_choice_required, [&] { initial.append(a(entered)); });
            else check(initial.append(a(entered)).transaction().allocations()[0].amount == m(expected));
            const auto result = initial.allow_mixed_signs().append(a(entered));
            check(result.transaction().allocations()[0].amount == m(expected));
            check(result.transaction().allocations()[1].amount == m(entered));
        }
    }
}

void boundaries() {
    check(Money::parse("92233720368547758.07", cad) == m(high));
    check(Money::parse("-92233720368547758.08", cad) == m(low));
    for (const auto s : {"92233720368547758.08", "-92233720368547758.09", "999999999999999999999999999"})
        fails(ErrorCode::overflow, [&] { Money::parse(s, cad); });
    fails(ErrorCode::overflow, [] { Money::parse("92233720368547759", cad); });
    // Every ordering of large, cancelling signed terms must produce the same total.
    std::vector<std::int64_t> terms{low, -1, 1, high};
    do {
        std::vector<Allocation> allocations;
        for (auto term : terms) allocations.push_back(a(term));
        auto t = Transaction::manual(m(-1), allocations);
        for (std::size_t i = 0; i < terms.size(); ++i) check(t.allocations()[i].amount.units() == terms[i]);
    } while (std::next_permutation(terms.begin(), terms.end()));
    check(Transaction::manual(m(high), {a(high), a(high), a(-high)}).effective_total() == m(high));
    check(Transaction::manual(m(low), {a(low), a(low), a(high), a(1)}).effective_total() == m(low));
    fails(ErrorCode::overflow, [] { Transaction::manual(m(0), {a(high), a(1)}); });
    fails(ErrorCode::overflow, [] { Transaction::manual(m(0), {a(low), a(-1)}); });
    // Exhaustive small arithmetic domain, independent of the checked formulas.
    for (std::int64_t x = -100; x <= 100; ++x)
        for (std::int64_t y = -100; y <= 100; ++y) {
            check((m(x) + m(y)).units() == x + y);
            check((m(x) - m(y)).units() == x - y);
        }
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected test case name");
        const std::string name = argv[1];
        if (name == "decimal") decimal();
        else if (name == "arithmetic") arithmetic();
        else if (name == "splits") splits();
        else if (name == "corrections") corrections();
        else if (name == "boundaries") boundaries();
        else if (name == "catalog") catalog();
        else if (name == "historical") historical();
        else if (name == "editing") editing();
        else if (name == "editing_boundaries") editing_boundaries();
        else throw std::runtime_error("Unknown test case");
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
