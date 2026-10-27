#include <spentit/qif.hpp>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace spentit;
using namespace spentit::qif;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("QIF assertion failed"); }
const Currency cad("CAD");
Result parse_dmy(std::string_view s) { return parse(s,DateOrder::dmy,cad); }
void invalid(std::string_view s,Issue issue) {
    const auto result=parse_dmy(s); check(!result.valid());
    check((result.diagnostic && result.diagnostic->issue==issue) ||
        (!result.records.empty() && result.records.back().diagnostic && result.records.back().diagnostic->issue==issue));
}
const std::string wise="D10/08/2026\nNCARD-SYNTHETIC-001\nT-29.31\nPSynthetic Transit\nMExample Person\nEWise Charges for: CARD-SYNTHETIC-001\n$-0.14\nECARD-SYNTHETIC-001\n$-29.17\n^\n";
void basics() {
    auto badDate=parse_dmy("D31/02/2026\nT-10\n^\n");
    check(badDate.records[0].repairable && !badDate.records[0].transaction && badDate.records[0].original_amount->units()==-1000);
    check(parse_dmy("D31/02/2026\nT-10\nL[Visa]\n^\n").records[0].repairable);
    auto badSplit=parse_dmy("D1/1/2026\nT-10\nSFood\n$-9\n^\n");
    check(badSplit.records[0].repairable && badSplit.records[0].source_allocations[0].amount.units()==-900);
    for(const auto& raw:{"D31/02/2026\nT-10\nU-10\n^\n", "D31/02/2026\nT-10\nSFood\n^\n"})
        check(!parse_dmy(raw).records[0].repairable);

    for(const auto raw:{"D31/02/2026\nTbad\n^\n","T-10\n^\n","D1/1/2026\nL[Visa]\n^\n"})check(parse_dmy(raw).records[0].repairable);
    check(!parse_dmy("D1/1/2026\nTbad\nLFood\nS[Visa]\n$bad\n^\n").records[0].repairable);
    const auto missing=parse_dmy("D1/1/2026\nS[Visa]/Family\n$bad\nSFood\n$2\n^\n");
    check(missing.records[0].repairable && !missing.records[0].original_amount && missing.records[0].source_allocation_fields.size()==2);
    check(!missing.records[0].source_allocation_fields[0].amount && missing.records[0].source_allocation_fields[0].category=="[Visa]" && missing.records[0].source_allocations[0].source_ordinal==1);
    auto r=parse_dmy(wise); check(r.valid() && r.records.size()==1);
    const auto& rec=r.records[0]; check(rec.date=="2026-08-10" && rec.begin==0 && rec.end==wise.size());
    check(rec.transaction->original_imported_amount()->units()==-2931);
    check(rec.transaction->allocations().size()==2 && rec.transaction->allocations()[0].amount.units()==-14);
    check(rec.transaction->allocations()[0].category.empty());
    check(parse(wise,DateOrder::mdy,cad).records[0].date=="2026-10-08");
    std::string crlf="\xef\xbb\xbf!Type:Bank\r\n";
    for (char c:wise) { if (c=='\n') crlf+='\r'; crlf+=c; }
    check(parse_dmy(crlf).valid());
    for(const auto type:{"Bank","Cash","CCard"})check(parse_dmy(std::string("!Type:")+type+"\n"+wise).valid());
    check(parse_dmy(wise.substr(0,wise.size()-1)).valid());
    check(parse_dmy(wise+wise).records.size()==2);
    check(parse_dmy("D29/2/2024\nT2200\nSSalary\n$3000\nSTax\n$-700\nSDeductions\n$-100\n^\n").valid());
    check(parse_dmy("D1/1/2026\nT0\nSZero\n$0\n^\n").valid());
    check(parse("D1/1/2026\nT1.234\n^",DateOrder::dmy,Currency("KWD")).valid());
    check(parse("D1/1/2026\nT1\n^",DateOrder::dmy,Currency::from_definition("six-amendment-174:HRK")).valid());
    check(date("1/1/0001",DateOrder::dmy)=="0001-01-01");
}
void malformed() {
    invalid("",Issue::empty_file); invalid("!Type:Bank\n",Issue::empty_file);
    invalid("!Type:Invst\n"+wise,Issue::header); invalid(wise+"!Type:Bank\n",Issue::header);
    invalid("^\n",Issue::empty_record); invalid("\n"+wise,Issue::empty_record);
    invalid("D29/2/2023\nT1\n^",Issue::date); invalid("D10/08/26\nT1\n^",Issue::date);
    invalid("D1/1/0000\nT1\n^",Issue::date); invalid("D1/1/2026\nT1,000.00\n^",Issue::amount);
    invalid("D1/1/2026\nT1.000\n^",Issue::amount);
    invalid("D1/1/2026\nT92233720368547758.08\n^",Issue::amount);
    invalid("T1\n^",Issue::missing_field); invalid("D1/1/2026\nT1\nT1\n^",Issue::repeated_field);
    invalid("D1/1/2026\nT1\n",Issue::unterminated); invalid("D1/1/2026\nT1\nU1\n^",Issue::unknown_field);
    invalid("D1/1/2026\nT1\n$2\n^",Issue::split_total);
    invalid("D1/1/2026\nT1\nEone\nEtwo\n$1\n^",Issue::split_structure);
    invalid("D1/1/2026\nT1\nSone\n^",Issue::split_structure);
    check(parse_dmy("D1/1/2026\nT1\nL[Visa]\n^").valid());
    check(parse_dmy("D1/1/2026\nT-9\nS[Visa]\n$-10\nSRefund\n$1\n^").valid());
    for(const auto name:{"[]","[ ]","[Visa","Visa]","[[Visa]]","[Visa]|Class"})
        invalid(std::string("D1/1/2026\nT1\nL")+name+"\n^",Issue::transfer);
    auto classified=parse_dmy("D1/1/2026\nT1\nS[Visa]/Class\n$1\n^");
    check(classified.valid() && classified.records[0].source_allocations[0].category=="[Visa]" && classified.records[0].source_allocation_classes[0]=="Class");
    auto food=parse_dmy("D1/1/2026\nT1\nLFood/Class\n^");check(food.valid() && food.records[0].source_category=="Food" && food.records[0].source_class=="Class");
    for(const auto value:{"Food/", "Food/A/B", "Food/|Class", "[Visa]/[Class]"}) invalid(std::string("D1/1/2026\nT1\nL")+value+"\n^",Issue::category_class);
    invalid("D1/1/2026\nT1\nC?\n^",Issue::cleared_status);
    invalid(std::string("D1/1/2026\nT1\nP")+char(0xff)+"\n^",Issue::encoding);
    invalid(std::string("D1/1/2026\nT1\nP")+char(0)+"\n^",Issue::encoding);
    invalid("D1/1/2026\rT1\n^",Issue::encoding);
    for (const auto s:{"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82"}) check(!valid_utf8(s));
    check(valid_utf8("\xf0\x9f\x98\x80"));
}
void limits() {
    check(!parse_dmy(std::string(max_file+1,'x')).admissible);
    check(!parse_dmy("D1/1/2026\nT1\nP"+std::string(max_field+1,'x')+"\n^").admissible);
    check(parse_dmy("D1/1/2026\nT1\nP"+std::string(max_field,'x')+"\n^").valid());
    std::string many="D1/1/2026\nT0\n";
    for (std::size_t i=0;i<max_allocations;++i) many+="$0\n";
    check(parse_dmy(many+"^").valid()); check(!parse_dmy(many+"$0\n^").admissible);
    std::string records;
    for (std::size_t i=0;i<max_records;++i) records+="D1/1/2026\nT0\n^\n";
    check(parse_dmy(records).valid()); check(!parse_dmy(records+"D1/1/2026\nT0\n^").admissible);
}
void mutations() {
    std::mt19937 gen(1701);
    for (unsigned i=0;i<10000;++i) {
        std::string s=wise;
        for (unsigned j=0;j<1+i%8;++j) s[gen()%s.size()]=static_cast<char>(gen()%256);
        auto r=parse_dmy(s);
        if (r.valid()) for (const auto& record:r.records) {
            check(record.begin<=record.end && record.end<=s.size());
            check(record.transaction->effective_total()==*record.transaction->original_imported_amount());
        }
    }
}
}
int main(int argc,char** argv) {
    try {
        const std::string name=argc>1 ? argv[1] : "";
        if (name=="basics") basics(); else if (name=="malformed") malformed();
        else if (name=="limits") limits(); else if (name=="mutations") mutations(); else return 2;
        std::cout<<"QIF "<<name<<" passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
