#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QFont>
using namespace spentit;

class RecoveryUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {
        model.command(action,args);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty()==success,qPrintable(action+": "+model.error()));
    }
    static bool click(QObject* root,const char* name) {
        auto* item=root->findChild<QObject*>(QString::fromLatin1(name));
        return item && QMetaObject::invokeMethod(item,"clicked");
    }
    void retained(const QString& path,std::string_view bytes) {
        auto gateway=storage::document_gateway();
        gateway->create(path.toStdString(),false,"","Synthetic recovery",Currency("CAD"),"2026-01-01");
        app::OperationContext operation;
        operation.progress=[&](std::string_view phase){if(phase=="Source evidence retained")operation.request();};
        QVERIFY_THROWS_EXCEPTION(app::OperationCancelled,gateway->repository().ingest_controlled("retained",bytes,qif::DateOrder::dmy,operation));
        const auto batches=gateway->repository().batches();QCOMPARE(batches.size(),std::size_t(1));
        QCOMPARE(batches.front().state,app::BatchState::captured);
        QVERIFY(!gateway->repository().projected_balance());
        gateway->close();
    }
private slots:
    void initTestCase() {
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("RecoveryUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    }
    void init(){QSettings().clear();}
    void reopenBackupAndExplicitResume() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";
        const QByteArray bytes="D1/1/2026\nT-10.00\nPSynthetic purchase\n^\nD2/1/2026\nT3.00\nPSynthetic credit\n^\n";
        retained(path,std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())));
        ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));run(model,"open",{{"path",path}});
        auto pending=model.state().value("pending").toMap();
        QCOMPARE(pending.value("count").toInt(),2);QVERIFY(pending.value("processingIncomplete").toBool());
        QVERIFY(!model.state().value("projectionAvailable").toBool());QCOMPARE(model.state().value("committed").toString(),"0.00");
        auto* notice=root->findChild<QObject*>("incompleteImportNotice");QVERIFY(notice);QVERIFY(notice->property("visible").toBool());
        QVERIFY(notice->property("text").toString().contains("2 parsed records"));
        QVERIFY(root->findChild<QObject*>("resumeImport")->property("visible").toBool());
        QVERIFY(!root->findChild<QObject*>("commitImport")->property("enabled").toBool());
        QVERIFY(!root->findChild<QObject*>("reviewRecord")->property("enabled").toBool());
        QVERIFY(click(root,"viewSourceFile"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());
        auto selected=model.state().value("selected").toMap();QCOMPARE(selected.value("source").toString(),QString::fromUtf8(bytes));
        QVERIFY(!selected.value("editable").toBool());QVERIFY(!selected.contains("draft"));
        QTest::qWait(100);
        if(qEnvironmentVariableIsSet("SPENTIT_RECOVERY_SCREENSHOT")) QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_RECOVERY_SCREENSHOT")));
        // A second selection of the same file finds retained evidence; only Resume processes it.
        QFile source(dir.path()+"/synthetic.qif");QVERIFY(source.open(QIODevice::WriteOnly));QCOMPARE(source.write(bytes),bytes.size());source.close();
        run(model,"import",{{"path",source.fileName()},{"profile","DMY"}});
        QCOMPARE(model.state().value("batches").toList().size(),1);
        QVERIFY(model.state().value("pending").toMap().value("processingIncomplete").toBool());QVERIFY(source.remove());
        const auto copy=dir.path()+"/backup";run(model,"backup",{{"path",copy}});run(model,"close");run(model,"open",{{"path",copy}});
        pending=model.state().value("pending").toMap();QVERIFY(pending.value("processingIncomplete").toBool());
        run(model,"resume",{{"batch",pending.value("id")},{"revision",pending.value("revision").toLongLong()-1}},false);
        QVERIFY(model.state().value("pending").toMap().value("processingIncomplete").toBool());
        QVERIFY(click(root,"resumeImport"));QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(!model.state().value("pending").toMap().value("processingIncomplete").toBool());
        QVERIFY(!root->findChild<QObject*>("resumeImport")->property("visible").toBool());
        QCOMPARE(model.state().value("projected").toString(),"-7.00");QCOMPARE(model.state().value("committed").toString(),"0.00");
        run(model,"close");run(model,"open",{{"path",path}});
        QVERIFY(model.state().value("pending").toMap().value("processingIncomplete").toBool());
        QCOMPARE(model.state().value("committed").toString(),"0.00");
    }
    void zeroRecordEvidenceCanBeDiscardedAndInspected() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";const std::string bytes="!Type:Bank\n";
        retained(path,bytes);
        ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        run(model,"open",{{"path",path}});
        QCOMPARE(model.state().value("pending").toMap().value("count").toInt(),0);
        QVERIFY(root->findChild<QObject*>("incompleteImportNotice")->property("text").toString().contains("0 parsed records"));
        QVERIFY(click(root,"discardImport"));
        auto* dialog=root->findChild<QObject*>("discardImportDialog");QVERIFY(dialog);QVERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog,"accepted"));QMetaObject::invokeMethod(dialog,"close");
        QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(model.state().value("pending").toMap().isEmpty());
        run(model,"close");run(model,"open",{{"path",path}});
        const auto batches=model.state().value("batches").toList();QCOMPARE(batches.size(),1);QCOMPARE(batches.front().toMap().value("status").toString(),"Discarded");
        QVERIFY(!root->findChild<QObject*>("resumeImport")->property("visible").toBool());
        QVERIFY(click(root,"viewSourceFile"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());
        QCOMPARE(model.state().value("selected").toMap().value("source").toString(),QString::fromStdString(bytes));
        QCOMPARE(model.state().value("committed").toString(),"0.00");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    RecoveryUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_recovery_tests.moc"
