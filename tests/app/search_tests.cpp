#include <spentit/import_view.hpp>
#include <iostream>
#include <source_location>
#include <array>
using namespace spentit;
using namespace spentit::app;
void check(bool value,const std::source_location& at=std::source_location::current()){if(!value)throw std::runtime_error("Search assertion at line "+std::to_string(at.line()));}
template<class F>void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed);}
bool match(std::string_view query,std::string_view date,std::initializer_list<std::string_view> fields,bool source=false){LedgerSearch search(query,2026);return search.matches(date,fields,source);}
int main(){try{
    {const std::array<std::string_view,1> gas{"Auto: Gas"},autoOnly{"Auto"},unrelated{"Automobile Club"};
     LedgerSearch stem("category:auto",2026),exact("category-exact:auto",2026),leaf("category:\"auto: gas\"",2026);
     check(stem.matches("2026-10-28",{},false,{},gas));check(stem.matches("2026-10-28",{},false,{},autoOnly));check(!stem.matches("2026-10-28",{},false,{},unrelated));
     check(!exact.matches("2026-10-28",{},false,{},gas));check(exact.matches("2026-10-28",{},false,{},autoOnly));check(leaf.matches("2026-10-28",{},false,{},gas));}
    {LedgerSearch dates("period:04/03/2026",2026,"DD/MM/YYYY");check(dates.matches("2026-03-04",{},false));check(!dates.matches("2026-04-03",{},false));}
    check(match("coffee food","2026-05-31",{"Coffee Shop","Food: Dining"}));
    check(!match("coffee food","2026-05-31",{"Coffee Shop","Travel"}));
    check(match("\"coffee shop\"","2026-05-31",{"COFFEE SHOP"}));
    check(!match("\"coffee shop\"","2026-05-31",{"Coffee","Shop"}));
    check(match("a.b","2026-05-31",{"a.b"}));check(!match("a.b","2026-05-31",{"axb"}));
    check(match("café","2026-05-31",{"CAFÉ"}));
    check(match("regex:\"^coffee [0-9]+$\"","2026-05-31",{"Coffee 123"}));
    check(!match("regex:\"(?-i)^Coffee$\"","2026-05-31",{"coffee"}));
    for(auto q:{"period:may-june","period:20260501..20260630","period:2026-05-01..2026-06-30","period:2026/05/01..2026/06/30"}){
        check(match(q,"2026-05-01",{}));check(match(q,"2026-06-30",{}));check(!match(q,"2026-07-01",{}));check(!match(q,"2025-05-01",{}));
    }
    check(match("period:2024-02-29","2024-02-29",{}));check(match("period:2026","2026-12-31",{}));
    for(auto q:{"period:2026-02-29","period:01/02/2026","period:june-may","period:20261001..20260930","period:may period:june","\"unclosed","regex:\"(\"","regex:abc","coffee QIF:","period:"})rejects([&]{LedgerSearch query(q,2026);});
    check(match("QIF: regex:\"^POriginal\"","2026-05-01",{"POriginal\n^\n"},true));
    check(!match("QIF:","2026-05-01",{},false));
    check(match("\"QIF:\"","2026-05-01",{"QIF:"}));
    check(match("\"regex:word\"","2026-05-01",{"regex:word"}));
    {LedgerSearch limited("regex:\"^(a+)+$\"",2026);const auto subject=std::string(10000,'a')+"!";const std::array<std::string_view,1> fields{subject};bool failed=false;try{limited.matches("2026-05-01",fields,false);}catch(const rules::RegexError& e){failed=e.kind()==rules::RegexFailure::resource_limit;}check(failed);}
    LedgerSearch cancel("coffee",2026);bool cancelled=false;try{cancel.matches("2026-05-01",{},false,[]{return true;});}catch(const rules::RegexError& e){cancelled=e.kind()==rules::RegexFailure::cancelled;}check(cancelled);
    const Currency cad("CAD");Account account{"Synthetic",cad,"2026-01-01",Money(10000,cad),false,0,true};
    std::vector<LedgerRow> rows{{{"a",0},1,{"2026-05-01","Coffee","",Transaction::imported(Money(-1000,cad),{{"Food","",Money(-700,cad)},{"Travel","",Money(-300,cad)}})},true},{{"a",1},2,{"2026-06-01","Other","",Transaction::imported(Money(2000,cad),{{"Income","",Money(2000,cad)}})},true}};
    const auto all=ledger_view(account,rows,"",0,false);
    for(int column=0;column<6;++column){LedgerSearch query("period:may-june food",2026);auto view=search_ledger(account,rows,query,column,true);check(view.order.size()==1 && view.withdrawals.units()==-1000 && view.deposits.units()==0 && view.balances==all.balances && view.ending==all.ending);}
    LedgerSearch source("QIF: original",2026);auto view=search_ledger(account,rows,source,0,false,[](const imports::Occurrence& id)->std::optional<std::string_view>{if(id.ordinal==0)return "POriginal\n^\n";return {};});check(view.order.size()==1 && view.withdrawals.units()==-1000);
    rejects([&]{LedgerSearch tooLong(std::string(4097,'x'),2026);});
    std::cout<<"Search grammar, boundaries, source scope, cancellation and reconciliation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
