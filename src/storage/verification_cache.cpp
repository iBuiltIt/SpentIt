#include "verification_cache.hpp"
#ifndef _WIN32
#include <cerrno>
#include <sys/stat.h>
#else
#include "windows_files.hpp"
#endif

namespace spentit::storage::verification_cache {
// Recovery-sidecar detection only: clean sessions never use file fingerprints
// or disposable audit approvals. Inspection errors conservatively require recovery.
#ifndef _WIN32
bool no_journals(const std::filesystem::path& path) {
    for(const auto* suffix:{"-journal","-wal","-shm"}){
        struct stat info{};if(::lstat((path.string()+suffix).c_str(),&info)==0 || errno!=ENOENT)return false;
    }
    return true;
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
#endif
}
