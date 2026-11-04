#pragma once
#include <spentit/money.hpp>
#include <functional>
#include <vector>

namespace spentit::reports {
struct Period {
    std::string preset="ytd", start, end;
    bool operator==(const Period&) const = default;
};
struct Range {std::string label,start,end;};
struct Definition {
    std::string id,name,graph="bar";
    std::vector<std::int64_t> accounts;
    // Empty identity is the explicit Uncategorized selection.
    std::vector<std::string> categories;
    std::vector<Period> periods;
    bool operator==(const Definition&) const = default;
};
struct Catalog {std::int64_t revision=0;std::vector<Definition> definitions;};
Range resolve(const Period&,std::string_view today,const Range* main=nullptr);
void validate(const Definition&);
struct Category {std::string id,name;};
struct Entry {
    std::int64_t account;
    std::string account_name,batch,allocation,date,payee,memo,category;
    std::size_t ordinal;
    Money amount;
    bool transfer=false,committed=false;
};
struct Cell {Money amount;std::vector<std::size_t> entries;};
struct Row {Category category;std::vector<Cell> cells;};
struct Result {
    Currency currency;
    std::vector<Row> rows;
    std::vector<Cell> totals;
};
struct Snapshot {
    Definition definition;
    std::vector<Range> periods;
    std::vector<Entry> entries;
    std::vector<Result> currencies;
};
// Caller supplies one immutable current-ledger snapshot, never opening balances.
Snapshot calculate(const Definition&,std::string_view today,std::vector<Entry>,
                   const std::vector<Category>&,const std::vector<Currency>&,
                   const std::function<void()>& checkpoint={});
}
