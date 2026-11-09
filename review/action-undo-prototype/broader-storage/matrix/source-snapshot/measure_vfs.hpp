#pragma once
// Test-only, pass-through SQLite VFS size accounting. Never linked into production.
// Byte counts are file lengths observed through this VFS, not filesystem block
// allocation. In-memory SQLite caches/temp stores and non-VFS external files are
// outside this accounting. Shared memory is reported separately as mapped logical
// extent; it is not included in the disk-file aggregate. Metrics are mutex-safe,
// but exact peak claims require the measured store's serial file mutation: the
// wrapper deliberately does not serialize independent underlying VFS calls.
// External file mutation/rename/deletion outside this VFS is not observed.
#include <sqlite3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace measure_vfs {
enum class Kind : std::size_t { main_db, rollback_journal, wal, temp, other, count };
constexpr std::size_t kind_count=static_cast<std::size_t>(Kind::count);
inline const char* name(Kind kind) {
    switch(kind){case Kind::main_db:return "main_db";case Kind::rollback_journal:return "rollback_journal";case Kind::wal:return "wal";case Kind::temp:return "temp";case Kind::other:return "other";default:return "unknown";}
}
struct Stats {
    std::array<std::uint64_t,kind_count> current_bytes{},peak_bytes{};
    std::uint64_t total_current_bytes=0,total_peak_bytes=0;
    std::uint64_t shm_current_logical_bytes=0,shm_peak_logical_bytes=0;
    std::uint64_t write_bytes=0,write_calls=0,sync_calls=0;
    std::uint64_t failed_writes=0,failed_syncs=0;
    double elapsed_seconds=0;
    bool valid=true;
};
namespace detail {
using Clock=std::chrono::steady_clock;
struct Entry {
    std::string path;
    Kind kind=Kind::other;
    std::uint64_t bytes=0;
    std::size_t handles=0;
    bool linked=true;
    std::map<std::uint64_t,std::uint64_t> shm;
};
inline sqlite3_vfs* base=nullptr;
inline sqlite3_vfs wrapper{};
inline std::mutex mutex;
inline std::map<std::uint64_t,Entry> entries;
inline std::map<std::string,std::uint64_t> names;
inline std::uint64_t next_entry=0,next_handle=0;
inline Stats totals;
inline Clock::time_point started=Clock::now();
inline std::atomic<bool> valid{true};
inline bool installed=false;
struct alignas(std::max_align_t) File {
    sqlite3_file header;
    sqlite3_io_methods methods;
    sqlite3_file* real;
    std::uint64_t identity,handle;
    bool delete_on_close;
};
inline File* file(sqlite3_file* f){return reinterpret_cast<File*>(f);}
inline std::uint64_t add(std::uint64_t a,std::uint64_t b){
    if(b>std::numeric_limits<std::uint64_t>::max()-a){valid=false;return std::numeric_limits<std::uint64_t>::max();}return a+b;
}
inline void recompute(){
    totals.current_bytes.fill(0);totals.total_current_bytes=0;totals.shm_current_logical_bytes=0;
    for(const auto& [id,e]:entries){(void)id;if(!e.linked&&!e.handles)continue;
        const auto k=static_cast<std::size_t>(e.kind);totals.current_bytes[k]=add(totals.current_bytes[k],e.bytes);
        std::uint64_t shm=0;for(const auto& [handle,size]:e.shm){(void)handle;shm=std::max(shm,size);}totals.shm_current_logical_bytes=add(totals.shm_current_logical_bytes,shm);
    }
    for(std::size_t i=0;i<kind_count;++i){totals.total_current_bytes=add(totals.total_current_bytes,totals.current_bytes[i]);totals.peak_bytes[i]=std::max(totals.peak_bytes[i],totals.current_bytes[i]);}
    totals.total_peak_bytes=std::max(totals.total_peak_bytes,totals.total_current_bytes);
    totals.shm_peak_logical_bytes=std::max(totals.shm_peak_logical_bytes,totals.shm_current_logical_bytes);
}
template<class F> inline void record(F&& callback) noexcept {
    try{std::lock_guard<std::mutex> lock(mutex);callback();recompute();}catch(...){valid=false;}
}
inline std::string normalized(const char* path){
    if(!path)return {};
    std::vector<char> result(static_cast<std::size_t>(base->mxPathname)+1);
    // Unix may return SQLITE_OK_SYMLINK: an extended success, not an error.
    if((base->xFullPathname(base,path,static_cast<int>(result.size()),result.data())&255)!=SQLITE_OK){valid=false;return path;}
    // Some platform VFSes resolve directory aliases only while the complete
    // file exists. Resolve the parent independently so a later xDelete observes
    // the same identity after removing the final path component.
    const std::filesystem::path full(result.data());std::error_code error;
    const auto parent=std::filesystem::weakly_canonical(full.parent_path(),error);
    if(error){valid=false;return result.data();}
    return (parent/full.filename()).string();
}
inline Kind classify(int flags){
    if(flags&SQLITE_OPEN_MAIN_DB)return Kind::main_db;
    if(flags&SQLITE_OPEN_MAIN_JOURNAL)return Kind::rollback_journal;
    if(flags&SQLITE_OPEN_WAL)return Kind::wal;
    if(flags&(SQLITE_OPEN_TEMP_DB|SQLITE_OPEN_TRANSIENT_DB|SQLITE_OPEN_TEMP_JOURNAL|SQLITE_OPEN_SUBJOURNAL))return Kind::temp;
    return Kind::other;
}
inline void refresh(File* f) noexcept {
    sqlite3_int64 size=0;const auto rc=f->real->pMethods->xFileSize(f->real,&size);
    if(rc!=SQLITE_OK||size<0){valid=false;return;}
    record([&]{const auto it=entries.find(f->identity);if(it!=entries.end())it->second.bytes=static_cast<std::uint64_t>(size);else valid=false;});
}
inline int close(sqlite3_file* f){auto* p=file(f);refresh(p);const auto rc=p->real->pMethods->xClose(p->real);if(rc==SQLITE_OK){record([&]{
    const auto it=entries.find(p->identity);if(it==entries.end()){valid=false;return;}auto& e=it->second;e.shm.erase(p->handle);if(e.handles)--e.handles;else valid=false;
    if(p->delete_on_close){e.linked=false;if(!e.path.empty()){const auto n=names.find(e.path);if(n!=names.end()&&n->second==p->identity)names.erase(n);}}
    if(!e.linked&&!e.handles)entries.erase(it);
});}else valid=false;return rc;}
inline int read(sqlite3_file* f,void* data,int n,sqlite3_int64 offset){auto* p=file(f);return p->real->pMethods->xRead(p->real,data,n,offset);}
inline int write(sqlite3_file* f,const void* data,int n,sqlite3_int64 offset){auto* p=file(f);const auto rc=p->real->pMethods->xWrite(p->real,data,n,offset);
    record([&]{if(rc==SQLITE_OK){++totals.write_calls;if(n>=0)totals.write_bytes=add(totals.write_bytes,static_cast<std::uint64_t>(n));else valid=false;}else ++totals.failed_writes;});
    // Real size rather than offset+n also accounts for VFS chunk/preallocation.
    // This adds metadata queries, never data I/O or a changed write/sync policy.
    refresh(p);return rc;
}
inline int truncate(sqlite3_file* f,sqlite3_int64 n){auto* p=file(f);const auto rc=p->real->pMethods->xTruncate(p->real,n);if(rc==SQLITE_OK)refresh(p);return rc;}
inline int sync(sqlite3_file* f,int flags){auto* p=file(f);const auto rc=p->real->pMethods->xSync(p->real,flags);record([&]{if(rc==SQLITE_OK)++totals.sync_calls;else ++totals.failed_syncs;});return rc;}
inline int size(sqlite3_file* f,sqlite3_int64* n){auto* p=file(f);const auto rc=p->real->pMethods->xFileSize(p->real,n);if(rc==SQLITE_OK&&*n>=0)record([&]{entries.at(p->identity).bytes=static_cast<std::uint64_t>(*n);});return rc;}
inline int lock(sqlite3_file* f,int n){auto* p=file(f);return p->real->pMethods->xLock(p->real,n);}
inline int unlock(sqlite3_file* f,int n){auto* p=file(f);return p->real->pMethods->xUnlock(p->real,n);}
inline int reserved(sqlite3_file* f,int* n){auto* p=file(f);return p->real->pMethods->xCheckReservedLock(p->real,n);}
inline int control(sqlite3_file* f,int n,void* arg){auto* p=file(f);const auto rc=p->real->pMethods->xFileControl(p->real,n,arg);if(rc==SQLITE_OK&&n==SQLITE_FCNTL_SIZE_HINT)refresh(p);return rc;}
inline int sector(sqlite3_file* f){auto* p=file(f);return p->real->pMethods->xSectorSize(p->real);}
inline int device(sqlite3_file* f){auto* p=file(f);return p->real->pMethods->xDeviceCharacteristics(p->real);}
inline int map(sqlite3_file* f,int page,int n,int extend,void volatile** out){auto* p=file(f);const auto rc=p->real->pMethods->xShmMap(p->real,page,n,extend,out);
    if(rc==SQLITE_OK&&out&&*out&&page>=0&&n>=0)record([&]{auto& extent=entries.at(p->identity).shm[p->handle];extent=std::max(extent,(static_cast<std::uint64_t>(page)+1)*static_cast<std::uint64_t>(n));});return rc;}
inline int shm_lock(sqlite3_file* f,int offset,int n,int flags){auto* p=file(f);return p->real->pMethods->xShmLock(p->real,offset,n,flags);}
inline void barrier(sqlite3_file* f){auto* p=file(f);p->real->pMethods->xShmBarrier(p->real);}
inline int unmap(sqlite3_file* f,int remove){auto* p=file(f);const auto rc=p->real->pMethods->xShmUnmap(p->real,remove);if(rc==SQLITE_OK)record([&]{entries.at(p->identity).shm.erase(p->handle);});return rc;}
inline int fetch(sqlite3_file* f,sqlite3_int64 offset,int n,void** out){auto* p=file(f);return p->real->pMethods->xFetch(p->real,offset,n,out);}
inline int unfetch(sqlite3_file* f,sqlite3_int64 offset,void* data){auto* p=file(f);return p->real->pMethods->xUnfetch(p->real,offset,data);}
inline int open(sqlite3_vfs*,const char* path,sqlite3_file* out,int flags,int* result){
    auto* p=file(out);p->real=reinterpret_cast<sqlite3_file*>(reinterpret_cast<char*>(out)+sizeof(File));p->identity=0;p->handle=0;p->delete_on_close=(flags&SQLITE_OPEN_DELETEONCLOSE)!=0;
    const auto rc=base->xOpen(base,path,p->real,flags,result);if(rc!=SQLITE_OK){p->header.pMethods=nullptr;return rc;}
    p->methods={};const auto* original=p->real->pMethods;p->methods.iVersion=original->iVersion;
    p->methods.xSectorSize=original->xSectorSize;p->methods.xDeviceCharacteristics=original->xDeviceCharacteristics;
    if(original->iVersion>=2){p->methods.xShmMap=original->xShmMap;p->methods.xShmLock=original->xShmLock;p->methods.xShmBarrier=original->xShmBarrier;p->methods.xShmUnmap=original->xShmUnmap;}
    if(original->iVersion>=3){p->methods.xFetch=original->xFetch;p->methods.xUnfetch=original->xUnfetch;}
    p->methods.xClose=close;p->methods.xRead=read;p->methods.xWrite=write;p->methods.xTruncate=truncate;p->methods.xSync=sync;p->methods.xFileSize=size;p->methods.xLock=lock;p->methods.xUnlock=unlock;p->methods.xCheckReservedLock=reserved;p->methods.xFileControl=control;
    if(p->methods.xSectorSize)p->methods.xSectorSize=sector;if(p->methods.xDeviceCharacteristics)p->methods.xDeviceCharacteristics=device;
    if(p->methods.iVersion>=2){if(p->methods.xShmMap)p->methods.xShmMap=map;if(p->methods.xShmLock)p->methods.xShmLock=shm_lock;if(p->methods.xShmBarrier)p->methods.xShmBarrier=barrier;if(p->methods.xShmUnmap)p->methods.xShmUnmap=unmap;}
    if(p->methods.iVersion>=3){if(p->methods.xFetch)p->methods.xFetch=fetch;if(p->methods.xUnfetch)p->methods.xUnfetch=unfetch;}
    p->header.pMethods=&p->methods;
    record([&]{p->handle=++next_handle;const auto canonical=normalized(path);
        if(path){const auto found=names.find(canonical);if(found!=names.end())p->identity=found->second;}
        if(!p->identity){p->identity=++next_entry;Entry e;e.path=canonical;e.kind=classify(flags);e.linked=path&&!p->delete_on_close;entries.emplace(p->identity,std::move(e));if(path)names.insert_or_assign(canonical,p->identity);}
        auto& e=entries.at(p->identity);++e.handles;
    });refresh(p);return rc;
}
inline int remove(sqlite3_vfs*,const char* path,int sync_directory){const auto rc=base->xDelete(base,path,sync_directory);if(rc==SQLITE_OK)record([&]{const auto found=names.find(normalized(path));if(found==names.end())return;const auto id=found->second;names.erase(found);const auto entry=entries.find(id);if(entry!=entries.end()){entry->second.linked=false;if(!entry->second.handles)entries.erase(entry);}});return rc;}
inline int access(sqlite3_vfs*,const char* path,int flags,int* out){return base->xAccess(base,path,flags,out);}
inline int fullpath(sqlite3_vfs*,const char* path,int n,char* out){return base->xFullPathname(base,path,n,out);}
inline void* dlopen(sqlite3_vfs*,const char* path){return base->xDlOpen(base,path);}
inline void dlerror(sqlite3_vfs*,int n,char* out){base->xDlError(base,n,out);}
inline void (*dlsym(sqlite3_vfs*,void* handle,const char* symbol))(void){return base->xDlSym(base,handle,symbol);}
inline void dlclose(sqlite3_vfs*,void* handle){base->xDlClose(base,handle);}
inline int random(sqlite3_vfs*,int n,char* out){return base->xRandomness(base,n,out);}
inline int sleep(sqlite3_vfs*,int us){return base->xSleep(base,us);}
inline int time(sqlite3_vfs*,double* out){return base->xCurrentTime(base,out);}
inline int error(sqlite3_vfs*,int n,char* out){return base->xGetLastError(base,n,out);}
inline int time64(sqlite3_vfs*,sqlite3_int64* out){return base->xCurrentTimeInt64(base,out);}
inline int setcall(sqlite3_vfs*,const char* name,sqlite3_syscall_ptr call){return base->xSetSystemCall(base,name,call);}
inline sqlite3_syscall_ptr getcall(sqlite3_vfs*,const char* name){return base->xGetSystemCall(base,name);}
inline const char* nextcall(sqlite3_vfs*,const char* name){return base->xNextSystemCall(base,name);}
} // namespace detail
inline void install(){
    if(detail::installed)return;
    if(sqlite3_initialize()!=SQLITE_OK)throw std::runtime_error("Cannot initialize measurement VFS");
    detail::base=sqlite3_vfs_find(nullptr);if(!detail::base)throw std::runtime_error("No measurement base VFS");
    detail::wrapper=*detail::base;auto& w=detail::wrapper;w.zName="spentit-test-measure";w.szOsFile=static_cast<int>(sizeof(detail::File))+detail::base->szOsFile;
    w.xOpen=detail::open;w.xDelete=detail::remove;w.xAccess=detail::access;w.xFullPathname=detail::fullpath;
    if(w.xDlOpen)w.xDlOpen=detail::dlopen;if(w.xDlError)w.xDlError=detail::dlerror;if(w.xDlSym)w.xDlSym=detail::dlsym;if(w.xDlClose)w.xDlClose=detail::dlclose;
    w.xRandomness=detail::random;w.xSleep=detail::sleep;w.xCurrentTime=detail::time;if(w.xGetLastError)w.xGetLastError=detail::error;
    if(w.iVersion>=2&&w.xCurrentTimeInt64)w.xCurrentTimeInt64=detail::time64;
    if(w.iVersion>=3){if(w.xSetSystemCall)w.xSetSystemCall=detail::setcall;if(w.xGetSystemCall)w.xGetSystemCall=detail::getcall;if(w.xNextSystemCall)w.xNextSystemCall=detail::nextcall;}
    if(sqlite3_vfs_register(&w,1)!=SQLITE_OK)throw std::runtime_error("Cannot install measurement VFS");detail::installed=true;
}
inline void reset_peaks(){
    std::lock_guard<std::mutex> lock(detail::mutex);detail::recompute();const auto current=detail::totals.current_bytes;const auto total=detail::totals.total_current_bytes;const auto shm=detail::totals.shm_current_logical_bytes;
    detail::totals=Stats{};detail::totals.current_bytes=current;detail::totals.peak_bytes=current;detail::totals.total_current_bytes=detail::totals.total_peak_bytes=total;detail::totals.shm_current_logical_bytes=detail::totals.shm_peak_logical_bytes=shm;detail::started=detail::Clock::now();
}
inline Stats snapshot(){std::lock_guard<std::mutex> lock(detail::mutex);auto result=detail::totals;result.valid=detail::valid.load();result.elapsed_seconds=std::chrono::duration<double>(detail::Clock::now()-detail::started).count();return result;}
} // namespace measure_vfs
