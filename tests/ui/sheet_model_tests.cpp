#include "sheet_model.hpp"
#include <QSignalSpy>
#include <QTest>
#include <QThreadPool>
class SheetTests : public QObject {
    Q_OBJECT
private slots:
    void identityAndEditing() {
        SheetModel model(1000); QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),10000);
        QCOMPARE(model.rowCount(),1000);
        model.select(1); model.startEdit(1); model.setField("payee","Changed payee");
        QVERIFY(model.dirty()); model.sortBy(1); QTRY_VERIFY(!model.busy());
        QCOMPARE(model.editingId(),1); QCOMPARE(model.edit().value("payee").toString(),QString("Changed payee"));
        QCOMPARE(model.selectedId(),1);
        model.setField("total","invalid"); model.save(); QTRY_VERIFY(!model.busy());
        QVERIFY(!model.error().isEmpty()); QCOMPARE(model.editingId(),1);
        QVERIFY(model.rowInfo(1).value("payee") != "Changed payee");
        model.setField("total","-20.00"); model.save(); QTRY_VERIFY(!model.busy());
        QVERIFY(model.error().isEmpty()); QCOMPARE(model.editingId(),0);
        QCOMPARE(model.rowInfo(1).value("total").toString(),QString("-20.00"));
        QCOMPARE(model.rowInfo(1).value("original").toString(),QString("-12.00"));
        QCOMPARE(model.rowInfo(1).value("payee").toString(),QString("Changed payee"));
    }
    void navigation() {
        SheetModel model(100); QTRY_VERIFY(!model.busy());
        QSignalSpy prompt(&model,&SheetModel::askNavigation), closed(&model,&SheetModel::closeAllowed);
        model.startEdit(1); model.setField("payee","Unsaved"); model.navigate(1);
        QCOMPARE(prompt.count(),1); model.resolve("stay"); QCOMPARE(model.title(),QString("Everyday checking")); QVERIFY(model.dirty());
        model.requestClose(); QCOMPARE(prompt.count(),2); model.resolve("stay"); QCOMPARE(closed.count(),0);
        model.navigate(1); model.resolve("save"); QTRY_VERIFY(!model.busy());
        QCOMPARE(model.title(),QString("Reserve account")); QCOMPARE(model.rowInfo(1).value("payee").toString(),QString("Unsaved"));
        model.back(); QTRY_VERIFY(!model.busy()); QCOMPARE(model.title(),QString("Everyday checking"));
        model.startEdit(1); model.setField("payee","Discarded"); model.forward(); model.resolve("discard"); QTRY_VERIFY(!model.busy());
        QCOMPARE(model.title(),QString("Reserve account")); QCOMPARE(model.rowInfo(1).value("payee").toString(),QString("Unsaved"));
        model.back(); QTRY_VERIFY(!model.busy()); model.startEdit(1); model.setField("total","bad"); model.navigate(1); model.resolve("save"); QTRY_VERIFY(!model.busy());
        QCOMPARE(model.title(),QString("Everyday checking")); QVERIFY(model.dirty());
        model.cancel(); model.requestClose(); QCOMPARE(closed.count(),1);
    }
    void selectionAndFilter() {
        SheetModel model(1000); QTRY_VERIFY(!model.busy());
        model.select(1); model.rememberViewport(1,7.0);
        QSignalSpy reveal(&model,&SheetModel::reveal);
        model.filter("Payday"); QTRY_VERIFY(!model.busy()); QCOMPARE(model.indexOf(1),-1);
        model.filter(""); QTRY_VERIFY(!model.busy()); QCOMPARE(model.selectedId(),1);
        QCOMPARE(reveal.last()[1].toDouble(),7.0);
        model.filter("Payday"); QTRY_VERIFY(!model.busy());
        const int target = model.data(model.index(0),SheetModel::Identity).toInt(); model.select(target);
        model.filter(""); QTRY_VERIFY(!model.busy()); QCOMPARE(model.selectedId(),target);
        const int another = model.data(model.index(1),SheetModel::Identity).toInt(); model.select(another,true);
        QCOMPARE(model.selectedCount(),2);
        model.sortBy(4); QTRY_VERIFY(!model.busy()); QCOMPARE(model.selectedCount(),2);
        model.moveSelection(1,true); QVERIFY(model.selectedCount() >= 1);
        model.startEdit(1); model.setField("payee","Retained while filtered");
        model.filter("absent"); QTRY_VERIFY(!model.busy()); QCOMPARE(model.rowCount(),0); QCOMPARE(model.editingId(),1);
        model.filter(""); QTRY_VERIFY(!model.busy()); QCOMPARE(model.edit().value("payee").toString(),QString("Retained while filtered"));
    }
    void cleanupTestCase() { QThreadPool::globalInstance()->waitForDone(); }
};
QTEST_GUILESS_MAIN(SheetTests)
#include "sheet_model_tests.moc"
