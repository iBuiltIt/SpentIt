#include <spentit/rule_preview.hpp>
#include <spentit/rule_commands.hpp>
#include <map>
#include <set>

namespace spentit::app {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
rules::Ownership owner(FieldOwnership value) {
    switch(value){case FieldOwnership::source:return rules::Ownership::source;case FieldOwnership::manual:case FieldOwnership::untracked:return rules::Ownership::manual;case FieldOwnership::rule:return rules::Ownership::rule;}
    invalid("Unknown field ownership");
}
FieldOwnership owner(rules::Ownership value) {
    switch(value){case rules::Ownership::source:return FieldOwnership::source;case rules::Ownership::manual:return FieldOwnership::manual;case rules::Ownership::rule:return FieldOwnership::rule;}
    invalid("Unknown field ownership");
}
}
std::vector<RuleProposal> preview_rules(const rules::RuleSet& ruleset,const CategoryCatalog& catalog,const std::vector<RuleInput>& inputs,bool replace,const std::function<bool()>& cancelled) {
    if(inputs.size()>rules::EvaluationLimits::records)throw rules::RegexError(rules::RegexFailure::resource_limit,"Too many records in one rule preview");
    if(catalog.categories.size()>rules::EvaluationLimits::scope_allocations)throw rules::RegexError(rules::RegexFailure::resource_limit,"Category catalog exceeds bounded rule preview limit");
    std::size_t allocation_count=0,source_count=0,bytes=0;
    auto checkpoint=[&] {bool stop=false;try{stop=cancelled&&cancelled();}catch(...){throw rules::RegexError(rules::RegexFailure::internal,"Rule cancellation check failed");}if(stop)throw rules::RegexError(rules::RegexFailure::cancelled,"Rule preview cancelled");};
    auto count=[&](std::string_view value){if(value.size()>rules::EvaluationLimits::input_bytes-bytes)throw rules::RegexError(rules::RegexFailure::resource_limit,"Rule preview text exceeds limit");bytes+=value.size();};
    // Bound before constructing engine copies, including empty-string allocations.
    for(const auto& input:inputs) {
        checkpoint();const auto& allocations=input.draft.transaction.allocations();
        if(allocations.size()>rules::EvaluationLimits::scope_allocations-allocation_count||input.source.source_allocation_fields.size()>rules::EvaluationLimits::scope_allocations-source_count)throw rules::RegexError(rules::RegexFailure::resource_limit,"Rule preview allocation count exceeds limit");
        allocation_count+=allocations.size();source_count+=input.source.source_allocation_fields.size();
        if(input.provenance.size()>2+2*allocations.size())invalid("Too much rule field provenance");
        count(input.source.payee);count(input.source.memo);count(input.source.source_category);count(input.draft.payee);count(input.draft.memo);
        for(const auto& a:input.source.source_allocation_fields){checkpoint();count(a.category);count(a.memo);}
        for(const auto& a:allocations){checkpoint();count(a.identity);count(a.category);count(a.memo);if(a.category_binding)count(a.category_binding->identity);}
        for(const auto& p:input.provenance){count(p.allocation);count(p.contribution.rule);}
    }
    std::map<std::string,CategoryDefinition> definitions;
    for(const auto& c:catalog.categories){checkpoint();count(c.identity);count(c.name);count(c.parent);if(!definitions.emplace(c.identity,c).second)invalid("Duplicate catalog identity");}
    std::map<std::string,std::string> paths;std::set<std::string> active;
    for(const auto& [id,c]:definitions) {
        if(c.deleted||c.kind!=CategoryKind::leaf)continue;
        std::string path=c.name;bool available=!c.archived;
        if(!c.parent.empty()) {
            auto parent=definitions.find(c.parent);
            if(parent==definitions.end()||parent->second.deleted||parent->second.kind!=CategoryKind::group)invalid("Invalid category parent");
            path=parent->second.name+": "+c.name;available&=!parent->second.archived;
        }
        paths.emplace(id,std::move(path));if(available)active.insert(id);
    }
    std::vector<rules::Record> records;records.reserve(inputs.size());std::set<imports::Occurrence> occurrences;
    for(const auto& input:inputs) {
        if(!occurrences.insert(input.identity).second)invalid("Rule scope repeats a transaction");
        imports::validate_draft(input.source,input.draft);
        using Key=std::pair<RuleField,std::string>;
        std::map<Key,rules::Contribution> contributions;
        for(const auto& p:input.provenance)if(!contributions.emplace(Key{p.field,p.allocation},p.contribution).second)invalid("Duplicate rule field provenance");
        auto contribution=[&](RuleField field,const std::string& allocation,FieldOwnership ownership)->std::optional<rules::Contribution>{
            auto found=contributions.find({field,allocation});
            if(ownership!=FieldOwnership::rule){if(found!=contributions.end())invalid("Non-rule field has rule provenance");return {};}
            if(found==contributions.end())invalid("Rule-owned field lacks retained provenance");
            auto value=found->second;contributions.erase(found);return value;
        };
        rules::Record record;
        record.source={input.source.payee,input.source.memo,input.source.source_category,input.source.has_source_splits,{}};
        for(const auto& a:input.source.source_allocation_fields)record.source.allocations.push_back({a.memo,a.category});
        record.payee={input.draft.payee,owner(input.draft.payee_owner),contribution(RuleField::payee,"",input.draft.payee_owner)};
        record.memo={input.draft.memo,owner(input.draft.memo_owner),contribution(RuleField::memo,"",input.draft.memo_owner)};
        std::set<std::string> allocation_ids;
        for(const auto& a:input.draft.transaction.allocations()) {
            if(a.identity.empty()||!allocation_ids.insert(a.identity).second)invalid("Rule preview requires stable allocation identities");
            if(a.category_binding&&!paths.contains(a.category_binding->identity))invalid("Unknown assigned category");
            rules::CategoryValue value=a.category_binding?rules::CategoryValue{a.category_binding->identity,""}:rules::CategoryValue{"",a.category};
            record.allocations.push_back({a.source_ordinal,{a.memo,owner(a.memo_owner),contribution(RuleField::allocation_memo,a.identity,a.memo_owner)},{std::move(value),owner(a.category_owner),contribution(RuleField::allocation_category,a.identity,a.category_owner)}});
        }
        if(!contributions.empty())invalid("Rule provenance references an absent field");
        records.push_back(std::move(record));
    }
    auto evaluated=ruleset.evaluate(records,active,replace,cancelled);
    std::vector<RuleProposal> proposals;proposals.reserve(inputs.size());
    for(std::size_t i=0;i<inputs.size();++i) {
        const auto& input=inputs[i];auto& result=evaluated[i];auto after=input.draft;std::vector<RuleProvenance> provenance;
        auto remember=[&](RuleField field,const std::string& allocation,const auto& value){if(value.contribution)provenance.push_back({field,allocation,*value.contribution});};
        auto marked=[&](const auto& value)->std::optional<RuleMark>{
            if(owner(value.ownership)!=FieldOwnership::rule)return {};
            if(!value.contribution)invalid("Rule-owned field lacks retained provenance");
            return RuleMark{value.contribution->rule,value.contribution->version};
        };
        after.payee=result.record.payee.value;after.memo=result.record.memo.value;
        after.payee_owner=owner(result.record.payee.ownership);after.memo_owner=owner(result.record.memo.ownership);
        after.payee_rule=marked(result.record.payee);after.memo_rule=marked(result.record.memo);
        remember(RuleField::payee,"",result.record.payee);remember(RuleField::memo,"",result.record.memo);
        auto allocations=after.transaction.allocations();
        for(std::size_t j=0;j<allocations.size();++j) {
            auto& a=allocations[j];const auto& fields=result.record.allocations[j];
            a.memo=fields.memo.value;a.memo_owner=owner(fields.memo.ownership);a.memo_rule=marked(fields.memo);
            // A protected manual binding keeps its original label/revision snapshot.
            if(fields.category.ownership!=rules::Ownership::manual) {
                if(fields.category.value.identity.empty()) {a.category_binding.reset();a.category=fields.category.value.unmapped;}
                else {a.category_binding=CategoryBinding{fields.category.value.identity,catalog.revision};a.category=paths.at(fields.category.value.identity);}
            }
            a.category_owner=owner(fields.category.ownership);a.category_rule=marked(fields.category);
            remember(RuleField::allocation_memo,a.identity,fields.memo);remember(RuleField::allocation_category,a.identity,fields.category);
        }
        after.transaction=after.transaction.corrected(after.transaction.effective_total(),std::move(allocations));
        imports::validate_draft(input.source,after);
        proposals.push_back({input.identity,std::move(after),std::move(provenance),std::move(result.explanations)});
    }
    return proposals;
}
std::vector<RuleFieldDiff> rule_field_diffs(const imports::DraftRow& before,const RuleProposal& proposal) {
    const auto& after=proposal.after;std::vector<RuleFieldDiff> diffs;
    auto consider=[&](RuleField field,const std::string& allocation,const std::string& before_text,const std::string& after_text,FieldOwnership before_owner,FieldOwnership after_owner,const std::optional<RuleMark>& before_mark,const std::optional<RuleMark>& after_mark) {
        if(before_text==after_text && before_owner==after_owner && before_mark==after_mark)return;
        const bool removed=before_mark.has_value() && before_mark!=after_mark;
        const bool assigned=after_owner==FieldOwnership::rule && (before_mark!=after_mark || before_owner!=after_owner || before_text!=after_text);
        diffs.push_back({field,allocation,before_text,after_text,before_owner,after_owner,before_mark,after_mark,removed,assigned});
    };
    consider(RuleField::payee,"",before.payee,after.payee,before.payee_owner,after.payee_owner,before.payee_rule,after.payee_rule);
    consider(RuleField::memo,"",before.memo,after.memo,before.memo_owner,after.memo_owner,before.memo_rule,after.memo_rule);
    const auto& left=before.transaction.allocations();const auto& right=after.transaction.allocations();
    if(left.size()!=right.size())invalid("Allocation identities do not line up");
    std::map<std::string,const Allocation*> by_identity;
    for(const auto& allocation:right)if(allocation.identity.empty() || !by_identity.emplace(allocation.identity,&allocation).second)invalid("Allocation identities do not line up");
    std::set<std::string> seen;
    for(const auto& allocation:left) {
        if(allocation.identity.empty() || !seen.insert(allocation.identity).second)invalid("Allocation identities do not line up");
        auto found=by_identity.find(allocation.identity);if(found==by_identity.end())invalid("Allocation identities do not line up");
        const auto& other=*found->second;
        consider(RuleField::allocation_memo,allocation.identity,allocation.memo,other.memo,allocation.memo_owner,other.memo_owner,allocation.memo_rule,other.memo_rule);
        consider(RuleField::allocation_category,allocation.identity,allocation.category,other.category,allocation.category_owner,other.category_owner,allocation.category_rule,other.category_rule);
    }
    return diffs;
}
}
