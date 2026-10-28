#pragma once
#include <spentit/import_repository.hpp>
#include <spentit/operation_context.hpp>
#include <filesystem>
#include <memory>
namespace spentit::app {
class UnlockRequired : public std::runtime_error {
public: UnlockRequired():std::runtime_error("Unable to unlock. Enter a password or choose another dataset; damaged encrypted input can produce the same result."){}
};
// Owned and used exclusively by a serial worker. Closing releases all dataset state.
class DocumentGateway {
public:
    virtual ~DocumentGateway() = default;
    virtual void create(const std::filesystem::path&, bool protected_mode, std::string_view password,
                        std::string_view name, const Currency&, std::string_view today) = 0;
    virtual void create_selected(const std::filesystem::path& path, bool overwrite, bool protected_mode,
                                 std::string_view password, std::string_view name, const Currency& currency, std::string_view today) {
        if(overwrite) throw std::logic_error("Dataset replacement is not supported by this gateway");
        create(path,protected_mode,password,name,currency,today);
    }
    virtual void open(const std::filesystem::path&, std::optional<std::string> password) = 0;
    virtual void open_controlled(const std::filesystem::path& path, std::optional<std::string> password,
                                 const OperationContext& context) {
        context.checkpoint("Opening dataset");
        open(path,std::move(password));
        try { context.checkpoint(); } catch (...) { close(); throw; }
    }
    virtual void close() = 0;
    virtual ImportRepository& repository() = 0;
    // Backup/restore publishes a new document with the current protection mode.
    virtual void backup(const std::filesystem::path&) = 0;
    virtual void backup_controlled(const std::filesystem::path& path, const OperationContext& context) {
        context.checkpoint("Preparing backup");
        context.seal();
        backup(path);
    }
};
}
