#pragma once
#include <spentit/import_review.hpp>
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
};
enum class BatchState { pending, committed, discarded };
struct BatchInfo {
    std::string id;
    qif::DateOrder profile;
    BatchState state;
    std::int64_t revision;
    std::size_t count;
    bool blocked;
};
struct RowView {
    imports::Occurrence identity;
    qif::Record source;
    std::optional<imports::DraftRow> draft;
    imports::Decision decision;
    std::vector<imports::Occurrence> candidates;
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
    virtual Account account() const = 0;
    // expected revision prevents stale opening-balance edits; frozen after commit.
    virtual void opening(std::int64_t expected, std::string_view date, const Money&) = 0;
    virtual std::vector<BatchInfo> batches() const = 0;
    // Same identity+bytes+profile is retry; different content is a conflict.
    // Identical pending bytes/profile returns the existing pending batch.
    virtual BatchInfo ingest(std::string_view id, std::string_view bytes, qif::DateOrder) = 0;
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
    virtual std::vector<LedgerRow> ledger(bool projected) const = 0;
    virtual Money committed_balance() const = 0;
    // No authoritative projection exists while the pending batch is blocked/unresolved.
    virtual std::optional<Money> projected_balance() const = 0;
};
} // namespace spentit::app
