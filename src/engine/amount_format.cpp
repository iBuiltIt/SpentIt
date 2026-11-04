#include <spentit/amount_format.hpp>
namespace spentit {
void validate_amount_format(const AmountFormat& format) {
    if(format.style!="comma-dot" && format.style!="dot-comma" && format.style!="space-comma")
        throw std::invalid_argument("Choose an available amount format.");
}
AmountFormat suggested_amount_format(std::string_view currency) {
    // Product defaults, not a claim that each currency has a unique locale.
    if(currency=="EUR" || currency=="BRL" || currency=="ARS" || currency=="CLP" || currency=="COP" || currency=="IDR" || currency=="TRY" || currency=="DKK")return {true,"dot-comma"};
    if(currency=="SEK" || currency=="NOK" || currency=="PLN" || currency=="CZK" || currency=="RUB" || currency=="HUF")return {true,"space-comma"};
    return {true,"comma-dot"};
}
std::string display_amount(const Money& value,const AmountFormat& format) {
    validate_amount_format(format);
    const auto units=value.units();
    const auto magnitude=units<0?std::uint64_t{0}-static_cast<std::uint64_t>(units):static_cast<std::uint64_t>(units);
    auto digits=std::to_string(magnitude);const auto scale=value.currency().scale();
    if(digits.size()<=scale)digits.insert(0,scale+1-digits.size(),'0');
    const auto integerSize=digits.size()-scale;
    const char decimal=format.enabled && format.style!="comma-dot"?',':'.';
    if(scale)digits.insert(integerSize,1,decimal);
    if(format.enabled){
        const char group=format.style=="comma-dot"?',':format.style=="dot-comma"?'.':' ';
        for(std::size_t i=integerSize;i>3;){i-=3;digits.insert(i,1,group);}
    }
    return (units<0?"-":"")+digits;
}
std::string normalize_amount_expression(std::string_view input,const AmountFormat& format) {
    validate_amount_format(format);
    if(input.size()>4096)throw FinancialError(ErrorCode::invalid_decimal,"Calculation is too long. Use at most 4096 characters.");
    if(!format.enabled)return std::string(input);
    const char decimal=format.style=="comma-dot"?'.':',';
    const char group=format.style=="comma-dot"?',':format.style=="dot-comma"?'.':' ';
    const auto digit=[](char c){return c>='0' && c<='9';};
    const auto invalid=[](){throw FinancialError(ErrorCode::invalid_decimal,"Check the amount separators against the format in Settings.");};
    std::string out;
    for(std::size_t i=0;i<input.size();){
        if(!digit(input[i]) && input[i]!=decimal){if(input[i]==group && group!=' ')invalid();out+=input[i++];continue;}
        bool fraction=false,grouped=false;std::size_t segment=0;
        while(i<input.size()){
            const char c=input[i];
            if(digit(c)){out+=c;++segment;++i;continue;}
            if(c==group){
                // Spaces beside operators are expression whitespace, not grouping.
                if(group==' '){std::size_t next=i;while(next<input.size() && input[next]==' ')++next;if(next==input.size() || !digit(input[next]))break;}
                if(fraction || segment==0 || (grouped?segment!=3:segment>3))invalid();
                grouped=true;segment=0;++i;continue;
            }
            if(c==decimal){
                if(fraction || (grouped && segment!=3))invalid();
                fraction=true;segment=0;out+='.';++i;continue;
            }
            break;
        }
        if(grouped && !fraction && segment!=3)invalid();
    }
    return out;
}
Money evaluate_amount(std::string_view expression,Currency currency,const AmountFormat& format){
    return Money::evaluate(normalize_amount_expression(expression,format),std::move(currency));
}
}
