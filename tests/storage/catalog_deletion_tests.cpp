#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include "session_test_helpers.hpp"
#include <filesystem>
#include <iostream>
#include <source_location>
#include <sys/wait.h>
using namespace spentit;
using namespace spentit::app;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-deletion-password";
const Currency cad("CAD");
const std::string source="!Type:Bank\nD01/01/2026\nT-10.00\nPSynthetic\n^\n";
struct Directory {fs::path path;Directory(){auto p=(fs::temp_directory_path()/"spentit-delete-XXXXXX").string();auto* value=mkdtemp(p.data());if(!value)throw std::runtime_error("Temporary directory failed");path=value;}~Directory(){std::error_code e;fs::remove_all(path,e);}};
void check(bool ok,const std::source_location& loc=std::source_location::current()){if(!ok)throw std::runtime_error("Deletion assertion at "+std::to_string(loc.line()));}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected);}
ImportStore create(const fs::path& path){auto s=ImportStore::create(path,password,"Main",cad,"2026-01-01");s.create_account(2,"Spare",cad,"2026-01-01");return s;}
void category(ImportStore& s,std::string id){CategoryRequest r{CategoryAction::create,id,id,{}};auto p=s.preview_category_change(r);s.apply_category_change("create-"+id,r,p.catalog_revision);}
void assign(ImportStore& s,std::string operation,std::string id){
 auto d=s.transaction_accepted({"import",0})?s.committed_entry({"import",0}).current.draft:*s.row({"import",0}).draft;
 auto a=d.transaction.allocations();a[0].category=id;a[0].category_binding=id.empty()?std::nullopt:std::optional(CategoryBinding{id,s.category_catalog().revision});a[0].category_owner=FieldOwnership::manual;d.transaction=d.transaction.corrected(d.transaction.effective_total(),a);
 s.edit_transaction(operation,{"import",0},s.batches().at(0).revision,s.transaction_accepted({"import",0})?std::optional(s.committed_entry({"import",0}).token):std::nullopt,d);
}
void remove_rows(ImportStore& s){OperationContext ctx;LedgerSelectionEdit r{LedgerSelectionAction::remove,s.account().identity,s.ledger_edit_generation(),{{"import",0}},{},false,{},false};s.edit_transactions("remove",r,ctx);check(s.current_ledger().empty());}
CategoryPreview removal(ImportStore& s){return s.preview_category_change({CategoryAction::erase,"Used",{},{}});}
void delete_category(ImportStore& s,bool consent=true){auto p=removal(s);s.apply_category_change("delete",p.request,p.catalog_revision,consent?std::optional(s.action_token()):std::nullopt);}
void undo(ImportStore& s){s.undo_action("undo",s.preview_undo(s.action_head()->id,s.action_token()));}
void redo(ImportStore& s){UndoPreview p;p.token=s.action_token();p.action=s.redo_history().at(0);s.redo_action("redo",p);}
void setup(const fs::path& path,bool account){auto s=create(path);category(s,"Used");s.ingest("import",source,qif::DateOrder::dmy);s.commit("import",0,"accept");if(account)remove_rows(s);else{assign(s,"assign","Used");assign(s,"clear","");}s.verify();}
void transfer_cleanup(){
 // A signed split correction keeps the sibling allocation and exact total.
 {Directory dir;auto s=create(dir.path/"doc");s.ingest("split","!Type:Bank\nD01/01/2026\nT-9.00\nPSplit payment\nS[Unknown destination]\n$-10.00\nSRefund\n$1.00\n^\n",qif::DateOrder::dmy);
  const auto original=s.row({"split",0}).draft->transaction.allocations();OperationContext context;
  s.assign_categories("clear-split",{1,s.ledger_edit_generation(),"",{{{"split",0},"source-0"}}},context);
  const auto current=s.row({"split",0}).draft->transaction.allocations();check(current.size()==2 && current[0].category.empty() && current[0].amount.units()==-1000 && current[0].identity==original[0].identity && current[0].source_ordinal==original[0].source_ordinal);check(current[1].category==original[1].category && current[1].amount==original[1].amount && s.balance().units()==-900);s.verify();undo(s);s.verify();redo(s);s.verify();
 }
 // An imported destination remains immutable in source, but its current category
 // can be corrected before/after linking, including after Unmerge.
 for(int mode=0;mode<4;++mode){
  std::cout<<"Transfer cleanup mode "<<mode<<std::endl;
  Directory dir;auto s=create(dir.path/"doc");
  s.ingest("out","!Type:Bank\nD01/01/2026\nT-10.00\nPPayment\nL[Spare]\n^\n",qif::DateOrder::dmy);
  s.select_account(2);s.ingest("in","!Type:Bank\nD01/01/2026\nT10.00\nPReceipt\nL[Main]\n^\n",qif::DateOrder::dmy);
  if(mode){
   const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::link && q.rows.size()>1;});check(q!=sheet.questions.end());
   ReviewAnswer a;a.generation=sheet.generation;a.question=q->key;a.action=ReviewAction::link;a.selected={{"out",0},{"in",0}};s.answer_review("link",a);
   if(mode==3)s.accept_all("accept");
   if(mode==2){check(s.preview_unmerge({{"out",0}}).available);s.unmerge("unmerge",{{"out",0}});}
  }
  if(mode==0) {
   const auto row=s.row({"in",0});auto draft=*row.draft;auto aa=draft.transaction.allocations();
   aa[0].category="";draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),aa);
   rejects([&]{imports::validate_draft(row.source,draft);});
   aa[0].category_owner=FieldOwnership::manual;draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),aa);imports::validate_draft(row.source,draft);
   aa[0].source_ordinal.reset();draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),aa);rejects([&]{imports::validate_draft(row.source,draft);});
   aa[0].source_ordinal=0;aa[0].category="[Invented]";draft.transaction=draft.transaction.corrected(draft.transaction.effective_total(),aa);rejects([&]{imports::validate_draft(row.source,draft);});
  }
  OperationContext context;
  const auto clear=[&](std::int64_t account,const char* batch,const char* op){s.select_account(account);s.assign_categories(op,{account,s.ledger_edit_generation(),"",{{{batch,0},{}}}},context);};
  clear(1,"out","clear-out");
  check(s.row({"out",0}).source.source_allocation_fields.front().category=="[Spare]");
  check(s.row({"out",0}).draft->transaction.effective_total().units()==-1000);s.verify();
  // The first correction must remain undoable and redoable, including its link.
  undo(s);check((s.transaction_accepted({"out",0})?s.committed_entry({"out",0}).current.draft:*s.row({"out",0}).draft).transaction.allocations().front().category=="[Spare]");s.verify();redo(s);s.verify();
  clear(2,"in","clear-in");check(s.balance().units()==1000);s.verify();
  LedgerSelectionEdit r{LedgerSelectionAction::remove,2,s.ledger_edit_generation(),{{"in",0}},{},false,{},false};
  s.edit_transactions("remove-in",r,context);check(s.current_ledger().empty() && s.balance().units()==0);
  check(s.preview_account_deletion());s.delete_account("delete-empty",s.action_token());s.select_account(1);check(s.balance().units()==-1000);s.verify();
  test_session::close_reopen(s,dir.path/"doc",password);check(s.accounts().size()==1 && s.current_ledger().size()==1);s.verify();
 }
}
void transfer_cleanup_faults(){
 fault_vfs::install();
 const auto setup=[](const fs::path& path){auto s=create(path);s.ingest("out","!Type:Bank\nD01/01/2026\nT-10.00\nPPayment\nL[Spare]\n^\n",qif::DateOrder::dmy);};
 const auto apply=[](ImportStore& s){OperationContext context;s.select_account(1);s.assign_categories("clear",{1,s.ledger_edit_generation(),"",{{{"out",0},{}}}},context);};
 const auto verify=[](ImportStore& s,bool saved){s.select_account(1);check(s.balance().units()==-1000);check(s.row({"out",0}).source.source_allocation_fields[0].category=="[Spare]");check(s.row({"out",0}).draft->transaction.allocations()[0].category==(saved?"":"[Spare]"));s.select_account(2);check(s.balance().units()==(saved?0:1000));s.select_account(1);s.verify();};
 int positions=0;{Directory d;setup(d.path/"doc");auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);apply(s);positions=fault_vfs::seen;fault_vfs::disarm();}check(positions>0);
 for(int position=1;position<=positions;++position){Directory d;setup(d.path/"doc");{auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const std::exception&){}check(fault_vfs::fired);fault_vfs::disarm();}
  auto s=ImportStore::open(d.path/"doc",password);const bool saved=s.review_outcome("clear").has_value();verify(s,saved);if(!saved)apply(s);verify(s,true);undo(s);verify(s,false);
 }
 for(auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;setup(d.path/"doc");auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});apply(s);_exit(0);}int status;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);auto s=ImportStore::open(d.path/"doc",password);const bool saved=s.review_outcome("clear").has_value();check(saved==(phase==ImportPhase::after_commit));verify(s,saved);if(!saved)apply(s);verify(s,true);}
 std::cout<<"Imported transfer release: "<<positions<<" fault positions\n";
}
void behavior(){
 for(bool accepted:{false,true}){
  Directory dir;auto s=create(dir.path/"doc");s.ingest("import",source,qif::DateOrder::dmy);if(accepted)s.commit("import",0,"accept");rejects([&]{s.preview_account_deletion();});remove_rows(s);
  check(s.preview_account_deletion());const auto token=s.action_token();rejects([&]{s.delete_account("delete");});check(s.action_token()==token && s.account_selected());
  s.delete_account("delete",token);check(!s.account_selected() && !s.action_head() && s.redo_history().empty());s.verify();
  s.select_account(2);test_session::close_reopen(s,dir.path/"doc",password);check(s.accounts().size()==1);rejects([&]{s.select_account(1);});s.verify();
  if(!accepted){s.select_account(2);check(!s.undoable_imports().empty());s.undo_import("undo-source","import",s.ledger_edit_generation());s.verify();check(s.accounts().size()==1);}
 }
 for(bool accepted:{false,true}){
  Directory dir;auto s=create(dir.path/"doc");category(s,"Used");s.ingest("import",source,qif::DateOrder::dmy);if(accepted)s.commit("import",0,"accept");assign(s,"assign","Used");rejects([&]{removal(s);});assign(s,"clear","");
  check(removal(s).clears_history);rejects([&]{delete_category(s,false);});const auto token=s.action_token();s.correct_opening("unrelated","2026-01-01",Money(1,cad));auto p=removal(s);rejects([&]{s.apply_category_change("stale",p.request,p.catalog_revision,token);});
  delete_category(s);check(!s.action_head());s.verify();test_session::close_reopen(s,dir.path/"doc",password);s.verify();check(s.current_ledger().size()==1);
 }
 // Past use after a clean close does not consume unrelated Undo/Redo.
 {Directory dir;auto s=create(dir.path/"doc");category(s,"Used");s.ingest("import",source,qif::DateOrder::dmy);assign(s,"assign","Used");assign(s,"clear","");test_session::close_reopen(s,dir.path/"doc",password);
  s.correct_opening("one","2026-01-01",Money(1,cad));s.correct_opening("two","2026-01-01",Money(2,cad));undo(s);check(s.redo_history().size()==1);check(!removal(s).clears_history);delete_category(s,false);check(s.action_head()->id=="one" && s.redo_history().size()==1);redo(s);check(s.account().opening.units()==2);s.verify();}
 // A Redo that would restore the assignment requires consent even without Undo.
 {Directory dir;auto s=create(dir.path/"doc");category(s,"Used");s.ingest("import",source,qif::DateOrder::dmy);test_session::close_reopen(s,dir.path/"doc",password);assign(s,"assign","Used");undo(s);check(!s.action_head() && removal(s).clears_history);delete_category(s);check(s.redo_history().empty());s.verify();}
 // Redo owns bindings even when Undo removed the underlying manual/repair row.
 for(bool repair:{false,true}){Directory dir;auto s=create(dir.path/"doc");category(s,"Used");
  Allocation a{"Used","",Money(-100,cad)};a.category_binding=CategoryBinding{"Used",s.category_catalog().revision};a.category_owner=FieldOwnership::manual;
  imports::DraftRow draft{"2026-01-01","Synthetic","",repair?Transaction::repaired_import({},a.amount,{a}):Transaction::manual(a.amount,{a})};
  if(repair){s.ingest("bad","!Type:Bank\nD01/01/2026\nTbad\nPBroken\n^\n",qif::DateOrder::dmy);s.save({"bad",0},0,draft);}else s.create_manual("manual",draft);
  undo(s);check(removal(s).clears_history);delete_category(s);check(s.redo_history().empty());s.verify();}
 // Empty-account deletion preserves unrelated Redo, including its replay.
 {Directory dir;auto s=create(dir.path/"doc");s.correct_opening("one","2026-01-01",Money(1,cad));s.correct_opening("two","2026-01-01",Money(2,cad));undo(s);s.select_account(2);check(!s.preview_account_deletion());s.delete_account("delete");check(s.redo_history().size()==1);s.select_account(1);redo(s);check(s.account().opening.units()==2);s.verify();}
 // Opening corrections returning to zero are deletable after confirmation.
 {Directory dir;auto s=create(dir.path/"doc");s.correct_opening("one","2026-01-01",Money(1,cad));s.correct_opening("zero","2026-01-02",Money(0,cad));check(s.preview_account_deletion());s.delete_account("delete",s.action_token());s.verify();}
 // Saved reports and disabled rules are current references, not history.
 {Directory dir;auto s=create(dir.path/"doc");category(s,"Used");auto reports=s.report_catalog();reports.definitions.push_back({"report","Report","none",{1},{"Used"},{{"ytd",{},{}}}});s.save_reports(reports);rejects([&]{removal(s);});rejects([&]{s.preview_account_deletion();});}
#ifdef SPENTIT_RULES_STORAGE
 {Directory dir;auto s=create(dir.path/"doc");category(s,"Used");RuleDefinitionImage rule;rule.identity="rule";rule.label="Rule";rule.enabled=false;rule.expression="Synthetic";rule.actions.category="Used";OperationContext ctx;
  s.save_rules("rules",{{rule}},s.rule_image().revision,ctx);rejects([&]{removal(s);});s.save_rules("remove-rule",{},s.rule_image().revision,ctx);check(!removal(s).clears_history);delete_category(s,false);s.verify();test_session::close_reopen(s,dir.path/"doc",password);s.verify();}
#endif
 // Removed transfer history remains verifiable after either account is retired.
 {Directory dir;auto s=create(dir.path/"doc");TransferRequest r{};r.action=TransferAction::manual;r.link="link";r.destination=2;r.expected_sequence=s.transfer_sequence();r.amount=Money(-100,cad);r.date="2026-01-01";r.payee="Synthetic transfer";s.apply_transfer("transfer",r);
  rejects([&]{s.preview_account_deletion();});undo(s);
  check(s.current_ledger().empty());s.delete_account("delete",s.action_token());s.select_account(2);check(s.current_ledger().empty());s.verify();test_session::close_reopen(s,dir.path/"doc",password);s.verify();}
 // A failed clear-history save leaves same-connection Undo/Redo usable; a lost
 // acknowledgement is settled by its receipt without a second history expiry.
 for(bool account:{false,true})for(auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){
  Directory dir;setup(dir.path/"doc",account);auto s=ImportStore::open(dir.path/"doc",password);const auto token=s.action_token();
  s.observer([&](ImportPhase at){if(at==phase)throw std::runtime_error("Synthetic lost acknowledgement");});
  rejects([&]{if(account)s.delete_account("delete",token);else delete_category(s);});s.observer({});
  const bool saved=account?s.account_command_outcome("delete").has_value():s.category_change_outcome("delete").has_value();check(saved==(phase==ImportPhase::after_commit));check(saved==!s.action_head().has_value());s.verify();
  if(account)s.delete_account("delete",s.action_token());else if(!saved)delete_category(s);s.verify();
 }

}
void faults(){fault_vfs::install();for(bool account:{false,true}){
 auto apply=[&](ImportStore& s){if(account)s.delete_account("delete",s.action_token());else delete_category(s);};
 auto saved=[&](ImportStore& s){return account?s.account_command_outcome("delete").has_value():s.category_change_outcome("delete").has_value();};
 int positions=0;{Directory d;setup(d.path/"doc",account);auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(-1);apply(s);positions=fault_vfs::seen;fault_vfs::disarm();s.verify();}check(positions>0);
 for(int position=1;position<=positions;++position){Directory d;setup(d.path/"doc",account);{auto s=ImportStore::open(d.path/"doc",password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const std::exception&){}check(fault_vfs::fired);fault_vfs::disarm();}
  auto s=ImportStore::open(d.path/"doc",password);check(saved(s)==!s.action_head().has_value());s.verify();if(!saved(s))apply(s);check(saved(s) && !s.action_head());s.verify();}
 for(auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory d;setup(d.path/"doc",account);auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(d.path/"doc",password);s.observer([&](ImportPhase at){if(at==phase)_exit(72);});apply(s);_exit(0);}int status;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);auto s=ImportStore::open(d.path/"doc",password);check(saved(s)==(phase==ImportPhase::after_commit));check(saved(s)==!s.action_head().has_value());s.verify();if(!saved(s))apply(s);s.verify();}
 std::cout<<"Deletion account="<<account<<": "<<positions<<" fault positions\n";
}}
}
int main(int argc,char** argv){try{if(argc>1 && std::string_view(argv[1])=="faults")faults();else if(argc>1 && std::string_view(argv[1])=="transfer_cleanup_faults")transfer_cleanup_faults();else if(argc>1 && std::string_view(argv[1])=="transfer_cleanup")transfer_cleanup();else behavior();std::cout<<"Catalog deletion passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
