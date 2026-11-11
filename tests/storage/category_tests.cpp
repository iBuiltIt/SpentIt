#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sys/wait.h>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-category-password";
constexpr auto marker="PRIVATE-SYNTHETIC-CATEGORY-612c";
const Currency cad("CAD");
const imports::Occurrence first{"first",0};
const std::string source="D10/08/2026\nT-29.31\nPPRIVATE-SYNTHETIC-CATEGORY-612c\nSFees\nEfee\n$-0.14\nSTransport\nEtransport\n$-29.17\n^\n";
struct Directory {
 fs::path path;
 Directory(){auto name=(fs::temp_directory_path()/"spentit-category-XXXXXX").string();auto* p=mkdtemp(name.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}
 ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location& loc=std::source_location::current()){if(!value)throw std::runtime_error("Category assertion failed at line "+std::to_string(loc.line()));}
template<class F>void rejects(F&& f,const std::source_location& loc=std::source_location::current()){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,loc);}
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void no_leaks(const fs::path& p){for(const auto& f:fs::directory_iterator(p))if(f.is_regular_file()){auto b=bytes(f.path());check(b.find(password)==b.npos&&b.find(marker)==b.npos&&!b.starts_with("SQLite format 3"));}}
ImportStore create(const fs::path& p){auto s=ImportStore::create(p,password,"Synthetic account",cad,"2026-08-01");s.opening(0,"2026-08-01",Money(0,cad));return s;}
app::CategoryRequest make(std::string id,std::string name,std::string parent={},CategoryKind kind=CategoryKind::leaf){return {app::CategoryAction::create,std::move(id),std::move(name),std::move(parent),kind};}
app::CategoryRequest change(app::CategoryAction action,std::string id,std::string name={},std::string parent={}){return {action,std::move(id),std::move(name),std::move(parent)};}
app::CategoryReceipt apply(ImportStore& s,std::string_view operation,const app::CategoryRequest& request){auto p=s.preview_category_change(request);return s.apply_category_change(operation,p.request,p.catalog_revision);}
CategoryDefinition definition(ImportStore& s,std::string_view id){for(const auto& d:s.category_catalog().categories)if(d.identity==id)return d;throw std::runtime_error("Missing category");}
void seed(ImportStore& s){apply(s,"auto-create",make("auto","Auto"));apply(s,"travel-create",make("travel","Travel"));apply(s,"parking-create",make("parking","Auto: Parking & Tolls"));}
void catalog(){
 Directory dir;
 {auto s=create(dir.path/"doc");check(s.category_catalog().revision==0&&s.category_catalog().categories.empty());seed(s);
 auto p=s.preview_category_change(make("amazon","Amazon"));auto r=s.apply_category_change("amazon-create",p.request,p.catalog_revision);
 check(r.catalog_before==3&&r.catalog_after==4&&s.resolve_category("amazon").assignable);
 check(s.apply_category_change("amazon-create",p.request,p.catalog_revision).catalog_after==4);
 rejects([&]{s.apply_category_change("amazon-create",make("other","Other"),p.catalog_revision);});
 rejects([&]{s.apply_category_change("stale",make("other","Other"),p.catalog_revision);});
 check(s.resolve_category("parking").path=="Auto: Parking & Tolls"&&s.resolve_category("auto").assignable);
 apply(s,"rename",change(app::CategoryAction::rename,"parking","Auto: Parking"));
 apply(s,"move",change(app::CategoryAction::rename,"parking","Travel: Parking"));
 check(s.resolve_category("parking").path=="Travel: Parking");
 check(s.category_catalog().creation_order.at("parking")==3);
 apply(s,"parent-archive",change(app::CategoryAction::archive,"travel"));
 check(s.resolve_category("parking").assignable&&!s.resolve_category("parking").archived&&!definition(s,"parking").archived);
 apply(s,"child-archive",change(app::CategoryAction::archive,"parking"));
 apply(s,"parent-unarchive",change(app::CategoryAction::unarchive,"travel"));
 check(!s.resolve_category("parking").assignable&&definition(s,"parking").archived);
 apply(s,"child-unarchive",change(app::CategoryAction::unarchive,"parking"));check(s.resolve_category("parking").assignable);
 apply(s,"group-delete",change(app::CategoryAction::erase,"travel"));check(s.resolve_category("parking").assignable);
 apply(s,"leaf-delete",change(app::CategoryAction::erase,"amazon"));
 rejects([&]{apply(s,"reuse-identity",make("amazon","Different"));});
 apply(s,"reuse-name",make("amazon-two","Amazon"));
 check(s.category_catalog().creation_order.at("amazon")==4 && s.category_catalog().creation_order.at("amazon-two")>4);
 check(s.category_history("parking").size()==5&&s.category_history("amazon").size()==2);
 check(s.committed_balance().units()==0&&s.ledger(false).empty());s.verify();}
 {auto s=ImportStore::open(dir.path/"doc",password);check(s.resolve_category("parking").path=="Travel: Parking");check(s.category_catalog().creation_order.at("parking")==3);s.verify();}no_leaks(dir.path);
}
void income(){Directory dir;
 {auto s=create(dir.path/"doc");seed(s);s.ingest("first",source,qif::DateOrder::dmy);s.commit("first",0,"publish");
 check(!definition(s,"parking").income);
 auto request=change(app::CategoryAction::set_income,"parking");request.income=true;
 const auto preview=s.preview_category_change(request);auto receipt=s.apply_category_change("income",request,preview.catalog_revision);
 check(receipt.after.income && !receipt.before->income);
 check(s.apply_category_change("income",request,preview.catalog_revision).after.income);
 auto different=request;different.income=false;rejects([&]{s.apply_category_change("income",different,preview.catalog_revision);});
 rejects([&]{s.apply_category_change("stale-income",different,preview.catalog_revision);});
 apply(s,"rename-income",change(app::CategoryAction::rename,"parking","Misc Income"));
 apply(s,"archive-income",change(app::CategoryAction::archive,"parking"));
 check(definition(s,"parking").income);apply(s,"unarchive-income",change(app::CategoryAction::unarchive,"parking"));
 check(s.category_catalog().creation_order.at("parking")==3);
 check(s.committed_balance().units()==-2931 && s.evidence("first")==source);s.verify();s.backup_to(dir.path/"backup",password);}
 for(const auto* file:{"doc","backup"}) {auto s=ImportStore::open(dir.path/file,password);check(definition(s,"parking").income);check(s.category_change_outcome("income")->request.income);
 apply(s,"expense",change(app::CategoryAction::set_income,"parking"));check(!definition(s,"parking").income);check(s.committed_balance().units()==-2931);s.verify();}
 no_leaks(dir.path);
}
void income_review(){Directory dir;
 {auto s=create(dir.path/"doc");seed(s);s.ingest("first",source,qif::DateOrder::dmy);
 auto other=source;other.replace(other.find(marker),std::string(marker).size(),"Other synthetic payee");s.ingest("second",other,qif::DateOrder::dmy);
 const auto before=s.review_sheet();check(!before.questions.empty());const auto acceptance=s.acceptance_review();
 const auto balance=s.balance();auto request=change(app::CategoryAction::set_income,"parking");request.income=true;
 int reviewed=0;s.observer([&](ImportPhase phase){if(phase==ImportPhase::review_row)++reviewed;});apply(s,"income",request);s.observer({});check(reviewed==0);
 const auto after=s.review_sheet();check(after.generation==before.generation+1 && !after.can_undo && after.questions.size()==before.questions.size());
 for(std::size_t i=0;i<before.questions.size();++i){check(before.questions[i].key==after.questions[i].key && before.questions[i].rows.size()==after.questions[i].rows.size());}
 check(s.acceptance_review().transactions==acceptance.transactions && s.acceptance_review().warnings.size()==acceptance.warnings.size());check(s.balance()==balance);
 auto expense=request;expense.income=false;
 s.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw std::runtime_error("Synthetic income rollback");});rejects([&]{apply(s,"rollback-income",expense);});s.observer({});
 check(definition(s,"parking").income && !s.category_change_outcome("rollback-income"));check(s.review_sheet().generation==after.generation);
 s.observer([](ImportPhase phase){if(phase==ImportPhase::after_commit)throw std::runtime_error("Synthetic income saved");});rejects([&]{apply(s,"saved-income",expense);});s.observer({});
 check(!definition(s,"parking").income && s.category_change_outcome("saved-income").has_value());check(s.review_sheet().generation==after.generation+1);
 const auto sheet=s.review_sheet();app::ReviewAnswer answer;answer.generation=sheet.generation;answer.question=sheet.questions.front().key;answer.action=app::ReviewAction::separate;
 for(const auto& row:sheet.questions.front().rows)answer.selected.push_back(row.identity);
 s.answer_review("separate",answer);check(s.review_sheet().can_undo);apply(s,"income-again",request);check(s.review_sheet().can_undo);s.verify();}
 {auto s=ImportStore::open(dir.path/"doc",password);check(definition(s,"parking").income && s.review_sheet().can_undo);s.verify();}
}
void setup(){Directory dir;
 {auto s=ImportStore::create(dir.path/"doc",password,"Setup",cad,"2026-10-28",false,{"MM/DD/YYYY",true});check(s.date_format()=="MM/DD/YYYY");check(s.category_catalog().categories.size()==46);for(const auto& c:s.category_catalog().categories){check(c.parent.empty() && c.kind==CategoryKind::leaf);check(s.resolve_category(c.identity).assignable);}check(s.category_catalog().categories[0].revision==0);s.verify();s.backup_to(dir.path/"backup",password);}
 {auto s=ImportStore::open(dir.path/"backup",password);check(s.date_format()=="MM/DD/YYYY" && s.category_catalog().categories.size()==46);s.verify();}
 {auto s=ImportStore::create(dir.path/"empty",password,"Empty",cad,"2026-10-28",false,{"YYYYMMDD",false});check(s.date_format()=="YYYYMMDD" && s.category_catalog().categories.empty());}
 no_leaks(dir.path);
}
void names(){Directory dir;auto s=create(dir.path/"doc");seed(s);
 for(const std::string& name:std::vector<std::string>{"","A:B:C","A:",":B","[A]","A|B",std::string("a\0b",3),"line\nfeed",std::string("\xff",1)})rejects([&]{apply(s,"invalid",make("invalid",name));});
 rejects([&]{apply(s,"duplicate",make("dupe","Auto"));});
 rejects([&]{apply(s,"deep",make("deep","Deep","parking"));});
 rejects([&]{apply(s,"nested",make("nested","Nested","auto",CategoryKind::group));});
 apply(s,"same-other-parent",make("other-parking","Travel: Parking & Tolls"));
 apply(s,"archive",change(app::CategoryAction::archive,"parking"));
 rejects([&]{apply(s,"archived-collision",make("collision","auto: parking & tolls"));});
 rejects([&]{apply(s,"case-distinct",make("lower-auto","auto"));});
 check(app::category_name_matches("Auto: Gas","auto",true));check(!app::category_name_matches("Automobile","Auto",true));check(!app::category_name_matches("Auto: Gas","Auto"));
 apply(s,"gas",make("gas","Auto:Gas"));check(s.resolve_category("gas").path=="Auto: Gas");rejects([&]{apply(s,"gas-again",make("gas-again","auto : gas"));});
 apply(s,"unicode",make("unicode","Énergie"));rejects([&]{apply(s,"unicode-again",make("unicode-again","énergie"));});
 apply(s,"composed",make("composed","Café"));apply(s,"decomposed",make("decomposed","Café"));
 check(s.resolve_category("composed").path!=s.resolve_category("decomposed").path);s.verify();
}
std::int64_t batch_revision(ImportStore& s){for(const auto& b:s.batches())if(b.id=="first")return b.revision;throw std::runtime_error("Missing batch");}
void bind(ImportStore& s,imports::DraftRow& d,std::size_t position,std::string id){auto a=d.transaction.allocations();a.at(position).category_binding=CategoryBinding{id,s.category_catalog().revision};a.at(position).category=s.resolve_category(id).path;a.at(position).category_owner=FieldOwnership::manual;d.transaction=d.transaction.corrected(d.transaction.effective_total(),std::move(a));}
app::ChangeReceipt correct(ImportStore& s,std::string_view op,const imports::DraftRow& d){app::ChangeRequest r{app::ChangeKind::correct,first,d,{}};auto p=s.preview_committed_change(r);return s.apply_committed_change_with_catalog(op,p.request,p.expected,p.preview_sequence,p.catalog_revision);}
void assignments(){Directory dir;auto s=create(dir.path/"doc");seed(s);apply(s,"fees-create",make("fees","Fees"));s.ingest("first",source,qif::DateOrder::dmy);
 auto d=*s.row(first).draft;const auto original=d.transaction.allocations();
 check(original.size()==2&&!original[0].identity.empty()&&original[0].identity!=original[1].identity);
 check(original[0].source_ordinal==0&&original[1].source_ordinal==1&&original[0].category_owner==FieldOwnership::source);
 check(!original[0].category_binding&&original[0].category=="Fees");
 bind(s,d,1,"parking");s.save(first,batch_revision(s),d);auto saved=*s.row(first).draft;
 check(saved.transaction.allocations()[1].category_binding->identity=="parking");
 auto moved=saved.transaction.allocations();std::reverse(moved.begin(),moved.end());moved[0].memo="";moved[0].memo_owner=FieldOwnership::manual;
 saved.transaction=saved.transaction.corrected(saved.transaction.effective_total(),moved);s.save(first,batch_revision(s),saved);
 check(s.row(first).draft->transaction.allocations()[0].source_ordinal==1);
 auto bad=saved;auto invalid=moved;invalid[0].source_ordinal=0;bad.transaction=bad.transaction.corrected(bad.transaction.effective_total(),invalid);
 rejects([&]{s.save(first,batch_revision(s),bad);});
 invalid=moved;invalid[0].identity="forged-source-link";bad.transaction=bad.transaction.corrected(bad.transaction.effective_total(),invalid);rejects([&]{s.save(first,batch_revision(s),bad);});
 invalid=moved;invalid[0].identity="source-999";invalid[0].source_ordinal.reset();invalid[0].category_owner=FieldOwnership::manual;invalid[0].memo_owner=FieldOwnership::manual;bad.transaction=bad.transaction.corrected(bad.transaction.effective_total(),invalid);rejects([&]{s.save(first,batch_revision(s),bad);});
 invalid=moved;invalid[0].category_owner=FieldOwnership::rule;bad.transaction=bad.transaction.corrected(bad.transaction.effective_total(),invalid);rejects([&]{s.save(first,batch_revision(s),bad);});
 apply(s,"archive",change(app::CategoryAction::archive,"parking"));
 saved.memo="Retain archived category";saved.memo_owner=FieldOwnership::manual;s.save(first,batch_revision(s),saved);
 auto replacement=saved;bind(s,replacement,1,"parking");rejects([&]{s.save(first,batch_revision(s),replacement);});
 const auto receipt=s.commit("first",batch_revision(s),"publish");check(receipt.after.units()==-2931);
 rejects([&]{apply(s,"delete-used",change(app::CategoryAction::erase,"parking"));});
 auto edit=s.committed_entry(first).current.draft;edit.payee="Manual correction";edit.payee_owner=FieldOwnership::manual;correct(s,"correct",edit);
 apply(s,"move-archived",change(app::CategoryAction::rename,"parking","Travel: Parking"));
 check(s.resolve_category("parking").path=="Travel: Parking");
 apply(s,"delete-former-parent",change(app::CategoryAction::erase,"auto"));
 check(s.committed_entry(first).current.draft.transaction.allocations()[0].category=="Auto: Parking & Tolls");
 app::ChangeRequest u{app::ChangeKind::undo,first,{},"correct"};auto preview=s.preview_committed_change(u);
 s.apply_committed_change_with_catalog("undo",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);
 check(s.committed_balance().units()==-2931&&s.outcome("publish")->after==receipt.after&&s.evidence("first")==source);s.verify();
 Directory manual;auto m=create(manual.path/"doc");m.ingest("first",source,qif::DateOrder::dmy);m.commit("first",0,"publish");
 auto md=m.committed_entry(first).current.draft;md.payee="";md.payee_owner=FieldOwnership::manual;
 md.transaction=md.transaction.corrected(md.transaction.effective_total(),{{"","",Money(-2931,cad)}});
 auto mp=m.preview_committed_change({app::ChangeKind::correct,first,md,{}});
 auto mr=m.apply_committed_change_with_catalog("manual-replace",mp.request,mp.expected,mp.preview_sequence,mp.catalog_revision);
 const auto& ma=mr.after_entry.draft.transaction.allocations()[0];check(!ma.identity.empty()&&!ma.source_ordinal&&ma.category_owner==FieldOwnership::manual&&ma.memo_owner==FieldOwnership::manual);
 check(mr.after_entry.draft.payee.empty()&&mr.after_entry.draft.payee_owner==FieldOwnership::manual);
 check(m.apply_committed_change_with_catalog("manual-replace",mp.request,mp.expected,mp.preview_sequence,mp.catalog_revision).after_entry.revision==mr.after_entry.revision);m.verify();
}
void history(){Directory dir;auto s=create(dir.path/"doc");seed(s);s.ingest("first",source,qif::DateOrder::dmy);s.commit("first",0,"publish");
 auto edit=s.committed_entry(first).current.draft;bind(s,edit,0,"parking");auto preview=s.preview_committed_change({app::ChangeKind::correct,first,edit,{}});
 apply(s,"rename",change(app::CategoryAction::rename,"parking","Auto: Parking"));
 rejects([&]{s.apply_committed_change_with_catalog("stale-apply",preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);});
 bind(s,edit,0,"parking");correct(s,"assign",edit);
 auto current=s.committed_entry(first).current.draft;auto a=current.transaction.allocations();a[0].category_binding.reset();a[0].category="";a[0].category_owner=FieldOwnership::manual;current.transaction=current.transaction.corrected(current.transaction.effective_total(),a);correct(s,"clear",current);
 rejects([&]{apply(s,"delete-history-used",change(app::CategoryAction::erase,"parking"));});
 apply(s,"archive",change(app::CategoryAction::archive,"parking"));auto u=s.preview_committed_change({app::ChangeKind::undo,first,{},"clear"});
 s.apply_committed_change_with_catalog("restore",u.request,u.expected,u.preview_sequence,u.catalog_revision);
 check(s.committed_entry(first).current.draft.transaction.allocations()[0].category_binding->identity=="parking");
 rejects([&]{apply(s,"publish",make("collision","Collision"));});
 rejects([&]{correct(s,"auto-create",s.committed_entry(first).current.draft);});s.verify();
}

void backup(){Directory dir;{auto s=create(dir.path/"doc");seed(s);s.ingest("first",source,qif::DateOrder::dmy);auto d=*s.row(first).draft;bind(s,d,0,"parking");s.save(first,0,d);apply(s,"archive",change(app::CategoryAction::archive,"auto"));s.backup_to(dir.path/"backup",password);}
 {auto s=ImportStore::open(dir.path/"backup",password);check(s.category_history("auto").size()==2&&!s.resolve_category("parking").archived);check(s.row(first).draft->transaction.allocations()[0].category_binding->identity=="parking");check(s.committed_balance().units()==0&&s.projected_balance()->units()==-2931);s.commit("first",batch_revision(s),"publish");s.verify();}no_leaks(dir.path);}
void legacy(){Directory dir;{auto s=create(dir.path/"doc");s.verify();}sqlite3* db=nullptr;check(sqlite3_open((dir.path/"doc").c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,"PRAGMA user_version=2",nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);const auto before=bytes(dir.path/"doc");rejects([&]{ImportStore::open(dir.path/"doc",password);});check(bytes(dir.path/"doc")==before);no_leaks(dir.path);}
app::CategoryRequest fault_request(int kind){return kind==0?make("new","New"):kind==1?change(app::CategoryAction::rename,"parking","Travel: Parking"):kind==2?change(app::CategoryAction::erase,"parking"):app::CategoryRequest{app::CategoryAction::set_income,"parking",{},{},CategoryKind::leaf,true};}
void fault_setup(const fs::path& path){auto s=create(path);seed(s);}
void recovered(const fs::path& path,const app::CategoryRequest& request,std::int64_t expected){auto s=ImportStore::open(path,password);s.verify();auto outcome=s.category_change_outcome("fault");check(s.category_catalog().revision==expected+(outcome?1:0));auto r=s.apply_category_change("fault",request,expected);check(r.catalog_after==expected+1&&s.category_catalog().revision==expected+1);check(s.apply_category_change("fault",request,expected).catalog_after==r.catalog_after);check(s.committed_balance().units()==0);s.verify();}
void faults(bool crash,bool income_only=false){fault_vfs::install();for(int kind=income_only?3:0;kind<4;++kind){const auto request=fault_request(kind);int positions=0;{Directory dir;fault_setup(dir.path/"doc");auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(-1);s.apply_category_change("fault",request,3);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
 for(int position=1;position<=positions;++position){Directory dir;fault_setup(dir.path/"doc");if(crash){auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);s.apply_category_change("fault",request,3);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&(WEXITSTATUS(status)==71||WEXITSTATUS(status)==0));}else{auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.apply_category_change("fault",request,3);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}recovered(dir.path/"doc",request,3);no_leaks(dir.path);}
 if(crash)for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory dir;fault_setup(dir.path/"doc");auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(dir.path/"doc",password);s.observer([&](ImportPhase current){if(current==phase)_exit(72);});s.apply_category_change("fault",request,3);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==72);{auto s=ImportStore::open(dir.path/"doc",password);check(s.category_change_outcome("fault").has_value()==(phase==ImportPhase::after_commit));}recovered(dir.path/"doc",request,3);}
 std::cout<<"category kind "<<kind<<": "<<positions<<" fault positions\n";
}}

void assignment_setup(const fs::path& path,bool committed){auto s=create(path);seed(s);s.ingest("first",source,qif::DateOrder::dmy);if(committed)s.commit("first",0,"publish");}
imports::DraftRow assignment_request(ImportStore& s,bool committed){auto d=committed?s.committed_entry(first).current.draft:*s.row(first).draft;bind(s,d,0,"parking");return d;}
void assignment_apply(ImportStore& s,bool committed,const imports::DraftRow& d){if(committed)correct(s,"fault-assignment",d);else s.save(first,0,d);}
void assignment_recovered(const fs::path& path,bool committed){auto s=ImportStore::open(path,password);s.verify();auto d=committed?s.committed_entry(first).current.draft:*s.row(first).draft;const auto& a=d.transaction.allocations()[0];
 if(a.category_binding){check(a.category_binding->identity=="parking"&&a.category_owner==FieldOwnership::manual);check(committed?s.committed_change_outcome("fault-assignment").has_value():batch_revision(s)==1);}
 else{check(a.category=="Fees"&&a.category_owner==FieldOwnership::source);check(committed?!s.committed_change_outcome("fault-assignment"):batch_revision(s)==0);assignment_apply(s,committed,assignment_request(s,committed));}
 check(s.committed_balance().units()==(committed?-2931:0));check(s.evidence("first")==source);s.verify();}
void assignment_faults(bool crash){for(bool committed:{false,true}){int positions=0;{Directory dir;assignment_setup(dir.path/"doc",committed);auto s=ImportStore::open(dir.path/"doc",password);const auto d=assignment_request(s,committed);fault_vfs::arm(-1);assignment_apply(s,committed,d);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
 for(int position=1;position<=positions;++position){Directory dir;assignment_setup(dir.path/"doc",committed);if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(dir.path/"doc",password);const auto d=assignment_request(s,committed);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);assignment_apply(s,committed,d);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&(WEXITSTATUS(status)==71||WEXITSTATUS(status)==0));}
 else{auto s=ImportStore::open(dir.path/"doc",password);const auto d=assignment_request(s,committed);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{assignment_apply(s,committed,d);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}
 assignment_recovered(dir.path/"doc",committed);no_leaks(dir.path);}
 std::cout<<"category assignment committed "<<committed<<": "<<positions<<" fault positions\n";
}}

void validation(){const char* damage[]={
 "DROP TRIGGER category_versions_immutable;UPDATE category_versions SET name='Forged' WHERE id='parking' AND revision=0",
 "DROP TRIGGER category_head_step;UPDATE category_heads SET revision=0 WHERE id='parking'",
 "UPDATE category_state SET revision=0",
 "DROP TRIGGER category_operations_immutable;UPDATE category_operations SET catalog_before=0 WHERE id='rename'",
 "DROP TRIGGER history_order_keep;DELETE FROM history_order WHERE category_operation='rename'",
 "DROP TRIGGER operation_registry_keep;DELETE FROM operation_registry WHERE id='rename'",
 "DROP TRIGGER draft_allocation_meta_immutable;UPDATE draft_allocation_meta SET source_ordinal=1 WHERE batch='first' AND ordinal=0 AND revision=0 AND position=0",
 "DROP TRIGGER draft_allocation_meta_keep;DELETE FROM draft_allocation_meta WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 "DROP TRIGGER entry_allocation_meta_immutable;UPDATE entry_allocation_meta SET category_id='travel' WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 "DROP TRIGGER entry_allocation_meta_immutable;UPDATE entry_allocation_meta SET catalog_revision=999 WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 "DROP TRIGGER entry_fields_immutable;UPDATE entry_fields SET payee_owner=3 WHERE batch='first' AND ordinal=0 AND revision=1",
 "DROP TRIGGER entry_allocation_meta_immutable;UPDATE entry_allocation_meta SET source_ordinal=NULL WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 // Ordered audit streams must not hide missing or displaced metadata.
 "DROP TRIGGER entry_fields_keep;DELETE FROM entry_fields WHERE batch='first' AND ordinal=0 AND revision=1",
 "DROP TRIGGER entry_allocation_meta_keep;DELETE FROM entry_allocation_meta WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 "DROP TRIGGER entry_allocation_meta_immutable;UPDATE entry_allocation_meta SET position=9 WHERE batch='first' AND ordinal=0 AND revision=1 AND position=0",
 // Different text cannot claim source ownership; an exact Restore is allowed.
 "DROP TRIGGER entry_fields_immutable;UPDATE entry_fields SET payee_owner=1 WHERE batch='first' AND ordinal=0 AND revision=2",
 "DROP TRIGGER category_versions_immutable;UPDATE category_versions SET income=1 WHERE id='parking' AND revision=0",
 "DROP TRIGGER category_operations_immutable;UPDATE category_operations SET income=1 WHERE id='rename'",
 "PRAGMA user_version=2"
};for(const auto* sql:damage){Directory dir;{auto s=create(dir.path/"doc");seed(s);s.ingest("first",source,qif::DateOrder::dmy);auto d=*s.row(first).draft;bind(s,d,0,"parking");s.save(first,0,d);s.commit("first",1,"publish");d=s.committed_entry(first).current.draft;d.payee="Manual";d.payee_owner=FieldOwnership::manual;d.memo="";d.memo_owner=FieldOwnership::manual;correct(s,"correct",d);d=s.committed_entry(first).current.draft;d.payee="Manual again";correct(s,"correct-again",d);apply(s,"rename",change(app::CategoryAction::rename,"parking","Auto: Parking"));s.verify();}
 sqlite3* db=nullptr;check(sqlite3_open((dir.path/"doc").c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);const auto before=bytes(dir.path/"doc");rejects([&]{ImportStore::open(dir.path/"doc",password);});check(bytes(dir.path/"doc")==before);no_leaks(dir.path);
}}

}
int main(int argc,char** argv){try{const std::string mode=argc>1?argv[1]:"";if(mode=="income_review")income_review();else if(mode=="income")income();else if(mode=="income_io")faults(false,true);else if(mode=="income_crashes")faults(true,true);else if(mode=="setup")setup();else if(mode=="catalog")catalog();else if(mode=="names")names();else if(mode=="assignments")assignments();else if(mode=="history")history();else if(mode=="backup")backup();else if(mode=="legacy")legacy();else if(mode=="validation")validation();else if(mode=="io"){faults(false);assignment_faults(false);}else if(mode=="crashes"){faults(true);assignment_faults(true);}else return 2;std::cout<<"Category "<<mode<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
