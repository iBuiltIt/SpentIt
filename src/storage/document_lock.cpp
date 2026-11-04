#include "document_lock.hpp"
#include <spentit/import_store.hpp>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <vector>
#ifdef _WIN32
#include "windows_files.hpp"
#include <shlobj.h>
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/mount.h>
#include <sys/acl.h>
#else
#include <sys/vfs.h>
#include <linux/magic.h>
#endif
#endif

namespace spentit::storage {
namespace {
namespace fs = std::filesystem;
[[noreturn]] void unavailable() {
    throw ImportError(SQLITE_CANTOPEN,"Cannot protect this file from simultaneous use. Check access to the file and SpentIt's application-data folder.");
}
#ifndef _WIN32
[[noreturn]] void busy() {
    throw ImportError(SQLITE_BUSY,"File is already open or unavailable. Close its other SpentIt window and try again.");
}
#endif
std::string digest(std::string_view value) {
    unsigned char bytes[EVP_MAX_MD_SIZE]; unsigned length=0;
    if(EVP_Digest(value.data(),value.size(),bytes,&length,EVP_sha256(),nullptr)!=1)unavailable();
    std::string result;
    for(unsigned i=0;i<length;++i){result+="0123456789abcdef"[bytes[i]>>4];result+="0123456789abcdef"[bytes[i]&15];}
    return result;
}
struct Identity {std::string key; bool private_file=false;};
#ifdef _WIN32
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h):value(h){}
    ~Handle(){win::close_handle(value);}
};
Identity identity(const fs::path& path,bool directory=false) {
    Handle file(directory ? win::open_directory(path,FILE_READ_ATTRIBUTES|READ_CONTROL)
        : win::open_nofollow(path,FILE_READ_ATTRIBUTES|READ_CONTROL,
          FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,OPEN_EXISTING,0));
    if(file.value==INVALID_HANDLE_VALUE)unavailable();
    const auto id=win::identify(file.value);
    if(!id || id->reparse || id->directory!=directory || (!directory && id->links!=1))unavailable();
    return {std::to_string(id->volume)+":"+std::to_string(id->index_high)+":"+std::to_string(id->index_low),
        win::owned_by_current_user(file.value) && win::owner_only_dacl(file.value)};
}
bool local_filesystem(const fs::path& path) {
    wchar_t volume[MAX_PATH]{};
    if(!GetVolumePathNameW(path.c_str(),volume,MAX_PATH))return false;
    const auto kind=GetDriveTypeW(volume);
    if(kind!=DRIVE_FIXED && kind!=DRIVE_REMOVABLE && kind!=DRIVE_RAMDISK)return false;
    DWORD flags=0;
    return GetVolumeInformationW(volume,nullptr,0,nullptr,nullptr,&flags,nullptr,0) && (flags&FILE_PERSISTENT_ACLS)!=0;
}
void private_directory(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path.parent_path(),error);if(error)unavailable();
    const bool created=CreateDirectoryW(path.c_str(),nullptr)!=0;
    if(!created && GetLastError()!=ERROR_ALREADY_EXISTS)unavailable();
    Handle directory(win::open_directory(path,FILE_READ_ATTRIBUTES|READ_CONTROL|WRITE_DAC));
    const auto id=win::identify(directory.value);
    if(!id || !id->directory || id->reparse || !win::owned_by_current_user(directory.value))unavailable();
    if(created && !win::set_owner_only(directory.value))unavailable();
    if(!win::owner_only_dacl(directory.value) || !local_filesystem(path))unavailable();
}
#else
Identity identity(const fs::path& path,bool directory=false) {
    struct stat st{};
    if(::lstat(path.c_str(),&st)!=0 || (directory ? !S_ISDIR(st.st_mode) : (!S_ISREG(st.st_mode)||st.st_nlink!=1)))unavailable();
    bool private_file=st.st_uid==::geteuid() && (st.st_mode&0077)==0;
#ifdef __APPLE__
    // macOS extended ACLs can grant access despite owner-only mode bits.
    acl_t acl=::acl_get_file(path.c_str(),ACL_TYPE_EXTENDED);
    if(!acl){if(errno!=ENOENT)private_file=false;} // No extended ACL is normal on macOS.
    else {
        acl_entry_t entry=nullptr;errno=0;
        const int result=::acl_get_entry(acl,ACL_FIRST_ENTRY,&entry);
        if(result==0 || errno!=EINVAL)private_file=false;
        ::acl_free(acl);
    }
#endif
    return {std::to_string(st.st_dev)+":"+std::to_string(st.st_ino),private_file};
}
bool local_filesystem(const fs::path& path) {
    struct statfs info{};if(::statfs(path.c_str(),&info)!=0)return false;
#ifdef __APPLE__
    const std::string_view type(info.f_fstypename);
    return (info.f_flags&MNT_LOCAL)!=0 && (type=="apfs" || type=="hfs");
#else
    switch(info.f_type) {
        case EXT4_SUPER_MAGIC: case XFS_SUPER_MAGIC: case BTRFS_SUPER_MAGIC: case TMPFS_MAGIC: return true;
        default: return false;
    }
#endif
}
void private_directory(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path.parent_path(),error);if(error)unavailable();
    if(::mkdir(path.c_str(),0700)!=0 && errno!=EEXIST)unavailable();
    if(!identity(path,true).private_file || !local_filesystem(path))unavailable();
}
#endif
bool path_exists(const fs::path& path) {
    std::error_code error;const auto status=fs::symlink_status(path,error);
    if(error && error!=std::errc::no_such_file_or_directory)unavailable();
    return fs::exists(status);
}
}

struct DocumentLock::Impl {
    fs::path path,root,legacy;
    std::string bound;
    bool adjacent=false;
#ifdef _WIN32
    std::vector<win::FileLock> locks;
    ~Impl(){for(auto& lock:locks)win::close_lock(lock);}
    void take(const fs::path& name,bool = true) {
        win::FileLock lock;const auto result=win::acquire_lock(name,lock);
        if(!result)throw ImportError(result.code,result.message);
        try {locks.push_back(lock);}catch(...){win::close_lock(lock);throw;}
    }
#else
    std::vector<int> locks;
    ~Impl(){for(int lock:locks)::close(lock);}
    void take(const fs::path& name,bool private_token=true) {
        const int fd=::open(name.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)unavailable();
        struct stat st{},named{};
        if(::fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_nlink!=1 || st.st_uid!=::geteuid() || (private_token && (st.st_mode&0077)!=0)){::close(fd);unavailable();}
        if(::flock(fd,LOCK_EX|LOCK_NB)!=0){::close(fd);busy();}
        if(::lstat(name.c_str(),&named)!=0 || named.st_dev!=st.st_dev || named.st_ino!=st.st_ino){::close(fd);unavailable();}
        try{locks.push_back(fd);}catch(...){::close(fd);throw;}
    }
#endif
    void take_adjacent(){if(!adjacent){take(legacy,false);adjacent=true;}}
};

fs::path DocumentLock::default_directory() {
#ifdef _WIN32
    PWSTR location=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&location)))unavailable();
    const fs::path base(location);CoTaskMemFree(location);
    return base/"SpentIt"/"Locks";
#else
#ifndef __APPLE__
    if(const char* xdg=std::getenv("XDG_DATA_HOME");xdg && *xdg){
        const fs::path base(xdg);if(!base.is_absolute())unavailable();
        return base/"SpentIt"/"Locks";
    }
#endif
    // Use the OS account's home, not a cwd-dependent or temporary location.
    std::vector<char> buffer(16384);struct passwd record{};struct passwd* found=nullptr;
    if(::getpwuid_r(::geteuid(),&record,buffer.data(),buffer.size(),&found)!=0 || !found || !record.pw_dir)unavailable();
    const fs::path home(record.pw_dir);if(!home.is_absolute())unavailable();
#ifdef __APPLE__
    return home/"Library"/"Application Support"/"SpentIt"/"Locks";
#else
    return home/".local"/"share"/"SpentIt"/"Locks";
#endif
#endif
}

DocumentLock::DocumentLock(const fs::path& document,const fs::path& directory):impl_(std::make_unique<Impl>()) {
    std::error_code error;
    const auto absolute=fs::absolute(document,error);if(error)unavailable();
    const auto parent=fs::canonical(absolute.parent_path(),error);if(error)unavailable();
    impl_->path=parent/absolute.filename();
    impl_->legacy=impl_->path;impl_->legacy+=".lock";
    impl_->root=directory.empty()?default_directory():directory;
    if(!impl_->root.is_absolute())unavailable();
    private_directory(impl_->root);
    // Reserve the directory entry even before the dataset exists. File identity
    // additionally catches renames and case/Unicode aliases of existing files.
    const auto filename=absolute.filename().u8string();
    std::string name(reinterpret_cast<const char*>(filename.data()),filename.size());
    impl_->take(impl_->root/("path-"+digest(identity(parent,true).key+":"+name)+".lock"));
    // Honor old sidecars without deleting them, including an older live holder.
    if(!local_filesystem(parent) || path_exists(impl_->legacy))impl_->take_adjacent();
    if(path_exists(impl_->path))bind_file();
}
DocumentLock::~DocumentLock()=default;
void DocumentLock::bind_file() {
    const auto file=identity(impl_->path);
    if(impl_->bound.empty()) {
        impl_->take(impl_->root/("file-"+digest(file.key)+".lock"));
        impl_->bound=file.key;
    } else if(impl_->bound!=file.key)unavailable();
    if(!file.private_file)impl_->take_adjacent();
    validate();
}
void DocumentLock::validate() const {
    if(impl_->bound.empty() || identity(impl_->path).key!=impl_->bound)unavailable();
}
}
