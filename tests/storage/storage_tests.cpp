#include <spentit/prototype_store.hpp>
#include "fault_vfs.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sys/wait.h>

using namespace spentit;
using namespace spentit::storage;
namespace fs = std::filesystem;
namespace {
constexpr auto password = "synthetic-test-password-only";
constexpr auto marker = "SYNTHETIC-PRIVATE-EVIDENCE-7fba2d";
void check(bool v) { if (!v) throw std::runtime_error("Storage assertion failed"); }
template<class F> void fails(F f) {
    try { f(); } catch (const StorageError&) { return; }
    throw std::runtime_error("Expected storage failure");
}
struct Directory {
    fs::path path;
    Directory() {
        std::string pattern = (fs::temp_directory_path() / "spentit-storage-XXXXXX").string();
        char* result = mkdtemp(pattern.data());
        if (!result) throw std::runtime_error("Cannot create test directory");
        path = fs::canonical(result);
    }
    ~Directory() { std::error_code ignored; fs::remove_all(path,ignored); }
};
std::string bytes(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot inspect test file");
    return {std::istreambuf_iterator<char>(stream),{}};
}
void no_leaks(const fs::path& directory) {
    for (const auto& item : fs::recursive_directory_iterator(directory)) if (item.is_regular_file()) {
        const auto data = bytes(item.path());
        for (const auto secret : {marker,password,"synthetic-backup-password","private-category-marker","private-memo-marker"})
            check(data.find(secret) == std::string::npos);
        check(!data.starts_with("SQLite format 3"));
    }
}
Transaction sample(std::int64_t units = -2931) {
    const auto currency = Currency::from_definition("six-amendment-174:HRK");
    Money amount(units,currency);
    return Transaction::imported(amount,{{"private-category-marker","private-memo-marker",amount}});
}
void prepare(const fs::path& path) {
    auto store = PrototypeStore::create(path,password);
    store.stage("one",marker,sample());
    store.stage("two",marker,sample(2931));
}
void recovered(const fs::path& path) {
    auto store = PrototypeStore::open(path,password);
    store.verify();
    const auto count = store.committed_amounts().size();
    check(count == 0 || count == 2);
    check(store.load("one").committed == (count == 2));
    check(store.load("two").committed == (count == 2));
    check(store.history_count() == (count == 2 ? 1u : 0u));
    store.commit({"two","one"},"operation");
    store.commit({"one","two"},"operation");
    check(store.history_count() == 1 && store.committed_amounts().size() == 2);
}
void basics() {
    Directory temp;
    const auto path = temp.path / "document";
    prepare(path);
    {
        auto store = PrototypeStore::open(path,password);
        check(store.committed_amounts().empty() && store.history_count() == 0);
        check(store.load("one").evidence == marker);
        check(store.load("one").transaction.effective_total().currency().code() == "HRK");
        const auto original = sample();
        auto correction = original.corrected(Money(-3000,original.effective_total().currency()),
            {{"private-category-marker","",Money(-3000,original.effective_total().currency())}});
        store.stage("corrected",marker,correction);
        check(store.load("corrected").transaction.original_imported_amount()->units() == -2931);
        check(store.load("corrected").transaction.effective_total().units() == -3000);
        fails([&]{ store.stage("one","different",sample()); });
        fails([&]{ store.commit({"one","missing"},"bad"); });
        check(store.history_count() == 0 && !store.load("one").committed);
        store.commit({"one","two"},"operation");
        store.commit({"two","one"},"operation");
        fails([&]{ store.commit({"one"},"operation"); });
        fails([&]{ store.commit({"one","one"},"duplicate"); });
        fails([&]{ store.commit({"one"},"different-operation"); });
    }
    recovered(path);
    const auto before = bytes(path);
    fails([&]{ PrototypeStore::open(path,"wrong-password"); });
    fails([&]{ PrototypeStore::create(path,password); });
    fails([&]{ PrototypeStore::open(temp.path / "missing",password); });
    fails([&]{ PrototypeStore::create(temp.path / "empty-password",""); });
    check(!fs::exists(temp.path / "missing") && !fs::exists(temp.path / "empty-password"));
    {
        auto store=PrototypeStore::open(path,password);
        const std::string large(1024*1024+1,'x');
        fails([&]{store.stage("oversized",large,sample());});
        fails([&]{store.load("oversized");});
    }
    check(bytes(path) == before);
    no_leaks(temp.path);
}
void io_failures() {
    for (const auto error : {SQLITE_FULL,SQLITE_IOERR_WRITE}) {
        int count;
        {
            Directory temp;
            prepare(temp.path / "db");
            auto store = PrototypeStore::open(temp.path / "db",password);
            fault_vfs::arm(-1,error,error != SQLITE_FULL);
            store.commit({"one","two"},"operation");
            count=fault_vfs::seen;
            fault_vfs::disarm();
        }
        check(count > 0);
        for (int nth=1; nth<=count; ++nth) {
            Directory temp;
            const auto path=temp.path / "db";
            prepare(path);
            {
                auto store=PrototypeStore::open(path,password);
                fault_vfs::arm(nth,error,error != SQLITE_FULL);
                fails([&]{store.commit({"one","two"},"operation");});
                check(fault_vfs::fired);
                fault_vfs::disarm();
            }
            no_leaks(temp.path);
            recovered(path);
        }
        std::cout << "Validated " << count << " commit I/O failure positions\n";
    }
}
void child_status(pid_t child,int expected) {
    check(child > 0);
    int status=0;
    check(waitpid(child,&status,0)==child);
    check(WIFEXITED(status) && WEXITSTATUS(status)==expected);
}
void crashes() {
    for (const auto phase : {CommitPhase::before_durable_commit,CommitPhase::after_durable_commit}) {
        Directory temp;
        const auto path=temp.path / "db";
        prepare(path);
        pid_t child=fork();
        if (child==0) {
            auto store=PrototypeStore::open(path,password);
            store.commit({"one","two"},"operation",[&](CommitPhase now){if(now==phase) _exit(71);});
            _exit(72);
        }
        child_status(child,71);
        no_leaks(temp.path);
        {
            auto store=PrototypeStore::open(path,password);
            check(store.history_count()==(phase==CommitPhase::after_durable_commit?1u:0u));
        }
        recovered(path);
    }
    int count;
    {
        Directory temp; prepare(temp.path / "db");
        auto store=PrototypeStore::open(temp.path / "db",password);
        fault_vfs::arm(-1); store.commit({"one","two"},"operation");
        count=fault_vfs::seen; fault_vfs::disarm();
    }
    for(int nth=1;nth<=count;++nth) {
        Directory temp; const auto path=temp.path / "db"; prepare(path);
        pid_t child=fork();
        if(child==0) {
            auto store=PrototypeStore::open(path,password);
            fault_vfs::arm(nth,SQLITE_IOERR_WRITE,true,true);
            store.commit({"one","two"},"operation"); _exit(72);
        }
        child_status(child,71);
        no_leaks(temp.path);
        recovered(path);
    }
    std::cout << "Validated " << count << " abrupt I/O exits and both commit boundaries\n";
}
void backup() {
    Directory temp;
    const auto path=temp.path / "db", saved=temp.path / "backup", restored=temp.path / "restored";
    prepare(path);
    {
        auto store=PrototypeStore::open(path,password);
        store.commit({"one"},"first");
        store.backup_to(saved,password);
        const auto good=bytes(saved);
        fails([&]{store.backup_to(saved,password);});
        check(bytes(saved)==good);
        check(store.history_count()==1);
    }
    {
        auto copy=PrototypeStore::open(saved,password);
        check(copy.load("one").committed && !copy.load("two").committed);
        check(copy.load("one").evidence==marker);
        copy.backup_to(restored,password);
    }
    auto restored_store=PrototypeStore::open(restored,password);
    check(restored_store.committed_amounts().size()==1 && restored_store.history_count()==1);
    check(restored_store.load("one").transaction.effective_total().units()==-2931);
    fails([&]{PrototypeStore::open(saved,"wrong-backup-password");});
    {
        auto source=PrototypeStore::open(path,password);
        source.backup_to(temp.path / "alternate-key","synthetic-backup-password");
        fails([&]{PrototypeStore::open(temp.path / "alternate-key",password);});
        auto alternate=PrototypeStore::open(temp.path / "alternate-key","synthetic-backup-password");
        check(alternate.history_count()==1);
    }
    const auto damaged=temp.path / "damaged";
    fs::copy_file(saved,damaged);
    { std::fstream out(damaged,std::ios::in|std::ios::out|std::ios::binary); out.seekg(1024); const int previous=out.get(); check(previous!=EOF); out.seekp(1024); out.put(static_cast<char>(previous ^ 0xff)); }
    fails([&]{PrototypeStore::open(damaged,password);});
    no_leaks(temp.path);
}
// Deliberately malformed authenticated fixtures and constraint checks.
struct Raw {
    sqlite3* db=nullptr;
    explicit Raw(const fs::path& path) {
        check(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READWRITE,nullptr)==SQLITE_OK);
        check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);
        check(sqlite3_exec(db,"PRAGMA foreign_keys=ON",nullptr,nullptr,nullptr)==SQLITE_OK);
    }
    ~Raw() { sqlite3_close(db); }
    int sql(const char* query) { return sqlite3_exec(db,query,nullptr,nullptr,nullptr); }
};
void validation() {
    {
        Directory temp; const auto plain=temp.path / "plain";
        sqlite3* db=nullptr;
        check(sqlite3_open(plain.c_str(),&db)==SQLITE_OK);
        check(sqlite3_exec(db,"CREATE TABLE plaintext(value TEXT)",nullptr,nullptr,nullptr)==SQLITE_OK);
        check(sqlite3_close(db)==SQLITE_OK);
        const auto original=bytes(plain);
        fails([&]{PrototypeStore::open(plain,password);});
        check(bytes(plain)==original);
    }
    for(const auto mutation : {
        "PRAGMA user_version=999",
        "DROP TRIGGER draft_snapshot; UPDATE drafts SET scale=3 WHERE id='one'",
        "DROP TRIGGER draft_snapshot; UPDATE drafts SET definition='unresolved-definition' WHERE id='one'",
        "DROP TRIGGER allocation_no_update; UPDATE allocations SET units=1 WHERE draft='one'",
        "DROP TRIGGER draft_snapshot; UPDATE drafts SET state='committed' WHERE id='one'"}) {
        Directory temp; const auto path=temp.path / "db"; prepare(path);
        { Raw raw(path); check(raw.sql(mutation)==SQLITE_OK); }
        const auto before=bytes(path);
        bool rejected=false;
        try { PrototypeStore::open(path,password); }
        catch(const StorageError&) { rejected=true; }
        catch(const FinancialError&) { rejected=true; }
        check(rejected && bytes(path)==before);
    }
    Directory temp; const auto path=temp.path / "db"; prepare(path);
    {
        Raw raw(path);
        check(raw.sql("UPDATE sources SET evidence=x'01'")==SQLITE_CONSTRAINT);
        check(raw.sql("DELETE FROM sources")==SQLITE_CONSTRAINT);
        check(raw.sql("INSERT INTO postings VALUES('missing','missing')")==SQLITE_CONSTRAINT);
    }
    {
        auto store=PrototypeStore::open(path,password);
        store.commit({"one","two"},"operation");
        const auto currency=Currency::from_definition("six-amendment-180:BGN");
        const auto low=std::numeric_limits<std::int64_t>::min();
        const auto high=std::numeric_limits<std::int64_t>::max();
        const Money total(-1,currency);
        const auto extreme=Transaction::imported(total,{{"positive","",Money(high,currency)}, {"negative","",Money(low,currency)}});
        store.stage("extreme",marker,extreme);
        store.commit({"extreme"},"extreme-operation");
        check(store.load("extreme").transaction.allocations()[0].amount.units()==high);
        check(store.load("extreme").transaction.allocations()[1].amount.units()==low);
        // A competing writer cannot expose a partial commit.
        store.stage("waiting",marker,sample());
        Raw raw(path); check(raw.sql("BEGIN IMMEDIATE")==SQLITE_OK);
        fails([&]{store.commit({"waiting"},"waiting-operation");});
        check(raw.sql("ROLLBACK")==SQLITE_OK);
        check(!store.load("waiting").committed);
        store.commit({"waiting"},"waiting-operation");
    }
    {
        Raw raw(path);
        for(const auto sql : {"UPDATE history SET operation='changed'", "DELETE FROM history",
             "DELETE FROM postings", "UPDATE drafts SET total=0 WHERE id='one'",
             "INSERT INTO allocations VALUES('one',10,'','',0)"})
            check(raw.sql(sql)==SQLITE_CONSTRAINT);
    }
    no_leaks(temp.path);
}
void backup_crashes() {
    int count;
    {
        Directory temp; prepare(temp.path / "db");
        auto store=PrototypeStore::open(temp.path / "db",password);
        fault_vfs::arm(-1); store.backup_to(temp.path / "backup",password);
        count=fault_vfs::seen; fault_vfs::disarm();
    }
    for(int nth=1;nth<=count;++nth) {
        Directory temp; const auto path=temp.path / "db"; prepare(path);
        const auto saved=temp.path / "good";
        { auto store=PrototypeStore::open(path,password); store.backup_to(saved,password); }
        const auto before=bytes(saved);
        pid_t child=fork();
        if(child==0) {
            auto store=PrototypeStore::open(path,password);
            fault_vfs::arm(nth,SQLITE_IOERR_WRITE,true,true);
            store.backup_to(temp.path / "interrupted",password); _exit(72);
        }
        child_status(child,71);
        check(!fs::exists(temp.path / "interrupted") && bytes(saved)==before);
        no_leaks(temp.path);
        auto good=PrototypeStore::open(saved,password);
        check(good.load("one").evidence==marker);
        auto source=PrototypeStore::open(path,password);
        source.verify();
    }
    std::cout << "Validated " << count << " interrupted backup I/O positions\n";
}
void backup_failures() {
    for(const auto error : {SQLITE_FULL,SQLITE_IOERR_WRITE}) {
        int count;
        {
            Directory temp; prepare(temp.path / "db");
            auto store=PrototypeStore::open(temp.path / "db",password);
            fault_vfs::arm(-1,error,error!=SQLITE_FULL);
            store.backup_to(temp.path / "backup",password);
            count=fault_vfs::seen; fault_vfs::disarm();
        }
        for(int nth=1;nth<=count;++nth) {
            Directory temp; const auto path=temp.path / "db"; prepare(path);
            auto store=PrototypeStore::open(path,password);
            const auto good=temp.path / "good";
            store.backup_to(good,password);
            const auto before=bytes(good);
            fault_vfs::arm(nth,error,error!=SQLITE_FULL);
            fails([&]{store.backup_to(temp.path / "failed",password);});
            check(fault_vfs::fired); fault_vfs::disarm();
            check(!fs::exists(temp.path / "failed") && bytes(good)==before);
            store.verify();
            auto restored=PrototypeStore::open(good,password);
            check(restored.load("one").evidence==marker);
            no_leaks(temp.path);
        }
        std::cout << "Validated " << count << " backup I/O failure positions\n";
    }
}
void stage_failures() {
    for(const auto error : {SQLITE_FULL,SQLITE_IOERR_WRITE}) {
        int count;
        {
            Directory temp; auto store=PrototypeStore::create(temp.path / "db",password);
            fault_vfs::arm(-1,error,error!=SQLITE_FULL); store.stage("one",marker,sample());
            count=fault_vfs::seen; fault_vfs::disarm();
        }
        for(int nth=1;nth<=count;++nth) {
            Directory temp; const auto path=temp.path / "db";
            {
                auto store=PrototypeStore::create(path,password);
                fault_vfs::arm(nth,error,error!=SQLITE_FULL);
                fails([&]{store.stage("one",marker,sample());});
                check(fault_vfs::fired); fault_vfs::disarm();
            }
            auto store=PrototypeStore::open(path,password);
            store.verify();
            fails([&]{store.load("one");});
            store.stage("one",marker,sample());
            check(store.load("one").evidence==marker && store.committed_amounts().empty());
            no_leaks(temp.path);
        }
        std::cout << "Validated " << count << " draft I/O failure positions\n";
    }
}

}
int main(int argc,char** argv) {
    try {
        fault_vfs::install();
        if(argc!=2) throw std::runtime_error("Expected test case");
        const std::string name=argv[1];
        if(name=="basics") basics();
        else if(name=="io") io_failures();
        else if(name=="crashes") crashes();
        else if(name=="backup") backup();
        else if(name=="backup_failures") backup_failures();
        else if(name=="validation") validation();
        else if(name=="stage_failures") stage_failures();
        else if(name=="backup_crashes") backup_crashes();
        else throw std::runtime_error("Unknown test case");
        std::cout << name << " passed\n";
        return 0;
    } catch(const StorageError& error) {
        std::cerr << "FAIL: " << error.what() << " (code " << error.code() << ")\n";
    } catch(const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; }
    return 1;
}
