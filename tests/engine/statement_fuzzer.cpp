#include <spentit/import_review.hpp>
#include <spentit/statements.hpp>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {
void check(const spentit::qif::Result& result, std::size_t size) {
    if(result.diagnostic && result.diagnostic->offset > size) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
    std::size_t previous = 0;
    for(const auto& record : result.records) {
        if(record.begin < previous || record.begin > record.end || record.end > size) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        previous = record.end;
        if(record.diagnostic && record.diagnostic->offset > size) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        if(record.transaction) {
            if(record.diagnostic || !record.original_amount || record.repairable) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
            const auto& transaction = *record.transaction;
            if(transaction.original_imported_amount() != record.original_amount || transaction.effective_total() != *record.original_amount) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
            std::vector<spentit::Money> parts;
            for(const auto& allocation : transaction.allocations()) parts.push_back(allocation.amount);
            if(spentit::imports::sum(transaction.effective_total().currency(), parts) != transaction.effective_total()) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        }
    }
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if(size > 256 * 1024) return 0;
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    for(const char* label : {"", "ofx", "qfx", "qbo", "qif", "aso"}) {
        const auto index = spentit::statements::inspect(input, label);
        if(index.diagnostic && index.diagnostic->offset > size) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        if(!index.refusal.empty() && !index.accounts.empty()) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        for(const auto& account : index.accounts)
            if(account.begin > account.end || account.end > size) { std::fprintf(stderr, "Harness invariant line %d\n", __LINE__); std::abort(); }
        if(!index.refusal.empty() || !index.admissible || index.diagnostic) continue;
        for(const auto code : {"JPY", "CAD", "KWD"}) {
            const spentit::Currency currency(code);
            for(std::size_t account = 0; account < index.accounts.size(); ++account) {
                const auto parsed = spentit::statements::parse_mapped(input, index, account, spentit::qif::DateOrder::dmy, currency);
                check(parsed, size);
                if(index.parser == spentit::statements::parser_sgml || index.parser == spentit::statements::parser_xml)
                    for(const auto& record : parsed.records)
                        if(record.begin <= record.end && record.end <= size)
                            check(spentit::statements::parse_occurrence(input.substr(record.begin, record.end - record.begin), index.parser, currency), record.end - record.begin);
            }
        }
    }
    return 0;
}
