#include <spentit/rule_regex.hpp>
#include <iostream>
using namespace spentit::rules;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
template<class F> void fails(RegexFailure kind,F f) {try {f();}catch(const RegexError& e){require(e.kind()==kind,"wrong failure kind");return;}throw std::runtime_error("expected rejection");}
int main() {try {
    Pattern payee("^tfl (travel)(?: (uk))?$");
    auto match=payee.match("TfL Travel");require(match.has_value(),"case-insensitive match");
    require(expand_template("${1} $$ ${2}",*match)=="Travel $ ","optional capture/template");
    require(!payee.match("other TfL Travel"),"anchors");
    Pattern search("café");require(search.match("CAFÉ card").has_value(),"Unicode caseless search");
    Pattern exact("(?-i:TfL)");require(!exact.match("tfl"),"explicit case override");
    Pattern blank("^$");require(blank.match("").has_value(),"empty source");
    Pattern unicode("(\\p{L}+)");require(unicode.match("東京").has_value(),"Unicode properties");
    fails(RegexFailure::invalid_pattern,[]{Pattern p("(");});
    fails(RegexFailure::invalid_pattern,[]{Pattern p("\\C");});
    fails(RegexFailure::invalid_pattern,[]{Pattern p(std::string(4097,'x'));});
    fails(RegexFailure::invalid_input,[&]{payee.match(std::string("\xff",1));});
    fails(RegexFailure::invalid_input,[&]{payee.match(std::string("a\0b",3));});
    fails(RegexFailure::resource_limit,[&]{payee.match(std::string(65537,'x'));});
    fails(RegexFailure::cancelled,[&]{payee.match("TfL Travel",[]{return true;});});
    fails(RegexFailure::internal,[&]{payee.match("TfL Travel",[]{throw std::runtime_error("check failed");return false;});});
    fails(RegexFailure::resource_limit,[]{Pattern p(std::string(65,'(')+"x"+std::string(65,')'));});
    fails(RegexFailure::resource_limit,[]{std::string expression;for(unsigned i=0;i<33;++i)expression+="(x)";Pattern p(expression);});
    unsigned checks=0;Pattern slow("(*NO_START_OPT)(a+)+$");
    fails(RegexFailure::cancelled,[&]{slow.match(std::string(30,'a')+"!",[&]{return ++checks>10;});});
    fails(RegexFailure::resource_limit,[&]{slow.match(std::string(30,'a')+"!");});
    fails(RegexFailure::invalid_template,[&]{expand_template("${3}",*match);});
    fails(RegexFailure::invalid_template,[]{validate_template("$1",1);});
    fails(RegexFailure::invalid_template,[]{validate_template("${}",1);});
    RegexMatch large{{std::string(40000,'x')}};
    fails(RegexFailure::resource_limit,[&]{expand_template("${0}${0}",large);});
    RegexMatch boundary{{std::string(65536,'x')}};require(expand_template("${0}",boundary).size()==65536,"exact output bound");
    std::cout<<"Regex semantics, capture templates, cancellation and resource bounds passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
