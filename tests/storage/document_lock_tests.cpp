#include "../../src/storage/document_lock.hpp"
#include <spentit/import_store.hpp>
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
using spentit::storage::DocumentLock;
using spentit::storage::ImportError;
namespace fs=std::filesystem;
namespace {
void check(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value)throw std::runtime_error("Lock assertion failed at line "+std::to_string(where.line()));
}
struct Directory {
    fs::path path;
    Directory(){std::string name=(fs::temp_directory_path()/"spentit-lock-XXXXXX").string();check(::mkdtemp(name.data())!=nullptr);path=fs::canonical(name);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void file(const fs::path& path) {
    const int fd=::open(path.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);check(fd>=0);check(::close(fd)==0);
}
template<class F> void fails(F action,int expected=0) {
    try{action();}catch(const ImportError& e){check(!expected || e.code()==expected);return;}
    throw std::runtime_error("Expected lock rejection");
}
void concurrent(const fs::path& path,const fs::path& root) {
    const auto child=::fork();check(child>=0);
    if(child==0){try{DocumentLock lock(path,root);_exit(1);}catch(const ImportError& e){_exit(e.code()==SQLITE_BUSY?0:2);}catch(...){_exit(3);}}
    int status=0;check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
void behavior() {
    Directory d;const auto root=d.path/"locks",path=d.path/"document";
    {
        DocumentLock pending(path,root);
        concurrent(path,root); // destination reserved before creation
        file(path);pending.bind_file();
        check(!fs::exists(path.string()+".lock"));
        concurrent(path,root);
        const auto moved=d.path/"renamed";fs::rename(path,moved);
        concurrent(moved,root); // identity still locked after rename
        fails([&]{pending.validate();});
        fs::rename(moved,path);pending.validate();
        const auto parentAlias=d.path/"parent-alias";fs::create_directory_symlink(d.path,parentAlias);
        concurrent(parentAlias/"document",root);
        const auto hard=d.path/"hard";fs::create_hard_link(path,hard);
        fails([&]{DocumentLock lock(hard,root);});fs::remove(hard);
        const auto symbolic=d.path/"symbolic";fs::create_symlink(path,symbolic);
        fails([&]{DocumentLock lock(symbolic,root);});
        // Changing permission policy cannot bypass the same-user identity lock.
        check(::chmod(path.c_str(),0644)==0);concurrent(path,root);check(::chmod(path.c_str(),0600)==0);
    }
    {DocumentLock reopened(path,root);reopened.validate();}
    // A shared-permission file retains the adjacent mechanism.
    check(::chmod(path.c_str(),0644)==0);
    {DocumentLock shared(path,root);check(fs::exists(path.string()+".lock"));}
    check(::chmod(path.c_str(),0600)==0);
    // An existing legacy lock is honored, even for an otherwise private file.
    const int legacy=::open((path.string()+".lock").c_str(),O_RDWR);check(legacy>=0);
    check(::flock(legacy,LOCK_EX|LOCK_NB)==0);
    fails([&]{DocumentLock lock(path,root);},SQLITE_BUSY);
    check(::close(legacy)==0);
    check(::chmod((path.string()+".lock").c_str(),0644)==0); // Preserve legacy permission acceptance.
    {DocumentLock lock(path,root);check(fs::exists(path.string()+".lock"));}
    // Failures never leave kernel ownership behind.
    {DocumentLock lock(path,root);}
    check(::chmod(root.c_str(),0777)==0);
    fails([&]{DocumentLock lock(path,root);});check(::chmod(root.c_str(),0700)==0);
    const auto aliasRoot=d.path/"root-alias";fs::create_directory_symlink(root,aliasRoot);
    fails([&]{DocumentLock lock(path,aliasRoot);});
    const auto notDirectory=d.path/"ordinary";file(notDirectory);
    fails([&]{DocumentLock lock(path,notDirectory);});
    // A lock-file symlink must never be followed.
    const auto cleanRoot=d.path/"clean-locks",other=d.path/"not-created";
    {DocumentLock lock(other,cleanRoot);}
    const auto token=fs::directory_iterator(cleanRoot)->path();
    fs::remove(token);fs::create_symlink(notDirectory,token);
    fails([&]{DocumentLock lock(other,cleanRoot);});
    check(fs::file_size(notDirectory)==0 && !fs::exists(other));
}
void crash() {
    Directory d;const auto path=d.path/"document",root=d.path/"locks";file(path);
    int ready[2];check(::pipe(ready)==0);
    const auto child=::fork();check(child>=0);
    if(child==0){::close(ready[0]);try{DocumentLock lock(path,root);const char byte='r';if(::write(ready[1],&byte,1)!=1)_exit(2);for(;;)::pause();}catch(...){_exit(3);}}
    ::close(ready[1]);char byte=0;check(::read(ready[0],&byte,1)==1 && byte=='r');::close(ready[0]);
    fails([&]{DocumentLock lock(path,root);},SQLITE_BUSY);
    check(::kill(child,SIGKILL)==0);int status=0;check(::waitpid(child,&status,0)==child && WIFSIGNALED(status));
    {DocumentLock lock(path,root);lock.validate();}
    check(!fs::exists(path.string()+".lock"));
}
}
int main(){try{behavior();crash();std::cout<<"Document locks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
