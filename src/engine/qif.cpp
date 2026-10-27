#include <spentit/qif.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace spentit::qif {
std::string_view message(Issue issue) noexcept {
    switch (issue) {
    case Issue::encoding: return "Invalid UTF-8 or unsupported control character";
    case Issue::resource_limit: return "Import resource limit exceeded";
    case Issue::empty_file: return "No banking records";
    case Issue::header: return "Unsupported or misplaced QIF header";
    case Issue::empty_record: return "Empty record or line";
    case Issue::unterminated: return "Record requires a terminator";
    case Issue::unknown_field: return "Unsupported QIF field";
    case Issue::repeated_field: return "Repeated singleton field";
    case Issue::missing_field: return "Required date or total is missing";
    case Issue::date: return "Invalid date for the selected profile";
    case Issue::amount: return "Invalid exact amount or precision";
    case Issue::split_structure: return "Ambiguous or incomplete split structure";
    case Issue::split_total: return "Splits do not equal the exact total";
    case Issue::transfer: return "Invalid or unsupported transfer destination syntax";
    case Issue::category_class: return "Invalid or unsupported category/class notation";
    case Issue::cleared_status: return "Unsupported cleared or budget status";
    }
    return "Invalid QIF input";
}
static bool checked_utf8(std::string_view s,const std::function<void()>& checkpoint) {
    std::size_t next_checkpoint=0;
    for (std::size_t i = 0; i < s.size();) {
        if(checkpoint && i>=next_checkpoint) {checkpoint();next_checkpoint=i+4096;}
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) continue;
        unsigned n = 0, cp = 0, min = 0;
        if (c >= 0xc2 && c <= 0xdf) { n = 1; cp = c & 31u; min = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { n = 2; cp = c & 15u; min = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { n = 3; cp = c & 7u; min = 0x10000; }
        else return false;
        if (s.size() - i < n) return false;
        while (n--) {
            const auto x = static_cast<unsigned char>(s[i++]);
            if ((x & 0xc0u) != 0x80u) return false;
            cp = (cp << 6u) | (x & 63u);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}
bool valid_utf8(std::string_view s) noexcept { return checked_utf8(s,{}); }
std::optional<std::string_view> transfer_destination(std::string_view text) noexcept {
    if(text.size()<3 || text.front()!='[' || text.back()!=']')return {};
    const auto name=text.substr(1,text.size()-2);
    if(name.find_first_of("[]/|\t\r\n")!=name.npos || name.find_first_not_of(' ')==name.npos)return {};
    return name;
}
bool supported_category(std::string_view text) noexcept { return text.find_first_of("[]/|") == text.npos; }
std::string date(std::string_view s, DateOrder order) {
    auto fail = []() -> void { throw std::invalid_argument("Invalid QIF date/profile"); };
    if (order != DateOrder::dmy && order != DateOrder::mdy) fail();
    const auto first = s.find('/'), second = first == s.npos ? s.npos : s.find('/', first + 1);
    if (first < 1 || first > 2 || second == s.npos || second - first < 2 || second - first > 3 || s.size() - second != 5) fail();
    auto number = [&](std::string_view part) {
        unsigned value = 0;
        for (char c : part) { if (c < '0' || c > '9') fail(); value = value * 10 + static_cast<unsigned>(c - '0'); }
        return value;
    };
    unsigned a = number(s.substr(0,first)), b = number(s.substr(first+1,second-first-1)), y = number(s.substr(second+1));
    const auto m = order == DateOrder::dmy ? b : a, d = order == DateOrder::dmy ? a : b;
    if (y == 0 || !std::chrono::year_month_day(std::chrono::year(static_cast<int>(y)),std::chrono::month(m),std::chrono::day(d)).ok()) fail();
    char out[11]; std::snprintf(out,sizeof(out),"%04u-%02u-%02u",y,m,d); return out;
}
bool Result::valid() const noexcept {
    if (diagnostic || !admissible || records.empty()) return false;
    for (const auto& r : records) if (r.diagnostic || !r.transaction) return false;
    return true;
}
Result parse(std::string_view bytes, DateOrder order, const Currency& currency,const std::function<void()>& checkpoint) {
    if(checkpoint) checkpoint();
    if (order != DateOrder::dmy && order != DateOrder::mdy) throw std::invalid_argument("Invalid QIF profile");
    Result out;
    auto file_error = [&](Issue issue, std::size_t offset) { out.diagnostic = Diagnostic{issue,offset}; if (issue == Issue::resource_limit) out.admissible = false; };
    if (bytes.size() > max_file) { file_error(Issue::resource_limit,0); return out; }
    if (!checked_utf8(bytes,checkpoint)) { file_error(Issue::encoding,0); return out; }
    for (std::size_t i=0;i<bytes.size();++i) {
        if(checkpoint && i % 4096 == 0) checkpoint();
        auto c = static_cast<unsigned char>(bytes[i]);
        if ((c < 32 && c != '\n' && c != '\t' && c != '\r') || c == 127 ||
            (c == '\r' && (i+1 == bytes.size() || bytes[i+1] != '\n'))) { file_error(Issue::encoding,i); return out; }
    }
    Record r; r.source_currency=currency;
    std::array<bool,256> seen{};
    std::optional<Money> total;
    std::vector<Allocation> allocations;
    std::string category, split_category, split_memo, source_class, split_class;
    std::vector<std::string> allocation_classes;
    std::vector<SourceAllocation> source_fields;
    bool split_started=false, has_s=false, has_e=false, any=false;
    unsigned addresses=0;
    std::size_t split_count=0;
    bool repair_allowed=true;
    auto error = [&](Issue issue,std::size_t at) { if(issue!=Issue::date && issue!=Issue::split_total && issue!=Issue::amount && issue!=Issue::missing_field) repair_allowed=false; if (!r.diagnostic) r.diagnostic=Diagnostic{issue,at}; };
    auto category_parts = [&](std::string_view value,std::size_t at,std::string& target,std::string& class_label) {
        const auto slash=value.find('/');
        const auto base=value.substr(0,slash);
        target=base; class_label.clear();
        if(slash!=value.npos) {
            const auto label=value.substr(slash+1);
            if(label.empty() || label.find_first_of("/|[]\t")!=label.npos || label.find_first_not_of(' ')==label.npos)
                error(Issue::category_class,at);
            else class_label=label;
        }
        if (transfer_destination(base)) return;
        if (base.find_first_of("[]") != base.npos) error(Issue::transfer,at);
        else if (!supported_category(base)) error(Issue::category_class,at);
    };
    std::size_t pos = bytes.starts_with("\xef\xbb\xbf") ? 3 : 0;
    r.begin=pos;
    while (pos < bytes.size()) {
        if(checkpoint) checkpoint();
        const auto start=pos, newline=bytes.find('\n',pos);
        pos = newline == bytes.npos ? bytes.size() : newline+1;
        auto line=bytes.substr(start,(newline == bytes.npos ? bytes.size() : newline)-start);
        if (line.ends_with('\r')) line.remove_suffix(1);
        if (line.size() > max_field+1 || pos-r.begin > max_record) { file_error(Issue::resource_limit,start); return out; }
        if (!any && out.records.empty() && start == (bytes.starts_with("\xef\xbb\xbf") ? 3u : 0u) && (line == "!Type:Bank" || line == "!Type:Cash" || line == "!Type:CCard")) { r.begin=pos; continue; }
        if (line.starts_with('!')) { file_error(Issue::header,start); return out; }
        if (line == "^") {
            if (out.records.size() == max_records) { file_error(Issue::resource_limit,start); return out; }
            if (!any) error(Issue::empty_record,start);
            if (!seen['D'] || !seen['T']) error(Issue::missing_field,start);
            if (has_s || has_e || (split_started && source_fields.empty())) error(Issue::split_structure,start);
            if (!source_fields.empty() && !category.empty() && category != "--Split--") error(Issue::split_structure,start);
            r.original_amount=total;
            if (!split_started) {
                source_fields.push_back({category,r.memo,source_class,total}); allocation_classes.push_back(source_class);
                if(total) {Allocation allocation{category,r.memo,*total};allocation.source_ordinal=0;allocations.push_back(std::move(allocation));}
            }
            r.has_source_splits=split_started;
            r.source_category=category; r.source_class=source_class;
            r.source_allocation_classes=allocation_classes; r.source_allocation_fields=source_fields;
            for(std::size_t index=0;index<allocations.size();++index){auto& allocation=allocations[index];allocation.identity="source-"+std::to_string(*allocation.source_ordinal);allocation.category_owner=FieldOwnership::source;allocation.memo_owner=FieldOwnership::source;}
            r.source_allocations=allocations;
            r.amount_repair=!total;for(const auto& field:source_fields)if(!field.amount)r.amount_repair=true;
            if (repair_allowed && total) {
                try { r.transaction=Transaction::imported(*total,std::move(allocations)); }
                catch (const FinancialError&) { r.amount_repair=true;error(Issue::split_total,start); }
            }
            r.repairable=r.diagnostic.has_value() && repair_allowed;
            if(r.diagnostic) r.transaction.reset();
            r.end=pos; out.records.push_back(std::move(r)); r=Record{}; r.source_currency=currency; r.begin=pos;
            seen.fill(false); total.reset(); allocations.clear(); category.clear(); split_category.clear(); split_memo.clear(); source_class.clear(); split_class.clear(); allocation_classes.clear(); source_fields.clear();
            split_started=has_s=has_e=any=false; repair_allowed=true; addresses=0; split_count=0; continue;
        }
        any=true;
        if (line.empty()) { error(Issue::empty_record,start); continue; }
        const auto key=static_cast<unsigned char>(line.front()); const auto value=line.substr(1);
        if (key=='S' || key=='E' || key=='$') {
            split_started=true;
            if (key=='S') { if (has_s || has_e) error(Issue::split_structure,start); has_s=true; category_parts(value,start,split_category,split_class); }
            if (key=='E') { if (has_e) error(Issue::split_structure,start); has_e=true; split_memo=value; }
            if (key=='$') {
                if (++split_count > max_allocations) { file_error(Issue::resource_limit,start); return out; }
                std::optional<Money> split_amount;
                try { split_amount=Money::parse(value,currency); Allocation allocation{split_category,split_memo,*split_amount};allocation.source_ordinal=split_count-1;allocations.push_back(std::move(allocation)); }
                catch (const FinancialError&) { error(Issue::amount,start); }
                source_fields.push_back({split_category,split_memo,split_class,split_amount});allocation_classes.push_back(split_class);
                split_category.clear(); split_memo.clear(); split_class.clear(); has_s=has_e=false;
            }
            continue;
        }
        if (split_started) error(Issue::split_structure,start);
        if (key=='A') { if (++addresses > 6) error(Issue::repeated_field,start); continue; }
        if (seen[key]) error(Issue::repeated_field,start);
        seen[key]=true;
        switch (key) {
        case 'D': try { r.date=date(value,order); } catch (const std::invalid_argument&) { error(Issue::date,start); } break;
        case 'T': try { total=Money::parse(value,currency); } catch (const FinancialError&) { error(Issue::amount,start); } break;
        case 'P': r.payee=value; break;
        case 'M': r.memo=value; break;
        case 'N': r.reference=value; break;
        case 'L': category_parts(value,start,category,source_class); break;
        case 'C': r.cleared=value; if (value!="" && value!="*" && value!="c" && value!="X" && value!="R") error(Issue::cleared_status,start); break;
        default: error(Issue::unknown_field,start); break;
        }
    }
    if (any && out.records.size() == max_records) { file_error(Issue::resource_limit,r.begin); return out; }
    if (any) { error(Issue::unterminated,bytes.size()); r.end=bytes.size(); out.records.push_back(std::move(r)); }
    if (out.records.empty()) file_error(Issue::empty_file,bytes.size());
    return out;
}
Document index_document(std::string_view bytes,const std::function<void()>& checkpoint) {
    Document out;
    auto fail=[&](Issue issue,std::size_t at) {out.diagnostic=Diagnostic{issue,at};if(issue==Issue::resource_limit)out.admissible=false;};
    if(bytes.size()>max_file){fail(Issue::resource_limit,0);return out;}
    if(!checked_utf8(bytes,checkpoint)){fail(Issue::encoding,0);return out;}
    for(std::size_t i=0;i<bytes.size();++i) {
        if(checkpoint && i%4096==0)checkpoint();
        const auto c=static_cast<unsigned char>(bytes[i]);
        if((c<32 && c!='\n' && c!='\t' && c!='\r') || c==127 || (c=='\r' && (i+1==bytes.size() || bytes[i+1]!='\n'))) {
            fail(Issue::encoding,i);return out;
        }
    }
    // A plain single-account source retains the existing parser's diagnostics.
    const auto content=bytes.starts_with("\xef\xbb\xbf")?bytes.substr(3):bytes;
    auto header_present=[&](std::string_view h){return content.starts_with(h) || content.find(std::string("\n")+std::string(h))!=content.npos;};
    if(!header_present("!Account") && !header_present("!Option:") && !header_present("!Clear:")) {
        out.accounts.push_back({});out.sections.push_back({0,0,bytes.size()});return out;
    }
    out.explicit_accounts=true;
    std::map<std::string,std::size_t> names;
    std::optional<std::size_t> selected;
    bool definitions=false, record_open=false;
    AccountDefinition definition;
    std::array<bool,256> seen{};
    std::size_t records=0;
    auto ordinary=[](std::string_view type){return type=="Bank" || type=="Cash" || type=="CCard";};
    auto close_section=[&](std::size_t at){if(!out.sections.empty() && out.sections.back().end==0)out.sections.back().end=at;};
    std::size_t pos=bytes.starts_with("\xef\xbb\xbf")?3:0;
    while(pos<bytes.size()) {
        if(checkpoint)checkpoint();
        const auto start=pos, newline=bytes.find('\n',pos);
        pos=newline==bytes.npos?bytes.size():newline+1;
        auto line=bytes.substr(start,(newline==bytes.npos?bytes.size():newline)-start);
        if(line.ends_with('\r'))line.remove_suffix(1);
        if(line.size()>max_field+1){fail(Issue::resource_limit,start);return out;}
        if(line.starts_with('!')) {
            if(record_open){fail(Issue::unterminated,start);return out;}
            close_section(start);
            if(line=="!Option:AutoSwitch" || line=="!Clear:AutoSwitch") {definitions=false;continue;}
            if(line=="!Account") {definitions=true;continue;}
            if(line.starts_with("!Type:") && ordinary(line.substr(6)) && selected) {
                if(out.sections.size()==10000){fail(Issue::resource_limit,start);return out;}
                definitions=false;out.sections.push_back({*selected,start,0});continue;
            }
            fail(Issue::header,start);return out;
        }
        if(definitions) {
            if(!record_open){definition=AccountDefinition{};definition.begin=start;seen.fill(false);}
            if(line=="^") {
                if(!seen['N'] || definition.name.empty() || definition.name.find_first_not_of(' ')==std::string::npos || (seen['T'] && !ordinary(definition.type))) {
                    fail(Issue::header,start);return out;
                }
                definition.end=pos;
                const auto prior=names.find(definition.name);
                if(prior==names.end()) {
                    if(!seen['T']){fail(Issue::header,start);return out;}
                    if(out.accounts.size()==1000){fail(Issue::resource_limit,start);return out;}
                    selected=out.accounts.size();names.emplace(definition.name,*selected);out.accounts.push_back(definition);
                } else {
                    selected=prior->second;const auto& old=out.accounts[*selected];
                    if((seen['T'] && old.type!=definition.type) || (definition.reported_balance && old.reported_balance!=definition.reported_balance)) {
                        fail(Issue::repeated_field,start);return out;
                    }
                }
                record_open=false;continue;
            }
            if(line.empty()){fail(Issue::empty_record,start);return out;}
            record_open=true;
            if(pos-definition.begin>max_record){fail(Issue::resource_limit,start);return out;}
            const auto key=static_cast<unsigned char>(line.front());
            if(seen[key]){fail(Issue::repeated_field,start);return out;}seen[key]=true;
            switch(key) {
            case 'N':definition.name=line.substr(1);break;
            case 'T':definition.type=line.substr(1);break;
            case 'B':definition.reported_balance=line.substr(1);break;
            case 'D':definition.description=line.substr(1);break;
            case 'L':definition.credit_limit=line.substr(1);break;
            case '/':definition.statement_date=line.substr(1);break;
            case '$':definition.statement_balance=line.substr(1);break;
            default:fail(Issue::unknown_field,start);return out;
            }
        } else {
            if(out.sections.empty() || out.sections.back().end!=0){fail(Issue::header,start);return out;}
            record_open=line!="^";
            if(!record_open && ++records>max_records){fail(Issue::resource_limit,start);return out;}
        }
    }
    close_section(bytes.size());
    if(record_open)fail(Issue::unterminated,bytes.size());
    if(out.accounts.empty() && !out.diagnostic)fail(Issue::empty_file,bytes.size());
    return out;
}
Result parse_account(std::string_view bytes,const Document& document,std::size_t account,
                     DateOrder order,const Currency& currency,const std::function<void()>& checkpoint) {
    Result result;
    if(document.diagnostic){result.diagnostic=document.diagnostic;result.admissible=document.admissible;return result;}
    if(account>=document.accounts.size())throw std::invalid_argument("Invalid QIF account index");
    for(const auto& section:document.sections) {
        if(section.account!=account)continue;
        if(section.begin>section.end || section.end>bytes.size())throw std::invalid_argument("Invalid QIF source range");
        auto parsed=parse(bytes.substr(section.begin,section.end-section.begin),order,currency,checkpoint);
        if(!parsed.admissible)result.admissible=false;
        if(parsed.diagnostic && parsed.diagnostic->issue==Issue::empty_file && parsed.records.empty() && document.explicit_accounts)continue;
        if(parsed.diagnostic) {
            parsed.diagnostic->offset+=section.begin;
            if(!result.diagnostic)result.diagnostic=parsed.diagnostic;
        }
        for(auto& record:parsed.records) {
            record.begin+=section.begin;record.end+=section.begin;
            if(record.diagnostic)record.diagnostic->offset+=section.begin;
            result.records.push_back(std::move(record));
        }
    }
    if(result.records.size()>max_records){result.admissible=false;result.diagnostic=Diagnostic{Issue::resource_limit,bytes.size()};}
    return result;
}
} // namespace spentit::qif
