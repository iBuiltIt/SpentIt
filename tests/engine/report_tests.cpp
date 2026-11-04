#include <spentit/reports.hpp>
#include <spentit/import_review.hpp>
#include <iostream>
#include <source_location>
using namespace spentit;
using namespace spentit::reports;
void check(bool ok,const std::source_location where=std::source_location::current()){if(!ok)throw std::runtime_error("Report assertion at "+std::to_string(where.line()));}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
int main(){try{
    auto range=resolve({"six_months",{},{}},"2026-01-12");check(range.start=="2025-07-01" && range.end=="2025-12-31");
    range=resolve({"last_quarter",{},{}},"2026-02-01");check(range.start=="2025-10-01" && range.end=="2025-12-31");
    range=resolve({"custom","2024-02-29","2024-02-29"},"2026-01-01");auto prior=resolve({"previous_year",{},{}},"2026-01-01",&range);check(prior.start=="2023-02-28" && prior.end=="2023-02-28");
    rejects([]{resolve({"custom","2026-02-29","2026-03-01"},"2026-01-01");});
    rejects([]{resolve({"custom","2026-03-01","2026-02-01"},"2026-01-01");});
    rejects([]{resolve({"last_year",{},{}},"0001-01-01");});
    check(resolve({"this_year",{},{}},"9999-12-31").end=="9999-12-31");
    check(resolve({"ytd",{},{}},"2026-11-04").end=="2026-11-04");
    const Currency cad("CAD"),usd("USD");
    Definition d{"report","Spending","bar",{1,2},{"food","income",""},{{"custom","2026-01-01","2026-01-31"},{"custom","2026-01-01","2026-01-01"}}};
    std::vector<Category> categories{{"food","Food"},{"income","Income"},{"","Uncategorized"}};
    std::vector<Entry> entries;
    const auto add=[&](int account,std::string category,std::int64_t units,const Currency& currency,std::string date,bool transfer=false){entries.push_back({account,"Account","batch",std::to_string(entries.size()),date,"Payee","Memo",category,entries.size(),Money(units,currency),transfer,false});};
    add(1,"food",-10000,cad,"2026-01-01");add(1,"food",2000,cad,"2026-01-31");add(1,"income",500000,cad,"2026-01-02");add(1,"",-300,cad,"2026-01-01");
    add(1,"food",-90000,cad,"2026-01-01",true);add(3,"food",-80000,cad,"2026-01-01");add(1,"food",-70000,cad,"2026-02-01");add(2,"food",-1200,usd,"2026-01-01");
    auto snapshot=calculate(d,"2026-11-04",entries,categories,{cad,usd});check(snapshot.currencies.size()==2);
    check(snapshot.currencies[0].totals[0].amount.units()==-491700);check(snapshot.currencies[0].totals[1].amount.units()==10300);check(snapshot.currencies[1].totals[0].amount.units()==1200);
    check(snapshot.currencies[0].rows[0].category.id=="food" && snapshot.currencies[0].rows[0].cells[0].amount.units()==8000);
    for(const auto& currency:snapshot.currencies)for(const auto& row:currency.rows)for(const auto& cell:row.cells){std::vector<Money> terms;for(auto index:cell.entries)terms.push_back(-snapshot.entries[index].amount);check(imports::sum(currency.currency,terms)==cell.amount);}
    d.categories={"food"};entries.clear();add(1,"food",INT64_MIN,cad,"2026-01-01");rejects([&]{calculate(d,"2026-11-04",entries,categories,{cad});});
    add(1,"food",1,cad,"2026-01-01");check(calculate(d,"2026-11-04",entries,categories,{cad}).currencies[0].totals[0].amount.units()==INT64_MAX);
    add(1,"food",INT64_MAX,cad,"2026-01-01");check(calculate(d,"2026-11-04",entries,categories,{cad}).currencies[0].totals[0].amount.units()==0);
    d.categories={"missing"};rejects([&]{calculate(d,"2026-11-04",entries,categories,{cad});});
    d.categories={"food"};entries.clear();add(1,"food",100,cad,"");rejects([&]{calculate(d,"2026-11-04",entries,categories,{cad});});
    d.categories={"auto","gas","repairs"};entries.clear();
    add(1,"auto",-10000,cad,"2026-01-01");add(1,"gas",-5000,cad,"2026-01-01");add(1,"repairs",-2500,cad,"2026-01-01");
    auto direct=calculate(d,"2026-11-04",entries,{{"auto","Auto"},{"gas","Auto: Gas"},{"repairs","Auto: Repairs"}},{cad});
    check(direct.currencies[0].rows.size()==3 && direct.currencies[0].totals[0].amount.units()==17500 && direct.currencies[0].totals[1].amount.units()==17500);
    std::cout<<"Exact report reconciliation, currencies, signs and boundaries passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
