#include <spentit/date_format.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace spentit;
void check(bool yes){if(!yes)throw std::runtime_error("Date format assertion failed");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}check(failed);}
int main(){try{
    std::array<std::string,3> tokens{"DD","MM","YYYY"};
    do {for(const auto separator:{"","-","/","."}){
        const auto format=tokens[0]+separator+tokens[1]+separator+tokens[2];
        for(const auto iso:{"0001-01-01","2000-02-29","2026-10-28","9999-12-31"})check(parse_display_date(display_date(iso,format),format)==iso);
    }}while(std::next_permutation(tokens.begin(),tokens.end()));
    check(display_date("2026-03-04","DD/MM/YYYY")=="04/03/2026");
    check(parse_display_date("04/03/2026","MM/DD/YYYY")=="2026-04-03");
    for(const auto pattern:{"YY-MM-DD","YYYY-M-D","YYYY-MM-MM","YYYY-MM/DD","YYYY-MM-DDx","YYYY--MM-DD","yyyy-MM-dd",""})rejects([&]{validate_date_format(pattern);});
    for(const auto date:{"29/02/2026","31/04/2026","01/01/0000","1/01/2026","01/1/2026","01-01-2026","01/01/2026extra"," 01/01/2026"})rejects([&]{parse_display_date(date,"DD/MM/YYYY");});
    rejects([]{display_date("2026-02-29","YYYYMMDD");});
    check(suggested_date_format("CAD")=="YYYY-MM-DD" && suggested_date_format("USD")=="MM/DD/YYYY" && suggested_date_format("GBP")=="DD/MM/YYYY" && suggested_date_format("EUR")=="YYYY-MM-DD");
    std::cout<<"Strict calendar-date format round trips passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
