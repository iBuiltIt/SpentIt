#include <spentit/import_store.hpp>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace {
const Currency cad("CAD");
void check(bool condition,const std::source_location at=std::source_location::current()) {
    if(!condition)throw std::runtime_error("Review assertion at line "+std::to_string(at.line()));
}
template<class F> void rejects(F work){bool rejected=false;try{work();}catch(const std::exception&){rejected=true;}check(rejected);}
struct Directory {
    std::filesystem::path path;
    Directory(){auto pattern=(std::filesystem::temp_directory_path()/"spentit-review-XXXXXX").string();const auto p=mkdtemp(pattern.data());if(!p)throw std::runtime_error("Temporary directory failed");path=std::filesystem::canonical(p);}
    ~Directory(){std::error_code error;std::filesystem::remove_all(path,error);}
};
void run() {
    Directory directory;
    const auto path=directory.path/"document";
    {
        auto store=ImportStore::create(path,"synthetic-review","Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));
        std::string source;
        for(int i=1;i<=2000;++i)source+="D1/1/2026\nT"+std::to_string(i)+"\nPSynthetic\n^\n";
        std::size_t reviewed=0,validated=0;
        store.observer([&](ImportPhase phase){if(phase==ImportPhase::review_row)++reviewed;if(phase==ImportPhase::validate_row)++validated;});
        store.ingest("first",source,qif::DateOrder::dmy);check(reviewed==2000);
        auto edit=*store.row({"first",5}).draft;edit.memo="Corrected memo";
        reviewed=0;store.save({"first",5},0,edit);check(reviewed==1);
        reviewed=0;validated=0;const auto receipt=store.commit("first",1,"accept-first");
        check(reviewed==0 && validated==0 && receipt.included==2000 && receipt.after==Money(200100000,cad));
        check(store.commit("first",1,"accept-first").after==receipt.after);
        store.ingest("duplicate","D1/1/2026\nT1\nPSynthetic\n^\nD2/1/2026\nT9\nPIndependent\n^\n",qif::DateOrder::dmy);
        store.decide({"duplicate",0},0,{imports::Disposition::duplicate,imports::Occurrence{"first",0}});
        const auto old=store.committed_entry({"first",0});auto correction=old.current.draft;correction.memo="Target changed";
        const auto preview=store.preview_committed_change({app::ChangeKind::correct,{"first",0},correction,{}});
        reviewed=0;store.apply_committed_change("change-target",preview.request,preview.expected,preview.preview_sequence);
        check(reviewed==1 && store.row({"duplicate",0}).review_stale);
        reviewed=0;rejects([&]{store.commit("duplicate",1,"stale");});check(reviewed==0);
        store.decide({"duplicate",0},1,{imports::Disposition::duplicate,imports::Occurrence{"first",0}});
        // An interrupted edit must not leave its review result behind, even if
        // the next edit reuses the rolled-back version number.
        auto changed=*store.row({"duplicate",1}).draft;changed.memo="Must roll back";
        store.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw std::runtime_error("Synthetic failure");});
        rejects([&]{store.save({"duplicate",1},2,changed);});
        store.observer({});check(store.row({"duplicate",1}).draft->memo!="Must roll back");
        changed.memo="Retained edit";store.save({"duplicate",1},2,changed);
        store.verify();
    }
    {
        auto store=ImportStore::open(path,"synthetic-review");
        std::size_t reviewed=0,validated=0;store.observer([&](ImportPhase phase){if(phase==ImportPhase::review_row)++reviewed;if(phase==ImportPhase::validate_row)++validated;});
        const auto receipt=store.commit("duplicate",3,"accept-duplicate");
        check(reviewed==0 && validated==0 && receipt.included==1 && receipt.excluded==1);
        check(store.committed_entry({"duplicate",1}).current.draft.memo=="Retained edit");
        store.verify();
    }
    {
        auto store=ImportStore::create(directory.path/"grouped","synthetic-review","Checking",cad,"2026-01-01");
        store.opening(0,"2026-01-01",Money(0,cad));
        store.create_account(2,"Visa",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));
        app::OperationContext capture,resume;
        store.prepare_import_group("group","!Account\nNChecking\nTBank\n^\n!Type:Bank\nD1/1/2026\nT-10\nL[Visa]\n^\n!Account\nNVisa\nTCCard\n^\n!Type:CCard\nD1/1/2026\nT10\nL[Checking]\n^\n",qif::DateOrder::dmy,{{0,1,{}},{1,2,{}}},capture);
        store.resume_import_group("group",resume);
        check(store.import_groups().front().unresolved==0);
        std::size_t validated=0;
        store.observer([&](ImportPhase phase){if(phase==ImportPhase::validate_row)++validated;});
        store.select_account(1);store.commit("group-account-0",1,"accept-checking");check(validated==0);
        store.select_account(2);store.commit("group-account-1",1,"accept-visa");check(validated==0);
        store.verify();
    }

}
}
int main(){try{run();std::cout<<"Mutation-maintained review passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
