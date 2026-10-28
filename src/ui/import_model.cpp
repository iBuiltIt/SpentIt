#include "import_model.hpp"
#include <spentit/import_view.hpp>
#include <spentit/statements.hpp>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>
#include <chrono>
#include <thread>
#include <map>
#include <set>
using namespace spentit;
namespace {
QString text(std::string_view s) { return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size())); }
std::string str(const QVariant& v) { return v.toString().toUtf8().toStdString(); }
QString money(const Money& m) { return text(app::decimal(m)); }
// Restores the worker's selected account after a cross-account fact read.
struct AccountRestore {
    app::ImportRepository& repo;
    std::int64_t home;
    bool moved=false;
    explicit AccountRestore(app::ImportRepository& repository):repo(repository),home(repository.account().identity){}
    void select(std::int64_t id){if(id!=repo.account().identity){repo.select_account(id);moved=true;}}
    void restore(){if(!moved)return;repo.select_account(home);moved=false;}
    ~AccountRestore(){if(!moved)return;try{repo.select_account(home);}catch(...){}}
};
struct TransferChoice {
    transfers::AllocationFact fact;
    std::int64_t revision=0;
    std::string payee,memo,link;
    bool linked=false;
};
TransferChoice read_transfer_choice(app::ImportRepository& repo,const QVariantMap& endpoint){
    const auto account=endpoint.value("account").toString().toLongLong();
    const auto batch=str(endpoint.value("batch"));
    const auto ordinal=static_cast<std::size_t>(endpoint.value("ordinal").toUInt());
    auto allocation=str(endpoint.value("allocation"));
    if(account<=0 || batch.empty()) throw std::invalid_argument("Choose a transaction");
    AccountRestore guard(repo);
    guard.select(account);
    const auto links=repo.transfers();
    const imports::Occurrence id{batch,ordinal};
    const auto batches=repo.batches();
    const auto info=std::find_if(batches.begin(),batches.end(),[&](const auto& item){return item.id==batch;});
    std::optional<imports::DraftRow> row;
    std::int64_t revision=0;
    bool committed=false;
    if(info!=batches.end()){
        revision=info->revision;
        committed=info->state==app::BatchState::committed;
        const bool editable=info->state==app::BatchState::pending || info->state==app::BatchState::protected_drafts;
        if(!committed && !editable) throw std::invalid_argument("Resolve the draft before assigning a transfer");
        if(committed){
            const auto entry=repo.committed_entry(id);
            if(!entry.current.active) throw std::invalid_argument("Transfer allocation changed; refresh before applying");
            row=entry.current.draft;
        }else{
            const auto loaded=repo.row(id);
            if(loaded.excluded_by_clear || !loaded.draft || loaded.decision.disposition!=imports::Disposition::include)
                throw std::invalid_argument("Resolve the draft before assigning a transfer");
            row=*loaded.draft;
        }
        if(allocation.empty()){
            if(row->transaction.allocations().size()!=1)
                throw std::invalid_argument("Choose the allocation explicitly. This transaction has more than one possible transfer endpoint.");
            allocation=row->transaction.allocations().front().identity;
        }
    }
    const transfers::Side* matched=nullptr;
    const app::TransferView* host=nullptr;
    for(const auto& value:links)for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(side->state==transfers::State::removed || side->endpoint.account!=account || side->endpoint.entry!=id) continue;
        if(!allocation.empty() && side->endpoint.allocation!=allocation) continue;
        if(matched) throw std::invalid_argument("Choose the allocation explicitly. This transaction has more than one possible transfer endpoint.");
        matched=side;host=&value;
    }
    if(!row && !matched) throw std::invalid_argument("Transfer entry is unavailable");
    if(row){
        const auto& allocations=row->transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==allocation;});
        if(found==allocations.end()) throw std::invalid_argument("Choose a transfer allocation");
        if(matched && (matched->amount!=found->amount || matched->date!=row->date))
            throw std::invalid_argument("Transfer allocation changed; refresh before applying");
    }
    const bool linked=host && !host->link.unresolved;
    TransferChoice choice{transfers::AllocationFact{{account,id,allocation},Money(0,repo.account().currency),"",transfers::State::draft,false,false,false},0,{},{},{},false};
    if(matched){
        const bool generated=!matched->independent_evidence && !matched->manually_entered && !matched->attached_evidence;
        choice={transfers::AllocationFact{matched->endpoint,matched->amount,matched->date,matched->state,matched->independent_evidence,generated,matched->manually_entered},matched->independent_evidence?revision:0,host->payee,host->memo,linked?host->link.identity:std::string{},linked};
    }else{
        const auto& allocations=row->transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==allocation;});
        const bool imported=row->transaction.original_imported_amount().has_value();
        choice={transfers::AllocationFact{{account,id,found->identity},found->amount,row->date,committed?transfers::State::committed:transfers::State::draft,imported,false,!imported},revision,row->payee,row->memo,{},false};
    }
    guard.restore();
    return choice;
}
QString evidencePreview(std::string_view bytes) {
    constexpr std::size_t limit=64*1024;
    const auto part=bytes.substr(0,limit);
    const auto prefix=bytes.size()>limit ? QString("Preview: first 65536 of %1 source bytes. Full evidence is retained in the dataset.\n").arg(bytes.size()) : QString{};
    if(!qif::valid_utf8(part) || part.find('\0')!=part.npos)
        return prefix+"Byte evidence (hex; no replacement decoding):\n"+QString::fromLatin1(QByteArray(part.data(),static_cast<qsizetype>(part.size())).toHex(' '));
    return prefix+text(part);
}
std::string identity() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
QString markText(const std::optional<RuleMark>& mark){return mark?text(mark->identity)+" v"+QString::number(mark->version):QString{};}
QString ownerName(FieldOwnership owner){
    switch(owner){case FieldOwnership::untracked:return "untracked";case FieldOwnership::source:return "source";case FieldOwnership::manual:return "manual";case FieldOwnership::rule:return "rule";}
    return {};
}
QVariantMap allocationMap(const Allocation& a,const QVariantMap& labels={}) {
    const auto categoryId=a.category_binding?text(a.category_binding->identity):QString{};
    const auto display=labels.value(categoryId).toMap();
    const auto label=qif::transfer_destination(a.category)?"Transfer destination: "+text(a.category):categoryId.isEmpty()?(a.category.empty()?QString("Uncategorized"):"Unmapped: "+text(a.category)):display.value("path",text(a.category)).toString()+(display.value("archived").toBool()?" [Archived]":"");
    return {{"category",text(a.category)},{"memo",text(a.memo)},{"amount",money(a.amount)},
        {"transferDestination",qif::transfer_destination(a.category)?text(std::string(*qif::transfer_destination(a.category))):QString{}},{"transferLabel",money(a.amount)+" · "+text(a.category)+" · "+text(a.identity)},{"categoryId",categoryId},{"bindingRevision",a.category_binding?QString::number(a.category_binding->catalog_revision):QString{}},
        {"allocationId",text(a.identity)},{"categoryDisplay",label},{"categoryTouched",false},{"memoTouched",false},
        {"sourceOrdinal",a.source_ordinal?QVariant(static_cast<int>(*a.source_ordinal)):QVariant(-1)},
        {"categoryOwner",ownerName(a.category_owner)},{"memoOwner",ownerName(a.memo_owner)},{"categoryRule",markText(a.category_rule)},{"memoRule",markText(a.memo_rule)}};
}
QVariantList allocations(const Transaction& t,const QVariantMap& labels={}) {
    QVariantList out;for(const auto& a:t.allocations())out.push_back(allocationMap(a,labels));return out;
}
QVariantMap draft(const imports::DraftRow& d,const QVariantMap& labels={}) {
    return {{"date",text(d.date)},{"payee",text(d.payee)},{"memo",text(d.memo)},
        {"payeeOwner",ownerName(d.payee_owner)},{"memoOwner",ownerName(d.memo_owner)},{"payeeRule",markText(d.payee_rule)},{"memoRule",markText(d.memo_rule)},
        {"total",money(d.transaction.effective_total())},{"splits",allocations(d.transaction,labels)}};
}
QString scopeName(rules::Scope scope){return scope==rules::Scope::split?"split":"transaction";}
QString inputName(rules::InputField input){return input==rules::InputField::memo?"memo":input==rules::InputField::category?"category":"payee";}
QVariantMap ruleMap(const app::RuleDefinitionImage& rule){
    return {{"id",text(rule.identity)},{"label",text(rule.label)},{"enabled",rule.enabled},{"scope",scopeName(rule.scope)},{"input",inputName(rule.input)},
        {"expression",text(rule.expression)},{"payeeTemplate",rule.actions.payee_template?text(*rule.actions.payee_template):QString{}},
        {"memoTemplate",rule.actions.memo_template?text(*rule.actions.memo_template):QString{}},{"categoryId",rule.actions.category?text(*rule.actions.category):QString{}},
        {"inheritParent",rule.actions.inherit_parent}};
}
std::optional<std::string> actionText(const QVariant& value){const auto body=str(value);if(body.empty())return {};return body;}
app::RuleDefinitionImage imageFrom(const QVariantMap& value){
    app::RuleDefinitionImage rule;rule.identity=str(value.value("id"));rule.label=str(value.value("label"));rule.enabled=value.value("enabled").toBool();
    const auto scope=value.value("scope").toString();const auto input=value.value("input").toString();
    if(scope!="transaction" && scope!="split")throw std::invalid_argument("Unknown rule scope");
    if(input!="payee" && input!="memo" && input!="category")throw std::invalid_argument("Unknown rule input");
    rule.scope=scope=="split"?rules::Scope::split:rules::Scope::transaction;
    rule.input=input=="memo"?rules::InputField::memo:input=="category"?rules::InputField::category:rules::InputField::payee;
    rule.expression=str(value.value("expression"));
    rule.actions.memo_template=actionText(value.value("memoTemplate"));
    rule.actions.inherit_parent=rule.scope==rules::Scope::split && value.value("inheritParent").toBool();
    if(rule.scope==rules::Scope::transaction)rule.actions.payee_template=actionText(value.value("payeeTemplate"));
    if(!rule.actions.inherit_parent)rule.actions.category=actionText(value.value("categoryId"));
    return rule;
}
QString effectName(rules::Effect effect){
    switch(effect){case rules::Effect::applied:return "applied";case rules::Effect::compatible:return "compatible";case rules::Effect::competing:return "competing";
        case rules::Effect::transfer_protected:return "transfer_protected";case rules::Effect::manual_protected:return "manual_protected";case rules::Effect::missing_parent:return "missing_parent";}
    return {};
}
QString effectWording(rules::Effect effect){
    switch(effect){case rules::Effect::applied:return "This rule will assign the field";
        case rules::Effect::compatible:return "Also matches and agrees; shadowed by the earlier field winner";
        case rules::Effect::competing:return "Also matches and disagrees; shadowed by the earlier field winner";
        case rules::Effect::transfer_protected:return "Transfer destination is preserved; resolve it in Transfers";
        case rules::Effect::manual_protected:return "Matches; the manual value, including a blank, is kept";
        case rules::Effect::missing_parent:return "Inheritance was not applied because parent classification is missing";}
    return {};
}
QString diffWording(bool removed,bool assigned){
    QStringList parts;
    if(removed)parts<<"Derived value will be cleared and the source value restored, or the previous rule mark replaced";
    if(assigned)parts<<"New rule assignment";
    return parts.join(" ");
}
QString fieldName(app::RuleField field){
    switch(field){case app::RuleField::payee:return "payee";case app::RuleField::memo:return "memo";
        case app::RuleField::allocation_memo:return "allocation memo";case app::RuleField::allocation_category:return "allocation category";}
    return {};
}
QString ruleLimitText(){
    return QString("Rule limits: %1 rules, %2 byte pattern, %3 byte field, %4 byte template, %5 records, %6 allocations, %7 scope allocations, %8 byte input, %9 comparisons, %10 callouts, %11 explanations, %12 captures, %13 match steps.")
        .arg(rules::EvaluationLimits::rules).arg(rules::RegexLimits::pattern_bytes).arg(rules::RegexLimits::subject_bytes).arg(rules::RegexLimits::output_bytes)
        .arg(rules::EvaluationLimits::records).arg(rules::EvaluationLimits::allocations).arg(rules::EvaluationLimits::scope_allocations).arg(rules::EvaluationLimits::input_bytes)
        .arg(rules::EvaluationLimits::comparisons).arg(rules::EvaluationLimits::callouts).arg(rules::EvaluationLimits::explanations).arg(rules::RegexLimits::captures).arg(rules::RegexLimits::match_steps);
}
QString ruleSaveRefreshFailure(const QString& operation){return "Rule save "+operation+" was saved durably, but the refreshed view could not be loaded. Reopen and inspect rule history; do not save it again.";}
QString ruleApplyRefreshFailure(const QString& operation){return "Rule application "+operation+" was saved durably, but the refreshed view could not be loaded. Reopen and inspect the pending drafts; do not apply it again.";}
QString ruleReapplyRefreshFailure(const QString& operation){return "Rule reapply "+operation+" was saved durably, but the refreshed view could not be loaded. Reopen and inspect committed history; do not apply it again.";}
QString phaseFor(const QString& action){
    if(action=="open"||action=="unlock")return "Opening dataset";
    if(action=="import")return "Reading statement file";
    if(action=="resume")return "Preparing draft interpretations";
    if(action=="commit")return "Committing import";
    if(action=="backup")return "Creating verified backup";
    if(action=="create")return "Creating dataset";
    if(action=="save")return "Saving draft";
    if(action=="preview_change")return "Preparing committed-change preview";
    if(action=="apply_change")return "Applying committed change";
    if(action=="rules_load"||action=="rules_discard"||action=="rules_prefill")return "Loading rules";
    if(action=="rules_preview")return "Evaluating rules";
    if(action=="rules_save")return "Saving rules";
    if(action=="rules_apply")return "Applying rules";
    if(action=="rules_reapply_preview")return "Evaluating committed rules";
    if(action=="rules_reapply_apply")return "Reapplying rules";
    if(action=="rename_account")return "Renaming account";
    if(action=="archive_account")return "Archiving account";
    if(action=="reactivate_account")return "Reactivating account";
    if(action=="delete_account")return "Deleting empty account";
    if(action=="correct_opening")return "Correcting opening balance";
    if(action=="reassign_currency")return "Reassigning account currency";
    if(action=="undo_account")return "Undoing account command";
    return "Updating view";
}
QString changeName(app::ChangeKind kind) {
    switch(kind) {case app::ChangeKind::publication:return "Original whole-batch publication";case app::ChangeKind::correct:return "Correction";
        case app::ChangeKind::remove:return "Removal";case app::ChangeKind::undo:return "Undo";case app::ChangeKind::transfer:return "Transfer";}
    return {};
}
QString snapshotText(const app::EntrySnapshot& entry) {
    const auto& d=entry.draft;
    return QString("Revision %1 · %2\n%3 · %4\nMemo: %5\nEffective total: %6 %7 · original imported: %8")
        .arg(entry.revision).arg(entry.active?"Active":"Removed",text(d.date),text(d.payee),text(d.memo),
        text(d.transaction.effective_total().currency().code()),money(d.transaction.effective_total()),d.transaction.original_imported_amount()?money(*d.transaction.original_imported_amount()):QString("Unavailable"));
}
QVariantMap snapshotMap(const app::EntrySnapshot& entry,const QString& label) {
    return {{"label",label},{"summary",snapshotText(entry)},{"splits",allocations(entry.draft.transaction)}};
}
QString snapshotDetails(const app::EntrySnapshot& entry) {
    auto result=snapshotText(entry);
    for(const auto& a:entry.draft.transaction.allocations())result+="\nSplit: "+text(a.category)+" · "+text(a.memo)+" · "+money(a.amount);
    return result;
}
QString balanceImpact(const Money& before,const Money& change,const Money& after) {
    return text(before.currency().code())+" account balance: "+money(before)+" · change "+money(change)+" · resulting balance "+money(after);
}
imports::DraftRow editedDraft(const QVariantMap& a,const std::optional<Money>& original,app::ImportRepository& repo,
                             const imports::DraftRow* baseline,const qif::Record* source) {
    std::vector<Allocation> values;const auto splits=a.value("splits").toList();
    if(splits.size()>static_cast<qsizetype>(qif::max_allocations)) throw std::invalid_argument("Too many allocations");
    const auto* prior=baseline?&baseline->transaction.allocations():source?&source->source_allocations:nullptr;
    for(const auto& value:splits) {
        const auto v=value.toMap();const auto allocationId=str(v.value("allocationId"));const Allocation* old=nullptr;
        if(prior && !allocationId.empty())for(const auto& candidate:*prior)if(candidate.identity==allocationId){old=&candidate;break;}
        Allocation next{str(v.value("category")),str(v.value("memo")),Money::parse(str(v.value("amount")),repo.account().currency)};
        // Marks come from the stored baseline. A touched field becomes manual and drops its mark.
        if(old) {next.identity=old->identity;next.source_ordinal=old->source_ordinal;next.category_binding=old->category_binding;next.category_owner=old->category_owner;next.memo_owner=old->memo_owner;next.category_rule=old->category_rule;next.memo_rule=old->memo_rule;}
        else if(repo.categories_supported()){
            next.identity=allocationId.empty()?identity():allocationId;next.category_owner=FieldOwnership::manual;next.memo_owner=FieldOwnership::manual;
            if(!baseline && source)for(std::size_t i=0;i<source->source_allocation_fields.size();++i)if(allocationId=="source-"+std::to_string(i)){
                next.source_ordinal=i;
                if(next.category==source->source_allocation_fields[i].category)next.category_owner=FieldOwnership::source;
                if(next.memo==source->source_allocation_fields[i].memo)next.memo_owner=FieldOwnership::source;
            }
        }
        const auto chosen=str(v.value("categoryId"));const auto oldId=old && old->category_binding?old->category_binding->identity:std::string{};
        const bool retainedSourceCategory=!old && source && next.source_ordinal && *next.source_ordinal<source->source_allocation_fields.size()
            && chosen.empty() && next.category==source->source_allocation_fields[*next.source_ordinal].category;
        const bool categoryChanged=v.value("categoryTouched").toBool() || (!old && !retainedSourceCategory) || chosen!=oldId || (old && next.category!=old->category);
        if(repo.categories_supported() && categoryChanged) {
            if(!chosen.empty()) {
                const auto display=repo.resolve_category(chosen);if(!display.assignable)throw std::invalid_argument("Choose an active leaf category");
                const auto catalog=repo.category_catalog();
                if(v.value("bindingRevision").toString().toLongLong()!=catalog.revision)throw std::invalid_argument("Categories changed; choose the category again");
                next.category_binding=CategoryBinding{chosen,catalog.revision};next.category=display.path;
            } else next.category_binding.reset();
            next.category_owner=FieldOwnership::manual;next.category_rule.reset();
        }
        if(v.value("memoTouched").toBool() || (old && next.memo!=old->memo)){next.memo_owner=FieldOwnership::manual;next.memo_rule.reset();}
        values.push_back(std::move(next));
    }
    const auto total=Money::parse(str(a.value("total")),repo.account().currency);
    const bool reassigned=(original && original->currency()!=total.currency()) || (source && source->source_currency && *source->source_currency!=total.currency());
    imports::DraftRow result{str(a.value("date")),str(a.value("payee")),str(a.value("memo")),
        Transaction::repaired_import(original,total,std::move(values),reassigned)};
    const auto oldPayee=baseline?baseline->payee:source?source->payee:std::string{};
    const auto oldMemo=baseline?baseline->memo:source?source->memo:std::string{};
    result.payee_owner=baseline?baseline->payee_owner:source?FieldOwnership::source:FieldOwnership::manual;
    result.memo_owner=baseline?baseline->memo_owner:source?FieldOwnership::source:FieldOwnership::manual;
    result.payee_rule=baseline?baseline->payee_rule:std::nullopt;
    result.memo_rule=baseline?baseline->memo_rule:std::nullopt;
    if(a.value("payeeTouched").toBool() || result.payee!=oldPayee){result.payee_owner=FieldOwnership::manual;result.payee_rule.reset();}
    if(a.value("memoTouched").toBool() || result.memo!=oldMemo){result.memo_owner=FieldOwnership::manual;result.memo_rule.reset();}
    return result;
}

}
struct ImportModel::Worker {
    std::unique_ptr<app::DocumentGateway> gateway;
    bool active=false,projected=true;
    QString path,filter;
    int sort=0; bool descending=false;
    QVariantMap selected,receipt,changePreview,changeReceipt,transferReceipt,transferMergePreview;
    std::optional<app::TransferRequest> preparedMerge;
    std::string mergeOperation;
    QVariantList transferCandidates;QString candidateLink;
    QVariantMap bulkPreview,bulkReceipt;
    struct PreparedImport {std::string id,bytes,label;statements::Index document;qif::DateOrder profile;};
    std::optional<PreparedImport> preparedImport;
    QVariantList importPlan,importMappingArgs;
    std::optional<std::vector<app::ImportAccountMapping>> preparedMappings;

    std::optional<app::BulkPreview> preparedBulk;
    std::string bulkOperation;
    std::set<imports::Occurrence> selection;
    std::optional<imports::Occurrence> selectionAnchor;
    std::optional<imports::Occurrence> selectionFocus;
    struct AccountView {QString filter;bool projected;int sort;bool descending;std::string batch;std::size_t ordinal;std::set<imports::Occurrence> selection;std::optional<imports::Occurrence> anchor,focus;};
    std::map<std::int64_t,AccountView> accountViews;
    QVariantList categories,categoryHistory;
    QVariantMap categoryById,categorySelected,categoryPreview,categoryReceipt;
    std::optional<app::CategoryPreview> preparedCategory;
    std::string categoryOperation;
    std::int64_t catalogRevision=0;bool catalogLoaded=false;
    QVariantList rules,ruleTombstones;
    QVariantMap rulePreview,ruleReceipt,rulePrefill;
    std::optional<app::RuleApplyRequest> preparedApply;
    std::string ruleApplyOperation;
    QVariantMap reapplyPreview,reapplyReceipt;
    std::optional<app::RuleReapplyPreview> preparedReapply;
    std::string reapplyOperation;
    std::int64_t ruleRevision=0;bool rulesLoaded=false;
    std::optional<app::ImportGroup> durableGroup(std::string_view id) {
        try{return gateway->repository().import_group_outcome(id);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();preparedImport.reset();preparedMappings.reset();importPlan.clear();throw std::runtime_error("Import operation outcome is uncertain. Reopen and inspect the retained source and account drafts before retrying.");}
    }
    app::ImportGroup resumeGroup(std::string_view id,app::OperationContext& context) {
        try{return gateway->repository().resume_import_group(id,context);}catch(...){const auto failure=std::current_exception();const auto saved=durableGroup(id);if(saved && (saved->state==app::GroupState::reviewing || saved->state==app::GroupState::started))return *saved;try{std::rethrow_exception(failure);}catch(const app::OperationCancelled&){throw app::OperationCancelled("Processing cancelled. The whole source and account mappings are saved; resume or abort the entire import.");}}
    }
    void catalog() {
        if(catalogLoaded)return;
        categories.clear();categoryById.clear();catalogRevision=0;
        auto& repo=gateway->repository();
        if(!repo.categories_supported()){catalogLoaded=true;return;}
        const auto current=repo.category_catalog();catalogRevision=current.revision;
        for(const auto& c:current.categories) {
            const CategoryDefinition* parent=nullptr;
            if(!c.parent.empty())for(const auto& p:current.categories)if(p.identity==c.parent){parent=&p;break;}
            const bool archived=c.archived || (parent && parent->archived);
            const auto path=parent?parent->name+": "+c.name:c.name;
            QVariantMap value{{"id",text(c.identity)},{"name",text(c.name)},{"parent",text(c.parent)},{"path",text(path)},
                {"group",c.kind==CategoryKind::group},{"archived",archived},{"ownArchived",c.archived},{"deleted",c.deleted},
                {"assignable",c.kind==CategoryKind::leaf && !archived && !c.deleted},{"revision",QString::number(c.revision)}};
            categories.push_back(value);categoryById[text(c.identity)]=value;
        }
        catalogLoaded=true;
    }
    void selectCategory(std::string_view id) {
        catalog();categorySelected=categoryById.value(text(id)).toMap();categoryHistory.clear();
        if(categorySelected.isEmpty())throw std::invalid_argument("Choose an existing category");
        for(const auto& item:gateway->repository().category_history(id)) {
            categoryHistory.prepend(QString("Catalog revision %1 · %2 → %3 · parent (current name): %4 · %5")
                .arg(item.catalog_after).arg(item.before?text(item.before->name):"(new)",text(item.after.name),item.after.parent.empty()?QString("Standalone"):categoryById.value(text(item.after.parent)).toMap().value("path",text(item.after.parent)).toString(),item.after.deleted?"Deleted":item.after.archived?"Archived":"Active"));
        }
    }
    QVariantList history,entryHistory;
    QString historyTitle,historyCursor;
    bool historyMore=false,historyLoaded=false;
    std::optional<app::EntryToken> selectedCommittedToken;
    std::optional<app::ChangePreview> preparedChange;
    std::string preparedOperation;
    void clearRules(){rules.clear();ruleTombstones.clear();rulePreview.clear();ruleReceipt.clear();rulePrefill.clear();preparedApply.reset();ruleApplyOperation.clear();preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();reapplyReceipt.clear();ruleRevision=0;rulesLoaded=false;}
    void clearChanges() {transferReceipt.clear();transferCandidates.clear();candidateLink.clear();transferMergePreview.clear();preparedMerge.reset();mergeOperation.clear();bulkPreview.clear();bulkReceipt.clear();preparedBulk.reset();bulkOperation.clear();categories.clear();categoryById.clear();categorySelected.clear();categoryHistory.clear();categoryPreview.clear();categoryReceipt.clear();preparedCategory.reset();categoryOperation.clear();catalogLoaded=false;clearRules();selectedCommittedToken.reset();preparedChange.reset();preparedOperation.clear();changePreview.clear();changeReceipt.clear();history.clear();entryHistory.clear();historyTitle.clear();historyCursor.clear();historyMore=false;historyLoaded=false;}
    void listHistory(std::int64_t before=0,bool append=false) {
        auto& repo=gateway->repository();
        const auto changes=repo.committed_changes(before,50);if(!append)history.clear();
        for(const auto& item:changes) {
            history.push_back(QVariantMap{{"operation",text(item.operation)},{"batch",text(item.identity.batch)},{"ordinal",static_cast<int>(item.identity.ordinal)},
                {"summary",changeName(item.kind)+" · current record: "+text(item.payee)+" · "+text(item.date)+" · "+money(item.total)+" · "+(item.active?"Active":"Removed")+" · operation "+text(item.operation)}});
        }
        historyMore=changes.size()==50;historyCursor=changes.empty()?QString{}:QString::number(changes.back().sequence);historyLoaded=true;
    }
    void entryDetails(const imports::Occurrence& id) {
        auto& repo=gateway->repository();const auto entry=repo.committed_entry(id);
        historyTitle="History · "+text(entry.current.draft.payee)+" · "+(entry.current.active?"Active":"Removed");entryHistory.clear();
        const auto entries=repo.committed_history(id);
        for(auto it=entries.rbegin();it!=entries.rend();++it) {
            const auto& item=*it;
            QString details=item.before_entry?"Before\n"+snapshotDetails(*item.before_entry)+"\n":"";
            details+="After\n"+snapshotDetails(item.after_entry);
            const auto reapplyGroup=text(repo.rule_reapply_group(item.operation));
            const bool reapply=!reapplyGroup.isEmpty();
            entryHistory.push_back(QVariantMap{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},{"operation",text(item.operation)},{"summary",changeName(item.kind)+" · "+text(item.operation)+"\n"+balanceImpact(item.before,item.change,item.after)},
                {"details",details},{"bulkGroup",text(repo.bulk_group(item.operation))},{"reapplyGroup",reapplyGroup},{"undoEligible",reapply?false:item.undo_eligible},{"undoReason",reapply?QString("Undo the whole rule reapply."):text(item.blocked_reason)}});
        }
    }
    // Session-local verified reads. Every mutation (including a failed command)
    // invalidates these before execution. The exclusive gateway owns all writes.
    QVariantMap metadata;
    std::optional<app::Account> cachedAccount;
    std::shared_ptr<std::vector<app::LedgerRow>> cachedLedger;
    std::shared_ptr<const app::LedgerView> cachedView;
    void invalidate() {catalogLoaded=false;rulesLoaded=false;metadata.clear();cachedAccount.reset();cachedLedger.reset();cachedView.reset();}
    std::string selectedBatch;
    std::size_t selectedOrdinal=0;
    std::int64_t selectedRevision=0;
    // Commit identity is deterministic for a batch revision, so restart retries
    // resolve the same durable operation without storing local financial metadata.
    explicit Worker(std::unique_ptr<app::DocumentGateway> g):gateway(std::move(g)){}
    void loadRules() {
        rules.clear();ruleTombstones.clear();ruleRevision=0;rulePrefill.clear();
        auto& repo=gateway->repository();
        if(!repo.rules_supported()){rulesLoaded=true;return;}
        const auto image=repo.rule_image();ruleRevision=image.revision;
        for(const auto& rule:image.ordered)rules.push_back(ruleMap(rule));
        for(const auto& tomb:repo.rule_tombstones()){auto map=ruleMap(app::RuleDefinitionImage{tomb.identity,tomb.label,false,tomb.scope,tomb.input,tomb.expression,tomb.actions});map["version"]=QString::number(tomb.version);ruleTombstones.push_back(map);}
        rulesLoaded=true;
    }
    QString ruleLabel(const QString& id) const {
        for(const auto& value:rules){const auto map=value.toMap();if(map.value("id").toString()==id){const auto label=map.value("label").toString();return label.isEmpty()?id:label;}}
        for(const auto& value:ruleTombstones){const auto map=value.toMap();if(map.value("id").toString()==id){const auto label=map.value("label").toString();return label.isEmpty()?id:label;}}
        return id;
    }
    void attachRules(app::ImportRepository& repo,const imports::Occurrence& id,app::OperationContext& context) {
        selected["ruleMatchError"]=QString();selected["winnerLines"]=QVariantList{};selected["ruleExplanations"]=QVariantList{};
        if(!repo.rules_supported())return;
        try {
            if(!rulesLoaded)loadRules();
            const auto match=repo.match_saved_rules(id,context);
            selected["ruleMatchError"]=text(match.error);
            QVariantList winners;for(const auto& line:match.winner_lines)winners.push_back(text(line));selected["winnerLines"]=winners;
            QVariantList explanations;
            for(const auto& item:match.explanations){
                const auto ruleId=text(item.contribution.rule);
                explanations.push_back(QVariantMap{{"rule",ruleId},{"version",QString::number(item.contribution.version)},{"label",ruleLabel(ruleId)},{"field",text(item.field)},
                    {"effect",effectName(item.effect)},{"wording",effectWording(item.effect)},{"allocation",item.allocation?QVariant(static_cast<int>(*item.allocation)):QVariant()}});
            }
            selected["ruleExplanations"]=explanations;
        } catch(const app::OperationCancelled&){throw;}
        catch(const app::OperationOutcomeUnknown&){throw;}
        catch(const std::exception& error){selected["ruleMatchError"]=text(error.what());}
    }
    void inspect(std::string batch,std::size_t ordinal,app::OperationContext& context) {
        auto& repo=gateway->repository(); const auto acct=repo.account();catalog();
        if(batch.starts_with("~transfer-")){
            for(const auto& value:repo.transfers())for(const auto* side:{&value.link.origin,&value.link.destination})if(side->endpoint.account==acct.identity && side->endpoint.entry==imports::Occurrence{batch,ordinal}){
                selectedBatch=batch;selectedOrdinal=ordinal;selectedCommittedToken.reset();
                Allocation allocation{"Transfer",value.memo,side->amount,{},side->endpoint.allocation};
                selected={{"batch",text(batch)},{"ordinal",static_cast<int>(ordinal)},{"editable",false},{"localTransfer",true},{"transferId",text(value.link.identity)},{"committed",side->state==transfers::State::committed},{"source",side->manually_entered?"Manually entered; no imported source":"Generated counterpart; no imported source"},{"draft",draft(imports::DraftRow{side->date,value.payee,value.memo,Transaction::manual(side->amount,{allocation})},categoryById)}};
                QVariantList origins;
                if(!value.link.unresolved && side==&value.link.origin)origins.push_back(QVariantMap{{"allocation",text(side->endpoint.allocation)},{"link",text(value.link.identity)}});
                selected["transferOrigins"]=origins;return;
            }
            throw std::invalid_argument("Transfer entry is unavailable");
        }
        const auto all=repo.batches();
        const auto found=std::find_if(all.begin(),all.end(),[&](const auto& b){return b.id==batch;});
        if(found==all.end() || ordinal>=found->count) throw std::invalid_argument("Choose a record in this import");
        auto row=repo.row({batch,ordinal});
        const auto& source=row.source;
        const auto raw=repo.record_evidence({batch,ordinal});
        selectedCommittedToken.reset();
        selectedBatch=batch; selectedOrdinal=ordinal; selectedRevision=found->revision;
        selected={{"batch",text(batch)},{"ordinal",static_cast<int>(ordinal)},{"editable",false},
            {"source",evidencePreview(raw)}};
        if(source.diagnostic) selected["diagnostic"]=text(qif::message(source.diagnostic->issue));
        QStringList classes;
        if(!source.source_class.empty())classes<<"Record class: "+text(source.source_class);
        if(source.has_source_splits)for(std::size_t i=0;i<source.source_allocation_fields.size();++i)if(!source.source_allocation_fields[i].class_label.empty())classes<<QString("Split %1 class: ").arg(i+1)+text(source.source_allocation_fields[i].class_label);
        selected["sourceClasses"]=classes.join(" · ");
        // Copy only parsed, supported evidence. Raw bytes remain separately visible;
        // an unavailable source date must never be replaced by a guessed value.
        QVariantList fields;
        const auto add=[&](QString label,QString field,QString value,int split=-1) {
            fields.push_back(QVariantMap{{"label",label},{"field",field},{"value",value},{"split",split}});
        };
        const auto format=source.format.empty()?"qif":source.format;
        add("Format","format",text(format));
        add("Grammar","grammar",text(source.grammar.empty()?"qif":source.grammar));
        if(!source.fitid.empty()) add("FITID","fitid",text(source.fitid));
        if(!source.time_evidence.empty()) add("Time evidence","time",text(source.time_evidence));
        if(!source.trntype.empty()) add("Transaction type","trntype",text(source.trntype));
        if(!source.uninterpreted.empty()) add("Uninterpreted evidence","uninterpreted",text(source.uninterpreted));
        if(source.transaction || source.repairable) {
            if(!source.date.empty()) add(format=="qif"?"Date (ISO, using import date order)":"Date (calendar date in the file)","date",text(source.date));
            add("Payee","payee",text(source.payee));
            add("Memo","memo",text(source.memo));
            if(source.original_amount) add("Original total","total",money(*source.original_amount));
            for(std::size_t i=0;i<source.source_allocation_fields.size();++i) {
                const auto& a=source.source_allocation_fields[i];
                const auto prefix=QString("Source split %1 → editor split %1: ").arg(i+1);
                add(prefix+"category","category",text(a.category),static_cast<int>(i));
                add(prefix+"memo","memo",text(a.memo),static_cast<int>(i));
                if(a.amount)add(prefix+"amount","amount",money(*a.amount),static_cast<int>(i));
            }
        }
        selected["sourceFields"]=fields;
        if(!row.draft) {
            if(source.repairable && !row.source.repairable) selected["diagnostic"]=selected["diagnostic"].toString()+" — repair requires a new experimental dataset; this older dataset was not migrated.";
            if(row.source.repairable && found->state==app::BatchState::pending) {
                QVariantList splits;
                for(std::size_t i=0;i<source.source_allocation_fields.size();++i){const auto& a=source.source_allocation_fields[i];
                    const auto destination=qif::transfer_destination(a.category);
                    const auto label=destination?"Transfer destination: "+text(a.category):a.category.empty()?QString("Uncategorized"):"Unmapped: "+text(a.category);
                    splits.push_back(QVariantMap{{"category",text(a.category)},{"categoryDisplay",label},{"memo",text(a.memo)},{"amount",a.amount?money(*a.amount):QString{}},{"allocationId",QString("source-%1").arg(i)},{"categoryId",QString{}},{"bindingRevision",QString{}},{"sourceOrdinal",static_cast<int>(i)},{"categoryOwner","source"},{"memoOwner","source"},{"categoryTouched",false},{"memoTouched",false},{"categoryRule",QString{}},{"memoRule",QString{}},{"transferDestination",destination?text(std::string(*destination)):QString{}},{"transferLabel",(a.amount?money(*a.amount):QString("Unavailable"))+" · "+text(a.category)}});
                }
                selected["draft"]=QVariantMap{{"date",text(source.date)},{"payee",text(source.payee)},{"memo",text(source.memo)},
                    {"total",source.original_amount?money(*source.original_amount):QString{}},{"splits",splits},{"payeeOwner","source"},{"memoOwner","source"},{"payeeRule",QString{}},{"memoRule",QString{}}};
                selected["original"]=source.original_amount?money(*source.original_amount):QString("Unavailable");selected["editable"]=true;selected["repair"]=true;
                selected["diagnostic"]=selected["diagnostic"].toString()+" — supply valid date, total and splits and save the repair; original evidence is retained.";
            }
            attachRules(repo,{batch,ordinal},context);
            return;
        }
        selected["draft"]=draft(*row.draft,categoryById);
        if(source.diagnostic) selected["diagnostic"]=selected["diagnostic"].toString()+" — saved interpretation repair; original diagnostic retained.";
        selected["original"]=row.draft->transaction.original_imported_amount()?money(*row.draft->transaction.original_imported_amount()):QString("Unavailable");
        selected["editable"]=found->state==app::BatchState::pending;
        selected["decision"]=static_cast<int>(row.decision.disposition);
        selected["reviewStale"]=row.review_stale;
        if(row.review_stale) selected["diagnostic"]=selected.value("diagnostic").toString()+" The matched committed record changed after this review. Reconfirm Include as separate transaction or the selected duplicate decision before committing this import.";
        const auto transferLinks=repo.transfers();
        for(const auto& allocation:row.draft->transaction.allocations())if(qif::transfer_destination(allocation.category)) {
            const transfers::Endpoint endpoint{acct.identity,{batch,ordinal},allocation.identity};bool resolved=false;
            for(const auto& value:transferLinks)for(const auto* side:{&value.link.origin,&value.link.destination})if(side->state!=transfers::State::removed)
                resolved|=side->endpoint==endpoint || (side->attached_evidence && side->attached_evidence->endpoint==endpoint) || (side->proposed_evidence && side->proposed_evidence->endpoint==endpoint);
            if(!resolved)selected["diagnostic"]=selected.value("diagnostic").toString()+" Unresolved QIF transfer destination: "+text(allocation.category)+". Review this allocation in Transfers; choose its destination or match an existing transfer before committing.";
        }
        selected["transferEligible"]=found->state==app::BatchState::pending && row.decision.disposition==imports::Disposition::include;
        const bool committed=found->state==app::BatchState::committed && row.decision.disposition==imports::Disposition::include && !row.excluded_by_clear;
        selected["committed"]=committed;
        if(committed && repo.committed_changes_supported()) {
            const auto entry=repo.committed_entry({batch,ordinal});selectedCommittedToken=entry.token;selected["draft"]=draft(entry.current.draft,categoryById);
            selected["removed"]=!entry.current.active;selected["canCorrect"]=entry.current.active;
        }
        QVariantList candidates;
        // Committed/discarded rows are read-only and have no duplicate actions.
        if(found->state==app::BatchState::pending) for(const auto& id:row.candidates) {
            auto prior=repo.row(id);
            const auto basis=imports::candidate_basis(row.source,prior.source);
            const bool removed=repo.committed_changes_supported() && !repo.committed_entry(id).current.active;
            const auto basisText=basis?text(imports::basis_label(*basis)):QString("review source evidence");
            candidates.push_back(QVariantMap{{"removed",removed},{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},
                {"label",text(id.batch)+" / "+QString::number(id.ordinal+1)+" · "+text(prior.source.format.empty()?"qif":prior.source.format)+" · "+text(prior.source.date)+" · "+text(prior.source.payee)+" · "+(prior.source.original_amount?money(*prior.source.original_amount):QString("Unavailable"))+" · "+basisText}});
        }
        for(auto& value:candidates) {auto candidate=value.toMap();if(candidate.value("removed").toBool())candidate["label"]=candidate.value("label").toString()+" · PREVIOUSLY REMOVED — explicit review required";value=candidate;}
        selected["candidates"]=candidates;
        if(row.decision.duplicate_of) {
            selected["duplicate"]=text(row.decision.duplicate_of->batch)+" / "+QString::number(row.decision.duplicate_of->ordinal+1);
            selected["removedDuplicate"]=repo.committed_changes_supported() && !repo.committed_entry(*row.decision.duplicate_of).current.active;
        }
        bool linked=false,endpoint=false,evidence=false;std::string transfer_id;const imports::Occurrence row_id{batch,ordinal};
        for(const auto& value:transferLinks)for(const auto* side:{&value.link.origin,&value.link.destination}){
            if(side->state==transfers::State::removed)continue;
            const bool hit_endpoint=side->endpoint.entry==row_id;
            const bool hit_evidence=(side->attached_evidence && side->attached_evidence->endpoint.entry==row_id) || (side->proposed_evidence && side->proposed_evidence->endpoint.entry==row_id);
            if(!hit_endpoint && !hit_evidence)continue;
            linked=true;endpoint=endpoint||hit_endpoint;evidence=evidence||hit_evidence;transfer_id=value.link.identity;
        }
        if(linked){selected["editable"]=false;if(!(committed && endpoint && !evidence && !batch.starts_with("~transfer-")))selected["canCorrect"]=false;selected["transferId"]=text(transfer_id);}
        QVariantList origins;
        for(const auto& value:transferLinks)if(!value.link.unresolved && value.link.origin.state!=transfers::State::removed && value.link.origin.endpoint.account==acct.identity && value.link.origin.endpoint.entry==imports::Occurrence{batch,ordinal})origins.push_back(QVariantMap{{"allocation",text(value.link.origin.endpoint.allocation)},{"link",text(value.link.identity)}});
        selected["transferOrigins"]=origins;
        if(row.excluded_by_clear)selected["diagnostic"]="Cleared from this workspace; immutable source evidence retained.";
        attachRules(repo,{batch,ordinal},context);
    }
    std::pair<QVariantMap,std::shared_ptr<const app::LedgerView>> snapshot(app::OperationContext* context=nullptr) {
        QVariantMap state{{"active",active},{"path",path}};
        if(!active) {state["bulkReceipt"]=bulkReceipt;state["changeReceipt"]=changeReceipt;state["categoryReceipt"]=categoryReceipt;state["ruleReceipt"]=ruleReceipt;return {state,{}};}
        auto& repo=gateway->repository();catalog();
        if(metadata.isEmpty()) {
            auto packAccount=[&](const app::Account& item){return QVariantMap{{"id",QString::number(item.identity)},{"name",text(item.name)},{"currency",text(item.currency.code())},{"archived",item.archived}};};
            QVariantList archivedList;for(const auto& item:repo.archived_accounts()) archivedList.push_back(packAccount(item));
            state["archivedAccounts"]=archivedList;
            state["committedChangesSupported"]=repo.committed_changes_supported();state["categoriesSupported"]=repo.categories_supported();
            if(!repo.account_selected()) {
                cachedAccount.reset();
                const auto home=archivedList.isEmpty()?QString():archivedList.front().toMap().value("currency").toString();
                state["accounts"]=QVariantList{};state["accountSelected"]=false;state["readOnly"]=true;state["archived"]=false;
                state["emptyActive"]="No active account is selected. Archived accounts remain available.";
                state["homeCurrency"]=home;state["accountId"]="";state["name"]="";state["currency"]=home;
                state["openingDate"]="";state["opening"]="";state["frozen"]=false;state["confirmed"]=false;
                state["committed"]="";state["projectionAvailable"]=false;state["projected"]="Incomplete — review import";
                state["batches"]=QVariantList{};state["undoableAccountOperation"]="";state["currencyWarning"]=false;
                state["transfersSupported"]=repo.transfers_supported();state["nextAccountId"]=QString::number(repo.next_account_identity());
                metadata=state;
            } else {
            cachedAccount=repo.account();const auto& acct=*cachedAccount;
            state["accountSelected"]=true;state["archived"]=acct.archived;state["readOnly"]=acct.archived;state["emptyActive"]="";
            state["currencyWarning"]=repo.account_currency_warning();
            const auto undoable=repo.undoable_account_operation();state["undoableAccountOperation"]=undoable?text(*undoable):QString();
            state["accountId"]=QString::number(acct.identity);
            QVariantList accountList;
            for(const auto& item:repo.accounts()) accountList.push_back(packAccount(item));
            state["accounts"]=accountList;state["homeCurrency"]=accountList.isEmpty()?text(acct.currency.code()):accountList.front().toMap().value("currency");
            state["transfersSupported"]=repo.transfers_supported();
            if(repo.transfers_supported()){
                state["transferSequence"]=QString::number(repo.transfer_sequence());QVariantList items;
                for(const auto& value:repo.transfers()){
                    const auto& side=value.link.origin.endpoint.account==acct.identity?value.link.origin:value.link.destination;
                    const auto& other=value.link.origin.endpoint.account==acct.identity?value.link.destination:value.link.origin;
                    if(side.state==transfers::State::removed)continue;
                    if(transfers::protected_draft(value.link,acct.identity))state["pendingProtected"]=true;
                    const auto target=other.attached_evidence?other.attached_evidence->endpoint:other.endpoint;
                    QString otherName;const auto otherId=QString::number(other.endpoint.account);
                    for(const auto& item:accountList)if(item.toMap().value("id").toString()==otherId)otherName=item.toMap().value("name").toString();
                    if(otherName.isEmpty())for(const auto& item:archivedList)if(item.toMap().value("id").toString()==otherId)otherName=item.toMap().value("name").toString();
                    items.push_back(QVariantMap{{"id",text(value.link.identity)},{"label",otherName+" · "+money(side.amount)+" · "+text(side.date)},{"amount",money(side.amount)},{"date",text(side.date)},{"committed",side.state==transfers::State::committed},{"accepted",side.accepted_unmatched},{"evidenced",side.independent_evidence},{"proposed",side.proposed_evidence.has_value()},{"provisional",side.provisional_date},{"protected",transfers::protected_draft(value.link,acct.identity)},{"local",!side.attached_evidence && (side.manually_entered || !side.independent_evidence)},{"isOrigin",value.link.origin.endpoint.account==acct.identity},{"counterpartAccount",QString::number(target.account)},{"counterpartBatch",text(target.entry.batch)},{"counterpartOrdinal",static_cast<int>(target.entry.ordinal)}});
                }state["transfers"]=items;
                QVariantList journal;
                for(const auto& entry:repo.transfer_journal()){
                    const auto line=text(entry.action)+" · "+text(entry.link)+" · account "+QString::number(entry.origin_account)+" "+money(entry.origin_amount)+" "+text(entry.origin_date)+" → account "+QString::number(entry.destination_account)+" "+money(entry.destination_amount)+" "+text(entry.destination_date)+" · sequence "+QString::number(entry.sequence)+(entry.unresolved?" · unresolved":"");
                    journal.push_back(QVariantMap{{"operation",text(entry.operation)},{"action",text(entry.action)},{"link",text(entry.link)},{"sequence",QString::number(entry.sequence)},{"originAccount",QString::number(entry.origin_account)},{"destinationAccount",QString::number(entry.destination_account)},{"originAmount",money(entry.origin_amount)},{"destinationAmount",money(entry.destination_amount)},{"originDate",text(entry.origin_date)},{"destinationDate",text(entry.destination_date)},{"unresolved",entry.unresolved},{"text",line}});
                }
                state["transferJournal"]=journal;
            }
            state["nextAccountId"]=QString::number(repo.next_account_identity());
            state["name"]=text(acct.name); state["currency"]=text(acct.currency.code());
            state["openingDate"]=text(acct.opening_date); state["opening"]=money(acct.opening);
            state["frozen"]=acct.frozen; state["confirmed"]=acct.opening_confirmed;
            state["committed"]=money(repo.committed_balance()); const auto projection=repo.projected_balance();
            state["projectionAvailable"]=projection.has_value(); state["projected"]=projection?money(*projection):"Incomplete — review import";
            QVariantList batches;
            for(const auto& b:repo.batches()) {
                QVariantMap info{{"id",text(b.id)},{"count",static_cast<int>(b.count)},{"status",b.state==app::BatchState::pending?"Draft":b.state==app::BatchState::committed?"Committed":b.state==app::BatchState::captured?"Processing incomplete":b.state==app::BatchState::protected_drafts?"Protected drafts":"Discarded"},{"blocked",b.blocked},{"processingIncomplete",b.processing_incomplete},{"revision",static_cast<qlonglong>(b.revision)}};
                if(b.processing_incomplete) info["diagnostics"]="Source evidence retained. Processing is incomplete; resume or discard this import.";
                else if(b.blocked) {
                    const auto parsed=repo.parsed_source(b.id);
                    QStringList diagnostics;
                    if(parsed.diagnostic) diagnostics<<text(qif::message(parsed.diagnostic->issue))+" at byte "+QString::number(parsed.diagnostic->offset);
                    for(std::size_t i=0;i<parsed.records.size() && diagnostics.size()<100;++i) if(parsed.records[i].diagnostic)
                        diagnostics<<"Record "+QString::number(i+1)+": "+text(qif::message(parsed.records[i].diagnostic->issue));
                    info["diagnostics"]=diagnostics.join('\n');
                }
                if(b.state==app::BatchState::pending || b.state==app::BatchState::captured){const auto progress=repo.repair_progress(b.id);info["repairs"]=static_cast<int>(progress.total);info["repairsDone"]=static_cast<int>(progress.done);info["amountRepairs"]=static_cast<int>(progress.amounts);info["unsupported"]=static_cast<int>(progress.blocked);}
                batches.push_back(info);
                if(b.state==app::BatchState::pending || b.state==app::BatchState::captured) state["pending"]=info;
                if(b.state==app::BatchState::protected_drafts){std::vector<Money> values;for(std::size_t n=0;n<b.count;++n){const auto r=repo.row({b.id,n});if(!r.excluded_by_clear && r.draft && r.decision.disposition==imports::Disposition::include)values.push_back(r.draft->transaction.effective_total());}const auto change=imports::sum(acct.currency,values);info["change"]=money(change);info["after"]=money(repo.committed_balance()+change);auto retained=state.value("protectedImports").toList();retained.push_back(info);state["protectedImports"]=retained;}
            }
            state["batches"]=batches;
            for(const auto& group:repo.import_groups())if(group.state!=app::GroupState::aborted) {
                const auto member=std::find_if(group.accounts.begin(),group.accounts.end(),[&](const auto& value){return value.account==acct.identity;});
                if(member==group.accounts.end())continue;
                const auto own=std::find_if(batches.begin(),batches.end(),[&](const auto& v){return str(v.toMap().value("id"))==member->batch;});
                if(own==batches.end() || own->toMap().value("status")=="Discarded" || own->toMap().value("status")=="Committed")continue;
                const auto document=qif::index_document(repo.evidence(member->batch));
                state["importGroup"]=QVariantMap{{"id",text(group.id)},{"started",group.state==app::GroupState::started},{"ready",group.unresolved==0},{"repairs",static_cast<int>(group.repairs_total)},{"done",static_cast<int>(group.repairs_done)},{"amountRepairs",static_cast<int>(group.amount_repairs)},{"blocked",static_cast<int>(group.blocked_records)},{"unresolved",static_cast<int>(group.unresolved)},{"sourceAccount",static_cast<int>(member->source_account)}};
                state["groupCreated"]=member->created && !acct.frozen;
                state["groupOpeningReady"]=member->created && member->balance_choice!=app::ReportedBalanceChoice::unreviewed;
                state["groupReviewedChange"]=member->reviewed_change?money(*member->reviewed_change):QString("Incomplete");
                state["groupCanReviewBalance"]=member->created && member->reviewed_change.has_value() && !acct.frozen;
                state["groupReportedBalance"]=document.accounts[member->source_account].reported_balance?text(*document.accounts[member->source_account].reported_balance):QString("Not supplied");
                state["groupProposedOpening"]=member->proposed_opening?money(*member->proposed_opening):QString("Not confirmed");
            }

            // Reconstruct the latest receipt after restart, including a commit whose
            // durable completion happened before the prior process could acknowledge it.
            for(const auto& b:repo.batches()) if(b.state==app::BatchState::committed) {
                const auto result=repo.durable_outcome("commit-"+b.id+"-"+std::to_string(b.revision-1));
                if(result) receipt={{"operation",text(result->operation)},{"included",static_cast<int>(result->included)},
                    {"excluded",static_cast<int>(result->excluded)},{"before",money(result->before)},
                    {"change",money(result->change)},{"after",money(result->after)}};
            }
            state["receipt"]=receipt;
            metadata=state;
            }
        } else state=metadata;
        state["importPlan"]=importPlan;
        state["categories"]=categories;state["categoryById"]=categoryById;state["catalogRevision"]=QString::number(catalogRevision);
        state["categorySelected"]=categorySelected;state["categoryHistory"]=categoryHistory;state["categoryPreview"]=categoryPreview;state["categoryReceipt"]=categoryReceipt;
        if(repo.rules_supported() && !rulesLoaded) loadRules();
        state["rulesSupported"]=repo.rules_supported();state["rules"]=rules;state["ruleTombstones"]=ruleTombstones;state["ruleRevision"]=QString::number(ruleRevision);
        state["rulePreview"]=rulePreview;state["ruleReceipt"]=ruleReceipt;state["rulePrefill"]=rulePrefill;state["ruleLimits"]=ruleLimitText();
        state["selected"]=selected;state["changePreview"]=changePreview;state["changeReceipt"]=changeReceipt;
        state["bulkSupported"]=repo.bulk_supported();state["bulkPreview"]=bulkPreview;state["bulkReceipt"]=bulkReceipt;
        state["reapplyPreview"]=reapplyPreview;state["reapplyReceipt"]=reapplyReceipt;
        state["history"]=history;state["entryHistory"]=entryHistory;state["historyTitle"]=historyTitle;state["historyMore"]=historyMore;state["historyCursor"]=historyCursor;
        state["mode"]=projected?"Projected":"Committed";state["sort"]=sort;state["descending"]=descending;
        state["selectedIndex"]=-1;
        // Never substitute a partial projected ledger for an authoritative one.
        if(!repo.account_selected() || (projected && !state.value("projectionAvailable").toBool())) return {state,{}};
        if(!cachedView) {
            if(!cachedLedger) {
                cachedLedger=std::make_shared<std::vector<app::LedgerRow>>(repo.ledger(projected));
                // A presentation-only copy resolves current names for both visible
                // rows and literal filtering. Stored interpretations remain intact.
                if(repo.categories_supported())for(auto& row:*cachedLedger) {
                    auto values=row.draft.transaction.allocations();
                    for(auto& value:values)value.category=allocationMap(value,categoryById).value("categoryDisplay").toString().toUtf8().toStdString();
                    row.draft.transaction=row.draft.transaction.corrected(row.draft.transaction.effective_total(),std::move(values));
                }
            }
            app::LedgerSearch query(filter.toStdString(),QDate::currentDate().year());
            struct SourceFile {std::string bytes;std::vector<std::pair<std::size_t,std::size_t>> spans;};
            std::map<std::string,SourceFile> sources;std::size_t sourceBytes=0;
            const auto lookup=[&](const imports::Occurrence& id)->std::optional<std::string_view>{
                if(id.batch.starts_with("~transfer-"))return {};
                auto found=sources.find(id.batch);
                if(found==sources.end()){
                    if(context)context->checkpoint("Searching original QIF evidence");
                    SourceFile file;file.bytes=repo.evidence(id.batch);
                    if(file.bytes.size()>256*1024*1024-sourceBytes)throw std::runtime_error("Search source budget exceeds 256 MiB");
                    sourceBytes+=file.bytes.size();
                    const auto batches=repo.batches();const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==id.batch;});
                    if(batch==batches.end())throw std::runtime_error("Search source batch is unavailable");
                    const auto parsed=repo.parsed_source(batch->id,[&]{if(context)context->checkpoint();});
                    if(!parsed.admissible || parsed.records.size()!=batch->count)throw std::runtime_error("Search source evidence could not be read completely");
                    for(const auto& record:parsed.records)file.spans.emplace_back(record.begin,record.end);
                    found=sources.emplace(id.batch,std::move(file)).first;
                }
                if(id.ordinal>=found->second.spans.size())throw std::runtime_error("Search source occurrence is unavailable");
                const auto [begin,end]=found->second.spans[id.ordinal];
                return std::string_view(found->second.bytes).substr(begin,end-begin);
            };
            cachedView=std::make_shared<const app::LedgerView>(app::search_ledger(*cachedAccount,*cachedLedger,query,sort,descending,lookup,[&]{return context && context->state.load()==app::OperationContext::requested;}));
        }
        state["withdrawals"]=money(cachedView->withdrawals);state["deposits"]=money(cachedView->deposits);
        if(selected.contains("ordinal")) {
            for(std::size_t position=0;position<cachedView->order.size();++position) {
                const auto& id=cachedView->rows[cachedView->order[position]].identity;
                if(id.batch==selectedBatch && id.ordinal==selectedOrdinal) {state["selectedIndex"]=static_cast<int>(position);break;}
            }
        }
        std::set<imports::Occurrence> retained;
        for(const auto& row:cachedView->rows)if(selection.contains(row.identity))retained.insert(row.identity);
        selection=std::move(retained);
        QVariantList selectedIds;QVariantMap selectedKeys;
        for(const auto& id:selection){selectedIds.push_back(QVariantMap{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)}});selectedKeys[text(id.batch)+":"+QString::number(id.ordinal)]=true;}
        state["filter"]=filter;state["transferReceipt"]=transferReceipt;state["transferCandidates"]=transferCandidates;state["candidateLink"]=candidateLink;state["transferMergePreview"]=transferMergePreview;
        state["selection"]=selectedIds;state["selectionKeys"]=selectedKeys;state["selectionCount"]=static_cast<int>(selection.size());
        int focus=-1;
        if(selectionFocus)for(std::size_t i=0;i<cachedView->order.size();++i)if(cachedView->rows[cachedView->order[i]].identity==*selectionFocus){focus=static_cast<int>(i);break;}
        state["selectionIndex"]=focus;
        return {state,cachedView};
    }
    void publishPreview(const app::RulePreviewResult& result,const QVariantList& editor) {
        QStringList lines;
        const auto label=result.unsaved_rules?QString("Unsaved rules"):QString("Saved ruleset revision %1").arg(result.ruleset_revision);
        lines<<label<<"Overlap results are only for the records just evaluated.";
        QVariantList records;
        for(const auto& record:result.records) {
            QVariantList explanations;
            for(const auto& item:record.proposal.explanations) {
                const auto id=text(item.contribution.rule);
                QString shown;
                for(const auto& value:editor) {
                    const auto map=value.toMap();
                    if(map.value("id").toString()==id) {shown=map.value("label").toString();break;}
                }
                if(shown.isEmpty()) shown=ruleLabel(id);
                const auto effect=effectName(item.effect);
                const auto wording=effectWording(item.effect);
                explanations.push_back(QVariantMap{{"rule",id},{"version",QString::number(item.contribution.version)},{"label",shown},{"field",text(item.field)},
                    {"effect",effect},{"wording",wording},{"allocation",item.allocation?QVariant(static_cast<int>(*item.allocation)):QVariant()}});
                lines<<effect+" · "+wording+" · "+shown+" · "+text(item.field);
            }
            QVariantList diffs;
            for(const auto& diff:record.diffs) {
                const auto wording=diffWording(diff.removed,diff.assigned);
                const auto before=text(diff.before_text), after=text(diff.after_text);
                diffs.push_back(QVariantMap{{"field",fieldName(diff.field)},{"allocation",text(diff.allocation)},{"before",before},{"after",after},{"removed",diff.removed},{"assigned",diff.assigned},{"wording",wording}});
                lines<<fieldName(diff.field)+" · "+before+" -> "+after+" · "+wording;
            }
            records.push_back(QVariantMap{{"batch",text(record.identity.batch)},{"ordinal",static_cast<int>(record.identity.ordinal)},{"payee",text(record.proposal.after.payee)},{"explanations",explanations},{"diffs",diffs}});
        }
        rulePreview={{"label",label},{"unsaved",result.unsaved_rules},{"batch",text(result.batch)},{"batchRevision",QString::number(result.batch_revision)},
            {"rulesetRevision",QString::number(result.ruleset_revision)},{"catalogRevision",QString::number(result.catalog_revision)},{"replaceManual",result.replace_manual},
            {"operation",result.unsaved_rules?QString():text(ruleApplyOperation)},{"evaluated",static_cast<int>(result.evaluated)},{"changed",static_cast<int>(result.changed)},
            {"overlap","Overlap results are only for the records just evaluated."},{"records",records},{"summaryText",lines.join('\n')}};
    }
    void execute(const QString& action,const QVariantMap& a, app::OperationContext& context) {
        context.checkpoint();
        if(action!="select" && action!="query" && action!="inspect" && action!="evidence" && action!="backup" && action!="history" && action!="entry_history" && action!="preview_change" && action!="cancel_change" && action!="cancel_merge" && action!="cancel_reapply" && action!="category_select" && action!="preview_category" && action!="cancel_category_preview" && action!="rules_load" && action!="rules_discard" && action!="rules_preview" && action!="rules_prefill") invalidate();
        if(action=="close" || action=="open" || action=="unlock" || action=="create"){preparedImport.reset();preparedMappings.reset();importMappingArgs.clear();importPlan.clear();accountViews.clear();transferReceipt.clear();transferCandidates.clear();candidateLink.clear();transferMergePreview.clear();preparedMerge.reset();mergeOperation.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();}
        if(action=="create" || action=="open" || action=="unlock") {
            if(active) throw std::logic_error("Close the current dataset first");
            path=a.value("path").toString();
            if(action=="create") {
                if(a.value("protected",true).toBool() && a.value("password")!=a.value("confirmation")) throw std::invalid_argument("Passwords do not match");
                const QFileInfo location(path);
                if(!QDir().mkpath(location.absolutePath())) throw std::runtime_error("Cannot create dataset directory");
                gateway->create(path.toStdString(),a.value("protected",true).toBool(),str(a.value("password")),str(a.value("name")),Currency(str(a.value("currency"))),QDate::currentDate().toString(Qt::ISODate).toStdString());
            } else {
                gateway->open_controlled(path.toStdString(),action=="unlock"?std::optional(str(a.value("password"))):std::nullopt,context);
                try { context.seal(); } catch (...) { gateway->close(); throw; }
            }
            active=true; selected.clear(); receipt.clear();clearChanges();
            QSettings settings; settings.setValue("datasetPath",path); settings.sync();
            if(settings.status()!=QSettings::NoError) throw std::runtime_error("Dataset is open, but the startup path could not be saved in Settings");
            return;
        }
        if(action=="close") {gateway->close();clearChanges();active=false;selected.clear();receipt.clear();selectedBatch.clear();filter.clear();projected=true;sort=0;descending=false;return;}
        if(action=="cancel_merge") {preparedMerge.reset();mergeOperation.clear();transferMergePreview.clear();return;}
        if(action=="cancel_reapply") {preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();return;}
        if(!active) throw std::logic_error("Open a dataset first");
        auto& repo=gateway->repository();
        if(action=="transfer_candidates"){
            transferCandidates.clear();candidateLink=a.value("link").toString();
            const auto links=repo.transfers();const auto found=std::find_if(links.begin(),links.end(),[&](const auto& value){return value.link.identity==str(a.value("link"));});
            if(found==links.end())throw std::invalid_argument("Choose a transfer");
            const auto& side=found->link.origin.endpoint.account==repo.account().identity?found->link.origin:found->link.destination;
            bool full=false;
            for(const auto& batch:repo.batches())if(batch.state==app::BatchState::pending && !batch.blocked){
                for(std::size_t ordinal=0;ordinal<batch.count;++ordinal){context.checkpoint("Finding amount-compatible bank entries");const auto row=repo.row({batch.id,ordinal});if(!row.draft || row.decision.disposition!=imports::Disposition::include)continue;
                    for(const auto& allocation:row.draft->transaction.allocations())if(allocation.amount==side.amount){
                        bool used=false;for(const auto& link:links)for(const auto* end:{&link.link.origin,&link.link.destination})if(end->state!=transfers::State::removed){const transfers::Endpoint candidate{repo.account().identity,{batch.id,ordinal},allocation.identity};used|=end->endpoint==candidate || (end->attached_evidence && end->attached_evidence->endpoint==candidate) || (end->proposed_evidence && end->proposed_evidence->endpoint==candidate);}
                        if(used)continue;
                        if(transferCandidates.size()==100){full=true;break;}
                        transferCandidates.push_back(QVariantMap{{"batch",text(batch.id)},{"ordinal",static_cast<int>(ordinal)},{"allocation",text(allocation.identity)},{"label",text(row.draft->date)+" · "+text(row.draft->payee)+" · "+money(allocation.amount)+" · "+text(batch.id)+" / "+QString::number(ordinal+1)}});
                    }
                    if(full)break;
                }
                if(full)break;
            }
            transferReceipt={{"summary",full?"First 100 amount-compatible candidates shown; inspect another source record to match it manually.":"Candidates share an exact amount. Review the bank evidence and confirm explicitly; equal amounts alone do not establish a match."}};
            return;
        }
        if(action.startsWith("transfer_")){
            std::string operation=str(a.value("operation"));
            app::TransferRequest request{};
            if(action=="transfer_merge_apply"){
                if(!preparedMerge || str(a.value("preview"))!=text(mergeOperation))throw std::logic_error("Review the merge preview first");
                request=*preparedMerge;operation=mergeOperation;
            }else{
            request.link=str(a.value("link"));request.expected_sequence=a.value("sequence").toString().toLongLong();
            if(action=="transfer_manual"){
                request.action=app::TransferAction::manual;request.destination=a.value("destination").toString().toLongLong();request.amount=Money::parse(str(a.value("amount")),repo.account().currency);request.date=str(a.value("date"));request.payee=str(a.value("payee"));request.memo=str(a.value("memo"));
            }else if(action=="transfer_from_draft"){
                request.action=app::TransferAction::from_draft;request.origin=imports::Occurrence{selectedBatch,selectedOrdinal};request.allocation=str(a.value("allocation"));request.batch_revision=selectedRevision;request.destination=a.value("destination").toString().toLongLong();
            }else if(action=="transfer_accept")request.action=app::TransferAction::accept;
            else if(action=="transfer_commit")request.action=app::TransferAction::commit_local;
            else if(action=="transfer_clear")request.action=app::TransferAction::clear_local;
            else if(action=="transfer_clear_row"){
                const imports::Occurrence id{str(a.value("batch")),static_cast<std::size_t>(a.value("ordinal").toUInt())};
                const auto account=repo.account().identity;const transfers::Link* found=nullptr;
                for(const auto& value:repo.transfers())for(const auto* side:{&value.link.origin,&value.link.destination}){
                    if(side->state==transfers::State::removed || side->endpoint.account!=account || side->endpoint.entry!=id)continue;
                    if(found)throw std::invalid_argument("Choose the allocation explicitly. This transaction has more than one possible transfer endpoint.");
                    found=&value.link;
                }
                if(!found)throw std::invalid_argument("Transfer entry is unavailable");
                request.action=app::TransferAction::clear_local;request.link=found->identity;
            }else if(action=="transfer_match"){
                request.action=app::TransferAction::propose_match;const imports::Occurrence id{a.contains("batch")?str(a.value("batch")):selectedBatch,a.contains("ordinal")?a.value("ordinal").toUInt():selectedOrdinal};
                const auto row=repo.row(id);if(!row.draft)throw std::invalid_argument("Choose a valid bank entry");
                const auto& allocations=row.draft->transaction.allocations();const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==str(a.value("allocation"));});if(allocation==allocations.end())throw std::invalid_argument("Choose a bank allocation");
                const auto batches=repo.batches();const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==id.batch;});if(batch==batches.end())throw std::invalid_argument("Import unavailable");request.batch_revision=batch->revision;
                request.evidence=transfers::BankEvidence{{repo.account().identity,id,allocation->identity},allocation->amount,row.draft->date};
            }else if(action=="transfer_unmatch"){
                request.action=app::TransferAction::discard_match;for(const auto& value:repo.transfers())if(value.link.identity==request.link){const auto& side=value.link.origin.endpoint.account==repo.account().identity?value.link.origin:value.link.destination;request.evidence=side.proposed_evidence;}
            }else if(action=="transfer_merge"){
                const auto endpoint_of=[&](const char* prefix){const auto name=QString::fromLatin1(prefix);return QVariantMap{{"account",a.value(name+"Account")},{"batch",a.value(name+"Batch")},{"ordinal",a.value(name+"Ordinal")},{"allocation",a.value(name+"Allocation")}};};
                const auto left=read_transfer_choice(repo,endpoint_of("left"));
                const auto right=read_transfer_choice(repo,endpoint_of("right"));
                if(left.fact.endpoint==right.fact.endpoint)throw std::invalid_argument("Choose two allocations");
                if(left.linked && right.linked && left.link!=right.link)throw std::invalid_argument("Choose one transfer relationship. These allocations belong to different links.");
                if(left.linked && right.linked)throw std::invalid_argument("Invalid transfer relationship");
                const bool leftOrigin=left.fact.manually_entered || (!right.fact.manually_entered && left.fact.endpoint.account<=right.fact.endpoint.account);
                const auto& origin=leftOrigin?left:right;
                request.action=app::TransferAction::merge;request.mode=0;request.kept=operation+"-kept";
                request.link=left.linked?left.link:right.linked?right.link:request.link.empty()?operation:request.link;
                request.left=left.fact;request.right=right.fact;request.batch_revision=left.revision;request.right_revision=right.revision;
                request.payee=origin.payee;request.memo=origin.memo;
                const auto preview=repo.preview_transfer(request);
                if(preview.change.units()!=0){
                    preparedMerge=request;mergeOperation=operation;
                    transferMergePreview={{"id",text(operation)},{"note",text(preview.note)},{"balanceImpact",balanceImpact(preview.before,preview.change,preview.after)}};
                    return;
                }
            }else if(action=="transfer_retarget"){
                const auto choice=read_transfer_choice(repo,QVariantMap{{"account",QString::number(repo.account().identity)},{"batch",a.value("batch")},{"ordinal",a.value("ordinal")},{"allocation",a.value("allocation")}});
                request.action=app::TransferAction::retarget;request.kept=operation+"-kept";request.left=choice.fact;request.batch_revision=choice.revision;
                request.payee=choice.payee;request.memo=choice.memo;request.link=choice.linked?choice.link:request.link;
                request.mode=a.value("mode").toInt();
                if(request.mode==1){
                    const auto destinationId=a.value("destination").toString().toLongLong();
                    const auto accounts=repo.accounts();
                    const auto destination=std::find_if(accounts.begin(),accounts.end(),[&](const auto& item){return item.identity==destinationId;});
                    if(destination==accounts.end())throw std::invalid_argument("Choose a destination account");
                    const auto generatedBatch=str(a.value("generatedBatch"));
                    if(generatedBatch.empty())throw std::invalid_argument("Choose a destination account");
                    request.generated=transfers::Endpoint{destination->identity,{generatedBatch,0},"generated"};
                    request.generated_currency=destination->currency.definition_id();
                    request.category_id.clear();request.category_path.clear();
                }else if(request.mode==0){
                    request.category_id=str(a.value("categoryId"));request.category_path=str(a.value("categoryPath"));
                    request.generated.reset();request.generated_currency.clear();
                }else throw std::invalid_argument("Unknown transfer command");
            }else throw std::invalid_argument("Unknown transfer command");
            }
            std::optional<app::TransferReceipt> result;
            try{result=repo.apply_transfer(operation,request);}catch(...){const auto failure=std::current_exception();try{result=repo.transfer_outcome(operation);if(result)result=repo.apply_transfer(operation,request);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Transfer outcome could not be confirmed. Reopen and inspect transfers before retrying.");}if(!result)std::rethrow_exception(failure);}
            transferReceipt={{"operation",text(operation)},{"summary","Transfer operation saved · "+balanceImpact(result->before,result->change,result->after)}};
            transferCandidates.clear();candidateLink.clear();preparedMerge.reset();mergeOperation.clear();transferMergePreview.clear();
            if(selectedBatch.starts_with("~transfer-")){
                bool present=false;const auto account=repo.account().identity;
                for(const auto& value:repo.transfers())for(const auto* side:{&value.link.origin,&value.link.destination})
                    present=present || (side->state!=transfers::State::removed && side->endpoint.account==account && side->endpoint.entry==imports::Occurrence{selectedBatch,selectedOrdinal});
                if(!present){selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();}
            }
            try {if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);} catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Transfer operation was saved durably, but its selection could not be refreshed. Reopen and inspect transfers before retrying.");}
            return;
        }
        if(action=="rename_account" || action=="archive_account" || action=="reactivate_account" || action=="delete_account" || action=="correct_opening" || action=="reassign_currency" || action=="undo_account") {
            auto operation=str(a.value("operation"));if(operation.empty()) operation=identity();
            const bool move=action=="archive_account" || action=="delete_account";
            auto matches=[&](const app::AccountCommandReceipt& saved){
                if(action=="rename_account") return saved.name==str(a.value("name"));
                if(action=="archive_account") return saved.kind==1;
                if(action=="reactivate_account") return saved.kind==2;
                if(action=="delete_account") return saved.kind==3 && saved.retired;
                if(action=="correct_opening") return saved.kind==4 && saved.opening_date==str(a.value("date")) && saved.opening==Money::parse(str(a.value("amount")),saved.currency);
                if(action=="reassign_currency") return saved.kind==5 && saved.currency==Currency(str(a.value("currency")));
                return saved.kind==6;
            };
            auto settle=[&]{
                if(move){
                    selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();receipt.clear();
                    if(!repo.account_selected() || repo.account().archived){
                        const auto live=repo.accounts();
                        if(!live.empty()) repo.select_account(live.front().identity);
                        else repo.release_account_selection();
                    }
                    return;
                }
                if(repo.account_selected() && !selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
            };
            try {
                if(action=="rename_account") repo.rename_account(operation,str(a.value("name")));
                else if(action=="archive_account") repo.archive_account(operation);
                else if(action=="reactivate_account") repo.reactivate_account(operation);
                else if(action=="delete_account") repo.delete_account(operation);
                else if(action=="correct_opening") {const auto acct=repo.account();repo.correct_opening(operation,str(a.value("date")),Money::parse(str(a.value("amount")),acct.currency));}
                else if(action=="reassign_currency") repo.reassign_currency(operation,Currency(str(a.value("currency"))));
                else repo.undo_account_operation(operation,str(a.value("undoOf")));
            } catch(...) {
                const auto failure=std::current_exception();
                std::optional<app::AccountCommandReceipt> saved;
                try {saved=repo.account_command_outcome(operation);}
                catch(...) {gateway->close();active=false;selected.clear();receipt.clear();selectedBatch.clear();throw std::runtime_error("Account command outcome is uncertain. Reopen and inspect the account before retrying.");}
                if(!saved || !matches(*saved)) std::rethrow_exception(failure);
            }
            settle();
            return;
        }
        if(action=="select_account" || action=="create_account") {
            bool valid=false;const auto id=a.value("id").toString().toLongLong(&valid);
            if(!valid || id<=0)throw std::invalid_argument("Choose a valid account");
            if(action=="create_account") {
                // Identity and date come from the unchanged creation request, so
                // retry after a lost acknowledgement cannot create another account.
                const auto name=str(a.value("name")),date=str(a.value("date"));const Currency currency(str(a.value("currency")));
                try {repo.create_account(id,name,currency,date);}
                catch(...) {
                    const auto failure=std::current_exception();std::optional<app::AccountCreation> saved;
                    try {saved=repo.account_creation_outcome(id);}
                    catch(...) {
                        gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();
                        throw std::runtime_error("Account creation could not be confirmed. Reopen and inspect the account list before retrying.");
                    }
                    if(!saved || saved->name!=name || saved->currency!=currency || saved->date!=date)std::rethrow_exception(failure);
                }
            }
            std::optional<std::int64_t> priorAccount;std::optional<AccountView> priorView;
            if(repo.account_selected()){priorAccount=repo.account().identity;priorView=AccountView{filter,projected,sort,descending,selectedBatch,selectedOrdinal,selection,selectionAnchor,selectionFocus};}
            try {repo.select_account(id);}
            catch(...) {
                if(action!="create_account")throw;
                gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();
                throw std::runtime_error("Account was created durably, but could not be selected. Reopen and inspect the account list before retrying.");
            }
            selected.clear();selectedBatch.clear();receipt.clear();clearChanges();
            selection.clear();selectionAnchor.reset();selectionFocus.reset();filter.clear();projected=true;sort=0;descending=false;
            if(priorAccount) accountViews.insert_or_assign(*priorAccount,std::move(*priorView));
            if(const auto saved=accountViews.find(id);saved!=accountViews.end()){
                const auto& view=saved->second;filter=view.filter;projected=view.projected;sort=view.sort;descending=view.descending;selection=view.selection;selectionAnchor=view.anchor;selectionFocus=view.focus;
                if(!view.batch.empty())inspect(view.batch,view.ordinal,context);
            }
            if(a.value("reveal").toBool()){filter.clear();projected=true;selection.clear();selectionAnchor.reset();selectionFocus.reset();inspect(str(a.value("batch")),a.value("ordinal").toUInt(),context);selection.insert({selectedBatch,selectedOrdinal});selectionFocus=imports::Occurrence{selectedBatch,selectedOrdinal};}
        }
        else if(action=="category_list") {catalog();}
        else if(action=="category_select") {selectCategory(str(a.value("id")));}
        else if(action=="cancel_category_preview") {preparedCategory.reset();categoryOperation.clear();categoryPreview.clear();}
        else if(action=="preview_category") {
            preparedCategory.reset();categoryOperation.clear();categoryPreview.clear();
            const auto current=repo.category_catalog();catalog();
            if(a.value("revision").toString().toLongLong()!=current.revision)throw std::invalid_argument("Categories changed. Refresh and review the category edit.");
            const auto actionName=a.value("action").toString();
            app::CategoryAction actionType;
            if(actionName=="create")actionType=app::CategoryAction::create;
            else if(actionName=="rename")actionType=app::CategoryAction::rename;
            else if(actionName=="move")actionType=app::CategoryAction::move;
            else if(actionName=="archive")actionType=app::CategoryAction::archive;
            else if(actionName=="unarchive")actionType=app::CategoryAction::unarchive;
            else if(actionName=="delete")actionType=app::CategoryAction::erase;
            else throw std::invalid_argument("Unknown category action");
            app::CategoryRequest request{actionType,actionType==app::CategoryAction::create?identity():str(a.value("id")),str(a.value("name")),str(a.value("parent")),a.value("group").toBool()?CategoryKind::group:CategoryKind::leaf};
            auto preview=repo.preview_category_change(request);preparedCategory=preview;categoryOperation=identity();
            categoryPreview={{"id",text(categoryOperation)},{"summary",actionName+" · "+(preview.before?text(preview.before->name):"(new)")+" → "+text(preview.after.name)+"\nParent: "+(preview.after.parent.empty()?QString("Standalone"):categoryById.value(text(preview.after.parent)).toMap().value("path",text(preview.after.parent)).toString())+"\n"+(preview.after.deleted?"Deleted":preview.after.archived?"Archived":"Active")}};
        } else if(action=="apply_category") {
            categoryReceipt.clear();
            if(!preparedCategory || str(a.value("preview"))!=categoryOperation)throw std::logic_error("Review a category change first");
            const auto preview=*preparedCategory;std::optional<app::CategoryReceipt> result;
            try {result=repo.apply_category_change(categoryOperation,preview.request,preview.catalog_revision);}
            catch(...) {
                const auto failure=std::current_exception();
                try {result=repo.category_change_outcome(categoryOperation);}
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("Category change outcome is uncertain. Reopen the dataset and inspect category history before retrying.");}
                if(!result)std::rethrow_exception(failure);
            }
            categoryReceipt={{"operation",text(result->operation)},{"summary","Category change saved at catalog revision "+QString::number(result->catalog_after)}};
            preparedCategory.reset();categoryOperation.clear();categoryPreview.clear();
            try {catalog();selectCategory(result->after.identity);if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);}
            catch(...) {const auto saved=categoryReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();categoryReceipt=saved;throw std::runtime_error("Category change was saved durably, but its refreshed view could not be loaded. Reopen and inspect category history; do not apply it again.");}
        } else if(action=="history") {listHistory(a.value("before").toString().toLongLong(),a.value("append").toBool());}
        else if(action=="entry_history") {
            const imports::Occurrence id{str(a.value("batch")),a.value("ordinal").toUInt()};inspect(id.batch,id.ordinal,context);entryDetails(id);
            if(!historyLoaded)listHistory();
        } else if(action=="cancel_change") {preparedChange.reset();preparedOperation.clear();changePreview.clear();}
        else if(action=="preview_change") {
            preparedChange.reset();preparedOperation.clear();changePreview.clear();
            const auto kind=a.value("kind").toString();
            const bool historyUndo=kind=="undo" && a.contains("batch") && a.contains("ordinal");
            if(!historyUndo && (selectedBatch.empty() || !selected.value("committed").toBool()))throw std::logic_error("Select an ordinary committed transaction first");
            const imports::Occurrence id=historyUndo?imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}:imports::Occurrence{selectedBatch,selectedOrdinal};
            const auto entry=repo.committed_entry(id);
            if(kind!="undo" && (!selectedCommittedToken || entry.token!=*selectedCommittedToken))throw std::logic_error("The committed record changed. Refresh and review before correcting it.");
            app::ChangeRequest request{app::ChangeKind::remove,id,std::nullopt,{}};
            if(kind=="correct") {request.kind=app::ChangeKind::correct;request.correction=editedDraft(a,entry.current.draft.transaction.original_imported_amount(),repo,&entry.current.draft,&entry.source);}
            else if(kind=="undo") {request.kind=app::ChangeKind::undo;request.undo_of=str(a.value("operation"));}
            else if(kind!="remove")throw std::invalid_argument("Unknown committed change");
            auto preview=repo.preview_committed_change(request);
            preparedOperation=identity();preparedChange=preview;
            auto impact=balanceImpact(preview.before,preview.change,preview.after);
            if(!preview.relationship.empty())impact+="\n"+text(preview.relationship);
            changePreview={{"id",text(preparedOperation)},{"title",changeName(request.kind)+" preview"},
                {"applyLabel",request.kind==app::ChangeKind::correct?"Apply correction":request.kind==app::ChangeKind::remove?"Remove transaction":"Apply Undo"},
                {"balanceImpact",impact},
                {"snapshots",QVariantList{snapshotMap(preview.before_entry,"Before"),snapshotMap(preview.after_entry,"After")}}};
        } else if(action=="apply_change") {
            changeReceipt.clear();
            if(!preparedChange || str(a.value("preview"))!=preparedOperation)throw std::logic_error("Review a fresh committed-change preview first");
            const auto preview=*preparedChange;
            std::optional<app::ChangeReceipt> result;
            context.checkpoint("Applying committed change");
            try {result=repo.apply_committed_change_with_catalog(preparedOperation,preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);}
            catch(...) {
                const auto failure=std::current_exception();
                try {result=repo.committed_change_outcome(preparedOperation);}
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("Committed change outcome is uncertain. Reopen this dataset and inspect its committed history before retrying.");}
                if(!result)std::rethrow_exception(failure);
            }
            changeReceipt={{"operation",text(result->operation)},{"summary",changeName(result->kind)+" applied · "+balanceImpact(result->before,result->change,result->after)}};
            preparedChange.reset();preparedOperation.clear();changePreview.clear();
            try {inspect(result->identity.batch,result->identity.ordinal,context);entryDetails(result->identity);if(historyLoaded)listHistory();}
            catch(...) {
                const auto saved=changeReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();changeReceipt=saved;
                throw std::runtime_error("Committed change "+result->operation+" was saved durably, but its refreshed view could not be loaded. Reopen this dataset and inspect committed history; do not apply the change again.");
            }
        } else if(action=="cancel_bulk") {preparedBulk.reset();bulkOperation.clear();bulkPreview.clear();}
        else if(action=="preview_bulk") {
            preparedBulk.reset();bulkOperation.clear();bulkPreview.clear();
            catalog();
            const auto kind=a.value("kind").toString();
            app::BulkRequest request{kind=="remove"?app::ChangeKind::remove:kind=="category"?app::ChangeKind::correct:app::ChangeKind::undo,{selection.begin(),selection.end()},str(a.value("category")),{}};
            if(kind=="undo"){request.undo_of=str(a.value("operation"));const auto original=repo.bulk_outcome(request.undo_of);if(!original)throw std::invalid_argument("Bulk operation was not found");request.identities=original->request.identities;}
            else if(kind!="remove" && kind!="category")throw std::invalid_argument("Unknown bulk action");
            auto preview=repo.preview_bulk(request);QStringList lines;
            for(const auto& entry:preview.entries){
                lines<<text(entry.before_entry.draft.date)+" · "+text(entry.before_entry.draft.payee)+" · "+money(entry.before_entry.draft.transaction.effective_total());
                lines<<(entry.before_entry.active?"Before: active":"Before: removed")<< (entry.after_entry.active?"After: active":"After: removed");
                const auto& before=entry.before_entry.draft.transaction.allocations();const auto& after=entry.after_entry.draft.transaction.allocations();
                for(std::size_t i=0;i<before.size();++i)lines<<"  "+money(before[i].amount)+" · "+allocationMap(before[i],categoryById).value("categoryDisplay").toString()+" → "+allocationMap(after[i],categoryById).value("categoryDisplay").toString()+" · "+text(before[i].memo);
            }
            bulkOperation=identity();preparedBulk=preview;
            bulkPreview={{"id",text(bulkOperation)},{"title",(request.kind==app::ChangeKind::remove?QString("Delete "):request.kind==app::ChangeKind::correct?QString("Change category for "):QString("Undo changes to "))+QString::number(preview.entries.size())+" transactions"},{"impact",balanceImpact(preview.before,preview.change,preview.after)},{"lines",lines}};
        } else if(action=="apply_bulk") {
            bulkReceipt.clear();if(!preparedBulk || str(a.value("preview"))!=bulkOperation)throw std::logic_error("Review a fresh bulk preview first");
            std::optional<app::BulkReceipt> result;
            try{result=repo.apply_bulk(bulkOperation,*preparedBulk);}
            catch(...){const auto failure=std::current_exception();try{result=repo.bulk_outcome(bulkOperation);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Bulk outcome is uncertain. Reopen and inspect committed history before retrying.");}if(!result)std::rethrow_exception(failure);}
            bulkReceipt={{"operation",text(result->operation)},{"summary",changeName(result->request.kind)+" applied to "+QString::number(result->entries.size())+" transactions · "+balanceImpact(result->before,result->change,result->after)}};
            preparedBulk.reset();bulkOperation.clear();bulkPreview.clear();
            selected.clear();selectedBatch.clear();selectedCommittedToken.reset();
            try{if(historyLoaded)listHistory();}catch(...){throw std::runtime_error("Bulk change was saved durably, but history could not be refreshed. Reopen and inspect committed history; do not apply it again.");}
        } else if(action=="rules_reapply_preview") {
            if(!repo.rules_supported()) throw std::invalid_argument("Rules require a new experimental dataset");
            preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();
            auto preview=repo.preview_rule_reapply(a.value("replaceManual").toBool(),str(a.value("undoOf")),context);
            QStringList lines;
            for(const auto& record:preview.records){
                lines<<text(record.before.date)+" · "+text(record.before.payee);
                for(const auto& diff:record.diffs) lines<<fieldName(diff.field)+" · "+text(diff.before_text)+" -> "+text(diff.after_text)+" · "+diffWording(diff.removed,diff.assigned);
            }
            for(const auto& skip:preview.skips) lines<<text(skip.note);
            QString omitted;
            if(lines.size()>40){omitted=QString::number(lines.size()-40)+" lines omitted.";lines=lines.mid(0,40);lines<<omitted;}
            reapplyOperation=identity();preparedReapply=std::move(preview);
            const auto& request=preparedReapply->request;
            reapplyPreview={{"id",text(reapplyOperation)},{"title",request.undo_of.empty()?QString("Reapply rules to this account"):QString("Undo rule reapply")},
                {"evaluated",static_cast<int>(request.evaluated)},{"changed",static_cast<int>(request.changed)},
                {"impact",balanceImpact(preparedReapply->before,preparedReapply->change,preparedReapply->after)+"\nAccount balance is unchanged."},
                {"lines",lines},{"omitted",omitted},{"replaceManual",request.replace_manual},
                {"rulesetRevision",QString::number(request.ruleset_revision)},{"catalogRevision",QString::number(request.catalog_revision)},
                {"undoOf",text(request.undo_of)},{"previewSequence",QString::number(request.preview_sequence)}};
        } else if(action=="rules_reapply_apply") {
            reapplyReceipt.clear();
            if(!preparedReapply || str(a.value("preview"))!=reapplyOperation) throw std::logic_error("Review a fresh rule reapply preview first");
            const auto request=preparedReapply->request;const auto operation=reapplyOperation;
            std::optional<app::RuleReapplyReceipt> result;
            try {result=repo.apply_rule_reapply(operation,request,context);}
            catch(...) {
                const auto failure=std::current_exception();
                try {result=repo.rule_reapply_outcome(operation);}
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("Rule reapply outcome is uncertain. Reopen and inspect committed history before retrying.");}
                if(!result){preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();std::rethrow_exception(failure);}
            }
            reapplyReceipt=result->operation.empty()?QVariantMap{{"operation",QString()},{"summary","No committed rows were changed"}}
                :QVariantMap{{"operation",text(result->operation)},{"summary",(request.undo_of.empty()?QString("Rule reapply "):QString("Undo rule reapply "))+text(result->operation)+" changed "+QString::number(result->request.changed)+" committed row(s) · "+balanceImpact(result->before,result->change,result->after)}};
            preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();
            try {if(historyLoaded)listHistory();if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);}
            catch(...) {
                const auto saved=reapplyReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();reapplyReceipt=saved;
                const auto savedOperation=saved.value("operation").toString();
                throw std::runtime_error(savedOperation.isEmpty()?"Unable to refresh verified dataset state. Close and reopen before continuing.":ruleReapplyRefreshFailure(savedOperation).toStdString());
            }
        } else if(action=="import") {
            QFile file(a.value("path").toString()); const QFileInfo info(file);
            if(!info.isFile() || !file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read statement file");
            if(file.size()>static_cast<qint64>(qif::max_file)) throw std::invalid_argument("Statement exceeds 64 MiB limit");
            QByteArray bytes;
            while(!file.atEnd()) {
                context.checkpoint("Reading statement file");
                auto chunk=file.read(64*1024);
                if(chunk.isEmpty() && !file.atEnd()) throw std::runtime_error("Statement read failed");
                bytes.append(chunk);
                if(bytes.size()>static_cast<qsizetype>(qif::max_file)) throw std::invalid_argument("Statement exceeds 64 MiB limit");
            }
            if(file.error()!=QFileDevice::NoError || !file.atEnd()) throw std::runtime_error("Statement read failed or exceeded limit");
            if(bytes.size()>static_cast<qsizetype>(qif::max_file)) throw std::invalid_argument("Statement exceeds 64 MiB limit");
            const auto profile=a.value("profile").toString();
            if(profile!="DMY" && profile!="MDY") throw std::invalid_argument("Select DMY or MDY before importing");
            const auto suffix=info.suffix().toLower().toStdString();
            const std::string requested=suffix=="qif"||suffix=="ofx"||suffix=="qfx"||suffix=="qbo"||suffix=="aso"?suffix:"";
            const auto owned=bytes.toStdString();
            const auto document=statements::inspect(owned,requested,[&]{context.checkpoint();});
            if(document.format=="qif" && document.explicit_accounts && (document.diagnostic || !document.admissible)) throw std::invalid_argument("Unsupported account section structure; no import was saved. The original file is unchanged.");
            if(!document.admissible) throw std::invalid_argument(document.refusal.empty()?"Statement exceeds a resource limit. The original file is unchanged.":document.refusal);
            if(!document.refusal.empty() || (document.format!="qif" && document.diagnostic)) throw std::invalid_argument(document.refusal.empty()?"Unsupported account section structure; no import was saved. The original file is unchanged.":document.refusal);
            const auto order=document.format=="qif"?(profile=="DMY"?qif::DateOrder::dmy:qif::DateOrder::mdy):qif::DateOrder::dmy;
            if(document.format!="qif" || document.explicit_accounts) {
                preparedImport=PreparedImport{identity(),owned,document.format,document,order};preparedMappings.reset();importMappingArgs.clear();importPlan.clear();
                const auto home=repo.accounts().empty()?std::string{}:repo.accounts().front().currency.code();
                for(std::size_t i=0;i<document.accounts.size();++i){const auto& source=document.accounts[i];importPlan.push_back(QVariantMap{{"source",static_cast<int>(i)},{"name",text(source.name)},{"type",text(source.type)},{"reported",source.reported_balance?text(*source.reported_balance):QString("Not supplied")},{"currency",text(document.format=="qif"?home:source.currency_code)}});}
                return;
            }
            const auto batch=repo.ingest_controlled(identity(),owned,order,context);
            projected=true; selected.clear();selectedBatch.clear();
            if(batch.count && !batch.processing_incomplete) inspect(batch.id,0,context);
            else selected={{"batch",text(batch.id)},{"editable",false},{"source",evidencePreview(owned)}};
        } else if(action=="cancel_import_mapping") {preparedImport.reset();preparedMappings.reset();importMappingArgs.clear();importPlan.clear();
        } else if(action=="map_import") {
            if(!preparedImport)throw std::logic_error("Choose a source file first");
            if(preparedMappings && importMappingArgs!=a.value("mappings").toList())throw std::logic_error("Retry the unchanged account mappings or cancel this mapping request");
            std::vector<app::ImportAccountMapping> mappings;auto next=repo.next_account_identity();
            for(const auto& value:a.value("mappings").toList()) {
                const auto item=value.toMap();const auto index=item.value("source").toUInt();
                if(index>=preparedImport->document.accounts.size())throw std::invalid_argument("Choose a valid source account");
                if(item.value("create").toBool()) {
                    if(next==INT64_MAX)throw std::overflow_error("Account identities exhausted");
                    mappings.push_back({index,next,app::AccountCreation{next,preparedImport->document.accounts[index].name,Currency(str(item.value("currency"))),str(item.value("date"))}});++next;
                }else mappings.push_back({index,item.value("account").toString().toLongLong(),{}});
            }
            if(!preparedMappings){preparedMappings=mappings;importMappingArgs=a.value("mappings").toList();}
            std::optional<app::ImportGroup> saved;
            try{saved=repo.prepare_import_group(preparedImport->id,preparedImport->bytes,preparedImport->profile,*preparedMappings,context,preparedImport->label);}
            catch(...){const auto failure=std::current_exception();try{saved=repo.import_group_outcome(preparedImport->id);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();preparedImport.reset();preparedMappings.reset();importPlan.clear();throw std::runtime_error("Import capture outcome is uncertain. Reopen and inspect the account imports before retrying.");}if(!saved){preparedMappings.reset();std::rethrow_exception(failure);}}
            auto group=*saved;
            preparedImport.reset();importPlan.clear();
            if(group.state==app::GroupState::aborted)throw std::runtime_error("This import was already aborted; choose the file again to start a new import.");
            accountViews.insert_or_assign(repo.account().identity,AccountView{filter,projected,sort,descending,selectedBatch,selectedOrdinal,selection,selectionAnchor,selectionFocus});
            try{repo.select_account(group.accounts.front().account);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Import source and accounts were saved, but selection failed. Reopen and resume or abort the retained import.");}selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();filter.clear();projected=true;
            if(group.state==app::GroupState::captured){auto sealed=app::OperationContext::sealed;if(!context.state.compare_exchange_strong(sealed,app::OperationContext::available))throw std::logic_error("Invalid import cancellation gate");group=resumeGroup(group.id,context);}

            if(!group.accounts.empty()){const auto batches=repo.batches();const auto b=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==group.accounts.front().batch;});if(b!=batches.end() && b->count)inspect(b->id,0,context);}
        } else if(action=="group_balance") {
            const auto review=[&]{repo.review_import_balance(str(a.value("group")),a.value("source").toUInt(),a.value("revision").toLongLong(),static_cast<app::ReportedBalanceChoice>(a.value("choice").toInt()),str(a.value("date")));};
            try{review();}catch(...){const auto failure=std::current_exception();if(!durableGroup(str(a.value("group"))))std::rethrow_exception(failure);review();}
        } else if(action=="group_abort") {
            try{repo.abort_import_group(str(a.value("group")));}catch(...){const auto failure=std::current_exception();const auto saved=durableGroup(str(a.value("group")));if(!saved || saved->state!=app::GroupState::aborted)std::rethrow_exception(failure);repo.abort_import_group(saved->id);}selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();filter.clear();projected=true;
        } else if(action=="resume") {
            for(const auto& group:repo.import_groups())for(const auto& member:group.accounts)if(member.batch==str(a.value("batch"))){resumeGroup(group.id,context);projected=true;selected.clear();selectedBatch.clear();if(repo.parsed_source(member.batch).records.size())inspect(member.batch,0,context);return;}

            const auto batch=repo.resume(str(a.value("batch")),a.value("revision").toLongLong(),context);
            projected=true;selected.clear();selectedBatch.clear();
            if(batch.count && !batch.processing_incomplete)inspect(batch.id,0,context);
        } else if(action=="evidence") {
            selectedBatch.clear();selected={{"batch",a.value("batch")},{"editable",false},{"source",evidencePreview(repo.evidence(str(a.value("batch"))))}};
        } else if(action=="select") {
            if(!cachedView)throw std::invalid_argument("No ledger is available for selection");
            const int index=a.value("row",-1).toInt();
            if(index<0 || static_cast<std::size_t>(index)>=cachedView->order.size())throw std::invalid_argument("Selection row is unavailable");
            const auto id=cachedView->rows[cachedView->order[static_cast<std::size_t>(index)]].identity;
            auto next=a.value("firstSearchSelection").toBool()?std::set<imports::Occurrence>{}:selection;
            if(a.value("range").toBool() && selectionAnchor){
                std::size_t anchor=static_cast<std::size_t>(index);
                for(std::size_t i=0;i<cachedView->order.size();++i)if(cachedView->rows[cachedView->order[i]].identity==*selectionAnchor){anchor=i;break;}
                const auto first=std::min(anchor,static_cast<std::size_t>(index)),last=std::max(anchor,static_cast<std::size_t>(index));
                if(last-first>=4096)throw std::invalid_argument("Select at most 4096 transactions per operation");
                if(!a.value("toggle").toBool())next.clear();
                for(auto i=first;i<=last;++i)next.insert(cachedView->rows[cachedView->order[i]].identity);
            } else {
                if(a.value("toggle").toBool()){if(!next.erase(id))next.insert(id);}
                else {next.clear();next.insert(id);}
            }
            if(next.size()>4096)throw std::invalid_argument("Select at most 4096 transactions per operation");
            selection=std::move(next);selectionFocus=id;if(!a.value("range").toBool())selectionAnchor=id;
        } else if(action=="inspect") {inspect(str(a.value("batch")),a.value("ordinal").toUInt(),context);selection={{selectedBatch,selectedOrdinal}};selectionAnchor=imports::Occurrence{selectedBatch,selectedOrdinal};selectionFocus=selectionAnchor;}
        else if(action=="save") {
            if(selectedBatch.empty()) throw std::logic_error("Select a draft first");
            auto row=repo.row({selectedBatch,selectedOrdinal});
            if(!row.draft && !row.source.repairable) throw std::invalid_argument("This record cannot be repaired");
            const auto edit=editedDraft(a,row.source.original_amount,repo,row.draft?&*row.draft:nullptr,&row.source);
            repo.save({selectedBatch,selectedOrdinal},selectedRevision,edit); inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="decide") {
            imports::Decision decision{imports::Disposition::include,{}};
            if(a.value("duplicate").toBool()) decision={imports::Disposition::duplicate,imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}};
            repo.decide({selectedBatch,selectedOrdinal},selectedRevision,decision);inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="opening") {
            const auto acct=repo.account();repo.opening(acct.revision,str(a.value("date")),Money::parse(str(a.value("amount")),acct.currency));
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="commit" || action=="discard") {
            const auto batches=repo.batches(); const auto b=std::find_if(batches.begin(),batches.end(),[&](const auto& item){return a.contains("batch")?item.id==str(a.value("batch")):(item.state==app::BatchState::pending || item.state==app::BatchState::captured);});
            if(b==batches.end()) throw std::logic_error("No pending import");
            if(action=="discard") repo.discard(b->id,b->revision);
            else {
                const auto operation="commit-"+b->id+"-"+std::to_string(b->revision);
                auto result=repo.durable_outcome(operation);
                if(!result) {
                    context.checkpoint("Committing import");
                    try { result=repo.commit(b->id,b->revision,operation); }
                    catch (...) {
                        const auto failure=std::current_exception();
                        context.checkpoint("Checking durable commit outcome");
                        try { result=repo.durable_outcome(operation); }
                        catch (...) {
                            gateway->close();active=false;selected.clear();receipt.clear();selectedBatch.clear();
                            throw std::runtime_error("Commit outcome is uncertain. Reopen this dataset to resolve its durable receipt before retrying.");
                        }
                        if(!result) std::rethrow_exception(failure);
                    }
                }
                receipt={{"operation",text(result->operation)},{"included",static_cast<int>(result->included)},{"excluded",static_cast<int>(result->excluded)},
                    {"before",money(result->before)},{"change",money(result->change)},{"after",money(result->after)}};
            }
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="backup") gateway->backup_controlled(str(a.value("path")),context);
        else if(action=="query") {
            // Validate and evaluate before publishing the new query state. A bad
            // query must not enter the generic document-refresh failure path.
            app::LedgerSearch validate(str(a.value("filter")),QDate::currentDate().year());
            const auto oldFilter=filter;const auto oldSort=sort;const auto oldDescending=descending,oldProjected=projected;
            const auto oldView=cachedView;auto oldLedger=std::move(cachedLedger);
            const auto oldSelected=selected;const auto oldBatch=selectedBatch;const auto oldOrdinal=selectedOrdinal;
            const auto oldSelection=selection;const auto oldAnchor=selectionAnchor,oldFocus=selectionFocus;
            try {
            cachedLedger=oldLedger;
            if(a.contains("restoreSelectionIds")){
                const auto ids=a.value("restoreSelectionIds").toList();if(ids.size()>4096)throw std::invalid_argument("Selection limit exceeded");selection.clear();
                for(const auto& value:ids){const auto id=value.toMap();selection.insert({str(id.value("batch")),id.value("ordinal").toUInt()});}
                selectionAnchor=selection.empty()?std::optional<imports::Occurrence>{}:*selection.begin();
                selectionFocus=selectionAnchor;
            }
            if(a.contains("restoreSelection")) {
                const auto restore=a.value("restoreSelection").toMap();
                if(restore.contains("ordinal")) inspect(str(restore.value("batch")),restore.value("ordinal").toUInt(),context);
                else {selected.clear();selectedBatch.clear();selectedOrdinal=0;}
            }
            const auto nextProjected=a.value("projected",true).toBool();
            if(projected!=nextProjected) cachedLedger.reset();
            cachedView.reset();
            filter=a.value("filter").toString();sort=a.value("sort",0).toInt();descending=a.value("descending",false).toBool();projected=nextProjected;
            (void)snapshot(&context);
            } catch(...) {
                filter=oldFilter;sort=oldSort;descending=oldDescending;projected=oldProjected;cachedView=oldView;cachedLedger=std::move(oldLedger);
                selected=oldSelected;selectedBatch=oldBatch;selectedOrdinal=oldOrdinal;
                selection=oldSelection;selectionAnchor=oldAnchor;selectionFocus=oldFocus;
                throw;
            }
        } else if(action=="rules_load" || action=="rules_discard" || action=="rules_save" || action=="rules_preview" || action=="rules_apply" || action=="rules_prefill") {
            if(!repo.rules_supported()) throw std::invalid_argument("Rules require a new experimental dataset");
            auto closeUnknown=[&](const char* message){gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error(message);};
            auto refreshFailed=[&](const QString& sentence){const auto saved=ruleReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();ruleReceipt=saved;throw std::runtime_error(sentence.toStdString());};
            const auto images=[&]{
                const auto rows=a.value("rules").toList();
                if(rows.size()>static_cast<qsizetype>(rules::EvaluationLimits::rules)) throw std::invalid_argument("Too many rules");
                std::vector<app::RuleDefinitionImage> ordered;for(const auto& value:rows) ordered.push_back(imageFrom(value.toMap()));return ordered;
            };
            if(action=="rules_load" || action=="rules_discard") {loadRules();if(action=="rules_discard"){rulePreview.clear();preparedApply.reset();}}
            else if(action=="rules_prefill") {
                if(selectedBatch.empty()) throw std::logic_error("Select a record first");
                auto row=repo.row({selectedBatch,selectedOrdinal});
                const auto kind=a.value("kind").toString();
                std::string source;auto scope=rules::Scope::transaction;auto input=rules::InputField::payee;
                if(kind=="payee") source=row.source.payee;
                else if(kind=="memo") {
                    const auto index=a.value("allocation").toInt();
                    std::optional<imports::DraftRow> committedDraft;
                    const imports::DraftRow* current=row.draft?&*row.draft:nullptr;
                    if(selected.value("committed").toBool()){committedDraft=repo.committed_entry({selectedBatch,selectedOrdinal}).current.draft;current=&*committedDraft;}
                    std::optional<std::size_t> sourceOrdinal;
                    if(current && index>=0 && static_cast<std::size_t>(index)<current->transaction.allocations().size())sourceOrdinal=current->transaction.allocations()[static_cast<std::size_t>(index)].source_ordinal;
                    else if(!current && row.source.repairable && index>=0 && static_cast<std::size_t>(index)<row.source.source_allocation_fields.size())sourceOrdinal=static_cast<std::size_t>(index);
                    if(!sourceOrdinal || *sourceOrdinal>=row.source.source_allocation_fields.size())throw std::invalid_argument("Manual splits are not rule sources");
                    const auto ordinal=*sourceOrdinal;
                    source=row.source.source_allocation_fields[ordinal].memo;scope=rules::Scope::split;input=rules::InputField::memo;
                } else throw std::invalid_argument("Unknown rule source");
                app::RuleDefinitionImage rule;rule.identity=identity();rule.enabled=true;rule.scope=scope;rule.input=input;rule.expression=app::quoted_field_pattern(source);
                rulePrefill=ruleMap(rule);
            } else if(action=="rules_preview") {
                if(qEnvironmentVariableIsSet("SPENTIT_RULE_CANCEL_WINDOW"))
                    for(int i=0;i<200 && context.state.load()==app::OperationContext::available;++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                app::RulePreviewRequest request;
                request.batch=str(a.value("batch"));
                request.batch_revision=a.value("batchRevision").toString().isEmpty()?-1:a.value("batchRevision").toString().toLongLong();
                request.replace_manual=a.value("replaceManual").toBool();
                std::optional<std::vector<app::RuleDefinitionImage>> editor;
                if(a.value("unsaved").toBool()) editor=images();
                request.editor=editor;
                const auto result=repo.preview_pending_rules(request,context);
                if(result.unsaved_rules) preparedApply.reset();
                else {
                    if(ruleApplyOperation.empty()) ruleApplyOperation=identity();
                    preparedApply=app::RuleApplyRequest{result.batch,result.batch_revision,result.ruleset_revision,result.catalog_revision,result.replace_manual};
                }
                publishPreview(result,editor?a.value("rules").toList():QVariantList{});
            } else if(action=="rules_save") {
                ruleReceipt.clear();
                if(!rulesLoaded) loadRules();
                const auto expected=ruleRevision;
                const auto operation=identity();
                std::optional<app::RuleSaveReceipt> result;
                try {result=repo.save_rules(operation,{images()},expected,context);}
                catch(...) {
                    const auto failure=std::current_exception();
                    try {result=repo.rule_save_outcome(operation);}
                    catch(...) {closeUnknown("Rule save outcome is uncertain. Reopen the dataset and inspect rule history before retrying.");}
                    if(!result) std::rethrow_exception(failure);
                }
                ruleReceipt=result->unchanged?QVariantMap{{"operation",QString()},{"unchanged",true},{"summary","Rules unchanged"}}
                    :QVariantMap{{"operation",text(result->operation)},{"unchanged",false},{"summary","Rules saved at revision "+QString::number(result->ruleset_after)}};
                rulePreview.clear();preparedApply.reset();ruleApplyOperation.clear();
                try {
                    if(qEnvironmentVariableIsSet("SPENTIT_RULE_REFRESH_FAIL")) throw std::runtime_error("refresh");
                    loadRules();if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
                } catch(...) {const auto saved=ruleReceipt.value("operation").toString();refreshFailed(saved.isEmpty()?QString("Unable to refresh verified dataset state. Close and reopen before continuing."):ruleSaveRefreshFailure(saved));}
            } else if(action=="rules_apply") {
                ruleReceipt.clear();
                if(!preparedApply || ruleApplyOperation.empty()) throw std::logic_error("Review a saved rules preview first");
                const auto request=*preparedApply;
                if(str(a.value("batch"))!=request.batch || a.value("batchRevision").toString()!=QString::number(request.batch_revision) || a.value("rulesetRevision").toString()!=QString::number(request.ruleset_revision) || a.value("catalogRevision").toString()!=QString::number(request.catalog_revision) || a.value("replaceManual").toBool()!=request.replace_manual)
                    throw std::invalid_argument("Rules, categories, or the import changed; refresh before applying");
                const auto operation=ruleApplyOperation;
                std::optional<app::RuleApplyReceipt> result;
                try {result=repo.apply_pending_rules(operation,request,context);}
                catch(...) {
                    const auto failure=std::current_exception();
                    try {result=repo.rule_apply_outcome(operation);}
                    catch(...) {closeUnknown("Rule application outcome is uncertain. Reopen the dataset and inspect the pending drafts before retrying.");}
                    if(!result) {ruleApplyOperation.clear();preparedApply.reset();rulePreview.clear();std::rethrow_exception(failure);}
                }
                ruleReceipt=result->operation.empty()?QVariantMap{{"operation",QString()},{"summary","No pending drafts were changed"}}
                    :QVariantMap{{"operation",text(result->operation)},{"summary","Rule application "+text(result->operation)+" changed "+QString::number(result->changed)+" draft(s)"}};
                rulePreview.clear();preparedApply.reset();ruleApplyOperation.clear();
                try {
                    if(qEnvironmentVariableIsSet("SPENTIT_RULE_REFRESH_FAIL")) throw std::runtime_error("refresh");
                    if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
                } catch(...) {const auto saved=ruleReceipt.value("operation").toString();refreshFailed(saved.isEmpty()?QString("Unable to refresh verified dataset state. Close and reopen before continuing."):ruleApplyRefreshFailure(saved));}
            }
        }
        else throw std::invalid_argument("Unknown application command");
    }
};
ImportModel::ImportModel(std::unique_ptr<app::DocumentGateway> gateway,QObject* parent):QAbstractListModel(parent),worker_(std::make_shared<Worker>(std::move(gateway))) {
    pool_.setMaxThreadCount(1); state_={{"active",false},{"path",QSettings().value("datasetPath",suggestedPath())},{"startup",QSettings().contains("datasetPath")}};
}
ImportModel::~ImportModel() {pool_.waitForDone();worker_->gateway->close();}
QStringList ImportModel::currencies() const {QStringList out;for(const auto& c:supported_currencies())out<<text(c.code);return out;}
QString ImportModel::suggestedPath() const {return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/SpentIt/Experimental.spentit";}
QString ImportModel::localPath(const QUrl& url) const {return url.toLocalFile();}
QString ImportModel::newIdentity() const {return QUuid::createUuid().toString(QUuid::WithoutBraces);}
int ImportModel::selectedRowIndex() const {return state_.value("selectedIndex",-1).toInt();}
QVariantMap ImportModel::rowIdentity(int row) const {
    if(row<0 || row>=rowCount()) return {};
    const auto& id=rows_->rows[rows_->order[static_cast<std::size_t>(row)]].identity;
    return {{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)}};
}
QVariantMap ImportModel::rowRecord(int row) const {return data(index(row),Qt::UserRole).toMap();}
int ImportModel::occurrenceIndex(const QString& batch,int ordinal) const {
    if(!rows_ || ordinal<0) return -1;
    const auto id=batch.toStdString();
    for(std::size_t i=0;i<rows_->order.size();++i) {
        const auto& occurrence=rows_->rows[rows_->order[i]].identity;
        if(occurrence.batch==id && occurrence.ordinal==static_cast<std::size_t>(ordinal)) return static_cast<int>(i);
    }
    return -1;
}
int ImportModel::rowCount(const QModelIndex& p) const {return p.isValid() || !rows_?0:static_cast<int>(rows_->order.size());}
QVariant ImportModel::data(const QModelIndex& index,int role) const {
    if(!index.isValid() || index.row()<0 || index.row()>=rowCount() || role!=Qt::UserRole) return {};
    const auto i=rows_->order[static_cast<std::size_t>(index.row())];
    const auto& r=rows_->rows[i];const auto& t=r.draft.transaction;const auto amount=t.effective_total();
    // QML requests only visible delegates: no per-account QVariant tree.
    const auto labels=state_.value("categoryById").toMap();
    auto row=draft(r.draft,labels);
    if(state_.value("categoriesSupported").toBool()) {
        auto splits=row.value("splits").toList();
        for(qsizetype n=0;n<splits.size();++n) {auto a=splits[n].toMap();a["categoryDisplay"]=text(t.allocations()[static_cast<std::size_t>(n)].category);splits[n]=a;}
        row["splits"]=splits;
    }
    row["batch"]=text(r.identity.batch);row["ordinal"]=static_cast<int>(r.identity.ordinal);
    row["status"]=r.committed?"Committed":"Draft";
    row["category"]=t.allocations().size()==1?text(t.allocations()[0].category):"Split";
    row["withdrawal"]=amount.units()<0?money(amount).mid(1):"";
    row["deposit"]=amount.units()>=0?money(amount):"";
    row["balance"]=money(rows_->balances[i]);
    return row;
}
QHash<int,QByteArray> ImportModel::roleNames() const {return {{Qt::UserRole,"record"}};}
void ImportModel::requestCancel() {
    if(busy_ && operation_ && operation_->request()) {phase_="Cancellation requested";emit changed();}
}
void ImportModel::command(const QString& action,const QVariantMap& args) {
    if(busy_) {if(action=="inspect" && activeAction_=="select")pendingInspection_=args;return;}
    activeAction_=action;
    operation_=std::make_shared<app::OperationContext>();
    const auto operation=operation_;
    if(action!="backup" && action!="open" && action!="unlock" && action!="import" && action!="map_import" && action!="resume" && action!="query" && action!="rules_preview" && action!="rules_apply" && action!="rules_save" && action!="rules_reapply_preview" && action!="rules_reapply_apply" && action!="transfer_candidates") operation->seal();
    operation->progress=[this,weak=std::weak_ptr(operation)](std::string_view value) {
        const auto operation=weak.lock();
        if(!operation) return;
        const auto label=text(value);
        QMetaObject::invokeMethod(this,[this,operation,label]{
            if(busy_ && operation_==operation) {if(!cancellationRequested())phase_=label;emit changed();}
        },Qt::QueuedConnection);
    };
    phase_=phaseFor(action);
    busy_=true;error_.clear();emit changed();
    struct Result {QVariantMap state;std::shared_ptr<const app::LedgerView> rows;QString error;bool success=false;};
    auto* watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,action]{
        auto result=watcher->result();watcher->deleteLater();
        if(rows_!=result.rows) {
            beginResetModel();auto retired=std::move(rows_);rows_=std::move(result.rows);endResetModel();
            // Destruction of large retired snapshots must not stall the UI thread.
            pool_.start([retired=std::move(retired)]{});
        }
        state_=std::move(result.state);error_=std::move(result.error);busy_=false;phase_.clear();operation_->progress={};operation_.reset();emit changed();emit completed(action,result.success);
        if(pendingInspection_ && !busy_){auto args=std::move(*pendingInspection_);pendingInspection_.reset();if(state_.value("active").toBool())command("inspect",args);}
    });
    watcher->setFuture(QtConcurrent::run(&pool_,[worker=worker_,action,args,operation]{
        Result result; bool unlock=false;
        try {worker->execute(action,args,*operation);result.success=true;}
        catch(const app::OperationOutcomeUnknown& e){worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();worker->selectedBatch.clear();worker->invalidate();result.error=QString::fromUtf8(e.what());}
        catch(const app::OperationCancelled& e){if(action!="query")worker->invalidate();result.error=action=="backup"?"Backup cancelled before publication. The source dataset is unchanged. An encrypted .partial candidate may remain; retry to a new destination.":QString::fromUtf8(e.what());}
        catch(const app::UnlockRequired& e){worker->invalidate();result.error=QString::fromUtf8(e.what());unlock=true;}
        catch(const std::exception& e){if(action!="query")worker->invalidate();result.error=QString::fromUtf8(e.what());}
        operation->state.store(app::OperationContext::sealed);
        operation->checkpoint("Refreshing verified ledger");
        try {auto snapshot=worker->snapshot();result.state=std::move(snapshot.first);result.rows=std::move(snapshot.second);}
        catch(const std::exception&){
            worker->invalidate();
            const auto ruleOperation=worker->ruleReceipt.value("operation").toString();
            const auto reapplyOperationText=worker->reapplyReceipt.value("operation").toString();
            result.error=(action=="create_account" && result.success)?"Account was created durably, but its view could not be refreshed. Reopen and inspect the account list before retrying."
                :((action=="map_import" || action=="group_balance" || action=="group_abort" || action=="resume") && result.success)?"Import operation was saved durably, but its view could not be refreshed. Reopen and inspect the retained import before retrying."
                :(action.startsWith("transfer_") && result.success)?"Transfer operation was saved durably, but its view could not be refreshed. Reopen and inspect transfers before retrying."
                :(action=="apply_category" && !worker->categoryReceipt.isEmpty())?"Category change was saved durably, but the refreshed view could not be loaded. Reopen and inspect category history; do not apply it again."
                :(action=="apply_change" && !worker->changeReceipt.isEmpty())?"Committed change "+worker->changeReceipt.value("operation").toString()+" was saved durably, but the refreshed ledger could not be loaded. Reopen and inspect committed history; do not apply the change again."
                :(action=="rules_apply" && !ruleOperation.isEmpty())?ruleApplyRefreshFailure(ruleOperation)
                :(action=="rules_save" && !ruleOperation.isEmpty())?ruleSaveRefreshFailure(ruleOperation)
                :(action=="rules_reapply_apply" && !reapplyOperationText.isEmpty())?ruleReapplyRefreshFailure(reapplyOperationText)
                :(action=="apply_bulk" && !worker->bulkReceipt.isEmpty())?"Bulk change "+worker->bulkReceipt.value("operation").toString()+" was saved durably, but its refreshed view could not be loaded. Reopen and inspect committed history; do not apply it again."
                :((action=="rename_account" || action=="archive_account" || action=="reactivate_account" || action=="delete_account" || action=="correct_opening" || action=="reassign_currency" || action=="undo_account") && result.success)?"Account command was saved durably, but its view could not be refreshed. Reopen and inspect the account before retrying."
                :"Unable to refresh verified dataset state. Close and reopen before continuing.";
            worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();
            result.state={{"active",false},{"path",worker->path},{"bulkReceipt",worker->bulkReceipt},{"changeReceipt",worker->changeReceipt},{"categoryReceipt",worker->categoryReceipt},{"ruleReceipt",worker->ruleReceipt},{"reapplyReceipt",worker->reapplyReceipt}};
            result.success=false;
        }
        result.state["unlockRequired"]=unlock;return result;
    }));
}
