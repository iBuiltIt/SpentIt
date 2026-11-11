#include <spentit/storage_document_gateway.hpp>
#include "fault_vfs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
namespace fs=std::filesystem;
using namespace spentit;
namespace {
void check(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value)throw std::runtime_error("Password change assertion failed at line "+std::to_string(where.line()));
}
struct Directory {
    fs::path path;
    Directory(){std::string name=(fs::temp_directory_path()/"spentit-password-XXXXXX").string();check(::mkdtemp(name.data())!=nullptr);path=fs::canonical(name);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
template<class F> void rejects(F action,std::string_view text) {
    try{action();}catch(const std::exception& error){check(std::string_view(error.what()).find(text)!=std::string_view::npos);return;}
    throw std::runtime_error(std::string("Expected rejection containing ")+std::string(text));
}
bool opens(const fs::path& path,std::optional<std::string> password,long units) {
    try {
        auto gateway=storage::document_gateway();
        gateway->open(path,std::move(password));
        const auto account=gateway->repository().account();
        const bool same=account.name=="Synthetic" && account.opening.units()==units;
        gateway->close();
        return same;
    } catch(const std::exception&) {return false;}
}
void expect(const fs::path& path,std::optional<std::string> password,long units) {
    check(opens(path,std::move(password),units));
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),{}};
}
void behavior() {
    Directory directory;
    const auto original=directory.path/"original";
    constexpr auto first="synthetic-password";
    constexpr auto second="next-password";
    constexpr long units=1234;
    {
        auto gateway=storage::document_gateway();
        gateway->create(original,true,first,"Synthetic",Currency("CAD"),"2026-01-01");
        gateway->repository().opening(0,"2026-01-01",Money(units,Currency("CAD")));
        check(gateway->password_protected());
        check(gateway->password_matches(first));
        check(!gateway->password_matches("wrong"));
        check(gateway->change_password(first,first,false)==app::PasswordUpdate::unchanged);
        rejects([&]{gateway->change_password("wrong",second,false);},"does not match");
        check(gateway->repository().account().opening.units()==units);
        check(gateway->change_password(first,second,false)==app::PasswordUpdate::changed);
        check(gateway->password_matches(second));
        check(!gateway->password_matches(first));
        gateway->backup(directory.path/"backup");
        gateway->close();
    }
    const auto stored=bytes(original);
    check(stored.find(first)==std::string::npos && stored.find(second)==std::string::npos);
    expect(original,second,units);
    check(!opens(original,first,units));
    expect(directory.path/"backup",second,units);
    check(!opens(directory.path/"backup",first,units));
    {
        auto gateway=storage::document_gateway();
        gateway->open(original,std::string(second));
        rejects([&]{gateway->change_password(second,"",false);},"1–1000");
        check(gateway->change_password(second,"",true)==app::PasswordUpdate::removed);
        check(!gateway->password_protected());
        check(!gateway->password_matches(second));
        gateway->close();
    }
    expect(original,std::nullopt,units);
    check(!opens(original,second,units));
    {
        auto gateway=storage::document_gateway();
        gateway->open(original,std::nullopt);
        rejects([&]{gateway->change_password("", "", true);},"no password");
        check(gateway->change_password("","third-password",false)==app::PasswordUpdate::changed);
        check(gateway->password_protected());
        check(gateway->password_matches("third-password"));
        gateway->close();
    }
    expect(original,"third-password",units);
    check(!opens(original,std::nullopt,units));
}
void faults() {
    fault_vfs::install();
    Directory directory;
    const auto seed=directory.path/"seed";
    constexpr auto current="synthetic-password";
    constexpr auto next="next-password";
    constexpr long units=1234;
    {
        auto gateway=storage::document_gateway();
        gateway->create(seed,true,current,"Synthetic",Currency("CAD"),"2026-01-01");
        gateway->repository().opening(0,"2026-01-01",Money(units,Currency("CAD")));
        gateway->close();
    }
    const auto counted=directory.path/"counted";
    fs::copy_file(seed,counted);
    int count=0;
    {
        auto gateway=storage::document_gateway();
        gateway->open(counted,std::string(current));
        fault_vfs::arm(-1);
        check(gateway->change_password(current,next,false)==app::PasswordUpdate::changed);
        count=fault_vfs::seen;
        fault_vfs::disarm();
        gateway->close();
    }
    check(count>1);
    const int step=count>40?std::max(1,count/20):1;
    for(int position=1;position<=count;position+=step) {
        const auto sample=directory.path/("sample-"+std::to_string(position));
        fs::copy_file(seed,sample);
        bool failed=true;
        if(position%2==0) {
            const auto child=::fork();
            check(child>=0);
            if(child==0) {
                try {
                    auto gateway=storage::document_gateway();
                    gateway->open(sample,std::string(current));
                    fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);
                    gateway->change_password(current,next,false);
                } catch(...) {}
                _exit(0);
            }
            int status=0;
            check(::waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==71 || WEXITSTATUS(status)==0));
        } else {
            auto gateway=storage::document_gateway();
            gateway->open(sample,std::string(current));
            fault_vfs::arm(position,position%4==1?SQLITE_FULL:SQLITE_IOERR_WRITE);
            failed=false;
            try {gateway->change_password(current,next,false);}
            catch(const std::exception&) {failed=true;}
            fault_vfs::disarm();
            try {gateway->close();} catch(...) {}
        }
        const bool old_key=opens(sample,std::string(current),units);
        const bool new_key=opens(sample,std::string(next),units);
        if(old_key==new_key)throw std::runtime_error("Password fault at position "+std::to_string(position)+" left old="+std::to_string(old_key)+" new="+std::to_string(new_key)+" threw="+std::to_string(failed));
        if(!failed)check(new_key);
    }
}
}
int main(int argc,char** argv) {
    try {
        const std::string mode=argc>1?argv[1]:"behavior";
        if(mode=="behavior") behavior();
        else if(mode=="faults") faults();
        else throw std::runtime_error("Unknown password change test");
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
