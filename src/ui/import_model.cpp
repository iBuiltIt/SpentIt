#include <exception>
#include <tuple>
#include <QGuiApplication>
#include <QWindow>
#include <QTimer>
#include <spentit/diagnostics.hpp>
#include "import_model.hpp"
#include <spentit/import_view.hpp>
#include <spentit/split_editor.hpp>
#include <spentit/statements.hpp>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDate>
#include <spentit/date_format.hpp>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>
#include <chrono>
#include <thread>
#include <map>
#include <set>
using namespace spentit;
namespace {
QVariantList sidebarAccounts(const QVariantList& accounts,const QString& key) {
    auto remaining=accounts;QVariantList ordered;
    for(const auto& id:QSettings().value(key).toStringList()) {
        const auto found=std::find_if(remaining.begin(),remaining.end(),[&](const QVariant& value){return value.toMap().value("id").toString()==id;});
        if(found!=remaining.end()){ordered.push_back(*found);remaining.erase(found);}
    }
    ordered.append(remaining);return ordered;
}
QString text(std::string_view s) { return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size())); }
std::string str(const QVariant& v) { return v.toString().toUtf8().toStdString(); }
QString money(const Money& m, const AmountFormat& amountFormat) { return text(display_amount(m,amountFormat)); }
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
TransferChoice read_transfer_choice(app::ImportRepository& repo,const QVariantMap& endpoint,bool ordinaryCategory=false){
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
        committed=repo.transaction_accepted(id);
        const bool editable=info->state==app::BatchState::pending || info->state==app::BatchState::protected_drafts;
        if(!committed && !editable) throw std::invalid_argument(ordinaryCategory?"Resolve this transaction in Review before changing its category.":"Resolve the draft before assigning a transfer");
        if(committed){
            const auto entry=repo.committed_entry(id);
            if(!entry.current.active) throw std::invalid_argument("This transfer changed. Select it again before continuing.");
            row=entry.current.draft;
        }else{
            const auto loaded=repo.row(id);
            if(loaded.excluded_by_clear || !loaded.draft || (loaded.decision.disposition!=imports::Disposition::include && !(ordinaryCategory && loaded.decision.disposition==imports::Disposition::unresolved)))
                throw std::invalid_argument(ordinaryCategory?"Resolve this transaction in Review before changing its category.":"Resolve the draft before assigning a transfer");
            row=*loaded.draft;
        }
        if(allocation.empty()){
            if(row->transaction.allocations().size()!=1)
                throw std::invalid_argument("Choose which split to transfer.");
            allocation=row->transaction.allocations().front().identity;
        }
    }
    const transfers::Side* matched=nullptr;
    const app::TransferView* host=nullptr;
    for(const auto& value:links)for(const auto* side:{&value.link.origin,&value.link.destination}){
        if(row && (value.link.unresolved || value.link.destination_released))continue;
        if(side->state==transfers::State::removed || side->endpoint.account!=account || side->endpoint.entry!=id) continue;
        if(!allocation.empty() && side->endpoint.allocation!=allocation) continue;
        if(matched) throw std::invalid_argument("Choose which split to transfer.");
        matched=side;host=&value;
    }
    if(!row && !matched) throw std::invalid_argument("Transfer entry is unavailable");
    if(row){
        const auto& allocations=row->transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==allocation;});
        if(found==allocations.end()) throw std::invalid_argument("Choose a transfer split");
        if(matched && (matched->amount!=found->amount || matched->date!=row->date))
            throw std::invalid_argument("This transfer changed. Select it again before continuing.");
    }
    const bool linked=host && !host->link.unresolved;
    TransferChoice choice{transfers::AllocationFact{{account,id,allocation},Money(0,repo.account().currency),"",transfers::State::draft,false,false,false},0,{},{},{},false};
    if(matched){
        const bool generated=!matched->independent_evidence && !matched->manually_entered && !matched->attached_evidence;
        choice={transfers::AllocationFact{matched->endpoint,matched->amount,matched->date,matched->state,matched->independent_evidence,generated,matched->manually_entered,matched->ledger_entry},(matched->independent_evidence || matched->ledger_entry)?revision:0,host->payee,host->memo,linked?host->link.identity:std::string{},linked};
    }else{
        const auto& allocations=row->transaction.allocations();
        const auto found=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==allocation;});
        const bool imported=!info->manual;
        choice={transfers::AllocationFact{{account,id,found->identity},found->amount,row->date,committed?transfers::State::committed:transfers::State::draft,imported,false,!imported,!imported},revision,row->payee,row->memo,{},false};
    }
    guard.restore();
    return choice;
}
QString evidencePreview(std::string_view bytes) {
    constexpr std::size_t limit=64*1024;
    const auto part=bytes.substr(0,limit);
    const auto prefix=bytes.size()>limit ? QString("Preview: first 65536 of %1 bytes.\n").arg(bytes.size()) : QString{};
    if(!qif::valid_utf8(part) || part.find('\0')!=part.npos)
        return prefix+"This part of the file is not readable text. Original bytes:\n"+QString::fromLatin1(QByteArray(part.data(),static_cast<qsizetype>(part.size())).toHex(' '));
    return prefix+text(part);
}
std::string identity() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
QString markText(const std::optional<RuleMark>& mark){return mark?text(mark->identity)+" v"+QString::number(mark->version):QString{};}
QString ownerName(FieldOwnership owner){
    switch(owner){case FieldOwnership::untracked:return "untracked";case FieldOwnership::source:return "source";case FieldOwnership::manual:return "manual";case FieldOwnership::rule:return "rule";}
    return {};
}
QVariantMap allocationMap(const Allocation& a,const QVariantMap& labels, const AmountFormat& amountFormat) {
    const auto categoryId=a.category_binding?text(a.category_binding->identity):QString{};
    const auto display=labels.value(categoryId).toMap();
    auto label=qif::transfer_destination(a.category)?"Transfer destination: "+text(a.category):categoryId.isEmpty()?(a.category.empty()?QString("Uncategorized"):"Unmapped: "+text(a.category)):display.value("path",text(a.category)).toString()+(display.value("archived").toBool()?" [Archived]":"");
    const auto destination=qif::transfer_destination(a.category).value_or(a.category);
    QString accountId;
    for(const auto& value:labels.value("@aliases").toList()) {
        const auto alias=value.toMap();if(!app::account_name_matches(str(alias.value("name")),destination))continue;
        if(!accountId.isEmpty() && accountId!=alias.value("accountId").toString()){label="Ambiguous account alias: "+text(a.category);break;}
        accountId=alias.value("accountId").toString();label=categoryId.isEmpty()?"Transfer destination: "+alias.value("display").toString():"Category/account name conflict: "+display.value("path",text(a.category)).toString();
    }
    return {{"category",text(a.category)},{"memo",text(a.memo)},{"amount",money(a.amount,amountFormat)},
        {"transferDestination",qif::transfer_destination(a.category)?text(std::string(*qif::transfer_destination(a.category))):QString{}},{"transferLabel",money(a.amount,amountFormat)+" · "+label+" · "+text(a.identity)},{"categoryId",categoryId},{"bindingRevision",a.category_binding?QString::number(a.category_binding->catalog_revision):QString{}},
        {"allocationId",text(a.identity)},{"categoryDisplay",label},{"reviewLabel",money(a.amount,amountFormat)+" · "+label},{"categoryTouched",false},{"memoTouched",false},
        {"transferAmount",!accountId.isEmpty() || qif::transfer_destination(a.category).has_value()},{"sourceOrdinal",a.source_ordinal?QVariant(static_cast<int>(*a.source_ordinal)):QVariant(-1)},
        {"categoryOwner",ownerName(a.category_owner)},{"memoOwner",ownerName(a.memo_owner)},{"categoryRule",markText(a.category_rule)},{"memoRule",markText(a.memo_rule)}};
}
QVariantList allocations(const Transaction& t,const QVariantMap& labels, const AmountFormat& amountFormat) {
    QVariantList out;for(const auto& a:t.allocations())out.push_back(allocationMap(a,labels,amountFormat));return out;
}
QVariantMap draft(const imports::DraftRow& d,const QVariantMap& labels,std::string_view format, const AmountFormat& amountFormat) {
    return {{"date",text(display_date(d.date,format))},{"payee",text(d.payee)},{"memo",text(d.memo)},
        {"payeeOwner",ownerName(d.payee_owner)},{"memoOwner",ownerName(d.memo_owner)},{"payeeRule",markText(d.payee_rule)},{"memoRule",markText(d.memo_rule)},
        {"remainderAllocation",text(d.remainder_allocation)},{"manualSplitAmounts",d.manual_split_amounts},{"currencyDefinition",text(d.transaction.effective_total().currency().definition_id())},
        {"total",money(d.transaction.effective_total(),amountFormat)},{"splits",allocations(d.transaction,labels,amountFormat)}};
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
    switch(effect){case rules::Effect::applied:return "This rule sets the value.";
        case rules::Effect::compatible:return "Also matches; an earlier rule already sets the same value.";
        case rules::Effect::competing:return "Also matches; an earlier rule sets a different value.";
        case rules::Effect::transfer_protected:return "Transfer destination is preserved; resolve it in Transfers";
        case rules::Effect::manual_protected:return "Matches, but keeps the value you entered.";
        case rules::Effect::missing_parent:return "The transaction has no category to use.";}
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
        case app::RuleField::allocation_memo:return "split memo";case app::RuleField::allocation_category:return "split category";}
    return {};
}

QString ruleSaveRefreshFailure(const QString&){return "Rules were saved, but the screen could not update. Reopen the file and check Rules before making more changes.";}
QString ruleApplyRefreshFailure(const QString&){return "Rule changes were saved, but the screen could not update. Reopen the file and check the transactions before making more changes.";}
QString ruleReapplyRefreshFailure(const QString&){return "Rule changes were saved, but the screen could not update. Reopen the file and check the transactions before making more changes.";}
QString phaseFor(const QString& action){
    if(action=="open"||action=="unlock")return "Opening file";
    if(action=="import")return "Reading statement file";
    if(action=="resume")return "Importing transactions";
    if(action=="commit")return "Accepting transactions";
    if(action=="backup")return "Saving backup";
    if(action=="review_answer")return "Saving Review decision";
    if(action=="review_batch")return "Checking selected matches";
    if(action=="assign_transfer_batch")return "Assigning selected transfers";
    if(action=="release_transfer_batch")return "Changing categories";
    if(action=="undo_transfer_assignment")return "Undoing transfer assignment";
    if(action=="review_undo")return "Undoing Review decision";
    if(action=="create")return "Creating file";
    if(action=="save" || action=="accept_edit")return "Accepting edit";
    if(action=="undo_edit")return "Undoing edit";
    if(action=="rename_payee_selection")return "Renaming selected payees";
    if(action=="restore_imported_selection")return "Restoring imported values";
    if(action=="preview_change")return "Preparing change preview";
    if(action=="apply_change")return "Saving changes";
    if(action=="rules_load"||action=="rules_discard"||action=="rules_prefill")return "Loading rules";
    if(action=="rules_preview")return "Evaluating rules";
    if(action=="rules_save")return "Saving rules";
    if(action=="rules_apply" || action=="rules_account_apply")return "Applying rules";
    if(action=="rules_account_undo")return "Undoing rule application";
    if(action=="rules_reapply_preview")return "Checking rules against accepted transactions";
    if(action=="rules_reapply_apply")return "Reapplying rules";
    if(action=="rename_account")return "Renaming account";
    if(action=="archive_account")return "Archiving account";
    if(action=="reactivate_account")return "Reactivating account";
    if(action=="delete_account")return "Deleting empty account";
    if(action=="correct_opening")return "Correcting opening balance";
    if(action=="reassign_currency")return "Reassigning account currency";
    if(action=="undo_account")return "Undoing account change";
    return "Updating view";
}
QString changeName(app::ChangeKind kind) {
    switch(kind) {case app::ChangeKind::publication:return "Transactions accepted";case app::ChangeKind::correct:return "Correction";
        case app::ChangeKind::remove:return "Removal";case app::ChangeKind::undo:return "Undo";case app::ChangeKind::transfer:return "Transfer";}
    return {};
}
QString snapshotText(const app::EntrySnapshot& entry,std::string_view format, const AmountFormat& amountFormat) {
    const auto& d=entry.draft;
    return QString("%1\n%2 · %3\nMemo: %4\nAmount: %5 %6 · imported amount: %7")
        .arg(entry.active?"Active":"Removed",text(display_date(d.date,format)),text(d.payee),text(d.memo),
        text(d.transaction.effective_total().currency().code()),money(d.transaction.effective_total(),amountFormat),d.transaction.original_imported_amount()?money(*d.transaction.original_imported_amount(),amountFormat):QString("Unavailable"));
}
QVariantMap snapshotMap(const app::EntrySnapshot& entry,const QString& label,std::string_view format, const AmountFormat& amountFormat) {
    return {{"label",label},{"summary",snapshotText(entry,format,amountFormat)},{"splits",allocations(entry.draft.transaction,{},amountFormat)}};
}
QString snapshotDetails(const app::EntrySnapshot& entry,std::string_view format, const AmountFormat& amountFormat) {
    auto result=snapshotText(entry,format,amountFormat);
    for(const auto& a:entry.draft.transaction.allocations())result+="\nSplit: "+text(a.category)+" · "+text(a.memo)+" · "+money(a.amount,amountFormat);
    return result;
}
QString balanceImpact(const Money& before,const Money& change,const Money& after, const AmountFormat& amountFormat) {
    return text(before.currency().code())+" account balance: "+money(before,amountFormat)+" · change "+money(change,amountFormat)+" · resulting balance "+money(after,amountFormat);
}
imports::DraftRow editedDraft(const QVariantMap& a,const std::optional<Money>& original,app::ImportRepository& repo,
                             const imports::DraftRow* baseline,const qif::Record* source, const AmountFormat& amountFormat) {
    std::vector<Allocation> values;const auto splits=a.value("splits").toList();
    if(splits.size()>static_cast<qsizetype>(qif::max_allocations)) throw std::invalid_argument("Too many splits");
    const auto* prior=baseline?&baseline->transaction.allocations():source?&source->source_allocations:nullptr;
    for(const auto& value:splits) {
        const auto v=value.toMap();const auto allocationId=str(v.value("allocationId"));const Allocation* old=nullptr;
        if(prior && !allocationId.empty())for(const auto& candidate:*prior)if(candidate.identity==allocationId){old=&candidate;break;}
        Allocation next{str(v.value("category")),str(v.value("memo")),evaluate_amount(str(v.value("amount")),repo.account().currency,amountFormat)};
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
                const auto display=repo.resolve_category(chosen);if(!display.assignable)throw std::invalid_argument("Choose an available category");
                const auto catalog=repo.category_catalog();
                if(v.value("bindingRevision").toString().toLongLong()!=catalog.revision)throw std::invalid_argument("Categories changed. Choose the category again.");
                next.category_binding=CategoryBinding{chosen,catalog.revision};next.category=display.path;
            } else next.category_binding.reset();
            next.category_owner=FieldOwnership::manual;next.category_rule.reset();
        }
        if(v.value("memoTouched").toBool() || (old && next.memo!=old->memo)){next.memo_owner=FieldOwnership::manual;next.memo_rule.reset();}
        if(a.value("restoreImportedText").toBool() && !v.value("memoTouched").toBool() && source && next.source_ordinal
            && *next.source_ordinal<source->source_allocation_fields.size() && next.memo==source->source_allocation_fields[*next.source_ordinal].memo){
            next.memo_owner=FieldOwnership::source;next.memo_rule.reset();
        }
        values.push_back(std::move(next));
    }
    const auto total=evaluate_amount(str(a.value("total")),repo.account().currency,amountFormat);
    const bool reassigned=(original && original->currency()!=total.currency()) || (source && source->source_currency && *source->source_currency!=total.currency());
    imports::DraftRow result{str(a.value("date")),str(a.value("payee")),str(a.value("memo")),
        Transaction::repaired_import(original,total,std::move(values),reassigned)};
    const auto oldPayee=baseline?baseline->payee:source?source->payee:std::string{};
    const auto oldMemo=baseline?baseline->memo:source?source->memo:std::string{};
    result.remainder_allocation=baseline && !baseline->remainder_allocation.empty()?baseline->remainder_allocation:str(a.value("remainderAllocation"));
    result.manual_split_amounts=(baseline && baseline->manual_split_amounts) || a.value("manualSplitAmounts").toBool();
    result.payee_owner=baseline?baseline->payee_owner:source?FieldOwnership::source:FieldOwnership::manual;
    result.memo_owner=baseline?baseline->memo_owner:source?FieldOwnership::source:FieldOwnership::manual;
    result.payee_rule=baseline?baseline->payee_rule:std::nullopt;
    result.memo_rule=baseline?baseline->memo_rule:std::nullopt;
    if(a.value("payeeTouched").toBool() || result.payee!=oldPayee){result.payee_owner=FieldOwnership::manual;result.payee_rule.reset();}
    if(a.value("memoTouched").toBool() || result.memo!=oldMemo){result.memo_owner=FieldOwnership::manual;result.memo_rule.reset();}
    if(a.value("restoreImportedText").toBool() && source){
        if(!a.value("payeeTouched").toBool() && result.payee==source->payee){result.payee_owner=FieldOwnership::source;result.payee_rule.reset();}
        if(!a.value("memoTouched").toBool() && result.memo==source->memo){result.memo_owner=FieldOwnership::source;result.memo_rule.reset();}
    }
    return result;
}

}
namespace diag = spentit::diagnostics;
struct ImportModel::Worker {
    bool diagnosticSaved=false,diagnosticUnchanged=false,diagnosticPresentationFailed=false;
    struct PresentationRead {
        bool& failed;int exceptions=std::uncaught_exceptions();
        ~PresentationRead(){if(std::uncaught_exceptions()>exceptions)failed=true;}
    };
    void observedSaved(diag::Code stage=diag::Code::unclassified) noexcept {
        diagnosticSaved=true;diag::durable(stage);
    }
    std::string dateFormat="YYYY-MM-DD";
    AmountFormat amountFormat;
    QString dateText(std::string_view iso) const {return text(display_date(iso,dateFormat));}
    std::unique_ptr<app::DocumentGateway> gateway;
    bool active=false,projected=true;
    QString path,filter;
    bool canOpen=false, welcomePassword=false, overwriteRequired=false;
    int sort=0; bool descending=false;
    QVariantMap selected,receipt,changePreview,changeReceipt,transferReceipt,transferMergePreview;
    QVariantMap reviewLinkPreview, linkExclusionPreview;
    std::optional<app::LinkExclusion> preparedLinkExclusion;
    std::int64_t linkExclusionGeneration=0;
    std::optional<app::TransferRequest> preparedMerge;
    std::string mergeOperation;
    QVariantList transferCandidates;QString candidateLink;
    QVariantMap bulkPreview,bulkReceipt;
    struct PreparedImport {std::string id,bytes,label;statements::Index document;qif::DateOrder profile;};
    std::optional<PreparedImport> preparedImport, dateImport;
    QVariantMap importUndoPreview;
    QVariantMap importDateQuestion;
    QVariantList importPlan,importMappingArgs;
    QString importTarget,cleanupNoticePath,importFeedback;bool importDestination=false,startupCleanupNotice=false;
    std::optional<std::vector<app::ImportAccountMapping>> preparedMappings;

    std::optional<app::BulkPreview> preparedBulk;
    std::string bulkOperation;
    std::set<imports::Occurrence> selection;
    std::optional<imports::Occurrence> selectionAnchor;
    std::optional<imports::Occurrence> selectionFocus;
    struct AccountView {QString filter;bool projected;int sort;bool descending;std::string batch;std::size_t ordinal;std::set<imports::Occurrence> selection;std::optional<imports::Occurrence> anchor,focus;};
    std::map<std::int64_t,AccountView> accountViews;
    QString sortDocument;
    std::optional<std::int64_t> sortAccount;
    QString sortSettingsGroup() const {
        const QFileInfo file(path);
        const auto canonical=file.canonicalFilePath();
        const auto location=canonical.isEmpty()?file.absoluteFilePath():canonical;
        return "LedgerSort/"+QString::fromLatin1(QCryptographicHash::hash(location.toUtf8(),QCryptographicHash::Sha256).toHex());
    }
    void restoreAccountSort() {
        auto& repo=gateway->repository();
        if(!repo.account_selected()){sortAccount.reset();return;}
        const auto account=repo.account().identity;
        if(sortAccount==account)return;
        sort=0;descending=false;
        if(const auto view=accountViews.find(account);view!=accountViews.end()) {
            sort=view->second.sort;descending=view->second.descending;
        } else {
            const auto saved=QSettings().value(sortDocument+"/"+QString::number(account)).toList();
            bool valid=false;const auto column=saved.size()==2?saved[0].toInt(&valid):0;
            if(valid && column>=0 && column<6 && saved[1].metaType().id()==QMetaType::Bool){sort=column;descending=saved[1].toBool();}
        }
        sortAccount=account;cachedView.reset();
    }
    void saveAccountSort() {
        QSettings settings;
        const auto key=sortDocument+"/"+QString::number(gateway->repository().account().identity);
        const QVariantList value{sort,descending};
        if(settings.value(key).toList()==value)return;
        settings.setValue(key,value);settings.sync();
        if(settings.status()!=QSettings::NoError)throw std::runtime_error("The account sort order could not be saved. Check that Settings is writable and try again.");
    }

    QVariantList categories,categoryHistory;
    QVariantMap categoryById,categorySelected,categoryPreview,categoryReceipt;
    std::optional<app::CategoryPreview> preparedCategory;
    std::string categoryOperation;
    std::int64_t catalogRevision=0;bool catalogLoaded=false;
    QVariantList rules,ruleTombstones;
    QVariantMap rulePreview,ruleReceipt,rulePrefill;
    std::optional<app::RuleApplyRequest> preparedApply;
    std::optional<app::AccountRuleRequest> preparedAccountApply;
    std::string ruleApplyOperation;
    QVariantMap reapplyPreview,reapplyReceipt;
    std::optional<app::RuleReapplyPreview> preparedReapply;
    std::string reapplyOperation;
    std::int64_t ruleRevision=0;bool rulesLoaded=false;
    std::optional<app::ImportGroup> durableGroup(std::string_view id) {
        try{return gateway->repository().import_group_outcome(id);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();preparedImport.reset();preparedMappings.reset();importPlan.clear();throw std::runtime_error("SpentIt could not confirm whether the import was saved. Reopen the file and check the imported transactions before trying again.");}
    }
    app::ImportGroup resumeGroup(std::string_view id,app::OperationContext& context) {
        try{return gateway->repository().resume_import_group(id,context);}catch(...){const auto failure=std::current_exception();const auto saved=durableGroup(id);if(saved && (saved->state==app::GroupState::reviewing || saved->state==app::GroupState::started))return *saved;try{std::rethrow_exception(failure);}catch(const app::OperationCancelled&){try{gateway->repository().cleanup_incomplete_imports(id);}catch(...){gateway->close();active=false;throw std::runtime_error("Interrupted import cleanup failed. Reopen to retry cleanup before making further changes.");}throw app::OperationCancelled("Import cancelled. You can import the file again.");}}
    }
    void catalog() {
        PresentationRead observation{diagnosticPresentationFailed};
        diag::cache(diag::Code::catalog,catalogLoaded?diag::EventKind::cache_hit:diag::EventKind::cache_miss);
        if(catalogLoaded)return;
        categories.clear();categoryById.clear();catalogRevision=0;
        auto& repo=gateway->repository();
        if(!repo.categories_supported()){catalogLoaded=true;return;}
        const auto current=repo.category_catalog();catalogRevision=current.revision;
        for(const auto& c:current.categories) {
            const CategoryDefinition* parent=nullptr;
            if(!c.parent.empty())for(const auto& p:current.categories)if(p.identity==c.parent){parent=&p;break;}
            const bool archived=c.archived || (parent && parent->archived);
            const auto categoryPath=parent?parent->name+": "+c.name:c.name;
            QVariantMap value{{"id",text(c.identity)},{"name",text(c.name)},{"parent",text(c.parent)},{"path",text(categoryPath)},
                {"createdOrder",QString::number(current.creation_order.at(c.identity))},
                {"group",c.kind==CategoryKind::group},{"archived",archived},{"ownArchived",c.archived},{"deleted",c.deleted},
                {"assignable",c.kind==CategoryKind::leaf && !archived && !c.deleted},{"revision",QString::number(c.revision)}};
            categories.push_back(value);categoryById[text(c.identity)]=value;
        }
        auto accounts=repo.accounts();const auto archived=repo.archived_accounts();accounts.insert(accounts.end(),archived.begin(),archived.end());
        const auto aliases=repo.account_aliases();
        QVariantList accountCategories,accountNames;
        for(const auto& account:accounts) {
            QStringList names;for(const auto& alias:aliases)if(alias.account==account.identity && alias.name!=account.name)names.push_back(text(alias.name));
            QVariantMap value{{"id","@account:"+QString::number(account.identity)},{"account",true},{"accountId",QString::number(account.identity)},
                {"name",text(account.name)},{"path",text(account.name)},{"aliases",names.join("; ")},{"createdOrder",QString::number(account.identity)},
                {"archived",account.archived},{"deleted",false},{"assignable",false},{"group",false}};
            accountCategories.push_back(value);
            for(const auto& alias:aliases)if(alias.account==account.identity)accountNames.push_back(QVariantMap{{"name",text(alias.name)},{"display",text(account.name)},{"accountId",QString::number(account.identity)}});
        }
        categoryById["@accounts"]=accountCategories;categoryById["@aliases"]=accountNames;
        catalogLoaded=true;
    }
    void selectCategory(std::string_view id) {
        PresentationRead observation{diagnosticPresentationFailed};
        catalog();categorySelected=categoryById.value(text(id)).toMap();categoryHistory.clear();
        if(categorySelected.isEmpty())throw std::invalid_argument("Choose an existing category");
        for(const auto& item:gateway->repository().category_history(id)) {
            categoryHistory.prepend(QString("%1 → %2 · group: %3 · %4")
                .arg(item.before?text(item.before->name):"(new)",text(item.after.name),item.after.parent.empty()?QString("Standalone"):categoryById.value(text(item.after.parent)).toMap().value("path",text(item.after.parent)).toString(),item.after.deleted?"Deleted":item.after.archived?"Archived":"Active"));
        }
    }
    QVariantList history,entryHistory;
    QString historyTitle,historyCursor;
    bool historyMore=false,historyLoaded=false;
    std::optional<app::EntryToken> selectedCommittedToken;
    std::optional<app::ChangePreview> preparedChange;
    std::string preparedOperation;
    void clearRules(){rules.clear();ruleTombstones.clear();rulePreview.clear();ruleReceipt.clear();rulePrefill.clear();preparedApply.reset();preparedAccountApply.reset();ruleApplyOperation.clear();preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();reapplyReceipt.clear();ruleRevision=0;rulesLoaded=false;}
    void clearChanges() {newManual.reset();newManualId.clear();manualUndo.clear();revealManual.reset();sessionEdits.clear();reviewLinkPreview.clear();linkExclusionPreview.clear();preparedLinkExclusion.reset();transferReceipt.clear();transferCandidates.clear();candidateLink.clear();transferMergePreview.clear();preparedMerge.reset();mergeOperation.clear();bulkPreview.clear();bulkReceipt.clear();preparedBulk.reset();bulkOperation.clear();categories.clear();categoryById.clear();categorySelected.clear();categoryHistory.clear();categoryPreview.clear();categoryReceipt.clear();preparedCategory.reset();categoryOperation.clear();catalogLoaded=false;clearRules();selectedCommittedToken.reset();preparedChange.reset();preparedOperation.clear();changePreview.clear();changeReceipt.clear();history.clear();entryHistory.clear();historyTitle.clear();historyCursor.clear();historyMore=false;historyLoaded=false;}
    void listHistory(std::int64_t before=0,bool append=false) {
        PresentationRead observation{diagnosticPresentationFailed};
        auto& repo=gateway->repository();
        const auto changes=repo.committed_changes(before,50);if(!append)history.clear();
        for(const auto& item:changes) {
            history.push_back(QVariantMap{{"operation",text(item.operation)},{"batch",text(item.identity.batch)},{"ordinal",static_cast<int>(item.identity.ordinal)},
                {"summary",changeName(item.kind)+" · current record: "+text(item.payee)+" · "+dateText(item.date)+" · "+money(item.total,amountFormat)+" · "+(item.active?"Active":"Removed")}});
        }
        historyMore=changes.size()==50;historyCursor=changes.empty()?QString{}:QString::number(changes.back().sequence);historyLoaded=true;
    }
    void entryDetails(const imports::Occurrence& id) {
        PresentationRead observation{diagnosticPresentationFailed};
        auto& repo=gateway->repository();const auto entry=repo.committed_entry(id);
        historyTitle="History · "+text(entry.current.draft.payee)+" · "+(entry.current.active?"Active":"Removed");entryHistory.clear();
        const auto entries=repo.committed_history(id);
        for(auto it=entries.rbegin();it!=entries.rend();++it) {
            const auto& item=*it;
            QString details=item.before_entry?"Before\n"+snapshotDetails(*item.before_entry,dateFormat,amountFormat)+"\n":"";
            details+="After\n"+snapshotDetails(item.after_entry,dateFormat,amountFormat);
            const auto reapplyGroup=text(repo.rule_reapply_group(item.operation));
            const bool reapply=!reapplyGroup.isEmpty();
            entryHistory.push_back(QVariantMap{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},{"operation",text(item.operation)},{"summary",changeName(item.kind)+" · "+text(item.operation)+"\n"+balanceImpact(item.before,item.change,item.after,amountFormat)},
                {"details",details},{"bulkGroup",text(repo.bulk_group(item.operation))},{"reapplyGroup",reapplyGroup},{"undoEligible",reapply?false:item.undo_eligible},{"undoReason",reapply?QString("Undo the whole rule reapply."):text(item.blocked_reason)}});
        }
    }
    // Session-local verified reads. Every mutation (including a failed command)
    // invalidates these before execution. The exclusive gateway owns all writes.
    QVariantMap metadata;
    QString metadataSelection;
    std::optional<app::Account> cachedAccount;
    std::shared_ptr<std::vector<app::LedgerRow>> cachedLedger;
    std::shared_ptr<const app::LedgerView> cachedView;
    std::set<imports::Occurrence> cachedLinkedRows;
    bool deferReview=false;
    struct SessionEdit {std::string operation;imports::Occurrence identity;std::int64_t account;std::vector<imports::Occurrence> selection={};QString label="Undo edit";};
    std::vector<SessionEdit> sessionEdits;
    std::optional<imports::DraftRow> newManual;
    std::string newManualId,manualUndo;
    std::int64_t manualUndoGeneration=-1;
    std::optional<imports::Occurrence> revealManual;

    void acceptEdit(const imports::DraftRow& edit,app::OperationContext& context) {
        auto& repo=gateway->repository();const imports::Occurrence id{selectedBatch,selectedOrdinal};
        const auto account=repo.account().identity;const auto operation=identity();
        try {repo.edit_transaction(operation,id,selectedRevision,selectedCommittedToken,edit);}
        catch(...) {const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;
            try {outcome=repo.review_outcome(operation);}catch(...){throw app::OperationOutcomeUnknown();}
            if(!outcome)std::rethrow_exception(failure);
        }
        observedSaved();sessionEdits.push_back({operation,id,account});
        selection={id};selectionFocus=id;selectionAnchor=id;
        try {inspect(id.batch,id.ordinal,context);if(selectedCommittedToken){entryDetails(id);if(historyLoaded)listHistory();}}
        catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();sessionEdits.clear();throw std::runtime_error("Edit was saved, but its view could not be refreshed. Reopen and inspect the transaction before retrying.");}
    }
    void invalidate() {diag::cache(diag::Code::ledger,diag::EventKind::invalidated);catalogLoaded=false;rulesLoaded=false;metadata.clear();cachedAccount.reset();cachedLedger.reset();cachedView.reset();}
    std::string selectedBatch;
    std::size_t selectedOrdinal=0;
    std::int64_t selectedRevision=0;
    // Commit identity is deterministic for a batch revision, so restart retries
    // resolve the same durable operation without storing local financial metadata.
    explicit Worker(std::unique_ptr<app::DocumentGateway> g):gateway(std::move(g)){}
    void loadRules() {
        PresentationRead observation{diagnosticPresentationFailed};
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
        selected["ruleAssignments"]=QVariantList{};selected["ruleMatchError"]=QString();selected["winnerLines"]=QVariantList{};selected["ruleExplanations"]=QVariantList{};
        if(!repo.rules_supported())return;
        try {
            if(!rulesLoaded)loadRules();
            const auto match=repo.match_saved_rules(id,context);
            if(!match.error.empty())selected["ruleMatchError"]=QString("Couldn’t check rules for this transaction. You can still edit it.");
            QVariantList winners;for(const auto& line:match.winner_lines)winners.push_back(text(line));selected["winnerLines"]=winners;
            QVariantList explanations;
            for(const auto& item:match.explanations){
                const auto ruleId=text(item.contribution.rule);
                explanations.push_back(QVariantMap{{"rule",ruleId},{"version",QString::number(item.contribution.version)},{"label",ruleLabel(ruleId)},{"field",text(item.field)},
                    {"effect",effectName(item.effect)},{"wording",effectWording(item.effect)},{"allocation",item.allocation?QVariant(static_cast<int>(*item.allocation)):QVariant()}});
            }
            selected["ruleExplanations"]=explanations;
            QVariantList assignments;
            for(const auto& item:match.assignments) {
                const auto ruleId=text(item.mark.identity);
                const bool available=std::any_of(rules.begin(),rules.end(),[&](const auto& value){return value.toMap().value("id").toString()==ruleId;});
                assignments.push_back(QVariantMap{{"rule",ruleId},{"label",ruleLabel(ruleId)},{"field",text(item.field)},{"available",available},{"scope",scopeName(item.scope)},{"allocation",item.allocation?QVariant(static_cast<int>(*item.allocation)):QVariant()}});
            }
            selected["ruleAssignments"]=assignments;
        } catch(const app::OperationCancelled&){throw;}
        catch(const app::OperationOutcomeUnknown&){throw;}
        catch(const std::exception&){selected["ruleMatchError"]=QString("Couldn’t check rules for this transaction. You can still edit it.");}
    }
    void decorateSelectedTransfers(app::ImportRepository& repo,const std::vector<app::TransferView>& links) {
        auto current=selected.value("draft").toMap();auto splits=current.value("splits").toList();
        const imports::Occurrence entry{str(selected.value("batch")),static_cast<std::size_t>(selected.value("ordinal").toUInt())};
        const auto account=repo.account().identity;
        std::map<std::int64_t,QString> names;
        for(const auto& link:links) {
            if(link.link.unresolved || link.link.destination_released || link.link.origin.state==transfers::State::removed || link.link.destination.state==transfers::State::removed)continue;
            for(const auto* side:{&link.link.origin,&link.link.destination}) {
                if(side->endpoint.account!=account || side->endpoint.entry!=entry)continue;
                if(names.empty()){
                    for(const auto& item:repo.accounts())names[item.identity]=text(item.name);
                    for(const auto& item:repo.archived_accounts())names[item.identity]=text(item.name);
                }
                const auto other=side==&link.link.origin?link.link.destination.endpoint.account:link.link.origin.endpoint.account;
                for(auto& value:splits){auto part=value.toMap();if(part.value("allocationId").toString()!=text(side->endpoint.allocation))continue;
                    // Display metadata only: never replace the stored category in an edit.
                    part["categoryDisplay"]=names.at(other)+" [Account]";
                    part["transferAmount"]=true;value=part;
                }
            }
        }
        current["splits"]=splits;selected["draft"]=current;
    }
    void inspect(std::string batch,std::size_t ordinal,app::OperationContext& context) {
        if(newManual && batch==newManualId)return;
        PresentationRead observation{diagnosticPresentationFailed};
        auto& repo=gateway->repository(); const auto acct=repo.account();catalog();
        if(batch.starts_with("~transfer-")){
            for(const auto& value:repo.transfers())for(const auto* side:{&value.link.origin,&value.link.destination})if(side->endpoint.account==acct.identity && side->endpoint.entry==imports::Occurrence{batch,ordinal}){
                selectedBatch=batch;selectedOrdinal=ordinal;selectedCommittedToken.reset();
                Allocation allocation{"Transfer",value.memo,side->amount,{},side->endpoint.allocation};
                selected={{"batch",text(batch)},{"ordinal",static_cast<int>(ordinal)},{"editable",false},{"localTransfer",true},{"transferId",text(value.link.identity)},{"committed",side->state==transfers::State::committed},{"source",side->manually_entered?"Manually entered; no imported source":"Generated counterpart; no imported source"},{"draft",draft(imports::DraftRow{side->date,value.payee,value.memo,Transaction::manual(side->amount,{allocation})},categoryById,dateFormat,amountFormat)}};
                QVariantList origins;
                if(!value.link.unresolved && side==&value.link.origin)origins.push_back(QVariantMap{{"allocation",text(side->endpoint.allocation)},{"link",text(value.link.identity)}});
                selected["transferOrigins"]=origins;decorateSelectedTransfers(repo,repo.transfers());return;
            }
            throw std::invalid_argument("Transfer entry is unavailable");
        }
        const auto all=repo.batches();
        const auto found=std::find_if(all.begin(),all.end(),[&](const auto& b){return b.id==batch;});
        if(found==all.end() || ordinal>=found->count) throw std::invalid_argument("Choose a record in this import");
        auto row=repo.row({batch,ordinal});
        const auto original=repo.imported_original({batch,ordinal});
        const auto source=original?repo.row(*original).source:row.source;
        const auto raw=original?repo.record_evidence(*original):std::string{};
        selectedCommittedToken.reset();
        selectedBatch=batch; selectedOrdinal=ordinal; selectedRevision=found->revision;
        selected={{"batch",text(batch)},{"ordinal",static_cast<int>(ordinal)},{"editable",false},
            {"source",evidencePreview(raw)},{"manual",row.source.format=="manual"},{"hasImportedOriginal",original.has_value()}};
        if(source.diagnostic) selected["diagnostic"]=text(qif::message(*source.diagnostic));
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
        if(!source.time_evidence.empty()) add("Time from imported file","time",text(source.time_evidence));
        if(!source.trntype.empty()) add("Transaction type","trntype",text(source.trntype));
        if(!source.uninterpreted.empty()) add("Other details from imported file","uninterpreted",text(source.uninterpreted));
        if(source.transaction || source.repairable) {
            if(!source.date.empty()) add(format=="qif"?"Date (using selected display format)":"Date (calendar date in the file)","date",dateText(source.date));
            add("Payee","payee",text(source.payee));
            add("Memo","memo",text(source.memo));
            if(source.original_amount) add("Original total","total",money(*source.original_amount,amountFormat));
            for(std::size_t i=0;i<source.source_allocation_fields.size();++i) {
                const auto& a=source.source_allocation_fields[i];
                const auto prefix=QString("Source split %1 → editor split %1: ").arg(i+1);
                add(prefix+"category","category",text(a.category),static_cast<int>(i));
                add(prefix+"memo","memo",text(a.memo),static_cast<int>(i));
                if(a.amount)add(prefix+"amount","amount",money(*a.amount,amountFormat),static_cast<int>(i));
            }
        }
        selected["sourceFields"]=original?fields:QVariantList{};
        if(!row.draft) {
            if(row.excluded_by_clear){selected["diagnostic"]="This source contribution was discarded.";return;}
            if(source.repairable && !row.source.repairable) selected["diagnostic"]=selected["diagnostic"].toString()+" — record repair is not supported by this dataset.";
            if(row.source.repairable && found->state==app::BatchState::pending) {
                QVariantList splits;
                for(std::size_t i=0;i<source.source_allocation_fields.size();++i){const auto& a=source.source_allocation_fields[i];
                    const auto destination=qif::transfer_destination(a.category);
                    const auto label=destination?"Transfer destination: "+text(a.category):a.category.empty()?QString("Uncategorized"):"Unmapped: "+text(a.category);
                    splits.push_back(QVariantMap{{"category",text(a.category)},{"categoryDisplay",label},{"memo",text(a.memo)},{"amount",a.amount?money(*a.amount,amountFormat):QString{}},{"allocationId",QString("source-%1").arg(i)},{"categoryId",QString{}},{"bindingRevision",QString{}},{"sourceOrdinal",static_cast<int>(i)},{"categoryOwner","source"},{"memoOwner","source"},{"categoryTouched",false},{"memoTouched",false},{"categoryRule",QString{}},{"memoRule",QString{}},{"transferDestination",destination?text(std::string(*destination)):QString{}},{"transferLabel",(a.amount?money(*a.amount,amountFormat):QString("Unavailable"))+" · "+text(a.category)}});
                }
                selected["draft"]=QVariantMap{{"date",dateText(source.date)},{"payee",text(source.payee)},{"memo",text(source.memo)},
                    {"total",source.original_amount?money(*source.original_amount,amountFormat):QString{}},{"splits",splits},{"payeeOwner","source"},{"memoOwner","source"},{"payeeRule",QString{}},{"memoRule",QString{}}};
                selected["original"]=source.original_amount?money(*source.original_amount,amountFormat):QString("Unavailable");selected["editable"]=true;selected["repair"]=true;
                selected["diagnostic"]=selected["diagnostic"].toString()+" — enter a valid date, amount and splits, then save.";
            }
            attachRules(repo,{batch,ordinal},context);
            return;
        }
        selected["draft"]=draft(*row.draft,categoryById,dateFormat,amountFormat);
        if(row.matched_generated){selected["diagnostic"]="Already present as an accepted generated transfer. This import adds source ownership only.";selected["editable"]=false;return;}
        if(source.diagnostic) selected["diagnostic"]=selected["diagnostic"].toString()+" — saved interpretation repair; original diagnostic retained.";
        selected["original"]=original?(source.original_amount?money(*source.original_amount,amountFormat):QString("Unavailable")):QString();
        selected["editable"]=found->state==app::BatchState::pending && !repo.transaction_accepted({batch,ordinal});
        selected["decision"]=static_cast<int>(row.decision.disposition);
        selected["reviewStale"]=row.review_stale;
        if(row.review_stale) selected["diagnostic"]=selected.value("diagnostic").toString()+" The matching transaction changed. Review whether these are the same transaction before accepting this import.";
        const auto transferLinks=repo.transfers();
        const auto accountAliases=repo.account_aliases();
        if(found->state==app::BatchState::pending)for(const auto& allocation:row.draft->transaction.allocations())if(qif::transfer_destination(allocation.category) || std::any_of(accountAliases.begin(),accountAliases.end(),[&](const auto& alias){return app::account_name_matches(alias.name,allocation.category);})) {
            const transfers::Endpoint endpoint{acct.identity,{batch,ordinal},allocation.identity};bool resolved=false;
            for(const auto& value:transferLinks)for(const auto* side:{&value.link.origin,&value.link.destination})if(side->state!=transfers::State::removed)
                resolved|=side->endpoint==endpoint || (side->attached_evidence && side->attached_evidence->endpoint==endpoint) || (side->proposed_evidence && side->proposed_evidence->endpoint==endpoint);
            if(!resolved)selected["diagnostic"]=selected.value("diagnostic").toString()+" Choose a destination for this transfer: "+text(allocation.category)+". In Transfers, choose the destination for this split or match an existing transfer.";
        }
        selected["transferEligible"]=found->state==app::BatchState::pending && !repo.transaction_accepted({batch,ordinal}) && row.decision.disposition==imports::Disposition::include;
        const bool committed=repo.transaction_accepted({batch,ordinal}) && row.decision.disposition==imports::Disposition::include && !row.excluded_by_clear;
        selected["committed"]=committed;
        if(committed && repo.committed_changes_supported()) {
            const auto entry=repo.committed_entry({batch,ordinal});selectedCommittedToken=entry.token;selected["draft"]=draft(entry.current.draft,categoryById,dateFormat,amountFormat);
            selected["removed"]=!entry.current.active;selected["canCorrect"]=entry.current.active;
        }
        QVariantList candidates;
        // Committed/discarded rows are read-only and have no duplicate actions.
        if(found->state==app::BatchState::pending && !committed) for(const auto& id:row.candidates) {
            auto prior=repo.row(id);
            const auto basis=imports::candidate_basis(row.source,prior.source);
            const bool removed=repo.transaction_accepted(id) && !repo.committed_entry(id).current.active;
            const auto basisText=basis?text(imports::basis_label(*basis)):QString("check the imported transaction");
            candidates.push_back(QVariantMap{{"removed",removed},{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},
                {"label",text(id.batch)+" / "+QString::number(id.ordinal+1)+" · "+text(prior.source.format.empty()?"qif":prior.source.format)+" · "+dateText(prior.source.date)+" · "+text(prior.source.payee)+" · "+(prior.source.original_amount?money(*prior.source.original_amount,amountFormat):QString("Unavailable"))+" · "+basisText}});
        }
        for(auto& value:candidates) {auto candidate=value.toMap();if(candidate.value("removed").toBool())candidate["label"]=candidate.value("label").toString()+" · PREVIOUSLY REMOVED — explicit review required";value=candidate;}
        selected["candidates"]=candidates;
        if(row.decision.duplicate_of) {
            selected["duplicate"]=text(row.decision.duplicate_of->batch)+" / "+QString::number(row.decision.duplicate_of->ordinal+1);
            selected["removedDuplicate"]=repo.transaction_accepted(*row.decision.duplicate_of) && !repo.committed_entry(*row.decision.duplicate_of).current.active;
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
        decorateSelectedTransfers(repo,transferLinks);
        if(row.excluded_by_clear)selected["diagnostic"]="Removed from this account.";
        attachRules(repo,{batch,ordinal},context);
    }
    std::pair<QVariantMap,std::shared_ptr<const app::LedgerView>> snapshot(app::OperationContext* context=nullptr) {
        diag::Span span(diag::Code::snapshot);
        const auto home=QDir::homePath()+"/";const auto full=QFileInfo(path).absoluteFilePath();
        QVariantMap state{{"displayPath",full.startsWith(home)?"~/"+full.mid(home.size()):full},{"active",active},{"path",path},{"canOpen",canOpen},{"unlockRequired",welcomePassword},{"overwriteRequired",overwriteRequired}};
        if(!active) {state["bulkReceipt"]=bulkReceipt;state["changeReceipt"]=changeReceipt;state["categoryReceipt"]=categoryReceipt;state["ruleReceipt"]=ruleReceipt;return {state,{}};}
        auto& repo=gateway->repository();restoreAccountSort();catalog();state["dateFormat"]=text(dateFormat);state["amountFormatting"]=amountFormat.enabled;state["amountStyle"]=text(amountFormat.style);state["startupImportCleanup"]=startupCleanupNotice;
        if(metadataSelection!=selected.value("batch").toString()){metadata.clear();metadataSelection=selected.value("batch").toString();}
        diag::cache(diag::Code::metadata,metadata.isEmpty()?diag::EventKind::cache_miss:diag::EventKind::cache_hit);
        if(metadata.isEmpty()) {
            std::map<std::int64_t,QVariantMap> accountBalances;
            for(const auto& [id,total]:repo.balances()) accountBalances.emplace(id,QVariantMap{{"balance",money(total,amountFormat)}});
            auto packAccount=[&](const app::Account& item){
                const auto found=accountBalances.find(item.identity);
                const auto extra=found==accountBalances.end()?QVariantMap{}:found->second;
                return QVariantMap{{"id",QString::number(item.identity)},{"name",text(item.name)},{"currency",text(item.currency.code())},{"archived",item.archived},{"balance",extra.value("balance")}};
            };
            QVariantList archivedList;for(const auto& item:repo.archived_accounts()) archivedList.push_back(packAccount(item));
            state["archivedAccounts"]=archivedList;
            const auto acceptance=repo.acceptance_review();QVariantList acceptanceWarnings;bool canOverride=true;
            for(const auto& warning:acceptance.warnings){acceptanceWarnings.push_back(QVariantMap{{"account",QString::number(warning.account)},{"batch",text(warning.batch)},{"ordinal",warning.ordinal?QVariant(static_cast<int>(*warning.ordinal)):QVariant()},{"reason",text(warning.reason)},{"overridable",warning.overridable}});canOverride &= warning.overridable;}
            const auto review=deferReview?app::ReviewSheet{}:repo.review_sheet();state["reviewReady"]=!deferReview;QVariantList questions;std::int64_t priorAccount=-1;
            for(const auto& q:review.questions){
                QVariantList rows;
                for(const auto& row:q.rows){auto value=draft(row.draft,categoryById,dateFormat,amountFormat);value["batch"]=text(row.identity.batch);value["ordinal"]=static_cast<int>(row.identity.ordinal);value["account"]=QString::number(row.account);value["accountName"]=text(row.account_name);value["committed"]=row.committed;value["currency"]=text(row.draft.transaction.effective_total().currency().code());value["withdrawal"]=row.draft.transaction.effective_total().units()<0?money(row.draft.transaction.effective_total(),amountFormat):QString();value["deposit"]=row.draft.transaction.effective_total().units()>0?money(row.draft.transaction.effective_total(),amountFormat):QString();value["note"]=text(row.note);value["sameDate"]=!q.rows.empty() && !row.draft.date.empty() && row.draft.date==q.rows.front().draft.date;value["suggestedLeftAllocation"]=text(row.suggested_left_allocation);value["suggestedRightAllocation"]=text(row.suggested_right_allocation);value["manual"]=row.note=="Manually entered";value["key"]=text(row.identity.batch)+":"+QString::number(row.identity.ordinal);rows.push_back(value);}
                questions.push_back(QVariantMap{{"waitingLink",text(q.waiting_link)},{"key",text(q.key)},{"kind",q.kind==app::ReviewKind::same?"same":q.kind==app::ReviewKind::link?"link":"correction"},{"account",QString::number(q.account)},{"accountName",text(q.account_name)},{"separator",q.account!=priorAccount},{"reason",text(q.reason)},{"batch",text(q.batch)},{"ordinal",q.ordinal?static_cast<int>(*q.ordinal):-1},{"rows",rows}});priorAccount=q.account;
            }
            QVariantList exclusions;
            for(const auto& rule:repo.link_exclusions())if(!rule.removed)exclusions.push_back(QVariantMap{{"identity",text(rule.identity)},{"field",text(rule.field)},{"expression",text(rule.expression)},{"account",QString::number(rule.account)},{"regex",rule.regex}});
            state["linkExclusions"]=exclusions;state["linkExclusionPreview"]=linkExclusionPreview;
            state["reviewQuestions"]=questions;state["reviewCount"]=questions.size();state["reviewGeneration"]=QString::number(review.generation);state["canUndoReview"]=review.can_undo;state["canUndoTransferAssignment"]=repo.can_undo_transfer_assignment();state["reviewLinkPreview"]=reviewLinkPreview;
            QVariantList undoableImports;
            for(const auto& item:repo.undoable_imports()) {
                QVariantList accounts;for(const auto id:item.accounts)accounts.push_back(QString::number(id));
                const auto dates=item.first_date.empty()?QString("No transaction dates"):dateText(item.first_date)+(item.last_date!=item.first_date?QString(" – ")+dateText(item.last_date):QString());
                const auto label=QString("%1 import · %2 · Records: %3").arg(text(item.format).toUpper(),dates).arg(item.records);
                undoableImports.push_back(QVariantMap{{"source",text(item.source)},{"label",label},{"accounts",accounts}});
            }
            state["undoableImports"]=undoableImports;
            const auto rollback=repo.rollback_review();QVariantList affected;for(const auto id:rollback.affected_accounts)affected.push_back(QString::number(id));
            state["rollback"]=QVariantMap{{"token",QString::number(rollback.token)},{"transactions",static_cast<qlonglong>(rollback.transactions)},{"failures",static_cast<qlonglong>(rollback.failures)},{"accounts",affected}};
            state["newTransactions"]=static_cast<qlonglong>(acceptance.transactions);state["importFailures"]=static_cast<qlonglong>(acceptance.failed_records);state["acceptanceWarnings"]=acceptanceWarnings;state["canIgnoreWarnings"]=canOverride;
            state["committedChangesSupported"]=repo.committed_changes_supported();state["categoriesSupported"]=repo.categories_supported();
            if(!repo.account_selected()) {
                cachedAccount.reset();
                const auto archivedHome=archivedList.isEmpty()?QString():archivedList.front().toMap().value("currency").toString();
                state["accounts"]=QVariantList{};state["accountSelected"]=false;state["readOnly"]=true;state["archived"]=false;
                state["emptyActive"]="No active account is selected. Archived accounts remain available.";
                state["homeCurrency"]=archivedHome;state["accountId"]="";state["name"]="";state["currency"]=archivedHome;
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
                state["transferSequence"]=QString::number(repo.transfer_sequence());QVariantList items;QVariantMap navigation;
                for(const auto& value:repo.transfers()){
                    const auto& side=value.link.origin.endpoint.account==acct.identity?value.link.origin:value.link.destination;
                    const auto& other=value.link.origin.endpoint.account==acct.identity?value.link.destination:value.link.origin;
                    if(side.state==transfers::State::removed)continue;
                    if(transfers::protected_draft(value.link,acct.identity))state["pendingProtected"]=true;
                    const auto target=other.attached_evidence?other.attached_evidence->endpoint:other.endpoint;
                    QString otherName;const auto otherId=QString::number(other.endpoint.account);
                    for(const auto& item:accountList)if(item.toMap().value("id").toString()==otherId)otherName=item.toMap().value("name").toString();
                    if(otherName.isEmpty())for(const auto& item:archivedList)if(item.toMap().value("id").toString()==otherId)otherName=item.toMap().value("name").toString();
                    if(side.endpoint.account==acct.identity && !value.link.unresolved && !value.link.destination_released && other.state!=transfers::State::removed) {
                        const QVariantMap destination{{"counterpartAccount",QString::number(target.account)},
                            {"counterpartBatch",text(target.entry.batch)},{"counterpartOrdinal",static_cast<int>(target.entry.ordinal)},
                            {"arrow",side.amount.units()<0?"←":side.amount.units()>0?"→":"↔"},
                            {"description",(side.amount.units()<0?QString("Transfer to "):side.amount.units()>0?QString("Transfer from "):QString("Transfer with "))+otherName+" — show matching transaction"}};
                        const auto addEndpoint=[&](const transfers::Endpoint& endpoint){
                            const auto key=text(endpoint.entry.batch)+":"+QString::number(endpoint.entry.ordinal);
                            auto splits=navigation.value(key).toMap();splits[text(endpoint.allocation)]=destination;navigation[key]=splits;
                        };
                        addEndpoint(side.endpoint);
                        if(side.attached_evidence)addEndpoint(side.attached_evidence->endpoint);
                    }
                    items.push_back(QVariantMap{{"id",text(value.link.identity)},{"label",otherName+" · "+money(side.amount,amountFormat)+" · "+dateText(side.date)},{"amount",money(side.amount,amountFormat)},{"date",dateText(side.date)},{"committed",side.state==transfers::State::committed},{"accepted",side.accepted_unmatched},{"evidenced",side.independent_evidence || value.source_matched},{"proposed",side.proposed_evidence.has_value()},{"provisional",side.provisional_date},{"protected",transfers::protected_draft(value.link,acct.identity)},{"local",!side.ledger_entry && !side.attached_evidence && (side.manually_entered || !side.independent_evidence)},{"isOrigin",value.link.origin.endpoint.account==acct.identity},{"counterpartAccount",QString::number(target.account)},{"counterpartBatch",text(target.entry.batch)},{"counterpartOrdinal",static_cast<int>(target.entry.ordinal)}});
                }state["transfers"]=items;state["transferNavigation"]=navigation;
                QVariantList journal;
                for(const auto& entry:repo.transfer_journal()){
                    const auto accountName=[&](std::int64_t id){
                        for(const auto& list:{accountList,archivedList})for(const auto& item:list)if(item.toMap().value("id").toString()==QString::number(id))return item.toMap().value("name").toString();
                        return QString("Deleted account");
                    };
                    auto actionLabel=text(entry.action);actionLabel.replace('_',' ');
                    const auto line=actionLabel+" · "+accountName(entry.origin_account)+" "+money(entry.origin_amount,amountFormat)+" "+dateText(entry.origin_date)+" → "+accountName(entry.destination_account)+" "+money(entry.destination_amount,amountFormat)+" "+dateText(entry.destination_date)+(entry.unresolved?" · unresolved":"");
                    journal.push_back(QVariantMap{{"operation",text(entry.operation)},{"action",text(entry.action)},{"link",text(entry.link)},{"sequence",QString::number(entry.sequence)},{"originAccount",QString::number(entry.origin_account)},{"destinationAccount",QString::number(entry.destination_account)},{"originAmount",money(entry.origin_amount,amountFormat)},{"destinationAmount",money(entry.destination_amount,amountFormat)},{"originDate",dateText(entry.origin_date)},{"destinationDate",dateText(entry.destination_date)},{"unresolved",entry.unresolved},{"text",line}});
                }
                state["transferJournal"]=journal;
            }
            state["nextAccountId"]=QString::number(repo.next_account_identity());
            state["name"]=text(acct.name); state["currency"]=text(acct.currency.code()); state["currencyDefinition"]=text(acct.currency.definition_id());
            state["openingDate"]=dateText(acct.opening_date); state["opening"]=money(acct.opening,amountFormat);
            state["frozen"]=acct.frozen; state["confirmed"]=acct.opening_confirmed;
            state["balance"]=accountBalances.contains(acct.identity)?accountBalances.at(acct.identity).value("balance"):QVariant(money(repo.balance(),amountFormat)); state["committed"]=money(repo.committed_balance(),amountFormat); const auto projection=repo.projected_balance();
            state["projectionAvailable"]=projection.has_value(); state["projected"]=projection?money(*projection,amountFormat):"Incomplete — review import";
            QVariantList batches;QVariantMap manualBatches;
            for(const auto& b:repo.batches()) {
                if(b.manual){manualBatches[text(b.id)]=true;continue;}
                QVariantMap info{{"id",text(b.id)},{"count",static_cast<int>(b.count)},{"status",b.state==app::BatchState::pending?"Draft":b.state==app::BatchState::committed?"Committed":b.state==app::BatchState::captured?"Processing incomplete":b.state==app::BatchState::protected_drafts?"Protected drafts":"Discarded"},{"blocked",b.blocked},{"processingIncomplete",b.processing_incomplete},{"revision",static_cast<qlonglong>(b.revision)}};
                if(b.processing_incomplete) info["diagnostics"]="Import unfinished. Resume or discard this import.";
                else if(b.blocked) {
                    const auto parsed=repo.parsed_source(b.id);
                    const auto sourceBytes=repo.evidence(b.id);
                    QStringList diagnostics;
                    if(parsed.diagnostic) diagnostics<<text(qif::message(parsed.diagnostic->issue))+" at byte "+QString::number(parsed.diagnostic->offset);
                    for(std::size_t i=0;i<parsed.records.size() && diagnostics.size()<100;++i) if(parsed.records[i].diagnostic)
                        diagnostics<<text(qif::describe_record(sourceBytes,parsed.records[i],i));
                    info["diagnostics"]=diagnostics.join('\n');
                }
                if(b.state==app::BatchState::pending || b.state==app::BatchState::captured){const auto progress=repo.repair_progress(b.id);info["repairs"]=static_cast<int>(progress.total);info["repairsDone"]=static_cast<int>(progress.done);info["amountRepairs"]=static_cast<int>(progress.amounts);info["unsupported"]=static_cast<int>(progress.blocked);}
                info["completed"]=b.completed;const auto result=repo.import_result(b.id);
                info["outcome"]=QString("Selected import: %1 added; %2 already present; %3 failed").arg(result.added).arg(result.already_present).arg(result.failed);
                if(b.completed)info["status"]="Complete";
                batches.push_back(info);
                if(!state.contains("importOutcome") || state.value("importOutcomeBatch").toString()!=selected.value("batch").toString() || b.id==str(selected.value("batch"))){state["importOutcomeBatch"]=text(b.id);state["importOutcome"]=info.value("outcome");}
                if(((b.state==app::BatchState::pending && !b.completed) || b.state==app::BatchState::captured) && (!state.contains("pending") || state.value("pending").toMap().value("id").toString()!=selected.value("batch").toString() || b.id==str(selected.value("batch")))) state["pending"]=info;
                if(b.state==app::BatchState::protected_drafts){std::vector<Money> values;for(std::size_t n=0;n<b.count;++n){const auto r=repo.row({b.id,n});if(!r.excluded_by_clear && r.draft && r.decision.disposition==imports::Disposition::include)values.push_back(r.draft->transaction.effective_total());}const auto change=imports::sum(acct.currency,values);info["change"]=money(change,amountFormat);info["after"]=money(repo.committed_balance()+change,amountFormat);auto retained=state.value("protectedImports").toList();retained.push_back(info);state["protectedImports"]=retained;}
            }
            state["batches"]=batches;state["manualBatches"]=manualBatches;
            QVariantList categoryDecisions;
            for(const auto& pending:repo.missing_import_categories()) {
                QVariantList names;for(const auto& name:pending.names)names.push_back(text(name));
                categoryDecisions.push_back(QVariantMap{{"source",text(pending.source)},{"names",names}});
            }
            state["importCategoryDecisions"]=categoryDecisions;
            for(const auto& group:repo.import_groups())if(group.state!=app::GroupState::aborted) {
                const auto member=std::find_if(group.accounts.begin(),group.accounts.end(),[&](const auto& value){return value.account==acct.identity;});
                if(member==group.accounts.end())continue;
                const auto own=std::find_if(batches.begin(),batches.end(),[&](const auto& v){return str(v.toMap().value("id"))==member->batch;});
                if(own==batches.end() || own->toMap().value("completed").toBool() || (state.contains("pending") && own->toMap().value("id")!=state.value("pending").toMap().value("id")) || own->toMap().value("status")=="Discarded" || own->toMap().value("status")=="Committed")continue;
                const auto document=statements::inspect(repo.evidence(member->batch),"");
                QVariantList importedCategories;const auto accountAliases=repo.account_aliases();
                for(const auto& c:document.categories)importedCategories.push_back(QVariantMap{{"name",text(c.name)},{"description",text(c.description)},{"type",c.type=='I'?"Income":c.type=='E'?"Expense":"Unspecified"},{"warning",std::any_of(accountAliases.begin(),accountAliases.end(),[&](const auto& alias){return app::account_name_matches(alias.name,c.name);})?"Import warning: existing account or alias; no ordinary category will be created":""},{"taxSchedule",text(c.tax_schedule)},{"budget",text(c.budget)},{"taxRelated",c.tax_related}});
                QVariantList blockers;
                const auto groupAccounts=repo.accounts();
                const auto accountName=[&](std::int64_t id){const auto account=std::find_if(groupAccounts.begin(),groupAccounts.end(),[&](const auto& a){return a.identity==id;});return account==groupAccounts.end()?QString::number(id):text(account->name);};
                for(const auto& item:group.blockers){
                    QVariantList candidates;
                    for(const auto& candidate:item.candidates)candidates.push_back(QVariantMap{{"account",QString::number(candidate.account)},{"name",accountName(candidate.account)},{"batch",text(candidate.batch)},{"record",static_cast<int>(candidate.ordinal)},{"allocation",text(candidate.allocation)},{"date",dateText(candidate.date)},{"amount",money(candidate.amount,amountFormat)},{"currency",text(candidate.amount.currency().code())},{"status",text(candidate.status)}});
                    blockers.push_back(QVariantMap{{"account",QString::number(item.account)},{"name",accountName(item.account)},{"batch",text(item.batch)},{"record",item.ordinal?static_cast<int>(*item.ordinal):-1},{"reason",text(item.reason)},{"allocation",text(item.allocation)},{"candidates",candidates},{"candidateCount",static_cast<int>(item.candidate_count)}});
                }
                state["importGroup"]=QVariantMap{{"id",text(group.id)},{"started",group.state==app::GroupState::started},{"blockers",blockers},{"unresolvedTransfers",static_cast<int>(group.unresolved_transfers)},{"ready",group.unresolved==0},{"repairs",static_cast<int>(group.repairs_total)},{"done",static_cast<int>(group.repairs_done)},{"amountRepairs",static_cast<int>(group.amount_repairs)},{"blocked",static_cast<int>(group.blocked_records)},{"unresolved",static_cast<int>(group.unresolved)},{"sourceAccount",static_cast<int>(member->source_account)},{"categories",importedCategories}};
                state["groupOpeningFixed"]=member->created && static_cast<int>(member->balance_choice)>=5;
                state["groupCreated"]=member->created && !acct.frozen && static_cast<int>(member->balance_choice)<5;
                state["groupOpeningReady"]=member->created && member->balance_choice!=app::ReportedBalanceChoice::unreviewed;
                state["groupReviewedChange"]=member->reviewed_change?money(*member->reviewed_change,amountFormat):QString("Incomplete");
                state["groupCanReviewBalance"]=member->created && member->reviewed_change.has_value() && !acct.frozen && static_cast<int>(member->balance_choice)<5;
                state["groupReportedBalance"]=document.accounts[member->source_account].reported_balance?text(*document.accounts[member->source_account].reported_balance):QString("Not supplied");
                state["groupProposedOpening"]=member->proposed_opening?money(*member->proposed_opening,amountFormat):QString("Not confirmed");
            }

            state["receipt"]=receipt;
            metadata=state;
            }
        } else state=metadata;
        state["sidebarAccounts"]=sidebarAccounts(state.value("accounts").toList(),sortDocument+"/SidebarOrder");
        if(!sessionEdits.empty() && !repo.can_undo_transaction_edit(sessionEdits.back().operation))sessionEdits.clear();
        if(!manualUndo.empty() && repo.ledger_edit_generation()!=manualUndoGeneration)manualUndo.clear();
        state["canUndoManual"]=!manualUndo.empty();
        state["canUndoEdit"]=!sessionEdits.empty();state["groupEditUndo"]=!sessionEdits.empty() && !sessionEdits.back().selection.empty();state["editUndoLabel"]=sessionEdits.empty()?QString("Undo edit"):sessionEdits.back().label;state["ledgerEditGeneration"]=QString::number(repo.ledger_edit_generation());
        state["importUndoPreview"]=importUndoPreview;
        state["importFeedback"]=importFeedback;state["importDestination"]=importDestination;state["importPlan"]=importPlan;state["importDateQuestion"]=importDateQuestion;
        QVariantList aliasList;for(const auto& alias:repo.account_aliases())aliasList.push_back(QVariantMap{{"name",text(alias.name)},{"accountId",QString::number(alias.account)}});
        state["accountAliases"]=aliasList;
        state["accountCategories"]=categoryById.value("@accounts");
        state["categories"]=categories;state["categoryById"]=categoryById;state["catalogRevision"]=QString::number(catalogRevision);
        state["categorySelected"]=categorySelected;state["categoryHistory"]=categoryHistory;state["categoryPreview"]=categoryPreview;state["categoryReceipt"]=categoryReceipt;
        if(repo.rules_supported() && !rulesLoaded) loadRules();
        state["rulesSupported"]=repo.rules_supported();state["rules"]=rules;state["ruleTombstones"]=ruleTombstones;state["ruleRevision"]=QString::number(ruleRevision);
        state["canUndoRules"]=repo.can_undo_account_rules();state["rulePreview"]=rulePreview;state["ruleReceipt"]=ruleReceipt;state["rulePrefill"]=rulePrefill;
        state["selected"]=selected;state["changePreview"]=changePreview;state["changeReceipt"]=changeReceipt;
        state["bulkSupported"]=repo.bulk_supported();state["bulkPreview"]=bulkPreview;state["bulkReceipt"]=bulkReceipt;
        state["reapplyPreview"]=reapplyPreview;state["reapplyReceipt"]=reapplyReceipt;
        state["history"]=history;state["entryHistory"]=entryHistory;state["historyTitle"]=historyTitle;state["historyMore"]=historyMore;state["historyCursor"]=historyCursor;
        state["mode"]=projected?"Projected":"Committed";state["sort"]=sort;state["descending"]=descending;
        state["selectedIndex"]=-1;
        // Failed records stay in import review; valid saved rows remain visible.
        if(!repo.account_selected()) return {state,{}};
        diag::cache(diag::Code::search,cachedView?diag::EventKind::cache_hit:diag::EventKind::cache_miss);
        if(!cachedView) {
            diag::Span searchSpan(diag::Code::search);
            diag::cache(diag::Code::ledger,cachedLedger?diag::EventKind::cache_hit:diag::EventKind::cache_miss);
            if(!cachedLedger) {
                diag::Span ledgerSpan(diag::Code::ledger);
                cachedLedger=std::make_shared<std::vector<app::LedgerRow>>(repo.current_ledger());
                if(newManual)cachedLedger->push_back({{newManualId,0},INT64_MAX,*newManual,false});
                // A presentation-only copy resolves current names for both visible
                // rows and literal filtering. Stored interpretations remain intact.
                std::map<std::tuple<std::int64_t,imports::Occurrence,std::string>,std::string> transferCategories;
                const auto endpointKey=[](const transfers::Endpoint& endpoint){return std::make_tuple(endpoint.account,endpoint.entry,endpoint.allocation);};
                if(repo.transfers_supported()){
                    std::map<std::int64_t,std::string> names;for(const auto& account:repo.accounts())names[account.identity]=account.name;for(const auto& account:repo.archived_accounts())names[account.identity]=account.name;
                    for(const auto& value:repo.transfers())if(!value.link.unresolved && !value.link.destination_released && value.link.origin.state!=transfers::State::removed && value.link.destination.state!=transfers::State::removed){
                        transferCategories[endpointKey(value.link.origin.endpoint)]=names.at(value.link.destination.endpoint.account)+" [Account]";
                        transferCategories[endpointKey(value.link.destination.endpoint)]=names.at(value.link.origin.endpoint.account)+" [Account]";
                    }
                }
                if(repo.categories_supported())for(auto& row:*cachedLedger) {
                    auto values=row.draft.transaction.allocations();
                    for(auto& value:values){
                        const auto transfer=transferCategories.find({repo.account().identity,row.identity,value.identity});
                        value.category=transfer==transferCategories.end()?allocationMap(value,categoryById,amountFormat).value("categoryDisplay").toString().toUtf8().toStdString():transfer->second;
                    }
                    row.draft.transaction=row.draft.transaction.corrected(row.draft.transaction.effective_total(),std::move(values));
                }
            }
            app::LedgerSearch query(filter.toStdString(),QDate::currentDate().year(),dateFormat);
            struct SourceFile {std::string bytes;std::vector<std::pair<std::size_t,std::size_t>> spans;};
            std::map<std::string,SourceFile> sources;std::size_t sourceBytes=0;
            const auto lookup=[&](const imports::Occurrence& ledgerId)->std::optional<std::string_view>{
                if(ledgerId.batch.starts_with("~transfer-") || ledgerId.batch==newManualId)return {};
                const auto original=repo.imported_original(ledgerId);if(!original)return {};const auto& id=*original;
                auto found=sources.find(id.batch);
                if(found==sources.end()){
                    if(context)context->checkpoint("Searching imported transactions");
                    SourceFile file;file.bytes=repo.evidence(id.batch);
                    if(file.bytes.size()>256*1024*1024-sourceBytes)throw std::runtime_error("Search source budget exceeds 256 MiB");
                    sourceBytes+=file.bytes.size();
                    const auto batches=repo.batches();const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==id.batch;});
                    if(batch==batches.end())throw std::runtime_error("Search source batch is unavailable");
                    const auto parsed=repo.parsed_source(batch->id,[&]{if(context)context->checkpoint();});
                    if(!parsed.admissible || parsed.records.size()!=batch->count)throw std::runtime_error("The imported file could not be read completely for search");
                    for(const auto& record:parsed.records)file.spans.emplace_back(record.begin,record.end);
                    found=sources.emplace(id.batch,std::move(file)).first;
                }
                if(id.ordinal>=found->second.spans.size())throw std::runtime_error("Search source occurrence is unavailable");
                const auto [begin,end]=found->second.spans[id.ordinal];
                return std::string_view(found->second.bytes).substr(begin,end-begin);
            };
            auto view=app::search_ledger(*cachedAccount,*cachedLedger,query,sort,descending,lookup,[&]{return context && context->state.load()==app::OperationContext::requested;});
            if(newManual || revealManual){
                const auto id=newManual?imports::Occurrence{newManualId,0}:*revealManual;
                const auto complete=app::ledger_view(*cachedAccount,*cachedLedger,"",sort,descending);
                for(const auto index:complete.order)if(view.rows[index].identity==id && std::find(view.order.begin(),view.order.end(),index)==view.order.end()) {
                    const auto rank=std::find(complete.order.begin(),complete.order.end(),index);
                    auto where=std::find_if(view.order.begin(),view.order.end(),[&](auto other){return std::find(complete.order.begin(),complete.order.end(),other)>rank;});
                    view.order.insert(where,index);
                    const auto amount=view.rows[index].draft.transaction.effective_total();
                    if(amount.units()<0)view.withdrawals=view.withdrawals+amount;else view.deposits=view.deposits+amount;
                    break;
                }
            }
            cachedView=std::make_shared<const app::LedgerView>(std::move(view));
        }
        state["withdrawals"]=money(cachedView->withdrawals,amountFormat);state["deposits"]=money(cachedView->deposits,amountFormat);
        if(selected.contains("ordinal")) {
            for(std::size_t position=0;position<cachedView->order.size();++position) {
                const auto& id=cachedView->rows[cachedView->order[position]].identity;
                if(id.batch==selectedBatch && id.ordinal==selectedOrdinal) {state["selectedIndex"]=static_cast<int>(position);break;}
            }
        }
        std::set<imports::Occurrence> retained;
        for(const auto& row:cachedView->rows)if(selection.contains(row.identity))retained.insert(row.identity);
        selection=std::move(retained);
        cachedLinkedRows.clear();
        if(repo.transfers_supported())for(const auto& value:repo.transfers())if(!value.link.unresolved && value.link.origin.state!=transfers::State::removed){cachedLinkedRows.insert(value.link.origin.endpoint.entry);if(value.link.destination.state!=transfers::State::removed)cachedLinkedRows.insert(value.link.destination.endpoint.entry);}
        QVariantList selectedIds;QVariantMap selectedKeys;
        for(const auto& id:selection){QVariantMap value{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)}};for(const auto& row:cachedView->rows)if(row.identity==id){value=draft(row.draft,categoryById,dateFormat,amountFormat);value["batch"]=text(id.batch);value["ordinal"]=static_cast<int>(id.ordinal);value["committed"]=row.committed;break;}value["manual"]=state.value("manualBatches").toMap().contains(text(id.batch));value["transferLinked"]=cachedLinkedRows.contains(id);selectedIds.push_back(value);selectedKeys[text(id.batch)+":"+QString::number(id.ordinal)]=true;}
        state["filter"]=filter;state["transferReceipt"]=transferReceipt;state["transferCandidates"]=transferCandidates;state["candidateLink"]=candidateLink;state["transferMergePreview"]=transferMergePreview;
        state["selection"]=selectedIds;state["selectionKeys"]=selectedKeys;state["selectionCount"]=static_cast<int>(selection.size());
        int focus=-1;
        if(selectionFocus)for(std::size_t i=0;i<cachedView->order.size();++i)if(cachedView->rows[cachedView->order[i]].identity==*selectionFocus){focus=static_cast<int>(i);break;}
        state["selectionIndex"]=focus;
        return {state,cachedView};
    }
    void publishPreview(const app::RulePreviewResult& result,bool single) {
        QVariantList records;
        for(const auto& match:result.matches)records.push_back(QVariantMap{
            {"batch",text(match.identity.batch)},{"ordinal",static_cast<int>(match.identity.ordinal)},
            {"date",dateText(match.date)},{"payee",text(match.payee)},{"amount",money(match.amount,amountFormat)}});
        const auto count=result.matches.size();
        rulePreview={{"label",single?QString("This rule · current account"):result.unsaved_rules?QString("Unsaved rules"):QString("Saved rules")},
            {"unsaved",result.unsaved_rules},{"single",single},{"batch",text(result.batch)},{"batchRevision",QString::number(result.batch_revision)},
            {"rulesetRevision",QString::number(result.ruleset_revision)},{"catalogRevision",QString::number(result.catalog_revision)},{"replaceManual",result.replace_manual},
            {"operation",result.unsaved_rules?QString():text(ruleApplyOperation)},{"evaluated",static_cast<int>(result.evaluated)},{"changed",static_cast<int>(result.changed)},
            {"matched",static_cast<int>(count)},{"records",records},
            {"summaryText",QString("%1 transactions match. %2 already categorized. %3 handled by earlier rules. %4 eligible; %5 will change.").arg(result.raw_matches).arg(result.categorized_matches).arg(result.earlier_matches).arg(result.eligible_matches).arg(result.changed)}};
    }
    void execute(const QString& action,const QVariantMap& input, app::OperationContext& context) {
        diagnosticSaved=false;diagnosticUnchanged=false;diagnosticPresentationFailed=false;
        auto a=input;
        if(active && a.contains("date"))a["date"]=text(parse_display_date(str(a.value("date")),dateFormat));
        if(active && action=="map_import") {
            auto mappings=a.value("mappings").toList();
            for(auto& item:mappings){auto mapping=item.toMap();if(mapping.value("create").toBool())mapping["date"]=text(parse_display_date(str(mapping.value("date")),dateFormat));item=mapping;}
            a["mappings"]=mappings;
        }
        context.checkpoint();
        overwriteRequired=false;
        if(action=="prepare_create") {
            if(active) throw std::logic_error("Close the current dataset first");
            path=a.value("path").toString();canOpen=false;welcomePassword=false;return;
        }
        if(action=="probe") {
            if(active) throw std::logic_error("Close the current dataset first");
            path=a.value("path").toString();canOpen=false;welcomePassword=false;
            if(!QFileInfo(path).isFile()) throw std::runtime_error("Dataset file not found or inaccessible");
            try {gateway->open_controlled(path.toStdString(),std::nullopt,context);if(gateway->repository().startup_import_cleanup())cleanupNoticePath=path;gateway->close();canOpen=true;}
            catch(const app::UnlockRequired&) {welcomePassword=true;}
            return;
        }
        if(action!="select" && action!="query" && action!="inspect" && action!="evidence" && action!="backup" && action!="history" && action!="entry_history" && action!="preview_change" && action!="cancel_change" && action!="cancel_merge" && action!="cancel_reapply" && action!="category_select" && action!="preview_category" && action!="cancel_category_preview" && action!="rules_load" && action!="rules_discard" && action!="rules_preview" && action!="rules_prefill") {if(action!="select_account")deferReview=false;invalidate();}
        if(action=="close" || action=="open" || action=="unlock" || action=="create"){importUndoPreview.clear();importDestination=false;importFeedback.clear();dateImport.reset();importDateQuestion.clear();preparedImport.reset();preparedMappings.reset();importMappingArgs.clear();importPlan.clear();accountViews.clear();transferReceipt.clear();transferCandidates.clear();candidateLink.clear();transferMergePreview.clear();preparedMerge.reset();mergeOperation.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();}
        if(action=="create" || action=="open" || action=="unlock") {
            if(active) throw std::logic_error("Close the current dataset first");
            path=a.value("path").toString();
            if(action=="create") {
                if(a.value("protected",true).toBool() && a.value("password").toString().isEmpty()) throw std::invalid_argument("Enter a password to create a password-protected dataset.");
                if(a.value("protected",true).toBool() && a.value("password")!=a.value("confirmation")) throw std::invalid_argument("Passwords do not match");
                const QFileInfo location(path);
                if(location.exists() && !a.value("overwrite").toBool()) {overwriteRequired=true;throw std::runtime_error((path+" already exists").toStdString());}
                if(!QDir().mkpath(location.absolutePath())) throw std::runtime_error("Cannot create dataset directory");
                gateway->create_configured(path.toStdString(),a.value("overwrite").toBool(),a.value("protected",true).toBool(),str(a.value("password")),str(a.value("name")),Currency(str(a.value("currency"))),QDate::currentDate().toString(Qt::ISODate).toStdString(),{str(a.value("dateFormat","YYYY-MM-DD")),a.value("defaultCategories",false).toBool()});observedSaved();
            } else {
                canOpen=false;welcomePassword=false;
                try {gateway->open_controlled(path.toStdString(),action=="unlock"?std::optional(str(a.value("password"))):std::nullopt,context);}
                catch(const app::UnlockRequired&) {
                    if(action!="open" || !a.value("startup").toBool())throw;
                    welcomePassword=true;return;
                }
                canOpen=true;if(gateway->repository().startup_import_cleanup())cleanupNoticePath=path;
                try { context.seal(); } catch (...) { gateway->close(); throw; }
            }
            startupCleanupNotice=action!="create" && (gateway->repository().startup_import_cleanup() || cleanupNoticePath==path);cleanupNoticePath.clear();
            deferReview=gateway->lazy_review() && action!="create";
            sortAccount.reset();sortDocument=sortSettingsGroup();
            dateFormat=gateway->repository().date_format();amountFormat=gateway->repository().amount_format();active=true; selected.clear(); receipt.clear();clearChanges();
            if(action=="create"){QSettings settings;settings.remove(sortDocument);settings.sync();if(settings.status()!=QSettings::NoError)throw std::runtime_error("The file was created, but its sort preferences could not be reset. Reopen the file before continuing.");}
            QSettings settings; settings.setValue("datasetPath",path); settings.sync();
            if(settings.status()!=QSettings::NoError) throw std::runtime_error("Dataset is open, but the startup path could not be saved in Settings");
            return;
        }
        if(action=="close") {if(a.value("skipAudit").toBool())gateway->close();else gateway->close_verified(context);clearChanges();active=false;selected.clear();receipt.clear();selectedBatch.clear();filter.clear();projected=true;sort=0;descending=false;return;}
        if(action=="cancel_merge") {preparedMerge.reset();mergeOperation.clear();transferMergePreview.clear();return;}
        if(action=="cancel_reapply") {preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();return;}
        if(!active) throw std::logic_error("Open a dataset first");
        auto& repo=gateway->repository();
        if(action=="amount_format") {
            const AmountFormat requested{a.value("enabled").toBool(),str(a.value("style"))};
            validate_amount_format(requested);
            try {repo.set_amount_format(requested);}
            catch(...) {
                const auto failure=std::current_exception();
                try {amountFormat=repo.amount_format();}catch(...){gateway->close();active=false;throw std::runtime_error("Reopen the file to check whether the amount format was saved.");}
                invalidate();if(amountFormat!=requested)std::rethrow_exception(failure);
            }
            observedSaved();amountFormat=requested;invalidate();
            selected.clear();selectedBatch.clear();history.clear();entryHistory.clear();historyLoaded=false;
            rulePreview.clear();reapplyPreview.clear();bulkPreview.clear();changePreview.clear();
            receipt.clear();changeReceipt.clear();bulkReceipt.clear();transferReceipt.clear();reapplyReceipt.clear();
            transferCandidates.clear();transferMergePreview.clear();reviewLinkPreview.clear();
            return;
        }
        if(action=="query" || action=="select_account")revealManual.reset();
        if(action=="review_load") {context.checkpoint("Preparing Review");return;}
        if(action=="link_exclusion_cancel"){preparedLinkExclusion.reset();linkExclusionPreview.clear();return;}
        if(action=="link_exclusion_preview"){
            preparedLinkExclusion.reset();linkExclusionPreview.clear();
            app::LinkExclusion rule;rule.identity=a.value("identity").toString().isEmpty()?identity():str(a.value("identity"));rule.field=str(a.value("field"));rule.expression=str(a.value("expression"));rule.account=a.value("account").toString().toLongLong();rule.regex=a.value("regex").toBool();rule.removed=a.value("removed").toBool();
            const auto preview=repo.preview_link_exclusion(rule);QVariantList comparisons;for(const auto& value:preview.comparisons)comparisons.push_back(text(value));
            preparedLinkExclusion=rule;linkExclusionGeneration=preview.generation;linkExclusionPreview={{"ready",true},{"comparisons",comparisons}};return;
        }
        if(action=="link_exclusion_save"){
            if(!preparedLinkExclusion)throw std::invalid_argument("Preview the exclusion first");
            const auto op=identity();try{repo.save_link_exclusion(op,*preparedLinkExclusion,linkExclusionGeneration);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
            preparedLinkExclusion.reset();linkExclusionPreview.clear();return;
        }
        if(action=="assign_transfer_batch" || action=="release_transfer_batch") {
            if(selection.empty() || selection.size()>4096)throw std::invalid_argument("Select between 1 and 4096 transactions");
            const auto destinationId=a.value("destination").toString().toLongLong();
            const auto accounts=repo.accounts();const auto destination=std::find_if(accounts.begin(),accounts.end(),[&](const auto& account){return account.identity==destinationId;});
            const bool release=action=="release_transfer_batch";
            if(!release && destination==accounts.end())throw std::invalid_argument("Choose a destination account");
            if(release){
                app::CategorySelection request;request.account=repo.account().identity;request.generation=repo.ledger_edit_generation();request.category=str(a.value("category"));
                for(const auto& id:selection)request.selected.push_back({id,selection.size()==1?str(a.value("allocation")):std::string{}});
                const auto op=identity();
                try{repo.assign_categories(op,request,context);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
                observedSaved();selected.clear();selectedBatch.clear();transferReceipt={{"summary","Categories changed. Undo reverses the whole selection."}};return;
            }
            const auto op=identity();std::vector<app::TransferRequest> requests;std::size_t serial=0;
            for(const auto& id:selection){
                context.checkpoint("Reading selected transfers");
                const auto choice=read_transfer_choice(repo,{{"account",QString::number(repo.account().identity)},{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},{"allocation",selection.size()==1?a.value("allocation"):QVariant()}},false);
                const auto prefix=op+"-"+std::to_string(++serial);
                app::TransferRequest request{};request.action=app::TransferAction::retarget;request.mode=1;
                request.expected_sequence=a.value("sequence").toString().toLongLong();request.link=choice.linked?choice.link:prefix;
                request.kept=prefix+"-kept";request.left=choice.fact;request.batch_revision=choice.revision;request.payee=choice.payee;request.memo=choice.memo;
                {request.generated=transfers::Endpoint{destinationId,{"~transfer-"+prefix+"-destination",0},"generated"};request.generated_currency=destination->currency.definition_id();}
                requests.push_back(std::move(request));
            }
            try{repo.assign_transfer_batch(op,requests,context);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
            observedSaved();selected.clear();selectedBatch.clear();transferReceipt={{"summary","Transfers assigned. Undo reverses the whole batch."}};return;
        }
        if(action=="undo_transfer_assignment") {
            if(!repo.can_undo_transfer_assignment())throw std::invalid_argument("Later changes prevent transfer assignment Undo");
            const auto op=identity();try{repo.undo_review(op);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
            observedSaved();selected.clear();selectedBatch.clear();transferReceipt.clear();return;
        }
        if(action=="review_batch") {
            std::vector<app::ReviewAnswer> answers;
            for(const auto& item:a.value("groups").toList()){
                const auto group=item.toMap();app::ReviewAnswer answer;answer.action=app::ReviewAction::merge;answer.generation=group.value("generation").toString().toLongLong();answer.question=str(group.value("question"));
                for(const auto& value:group.value("selected").toList()){const auto row=value.toMap();answer.selected.push_back({str(row.value("batch")),static_cast<std::size_t>(row.value("ordinal").toUInt())});}
                if(group.contains("survivor")){const auto row=group.value("survivor").toMap();answer.survivor=imports::Occurrence{str(row.value("batch")),static_cast<std::size_t>(row.value("ordinal").toUInt())};}
                answers.push_back(std::move(answer));
            }
            const auto op=identity();try{repo.merge_review_batch(op,answers,context);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
            diagnosticSaved=true;selected.clear();return;
        }
        if(action=="review_answer" || action=="review_link_preview") {
            app::ReviewAnswer answer;answer.generation=a.value("generation").toString().toLongLong();answer.question=str(a.value("question"));
            const auto kind=a.value("answer").toString();answer.action=kind=="accept_missing"?app::ReviewAction::accept_missing:kind=="merge"?app::ReviewAction::merge:kind=="link"?app::ReviewAction::link:kind=="not_linked"?app::ReviewAction::not_linked:app::ReviewAction::separate;
            for(const auto& item:a.value("selected").toList()){const auto row=item.toMap();answer.selected.push_back({str(row.value("batch")),static_cast<std::size_t>(row.value("ordinal").toUInt())});}
            if(a.contains("survivor")){const auto row=a.value("survivor").toMap();answer.survivor=imports::Occurrence{str(row.value("batch")),static_cast<std::size_t>(row.value("ordinal").toUInt())};}
            answer.left_allocation=str(a.value("leftAllocation"));answer.right_allocation=str(a.value("rightAllocation"));answer.remainder_category=str(a.value("category"));answer.split_difference=a.value("splitDifference").toBool();
            if(action=="review_link_preview"){
                const auto sheet=repo.review_sheet();if(sheet.generation!=answer.generation)throw std::invalid_argument("The transactions changed. Review them again.");
                const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& value){return value.key==answer.question;});if(q==sheet.questions.end() || answer.selected.size()!=2)throw std::invalid_argument("Select one counterpart");
                const auto part=[&](const imports::Occurrence& id,const std::string& allocation){const auto row=std::find_if(q->rows.begin(),q->rows.end(),[&](const auto& r){return r.identity==id;});if(row==q->rows.end())throw std::invalid_argument("Transaction changed");const auto& aa=row->draft.transaction.allocations();if(allocation.empty() && aa.size()==1)return aa.front().amount;const auto found=std::find_if(aa.begin(),aa.end(),[&](const auto& v){return v.identity==allocation;});if(found==aa.end())throw std::invalid_argument("Choose the split to link");return found->amount;};
                const auto left=part(answer.selected[0],answer.left_allocation),right=part(answer.selected[1],answer.right_allocation);
                const auto magnitude=[](std::int64_t n)->std::uint64_t{return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);};
                if(left.currency()!=right.currency() || left.units()==0 || right.units()==0 || (left.units()<0)==(right.units()<0))throw std::invalid_argument("Choose opposite amounts in the same currency");
                if(magnitude(left.units())!=magnitude(right.units())){
                    const auto minimum=std::min(magnitude(left.units()),magnitude(right.units()));const auto difference=std::max(magnitude(left.units()),magnitude(right.units()))-minimum;
                    if(minimum>INT64_MAX || difference>INT64_MAX)throw std::overflow_error("Amount exceeds display capacity");
                    reviewLinkPreview={{"args",a},{"common",money(Money(static_cast<std::int64_t>(minimum),left.currency()),amountFormat)},{"remainder",money(Money(static_cast<std::int64_t>(difference),left.currency()),amountFormat)}};return;
                }
            }
            const auto op=identity();try{repo.answer_review(op,answer);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}
            diagnosticSaved=true;reviewLinkPreview.clear();selected.clear();return;
        }
        if(action=="review_cancel_link"){reviewLinkPreview.clear();return;}
        if(action=="review_undo") {const auto op=identity();try{repo.undo_review(op);}catch(...){const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;try{outcome=repo.review_outcome(op);}catch(...){throw app::OperationOutcomeUnknown();}if(!outcome)std::rethrow_exception(failure);}diagnosticSaved=true;reviewLinkPreview.clear();selected.clear();return;}
        if(action=="transfer_candidates"){
            transferCandidates.clear();candidateLink=a.value("link").toString();
            const auto links=repo.transfers();const auto found=std::find_if(links.begin(),links.end(),[&](const auto& value){return value.link.identity==str(a.value("link"));});
            if(found==links.end())throw std::invalid_argument("Choose a transfer");
            const auto& side=found->link.origin.endpoint.account==repo.account().identity?found->link.origin:found->link.destination;
            bool full=false;
            for(const auto& batch:repo.batches())if(batch.state==app::BatchState::pending && !batch.blocked){
                for(std::size_t ordinal=0;ordinal<batch.count;++ordinal){context.checkpoint("Finding amount-compatible bank entries");const auto row=repo.row({batch.id,ordinal});if(repo.transaction_accepted(row.identity) || row.matched_generated || !row.draft || row.decision.disposition!=imports::Disposition::include)continue;
                    for(const auto& allocation:row.draft->transaction.allocations())if(allocation.amount==side.amount){
                        bool used=false;for(const auto& link:links)for(const auto* end:{&link.link.origin,&link.link.destination})if(end->state!=transfers::State::removed){const transfers::Endpoint candidate{repo.account().identity,{batch.id,ordinal},allocation.identity};used|=end->endpoint==candidate || (end->attached_evidence && end->attached_evidence->endpoint==candidate) || (end->proposed_evidence && end->proposed_evidence->endpoint==candidate);}
                        if(used)continue;
                        if(transferCandidates.size()==100){full=true;break;}
                        transferCandidates.push_back(QVariantMap{{"batch",text(batch.id)},{"ordinal",static_cast<int>(ordinal)},{"allocation",text(allocation.identity)},{"label",dateText(row.draft->date)+" · "+text(row.draft->payee)+" · "+money(allocation.amount,amountFormat)}});
                    }
                    if(full)break;
                }
                if(full)break;
            }
            transferReceipt={{"summary",full?"First 100 amount-compatible candidates shown; inspect another source record to match it manually.":"Candidates share an exact amount. Check the imported transaction before choosing a match; equal amounts alone do not establish a match."}};
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
                request.action=app::TransferAction::manual;request.destination=a.value("destination").toString().toLongLong();request.amount=evaluate_amount(str(a.value("amount")),repo.account().currency,amountFormat);request.date=str(a.value("date"));request.payee=str(a.value("payee"));request.memo=str(a.value("memo"));
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
                    if(found)throw std::invalid_argument("Choose which split to transfer.");
                    found=&value.link;
                }
                if(!found)throw std::invalid_argument("Transfer entry is unavailable");
                request.action=app::TransferAction::clear_local;request.link=found->identity;
            }else if(action=="transfer_match"){
                request.action=app::TransferAction::propose_match;const imports::Occurrence id{a.contains("batch")?str(a.value("batch")):selectedBatch,a.contains("ordinal")?a.value("ordinal").toUInt():selectedOrdinal};
                const auto row=repo.row(id);if(!row.draft)throw std::invalid_argument("Choose a valid bank entry");
                const auto& allocations=row.draft->transaction.allocations();const auto allocation=std::find_if(allocations.begin(),allocations.end(),[&](const auto& value){return value.identity==str(a.value("allocation"));});if(allocation==allocations.end())throw std::invalid_argument("Choose a split from the imported transaction");
                const auto batches=repo.batches();const auto batch=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==id.batch;});if(batch==batches.end())throw std::invalid_argument("Import unavailable");request.batch_revision=batch->revision;
                request.evidence=transfers::BankEvidence{{repo.account().identity,id,allocation->identity},allocation->amount,row.draft->date};
            }else if(action=="transfer_unmatch"){
                request.action=app::TransferAction::discard_match;for(const auto& value:repo.transfers())if(value.link.identity==request.link){const auto& side=value.link.origin.endpoint.account==repo.account().identity?value.link.origin:value.link.destination;request.evidence=side.proposed_evidence;}
            }else if(action=="transfer_merge"){
                const auto endpoint_of=[&](const char* prefix){const auto name=QString::fromLatin1(prefix);return QVariantMap{{"account",a.value(name+"Account")},{"batch",a.value(name+"Batch")},{"ordinal",a.value(name+"Ordinal")},{"allocation",a.value(name+"Allocation")}};};
                const auto left=read_transfer_choice(repo,endpoint_of("left"));
                const auto right=read_transfer_choice(repo,endpoint_of("right"));
                if(left.fact.endpoint==right.fact.endpoint)throw std::invalid_argument("Choose two splits");
                if(left.linked && right.linked && left.link!=right.link)throw std::invalid_argument("These splits belong to different transfers. Choose splits from the same transfer.");
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
                    transferMergePreview={{"id",text(operation)},{"note",text(preview.note)},{"balanceImpact",balanceImpact(preview.before,preview.change,preview.after,amountFormat)}};
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
            transferReceipt={{"operation",text(operation)},{"summary","Transfer operation saved · "+balanceImpact(result->before,result->change,result->after,amountFormat)},{"balanceImpact",balanceImpact(result->before,result->change,result->after,amountFormat)}};observedSaved();
            transferCandidates.clear();candidateLink.clear();preparedMerge.reset();mergeOperation.clear();transferMergePreview.clear();
            if(selectedBatch.starts_with("~transfer-")){
                bool present=false;const auto account=repo.account().identity;
                for(const auto& value:repo.transfers())for(const auto* side:{&value.link.origin,&value.link.destination})
                    present=present || (side->state!=transfers::State::removed && side->endpoint.account==account && side->endpoint.entry==imports::Occurrence{selectedBatch,selectedOrdinal});
                if(!present){selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();}
            }
            try {if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);} catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Transfer operation was saved, but its selection could not be refreshed. Reopen and inspect transfers before retrying.");}
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
                if(action=="correct_opening") return saved.kind==4 && saved.opening_date==str(a.value("date")) && saved.opening==evaluate_amount(str(a.value("amount")),saved.currency,amountFormat);
                if(action=="reassign_currency") return saved.kind==5 && saved.currency==Currency(str(a.value("currency")));
                return saved.kind==6;
            };
            auto settle=[&]{
                PresentationRead observation{diagnosticPresentationFailed};
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
                else if(action=="correct_opening") {const auto acct=repo.account();repo.correct_opening(operation,str(a.value("date")),evaluate_amount(str(a.value("amount")),acct.currency,amountFormat));}
                else if(action=="reassign_currency") repo.reassign_currency(operation,Currency(str(a.value("currency"))));
                else repo.undo_account_operation(operation,str(a.value("undoOf")));
            } catch(...) {
                const auto failure=std::current_exception();
                std::optional<app::AccountCommandReceipt> saved;
                try {saved=repo.account_command_outcome(operation);}
                catch(...) {gateway->close();active=false;selected.clear();receipt.clear();selectedBatch.clear();throw std::runtime_error("Account command outcome is uncertain. Reopen and inspect the account before retrying.");}
                if(!saved || !matches(*saved)) std::rethrow_exception(failure);
            }
            observedSaved();settle();
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
            if(action=="create_account")observedSaved();
            std::optional<std::int64_t> priorAccount;std::optional<AccountView> priorView;
            if(repo.account_selected()){priorAccount=repo.account().identity;priorView=AccountView{filter,projected,sort,descending,selectedBatch,selectedOrdinal,selection,selectionAnchor,selectionFocus};}
            try {repo.select_account(id);}
            catch(...) {
                if(action!="create_account")throw;
                diagnosticPresentationFailed=true;
                gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();
                throw std::runtime_error("Account was created, but could not be selected. Reopen and inspect the account list before retrying.");
            }
            selected.clear();selectedBatch.clear();receipt.clear();clearChanges();
            selection.clear();selectionAnchor.reset();selectionFocus.reset();filter.clear();projected=true;sort=0;descending=false;
            if(priorAccount) accountViews.insert_or_assign(*priorAccount,std::move(*priorView));
            if(const auto saved=accountViews.find(id);saved!=accountViews.end()){
                const auto& view=saved->second;filter=view.filter;projected=view.projected;sort=view.sort;descending=view.descending;selection=view.selection;selectionAnchor=view.anchor;selectionFocus=view.focus;
                if(!view.batch.empty())inspect(view.batch,view.ordinal,context);
            }
            if(!a.value("reviewBatch").toString().isEmpty()){selectedBatch=str(a.value("reviewBatch"));selected={{"batch",a.value("reviewBatch")}};}
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
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("Category change outcome is uncertain. Reopen the file and inspect category history before retrying.");}
                if(!result)std::rethrow_exception(failure);
            }
            categoryReceipt={{"operation",text(result->operation)},{"summary","Category updated"}};observedSaved();
            preparedCategory.reset();categoryOperation.clear();categoryPreview.clear();
            try {catalog();selectCategory(result->after.identity);if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);}
            catch(...) {const auto saved=categoryReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();categoryReceipt=saved;throw std::runtime_error("Category change was saved, but its refreshed view could not be loaded. Reopen and inspect category history; do not apply it again.");}
        } else if(action=="history") {listHistory(a.value("before").toString().toLongLong(),a.value("append").toBool());}
        else if(action=="entry_history") {
            const imports::Occurrence id{str(a.value("batch")),a.value("ordinal").toUInt()};inspect(id.batch,id.ordinal,context);entryDetails(id);
            if(!historyLoaded)listHistory();
        } else if(action=="cancel_change") {preparedChange.reset();preparedOperation.clear();changePreview.clear();}
        else if(action=="preview_change") {
            preparedChange.reset();preparedOperation.clear();changePreview.clear();
            const auto kind=a.value("kind").toString();
            const bool historyUndo=kind=="undo" && a.contains("batch") && a.contains("ordinal");
            if(!historyUndo && (selectedBatch.empty() || !selected.value("committed").toBool()))throw std::logic_error("Select an accepted transaction first");
            const imports::Occurrence id=historyUndo?imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}:imports::Occurrence{selectedBatch,selectedOrdinal};
            const auto entry=repo.committed_entry(id);
            if(kind!="undo" && (!selectedCommittedToken || entry.token!=*selectedCommittedToken))throw std::logic_error("This transaction changed. Refresh and review it before editing.");
            app::ChangeRequest request{app::ChangeKind::remove,id,std::nullopt,{}};
            if(kind=="correct") {request.kind=app::ChangeKind::correct;request.correction=editedDraft(a,entry.current.draft.transaction.original_imported_amount(),repo,&entry.current.draft,&entry.source,amountFormat);}
            else if(kind=="undo") {request.kind=app::ChangeKind::undo;request.undo_of=str(a.value("operation"));}
            else if(kind!="remove")throw std::invalid_argument("Unknown committed change");
            auto preview=repo.preview_committed_change(request);
            preparedOperation=identity();preparedChange=preview;
            auto impact=balanceImpact(preview.before,preview.change,preview.after,amountFormat);
            if(!preview.relationship.empty())impact+="\n"+text(preview.relationship);
            changePreview={{"id",text(preparedOperation)},{"title",changeName(request.kind)+" preview"},
                {"applyLabel",request.kind==app::ChangeKind::correct?"Apply correction":request.kind==app::ChangeKind::remove?"Remove transaction":"Apply Undo"},
                {"balanceImpact",impact},
                {"snapshots",QVariantList{snapshotMap(preview.before_entry,"Before",dateFormat,amountFormat),snapshotMap(preview.after_entry,"After",dateFormat,amountFormat)}}};
        } else if(action=="apply_change") {
            changeReceipt.clear();
            if(!preparedChange || str(a.value("preview"))!=preparedOperation)throw std::logic_error("Review a fresh committed-change preview first");
            const auto preview=*preparedChange;
            std::optional<app::ChangeReceipt> result;
            context.checkpoint("Saving changes");
            try {result=repo.apply_committed_change_with_catalog(preparedOperation,preview.request,preview.expected,preview.preview_sequence,preview.catalog_revision);}
            catch(...) {
                const auto failure=std::current_exception();
                try {result=repo.committed_change_outcome(preparedOperation);}
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("SpentIt could not confirm whether your change was saved. Reopen the file and check the transaction before trying again.");}
                if(!result)std::rethrow_exception(failure);
            }
            changeReceipt={{"operation",text(result->operation)},{"summary",changeName(result->kind)+" applied · "+balanceImpact(result->before,result->change,result->after,amountFormat)}};observedSaved();
            preparedChange.reset();preparedOperation.clear();changePreview.clear();
            try {inspect(result->identity.batch,result->identity.ordinal,context);entryDetails(result->identity);if(historyLoaded)listHistory();}
            catch(...) {
                const auto saved=changeReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();changeReceipt=saved;
                throw std::runtime_error("Your change was saved, but the screen could not update. Reopen the file and check the transaction before making more changes.");
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
                lines<<dateText(entry.before_entry.draft.date)+" · "+text(entry.before_entry.draft.payee)+" · "+money(entry.before_entry.draft.transaction.effective_total(),amountFormat);
                lines<<(entry.before_entry.active?"Before: active":"Before: removed")<< (entry.after_entry.active?"After: active":"After: removed");
                const auto& before=entry.before_entry.draft.transaction.allocations();const auto& after=entry.after_entry.draft.transaction.allocations();
                for(std::size_t i=0;i<before.size();++i)lines<<"  "+money(before[i].amount,amountFormat)+" · "+allocationMap(before[i],categoryById,amountFormat).value("categoryDisplay").toString()+" → "+allocationMap(after[i],categoryById,amountFormat).value("categoryDisplay").toString()+" · "+text(before[i].memo);
            }
            bulkOperation=identity();preparedBulk=preview;
            bulkPreview={{"id",text(bulkOperation)},{"title",(request.kind==app::ChangeKind::remove?QString("Delete "):request.kind==app::ChangeKind::correct?QString("Change category for "):QString("Undo changes to "))+QString::number(preview.entries.size())+" transactions"},{"impact",balanceImpact(preview.before,preview.change,preview.after,amountFormat)},{"lines",lines}};
        } else if(action=="apply_bulk") {
            bulkReceipt.clear();if(!preparedBulk || str(a.value("preview"))!=bulkOperation)throw std::logic_error("Review a fresh bulk preview first");
            std::optional<app::BulkReceipt> result;
            try{result=repo.apply_bulk(bulkOperation,*preparedBulk);}
            catch(...){const auto failure=std::current_exception();try{result=repo.bulk_outcome(bulkOperation);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Bulk outcome is uncertain. Reopen and check Change history before retrying.");}if(!result)std::rethrow_exception(failure);}
            bulkReceipt={{"operation",text(result->operation)},{"summary",changeName(result->request.kind)+" applied to "+QString::number(result->entries.size())+" transactions · "+balanceImpact(result->before,result->change,result->after,amountFormat)}};observedSaved();
            preparedBulk.reset();bulkOperation.clear();bulkPreview.clear();
            selected.clear();selectedBatch.clear();selectedCommittedToken.reset();
            try{if(historyLoaded)listHistory();}catch(...){throw std::runtime_error("Bulk change was saved, but history could not be refreshed. Reopen and check Change history; do not apply it again.");}
        } else if(action=="rules_reapply_preview") {
            if(!repo.rules_supported()) throw std::invalid_argument("Rules are not enabled for this dataset");
            preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();
            auto preview=repo.preview_rule_reapply(a.value("replaceManual").toBool(),str(a.value("undoOf")),context);
            QStringList lines;
            for(const auto& record:preview.records){
                lines<<dateText(record.before.date)+" · "+text(record.before.payee);
                for(const auto& diff:record.diffs) lines<<fieldName(diff.field)+" · "+text(diff.before_text)+" -> "+text(diff.after_text)+" · "+diffWording(diff.removed,diff.assigned);
            }
            for(const auto& skip:preview.skips) lines<<text(skip.note);
            QString omitted;
            if(lines.size()>40){omitted=QString::number(lines.size()-40)+" lines omitted.";lines=lines.mid(0,40);lines<<omitted;}
            reapplyOperation=identity();preparedReapply=std::move(preview);
            const auto& request=preparedReapply->request;
            reapplyPreview={{"id",text(reapplyOperation)},{"title",request.undo_of.empty()?QString("Reapply rules to this account"):QString("Undo rule reapply")},
                {"evaluated",static_cast<int>(request.evaluated)},{"changed",static_cast<int>(request.changed)},
                {"impact",balanceImpact(preparedReapply->before,preparedReapply->change,preparedReapply->after,amountFormat)+"\nAccount balance is unchanged."},
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
                catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error("Rule reapply outcome is uncertain. Reopen and check Change history before retrying.");}
                if(!result){preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();std::rethrow_exception(failure);}
            }
            if(result->operation.empty())diagnosticUnchanged=true;else observedSaved();
            reapplyReceipt=result->operation.empty()?QVariantMap{{"operation",QString()},{"summary","No accepted transactions were changed"}}
                :QVariantMap{{"operation",text(result->operation)},{"summary",(request.undo_of.empty()?QString("Rule reapply "):QString("Undo rule reapply "))+"changed "+QString::number(result->request.changed)+" accepted transactions · "+balanceImpact(result->before,result->change,result->after,amountFormat)}};
            preparedReapply.reset();reapplyOperation.clear();reapplyPreview.clear();
            try {if(historyLoaded)listHistory();if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);}
            catch(...) {
                const auto saved=reapplyReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();reapplyReceipt=saved;
                const auto savedOperation=saved.value("operation").toString();
                throw std::runtime_error(savedOperation.isEmpty()?"The screen could not update. Close and reopen the file before continuing.":ruleReapplyRefreshFailure(savedOperation).toStdString());
            }
        } else if(action=="cancel_import_destination") {preparedImport.reset();importDestination=false;
        } else if(action=="cancel_import_date") {dateImport.reset();importDateQuestion.clear();
        } else if(action=="import" || action=="confirm_import_date" || action=="confirm_import_destination") {
            std::string owned;
            statements::Index document;
            if(action=="import") {
                importTarget=a.value("account").toString();importDestination=false;importFeedback.clear();
                dateImport.reset();importDateQuestion.clear();
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
                const auto suffix=info.suffix().toLower().toStdString();
                const std::string requested=suffix=="qif"||suffix=="ofx"||suffix=="qfx"||suffix=="qbo"||suffix=="aso"?suffix:"";
                owned=bytes.toStdString();
                document=statements::inspect(owned,requested,[&]{context.checkpoint();});
                if(document.format=="qif" && (document.explicit_accounts || !document.categories.empty()) && (document.diagnostic || !document.admissible)) throw std::invalid_argument("Unsupported account section structure; no import was saved. The original file is unchanged.");
                if(!document.admissible) throw std::invalid_argument(document.refusal.empty()?"Statement exceeds a resource limit. The original file is unchanged.":document.refusal);
                if(!document.refusal.empty() || (document.format!="qif" && document.diagnostic)) throw std::invalid_argument(document.refusal.empty()?"Unsupported account section structure; no import was saved. The original file is unchanged.":document.refusal);
            } else if(action=="confirm_import_destination") {
                if(!preparedImport || !importDestination)throw std::logic_error("Choose a source file first");
                owned=preparedImport->bytes;document=preparedImport->document;importTarget=a.value("account").toString();
                if(importTarget.isEmpty())throw std::invalid_argument("Choose a destination account");
                importDestination=false;
            } else {
                if(!dateImport)throw std::logic_error("Choose a source file first");
                owned=dateImport->bytes;document=dateImport->document;
            }
            auto order=qif::DateOrder::dmy;
            if(action=="confirm_import_destination")order=preparedImport->profile;
            else if(document.format=="qif") {
                const auto inference=qif::infer_date_order(owned,[&]{context.checkpoint();});
                if(inference.conflicting)throw std::invalid_argument("This QIF file contains conflicting date orders. Correct the source dates and import again; no import was saved.");
                order=inference.order;
                if(inference.ambiguous) {
                    const auto profile=a.value("profile").toString();
                    if(profile!="DMY" && profile!="MDY") {
                        dateImport=PreparedImport{identity(),owned,document.format,document,order};
                        const auto example=text(inference.example);
                        importDateQuestion={{"example",example},{"dmy",QDate::fromString(text(qif::date(inference.example,qif::DateOrder::dmy)),Qt::ISODate).toString("d MMMM yyyy")},{"mdy",QDate::fromString(text(qif::date(inference.example,qif::DateOrder::mdy)),Qt::ISODate).toString("MMMM d, yyyy")}};
                        return;
                    }
                    order=profile=="DMY"?qif::DateOrder::dmy:qif::DateOrder::mdy;
                }
            }
            dateImport.reset();importDateQuestion.clear();if(action=="confirm_import_destination")preparedImport.reset();
            std::optional<std::int64_t> destination;
            const bool unnamed=document.accounts.size()<=1 && !document.explicit_accounts;
            if(document.accounts.size()==1 || unnamed) {
                auto identified=unnamed?std::optional<std::int64_t>{}:repo.resolve_account_name(document.accounts.front().name);
                if(document.format!="qif") {
                    // Bank account identity is a confirmed source mapping, not
                    // a transfer-name alias. Require one unambiguous destination.
                    const auto& incoming=document.accounts.front();std::set<std::int64_t> matches;
                    if(identified)matches.insert(*identified);
                    const auto previous=repo.account_selected()?repo.account().identity:0;
                    try {
                        for(const auto& group:repo.import_groups())if(group.state!=app::GroupState::aborted && !group.accounts.empty()) {
                            repo.select_account(group.accounts.front().account);
                            const auto old=statements::inspect(repo.evidence(group.accounts.front().batch),"",[&]{context.checkpoint();});
                            if(old.format=="qif")continue;
                            for(const auto& member:group.accounts)if(member.source_account<old.accounts.size()) {
                                const auto& source=old.accounts[member.source_account];
                                if(!incoming.account_number.empty() && source.account_number==incoming.account_number && source.institution_id==incoming.institution_id && source.account_type==incoming.account_type)matches.insert(member.account);
                            }
                        }
                    } catch(...) {if(previous)repo.select_account(previous);else repo.release_account_selection();throw;}
                    if(previous)repo.select_account(previous);else repo.release_account_selection();
                    identified=matches.size()==1?std::optional<std::int64_t>(*matches.begin()):std::nullopt;
                }
                if(importTarget.isEmpty())destination=identified;
                else if(unnamed || (identified && *identified==importTarget.toLongLong()))destination=importTarget.toLongLong();
            }
            if(unnamed && !destination) {
                preparedImport=PreparedImport{identity(),owned,document.format,document,order};importDestination=true;return;
            }
            if(!document.accounts.empty() && !document.diagnostic) {
                QVariantList nextPlan;
                const auto home=repo.accounts().empty()?std::string{}:repo.accounts().front().currency.code();
                for(std::size_t i=0;i<document.accounts.size();++i){
                    const auto& source=document.accounts[i];const Currency currency(document.format=="qif"?home:source.currency_code);
                    const auto parsed=statements::parse_mapped(owned,document,i,order,currency,[&]{context.checkpoint();});std::string oldest;
                    for(const auto& record:parsed.records)if(!record.date.empty()){try{const auto date=qif::date(record.date,order);if(oldest.empty() || date<oldest)oldest=date;}catch(const std::invalid_argument&) {}}
                    QString reported=source.reported_balance?text(*source.reported_balance):QString("Not supplied");bool offer_reported=source.reported_balance.has_value();
                    if(source.reported_balance){try{const auto amount=qif::amount(*source.reported_balance,currency);reported=money(amount,amountFormat);offer_reported=amount.units()!=0;}catch(const FinancialError&) {}catch(const std::invalid_argument&) {}}
                    nextPlan.push_back(QVariantMap{{"source",static_cast<int>(i)},{"name",text(source.name)},{"type",text(source.type)},{"reported",reported},{"offerReportedOpening",offer_reported},{"openingDate",dateText(oldest.empty()?"1900-01-01":oldest)},{"currency",text(currency.code())}});
                }
                preparedImport=PreparedImport{identity(),owned,document.format,document,order};preparedMappings.reset();importMappingArgs.clear();importPlan=std::move(nextPlan);
                if(destination)execute("map_import",{{"mappings",QVariantList{QVariantMap{{"source",0},{"account",QString::number(*destination)},{"create",false}}}}},context);
                return;
            }
            if(!destination)throw std::logic_error("No import destination was chosen");
            if(repo.account().identity!=*destination)execute("select_account",{{"id",QString::number(*destination)}},context);
            auto batch=repo.ingest_controlled(identity(),owned,order,context);
            if(batch.processing_incomplete){context.state.store(app::OperationContext::available);context.observeState();batch=repo.resume(batch.id,batch.revision,context);}
            observedSaved(diag::Code::prepare);
            const auto result=repo.import_result(batch.id);
            importFeedback=QString("Import: %1 added; %2 already present; %3 failed").arg(result.added+result.unresolved).arg(result.already_present).arg(result.failed);
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
                    mappings.push_back({index,next,app::AccountCreation{next,preparedImport->document.accounts[index].name,Currency(str(item.value("currency"))),str(item.value("date"))},item.contains("useReportedOpening")?std::optional<bool>{item.value("useReportedOpening").toBool()}:std::nullopt});++next;
                }else mappings.push_back({index,item.value("account").toString().toLongLong(),{}});
            }
            if(!preparedMappings){preparedMappings=mappings;importMappingArgs=a.value("mappings").toList();}
            std::optional<app::ImportGroup> saved;
            try{saved=repo.prepare_import_group(preparedImport->id,preparedImport->bytes,preparedImport->profile,*preparedMappings,context,preparedImport->label);}
            catch(...){const auto failure=std::current_exception();try{saved=repo.import_group_outcome(preparedImport->id);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();preparedImport.reset();preparedMappings.reset();importPlan.clear();throw std::runtime_error("Import capture outcome is uncertain. Reopen and inspect the account imports before retrying.");}if(!saved){preparedMappings.reset();std::rethrow_exception(failure);}}
            observedSaved(diag::Code::capture);
            auto group=*saved;
            preparedImport.reset();importPlan.clear();
            if(group.state==app::GroupState::aborted)throw std::runtime_error("This import was already aborted; choose the file again to start a new import.");
            accountViews.insert_or_assign(repo.account().identity,AccountView{filter,projected,sort,descending,selectedBatch,selectedOrdinal,selection,selectionAnchor,selectionFocus});
            try{repo.select_account(group.accounts.front().account);}catch(...){diagnosticPresentationFailed=true;gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Import source and accounts were saved, but selection failed. Reopen to remove incomplete preparation and import the file again.");}selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();filter.clear();projected=true;
            if(group.state==app::GroupState::captured){auto sealed=app::OperationContext::sealed;if(!context.state.compare_exchange_strong(sealed,app::OperationContext::available))throw std::logic_error("Invalid import cancellation gate");context.observeState();group=resumeGroup(group.id,context);}

            std::size_t added=0,present=0,failed=0;const auto selectedAccount=repo.account().identity;
            try {for(const auto& member:group.accounts){repo.select_account(member.account);const auto result=repo.import_result(member.batch);added+=result.added+result.unresolved;present+=result.already_present;failed+=result.failed;}}
            catch(...){repo.select_account(selectedAccount);throw;}
            repo.select_account(selectedAccount);
            importFeedback=QString("Import: %1 added; %2 already present; %3 failed").arg(added).arg(present).arg(failed);
            if(!group.accounts.empty()){const auto batches=repo.batches();const auto b=std::find_if(batches.begin(),batches.end(),[&](const auto& b){return b.id==group.accounts.front().batch;});if(b!=batches.end() && b->count)inspect(b->id,0,context);}
        } else if(action=="resolve_import_categories") {
            std::vector<std::string> selectedNames;for(const auto& name:a.value("selected").toList())selectedNames.push_back(str(name));
            repo.resolve_import_categories(str(a.value("source")),selectedNames,context);observedSaved();
            if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);
            importFeedback="Imported categories applied.";
        } else if(action=="group_resolve_transfers") {
            try{repo.resolve_import_transfers(str(a.value("group")),context);}catch(...){const auto failure=std::current_exception();(void)durableGroup(str(a.value("group")));std::rethrow_exception(failure);}
            if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);
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
        else if(action=="rename_payee_selection" || action=="restore_imported_selection" || action=="delete_selection") {
            app::LedgerSelectionEdit request;
            request.action=action=="delete_selection"?app::LedgerSelectionAction::remove:action=="rename_payee_selection"?app::LedgerSelectionAction::rename_payee:app::LedgerSelectionAction::restore_imported;
            request.account=a.value("account").toString().toLongLong();request.generation=a.value("generation").toString().toLongLong();
            request.payee=str(a.value("payee"));
            for(const auto& value:a.value("selected").toList()){const auto row=value.toMap();request.selected.push_back({str(row.value("batch")),static_cast<std::size_t>(row.value("ordinal").toUInt())});}
            const auto operation=identity();
            try {repo.edit_transactions(operation,request,context);}
            catch(...) {const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;
                try {outcome=repo.review_outcome(operation);}catch(...){throw app::OperationOutcomeUnknown();}
                if(!outcome)std::rethrow_exception(failure);
            }
            observedSaved();
            const auto inspected=selectedBatch.empty()?request.selected.front():imports::Occurrence{selectedBatch,selectedOrdinal};
            sessionEdits.push_back({operation,inspected,request.account,request.selected,action=="delete_selection"?QString("Undo delete"):action=="rename_payee_selection"?QString("Undo payee rename"):QString("Undo restore")});
            selection=std::set<imports::Occurrence>(request.selected.begin(),request.selected.end());
            if(action=="delete_selection") {selection.clear();selectionFocus.reset();selectionAnchor.reset();selected.clear();selectedBatch.clear();selectedCommittedToken.reset();}
            else try {inspect(inspected.batch,inspected.ordinal,context);if(selectedCommittedToken){entryDetails(inspected);if(historyLoaded)listHistory();}}
            catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();sessionEdits.clear();throw std::runtime_error("Changes were saved, but the view could not update. Reopen the file and check the transactions before trying again.");}
        }
        else if(action=="undo_edit") {
            if(sessionEdits.empty())throw std::logic_error("No edits to undo in this session");
            const auto edit=sessionEdits.back();const auto operation=identity();
            try {repo.undo_transaction_edit(operation,edit.operation);}
            catch(...) {const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;
                try {outcome=repo.review_outcome(operation);}catch(...){throw app::OperationOutcomeUnknown();}
                if(!outcome)std::rethrow_exception(failure);
            }
            observedSaved();sessionEdits.pop_back();
            selection=edit.selection.empty()?std::set<imports::Occurrence>{edit.identity}:std::set<imports::Occurrence>(edit.selection.begin(),edit.selection.end());selectionFocus=edit.identity;selectionAnchor=edit.identity;
            try {repo.select_account(edit.account);inspect(edit.identity.batch,edit.identity.ordinal,context);if(selectedCommittedToken){entryDetails(edit.identity);if(historyLoaded)listHistory();}}
            catch(...) {gateway->close();active=false;selected.clear();selectedBatch.clear();sessionEdits.clear();throw std::runtime_error("Undo was saved, but its view could not be refreshed. Reopen and inspect the transaction before retrying.");}
        }
        else if(action=="accept_edit") {
            if(!selectedCommittedToken || !selected.value("canCorrect").toBool())throw std::logic_error("This transaction cannot be edited");
            const auto entry=repo.committed_entry({selectedBatch,selectedOrdinal});
            acceptEdit(editedDraft(a,entry.current.draft.transaction.original_imported_amount(),repo,&entry.current.draft,&entry.source,amountFormat),context);
        }
        else if(action=="begin_manual") {
            if(repo.account().archived)throw std::logic_error("Choose an active account");
            if(newManual)throw std::logic_error("Finish the new transaction first");
            auto date=QDate::currentDate().toString(Qt::ISODate).toStdString();
            if(selectionFocus && selection.contains(*selectionFocus))for(const auto& r:repo.current_ledger())if(r.identity==*selectionFocus){date=r.draft.date;break;}
            Money zero(0,repo.account().currency);Allocation only{"","",zero,{},"manual-d-0-0"};only.category_owner=only.memo_owner=FieldOwnership::manual;
            newManual=imports::DraftRow{date,"","",Transaction::manual(zero,{only}),FieldOwnership::manual,FieldOwnership::manual};
            newManualId="manual-"+identity();selectedBatch=newManualId;selectedOrdinal=0;selectedCommittedToken.reset();
            selected={{"batch",text(newManualId)},{"ordinal",0},{"editable",true},{"manual",true},{"newManual",true},{"hasImportedOriginal",false},{"draft",draft(*newManual,categoryById,dateFormat,amountFormat)}};
            selection={{newManualId,0}};selectionFocus=imports::Occurrence{newManualId,0};selectionAnchor=selectionFocus;invalidate();
        }
        else if(action=="cancel_manual") {
            newManual.reset();newManualId.clear();selected.clear();selectedBatch.clear();selection.clear();selectionFocus.reset();selectionAnchor.reset();invalidate();
        }
        else if(action=="save_manual") {
            if(!newManual)throw std::logic_error("No new transaction to save");
            const auto edit=editedDraft(a,{},repo,&*newManual,nullptr,amountFormat);
            const imports::Occurrence id{newManualId,0};
            try {repo.create_manual(newManualId,edit);}
            catch(...) {const auto failure=std::current_exception();bool saved=false;try{saved=repo.manual_outcome(newManualId);}catch(...){throw app::OperationOutcomeUnknown();}if(!saved)std::rethrow_exception(failure);}
            observedSaved();manualUndo=id.batch;manualUndoGeneration=repo.ledger_edit_generation();
            newManual.reset();newManualId.clear();revealManual=id;sessionEdits.clear();selection={id};selectionFocus=id;selectionAnchor=id;
            inspect(id.batch,id.ordinal,context);
        }
        else if(action=="undo_manual") {
            if(manualUndo.empty())throw std::logic_error("No new transaction to undo");
            repo.undo_manual(manualUndo,manualUndoGeneration);observedSaved();manualUndo.clear();selected.clear();selectedBatch.clear();selection.clear();selectionFocus.reset();selectionAnchor.reset();revealManual.reset();
        }
        else if(action=="save") {
            if(selectedBatch.empty()) throw std::logic_error("Select a draft first");
            auto row=repo.row({selectedBatch,selectedOrdinal});
            if(!row.draft && !row.source.repairable) throw std::invalid_argument("This record cannot be repaired");
            const auto edit=editedDraft(a,row.source.original_amount,repo,row.draft?&*row.draft:nullptr,&row.source,amountFormat);
            if(row.draft)acceptEdit(edit,context);
            else {repo.save({selectedBatch,selectedOrdinal},selectedRevision,edit);observedSaved();inspect(selectedBatch,selectedOrdinal,context);}
        } else if(action=="decide") {
            imports::Decision decision{imports::Disposition::include,{}};
            if(a.value("duplicate").toBool()) decision={imports::Disposition::duplicate,imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}};
            repo.decide({selectedBatch,selectedOrdinal},selectedRevision,decision);observedSaved();inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="opening") {
            const auto acct=repo.account();repo.opening(acct.revision,str(a.value("date")),evaluate_amount(str(a.value("amount")),acct.currency,amountFormat));observedSaved();
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="commit") {
            const auto operation="accept-"+str(QUuid::createUuid().toString(QUuid::WithoutBraces));
            std::optional<app::AcceptanceReceipt> result;
            try {result=repo.accept_all(operation,a.value("ignoreWarnings").toBool());}
            catch(...) {
                const auto failure=std::current_exception();
                try {result=repo.acceptance_outcome(operation);}
                catch(...) {gateway->close();active=false;selected.clear();receipt.clear();selectedBatch.clear();throw std::runtime_error("SpentIt could not confirm whether the transactions were accepted. Reopen the file and check them before trying again.");}
                if(!result)std::rethrow_exception(failure);
            }
            receipt={{"operation",text(result->operation)},{"summary",QString("Accepted %1 new transactions across all accounts. %2 import problems remain.").arg(result->transactions).arg(result->failed_records)}};observedSaved();
            if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);
        } else if(action=="preview_import_undo") {
            importUndoPreview.clear();
            const auto source=str(a.value("source"));const auto preview=repo.review_import_undo(source);
            QVariantList accounts;for(const auto id:preview.affected_accounts)accounts.push_back(QString::number(id));
            importUndoPreview={{"source",text(source)},{"token",QString::number(preview.token)},{"transactions",static_cast<qlonglong>(preview.transactions)},{"failures",static_cast<qlonglong>(preview.failures)},{"accounts",accounts}};
        } else if(action=="undo_import") {
            const auto source=str(a.value("source"));
            if(importUndoPreview.value("source").toString()!=text(source) || importUndoPreview.value("token")!=a.value("token"))throw std::logic_error("Review this import before undoing it.");
            const auto operation="undo-import-"+identity();std::optional<app::RollbackReceipt> result;
            try{result=repo.undo_import(operation,source,a.value("token").toString().toLongLong());}
            catch(...){const auto failure=std::current_exception();try{result=repo.import_undo_outcome(operation,source);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("SpentIt could not confirm whether the import was undone. Reopen the file and check the transactions before trying again.");}if(!result)std::rethrow_exception(failure);}
            observedSaved();importUndoPreview.clear();selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();
            receipt={{"operation",text(result->operation)},{"summary",QString("Import undone. Removed %1 new transactions and %2 import problems.").arg(result->transactions).arg(result->failures)}};
        } else if(action=="discard") {
            if(!a.contains("token"))throw std::logic_error("Review account rollback first");
            const auto operation="rollback-"+identity();std::optional<app::RollbackReceipt> result;
            try{result=repo.rollback_account(operation,a.value("token").toString().toLongLong());}
            catch(...){const auto failure=std::current_exception();try{result=repo.rollback_outcome(operation);}catch(...){gateway->close();active=false;selected.clear();selectedBatch.clear();throw std::runtime_error("Rollback outcome is uncertain. Reopen the file before retrying.");}if(!result)std::rethrow_exception(failure);}
            observedSaved();selected.clear();selectedBatch.clear();selection.clear();selectionAnchor.reset();selectionFocus.reset();
            receipt={{"operation",text(result->operation)},{"summary",QString("Discarded %1 new transactions and %2 import problems.").arg(result->transactions).arg(result->failures)}};
        } else if(action=="backup") gateway->backup_controlled(str(a.value("path")),context);
        else if(action=="query") {
            // Validate and evaluate before publishing the new query state. A bad
            // query must not enter the generic document-refresh failure path.
            app::LedgerSearch validate(str(a.value("filter")),QDate::currentDate().year(),dateFormat);
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
            if(sort<0 || sort>=6)throw std::invalid_argument("Choose a valid ledger sort column.");
            (void)snapshot(&context);
            if(sort!=oldSort || descending!=oldDescending)saveAccountSort();
            } catch(...) {
                filter=oldFilter;sort=oldSort;descending=oldDescending;projected=oldProjected;cachedView=oldView;cachedLedger=std::move(oldLedger);
                selected=oldSelected;selectedBatch=oldBatch;selectedOrdinal=oldOrdinal;
                selection=oldSelection;selectionAnchor=oldAnchor;selectionFocus=oldFocus;
                throw;
            }
        } else if(action=="rules_load" || action=="rules_discard" || action=="rules_save" || action=="rules_preview" || action=="rules_apply" || action=="rules_account_apply" || action=="rules_account_undo" || action=="rules_prefill") {
            if(!repo.rules_supported()) throw std::invalid_argument("Rules are not enabled for this dataset");
            auto closeUnknown=[&](const char* message){gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();throw std::runtime_error(message);};
            auto refreshFailed=[&](const QString& sentence){diagnosticPresentationFailed=true;const auto saved=ruleReceipt;gateway->close();active=false;selected.clear();selectedBatch.clear();clearChanges();ruleReceipt=saved;throw std::runtime_error(sentence.toStdString());};
            const auto images=[&]{
                const auto rows=a.value("rules").toList();
                if(rows.size()>static_cast<qsizetype>(rules::EvaluationLimits::rules)) throw std::invalid_argument("Too many rules");
                std::vector<app::RuleDefinitionImage> ordered;for(const auto& value:rows) ordered.push_back(imageFrom(value.toMap()));return ordered;
            };
            if(action=="rules_load" || action=="rules_discard") {loadRules();if(action=="rules_discard"){rulePreview.clear();preparedApply.reset();preparedAccountApply.reset();}}
            else if(action=="rules_prefill") {
                if(selectedBatch.empty()) throw std::logic_error("Select a record first");
                auto row=repo.row({selectedBatch,selectedOrdinal});
                const auto kind=a.value("kind").toString();
                std::string source;auto scope=rules::Scope::transaction;auto inputField=rules::InputField::payee;
                if(kind=="payee") {source=row.source.payee;if(a.contains("allocation"))scope=rules::Scope::split;}
                else if(kind=="memo") {
                    const auto index=a.value("allocation").toInt();
                    std::optional<imports::DraftRow> committedDraft;
                    const imports::DraftRow* current=row.draft?&*row.draft:nullptr;
                    if(selected.value("committed").toBool()){committedDraft=repo.committed_entry({selectedBatch,selectedOrdinal}).current.draft;current=&*committedDraft;}
                    std::optional<std::size_t> sourceOrdinal;
                    if(current && index>=0 && static_cast<std::size_t>(index)<current->transaction.allocations().size())sourceOrdinal=current->transaction.allocations()[static_cast<std::size_t>(index)].source_ordinal;
                    else if(!current && row.source.repairable && index>=0 && static_cast<std::size_t>(index)<row.source.source_allocation_fields.size())sourceOrdinal=static_cast<std::size_t>(index);
                    if(!sourceOrdinal || *sourceOrdinal>=row.source.source_allocation_fields.size())throw std::invalid_argument("Rules require an imported split");
                    const auto ordinal=*sourceOrdinal;
                    source=row.source.source_allocation_fields[ordinal].memo;scope=rules::Scope::split;inputField=rules::InputField::memo;
                } else throw std::invalid_argument("Unknown rule source");
                app::RuleDefinitionImage rule;rule.identity=identity();rule.enabled=true;rule.scope=scope;rule.input=inputField;rule.expression=app::quoted_field_pattern(source);
                std::optional<imports::DraftRow> current=row.draft;
                if(selected.value("committed").toBool())current=repo.committed_entry({selectedBatch,selectedOrdinal}).current.draft;
                rule.label=source.empty()?"New rule":source;
                if(current) {
                    const auto& allocations=current->transaction.allocations();
                    const auto index=a.contains("allocation")?a.value("allocation").toInt():(allocations.size()==1?0:-1);
                    if(a.contains("allocation") && (index<0 || static_cast<std::size_t>(index)>=allocations.size() || !allocations[static_cast<std::size_t>(index)].source_ordinal))throw std::invalid_argument("Choose an imported split for this rule.");
                    if(index>=0 && static_cast<std::size_t>(index)<allocations.size()) {
                        const auto& allocation=allocations[static_cast<std::size_t>(index)];
                        if(!repo.resolve_account_name(allocation.category) && allocation.category_binding)rule.actions.category=allocation.category_binding->identity;
                    }
                }
                rulePrefill=ruleMap(rule);
            } else if(action=="rules_preview") {
                preparedApply.reset();preparedAccountApply.reset();rulePreview.clear();ruleApplyOperation.clear();
                if(qEnvironmentVariableIsSet("SPENTIT_RULE_CANCEL_WINDOW"))
                    for(int i=0;i<200 && context.state.load()==app::OperationContext::available;++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                app::RulePreviewRequest request;
                request.batch=str(a.value("batch"));
                request.batch_revision=a.value("batchRevision").toString().isEmpty()?-1:a.value("batchRevision").toString().toLongLong();
                request.replace_manual=a.value("replaceManual").toBool();
                request.single_rule=a.value("singleRule").toBool();
                request.matches_only=true;
                request.account_scope=a.value("accountScope").toBool();
                request.new_only=a.value("newOnly").toBool();
                request.saved_rule=str(a.value("rule"));
                std::optional<std::vector<app::RuleDefinitionImage>> editor;
                if(a.value("unsaved").toBool()) editor=images();
                request.editor=editor;
                const auto result=repo.preview_pending_rules(request,context);
                if(result.unsaved_rules) {preparedApply.reset();preparedAccountApply.reset();}
                else if(request.account_scope) {ruleApplyOperation=identity();preparedAccountApply=result.account_request;}
                else {
                    if(ruleApplyOperation.empty()) ruleApplyOperation=identity();
                    preparedApply=app::RuleApplyRequest{result.batch,result.batch_revision,result.ruleset_revision,result.catalog_revision,result.replace_manual};
                }
                publishPreview(result,request.single_rule);
                rulePreview["accountScope"]=request.account_scope;rulePreview["newOnly"]=request.new_only;rulePreview["rule"]=text(request.saved_rule);
                rulePreview["label"]=request.account_scope?(request.single_rule?"This rule":"All enabled rules")+QString(request.new_only?" · new transactions only":" · all transactions in this account"):rulePreview.value("label").toString();
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
                    catch(...) {closeUnknown("Rule save outcome is uncertain. Reopen the file and inspect rule history before retrying.");}
                    if(!result) std::rethrow_exception(failure);
                }
                if(result->unchanged)diagnosticUnchanged=true;else observedSaved();
                ruleReceipt=result->unchanged?QVariantMap{{"operation",QString()},{"unchanged",true},{"summary","Rules unchanged"}}
                    :QVariantMap{{"operation",text(result->operation)},{"unchanged",false},{"summary","Rules saved"}};
                rulePreview.clear();preparedApply.reset();preparedAccountApply.reset();ruleApplyOperation.clear();
                try {
                    if(qEnvironmentVariableIsSet("SPENTIT_RULE_REFRESH_FAIL")) throw std::runtime_error("refresh");
                    loadRules();if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
                } catch(...) {const auto saved=ruleReceipt.value("operation").toString();refreshFailed(saved.isEmpty()?QString("The screen could not update. Close and reopen the file before continuing."):ruleSaveRefreshFailure(saved));}
            } else if(action=="rules_account_apply" || action=="rules_account_undo") {
                const bool undo=action=="rules_account_undo";
                if(!undo && (!preparedAccountApply || str(a.value("preview"))!=ruleApplyOperation))throw std::logic_error("Preview these rules again before applying.");
                const auto operation=undo?identity():ruleApplyOperation;
                app::ReviewReceipt result;
                const auto changed=rulePreview.value("changed").toInt();
                try {result=undo?repo.undo_account_rules(operation):repo.apply_account_rules(operation,*preparedAccountApply,context);}
                catch(...) {
                    const auto failure=std::current_exception();std::optional<app::ReviewReceipt> outcome;
                    try {outcome=repo.review_outcome(operation);}catch(...){closeUnknown("Rule application outcome is uncertain. Reopen and check the transactions before retrying.");}
                    if(!outcome){preparedAccountApply.reset();rulePreview.clear();ruleApplyOperation.clear();std::rethrow_exception(failure);}result=*outcome;
                }
                if(result.operation.empty())diagnosticUnchanged=true;else observedSaved();
                ruleReceipt={{"operation",text(result.operation)},{"summary",undo?QString("Rule application undone."):QString("%1 transaction%2 updated.").arg(changed).arg(changed==1?"":"s")}};
                preparedAccountApply.reset();rulePreview.clear();ruleApplyOperation.clear();
                try {if(qEnvironmentVariableIsSet("SPENTIT_RULE_REFRESH_FAIL"))throw std::runtime_error("refresh");if(historyLoaded)listHistory();if(!selectedBatch.empty())inspect(selectedBatch,selectedOrdinal,context);}
                catch(...) {refreshFailed(ruleApplyRefreshFailure(text(result.operation)));}
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
                    catch(...) {closeUnknown("Rule application outcome is uncertain. Reopen the file and check the new transactions before retrying.");}
                    if(!result) {ruleApplyOperation.clear();preparedApply.reset();preparedAccountApply.reset();rulePreview.clear();std::rethrow_exception(failure);}
                }
                if(result->operation.empty())diagnosticUnchanged=true;else observedSaved();
                ruleReceipt=result->operation.empty()?QVariantMap{{"operation",QString()},{"summary","No pending drafts were changed"}}
                    :QVariantMap{{"operation",text(result->operation)},{"summary","Rule application "+text(result->operation)+" changed "+QString::number(result->changed)+" draft(s)"}};
                rulePreview.clear();preparedApply.reset();preparedAccountApply.reset();ruleApplyOperation.clear();
                try {
                    if(qEnvironmentVariableIsSet("SPENTIT_RULE_REFRESH_FAIL")) throw std::runtime_error("refresh");
                    if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal,context);
                } catch(...) {const auto saved=ruleReceipt.value("operation").toString();refreshFailed(saved.isEmpty()?QString("The screen could not update. Close and reopen the file before continuing."):ruleApplyRefreshFailure(saved));}
            }
        }
        else throw std::invalid_argument("Unknown application command");
    }
};
ImportModel::ImportModel(std::unique_ptr<app::DocumentGateway> gateway,QObject* parent):QAbstractListModel(parent),worker_(std::make_shared<Worker>(std::move(gateway))) {
    // SQLCipher verification under ASan exceeds macOS's small default worker stack.
    // Reserve a bounded stack for the single document worker in every build.
    rememberWindowPosition_=QSettings().value("Window/rememberPosition",false).toBool();
    pool_.setStackSize(4 * 1024 * 1024);
    pool_.setMaxThreadCount(1); state_={{"active",false},{"path",QSettings().value("datasetPath",suggestedPath())},{"startup",QSettings().contains("datasetPath")}};
}
ImportModel::~ImportModel() {pool_.waitForDone();worker_->gateway->close();}
void ImportModel::setRememberWindowPosition(bool enabled) {
    if(rememberWindowPosition_==enabled)return;
    rememberWindowPosition_=enabled;
    QSettings settings;settings.setValue("Window/rememberPosition",enabled);settings.sync();
    if(settings.status()!=QSettings::NoError){error_="The window-geometry preference could not be saved.";emit changed();}
    emit windowPreferencesChanged();
}
void ImportModel::restoreWindowPosition(QObject* object) {
    auto* window=qobject_cast<QWindow*>(object);
    if(!window)return;
    // Track normal placement in memory, including while the preference is off.
    // Deferred sampling avoids recording maximized/full-screen transition geometry.
    if(!window->property("spentitGeometryObserved").toBool()) {
        window->setProperty("spentitGeometryObserved",true);
        const auto sample=[window]{
            if(!window->isVisible() || window->windowState()==Qt::WindowMinimized)return;
            window->setProperty("spentitLastWindowState",int(window->windowState()));
            if(window->windowState()==Qt::WindowNoState){
                window->setProperty("spentitNormalPosition",window->framePosition());
                window->setProperty("spentitNormalSize",window->size());
            }
        };
        const auto schedule=[window,sample]{QTimer::singleShot(0,window,sample);};
        connect(window,&QWindow::xChanged,window,schedule);connect(window,&QWindow::yChanged,window,schedule);
        connect(window,&QWindow::widthChanged,window,schedule);connect(window,&QWindow::heightChanged,window,schedule);
        connect(window,&QWindow::windowStateChanged,window,schedule);
        connect(window,&QWindow::visibleChanged,window,schedule);
    }
    QSettings settings;const auto saved=settings.value("Window/position");
    int state=int(Qt::WindowNoState);
    if(rememberWindowPosition_ && saved.canConvert<QPoint>()) {
        const QPoint position=saved.toPoint();
        QSize size=settings.value("Window/size",window->size()).toSize();if(!size.isValid() || size.isEmpty())size=window->size();
        QScreen* target=QGuiApplication::primaryScreen();qreal largest=0;
        const QRectF bounds{QPointF(position),QSizeF(size)};
        for(auto* screen:QGuiApplication::screens()){
            const auto overlap=bounds.intersected(QRectF(screen->availableGeometry()));
            const qreal area=overlap.width()*overlap.height();
            if(area>largest){largest=area;target=screen;}
        }
        if(target){
            window->setScreen(target);window->create();
            const auto available=target->availableGeometry();const auto margins=window->frameMargins();
            size.setWidth(std::clamp(size.width(),std::max(1,window->minimumWidth()),std::max(window->minimumWidth(),available.width()-margins.left()-margins.right())));
            size.setHeight(std::clamp(size.height(),std::max(1,window->minimumHeight()),std::max(window->minimumHeight(),available.height()-margins.top()-margins.bottom())));
            window->resize(size);
            window->setFramePosition({std::clamp(position.x(),available.left(),available.left()+std::max(0,available.width()-size.width()-margins.left()-margins.right())),
                                      std::clamp(position.y(),available.top(),available.top()+std::max(0,available.height()-size.height()-margins.top()-margins.bottom()))});
        }
        state=settings.value("Window/state",int(Qt::WindowNoState)).toInt();
    }
    window->setProperty("spentitNormalPosition",window->framePosition());window->setProperty("spentitNormalSize",window->size());
    if(state==int(Qt::WindowMaximized))window->showMaximized();
    else if(state==int(Qt::WindowFullScreen))window->showFullScreen();
    else window->showNormal(); // A minimized application must reopen visibly.
}
bool ImportModel::saveWindowPosition(QObject* object) {
    auto* window=qobject_cast<QWindow*>(object);
    if(!rememberWindowPosition_ || !window)return true;
    const bool normal=window->windowState()==Qt::WindowNoState;
    const auto position=normal?QVariant(window->framePosition()):window->property("spentitNormalPosition");
    const auto size=normal?QVariant(window->size()):window->property("spentitNormalSize");
    const int state=window->windowState()==Qt::WindowMinimized?window->property("spentitLastWindowState").toInt():int(window->windowState());
    QSettings settings;bool preferencesChanged=false;
    const auto store=[&](const char* key,const QVariant& value){if(value.isValid() && settings.value(key)!=value){settings.setValue(key,value);preferencesChanged=true;}};
    store("Window/position",position);store("Window/size",size);store("Window/state",state);
    if(!preferencesChanged)return true;
    settings.sync();if(settings.status()==QSettings::NoError)return true;
    error_="The window geometry could not be saved. Disable Remember window geometry to quit without saving it.";emit changed();return false;
}
QStringList ImportModel::currencies() const {QStringList out;for(const auto& c:supported_currencies())out<<text(c.code);return out;}
QString ImportModel::suggestedPath() const {return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/spentit.spentit";}
QUrl ImportModel::documentsFolder() const {return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));}
QUrl ImportModel::suggestedFile() const {return QUrl::fromLocalFile(suggestedPath());}
QString ImportModel::datasetFilename(const QString& path) const {return path.endsWith(".spentit",Qt::CaseInsensitive)?path:path+".spentit";}
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
    const AmountFormat amountFormat{state_.value("amountFormatting",true).toBool(),str(state_.value("amountStyle","comma-dot"))};
    if(!index.isValid() || index.row()<0 || index.row()>=rowCount() || role!=Qt::UserRole) return {};
    const auto i=rows_->order[static_cast<std::size_t>(index.row())];
    const auto& r=rows_->rows[i];const auto& t=r.draft.transaction;const auto amount=t.effective_total();
    // QML requests only visible delegates: no per-account QVariant tree.
    const auto labels=state_.value("categoryById").toMap();
    auto row=draft(r.draft,labels,state_.value("dateFormat","YYYY-MM-DD").toString().toStdString(),amountFormat);
    if(state_.value("categoriesSupported").toBool()) {
        auto splits=row.value("splits").toList();
        for(qsizetype n=0;n<splits.size();++n) {auto a=splits[n].toMap();a["categoryDisplay"]=text(t.allocations()[static_cast<std::size_t>(n)].category);splits[n]=a;}
        row["splits"]=splits;
    }
    row["batch"]=text(r.identity.batch);row["ordinal"]=static_cast<int>(r.identity.ordinal);
    row["manual"]=state_.value("manualBatches").toMap().contains(text(r.identity.batch));
    row["status"]=r.sequence==INT64_MAX?"New transaction":r.committed?"Committed":"Draft";
    row["category"]=t.allocations().size()==1?text(t.allocations()[0].category):"Split";
    row["withdrawal"]=amount.units()<0?money(amount,amountFormat).mid(1):"";
    row["deposit"]=amount.units()>=0?money(amount,amountFormat):"";
    row["balance"]=money(rows_->balances[i],amountFormat);
    return row;
}
QHash<int,QByteArray> ImportModel::roleNames() const {return {{Qt::UserRole,"record"}};}
bool ImportModel::reorderAccounts(const QStringList& ids) {
    if(busy_ || !state_.value("active").toBool())return false;
    const auto accounts=state_.value("accounts").toList();
    QSet<QString> expected;for(const auto& value:accounts)expected.insert(value.toMap().value("id").toString());
    const QSet<QString> requested(ids.begin(),ids.end());
    if(ids.size()!=accounts.size() || requested!=expected){error_="The account list changed. Try dragging the account again.";emit changed();return false;}
    QSettings settings;const auto key=worker_->sortDocument+"/SidebarOrder";
    const auto prior=settings.value(key);
    settings.setValue(key,ids);settings.sync();
    if(settings.status()!=QSettings::NoError){
        if(prior.isValid())settings.setValue(key,prior);else settings.remove(key);
        error_="The account order could not be saved. Check that Settings is writable and try again.";emit changed();return false;
    }
    state_["sidebarAccounts"]=sidebarAccounts(accounts,key);error_.clear();emit changed();return true;
}

void ImportModel::requestCancel() {
    if(busy_ && operation_ && operation_->request()) {diag::Collector::instance().cancel(diagnosticOperation_,diag::Code::requested);phase_="Cancellation requested";emit changed();}
}
void ImportModel::command(const QString& action,const QVariantMap& args) {
    const AmountFormat amountFormat{state_.value("amountFormatting",true).toBool(),str(state_.value("amountStyle","comma-dot"))};
    auto& observer=diag::Collector::instance();
    auto safeAction=diag::Action::unclassified;
    for(unsigned i=1;i<static_cast<unsigned>(diag::Action::count);++i){
        const auto candidate=static_cast<diag::Action>(i);const auto name=diag::name(candidate);
        if(action==QLatin1StringView(name.data(),static_cast<qsizetype>(name.size()))){safeAction=candidate;break;}
    }

    if(busy_) {
        if(action=="inspect" && activeAction_=="select") {
            if(deferredDiagnostic_)observer.admit(safeAction,diag::EventKind::superseded,deferredDiagnostic_);
            deferredDiagnostic_=observer.admit(safeAction,diag::EventKind::deferred);observer.deferred(deferredDiagnostic_);pendingInspection_=args;
        } else observer.admit(safeAction,diag::EventKind::rejected_busy);
        return;
    }
    if(safeAction==diag::Action::unclassified){observer.admit(safeAction,diag::EventKind::rejected_invalid);diagnosticOperation_=0;}
    else diagnosticOperation_=observer.admit(safeAction,diag::EventKind::admitted,resumedDiagnostic_);
    resumedDiagnostic_=0;
    const auto diagnosticId=diagnosticOperation_;
    // A plain selection uses only the published immutable ledger and cached
    // relationship flags. With no command in flight, the worker's selection
    // identities can be updated here without SQL, a model reset, or global busy
    // presentation. Range/toggle selection retains the bounded worker path.
    if(action=="select" && !args.value("range").toBool() && !args.value("toggle").toBool()) {
        diag::Scope scope(diagnosticId);observer.start(diagnosticId);
        try {
            const int row=args.value("row",-1).toInt();
            if(!rows_ || rows_!=worker_->cachedView || row<0 || static_cast<std::size_t>(row)>=rows_->order.size())throw std::invalid_argument("Selection row is unavailable");
            const auto& record=rows_->rows[rows_->order[static_cast<std::size_t>(row)]];const auto& id=record.identity;
            if(worker_->selection.size()==1 && worker_->selection.contains(id) && worker_->selectionFocus==id && worker_->selectionAnchor==id && !args.value("firstSearchSelection").toBool()) {
                observer.finish(diagnosticId,diag::Result::succeeded,diag::Persistence::not_applicable,diag::Presentation::model_applied);observer.applied(diagnosticId);return;
            }
            auto value=draft(record.draft,worker_->categoryById,worker_->dateFormat,amountFormat);
            value["batch"]=text(id.batch);value["ordinal"]=static_cast<int>(id.ordinal);value["committed"]=record.committed;value["transferLinked"]=worker_->cachedLinkedRows.contains(id);
            auto next=state_;next["selection"]=QVariantList{value};next["selectionKeys"]=QVariantMap{{text(id.batch)+":"+QString::number(id.ordinal),true}};next["selectionCount"]=1;next["selectionIndex"]=row;
            worker_->selection={id};worker_->selectionFocus=id;worker_->selectionAnchor=id;state_=std::move(next);error_.clear();
            observer.finish(diagnosticId,diag::Result::succeeded,diag::Persistence::not_applicable,diag::Presentation::model_applied);observer.applied(diagnosticId);
            emit changed();emit completed(action,true);
        } catch(const std::exception& error) {
            error_=QString::fromUtf8(error.what());observer.finish(diagnosticId,diag::Result::failed,diag::Persistence::not_applicable,diag::Presentation::failed);emit changed();emit completed(action,false);
        }
        return;
    }
    activeAction_=action;
    operation_=std::make_shared<app::OperationContext>();
    const auto operation=operation_;
    if(observer.enabled())try{operation->observeCancellation=[diagnosticId](app::OperationContext::State state){diag::Collector::instance().cancel(diagnosticId,state==app::OperationContext::available?diag::Code::available:state==app::OperationContext::requested?diag::Code::requested:diag::Code::sealed);};}catch(...){observer.healthFailure();}
    if(action!="probe" && action!="backup" && action!="open" && action!="unlock" && action!="import" && action!="confirm_import_date" && action!="confirm_import_destination" && action!="map_import" && action!="resolve_import_categories" && action!="resume" && action!="query" && action!="rules_preview" && action!="rules_apply" && action!="rules_account_apply" && action!="rules_save" && action!="rules_reapply_preview" && action!="rules_reapply_apply" && action!="transfer_candidates" && action!="review_batch" && action!="assign_transfer_batch" && action!="release_transfer_batch" && action!="rename_payee_selection" && action!="restore_imported_selection") operation->seal();
    observer.cancel(diagnosticId,operation->state.load()==app::OperationContext::sealed?diag::Code::sealed:diag::Code::available);
    operation->progress=[this,weak=std::weak_ptr(operation)](std::string_view value) {
        const auto operation=weak.lock();
        if(!operation) return;
        const auto label=text(value);
        QMetaObject::invokeMethod(this,[this,operation,label]{
            if(busy_ && operation_==operation && !cancellationRequested() && phase_!=label) {phase_=label;emit changed();}
        },Qt::QueuedConnection);
    };
    phase_=phaseFor(action);
    busy_=true;error_.clear();emit changed();
    struct Result {QVariantMap state;std::shared_ptr<const app::LedgerView> rows;QString error;bool success=false;};
    auto* watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,action,diagnosticId]{
        diag::Scope scope(diagnosticId);diag::Span applySpan(diag::Code::model_apply);
        auto result=watcher->result();watcher->deleteLater();
        if(rows_!=result.rows) {
            beginResetModel();state_=std::move(result.state);auto retired=std::move(rows_);rows_=std::move(result.rows);endResetModel();
            // Destruction of large retired snapshots must not stall the UI thread.
            diag::Collector::instance().job(true);
            pool_.start([retired=std::move(retired),diagnosticId]() mutable {diag::Scope scope(diagnosticId);diag::Span span(diag::Code::cleanup);retired.reset();diag::Collector::instance().job(false);});
        } else state_=std::move(result.state);
        diag::Collector::instance().applied(diagnosticId,action=="close" || action=="open" || action=="unlock" || action=="create");
        error_=std::move(result.error);busy_=false;phase_.clear();operation_->progress={};operation_.reset();emit changed();emit completed(action,result.success);
        if(pendingInspection_ && !busy_){auto args=std::move(*pendingInspection_);pendingInspection_.reset();resumedDiagnostic_=deferredDiagnostic_;deferredDiagnostic_=0;diag::Collector::instance().deferred(0);if(state_.value("active").toBool())command("inspect",args);else{diag::Collector::instance().admit(diag::Action::inspect,diag::EventKind::superseded,resumedDiagnostic_);resumedDiagnostic_=0;}}
    });
    watcher->setFuture(QtConcurrent::run(&pool_,[worker=worker_,action,args,operation,diagnosticId,safeAction]{
        diag::Scope scope(diagnosticId);diag::Span workerSpan(diag::Code::worker);auto& observer=diag::Collector::instance();observer.start(diagnosticId);
        auto outcome=diag::Result::failed;auto persistence=diag::Persistence::unknown;auto presentation=diag::Presentation::awaiting_refresh;
        Result result; bool unlock=false,uncertain=false;
        try {worker->execute(action,args,*operation);result.success=true;outcome=diag::Result::succeeded;
            // Successful return from these existing complete commands confirms persistence.
            switch(safeAction) {
            case diag::Action::create_account:
            case diag::Action::rename_account:
            case diag::Action::archive_account:
            case diag::Action::reactivate_account:
            case diag::Action::delete_account:
            case diag::Action::correct_opening:
            case diag::Action::reassign_currency:
            case diag::Action::undo_account:
            case diag::Action::map_import:
            case diag::Action::group_balance:
            case diag::Action::group_abort:
            case diag::Action::group_resolve_transfers:
            case diag::Action::resume:
            case diag::Action::save:
            case diag::Action::commit:
            case diag::Action::discard:
            case diag::Action::opening:
            case diag::Action::apply_change:
            case diag::Action::apply_bulk:
            case diag::Action::apply_category:
            case diag::Action::backup:
                persistence=diag::Persistence::confirmed_saved;break;
            case diag::Action::probe:
            case diag::Action::open:
            case diag::Action::unlock:
            case diag::Action::close:
            case diag::Action::select:
            case diag::Action::select_account:
            case diag::Action::inspect:
            case diag::Action::query:
            case diag::Action::evidence:
            case diag::Action::history:
            case diag::Action::entry_history:
            case diag::Action::rules_load:
            case diag::Action::rules_preview:
            case diag::Action::rules_reapply_preview:
            case diag::Action::preview_change:
            case diag::Action::preview_category:
            case diag::Action::transfer_candidates:
            case diag::Action::prepare_create:
            case diag::Action::cancel_change:
            case diag::Action::cancel_bulk:
            case diag::Action::cancel_merge:
            case diag::Action::cancel_reapply:
            case diag::Action::category_select:
            case diag::Action::rules_discard:
            case diag::Action::rules_prefill:
            case diag::Action::cancel_category_preview:
            case diag::Action::category_list:
            case diag::Action::preview_bulk:
            case diag::Action::cancel_import_mapping:
                persistence=diag::Persistence::not_applicable;break;
            default:break;
            }
        }
        catch(const app::OperationOutcomeUnknown& e){uncertain=true;worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();worker->selectedBatch.clear();worker->invalidate();result.error=QString::fromUtf8(e.what());}
        catch(const app::OperationCancelled& e){outcome=diag::Result::cancelled;if(action!="query")worker->invalidate();result.error=action=="backup"?"Backup cancelled. An unfinished .partial file may remain. Use a new filename when you try again.":QString::fromUtf8(e.what());}
        catch(const app::UnlockRequired& e){worker->invalidate();result.error=QString::fromUtf8(e.what());unlock=true;}
        catch(const std::exception& e){if(action!="query")worker->invalidate();result.error=QString::fromUtf8(e.what());}
        if(!result.success && worker->active && (action=="import" || action=="confirm_import_date" || action=="confirm_import_destination" || action=="map_import")) {
            try {if(worker->gateway->repository().cleanup_incomplete_imports())result.error+=" Incomplete import preparation was removed; you can import the file again.";}
            catch(...) {worker->gateway->close();worker->active=false;worker->selected.clear();worker->selectedBatch.clear();result.error="Interrupted import cleanup failed. Reopen to retry cleanup before making further changes.";}
            worker->invalidate();
        }
        if(worker->diagnosticUnchanged)persistence=diag::Persistence::confirmed_unchanged;
        if(worker->diagnosticSaved || diag::context().saved)persistence=diag::Persistence::confirmed_saved;
        if(worker->diagnosticPresentationFailed && outcome!=diag::Result::cancelled)presentation=diag::Presentation::failed;
        if(uncertain)persistence=diag::Persistence::unknown;
        operation->state.store(app::OperationContext::sealed);observer.cancel(diagnosticId,diag::Code::sealed);
        operation->checkpoint("Refreshing verified ledger");
        try {auto snapshot=worker->snapshot();result.state=std::move(snapshot.first);result.rows=std::move(snapshot.second);}
        catch(const std::exception&){
            presentation=diag::Presentation::failed;
            worker->invalidate();
            const auto ruleOperation=worker->ruleReceipt.value("operation").toString();
            const auto reapplyOperationText=worker->reapplyReceipt.value("operation").toString();
            result.error=((action=="save_manual" || action=="save" || action=="accept_edit" || action=="undo_edit" || action=="rename_payee_selection" || action=="restore_imported_selection") && worker->diagnosticSaved)?"Edit was saved, but its view could not be refreshed. Reopen and inspect the transaction before retrying."
                :(action=="delete_selection" && worker->diagnosticSaved)?"The transactions were deleted, but the screen could not update. Reopen the file before continuing."
                 :(action=="undo_import" && worker->diagnosticSaved)?"The import was undone, but the screen could not update. Reopen the file and check the transactions before continuing."
                :(action=="create_account" && result.success)?"Account was created, but its view could not be refreshed. Reopen and inspect the account list before retrying."
                :((action=="map_import" || action=="resolve_import_categories" || action=="group_balance" || action=="group_abort" || action=="resume") && result.success)?"Import operation was saved, but its view could not be refreshed. Reopen and inspect the retained import before retrying."
                :((action=="review_answer" || action=="review_batch" || action=="review_undo") && result.success)?"Review decision was saved, but its view could not be refreshed. Reopen and inspect Review before retrying."
                :((action.startsWith("transfer_") || action=="assign_transfer_batch" || action=="release_transfer_batch" || action=="undo_transfer_assignment") && result.success)?"Transfer operation was saved, but its view could not be refreshed. Reopen and inspect transfers before retrying."
                :(action=="apply_category" && !worker->categoryReceipt.isEmpty())?"Category change was saved, but the refreshed view could not be loaded. Reopen and inspect category history; do not apply it again."
                :(action=="apply_change" && !worker->changeReceipt.isEmpty())?"Your change was saved, but the screen could not update. Reopen the file and check the transaction before making more changes."
                :((action=="rules_apply" || action=="rules_account_apply" || action=="rules_account_undo") && !ruleOperation.isEmpty())?ruleApplyRefreshFailure(ruleOperation)
                :(action=="rules_save" && !ruleOperation.isEmpty())?ruleSaveRefreshFailure(ruleOperation)
                :(action=="rules_reapply_apply" && !reapplyOperationText.isEmpty())?ruleReapplyRefreshFailure(reapplyOperationText)
                :(action=="apply_bulk" && !worker->bulkReceipt.isEmpty())?"Your changes were saved, but the screen could not update. Reopen the file and check the transactions before making more changes."
                :((action=="rename_account" || action=="archive_account" || action=="reactivate_account" || action=="delete_account" || action=="correct_opening" || action=="reassign_currency" || action=="undo_account") && result.success)?"Account command was saved, but its view could not be refreshed. Reopen and inspect the account before retrying."
                :"The screen could not update. Close and reopen the file before continuing.";
            worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();
            result.state={{"active",false},{"path",worker->path},{"bulkReceipt",worker->bulkReceipt},{"changeReceipt",worker->changeReceipt},{"categoryReceipt",worker->categoryReceipt},{"ruleReceipt",worker->ruleReceipt},{"reapplyReceipt",worker->reapplyReceipt}};
            result.success=false;
        }
        observer.finish(diagnosticId,outcome,persistence,presentation);
        result.state["unlockRequired"]=unlock || worker->welcomePassword;return result;
    }));
}

bool ImportModel::accountNameMatches(const QString& name,const QString& selected) { return app::account_name_matches(str(name),str(selected)); }
bool ImportModel::categoryNameMatches(const QString& name,const QString& selected,bool includeChildren) {
    try {return spentit::app::category_name_matches(name.toStdString(),selected.toStdString(),includeChildren);}
    catch(const std::invalid_argument&) {return false;} // Incomplete text is not yet a selectable category name.
    catch(const std::exception& error) {error_=QString::fromUtf8(error.what());emit changed();return false;}
}

QString ImportModel::suggestedDateFormat(const QString& currency) const {return text(suggested_date_format(currency.toStdString()));}
QString ImportModel::todayDisplay() const {return text(display_date(QDate::currentDate().toString(Qt::ISODate).toStdString(),state_.value("dateFormat","YYYY-MM-DD").toString().toStdString()));}
QString ImportModel::dateFormatExample(const QString& format) const {try{return text(display_date("2026-10-28",format.toStdString()));}catch(const std::exception&){return "Invalid format: use YYYY, MM and DD once each";}}

void ImportModel::observeUi(const QVariantMap& values) {
    auto& observer=diag::Collector::instance();if(!observer.enabled())return;
    try {
    diag::Ui ui;ui.generation=values.value("generation").toULongLong();
    ui.surface=diag::surface(values.value("surface").toString().toStdString());
    ui.modal_mask=values.value("modals").toULongLong() & ((1ULL<<40)-1);
    ui.pending_mask=values.value("pending").toULongLong() & 4095;
    ui.control_visible=values.value("visibleControls").toULongLong() & 65535;
    ui.control_enabled=values.value("enabledControls").toULongLong() & 65535;
    ui.selection=state_.value("selectionCount").toUInt();ui.filtered=static_cast<std::uint32_t>(rowCount());
    ui.loaded=rows_?static_cast<std::uint32_t>(rows_->rows.size()):0;
    ui.delegates=values.value("delegates").toUInt();ui.first=values.value("first",-1).toInt();ui.last=values.value("last",-1).toInt();
    ui.editor_kind=std::clamp(values.value("editorKind").toInt(),0,3);
    for(const auto& value:values.value("modalStack").toList()){
        const int code=value.toInt();if(code<0 || code>29 || ui.modal_count==ui.modal_stack.size())continue;
        ui.modal_stack[ui.modal_count++]=static_cast<unsigned>(code);
        ui.modal_mask|=1ULL<<static_cast<unsigned>(code);
    }
    ui.focus_role=std::clamp(values.value("focusRole").toInt(),0,11);
    ui.sort=state_.value("sort").toInt();ui.descending=state_.value("descending").toBool();
    ui.projected=state_.value("mode")=="Projected";ui.search=values.value("search").toBool();ui.search_valid=values.value("searchValid").toBool();
    ui.sidebar=values.value("sidebar").toBool();ui.dirty=values.value("dirty").toBool();ui.selected_visible=values.value("selectedVisible").toBool();
    const auto group=state_.value("importGroup").toMap();ui.blockers=static_cast<std::uint32_t>(group.value("blockers").toList().size());ui.blockers_total=group.value("unresolved").toUInt();
    ui.native_exposed=false;
    if(QGuiApplication::platformName()!="offscreen" && QGuiApplication::platformName()!="minimal")for(auto* window:QGuiApplication::topLevelWindows())if(window->isExposed() && window->visibility()!=QWindow::Minimized)ui.native_exposed=true;
    ui.interaction_ready=values.value("ready").toBool() && !busy_;
    // Native chooser pixels are outside this QQuickWindow frame proxy.
    if(ui.modal_mask & ((1ULL<<8)|(1ULL<<9)|(1ULL<<11)|(1ULL<<12)))ui.native_exposed=false;
    observer.publish(ui);
    const auto uiRevision=observer.snapshot(UINT64_MAX).ui.revision;
    if(!ui.native_exposed)diagnosticFrameRequested_=UINT64_MAX;
    if(ui.interaction_ready && ui.native_exposed && diagnosticFrameRequested_!=uiRevision){
        diagnosticFrameRequested_=uiRevision;
        for(auto* window:QGuiApplication::topLevelWindows())if(window->isExposed())window->requestUpdate();
    }
    }catch(...){observer.healthFailure();}
}

QVariantMap ImportModel::amountValue(const QString& expression,const QString& definition) const {
    const AmountFormat amountFormat{state_.value("amountFormatting",true).toBool(),str(state_.value("amountStyle","comma-dot"))};
    try {
        return {{"valid",true},{"amount",money(evaluate_amount(str(expression),Currency::from_definition(str(definition)),amountFormat),amountFormat)}};
    } catch(const std::exception& e) { return {{"valid",false},{"error",QString::fromUtf8(e.what())}}; }
}

QVariantMap ImportModel::splitAmounts(const QString& totalText,const QVariantList& amounts,bool automatic,const QString& definition,int remainderIndex) const {
    const AmountFormat amountFormat{state_.value("amountFormatting",true).toBool(),str(state_.value("amountStyle","comma-dot"))};
    try {
        if(amounts.empty() || amounts.size()>static_cast<qsizetype>(qif::max_allocations))throw std::invalid_argument("Enter between 1 and 1000 splits.");
        const auto currency=Currency::from_definition(str(definition));
        const auto total=evaluate_amount(str(totalText),currency,amountFormat);
        std::vector<Money> values;
        if(automatic && (remainderIndex<0 || remainderIndex>=amounts.size()))throw std::invalid_argument("Choose a valid balancing split.");
        for(qsizetype i=0;i<amounts.size();++i)if(!automatic || i!=remainderIndex)values.push_back(evaluate_amount(str(amounts[i]),currency,amountFormat));
        if(automatic)values.insert(values.begin()+remainderIndex,split_remainder(total,values));
        const auto sum=imports::sum(currency,values);
        QVariantList formatted;for(const auto& value:values)formatted.push_back(money(value,amountFormat));
        return {{"valid",true},{"balanced",sum==total},{"sum",money(sum,amountFormat)},{"total",money(total,amountFormat)},{"amounts",formatted}};
    }catch(const std::exception& e){return {{"valid",false},{"error",QString::fromUtf8(e.what())}};}
}

int ImportModel::chooseSplitRemainder(const QString& totalText,const QVariantList& splits,const QString& definition) const {
    const AmountFormat amountFormat{state_.value("amountFormatting",true).toBool(),str(state_.value("amountStyle","comma-dot"))};
    try {
        const auto currency=Currency::from_definition(str(definition));
        const auto total=evaluate_amount(str(totalText),currency,amountFormat).units();
        int best=-1;bool bestSame=false;std::uint64_t largest=0;
        for(qsizetype i=0;i<splits.size();++i){
            const auto part=splits[i].toMap();if(part.value("transferAmount").toBool())continue;
            const auto units=evaluate_amount(str(part.value("amount")),currency,amountFormat).units();
            const bool same=(total>0 && units>0) || (total<0 && units<0);
            const auto magnitude=units<0?std::uint64_t(-(units+1))+1:std::uint64_t(units);
            if(best<0 || (same && !bestSame) || (same==bestSame && magnitude>largest)){
                best=static_cast<int>(i);bestSame=same;largest=magnitude;
            }
        }
        return best;
    }catch(const std::exception&){return -1;}
}
