#include <spentit/amount_format.hpp>
#include <iostream>
#include <limits>
#include <source_location>
using namespace spentit;
void check(bool ok,const std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("Amount format failure at "+std::to_string(at.line()));}
int main(){try {
    const Currency cad("CAD");
    const AmountFormat us{true,"comma-dot"},eu{true,"dot-comma"},space{true,"space-comma"},off{false,"dot-comma"};
    check(display_amount(Money(1678200,cad),us)=="16,782.00");
    check(display_amount(Money(187600,cad),eu)=="1.876,00");
    check(display_amount(Money(-1000000,cad),space)=="-10 000,00");
    check(display_amount(Money(1000000,cad),off)=="10000.00");
    check(evaluate_amount("10,000.00+250",cad,us).units()==1025000);
    check(evaluate_amount("10.000,00+250",cad,eu).units()==1025000);
    check(evaluate_amount("10 000,00 + 250",cad,space).units()==1025000);
    check(evaluate_amount("10000,00 + 250",cad,eu).units()==1025000);
    check(evaluate_amount("(10 / 3) * 3",cad,space).units()==1000);
    check(evaluate_amount("0,001 * 1000",cad,eu).units()==100);
    for(const auto& format:{us,eu,space,off})for(const auto& currency:{cad,Currency("JPY"),Currency("KWD"),Currency("CLF")}){
        for(const auto units:{std::numeric_limits<std::int64_t>::min(),std::int64_t(-123456789),std::int64_t(-1),std::int64_t(0),std::int64_t(1),std::int64_t(123456789),std::numeric_limits<std::int64_t>::max()}){
            const Money value(units,currency);check(evaluate_amount(display_amount(value,format),currency,format)==value);
        }
    }
    auto fails=[&](std::string_view value,const AmountFormat& format){bool rejected=false;try{(void)evaluate_amount(value,cad,format);}catch(const FinancialError&){rejected=true;}check(rejected);};
    for(auto value:{"1,23","12,34,567","1234,567","1,,234","1,234,","1.23,456","1,234.5.6","1 234","1e3","10/3","1/0"})fails(value,us);
    for(auto value:{".25","1.23","12.34.567","1,234.56","1.234.567,8,9"})fails(value,eu);
    for(auto value:{"1 23","1  234","1234 567","1,2 345","1 234 56"})fails(value,space);
    fails("1,234.56",off);fails(std::string(4097,'1'),us);
    check(suggested_amount_format("CAD")==us);check(suggested_amount_format("USD")==us);check(suggested_amount_format("EUR")==eu);
    std::cout<<"Amount format passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
