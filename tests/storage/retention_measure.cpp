// Synthetic retention investigation; uses public commands only. No storage changes.
#include <spentit/import_store.hpp>
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <fstream>
#include <algorithm>
using namespace spentit;
using storage::ImportStore;
namespace fs=std::filesystem;
constexpr auto password="synthetic-storage-comparison";
const Currency cad("CAD");
void require(bool b){if(!b)throw std::runtime_error("Workload assertion failed");}
std::int64_t rev(ImportStore& s,std::string_view id){for(const auto& b:s.batches())if(b.id==id)return b.revision;throw std::runtime_error("Missing batch");}
void category(ImportStore& s,std::string id,const app::CategoryRequest& r){auto p=s.preview_category_change(r);s.apply_category_change(id,p.request,p.catalog_revision);}
std::string source(int rows){std::string b;for(int i=0;i<rows;++i)b+="D02/01/2026\nT-1.00\nPSynthetic "+std::to_string(i)+"\n^\n";return b;}
using Clock=std::chrono::steady_clock;
template<class F>double timed(F f){const auto start=Clock::now();f();return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void measure(const fs::path& path,const std::string& phase){
 sqlite3* db=nullptr;require(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);require(sqlite3_key(db,password,28)==SQLITE_OK);
 auto scalar=[&](std::string sql){sqlite3_stmt* q=nullptr;if(sqlite3_prepare_v2(db,sql.c_str(),-1,&q,nullptr)!=SQLITE_OK)throw std::runtime_error(sql+": "+sqlite3_errmsg(db));require(sqlite3_step(q)==SQLITE_ROW);auto v=sqlite3_column_int64(q,0);sqlite3_finalize(q);return v;};
 std::cout<<"SIZE,"<<phase<<",file,"<<fs::file_size(path)<<",free_bytes,"<<scalar("PRAGMA freelist_count")*scalar("PRAGMA page_size");
 for(const auto* t:{"rule_operations","rule_operation_requests","rule_snapshots","rule_definitions","category_operations","category_versions","review_answers","review_undos","review_link_history","review_members","transfer_events","transfer_versions","link_exclusion_events","account_operations","account_amount_revisions","import_balance_events","versions","entry_revisions","ledger_operations","command_completions"}){
  std::cout<<","<<t<<","<<scalar("SELECT count(*) FROM "+std::string(t));
 }
 // Logical values, not B-tree payload: report separately from physical pages.
 long long total=0;sqlite3_stmt* tables=nullptr;require(sqlite3_prepare_v2(db,"SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'",-1,&tables,nullptr)==SQLITE_OK);
 while(sqlite3_step(tables)==SQLITE_ROW){std::string t=reinterpret_cast<const char*>(sqlite3_column_text(tables,0));sqlite3_stmt* cols=nullptr;require(sqlite3_prepare_v2(db,("PRAGMA table_info(\""+t+"\")").c_str(),-1,&cols,nullptr)==SQLITE_OK);std::string expr;
  while(sqlite3_step(cols)==SQLITE_ROW){if(!expr.empty())expr+="+";expr+="coalesce(length(cast(\""+std::string(reinterpret_cast<const char*>(sqlite3_column_text(cols,1)))+"\" as blob)),0)";}sqlite3_finalize(cols);
  total+=scalar("SELECT coalesce(sum("+expr+"),0) FROM \""+t+"\"");
 }sqlite3_finalize(tables);sqlite3_close(db);std::cout<<",logical_value_bytes,"<<total<<std::endl;
}
void inspect(ImportStore& s,const std::string& phase){try{auto ms=timed([&]{s.verify();});std::cout<<"VERIFY,"<<phase<<",pass,"<<ms<<std::endl;}catch(const std::exception& e){std::cout<<"VERIFY,"<<phase<<",FAIL,"<<e.what()<<std::endl;}}
int main(int argc,char** argv){try{
 require(argc==6);const fs::path root=argv[1];const std::string mode=argv[2];const int rows=std::stoi(argv[3]),rules=std::stoi(argv[4]),cycles=std::stoi(argv[5]);const bool balance_mode=mode.starts_with("balance"),fixed_mode=mode.starts_with("fixed");fs::create_directories(root);const auto path=root/(mode+"-"+std::to_string(rules)+".spentit");require(!fs::exists(path));
 {
 auto s=ImportStore::create(path,password,"Synthetic",cad,"2026-01-01");
 if(balance_mode || fixed_mode){
  app::OperationContext c;s.prepare_import_group("group","!Account\nNBalance\nTBank\nB100\n^\n!Type:Bank\n"+source(rows),qif::DateOrder::dmy,{{0,2,app::AccountCreation{2,"Balance",cad,"2026-01-01"},fixed_mode?std::optional<bool>(false):std::nullopt}},c);app::OperationContext r;s.resume_import_group("group",r);s.select_account(2);
  if(balance_mode)s.review_import_balance("group",0,rev(s,"group-account-0"),app::ReportedBalanceChoice::opening,"2026-01-01");
 }else{
  s.ingest("base",source(rows),qif::DateOrder::dmy);s.accept_all("accept",true);
  category(s,"category-create",{app::CategoryAction::create,"cat","Synthetic category",{},CategoryKind::leaf});
  if(mode=="rules"){
   std::vector<app::RuleDefinitionImage> images;for(int i=0;i<rules;++i){app::RuleDefinitionImage r;r.identity="rule-"+std::to_string(i);r.label="Synthetic rule "+std::to_string(i);r.expression="^Synthetic merchant "+std::to_string(i)+"$";r.actions.category="cat";images.push_back(r);}app::OperationContext c;s.save_rules("rules-create",{images},0,c);
  }
  if(mode=="links" || mode=="currency-linked"){
   s.create_account(2,"Savings",cad,"2026-01-01");s.ingest("out","D03/01/2026\nT-10\nPPayment\nL[Savings]\n^\n",qif::DateOrder::dmy);s.accept_all("accept-transfer",true);
   if(mode=="links"){s.select_account(2);s.ingest("in","D03/01/2026\nT10\nPPayment\n^\n",qif::DateOrder::dmy);}
  }
 }
 inspect(s,"setup-before-close");s.certify({});inspect(s,"setup-after-close");
 }measure(path,"0");
 for(int i=1;i<=cycles;++i){
  auto s=ImportStore::open(path,password);const auto id="op-"+std::to_string(i);
  auto ms=timed([&]{
   if(mode=="rules"){auto image=s.rule_image();image.ordered.front().label="Changed rule "+std::to_string(i);app::OperationContext c;s.save_rules(id,{image.ordered},image.revision,c);}
   else if(mode=="categories"){category(s,id,{app::CategoryAction::rename,"cat","Category name "+std::to_string(i),{},CategoryKind::leaf});}
   else if(mode=="income"){app::CategoryRequest r{app::CategoryAction::set_income,"cat",{},{},CategoryKind::leaf};r.income=i%2;category(s,id,r);}
   else if(mode=="exclusions"){app::LinkExclusion r{"exclusion","payee","Synthetic",0,false,i%2==0};auto p=s.preview_link_exclusion(r);s.save_link_exclusion(id,r,p.generation);}
   else if(mode=="opening")s.correct_opening(id,"2026-01-01",Money(i,cad));
   else if(mode=="currency" || mode=="currency-linked")s.reassign_currency(id,Currency(i%2?"GBP":"CAD"));
   else if(mode=="links"){
    s.select_account(2);auto sh=s.review_sheet();const auto q=std::find_if(sh.questions.begin(),sh.questions.end(),[](const auto& q){return q.kind==app::ReviewKind::link;});require(q!=sh.questions.end());app::ReviewAnswer a;a.generation=sh.generation;a.question=q->key;a.action=app::ReviewAction::link;a.selected={{"out",0},{"in",0}};s.answer_review(id,a);s.undo_review("undo-"+id);require(s.balance()==Money(2000,cad));
   }else if(balance_mode || fixed_mode){
    s.select_account(2);const imports::Occurrence occurrence{"group-account-0",0};auto d=*s.row(occurrence).draft;d.payee="Changed draft "+std::to_string(i);if(mode.ends_with("-amount")){auto allocations=d.transaction.allocations();allocations.front().amount=Money(-100-i,cad);d.transaction=d.transaction.corrected(Money(-100-i,cad),allocations);}s.save(occurrence,rev(s,occurrence.batch),d);
    if(balance_mode)s.review_import_balance("group",0,rev(s,occurrence.batch),i%2?app::ReportedBalanceChoice::closing:app::ReportedBalanceChoice::opening,"2026-01-01");
    if(fixed_mode && i==1){try{s.review_import_balance("group",0,rev(s,occurrence.batch),app::ReportedBalanceChoice::closing,"2026-01-01");throw std::runtime_error("Unexpected allowed fixed balance toggle");}catch(const storage::ImportError& e){std::cout<<"FIXED-REFUSAL,"<<e.what()<<std::endl;}}
   }else throw std::runtime_error("Unknown mode");
  });
  const bool sample=i<=3||i==8||i==32||i==cycles;
  if(sample){std::cout<<"MUTATION,"<<i<<","<<ms<<std::endl;inspect(s,"before-close-"+std::to_string(i));}
  auto close=timed([&]{s.certify({});});
  if(sample){std::cout<<"CLOSE,"<<i<<","<<close<<std::endl;inspect(s,"after-close-"+std::to_string(i));measure(path,std::to_string(i));}
 }
 {auto s=ImportStore::open(path,password);inspect(s,"reopen");auto close=timed([&]{s.certify({});});std::cout<<"UNCHANGED-CLOSE,"<<close<<std::endl;}
 measure(path,"unchanged");
 {auto s=ImportStore::open(path,password);try{s.backup_to(root/(mode+"-"+std::to_string(rules)+".backup"),password);std::cout<<"BACKUP,pass"<<std::endl;}catch(const std::exception& e){std::cout<<"BACKUP,FAIL,"<<e.what()<<std::endl;}s.certify({});}
 if(mode=="links") {
  {auto s=ImportStore::open(path,password);s.select_account(2);auto sh=s.review_sheet();const auto q=std::find_if(sh.questions.begin(),sh.questions.end(),[](const auto& q){return q.kind==app::ReviewKind::link;});require(q!=sh.questions.end());app::ReviewAnswer a;a.generation=sh.generation;a.question=q->key;a.action=app::ReviewAction::link;a.selected={{"out",0},{"in",0}};s.answer_review("final-active-match",a);require(s.balance()==Money(1000,cad));inspect(s,"final-active-match");s.certify({});inspect(s,"final-active-match-closed");}
  auto s=ImportStore::open(path,password);s.select_account(2);const auto p=s.review_import_undo("in");s.undo_import("final-import-undo","in",p.token);require(s.balance()==Money(1000,cad));inspect(s,"import-undo");s.certify({});inspect(s,"import-undo-closed");}
 if(mode.ends_with("-amount")){
  {auto s=ImportStore::open(path,password);s.select_account(2);require(s.balance()==Money(-100LL*rows-cycles,cad));std::cout<<"BALANCE,current-exact,"<<s.balance().units()<<std::endl;}
  try{auto s=ImportStore::open(path,password);std::cout<<"RECOVERY,pass"<<std::endl;s.certify({});}catch(const std::exception& e){std::cout<<"RECOVERY,FAIL,"<<e.what()<<std::endl;}
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<"FATAL,"<<e.what()<<std::endl;return 1;}}
