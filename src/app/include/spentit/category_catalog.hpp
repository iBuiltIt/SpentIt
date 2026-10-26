#pragma once
#include <spentit/category.hpp>
#include <vector>
namespace spentit::app {
enum class CategoryAction { create, rename, move, archive, unarchive, erase };
struct CategoryCatalog { std::int64_t revision;std::vector<CategoryDefinition> categories; };
struct CategoryRequest {
    CategoryAction action;
    std::string identity;
    std::string name,parent;
    CategoryKind kind=CategoryKind::leaf;
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
