#include "action_journal_runtime.hpp"
using namespace action_probe;
using namespace spentit;
int main(int argc,char** argv){try{
    self_check();
    if(argc==2&&std::string_view(argv[1])=="--self-check")return 0;
    if(argc==3&&std::string_view(argv[1])=="--audit-payloads"){audit_payloads(argv[2]);return 0;}
#ifndef SPENTIT_ACTION_PAYLOAD_PROBE
    if(argc==3&&std::string_view(argv[1])=="--assert-production-refusal"){production_refusal(argv[2]);return 0;}
#endif
    require(argc==4,"Usage: action_journal_probe fresh-directory rename|accept|remove|remove-pinned rows");
    const std::filesystem::path root=argv[1];const std::string mode=argv[2];const int rows=std::stoi(argv[3]);
    require(mode=="rename"||mode=="accept"||mode=="remove"||mode=="remove-pinned","Unknown probe mode");require(rows>0&&rows<=100000,"Invalid probe row count");
    std::filesystem::create_directories(root);const auto path=root/(mode+".spentit");require(!std::filesystem::exists(path),"Probe refuses to overwrite an existing file");
    require(sqlite3_auto_extension(reinterpret_cast<void(*)()>(extension))==SQLITE_OK,"Cannot install test connection observer");
    const Currency cad("CAD");
    {auto store=ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");store.ingest("first",source(rows),qif::DateOrder::dmy);if(mode=="remove-pinned")store.ingest("second",source(rows,rows),qif::DateOrder::dmy);store.verify();store.certify({});}
    auto store=ImportStore::open(path,password);auto* db=connection;
    require(std::filesystem::equivalent(sqlite3_db_filename(db,"main"),path),"Wrong probe connection");
    const bool pinned=mode=="remove-pinned";
#ifdef SPENTIT_ACTION_PAYLOAD_PROBE
    const std::string source_members="sources_membership",source_bodies="probe_payload_sources",source_pins="probe_payload_pins WHERE name='sources'";
#else
    const std::string source_members="source_memberships",source_bodies="probe_source_payload",source_pins="probe_source_pins";
    if(pinned)split_source_payload(db);
#endif
    const auto manifest=tables(db);install(db,manifest);
#ifdef SPENTIT_ACTION_PAYLOAD_PROBE
    install_payload_pins(db);
#else
    if(pinned)install_source_pin(db);
#endif
    const auto before=canonical(db,manifest);measure(db,path,"installed",manifest);
    sql(db,"UPDATE probe_context SET action=1");
    if(mode=="rename")store.rename_account("rename","Renamed synthetic");
    else if(mode=="accept")store.accept_all("accept");
    else {const auto preview=store.review_import_undo("first");store.undo_import("remove","first",preview.token);}
    sql(db,"UPDATE probe_context SET action=0");
    coalesce(db,manifest);
    require(store.balance()==Money(pinned?-100LL*rows:(mode=="remove"?0:-100LL*rows),cad),"Probe balance mismatch");store.verify();
    measure(db,path,"captured",manifest);
    if(pinned){
        require(number(db,"SELECT count(*) FROM sources WHERE id='first'")==0&&number(db,"SELECT count(*) FROM "+source_members+" WHERE id='first'")==0,"Pinned source remained live");
        require(number(db,"SELECT count(*) FROM "+source_bodies)==2&&number(db,"SELECT count(*) FROM "+source_pins)==1,"Missing payload or pin");
        Query body(db,"SELECT bytes FROM "+source_bodies+" WHERE id='first'");require(body.next()&&body.text(0)==source(rows),"Pinned bytes changed");
        require(store.current_ledger().size()==static_cast<std::size_t>(rows)&&store.batches().size()==1,"Pinned source visible in live repository queries");
        require((sqlite3_exec(db,("DELETE FROM "+source_bodies+" WHERE id='first'").c_str(),nullptr,nullptr,nullptr)&255)==SQLITE_CONSTRAINT,"Inactive pinned body could be deleted");
        std::cout<<"Pinned source inactive; one original body retained; membership-only inverse.\n";
    }
    restore(db,manifest);
#ifdef SPENTIT_ACTION_PAYLOAD_PROBE
    sql(db,"DELETE FROM probe_payload_pins WHERE action=1");
    collect_payloads(db);
#endif
    require(canonical(db,manifest)==before,"Canonical restoration mismatch");
    require(store.balance()==Money((pinned?-200LL:-100LL)*rows,cad),"Restored balance mismatch");
    if(mode=="remove"||pinned)store.verify();
    else {
        // The existing verifier insists all registry IDs have active operation
        // rows; the approved monotonic registry deliberately violates that old
        // rule after reversal. Canonical equality/FKs/money were checked above.
        bool legacy_rejected=false;try{store.verify();}catch(const storage::ImportError& error){require(error.code()==SQLITE_CORRUPT,"Unexpected verifier failure");legacy_rejected=true;}
        require(legacy_rejected,"Expected legacy registry verification refusal");
        std::cout<<"Full legacy verifier failed with SQLITE_CORRUPT after reversal; this is not a passing full verification. Registry-policy conversion remains required.\n";
    }
    if(pinned){
        require(number(db,"SELECT count(*) FROM sources")==2&&number(db,"SELECT count(*) FROM "+source_members)==2,"Source membership not restored");
        #ifdef SPENTIT_ACTION_PAYLOAD_PROBE
        sql(db,"DELETE FROM probe_payload_pins WHERE action=1");
        collect_payloads(db);
#else
        sql(db,"DELETE FROM probe_source_pins WHERE action=1");
#endif
        require(number(db,"SELECT count(*) FROM "+source_pins)==0&&number(db,"SELECT count(*) FROM "+source_bodies)==2,"Incorrect pin release");
        require(number(db,"SELECT rowid FROM sources WHERE id='first'")==1&&number(db,"SELECT rowid FROM sources WHERE id='second'")==2,"Restored source order changed");
        require((sqlite3_exec(db,("DELETE FROM "+source_bodies+" WHERE id='first'").c_str(),nullptr,nullptr,nullptr)&255)==SQLITE_CONSTRAINT,"Live body could be deleted");
    }
    measure(db,path,"restored",manifest);
    // A database carrying probe-only schema is intentionally not certified as a
    // usable application format. No production UI or clean marker is published.
    std::cout<<"Representative inverse round trip passed; payload split, coordinator and storage acceptance remain unverified.\n";
    return 0;
}catch(const storage::ImportError& error){std::cerr<<"Storage code="<<error.code()<<" "<<error.what()<<'\n';return 1;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
