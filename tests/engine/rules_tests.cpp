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
        rule("parent",Scope::transaction,InputField::payee,"^Tfl Travel",{"TfL",{},"transport",false}),
        rule("fee",Scope::split,InputField::memo,"^Wise Charges for:",{{},{},"fees",false}),
        rule("inherit",Scope::split,InputField::memo,"^CARD-",{{},{},{},true}),
        rule("later",Scope::transaction,InputField::payee,"Tfl",{"Wrong","derived memo",{},false})};
    RuleSet rules(definitions);auto original=wise();auto out=rules.evaluate({original},{"transport","fees"}).front();
    require(out.record.payee.value=="TfL"&&out.record.memo.value=="derived memo","per-field precedence");
    require(out.record.allocations[0].category.value.identity=="transport"&&out.record.allocations[1].category.value.identity=="fees","source split association and inheritance");
    require(out.record.source.payee==original.source.payee&&original.payee.value=="Tfl Travel UK","source and caller immutable");
    require(out.record.allocations.size()==original.allocations.size(),"structure retained");
    bool competing=false;for(const auto& e:out.explanations)competing|=e.effect==Effect::competing;require(competing,"competing explanation");
    auto manual=original;manual.payee={"",Ownership::manual,{}};manual.allocations[0].category={{"old-archived",""},Ownership::manual,{}};
    auto protected_result=rules.evaluate({manual},{"transport","fees"}).front();
    require(protected_result.record.payee.value.empty()&&protected_result.record.allocations[0].category.value.identity=="old-archived","manual blank/archive protected");
    auto replaced=rules.evaluate({manual},{"transport","fees"},true).front();require(replaced.record.payee.value=="TfL"&&replaced.record.payee.ownership==Ownership::rule,"explicit replacement provenance");
    RuleSet none({});auto removed=none.evaluate({out.record},{}).front();require(removed.record.payee.value==original.source.payee&&removed.record.allocations[0].category.value.identity.empty(),"obsolete rule effects removed");
    auto untouched=none.evaluate({manual},{},true).front();require(untouched.record.payee==manual.payee,"replace option must not erase unmatched manual field");
    auto created=original;created.allocations.push_back({{}, {"CARD-new",Ownership::manual,{}},{{"manual",""},Ownership::manual,{}}});
    require(rules.evaluate({created},{"transport","fees"},true).front().record.allocations.back().category.value.identity=="manual","new split excluded");
    auto forged=original;forged.allocations[1].source_ordinal=1;fails(RegexFailure::invalid_input,[&]{rules.evaluate({forged},{"transport","fees"});});
    fails(RegexFailure::invalid_input,[&]{rules.evaluate({original},{"transport"});});
    fails(RegexFailure::cancelled,[&]{rules.evaluate({original},{"transport","fees"},false,[]{return true;});});
    fails(RegexFailure::internal,[&]{rules.evaluate({original},{"transport","fees"},false,[]{throw std::runtime_error("callback");return false;});});
    auto invalid_utf=original;invalid_utf.source.memo=std::string("\xff",1);fails(RegexFailure::invalid_input,[&]{none.evaluate({invalid_utf},{});});
    auto huge=original;huge.allocations.clear();huge.source.allocations.clear();
    for(std::size_t i=0;i<4096;++i){huge.allocations.push_back({i,{""},{{"",""}}});huge.source.allocations.push_back({"",""});}
    fails(RegexFailure::resource_limit,[&]{none.evaluate(std::vector<Record>(5,huge),{});});
    unsigned validation_checks=0;fails(RegexFailure::cancelled,[&]{none.evaluate({huge},{},false,[&]{return ++validation_checks>10;});});
    auto invalid=definitions;invalid[0].expression="(";fails(RegexFailure::invalid_pattern,[&]{RuleSet bad(invalid);});
    RuleSet inherit_only({definitions[2]});auto missing=inherit_only.evaluate({original},{}).front();require(missing.explanations.front().effect==Effect::missing_parent,"missing parent explained");
    Record single;single.source={"Tfl Travel","","old",false,{{"","old"}}};single.payee={single.source.payee};single.memo={""};single.allocations={{0,{""},{{"","old"}}}};
    require(rules.evaluate({single},{"transport","fees"}).front().record.allocations[0].category.value.identity=="transport","single allocation transaction category");
    // A derived display payee never becomes input to another rule.
    RuleSet no_recursive({definitions[0],rule("recursive",Scope::transaction,InputField::payee,"^TfL$",{{},"bad",{},false})});
    require(no_recursive.evaluate({original},{"transport"}).front().record.memo.value=="bank memo","immutable matching only");
    std::cout<<"Rule phases, source associations, provenance, manual protection and explanations passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
