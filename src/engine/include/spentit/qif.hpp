#pragma once
#include <spentit/transaction.hpp>
#include <cstddef>
#include <optional>
#include <string_view>

namespace spentit::qif {
inline constexpr std::size_t max_file = 64 * 1024 * 1024;
inline constexpr std::size_t max_records = 200000;
inline constexpr std::size_t max_record = 1024 * 1024;
inline constexpr std::size_t max_field = 64 * 1024;
inline constexpr std::size_t max_allocations = 1000;
inline constexpr std::string_view parser_version = "bank-v1";
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
bool supported_category(std::string_view text) noexcept;
struct Record {
    std::size_t begin = 0, end = 0;
    std::string date, payee, memo, reference, cleared;
    std::optional<Transaction> transaction;
    std::optional<Diagnostic> diagnostic;
};
struct Result {
    std::vector<Record> records;
    std::optional<Diagnostic> diagnostic;
    bool admissible = true; // Resource rejection: do not persist this source.
    bool valid() const noexcept;
};
Result parse(std::string_view bytes, DateOrder order, const Currency& currency);
} // namespace spentit::qif
