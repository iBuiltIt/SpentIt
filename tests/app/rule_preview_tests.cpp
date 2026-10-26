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
    rules::RuleSet empty({});auto restored=app::preview_rules(empty,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    require(restored.after.payee==source.payee&&restored.after.transaction.allocations()[0].category=="Unmapped Fee"&&restored.provenance.empty(),"recompute removed rule effects");
    auto manual=out.after;manual.payee="";manual.payee_owner=FieldOwnership::manual;auto provenance=out.provenance;provenance.erase(provenance.begin());
    auto protected_result=app::preview_rules(rules,catalog,{{{"batch",0},source,manual,provenance}}).front();require(protected_result.after.payee.empty(),"manual blank preserved");
    bool rejected=false;try{app::preview_rules(rules,catalog,{{{"batch",0},source,out.after,{}}});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"missing provenance rejected");
    std::cout<<"Rule preview preserves exact amounts, identities, source and retained field provenance\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
