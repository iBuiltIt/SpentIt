#pragma once
#include <spentit/document_gateway.hpp>
#include "about/about_info.hpp"
#include <spentit/import_view.hpp>
#include <QAbstractListModel>
#include <QStringList>
#include <QThreadPool>
#include <QVariantMap>
#include <QUrl>
#include <functional>
#include <spentit/diagnostics.hpp>
class ImportModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(AboutInfo* aboutInformation READ aboutInformation CONSTANT)
    Q_PROPERTY(bool rememberWindowPosition READ rememberWindowPosition WRITE setRememberWindowPosition NOTIFY windowPreferencesChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool selecting READ selecting NOTIFY changed)
    Q_PROPERTY(bool openingDocument READ openingDocument NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
    Q_PROPERTY(bool cancellationRequested READ cancellationRequested NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)
    Q_PROPERTY(QString suggestedPath READ suggestedPath CONSTANT)
    Q_PROPERTY(QUrl documentsFolder READ documentsFolder CONSTANT)
    Q_PROPERTY(QUrl suggestedFile READ suggestedFile CONSTANT)
    Q_PROPERTY(bool diagnosticsEnabled READ diagnosticsEnabled CONSTANT)
    Q_PROPERTY(qulonglong diagnosticGeneration READ diagnosticGeneration NOTIFY changed)
public:
    AboutInfo* aboutInformation() { return &aboutInformation_; }
    bool rememberWindowPosition() const {return rememberWindowPosition_;}
    void setRememberWindowPosition(bool);
    Q_INVOKABLE void restoreWindowPosition(QObject*);
    Q_INVOKABLE bool saveWindowPosition(QObject*);
    bool diagnosticsEnabled() const { return spentit::diagnostics::Collector::instance().enabled(); }
    qulonglong diagnosticGeneration() const { return spentit::diagnostics::Collector::instance().generation(); }
    Q_INVOKABLE void observeUi(const QVariantMap&);
    explicit ImportModel(std::unique_ptr<spentit::app::DocumentGateway>,QObject* parent=nullptr);
    // A file passed on the command line, including a double-clicked .spentit file.
    // Empty error means no document argument. A message means SpentIt must not start.
    static QString launchDocumentPath(const QStringList& arguments, QString* error);
    void setLaunchDocument(const QString& path);
    ~ImportModel() override;
    QVariantMap state() const { return state_; }
    bool busy() const { return busy_; }
    bool openingDocument() const {return busy_ && (activeAction_=="open" || activeAction_=="unlock" || activeAction_=="probe");}
    bool selecting() const { return busy_ && activeAction_=="select"; }
    QString phase() const { return phase_; }
    bool canCancel() const { return busy_ && operation_ && operation_->state.load()==spentit::app::OperationContext::available; }
    bool cancellationRequested() const { return busy_ && operation_ && operation_->state.load()==spentit::app::OperationContext::requested; }
    Q_INVOKABLE void requestCancel();
    Q_INVOKABLE bool reorderAccounts(const QStringList& ids);
    QString error() const { return error_; }
    QStringList currencies() const;
    QString suggestedPath() const;
    QUrl documentsFolder() const;
    QUrl suggestedFile() const;
    Q_INVOKABLE QString datasetFilename(const QString&) const;
    int rowCount(const QModelIndex& parent={}) const override;
    QVariant data(const QModelIndex&,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    Q_INVOKABLE void command(const QString& action,const QVariantMap& args={});
    Q_INVOKABLE bool passwordMatches(const QString&) const;
    Q_INVOKABLE QString suggestedDateFormat(const QString&) const;
    Q_INVOKABLE QString todayDisplay() const;
    Q_INVOKABLE QVariantMap calendarPage(const QString& display) const;
    Q_INVOKABLE QString calendarMove(const QString& iso,int days,int months=0,int years=0) const;
    Q_INVOKABLE QString dateDisplay(const QString& iso) const;
    Q_INVOKABLE QVariantMap reportPeriod(const QVariantMap& period,const QVariantMap& main={}) const;
    Q_INVOKABLE QString dateFormatExample(const QString&) const;
    Q_INVOKABLE bool accountNameMatches(const QString& name,const QString& selected);
    Q_INVOKABLE bool categoryNameMatches(const QString& name,const QString& selected,bool includeChildren=false);
    Q_INVOKABLE QString newIdentity() const;
    Q_INVOKABLE QVariantMap amountValue(const QString& expression,const QString& currencyDefinition) const;
    Q_INVOKABLE QVariantMap splitAmounts(const QString& total,const QVariantList& amounts,bool automatic,const QString& currencyDefinition,int remainderIndex=0) const;
    Q_INVOKABLE int chooseSplitRemainder(const QString& total,const QVariantList& splits,const QString& currencyDefinition) const;
    Q_INVOKABLE QString localPath(const QUrl&) const;
    Q_INVOKABLE int selectedRowIndex() const;
    Q_INVOKABLE QVariantMap rowIdentity(int row) const;
    Q_INVOKABLE int occurrenceIndex(const QString& batch, int ordinal) const;
    Q_INVOKABLE QVariantMap rowRecord(int row) const;
signals: void windowPreferencesChanged(); void changed(); void completed(QString action,bool success);
private:
    AboutInfo aboutInformation_{this};
    struct Worker;
    std::shared_ptr<Worker> worker_;
    QThreadPool pool_;
    QVariantMap state_;
    std::shared_ptr<const spentit::app::LedgerView> rows_;
    spentit::diagnostics::Id diagnosticOperation_=0, deferredDiagnostic_=0, resumedDiagnostic_=0, diagnosticFrameRequested_=UINT64_MAX;
    bool rememberWindowPosition_=false;
    bool busy_=false;
    QString error_,phase_;
    QString activeAction_;
    std::optional<QVariantMap> pendingInspection_;
    std::shared_ptr<spentit::app::OperationContext> operation_;
};
