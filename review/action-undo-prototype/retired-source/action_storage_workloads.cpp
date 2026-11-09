// Same synthetic forward workload for cf8f1a5, current production and the isolated
// captured-payload prototype. Prototype plumbing is compile-time selected; all
// financial operations, assertions and checkpoints are common.
#include "action_journal_runtime.hpp"
#include "measure_vfs.hpp"
#include <iomanip>
#include <chrono>
#include <utility>
using namespace action_probe;
using namespace spentit;
namespace fs=std::filesystem;
namespace {
const Currency cad("CAD");
#ifdef SPENTIT_WORKLOAD_UUID_IDS
constexpr const char* first_batch="00000000-0000-4000-8000-000000000001";
constexpr const char* second_batch="00000000-0000-4000-8000-000000000002";
#else
constexpr const char* first_batch="first";
constexpr const char* second_batch="second";
#endif
std::uint64_t cumulative_peak=0;
#ifdef SPENTIT_ACTION_CAPTURE
std::int64_t peak_inverse_rows=0,peak_inverse_bytes=0,peak_pinned_bytes=0;
#endif
std::string mode;
void reset_measurement(){const auto prior=measure_vfs::snapshot();require(prior.valid&&prior.failed_writes==0&&prior.failed_syncs==0,"Prior VFS phase was invalid or failed");cumulative_peak=std::max(cumulative_peak,prior.total_peak_bytes);measure_vfs::reset_peaks();}
ImportStore open_store(const fs::path& target,bool create){reset_measurement();return create?ImportStore::create(target,password,"Synthetic",cad,"2026-01-01"):ImportStore::open(target,password);}

void physical(const fs::path& path,const std::string& phase,sqlite3* db){
    const auto stats=measure_vfs::snapshot();require(stats.valid&&stats.failed_writes==0&&stats.failed_syncs==0,"VFS metrics invalid or unexpected I/O failure");cumulative_peak=std::max(cumulative_peak,stats.total_peak_bytes);
    const auto page=number(db,"PRAGMA page_size"),pages=number(db,"PRAGMA page_count"),free=number(db,"PRAGMA freelist_count");
    std::cout<<"phase,"<<mode<<','<<phase<<",file_bytes="<<fs::file_size(path)<<",live_page_bytes="<<(pages-free)*page<<",free_page_bytes="<<free*page
        <<",vfs_total_peak="<<stats.total_peak_bytes<<",vfs_cumulative_peak="<<cumulative_peak<<",vfs_total_current="<<stats.total_current_bytes;
    for(std::size_t i=0;i<measure_vfs::kind_count;++i)std::cout<<",vfs_"<<measure_vfs::name(static_cast<measure_vfs::Kind>(i))<<"_peak="<<stats.peak_bytes[i];
    std::cout<<",shm_logical_peak="<<stats.shm_peak_logical_bytes<<",write_bytes="<<stats.write_bytes<<",write_calls="<<stats.write_calls<<",sync_calls="<<stats.sync_calls<<",elapsed_seconds="<<std::fixed<<std::setprecision(6)<<stats.elapsed_seconds<<'\n';
}
#ifdef SPENTIT_ACTION_CAPTURE
void close_cleanup(sqlite3* db){
    require(!sqlite3_get_autocommit(db),"Close cleanup escaped transaction");
    sql(db,"DELETE FROM probe_inverse; DELETE FROM probe_payload_pins; DELETE FROM probe_actions");
    collect_payloads(db);
    require(number(db,"SELECT count(*) FROM probe_inverse")==0&&number(db,"SELECT count(*) FROM probe_payload_pins")==0,"Close did not expire journal/pins");
}
void logical(sqlite3* db,const std::string& phase){
    const auto rows=number(db,"SELECT count(*) FROM probe_inverse");
    const auto keys=number(db,"SELECT coalesce(sum(length(key)),0) FROM probe_inverse"),images=number(db,"SELECT coalesce(sum(length(prior)),0) FROM probe_inverse");
    const auto pins=number(db,"SELECT count(*) FROM probe_payload_pins");std::int64_t pinned=0,pinned_rows=0;
    for(const auto& [name,spec]:storage::action_payload_probe::specs()){
        const auto predicate=" EXISTS(SELECT 1 FROM probe_payload_pins WHERE name='"+name+"' AND key="+expression(spec.keys,"p")+") AND NOT EXISTS(SELECT 1 FROM "+sql_identifier(spec.membership)+" m WHERE "+storage::action_payload_probe::equality(spec.keys,"m","p")+")";
        pinned_rows+=number(db,"SELECT count(*) FROM "+sql_identifier(spec.payload)+" p WHERE"+predicate);
        pinned+=number(db,"SELECT coalesce(sum(length("+expression(spec.columns,"p")+")),0) FROM "+sql_identifier(spec.payload)+" p WHERE"+predicate);
    }
    peak_inverse_rows=std::max(peak_inverse_rows,rows);peak_inverse_bytes=std::max(peak_inverse_bytes,keys+images);peak_pinned_bytes=std::max(peak_pinned_bytes,pinned);
    std::cout<<"logical,"<<mode<<','<<phase<<",inverse_rows="<<rows<<",inverse_key_bytes="<<keys<<",inverse_image_bytes="<<images<<",pin_references="<<pins<<",unique_pinned_only_bodies="<<pinned_rows<<",pinned_only_codec_bytes="<<pinned
        <<",peak_inverse_rows="<<peak_inverse_rows<<",peak_inverse_codec_bytes="<<peak_inverse_bytes<<",peak_pinned_only_codec_bytes="<<peak_pinned_bytes<<'\n';
}
#endif
class Session {
public:
    ImportStore store;
    sqlite3* db;
    fs::path path;
#ifdef SPENTIT_ACTION_CAPTURE
    std::vector<Table> manifest;
#endif
    Session(const fs::path& target,bool create):store(open_store(target,create)),db(connection),path(target){
        require(db&&fs::equivalent(sqlite3_db_filename(db,"main"),path),"Wrong workload connection");
#ifdef SPENTIT_ACTION_CAPTURE
        storage::action_payload_probe::load_specs(db);
        manifest=tables(db);install(db,manifest);install_payload_pins(db);
        sql(db,"CREATE TABLE IF NOT EXISTS probe_actions(sequence INTEGER PRIMARY KEY AUTOINCREMENT,label TEXT NOT NULL) STRICT");
        storage::action_payload_probe::close_cleanup=close_cleanup;
#endif
        physical(path,create?"session-created":"session-opened",db);
    }
    ~Session(){
#ifdef SPENTIT_ACTION_CAPTURE
        storage::action_payload_probe::close_cleanup=nullptr;
#endif
    }
    template<class F>void action(const std::string& label,F&& operation){
        reset_measurement();
#ifdef SPENTIT_ACTION_CAPTURE
        sql(db,"INSERT INTO probe_actions(label) VALUES("+str_literal(label)+")");const auto id=sqlite3_last_insert_rowid(db);
        sql(db,"UPDATE probe_context SET action="+std::to_string(id));
#endif
        try{std::forward<F>(operation)();}
        catch(...){
#ifdef SPENTIT_ACTION_CAPTURE
            sql(db,"UPDATE probe_context SET action=0");
#endif
            throw;
        }
#ifdef SPENTIT_ACTION_CAPTURE
        logical(db,label+"-precoalesce");sql(db,"UPDATE probe_context SET action=0; BEGIN IMMEDIATE");
        try{coalesce(db,manifest,id);sql(db,"COMMIT");}catch(...){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
        logical(db,label+"-active");
#endif
        physical(path,label,db);
    }
    void close(const std::string& label){
        store.verify();reset_measurement();store.certify({});
#ifdef SPENTIT_ACTION_CAPTURE
        logical(db,label+"-closed");require(number(db,"SELECT count(*) FROM probe_actions")==0,"Closed actions retained");
#endif
        physical(path,label,db);
    }
};
void edit(Session& session,int generation,int index,std::size_t ordinal=0){
    const imports::Occurrence id{first_batch,ordinal};auto d=session.store.committed_entry(id).current.draft;d.payee="Edited "+std::to_string(generation)+" "+std::to_string(index);
    app::ChangeRequest request{app::ChangeKind::correct,id,d,{}};const auto preview=session.store.preview_committed_change(request);
    session.action("edit-"+std::to_string(generation)+"-"+std::to_string(index),[&]{session.store.apply_committed_change("edit-"+std::to_string(generation)+"-"+std::to_string(index),request,preview.expected,preview.preview_sequence);});
}
std::string rich_source(int count,int offset){
    std::string text;for(int i=0;i<count;++i){const auto identity=i+offset;
        text+="D"+std::to_string(1+i%28)+"/"+std::to_string(1+(i/28)%12)+"/2026\nT-1.00\nPSynthetic merchant "+std::to_string(identity)+"\nN"+std::to_string(identity)+"\nMOnline purchase ref "+std::to_string(identity)+"; synthetic detail retained for reconciliation and review.\n";
        if(i%10==0)text+="SFood\n$-0.75\nSUtilities\n$-0.25\n";
        text+="^\n";
    }return text;
}
void report(Session& s,int index){
    auto catalog=s.store.report_catalog();reports::Definition definition{"monthly","Synthetic monthly "+std::to_string(index),"grouped_bar",{1,2},{""},{{"this_year",{},{}},{"last_year",{},{}}}};
    catalog.definitions={definition};s.action("report-save-"+std::to_string(index),[&]{s.store.save_reports(catalog);});
}
void closed_final(const fs::path& path){
    sqlite3* db=nullptr;require(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK,"Cannot inspect closed workload");
    try{require(sqlite3_key(db,password,static_cast<int>(std::char_traits<char>::length(password)))==SQLITE_OK,"Cannot key closed workload");
        physical(path,"final-closed",db);
#ifdef SPENTIT_ACTION_CAPTURE
        require(number(db,"SELECT count(*) FROM probe_inverse")==0&&number(db,"SELECT count(*) FROM probe_payload_pins")==0,"Final journal/pins did not expire");
#endif
        sqlite3_close(db);
    }catch(...){sqlite3_close(db);throw;}
}
}
int main(int argc,char** argv){try{
    require(argc==4,"Usage: workload fresh-directory import|edits|undo|linked|mixed rows");const fs::path root=argv[1];mode=argv[2];const int rows=std::stoi(argv[3]);
    require(rows>0&&rows<=100000,"Invalid row count");require(mode=="import"||mode=="edits"||mode=="undo"||mode=="linked"||mode=="mixed","Invalid workload");fs::create_directories(root);const auto path=root/(mode+".spentit");require(!fs::exists(path),"Refusing to overwrite workload");
    measure_vfs::install();require(sqlite3_auto_extension(reinterpret_cast<void(*)()>(extension))==SQLITE_OK,"Cannot observe workload connection");
    if(mode=="linked"){
        {Session s(path,true);s.action("account-create",[&]{s.store.create_account(2,"Savings",cad,"2026-01-01");});s.action("import-first",[&]{s.store.ingest(first_batch,source(rows)+"D01/02/2026\nT-10.00\nPPayment\nL[Savings]\n^\n",qif::DateOrder::dmy);});s.action("accept",[&]{s.store.accept_all("accept",true);});s.close("initial-close");}
        {Session s(path,false);s.store.select_account(2);s.action("import-second",[&]{s.store.ingest(second_batch,source(rows,rows)+"D01/02/2026\nT10.00\nPPayment\n^\n",qif::DateOrder::dmy);});const auto sheet=s.store.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& item){return item.kind==app::ReviewKind::link&&item.rows.size()>1;});require(q!=sheet.questions.end(),"Missing expected transfer link");app::ReviewAnswer answer;answer.generation=sheet.generation;answer.question=q->key;answer.action=app::ReviewAction::link;answer.selected={{first_batch,static_cast<std::size_t>(rows)},{second_batch,static_cast<std::size_t>(rows)}};s.action("review-link",[&]{s.store.answer_review("match",answer);});require(s.store.balance()==Money(1000-100LL*rows,cad),"Linked balance mismatch");s.close("linked-close");}
        {Session s(path,false);s.store.select_account(2);const auto preview=s.store.review_import_undo(second_batch);s.action("remove-second",[&]{s.store.undo_import("undo-second",second_batch,preview.token);});require(s.store.balance()==Money(1000,cad),"Restored transfer balance mismatch");s.store.select_account(1);require(s.store.balance()==Money(-1000-100LL*rows,cad),"First transfer balance mismatch");s.close("remove-close");}
        {Session s(path,false);s.store.verify();require(s.store.balance()==Money(-1000-100LL*rows,cad),"Reopened first transfer balance mismatch");s.store.select_account(2);require(s.store.balance()==Money(1000,cad),"Reopened second transfer balance mismatch");s.close("reopened-close");}
    }else if(mode=="mixed"){
        const int additional=std::max(1,rows/4);
        {Session s(path,true);s.action("account-create",[&]{s.store.create_account(2,"Savings",cad,"2026-01-01");});s.action("import-first",[&]{s.store.ingest(first_batch,rich_source(rows,0),qif::DateOrder::dmy);});s.store.select_account(2);s.action("import-second",[&]{s.store.ingest(second_batch,rich_source(additional,rows),qif::DateOrder::dmy);});s.action("accept",[&]{s.store.accept_all("accept",true);});s.store.select_account(1);report(s,0);s.close("initial-close");}
        for(int generation=0;generation<3;++generation){Session s(path,false);
            if(generation>0){const auto old="extra-"+std::to_string(generation-1);const auto preview=s.store.review_import_undo(old);s.action("remove-extra-"+std::to_string(generation-1),[&]{s.store.undo_import("remove-"+std::to_string(generation-1),old,preview.token);});}
            for(int i=0;i<8;++i)edit(s,generation,i,static_cast<std::size_t>((generation*8+i)%rows));report(s,generation+1);
            s.action("account-rename-"+std::to_string(generation),[&]{s.store.rename_account("name-"+std::to_string(generation),"Synthetic checking "+std::to_string(generation));});
            const std::string batch="extra-"+std::to_string(generation);s.action("import-extra-"+std::to_string(generation),[&]{s.store.ingest(batch,rich_source(additional,rows+additional*(generation+1)),qif::DateOrder::dmy);});
            require(s.store.balance()==Money(-100LL*(rows+additional),cad),"Mixed first balance mismatch");s.store.select_account(2);require(s.store.balance()==Money(-100LL*additional,cad),"Mixed second balance mismatch");s.store.select_account(1);s.close("mixed-close-"+std::to_string(generation+1));}
        {Session s(path,false);const auto preview=s.store.review_import_undo("extra-2");s.action("remove-extra-2",[&]{s.store.undo_import("remove-2","extra-2",preview.token);});require(s.store.balance()==Money(-100LL*rows,cad),"Mixed reopen balance mismatch");s.close("reopened-close");}
    }else{
        {Session s(path,true);s.action("opening",[&]{s.store.opening(0,"2026-01-01",Money(0,cad));});s.action("import-first",[&]{s.store.ingest(first_batch,source(rows),qif::DateOrder::dmy);});if(mode=="edits")s.action("accept",[&]{s.store.accept_all("accept");});s.close("initial-close");}
        if(mode=="edits")for(int generation=0;generation<4;++generation){Session s(path,false);for(int i=0;i<8;++i)edit(s,generation,i);require(s.store.balance()==Money(-100LL*rows,cad),"Edited balance mismatch");s.close("edit-close-"+std::to_string(generation+1));}
        if(mode=="undo"){
            {Session s(path,false);s.action("import-second",[&]{s.store.ingest(second_batch,source(rows,rows),qif::DateOrder::dmy);});require(s.store.balance()==Money(-200LL*rows,cad),"Second import balance mismatch");s.close("before-remove-close");}
            {Session s(path,false);const auto preview=s.store.review_import_undo(second_batch);s.action("remove-second",[&]{s.store.undo_import("undo-second",second_batch,preview.token);});require(s.store.balance()==Money(-100LL*rows,cad),"Removal balance mismatch");s.close("remove-close");}
        }
        {Session s(path,false);require(s.store.balance()==Money(-100LL*rows,cad),"Reopened balance mismatch");s.close("reopened-close");}
    }
    closed_final(path);
#ifdef SPENTIT_ACTION_CAPTURE
    audit_payloads(path);
#endif
    std::cout<<"Workload passed; forward/close semantics only, not full unified Undo delivery.\n";return 0;
}catch(const storage::ImportError& error){std::cerr<<"Storage code="<<error.code()<<" "<<error.what()<<'\n';return 1;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
