#include <spentit/sheet_session.hpp>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace spentit;
using namespace spentit::sheet;
void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } check(rejected,"Expected rejection"); }
int main() {
    try {
        const auto rows = synthetic(1000);
        const auto view = query(*rows,0,"",0,false);
        check(view.indices.size() == 1000,"Account boundary");
        Money withdrawals(0,Currency("CAD")), deposits(0,Currency("CAD"));
        for (int i : view.indices) {
            const auto amount = (*rows)[static_cast<std::size_t>(i)].transaction.effective_total();
            if (amount.units() < 0) withdrawals = withdrawals - amount; else deposits = deposits + amount;
        }
        check(view.withdrawals == decimal(withdrawals) && view.deposits == decimal(deposits),"Exact totals");
        const auto sorted = query(*rows,0,"",1,true);
        check(view.balances == sorted.balances,"Sort must retain chronological balances");
        const auto filtered = query(*rows,0,"ticket",5,true);
        check(filtered.indices.size() == 143,"Whole-record split memo filter");
        check(filtered.committed == view.committed && filtered.projected == view.projected,"Filter must not change account totals");
        check(filtered.balances == view.balances,"Filter must not recalculate balances");
        Money expectedCommitted(0,Currency("CAD"));
        for (const auto& row : *rows) if (row.account == 0 && !row.draft) expectedCommitted = expectedCommitted + row.transaction.effective_total();
        check(view.committed == decimal(expectedCommitted),"Draft exclusion");
        auto edit = begin_edit(*rows,1); edit.total = "-20.00"; edit.allocation = "-7.00"; edit.payee = "Revised";
        const auto updated = save_edit(*rows,edit);
        check(by_id(*updated,1).transaction.allocations()[0].amount.units() == -1300,"Remainder adjustment");
        check(by_id(*updated,1).transaction.original_imported_amount()->units() == -1200,"Original amount preserved");
        check(by_id(*rows,1).payee != "Revised","Original snapshot immutable");
        auto simple = begin_edit(*rows,2); simple.note = "Note only";
        check(save_edit(*rows,simple)->at(1).transaction.allocations().size() == 1,"Note save must not create split");
        simple.addAllocation = true; simple.category = "Explicit zero";
        check(save_edit(*rows,simple)->at(1).transaction.allocations().size() == 2,"Explicit zero allocation retained");
        auto simultaneous = begin_edit(*rows,1); simultaneous.total = "-1.00"; simultaneous.allocation = "-0.50";
        check(save_edit(*rows,simultaneous)->at(0).transaction.allocations()[0].amount.units() == -50,"Combined edit validates final state");
        edit.allocation = "not money"; rejects([&] { save_edit(*rows,edit); });
        edit.allocation = "-200.00"; rejects([&] { save_edit(*rows,edit); });
        edit.mixed = true; check(save_edit(*rows,edit)->at(0).transaction.allocations()[0].amount.units() == 18000,"Explicit mixed signs");
        edit.total = "9223372036854775808.00"; rejects([&] { save_edit(*rows,edit); });
        check(decimal(Money(std::numeric_limits<std::int64_t>::min(),Currency("CAD"))) == "-92233720368547758.08","Signed minimum formatting");
        rejects([&] { query(*rows,0,std::string(257,'x'),0,false); });
        const auto start = std::chrono::steady_clock::now(); const auto large = synthetic(50000);
        const auto generated = std::chrono::steady_clock::now();
        for (int column = 0; column < 6; ++column) {
            const auto t = std::chrono::steady_clock::now(); const auto result = query(*large,0,"",column,true);
            check(result.indices.size() == 50000,"Large workload complete");
            std::cout << "50000 rows sort " << column << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t).count() << " ms\n";
        }
        std::cout << "Generate 100000 records: " << std::chrono::duration_cast<std::chrono::milliseconds>(generated-start).count() << " ms\nSession invariants passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
