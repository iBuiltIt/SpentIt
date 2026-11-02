#include <spentit/rule_commands.hpp>
#include <spentit/rule_preview.hpp>
#include <iostream>
using namespace spentit;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    require(app::quoted_field_pattern("")=="^$","empty whole field");
    require(app::quoted_field_pattern("Tfl Travel UK")=="^Tfl Travel UK$","plain source needs no quoting");
    require(app::quoted_field_pattern("AMZN Mktp CA*TEST")==R"(^\QAMZN Mktp CA*TEST\E$)","literal star requires quoting");
    require(app::quoted_field_pattern(R"(a\Eb)")==R"(^\Qa\E\\E\Qb\E$)","quote closes around literal quote terminator");
    for(const std::string value:{"", "STARBUCKS", "A/B, C-D: 123", "Café", "a.b", "a^b$c", "a|b", "a?b*c+d", "(a)[b]{c}", R"(a\Eb)", R"(a\E)", R"(a\\E)", R"(a\E\Eb)"}) {
        rules::Pattern literal(app::quoted_field_pattern(value));
        require(literal.match(value).has_value(),"generated pattern matches the literal source");
        require(!literal.match("prefix"+value) && !literal.match(value+"suffix"),"generated pattern retains its anchors");
        require(literal.capture_count()==0,"generated pattern has no captures");
    }
    const Currency cad("CAD");
    auto parsed=qif::parse("!Type:Bank\nD01/01/2026\nT-29.31\nPTfl Travel UK\nEWise Charges for: card\n$-0.14\nECARD-123\n$-29.17\n^\n",qif::DateOrder::dmy,cad);
    require(parsed.valid(),"synthetic fixture");auto source=parsed.records.front();
    imports::DraftRow draft{source.date,source.payee,source.memo,*source.transaction,FieldOwnership::source,FieldOwnership::source};
    app::CategoryCatalog catalog{3,{{"travel","Travel","",CategoryKind::leaf,0,false,false},{"transport","Travel: Transport","",CategoryKind::leaf,0,false,false},{"fees","Service Fees","",CategoryKind::leaf,0,false,false}}};
    rules::RuleSet rules({{"parent",1,true,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"TfL",{},"transport",false}},{"fee",1,true,rules::Scope::split,rules::InputField::memo,"^Wise",{{},{},"fees",false}},{"trip",1,true,rules::Scope::split,rules::InputField::memo,"^CARD-",{{},{},"transport",false}}});
    auto out=app::preview_rules(rules,catalog,{{{"batch",0},source,draft,{}}}).front();
    require(out.after.payee==source.payee&&out.after.date==draft.date,"display only");
    const auto& allocations=out.after.transaction.allocations();
    require(allocations[0].category_binding->identity=="fees"&&allocations[1].category_binding->identity=="transport","mapped stable IDs");
    require(allocations[0].amount.units()==-14&&allocations[1].amount.units()==-2917&&out.after.transaction.effective_total()==draft.transaction.effective_total(),"exact money unchanged");
    require(allocations[1].identity==draft.transaction.allocations()[1].identity&&allocations[1].source_ordinal==1,"source identity retained");
    require(allocations[1].category=="Travel: Transport"&&out.provenance.size()==2,"labels and provenance");
    require(out.after.payee_owner==FieldOwnership::source && !out.after.payee_rule && !out.after.memo_rule,"payee mark");
    require(allocations[0].category_rule==RuleMark{"fee",1} && allocations[1].category_rule==RuleMark{"trip",1} && !allocations[0].memo_rule && !allocations[1].memo_rule,"allocation marks");
    auto again=app::preview_rules(rules,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    require(again.after.payee_rule==out.after.payee_rule && again.after.memo_rule==out.after.memo_rule,"transaction mark round-trip");
    require(again.after.transaction.allocations()[0].category_rule==allocations[0].category_rule && again.after.transaction.allocations()[1].category_rule==allocations[1].category_rule,"allocation mark round-trip");
    require(again.after.transaction.allocations()[0].amount.units()==-14 && again.after.transaction.allocations()[1].amount.units()==-2917 && again.after.transaction.effective_total()==draft.transaction.effective_total(),"round-trip money unchanged");
    rules::RuleSet isolated({{"test",0,true,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"Test payee",{},"transport",false}}});
    auto isolated_out=app::preview_rules(isolated,catalog,{{{"batch",0},source,out.after,out.provenance}},false,{},true).front();
    require(isolated_out.after.payee==source.payee,"isolated rule runs");
    require(isolated_out.after.transaction.allocations()[0].category_rule==allocations[0].category_rule && isolated_out.after.transaction.allocations()[1].category_rule==allocations[1].category_rule,"isolated preview preserves unrelated rule assignments");
    require(app::rule_field_diffs(out.after,isolated_out).empty(),"isolated diff only contains tested action");
    auto renamed_catalog=catalog;renamed_catalog.revision=4;renamed_catalog.categories[2].name="Renamed fees";
    auto renamed_test=app::preview_rules(isolated,renamed_catalog,{{{"batch",0},source,out.after,out.provenance}},false,{},true).front();
    require(renamed_test.after.transaction.allocations()[0].category==allocations[0].category && renamed_test.after.transaction.allocations()[0].category_binding==allocations[0].category_binding,"untouched category keeps exact label and binding snapshot");
    rules::RuleSet disabled({{"test",0,false,rules::Scope::transaction,rules::InputField::payee,"^Tfl",{"Test payee",{},"transport",false}}});
    auto disabled_out=app::preview_rules(disabled,catalog,{{{"batch",0},source,out.after,out.provenance}},false,{},true).front();
    require(app::rule_field_diffs(out.after,disabled_out).empty() && disabled_out.explanations.empty(),"disabled isolated rule leaves everything unchanged");
    rules::RuleSet empty({});auto restored=app::preview_rules(empty,catalog,{{{"batch",0},source,out.after,out.provenance}}).front();
    require(app::rule_field_diffs(out.after,restored).empty(),"removing rules preserves all assignments");
    auto field_diff=[&](const std::vector<app::RuleFieldDiff>& diffs,app::RuleField field,const std::string& allocation)->const app::RuleFieldDiff*{
        const app::RuleFieldDiff* match=nullptr;
        for(const auto& diff:diffs)if(diff.field==field && diff.allocation==allocation){require(match==nullptr,"one diff per field");match=&diff;}
        return match;
    };
    auto blank=draft;blank.payee="";blank.payee_owner=FieldOwnership::manual;
    auto kept=app::preview_rules(rules,catalog,{{{"batch",0},source,blank,{}}}).front();
    require(kept.after.payee.empty() && kept.after.payee_owner==FieldOwnership::manual && !kept.after.payee_rule,"manual blank retained");
    auto kept_diffs=app::rule_field_diffs(blank,kept);
    require(field_diff(kept_diffs,app::RuleField::payee,"")==nullptr,"manual blank is neither removed nor assigned");
    bool force_rejected=false;try{app::preview_rules(rules,catalog,{{{"batch",0},source,blank,{}}},true);}catch(const rules::RegexError&){force_rejected=true;}require(force_rejected,"force override rejected");
    auto reordered=out.after;auto parts=reordered.transaction.allocations();
    require(parts[0].amount.units()==-14 && parts[1].amount.units()==-2917,"fee then travel amounts");
    const auto fee_id=parts[0].identity,travel_id=parts[1].identity,fee_category=parts[0].category;
    auto flipped=parts;flipped[0]=parts[1];flipped[1]=parts[0];flipped[1].category=fee_category+" revised";
    reordered.transaction=reordered.transaction.corrected(reordered.transaction.effective_total(),flipped);
    require(reordered.transaction.allocations()[0].identity==travel_id && reordered.transaction.allocations()[0].amount.units()==-2917,"reordered travel amount");
    require(reordered.transaction.allocations()[1].identity==fee_id && reordered.transaction.allocations()[1].amount.units()==-14,"reordered fee amount");
    auto reordered_diffs=app::rule_field_diffs(out.after,{out.identity,reordered,{},{}});
    auto category_diff=field_diff(reordered_diffs,app::RuleField::allocation_category,fee_id);
    require(reordered_diffs.size()==1 && category_diff && category_diff->allocation==fee_id && category_diff->before_text==fee_category && category_diff->after_text==fee_category+" revised" && category_diff->before_mark==RuleMark{"fee",1} && category_diff->after_mark==RuleMark{"fee",1},"reordered category pairs by identity");
    require(field_diff(reordered_diffs,app::RuleField::allocation_category,travel_id)==nullptr,"unchanged travel category");
    auto reject_identities=[&](std::vector<Allocation> changed){auto row=out.after;row.transaction=row.transaction.corrected(row.transaction.effective_total(),std::move(changed));bool threw=false;try{app::rule_field_diffs(out.after,{out.identity,row,{},{}});}catch(const std::invalid_argument& error){threw=true;require(std::string(error.what())=="Allocation identities do not line up","Allocation identities do not line up");}require(threw,"Allocation identities do not line up");};
    auto mismatched=parts;mismatched[0].identity="not-the-fee";reject_identities(mismatched);
    auto blank_id=parts;blank_id[0].identity.clear();reject_identities(blank_id);
    auto duplicated=parts;duplicated[1].identity=duplicated[0].identity;reject_identities(duplicated);
    auto manual=out.after;manual.payee="";manual.payee_owner=FieldOwnership::manual;manual.payee_rule.reset();auto provenance=out.provenance;
    auto protected_result=app::preview_rules(rules,catalog,{{{"batch",0},source,manual,provenance}}).front();require(protected_result.after.payee.empty(),"manual blank preserved");
    bool rejected=false;try{app::preview_rules(rules,catalog,{{{"batch",0},source,out.after,{}}});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"missing provenance rejected");
    for(const auto raw:{"D1/1/2026\nT-10\nPTfl\nLVisa\n^\n", "D1/1/2026\nT-10\nPTfl\nL[Visa]\n^\n", "D1/1/2026\nT-10\nPTfl\nS[Visa]\nEWise transfer\n$-9\nSFee\nEWise fee\n$-1\n^\n"}) {
        const auto parsed_transfer=qif::parse(raw,qif::DateOrder::dmy,cad);require(parsed_transfer.valid(),"transfer parses");
        const auto& src=parsed_transfer.records.front();
        imports::DraftRow transfer{src.date,src.payee,src.memo,*src.transaction,FieldOwnership::source,FieldOwnership::source};
        for(bool replace:{false}) {
            const auto result=app::preview_rules(rules,catalog,{{{"transfer",0},src,transfer,{}, {"source-0"}}},replace).front();
            const auto& first=result.after.transaction.allocations().front();
            require(first.category==src.source_allocation_fields.front().category && !first.category_binding && !first.category_rule,"rules preserve transfer intent even with replacement");
            require(!result.match.eligible,"categorized transfers excluded by category gate");
        }
    }
    std::cout<<"Rule preview preserves exact amounts, identities, source and retained field provenance\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
