#include <spentit/category_catalog.hpp>
#include <spentit/rule_regex.hpp>
namespace spentit::app {
bool category_name_matches(std::string_view name,std::string_view selected,bool include_children) {
    const auto canonical=canonical_category_name(selected);
    std::string pattern="\\A";
    for(char c:canonical){if(std::string_view(R"(\.^$|()[]{}*+?)").find(c)!=std::string_view::npos)pattern+='\\';pattern+=c;}
    pattern+=include_children && canonical.find(':')==canonical.npos?"(?:\\z|: )":"\\z";
    return rules::Pattern(pattern).match(canonical_category_name(name)).has_value();
}
}
