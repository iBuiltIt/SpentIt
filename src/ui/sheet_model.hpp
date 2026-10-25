#pragma once
#include <spentit/sheet_session.hpp>
#include <QAbstractListModel>
#include <QSet>
#include <QVariantMap>
#include <functional>

class SheetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString totals READ totals NOTIFY changed)
    Q_PROPERTY(QString balance READ balance NOTIFY changed)
    Q_PROPERTY(QStringList accountBalances READ accountBalances NOTIFY changed)
    Q_PROPERTY(QString metrics READ metrics NOTIFY changed)
    Q_PROPERTY(int selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(int editingId READ editingId NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(QVariantMap edit READ edit NOTIFY changed)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY changed)
    Q_PROPERTY(bool descending READ descending NOTIFY changed)
    Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
    Q_PROPERTY(bool canForward READ canForward NOTIFY changed)
public:
    explicit SheetModel(int count, QObject* parent = nullptr);
    enum Role { Identity = Qt::UserRole + 1, Date, Payee, Category, Withdrawal, Deposit, Balance, Draft, Selected, Expanded, Splits };
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex&, int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    bool busy() const { return busy_; }
    QString error() const { return error_; }
    QString title() const;
    QString totals() const;
    QString balance() const;
    QStringList accountBalances() const;
    QString metrics() const { return metrics_; }
    int selectedId() const { return selected_; }
    int selectedCount() const { return static_cast<int>(selection_.size()); }
    int editingId() const { return edit_ ? edit_->id : 0; }
    bool dirty() const;
    QVariantMap edit() const;
    int sortColumn() const { return column_; }
    bool descending() const { return descending_; }
    bool canBack() const { return historyPosition_ > 0; }
    bool canForward() const { return historyPosition_ + 1 < static_cast<int>(history_.size()); }
    Q_INVOKABLE int indexOf(int id) const;
    Q_INVOKABLE void select(int id, bool toggle = false, bool range = false);
    Q_INVOKABLE void moveSelection(int delta, bool range);
    Q_INVOKABLE void expand(int id);
    Q_INVOKABLE void startEdit(int id);
    Q_INVOKABLE void setField(const QString&, const QString&);
    Q_INVOKABLE void setMixed(bool);
    Q_INVOKABLE void save();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void sortBy(int column);
    Q_INVOKABLE void filter(const QString& text);
    Q_INVOKABLE void navigate(int account);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void requestClose();
    Q_INVOKABLE void resolve(const QString& choice);
    Q_INVOKABLE void toggleBalance();
    Q_INVOKABLE void toggleAccountBalance(int account);
    Q_INVOKABLE void rememberViewport(int id, double offset);
    Q_INVOKABLE QVariantMap rowInfo(int id) const;
signals:
    void changed();
    void askNavigation();
    void closeAllowed();
    void reveal(int index, double offset);
private:
    spentit::sheet::Snapshot rows_;
    spentit::sheet::View view_;
    std::optional<spentit::sheet::Edit> edit_;
    QSet<int> selection_, expanded_;
    int selected_ = 0, anchor_ = 0, account_ = 0, column_ = 0;
    bool busy_ = true, descending_ = false;
    bool projected_[2] = {true,true};
    QString committedBalances_[2], projectedBalances_[2];
    QString error_, metrics_, filter_;
    std::vector<int> history_{0}; int historyPosition_ = 0;
    struct Location { int selected = 0; int top = 0; double offset = 0; };
    Location locations_[2], preSearch_;
    bool searchSelected_ = false;
    std::function<void()> pending_;
    void refresh();
    void guard(std::function<void()>);
    void changeAccount(int);
    void emitRows();
};
