#include <spentit/qif.hpp>
#include <spentit/rules.hpp>
#include <algorithm>

namespace spentit::rules {
namespace {
[[noreturn]] void invalid(const char* message) { throw RegexError(RegexFailure::invalid_input,message); }
struct Compiled { Rule rule; Pattern pattern; };
template<class T> void reset(Field<T>& field,T original) {
    if(field.ownership!=Ownership::manual) field={std::move(original),Ownership::source,std::nullopt};
}
template<class T> void validate(const Field<T>& field) {
    if(field.ownership!=Ownership::source&&field.ownership!=Ownership::manual&&field.ownership!=Ownership::rule)invalid("Unknown field ownership");
    if((field.ownership==Ownership::rule)!=field.contribution.has_value())invalid("Field provenance does not match its ownership");
    if(field.contribution&&(field.contribution->rule.empty()||field.contribution->rule.size()>256||field.contribution->version<0))invalid("Invalid field contribution");
}
std::size_t size(const std::string& value) {return value.size();}
std::size_t size(const CategoryValue& value) {return value.identity.size()+value.unmapped.size();}
}
struct RuleSet::Impl { std::vector<Compiled> rules; };
RuleSet::RuleSet(std::vector<Rule> rules):impl_(std::make_unique<Impl>()) {
    if(rules.size()>EvaluationLimits::rules)throw RegexError(RegexFailure::resource_limit,"Too many rules in the evaluation set");
    std::set<std::string> identities;
    std::size_t compiled_bytes=0;
    for(auto& rule:rules) {
        if(rule.identity.empty()||rule.identity.size()>256||rule.version<0||!identities.insert(rule.identity).second)invalid("Rules require unique stable identities and nonnegative versions");
        validate_source_text(rule.identity);
        const auto& a=rule.actions;
        if((rule.scope!=Scope::transaction&&rule.scope!=Scope::split)||(rule.input!=InputField::payee&&rule.input!=InputField::memo&&rule.input!=InputField::category))invalid("Unknown rule scope/input field");
        if(!a.payee_template&&!a.memo_template&&!a.category&&!a.inherit_parent)invalid("Rule has no actions");
        if(a.inherit_parent&&(a.category||rule.scope!=Scope::split))invalid("Parent inheritance is a distinct split action");
        if(rule.scope==Scope::split&&a.payee_template)invalid("Split rules cannot change the transaction payee");
        if(a.category&&(a.category->empty()||a.category->size()>256))invalid("Rule category must identify an assignable category");
        if(a.category)validate_source_text(*a.category);
        Pattern pattern(rule.expression);
        if(pattern.compiled_bytes()>EvaluationLimits::input_bytes-compiled_bytes)throw RegexError(RegexFailure::resource_limit,"Compiled rule set exceeds memory limit");
        compiled_bytes+=pattern.compiled_bytes();
        if(a.payee_template)validate_template(*a.payee_template,pattern.capture_count());
        if(a.memo_template)validate_template(*a.memo_template,pattern.capture_count());
        impl_->rules.push_back({std::move(rule),std::move(pattern)});
    }
}
RuleSet::~RuleSet()=default;
RuleSet::RuleSet(RuleSet&&) noexcept=default;
RuleSet& RuleSet::operator=(RuleSet&&) noexcept=default;
std::vector<Result> RuleSet::evaluate(const std::vector<Record>& records,const std::set<std::string>& active,bool replace,const std::function<bool()>& cancelled) const {
    if(records.size()>EvaluationLimits::records)throw RegexError(RegexFailure::resource_limit,"Too many records in one rule preview");
    std::size_t bytes=0,comparisons=0,callouts=0,explanations=0,generated_bytes=0,allocation_count=0,source_count=0;
    auto check_cancel=[&] {
        try {return cancelled&&cancelled();}catch(...){throw RegexError(RegexFailure::internal,"Rule cancellation check failed");}
    };
    auto checkpoint=[&] {if(check_cancel())throw RegexError(RegexFailure::cancelled,"Rule evaluation cancelled");};
    auto count=[&](std::string_view value) {validate_source_text(value);if(value.size()>EvaluationLimits::input_bytes-bytes)throw RegexError(RegexFailure::resource_limit,"Rule input scope exceeds limit");bytes+=value.size();};
    auto field=[&]<class T>(const Field<T>& value){validate(value);if(value.contribution)count(value.contribution->rule);};
    for(const auto& c:impl_->rules)if(c.rule.enabled&&c.rule.actions.category&&!active.contains(*c.rule.actions.category))invalid("Enabled rule targets an unavailable category");
    // Validate the complete chosen scope before evaluation. No partial preview is returned.
    for(const auto& record:records) {
        checkpoint();
        if(record.allocations.empty()||record.allocations.size()>EvaluationLimits::allocations||record.source.allocations.empty()||record.source.allocations.size()>EvaluationLimits::allocations||(!record.source.split&&record.source.allocations.size()!=1))invalid("Invalid source/allocation association shape");
        if(record.allocations.size()>EvaluationLimits::scope_allocations-allocation_count||record.source.allocations.size()>EvaluationLimits::scope_allocations-source_count)throw RegexError(RegexFailure::resource_limit,"Rule scope allocation count exceeds limit");
        allocation_count+=record.allocations.size();source_count+=record.source.allocations.size();
        count(record.source.payee);count(record.source.memo);count(record.source.category);
        field(record.payee);field(record.memo);count(record.payee.value);count(record.memo.value);
        std::set<std::size_t> associations;
        for(const auto& s:record.source.allocations){checkpoint();count(s.memo);count(s.category);}
        for(const auto& a:record.allocations) {
            checkpoint();field(a.memo);field(a.category);count(a.memo.value);count(a.category.value.identity);count(a.category.value.unmapped);
            if(!a.category.value.identity.empty()&&!a.category.value.unmapped.empty())invalid("Classification cannot be both mapped and unmapped");
            if(a.source_ordinal&&(*a.source_ordinal>=record.source.allocations.size()||!associations.insert(*a.source_ordinal).second))invalid("Invalid or reused retained source-allocation association");
            if(!a.source_ordinal&&(a.memo.ownership!=Ownership::manual||a.category.ownership!=Ownership::manual))invalid("New allocations require manual field provenance");
        }
    }
    bool exhausted=false;
    auto stop=[&] {if(++callouts>EvaluationLimits::callouts){exhausted=true;return true;}return check_cancel();};
    auto match=[&](const Compiled& c,std::string_view text) {
        if(++comparisons>EvaluationLimits::comparisons)throw RegexError(RegexFailure::resource_limit,"Rule scope comparison limit reached");
        try{return c.pattern.match(text,stop);}catch(const RegexError&){if(exhausted)throw RegexError(RegexFailure::resource_limit,"Rule scope execution limit reached");throw;}
    };
    std::vector<Result> result;result.reserve(records.size());
    for(const auto& input:records) {
        if(stop())throw RegexError(exhausted?RegexFailure::resource_limit:RegexFailure::cancelled,"Rule evaluation stopped");
        Result out{input,{}};auto& row=out.record;
        reset(row.payee,row.source.payee);reset(row.memo,row.source.memo);
        for(auto& allocation:row.allocations)if(allocation.source_ordinal) {
            const auto& s=row.source.allocations[*allocation.source_ordinal];
            reset(allocation.memo,s.memo);reset(allocation.category,CategoryValue{"",s.category});
        }
        auto explain=[&](const Rule& r,std::optional<std::size_t> allocation,std::string field,Effect effect) {
            if(++explanations>EvaluationLimits::explanations)throw RegexError(RegexFailure::resource_limit,"Rule explanation limit reached");
            out.explanations.push_back({{r.identity,r.version},allocation,std::move(field),effect});
        };
        auto assign=[&]<class T>(Field<T>& field,T value,const Rule& r,std::optional<std::size_t> index,const char* name) {
            if(index && std::string_view(name)=="category" && row.allocations[*index].source_ordinal &&
               qif::transfer_destination(row.source.allocations[*row.allocations[*index].source_ordinal].category)) {
                explain(r,index,name,Effect::transfer_protected);return;
            }
            if(field.ownership==Ownership::manual&&!replace){explain(r,index,name,Effect::manual_protected);return;}
            if(field.ownership==Ownership::rule){explain(r,index,name,field.value==value?Effect::compatible:Effect::competing);return;}
            if(size(value)>EvaluationLimits::input_bytes-generated_bytes)throw RegexError(RegexFailure::resource_limit,"Rule result scope exceeds memory limit");
            generated_bytes+=size(value);
            field={std::move(value),Ownership::rule,Contribution{r.identity,r.version}};explain(r,index,name,Effect::applied);
        };
        Field<CategoryValue> parent{{"",""},Ownership::source,std::nullopt};
        for(const auto& c:impl_->rules) {
            const auto& r=c.rule;if(!r.enabled||r.scope!=Scope::transaction)continue;
            auto subject=r.input==InputField::payee?row.source.payee:r.input==InputField::memo?row.source.memo:row.source.category;
            auto hit=match(c,subject);if(!hit)continue;
            if(r.actions.payee_template)assign(row.payee,expand_template(*r.actions.payee_template,*hit),r,std::nullopt,"payee");
            if(r.actions.memo_template)assign(row.memo,expand_template(*r.actions.memo_template,*hit),r,std::nullopt,"memo");
            if(r.actions.category)assign(parent,CategoryValue{*r.actions.category,""},r,std::nullopt,"parent category");
        }
        if(!row.source.split&&parent.ownership==Ownership::rule) {
            for(std::size_t i=0;i<row.allocations.size();++i)if(row.allocations[i].source_ordinal) {
                const auto& contribution=*parent.contribution;
                const auto& r=std::find_if(impl_->rules.begin(),impl_->rules.end(),[&](const auto& c){return c.rule.identity==contribution.rule;})->rule;
                assign(row.allocations[i].category,parent.value,r,i,"category");
            }
        }
        // Fixed second phase: each retained source allocation has independent field winners.
        for(std::size_t i=0;i<row.allocations.size();++i) {
            if(!row.source.split)break;
            auto& allocation=row.allocations[i];if(!allocation.source_ordinal)continue;
            const auto& source=row.source.allocations[*allocation.source_ordinal];
            for(const auto& c:impl_->rules) {
                const auto& r=c.rule;if(!r.enabled||r.scope!=Scope::split)continue;
                auto subject=r.input==InputField::payee?row.source.payee:r.input==InputField::memo?source.memo:source.category;
                auto hit=match(c,subject);if(!hit)continue;
                if(r.actions.memo_template)assign(allocation.memo,expand_template(*r.actions.memo_template,*hit),r,i,"memo");
                if(r.actions.category)assign(allocation.category,CategoryValue{*r.actions.category,""},r,i,"category");
                if(r.actions.inherit_parent) {
                    if(parent.ownership==Ownership::rule)assign(allocation.category,parent.value,r,i,"category");
                    else explain(r,i,"category",Effect::missing_parent);
                }
            }
        }
        result.push_back(std::move(out));
    }
    return result;
}
} // namespace spentit::rules
