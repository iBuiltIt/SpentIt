#pragma once
#include <spentit/rule_regex.hpp>
#include <functional>
#include <span>

namespace spentit::app {
// A query is compiled once, on the application worker. The caller supplies the
// local calendar year so relative periods are deterministic in tests.
class LedgerSearch {
public:
    LedgerSearch(std::string_view query, int current_year,std::string_view date_format="YYYY-MM-DD");
    std::string display_date(std::string_view iso) const;
    bool source_mode() const { return source_; }
    bool empty() const { return !source_ && first_.empty() && terms_.empty() && !category_; }
    bool matches(std::string_view effective_date, std::span<const std::string_view> fields,
                 bool has_source, const std::function<bool()>& cancelled = {},
                 std::span<const std::string_view> categories = {});
private:
    bool source_=false;
    std::string first_,last_,date_format_;
    std::vector<rules::Pattern> terms_;
    std::optional<rules::Pattern> category_;
    std::size_t bytes_=0,comparisons_=0,steps_=0;
};
}
