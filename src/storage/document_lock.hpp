#pragma once
#include <filesystem>
#include <memory>

namespace spentit::storage {
// Session locks are independent of SQLite's transaction locks. Files persist;
// only kernel ownership ends at close/crash. Never unlink a lock to acquire it.
class DocumentLock final {
public:
    explicit DocumentLock(const std::filesystem::path& document,
                          const std::filesystem::path& directory = {});
    ~DocumentLock();
    DocumentLock(const DocumentLock&) = delete;
    DocumentLock& operator=(const DocumentLock&) = delete;
    // Called before modifying an existing file, and after exclusive creation.
    void bind_file();
    void validate() const;
    static std::filesystem::path default_directory();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
