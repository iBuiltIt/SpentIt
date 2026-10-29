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
inline constexpr std::string_view parser_version = "bank-v6";
enum class DateOrder { dmy, mdy };
enum class Issue { encoding, resource_limit, empty_file, header, empty_record,
    unterminated, unknown_field, repeated_field, missing_field, date, amount,
    split_structure, split_total, transfer, category_class, cleared_status,
    currency_mismatch, financial_blocker };
enum class SplitProblem { none, missing_amount, repeated_category, repeated_memo, parent_after_split, parent_category };
struct Diagnostic {
    Issue issue;
    std::size_t offset;
    SplitProblem split_problem = SplitProblem::none;
    bool operator==(const Diagnostic&) const = default;
};
std::string_view message(Issue issue) noexcept;
std::string_view message(const Diagnostic&) noexcept;
// Throws invalid_argument without input values for an invalid date or profile.
std::string date(std::string_view text, DateOrder order);
Money amount(std::string_view text, const Currency& currency);
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
    bool amount_repair = false;
    bool has_source_splits = false;
    std::optional<Diagnostic> diagnostic;
    // Source facts. QIF stays qif/qif with an empty FITID. OFX-family imports
    // fill these from the statement; they are not edited ledger fields.
    std::string format = "qif";
    std::string grammar = "qif";
    std::string fitid, source_account_key, time_evidence, trntype, uninterpreted;
    // Not a parsed file field. A confirmed association may carry a FITID that this
    // occurrence's own file did not. Match classification reads it; storage does not write it.
    std::string associated_fitid;
};
// Value-free diagnostics; LF counts lines, embedded CR does not. Ordinal is account-local.
std::string describe_record(std::string_view bytes, const Record&, std::size_t ordinal, bool fields = false);
struct Result {
    std::vector<Record> records;
    std::optional<Diagnostic> diagnostic;
    bool admissible = true; // Resource rejection: do not persist this source.
    bool valid() const noexcept;
};
struct CategoryDefinition {
    std::string name, description, tax_schedule, budget;
    char type=0;
    bool tax_related=false;
    std::size_t begin=0, end=0;
};
Result parse(std::string_view bytes, DateOrder order, const Currency& currency, const std::function<void()>& checkpoint = {},
             const std::vector<CategoryDefinition>& categories = {});
// Offsets always address the unchanged complete source, including account lists.
struct AccountDefinition {
    std::string name, type;
    std::optional<std::string> reported_balance;
    std::string description, credit_limit, statement_date, statement_balance;
    std::size_t begin=0, end=0;
};
struct Section { std::size_t account=0, begin=0, end=0; };
struct Document {
    std::vector<AccountDefinition> accounts;
    std::vector<Section> sections;
    std::vector<CategoryDefinition> categories;
    std::optional<Diagnostic> diagnostic;
    bool admissible=true;
    bool explicit_accounts=false;
};
Document index_document(std::string_view bytes,const std::function<void()>& checkpoint = {});
Result parse_account(std::string_view bytes,const Document&,std::size_t account,
                     DateOrder,const Currency&,const std::function<void()>& checkpoint = {});
} // namespace spentit::qif
