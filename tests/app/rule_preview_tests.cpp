#include <spentit/rule_commands.hpp>
#include <spentit/rule_preview.hpp>
#include <iostream>
using namespace spentit;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    const Currency cad("CAD");
    auto parsed=qif::parse("!Type:Bank\nD01/01/2026\nT-29.31\nPTfl Travel UK\nSUnmapped Fee\nEWise Charges for: card\n$-0.14\nSUnmapped Travel\nECARD-123\n$-29.17\n^\n",qif::DateOrder::dmy,cad);
    require(parsed.valid(),"synthetic fixture");auto source=parsed.records.front();
    imports::DraftRow draft{source.date,source.payee,source.memo,*source.transaction,FieldOwnership::source,FieldOwnership::source};
    app::CategoryCatalog catalog{3,{{"travel","Travel","",CategoryKind::group,0,false,false},{"transport","Transport","travel",CategoryKind::leaf,0,false,false},{"fees","Service Fees","",CategoryKind::leaf,0,false,false}}};
    rules::RuleSet rules({{"parent",1,true,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"TfL",{},"transport",false}},{"fee",1,true,rules::Scope::split,rules::InputField::memo,"^Wise",{{},{},"fees",false}},{"trip",1,true,rules::Scope::split,rules::InputField::memo,"^CARD-",{{},{},{},true}}});
    auto out=app::preview_rules(rules,catalog,{{{"batch",0},source,draft,{}}}).front();
    require(out.after.payee=="TfL"&&out.after.date==draft.date,"display only");
    const auto& allocations=out.after.transaction.allocations();
    require(allocations[0].category_binding->identity=="fees"&&allocations[1].category_binding->identity=="transport","mapped stable IDs");
    require(allocations[0].amount.units()==-14&&allocations[1].amount.units()==-2917&&out.after.transaction.effective_total()==draft.transaction.effective_total(),"exact money unchanged");
    require(allocations[1].identity==draft.transaction.allocations()[1].identity&&allocations[1].source_ordinal==1,"source identity retained");
    require(allocations[1].category=="Travel: Transport"&&out.provenance.size()==3,"labels and provenance");
    require(out.after.payee_owner==FieldOwnership::rule && out.after.payee_rule==RuleMark{"parent",1} && !out.after.memo_rule,"payee mark");
    require(allocations[0].category_rule==RuleMark{"fee",1} && allocations[1].category_rule==RuleMark{"trip",1} && !allocations[0].memo_rule && !allocations[1].memo_rule,"allocation marks");
    auto again=app::preview_rules(rules,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    require(again.after.payee_rule==out.after.payee_rule && again.after.memo_rule==out.after.memo_rule,"transaction mark round-trip");
    require(again.after.transaction.allocations()[0].category_rule==allocations[0].category_rule && again.after.transaction.allocations()[1].category_rule==allocations[1].category_rule,"allocation mark round-trip");
    require(again.after.transaction.allocations()[0].amount.units()==-14 && again.after.transaction.allocations()[1].amount.units()==-2917 && again.after.transaction.effective_total()==draft.transaction.effective_total(),"round-trip money unchanged");
    rules::RuleSet empty({});auto restored=app::preview_rules(empty,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    require(restored.after.payee==source.payee&&restored.after.transaction.allocations()[0].category=="Unmapped Fee"&&restored.provenance.empty(),"recompute removed rule effects");
    require(!restored.after.payee_rule && !restored.after.transaction.allocations()[0].category_rule,"recompute clears marks");
    auto field_diff=[&](const std::vector<app::RuleFieldDiff>& diffs,app::RuleField field,const std::string& allocation)->const app::RuleFieldDiff*{
        const app::RuleFieldDiff* match=nullptr;
        for(const auto& diff:diffs)if(diff.field==field && diff.allocation==allocation){require(match==nullptr,"one diff per field");match=&diff;}
        return match;
    };
    rules::RuleSet echo({{"echo",1,true,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"Tfl Travel UK",{},{},false}}});
    auto echoed=app::preview_rules(echo,catalog,{{{"batch",0},source,draft,{}}}).front();
    require(echoed.after.payee==source.payee && echoed.after.payee_rule==RuleMark{"echo",1},"rule text equals source");
    auto cleared=app::preview_rules(empty,catalog,{{{"batch",0},source,echoed.after,echoed.provenance}}).front();
    auto obsolete=app::rule_field_diffs(echoed.after,cleared);
    auto removal=field_diff(obsolete,app::RuleField::payee,"");
    require(obsolete.size()==1 && removal && removal->removed && !removal->assigned && removal->before_text==source.payee && removal->after_text==source.payee,"obsolete removal when the text equals the source");
    auto blank=draft;blank.payee="";blank.payee_owner=FieldOwnership::manual;
    auto kept=app::preview_rules(rules,catalog,{{{"batch",0},source,blank,{}}}).front();
    require(kept.after.payee.empty() && kept.after.payee_owner==FieldOwnership::manual && !kept.after.payee_rule,"manual blank retained");
    auto kept_diffs=app::rule_field_diffs(blank,kept);
    require(field_diff(kept_diffs,app::RuleField::payee,"")==nullptr,"manual blank is neither removed nor assigned");
    auto replaced=app::preview_rules(rules,catalog,{{{"batch",0},source,blank,{}}},true).front();
    auto replace_diffs=app::rule_field_diffs(blank,replaced);
    auto replace_diff=field_diff(replace_diffs,app::RuleField::payee,"");
    require(replaced.after.payee=="TfL" && replace_diff && replace_diff->assigned && !replace_diff->removed,"explicit replace is assigned, not removed");
    rules::RuleSet bumped({{"parent",2,true,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"TfL",{},"transport",false}},{"fee",1,true,rules::Scope::split,rules::InputField::memo,"^Wise",{{},{},"fees",false}},{"trip",1,true,rules::Scope::split,rules::InputField::memo,"^CARD-",{{},{},{},true}}});
    auto bumped_out=app::preview_rules(bumped,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    auto changes=app::rule_field_diffs(out.after,bumped_out);
    require(changes.size()==1 && changes[0].field==app::RuleField::payee && changes[0].removed && changes[0].assigned && changes[0].before_text==changes[0].after_text && changes[0].before_mark==RuleMark{"parent",1} && changes[0].after_mark==RuleMark{"parent",2},"contribution change is removal and assignment");
    auto manual=out.after;manual.payee="";manual.payee_owner=FieldOwnership::manual;manual.payee_rule.reset();auto provenance=out.provenance;provenance.erase(provenance.begin());
    auto protected_result=app::preview_rules(rules,catalog,{{{"batch",0},source,manual,provenance}}).front();require(protected_result.after.payee.empty(),"manual blank preserved");
    bool rejected=false;try{app::preview_rules(rules,catalog,{{{"batch",0},source,out.after,{}}});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"missing provenance rejected");
    std::cout<<"Rule preview preserves exact amounts, identities, source and retained field provenance\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
