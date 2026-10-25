#include <spentit/qif.hpp>
#include <cstdint>
#include <cstdlib>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    const spentit::Currency currency("CAD");
    const std::string_view input(reinterpret_cast<const char*>(data),size);
    for(const auto order:{spentit::qif::DateOrder::dmy,spentit::qif::DateOrder::mdy}) {
        const auto result=spentit::qif::parse(input,order,currency);
        for(const auto& record:result.records) {
            if(record.begin>record.end || record.end>size) std::abort();
            if(record.transaction && (record.diagnostic || !record.transaction->original_imported_amount())) std::abort();
        }
    }
    return 0;
}
