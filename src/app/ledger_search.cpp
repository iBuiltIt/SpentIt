#include <spentit/ledger_search.hpp>
#include <spentit/qif.hpp>
#include <spentit/date_format.hpp>
#include <spentit/category.hpp>
#include <array>
#include <chrono>
#include <cstdio>

namespace spentit::app {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
std::string lower(std::string value) { for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value; }
std::string literal(std::string_view value) {
    std::string out;
    for(char c:value){if(std::string_view(R"(\.^$|()[]{}*+?)").find(c)!=std::string_view::npos)out+='\\';out+=c;}
    return out;
}
std::string date(int year,unsigned month,unsigned day) {
    const std::chrono::year_month_day value{std::chrono::year(year),std::chrono::month(month),std::chrono::day(day)};
    if(year<1 || year>9999 || !value.ok())invalid("Invalid search date");
    // Wide enough for the format's largest int and unsigned values. Valid dates stay YYYY-MM-DD.
    std::array<char,48> out{};std::snprintf(out.data(),out.size(),"%04d-%02u-%02u",year,month,day);return out.data();
}
int number(std::string_view value) {
    int out=0;if(value.empty() || value.size()>4)invalid("Invalid search date");
    for(char c:value){if(c<'0'||c>'9')invalid("Use an unambiguous year-first search date");out=out*10+c-'0';}return out;
}
std::pair<std::string,std::string> period(std::string_view value,int year,std::string_view format) {
    const auto range=value.find("..");
    if(range!=value.npos){auto a=period(value.substr(0,range),year,format),b=period(value.substr(range+2),year,format);if(value.find("..",range+2)!=value.npos || a.first>b.second)invalid("Invalid or reversed period range");return {a.first,b.second};}
    constexpr std::array<std::string_view,12> months{"january","february","march","april","may","june","july","august","september","october","november","december"};
    const auto name=lower(std::string(value));
    if(!name.empty() && name[0]>='a' && name[0]<='z'){
        const auto dash=name.find('-');
        if(dash!=name.npos){auto a=period(name.substr(0,dash),year,format),b=period(name.substr(dash+1),year,format);if(a.first>b.second)invalid("Month range crosses a year; use explicit dates");return {a.first,b.second};}
        for(unsigned i=0;i<months.size();++i)if(name==months[i] || name==months[i].substr(0,3)){
            const auto end=std::chrono::year_month_day_last{std::chrono::year(year),std::chrono::month_day_last{std::chrono::month(i+1)}};
            return {date(year,i+1,1),date(year,i+1,static_cast<unsigned>(end.day()))};
        }
        invalid("Unknown period month");
    }
    if(value.size()==4){const auto y=number(value);return {date(y,1,1),date(y,12,31)};}
    if(format!="YYYY-MM-DD"){const auto result=parse_display_date(value,format);return {result,result};}
    std::string result;
    if(value.size()==8)result=date(number(value.substr(0,4)),static_cast<unsigned>(number(value.substr(4,2))),static_cast<unsigned>(number(value.substr(6,2))));
    else if(value.size()==10 && (value[4]=='-' || value[4]=='/') && value[7]==value[4])result=date(number(value.substr(0,4)),static_cast<unsigned>(number(value.substr(5,2))),static_cast<unsigned>(number(value.substr(8,2))));
    else invalid("Use YYYYMMDD, YYYY-MM-DD, YYYY/MM/DD, or a named month for periods");
    return {result,result};
}
struct Token {std::string text;bool quoted=false;bool starts_quoted=false;};
std::vector<Token> tokenize(std::string_view query) {
    std::vector<Token> out;std::size_t i=0;
    while(i<query.size()){
        if(query[i]==' ' || query[i]=='\t'){++i;continue;}
        Token token;token.starts_quoted=query[i]=='"';bool quoted=false;
        while(i<query.size()){
            char c=query[i++];
            if(c=='"'){quoted=!quoted;token.quoted=true;continue;}
            if(quoted && c=='\\' && i<query.size() && (query[i]=='"'||query[i]=='\\')){token.text+=query[i++];continue;}
            if(!quoted && (c==' ' || c=='\t'))break;
            token.text+=c;
        }
        if(quoted)invalid("Unclosed search quote");
        if(token.text.empty())invalid("Empty search term");
        out.push_back(std::move(token));
        if(out.size()>18)invalid("Search has too many terms (maximum 16 text terms)");
    }
    return out;
}
}
LedgerSearch::LedgerSearch(std::string_view query,int year,std::string_view format):date_format_(format) {
    validate_date_format(format);
    if(query.size()>4096 || !qif::valid_utf8(query) || query.find('\0')!=query.npos)invalid("Invalid search text or query exceeds 4096 bytes");
    if(year<1 || year>9999)invalid("Invalid current search year");
    auto tokens=tokenize(query);
    for(std::size_t i=0;i<tokens.size();++i){
        const auto& token=tokens[i];const auto key=lower(token.text);
        if(key=="qif:" && !token.quoted){if(i!=0)invalid("QIF: must be the first query token");source_=true;continue;}
        if(key.starts_with("period:") && !token.quoted){if(!first_.empty())invalid("Use one period filter per query");const auto bounds=period(std::string_view(key).substr(7),year,format);first_=bounds.first;last_=bounds.second;continue;}
        if(!token.starts_quoted && (key.starts_with("category:") || key.starts_with("category-exact:"))) {
            if(category_)invalid("Use one category filter per query");
            if(source_)invalid("Category filters use interpreted transactions, not raw source search");
            const bool exact=key.starts_with("category-exact:");
            const auto name=canonical_category_name(std::string_view(token.text).substr(exact?15:9));
            category_.emplace("\\A"+literal(name)+(!exact && name.find(':')==name.npos?"(?:\\z|: )":"\\z"));
            continue;
        }
        if(terms_.size()>=16)invalid("Search has too many terms (maximum 16)");
        if(key.starts_with("regex:") && !token.starts_quoted){if(!token.quoted || token.text.size()==6)invalid("Use regex:\"pattern\" for a regular expression");terms_.emplace_back(std::string_view(token.text).substr(6));}
        else terms_.emplace_back(literal(token.text));
    }
}
std::string LedgerSearch::display_date(std::string_view iso) const {return spentit::display_date(iso,date_format_);}
bool LedgerSearch::matches(std::string_view effective_date,std::span<const std::string_view> fields,bool has_source,const std::function<bool()>& cancelled,std::span<const std::string_view> categories) {
    auto poll=[&]{return ++steps_>20000000 || (cancelled && cancelled());};
    if(poll()){if(steps_>20000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search matching budget exhausted");throw rules::RegexError(rules::RegexFailure::cancelled,"Search cancelled");}
    if(source_ && !has_source)return false;
    if(!first_.empty() && (effective_date<first_ || effective_date>last_))return false;
    if(category_) {
        bool matched=false;
        for(const auto name:categories) {
            if(poll()){if(steps_>20000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search matching budget exhausted");throw rules::RegexError(rules::RegexFailure::cancelled,"Search cancelled");}
            if(name.size()>256*1024*1024-bytes_ || ++comparisons_>4000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search input budget exhausted");
            bytes_+=name.size();
            try {if(category_->match(name,poll)){matched=true;break;}}
            catch(const rules::RegexError&){if(steps_>20000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search matching budget exhausted");throw;}
        }
        if(!matched)return false;
    }
    for(const auto& term:terms_){
        bool found=false;
        for(const auto field:fields){
            if(field.size()>256*1024*1024-bytes_ || ++comparisons_>4000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search input budget exhausted");
            bytes_+=field.size();
            try {if(term.match(field,poll)){found=true;break;}}
            catch(const rules::RegexError&){if(steps_>20000000)throw rules::RegexError(rules::RegexFailure::resource_limit,"Search matching budget exhausted");throw;}
        }
        if(!found)return false;
    }
    return true;
}
}
