#include <spentit/date_format.hpp>
#include <array>
#include <chrono>
#include <stdexcept>
namespace spentit {
namespace {
struct Format {std::array<char,3> order;char separator=0;};
Format parse_format(std::string_view value) {
    Format result;std::size_t position=0;unsigned seen=0;
    for(unsigned i=0;i<3;++i){
        char token=position<value.size()?value[position]:0;
        const auto word=token=='Y'?"YYYY":token=='M'?"MM":token=='D'?"DD":"";
        unsigned bit=token=='Y'?1U:token=='M'?2U:token=='D'?4U:0U;
        if(!bit || (seen&bit) || value.substr(position,std::char_traits<char>::length(word))!=word)
            throw std::invalid_argument("Use YYYY, MM and DD exactly once, with -, /, . or no separator");
        seen|=bit;result.order[i]=token;position+=std::char_traits<char>::length(word);
        if(i==0 && position<value.size() && (value[position]=='-' || value[position]=='/' || value[position]=='.'))result.separator=value[position];
        if(i<2 && result.separator){if(position>=value.size() || value[position++]!=result.separator)throw std::invalid_argument("Use the same date separator twice");}
    }
    if(position!=value.size())throw std::invalid_argument("Use YYYY, MM and DD exactly once");
    return result;
}
void validate_iso(std::string_view value){
    if(value.size()!=10 || value[4]!='-' || value[7]!='-')throw std::invalid_argument("Invalid calendar date");
    int y=0,m=0,d=0;
    for(std::size_t i=0;i<value.size();++i){if(i==4 || i==7)continue;if(value[i]<'0'||value[i]>'9')throw std::invalid_argument("Date fields require digits");int& n=i<4?y:i<7?m:d;n=n*10+value[i]-'0';}
    if(y==0 || !std::chrono::year_month_day{std::chrono::year(y),std::chrono::month(static_cast<unsigned>(m)),std::chrono::day(static_cast<unsigned>(d))}.ok())throw std::invalid_argument("Invalid calendar date");
}
}
void validate_date_format(std::string_view value){(void)parse_format(value);}
std::string display_date(std::string_view iso,std::string_view format){
    const auto parsed=parse_format(format);if(iso.empty())return {};validate_iso(iso);std::string out;
    for(char part:parsed.order){if(!out.empty() && parsed.separator)out+=parsed.separator;out+=part=='Y'?iso.substr(0,4):part=='M'?iso.substr(5,2):iso.substr(8,2);}return out;
}
std::string parse_display_date(std::string_view value,std::string_view format){
    const auto parsed=parse_format(format);std::string result="0000-00-00";std::size_t pos=0;
    for(unsigned i=0;i<3;++i){char token=parsed.order[i];const std::size_t width=token=='Y'?4:2;if(pos+width>value.size())throw std::invalid_argument("Enter the date using the selected format");result.replace(token=='Y'?0:token=='M'?5:8,width,value.substr(pos,width));pos+=width;if(i<2 && parsed.separator){if(pos>=value.size() || value[pos++]!=parsed.separator)throw std::invalid_argument("Enter the date using the selected format");}}
    if(pos!=value.size())throw std::invalid_argument("Enter the date using the selected format");
    validate_iso(result);return result;
}
std::string suggested_date_format(std::string_view currency){
    if(currency=="USD")return "MM/DD/YYYY";
    if(currency=="GBP" || currency=="AUD" || currency=="NZD")return "DD/MM/YYYY";
    return "YYYY-MM-DD";
}
}
