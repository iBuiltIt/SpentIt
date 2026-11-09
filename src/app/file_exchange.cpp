#include <spentit/file_exchange.hpp>
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
namespace spentit::app {
namespace {
struct RestoreAccount {
    ImportRepository& repo;std::int64_t home;
    explicit RestoreAccount(ImportRepository& r):repo(r),home(r.account_selected()?r.account().identity:0){}
    void restore(){if(home)repo.select_account(home);else repo.release_account_selection();}
    ~RestoreAccount(){try{restore();}catch(...) {}}
};
std::string field(std::string_view value) {return qif_export_text(value);}
std::string decimal(const Money& value){AmountFormat format;format.enabled=false;return display_amount(value,format);}
}
std::string qif_export_text(std::string_view value) {
    if(!qif::valid_utf8(value))throw std::invalid_argument("QIF export contains invalid text.");
    std::string result;result.reserve(value.size());bool replacing=false;
    for(std::size_t i=0;i<value.size();) {
        const auto start=i;unsigned code=static_cast<unsigned char>(value[i++]);unsigned trailing=0;
        if(code>=0xf0){code&=7;trailing=3;}else if(code>=0xe0){code&=15;trailing=2;}else if(code>=0xc0){code&=31;trailing=1;}
        while(trailing--)code=(code<<6)|(static_cast<unsigned char>(value[i++])&63);
        const bool control=code<32 || (code>=0x7f && code<=0x9f) || code==0x2028 || code==0x2029;
        if(control){if(!replacing)result.push_back(' ');replacing=true;}
        else {result.append(value.substr(start,i-start));replacing=false;}
    }
    return result;
}
std::vector<QifExportAccount> qif_export_accounts(ImportRepository& repo,const OperationContext& context) {
    RestoreAccount restore(repo);std::vector<QifExportAccount> result;
    auto available=repo.accounts();const auto archived=repo.archived_accounts();available.insert(available.end(),archived.begin(),archived.end());
    for(const auto& account:available){
        context.checkpoint();repo.select_account(account.identity);std::string first,last;
        for(const auto& row:repo.current_ledger()){context.checkpoint();if(first.empty() || row.draft.date<first)first=row.draft.date;if(row.draft.date>last)last=row.draft.date;}
        result.push_back({account,first.empty()?account.opening_date:first,last.empty()?account.opening_date:last});
    }
    restore.restore();return result;
}
std::string export_qif(ImportRepository& repo,const std::vector<QifExportRange>& ranges,bool categories,bool opening,const OperationContext& context) {
    if(ranges.empty())throw std::invalid_argument("Select at least one account.");
    RestoreAccount restore(repo);auto accounts=repo.accounts();const auto archived=repo.archived_accounts();accounts.insert(accounts.end(),archived.begin(),archived.end());
    std::map<std::int64_t,std::string> names;bool changedName=false;
    for(const auto& a:accounts){auto clean=field(a.name);changedName|=clean!=a.name;names.emplace(a.identity,std::move(clean));}
    // QIF addresses transfers by account name. Normalization must not merge two
    // distinct accounts or make a transfer point to a different account.
    if(changedName)for(auto a=names.begin();a!=names.end();++a)for(auto b=std::next(a);b!=names.end();++b)
        if(account_name_matches(a->second,b->second))throw std::invalid_argument("Two account names become identical in QIF. Give those accounts distinct names before exporting.");
    using Key=std::tuple<std::int64_t,imports::Occurrence,std::string>;
    std::map<Key,std::string> transfers;
    for(const auto& value:repo.transfers())if(!value.link.unresolved && !value.link.destination_released && value.link.origin.state!=transfers::State::removed && value.link.destination.state!=transfers::State::removed){
        const auto& a=value.link.origin.endpoint;const auto& b=value.link.destination.endpoint;
        transfers[{a.account,a.entry,a.allocation}]="["+names.at(b.account)+"]";
        transfers[{b.account,b.entry,b.allocation}]="["+names.at(a.account)+"]";
    }
    std::string out;std::set<std::int64_t> seen;std::set<std::string> usedCategories;
    for(const auto& range:ranges){
        context.checkpoint();if(!seen.insert(range.account).second)throw std::invalid_argument("An account was selected more than once.");
        if(qif::date(range.start,qif::DateOrder::mdy)!=range.start || qif::date(range.end,qif::DateOrder::mdy)!=range.end || range.start>range.end)throw std::invalid_argument("Choose a valid start and end date.");
        repo.select_account(range.account);const auto account=repo.account();auto rows=repo.current_ledger();
        std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.draft.date<b.draft.date;});
        auto balance=account.opening;
        if(opening)for(const auto& row:rows){context.checkpoint();if(row.draft.date<range.start)balance=balance+row.draft.transaction.effective_total();}
        out+="!Account\nN"+field(account.name)+"\nTBank\n";
        if(opening)out+="B"+decimal(balance)+"\n";
        out+="^\n!Type:Bank\n";
        for(const auto& row:rows){
            context.checkpoint();const auto& d=row.draft;if(d.date<range.start || d.date>range.end)continue;
            out+="D"+d.date.substr(5,2)+"/"+d.date.substr(8,2)+"/"+d.date.substr(0,4)+"\nT"+decimal(d.transaction.effective_total())+"\nP"+field(d.payee)+"\nM"+field(d.memo)+"\n";
            const auto& allocations=d.transaction.allocations();
            for(const auto& a:allocations){
                std::string category;const auto transfer=transfers.find({range.account,row.identity,a.identity});
                if(transfer!=transfers.end())category=transfer->second;
                else if(auto target=repo.resolve_account_name(a.category))category="["+names.at(*target)+"]";
                else if(categories){category=a.category_binding?repo.resolve_category(a.category_binding->identity).path:a.category;if(!category.empty())usedCategories.insert(category);}
                if(allocations.size()==1 && (a.memo.empty() || a.memo==d.memo)){if(!category.empty())out+="L"+field(category)+"\n";}
                else out+="S"+field(category)+"\nE"+field(a.memo)+"\n$"+decimal(a.amount)+"\n";
            }
            out+="^\n";
            if(out.size()>64U*1024U*1024U)throw std::invalid_argument("This export is too large. Select a shorter period.");
        }
    }
    if(categories){std::string catalog;for(const auto& c:repo.category_catalog().categories)if(!c.deleted && usedCategories.contains(c.name))catalog+="N"+field(c.name)+"\n"+(c.income?"I":"E")+"\n^\n";if(!catalog.empty())out="!Type:Cat\n"+catalog+out;}
    if(out.size()>64U*1024U*1024U)throw std::invalid_argument("This export is too large. Select a shorter period.");
    restore.restore();return out;
}
}
