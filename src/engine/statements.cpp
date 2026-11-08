#include <spentit/statements.hpp>
#include <spentit/category.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <utility>

namespace spentit::statements {
namespace {
constexpr std::size_t max_depth = 32;
constexpr std::size_t max_header = 64 * 1024;

struct Txn {
    std::size_t begin = 0, end = 0;
    bool attributes = false, transfer = false, image = false, orig_currency = false, unknown = false;
    std::string currency_sym, payee_name;
    std::map<std::string, std::vector<std::string>> fields;
};
struct Acct {
    bool credit = false;
    std::size_t begin = 0, end = 0;
    std::string bank_id, acct_id, acct_type, currency, ledger, status;
    bool suppress_balance = false, in_txn = false;
    std::set<std::string> seen_fields;
    Txn txn;
    std::vector<Txn> txns;
};
struct Built {
    std::vector<Acct> accounts;
    std::string refusal, notes, signon;
    std::optional<qif::Diagnostic> diagnostic;
    bool admissible = true, stop = false;
};
struct Builder {
    Built& out;
    std::vector<std::string> stack;
    Acct* current = nullptr;
    bool in_signon = false, slice = false;
    std::string slice_currency;
    explicit Builder(Built& built): out(built) {}
    void fail(qif::Issue issue, std::size_t at, std::string refusal, bool resource = false) {
        if(out.stop) return;
        out.stop = true; out.refusal = std::move(refusal); out.diagnostic = qif::Diagnostic{issue, at};
        if(resource) out.admissible = false;
        out.accounts.clear(); current = nullptr;
    }
    void statement_field(const std::string& name, const std::string& value,
                         std::string& stored, std::size_t at) {
        if(!current->seen_fields.insert(name).second && stored != value) {
            fail(qif::Issue::header, at,
                 "Statement contains conflicting fields; no import was saved. Obtain a corrected export and try again.");
            return;
        }
        stored = value;
    }
    [[nodiscard]] bool extra_balance(std::string_view name) const {
        return name != "LEDGERBAL" && name != "AVAILBAL" && name != "BALAMT" && name.find("BAL") != name.npos;
    }
    void open(std::string name, std::size_t at, std::string_view attrs) {
        if(out.stop) return;
        if(stack.size() >= max_depth) { fail(qif::Issue::resource_limit, at, "Import resource limit exceeded", true); return; }
        const std::string parent = stack.empty() ? std::string{} : stack.back();
        if(parent == "OFX" && name != "SIGNONMSGSRSV1" && name != "BANKMSGSRSV1" && name != "CREDITCARDMSGSRSV1") {
            fail(qif::Issue::header, at, "Unsupported OFX message set; no transactions were imported"); return;
        }
        if((parent == "BANKMSGSRSV1" && name != "STMTTRNRS") || (parent == "CREDITCARDMSGSRSV1" && name != "CCSTMTTRNRS")) {
            fail(qif::Issue::header, at, "Unsupported OFX message set; no transactions were imported"); return;
        }
        if((parent == "STMTTRNRS" && name != "TRNUID" && name != "STATUS" && name != "STMTRS" && name != "CLTCOOKIE") ||
           (parent == "CCSTMTTRNRS" && name != "TRNUID" && name != "STATUS" && name != "CCSTMTRS" && name != "CLTCOOKIE")) {
            fail(qif::Issue::header, at, "Malformed OFX; no import was saved"); return;
        }
        if(parent == "STMTRS" || parent == "CCSTMTRS") {
            const bool known = name == "CURDEF" || name == "BANKACCTFROM" || name == "CCACCTFROM" || name == "BANKTRANLIST" ||
                name == "LEDGERBAL" || name == "AVAILBAL" || name == "MKTGINFO" || extra_balance(name);
            if(!known) { fail(qif::Issue::header, at, "Malformed OFX; no import was saved"); return; }
        }
        if(parent == "BANKTRANLIST" && name != "DTSTART" && name != "DTEND" && name != "STMTTRN" && name != "CCSTMTTRN") {
            fail(qif::Issue::header, at, "Malformed OFX; no import was saved"); return;
        }
        if(name == "OFX" && !attrs.empty()) out.notes = "OFX root attributes: " + std::string(attrs);
        if(name == "STMTTRNRS" || name == "CCSTMTTRNRS") {
            if(out.accounts.size() == 1000) { fail(qif::Issue::resource_limit, at, "Import resource limit exceeded", true); return; }
            out.accounts.push_back({}); current = &out.accounts.back(); current->credit = name == "CCSTMTTRNRS"; current->begin = at;
        }
        if(name == "STMTTRN" || name == "CCSTMTTRN") {
            if(!current && slice) {
                out.accounts.push_back({}); current = &out.accounts.back(); current->currency = slice_currency; current->status = "0"; current->acct_id = "occurrence";
            }
            if(!current) { fail(qif::Issue::header, at, "Malformed OFX; no import was saved"); return; }
            if(current->txns.size() == qif::max_records) { fail(qif::Issue::resource_limit, at, "Import resource limit exceeded", true); return; }
            current->in_txn = true; current->txn = Txn{}; current->txn.begin = at; current->txn.attributes = !attrs.empty();
        }
        if(current && extra_balance(name)) current->suppress_balance = true;
        if(name == "SIGNONMSGSRSV1") in_signon = true;
        stack.push_back(std::move(name));
    }
    void pop_only() {
        if(out.stop) return;
        if(stack.empty()) { fail(qif::Issue::unterminated, 0, "Malformed OFX; no import was saved"); return; }
        stack.pop_back();
    }
    void close(std::string_view name, std::size_t end) {
        if(out.stop) return;
        if(stack.empty() || stack.back() != name) { fail(qif::Issue::unterminated, end, "Malformed OFX; no import was saved"); return; }
        stack.pop_back();
        if(name == "SIGNONMSGSRSV1") in_signon = false;
        if((name == "STMTTRN" || name == "CCSTMTTRN") && current && current->in_txn) {
            current->txn.end = end; current->txns.push_back(std::move(current->txn)); current->in_txn = false;
        }
        if((name == "STMTTRNRS" || name == "CCSTMTTRNRS") && current) { current->end = end; current = nullptr; }
    }
    void leaf(std::string name, std::string value, std::size_t at) {
        if(out.stop) return;
        // USERKEY is credential-like. It stays in the original bytes and is not copied into fields, notes, or diagnostics.
        if(name == "USERKEY") {
            if(value.size() > qif::max_field) { fail(qif::Issue::resource_limit, at, "Import resource limit exceeded", true); return; }
            return;
        }
        if(value.size() > qif::max_field) { fail(qif::Issue::resource_limit, at, "Import resource limit exceeded", true); return; }
        const std::string parent = stack.empty() ? std::string{} : stack.back();
        if(parent == "STATUS" && name == "CODE") {
            bool statement = false;
            for(const auto& frame : stack) if(frame == "STMTTRNRS" || frame == "CCSTMTTRNRS") statement = true;
            if(statement && current && !current->in_txn) statement_field(name, value, current->status, at);
            else if(in_signon) out.signon = value;
            return;
        }
        if(!current) return;
        if(parent == "BANKACCTFROM" || parent == "CCACCTFROM") {
            if(name == "BANKID") statement_field(name, value, current->bank_id, at);
            else if(name == "ACCTID") statement_field(name, value, current->acct_id, at);
            else if(name == "ACCTTYPE") statement_field(name, value, current->acct_type, at);
            return;
        }
        if(parent == "STMTRS" || parent == "CCSTMTRS") {
            if(name == "CURDEF") {
                statement_field(name, value, current->currency, at); return;
            }
            if(name == "MKTGINFO") return;
            fail(qif::Issue::header, at, "Malformed OFX; no import was saved"); return;
        }
        if(parent == "LEDGERBAL" && name == "BALAMT") { statement_field(name, value, current->ledger, at); return; }
        if(parent == "AVAILBAL" || parent == "LEDGERBAL" || parent == "BANKTRANLIST" || parent == "STATUS" || parent == "SONRS" || parent == "FI") return;
        if(!current->in_txn) return;
        if(parent == "BANKACCTTO" || parent == "CCACCTTO") { current->txn.transfer = true; return; }
        if(parent == "IMAGEDATA") { current->txn.image = true; return; }
        if(parent == "ORIGCURRENCY") { current->txn.orig_currency = true; if(name == "CURSYM") current->txn.fields["ORIGCURRENCY"].push_back(std::move(value)); return; }
        if(parent == "CURRENCY" && name == "CURSYM") { current->txn.currency_sym = std::move(value); return; }
        if(parent == "PAYEE" && (name == "NAME" || name == "PAYEE")) { current->txn.payee_name = std::move(value); return; }
        if(parent == "STMTTRN" || parent == "CCSTMTTRN") {
            if(name == "BANKACCTTO" || name == "CCACCTTO") current->txn.transfer = true;
            current->txn.fields[std::move(name)].push_back(std::move(value));
            return;
        }
        current->txn.unknown = true;
        current->txn.fields["UNPARSED"].push_back(name + "=" + value);
    }
};

std::string_view trim(std::string_view text) {
    while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r')) text.remove_prefix(1);
    while(!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' || text.back() == '\r')) text.remove_suffix(1);
    return text;
}
std::size_t skip_bom(std::string_view bytes) { return bytes.starts_with("\xef\xbb\xbf") ? 3 : 0; }
std::size_t skip_ws(std::string_view bytes, std::size_t i) {
    while(i < bytes.size() && (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\n' || bytes[i] == '\r')) ++i;
    return i;
}
bool aggregate_name(std::string_view name) {
    static constexpr std::string_view known[]{
        "OFX", "SIGNONMSGSRSV1", "SONRS", "STATUS", "FI", "BANKMSGSRSV1", "STMTTRNRS", "STMTRS", "BANKACCTFROM",
        "BANKTRANLIST", "STMTTRN", "CCSTMTTRN", "LEDGERBAL", "AVAILBAL", "CURRENCY", "ORIGCURRENCY", "PAYEE",
        "BANKACCTTO", "CCACCTTO", "CREDITCARDMSGSRSV1", "CCSTMTTRNRS", "CCSTMTRS", "CCACCTFROM", "IMAGEDATA", "BALLIST",
        "INVSTMTMSGSRSV1", "SECLISTMSGSRSV1", "BILLPAYMSGSRSV1"};
    return std::find(std::begin(known), std::end(known), name) != std::end(known) || name.ends_with("MSGSRSV1") || name.ends_with("MSGSRQV1");
}
bool looks_sgml(std::string_view bytes) { return bytes.substr(skip_ws(bytes, skip_bom(bytes))).starts_with("OFXHEADER:"); }
bool looks_xml(std::string_view bytes) {
    const auto rest = bytes.substr(skip_ws(bytes, skip_bom(bytes)));
    return rest.starts_with("<?xml") || rest.starts_with("<?OFX") || rest.starts_with("<OFX");
}
std::string normalize_label(std::string_view label) {
    std::string out;
    for(char ch : label) {
        const auto c = static_cast<unsigned char>(ch);
        if(c == ' ' || c == '\t') continue;
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    if(!out.empty() && out.front() == '.') out.erase(out.begin());
    if(out != "qif" && out != "ofx" && out != "qfx" && out != "qbo" && out != "aso") return {};
    return out;
}
bool controls_ok(std::string_view bytes, const std::function<void()>& checkpoint, std::size_t& bad) {
    if(!qif::valid_utf8(bytes)) { bad = 0; return false; }
    for(std::size_t i = 0; i < bytes.size(); ++i) {
        if(checkpoint && i % 4096 == 0) checkpoint();
        const auto c = static_cast<unsigned char>(bytes[i]);
        if((c < 32 && c != '\n' && c != '\t' && c != '\r') || c == 127 || (c == '\r' && (i + 1 == bytes.size() || bytes[i + 1] != '\n'))) { bad = i; return false; }
    }
    return true;
}

struct Header {
    bool xml = false;
    std::string raw;
    std::size_t body = 0;
};
std::optional<Header> read_header(std::string_view bytes, bool xml, std::string& refusal, qif::Diagnostic& diagnostic, bool& admissible) {
    Header head; head.xml = xml;
    const auto start = skip_bom(bytes);
    auto reject = [&](std::string text, std::size_t at, bool resource = false) {
        refusal = std::move(text); diagnostic = qif::Diagnostic{resource ? qif::Issue::resource_limit : qif::Issue::header, at};
        if(resource) admissible = false;
        return std::optional<Header>{};
    };
    std::string security, compression, version, ofxheader, data;
    if(!xml) {
        std::size_t i = start;
        if(!bytes.substr(i).starts_with("OFXHEADER:")) return reject("OFX header is missing or does not match the body; no import was saved", i);
        bool done = false;
        while(i < bytes.size()) {
            if(i - start > max_header) return reject("Import resource limit exceeded", i, true);
            const auto nl = bytes.find('\n', i);
            const auto line_end = nl == bytes.npos ? bytes.size() : nl;
            auto line = bytes.substr(i, line_end - i);
            if(line.ends_with('\r')) line.remove_suffix(1);
            i = nl == bytes.npos ? bytes.size() : nl + 1;
            if(line.empty()) { head.raw = std::string(bytes.substr(0, i)); head.body = i; done = true; break; }
            const auto colon = line.find(':');
            if(colon == line.npos || colon == 0) return reject("OFX header is missing or does not match the body; no import was saved", start);
            const auto key = std::string(line.substr(0, colon)); const auto value = std::string(trim(line.substr(colon + 1)));
            if(key == "OFXHEADER") ofxheader = value; else if(key == "DATA") data = value; else if(key == "VERSION") version = value;
            else if(key == "SECURITY") security = value; else if(key == "COMPRESSION") compression = value;
        }
        if(!done || ofxheader != "100" || data != "OFXSGML" || version.empty()) return reject("OFX header is missing or does not match the body; no import was saved", start);
    } else {
        std::size_t i = skip_ws(bytes, start);
        if(bytes.substr(i).starts_with("<?xml")) {
            const auto end = bytes.find("?>", i);
            if(end == bytes.npos || end - i > max_header) return reject("OFX header is missing or does not match the body; no import was saved", i);
            i = skip_ws(bytes, end + 2);
        }
        if(!bytes.substr(i).starts_with("<?OFX")) return reject("OFX header is missing or does not match the body; no import was saved", i);
        const auto end = bytes.find("?>", i);
        if(end == bytes.npos || end - i > max_header) return reject("OFX header is missing or does not match the body; no import was saved", i);
        const auto pi = bytes.substr(i, end - i);
        auto attr = [&](std::string_view key) {
            const auto token = std::string(key) + "=\""; const auto at = pi.find(token); if(at == pi.npos) return std::string{};
            const auto from = at + token.size(); const auto to = pi.find('"', from); if(to == pi.npos) return std::string{};
            return std::string(pi.substr(from, to - from));
        };
        ofxheader = attr("OFXHEADER"); version = attr("VERSION"); security = attr("SECURITY"); compression = attr("COMPRESSION");
        if(ofxheader != "200" || version.empty()) return reject("OFX header is missing or does not match the body; no import was saved", i);
        head.raw = std::string(bytes.substr(0, end + 2)); head.body = end + 2;
    }
    const bool security_ok = security.empty() || security == "NONE" || security == "TYPE1";
    const bool compression_ok = compression.empty() || compression == "NONE";
    if(!security_ok || !compression_ok)
        return reject("Encrypted or compressed OFX is not supported; no import was saved", start);
    return head;
}

std::string xml_decode(std::string_view text, bool& ok) {
    std::string out; ok = true;
    for(std::size_t i = 0; i < text.size(); ++i) {
        if(text[i] != '&') { out.push_back(text[i]); continue; }
        const auto semi = text.find(';', i);
        if(semi == text.npos) { ok = false; return {}; }
        const auto token = text.substr(i + 1, semi - i - 1);
        auto put = [&](unsigned value) {
            if(value == 0 || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) { ok = false; return; }
            if(value < 0x80) out.push_back(static_cast<char>(value));
            else if(value < 0x800) { out.push_back(static_cast<char>(0xc0 | (value >> 6))); out.push_back(static_cast<char>(0x80 | (value & 0x3f))); }
            else if(value < 0x10000) { out.push_back(static_cast<char>(0xe0 | (value >> 12))); out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (value & 0x3f))); }
            else { out.push_back(static_cast<char>(0xf0 | (value >> 18))); out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f))); out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (value & 0x3f))); }
        };
        if(token == "amp") out.push_back('&');
        else if(token == "lt") out.push_back('<');
        else if(token == "gt") out.push_back('>');
        else if(token == "quot") out.push_back('"');
        else if(token == "apos") out.push_back('\'');
        else if(!token.empty() && token.front() == '#') {
            unsigned value = 0; const auto digits = token.substr(1);
            const bool hex = !digits.empty() && (digits.front() == 'x' || digits.front() == 'X');
            const auto body = hex ? digits.substr(1) : digits;
            if(body.empty()) { ok = false; return {}; }
            for(char ch : body) {
                const auto c = static_cast<unsigned char>(ch);
                unsigned digit = 0;
                if(c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
                else if(hex && c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
                else if(hex && c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
                else { ok = false; return {}; }
                if(value > 0x10ffffu / 16u) { ok = false; return {}; }
                value = hex ? (value << 4u) | digit : value * 10u + digit;
            }
            put(value); if(!ok) return {};
        } else { ok = false; return {}; }
        i = semi;
    }
    return out;
}

bool parse_sgml(std::string_view bytes, std::size_t& pos, Builder& builder, const std::function<void()>& checkpoint, const std::string& stop, int depth) {
    while(pos < bytes.size() && !builder.out.stop) {
        if(checkpoint && pos % 4096 == 0) checkpoint();
        pos = skip_ws(bytes, pos);
        if(pos >= bytes.size()) break;
        if(bytes[pos] != '<') { builder.fail(qif::Issue::header, pos, "Malformed OFX; no import was saved"); return false; }
        if(depth > static_cast<int>(max_depth)) { builder.fail(qif::Issue::resource_limit, pos, "Import resource limit exceeded", true); return false; }
        const auto begin = pos++;
        const bool closing = pos < bytes.size() && bytes[pos] == '/'; if(closing) ++pos;
        const auto name_at = pos;
        while(pos < bytes.size() && bytes[pos] != '>' && bytes[pos] != ' ' && bytes[pos] != '\n' && bytes[pos] != '\r' && bytes[pos] != '\t') ++pos;
        if(pos >= bytes.size() || name_at == pos || bytes[pos] != '>') { builder.fail(qif::Issue::unterminated, begin, "Malformed OFX; no import was saved"); return false; }
        const auto name = std::string(bytes.substr(name_at, pos - name_at)); ++pos;
        if(closing) {
            if(name != stop) { builder.fail(qif::Issue::unterminated, begin, "Malformed OFX; no import was saved"); return false; }
            return true;
        }
        if(aggregate_name(name)) {
            builder.open(name, begin, {});
            if(builder.out.stop) return false;
            if(!parse_sgml(bytes, pos, builder, checkpoint, name, depth + 1)) return false;
            builder.close(name, pos);
        } else {
            const auto value_at = pos;
            while(pos < bytes.size() && bytes[pos] != '<' && bytes[pos] != '\n') ++pos;
            auto value = std::string(trim(bytes.substr(value_at, pos - value_at)));
            if(pos < bytes.size() && bytes[pos] == '\n') ++pos;
            builder.leaf(std::move(name), std::move(value), begin);
        }
    }
    if(!stop.empty() && !builder.out.stop) { builder.fail(qif::Issue::unterminated, pos, "Malformed OFX; no import was saved"); return false; }
    return !builder.out.stop;
}

bool parse_xml(std::string_view bytes, std::size_t pos, Builder& builder, const std::function<void()>& checkpoint) {
    struct Frame { std::string name, text, attrs; bool child = false; std::size_t begin = 0; };
    std::vector<Frame> frames;
    while(pos < bytes.size() && !builder.out.stop) {
        if(checkpoint && pos % 4096 == 0) checkpoint();
        if(bytes[pos] != '<') {
            const auto end = bytes.find('<', pos);
            const auto raw = bytes.substr(pos, (end == bytes.npos ? bytes.size() : end) - pos);
            pos = end == bytes.npos ? bytes.size() : end;
            if(trim(raw).empty()) continue;
            if(frames.empty() || frames.back().child) { builder.fail(qif::Issue::header, pos, "Malformed OFX; no import was saved"); return false; }
            bool ok = true; auto decoded = xml_decode(raw, ok);
            if(!ok) { builder.fail(qif::Issue::encoding, pos, "Malformed OFX; no import was saved"); return false; }
            frames.back().text += decoded; continue;
        }
        const auto begin = pos;
        if(bytes.substr(pos).starts_with("<!--")) {
            const auto end = bytes.find("-->", pos + 4);
            if(end == bytes.npos) { builder.fail(qif::Issue::unterminated, pos, "Malformed OFX; no import was saved"); return false; }
            pos = end + 3; continue;
        }
        if(bytes.substr(pos).starts_with("<!") || bytes.substr(pos).starts_with("<?")) {
            builder.fail(qif::Issue::header, pos, "Malformed OFX; no import was saved"); return false;
        }
        ++pos; const bool closing = pos < bytes.size() && bytes[pos] == '/'; if(closing) ++pos;
        const auto name_at = pos;
        while(pos < bytes.size() && ((bytes[pos] >= 'A' && bytes[pos] <= 'Z') || (bytes[pos] >= 'a' && bytes[pos] <= 'z') || (bytes[pos] >= '0' && bytes[pos] <= '9') || bytes[pos] == '_' || bytes[pos] == ':' || bytes[pos] == '.')) ++pos;
        if(name_at == pos) { builder.fail(qif::Issue::header, begin, "Malformed OFX; no import was saved"); return false; }
        const auto name = std::string(bytes.substr(name_at, pos - name_at));
        std::string attrs;
        while(pos < bytes.size() && bytes[pos] != '>' && bytes[pos] != '/') {
            attrs.push_back(bytes[pos]); ++pos;
            if(attrs.size() > qif::max_field) { builder.fail(qif::Issue::resource_limit, begin, "Import resource limit exceeded", true); return false; }
        }
        bool empty = false;
        if(pos < bytes.size() && bytes[pos] == '/') { empty = true; ++pos; }
        if(pos >= bytes.size() || bytes[pos] != '>') { builder.fail(qif::Issue::unterminated, begin, "Malformed OFX; no import was saved"); return false; }
        ++pos;
        if(closing) {
            if(empty || frames.empty() || frames.back().name != name) { builder.fail(qif::Issue::unterminated, begin, "Malformed OFX; no import was saved"); return false; }
            auto frame = std::move(frames.back()); frames.pop_back();
            if(frame.child) {
                if(!trim(frame.text).empty()) { builder.fail(qif::Issue::header, begin, "Malformed OFX; no import was saved"); return false; }
                builder.close(name, pos);
            } else { builder.pop_only(); builder.leaf(name, std::string(trim(frame.text)), frame.begin); }
            continue;
        }
        if(!frames.empty()) frames.back().child = true;
        builder.open(name, begin, trim(attrs));
        if(builder.out.stop) return false;
        if(empty) { builder.pop_only(); builder.leaf(name, {}, begin); continue; }
        frames.push_back(Frame{name, {}, std::string(trim(attrs)), false, begin});
    }
    if(!frames.empty() && !builder.out.stop) { builder.fail(qif::Issue::unterminated, pos, "Malformed OFX; no import was saved"); return false; }
    return !builder.out.stop;
}

const std::string* field(const Txn& txn, const char* key, bool& conflict) {
    const auto found = txn.fields.find(key);
    if(found == txn.fields.end() || found->second.empty()) return nullptr;
    for(std::size_t i = 1; i < found->second.size(); ++i) if(found->second[i] != found->second[0]) conflict = true;
    return &found->second[0];
}
bool known_type(std::string_view type) {
    static constexpr std::string_view types[]{
        "CREDIT", "DEBIT", "INT", "DIV", "FEE", "SRVCHG", "DEP", "ATM", "POS", "XFER", "CHECK", "PAYMENT", "CASH",
        "DIRECTDEP", "DIRECTDEBIT", "REPEATPMT", "HOLD", "OTHER"};
    return std::find(std::begin(types), std::end(types), type) != std::end(types);
}
bool calendar_date(std::string_view text, std::string& iso, std::string& rest) {
    if(text.size() < 8) return false;
    unsigned y = 0, m = 0, d = 0;
    for(int i = 0; i < 8; ++i) {
        if(text[static_cast<std::size_t>(i)] < '0' || text[static_cast<std::size_t>(i)] > '9') return false;
        const auto digit = static_cast<unsigned>(text[static_cast<std::size_t>(i)] - '0');
        if(i < 4) y = y * 10 + digit; else if(i < 6) m = m * 10 + digit; else d = d * 10 + digit;
    }
    if(!std::chrono::year_month_day{std::chrono::year{static_cast<int>(y)}, std::chrono::month{m}, std::chrono::day{d}}.ok()) return false;
    char buf[11]; std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u", y, m, d); iso = buf; rest = std::string(text.substr(8)); return true;
}
void note(std::string& text, std::string piece, bool& overflow) {
    if(overflow) return;
    const auto extra = text.empty() ? piece.size() : text.size() + 2 + piece.size();
    if(extra > qif::max_field) overflow = true;
    else { if(!text.empty()) text += "; "; text += piece; }
}
bool harmless(std::string_view name) {
    return name == "SIC" || name == "PAYEEID" || name == "SRVRTID" || name == "DTUSER" || name == "DTAVAIL" || name == "EXTDNAME";
}
bool known_leaf(std::string_view name) {
    return name == "TRNTYPE" || name == "DTPOSTED" || name == "TRNAMT" || name == "FITID" || name == "NAME" || name == "PAYEE" ||
        name == "MEMO" || name == "CHECKNUM" || name == "REFNUM" || name == "CORRECTFITID" || name == "CORRECTACTION" ||
        name == "INV401KSOURCE" || name == "BANKACCTTO" || name == "CCACCTTO" || harmless(name);
}

qif::Record make_record(const Txn& txn, const Currency& currency, const std::string& format, const std::string& grammar, const std::string& key, bool& overflow) {
    qif::Record record; record.begin = txn.begin; record.end = txn.end; record.source_currency = currency;
    record.format = format; record.grammar = grammar; record.source_account_key = key;
    bool conflict = false, hard = txn.attributes || txn.transfer || txn.image || txn.orig_currency || txn.unknown;
    qif::Issue hard_issue = txn.transfer ? qif::Issue::transfer : txn.currency_sym.size() ? qif::Issue::currency_mismatch : qif::Issue::financial_blocker;
    const auto type = field(txn, "TRNTYPE", conflict); const auto posted = field(txn, "DTPOSTED", conflict);
    const auto amount_text = field(txn, "TRNAMT", conflict); const auto fit = field(txn, "FITID", conflict);
    const auto name = field(txn, "NAME", conflict); const auto payee = field(txn, "PAYEE", conflict);
    const auto memo = field(txn, "MEMO", conflict); const auto check = field(txn, "CHECKNUM", conflict); const auto ref = field(txn, "REFNUM", conflict);
    if(field(txn, "CORRECTFITID", conflict) || field(txn, "CORRECTACTION", conflict) || field(txn, "INV401KSOURCE", conflict)) hard = true;
    if(txn.transfer || (type && *type == "XFER")) { hard = true; hard_issue = qif::Issue::transfer; }
    else if(!txn.currency_sym.empty() && txn.currency_sym != currency.code()) { hard = true; hard_issue = qif::Issue::currency_mismatch; }
    else if(hard) hard_issue = qif::Issue::financial_blocker;
    if(!type || !known_type(*type) || *type == "HOLD") hard = true;
    if(conflict) hard = true;
    for(const auto& [field_name, values] : txn.fields) if(!known_leaf(field_name)) { hard = true; note(record.uninterpreted, field_name, overflow); }
    if(type) record.trntype = *type;
    if(fit && !fit->empty()) record.fitid = *fit; else note(record.uninterpreted, "FITID absent", overflow);
    if(posted) {
        std::string iso, rest;
        if(calendar_date(*posted, iso, rest)) { record.date = std::move(iso); record.time_evidence = std::move(rest); }
    }
    if(memo) record.memo = *memo;
    if(name && !name->empty()) record.payee = *name; else if(!txn.payee_name.empty()) record.payee = txn.payee_name; else if(payee) record.payee = *payee;
    if(name && payee && !name->empty() && !payee->empty() && *name != *payee) hard = true;
    if(check && !check->empty()) record.reference = *check; else if(ref) record.reference = *ref;
    if(check && ref && !check->empty() && !ref->empty() && *check != *ref) hard = true;
    if(txn.orig_currency) note(record.uninterpreted, "ORIGCURRENCY", overflow);
    if(!txn.currency_sym.empty()) note(record.uninterpreted, "CURRENCY " + txn.currency_sym, overflow);
    for(const auto* extra : {"SIC", "PAYEEID", "SRVRTID", "DTUSER", "DTAVAIL", "EXTDNAME"}) if(const auto* value = field(txn, extra, conflict)) note(record.uninterpreted, std::string(extra) + " " + *value, overflow);
    const bool foreign = txn.orig_currency || (!txn.currency_sym.empty() && txn.currency_sym != currency.code());
    if(amount_text && !foreign) {
        try { record.original_amount = Money::parse(*amount_text, currency); }
        catch(const FinancialError&) { record.original_amount.reset(); }
    } else if(amount_text) note(record.uninterpreted, "TRNAMT " + *amount_text, overflow);
    const bool bad_date = record.date.empty();
    const bool bad_amount = !record.original_amount.has_value();
    if(hard) {
        record.diagnostic = qif::Diagnostic{hard_issue, txn.begin}; record.repairable = false; record.transaction.reset(); record.original_amount = foreign ? std::nullopt : record.original_amount;
    } else if(bad_date || bad_amount) {
        record.diagnostic = qif::Diagnostic{bad_amount && !bad_date ? qif::Issue::amount : qif::Issue::date, txn.begin};
        record.repairable = true; record.amount_repair = bad_amount; record.transaction.reset();
    } else {
        Allocation allocation{"", record.memo, *record.original_amount, {}, "source-0", 0, FieldOwnership::source, FieldOwnership::source};
        record.source_allocations = {allocation};
        record.source_allocation_fields.push_back({"", record.memo, "", record.original_amount});
        record.transaction = Transaction::imported(*record.original_amount, {std::move(allocation)});
    }
    return record;
}

struct Parsed { Index index; std::vector<qif::Result> results; };

Parsed parse_ofx(std::string_view bytes, const std::string& format, bool xml, const std::function<void()>& checkpoint, bool slice, const std::string& slice_currency) {
    Parsed parsed; parsed.index.parser = std::string(xml ? parser_xml : parser_sgml); parsed.index.format = format;
    parsed.index.grammar = xml ? "ofx-xml" : "ofx-sgml"; parsed.index.explicit_accounts = !slice;
    if(bytes.size() > qif::max_file) {
        parsed.index.admissible = false; parsed.index.refusal = "Import resource limit exceeded";
        parsed.index.diagnostic = qif::Diagnostic{qif::Issue::resource_limit, 0}; return parsed;
    }
    std::size_t bad = 0;
    if(!controls_ok(bytes, checkpoint, bad)) {
        parsed.index.refusal = "Invalid UTF-8 or unsupported control character";
        parsed.index.diagnostic = qif::Diagnostic{qif::Issue::encoding, bad}; return parsed;
    }
    std::size_t body = 0;
    if(!slice) {
        auto header = read_header(bytes, xml, parsed.index.refusal, parsed.index.diagnostic.emplace(), parsed.index.admissible);
        if(!header) { if(!parsed.index.diagnostic) parsed.index.diagnostic = qif::Diagnostic{qif::Issue::header, 0}; return parsed; }
        parsed.index.header = std::move(header->raw); body = header->body; parsed.index.diagnostic.reset();
    }
    Built built; Builder builder(built); builder.slice = slice; builder.slice_currency = slice_currency;
    bool walked = false;
    if(xml) walked = parse_xml(bytes, body, builder, checkpoint);
    else { std::size_t pos = body; walked = parse_sgml(bytes, pos, builder, checkpoint, {}, 0); if(walked) { pos = skip_ws(bytes, pos); if(pos < bytes.size()) builder.fail(qif::Issue::header, pos, "Malformed OFX; no import was saved"); } }
    if(!walked || built.stop) {
        parsed.index.refusal = built.refusal.empty() ? "Malformed OFX; no import was saved" : built.refusal;
        parsed.index.diagnostic = built.diagnostic ? built.diagnostic : qif::Diagnostic{qif::Issue::header, body};
        parsed.index.admissible = built.admissible; parsed.index.accounts.clear(); return parsed;
    }
    parsed.index.notes = built.notes;
    if(!built.signon.empty() && built.signon != "0") {
        if(!parsed.index.notes.empty()) parsed.index.notes.push_back('\n');
        parsed.index.notes += "SIGNON status " + built.signon;
    }
    if(!slice && built.accounts.empty()) {
        parsed.index.refusal = !built.signon.empty() && built.signon != "0" ? "Signon failed and no statement was included; no import was saved" : "No bank or credit-card statement was found";
        parsed.index.diagnostic = qif::Diagnostic{qif::Issue::empty_file, body}; return parsed;
    }
    for(auto& account : built.accounts) {
        if(!slice && (account.acct_id.empty() || account.status.empty())) {
            parsed.index.refusal = account.acct_id.empty() ? "Statement account id is missing; no import was saved" : "Malformed OFX; no import was saved";
            parsed.index.diagnostic = qif::Diagnostic{qif::Issue::header, account.begin}; parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
        }
        if(!slice && account.status != "0") {
            parsed.index.refusal = "Statement status is not success; no import was saved";
            parsed.index.diagnostic = qif::Diagnostic{qif::Issue::header, account.begin}; parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
        }
        bool letters = account.currency.size() == 3;
        for(char ch : account.currency) { const auto c = static_cast<unsigned char>(ch); letters = letters && c >= 'A' && c <= 'Z'; }
        Currency currency("CAD");
        if(!letters) {
            parsed.index.refusal = "Statement currency is missing or unsupported; no conversion was applied";
            parsed.index.diagnostic = qif::Diagnostic{qif::Issue::currency_mismatch, account.begin}; parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
        }
        try { currency = Currency(account.currency); }
        catch(const FinancialError&) {
            parsed.index.refusal = "Statement currency is missing or unsupported; no conversion was applied";
            parsed.index.diagnostic = qif::Diagnostic{qif::Issue::currency_mismatch, account.begin}; parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
        }
        Account view; view.institution_id = account.bank_id; view.account_number = account.acct_id; view.account_type = account.acct_type;
        view.currency_code = account.currency; view.begin = account.begin; view.end = account.end; view.type = account.acct_type.empty() && account.credit ? "CREDITCARD" : account.acct_type;
        const std::string raw_name = account.bank_id.empty() ? account.acct_id : account.bank_id + " " + account.acct_id;
        view.name = std::string(trim(raw_name));
        if(!account.suppress_balance && account.seen_fields.contains("BALAMT")) {
            try { (void)Money::parse(account.ledger, currency); view.reported_balance = account.ledger; }
            catch(const FinancialError&) {
                parsed.index.refusal = "The statement balance could not be read; no import was saved. Obtain a corrected export and try again.";
                parsed.index.diagnostic = qif::Diagnostic{qif::Issue::header, account.begin};
                parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
            }
        }
        const auto key = account.bank_id + "\t" + account.acct_id;
        qif::Result result; bool overflow = false;
        for(const auto& txn : account.txns) {
            if(checkpoint) checkpoint();
            result.records.push_back(make_record(txn, currency, format, parsed.index.grammar, key, overflow));
            if(overflow) {
                parsed.index.admissible = false; parsed.index.refusal = "Import resource limit exceeded";
                parsed.index.diagnostic = qif::Diagnostic{qif::Issue::resource_limit, txn.begin}; parsed.index.accounts.clear(); parsed.results.clear(); return parsed;
            }
        }
        parsed.index.accounts.push_back(std::move(view)); parsed.results.push_back(std::move(result));
    }
    return parsed;
}
} // namespace

Index inspect(std::string_view bytes, std::string_view requested, const std::function<void()>& checkpoint) {
    if(checkpoint) checkpoint();
    const auto label = normalize_label(requested);
    const bool sgml = looks_sgml(bytes), xml = !sgml && looks_xml(bytes);
    std::string format = label;
    if(format.empty()) format = (sgml || xml) ? "ofx" : "qif";
    if(format == "qif") {
        if(sgml || xml) {
            Index refused; refused.format = "qif"; refused.refusal = "File label does not match the statement grammar; no import was saved";
            refused.diagnostic = qif::Diagnostic{qif::Issue::header, 0}; return refused;
        }
        const auto document = qif::index_document(bytes, checkpoint);
        Index index; index.parser = std::string(qif::parser_version); index.format = "qif"; index.grammar = "qif";
        index.categories = document.categories;
        index.explicit_accounts = document.explicit_accounts; index.admissible = document.admissible; index.diagnostic = document.diagnostic;
        for(const auto& account : document.accounts) index.accounts.push_back(Account{account.name, account.type, account.reported_balance, {}, {}, {}, {}, account.begin, account.end});
        return index;
    }
    if(format != "ofx" && format != "qfx" && format != "qbo" && format != "aso") {
        Index refused; refused.refusal = "File label does not match the statement grammar; no import was saved";
        refused.diagnostic = qif::Diagnostic{qif::Issue::header, 0}; return refused;
    }
    if(!sgml && !xml) {
        Index refused; refused.format = format; refused.refusal = "File label does not match the statement grammar; no import was saved";
        refused.diagnostic = qif::Diagnostic{qif::Issue::header, 0}; return refused;
    }
    return parse_ofx(bytes, format, xml, checkpoint, false, {}).index;
}

qif::Result parse_mapped(std::string_view bytes, const Index& index, std::size_t account, qif::DateOrder order, const Currency& currency, const std::function<void()>& checkpoint) {
    if(index.format == "qif" || index.parser == qif::parser_version) {
        const auto document = qif::index_document(bytes, checkpoint);
        if(!document.explicit_accounts && document.categories.empty()) return qif::parse(bytes, order, currency, checkpoint);
        return qif::parse_account(bytes, document, account, order, currency, checkpoint);
    }
    qif::Result rejected;
    if(!index.admissible || !index.refusal.empty() || index.diagnostic) {
        rejected.admissible = index.admissible; rejected.diagnostic = index.diagnostic ? index.diagnostic : qif::Diagnostic{qif::Issue::header, 0}; return rejected;
    }
    const bool xml = index.grammar == "ofx-xml" || index.parser == parser_xml;
    auto parsed = parse_ofx(bytes, index.format.empty() ? "ofx" : index.format, xml, checkpoint, false, {});
    if(!parsed.index.admissible || !parsed.index.refusal.empty() || parsed.index.diagnostic) {
        rejected.admissible = parsed.index.admissible; rejected.diagnostic = parsed.index.diagnostic; return rejected;
    }
    if(account >= parsed.results.size()) throw std::invalid_argument("Invalid statement account index");
    if(parsed.index.accounts[account].currency_code != currency.code()) {
        rejected.diagnostic = qif::Diagnostic{qif::Issue::currency_mismatch, parsed.index.accounts[account].begin}; return rejected;
    }
    return std::move(parsed.results[account]);
}

qif::Result parse_occurrence(std::string_view slice, std::string_view parser, const Currency& currency, qif::DateOrder order) {
    if(parser == qif::parser_version || parser.empty()) return qif::parse(slice, order, currency);
    const bool xml = parser == parser_xml;
    if(parser != parser_sgml && !xml) { qif::Result bad; bad.diagnostic = qif::Diagnostic{qif::Issue::header, 0}; return bad; }
    auto parsed = parse_ofx(slice, "ofx", xml, {}, true, currency.code());
    if(!parsed.index.admissible || !parsed.index.refusal.empty() || parsed.results.size() != 1 || parsed.results[0].records.size() != 1) {
        qif::Result bad; bad.admissible = parsed.index.admissible;
        bad.diagnostic = parsed.index.diagnostic ? parsed.index.diagnostic : qif::Diagnostic{qif::Issue::header, 0}; return bad;
    }
    return std::move(parsed.results[0]);
}
} // namespace spentit::statements
