// Read-only local diagnostic. Never print paths, exception text or field values.
#include <spentit/qif.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <array>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
using namespace spentit;
int main(int argc,char** argv) {
#if defined(__unix__) || defined(__APPLE__)
    const rlimit core_limit{0,0};
    if(setrlimit(RLIMIT_CORE,&core_limit)!=0){std::cerr<<"Cannot disable process core dumps. Stopped.\n";return 2;}
#endif
    try {
        if(argc!=3 && argc!=4){std::cerr<<"Usage: spentit_qif_diagnose CURRENCY dmy|mdy [--synthetic]\nEnter the private path only at the local prompt, without quotes.\n";return 2;}
        const Currency currency(argv[1]);
        const std::string_view order_arg(argv[2]);
        if(order_arg!="dmy" && order_arg!="mdy"){std::cerr<<"Choose dmy or mdy.\n";return 2;}
        const auto order=order_arg=="dmy"?qif::DateOrder::dmy:qif::DateOrder::mdy;
        std::string bytes;
        if(argc==4) {
            if(std::string_view(argv[3])!="--synthetic")return 2;
            bytes="!Account\nNSynthetic\nTBank\n^\n!Type:Bank\n";
            for(std::size_t i=0;i<19436;++i) {
                bytes+="D2026-01-01\nT-10.38\nPSynthetic\n";
                bytes+=(i==294 || i==570 || i==571 || i==893)?"SSynthetic\n":"LSynthetic\n";
                bytes+="Cc\n^\n";
            }
        } else {
            std::cout<<"Local QIF path (read only; path and values are not reported): "<<std::flush;
            std::string path;
            if(!std::getline(std::cin,path))return 2;
            std::ifstream input(path,std::ios::binary);
            if(!input){std::cerr<<"Cannot open input.\n";return 2;}
            std::array<char,65536> buffer{};
            while(input) {
                input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
                const auto count=static_cast<std::size_t>(input.gcount());
                if(bytes.size()+count>qif::max_file){std::cerr<<"Input exceeds 64 MiB.\n";return 2;}
                bytes.append(buffer.data(),count);
            }
            if(!input.eof()){std::cerr<<"Input read failed.\n";return 2;}
        }
        const auto start=std::chrono::steady_clock::now();
        const auto document=qif::index_document(bytes);
        const auto indexed=std::chrono::steady_clock::now();
        std::size_t records=0,errors=0,shown=0;
        if(document.diagnostic) {
            std::cout<<"File diagnostic: "<<qif::message(*document.diagnostic)<<" at byte "<<document.diagnostic->offset<<'\n';
            ++errors;
        } else for(std::size_t account=0;account<document.accounts.size();++account) {
            const auto parsed=qif::parse_account(bytes,document,account,order,currency);
            if(parsed.diagnostic) {
                ++errors;
                if(shown++<100)std::cout<<"Account section "<<account+1<<": "<<qif::message(*parsed.diagnostic)<<" at byte "<<parsed.diagnostic->offset<<'\n';
            }
            records+=parsed.records.size();
            for(std::size_t i=0;i<parsed.records.size();++i)if(parsed.records[i].diagnostic) {
                ++errors;
                if(shown++<100)std::cout<<"Account section "<<account+1<<": "<<qif::describe_record(bytes,parsed.records[i],i,true)<<'\n';
            }
        }
        const auto done=std::chrono::steady_clock::now();
        std::size_t lines=0;for(char c:bytes)if(c=='\n')++lines;
        if(!bytes.empty() && bytes.back()!='\n')++lines;
        std::cout<<"Lines: "<<lines<<"; accounts: "<<document.accounts.size()<<"; transaction records: "<<records<<"; diagnostics: "<<errors<<" (at most 100 displayed).\n"
                 <<"Index ms: "<<std::chrono::duration<double,std::milli>(indexed-start).count()<<"; parse/report ms: "<<std::chrono::duration<double,std::milli>(done-indexed).count()<<".\n"
                 <<"No source values or paths are included above. Currency/date checks use your supplied profile for every account. This is parsing only, not a durable import.\n";
        return errors?1:0;
    } catch(...) {std::cerr<<"Diagnostic failed; no input details reported. Check the profile and available memory.\n";return 2;}
}
