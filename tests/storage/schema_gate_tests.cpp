#include <spentit/import_store.hpp>
#include <filesystem>
#include <string>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <sqlite3.h>
#include <unistd.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-schema-gate-password";
struct Directory {
    fs::path path;
    Directory(){auto name=(fs::temp_directory_path()/"spentit-schema-gate-XXXXXX").string();auto* p=mkdtemp(name.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location& loc=std::source_location::current()){if(!value)throw std::runtime_error("Schema gate assertion failed at line "+std::to_string(loc.line()));}
template<class F>void rejects_text(F&& f,std::string_view text,const std::source_location& loc=std::source_location::current()){try{f();}catch(const std::exception& e){if(std::string(e.what()).find(text)==std::string::npos)throw std::runtime_error(std::string(e.what())+" (wanted "+std::string(text)+") at line "+std::to_string(loc.line()));return;}throw std::runtime_error("missing rejection at line "+std::to_string(loc.line()));}
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::string query(const fs::path& p,const char* sql){sqlite3* db=nullptr;check(sqlite3_open(p.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);sqlite3_stmt* st=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&st,nullptr)==SQLITE_OK);check(sqlite3_step(st)==SQLITE_ROW);const auto* text=sqlite3_column_text(st,0);std::string value=text?reinterpret_cast<const char*>(text):"";check(sqlite3_finalize(st)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);return value;}
void gate(){
    Directory dir;const auto path=dir.path/"doc";
    {auto s=ImportStore::create(path,password,"Synthetic account",Currency("CAD"),"2026-08-01");check(!s.rules_supported());app::OperationContext context;rejects_text([&]{s.save_rules("rules",{},0,context);},"Rules are not enabled for this dataset");rejects_text([&]{s.preview_rule_reapply(false,"",context);},"Rules are not enabled for this dataset");rejects_text([&]{s.apply_rule_reapply("reapply",{},context);},"Rules are not enabled for this dataset");check(s.rule_reapply_group("reapply").empty());}
    check(query(path,"PRAGMA user_version")=="29");
    for(const char* version:{"6","16","17","18","19","20","21","22","23","24","25","26","27","28","30"}) {
        sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,(std::string("PRAGMA user_version=")+version).c_str(),nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);
        const auto before=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==before);check(query(path,"PRAGMA user_version")==version);
    }
}
}
int main(){try{gate();std::cout<<"Schema gate passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
