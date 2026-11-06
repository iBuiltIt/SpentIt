#include <tuple>
#pragma once
#include <spentit/import_repository.hpp>
#include <spentit/operation_context.hpp>
#include <filesystem>
#include <memory>
#include <set>

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
enum class ImportPhase { before_commit, after_commit, materialize_row, review_row, validate_row };
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
                              std::string_view name,const Currency&,std::string_view today,bool overwrite=false,const app::DatasetOptions& options={});
    // Optional local approval cache; ordinary open always performs full verification.
    static ImportStore open_cached(const std::filesystem::path&,std::string_view,
                                   const std::filesystem::path& cache,const app::OperationContext* = nullptr);
    void certify(const std::filesystem::path& cache,const app::OperationContext* = nullptr);
    static ImportStore open(const std::filesystem::path&,std::string_view password, const app::OperationContext* = nullptr);
    ~ImportStore();
    ImportStore(ImportStore&&) noexcept;
    ImportStore& operator=(ImportStore&&) noexcept;
    reports::Catalog report_catalog() const override;
    void save_reports(const reports::Catalog&) override;
    std::vector<reports::Entry> report_entries(const std::vector<std::int64_t>&,const app::OperationContext&) const override;
    std::string date_format() const override;
    AmountFormat amount_format() const override;
    void set_amount_format(const AmountFormat&) override;
    imports::Occurrence create_manual(std::string_view,const imports::DraftRow&) override;
    bool manual_outcome(std::string_view) const override;
    void undo_manual(std::string_view,std::int64_t) override;
    std::optional<imports::Occurrence> imported_original(const imports::Occurrence&) const override;
    app::Account account() const override;
    bool transfers_supported() const override { return true; }
    std::int64_t transfer_sequence() const override;
    std::vector<app::TransferView> transfers() const override;
    app::TransferReceipt apply_transfer(std::string_view,const app::TransferRequest&) override;
    app::TransferPreview preview_transfer(const app::TransferRequest&) const override;
    std::optional<app::TransferReceipt> transfer_outcome(std::string_view) const override;
    std::vector<app::TransferJournalEntry> transfer_journal() const override;
    std::vector<app::Account> accounts() const override;
    std::int64_t next_account_identity() const override;
    void select_account(std::int64_t) override;
    std::optional<app::AccountCreation> account_creation_outcome(std::int64_t) const override;
    app::Account create_account(std::int64_t,std::string_view,const Currency&,std::string_view) override;
    void opening(std::int64_t,std::string_view,const Money&) override;
    bool account_selected() const override;
    void release_account_selection() override;
    std::vector<app::Account> archived_accounts() const override;
    app::AccountCommandReceipt rename_account(std::string_view,std::string_view) override;
    app::AccountCommandReceipt archive_account(std::string_view) override;
    app::AccountCommandReceipt reactivate_account(std::string_view) override;
    app::AccountCommandReceipt delete_account(std::string_view) override;
    app::AccountCommandReceipt correct_opening(std::string_view,std::string_view,const Money&) override;
    app::AccountCommandReceipt reassign_currency(std::string_view,const Currency&) override;
    app::AccountCommandReceipt undo_account_operation(std::string_view,std::string_view) override;
    std::optional<app::AccountCommandReceipt> account_command_outcome(std::string_view) const override;
    std::optional<std::string> undoable_account_operation() const override;
    bool account_currency_warning() const override;
    std::vector<app::BatchInfo> batches() const override;
    std::vector<app::ImportGroup> import_groups() const override;
    std::optional<app::ImportGroup> import_group_outcome(std::string_view) const override;
    app::ImportGroup prepare_import_group(std::string_view,std::string_view,qif::DateOrder,const std::vector<app::ImportAccountMapping>&,app::OperationContext&,std::string_view ="qif") override;
    std::vector<app::MissingImportCategories> missing_import_categories() const override;
    void resolve_import_categories(std::string_view,const std::vector<std::string>&,app::OperationContext&) override;
    app::ImportGroup resume_import_group(std::string_view,app::OperationContext&) override;
    bool cleanup_incomplete_imports(std::string_view source = {}) override;
    bool startup_import_cleanup() const override;
    void abort_import_group(std::string_view) override;
    void resolve_import_transfers(std::string_view,app::OperationContext&) override;
    void review_import_balance(std::string_view,std::size_t,std::int64_t,app::ReportedBalanceChoice,std::string_view) override;
    app::BatchInfo ingest(std::string_view,std::string_view,qif::DateOrder) override;
    app::BatchInfo ingest_controlled(std::string_view,std::string_view,qif::DateOrder,app::OperationContext&) override;
    app::BatchInfo resume(std::string_view,std::int64_t,app::OperationContext&) override;
    std::string evidence(std::string_view) const override;
    qif::Result parsed_source(std::string_view,const std::function<void()>& = {}) const override;
    app::ImportRepairProgress repair_progress(std::string_view) const override;
    std::string record_evidence(const imports::Occurrence&) const override;
    app::RowView row(const imports::Occurrence&) const override;
    void save(const imports::Occurrence&,std::int64_t,const imports::DraftRow&) override;
    void decide(const imports::Occurrence&,std::int64_t,const imports::Decision&) override;
    void discard(std::string_view,std::int64_t) override;
    app::Receipt commit(std::string_view,std::int64_t,std::string_view) override;
    std::optional<app::Receipt> outcome(std::string_view) const override;
    std::optional<app::Receipt> durable_outcome(std::string_view) const override;
    std::vector<app::LedgerRow> ledger(bool) const override;
    std::vector<app::LedgerRow> current_ledger() const override;
    Money balance() const override;
    std::vector<std::pair<std::int64_t,Money>> balances() const override;
    bool categories_supported() const override;
    std::vector<app::AccountAlias> account_aliases() const override;
    std::optional<std::int64_t> resolve_account_name(std::string_view) const override;
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
    std::vector<app::AccountBalance> account_balances() const override;
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
    app::ReviewReceipt apply_account_rules(std::string_view,const app::AccountRuleRequest&,const app::OperationContext&) override;
    bool can_undo_account_rules() const override;
    app::ReviewReceipt undo_account_rules(std::string_view) override;
    app::RulePreviewResult preview_pending_rules(const app::RulePreviewRequest&,const app::OperationContext&) const override;
    app::RuleApplyReceipt apply_pending_rules(std::string_view,const app::RuleApplyRequest&,const app::OperationContext&) override;
    std::optional<app::RuleApplyReceipt> rule_apply_outcome(std::string_view) const override;
    app::RuleMatch match_saved_rules(const imports::Occurrence&,const app::OperationContext&) const override;
    app::RuleReapplyPreview preview_rule_reapply(bool,std::string_view,const app::OperationContext&) const override;
    app::RuleReapplyReceipt apply_rule_reapply(std::string_view,const app::RuleReapplyRequest&,const app::OperationContext&) override;
    std::optional<app::RuleReapplyReceipt> rule_reapply_outcome(std::string_view) const override;
    std::string rule_reapply_group(std::string_view) const override;
    void verify(const app::OperationContext* = nullptr) const;
    void backup_to(const std::filesystem::path&,std::string_view password,
                   const BackupObserver& observer = {}, const app::OperationContext* context = nullptr) const;
    void observer(ImportObserver);
    bool transaction_accepted(const imports::Occurrence&) const override;
    app::ImportResult import_result(std::string_view) const override;
    std::vector<app::UndoableImport> undoable_imports() const override;
    app::RollbackReview review_import_undo(std::string_view) const override;
    app::RollbackReceipt undo_import(std::string_view,std::string_view,std::int64_t) override;
    std::optional<app::RollbackReceipt> import_undo_outcome(std::string_view,std::string_view) const override;
    app::RollbackReview rollback_review() const override;
    app::RollbackReceipt rollback_account(std::string_view,std::int64_t) override;
    std::optional<app::RollbackReceipt> rollback_outcome(std::string_view) const override;
    std::vector<app::LinkExclusion> link_exclusions() const override;
    app::LinkExclusionPreview preview_link_exclusion(const app::LinkExclusion&) const override;
    app::ReviewReceipt save_link_exclusion(std::string_view,const app::LinkExclusion&,std::int64_t) override;
    app::ReviewSheet review_sheet() const override;
    app::ReviewReceipt merge_review_batch(std::string_view,const std::vector<app::ReviewAnswer>&,const app::OperationContext&) override;
    app::ReviewReceipt answer_review(std::string_view,const app::ReviewAnswer&) override;
    app::ReviewReceipt assign_categories(std::string_view,const app::CategorySelection&,const app::OperationContext&) override;
    app::ReviewReceipt assign_transfer_batch(std::string_view,const std::vector<app::TransferRequest>&,const app::OperationContext&) override;
    bool can_undo_transfer_assignment() const override;
    std::int64_t ledger_edit_generation() const override;
    app::DeletionPreview preview_deletion(const app::LedgerSelectionEdit&,const app::OperationContext&) override;
    app::ReviewReceipt edit_transactions(std::string_view,const app::LedgerSelectionEdit&,const app::OperationContext&) override;
    app::ReviewReceipt edit_transaction(std::string_view,const imports::Occurrence&,std::int64_t,const std::optional<app::EntryToken>&,const imports::DraftRow&) override;
    bool can_undo_transaction_edit(std::string_view) const override;
    app::ReviewReceipt undo_transaction_edit(std::string_view,std::string_view) override;
    app::ReviewReceipt undo_review(std::string_view) override;
    std::optional<app::ReviewReceipt> review_outcome(std::string_view) const override;
    app::AcceptanceReview acceptance_review() const override;
    app::AcceptanceReceipt accept_all(std::string_view,bool=false) override;
    std::optional<app::AcceptanceReceipt> acceptance_outcome(std::string_view) const override;
private:
    std::vector<std::string> import_link_undos(std::string_view) const;
    app::RollbackReceipt undo_linked_import(std::string_view,std::string_view,const app::RollbackReview&,const std::vector<std::string>&);
    app::RollbackReceipt rollback_scope(std::string_view,std::int64_t,std::string_view);
    std::vector<imports::Occurrence> duplicate_candidates(const qif::Record&,std::string_view,bool first_only) const;
    app::Receipt publish_review(std::string_view,std::int64_t,std::string_view);
    void accept_local(std::string_view,const app::TransferRequest&);
    void prepare_review();
    void finish(const std::function<void()>&);
    void require_account_name(std::string_view,std::int64_t) const;
    std::optional<std::string> transfer_name(std::string_view) const;
    bool review_link_category(std::string_view,std::int64_t counterpart_account) const;
    void require_qif_transfers(const imports::Occurrence&,const imports::DraftRow&,const std::set<std::tuple<std::string,std::size_t,std::string>>&) const;
    void resolve_group_transfers(std::string_view,const app::OperationContext&);
    app::ImportGroup import_group(std::string_view) const;
    void require_group_ready(std::string_view) const;
    std::set<std::string> financial_group_batches(std::int64_t) const;
    void publish_group_opening(std::int64_t);
    const std::vector<qif::CategoryDefinition>& source_categories(std::string_view batch) const;
    imports::DraftRow resolve_repaired_categories(std::string_view,const imports::DraftRow&) const;
    void stage_import_categories(std::string_view,const app::OperationContext&);
    void finish_import_categories(std::string_view,const app::OperationContext&);
    void publish_import_categories(std::string_view batch);
    imports::DraftRow bind_import_categories(std::string_view batch,const imports::DraftRow&) const;
    void start_import_groups(const std::set<std::string>&,std::string_view);
    void verify_import_groups(const app::OperationContext*) const;
    std::vector<app::TransferView> all_transfers() const;
    Money local_transfer_total(std::int64_t,bool projected) const;
    std::vector<app::LedgerRow> local_transfer_ledger(bool) const;
    void verify_transfers(const app::OperationContext*) const;
    void require_unlinked(const imports::Occurrence&) const;
    bool transfer_linked(const imports::Occurrence&) const;
    struct DeletionPlan;
    DeletionPlan plan_deletion(const app::LedgerSelectionEdit&,const app::OperationContext&) const;
    app::ReviewReceipt delete_transactions(std::string_view,const app::LedgerSelectionEdit&,const app::OperationContext&);
    app::ChangePreview preview_change(const app::ChangeRequest&,bool links,std::optional<Money> balance={}) const;
    struct EntryLinkPlan {
        std::vector<app::TransferView> changed;
        std::string relationship;
        std::string stamp;
    };
    EntryLinkPlan plan_entry_links(const std::vector<app::TransferView>&,const imports::Occurrence&,const imports::DraftRow&,const imports::DraftRow&,bool remove) const;
    EntryLinkPlan plan_restore(std::string_view) const;
    bool later_relationship_blocks(const imports::Occurrence&,std::string_view) const;
    void write_entry_links(std::string_view,std::int64_t,app::TransferAction,std::int64_t,const std::vector<app::TransferView>&);
    Money transfer_import_transition(std::string_view,std::string_view,bool);
    Money transfer_import_adjustment(std::string_view) const;
    std::set<std::size_t> protected_import_rows(std::string_view) const;
    app::Account account_by_id(std::int64_t) const;
    std::vector<app::BatchInfo> batches_for(std::int64_t) const;
    Money committed_for(std::int64_t) const;
    Money balance_for(std::int64_t) const;
    std::vector<app::LedgerRow> ledger_rows(bool,bool) const;
    std::optional<Money> projected_for(std::int64_t) const;
    app::Account account_for_batch(std::string_view) const;
    void require_account(std::string_view) const;
    app::ReviewReceipt write_review_answer(std::string_view,const app::ReviewAnswer&,const app::ReviewSheet&,std::string_view parent={});
    void restore_review_answer(std::string_view operation,std::string_view answer);
    struct ReviewRestore {std::int64_t prior,result;std::string child;};
    void write_review_row(std::string_view,std::string_view,const imports::Occurrence&,const imports::DraftRow&,const imports::Decision&,bool,bool=false,bool=true,std::optional<std::int64_t> = {},std::optional<Money> = {},Money* = nullptr,const app::CommittedEntry* = nullptr,const ReviewRestore* = nullptr);
    bool review_draft_undo(const imports::Occurrence&,std::int64_t,const imports::DraftRow&) const;
    imports::DraftRow review_draft(const imports::Occurrence&,std::int64_t) const;
    void generate_explicit_transfers(app::OperationContext* context=nullptr);
    app::ReviewSheet build_review_sheet(const std::vector<app::LinkExclusion>&) const;
    void verify_link_exclusions() const;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit ImportStore(std::unique_ptr<Impl>);
    imports::DraftRow canonical_draft(const imports::Occurrence&,const imports::DraftRow&,const std::optional<imports::DraftRow>&,std::int64_t,bool entry,bool check_assignment=true) const;
    void initialize_categories();
    void write_category_change(std::string_view,const app::CategoryPreview&);
    void verify_categories(const app::OperationContext* context) const;
    app::EntrySnapshot entry_snapshot(const imports::Occurrence&,std::int64_t,const qif::Record* = nullptr) const;
    std::vector<app::DependencyStamp> dependencies(const imports::Occurrence&,std::int64_t history_before=0) const;
    void verify_committed(const app::OperationContext* = nullptr) const;
    void verify_rules(const app::OperationContext* = nullptr) const;
    void verify_bulk(const app::OperationContext* = nullptr) const;
    void verify_rule_reapply(const app::OperationContext* = nullptr) const;
    void verify_account_commands(const app::OperationContext* = nullptr) const;
    app::Account command_account(bool money_edit) const;
    void populate(std::string_view,std::string_view,const qif::Result&,std::int64_t,const app::OperationContext*);
    app::RowView load_row(const imports::Occurrence&, bool candidates) const;
    static ImportStore connect(const std::filesystem::path&,std::string_view,bool,const app::OperationContext* = nullptr,bool overwrite=false,const std::filesystem::path& cache={});
};
} // namespace spentit::storage
