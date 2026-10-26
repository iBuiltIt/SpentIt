#pragma once
#include <spentit/rule_regex.hpp>
#include <functional>
#include <span>

namespace spentit::app {
// A query is compiled once, on the application worker. The caller supplies the
// local calendar year so relative periods are deterministic in tests.
class LedgerSearch {
public:
    LedgerSearch(std::string_view query, int current_year);
    bool source_mode() const { return source_; }
    bool empty() const { return !source_ && first_.empty() && terms_.empty(); }
    bool matches(std::string_view effective_date, std::span<const std::string_view> fields,
                 bool has_source, const std::function<bool()>& cancelled = {});
private:
    bool source_=false;
    std::string first_,last_;
    std::vector<rules::Pattern> terms_;
    std::size_t bytes_=0,comparisons_=0,steps_=0;
};
}
