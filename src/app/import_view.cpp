#include <spentit/import_view.hpp>
#include <algorithm>
#include <numeric>
namespace spentit::app {
std::string decimal(const Money& value) {
    const auto units=value.units();
    const auto magnitude=units<0 ? std::uint64_t{0}-static_cast<std::uint64_t>(units) : static_cast<std::uint64_t>(units);
    std::string digits=std::to_string(magnitude); const auto scale=value.currency().scale();
    if(scale) { if(digits.size()<=scale) digits.insert(0,scale+1-digits.size(),'0'); digits.insert(digits.size()-scale,1,'.'); }
    return (units<0?"-":"")+digits;
}
namespace {
std::string folded(std::string text) {
    for(char& c:text) if(c>='A' && c<='Z') c=static_cast<char>(c+('a'-'A'));
    return text;
}
std::string category(const LedgerRow& r) {
    return r.draft.transaction.allocations().size()==1 ? r.draft.transaction.allocations()[0].category : "Split";
}
}
LedgerView ledger_view(const Account& account,std::vector<LedgerRow> rows,std::string_view literal,int column,bool descending) {
    if(column<0 || column>5 || literal.size()>256 || !qif::valid_utf8(literal)) throw std::invalid_argument("Invalid ledger query");
    rows.push_back({{"",0},0,{account.opening_date,"Opening Balance","",Transaction::manual(account.opening,{{"Opening Balance","",account.opening}})},true});
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.draft.date==b.draft.date?a.sequence<b.sequence:a.draft.date<b.draft.date;});
    const Money zero(0,account.currency);
    LedgerView out{std::move(rows),{},{},zero,zero,zero}; std::vector<Money> withdrawals,deposits;
    const auto needle=folded(std::string(literal));
    for(std::size_t i=0;i<out.rows.size();++i) {
        const auto& r=out.rows[i]; const auto amount=r.draft.transaction.effective_total();
        out.ending=out.ending+amount; out.balances.push_back(out.ending);
        bool match=needle.empty();
        for(const auto* text:{&r.draft.date,&r.draft.payee,&r.draft.memo}) match|=folded(*text).find(needle)!=std::string::npos;
        for(const auto& allocation:r.draft.transaction.allocations()) match|=folded(allocation.category).find(needle)!=std::string::npos || folded(allocation.memo).find(needle)!=std::string::npos;
        if(!match) continue;
        out.order.push_back(i); (amount.units()<0?withdrawals:deposits).push_back(amount);
    }
    out.withdrawals=imports::sum(account.currency,withdrawals); out.deposits=imports::sum(account.currency,deposits);
    std::sort(out.order.begin(),out.order.end(),[&](std::size_t a,std::size_t b){
        const auto& x=out.rows[a]; const auto& y=out.rows[b]; int cmp=0;
        if(column<3) {
            const auto l=column==0?x.draft.date:column==1?x.draft.payee:category(x);
            const auto r=column==0?y.draft.date:column==1?y.draft.payee:category(y); cmp=l.compare(r);
        } else {
            auto value=[&](std::size_t i){const auto units=out.rows[i].draft.transaction.effective_total().units();return column==3?std::min<std::int64_t>(0,units):column==4?std::max<std::int64_t>(0,units):out.balances[i].units();};
            const auto l=value(a),r=value(b);cmp=l<r?-1:l>r?1:0;if(column==3)cmp=-cmp;
        }
        return cmp==0?x.sequence<y.sequence:descending?cmp>0:cmp<0;
    });return out;
}
} // namespace spentit::app
