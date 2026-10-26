#pragma once
#include <spentit/import_repository.hpp>
#include <spentit/operation_context.hpp>
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
enum class ImportPhase { before_commit, after_commit, materialize_row };
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
    static ImportStore open(const std::filesystem::path&,std::string_view password, const app::OperationContext* = nullptr);
    ~ImportStore();
    ImportStore(ImportStore&&) noexcept;
    ImportStore& operator=(ImportStore&&) noexcept;
    app::Account account() const override;
    std::vector<app::Account> accounts() const override;
    void select_account(std::int64_t) override;
    std::optional<app::AccountCreation> account_creation_outcome(std::int64_t) const override;
    app::Account create_account(std::int64_t,std::string_view,const Currency&,std::string_view) override;
    void opening(std::int64_t,std::string_view,const Money&) override;
    std::vector<app::BatchInfo> batches() const override;
    app::BatchInfo ingest(std::string_view,std::string_view,qif::DateOrder) override;
    app::BatchInfo ingest_controlled(std::string_view,std::string_view,qif::DateOrder,app::OperationContext&) override;
    app::BatchInfo resume(std::string_view,std::int64_t,app::OperationContext&) override;
    std::string evidence(std::string_view) const override;
    app::RowView row(const imports::Occurrence&) const override;
    void save(const imports::Occurrence&,std::int64_t,const imports::DraftRow&) override;
    void decide(const imports::Occurrence&,std::int64_t,const imports::Decision&) override;
    void discard(std::string_view,std::int64_t) override;
    app::Receipt commit(std::string_view,std::int64_t,std::string_view) override;
    std::optional<app::Receipt> outcome(std::string_view) const override;
    std::optional<app::Receipt> durable_outcome(std::string_view) const override;
    std::vector<app::LedgerRow> ledger(bool) const override;
    bool categories_supported() const override;
    app::CategoryCatalog category_catalog() const override;
    app::CategoryPreview preview_category_change(const app::CategoryRequest&) const override;
    app::CategoryReceipt apply_category_change(std::string_view,const app::CategoryRequest&,std::int64_t) override;
    std::optional<app::CategoryReceipt> category_change_outcome(std::string_view) const override;
    std::vector<app::CategoryReceipt> category_history(std::string_view) const override;
    app::CategoryDisplay resolve_category(std::string_view) const override;
    app::ChangeReceipt apply_committed_change_with_catalog(std::string_view,const app::ChangeRequest&,const app::EntryToken&,std::int64_t,std::int64_t) override;
    bool committed_changes_supported() const override;
    app::CommittedEntry committed_entry(const imports::Occurrence&) const override;
    std::vector<app::CommittedHistoryItem> committed_history(const imports::Occurrence&) const override;
    std::vector<app::CommittedChangeSummary> committed_changes(std::int64_t before_sequence=0,std::size_t limit=50) const override;
    app::ChangePreview preview_committed_change(const app::ChangeRequest&) const override;
    app::ChangeReceipt apply_committed_change(std::string_view,const app::ChangeRequest&,const app::EntryToken&,std::int64_t) override;
    std::optional<app::ChangeReceipt> committed_change_outcome(std::string_view) const override;
    Money committed_balance() const override;
    bool bulk_supported() const override;
    std::string bulk_group(std::string_view) const override;
    app::BulkPreview preview_bulk(const app::BulkRequest&) const override;
    app::BulkReceipt apply_bulk(std::string_view,const app::BulkPreview&) override;
    std::optional<app::BulkReceipt> bulk_outcome(std::string_view) const override;
    std::optional<Money> projected_balance() const override;
    bool rules_supported() const override;
    app::RuleImage rule_image() const override;
    std::vector<app::RuleTombstone> rule_tombstones() const override;
    app::RuleSaveReceipt save_rules(std::string_view,const app::RuleSaveRequest&,std::int64_t,const app::OperationContext&) override;
    std::optional<app::RuleSaveReceipt> rule_save_outcome(std::string_view) const override;
    app::RulePreviewResult preview_pending_rules(const app::RulePreviewRequest&,const app::OperationContext&) const override;
    app::RuleApplyReceipt apply_pending_rules(std::string_view,const app::RuleApplyRequest&,const app::OperationContext&) override;
    std::optional<app::RuleApplyReceipt> rule_apply_outcome(std::string_view) const override;
    app::RuleMatch match_saved_rules(const imports::Occurrence&,const app::OperationContext&) const override;
    void verify(const app::OperationContext* = nullptr) const;
    void backup_to(const std::filesystem::path&,std::string_view password,
                   const BackupObserver& observer = {}) const;
    void observer(ImportObserver);
private:
    app::Account account_by_id(std::int64_t) const;
    app::Account account_for_batch(std::string_view) const;
    void require_account(std::string_view) const;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit ImportStore(std::unique_ptr<Impl>);
    imports::DraftRow canonical_draft(const imports::Occurrence&,const imports::DraftRow&,const std::optional<imports::DraftRow>&,std::int64_t,bool entry,bool check_assignment=true) const;
    void verify_categories(const app::OperationContext* context) const;
    app::EntrySnapshot entry_snapshot(const imports::Occurrence&,std::int64_t) const;
    std::vector<app::DependencyStamp> dependencies(const imports::Occurrence&,std::int64_t history_before=0) const;
    void verify_committed(const app::OperationContext* = nullptr) const;
    void verify_rules(const app::OperationContext* = nullptr) const;
    void verify_bulk(const app::OperationContext* = nullptr) const;
    void populate(std::string_view,std::string_view,const qif::Result&,std::int64_t,const app::OperationContext*);
    app::RowView load_row(const imports::Occurrence&, bool candidates) const;
    static ImportStore connect(const std::filesystem::path&,std::string_view,bool,const app::OperationContext* = nullptr);
};
} // namespace spentit::storage
