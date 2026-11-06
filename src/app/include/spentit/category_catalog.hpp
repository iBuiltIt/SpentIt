#pragma once
#include <spentit/category.hpp>
#include <vector>
#include <map>
namespace spentit::app {
// Unicode caseless literal matching, with a colon boundary for stem selection.
bool category_name_matches(std::string_view name, std::string_view selected, bool include_children=false);
// Unicode caseless account-name matching without category syntax restrictions.
bool account_name_matches(std::string_view, std::string_view);
enum class CategoryAction { create, rename, move, archive, unarchive, erase, set_income };
struct CategoryCatalog { std::int64_t revision;std::vector<CategoryDefinition> categories;
    // Immutable creation catalog revision; later edits do not change this order.
    std::map<std::string,std::int64_t> creation_order{};
};
struct CategoryRequest {
    CategoryAction action;
    std::string identity;
    std::string name,parent;
    CategoryKind kind=CategoryKind::leaf;
    bool income=false;
    bool operator==(const CategoryRequest&) const = default;
};
struct CategoryPreview {
    CategoryRequest request;
    std::int64_t catalog_revision;
    std::optional<CategoryDefinition> before;
    CategoryDefinition after;
};
struct CategoryReceipt {
    std::string operation;
    CategoryRequest request;
    std::int64_t catalog_before,catalog_after;
    std::optional<CategoryDefinition> before;
    CategoryDefinition after;
};
struct CategoryDisplay {
    std::string identity,path;
    bool assignable;
    bool archived;
};
}
