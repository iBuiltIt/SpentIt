#include <spentit/import_view.hpp>
#include <iostream>
#include <limits>
using namespace spentit;
using namespace spentit::app;
void check(bool b){if(!b)throw std::runtime_error("Ledger reconciliation failed");}
int main(){try{
    const Currency cad("CAD");Account a{"Synthetic",cad,"2026-01-02",Money(10000,cad),false,0,true};
    std::vector<LedgerRow> rows;
    auto add=[&](int sequence,const char* date,int amount){Money total(amount,cad);rows.push_back({{"a",static_cast<std::size_t>(sequence)},sequence,{date,"Synthetic","",Transaction::imported(total,{{"Food","",total}})},true});};
    add(1,"2026-01-01",-1000);add(2,"2026-01-02",2000);add(3,"2026-01-02",-3000);
    const auto v=ledger_view(a,rows,"",0,false);check(v.rows.size()==4 && v.balances[0].units()==-1000 && v.balances[1].units()==9000 && v.balances[2].units()==11000 && v.balances[3].units()==8000 && v.ending.units()==8000);
    check(v.order==std::vector<std::size_t>{0,1,2,3});
    const auto newest=ledger_view(a,rows,"",0,true);check(newest.order==std::vector<std::size_t>{3,2,1,0} && newest.balances==v.balances);
    check(newest.balances[newest.order[0]].units()==8000 && newest.balances[newest.order[1]].units()==11000 && newest.balances[newest.order[2]].units()==9000 && newest.balances[newest.order[3]].units()==-1000);
    const auto sameDay=ledger_view(a,rows,"Food",0,true);check(sameDay.order==std::vector<std::size_t>{3,2,0} && sameDay.balances==v.balances);
    const auto payee=ledger_view(a,rows,"Food",1,true);check(payee.order==std::vector<std::size_t>{0,2,3});
    check(v.withdrawals.units()==-4000 && v.deposits.units()==12000);
    for(int col=0;col<6;++col){const auto f=ledger_view(a,rows,"Food",col,true);check(f.order.size()==3 && f.balances==v.balances && f.ending==v.ending && f.deposits.units()==2000);}
    check(decimal(Money(std::numeric_limits<std::int64_t>::min(),cad))=="-92233720368547758.08");
    std::cout<<"Import ledger reconciliation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
