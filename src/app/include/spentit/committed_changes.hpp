#pragma once
#include <spentit/import_review.hpp>
#include <optional>
namespace spentit::app {
enum class BatchState;
enum class ChangeKind { publication, correct, remove, undo, transfer };
struct DependencyStamp {
    std::string batch;
    std::int64_t revision;
    BatchState state;
    bool operator==(const DependencyStamp&) const = default;
};
struct EntryToken {
    imports::Occurrence identity;
    std::int64_t revision;
    std::vector<DependencyStamp> dependencies;
    bool operator==(const EntryToken&) const = default;
};
struct EntrySnapshot {
    std::int64_t revision;
    bool active;
    imports::DraftRow draft;
};
struct CommittedEntry {
    imports::Occurrence identity;
    std::int64_t sequence;
    EntrySnapshot current;
    qif::Record source;
    EntryToken token;
};
struct ChangeRequest {
    ChangeKind kind;
    imports::Occurrence identity;
    std::optional<imports::DraftRow> correction;
    std::string undo_of;
};
struct ChangePreview {
    ChangeRequest request;
    EntrySnapshot before_entry,after_entry;
    Money before,change,after;
    EntryToken expected;
    std::int64_t preview_sequence;
    std::int64_t catalog_revision=0;
    // In-memory only. Empty when the correction does not change a transfer link.
    std::string relationship;
    std::string link_stamp;
};
struct ChangeReceipt {
    std::string operation;
    ChangeKind kind;
    imports::Occurrence identity;
    EntrySnapshot before_entry,after_entry;
    Money before,change,after;
    std::string undo_of;
    EntryToken result_token;
    std::int64_t sequence;
};
struct CommittedHistoryItem {
    std::string operation;
    ChangeKind kind;
    std::optional<EntrySnapshot> before_entry;
    EntrySnapshot after_entry;
    Money before,change,after;
    bool undo_eligible;
    std::string blocked_reason;
    std::int64_t sequence;
};
struct CommittedChangeSummary {
    std::string operation;
    ChangeKind kind;
    imports::Occurrence identity;
    std::int64_t sequence;
    bool active;
    std::string date,payee;
    Money total;
};
struct BulkRequest {
    ChangeKind kind;
    std::vector<imports::Occurrence> identities;
    std::string category,undo_of;
    bool operator==(const BulkRequest&) const = default;
};
struct BulkPreview {
    BulkRequest request;
    std::vector<ChangePreview> entries;
    Money before,change,after;
    std::int64_t preview_sequence,catalog_revision;
};
struct BulkReceipt {
    std::string operation;
    BulkRequest request;
    std::vector<ChangeReceipt> entries;
    Money before,change,after;
    std::int64_t preview_sequence,catalog_revision;
};
}
