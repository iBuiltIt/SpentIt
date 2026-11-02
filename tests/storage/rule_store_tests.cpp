#include <spentit/import_store.hpp>
#include "fault_vfs.hpp"
#include "import_legacy_schema.hpp"
#include "import_schema3_fixture.hpp"
#include "import_schema4_fixture.hpp"
#include "import_schema5_fixture.hpp"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>
using namespace spentit;
using namespace spentit::storage;
namespace fs=std::filesystem;
namespace {
constexpr auto password="synthetic-rule-password";
constexpr auto marker="PRIVATE-SYNTHETIC-RULE-612c";
const Currency cad("CAD");
const std::string split_source=std::string("D10/08/2026\nT-29.31\nP")+marker+"\nEfee\n$-0.14\nEtransport\n$-29.17\n^\n";
const std::string plain_source=std::string("D10/08/2026\nT-1.00\nP")+marker+"\n^\n";
struct Directory {
    fs::path path;
    Directory(const char* name){auto temp=(fs::temp_directory_path()/name).string();auto* p=mkdtemp(temp.data());if(!p)throw std::runtime_error("Temporary directory failed");path=fs::canonical(p);}
    ~Directory(){std::error_code error;fs::remove_all(path,error);}
};
void check(bool value,const std::source_location& loc=std::source_location::current()){if(!value)throw std::runtime_error("Rule store assertion failed at line "+std::to_string(loc.line()));}
template<class F>void rejects_text(F&& f,std::string_view text,const std::source_location& loc=std::source_location::current()){try{f();}catch(const std::exception& e){if(std::string(e.what()).find(text)==std::string::npos)throw std::runtime_error(std::string(e.what())+" (wanted "+std::string(text)+") at line "+std::to_string(loc.line()));return;}throw std::runtime_error(std::string("missing rejection ")+std::string(text)+" at line "+std::to_string(loc.line()));}
template<class F>void rejects_kind(F&& f,rules::RegexFailure kind,std::string_view text,const std::source_location& loc=std::source_location::current()){try{f();}catch(const rules::RegexError& e){if(e.kind()!=kind || std::string(e.what()).find(text)==std::string::npos)throw std::runtime_error(std::string(e.what())+" (wanted kind) at line "+std::to_string(loc.line()));return;}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" (wanted RegexError) at line "+std::to_string(loc.line()));}throw std::runtime_error("missing regex rejection at line "+std::to_string(loc.line()));}
template<class F>void before_begin(F&& f,const std::source_location& loc=std::source_location::current()){fault_vfs::arm(1,SQLITE_FULL);f();check(!fault_vfs::fired && fault_vfs::seen==0,loc);fault_vfs::disarm();}
template<class F>void rejects_before_begin(F&& f,std::string_view text,const std::source_location& loc=std::source_location::current()){fault_vfs::arm(1,SQLITE_FULL);rejects_text(std::forward<F>(f),text,loc);check(!fault_vfs::fired && fault_vfs::seen==0,loc);fault_vfs::disarm();}
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void no_leaks(const fs::path& p){for(const auto& f:fs::directory_iterator(p))if(f.is_regular_file()){auto b=bytes(f.path());check(b.find(password)==b.npos&&b.find(marker)==b.npos&&!b.starts_with("SQLite format 3"));}}
std::string query(const fs::path& p,const char* sql){sqlite3* db=nullptr;check(sqlite3_open(p.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);sqlite3_stmt* st=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&st,nullptr)==SQLITE_OK);check(sqlite3_step(st)==SQLITE_ROW);const auto* text=sqlite3_column_text(st,0);std::string value=text?reinterpret_cast<const char*>(text):"";check(sqlite3_finalize(st)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);return value;}
ImportStore create(const fs::path& p){auto s=ImportStore::create(p,password,"Synthetic account",cad,"2026-08-01");s.opening(0,"2026-08-01",Money(0,cad));auto pcat=s.preview_category_change({app::CategoryAction::create,"rule-default","Rule category",{},CategoryKind::leaf});s.apply_category_change("fixture-category",pcat.request,pcat.catalog_revision);return s;}
app::CategoryRequest make(std::string id,std::string name,std::string parent={},CategoryKind kind=CategoryKind::leaf){return {app::CategoryAction::create,std::move(id),std::move(name),std::move(parent),kind};}
app::CategoryRequest change(app::CategoryAction action,std::string id,std::string name={},std::string parent={}){return {action,std::move(id),std::move(name),std::move(parent)};}
app::CategoryReceipt apply_category(ImportStore& s,std::string_view operation,const app::CategoryRequest& request){auto p=s.preview_category_change(request);return s.apply_category_change(operation,p.request,p.catalog_revision);}
std::int64_t batch_revision(ImportStore& s,std::string_view id){for(const auto& b:s.batches())if(b.id==id)return b.revision;throw std::runtime_error("Missing batch");}
app::RuleDefinitionImage rule_of(std::string id,std::string label,rules::Scope scope,rules::InputField input,std::string expression,rules::Actions actions,bool enabled=true){app::RuleDefinitionImage rule;rule.identity=std::move(id);rule.label=std::move(label);rule.enabled=enabled;rule.scope=scope;rule.input=input;rule.expression=std::move(expression);rule.actions=std::move(actions);return rule;}
app::RuleSaveReceipt save_rules(ImportStore& s,std::string_view operation,std::vector<app::RuleDefinitionImage> ordered,std::int64_t expected){app::OperationContext context;return s.save_rules(operation,{std::move(ordered)},expected,context);}
std::int64_t version_of(const app::RuleSaveReceipt& receipt,std::string_view id){for(const auto& item:receipt.versions)if(item.first==id)return item.second;throw std::runtime_error("Missing rule version");}
app::RulePreviewResult preview(ImportStore& s,std::string batch,std::int64_t revision,bool replace=false){app::OperationContext context;return s.preview_pending_rules({std::move(batch),revision,std::nullopt,replace},context);}
app::RuleApplyReceipt apply_rules(ImportStore& s,std::string_view operation,const app::RuleApplyRequest& request){app::OperationContext context;return s.apply_pending_rules(operation,request,context);}
app::RuleApplyRequest request_from(const app::RulePreviewResult& result){return {result.batch,result.batch_revision,result.ruleset_revision,result.catalog_revision,result.replace_manual};}
void bind(ImportStore& s,imports::DraftRow& d,std::size_t position,std::string id){auto a=d.transaction.allocations();a.at(position).category_binding=CategoryBinding{id,s.category_catalog().revision};a.at(position).category=s.resolve_category(id).path;a.at(position).category_owner=FieldOwnership::manual;a.at(position).category_rule.reset();d.transaction=d.transaction.corrected(d.transaction.effective_total(),std::move(a));}
app::ChangeReceipt correct(ImportStore& s,std::string_view op,const imports::Occurrence& id,const imports::DraftRow& d){app::ChangeRequest r{app::ChangeKind::correct,id,d,{}};auto p=s.preview_committed_change(r);return s.apply_committed_change_with_catalog(op,p.request,p.expected,p.preview_sequence,p.catalog_revision);}
bool same_fields(const imports::DraftRow& a,const imports::DraftRow& b){if(a.date!=b.date||a.payee!=b.payee||a.memo!=b.memo||a.payee_owner!=b.payee_owner||a.memo_owner!=b.memo_owner||a.payee_rule!=b.payee_rule||a.memo_rule!=b.memo_rule||a.transaction.effective_total()!=b.transaction.effective_total())return false;const auto& aa=a.transaction.allocations();const auto& bb=b.transaction.allocations();if(aa.size()!=bb.size())return false;for(std::size_t i=0;i<aa.size();++i)if(aa[i].category!=bb[i].category||aa[i].memo!=bb[i].memo||aa[i].amount!=bb[i].amount||aa[i].category_binding!=bb[i].category_binding||aa[i].identity!=bb[i].identity||aa[i].source_ordinal!=bb[i].source_ordinal||aa[i].category_owner!=bb[i].category_owner||aa[i].memo_owner!=bb[i].memo_owner||aa[i].category_rule!=bb[i].category_rule||aa[i].memo_rule!=bb[i].memo_rule)return false;return true;}
void open_legacy(const fs::path& path,int version){sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);const char* schema=version==5?schema5_import_fixture:version==4?schema4_import_fixture:version==3?schema3_import_schema:legacy_import_schema;check(sqlite3_exec(db,schema,nullptr,nullptr,nullptr)==SQLITE_OK);if(version==1)check(sqlite3_exec(db,"PRAGMA user_version=1",nullptr,nullptr,nullptr)==SQLITE_OK);sqlite3_stmt* insert=nullptr;check(sqlite3_prepare_v2(db,"INSERT INTO account VALUES(1,'Synthetic',?,'2026-08-01',0,0,0,0)",-1,&insert,nullptr)==SQLITE_OK);const auto definition=cad.definition_id();check(sqlite3_bind_text(insert,1,definition.data(),static_cast<int>(definition.size()),SQLITE_TRANSIENT)==SQLITE_OK);check(sqlite3_step(insert)==SQLITE_DONE);check(sqlite3_finalize(insert)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);}

void definitions(){
    fault_vfs::install();Directory dir("spentit-rulestore-def-XXXXXX");const auto path=dir.path/"doc";
    {auto s=create(path);check(s.rules_supported()&&s.categories_supported()&&s.rule_image().revision==0&&s.rule_image().ordered.empty());
    apply_category(s,"fees-create",make("fees","Fees"));apply_category(s,"parking-create",make("parking","Parking"));
    const auto payee=rule_of("payee-rule",std::string("N\xc3\xa4he"),rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false});
    const auto fees=rule_of("fees-rule","Split rule",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false});
    rejects_before_begin([&]{save_rules(s,"fees-create",{payee},0);},"Operation identity belongs to another command");
    const auto created=save_rules(s,"rules-create",{payee,fees},0);check(!created.unchanged&&created.operation=="rules-create"&&created.ruleset_before==0&&created.ruleset_after==1);check(version_of(created,"payee-rule")==0&&version_of(created,"fees-rule")==0);check(s.rule_image().ordered.size()==2&&s.rule_image().ordered[0].label==payee.label);
    const auto unchanged=save_rules(s,"spare-id",s.rule_image().ordered,s.rule_image().revision);check(unchanged.unchanged&&unchanged.operation.empty()&&unchanged.ruleset_after==1&&!s.rule_save_outcome("spare-id")&&s.rule_image().revision==1);
    before_begin([&]{auto retry=save_rules(s,"rules-create",{payee,fees},0);check(retry.operation=="rules-create"&&retry.ruleset_after==1&&retry.versions==created.versions);});
    rejects_before_begin([&]{save_rules(s,"rules-create",{payee},0);},"Operation identity was reused with different content");
    auto edited=s.rule_image().ordered;edited[1].label="Fees renamed";const auto edit=save_rules(s,"spare-id",edited,1);check(!edit.unchanged&&edit.ruleset_after==2&&version_of(edit,"payee-rule")==0&&version_of(edit,"fees-rule")==1);
    auto swapped=s.rule_image().ordered;std::swap(swapped[0],swapped[1]);swapped[1].enabled=false;const auto reordered=save_rules(s,"reorder",swapped,2);check(reordered.ruleset_after==3&&version_of(reordered,"payee-rule")==0&&version_of(reordered,"fees-rule")==1);check(!s.rule_image().ordered[1].enabled&&s.rule_image().ordered[0].identity=="fees-rule");
    const auto removed=save_rules(s,"delete-fees",{s.rule_image().ordered[1]},3);check(removed.ruleset_after==4&&s.rule_image().ordered.size()==1&&s.rule_tombstones().size()==1&&s.rule_tombstones()[0].identity=="fees-rule"&&s.rule_tombstones()[0].version==1);
    auto revived=s.rule_image().ordered;revived.push_back(fees);rejects_before_begin([&]{save_rules(s,"revive",revived,s.rule_image().revision);},"Deleted rule identity cannot return");
    auto tabbed=s.rule_image().ordered;tabbed[0].label="Na\the";rejects_before_begin([&]{save_rules(s,"tab-label",tabbed,s.rule_image().revision);},"Invalid rule label");
    const auto current=s.rule_image();rejects_text([&]{save_rules(s,"stale-save",current.ordered,0);},"Rules, categories, or the import changed; refresh before applying");check(!s.rule_save_outcome("stale-save")&&s.rule_image().revision==current.revision);
    const auto kept=s.rule_image();s.ingest("temporary",plain_source,qif::DateOrder::dmy);s.discard("temporary",0);check(s.rule_image().revision==kept.revision&&s.rule_image().ordered.size()==kept.ordered.size()&&s.rule_image().ordered[0].label==kept.ordered[0].label);
    rejects_text([&]{apply_category(s,"erase-fees",change(app::CategoryAction::erase,"fees"));},"Transactions use this category. Archive it instead.");apply_category(s,"archive-fees",change(app::CategoryAction::archive,"fees"));check(s.resolve_category("fees").archived);
    const imports::Occurrence row{"assigned",0};s.ingest(row.batch,split_source,qif::DateOrder::dmy);auto draft=*s.row(row).draft;bind(s,draft,1,"parking");s.save(row,0,draft);s.commit(row.batch,1,"publish-assigned");check(s.committed_balance().units()==-2931&&s.committed_entry(row).current.draft.transaction.allocations()[1].category_binding->identity=="parking");s.verify();}
    check(query(path,"PRAGMA user_version")=="42");no_leaks(dir.path);
}

void application(){
    Directory dir("spentit-rule-gate-XXXXXX");const auto path=dir.path/"doc";
    {
        auto s=create(path);apply_category(s,"fees-create",make("fees","Fees"));
        auto first=rule_of("first","First",rules::Scope::transaction,rules::InputField::payee,"^Synthetic Park",{{},{},"fees",false});
        auto later=rule_of("later","Later",rules::Scope::transaction,rules::InputField::payee,"^Synthetic",{"Wrong for parking","memo","rule-default",false});
        save_rules(s,"save",{first,later},0);
        s.ingest("batch","D1/8/2026\nT-1\nPSynthetic Park\n^\nD2/8/2026\nT-2\nPSynthetic Book\n^\nD3/8/2026\nT-3\nPSynthetic Categorized\nLImported label\n^\n",qif::DateOrder::dmy);
        app::RulePreviewRequest query;query.account_scope=true;query.single_rule=true;query.saved_rule="later";query.matches_only=true;
        app::OperationContext c;auto p=s.preview_pending_rules(query,c);
        check(p.raw_matches==3 && p.categorized_matches==1 && p.earlier_matches==1 && p.eligible_matches==1 && p.changed==1 && p.matches.size()==1 && p.matches[0].identity.ordinal==1);
        app::OperationContext apply;s.apply_account_rules("selected",p.account_request,apply);
        check(s.row({"batch",0}).draft->payee=="Synthetic Park" && s.row({"batch",0}).draft->transaction.allocations()[0].category.empty());
        check(s.row({"batch",1}).draft->payee=="Wrong for parking");
        s.undo_account_rules("undo");
        query.saved_rule.clear();query.single_rule=false;p=s.preview_pending_rules(query,c);
        app::OperationContext all;s.apply_account_rules("all",p.account_request,all);
        check(s.row({"batch",0}).draft->payee=="Synthetic Park" && s.row({"batch",0}).draft->memo.empty());
        check(s.row({"batch",0}).draft->transaction.allocations()[0].category=="Fees");
        check(s.row({"batch",2}).draft->transaction.allocations()[0].category=="Imported label");
        auto no_category=later;no_category.actions.category.reset();
        rejects_text([&]{save_rules(s,"invalid",{no_category},1);},"Choose a category");
        query.single_rule=true;query.editor=std::vector{no_category};
        rejects_text([&]{s.preview_pending_rules(query,c);},"Choose a category");
        s.verify();s.commit("batch",batch_revision(s,"batch"),"accept");
        auto clear=s.committed_entry({"batch",0}).current.draft;auto aa=clear.transaction.allocations();aa[0].category.clear();aa[0].category_binding.reset();aa[0].category_owner=FieldOwnership::manual;aa[0].category_rule.reset();clear.transaction=clear.transaction.corrected(clear.transaction.effective_total(),aa);clear.payee="Manual name";clear.payee_owner=FieldOwnership::manual;correct(s,"clear",{"batch",0},clear);
        query.editor.reset();query.saved_rule="first";p=s.preview_pending_rules(query,c);check(p.changed==1);
        app::OperationContext rerun;s.apply_account_rules("rerun",p.account_request,rerun);check(s.committed_entry({"batch",0}).current.draft.payee=="Manual name");
        s.undo_account_rules("undo-rerun");check(s.committed_entry({"batch",0}).current.draft.transaction.allocations()[0].category.empty());s.verify();
    }
    {auto s=ImportStore::open(path,password);s.verify();}
}

void schema(){
    fault_vfs::install();
    for(int version:{1,2,3,4,5}){Directory dir("spentit-rulestore-legacy-XXXXXX");const auto path=dir.path/"doc";open_legacy(path,version);const auto before=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==before);no_leaks(dir.path);}
    Directory mismatch("spentit-rulestore-shape-XXXXXX");const auto path=mismatch.path/"doc";open_legacy(path,5);
    auto alter=[&](const char* sql){sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);check(sqlite3_key(db,password,static_cast<int>(std::string_view(password).size()))==SQLITE_OK);check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);check(sqlite3_close(db)==SQLITE_OK);};
    alter("PRAGMA user_version=99");const auto unknown=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==unknown&&!fs::exists(fs::path(path.string()+"-journal")));
    alter("PRAGMA user_version=16");const auto rejected16=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==rejected16);
    alter("PRAGMA user_version=17");const auto rejected17=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==rejected17);
    alter("PRAGMA user_version=18");const auto rejected18=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==rejected18);
    alter("PRAGMA user_version=19");const auto rejected19=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==rejected19);
    alter("PRAGMA user_version=20");const auto bumped=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported by this version of SpentIt; the file has not been changed");check(bytes(path)==bumped);
    for(const auto version:{27,28,29,30}){alter(("PRAGMA user_version="+std::to_string(version)).c_str());const auto before=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"This dataset format is not supported");check(bytes(path)==before);}
    alter("PRAGMA user_version=42");const auto current=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"Account structure does not match format version");check(bytes(path)==current);
    alter("CREATE TABLE rule_state(singleton INTEGER PRIMARY KEY)");const auto extra=bytes(path);rejects_text([&]{ImportStore::open(path,password);},"Account structure does not match format version");check(bytes(path)==extra);
    const auto sibling=bytes(path);fault_vfs::arm(1,SQLITE_FULL);rejects_text([&]{ImportStore::create(mismatch.path/"other",password,"Synthetic account",cad,"2026-08-01");},"");fault_vfs::disarm();check(bytes(path)==sibling);
}

void validation(){
    fault_vfs::install();
    {Directory dir("spentit-rulestore-pattern-XXXXXX");auto s=create(dir.path/"doc");rejects_before_begin([&]{save_rules(s,"bad-pattern",{rule_of("bad","Bad",rules::Scope::transaction,rules::InputField::payee,"(",{"X",{},"rule-default",false})},0);},"");rejects_kind([&]{save_rules(s,"bad-pattern",{rule_of("bad","Bad",rules::Scope::transaction,rules::InputField::payee,"(",{"X",{},"rule-default",false})},0);},rules::RegexFailure::invalid_pattern,"Pattern");
    rejects_kind([&]{save_rules(s,"bad-template",{rule_of("bad","Bad",rules::Scope::transaction,rules::InputField::payee,"fee",{"${99}",{},"rule-default",false})},0);},rules::RegexFailure::invalid_template,"capture reference");
    auto empty=save_rules(s,"empty-expression",{rule_of("empty","Empty",rules::Scope::transaction,rules::InputField::payee,"",{"X",{},"rule-default",false})},0);check(empty.ruleset_after==1&&s.rule_image().ordered[0].expression.empty());}
    {Directory dir("spentit-rulestore-category-XXXXXX");auto s=create(dir.path/"doc");apply_category(s,"group",make("group","Group"));apply_category(s,"fees",make("fees","Fees"));apply_category(s,"archive",change(app::CategoryAction::archive,"fees"));
    check(save_rules(s,"disabled",{rule_of("disabled","Disabled",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false},false)},0).ruleset_after==1);
    rejects_kind([&]{save_rules(s,"enabled-archived",{rule_of("enabled","Enabled",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false})},1);},rules::RegexFailure::invalid_input,"Enabled rule targets an unavailable category");
    check(save_rules(s,"group-rule",{rule_of("grouped","Grouped",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"group",false},false)},1).ruleset_after==2);}
    {Directory dir("spentit-rulestore-later-XXXXXX");auto s=create(dir.path/"doc");apply_category(s,"fees",make("fees","Fees"));save_rules(s,"enabled",{rule_of("enabled","Enabled",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false})},0);apply_category(s,"archive",change(app::CategoryAction::archive,"fees"));check(s.rule_image().ordered[0].enabled);const imports::Occurrence row{"pending",0};s.ingest(row.batch,split_source,qif::DateOrder::dmy);rejects_kind([&]{preview(s,row.batch,0);},rules::RegexFailure::invalid_input,"Enabled rule targets an unavailable category");check(s.rule_image().ordered[0].enabled);}
    {Directory dir("spentit-rulestore-limit-XXXXXX");auto s=create(dir.path/"doc");std::vector<app::RuleDefinitionImage> many;for(int i=0;i<129;++i)many.push_back(rule_of("r"+std::to_string(i),"Rule",rules::Scope::transaction,rules::InputField::payee,"fee",{"X",{},"rule-default",false}));rejects_kind([&]{save_rules(s,"too-many",many,0);},rules::RegexFailure::resource_limit,"Too many rules");
    std::string bulk;bulk.reserve(4097*32);for(int i=0;i<4097;++i)bulk+="D10/08/2026\nT-1.00\nPItem\n^\n";s.ingest("bulk",bulk,qif::DateOrder::dmy);check(preview(s,"bulk",0).evaluated==4097);}
    {Directory dir("spentit-rulestore-ordinal-XXXXXX");const auto path=dir.path/"doc";const imports::Occurrence row{"pending",0};
    {auto s=create(path);apply_category(s,"fees",make("fees","Fees"));const auto split=rule_of("split-rule","Split rule",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false});save_rules(s,"save-split",{split},0);s.ingest(row.batch,split_source,qif::DateOrder::dmy);auto forged=*s.row(row).draft;auto allocations=forged.transaction.allocations();allocations[0].source_ordinal=1;forged.transaction=forged.transaction.corrected(forged.transaction.effective_total(),allocations);rejects_text([&]{s.save(row,0,forged);},"Source association");
    auto invented=*s.row(row).draft;allocations=invented.transaction.allocations();allocations[0].category_owner=FieldOwnership::rule;allocations[0].category_rule.reset();invented.transaction=invented.transaction.corrected(invented.transaction.effective_total(),std::move(allocations));rejects_text([&]{s.save(row,0,invented);},"Ordinary save cannot introduce rule ownership");
    auto applied=apply_rules(s,"apply-split",request_from(preview(s,row.batch,0)));check(applied.changed>=1);auto carried=*s.row(row).draft;check(carried.transaction.allocations()[0].category_rule==RuleMark{"split-rule",0});s.save(row,1,carried);}
    {auto carry=ImportStore::open(path,password);check(carry.row(row).draft->transaction.allocations()[0].category_rule==RuleMark{"split-rule",0});}}
}

void backup(){
    Directory dir("spentit-rulestore-backup-XXXXXX");const auto path=dir.path/"doc";app::RuleImage image;std::optional<imports::DraftRow> ruled;
    {auto s=create(path);apply_category(s,"fees",make("fees","Fees"));save_rules(s,"save-rules",{rule_of("payee-rule","Payee rule",rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false}),rule_of("split-rule","Split rule",rules::Scope::split,rules::InputField::memo,"fee",{{},{},"fees",false})},0);s.ingest("pending",split_source,qif::DateOrder::dmy);apply_rules(s,"apply-rules",request_from(preview(s,"pending",0)));image=s.rule_image();ruled=*s.row({"pending",0}).draft;s.backup_to(dir.path/"backup",password);s.discard("pending",batch_revision(s,"pending"));check(s.rule_image().revision==image.revision&&s.rule_image().ordered.size()==image.ordered.size());}
    {auto s=ImportStore::open(dir.path/"backup",password);check(s.rule_image().revision==image.revision&&s.rule_image().ordered[0].label==image.ordered[0].label&&s.rule_image().ordered[1].identity=="split-rule");auto draft=*s.row({"pending",0}).draft;check(same_fields(draft,*ruled)&&s.batches().at(0).state==app::BatchState::pending);s.verify();}
    {auto s=ImportStore::open(dir.path/"backup",password);check(s.rule_image().revision==image.revision&&same_fields(*s.row({"pending",0}).draft,*ruled));}no_leaks(dir.path);
}

app::RuleSaveRequest fault_save_request(){return {{rule_of("payee-rule","Payee rule",rules::Scope::transaction,rules::InputField::payee,marker,{"Ruled Payee",{},"rule-default",false})}};}
void save_recovered(const fs::path& path,bool strict){auto s=ImportStore::open(path,password);s.verify();auto outcome=s.rule_save_outcome("fault");if(strict)check(!outcome);if(outcome)check(outcome->ruleset_after==1&&s.rule_image().revision==1);else check(s.rule_image().revision==0);app::OperationContext context;auto saved=s.save_rules("fault",fault_save_request(),0,context);check(saved.ruleset_after==1&&saved.operation=="fault");auto again=s.save_rules("fault",fault_save_request(),0,context);check(again.ruleset_after==saved.ruleset_after&&again.versions==saved.versions);s.verify();}
void apply_setup(const fs::path& path){auto s=create(path);save_rules(s,"save-payee",fault_save_request().ordered,0);s.ingest("pending",plain_source+plain_source,qif::DateOrder::dmy);}
app::RuleApplyRequest fault_apply_request(){return {"pending",0,1,1,false};}
void apply_recovered(const fs::path& path,bool strict){auto s=ImportStore::open(path,password);s.verify();auto outcome=s.rule_apply_outcome("fault");if(strict)check(!outcome);if(!outcome){check(batch_revision(s,"pending")==0);auto draft=*s.row({"pending",0}).draft;check(draft.payee==marker&&draft.payee_owner==FieldOwnership::source);check(s.row({"pending",1}).draft->payee==marker);}app::OperationContext context;auto saved=s.apply_pending_rules("fault",fault_apply_request(),context);check(saved.changed==2&&saved.batch_after==1&&saved.operation=="fault");auto again=s.apply_pending_rules("fault",fault_apply_request(),context);check(again.batch_after==saved.batch_after&&again.changed==saved.changed);check(s.row({"pending",0}).draft->payee=="Ruled Payee" && s.row({"pending",1}).draft->payee=="Ruled Payee");s.verify();}
void sweep(bool apply,bool crash){int positions=0;{Directory dir("spentit-rulestore-probe-XXXXXX");if(apply)apply_setup(dir.path/"doc");else create(dir.path/"doc");auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(-1);app::OperationContext context;if(apply)s.apply_pending_rules("fault",fault_apply_request(),context);else s.save_rules("fault",fault_save_request(),0,context);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    for(int position=1;position<=positions;++position){Directory dir("spentit-rulestore-fault-XXXXXX");if(apply)apply_setup(dir.path/"doc");else create(dir.path/"doc");if(crash){const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,SQLITE_IOERR_WRITE,true,true);app::OperationContext context;if(apply)s.apply_pending_rules("fault",fault_apply_request(),context);else s.save_rules("fault",fault_save_request(),0,context);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&(WEXITSTATUS(status)==71||WEXITSTATUS(status)==0));}
    else{auto s=ImportStore::open(dir.path/"doc",password);fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);app::OperationContext context;try{if(apply)s.apply_pending_rules("fault",fault_apply_request(),context);else s.save_rules("fault",fault_save_request(),0,context);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}if(apply)apply_recovered(dir.path/"doc",!crash);else save_recovered(dir.path/"doc",!crash);no_leaks(dir.path);}
    if(crash)for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory dir("spentit-rulestore-observer-XXXXXX");if(apply)apply_setup(dir.path/"doc");else create(dir.path/"doc");const auto child=fork();check(child>=0);if(child==0){auto s=ImportStore::open(dir.path/"doc",password);s.observer([&](ImportPhase current){if(current==phase)_exit(72);});app::OperationContext context;if(apply)s.apply_pending_rules("fault",fault_apply_request(),context);else s.save_rules("fault",fault_save_request(),0,context);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==72);{auto s=ImportStore::open(dir.path/"doc",password);const auto present=apply?s.rule_apply_outcome("fault").has_value():s.rule_save_outcome("fault").has_value();check(present==(phase==ImportPhase::after_commit));if(phase==ImportPhase::after_commit){app::OperationContext context;if(apply){auto retry=s.apply_pending_rules("fault",fault_apply_request(),context);check(retry.batch_after==s.rule_apply_outcome("fault")->batch_after);}else{auto retry=s.save_rules("fault",fault_save_request(),0,context);check(retry.ruleset_after==s.rule_save_outcome("fault")->ruleset_after);}}}if(apply)apply_recovered(dir.path/"doc",false);else save_recovered(dir.path/"doc",false);}
    std::cout<<"rulestore "<<(apply?"apply":"save")<<(crash?" crashes":" io")<<": "<<positions<<" fault positions\n";
}
void match_preview(){
    Directory dir("spentit-matches-XXXXXX");auto s=create(dir.path/"doc");
    apply_category(s,"fees",make("fees","Fees"));
    auto prior=rule_of("prior","Prior",rules::Scope::transaction,rules::InputField::payee,"^Synthetic",{"Prior name","Prior memo","rule-default",false});
    save_rules(s,"prior-rules",{prior},0);
    s.ingest("batch","D10/08/2026\nT-1.00\nPSynthetic shop\n^\nD10/08/2026\nT-2.00\nPUnrelated\n^\nD10/08/2026\nT-3.00\nPSynthetic ride\n^\nD10/08/2026\nT-4.00\nPTravel split\nEride one\n$-1.00\nEride two\n$-3.00\n^\n",qif::DateOrder::dmy);
    apply_rules(s,"prior-apply",request_from(preview(s,"batch",0)));
    const auto shop=*s.row({"batch",0}).draft;
    prior.enabled=false;
    auto ride=rule_of("ride","Ride",rules::Scope::transaction,rules::InputField::payee,"^Synthetic ride$",{"Ride",{},"fees",false});
    auto split=rule_of("split","Split",rules::Scope::split,rules::InputField::memo,"^ride",{{},{},"fees",false});
    save_rules(s,"new-rules",{prior,ride,split},1);
    auto manual=*s.row({"batch",2}).draft;bind(s,manual,0,"fees");s.save({"batch",2},batch_revision(s,"batch"),manual);
    app::OperationContext context;app::RulePreviewRequest request{"batch",batch_revision(s,"batch"),{},false,false,true};
    const auto result=s.preview_pending_rules(request,context);
    check(result.evaluated==4 && result.matches.size()==1 && result.changed==1 && result.records.empty());
    check(result.matches[0].identity.ordinal==3 && result.categorized_matches==1);
    check(same_fields(shop,*s.row({"batch",0}).draft));
    const auto applied=apply_rules(s,"matches-apply",request_from(result));check(applied.changed==1 && applied.evaluated==4);
    check(same_fields(shop,*s.row({"batch",0}).draft));
    const auto after=*s.row({"batch",2}).draft;check(after.payee=="Prior name" && after.memo=="Prior memo" && after.memo_rule==RuleMark{"prior",0});
    check(after.transaction.allocations()[0].category_owner==FieldOwnership::manual);
    s.verify();
    app::OperationContext cancelled;cancelled.request();bool stopped=false;
    try{s.preview_pending_rules(request,cancelled);}catch(const app::OperationCancelled&){stopped=true;}check(stopped);
    auto invalid=request;invalid.batch_revision=batch_revision(s,"batch");invalid.editor=std::vector{ride};invalid.editor->front().expression="[";
    rejects_kind([&]{s.preview_pending_rules(invalid,context);},rules::RegexFailure::invalid_pattern,"");
    check(same_fields(after,*s.row({"batch",2}).draft));
    save_rules(s,"remove-rules",{},2);
    const auto none=preview(s,"batch",batch_revision(s,"batch"));check(none.matches.empty() && none.changed==0);
    check(apply_rules(s,"empty-apply",request_from(none)).changed==0);
    check(same_fields(after,*s.row({"batch",2}).draft));s.verify();
}
void large_preview(std::size_t count,bool accepted=false){
    Directory dir("spentit-large-preview-XXXXXX");auto s=create(dir.path/"doc");
    auto ride=rule_of("ride","Ride",rules::Scope::transaction,rules::InputField::payee,"^Synthetic ride",{"Ride",{},"rule-default",false});
    save_rules(s,"rules",{ride},0);
    std::string source;source.reserve(count*65);
    for(std::size_t i=0;i<count;++i)source+="D10/08/2026\nT-1.00\nP"+std::string(i%10==0?"Synthetic ride ":"Unrelated ")+std::to_string(i)+"\n^\n";
    const auto start=std::chrono::steady_clock::now();s.ingest("large",source,qif::DateOrder::dmy);
    const auto imported=std::chrono::steady_clock::now();
    app::OperationContext context;app::RulePreviewRequest request{"large",0,{},false,false,true};
    const auto result=s.preview_pending_rules(request,context);
    const auto previewed=std::chrono::steady_clock::now();
    check(result.evaluated==count && result.matches.size()==(count+9)/10 && result.records.empty());
    request.single_rule=true;request.editor=std::vector{ride};
    const auto single=s.preview_pending_rules(request,context);check(single.evaluated==count && single.matches.size()==result.matches.size() && single.records.empty());
    const auto single_done=std::chrono::steady_clock::now();
    request.editor->front().expression=".*";
    const auto all=s.preview_pending_rules(request,context);check(all.matches.size()==count && all.records.empty());
    const auto all_done=std::chrono::steady_clock::now();
    app::OperationContext cancel;std::size_t checkpoints=0;cancel.progress=[&](std::string_view){if(++checkpoints==3)cancel.request();};
    bool stopped=false;try{s.preview_pending_rules(request,cancel);}catch(const app::OperationCancelled&){stopped=true;}check(stopped);
    check(batch_revision(s,"large")==0 && s.row({"large",0}).draft->payee=="Synthetic ride 0");
    app::OperationContext cancelApply;std::size_t applyCheckpoints=0;cancelApply.progress=[&](std::string_view){if(++applyCheckpoints==3)cancelApply.request();};
    bool applyStopped=false;try{s.apply_pending_rules("cancelled-apply",request_from(result),cancelApply);}catch(const app::OperationCancelled&){applyStopped=true;}
    check(applyStopped && !s.rule_apply_outcome("cancelled-apply") && batch_revision(s,"large")==0);
    const auto applied=apply_rules(s,"large-apply",request_from(result));check(applied.changed==result.matches.size() && applied.evaluated==count);
    check(s.row({"large",0}).draft->payee=="Ride" && s.row({"large",1}).draft->payee=="Unrelated 1");
    check(s.balance().units()==-100*static_cast<std::int64_t>(count));
    auto ms=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::milliseconds>(b-a).count();};
    std::cout<<"rows="<<count<<" import_ms="<<ms(start,imported)<<" full_preview_ms="<<ms(imported,previewed)<<" single_preview_ms="<<ms(previewed,single_done)<<" all_match_ms="<<ms(single_done,all_done)<<" apply_and_cancel_ms="<<ms(all_done,std::chrono::steady_clock::now())<<'\n';
    if(accepted)s.commit("large",batch_revision(s,"large"),"accept-large");
    auto current=[&](std::size_t ordinal){return accepted?s.committed_entry({"large",ordinal}).current.draft:*s.row({"large",ordinal}).draft;};
    ride.actions.payee_template="Ride updated";save_rules(s,"account-rules",{ride},1);
    app::RulePreviewRequest accountQuery;accountQuery.account_scope=true;accountQuery.matches_only=true;accountQuery.saved_rule="ride";
    const auto accountPreview=s.preview_pending_rules(accountQuery,context);check(accountPreview.changed==0 && accountPreview.categorized_matches==result.matches.size());
    app::OperationContext accountApply;check(s.apply_account_rules("account-large",accountPreview.account_request,accountApply).operation.empty());
    check(current(0).payee=="Ride" && current(1).payee=="Unrelated 1");
    if(count<=5000)s.verify();
}
void account_apply(){
    fault_vfs::install();Directory dir("spentit-account-rules-XXXXXX");const auto path=dir.path/"doc";
    auto s=create(path);
    s.ingest("old","D01/08/2026\nT-1.00\nPRide old\n^\n",qif::DateOrder::dmy);s.commit("old",0,"commit-old");
    s.ingest("new","D02/08/2026\nT-2.00\nPRide new\n^\nD03/08/2026\nT-3.00\nPShop\n^\n",qif::DateOrder::dmy);
    s.ingest("later","D04/08/2026\nT-4.00\nPRide later\n^\n",qif::DateOrder::dmy);
    s.create_account(2,"Other account",cad,"2026-08-01");s.select_account(2);s.ingest("other","D05/08/2026\nT-5.00\nPRide other\n^\n",qif::DateOrder::dmy);s.select_account(1);
    auto ride=rule_of("ride","Ride",rules::Scope::transaction,rules::InputField::payee,"^Ride",{"Ruled ride",{},"rule-default",false});
    auto shop=rule_of("shop","Shop",rules::Scope::transaction,rules::InputField::payee,"^Shop",{"Ruled shop",{},"rule-default",false});
    save_rules(s,"save",{ride,shop},0);
    app::RulePreviewRequest query;query.account_scope=true;query.matches_only=true;query.saved_rule="ride";query.new_only=true;
    app::OperationContext context;auto preview=s.preview_pending_rules(query,context);check(preview.matches.size()==2 && preview.changed==2);
    const auto balance=s.balance();const auto committed=s.committed_balance();
    s.select_account(2);app::OperationContext wrongAccount;rejects_text([&]{s.apply_account_rules("wrong-account",preview.account_request,wrongAccount);},"refresh");s.select_account(1);
    app::OperationContext apply;auto receipt=s.apply_account_rules("apply-new",preview.account_request,apply);check(!receipt.operation.empty());
    check(s.committed_entry({"old",0}).current.draft.payee=="Ride old");check(s.row({"new",0}).draft->payee=="Ruled ride" && s.row({"later",0}).draft->payee=="Ruled ride" && s.row({"new",1}).draft->payee=="Shop");
    check(s.balance()==balance && s.committed_balance()==committed && s.can_undo_account_rules());s.verify();
    app::OperationContext retry;check(s.apply_account_rules("apply-new",preview.account_request,retry).operation==receipt.operation);
    s.undo_account_rules("undo-new");check(s.row({"new",0}).draft->payee=="Ride new" && s.row({"later",0}).draft->payee=="Ride later");s.verify();
    query.new_only=false;preview=s.preview_pending_rules(query,context);check(preview.matches.size()==3 && preview.changed==3);
    app::OperationContext cancel;int writes=0;cancel.progress=[&](std::string_view phase){if(phase=="Applying matching rules" && ++writes==2)cancel.request();};
    rejects_text([&]{s.apply_account_rules("cancel-account",preview.account_request,cancel);},"cancel");check(!s.review_outcome("cancel-account"));check(s.row({"new",0}).draft->payee=="Ride new");
    app::OperationContext all;s.apply_account_rules("apply-all",preview.account_request,all);
    check(s.committed_entry({"old",0}).current.draft.payee=="Ruled ride" && s.row({"new",0}).draft->payee=="Ruled ride");check(s.balance()==balance);s.verify();
    s.undo_account_rules("undo-all");check(s.committed_entry({"old",0}).current.draft.payee=="Ride old" && s.row({"new",0}).draft->payee=="Ride new");s.verify();
    query.saved_rule="";preview=s.preview_pending_rules(query,context);check(preview.matches.size()==4);
    auto manual=*s.row({"new",0}).draft;manual.payee="My ride";manual.payee_owner=FieldOwnership::manual;s.save({"new",0},batch_revision(s,"new"),manual);
    app::OperationContext stale;rejects_text([&]{s.apply_account_rules("stale",preview.account_request,stale);},"refresh");
    preview=s.preview_pending_rules(query,context);app::OperationContext keep;s.apply_account_rules("manual-kept",preview.account_request,keep);check(s.row({"new",0}).draft->payee=="My ride");s.verify();
    query.replace_manual=true;rejects_text([&]{s.preview_pending_rules(query,context);},"Clear categories");
    check(s.row({"new",0}).draft->payee=="My ride");s.verify();
    s.select_account(2);check(s.row({"other",0}).draft->payee=="Ride other" && !s.can_undo_account_rules());s.select_account(1);
}

app::AccountRuleRequest account_fault_setup(const fs::path& path){
    auto s=create(path);s.ingest("old","D01/08/2026\nT-1\nPRide old\n^\n",qif::DateOrder::dmy);s.commit("old",0,"accept");
    s.ingest("new","D02/08/2026\nT-2\nPRide new\n^\nD03/08/2026\nT-3\nPRide next\n^\n",qif::DateOrder::dmy);
    save_rules(s,"save",{rule_of("ride","Ride",rules::Scope::transaction,rules::InputField::payee,"Ride",{"Ruled",{},"rule-default",false})},0);
    app::OperationContext c;app::RulePreviewRequest r;r.account_scope=true;r.matches_only=true;return s.preview_pending_rules(r,c).account_request;
}
void account_fault_recover(const fs::path& path,const app::AccountRuleRequest& request){
    auto s=ImportStore::open(path,password);const bool saved=s.review_outcome("fault").has_value();
    check(s.committed_entry({"old",0}).current.draft.payee==(saved?"Ruled":"Ride old"));
    check(s.row({"new",0}).draft->payee==(saved?"Ruled":"Ride new"));check(s.row({"new",1}).draft->payee==(saved?"Ruled":"Ride next"));
    check(s.balance().units()==-600);app::OperationContext c;s.apply_account_rules("fault",request,c);s.verify();
    check(s.can_undo_account_rules());s.undo_account_rules("undo");check(s.committed_entry({"old",0}).current.draft.payee=="Ride old" && s.row({"new",1}).draft->payee=="Ride next");s.verify();
}
void account_faults(){
    fault_vfs::install();int positions=0;
    {Directory dir("spentit-account-probe-XXXXXX");const auto r=account_fault_setup(dir.path/"doc");auto s=ImportStore::open(dir.path/"doc",password);app::OperationContext c;fault_vfs::arm(-1);s.apply_account_rules("fault",r,c);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    for(int position=1;position<=positions;++position){Directory dir("spentit-account-fault-XXXXXX");const auto path=dir.path/"doc";const auto r=account_fault_setup(path);
        {auto s=ImportStore::open(path,password);app::OperationContext c;fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.apply_account_rules("fault",r,c);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}
        account_fault_recover(path,r);no_leaks(dir.path);
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}){Directory dir("spentit-account-crash-XXXXXX");const auto path=dir.path/"doc";const auto r=account_fault_setup(path);const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});app::OperationContext c;s.apply_account_rules("fault",r,c);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        {auto s=ImportStore::open(path,password);check(s.review_outcome("fault").has_value()==(phase==ImportPhase::after_commit));}account_fault_recover(path,r);
    }
    std::cout<<"account Apply fault positions swept: "<<positions<<'\n';
}

void io(){fault_vfs::install();sweep(false,false);sweep(true,false);}
void crashes(){fault_vfs::install();sweep(false,true);sweep(true,true);}
#include "bulk_cases.inc"
#include "reapply_cases.inc"

}
void category_gate() {
    Directory dir("spentit-category-gate-XXXXXX");const auto path=dir.path/"doc";
    const std::string data="!Type:Bank\nD10/08/2026\nT-12.50\nPSynthetic Cafe\nEFee text\n$-0.25\nEPurchase\n$-12.25\n^\nD11/08/2026\nT-7.00\nPSynthetic Cafe\nLImported Food\n^\nD12/08/2026\nT-2.00\nPOther\nLDiscarded Name\n^\n";
    std::string batch_id;
    {
        auto s=create(path);apply_category(s,"create-fees",make("fees","Fees"));apply_category(s,"create-food",make("food","Food"));
        save_rules(s,"rules",{rule_of("food-rule","Food",rules::Scope::transaction,rules::InputField::payee,"Synthetic Cafe",{"Do not rename splits",{},"food",false}),rule_of("fee-rule","Fee",rules::Scope::split,rules::InputField::memo,"^Fee",{{},{},"fees",false})},0);
        app::OperationContext capture;auto g=s.prepare_import_group("source",data,qif::DateOrder::dmy,{{0,1,{}}},capture);batch_id=g.accounts.front().batch;
        app::OperationContext prepare;s.resume_import_group(g.id,prepare);
        auto pending=s.missing_import_categories();check(pending.size()==1 && pending[0].names.size()==2);
        app::OperationContext preview_context;app::RulePreviewRequest query;query.account_scope=true;
        check(s.preview_pending_rules(query,preview_context).changed==0);
        rejects_text([&]{s.accept_all("early",true);},"Choose the imported categories");
        s.verify();
    }
    {
        auto s=ImportStore::open(path,password);check(s.missing_import_categories().size()==1);check(!s.cleanup_incomplete_imports());
        app::OperationContext resolve;s.resolve_import_categories("source",{"Imported Food"},resolve);
        check(s.missing_import_categories().empty());
        const auto split=*s.row({batch_id,0}).draft;check(split.payee=="Synthetic Cafe");
        check(split.transaction.allocations()[0].category=="Fees" && split.transaction.allocations()[1].category=="Food");
        check(s.row({batch_id,1}).draft->transaction.allocations()[0].category=="Imported Food");
        check(s.row({batch_id,2}).draft->transaction.allocations()[0].category.empty());
        check(s.evidence(batch_id).find("LDiscarded Name")!=std::string::npos);
        app::OperationContext retry;s.resolve_import_categories("source",{"Imported Food"},retry);
        app::OperationContext conflict;rejects_text([&]{s.resolve_import_categories("source",{},conflict);},"changed on retry");
        check(s.accept_all("accept",true).transactions==3);s.verify();
    }
    {auto s=ImportStore::open(path,password);s.verify();check(s.committed_entry({batch_id,0}).current.draft.transaction.allocations()[0].category=="Fees");}
}
void category_multi(){
    Directory dir("spentit-category-multi-XXXXXX");const auto path=dir.path/"doc";
    const std::string source="!Type:Cat\nNCatalog only\nE\n^\n!Account\nNSynthetic account\nTBank\n^\n!Type:Bank\nD1/8/2026\nT-1\nPOne\nLShared name\n^\n!Account\nNSecond\nTBank\n^\n!Type:Bank\nD2/8/2026\nT-2\nPTwo\nLshared NAME\n^\nD3/8/2026\nTbroken\nPRepair\nLClear name\n^\n";
    {auto s=create(path);s.create_account(2,"Second",cad,"2026-01-01");app::OperationContext c;s.prepare_import_group("multi",source,qif::DateOrder::dmy,{{0,1,{}},{1,2,{}}},c);app::OperationContext p;s.resume_import_group("multi",p);auto choices=s.missing_import_categories();check(choices.size()==1 && choices[0].names.size()==3);app::OperationContext resolve;s.resolve_import_categories("multi",{"Shared name","Catalog only"},resolve);check(s.row({"multi-account-0",0}).draft->transaction.allocations()[0].category=="Shared name");s.select_account(2);check(s.row({"multi-account-1",0}).draft->transaction.allocations()[0].category=="Shared name");
        auto broken=s.row({"multi-account-1",1});check(!broken.draft && broken.source.repairable);auto aa=broken.source.source_allocations;if(aa.empty()){Allocation a{"Clear name","",Money(-300,cad)};a.identity="source-0";a.source_ordinal=0;a.category_owner=FieldOwnership::source;a.memo_owner=FieldOwnership::source;aa.push_back(a);}else aa[0].amount=Money(-300,cad);
        imports::DraftRow repaired{broken.source.date,broken.source.payee,broken.source.memo,Transaction::repaired_import({},Money(-300,cad),aa),FieldOwnership::source,FieldOwnership::source};s.save(broken.identity,batch_revision(s,broken.identity.batch),repaired);check(s.row(broken.identity).draft->transaction.allocations()[0].category.empty());s.verify();
    }
    {auto s=ImportStore::open(path,password);s.select_account(2);s.verify();check(s.row({"multi-account-1",1}).draft->transaction.allocations()[0].category.empty());}
}
void category_fault_setup(const fs::path& path) {
    auto s=create(path);save_rules(s,"automatic-rule",{rule_of("auto","Automatic",rules::Scope::transaction,rules::InputField::payee,"Automatic",{{},{},"rule-default",false})},0);
    const std::string source="!Type:Bank\nD1/8/2026\nT-1\nPSynthetic\nLMissing category\n^\nD2/8/2026\nT-2\nPAutomatic\n^\n";
    app::OperationContext c;s.prepare_import_group("source",source,qif::DateOrder::dmy,{{0,1,{}}},c);
    app::OperationContext p;s.resume_import_group("source",p);
}
void category_fault_recover(const fs::path& path) {
    auto s=ImportStore::open(path,password);s.verify();
    const bool pending=!s.missing_import_categories().empty();
    const auto draft=*s.row({"source-account-0",0}).draft;
    check(draft.transaction.allocations()[0].category_binding.has_value()!=pending);
    check(s.row({"source-account-0",1}).draft->transaction.allocations()[0].category_binding.has_value()!=pending);
    app::OperationContext c;s.resolve_import_categories("source",{"Missing category"},c);s.verify();
    check(s.missing_import_categories().empty() && s.row({"source-account-0",0}).draft->transaction.allocations()[0].category_binding.has_value());
    check(s.row({"source-account-0",1}).draft->transaction.allocations()[0].category_rule->identity=="auto");
}
void category_faults() {
    fault_vfs::install();int positions=0;
    {Directory dir("spentit-category-probe-XXXXXX");category_fault_setup(dir.path/"doc");auto s=ImportStore::open(dir.path/"doc",password);app::OperationContext c;fault_vfs::arm(-1);s.resolve_import_categories("source",{"Missing category"},c);positions=fault_vfs::seen;fault_vfs::disarm();check(positions>0);}
    for(int position=1;position<=positions;++position) {
        Directory dir("spentit-category-fault-XXXXXX");const auto path=dir.path/"doc";category_fault_setup(path);
        {auto s=ImportStore::open(path,password);app::OperationContext c;fault_vfs::arm(position,position%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{s.resolve_import_categories("source",{"Missing category"},c);}catch(const ImportError&){}catch(const app::OperationOutcomeUnknown&){}check(fault_vfs::fired);fault_vfs::disarm();}
        category_fault_recover(path);no_leaks(dir.path);
    }
    for(const auto phase:{ImportPhase::before_commit,ImportPhase::after_commit}) {
        Directory dir("spentit-category-crash-XXXXXX");const auto path=dir.path/"doc";category_fault_setup(path);const auto child=fork();check(child>=0);
        if(child==0){auto s=ImportStore::open(path,password);s.observer([&](ImportPhase p){if(p==phase)_exit(72);});app::OperationContext c;s.resolve_import_categories("source",{"Missing category"},c);_exit(0);}
        int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==72);
        {auto s=ImportStore::open(path,password);check(s.missing_import_categories().empty()==(phase==ImportPhase::after_commit));}category_fault_recover(path);
    }
    {Directory dir("spentit-category-cancel-XXXXXX");const auto path=dir.path/"doc";category_fault_setup(path);auto s=ImportStore::open(path,password);app::OperationContext c;c.progress=[&](std::string_view phase){if(phase=="Assigning imported categories")c.request();};rejects_text([&]{s.resolve_import_categories("source",{"Missing category"},c);},"cancel");check(s.missing_import_categories().size()==1);s.verify();}
    std::cout<<"Import category resolution: "<<positions<<" write/sync faults, cancellation and both commit boundaries passed\n";
}
int main(int argc,char** argv){try{const std::string mode=argc>1?argv[1]:"";if(mode=="category_multi")category_multi();else if(mode=="category_faults")category_faults();else if(mode=="category_gate")category_gate();else if(mode=="account_faults")account_faults();else if(mode=="account_apply")account_apply();else if(mode=="match_preview")match_preview();else if(mode=="large_preview")large_preview(100000);else if(mode=="paged_preview")large_preview(5000,true);else if(mode=="definitions")definitions();else if(mode=="application")application();else if(mode=="schema")schema();else if(mode=="validation")validation();else if(mode=="backup")backup();else if(mode=="io")io();else if(mode=="crashes")crashes();else if(mode=="bulk")bulk_basics();else if(mode=="bulk_io")bulk_sweep(false);else if(mode=="bulk_crashes")bulk_sweep(true);else if(mode=="reapply")reapply_basics();else if(mode=="reapply_faults")reapply_faults();else return 2;std::cout<<"Rule store "<<mode<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
