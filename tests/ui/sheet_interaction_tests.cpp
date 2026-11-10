#include "sheet_model.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include "controls_style.hpp"
#include <QTest>
#include <QThreadPool>
#include <QSignalSpy>

static QQuickItem* visual(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto* child : root->childItems()) if (auto* found = visual(child,name)) return found;
    return nullptr;
}
class InteractionTests : public QObject {
    Q_OBJECT
private slots:
    void pointerKeyboardAndRecycling() {
        SheetModel model(1000);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("sheet", &model);
        engine.load(QUrl("qrc:/spentit/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window)); QTRY_VERIFY(!model.busy());
        auto* ledger = window->findChild<QQuickItem*>("ledger"); QVERIFY(ledger);
        auto rowPoint = [&](int y) { return ledger->mapToScene(QPointF(180,y)).toPoint(); };
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,rowPoint(24));
        QCOMPARE(model.selectedId(),1);
        QTest::keyClick(window,Qt::Key_Down); QVERIFY(model.selectedId() != 1);
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,rowPoint(24));
        QTRY_COMPARE(model.editingId(),1);
        QQuickItem* payee = nullptr; QTRY_VERIFY((payee = visual(window->contentItem(),"editPayee")) != nullptr);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,payee->mapToScene(QPointF(50,15)).toPoint());
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        // Use the public QML input signal to check the actual field-to-model binding.
        payee->setProperty("text","Pointer edit"); QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QCOMPARE(model.edit().value("payee").toString(),QString("Pointer edit"));
        model.expand(2); model.expand(3);
        ledger->setProperty("contentY",20000); QTest::qWait(100);
        QCOMPARE(model.editingId(),1); QCOMPARE(model.edit().value("payee").toString(),QString("Pointer edit"));
        QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning")); QTest::qWait(100);
        QQuickItem* save = nullptr; QTRY_VERIFY((save = visual(window->contentItem(),"saveEdit")) != nullptr);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,save->mapToScene(QPointF(save->width()/2,save->height()/2)).toPoint());
        QTRY_VERIFY(!model.busy()); QTRY_COMPARE(model.editingId(),0);
        QCOMPARE(model.rowInfo(1).value("payee").toString(),QString("Pointer edit"));
        model.startEdit(1); model.setField("payee","Unsaved close");
        QVERIFY(!window->close());
        auto* dialog = window->findChild<QObject*>("navigationDialog"); QVERIFY(dialog); QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(window->isVisible()); model.resolve("stay");
        window->setProperty("closingApproved",true); window->close();
    }
    void cleanupTestCase() { QThreadPool::globalInstance()->waitForDone(); }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv); app.setFont(QFont("Helvetica Neue",13)); applySpentItControlsStyle();
    InteractionTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "sheet_interaction_tests.moc"
