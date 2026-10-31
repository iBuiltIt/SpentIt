#pragma once
#include <spentit/operation_context.hpp>
#include <filesystem>
namespace spentit::storage::verification_cache {
bool no_journals(const std::filesystem::path&);
bool matches(const std::filesystem::path&,const std::filesystem::path&,const app::OperationContext*);
std::string fingerprint(const std::filesystem::path&,const app::OperationContext*);
void publish(const std::filesystem::path&,const std::filesystem::path&,std::string_view expected,const app::OperationContext*);
}
