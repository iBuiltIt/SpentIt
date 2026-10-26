#include "import_model.hpp"
#include <spentit/import_view.hpp>
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
using namespace spentit;
namespace {
QString text(std::string_view s) { return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size())); }
std::string str(const QVariant& v) { return v.toString().toUtf8().toStdString(); }
QString money(const Money& m) { return text(app::decimal(m)); }
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
    const auto label=categoryId.isEmpty()?(a.category.empty()?QString("Uncategorized"):"Unmapped: "+text(a.category)):display.value("path",text(a.category)).toString()+(display.value("archived").toBool()?" [Archived]":"");
    return {{"category",text(a.category)},{"memo",text(a.memo)},{"amount",money(a.amount)},
        {"categoryId",categoryId},{"bindingRevision",a.category_binding?QString::number(a.category_binding->catalog_revision):QString{}},
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
        case rules::Effect::manual_protected:return "manual_protected";case rules::Effect::missing_parent:return "missing_parent";}
    return {};
}
QString effectWording(rules::Effect effect){
    switch(effect){case rules::Effect::applied:return "This rule will assign the field";
        case rules::Effect::compatible:return "Also matches and agrees; shadowed by the earlier field winner";
        case rules::Effect::competing:return "Also matches and disagrees; shadowed by the earlier field winner";
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
QString phaseFor(const QString& action){
    if(action=="open"||action=="unlock")return "Opening dataset";
    if(action=="import")return "Reading QIF file";
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
    return "Updating view";
}
QString changeName(app::ChangeKind kind) {
    switch(kind) {case app::ChangeKind::publication:return "Original whole-batch publication";case app::ChangeKind::correct:return "Correction";
        case app::ChangeKind::remove:return "Removal";case app::ChangeKind::undo:return "Undo";}
    return {};
}
QString snapshotText(const app::EntrySnapshot& entry) {
    const auto& d=entry.draft;
    return QString("Revision %1 · %2\n%3 · %4\nMemo: %5\nEffective total: %6 %7 · original imported: %8")
        .arg(entry.revision).arg(entry.active?"Active":"Removed",text(d.date),text(d.payee),text(d.memo),
        text(d.transaction.effective_total().currency().code()),money(d.transaction.effective_total()),money(*d.transaction.original_imported_amount()));
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
imports::DraftRow editedDraft(const QVariantMap& a,const Money& original,app::ImportRepository& repo,
                             const imports::DraftRow* baseline,const qif::Record* source) {
    std::vector<Allocation> values;const auto splits=a.value("splits").toList();
    if(splits.size()>static_cast<qsizetype>(qif::max_allocations)) throw std::invalid_argument("Too many allocations");
    const auto* prior=baseline?&baseline->transaction.allocations():source?&source->source_allocations:nullptr;
    for(const auto& value:splits) {
        const auto v=value.toMap();const auto allocationId=str(v.value("allocationId"));const Allocation* old=nullptr;
        if(prior && !allocationId.empty())for(const auto& candidate:*prior)if(candidate.identity==allocationId){old=&candidate;break;}
        Allocation next{str(v.value("category")),str(v.value("memo")),Money::parse(str(v.value("amount")),original.currency())};
        // Marks come from the stored baseline. A touched field becomes manual and drops its mark.
        if(old) {next.identity=old->identity;next.source_ordinal=old->source_ordinal;next.category_binding=old->category_binding;next.category_owner=old->category_owner;next.memo_owner=old->memo_owner;next.category_rule=old->category_rule;next.memo_rule=old->memo_rule;}
        else if(repo.categories_supported()){next.identity=allocationId.empty()?identity():allocationId;next.category_owner=FieldOwnership::manual;next.memo_owner=FieldOwnership::manual;}
        const auto chosen=str(v.value("categoryId"));const auto oldId=old && old->category_binding?old->category_binding->identity:std::string{};
        const bool categoryChanged=v.value("categoryTouched").toBool() || !old || chosen!=oldId || next.category!=old->category;
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
    imports::DraftRow result{str(a.value("date")),str(a.value("payee")),str(a.value("memo")),
        Transaction::corrected_import(original,Money::parse(str(a.value("total")),original.currency()),std::move(values))};
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
    QVariantMap selected,receipt,changePreview,changeReceipt;
    QVariantList categories,categoryHistory;
    QVariantMap categoryById,categorySelected,categoryPreview,categoryReceipt;
    std::optional<app::CategoryPreview> preparedCategory;
    std::string categoryOperation;
    std::int64_t catalogRevision=0;bool catalogLoaded=false;
    QVariantList rules,ruleTombstones;
    QVariantMap rulePreview,ruleReceipt,rulePrefill;
    std::optional<app::RuleApplyRequest> preparedApply;
    std::string ruleApplyOperation;
    std::int64_t ruleRevision=0;bool rulesLoaded=false;
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
    void clearRules(){rules.clear();ruleTombstones.clear();rulePreview.clear();ruleReceipt.clear();rulePrefill.clear();preparedApply.reset();ruleApplyOperation.clear();ruleRevision=0;rulesLoaded=false;}
    void clearChanges() {categories.clear();categoryById.clear();categorySelected.clear();categoryHistory.clear();categoryPreview.clear();categoryReceipt.clear();preparedCategory.reset();categoryOperation.clear();catalogLoaded=false;clearRules();selectedCommittedToken.reset();preparedChange.reset();preparedOperation.clear();changePreview.clear();changeReceipt.clear();history.clear();entryHistory.clear();historyTitle.clear();historyCursor.clear();historyMore=false;historyLoaded=false;}
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
            entryHistory.push_back(QVariantMap{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},{"operation",text(item.operation)},{"summary",changeName(item.kind)+" · "+text(item.operation)+"\n"+balanceImpact(item.before,item.change,item.after)},
                {"details",details},{"undoEligible",item.undo_eligible},{"undoReason",text(item.blocked_reason)}});
        }
    }
    // Session-local verified reads. Every mutation (including a failed command)
    // invalidates these before execution. The exclusive gateway owns all writes.
    QVariantMap metadata;
    std::optional<app::Account> cachedAccount;
    std::optional<std::vector<app::LedgerRow>> cachedLedger;
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
        const auto all=repo.batches();
        const auto found=std::find_if(all.begin(),all.end(),[&](const auto& b){return b.id==batch;});
        if(found==all.end() || ordinal>=found->count) throw std::invalid_argument("Choose a record in this import");
        auto bytes=repo.evidence(batch); const auto parsed=qif::parse(bytes,found->profile,acct.currency);
        const auto& source=parsed.records.at(ordinal);
        selectedCommittedToken.reset();
        selectedBatch=batch; selectedOrdinal=ordinal; selectedRevision=found->revision;
        selected={{"batch",text(batch)},{"ordinal",static_cast<int>(ordinal)},{"editable",false},
            {"source",evidencePreview(std::string_view(bytes).substr(source.begin,source.end-source.begin))}};
        if(source.diagnostic) selected["diagnostic"]=text(qif::message(source.diagnostic->issue));
        // Copy only parsed, supported evidence. Raw bytes remain separately visible;
        // an unavailable source date must never be replaced by a guessed value.
        QVariantList fields;
        if(source.transaction || source.repairable) {
            const auto add=[&](QString label,QString field,QString value,int split=-1) {
                fields.push_back(QVariantMap{{"label",label},{"field",field},{"value",value},{"split",split}});
            };
            if(!source.date.empty()) add("Date (ISO, using import date order)","date",text(source.date));
            add("Payee","payee",text(source.payee));
            add("Memo","memo",text(source.memo));
            if(source.original_amount) add("Original total","total",money(*source.original_amount));
            for(std::size_t i=0;i<source.source_allocations.size();++i) {
                const auto& a=source.source_allocations[i];
                const auto prefix=QString("Source split %1 → editor split %1: ").arg(i+1);
                add(prefix+"category","category",text(a.category),static_cast<int>(i));
                add(prefix+"memo","memo",text(a.memo),static_cast<int>(i));
                add(prefix+"amount","amount",money(a.amount),static_cast<int>(i));
            }
        }
        selected["sourceFields"]=fields;
        auto row=repo.row({batch,ordinal});
        if(!row.draft) {
            if(source.repairable && !row.source.repairable) selected["diagnostic"]=selected["diagnostic"].toString()+" — repair requires a new experimental dataset; this older dataset was not migrated.";
            if(row.source.repairable && found->state==app::BatchState::pending) {
                QVariantList splits;
                for(const auto& a:source.source_allocations) splits.push_back(allocationMap(a,categoryById));
                selected["draft"]=QVariantMap{{"date",text(source.date)},{"payee",text(source.payee)},{"memo",text(source.memo)},
                    {"total",money(*source.original_amount)},{"splits",splits}};
                selected["original"]=money(*source.original_amount);selected["editable"]=true;selected["repair"]=true;
                selected["diagnostic"]=selected["diagnostic"].toString()+" — correct the date/splits and save the repair; original evidence is retained.";
            }
            attachRules(repo,{batch,ordinal},context);
            return;
        }
        selected["draft"]=draft(*row.draft,categoryById);
        if(source.diagnostic) selected["diagnostic"]=selected["diagnostic"].toString()+" — saved interpretation repair; original diagnostic retained.";
        selected["original"]=money(*row.draft->transaction.original_imported_amount());
        selected["editable"]=found->state==app::BatchState::pending;
        selected["decision"]=static_cast<int>(row.decision.disposition);
        selected["reviewStale"]=row.review_stale;
        if(row.review_stale) selected["diagnostic"]=selected.value("diagnostic").toString()+" The matched committed record changed after this review. Reconfirm Include as separate transaction or the selected duplicate decision before committing this import.";
        const bool committed=found->state==app::BatchState::committed && row.decision.disposition==imports::Disposition::include;
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
            candidates.push_back(QVariantMap{{"removed",removed},{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},
                {"label",text(id.batch)+" / "+QString::number(id.ordinal+1)+" · "+text(prior.source.date)+" · "+text(prior.source.payee)+" · "+money(*prior.source.original_amount)+" · "+(basis==imports::MatchBasis::same_source_fields?"same source fields":basis==imports::MatchBasis::same_amount_unknown_date?"same original amount; source date unavailable":"same date and amount")}});
        }
        for(auto& value:candidates) {auto candidate=value.toMap();if(candidate.value("removed").toBool())candidate["label"]=candidate.value("label").toString()+" · PREVIOUSLY REMOVED — explicit review required";value=candidate;}
        selected["candidates"]=candidates;
        if(row.decision.duplicate_of) {
            selected["duplicate"]=text(row.decision.duplicate_of->batch)+" / "+QString::number(row.decision.duplicate_of->ordinal+1);
            selected["removedDuplicate"]=repo.committed_changes_supported() && !repo.committed_entry(*row.decision.duplicate_of).current.active;
        }
        attachRules(repo,{batch,ordinal},context);
    }
    std::pair<QVariantMap,std::shared_ptr<const app::LedgerView>> snapshot() {
        QVariantMap state{{"active",active},{"path",path}};
        if(!active) {state["changeReceipt"]=changeReceipt;state["categoryReceipt"]=categoryReceipt;state["ruleReceipt"]=ruleReceipt;return {state,{}};}
        auto& repo=gateway->repository();catalog();
        if(metadata.isEmpty()) {
            cachedAccount=repo.account();const auto& acct=*cachedAccount;
            state["committedChangesSupported"]=repo.committed_changes_supported();state["categoriesSupported"]=repo.categories_supported();
            state["name"]=text(acct.name); state["currency"]=text(acct.currency.code());
            state["openingDate"]=text(acct.opening_date); state["opening"]=money(acct.opening);
            state["frozen"]=acct.frozen; state["confirmed"]=acct.opening_confirmed;
            state["committed"]=money(repo.committed_balance()); const auto projection=repo.projected_balance();
            state["projectionAvailable"]=projection.has_value(); state["projected"]=projection?money(*projection):"Incomplete — review import";
            QVariantList batches;
            for(const auto& b:repo.batches()) {
                QVariantMap info{{"id",text(b.id)},{"count",static_cast<int>(b.count)},{"status",b.state==app::BatchState::pending?"Draft":b.state==app::BatchState::committed?"Committed":b.state==app::BatchState::captured?"Processing incomplete":"Discarded"},{"blocked",b.blocked},{"processingIncomplete",b.processing_incomplete},{"revision",static_cast<qlonglong>(b.revision)}};
                if(b.processing_incomplete) info["diagnostics"]="Source evidence retained. Processing is incomplete; resume or discard this import.";
                else if(b.blocked) {
                    const auto parsed=qif::parse(repo.evidence(b.id),b.profile,acct.currency);
                    QStringList diagnostics;
                    if(parsed.diagnostic) diagnostics<<text(qif::message(parsed.diagnostic->issue))+" at byte "+QString::number(parsed.diagnostic->offset);
                    for(std::size_t i=0;i<parsed.records.size() && diagnostics.size()<100;++i) if(parsed.records[i].diagnostic)
                        diagnostics<<"Record "+QString::number(i+1)+": "+text(qif::message(parsed.records[i].diagnostic->issue));
                    info["diagnostics"]=diagnostics.join('\n');
                }
                batches.push_back(info);
                if(b.state==app::BatchState::pending || b.state==app::BatchState::captured) state["pending"]=info;
            }
            state["batches"]=batches;
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
        } else state=metadata;
        state["categories"]=categories;state["categoryById"]=categoryById;state["catalogRevision"]=QString::number(catalogRevision);
        state["categorySelected"]=categorySelected;state["categoryHistory"]=categoryHistory;state["categoryPreview"]=categoryPreview;state["categoryReceipt"]=categoryReceipt;
        if(repo.rules_supported() && !rulesLoaded) loadRules();
        state["rulesSupported"]=repo.rules_supported();state["rules"]=rules;state["ruleTombstones"]=ruleTombstones;state["ruleRevision"]=QString::number(ruleRevision);
        state["rulePreview"]=rulePreview;state["ruleReceipt"]=ruleReceipt;state["rulePrefill"]=rulePrefill;state["ruleLimits"]=ruleLimitText();
        state["selected"]=selected;state["changePreview"]=changePreview;state["changeReceipt"]=changeReceipt;
        state["history"]=history;state["entryHistory"]=entryHistory;state["historyTitle"]=historyTitle;state["historyMore"]=historyMore;state["historyCursor"]=historyCursor;
        state["mode"]=projected?"Projected":"Committed";state["sort"]=sort;state["descending"]=descending;
        state["selectedIndex"]=-1;
        // Never substitute a partial projected ledger for an authoritative one.
        if(projected && !state.value("projectionAvailable").toBool()) return {state,{}};
        if(!cachedView) {
            if(!cachedLedger) {
                cachedLedger=repo.ledger(projected);
                // A presentation-only copy resolves current names for both visible
                // rows and literal filtering. Stored interpretations remain intact.
                if(repo.categories_supported())for(auto& row:*cachedLedger) {
                    auto values=row.draft.transaction.allocations();
                    for(auto& value:values)value.category=allocationMap(value,categoryById).value("categoryDisplay").toString().toUtf8().toStdString();
                    row.draft.transaction=row.draft.transaction.corrected(row.draft.transaction.effective_total(),std::move(values));
                }
            }
            cachedView=std::make_shared<const app::LedgerView>(app::ledger_view(*cachedAccount,*cachedLedger,filter.toStdString(),sort,descending));
        }
        state["withdrawals"]=money(cachedView->withdrawals);state["deposits"]=money(cachedView->deposits);
        if(selected.contains("ordinal")) {
            for(std::size_t position=0;position<cachedView->order.size();++position) {
                const auto& id=cachedView->rows[cachedView->order[position]].identity;
                if(id.batch==selectedBatch && id.ordinal==selectedOrdinal) {state["selectedIndex"]=static_cast<int>(position);break;}
            }
        }
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
        if(action!="query" && action!="inspect" && action!="evidence" && action!="backup" && action!="history" && action!="entry_history" && action!="preview_change" && action!="cancel_change" && action!="category_select" && action!="preview_category" && action!="cancel_category_preview" && action!="rules_load" && action!="rules_discard" && action!="rules_preview" && action!="rules_prefill") invalidate();
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
        if(!active) throw std::logic_error("Open a dataset first");
        auto& repo=gateway->repository();
        if(action=="category_list") {catalog();}
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
            if(kind=="correct") {request.kind=app::ChangeKind::correct;request.correction=editedDraft(a,*entry.current.draft.transaction.original_imported_amount(),repo,&entry.current.draft,&entry.source);}
            else if(kind=="undo") {request.kind=app::ChangeKind::undo;request.undo_of=str(a.value("operation"));}
            else if(kind!="remove")throw std::invalid_argument("Unknown committed change");
            auto preview=repo.preview_committed_change(request);
            preparedOperation=identity();preparedChange=preview;
            changePreview={{"id",text(preparedOperation)},{"title",changeName(request.kind)+" preview"},
                {"applyLabel",request.kind==app::ChangeKind::correct?"Apply correction":request.kind==app::ChangeKind::remove?"Remove transaction":"Apply Undo"},
                {"balanceImpact",balanceImpact(preview.before,preview.change,preview.after)},
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
        } else if(action=="import") {
            QFile file(a.value("path").toString()); const QFileInfo info(file);
            if(!info.isFile() || !file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read QIF file");
            if(file.size()>static_cast<qint64>(qif::max_file)) throw std::invalid_argument("QIF exceeds 64 MiB limit");
            QByteArray bytes;
            while(!file.atEnd()) {
                context.checkpoint("Reading QIF file");
                auto chunk=file.read(64*1024);
                if(chunk.isEmpty() && !file.atEnd()) throw std::runtime_error("QIF read failed");
                bytes.append(chunk);
                if(bytes.size()>static_cast<qsizetype>(qif::max_file)) throw std::invalid_argument("QIF exceeds 64 MiB limit");
            }
            if(file.error()!=QFileDevice::NoError || !file.atEnd()) throw std::runtime_error("QIF read failed or exceeded limit");
            if(bytes.size()>static_cast<qsizetype>(qif::max_file)) throw std::invalid_argument("QIF exceeds 64 MiB limit");
            const auto profile=a.value("profile").toString();
            if(profile!="DMY" && profile!="MDY") throw std::invalid_argument("Select DMY or MDY before importing");
            const auto batch=repo.ingest_controlled(identity(),bytes.toStdString(),profile=="DMY"?qif::DateOrder::dmy:qif::DateOrder::mdy,context);
            projected=true; selected.clear();selectedBatch.clear();
            if(batch.count && !batch.processing_incomplete) inspect(batch.id,0,context);
            else selected={{"batch",text(batch.id)},{"editable",false},{"source",evidencePreview(bytes.toStdString())}};
        } else if(action=="resume") {
            const auto batch=repo.resume(str(a.value("batch")),a.value("revision").toLongLong(),context);
            projected=true;selected.clear();selectedBatch.clear();
            if(batch.count && !batch.processing_incomplete)inspect(batch.id,0,context);
        } else if(action=="evidence") {
            selectedBatch.clear();selected={{"batch",a.value("batch")},{"editable",false},{"source",evidencePreview(repo.evidence(str(a.value("batch"))))}};
        } else if(action=="inspect") inspect(str(a.value("batch")),a.value("ordinal").toUInt(),context);
        else if(action=="save") {
            if(selectedBatch.empty()) throw std::logic_error("Select a draft first");
            auto row=repo.row({selectedBatch,selectedOrdinal});
            if(!row.source.original_amount || (!row.draft && !row.source.repairable)) throw std::invalid_argument("This record cannot be repaired");
            const auto edit=editedDraft(a,*row.source.original_amount,repo,row.draft?&*row.draft:nullptr,&row.source);
            repo.save({selectedBatch,selectedOrdinal},selectedRevision,edit); inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="decide") {
            imports::Decision decision{imports::Disposition::include,{}};
            if(a.value("duplicate").toBool()) decision={imports::Disposition::duplicate,imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}};
            repo.decide({selectedBatch,selectedOrdinal},selectedRevision,decision);inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="opening") {
            const auto acct=repo.account();repo.opening(acct.revision,str(a.value("date")),Money::parse(str(a.value("amount")),acct.currency));
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="commit" || action=="discard") {
            const auto batches=repo.batches(); const auto b=std::find_if(batches.begin(),batches.end(),[](const auto& item){return item.state==app::BatchState::pending || item.state==app::BatchState::captured;});
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
        } else if(action=="backup") gateway->backup(str(a.value("path")));
        else if(action=="query") {
            if(a.contains("restoreSelection")) {
                const auto restore=a.value("restoreSelection").toMap();
                if(restore.contains("ordinal")) inspect(str(restore.value("batch")),restore.value("ordinal").toUInt(),context);
                else {selected.clear();selectedBatch.clear();selectedOrdinal=0;}
            }
            const auto nextProjected=a.value("projected",true).toBool();
            if(projected!=nextProjected) cachedLedger.reset();
            cachedView.reset();
            filter=a.value("filter").toString();sort=a.value("sort",0).toInt();descending=a.value("descending",false).toBool();projected=nextProjected;
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
                    if(!current || index<0 || static_cast<std::size_t>(index)>=current->transaction.allocations().size() || !current->transaction.allocations()[static_cast<std::size_t>(index)].source_ordinal)
                        throw std::invalid_argument("Manual splits are not rule sources");
                    const auto ordinal=*current->transaction.allocations()[static_cast<std::size_t>(index)].source_ordinal;
                    if(ordinal>=row.source.source_allocations.size()) throw std::invalid_argument("Manual splits are not rule sources");
                    source=row.source.source_allocations[ordinal].memo;scope=rules::Scope::split;input=rules::InputField::memo;
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
    if(busy_) return;
    operation_=std::make_shared<app::OperationContext>();
    const auto operation=operation_;
    if(action!="open" && action!="unlock" && action!="import" && action!="resume" && action!="rules_preview" && action!="rules_apply" && action!="rules_save") operation->seal();
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
    });
    watcher->setFuture(QtConcurrent::run(&pool_,[worker=worker_,action,args,operation]{
        Result result; bool unlock=false;
        try {worker->execute(action,args,*operation);result.success=true;}
        catch(const app::OperationOutcomeUnknown& e){worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();worker->selectedBatch.clear();worker->invalidate();result.error=QString::fromUtf8(e.what());}
        catch(const app::UnlockRequired& e){worker->invalidate();result.error=QString::fromUtf8(e.what());unlock=true;}
        catch(const std::exception& e){worker->invalidate();result.error=QString::fromUtf8(e.what());}
        operation->state.store(app::OperationContext::sealed);
        operation->checkpoint("Refreshing verified ledger");
        try {auto snapshot=worker->snapshot();result.state=std::move(snapshot.first);result.rows=std::move(snapshot.second);}
        catch(const std::exception&){
            worker->invalidate();
            const auto ruleOperation=worker->ruleReceipt.value("operation").toString();
            result.error=(action=="apply_category" && !worker->categoryReceipt.isEmpty())?"Category change was saved durably, but the refreshed view could not be loaded. Reopen and inspect category history; do not apply it again."
                :(action=="apply_change" && !worker->changeReceipt.isEmpty())?"Committed change "+worker->changeReceipt.value("operation").toString()+" was saved durably, but the refreshed ledger could not be loaded. Reopen and inspect committed history; do not apply the change again."
                :(action=="rules_apply" && !ruleOperation.isEmpty())?ruleApplyRefreshFailure(ruleOperation)
                :(action=="rules_save" && !ruleOperation.isEmpty())?ruleSaveRefreshFailure(ruleOperation)
                :"Unable to refresh verified dataset state. Close and reopen before continuing.";
            worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();
            result.state={{"active",false},{"path",worker->path},{"changeReceipt",worker->changeReceipt},{"categoryReceipt",worker->categoryReceipt},{"ruleReceipt",worker->ruleReceipt}};
            result.success=false;
        }
        result.state["unlockRequired"]=unlock;return result;
    }));
}
