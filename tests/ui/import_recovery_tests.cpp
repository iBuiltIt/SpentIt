#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "controls_style.hpp"
#include <QFont>
using namespace spentit;

class RecoveryUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {
        auto request=args;if(action=="import" && !request.contains("account"))request["account"]=model.state().value("accountId");model.command(action,request);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty()==success,qPrintable(action+": "+model.error()));
    }
    void retained(const QString& path,std::string_view bytes) {
        auto gateway=storage::document_gateway();
        gateway->create(path.toStdString(),false,"","Synthetic recovery",Currency("CAD"),"2026-01-01");
        app::OperationContext operation;
        operation.progress=[&](std::string_view phase){if(phase=="Import saved")throw std::runtime_error("Injected interruption after capture");};
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,gateway->repository().ingest_controlled("retained",bytes,qif::DateOrder::dmy,operation));
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
    void reopenCleansOnlyIncompleteImport() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";
        const QByteArray bytes="D1/1/2026\nT-10.00\nPSynthetic purchase\n^\nD2/1/2026\nT3.00\nPSynthetic credit\n^\n";
        retained(path,std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())));
        ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        run(model,"open",{{"path",path}});
        QVERIFY(model.state().value("startupImportCleanup").toBool());QVERIFY(model.state().value("batches").toList().isEmpty());
        QVERIFY(root->property("workspaceFeedback").toString().contains("incomplete import has been removed"));
        QVERIFY(!root->findChild<QObject*>("resumeImport"));QCOMPARE(model.state().value("balance").toString(),"0.00");
        QFile source(dir.path()+"/synthetic.qif");QVERIFY(source.open(QIODevice::WriteOnly));QCOMPARE(source.write(bytes),bytes.size());source.close();
        run(model,"import",{{"path",source.fileName()},{"profile","DMY"}});QCOMPARE(model.state().value("balance").toString(),"-7.00");
        QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),bytes);source.close();
        const auto copy=dir.path()+"/backup";run(model,"backup",{{"path",copy}});run(model,"close");run(model,"open",{{"path",copy}});
        QVERIFY(!model.state().value("startupImportCleanup").toBool());QCOMPARE(model.state().value("newTransactions").toInt(),2);QCOMPARE(model.state().value("balance").toString(),"-7.00");
    }
    void zeroRecordCaptureIsRemoved() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";retained(path,"!Type:Bank\n");
        ImportModel model(storage::document_gateway());run(model,"open",{{"path",path}});
        QVERIFY(model.state().value("batches").toList().isEmpty());QVERIFY(model.state().value("startupImportCleanup").toBool());
        run(model,"close");run(model,"open",{{"path",path}});
        QVERIFY(!model.state().value("startupImportCleanup").toBool());QCOMPARE(model.state().value("balance").toString(),"0.00");
    }

};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));applySpentItControlsStyle();
    RecoveryUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_recovery_tests.moc"
