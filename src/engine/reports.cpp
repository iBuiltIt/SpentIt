#include <spentit/reports.hpp>
#include <spentit/date_format.hpp>
#include <spentit/qif.hpp>
#include "exact_sum.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <set>

namespace spentit::reports {
namespace {
using namespace std::chrono;
year_month_day date(std::string_view s){
    (void)parse_display_date(s,"YYYY-MM-DD");
    return year(std::stoi(std::string(s.substr(0,4))))/month(static_cast<unsigned>(std::stoi(std::string(s.substr(5,2)))))/day(static_cast<unsigned>(std::stoi(std::string(s.substr(8,2)))));
}
std::string iso(year_month_day d){
    if(!d.ok() || int(d.year())<1 || int(d.year())>9999)throw std::invalid_argument("The period falls outside supported dates (0001–9999). Choose a custom range.");
    char buffer[11];std::snprintf(buffer,sizeof(buffer),"%04d-%02u-%02u",int(d.year()),unsigned(d.month()),unsigned(d.day()));return buffer;
}
year_month_day last(year_month m){return year_month_day(m/last_spec{});}
year_month_day earlier_year(year_month_day d,years offset){
    const auto result=d-offset;
    return result.ok()?result:last(result.year()/result.month());
}
Range resolve(const Period& p,std::string_view today,const Range* anchor,years offset){
    using namespace std::chrono;
    const auto now=date(today);auto start=now,end=now;std::string label;
    const auto month=now.year()/now.month();
    if(p.preset=="custom"){start=date(p.start);end=date(p.end);label="Custom";}
    else if(p.preset=="ytd"){start=now.year()/January/1;label="Year to date";}
    else if(p.preset=="this_year"){start=now.year()/January/1;end=now.year()/December/31;label="This year";}
    else if(p.preset=="last_year"){start=(now.year()-years(1))/January/1;end=(now.year()-years(1))/December/31;label="Last year";}
    else if(p.preset=="prior_calendar_year"){
        const auto year=(anchor?date(anchor->start):now).year()-years(1);
        start=year/January/1;end=year/December/31;label="Previous year";
    }
    else if(p.preset=="this_month"){start=month/1;end=last(month);label="This month";}
    else if(p.preset=="last_month"){start=(month-months(1))/1;end=last(month-months(1));label="Previous calendar month";}
    else if(p.preset=="prior_calendar_month"){
        const auto reference=anchor?date(anchor->start):now;
        const auto previous=(reference.year()/reference.month())-months(1);
        start=previous/1;end=last(previous);label="Previous calendar month";
    }
    else if(p.preset=="six_months"){start=(month-months(6))/1;end=last(month-months(1));label="Last 6 complete months";}
    else if(p.preset=="this_quarter" || p.preset=="last_quarter"){
        auto first=now.year()/std::chrono::month(((unsigned(now.month())-1)/3)*3+1);
        if(p.preset=="last_quarter")first-=months(3);
        start=first/1;end=last(first+months(2));label=p.preset=="this_quarter"?"This quarter":"Last quarter";
    } else if(anchor && (p.preset=="previous_year" || p.preset=="prior_year_same_period")){
        start=earlier_year(date(anchor->start),offset);
        end=earlier_year(date(anchor->end),offset);
        label=p.preset=="previous_year"?"Same period last year":"Same period previous year";
    }
    else throw std::invalid_argument("Choose a valid period preset.");
    if(sys_days(start)>sys_days(end))throw std::invalid_argument("The start date must be on or before the end date.");
    return {label,iso(start),iso(end)};
}
}
std::optional<std::size_t> period_anchor(const Period& period,std::size_t index){
    if(index==0)return std::nullopt;
    if(period.preset=="previous_year")return 0;
    if(period.preset=="prior_calendar_year" || period.preset=="prior_calendar_month" || period.preset=="prior_year_same_period")
        return index-1;
    return std::nullopt;
}
Range resolve_period(std::span<const Period> periods,std::string_view today,std::size_t index){
    if(index>=periods.size() || index>=32)throw std::invalid_argument("Choose a main period and no more than 31 comparisons.");
    const auto& period=periods[index];
    auto anchor_index=period_anchor(period,index);
    if(!anchor_index)return resolve(period,today,nullptr,years(0));
    if(period.preset=="prior_year_same_period"){
        // Shift from the first range in the chain so a leap-day cutoff can recur.
        while(*anchor_index>0 && periods[*anchor_index].preset=="prior_year_same_period")
            --*anchor_index;
    }
    const auto anchor=resolve_period(periods,today,*anchor_index);
    const auto offset=period.preset=="previous_year"?1:static_cast<int>(index-*anchor_index);
    return resolve(period,today,&anchor,years(offset));
}
void validate(const Definition& d){
    const auto validText=[](const std::string& s,std::size_t limit){return s.size()<=limit && qif::valid_utf8(s) && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;});};
    if(d.id.empty() || !validText(d.id,128))throw std::invalid_argument("Invalid report identity.");
    if(d.name.find_first_not_of(" \t")==std::string::npos || !validText(d.name,256))throw std::invalid_argument("Enter a report name of up to 256 characters.");
    if(d.graph!="none" && d.graph!="bar" && d.graph!="pie" && d.graph!="grouped_bar")throw std::invalid_argument("Choose None, separate or grouped Bar, or Pie.");
    if(d.periods.empty() || d.periods.size()>32)throw std::invalid_argument("Choose a main period and no more than 31 comparisons.");
    if(d.accounts.size()>10000 || d.categories.size()>100000)throw std::invalid_argument("Too many report selections.");
    if(std::set<std::int64_t>(d.accounts.begin(),d.accounts.end()).size()!=d.accounts.size() || std::any_of(d.accounts.begin(),d.accounts.end(),[](auto id){return id<=0;}))throw std::invalid_argument("Invalid account selection.");
    if(std::set<std::string>(d.categories.begin(),d.categories.end()).size()!=d.categories.size())throw std::invalid_argument("Duplicate category selection.");
    for(const auto& id:d.categories)if(!validText(id,256))throw std::invalid_argument("Invalid category selection.");
    for(std::size_t i=0;i<d.periods.size();++i)(void)resolve_period(d.periods,"2026-11-04",i);
}
Snapshot calculate(const Definition& d,std::string_view today,std::vector<Entry> entries,const std::vector<Category>& categories,const std::vector<Currency>& currencies,const std::function<void()>& checkpoint){
    validate(d);Snapshot out{d,{},std::move(entries),{}};
    for(std::size_t i=0;i<d.periods.size();++i)out.periods.push_back(resolve_period(d.periods,today,i));
    const std::set<std::int64_t> accounts(d.accounts.begin(),d.accounts.end());
    std::map<std::string,Category> selected;
    for(const auto& id:d.categories){const auto found=std::find_if(categories.begin(),categories.end(),[&](const auto& c){return c.id==id;});if(found==categories.end())throw std::invalid_argument("A selected category is no longer available. Edit the report selection.");selected.emplace(id,*found);}
    std::set<std::string> seen;
    for(const auto& currency:currencies){
        if(!seen.insert(currency.definition_id()).second)continue;
        Result result{currency,{}, {}};
        for(const auto& [id,cat]:selected){(void)id;Row row{cat,{}};for(std::size_t p=0;p<out.periods.size();++p)row.cells.push_back({Money(0,currency),{}});result.rows.push_back(std::move(row));}
        for(std::size_t p=0;p<out.periods.size();++p)result.totals.push_back({Money(0,currency),{}});
        std::map<std::string,std::size_t> rowIndex;for(std::size_t r=0;r<result.rows.size();++r)rowIndex.emplace(result.rows[r].category.id,r);
        for(std::size_t i=0;i<out.entries.size();++i){
            if(checkpoint && i%256==0)checkpoint();
            const auto& e=out.entries[i];if(e.transfer || !accounts.contains(e.account) || e.amount.currency()!=currency || !selected.contains(e.category))continue;
            if(e.date.empty())throw std::invalid_argument("A selected transaction has no valid date. Repair it before showing this report.");
            (void)date(e.date);
            for(std::size_t p=0;p<out.periods.size();++p)if(e.date>=out.periods[p].start && e.date<=out.periods[p].end){result.rows[rowIndex.at(e.category)].cells[p].entries.push_back(i);result.totals[p].entries.push_back(i);}
        }
        const auto sum=[&](Cell& c){std::vector<detail::Term> terms;terms.reserve(c.entries.size());for(auto i:c.entries)terms.push_back({out.entries[i].amount.units(),true});c.amount=Money(detail::exact_sum(terms),currency);};
        for(auto& row:result.rows)for(auto& c:row.cells)sum(c);
        for(auto& c:result.totals)sum(c);
        // A category that is exactly zero in every shown period takes no table or chart space. Its allocations stay in the period total.
        result.rows.erase(std::remove_if(result.rows.begin(),result.rows.end(),[](const Row& row){return std::all_of(row.cells.begin(),row.cells.end(),[](const Cell& cell){return cell.amount.units()==0;});}),result.rows.end());
        std::stable_sort(result.rows.begin(),result.rows.end(),[](const Row& a,const Row& b){return a.cells.front().amount.units()!=b.cells.front().amount.units()?a.cells.front().amount.units()>b.cells.front().amount.units():a.category.name<b.category.name;});
        out.currencies.push_back(std::move(result));
    }
    return out;
}
}
