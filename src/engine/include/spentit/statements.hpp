#pragma once
#include <spentit/qif.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace spentit::statements {
inline constexpr std::string_view parser_sgml = "ofx-sgml-1";
inline constexpr std::string_view parser_xml = "ofx-xml-2";

// One statement account inside an OFX-family file, or one QIF account section.
// name is the evidence label used for explicit mapping. It is not an account identity.
struct Account {
    std::string name, type;
    std::optional<std::string> reported_balance;
    std::string institution_id, account_number, account_type, currency_code;
    std::size_t begin = 0, end = 0;
};

struct Index {
    std::string parser, format, grammar, header, notes, refusal;
    bool explicit_accounts = false;
    bool admissible = true;
    std::optional<qif::Diagnostic> diagnostic;
    std::vector<Account> accounts;
    std::vector<qif::CategoryDefinition> categories;
};

// label is qif, ofx, qfx, qbo, aso, or empty to sniff.
// aso keeps that file label and then uses the SGML or XML parser the body requires.
// An unlabeled OFX body is stored as ofx. Parsing does not modify the caller's bytes or any file.
Index inspect(std::string_view bytes, std::string_view label, const std::function<void()>& checkpoint = {});
qif::Result parse_mapped(std::string_view bytes, const Index& index, std::size_t account, qif::DateOrder order,
                         const Currency& currency, const std::function<void()>& checkpoint = {});
// Reparse one stored occurrence slice. QIF uses the existing record parser.
qif::Result parse_occurrence(std::string_view slice, std::string_view parser, const Currency& currency,
                             qif::DateOrder order = qif::DateOrder::dmy);
} // namespace spentit::statements
