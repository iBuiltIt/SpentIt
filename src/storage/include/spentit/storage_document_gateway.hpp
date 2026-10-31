#pragma once
#include <spentit/import_store.hpp>
#include <spentit/document_gateway.hpp>
namespace spentit::storage {
std::unique_ptr<app::DocumentGateway> document_gateway(const std::filesystem::path& verification_cache={});
// Public SQLCipher raw-key specification, deliberately provides NO confidentiality.
inline constexpr std::string_view public_document_key = "x'5370656e744974207075626c696320666f726d6174206b6579207631000000000000'";
}
