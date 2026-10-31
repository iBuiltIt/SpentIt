#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFont>

class NavigationTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    void run(ImportModel& model,const QString& action,const QVariantMap& args={}) {
        model.command(action,args);
        const auto id=spentit::diagnostics::Collector::instance().snapshot().active.id;
        if(model.diagnosticsEnabled())QTRY_VERIFY_WITH_TIMEOUT(spentit::diagnostics::Collector::instance().snapshot().latest.id==id && spentit::diagnostics::Collector::instance().snapshot().latest.delivered!=0,30000);
        else QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
    }
    static bool click(QObject* root,const char* name) {
        auto* item=root->findChild<QObject*>(QString::fromLatin1(name));
        return item && QMetaObject::invokeMethod(item,"clicked");
    }
    static bool edit(QObject* root,const char* name,const QString& value) {
        auto* item=root->findChild<QObject*>(QString::fromLatin1(name));
        return item && item->setProperty("text",value) && QMetaObject::invokeMethod(item,"textEdited");
    }
private slots:
    void initTestCase() {
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("Navigation");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());QSettings().clear();
    }
    void init() { QSettings().clear(); } // Never reopen a previous test's removed temporary dataset.
    void diagnosticSemanticPrivacy() {
        if(!spentit::diagnostics::Collector::instance().enabled())QSKIP("Opt-in diagnostics test");
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        run(model,"create",{{"path",dir.path()+"/CANARY-path"},{"protected",true},{"password","CANARY-password"},{"confirmation","CANARY-password"},{"name","CANARY-account"},{"currency","CAD"}});
        root->setProperty("currentPage","settings");QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));
        auto observation=spentit::diagnostics::Collector::instance().snapshot();QCOMPARE(observation.ui.surface,spentit::diagnostics::Surface::settings);
        QVERIFY(observation.ui.time!=0);
        QVERIFY((observation.ui.control_visible & (1ULL<<1))==0);
        QVERIFY((observation.ui.control_enabled & (1ULL<<1))==0);
        root->setProperty("dirty",true);QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));QVERIFY(spentit::diagnostics::Collector::instance().snapshot().ui.dirty);
        window->hide();QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));QVERIFY(!spentit::diagnostics::Collector::instance().snapshot().ui.native_exposed);
        QVERIFY(spentit::diagnostics::Collector::instance().json().find("CANARY")==std::string::npos);
        const auto generation=model.diagnosticGeneration();model.observeUi({{"generation",generation-1},{"surface","CANARY-payee"},{"ready",true}});
        QCOMPARE(spentit::diagnostics::Collector::instance().snapshot().ui.surface,spentit::diagnostics::Surface::settings);
    }
    void guardedNavigationAndIsolation() {
        QTemporaryDir dir;
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto path=dir.path()+"/document";
        run(model,"create",{{"path",path},{"protected",false},{"name","Synthetic navigation"},{"currency","CAD"}});
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes="D1/1/2026\nT-1\nPOriginal\n^\n";QCOMPARE(file.write(bytes),bytes.size());file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        if(model.diagnosticsEnabled()) {
            const auto record=model.rowIdentity(0);
            model.command("select",{{"row",0}});
            const auto selectedOperation=spentit::diagnostics::Collector::instance().snapshot().active.id;
            model.command("history");
            model.command("inspect",record);const auto deferred=spentit::diagnostics::Collector::instance().snapshot().deferred;
            model.command("inspect",record);
            QVERIFY(deferred!=0);QVERIFY(spentit::diagnostics::Collector::instance().snapshot().deferred!=deferred);
            auto observation=spentit::diagnostics::Collector::instance().snapshot();
            QCOMPARE(observation.outcomes[deferred%32].result,spentit::diagnostics::Result::superseded);
            QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
            QVERIFY(spentit::diagnostics::Collector::instance().snapshot().latest.id!=selectedOperation);
            QTRY_COMPARE(spentit::diagnostics::Collector::instance().snapshot().cleanup,0ULL);
        }
        const auto identity=model.rowIdentity(model.selectedRowIndex());
        QCOMPARE(root->property("currentPage").toString(),"ledger");
        auto* toggle=root->findChild<QQuickItem*>("sidebarToggle");QVERIFY(toggle);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,toggle->mapToScene(QPointF(toggle->width()/2,toggle->height()/2)).toPoint());
        QTRY_VERIFY(!root->property("sidebarVisible").toBool());
        QVERIFY(click(root,"settingsNavigation"));QCOMPARE(root->property("currentPage").toString(),"settings");
        QCOMPARE(model.rowIdentity(model.selectedRowIndex()),identity);
        QVERIFY(click(root,"navigationBack"));QCOMPARE(root->property("currentPage").toString(),"ledger");
        QVERIFY(click(root,"navigationForward"));QCOMPARE(root->property("currentPage").toString(),"settings");
        QVERIFY(click(root,"navigationBack"));
        QVERIFY(click(root,"sidebarToggle"));QVERIFY(root->property("sidebarVisible").toBool());
        window->resize(1100,750);QTest::qWait(100);
        auto* sidebar=root->findChild<QQuickItem*>("accountSidebar");QVERIFY(sidebar);
        QVERIFY(sidebar->width()>=140);QVERIFY(sidebar->width()<=360);
        auto* settingsButton=root->findChild<QQuickItem*>("settingsNavigation");QVERIFY(settingsButton);
        const auto settingsCenter=settingsButton->mapToScene(QPointF(settingsButton->width()/2,settingsButton->height()/2));
        QVERIFY(settingsCenter.x()>0 && settingsCenter.x()<window->width());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,settingsCenter.toPoint());
        QTRY_COMPARE(root->property("currentPage").toString(),"settings");
        QVERIFY(click(root,"navigationBack"));
        window->resize(1440,850);QTest::qWait(100);
        QVERIFY(edit(root,"durablePayee","Unsaved"));QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"settingsNavigation"));
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(click(root,"guardStay"));QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(edit(root,"durableTotal","invalid"));QVERIFY(click(root,"settingsNavigation"));QVERIFY(click(root,"guardSave"));
        QTRY_VERIFY(!model.busy());QVERIFY(!model.error().isEmpty());
        QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"settingsNavigation"));QVERIFY(click(root,"guardDiscard"));
        QCOMPARE(root->property("currentPage").toString(),"settings");QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Original");
        QVERIFY(click(root,"navigationBack"));
        QVERIFY(edit(root,"durablePayee","Saved navigation"));QVERIFY(click(root,"settingsNavigation"));QVERIFY(click(root,"guardSave"));
        QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(root->property("currentPage").toString(),"settings");
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Saved navigation");
        QVERIFY(click(root,"closeDataset"));QTRY_VERIFY(!model.busy());QVERIFY(!model.state().value("active").toBool());
        QCOMPARE(root->property("currentPage").toString(),"ledger");QCOMPARE(root->property("navigationIndex").toInt(),0);
        run(model,"create",{{"path",dir.path()+"/second"},{"protected",false},{"name","Other dataset"},{"currency","CAD"}});
        QVERIFY(model.state().value("selected").toMap().isEmpty());QCOMPARE(root->property("navigationIndex").toInt(),0);
        QVERIFY(!root->findChild<QObject*>("navigationBack")->property("enabled").toBool());
        run(model,"close");run(model,"open",{{"path",path}});
        run(model,"inspect",identity);
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Saved navigation");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    NavigationTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_navigation_tests.moc"
