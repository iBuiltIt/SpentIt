#include <spentit/qif.hpp>
#include <spentit/import_review.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace {
void check(const spentit::qif::Result& result, std::size_t size) {
    if(result.diagnostic && result.diagnostic->offset>size) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
    std::size_t previous=0;
    for(const auto& record:result.records) {
        if(record.begin<previous || record.begin>record.end || record.end>size) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
        previous=record.end;
        if(record.diagnostic && record.diagnostic->offset>size) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
        if(record.transaction) {
            if(record.diagnostic || !record.original_amount || record.repairable) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
            const auto& t=*record.transaction;
            if(t.original_imported_amount()!=record.original_amount || t.effective_total()!=*record.original_amount) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
            std::vector<spentit::Money> parts;
            for(const auto& a:t.allocations()) parts.push_back(a.amount);
            if(spentit::imports::sum(t.effective_total().currency(),parts)!=t.effective_total()) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
        }
    }
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data),size);
    const auto document=spentit::qif::index_document(input);
    if(document.diagnostic && document.diagnostic->offset>size) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
    // Failed indexing may retain a partially scanned section; only successful
    // indexes are admitted to parse_account.
    if(!document.diagnostic) for(const auto& section:document.sections)
        if(section.account>=document.accounts.size() || section.begin>section.end || section.end>size) {std::fprintf(stderr,"Harness invariant line %d\n",__LINE__);std::abort();}
    // Exercise integral, ordinary and three-decimal currencies without guessing scale.
    for(const auto code:{"JPY","CAD","KWD"}) {
        const spentit::Currency currency(code);
        for(const auto order:{spentit::qif::DateOrder::dmy,spentit::qif::DateOrder::mdy}) {
            check(spentit::qif::parse(input,order,currency),size);
            if(!document.diagnostic)
                for(std::size_t account=0;account<document.accounts.size();++account)
                    check(spentit::qif::parse_account(input,document,account,order,currency),size);
        }
    }
    return 0;
}
