#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace spentit {
enum class CategoryKind { group, leaf };
enum class FieldOwnership { untracked, source, manual, rule };
struct RuleMark {
    std::string identity;
    std::int64_t version = 0;
    bool operator==(const RuleMark&) const = default;
};
struct CategoryBinding {
    std::string identity;
    std::int64_t catalog_revision;
    bool operator==(const CategoryBinding&) const = default;
};
struct CategoryDefinition {
    std::string identity,name,parent;
    CategoryKind kind;
    std::int64_t revision;
    bool archived,deleted;
    bool operator==(const CategoryDefinition&) const = default;
};
// Exact UTF-8 identity of names: no normalization, folding or silent trimming.
void validate_category_name(std::string_view);
}
