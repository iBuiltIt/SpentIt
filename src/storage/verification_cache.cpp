#include "verification_cache.hpp"
#include <openssl/evp.h>
#include <array>
#include <memory>
#include <cerrno>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include "windows_files.hpp"
#include <cstdio>
#include <random>
#endif

namespace spentit::storage::verification_cache {
namespace {
#ifndef _WIN32
struct File {
    int fd=-1;
    explicit File(int value):fd(value){}
    ~File(){if(fd>=0)::close(fd);}
};
#endif
std::string hex(const unsigned char* data,std::size_t size) {
    std::string out;out.reserve(size*2);
    for(std::size_t i=0;i<size;++i){out+="0123456789abcdef"[data[i]>>4];out+="0123456789abcdef"[data[i]&15];}
    return out;
}
std::string digest(std::string_view value) {
    unsigned char result[EVP_MAX_MD_SIZE];unsigned length=0;
    if(EVP_Digest(value.data(),value.size(),result,&length,EVP_sha256(),nullptr)!=1)throw std::runtime_error("Cannot fingerprint verification state");
    return hex(result,length);
}
#ifndef _WIN32
std::string file_fingerprint(const std::filesystem::path& path,const app::OperationContext* context) {
    if(!no_journals(path))return {};
    File file(::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat before{},after{},named{};
    if(file.fd<0 || ::fstat(file.fd,&before)!=0 || !S_ISREG(before.st_mode) || before.st_nlink!=1)return {};
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if(!hash || EVP_DigestInit_ex(hash.get(),EVP_sha256(),nullptr)!=1)throw std::runtime_error("Cannot fingerprint verification state");
    std::array<char,256*1024> block;std::int64_t total=0;
    for(;;){
        if(context)context->checkpoint();
        const auto count=::read(file.fd,block.data(),block.size());
        if(count<0){if(errno==EINTR)continue;return {};}
        if(count==0)break;
        total+=count;
        if(EVP_DigestUpdate(hash.get(),block.data(),static_cast<std::size_t>(count))!=1)throw std::runtime_error("Cannot fingerprint verification state");
    }
    if(::fstat(file.fd,&after)!=0 || ::lstat(path.c_str(),&named)!=0 || before.st_dev!=named.st_dev || before.st_ino!=named.st_ino || before.st_size!=total || after.st_size!=total || !no_journals(path))return {};
    unsigned char result[EVP_MAX_MD_SIZE];unsigned length=0;
    if(EVP_DigestFinal_ex(hash.get(),result,&length)!=1)throw std::runtime_error("Cannot fingerprint verification state");
    return hex(result,length);
}
std::filesystem::path receipt(const std::filesystem::path& path,const std::filesystem::path& cache) {
    return cache/(digest(std::filesystem::absolute(path).lexically_normal().string())+".verified");
}
std::string content(const std::string& hash) {
    return std::string("SpentIt verification 1\n")+SPENTIT_VERIFIER_ID+"\n"+hash+"\n";
}
bool private_directory(const std::filesystem::path& cache,bool create) {
    if(cache.empty())return false;
    std::error_code error;
    if(create){std::filesystem::create_directories(cache,error);if(error)return false;}
    struct stat info{};
    if(::lstat(cache.c_str(),&info)!=0 || !S_ISDIR(info.st_mode) || info.st_uid!=::geteuid())return false;
    if(create && ::chmod(cache.c_str(),0700)!=0)return false;
    return (info.st_mode&0077)==0 || create;
}
#else
std::filesystem::path receipt(const std::filesystem::path& path,const std::filesystem::path& cache) {
    const auto bytes=std::filesystem::absolute(path).lexically_normal().u8string();
    return cache/(digest(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()))+".verified");
}
std::string content(const std::string& hash) {
    return std::string("SpentIt verification 1\n")+SPENTIT_VERIFIER_ID+"\n"+hash+"\n";
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE opened):value(opened){}
    ~Handle(){win::close_handle(value);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
std::string file_fingerprint(const std::filesystem::path& path,const app::OperationContext* context) {
    if(!no_journals(path))return {};
    Handle file(win::open_read_nofollow(path));
    std::uint64_t before=0;
    if(file.value==INVALID_HANDLE_VALUE || !win::regular_private(file.value,false,false,&before))return {};
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if(!hash || EVP_DigestInit_ex(hash.get(),EVP_sha256(),nullptr)!=1)throw std::runtime_error("Cannot fingerprint verification state");
    std::array<char,256*1024> block;std::uint64_t total=0;
    for(;;){
        if(context)context->checkpoint();
        const auto count=win::read_some(file.value,block.data(),block.size());
        if(count<0)return {};
        if(count==0)break;
        total+=static_cast<std::uint64_t>(count);
        if(EVP_DigestUpdate(hash.get(),block.data(),static_cast<std::size_t>(count))!=1)throw std::runtime_error("Cannot fingerprint verification state");
    }
    std::uint64_t after=0;
    if(!win::regular_private(file.value,false,false,&after) || !win::same_named_file(file.value,path) || before!=total || after!=total || !no_journals(path))return {};
    unsigned char result[EVP_MAX_MD_SIZE];unsigned length=0;
    if(EVP_DigestFinal_ex(hash.get(),result,&length)!=1)throw std::runtime_error("Cannot fingerprint verification state");
    return hex(result,length);
}
bool private_directory(const std::filesystem::path& cache,bool create) {
    if(cache.empty())return false;
    std::error_code error;
    if(create){std::filesystem::create_directories(cache,error);if(error)return false;}
    Handle directory(win::open_directory(cache,FILE_READ_ATTRIBUTES|READ_CONTROL|(create?WRITE_DAC:0)));
    if(directory.value==INVALID_HANDLE_VALUE)return false;
    const auto id=win::identify(directory.value);
    if(!id || !id->directory || id->reparse || !win::owned_by_current_user(directory.value))return false;
    if(create && !win::set_owner_only(directory.value))return false;
    return win::owner_only_dacl(directory.value) || create;
}
#endif
}
std::string fingerprint(const std::filesystem::path& path,const app::OperationContext* context) {return file_fingerprint(path,context);}
#ifndef _WIN32
bool no_journals(const std::filesystem::path& path) {
    for(const auto* suffix:{"-journal","-wal","-shm"}){
        struct stat info{};if(::lstat((path.string()+suffix).c_str(),&info)==0 || errno!=ENOENT)return false;
    }
    return true;
}
bool matches(const std::filesystem::path& path,const std::filesystem::path& cache,const app::OperationContext* context) {
    if(!private_directory(cache,false))return false;
    File file(::open(receipt(path,cache).c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat info{};
    if(file.fd<0 || ::fstat(file.fd,&info)!=0 || !S_ISREG(info.st_mode) || info.st_nlink!=1 || info.st_uid!=::geteuid() || (info.st_mode&0077)!=0 || info.st_size>256)return false;
    std::array<char,257> bytes{};const auto count=::read(file.fd,bytes.data(),bytes.size());
    if(count<0 || count!=info.st_size)return false;
    const auto hash=fingerprint(path,context);
    return !hash.empty() && std::string_view(bytes.data(),static_cast<std::size_t>(count))==content(hash);
}
void publish(const std::filesystem::path& path,const std::filesystem::path& cache,std::string_view expected,const app::OperationContext* context) {
    const auto hash=fingerprint(path,context);
    if(hash.empty() || hash!=expected || !private_directory(cache,true))throw std::runtime_error("The file passed its checks. SpentIt will check it again next time you open it.");
    const auto destination=receipt(path,cache);
    auto pattern=(cache/".verification-XXXXXX").string();
    File file(::mkstemp(pattern.data()));
    if(file.fd<0)throw std::runtime_error("Cannot save startup verification approval");
    struct Cleanup{std::string path;~Cleanup(){::unlink(path.c_str());}} cleanup{pattern};
    const auto value=content(hash);std::size_t offset=0;
    while(offset<value.size()){
        const auto count=::write(file.fd,value.data()+offset,value.size()-offset);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)throw std::runtime_error("Cannot save startup verification approval");
        offset+=static_cast<std::size_t>(count);
    }
    if(::fsync(file.fd)!=0 || ::rename(pattern.c_str(),destination.c_str())!=0)throw std::runtime_error("Cannot publish startup verification approval");
    File directory(::open(cache.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if(directory.fd<0 || ::fsync(directory.fd)!=0)throw std::runtime_error("Cannot synchronize startup verification approval");
}
#else
bool no_journals(const std::filesystem::path& path) {
    for(const auto* suffix:{"-journal","-wal","-shm"}){
        auto sidecar=path;sidecar+=suffix;
        if(GetFileAttributesW(sidecar.c_str())!=INVALID_FILE_ATTRIBUTES)return false;
        const auto error=GetLastError();
        if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND)return false;
    }
    return true;
}
bool matches(const std::filesystem::path& path,const std::filesystem::path& cache,const app::OperationContext* context) {
    if(!private_directory(cache,false))return false;
    Handle file(win::open_read_nofollow(receipt(path,cache)));
    std::uint64_t size=0;
    if(file.value==INVALID_HANDLE_VALUE || !win::regular_private(file.value,true,true,&size) || size>256)return false;
    std::array<char,257> bytes{};
    const auto count=win::read_some(file.value,bytes.data(),bytes.size());
    if(count<0 || static_cast<std::uint64_t>(count)!=size)return false;
    const auto hash=fingerprint(path,context);
    return !hash.empty() && std::string_view(bytes.data(),static_cast<std::size_t>(count))==content(hash);
}
void publish(const std::filesystem::path& path,const std::filesystem::path& cache,std::string_view expected,const app::OperationContext* context) {
    const auto hash=fingerprint(path,context);
    if(hash.empty() || hash!=expected || !private_directory(cache,true))throw std::runtime_error("The file passed its checks. SpentIt will check it again next time you open it.");
    const auto destination=receipt(path,cache);
    std::filesystem::path pattern;
    HANDLE opened=INVALID_HANDLE_VALUE;
    for(int attempt=0;attempt<64 && opened==INVALID_HANDLE_VALUE;++attempt){
        unsigned token=0;std::random_device device;token=device();
        char suffix[9];std::snprintf(suffix,sizeof suffix,"%08x",token);
        pattern=cache/(std::string(".verification-")+suffix);
        opened=win::create_private(pattern);
    }
    Handle file(opened);
    if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot save startup verification approval");
    struct Cleanup{std::filesystem::path path;~Cleanup(){DeleteFileW(path.c_str());}} cleanup{pattern};
    if(!win::write_all(file.value,content(hash)) || !FlushFileBuffers(file.value))throw std::runtime_error("Cannot save startup verification approval");
    if(!win::rename_replace(pattern,destination))throw std::runtime_error("Cannot publish startup verification approval");
    const auto synced=win::sync_directory(cache,"Cannot synchronize startup verification approval");
    if(!synced)throw std::runtime_error(synced.message);
}
#endif
}
