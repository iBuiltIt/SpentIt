#pragma once
#include <spentit/import_review.hpp>
#include <spentit/review_commands.hpp>
#include <spentit/operation_context.hpp>
#include <spentit/committed_changes.hpp>
#include <spentit/category_catalog.hpp>
#include <spentit/rule_commands.hpp>
#include <spentit/transfer_commands.hpp>
#include <functional>
#include <optional>
#include <utility>

namespace spentit::app {
enum class LedgerSelectionAction { rename_payee, restore_imported };
struct LedgerSelectionEdit {
    LedgerSelectionAction action=LedgerSelectionAction::rename_payee;
    std::int64_t account=0, generation=0;
    std::vector<imports::Occurrence> selected;
    std::string payee;
};
struct DatasetOptions {std::string date_format="YYYY-MM-DD";bool default_categories=false;};
// Synchronous application persistence boundary. Call on a worker, never the UI
// thread. Each command is one complete atomic operation including its history.
struct Account {
    std::string name;
    Currency currency;
    std::string opening_date;
    Money opening;
    bool frozen;
    std::int64_t revision;
    bool opening_confirmed;
    std::int64_t identity=1;
    bool archived=false;
};
// Read-only copy of one account's committed and projected totals. Money has no
// default value, so construct this only with all three fields.
struct AccountAlias { std::int64_t account; std::string name; };
struct AccountBalance {
    std::int64_t identity=0;
    Money committed;
    std::optional<Money> projected;
};
// Explicit account command receipt. Retry returns this stored after-image.
struct AccountCommandReceipt {
    std::string operation;
    int kind=0;
    std::int64_t identity=0;
    std::string name;
    Currency currency;
    std::string opening_date;
    Money opening;
    bool archived=false;
    bool retired=false;
    std::int64_t revision=0;
    bool frozen=false;
    bool opening_confirmed=false;
};
struct AccountCreation {
    std::int64_t identity;
    std::string name;
    Currency currency;
    std::string date;
};
enum class BatchState { pending, committed, discarded, captured, protected_drafts };
struct BatchInfo {
    std::string id;
    qif::DateOrder profile;
    BatchState state;
    std::int64_t revision;
    std::size_t count;
    bool blocked;
    bool processing_incomplete=false;
    bool completed=false;
};
struct ImportAccountMapping {
    std::size_t source_account;
    std::int64_t account;
    std::optional<AccountCreation> creation;
    std::optional<bool> use_reported_opening={}; // Explicit creation-time choice; absent for legacy staged review.
};
enum class GroupState { captured, reviewing, started, aborted };
enum class ReportedBalanceChoice { unreviewed, zero, opening, closing, ignore, fixed_zero, fixed_reported };
struct ImportGroupAccount {
    std::size_t source_account;
    std::int64_t account;
    std::string batch;
    bool created;
    ReportedBalanceChoice balance_choice;
    std::optional<Money> reviewed_change, proposed_opening;
    std::string opening_date;
};
struct ImportCounterpart {
    std::int64_t account;
    std::string batch;
    std::size_t ordinal;
    std::string allocation,date;
    Money amount;
    std::string status;
};
struct ImportBlocker {
    std::int64_t account;
    std::string batch;
    std::optional<std::size_t> ordinal;
    std::string allocation, reason;
    std::vector<ImportCounterpart> candidates={};
    std::size_t candidate_count=0;
    bool overridable=false;
};
struct ImportGroup {
    std::string id;
    GroupState state;
    qif::DateOrder profile;
    std::vector<ImportGroupAccount> accounts;
    std::size_t repairs_total=0, repairs_done=0, unresolved=0, blocked_records=0, amount_repairs=0;
    std::size_t unresolved_transfers=0;
    std::vector<ImportBlocker> blockers={};
};
struct MissingImportCategories { std::string source; std::vector<std::string> names; };
struct ImportRepairProgress {std::size_t total=0,done=0,amounts=0,blocked=0;};
struct RowView {
    imports::Occurrence identity;
    qif::Record source;
    std::optional<imports::DraftRow> draft;
    imports::Decision decision;
    std::vector<imports::Occurrence> candidates;
    bool review_stale=false;
    bool excluded_by_clear=false;
    bool matched_generated=false;
};
struct Receipt {
    std::string operation, batch;
    std::int64_t revision;
    std::size_t included, excluded;
    Money before, change, after;
};
struct ImportResult {std::size_t added=0,already_present=0,unresolved=0,failed=0;};
struct RollbackReview {
    std::int64_t token=0;
    std::size_t transactions=0,failures=0;
    std::vector<std::int64_t> affected_accounts;
};
struct UndoableImport {
    std::string source,format,first_date,last_date;
    std::int64_t order=0;
    std::size_t records=0;
    std::vector<std::int64_t> accounts;
};
struct RollbackReceipt {std::string operation;std::int64_t account=0;std::size_t transactions=0,failures=0;};
struct AcceptanceReview {
    std::size_t transactions=0,failed_records=0;
    std::vector<ImportBlocker> warnings;
};
struct AcceptanceReceipt {
    std::string operation;
    std::size_t transactions=0,failed_records=0;
    bool ignored_warnings=false;
};
struct LedgerRow {
    imports::Occurrence identity;
    std::int64_t sequence;
    imports::DraftRow draft;
    bool committed;
};
class ImportRepository {
public:
    virtual ~ImportRepository() = default;
    virtual bool transfers_supported() const { return false; }
    virtual std::int64_t transfer_sequence() const { throw std::logic_error("Transfers unavailable"); }
    virtual std::vector<TransferView> transfers() const { return {}; }
    virtual TransferReceipt apply_transfer(std::string_view,const TransferRequest&) { throw std::logic_error("Transfers unavailable"); }
    virtual TransferPreview preview_transfer(const TransferRequest&) const { throw std::logic_error("Transfers unavailable"); }
    virtual std::optional<TransferReceipt> transfer_outcome(std::string_view) const { throw std::logic_error("Transfers unavailable"); }
    virtual std::vector<TransferJournalEntry> transfer_journal() const { return {}; }
    virtual Account account() const = 0;
    // Serial-worker session selection; commands never span accounts implicitly.
    virtual std::vector<Account> accounts() const { return {account()}; }
    virtual std::vector<AccountAlias> account_aliases() const { std::vector<AccountAlias> names;for(const auto& a:accounts())names.push_back({a.identity,a.name});return names; }
    virtual std::optional<std::int64_t> resolve_account_name(std::string_view name) const {
        std::optional<std::int64_t> found;
        for(const auto& alias:account_aliases())if(account_name_matches(alias.name,name)) {
            if(found && *found!=alias.account)throw std::invalid_argument("Account name or alias is ambiguous; review the account mapping");
            found=alias.account;
        }
        return found;
    }
    virtual std::int64_t next_account_identity() const { std::int64_t last=0;for(const auto& a:accounts())if(a.identity>last)last=a.identity;if(last==INT64_MAX)throw std::overflow_error("Account identities exhausted");return last+1; }
    virtual void select_account(std::int64_t id) { if(id!=account().identity) throw std::logic_error("Unknown account"); }
    virtual Account create_account(std::int64_t, std::string_view, const Currency&, std::string_view) {
        throw std::logic_error("Multiple accounts are not supported by this dataset");
    }
    virtual std::optional<AccountCreation> account_creation_outcome(std::int64_t) const {
        throw std::logic_error("Account creation outcome is unavailable");
    }
    // expected revision prevents stale opening-balance edits; frozen after commit.
    virtual void opening(std::int64_t expected, std::string_view date, const Money&) = 0;
    virtual bool account_selected() const { return true; }
    virtual void release_account_selection() { throw std::logic_error("Account selection is unavailable"); }
    virtual std::vector<Account> archived_accounts() const { return {}; }
    virtual AccountCommandReceipt rename_account(std::string_view,std::string_view) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt archive_account(std::string_view) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt reactivate_account(std::string_view) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt delete_account(std::string_view) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt correct_opening(std::string_view,std::string_view,const Money&) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt reassign_currency(std::string_view,const Currency&) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual AccountCommandReceipt undo_account_operation(std::string_view,std::string_view) { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual std::optional<AccountCommandReceipt> account_command_outcome(std::string_view) const { throw std::logic_error("Account changes are not supported by this dataset"); }
    virtual std::optional<std::string> undoable_account_operation() const { return {}; }
    virtual bool account_currency_warning() const { return false; }
    virtual std::vector<BatchInfo> batches() const = 0;
    virtual std::vector<MissingImportCategories> missing_import_categories() const {return {};}
    virtual void resolve_import_categories(std::string_view,const std::vector<std::string>&,OperationContext&) {throw std::logic_error("Import category decisions unavailable");}
    virtual std::vector<ImportGroup> import_groups() const { return {}; }
    virtual std::optional<ImportGroup> import_group_outcome(std::string_view id) const {for(const auto& g:import_groups())if(g.id==id)return g;return {};}
    virtual ImportGroup prepare_import_group(std::string_view,std::string_view,qif::DateOrder,
                                             const std::vector<ImportAccountMapping>&,OperationContext&,std::string_view ="qif") {
        throw std::logic_error("Grouped imports unavailable");
    }
    virtual ImportGroup resume_import_group(std::string_view,OperationContext&) { throw std::logic_error("Grouped imports unavailable"); }
    // Removes only durably captured, unprepared imports; completed imports are untouched.
    virtual bool cleanup_incomplete_imports(std::string_view = {}) { throw std::logic_error("Incomplete import cleanup unavailable"); }
    virtual bool startup_import_cleanup() const { return false; }
    virtual void abort_import_group(std::string_view) { throw std::logic_error("Grouped imports unavailable"); }
    virtual void resolve_import_transfers(std::string_view,app::OperationContext&) { throw std::logic_error("Mapped transfer resolution unavailable"); }
    virtual void review_import_balance(std::string_view,std::size_t,std::int64_t,ReportedBalanceChoice,std::string_view) {
        throw std::logic_error("Grouped imports unavailable");
    }
    // Same identity+bytes+profile is retry; different content is a conflict.
    // Identical pending bytes/profile returns the existing pending batch.
    virtual BatchInfo ingest(std::string_view id, std::string_view bytes, qif::DateOrder) = 0;
    // Controlled ingestion may durably retain evidence before interpretations.
    // Existing retained input is returned without implicitly resuming it.
    virtual BatchInfo ingest_controlled(std::string_view id,std::string_view bytes,qif::DateOrder profile,OperationContext& context) {
        context.seal(); return ingest(id,bytes,profile);
    }
    virtual BatchInfo resume(std::string_view,std::int64_t,OperationContext&) {
        throw std::logic_error("This document does not support resuming retained evidence");
    }
    virtual std::string evidence(std::string_view batch) const = 0;
    virtual qif::Result parsed_source(std::string_view id,const std::function<void()>& checkpoint = {}) const {
        const auto all=batches();
        for(const auto& b:all)if(b.id==id)return qif::parse(evidence(id),b.profile,account().currency,checkpoint);
        throw std::logic_error("Import does not exist");
    }
    // Exact immutable bytes for one occurrence, not a regenerated QIF record.
    virtual std::string record_evidence(const imports::Occurrence& id) const {
        const auto parsed=parsed_source(id.batch);
        if(id.ordinal>=parsed.records.size())throw std::out_of_range("Source occurrence is unavailable");
        const auto& record=parsed.records[id.ordinal];const auto bytes=evidence(id.batch);
        return bytes.substr(record.begin,record.end-record.begin);
    }
    virtual RowView row(const imports::Occurrence&) const = 0;
    virtual ImportRepairProgress repair_progress(std::string_view id) const {
        ImportRepairProgress result;const auto source=parsed_source(id);if(source.diagnostic)++result.blocked;
        for(std::size_t i=0;i<source.records.size();++i){const auto& r=source.records[i];if(r.repairable){++result.total;if(r.amount_repair)++result.amounts;if(row({std::string(id),i}).draft)++result.done;}else if(r.diagnostic)++result.blocked;}
        return result;
    }
    // A repairable source may initially have no draft. Its first validated save
    // preserves original evidence and creates an unresolved duplicate decision.
    // Unsupported diagnostics and legacy schemas must reject such a save.
    virtual void save(const imports::Occurrence&, std::int64_t expected_revision, const imports::DraftRow&) = 0;
    virtual void decide(const imports::Occurrence&, std::int64_t expected_revision, const imports::Decision&) = 0;
    virtual void discard(std::string_view batch, std::int64_t expected_revision) = 0;
    // All included rows, decisions, immutable history, and state transition commit
    // together. operation+batch+revision identifies content frozen at that revision.
    virtual Receipt commit(std::string_view batch, std::int64_t expected_revision, std::string_view operation) = 0;
    virtual std::optional<Receipt> outcome(std::string_view operation) const = 0;
    virtual bool transaction_accepted(const imports::Occurrence&) const {return false;}
    virtual ImportResult import_result(std::string_view) const {return {};}
    virtual std::vector<UndoableImport> undoable_imports() const {return {};}
    virtual RollbackReview review_import_undo(std::string_view) const {throw std::logic_error("Import undo unavailable");}
    virtual RollbackReceipt undo_import(std::string_view,std::string_view,std::int64_t) {throw std::logic_error("Import undo unavailable");}
    virtual std::optional<RollbackReceipt> import_undo_outcome(std::string_view,std::string_view) const {return {};}
    virtual RollbackReview rollback_review() const {return {};}
    virtual RollbackReceipt rollback_account(std::string_view,std::int64_t) {throw std::logic_error("Rollback unavailable");}
    virtual std::optional<RollbackReceipt> rollback_outcome(std::string_view) const {return {};}
    virtual std::vector<LinkExclusion> link_exclusions() const {return {};}
    virtual LinkExclusionPreview preview_link_exclusion(const LinkExclusion&) const {throw std::logic_error("Link exclusions unavailable");}
    virtual ReviewReceipt save_link_exclusion(std::string_view,const LinkExclusion&,std::int64_t) {throw std::logic_error("Link exclusions unavailable");}
    virtual ReviewSheet review_sheet() const {return {};}
    virtual ReviewReceipt merge_review_batch(std::string_view,const std::vector<ReviewAnswer>&,const OperationContext&) {throw std::logic_error("Batch Review unavailable");}
    virtual ReviewReceipt answer_review(std::string_view,const ReviewAnswer&) {throw std::logic_error("Review unavailable");}
    virtual ReviewReceipt assign_transfer_batch(std::string_view,const std::vector<TransferRequest>&,const OperationContext&) {throw std::logic_error("Transfer assignment unavailable");}
    virtual bool can_undo_transfer_assignment() const {return false;}
    // Complete editor acceptance and exact, stacked reversal. The UI owns the
    // session capability; the durable journal owns snapshots and mutation gates.
    virtual std::int64_t ledger_edit_generation() const {return 0;}
    virtual ReviewReceipt edit_transactions(std::string_view,const LedgerSelectionEdit&,const OperationContext&) {throw std::logic_error("Selected transaction editing unavailable");}
    virtual ReviewReceipt edit_transaction(std::string_view,const imports::Occurrence&,std::int64_t,const std::optional<EntryToken>&,const imports::DraftRow&) {throw std::logic_error("Transaction editing unavailable");}
    virtual bool can_undo_transaction_edit(std::string_view) const {return false;}
    virtual ReviewReceipt undo_transaction_edit(std::string_view,std::string_view) {throw std::logic_error("Transaction edit Undo unavailable");}
    virtual ReviewReceipt undo_review(std::string_view) {throw std::logic_error("Review Undo unavailable");}
    virtual std::optional<ReviewReceipt> review_outcome(std::string_view) const {return {};}
    virtual AcceptanceReview acceptance_review() const {return {};}
    virtual AcceptanceReceipt accept_all(std::string_view,bool=false) {throw std::logic_error("Dataset acceptance unavailable");}
    virtual std::optional<AcceptanceReceipt> acceptance_outcome(std::string_view) const {return {};}

    // Only a durable, transaction-free read may resolve an uncertain acknowledgement.
    // Storage overrides this to reject an unclosed/failed rollback transaction.
    virtual std::optional<Receipt> durable_outcome(std::string_view operation) const { return outcome(operation); }
    virtual std::vector<LedgerRow> ledger(bool projected) const = 0;
    // Normal saved ledger, including uncommitted rows. Import failures are
    // reported separately and never suppress successfully imported rows.
    virtual std::vector<LedgerRow> current_ledger() const { return ledger(true); }
    virtual Money balance() const {
        std::vector<Money> amounts{account().opening};
        for(const auto& row:current_ledger())amounts.push_back(row.draft.transaction.effective_total());
        return imports::sum(account().currency,amounts);
    }
    virtual std::vector<std::pair<std::int64_t,Money>> balances() const { return {}; }
    virtual std::string date_format() const {return "YYYY-MM-DD";}
    virtual bool categories_supported() const { return false; }
    virtual CategoryCatalog category_catalog() const { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual CategoryPreview preview_category_change(const CategoryRequest&) const { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual CategoryReceipt apply_category_change(std::string_view,const CategoryRequest&,std::int64_t) { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual std::optional<CategoryReceipt> category_change_outcome(std::string_view) const { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual std::vector<CategoryReceipt> category_history(std::string_view) const { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual CategoryDisplay resolve_category(std::string_view) const { throw std::logic_error("Categories are not supported by this dataset"); }
    virtual bool committed_changes_supported() const { return false; }
    virtual CommittedEntry committed_entry(const imports::Occurrence&) const { throw std::logic_error("Committed changes require a new dataset"); }
    virtual std::vector<CommittedHistoryItem> committed_history(const imports::Occurrence&) const { throw std::logic_error("Committed history is unavailable"); }
    // Descending operation sequence; before_sequence=0 starts at newest. Limit 1..100.
    virtual std::vector<CommittedChangeSummary> committed_changes(std::int64_t before_sequence=0,std::size_t limit=50) const { (void)before_sequence;(void)limit;throw std::logic_error("Committed history is unavailable"); }
    virtual ChangePreview preview_committed_change(const ChangeRequest&) const { throw std::logic_error("Committed changes require a new dataset"); }
    virtual ChangeReceipt apply_committed_change(std::string_view,const ChangeRequest&,const EntryToken&,std::int64_t) { throw std::logic_error("Committed changes require a new dataset"); }
    virtual ChangeReceipt apply_committed_change_with_catalog(std::string_view op,const ChangeRequest& request,const EntryToken& token,std::int64_t sequence,std::int64_t catalog_revision) { if(catalog_revision!=0)throw std::logic_error("Catalog-aware changes unavailable");return apply_committed_change(op,request,token,sequence); }
    virtual std::optional<ChangeReceipt> committed_change_outcome(std::string_view) const { throw std::logic_error("Committed changes require a new dataset"); }
    virtual Money committed_balance() const = 0;
    virtual bool bulk_supported() const { return false; }
    virtual std::string bulk_group(std::string_view) const { return {}; }
    virtual BulkPreview preview_bulk(const BulkRequest&) const { throw std::logic_error("Bulk changes are not supported by this dataset"); }
    virtual BulkReceipt apply_bulk(std::string_view,const BulkPreview&) { throw std::logic_error("Bulk changes are not supported by this dataset"); }
    virtual std::optional<BulkReceipt> bulk_outcome(std::string_view) const { throw std::logic_error("Bulk changes are not supported by this dataset"); }
    // No authoritative projection exists while the pending batch is blocked/unresolved.
    virtual std::optional<Money> projected_balance() const = 0;
    // Same formulas as the selected-account methods, for every live account.
    // Empty when a test double has not forwarded the read.
    virtual std::vector<AccountBalance> account_balances() const { return {}; }
    virtual bool rules_supported() const { return false; }
    virtual RuleImage rule_image() const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual std::vector<RuleTombstone> rule_tombstones() const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual RuleSaveReceipt save_rules(std::string_view,const RuleSaveRequest&,std::int64_t,const OperationContext&) { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual std::optional<RuleSaveReceipt> rule_save_outcome(std::string_view) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual ReviewReceipt apply_account_rules(std::string_view,const AccountRuleRequest&,const OperationContext&) { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual bool can_undo_account_rules() const { return false; }
    virtual ReviewReceipt undo_account_rules(std::string_view) { throw std::logic_error("No rule application to undo"); }
    virtual RulePreviewResult preview_pending_rules(const RulePreviewRequest&,const OperationContext&) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual RuleApplyReceipt apply_pending_rules(std::string_view,const RuleApplyRequest&,const OperationContext&) { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual std::optional<RuleApplyReceipt> rule_apply_outcome(std::string_view) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual RuleMatch match_saved_rules(const imports::Occurrence&,const OperationContext&) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual RuleReapplyPreview preview_rule_reapply(bool,std::string_view,const OperationContext&) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual RuleReapplyReceipt apply_rule_reapply(std::string_view,const RuleReapplyRequest&,const OperationContext&) { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual std::optional<RuleReapplyReceipt> rule_reapply_outcome(std::string_view) const { throw std::logic_error("Rules are not enabled for this dataset"); }
    virtual std::string rule_reapply_group(std::string_view) const { return {}; }
};
} // namespace spentit::app
