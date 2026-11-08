#include <spentit/import_review.hpp>
#include <spentit/statements.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string>
using namespace spentit;
namespace fs = std::filesystem;
namespace {
void check(bool value, const std::source_location where = std::source_location::current()) {
    if(!value) throw std::runtime_error("Statement assertion failed at line " + std::to_string(where.line()));
}
const Currency cad("CAD");
std::string sgml(std::string body, std::string version = "102") {
    return "OFXHEADER:100\nDATA:OFXSGML\nVERSION:" + version + "\nSECURITY:NONE\nENCODING:USASCII\nCHARSET:1252\nCOMPRESSION:NONE\nOLDFILEUID:NONE\nNEWFILEUID:NONE\n\n<OFX>\n" + body + "</OFX>\n";
}
std::string bank(std::string txns, std::string cur = "CAD", std::string acct = "1001", std::string ledger = "") {
    const auto bal = ledger.empty() ? std::string{} : "<LEDGERBAL>\n<BALAMT>" + ledger + "\n<DTASOF>20260202\n</LEDGERBAL>\n";
    return "<BANKMSGSRSV1>\n<STMTTRNRS>\n<TRNUID>1\n<STATUS>\n<CODE>0\n<SEVERITY>INFO\n</STATUS>\n<STMTRS>\n<CURDEF>" + cur +
        "\n<BANKACCTFROM>\n<BANKID>999\n<ACCTID>" + acct + "\n<ACCTTYPE>CHECKING\n</BANKACCTFROM>\n<BANKTRANLIST>\n" + txns + "</BANKTRANLIST>\n" + bal +
        "</STMTRS>\n</STMTTRNRS>\n</BANKMSGSRSV1>\n";
}
std::string txn(std::string fit, std::string date, std::string amount, std::string name, std::string type = "DEBIT", std::string extra = "") {
    return "<STMTTRN>\n<TRNTYPE>" + type + "\n<DTPOSTED>" + date + "\n<TRNAMT>" + amount + "\n<FITID>" + fit + "\n<NAME>" + name + "\n" + extra + "</STMTTRN>\n";
}
std::string fixture_bytes() {
    const auto path = fs::path(__FILE__).parent_path().parent_path() / "fixtures/statements/card-sgml.aso";
    std::ifstream input(path, std::ios::binary);
    check(static_cast<bool>(input));
    return {std::istreambuf_iterator<char>(input), {}};
}
qif::Record basis_row(std::string date, std::optional<Money> amount, std::string fit, std::string format) {
    qif::Record row; row.repairable = true; row.date = std::move(date); row.source_currency = cad; row.original_amount = amount; row.fitid = std::move(fit); row.format = std::move(format); row.payee = "Coffee"; return row;
}
void statement_headers() {
    // Exercise both grammars and both statement types with the same cases.
    for(bool xml : {false, true}) for(bool credit : {false, true}) {
        const auto leaf=[&](std::string name,std::string value) {
            return "<"+name+">"+value+(xml?"</"+name+">":"\n");
        };
        const auto statement=[&](std::string status,std::string identity,std::string currency,std::string balance) {
            const std::string messages=credit?"CREDITCARDMSGSRSV1":"BANKMSGSRSV1";
            const std::string response=credit?"CCSTMTTRNRS":"STMTTRNRS";
            const std::string body=credit?"CCSTMTRS":"STMTRS";
            const std::string account=credit?"CCACCTFROM":"BANKACCTFROM";
            return "<"+messages+"><"+response+"><STATUS>"+status+"</STATUS><"+body+">"+currency+
                "<"+account+">"+identity+"</"+account+"><BANKTRANLIST>"+leaf("DTSTART","20260101")+"</BANKTRANLIST>"+balance+
                "</"+body+"></"+response+"></"+messages+">";
        };
        const auto inspect=[&](std::string body) {
            const auto bytes=xml?std::string("<?xml version=\"1.0\"?>\n<?OFX OFXHEADER=\"200\" VERSION=\"203\" SECURITY=\"NONE\" OLDFILEUID=\"NONE\" NEWFILEUID=\"NONE\"?>\n<OFX>")+body+"</OFX>":sgml(body);
            return statements::inspect(bytes,"ofx");
        };
        const auto status=leaf("CODE","0"), currency=leaf("CURDEF","CAD");
        const auto identity=leaf("BANKID","999")+leaf("ACCTID","1001")+leaf("ACCTTYPE","CHECKING");
        const auto balance=[&](std::string value){return "<LEDGERBAL>"+leaf("BALAMT",value)+"</LEDGERBAL>";};
        const auto refused=[&](const statements::Index& index,std::string_view message){
            check(index.diagnostic.has_value() && index.accounts.empty() && index.refusal.find(message)!=std::string::npos);
        };
        for(const auto& value : {"10.00","-10.00","+0.00","92233720368547758.07","-92233720368547758.08"}) {
            const auto index=inspect(statement(status,identity,currency,balance(value)));
            if(!index.refusal.empty()) throw std::runtime_error(std::string(xml?"XML: ":"SGML: ")+value+": "+index.refusal);
            check(index.accounts.size()==1 && index.accounts[0].reported_balance==value);
            check(qif::amount(*index.accounts[0].reported_balance,cad)==Money::parse(value,cad));
        }
        for(const auto& value : {"1,234.56","1.001","nonsense","","92233720368547758.08"}) {
            const auto bad=statement(status,identity,currency,balance(value));
            refused(inspect(bad),"balance could not be read");
            // A later invalid statement must also discard earlier account mappings.
            refused(inspect(statement(status,identity,currency,balance("10.00"))+bad),"balance could not be read");
        }
        const auto missing=inspect(statement(status,identity,currency,""));
        check(missing.refusal.empty() && missing.accounts.size()==1 && !missing.accounts[0].reported_balance);
        const auto suppressed=inspect(statement(status,identity,currency,balance("10.00")+"<BALLIST>"+leaf("BAL","1.00")+"</BALLIST>"));
        check(suppressed.refusal.empty() && suppressed.accounts.size()==1 && !suppressed.accounts[0].reported_balance);
        for(const auto& field : {"BANKID","ACCTID","ACCTTYPE"}) {
            refused(inspect(statement(status,identity+leaf(field,"different"),currency,balance("10.00"))),"conflicting fields");
            refused(inspect(statement(status,leaf(field,"")+identity,currency,balance("10.00"))),"conflicting fields");
        }
        refused(inspect(statement(status,identity,currency+leaf("CURDEF","USD"),balance("10.00"))),"conflicting fields");
        refused(inspect(statement(status,identity,currency,balance("10.00")+balance("20.00"))),"conflicting fields");
        refused(inspect(statement(status+leaf("CODE","1"),identity,currency,"")),"conflicting fields");
        refused(inspect(statement(leaf("CODE","1")+status,identity,currency,"")),"conflicting fields");
        refused(inspect(statement(leaf("CODE","1"),identity,currency,"")),"Statement status is not success");
        const auto identical=inspect(statement(status+status,identity+identity,currency+currency,balance("10.00")+balance("10.00")));
        check(identical.refusal.empty() && identical.accounts.size()==1 && identical.accounts[0].account_number=="1001" && identical.accounts[0].institution_id=="999");
    }
}
void parser() {
    const auto amp_sgml=statements::parse_occurrence(txn("AMP","20261028","-1.00","Coffee & Food"),statements::parser_sgml,cad);
    check(amp_sgml.records.size()==1 && amp_sgml.records[0].payee=="Coffee & Food");
    const auto amp_xml=statements::parse_occurrence("<STMTTRN><TRNTYPE>DEBIT</TRNTYPE><DTPOSTED>20261028</DTPOSTED><TRNAMT>-1.00</TRNAMT><FITID>AMP</FITID><NAME>Coffee &amp; Food</NAME></STMTTRN>",statements::parser_xml,cad);
    check(amp_xml.records.size()==1 && amp_xml.records[0].payee=="Coffee & Food");
    const auto coffee = txn("FIT-1", "20260201120000.000[-5:EST]", "-12.34", "Coffee");
    const auto text = sgml(bank(coffee, "CAD", "1001", "10.00"));
    const auto original = text;
    const auto index = statements::inspect(text, "qfx");
    check(text == original && index.refusal.empty() && !index.diagnostic && index.admissible);
    check(index.format == "qfx" && index.grammar == "ofx-sgml" && index.parser == statements::parser_sgml && index.explicit_accounts);
    check(index.accounts.size() == 1 && index.accounts[0].name == "999 1001" && index.accounts[0].currency_code == "CAD");
    check(index.accounts[0].reported_balance && *index.accounts[0].reported_balance == "10.00");
    check(index.header.find("CHARSET:1252") != std::string::npos && index.header.find("<OFX>") == std::string::npos);
    auto parsed = statements::parse_mapped(text, index, 0, qif::DateOrder::mdy, cad);
    check(parsed.records.size() == 1 && !parsed.diagnostic);
    const auto& row = parsed.records[0];
    check(row.date == "2026-02-01" && row.time_evidence == "120000.000[-5:EST]" && row.fitid == "FIT-1" && row.payee == "Coffee" && row.trntype == "DEBIT");
    check(row.format == "qfx" && row.grammar == "ofx-sgml" && row.source_account_key == "999\t1001");
    check(row.original_amount && row.original_amount->units() == -1234 && row.transaction && row.transaction->allocations().size() == 1);
    check(row.transaction->allocations()[0].identity == "source-0" && row.transaction->effective_total() == *row.original_amount);
    const auto slice = std::string_view(text).substr(row.begin, row.end - row.begin);
    auto again = statements::parse_occurrence(slice, statements::parser_sgml, cad);
    check(again.records.size() == 1 && again.records[0].fitid == "FIT-1" && again.records[0].date == "2026-02-01" && again.records[0].original_amount->units() == -1234);
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<?OFX OFXHEADER="200" VERSION="203" SECURITY="NONE" OLDFILEUID="NONE" NEWFILEUID="NONE"?>
<OFX>
<CREDITCARDMSGSRSV1><CCSTMTTRNRS><TRNUID>1</TRNUID><STATUS><CODE>0</CODE><SEVERITY>INFO</SEVERITY></STATUS>
<CCSTMTRS><CURDEF>CAD</CURDEF><CCACCTFROM><ACCTID>4400</ACCTID></CCACCTFROM><BANKTRANLIST>
<CCSTMTTRN><TRNTYPE>DEBIT</TRNTYPE><DTPOSTED>20260201</DTPOSTED><TRNAMT>-12.34</TRNAMT><FITID>FIT-1</FITID><NAME>Coffee</NAME></CCSTMTTRN>
</BANKTRANLIST></CCSTMTRS></CCSTMTTRNRS></CREDITCARDMSGSRSV1></OFX>)";
    const auto card = statements::inspect(xml, "qbo");
    check(card.format == "qbo" && card.grammar == "ofx-xml" && card.parser == statements::parser_xml && card.accounts.size() == 1 && card.accounts[0].name == "4400");
    auto card_rows = statements::parse_mapped(xml, card, 0, qif::DateOrder::dmy, cad);
    check(card_rows.records.size() == 1 && card_rows.records[0].fitid == "FIT-1" && card_rows.records[0].grammar == "ofx-xml");
    const auto sniffed = statements::inspect(text, "");
    check(sniffed.format == "ofx" && sniffed.grammar == "ofx-sgml");
    const auto labeled = statements::inspect(text, "qif");
    check(!labeled.refusal.empty() && labeled.accounts.empty());
    const auto aso_label = statements::inspect(text, "aso");
    check(aso_label.refusal.empty() && aso_label.format == "aso" && aso_label.grammar == "ofx-sgml" && aso_label.accounts.size() == 1 && text == original);
    const auto card_bytes = fixture_bytes();
    check(card_bytes.find("USERKEY") == std::string::npos && card_bytes.find("SECURITY:TYPE1\r\n") != std::string::npos && card_bytes.find("CHARSET:1252\r\n") != std::string::npos);
    const auto card_original = card_bytes;
    const auto aso_card = statements::inspect(card_bytes, "aso");
    check(card_bytes == card_original && aso_card.refusal.empty() && !aso_card.diagnostic && aso_card.admissible);
    check(aso_card.format == "aso" && aso_card.grammar == "ofx-sgml" && aso_card.parser == statements::parser_sgml && aso_card.accounts.size() == 1);
    check(aso_card.accounts[0].name == "4400" && aso_card.accounts[0].currency_code == "CAD" && aso_card.accounts[0].reported_balance && *aso_card.accounts[0].reported_balance == "10.00");
    check(aso_card.header.find("TYPE1") != std::string::npos && aso_card.header.find("USERKEY") == std::string::npos && aso_card.header.find("<OFX>") == std::string::npos);
    check(aso_card.notes.find("USERKEY") == std::string::npos);
    auto aso_rows = statements::parse_mapped(card_bytes, aso_card, 0, qif::DateOrder::mdy, cad);
    check(aso_rows.records.size() == 2 && !aso_rows.diagnostic);
    check(aso_rows.records[0].date == "2026-01-15" && aso_rows.records[0].time_evidence == "143000[-5:EST]" && aso_rows.records[0].fitid == "SYNTH-DEBIT-A");
    check(aso_rows.records[0].payee == "Coffee" && aso_rows.records[0].trntype == "DEBIT" && aso_rows.records[0].format == "aso" && aso_rows.records[0].grammar == "ofx-sgml");
    check(aso_rows.records[0].original_amount && aso_rows.records[0].original_amount->units() == -123 && aso_rows.records[0].memo.empty());
    check(aso_rows.records[1].date == "2026-01-16" && aso_rows.records[1].time_evidence == "120000[0:GMT]" && aso_rows.records[1].fitid == "SYNTH-CREDIT-B");
    check(aso_rows.records[1].payee == "Refund" && aso_rows.records[1].trntype == "CREDIT" && aso_rows.records[1].original_amount->units() == 400);
    const auto sniffed_card = statements::inspect(card_bytes, "");
    check(sniffed_card.format == "ofx" && sniffed_card.grammar == "ofx-sgml");
    std::string with_key = card_bytes;
    const auto language = with_key.find("<LANGUAGE>ENG\r\n");
    check(language != std::string::npos);
    with_key.insert(language, "<USERKEY>synthetic-signon-token\r\n");
    const auto keyed = statements::inspect(with_key, "aso");
    check(keyed.refusal.empty() && keyed.accounts.size() == 1 && keyed.header.find("synthetic-signon-token") == std::string::npos && keyed.notes.find("synthetic-signon-token") == std::string::npos);
    auto keyed_rows = statements::parse_mapped(with_key, keyed, 0, qif::DateOrder::dmy, cad);
    check(keyed_rows.records.size() == 2 && keyed_rows.records[0].uninterpreted.find("synthetic-signon-token") == std::string::npos && keyed_rows.records[1].uninterpreted.find("synthetic-signon-token") == std::string::npos);
    check(keyed_rows.records[0].fitid == "SYNTH-DEBIT-A" && card_bytes.find("USERKEY") == std::string::npos);
    const std::string not_ofx = "not an OFX statement\n";
    const auto refused_aso = statements::inspect(not_ofx, "aso");
    check(!refused_aso.refusal.empty() && refused_aso.accounts.empty() && refused_aso.format == "aso" && not_ofx == "not an OFX statement\n");
    auto secret_text = text;
    const auto none = secret_text.find("SECURITY:NONE");
    check(none != std::string::npos);
    secret_text.replace(none, std::string("SECURITY:NONE").size(), "SECURITY:SECRET");
    const auto secret = statements::inspect(secret_text, "aso");
    check(!secret.refusal.empty() && secret.accounts.empty());
    auto compressed = text;
    const auto compression = compressed.find("COMPRESSION:NONE");
    check(compression != std::string::npos);
    compressed.replace(compression, std::string("COMPRESSION:NONE").size(), "COMPRESSION:GZIP");
    const auto gzip = statements::inspect(compressed, "ofx");
    check(!gzip.refusal.empty() && gzip.accounts.empty());
    std::string xml_aso = xml;
    const auto security = xml_aso.find("SECURITY=\"NONE\"");
    check(security != std::string::npos);
    xml_aso.replace(security, std::string("SECURITY=\"NONE\"").size(), "SECURITY=\"TYPE1\"");
    const auto aso_xml = statements::inspect(xml_aso, "aso");
    check(aso_xml.refusal.empty() && aso_xml.format == "aso" && aso_xml.grammar == "ofx-xml" && aso_xml.parser == statements::parser_xml && aso_xml.accounts.size() == 1);
    const auto investment = statements::inspect(sgml("<INVSTMTMSGSRSV1><INVSTMTTRNRS></INVSTMTTRNRS></INVSTMTMSGSRSV1>\n"), "ofx");
    check(investment.refusal.find("Unsupported OFX") != std::string::npos && investment.accounts.empty());
    const auto mixed = statements::inspect(sgml(bank(coffee) + "<SECLISTMSGSRSV1></SECLISTMSGSRSV1>\n"), "ofx");
    check(!mixed.refusal.empty() && mixed.accounts.empty());
    const auto bad_currency = statements::inspect(sgml(bank(coffee, "USD")), "ofx");
    check(bad_currency.refusal.empty() && bad_currency.accounts.size() == 1 && bad_currency.accounts[0].currency_code == "USD");
    auto refused = statements::parse_mapped(sgml(bank(coffee, "USD")), bad_currency, 0, qif::DateOrder::dmy, cad);
    check(refused.records.empty() && refused.diagnostic && refused.diagnostic->issue == qif::Issue::currency_mismatch);
    auto missing_cur = statements::inspect(sgml("<BANKMSGSRSV1><STMTTRNRS><TRNUID>1<STATUS><CODE>0<SEVERITY>INFO</STATUS><STMTRS><BANKACCTFROM><BANKID>9<ACCTID>1<ACCTTYPE>CHECKING</BANKACCTFROM><BANKTRANLIST></BANKTRANLIST></STMTRS></STMTTRNRS></BANKMSGSRSV1>\n"), "ofx");
    check(missing_cur.refusal.find("currency") != std::string::npos && missing_cur.accounts.empty());
    const auto repeated_cur = statements::inspect(sgml("<BANKMSGSRSV1><STMTTRNRS><TRNUID>1<STATUS><CODE>0<SEVERITY>INFO</STATUS><STMTRS><CURDEF>CAD<CURDEF>USD<BANKACCTFROM><BANKID>9<ACCTID>1<ACCTTYPE>CHECKING</BANKACCTFROM><BANKTRANLIST></BANKTRANLIST></STMTRS></STMTTRNRS></BANKMSGSRSV1>\n"), "ofx");
    check(repeated_cur.refusal.find("conflicting fields") != std::string::npos && repeated_cur.accounts.empty());
    const auto empty_list = statements::inspect(sgml(bank("")), "ofx");
    check(empty_list.refusal.empty() && !empty_list.diagnostic && empty_list.accounts.size() == 1);
    check(statements::parse_mapped(sgml(bank("")), empty_list, 0, qif::DateOrder::dmy, cad).records.empty());
    const auto malformed = statements::inspect("OFXHEADER:100\nDATA:OFXSGML\nVERSION:102\n\n<OFX><BANKMSGSRSV1>", "ofx");
    check(!malformed.refusal.empty() && malformed.accounts.empty() && malformed.refusal.find("No bank") == std::string::npos);
    auto missing_amount = statements::parse_mapped(sgml(bank("<STMTTRN>\n<TRNTYPE>DEBIT\n<DTPOSTED>20260201\n<FITID>FIT-1\n<NAME>Coffee\n</STMTTRN>\n")), statements::inspect(sgml(bank("<STMTTRN>\n<TRNTYPE>DEBIT\n<DTPOSTED>20260201\n<FITID>FIT-1\n<NAME>Coffee\n</STMTTRN>\n")), "ofx"), 0, qif::DateOrder::dmy, cad);
    check(missing_amount.records.size() == 1 && missing_amount.records[0].repairable && !missing_amount.records[0].original_amount && missing_amount.records[0].diagnostic->issue == qif::Issue::amount);
    const auto bad_date_text = sgml(bank(txn("FIT-1", "20260231", "-1.00", "Coffee")));
    auto bad_date = statements::parse_mapped(bad_date_text, statements::inspect(bad_date_text, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(bad_date.records.size() == 1 && bad_date.records[0].repairable && bad_date.records[0].date.empty() && bad_date.records[0].diagnostic->issue == qif::Issue::date);
    const auto no_fit = sgml(bank("<STMTTRN>\n<TRNTYPE>DEBIT\n<DTPOSTED>20260201\n<TRNAMT>-1.00\n<NAME>Coffee\n</STMTTRN>\n"));
    auto absent = statements::parse_mapped(no_fit, statements::inspect(no_fit, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(absent.records.size() == 1 && absent.records[0].transaction && absent.records[0].fitid.empty() && absent.records[0].uninterpreted.find("FITID absent") != std::string::npos);
    const auto foreign = sgml(bank(txn("FIT-1", "20260201", "-1.00", "Coffee", "DEBIT", "<ORIGCURRENCY><CURSYM>USD</ORIGCURRENCY>\n")));
    auto blocked_foreign = statements::parse_mapped(foreign, statements::inspect(foreign, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(blocked_foreign.records.size() == 1 && !blocked_foreign.records[0].repairable && !blocked_foreign.records[0].original_amount && blocked_foreign.records[0].diagnostic->issue == qif::Issue::financial_blocker);
    const auto moved = sgml(bank(txn("FIT-1", "20260201", "-1.00", "Coffee", "XFER", "<BANKACCTTO>\n<BANKID>1\n<ACCTID>2\n<ACCTTYPE>CHECKING\n</BANKACCTTO>\n")));
    auto transfer = statements::parse_mapped(moved, statements::inspect(moved, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(transfer.records.size() == 1 && !transfer.records[0].repairable && transfer.records[0].diagnostic->issue == qif::Issue::transfer);
    const auto other_cur = sgml(bank(txn("FIT-1", "20260201", "-1.00", "Coffee", "DEBIT", "<CURRENCY><CURSYM>USD</CURRENCY>\n")));
    auto txn_cur = statements::parse_mapped(other_cur, statements::inspect(other_cur, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(txn_cur.records.size() == 1 && txn_cur.records[0].diagnostic->issue == qif::Issue::currency_mismatch && !txn_cur.records[0].original_amount);
    const auto held = sgml(bank(txn("FIT-1", "20260201", "-1.00", "Coffee", "HOLD")));
    check(statements::parse_mapped(held, statements::inspect(held, "ofx"), 0, qif::DateOrder::dmy, cad).records[0].diagnostic->issue == qif::Issue::financial_blocker);
    std::string bytes = text; bytes[10] = static_cast<char>(0xff);
    const auto encoding = statements::inspect(bytes, "ofx");
    check(!encoding.refusal.empty() && encoding.accounts.empty() && bytes[10] == static_cast<char>(0xff));
    const auto two = sgml(bank(coffee, "CAD", "1001") + bank(txn("FIT-2", "20260201", "-1.00", "Tea"), "CAD", "2002"));
    const auto both = statements::inspect(two, "ofx");
    check(both.accounts.size() == 2 && both.accounts[0].account_number == "1001" && both.accounts[1].account_number == "2002");
    const auto extra_bal = sgml(bank(coffee, "CAD", "1001", "10.00") );
    auto with_ballist = sgml("<BANKMSGSRSV1><STMTTRNRS><TRNUID>1<STATUS><CODE>0<SEVERITY>INFO</STATUS><STMTRS><CURDEF>CAD<BANKACCTFROM><BANKID>999<ACCTID>1001<ACCTTYPE>CHECKING</BANKACCTFROM><BANKTRANLIST>" + txn("FIT-1", "20260201", "-1.00", "Coffee") + "</BANKTRANLIST><LEDGERBAL><BALAMT>10.00</LEDGERBAL><BALLIST>\n<BAL>1.00\n</BALLIST></STMTRS></STMTTRNRS></BANKMSGSRSV1>\n");
    const auto ballist = statements::inspect(with_ballist, "ofx");
    check(ballist.refusal.empty() && ballist.accounts.size() == 1 && !ballist.accounts[0].reported_balance);
    (void)extra_bal;
    const auto repeats = sgml(bank(txn("FIT-1", "20260201", "-1.00", "Coffee") + txn("FIT-2", "20260201", "-1.00", "Coffee")));
    auto repeated = statements::parse_mapped(repeats, statements::inspect(repeats, "ofx"), 0, qif::DateOrder::dmy, cad);
    check(repeated.records.size() == 2 && repeated.records[0].fitid == "FIT-1" && repeated.records[1].fitid == "FIT-2");
    const Money amount(-1234, cad);
    const auto qif_row = basis_row("2026-02-01", amount, "", "qif");
    const auto same = basis_row("2026-02-01", amount, "FIT-1", "ofx");
    const auto later = basis_row("2026-02-02", amount, "FIT-1", "qfx");
    const auto other_amount = basis_row("2026-02-01", Money(-1299, cad), "FIT-1", "qbo");
    const auto other_id = basis_row("2026-02-01", amount, "FIT-2", "ofx");
    const auto qif_other_day = basis_row("2026-02-02", amount, "", "qif");
    const auto missing = basis_row("2026-02-01", std::nullopt, "FIT-1", "ofx");
    check(imports::reimport_key(qif_row)==imports::reimport_key(same));
    check(imports::reimport_key(qif_row)!=imports::reimport_key(later));
    check(imports::reimport_key(qif_row)!=imports::reimport_key(other_amount));
    check(!imports::reimport_key(missing));
    auto exact=qif_row;exact.payee.clear();check(!imports::reimport_key(exact));
    exact=qif_row;exact.source_currency=Currency("USD");
    check(imports::reimport_key(exact)!=imports::reimport_key(qif_row));
    exact=qif_row;exact.payee+=" ";check(imports::reimport_key(exact)!=imports::reimport_key(qif_row));
    check(imports::candidate_basis(qif_row, same) == imports::MatchBasis::same_source_fields);
    check(imports::candidate_basis(same, later) == imports::MatchBasis::same_fitid);
    check(imports::candidate_basis(same, other_amount) == imports::MatchBasis::uncertain_fitid);
    check(imports::candidate_basis(same, missing) == imports::MatchBasis::uncertain_fitid);
    check(!imports::candidate_basis(same, other_id));
    check(!imports::candidate_basis(qif_row, later));
    check(!imports::candidate_basis(qif_row, qif_other_day));
    const auto aso_other_day = basis_row("2026-02-02", amount, "", "aso");
    const auto aso_fit = basis_row("2026-02-02", amount, "FIT-1", "aso");
    check(!imports::candidate_basis(qif_row, aso_other_day));
    check(imports::candidate_basis(same, aso_fit) == imports::MatchBasis::same_fitid);
    check(imports::basis_label(imports::MatchBasis::same_fitid).find("bank transaction ID") != std::string::npos);
    // Holding internal facts constant, every format pairing has identical policy.
    for(const auto left:{"qif","ofx","qfx","qbo","aso"})for(const auto right:{"qif","ofx","qfx","qbo","aso"}) {
        auto a=basis_row("2026-02-01",amount,"",left);
        auto b=basis_row("2026-02-01",amount,"",right);
        check(imports::candidate_basis(a,b)==imports::MatchBasis::same_source_fields);
        b.payee="Different export description";check(imports::candidate_basis(a,b)==imports::MatchBasis::same_date_amount);
        for(const auto date:{"2026-01-31","2026-02-02","2010-01-01"}){b.date=date;check(!imports::candidate_basis(a,b));check(imports::candidate_basis(a,b,imports::MatchContext::confirmed_decision)==imports::MatchBasis::uncertain_date);}
        a.fitid=b.fitid="BANK-1";a.trntype=b.trntype="DEBIT";
        check(imports::candidate_basis(a,b)==imports::MatchBasis::same_fitid);check(!imports::same_bank_transaction(a,b));
        b.date=a.date;check(imports::same_bank_transaction(a,b));
        b.fitid="BANK-2";check(!imports::candidate_basis(a,b));check(!imports::same_bank_transaction(a,b));
        b.fitid.clear();b.associated_fitids={"BANK-2","BANK-3"};check(!imports::candidate_basis(a,b));
        b.associated_fitids.push_back("BANK-1");check(imports::candidate_basis(a,b)==imports::MatchBasis::same_fitid);
        check(!imports::same_bank_transaction(a,b));
    }
    // Synthetic decision table: format and text do not change bank identity;
    // contradictory financial facts and unavailable identities never auto-match.
    for(const auto label:{"ofx","qfx","qbo","aso"}) {
        const auto posted=sgml(bank(txn("STRICT-1","20260201","-4.25","Cafe & Books")));
        const auto exported=sgml(bank(txn("STRICT-1","20260201","-4.25","Cafe &amp; Books")));
        const auto a=statements::parse_mapped(posted,statements::inspect(posted,"ofx"),0,qif::DateOrder::dmy,cad).records.at(0);
        auto b=statements::parse_mapped(exported,statements::inspect(exported,label),0,qif::DateOrder::dmy,cad).records.at(0);
        b.payee="Cafe &amp; Books";b.memo="Export description";
        check(imports::same_bank_transaction(a,b));
        b.date="2026-02-02";check(!imports::same_bank_transaction(a,b));b.date=a.date;
        b.trntype="CREDIT";check(!imports::same_bank_transaction(a,b));b.trntype=a.trntype;
        b.original_amount=Money(-426,cad);check(!imports::same_bank_transaction(a,b));b.original_amount=a.original_amount;
        b.source_currency=Currency("USD");check(!imports::same_bank_transaction(a,b));b.source_currency=cad;
        b.fitid="STRICT-2";check(!imports::same_bank_transaction(a,b));b.fitid.clear();
        b.associated_fitid=a.fitid;check(!imports::same_bank_transaction(a,b));
        check(!imports::same_bank_transaction(a,qif_row));
    }
}
}
int main() {
    try { statement_headers(); parser(); std::cout << "Statement parser passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
