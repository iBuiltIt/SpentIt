#pragma once
#include <spentit/document_gateway.hpp>
#include <spentit/import_view.hpp>
#include <QAbstractListModel>
#include <QThreadPool>
#include <QVariantMap>
#include <functional>
class ImportModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
    Q_PROPERTY(bool cancellationRequested READ cancellationRequested NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)
    Q_PROPERTY(QString suggestedPath READ suggestedPath CONSTANT)
public:
    explicit ImportModel(std::unique_ptr<spentit::app::DocumentGateway>,QObject* parent=nullptr);
    ~ImportModel() override;
    QVariantMap state() const { return state_; }
    bool busy() const { return busy_; }
    QString phase() const { return phase_; }
    bool canCancel() const { return busy_ && operation_ && operation_->state.load()==spentit::app::OperationContext::available; }
    bool cancellationRequested() const { return busy_ && operation_ && operation_->state.load()==spentit::app::OperationContext::requested; }
    Q_INVOKABLE void requestCancel();
    QString error() const { return error_; }
    QStringList currencies() const;
    QString suggestedPath() const;
    int rowCount(const QModelIndex& parent={}) const override;
    QVariant data(const QModelIndex&,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    Q_INVOKABLE void command(const QString& action,const QVariantMap& args={});
    Q_INVOKABLE QString localPath(const QUrl&) const;
    Q_INVOKABLE int selectedRowIndex() const;
    Q_INVOKABLE QVariantMap rowIdentity(int row) const;
    Q_INVOKABLE int occurrenceIndex(const QString& batch, int ordinal) const;
signals: void changed(); void completed(QString action,bool success);
private:
    struct Worker;
    std::shared_ptr<Worker> worker_;
    QThreadPool pool_;
    QVariantMap state_;
    std::shared_ptr<const spentit::app::LedgerView> rows_;
    bool busy_=false;
    QString error_,phase_;
    std::shared_ptr<spentit::app::OperationContext> operation_;
};
