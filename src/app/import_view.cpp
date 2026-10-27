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
std::string_view category(const LedgerRow& r) {
    if(r.draft.transaction.allocations().size()==1)return r.draft.transaction.allocations()[0].category;
    return "Split";
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
        const auto& r=out.rows[i]; const auto& amount=r.draft.transaction.effective_total();
        out.ending=out.ending+amount; out.balances.push_back(out.ending);
        bool match=needle.empty();
        if(!match) {
            for(const auto* text:{&r.draft.date,&r.draft.payee,&r.draft.memo}) match|=folded(*text).find(needle)!=std::string::npos;
            for(const auto& allocation:r.draft.transaction.allocations()) match|=folded(allocation.category).find(needle)!=std::string::npos || folded(allocation.memo).find(needle)!=std::string::npos;
        }
        if(!match) continue;
        out.order.push_back(i); (amount.units()<0?withdrawals:deposits).push_back(amount);
    }
    out.withdrawals=imports::sum(account.currency,withdrawals); out.deposits=imports::sum(account.currency,deposits);
    std::sort(out.order.begin(),out.order.end(),[&](std::size_t a,std::size_t b){
        const auto& x=out.rows[a]; const auto& y=out.rows[b]; int cmp=0;
        if(column<3) {
            const std::string_view l=column==0?std::string_view(x.draft.date):column==1?std::string_view(x.draft.payee):category(x);
            const std::string_view r=column==0?std::string_view(y.draft.date):column==1?std::string_view(y.draft.payee):category(y); cmp=l.compare(r);
        } else {
            auto value=[&](std::size_t i){const auto units=out.rows[i].draft.transaction.effective_total().units();return column==3?std::min<std::int64_t>(0,units):column==4?std::max<std::int64_t>(0,units):out.balances[i].units();};
            const auto l=value(a),r=value(b);cmp=l<r?-1:l>r?1:0;if(column==3)cmp=-cmp;
        }
        return cmp==0?x.sequence<y.sequence:descending?cmp>0:cmp<0;
    });return out;
}
LedgerView search_ledger(const Account& account,std::vector<LedgerRow> rows,LedgerSearch& query,int column,bool descending,
                         const SourceLookup& source,const std::function<bool()>& cancelled) {
    const auto check_cancel=[&]{if(cancelled && cancelled())throw rules::RegexError(rules::RegexFailure::cancelled,"Search cancelled");};
    check_cancel();
    auto out=ledger_view(account,std::move(rows),"",column,descending);
    check_cancel();if(query.empty())return out;
    std::vector<std::size_t> order;std::vector<Money> withdrawals,deposits;
    for(const auto index:out.order){
        const auto& row=out.rows[index];
        std::vector<std::string> amounts;amounts.reserve(row.draft.transaction.allocations().size()+2);
        amounts.push_back(decimal(row.draft.transaction.effective_total()));amounts.push_back(decimal(out.balances[index]));
        for(const auto& a:row.draft.transaction.allocations())amounts.push_back(decimal(a.amount));
        std::vector<std::string_view> fields;std::optional<std::string_view> raw;
        if(query.source_mode()) {if(source && !row.identity.batch.empty())raw=source(row.identity);if(raw)fields.push_back(*raw);}
        else {
            fields={row.draft.date,row.draft.payee,row.draft.memo,account.currency.code()};
            for(const auto& amount:amounts)fields.push_back(amount);
            for(const auto& a:row.draft.transaction.allocations()){fields.push_back(a.category);fields.push_back(a.memo);}
        }
        if(!query.matches(row.draft.date,fields,raw.has_value(),cancelled))continue;
        order.push_back(index);const auto total=row.draft.transaction.effective_total();(total.units()<0?withdrawals:deposits).push_back(total);
    }
    out.order=std::move(order);out.withdrawals=imports::sum(account.currency,withdrawals);out.deposits=imports::sum(account.currency,deposits);return out;
}
} // namespace spentit::app
