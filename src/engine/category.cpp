#include <spentit/category.hpp>
#include <spentit/qif.hpp>
#include <stdexcept>
namespace spentit {
void validate_category_name(std::string_view name) {
    if(name.empty() || name.size()>256 || !qif::valid_utf8(name) || name.front()==' ' || name.back()==' ' || name.find_first_of("[]/|:")!=name.npos)
        throw std::invalid_argument("Category names must be nonempty UTF-8 segments without surrounding spaces or []/|:");
    const auto whitespace=[](unsigned value){return value==0x20 || value==0x85 || value==0xa0 || value==0x1680 || (value>=0x2000 && value<=0x200a) || value==0x2028 || value==0x2029 || value==0x202f || value==0x205f || value==0x3000;};
    for(std::size_t i=0;i<name.size();) {
        const auto start=i;unsigned value=static_cast<unsigned char>(name[i++]);unsigned trailing=0;
        if(value>=0xf0){value&=7;trailing=3;}else if(value>=0xe0){value&=15;trailing=2;}else if(value>=0xc0){value&=31;trailing=1;}
        while(trailing--)value=(value<<6)|(static_cast<unsigned char>(name[i++])&63);
        if(value<32 || (value>=0x7f && value<=0x9f) || value==0x2028 || value==0x2029 || ((start==0 || i==name.size()) && whitespace(value)))
            throw std::invalid_argument("Category names cannot contain controls or surrounding whitespace");
    }
}
}
