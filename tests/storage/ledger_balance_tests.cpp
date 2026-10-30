#include <spentit/import_store.hpp>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <cstdlib>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace {
const Currency cad("CAD");
constexpr auto password="synthetic-ledger-balance";
struct Directory {
    std::filesystem::path path;
    Directory(){auto name=(std::filesystem::temp_directory_path()/"spentit-balance-XXXXXX").string();auto p=mkdtemp(name.data());if(!p)throw std::runtime_error("Temporary directory failed");path=std::filesystem::canonical(p);}
    ~Directory(){std::error_code error;std::filesystem::remove_all(path,error);}
};
void check(bool value,const std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error("Balance assertion at line "+std::to_string(at.line()));
}
void reconcile(ImportStore& store,std::int64_t expected) {
    std::vector<Money> amounts{store.account().opening};
    for(const auto& row:store.current_ledger())amounts.push_back(row.draft.transaction.effective_total());
    check(store.balance()==Money(expected,cad));
    check(imports::sum(cad,amounts)==store.balance());
    bool found=false;
    for(const auto& [id,total]:store.balances())if(id==store.account().identity){found=true;check(total==store.balance());}
    check(found);
}
void run() {
    Directory directory;const auto path=directory.path/"document";
    {
        auto store=ImportStore::create(path,password,"Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(10000,cad));
        store.ingest("first","D1/1/2026\nT-10\nPSynthetic\nSFood\n$-6\nSFee\n$-5\nSRefund\n$1\n^\n",qif::DateOrder::dmy);
        reconcile(store,9000);check(!store.current_ledger().front().committed);
        store.commit("first",0,"accept-first");reconcile(store,9000);check(store.current_ledger().front().committed);
        store.ingest("review","D1/1/2026\nT-10\nPSynthetic\n^\nD2/1/2026\nTbad\nPRepair\n^\nD3/1/2026\nT-2\nPValid\n^\n",qif::DateOrder::dmy);
        check(store.row({"review",0}).decision.disposition==imports::Disposition::unresolved);
        check(!store.projected_balance()); // Readiness is separate from ledger totals.
        reconcile(store,7800);check(store.current_ledger().size()==3);
        store.decide({"review",0},0,{imports::Disposition::duplicate,imports::Occurrence{"first",0}});
        reconcile(store,8800);check(store.current_ledger().size()==2);
        store.verify();store.backup_to(directory.path/"backup",password);
    }
    {
        auto store=ImportStore::open(directory.path/"backup",password);reconcile(store,8800);
        store.create_account(2,"Visa",cad,"2026-01-01");
        app::TransferRequest request{};request.action=app::TransferAction::manual;request.link="payment";
        request.expected_sequence=store.transfer_sequence();request.destination=2;request.amount=Money(-1000,cad);request.date="2026-01-04";
        store.apply_transfer("create-payment",request);reconcile(store,7800);
        store.select_account(2);reconcile(store,1000);
        store.ingest("visa","D4/1/2026\nT10\nPPayment\n^\n",qif::DateOrder::dmy);
        request={};request.action=app::TransferAction::propose_match;request.link="payment";request.expected_sequence=store.transfer_sequence();
        request.evidence=transfers::BankEvidence{{2,{"visa",0},"source-0"},Money(1000,cad),"2026-01-04"};
        store.apply_transfer("match-payment",request);reconcile(store,1000);check(store.current_ledger().size()==1);
        store.verify();
    }
    {
        auto store=ImportStore::create(directory.path/"currencies",password,"Checking",cad,"2026-01-01");
        store.ingest("cad","D1/1/2026\nT10.01\nPSynthetic\n^\n",qif::DateOrder::dmy);
        store.create_account(2,"Japan",Currency("JPY"),"2026-01-01");store.select_account(2);
        store.ingest("jpy","D1/1/2026\nT10\nPSynthetic\n^\n",qif::DateOrder::dmy);
        const auto balances=store.balances();check(balances.size()==2);
        check(balances[0].second==Money(1001,cad));check(balances[1].second==Money(10,Currency("JPY")));
    }
    {
        auto store=ImportStore::create(directory.path/"overflow",password,"Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(INT64_MAX,cad));
        store.ingest("overflow","D1/1/2026\nT0.01\nPSynthetic\n^\n",qif::DateOrder::dmy);
        bool refused=false;try{(void)store.balance();}catch(const FinancialError&){refused=true;}
        check(refused);check(store.current_ledger().size()==1);
    }

}
}
int main(){try{run();std::cout<<"Ledger balance checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
