#pragma once
#include <spentit/transaction.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spentit::storage {
// Experimental single-process storage, not the application document API.
class StorageError : public std::runtime_error {
public:
    StorageError(int code, const char* message) : std::runtime_error(message), code_(code) {}
    int code() const noexcept { return code_; }
private:
    int code_;
};
enum class CommitPhase { before_durable_commit, after_durable_commit };
using CommitObserver = std::function<void(CommitPhase)>;
struct Draft {
    std::string id;
    std::string evidence;
    Transaction transaction;
    bool committed;
};
class PrototypeStore {
public:
    static PrototypeStore create(const std::filesystem::path&, std::string_view password);
    static PrototypeStore open(const std::filesystem::path&, std::string_view password);
    ~PrototypeStore();
    PrototypeStore(PrototypeStore&&) noexcept;
    PrototypeStore& operator=(PrototypeStore&&) noexcept;
    PrototypeStore(const PrototypeStore&) = delete;
    PrototypeStore& operator=(const PrototypeStore&) = delete;
    // Stage a validated snapshot. IDs and original evidence are immutable.
    void stage(std::string_view id, std::string_view evidence, const Transaction&);
    Draft load(std::string_view id) const;
    // Same operation + same member set is an idempotent retry; other reuse fails.
    // Observer is a prototype fault-testing seam, never a durability acknowledgment.
    void commit(std::vector<std::string> ids, std::string_view operation,
                const CommitObserver& observer = {});
    std::vector<Money> committed_amounts() const;
    std::size_t history_count() const;
    void verify() const;
    // Encrypted candidate -> reopen/validate -> exclusive publication. Never overwrite.
    void backup_to(const std::filesystem::path&, std::string_view backup_password) const;
private:
    struct Impl;
    explicit PrototypeStore(std::unique_ptr<Impl>);
    static PrototypeStore connect(const std::filesystem::path&, std::string_view, bool create);
    std::unique_ptr<Impl> impl_;
};
} // namespace spentit::storage
