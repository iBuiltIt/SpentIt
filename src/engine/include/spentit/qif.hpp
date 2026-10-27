#pragma once
#include <spentit/transaction.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>

namespace spentit::qif {
inline constexpr std::size_t max_file = 64 * 1024 * 1024;
inline constexpr std::size_t max_records = 200000;
inline constexpr std::size_t max_record = 1024 * 1024;
inline constexpr std::size_t max_field = 64 * 1024;
inline constexpr std::size_t max_allocations = 1000;
inline constexpr std::string_view parser_version = "bank-v3";
enum class DateOrder { dmy, mdy };
enum class Issue { encoding, resource_limit, empty_file, header, empty_record,
    unterminated, unknown_field, repeated_field, missing_field, date, amount,
    split_structure, split_total, transfer, category_class, cleared_status };
struct Diagnostic {
    Issue issue;
    std::size_t offset;
    bool operator==(const Diagnostic&) const = default;
};
std::string_view message(Issue issue) noexcept;
// Throws invalid_argument without input values for an invalid date or profile.
std::string date(std::string_view text, DateOrder order);
bool valid_utf8(std::string_view text) noexcept;
std::optional<std::string_view> transfer_destination(std::string_view text) noexcept;
bool supported_category(std::string_view text) noexcept;
struct SourceAllocation {
    std::string category, memo, class_label;
    std::optional<Money> amount;
};
struct Record {
    std::size_t begin = 0, end = 0;
    std::string date, payee, memo, reference, cleared, source_category, source_class;
    // Class labels are source metadata, never category names or postings.
    std::vector<std::string> source_allocation_classes;
    std::vector<SourceAllocation> source_allocation_fields;
    std::optional<Currency> source_currency;
    std::optional<Transaction> transaction;
    // Parsed evidence only: allocations may not balance and are never postings.
    std::optional<Money> original_amount;
    std::vector<Allocation> source_allocations;
    // True only for explicitly repairable date/amount errors with intact structure.
    bool repairable = false;
    bool has_source_splits = false;
    std::optional<Diagnostic> diagnostic;
};
struct Result {
    std::vector<Record> records;
    std::optional<Diagnostic> diagnostic;
    bool admissible = true; // Resource rejection: do not persist this source.
    bool valid() const noexcept;
};
Result parse(std::string_view bytes, DateOrder order, const Currency& currency, const std::function<void()>& checkpoint = {});
} // namespace spentit::qif
