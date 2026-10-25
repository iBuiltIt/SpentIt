#include "sheet_model.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <algorithm>

using namespace spentit;
using namespace spentit::sheet;
static QString q(const std::string& s) { return QString::fromStdString(s); }
struct Work { Snapshot rows; View view; QString error, otherCommitted, otherProjected; qint64 ms = 0; };
SheetModel::SheetModel(int count, QObject* parent) : QAbstractListModel(parent) {
    auto* watcher = new QFutureWatcher<Work>(this);
    connect(watcher, &QFutureWatcher<Work>::finished, this, [this, watcher] {
        auto result = watcher->future().takeResult(); watcher->deleteLater();
        beginResetModel(); rows_ = std::move(result.rows); view_ = std::move(result.view); endResetModel();
        committedBalances_[0] = q(view_.committed); projectedBalances_[0] = q(view_.projected);
        committedBalances_[1] = result.otherCommitted; projectedBalances_[1] = result.otherProjected;
        error_ = result.error; busy_ = false;
        metrics_ = QString("%1 rows/account · fixture + first query %2 ms").arg(rowCount()).arg(result.ms);
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([count] {
        Work work; QElapsedTimer timer; timer.start();
        try { work.rows = synthetic(count); work.view = query(*work.rows, 0, "", 0, false);
            const auto other = query(*work.rows,1,"",0,false); work.otherCommitted = q(other.committed); work.otherProjected = q(other.projected); }
        catch (const std::exception& e) { work.error = QString::fromUtf8(e.what()); }
        work.ms = timer.elapsed(); return work;
    }));
}
int SheetModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(view_.indices.size()); }
QHash<int,QByteArray> SheetModel::roleNames() const {
    return {{Identity,"transactionId"},{Date,"dateText"},{Payee,"payeeText"},{Category,"categoryText"},
            {Withdrawal,"withdrawalText"},{Deposit,"depositText"},{Balance,"balanceText"},{Draft,"isDraft"},
            {Selected,"isSelected"},{Expanded,"isExpanded"},{Splits,"splitText"}};
}
QVariant SheetModel::data(const QModelIndex& index, int role) const {
    if (!rows_ || !index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto original = static_cast<std::size_t>(view_.indices[static_cast<std::size_t>(index.row())]);
    const auto& row = (*rows_)[original]; const auto& total = row.transaction.effective_total();
    switch (role) {
    case Identity: return row.id;
    case Date: return q(row.date);
    case Payee: return q(row.payee);
    case Category: return row.transaction.allocations().size() > 1 ? QString("Split · %1").arg(row.transaction.allocations().size()) : q(row.transaction.allocations()[0].category);
    case Withdrawal: { auto text = q(decimal(total)); return total.units() < 0 ? text.mid(1) : QString(); }
    case Deposit: return total.units() >= 0 ? q(decimal(total)) : QString();
    case Balance: return q(view_.balances[original]);
    case Draft: return row.draft;
    case Selected: return selection_.contains(row.id);
    case Expanded: return expanded_.contains(row.id);
    case Splits: {
        QStringList lines;
        for (const auto& part : row.transaction.allocations()) lines << q(part.category) + "   ·   " + q(part.memo) + "   ·   CAD " + q(decimal(part.amount));
        return lines.join('\n');
    }
    default: return {};
    }
}
QString SheetModel::title() const { return account_ == 0 ? "Everyday checking" : "Reserve account"; }
QString SheetModel::totals() const { return QString("Filtered totals · %1 rows     Withdrawal CAD %2     Deposit CAD %3").arg(rowCount()).arg(q(view_.withdrawals), q(view_.deposits)); }
QString SheetModel::balance() const { return (projected_[account_] ? "Projected · CAD " : "Committed · CAD ") + q(projected_[account_] ? view_.projected : view_.committed); }
QStringList SheetModel::accountBalances() const {
    QStringList result;
    for (int i=0;i<2;++i) result << (projected_[i] ? "Projected · CAD " : "Committed · CAD ") + (projected_[i] ? projectedBalances_[i] : committedBalances_[i]);
    return result;
}
bool SheetModel::dirty() const {
    if (!edit_ || !rows_) return false;
    const auto original = begin_edit(*rows_, edit_->id);
    return edit_->payee != original.payee || edit_->note != original.note || edit_->total != original.total || edit_->category != original.category || edit_->memo != original.memo || edit_->allocation != original.allocation || edit_->mixed != original.mixed || edit_->addAllocation != original.addAllocation;
}
QVariantMap SheetModel::edit() const {
    if (!edit_) return {};
    return {{"payee",q(edit_->payee)},{"note",q(edit_->note)},{"total",q(edit_->total)},
        {"category",q(edit_->category)},{"memo",q(edit_->memo)},{"allocation",q(edit_->allocation)},{"mixed",edit_->mixed},{"addAllocation",edit_->addAllocation},
        {"hasSplit",by_id(*rows_,edit_->id).transaction.allocations().size() > 1}};
}
int SheetModel::indexOf(int id) const {
    if (!rows_) return -1;
    const auto found = std::find_if(view_.indices.begin(), view_.indices.end(), [&](int i) { return (*rows_)[static_cast<std::size_t>(i)].id == id; });
    return found == view_.indices.end() ? -1 : static_cast<int>(found - view_.indices.begin());
}
void SheetModel::emitRows() { if (rowCount()) emit dataChanged(index(0), index(rowCount()-1), {Selected, Expanded}); emit changed(); }
void SheetModel::select(int id, bool toggle, bool range) {
    if (busy_ || indexOf(id) < 0) return;
    if (!filter_.isEmpty()) searchSelected_ = true;
    if (range && indexOf(anchor_) >= 0) {
        selection_.clear(); const int a = indexOf(anchor_), b = indexOf(id);
        for (int i = std::min(a,b); i <= std::max(a,b); ++i) selection_.insert((*rows_)[static_cast<std::size_t>(view_.indices[static_cast<std::size_t>(i)])].id);
    } else {
        if (!toggle) selection_.clear();
        if (toggle && selection_.contains(id)) selection_.remove(id); else selection_.insert(id);
        anchor_ = id;
    }
    selected_ = id; locations_[account_].selected = id; emitRows();
}
void SheetModel::moveSelection(int delta, bool range) {
    if (busy_ || !rowCount()) return;
    const int current = indexOf(selected_);
    const int next = std::clamp(current < 0 ? 0 : current + delta, 0, rowCount()-1);
    select(data(index(next), Identity).toInt(), false, range); emit reveal(next, -1);
}
void SheetModel::expand(int id) { if (expanded_.contains(id)) expanded_.remove(id); else expanded_.insert(id); emitRows(); }
void SheetModel::guard(std::function<void()> action) {
    if (busy_) return;
    if (dirty()) { pending_ = std::move(action); emit askNavigation(); }
    else { edit_.reset(); error_.clear(); emit changed(); action(); }
}
void SheetModel::startEdit(int id) {
    if (!rows_ || busy_ || id == editingId()) return;
    guard([this,id] { select(id); edit_ = begin_edit(*rows_,id); error_.clear(); emit changed(); emit reveal(indexOf(id), -1); });
}
void SheetModel::setField(const QString& name, const QString& value) {
    if (!edit_ || busy_) return;
    auto text = value.toStdString();
    if (name == "payee") edit_->payee = text;
    else if (name == "note") edit_->note = text;
    else if (name == "total") edit_->total = text;
    else if (name == "category") edit_->category = text;
    else if (name == "memo") edit_->memo = text;
    else if (name == "allocation") edit_->allocation = text;
    else if (name == "addAllocation") edit_->addAllocation = value == "true";
    emit changed();
}
void SheetModel::setMixed(bool value) { if (edit_ && !busy_) { edit_->mixed = value; emit changed(); } }
void SheetModel::save() {
    if (!edit_ || busy_) return;
    busy_ = true; error_.clear(); emit changed();
    const auto rows = rows_; const auto edit = *edit_; const int account = account_, column = column_; const bool desc = descending_; const auto filter = filter_.toStdString();
    auto* watcher = new QFutureWatcher<Work>(this);
    connect(watcher, &QFutureWatcher<Work>::finished, this, [this,watcher] {
        auto result = watcher->future().takeResult(); watcher->deleteLater(); busy_ = false;
        if (!result.error.isEmpty()) { error_ = result.error; pending_ = {}; emit changed(); return; }
        beginResetModel(); rows_ = std::move(result.rows); view_ = std::move(result.view); endResetModel();
        committedBalances_[account_] = q(view_.committed); projectedBalances_[account_] = q(view_.projected);
        edit_.reset(); emit changed(); emit reveal(indexOf(selected_), -1);
        auto action = std::move(pending_); pending_ = {}; if (action) action();
    });
    watcher->setFuture(QtConcurrent::run([rows,edit,account,column,desc,filter] {
        Work result;
        try { result.rows = save_edit(*rows,edit); result.view = query(*result.rows,account,filter,column,desc); }
        catch (const std::exception& e) { result.error = QString::fromUtf8(e.what()); }
        return result;
    }));
}
void SheetModel::cancel() { if (busy_) return; edit_.reset(); error_.clear(); pending_ = {}; emit changed(); }
void SheetModel::refresh() {
    if (!rows_) return;
    busy_ = true; error_.clear(); emit changed();
    const auto rows = rows_; const int account = account_, column = column_; const bool desc = descending_; const auto filter = filter_.toStdString();
    auto* watcher = new QFutureWatcher<Work>(this);
    connect(watcher, &QFutureWatcher<Work>::finished, this, [this,watcher] {
        auto result = watcher->future().takeResult(); watcher->deleteLater(); busy_ = false;
        if (result.error.isEmpty()) { beginResetModel(); view_ = std::move(result.view); endResetModel(); }
        error_ = result.error; metrics_ = QString("%1 visible · worker query %2 ms").arg(rowCount()).arg(result.ms); emit changed();
        const auto location = locations_[account_];
        const int top = indexOf(location.top);
        emit reveal(top >= 0 ? top : indexOf(selected_), top >= 0 ? location.offset : -1);
    });
    watcher->setFuture(QtConcurrent::run([rows,account,column,desc,filter] {
        Work result; QElapsedTimer timer; timer.start();
        try { result.view = query(*rows,account,filter,column,desc); }
        catch (const std::exception& e) { result.error = QString::fromUtf8(e.what()); }
        result.ms = timer.elapsed(); return result;
    }));
}
void SheetModel::sortBy(int column) {
    if (busy_ || column < 0 || column > 5) return;
    descending_ = column == column_ ? !descending_ : false; column_ = column;
    locations_[account_].top = selected_; locations_[account_].offset = -1; refresh();
}
void SheetModel::filter(const QString& text) {
    if (busy_ || text == filter_) return;
    // Literal-only prototype filter. Query language is deliberately not inferred.
    if (text.toUtf8().size() > 256) { error_ = "Filter exceeds 256 bytes"; emit changed(); return; }
    if (filter_.isEmpty() && !text.isEmpty()) { preSearch_ = locations_[account_]; searchSelected_ = false; }
    if (text.isEmpty()) {
        if (!searchSelected_) { locations_[account_] = preSearch_; selected_ = preSearch_.selected; selection_.clear(); if (selected_) selection_.insert(selected_); }
        else { locations_[account_].top = selected_; locations_[account_].offset = -1; }
    } else { locations_[account_].top = selected_; locations_[account_].offset = -1; }
    filter_ = text; refresh();
}
void SheetModel::changeAccount(int account) {
    account_ = account; selected_ = locations_[account_].selected;
    selection_.clear(); if (selected_) selection_.insert(selected_); filter_.clear(); refresh();
}
void SheetModel::navigate(int account) {
    if (account < 0 || account > 1 || account == account_) return;
    guard([this,account] { history_.resize(static_cast<std::size_t>(historyPosition_ + 1)); history_.push_back(account); ++historyPosition_; changeAccount(account); });
}
void SheetModel::back() { if (canBack()) guard([this] { --historyPosition_; changeAccount(history_[static_cast<std::size_t>(historyPosition_)]); }); }
void SheetModel::forward() { if (canForward()) guard([this] { ++historyPosition_; changeAccount(history_[static_cast<std::size_t>(historyPosition_)]); }); }
void SheetModel::requestClose() { guard([this] { emit closeAllowed(); }); }
void SheetModel::resolve(const QString& choice) {
    if (choice == "stay") { pending_ = {}; return; }
    if (choice == "save") { save(); return; }
    if (choice == "discard") { edit_.reset(); auto action = std::move(pending_); pending_ = {}; error_.clear(); emit changed(); if (action) action(); }
}
void SheetModel::toggleBalance() { toggleAccountBalance(account_); }
void SheetModel::toggleAccountBalance(int account) { if (account < 0 || account > 1) return; projected_[account] = !projected_[account]; emit changed(); }
void SheetModel::rememberViewport(int id, double offset) { if (!busy_) { locations_[account_].top = id; locations_[account_].offset = offset; } }
QVariantMap SheetModel::rowInfo(int id) const {
    if (!rows_ || id <= 0) return {};
    const auto& row = by_id(*rows_,id);
    return {{"total",q(decimal(row.transaction.effective_total()))},{"original",q(decimal(*row.transaction.original_imported_amount()))},
        {"payee",q(row.payee)},{"corrected",row.transaction.has_corrected_imported_total()}};
}
