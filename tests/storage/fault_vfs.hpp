#pragma once
#include <sqlite3.h>
#include <cstddef>
#include <cstdlib>
#include <unistd.h>

// Test-only VFS proxy: all I/O reaches the original VFS except one selected
// write/sync failure or abrupt process exit. Never linked into production code.
namespace fault_vfs {
inline sqlite3_vfs* base = nullptr;
inline sqlite3_vfs wrapper{};
inline int remaining = -1, seen = 0, code = 0;
inline bool syncs = true, fired = false, crash = false;
struct alignas(std::max_align_t) File { sqlite3_file header; sqlite3_file* real; };
inline File* file(sqlite3_file* f) { return reinterpret_cast<File*>(f); }
inline bool trip(bool syncing) {
    if (syncing && !syncs) return false;
    ++seen;
    if (remaining > 0 && --remaining == 0) {
        fired = true;
        if (crash) _exit(71);
        return true;
    }
    return false;
}
inline int close(sqlite3_file* f) { return file(f)->real->pMethods->xClose(file(f)->real); }
inline int read(sqlite3_file* f, void* p, int n, sqlite3_int64 off) { return file(f)->real->pMethods->xRead(file(f)->real,p,n,off); }
inline int write(sqlite3_file* f, const void* p, int n, sqlite3_int64 off) {
    if (trip(false)) return code;
    return file(f)->real->pMethods->xWrite(file(f)->real,p,n,off);
}
inline int truncate(sqlite3_file* f, sqlite3_int64 n) { return file(f)->real->pMethods->xTruncate(file(f)->real,n); }
inline int sync(sqlite3_file* f, int flags) {
    if (trip(true)) return SQLITE_IOERR_FSYNC;
    return file(f)->real->pMethods->xSync(file(f)->real,flags);
}
inline int size(sqlite3_file* f, sqlite3_int64* n) { return file(f)->real->pMethods->xFileSize(file(f)->real,n); }
inline int lock(sqlite3_file* f,int n) { return file(f)->real->pMethods->xLock(file(f)->real,n); }
inline int unlock(sqlite3_file* f,int n) { return file(f)->real->pMethods->xUnlock(file(f)->real,n); }
inline int reserved(sqlite3_file* f,int* n) { return file(f)->real->pMethods->xCheckReservedLock(file(f)->real,n); }
inline int control(sqlite3_file* f,int n,void* p) { return file(f)->real->pMethods->xFileControl(file(f)->real,n,p); }
inline int sector(sqlite3_file* f) { return file(f)->real->pMethods->xSectorSize(file(f)->real); }
inline int device(sqlite3_file* f) { return file(f)->real->pMethods->xDeviceCharacteristics(file(f)->real); }
inline int map(sqlite3_file* f,int i,int n,int extend,void volatile** p) { return file(f)->real->pMethods->xShmMap(file(f)->real,i,n,extend,p); }
inline int shm_lock(sqlite3_file* f,int off,int n,int flags) { return file(f)->real->pMethods->xShmLock(file(f)->real,off,n,flags); }
inline void barrier(sqlite3_file* f) { file(f)->real->pMethods->xShmBarrier(file(f)->real); }
inline int unmap(sqlite3_file* f,int n) { return file(f)->real->pMethods->xShmUnmap(file(f)->real,n); }
inline int fetch(sqlite3_file* f,sqlite3_int64 off,int n,void** p) { return file(f)->real->pMethods->xFetch(file(f)->real,off,n,p); }
inline int unfetch(sqlite3_file* f,sqlite3_int64 off,void* p) { return file(f)->real->pMethods->xUnfetch(file(f)->real,off,p); }
inline const sqlite3_io_methods methods = {3,close,read,write,truncate,sync,size,lock,unlock,reserved,control,sector,device,map,shm_lock,barrier,unmap,fetch,unfetch};
inline int open(sqlite3_vfs*,const char* name,sqlite3_file* f,int flags,int* out) {
    auto* proxy = file(f);
    proxy->real = reinterpret_cast<sqlite3_file*>(reinterpret_cast<char*>(f) + sizeof(File));
    int rc = base->xOpen(base,name,proxy->real,flags,out);
    if (rc == SQLITE_OK) proxy->header.pMethods = &methods;
    return rc;
}
inline void install() {
    base = sqlite3_vfs_find(nullptr);
    wrapper = *base;
    wrapper.zName = "spentit-test-faults";
    wrapper.szOsFile = static_cast<int>(sizeof(File)) + base->szOsFile;
    wrapper.xOpen = open;
    if (sqlite3_vfs_register(&wrapper,1) != SQLITE_OK) std::abort();
}
inline void arm(int nth, int error = SQLITE_IOERR_WRITE, bool include_sync = true, bool abrupt = false) {
    remaining=nth; code=error; syncs=include_sync; crash=abrupt; seen=0; fired=false;
}
inline void disarm() { remaining=-1; crash=false; }
} // namespace fault_vfs
