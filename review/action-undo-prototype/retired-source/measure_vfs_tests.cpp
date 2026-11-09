#include "measure_vfs.hpp"
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
namespace fs=std::filesystem;
using measure_vfs::Kind;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr auto index(Kind kind){return static_cast<std::size_t>(kind);}
struct Directory {
    fs::path path;
    Directory(){
        std::random_device random;
        for(int attempt=0;attempt<32;++attempt){
            auto candidate=fs::temp_directory_path()/("spentit-vfs-"+std::to_string(random())+"-"+std::to_string(random()));
            std::error_code error;
            if(fs::create_directory(candidate,error)){path=std::move(candidate);return;}
            require(!error||error==std::errc::file_exists,"Cannot create synthetic VFS test directory");
        }
        throw std::runtime_error("Cannot allocate unique VFS test directory");
    }
    ~Directory(){std::error_code ignored;fs::remove_all(path,ignored);}
};
struct Database {
    sqlite3* value=nullptr;
    void open(const fs::path& path){
        require(!value,"Database already open");
        require(sqlite3_open(path.c_str(),&value)==SQLITE_OK,"Cannot open synthetic database");
#ifdef SQLITE_HAS_CODEC
        constexpr auto password="synthetic-vfs";
        require(sqlite3_key(value,password,13)==SQLITE_OK,"Cannot key synthetic database");
#endif
    }
    void close(){auto* closing=value;value=nullptr;require(sqlite3_close(closing)==SQLITE_OK,"Cannot close synthetic database");}
    ~Database(){if(value)sqlite3_close_v2(value);}
    void exec(const char* sql){
        char* error=nullptr;const auto result=sqlite3_exec(value,sql,nullptr,nullptr,&error);sqlite3_free(error);
        require(result==SQLITE_OK,"Synthetic SQLite operation failed");
    }
};
std::uint64_t bytes(const fs::path& path){
    std::error_code error;const auto result=fs::file_size(path,error);
    if(error==std::errc::no_such_file_or_directory)return 0;
    require(!error,"Cannot inspect synthetic file size");return result;
}
void check_files(const fs::path& path){
    const auto stats=measure_vfs::snapshot();require(stats.valid,"VFS measurements became invalid");
    const auto main=bytes(path),journal=bytes(path.string()+"-journal"),wal=bytes(path.string()+"-wal");
    require(stats.current_bytes[index(Kind::main_db)]==main,"Main-file accounting differs from filesystem");
    require(stats.current_bytes[index(Kind::rollback_journal)]==journal,"Rollback-journal accounting differs from filesystem");
    require(stats.current_bytes[index(Kind::wal)]==wal,"WAL accounting differs from filesystem");
    require(stats.total_current_bytes==main+journal+wal,"Simultaneous aggregate differs from filesystem");
}
struct RawFile {
    sqlite3_vfs* vfs=sqlite3_vfs_find(nullptr);
    std::vector<std::max_align_t> memory;
    std::string path;
    sqlite3_file* value=nullptr;
    bool opened=false;
    RawFile(const fs::path& name,int flags):memory((static_cast<std::size_t>(vfs->szOsFile)+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t)),path(name.string()){
        value=reinterpret_cast<sqlite3_file*>(memory.data());int output_flags=0;
        require(vfs->xOpen(vfs,path.empty()?nullptr:path.c_str(),value,flags,&output_flags)==SQLITE_OK,"Cannot open synthetic VFS file");opened=true;
    }
    void write(int n,sqlite3_int64 offset=0){
        std::vector<unsigned char> data(static_cast<std::size_t>(n));
        require(value->pMethods->xWrite(value,data.data(),n,offset)==SQLITE_OK,"Cannot write synthetic VFS file");
    }
    void truncate(sqlite3_int64 n){require(value->pMethods->xTruncate(value,n)==SQLITE_OK,"Cannot truncate synthetic VFS file");}
    void close(){const auto result=value->pMethods->xClose(value);opened=false;require(result==SQLITE_OK,"Cannot close synthetic VFS file");}
    ~RawFile(){if(opened)value->pMethods->xClose(value);}
};
void remove_file(const fs::path& path){auto* vfs=sqlite3_vfs_find(nullptr);require(vfs->xDelete(vfs,path.c_str(),0)==SQLITE_OK,"Cannot remove synthetic VFS file");}
void sqlite_modes(const fs::path& path){
    Database db;db.open(path);
    db.exec("PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA cache_size=1; CREATE TABLE t(id INTEGER PRIMARY KEY,b BLOB); INSERT INTO t VALUES(1,zeroblob(262144));");
    check_files(path);measure_vfs::reset_peaks();
    auto stats=measure_vfs::snapshot();require(stats.total_peak_bytes==stats.total_current_bytes,"Reset lost current file extent");
    db.exec("BEGIN IMMEDIATE; UPDATE t SET b=zeroblob(524288);");check_files(path);
    require(measure_vfs::snapshot().peak_bytes[index(Kind::rollback_journal)]>0,"Rollback journal peak was not recorded");
    db.exec("ROLLBACK");check_files(path);
    require(measure_vfs::snapshot().current_bytes[index(Kind::rollback_journal)]==0,"Deleted rollback journal remains counted");
    require(measure_vfs::snapshot().write_bytes>0,"Successful write bytes were not recorded");

    db.exec("PRAGMA journal_mode=TRUNCATE; BEGIN; UPDATE t SET b=zeroblob(131072); COMMIT;");check_files(path);
    require(measure_vfs::snapshot().current_bytes[index(Kind::rollback_journal)]==0,"Truncated journal remains counted");
    require(measure_vfs::snapshot().sync_calls>0,"Successful sync calls were not recorded");

    db.exec("PRAGMA journal_mode=PERSIST; BEGIN; UPDATE t SET b=zeroblob(196608); COMMIT;");check_files(path);
    require(measure_vfs::snapshot().current_bytes[index(Kind::rollback_journal)]>0,"Persistent journal was omitted");
    db.close();check_files(path);const auto closed=measure_vfs::snapshot();db.open(path);check_files(path);
    require(measure_vfs::snapshot().total_current_bytes==closed.total_current_bytes,"Reopening counted persistent files twice");

    db.exec("PRAGMA journal_mode=WAL; PRAGMA wal_autocheckpoint=0; INSERT INTO t VALUES(2,zeroblob(65536));");check_files(path);
    require(measure_vfs::snapshot().current_bytes[index(Kind::wal)]>0,"WAL file was omitted");
    require(measure_vfs::snapshot().shm_current_logical_bytes>=32768,"Shared-memory mapping was omitted");
    db.exec("PRAGMA wal_checkpoint(TRUNCATE)");check_files(path);
    require(measure_vfs::snapshot().current_bytes[index(Kind::wal)]==0,"Truncated WAL remains counted");
    db.close();check_files(path);
    require(measure_vfs::snapshot().shm_current_logical_bytes==0,"Closed shared-memory mapping remains counted");
    for(const auto* suffix:{"-journal","-wal"}){const fs::path sidecar=path.string()+suffix;if(fs::exists(sidecar))remove_file(sidecar);}
    remove_file(path);require(measure_vfs::snapshot().total_current_bytes==0,"Deleted database remains counted");
}
void direct_file_lifetimes(const fs::path& root){
    {
        RawFile temp({},SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_DELETEONCLOSE|SQLITE_OPEN_TEMP_DB);
        temp.write(256);require(measure_vfs::snapshot().current_bytes[index(Kind::temp)]==256,"Temporary file extent was omitted");
        temp.close();require(measure_vfs::snapshot().current_bytes[index(Kind::temp)]==0,"Delete-on-close temporary file remains counted");
    }
    const auto extra=root/"other";
    {RawFile other(extra,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_SUPER_JOURNAL);other.write(128);other.close();}
    require(measure_vfs::snapshot().current_bytes[index(Kind::other)]==128,"Closed persistent other file was omitted");
    remove_file(extra);require(measure_vfs::snapshot().current_bytes[index(Kind::other)]==0,"Deleted other file remains counted");
}
void simultaneous_peak(const fs::path& root){
    const auto db_path=root/"peak-db",journal_path=root/"peak-db-journal";
    RawFile db(db_path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_MAIN_DB);
    RawFile journal(journal_path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_MAIN_JOURNAL);
    measure_vfs::reset_peaks();require(measure_vfs::snapshot().total_current_bytes==0,"Peak fixture did not start empty");
    db.write(4096);journal.write(8192);
    auto stats=measure_vfs::snapshot();require(stats.total_current_bytes==12288&&stats.total_peak_bytes==12288,"Simultaneous database/journal peak was not exact");
    journal.truncate(1024);db.write(4096,4096);
    stats=measure_vfs::snapshot();require(stats.total_current_bytes==9216&&stats.total_peak_bytes==12288,"Peak incorrectly summed peaks from different times");
    require(stats.peak_bytes[index(Kind::main_db)]==8192&&stats.peak_bytes[index(Kind::rollback_journal)]==8192,"Per-kind peaks were not retained");
    require(stats.total_peak_bytes<stats.peak_bytes[index(Kind::main_db)]+stats.peak_bytes[index(Kind::rollback_journal)],"Aggregate is sum of independent peaks");
    journal.close();remove_file(journal_path);measure_vfs::reset_peaks();
    stats=measure_vfs::snapshot();require(stats.total_current_bytes==8192&&stats.total_peak_bytes==8192,"Reset did not retain current database size");
    {RawFile second(db_path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_MAIN_DB);require(measure_vfs::snapshot().total_current_bytes==8192,"Second handle doubled database size");second.close();}
    db.close();remove_file(db_path);stats=measure_vfs::snapshot();
    require(stats.valid&&stats.total_current_bytes==0&&stats.total_peak_bytes==8192,"Final lifetime accounting failed");
    require(stats.elapsed_seconds>=0,"Elapsed interval is invalid");
}
} // namespace
int main(){try{
    Directory directory;measure_vfs::install();
    fs::create_directory(directory.path/"real");
    fs::create_directory_symlink("real",directory.path/"alias");
    // Exercise an explicit directory alias: SQLite opens a canonical filename,
    // while the direct cleanup call below uses the original alias.
    sqlite_modes(directory.path/"alias"/"test.db");
    direct_file_lifetimes(directory.path);simultaneous_peak(directory.path);
    std::cout<<"VFS measurement tests passed: DELETE, TRUNCATE, PERSIST, WAL, temporary lifetimes, reopening and exact simultaneous peaks.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
