#pragma once
#include <spentit/import_review.hpp>
#include <spentit/operation_context.hpp>
#include <spentit/committed_changes.hpp>
#include <spentit/category_catalog.hpp>
#include <spentit/rule_commands.hpp>
#include <spentit/transfer_commands.hpp>
#include <functional>
#include <optional>

namespace spentit::app {
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
};
struct RowView {
    imports::Occurrence identity;
    qif::Record source;
    std::optional<imports::DraftRow> draft;
    imports::Decision decision;
    std::vector<imports::Occurrence> candidates;
    bool review_stale=false;
    bool excluded_by_clear=false;
};
struct Receipt {
    std::string operation, batch;
    std::int64_t revision;
    std::size_t included, excluded;
    Money before, change, after;
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
    virtual std::optional<TransferReceipt> transfer_outcome(std::string_view) const { throw std::logic_error("Transfers unavailable"); }
    virtual Account account() const = 0;
    // Serial-worker session selection; commands never span accounts implicitly.
    virtual std::vector<Account> accounts() const { return {account()}; }
    virtual void select_account(std::int64_t id) { if(id!=account().identity) throw std::logic_error("Unknown account"); }
    virtual Account create_account(std::int64_t, std::string_view, const Currency&, std::string_view) {
        throw std::logic_error("Multiple accounts require a new dataset");
    }
    virtual std::optional<AccountCreation> account_creation_outcome(std::int64_t) const {
        throw std::logic_error("Account creation outcome is unavailable");
    }
    // expected revision prevents stale opening-balance edits; frozen after commit.
    virtual void opening(std::int64_t expected, std::string_view date, const Money&) = 0;
    virtual std::vector<BatchInfo> batches() const = 0;
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
    virtual RowView row(const imports::Occurrence&) const = 0;
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
    // Only a durable, transaction-free read may resolve an uncertain acknowledgement.
    // Storage overrides this to reject an unclosed/failed rollback transaction.
    virtual std::optional<Receipt> durable_outcome(std::string_view operation) const { return outcome(operation); }
    virtual std::vector<LedgerRow> ledger(bool projected) const = 0;
    virtual bool categories_supported() const { return false; }
    virtual CategoryCatalog category_catalog() const { throw std::logic_error("Categories require a new dataset"); }
    virtual CategoryPreview preview_category_change(const CategoryRequest&) const { throw std::logic_error("Categories require a new dataset"); }
    virtual CategoryReceipt apply_category_change(std::string_view,const CategoryRequest&,std::int64_t) { throw std::logic_error("Categories require a new dataset"); }
    virtual std::optional<CategoryReceipt> category_change_outcome(std::string_view) const { throw std::logic_error("Categories require a new dataset"); }
    virtual std::vector<CategoryReceipt> category_history(std::string_view) const { throw std::logic_error("Categories require a new dataset"); }
    virtual CategoryDisplay resolve_category(std::string_view) const { throw std::logic_error("Categories require a new dataset"); }
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
    virtual BulkPreview preview_bulk(const BulkRequest&) const { throw std::logic_error("Bulk changes require a new schema-7 dataset"); }
    virtual BulkReceipt apply_bulk(std::string_view,const BulkPreview&) { throw std::logic_error("Bulk changes require a new schema-7 dataset"); }
    virtual std::optional<BulkReceipt> bulk_outcome(std::string_view) const { throw std::logic_error("Bulk changes require a new schema-7 dataset"); }
    // No authoritative projection exists while the pending batch is blocked/unresolved.
    virtual std::optional<Money> projected_balance() const = 0;
    virtual bool rules_supported() const { return false; }
    virtual RuleImage rule_image() const { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual std::vector<RuleTombstone> rule_tombstones() const { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual RuleSaveReceipt save_rules(std::string_view,const RuleSaveRequest&,std::int64_t,const OperationContext&) { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual std::optional<RuleSaveReceipt> rule_save_outcome(std::string_view) const { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual RulePreviewResult preview_pending_rules(const RulePreviewRequest&,const OperationContext&) const { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual RuleApplyReceipt apply_pending_rules(std::string_view,const RuleApplyRequest&,const OperationContext&) { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual std::optional<RuleApplyReceipt> rule_apply_outcome(std::string_view) const { throw std::logic_error("Rules require a new experimental dataset"); }
    virtual RuleMatch match_saved_rules(const imports::Occurrence&,const OperationContext&) const { throw std::logic_error("Rules require a new experimental dataset"); }
};
} // namespace spentit::app
