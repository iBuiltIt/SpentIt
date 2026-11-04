#pragma once
// Windows equivalents of the POSIX publication, lock and verification file
// operations. Included only from Windows storage translation units. The Unix
// call sites stay in those units and are not compiled here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <aclapi.h>
#include <sqlite3.h>

// aclapi.h pulls in rpcndr.h, which defines these as macros.
#ifdef small
#undef small
#endif
#ifdef hyper
#undef hyper
#endif

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace spentit::storage::win {
struct FileResult {
    int code = SQLITE_OK;
    const char* message = "";
    explicit operator bool() const { return code == SQLITE_OK; }
};
struct FileLock {
    void* handle = nullptr;
};
struct FileId {
    DWORD volume = 0;
    DWORD index_low = 0;
    DWORD index_high = 0;
    DWORD links = 0;
    bool directory = false;
    bool reparse = false;
    bool operator==(const FileId& other) const {
        return volume == other.volume && index_low == other.index_low && index_high == other.index_high;
    }
};

inline void close_handle(void* handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) CloseHandle(static_cast<HANDLE>(handle));
}
inline void close_lock(FileLock& lock) {
    close_handle(lock.handle);
    lock.handle = nullptr;
}

inline std::string sqlite_filename(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

inline std::vector<unsigned char> current_user() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> bytes(size);
    if (size == 0 || !GetTokenInformation(token, TokenUser, bytes.data(), size, &size)) bytes.clear();
    CloseHandle(token);
    return bytes;
}
inline bool set_owner_only(HANDLE handle) {
    auto bytes = current_user();
    if (bytes.empty()) return false;
    auto* user = reinterpret_cast<TOKEN_USER*>(bytes.data());
    EXPLICIT_ACCESSW entry{};
    entry.grfAccessPermissions = FILE_ALL_ACCESS;
    entry.grfAccessMode = SET_ACCESS;
    entry.grfInheritance = NO_INHERITANCE;
    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
    entry.Trustee.ptstrName = static_cast<LPWCH>(user->User.Sid);
    PACL dacl = nullptr;
    if (SetEntriesInAclW(1, &entry, nullptr, &dacl) != ERROR_SUCCESS) return false;
    const DWORD applied = SetSecurityInfo(handle, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl, nullptr);
    LocalFree(dacl);
    return applied == ERROR_SUCCESS;
}
inline bool owned_by_current_user(HANDLE handle) {
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &descriptor) != ERROR_SUCCESS)
        return false;
    auto bytes = current_user();
    const bool same = !bytes.empty() && owner != nullptr && EqualSid(owner, reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid);
    LocalFree(descriptor);
    return same;
}
inline bool owner_only_dacl(HANDLE handle) {
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &descriptor) != ERROR_SUCCESS)
        return false;
    bool allowed = false;
    if (dacl != nullptr) {
        auto bytes = current_user();
        ACL_SIZE_INFORMATION info{};
        allowed = !bytes.empty() && GetAclInformation(dacl, &info, sizeof info, AclSizeInformation);
        auto* user = reinterpret_cast<TOKEN_USER*>(bytes.data());
        for (DWORD index = 0; allowed && index < info.AceCount; ++index) {
            void* ace = nullptr;
            if (!GetAce(dacl, index, &ace)) allowed = false;
            else {
                auto* header = static_cast<ACE_HEADER*>(ace);
                if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                    auto* sid = reinterpret_cast<PSID>(&static_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart);
                    if (!EqualSid(sid, user->User.Sid)) allowed = false;
                } else if (header->AceType != ACCESS_DENIED_ACE_TYPE) {
                    allowed = false;
                }
            }
        }
    }
    LocalFree(descriptor);
    return allowed;
}
inline std::optional<FileId> identify(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION info{};
    FILE_ATTRIBUTE_TAG_INFO tag{};
    if (!GetFileInformationByHandle(handle, &info)) return std::nullopt;
    if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof tag)) return std::nullopt;
    FileId id;
    id.volume = info.dwVolumeSerialNumber;
    id.index_low = info.nFileIndexLow;
    id.index_high = info.nFileIndexHigh;
    id.links = info.nNumberOfLinks;
    id.directory = (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    id.reparse = (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    return id;
}
inline HANDLE open_nofollow(const std::filesystem::path& path, DWORD access, DWORD share, DWORD disposition, DWORD extra) {
    return CreateFileW(path.c_str(), access, share, nullptr, disposition,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | extra, nullptr);
}
inline bool is_single_regular_file(const std::filesystem::path& path) {
    const HANDLE handle = open_nofollow(path, FILE_READ_ATTRIBUTES | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const auto id = identify(handle);
    close_handle(handle);
    return id && !id->directory && !id->reparse && id->links == 1;
}
inline bool path_exists(const std::filesystem::path& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}
inline FileResult exclusive_file(const std::filesystem::path& path) {
    const HANDLE handle = open_nofollow(path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC, 0, CREATE_NEW, 0);
    if (handle == INVALID_HANDLE_VALUE) return {SQLITE_CANTOPEN, "Cannot create new storage file exclusively"};
    const auto id = identify(handle);
    const bool secured = id && !id->reparse && !id->directory && set_owner_only(handle);
    const BOOL closed = CloseHandle(handle);
    if (!secured) {
        DeleteFileW(path.c_str());
        return {SQLITE_CANTOPEN, "Cannot create new storage file exclusively"};
    }
    if (!closed) return {SQLITE_IOERR_CLOSE, "Encrypted storage operation failed"};
    return {};
}
inline FileResult sync_directory(const std::filesystem::path& directory, const char* message) {
    // Attribute-only access opens the directory, then FlushFileBuffers returns
    // ERROR_ACCESS_DENIED. Write access is what this host accepts. The call is
    // the NTFS analogue of a parent fsync, not a physical power-loss proof.
    const HANDLE handle = CreateFileW(directory.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {SQLITE_IOERR_FSYNC, message};
    const BOOL flushed = FlushFileBuffers(handle);
    const BOOL closed = CloseHandle(handle);
    if (!flushed || !closed) return {SQLITE_IOERR_FSYNC, message};
    return {};
}
inline FileResult sync_parent(const std::filesystem::path& path) {
    auto parent = path.parent_path();
    if (parent.empty()) parent = ".";
    return sync_directory(parent, "Encrypted storage operation failed");
}
inline FileResult acquire_lock(const std::filesystem::path& path, FileLock& lock) {
    const HANDLE handle = open_nofollow(path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_ALWAYS, 0);
    if (handle == INVALID_HANDLE_VALUE) return {SQLITE_BUSY, "Document is already open or unavailable"};
    const DWORD opened = GetLastError();
    const auto id = identify(handle);
    if (!id || id->reparse || id->directory) {
        close_handle(handle);
        return {SQLITE_BUSY, "Document is already open or unavailable"};
    }
    if (opened != ERROR_ALREADY_EXISTS && !set_owner_only(handle)) {
        close_handle(handle);
        return {SQLITE_CANTOPEN, "Document lock is not a supported regular file"};
    }
    OVERLAPPED overlapped{};
    if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &overlapped)) {
        close_handle(handle);
        return {SQLITE_BUSY, "Document is already open or unavailable"};
    }
    const auto held = identify(handle);
    if (!held || held->reparse || held->directory || held->links != 1 || !owned_by_current_user(handle)) {
        close_handle(handle);
        return {SQLITE_CANTOPEN, "Document lock is not a supported regular file"};
    }
    lock.handle = handle;
    return {};
}
inline FileResult remove_if_present(const std::filesystem::path& path) {
    if (DeleteFileW(path.c_str())) return {};
    const DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return {};
    return {SQLITE_IOERR_DELETE, "Cannot remove the old dataset journal"};
}
inline FileResult truncate_owned_regular(const std::filesystem::path& path) {
    const HANDLE probe = open_nofollow(path, FILE_READ_ATTRIBUTES | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0);
    if (probe == INVALID_HANDLE_VALUE) return {SQLITE_CANTOPEN, "Cannot overwrite dataset"};
    const auto before = identify(probe);
    close_handle(probe);
    if (!before || before->directory || before->reparse || before->links != 1) return {SQLITE_IOERR, "Cannot overwrite dataset"};
    const HANDLE handle = open_nofollow(path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC, 0, OPEN_EXISTING, 0);
    if (handle == INVALID_HANDLE_VALUE) return {SQLITE_CANTOPEN, "Cannot overwrite dataset"};
    const auto opened = identify(handle);
    if (!opened || !(*opened == *before) || opened->links != 1 || opened->reparse || opened->directory || !set_owner_only(handle)) {
        close_handle(handle);
        return {SQLITE_IOERR, "Cannot overwrite dataset"};
    }
    if (SetFilePointer(handle, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER || !SetEndOfFile(handle)) {
        close_handle(handle);
        return {SQLITE_IOERR, "Cannot overwrite dataset"};
    }
    const BOOL flushed = FlushFileBuffers(handle);
    const BOOL closed = CloseHandle(handle);
    if (!flushed || !closed) return {SQLITE_IOERR, "Dataset replacement could not be completed"};
    return {};
}
inline FileResult publish_link(const std::filesystem::path& candidate, const std::filesystem::path& destination, const char* message) {
    if (!CreateHardLinkW(destination.c_str(), candidate.c_str(), nullptr)) return {SQLITE_CANTOPEN, message};
    return {};
}
inline FileResult remove_file(const std::filesystem::path& path, const char* message) {
    if (!DeleteFileW(path.c_str())) return {SQLITE_IOERR_DELETE, message};
    return {};
}

inline HANDLE open_read_nofollow(const std::filesystem::path& path) {
    return open_nofollow(path, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0);
}
inline bool regular_private(HANDLE handle, bool require_owner, bool require_private, std::uint64_t* size) {
    const auto id = identify(handle);
    if (!id || id->directory || id->reparse || id->links != 1) return false;
    if (require_owner && !owned_by_current_user(handle)) return false;
    if (require_private && !owner_only_dacl(handle)) return false;
    if (size != nullptr) {
        LARGE_INTEGER bytes{};
        if (!GetFileSizeEx(handle, &bytes) || bytes.QuadPart < 0) return false;
        *size = static_cast<std::uint64_t>(bytes.QuadPart);
    }
    return true;
}
inline bool same_named_file(HANDLE handle, const std::filesystem::path& path) {
    const HANDLE named = open_read_nofollow(path);
    if (named == INVALID_HANDLE_VALUE) return false;
    const auto left = identify(handle);
    const auto right = identify(named);
    close_handle(named);
    return left && right && *left == *right && !right->reparse;
}
inline std::int64_t read_some(HANDLE handle, char* data, std::size_t capacity) {
    DWORD count = 0;
    if (capacity > MAXDWORD) return -1;
    if (!ReadFile(handle, data, static_cast<DWORD>(capacity), &count, nullptr)) return -1;
    return count;
}
inline HANDLE create_private(const std::filesystem::path& path) {
    // FILE_SHARE_DELETE lets the caller replace or remove this file while the
    // handle is still open, which is what the POSIX mkstemp/rename/unlink path does.
    const HANDLE handle = open_nofollow(path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
        FILE_SHARE_DELETE, CREATE_NEW, 0);
    if (handle == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    if (!set_owner_only(handle)) {
        close_handle(handle);
        DeleteFileW(path.c_str());
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}
inline bool write_all(HANDLE handle, std::string_view value) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const std::size_t remaining = value.size() - offset;
        DWORD count = 0;
        if (remaining > MAXDWORD || !WriteFile(handle, value.data() + offset, static_cast<DWORD>(remaining), &count, nullptr) || count == 0)
            return false;
        offset += count;
    }
    return true;
}
inline bool rename_replace(const std::filesystem::path& from, const std::filesystem::path& to) {
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
inline HANDLE open_directory(const std::filesystem::path& path, DWORD access) {
    return CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}
} // namespace spentit::storage::win
#pragma comment(lib, "advapi32.lib")
