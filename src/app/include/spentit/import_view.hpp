#pragma once
#include <spentit/import_repository.hpp>
#include <spentit/ledger_search.hpp>
namespace spentit::app {
struct LedgerView {
    std::vector<LedgerRow> rows; // includes the explicit Opening Balance at sequence zero
    std::vector<Money> balances; // keyed by rows, independent of display order/filter
    std::vector<std::size_t> order;
    Money withdrawals; // signed negative; formatting can display magnitude without negating INT64_MIN
    Money deposits;
    Money ending;
};
std::string decimal(const Money&);
LedgerView ledger_view(const Account&,std::vector<LedgerRow>,std::string_view literal,int sort_column,bool descending);
using SourceLookup = std::function<std::optional<std::string_view>(const imports::Occurrence&)>;
LedgerView search_ledger(const Account&,std::vector<LedgerRow>,LedgerSearch&,int sort_column,bool descending,
                         const SourceLookup& source = {}, const std::function<bool()>& cancelled = {});
} // namespace spentit::app
