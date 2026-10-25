#pragma once
#include <spentit/import_repository.hpp>
#include <filesystem>
#include <memory>

namespace spentit::storage {
class ImportError : public std::runtime_error {
public:
    ImportError(int code,const char* message):std::runtime_error(message),code_(code){}
    int code() const noexcept { return code_; }
private: int code_;
};
class ImportAuthenticationError final : public ImportError {
public:
    explicit ImportAuthenticationError(int code):ImportError(code,"Unable to authenticate encrypted dataset"){}
};
enum class ImportPhase { before_commit, after_commit };
// Fault-test seam: no UI durability acknowledgement is sent through this hook.
using ImportObserver=std::function<void(ImportPhase)>;
enum class BackupPhase { before_publish, before_publish_sync, before_cleanup,
                         before_cleanup_sync, complete };
// Test seam around non-SQLite publication I/O. Throwing simulates a failed
// boundary; exiting simulates interruption. Never used to acknowledge success.
using BackupObserver=std::function<void(BackupPhase)>;
class ImportStore final : public app::ImportRepository {
public:
    static ImportStore create(const std::filesystem::path&,std::string_view password,
                              std::string_view name,const Currency&,std::string_view today);
    static ImportStore open(const std::filesystem::path&,std::string_view password);
    ~ImportStore();
    ImportStore(ImportStore&&) noexcept;
    ImportStore& operator=(ImportStore&&) noexcept;
    app::Account account() const override;
    void opening(std::int64_t,std::string_view,const Money&) override;
    std::vector<app::BatchInfo> batches() const override;
    app::BatchInfo ingest(std::string_view,std::string_view,qif::DateOrder) override;
    std::string evidence(std::string_view) const override;
    app::RowView row(const imports::Occurrence&) const override;
    void save(const imports::Occurrence&,std::int64_t,const imports::DraftRow&) override;
    void decide(const imports::Occurrence&,std::int64_t,const imports::Decision&) override;
    void discard(std::string_view,std::int64_t) override;
    app::Receipt commit(std::string_view,std::int64_t,std::string_view) override;
    std::optional<app::Receipt> outcome(std::string_view) const override;
    std::vector<app::LedgerRow> ledger(bool) const override;
    Money committed_balance() const override;
    std::optional<Money> projected_balance() const override;
    void verify() const;
    void backup_to(const std::filesystem::path&,std::string_view password,
                   const BackupObserver& observer = {}) const;
    void observer(ImportObserver);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit ImportStore(std::unique_ptr<Impl>);
    app::RowView load_row(const imports::Occurrence&, bool candidates) const;
    static ImportStore connect(const std::filesystem::path&,std::string_view,bool);
};
} // namespace spentit::storage
