#include <spentit/import_view.hpp>
#include <chrono>
#include <iostream>
using namespace spentit;
using namespace spentit::app;
int main(int argc,char** argv){try{
    const std::size_t count=argc>1?std::stoul(argv[1]):100000;if(count>200000)throw std::invalid_argument("Use up to 200000 synthetic benchmark rows");
    const Currency cad("CAD");const Money amount(-100,cad);Account account{"Synthetic search workload",cad,"2026-01-01",Money(0,cad),false,0,true};
    std::vector<LedgerRow> rows;rows.reserve(count);std::size_t expected=0;
    for(std::size_t i=0;i<count;++i){const auto month=i%12+1;const auto date="2026-"+std::string(month<10?"0":"")+std::to_string(month)+"-01";const bool target=i%10==0;if(target&&(month==5 || month==6))++expected;
        rows.push_back({{"synthetic",i},static_cast<std::int64_t>(i+1),{date,target?"Target Shop":"Other Shop","Synthetic memo",Transaction::imported(amount,{{"Food","Synthetic split memo",amount}})},true});
    }
    for(const auto* text:{"period:may-june target","period:may-june regex:\"^Target Shop$\"","QIF: period:may-june Target"}){
        const auto start=std::chrono::steady_clock::now();LedgerSearch query(text,2026);
        const auto view=search_ledger(account,rows,query,0,false,[](const imports::Occurrence& id)->std::optional<std::string_view>{return id.ordinal%10==0?"PTarget Shop\n^\n":"POther Shop\n^\n";});
        if(view.order.size()!=expected || view.withdrawals.units()!=-static_cast<std::int64_t>(expected)*100 || view.deposits.units()!=0 || view.ending.units()!=-static_cast<std::int64_t>(count)*100)throw std::runtime_error("Search workload reconciliation failed");
        std::cout<<count<<" rows, "<<text<<": "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<" ms, "<<expected<<" matches\n";
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
