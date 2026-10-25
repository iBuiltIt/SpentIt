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
QVariantList allocations(const Transaction& t) {
    QVariantList out;
    for(const auto& a:t.allocations()) out.push_back(QVariantMap{{"category",text(a.category)},{"memo",text(a.memo)},{"amount",money(a.amount)}});
    return out;
}
QVariantMap draft(const imports::DraftRow& d) {
    return {{"date",text(d.date)},{"payee",text(d.payee)},{"memo",text(d.memo)},
        {"total",money(d.transaction.effective_total())},{"splits",allocations(d.transaction)}};
}
}
struct ImportModel::Worker {
    std::unique_ptr<app::DocumentGateway> gateway;
    bool active=false,projected=true;
    QString path,filter;
    int sort=0; bool descending=false;
    QVariantMap selected,receipt;
    // Session-local verified reads. Every mutation (including a failed command)
    // invalidates these before execution. The exclusive gateway owns all writes.
    QVariantMap metadata;
    std::optional<app::Account> cachedAccount;
    std::optional<std::vector<app::LedgerRow>> cachedLedger;
    std::shared_ptr<const app::LedgerView> cachedView;
    void invalidate() {metadata.clear();cachedAccount.reset();cachedLedger.reset();cachedView.reset();}
    std::string selectedBatch;
    std::size_t selectedOrdinal=0;
    std::int64_t selectedRevision=0;
    // Commit identity is deterministic for a batch revision, so restart retries
    // resolve the same durable operation without storing local financial metadata.
    explicit Worker(std::unique_ptr<app::DocumentGateway> g):gateway(std::move(g)){}
    void inspect(std::string batch,std::size_t ordinal) {
        auto& repo=gateway->repository(); const auto acct=repo.account();
        const auto all=repo.batches();
        const auto found=std::find_if(all.begin(),all.end(),[&](const auto& b){return b.id==batch;});
        if(found==all.end() || ordinal>=found->count) throw std::invalid_argument("Choose a record in this import");
        auto bytes=repo.evidence(batch); const auto parsed=qif::parse(bytes,found->profile,acct.currency);
        const auto& source=parsed.records.at(ordinal);
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
                for(const auto& a:source.source_allocations) splits.push_back(QVariantMap{{"category",text(a.category)},{"memo",text(a.memo)},{"amount",money(a.amount)}});
                selected["draft"]=QVariantMap{{"date",text(source.date)},{"payee",text(source.payee)},{"memo",text(source.memo)},
                    {"total",money(*source.original_amount)},{"splits",splits}};
                selected["original"]=money(*source.original_amount);selected["editable"]=true;selected["repair"]=true;
                selected["diagnostic"]=selected["diagnostic"].toString()+" — correct the date/splits and save the repair; original evidence is retained.";
            }
            return;
        }
        selected["draft"]=draft(*row.draft);
        if(source.diagnostic) selected["diagnostic"]=selected["diagnostic"].toString()+" — saved interpretation repair; original diagnostic retained.";
        selected["original"]=money(*row.draft->transaction.original_imported_amount());
        selected["editable"]=found->state==app::BatchState::pending;
        selected["decision"]=static_cast<int>(row.decision.disposition);
        QVariantList candidates;
        // Committed/discarded rows are read-only and have no duplicate actions.
        if(found->state==app::BatchState::pending) for(const auto& id:row.candidates) {
            auto prior=repo.row(id);
            const auto basis=imports::candidate_basis(row.source,prior.source);
            candidates.push_back(QVariantMap{{"batch",text(id.batch)},{"ordinal",static_cast<int>(id.ordinal)},
                {"label",text(id.batch)+" / "+QString::number(id.ordinal+1)+" · "+text(prior.source.date)+" · "+text(prior.source.payee)+" · "+money(*prior.source.original_amount)+" · "+(basis==imports::MatchBasis::same_source_fields?"same source fields":basis==imports::MatchBasis::same_amount_unknown_date?"same original amount; source date unavailable":"same date and amount")}});
        }
        selected["candidates"]=candidates;
        if(row.decision.duplicate_of) selected["duplicate"]=text(row.decision.duplicate_of->batch)+" / "+QString::number(row.decision.duplicate_of->ordinal+1);
    }
    std::pair<QVariantMap,std::shared_ptr<const app::LedgerView>> snapshot() {
        QVariantMap state{{"active",active},{"path",path}};
        if(!active) return {state,{}};
        auto& repo=gateway->repository();
        if(metadata.isEmpty()) {
            cachedAccount=repo.account();const auto& acct=*cachedAccount;
            state["name"]=text(acct.name); state["currency"]=text(acct.currency.code());
            state["openingDate"]=text(acct.opening_date); state["opening"]=money(acct.opening);
            state["frozen"]=acct.frozen; state["confirmed"]=acct.opening_confirmed;
            state["committed"]=money(repo.committed_balance()); const auto projection=repo.projected_balance();
            state["projectionAvailable"]=projection.has_value(); state["projected"]=projection?money(*projection):"Incomplete — review import";
            QVariantList batches;
            for(const auto& b:repo.batches()) {
                QVariantMap info{{"id",text(b.id)},{"count",static_cast<int>(b.count)},{"status",b.state==app::BatchState::pending?"Draft":b.state==app::BatchState::committed?"Committed":"Discarded"},{"blocked",b.blocked}};
                if(b.blocked) {
                    const auto parsed=qif::parse(repo.evidence(b.id),b.profile,acct.currency);
                    QStringList diagnostics;
                    if(parsed.diagnostic) diagnostics<<text(qif::message(parsed.diagnostic->issue))+" at byte "+QString::number(parsed.diagnostic->offset);
                    for(std::size_t i=0;i<parsed.records.size() && diagnostics.size()<100;++i) if(parsed.records[i].diagnostic)
                        diagnostics<<"Record "+QString::number(i+1)+": "+text(qif::message(parsed.records[i].diagnostic->issue));
                    info["diagnostics"]=diagnostics.join('\n');
                }
                batches.push_back(info);
                if(b.state==app::BatchState::pending) state["pending"]=info;
            }
            state["batches"]=batches;
            // Reconstruct the latest receipt after restart, including a commit whose
            // durable completion happened before the prior process could acknowledge it.
            for(const auto& b:repo.batches()) if(b.state==app::BatchState::committed) {
                const auto result=repo.outcome("commit-"+b.id+"-"+std::to_string(b.revision-1));
                if(result) receipt={{"operation",text(result->operation)},{"included",static_cast<int>(result->included)},
                    {"excluded",static_cast<int>(result->excluded)},{"before",money(result->before)},
                    {"change",money(result->change)},{"after",money(result->after)}};
            }
            state["receipt"]=receipt;
            metadata=state;
        } else state=metadata;
        state["selected"]=selected;
        state["mode"]=projected?"Projected":"Committed";state["sort"]=sort;state["descending"]=descending;
        state["selectedIndex"]=-1;
        // Never substitute a partial projected ledger for an authoritative one.
        if(projected && !state.value("projectionAvailable").toBool()) return {state,{}};
        if(!cachedView) {
            if(!cachedLedger) cachedLedger=repo.ledger(projected);
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
    void execute(const QString& action,const QVariantMap& a) {
        if(action!="query" && action!="inspect" && action!="evidence" && action!="backup") invalidate();
        if(action=="create" || action=="open" || action=="unlock") {
            if(active) throw std::logic_error("Close the current dataset first");
            path=a.value("path").toString();
            if(action=="create") {
                if(a.value("protected",true).toBool() && a.value("password")!=a.value("confirmation")) throw std::invalid_argument("Passwords do not match");
                const QFileInfo location(path);
                if(!QDir().mkpath(location.absolutePath())) throw std::runtime_error("Cannot create dataset directory");
                gateway->create(path.toStdString(),a.value("protected",true).toBool(),str(a.value("password")),str(a.value("name")),Currency(str(a.value("currency"))),QDate::currentDate().toString(Qt::ISODate).toStdString());
            } else gateway->open(path.toStdString(),action=="unlock"?std::optional(str(a.value("password"))):std::nullopt);
            active=true; selected.clear(); receipt.clear();
            QSettings settings; settings.setValue("datasetPath",path); settings.sync();
            if(settings.status()!=QSettings::NoError) throw std::runtime_error("Dataset is open, but the startup path could not be saved in Settings");
            return;
        }
        if(action=="close") {gateway->close();active=false;selected.clear();receipt.clear();selectedBatch.clear();filter.clear();projected=true;sort=0;descending=false;return;}
        if(!active) throw std::logic_error("Open a dataset first");
        auto& repo=gateway->repository();
        if(action=="import") {
            QFile file(a.value("path").toString()); const QFileInfo info(file);
            if(!info.isFile() || !file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read QIF file");
            if(file.size()>static_cast<qint64>(qif::max_file)) throw std::invalid_argument("QIF exceeds 64 MiB limit");
            const auto bytes=file.read(static_cast<qint64>(qif::max_file)+1);
            if(file.error()!=QFileDevice::NoError || !file.atEnd()) throw std::runtime_error("QIF read failed or exceeded limit");
            if(bytes.size()>static_cast<qsizetype>(qif::max_file)) throw std::invalid_argument("QIF exceeds 64 MiB limit");
            const auto profile=a.value("profile").toString();
            if(profile!="DMY" && profile!="MDY") throw std::invalid_argument("Select DMY or MDY before importing");
            const auto batch=repo.ingest(identity(),bytes.toStdString(),profile=="DMY"?qif::DateOrder::dmy:qif::DateOrder::mdy);
            projected=true; selected.clear();selectedBatch.clear();
            if(batch.count) inspect(batch.id,0);
            else selected={{"batch",text(batch.id)},{"editable",false},{"source",evidencePreview(bytes.toStdString())}};
        } else if(action=="evidence") {
            selectedBatch.clear();selected={{"batch",a.value("batch")},{"editable",false},{"source",evidencePreview(repo.evidence(str(a.value("batch"))))}};
        } else if(action=="inspect") inspect(str(a.value("batch")),a.value("ordinal").toUInt());
        else if(action=="save") {
            if(selectedBatch.empty()) throw std::logic_error("Select a draft first");
            auto row=repo.row({selectedBatch,selectedOrdinal});
            if(!row.source.original_amount || (!row.draft && !row.source.repairable)) throw std::invalid_argument("This record cannot be repaired");
            const auto currency=row.source.original_amount->currency();
            std::vector<Allocation> aa;
            const auto splits=a.value("splits").toList();
            if(splits.size()>static_cast<qsizetype>(qif::max_allocations)) throw std::invalid_argument("Too many allocations");
            for(const auto& value:splits) {const auto v=value.toMap();aa.push_back({str(v.value("category")),str(v.value("memo")),Money::parse(str(v.value("amount")),currency)});}
            imports::DraftRow edit{str(a.value("date")),str(a.value("payee")),str(a.value("memo")),
                Transaction::corrected_import(*row.source.original_amount,Money::parse(str(a.value("total")),currency),std::move(aa))};
            repo.save({selectedBatch,selectedOrdinal},selectedRevision,edit); inspect(selectedBatch,selectedOrdinal);
        } else if(action=="decide") {
            imports::Decision decision{imports::Disposition::include,{}};
            if(a.value("duplicate").toBool()) decision={imports::Disposition::duplicate,imports::Occurrence{str(a.value("batch")),a.value("ordinal").toUInt()}};
            repo.decide({selectedBatch,selectedOrdinal},selectedRevision,decision);inspect(selectedBatch,selectedOrdinal);
        } else if(action=="opening") {
            const auto acct=repo.account();repo.opening(acct.revision,str(a.value("date")),Money::parse(str(a.value("amount")),acct.currency));
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal);
        } else if(action=="commit" || action=="discard") {
            const auto batches=repo.batches(); const auto b=std::find_if(batches.begin(),batches.end(),[](const auto& item){return item.state==app::BatchState::pending;});
            if(b==batches.end()) throw std::logic_error("No pending import");
            if(action=="discard") repo.discard(b->id,b->revision);
            else {
                const auto operation="commit-"+b->id+"-"+std::to_string(b->revision);
                auto result=repo.outcome(operation);
                if(!result) result=repo.commit(b->id,b->revision,operation);
                receipt={{"operation",text(result->operation)},{"included",static_cast<int>(result->included)},{"excluded",static_cast<int>(result->excluded)},
                    {"before",money(result->before)},{"change",money(result->change)},{"after",money(result->after)}};
            }
            if(!selectedBatch.empty()) inspect(selectedBatch,selectedOrdinal);
        } else if(action=="backup") gateway->backup(str(a.value("path")));
        else if(action=="query") {
            if(a.contains("restoreSelection")) {
                const auto restore=a.value("restoreSelection").toMap();
                if(restore.contains("ordinal")) inspect(str(restore.value("batch")),restore.value("ordinal").toUInt());
                else {selected.clear();selectedBatch.clear();selectedOrdinal=0;}
            }
            const auto nextProjected=a.value("projected",true).toBool();
            if(projected!=nextProjected) cachedLedger.reset();
            cachedView.reset();
            filter=a.value("filter").toString();sort=a.value("sort",0).toInt();descending=a.value("descending",false).toBool();projected=nextProjected;
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
    auto row=draft(r.draft);
    row["batch"]=text(r.identity.batch);row["ordinal"]=static_cast<int>(r.identity.ordinal);
    row["status"]=r.committed?"Committed":"Draft";
    row["category"]=t.allocations().size()==1?text(t.allocations()[0].category):"Split";
    row["withdrawal"]=amount.units()<0?money(amount).mid(1):"";
    row["deposit"]=amount.units()>=0?money(amount):"";
    row["balance"]=money(rows_->balances[i]);
    return row;
}
QHash<int,QByteArray> ImportModel::roleNames() const {return {{Qt::UserRole,"record"}};}
void ImportModel::command(const QString& action,const QVariantMap& args) {
    if(busy_) return;
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
        state_=std::move(result.state);error_=std::move(result.error);busy_=false;emit changed();emit completed(action,result.success);
    });
    watcher->setFuture(QtConcurrent::run(&pool_,[worker=worker_,action,args]{
        Result result; bool unlock=false;
        try {worker->execute(action,args);result.success=true;}
        catch(const app::UnlockRequired& e){worker->invalidate();result.error=QString::fromUtf8(e.what());unlock=true;}
        catch(const std::exception& e){worker->invalidate();result.error=QString::fromUtf8(e.what());}
        try {auto snapshot=worker->snapshot();result.state=std::move(snapshot.first);result.rows=std::move(snapshot.second);}
        catch(const std::exception&){worker->invalidate();result.error="Unable to refresh verified dataset state. Close and reopen before continuing.";worker->gateway->close();worker->active=false;worker->selected.clear();worker->receipt.clear();result.state={{"active",false},{"path",worker->path}};result.success=false;}
        result.state["unlockRequired"]=unlock;return result;
    }));
}
