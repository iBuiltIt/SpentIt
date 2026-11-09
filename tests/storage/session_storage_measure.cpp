// Identical synthetic workload driver for the pre-session and current stores.
// No VACUUM, padding, checkpoint tricks, or private input data.
#include <spentit/import_store.hpp>
#include <sqlite3.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace spentit;
using storage::ImportStore;
namespace fs=std::filesystem;
constexpr auto password="synthetic-storage-comparison";
const Currency cad("CAD");
void require(bool value){if(!value)throw std::runtime_error("Storage workload assertion failed");}
std::string source(int count,int offset=0){std::string text;for(int i=0;i<count;++i)text+="D01/02/2026\nT-1.00\nPSynthetic "+std::to_string(i+offset)+"\nN"+std::to_string(i+offset)+"\n^\n";return text;}
void measure(const fs::path& path,std::string_view phase){
 sqlite3* db=nullptr;require(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
 require(sqlite3_key(db,password,static_cast<int>(std::char_traits<char>::length(password)))==SQLITE_OK);
 auto scalar=[&](const char* sql){sqlite3_stmt* q=nullptr;require(sqlite3_prepare_v2(db,sql,-1,&q,nullptr)==SQLITE_OK);require(sqlite3_step(q)==SQLITE_ROW);auto result=sqlite3_column_int64(q,0);sqlite3_finalize(q);return result;};
 const auto pages=scalar("PRAGMA page_count"),free=scalar("PRAGMA freelist_count"),size=scalar("PRAGMA page_size");
 std::cout<<phase<<",file="<<fs::file_size(path)<<",pages="<<pages<<",free="<<free<<",live_page_bytes="<<(pages-free)*size;
 for(const char* table:{"versions","entry_revisions","ledger_operations","snapshot_facts","allocation_origins","command_completions","action_headers","action_receipts","review_members","review_link_history","review_undo_members","review_repairs"}){
  const auto exists="SELECT count(*) FROM sqlite_master WHERE type='table' AND name='"+std::string(table)+"'";
  if(scalar(exists.c_str()))std::cout<<","<<table<<"="<<scalar(("SELECT count(*) FROM "+std::string(table)).c_str());
 }
 if(scalar("SELECT sqlite_compileoption_used('ENABLE_DBSTAT_VTAB')"))std::cout<<",payload_bytes="<<scalar("SELECT coalesce(sum(payload),0) FROM dbstat");
 std::cout<<'\n';sqlite3_close(db);
}
int main(int argc,char** argv){try{
 require(argc==4);fs::path root=argv[1];fs::create_directories(root);const std::string mode=argv[2];const int rows=std::stoi(argv[3]);require(rows>0 && rows<=100000);require(mode=="import" || mode=="edits" || mode=="undo" || mode=="linked" || mode=="accumulated");
 const auto path=root/(mode+".spentit");require(!fs::exists(path));
 if(mode=="accumulated"){
  {auto s=ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");s.ingest("first",source(rows),qif::DateOrder::dmy);s.accept_all("accept");
   s.apply_category_change("food",{app::CategoryAction::create,"food","Food",{},CategoryKind::leaf},s.category_catalog().revision);
   app::RuleDefinitionImage rule;rule.identity="food-rule";rule.label="Food";rule.expression="Synthetic";rule.actions.category="food";app::OperationContext context;
   s.save_rules("rules",{{rule}},s.rule_image().revision,context);
   s.apply_account_rules("categorize",{s.account().identity,s.ledger_edit_generation(),s.rule_image().revision,s.category_catalog().revision,false,false,{}},context);
   require(s.balance()==Money(-100LL*rows,cad));s.verify();measure(path,"categorized-open");s.certify({});}
  measure(path,"categorized-close");
  for(int session=0;session<4;++session){std::cerr<<"session "<<session<<" open\n";auto s=ImportStore::open(path,password);const auto batch="later-"+std::to_string(session);std::cerr<<"ingest\n";s.ingest(batch,source(25,rows+session*25),qif::DateOrder::dmy);std::cerr<<"accept\n";s.accept_all("later-accept-session-"+std::to_string(session),true);
   std::cerr<<"edit\n";const imports::Occurrence id{"first",0};const auto entry=s.committed_entry(id);auto draft=entry.current.draft;draft.payee="Later edit "+std::to_string(session);
   const auto request=app::ChangeRequest{app::ChangeKind::correct,id,draft,{}};const auto preview=s.preview_committed_change(request);s.apply_committed_change_with_catalog("edit-"+std::to_string(session),request,preview.expected,preview.preview_sequence,preview.catalog_revision);
   require(s.balance()==Money(-100LL*(rows+(session+1)*25),cad));std::cerr<<"verify\n";s.verify();std::cerr<<"close\n";s.certify({});measure(path,"later-close-"+std::to_string(session+1));}
  {auto s=ImportStore::open(path,password);s.verify();require(s.balance()==Money(-100LL*(rows+100),cad));s.certify({});}measure(path,"reopened");return 0;
 }
 if(mode=="linked"){
  {auto s=ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");s.create_account(2,"Savings",cad,"2026-01-01");s.ingest("first",source(rows)+"D01/02/2026\nT-10.00\nPPayment\nL[Savings]\n^\n",qif::DateOrder::dmy);s.accept_all("accept",true);s.verify();s.certify({});}
  measure(path,"initial");
  {auto s=ImportStore::open(path,password);s.select_account(2);s.ingest("second",source(rows,rows)+"D01/02/2026\nT10.00\nPPayment\n^\n",qif::DateOrder::dmy);const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==app::ReviewKind::link && q.rows.size()>1;});require(q!=sheet.questions.end());app::ReviewAnswer a;a.generation=sheet.generation;a.question=q->key;a.action=app::ReviewAction::link;a.selected={{"first",static_cast<std::size_t>(rows)},{"second",static_cast<std::size_t>(rows)}};s.answer_review("match",a);require(s.balance()==Money(1000-100LL*rows,cad));s.verify();s.certify({});}
  measure(path,"before-linked-import-undo");
  {auto s=ImportStore::open(path,password);s.select_account(2);const auto p=s.review_import_undo("second");s.undo_import("undo-second","second",p.token);require(s.balance()==Money(1000,cad));s.select_account(1);require(s.balance()==Money(-1000-100LL*rows,cad));s.verify();s.certify({});}
  measure(path,"after-linked-import-undo");
  {auto s=ImportStore::open(path,password);s.verify();require(s.balance()==Money(-1000-100LL*rows,cad));s.select_account(2);require(s.balance()==Money(1000,cad));s.certify({});}
  measure(path,"reopened");return 0;
 }
 {auto s=ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.ingest("first",source(rows),qif::DateOrder::dmy);if(mode=="edits")s.accept_all("accept");s.verify();measure(path,"before-close");s.certify({});}
 measure(path,"initial");
 if(mode=="edits")for(int generation=0;generation<4;++generation){
  {auto s=ImportStore::open(path,password);for(int edit=0;edit<8;++edit){const imports::Occurrence id{"first",0};auto d=s.committed_entry(id).current.draft;d.payee="Edited "+std::to_string(generation)+" "+std::to_string(edit);app::ChangeRequest r{app::ChangeKind::correct,id,d,{}};const auto p=s.preview_committed_change(r);s.apply_committed_change("edit-"+std::to_string(generation)+"-"+std::to_string(edit),r,p.expected,p.preview_sequence);}require(s.balance()==Money(-100LL*rows,cad));s.verify();s.certify({});}
  measure(path,"edit-close-"+std::to_string(generation+1));
 }
 if(mode=="undo"){
  {auto s=ImportStore::open(path,password);s.ingest("second",source(rows,rows),qif::DateOrder::dmy);require(s.balance()==Money(-200LL*rows,cad));s.certify({});}
  measure(path,"before-import-undo");
  {auto s=ImportStore::open(path,password);const auto p=s.review_import_undo("second");s.undo_import("undo-second","second",p.token);require(s.balance()==Money(-100LL*rows,cad));s.verify();s.certify({});}
  measure(path,"after-import-undo");
 }
 {auto s=ImportStore::open(path,password);require(s.balance()==Money(-100LL*rows,cad));s.verify();s.certify({});}
 measure(path,"reopened");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
