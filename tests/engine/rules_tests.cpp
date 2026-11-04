#include <spentit/rules.hpp>
#include <iostream>
using namespace spentit::rules;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
template<class F> void fails(RegexFailure kind,F f) {try {f();}catch(const RegexError& e){require(e.kind()==kind,"wrong failure kind");return;}throw std::runtime_error("expected rejection");}
Rule rule(std::string id,Scope scope,InputField input,std::string pattern,Actions actions) {return {std::move(id),1,true,scope,input,std::move(pattern),std::move(actions)};}
Record wise() {
    Record r;r.source={"Tfl Travel UK","bank memo","",true,{{"Wise Charges for: card",""},{"CARD-123",""}}};
    r.payee={r.source.payee};r.memo={r.source.memo};
    // Reorder on purpose: source identity comes from the saved association.
    r.allocations={{1,{"CARD-123"},{{"",""}}},{0,{"Wise Charges for: card"},{{"",""}}}};
    return r;
}
int main() {try {
    std::vector<Rule> definitions{
        rule("parent",Scope::transaction,InputField::payee,"^Tfl Travel",{"TfL","parent memo","transport",false}),
        rule("fee",Scope::split,InputField::memo,"^Wise Charges for:",{{},"fee memo","fees",false}),
        rule("later",Scope::transaction,InputField::payee,"Tfl",{"Wrong","derived memo","fees",false})};
    RuleSet rules(definitions);auto original=wise();auto out=rules.evaluate({original},{"transport","fees"}).front();
    require(out.record.payee.value==original.payee.value&&out.record.memo.value==original.memo.value,"split parent text unchanged");
    require(out.record.allocations[0].category.value.identity=="transport"&&out.record.allocations[1].category.value.identity=="fees","split first then transaction fallback despite display order");
    require(out.record.allocations[1].memo.value=="fee memo","split memo action");
    require(out.record.source.payee==original.source.payee&&original.payee.value=="Tfl Travel UK","source and caller immutable");
    require(out.record.allocations.size()==original.allocations.size(),"structure retained");
    auto manual=original;manual.payee={"",Ownership::manual,{}};manual.allocations[0].category={{"old-archived",""},Ownership::manual,{}};
    auto protected_result=rules.evaluate({manual},{"transport","fees"}).front();
    require(protected_result.record.payee.value.empty()&&protected_result.record.allocations[0].category.value.identity=="old-archived","manual blank/archive protected");
    fails(RegexFailure::invalid_input,[&]{rules.evaluate({manual},{"transport","fees"},true);});
    RuleSet none({});auto retained=none.evaluate({out.record},{}).front();require(retained.record.allocations[0].category.value.identity=="transport","removing rules preserves prior assignments");
    auto created=original;created.allocations.push_back({{}, {"CARD-new",Ownership::manual,{}},{{"manual",""},Ownership::manual,{}}});
    require(rules.evaluate({created},{"transport","fees"}).front().record.allocations.back().category.value.identity=="manual","categorized new split protected");
    RuleSet selected(definitions,"parent");auto preview=selected.evaluate({original},{"transport","fees"}).front();
    require(preview.match.raw && preview.match.eligible && preview.record.allocations[0].category.value.identity=="transport" && preview.record.allocations[1].category.value.identity.empty() && preview.record.allocations[1].memo.value==original.allocations[1].memo.value,"selected rule respects earlier phase without applying it");
    RuleSet blocked(definitions,"later");auto excluded=blocked.evaluate({original},{"transport","fees"}).front();
    require(excluded.match.raw && excluded.match.earlier && !excluded.match.eligible && excluded.explanations.empty(),"earlier exclusion count and no contextual side effects");
    auto forged=original;forged.allocations[1].source_ordinal=1;fails(RegexFailure::invalid_input,[&]{rules.evaluate({forged},{"transport","fees"});});
    fails(RegexFailure::invalid_input,[&]{rules.evaluate({original},{"transport"});});
    fails(RegexFailure::cancelled,[&]{rules.evaluate({original},{"transport","fees"},false,[]{return true;});});
    fails(RegexFailure::internal,[&]{rules.evaluate({original},{"transport","fees"},false,[]() -> bool {throw std::runtime_error("callback");});});
    auto invalid_utf=original;invalid_utf.source.memo=std::string("\xff",1);fails(RegexFailure::invalid_input,[&]{none.evaluate({invalid_utf},{});});
    auto huge=original;huge.allocations.clear();huge.source.allocations.clear();
    for(std::size_t i=0;i<4096;++i){huge.allocations.push_back({i,{""},{{"",""}}});huge.source.allocations.push_back({"",""});}
    fails(RegexFailure::resource_limit,[&]{none.evaluate(std::vector<Record>(5,huge),{});});
    unsigned validation_checks=0;fails(RegexFailure::cancelled,[&]{none.evaluate({huge},{},false,[&]{return ++validation_checks>10;});});
    auto invalid=definitions;invalid[0].expression="(";fails(RegexFailure::invalid_pattern,[&]{RuleSet bad(invalid);});
    fails(RegexFailure::invalid_input,[&]{RuleSet bad({rule("bad",Scope::split,InputField::memo,".*",{{},{},{},true})});});
    fails(RegexFailure::invalid_input,[&]{RuleSet bad({rule("bad",Scope::transaction,InputField::payee,".*",{"name",{},{},false})});});
    Record single;single.source={"Tfl Travel","","old",false,{{"","old"}}};single.payee={single.source.payee};single.memo={""};single.allocations={{0,{""},{{"","old"}}}};
    require(rules.evaluate({single},{"transport","fees"}).front().record.allocations[0].category.value.unmapped=="old","imported category protected");
    single.allocations[0].category={{"",""},Ownership::manual,{}};
    single.payee={"my payee",Ownership::manual,{}};
    auto rerun=rules.evaluate({single},{"transport","fees"}).front();
    require(rerun.record.allocations[0].category.value.identity=="transport" && rerun.record.payee.value=="my payee" && rerun.record.memo.value=="parent memo","deliberate clearing permits category while preserving manual payee");
    auto again=rules.evaluate({rerun.record},{"transport","fees"}).front();
    require(again.match.categorized && !again.match.eligible && again.explanations.empty(),"categorized rule-owned row untouched");
    RuleSet fallback({definitions.front()});auto both=fallback.evaluate({original},{"transport"}).front();
    require(both.record.allocations[0].category.value.identity=="transport" && both.record.allocations[1].category.value.identity=="transport","fallback fills all remaining splits");
    std::cout<<"Rule phases, source associations, provenance, manual protection and explanations passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
