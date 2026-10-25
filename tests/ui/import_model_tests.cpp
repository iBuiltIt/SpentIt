#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFont>
#include <sqlite3.h>
using namespace spentit;
class ImportUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QVariantMap selected(const ImportModel& m) { return m.state().value("selected").toMap(); }
    static QString fixture(const QString& dir,const QByteArray& bytes) {
        const auto path=dir+"/synthetic.qif";QFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())qFatal("Fixture failed");return path;
    }
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {
        model.command(action,args);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty()==success,qPrintable(action+": "+model.error()));
    }
    void create(ImportModel& m,const QString& path) {
        run(m,"create",{{"path",path},{"protected",false},{"name","Synthetic test"},{"currency","CAD"}});
    }
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("ImportUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    }
    void init() {QSettings().clear();}
    void durableWorkflow() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";
        QFile walkthrough(QString::fromUtf8(SPENTIT_REVIEW_FIXTURE));QVERIFY(walkthrough.open(QIODevice::ReadOnly));
        const auto source=fixture(dir.path(),walkthrough.readAll());
        ImportModel m(storage::document_gateway());create(m,path);
        run(m,"opening",{{"date","2026-08-01"},{"amount","100.00"}});
        run(m,"import",{{"path",source},{"profile","DMY"}});
        QCOMPARE(m.state().value("committed").toString(),"100.00");QCOMPARE(m.state().value("projected").toString(),"120.69");
        QCOMPARE(selected(m).value("draft").toMap().value("date").toString(),"2026-08-10");
        auto edit=selected(m).value("draft").toMap(); edit["total"]="-30.00";
        run(m,"save",edit,false);QCOMPARE(m.state().value("projected").toString(),"120.69");
        auto splits=edit.value("splits").toList();auto second=splits[1].toMap();second["amount"]="-29.86";splits[1]=second;edit["splits"]=splits;
        run(m,"save",edit);QCOMPARE(m.state().value("projected").toString(),"120.00");
        const auto batch=selected(m).value("batch");
        run(m,"backup",{{"path",dir.path()+"/backup"}});
        run(m,"close");QCOMPARE(m.rowCount(),0);QVERIFY(selected(m).isEmpty());
        run(m,"open",{{"path",path}});QCOMPARE(m.state().value("projected").toString(),"120.00");
        run(m,"inspect",{{"batch",batch},{"ordinal",0}});QCOMPARE(selected(m).value("original").toString(),"-29.31");
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");QCOMPARE(m.state().value("receipt").toMap().value("change").toString(),"20.00");
        QCOMPARE(m.state().value("receipt").toMap().value("included").toInt(),2);
        QCOMPARE(m.state().value("receipt").toMap().value("excluded").toInt(),0);
        QVERIFY(m.state().value("frozen").toBool());
        run(m,"opening",{{"date","2026-08-01"},{"amount","200.00"}},false);
        QCOMPARE(m.state().value("committed").toString(),"120.00");
        run(m,"close");run(m,"open",{{"path",path}});
        QCOMPARE(m.state().value("receipt").toMap().value("change").toString(),"20.00");
        run(m,"import",{{"path",source},{"profile","DMY"}});QCOMPARE(selected(m).value("decision").toInt(),2);
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");
        QCOMPARE(m.state().value("receipt").toMap().value("included").toInt(),0);
        QCOMPARE(m.state().value("receipt").toMap().value("excluded").toInt(),2);
        run(m,"query",{{"filter","Synthetic transit"},{"sort",1},{"descending",true},{"projected",false}});
        QCOMPARE(m.rowCount(),1);QCOMPARE(m.state().value("withdrawals").toString(),"-30.00");
        QCOMPARE(m.state().value("deposits").toString(),"0.00");
        QCOMPARE(m.data(m.index(0),Qt::UserRole).toMap().value("balance").toString(),"70.00");
        run(m,"close");run(m,"open",{{"path",dir.path()+"/backup"}});QCOMPARE(m.state().value("committed").toString(),"100.00");QCOMPARE(m.state().value("projected").toString(),"120.00");
        run(m,"backup",{{"path",dir.path()+"/restored"}});
        run(m,"close");run(m,"open",{{"path",dir.path()+"/restored"}});
        QCOMPARE(m.state().value("projected").toString(),"120.00");
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");
    }
    void recordRepair() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());create(m,dir.path()+"/doc");
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D31/02/2026\nT-10\nPDate repair\n^\nD1/3/2026\nT-20\nSFood\n$-19\n^\n")},{"profile","DMY"}});
        QVERIFY(selected(m).value("repair").toBool());QVERIFY(selected(m).value("editable").toBool());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));QTest::qWait(100);
        if(qEnvironmentVariableIsSet("SPENTIT_REPAIR_SCREENSHOT")) QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REPAIR_SCREENSHOT")));
        auto edit=selected(m).value("draft").toMap();edit["date"]="2026-02-28";
        run(m,"save",edit);QVERIFY(!selected(m).value("repair").toBool());
        QVERIFY(!selected(m).value("diagnostic").toString().isEmpty());
        run(m,"decide");QVERIFY(!m.state().value("projectionAvailable").toBool());
        const auto batch=selected(m).value("batch");run(m,"inspect",{{"batch",batch},{"ordinal",1}});
        QVERIFY(selected(m).value("repair").toBool());edit=selected(m).value("draft").toMap();
        run(m,"save",edit,false);QVERIFY(selected(m).value("repair").toBool());
        auto splits=edit.value("splits").toList();auto split=splits[0].toMap();split["amount"]="-20.00";splits[0]=split;edit["splits"]=splits;
        run(m,"save",edit);QVERIFY(!m.state().value("projectionAvailable").toBool());
        run(m,"commit",{},false);run(m,"decide");QCOMPARE(m.state().value("projected").toString(),"70.00");
        run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});
        run(m,"inspect",{{"batch",batch},{"ordinal",1}});QVERIFY(!selected(m).value("diagnostic").toString().isEmpty());
        QVERIFY(selected(m).value("source").toString().contains("$-19"));
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"70.00");
    }
    void errorsAndIsolation() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        const auto path=dir.path()+"/protected";
        run(m,"create",{{"path",path},{"protected",true},{"password","test-password"},{"confirmation","different"},{"name","Synthetic"},{"currency","CAD"}},false);
        QVERIFY(!QFile::exists(path));
        run(m,"create",{{"path",path},{"protected",true},{"password","test-password"},{"confirmation","test-password"},{"name","Synthetic"},{"currency","CAD"}});
        run(m,"close");run(m,"open",{{"path",path}},false);QVERIFY(m.state().value("unlockRequired").toBool());
        run(m,"unlock",{{"path",path},{"password","wrong"}},false);QVERIFY(m.state().value("unlockRequired").toBool());
        run(m,"unlock",{{"path",path},{"password","test-password"}});
        ImportModel other(storage::document_gateway());run(other,"open",{{"path",path}},false);QVERIFY(!other.state().value("unlockRequired").toBool());
        run(m,"close");run(m,"open",{{"path",dir.path()+"/missing"}},false);QVERIFY(!m.state().value("unlockRequired").toBool());QVERIFY(!QFile::exists(dir.path()+"/missing"));
        create(m,dir.path()+"/new");QCOMPARE(m.state().value("committed").toString(),"0.00");QVERIFY(selected(m).isEmpty());
        const auto invalid=fixture(dir.path(),"D1/1/2026\nT1\nL[Other]\n^\n");run(m,"import",{{"path",invalid},{"profile","DMY"}});
        QVERIFY(!m.state().value("projectionAvailable").toBool());QCOMPARE(m.rowCount(),0);run(m,"commit",{},false);
        QVERIFY(!selected(m).value("diagnostic").toString().isEmpty());run(m,"discard");QCOMPARE(m.state().value("committed").toString(),"0.00");
        run(m,"import",{{"path",fixture(dir.path(),QByteArray(1,static_cast<char>(0xff)))},{"profile","DMY"}});
        QVERIFY(selected(m).value("source").toString().contains("hex"));QVERIFY(!selected(m).contains("draft"));run(m,"discard");
        QFile oversized(dir.path()+"/oversized.qif");QVERIFY(oversized.open(QIODevice::WriteOnly));QVERIFY(oversized.resize(static_cast<qint64>(qif::max_file)+1));oversized.close();
        run(m,"import",{{"path",oversized.fileName()},{"profile","DMY"}},false);QVERIFY(!m.state().contains("pending"));
    }
    void publicRecoveryAndSeparation() {
        QTemporaryDir dir;auto g=storage::document_gateway();const auto path=(dir.path()+"/public").toStdString();
        g->create(path,false,"","Synthetic",Currency("CAD"),"2026-10-25");g->repository().opening(0,"2026-10-25",Money(123,Currency("CAD")));
        g->backup((dir.path()+"/portable").toStdString());g->close();g.reset();QSettings().clear();
        // Independent SQLCipher C interface: no application factory/store or Settings.
        sqlite3* raw=nullptr;
        const auto copy=QFileInfo(dir.path()+"/portable").canonicalFilePath().toUtf8();
        QCOMPARE(sqlite3_open_v2(copy.constData(),&raw,SQLITE_OPEN_READWRITE,nullptr),SQLITE_OK);
        QCOMPARE(sqlite3_key(raw,storage::public_document_key.data(),static_cast<int>(storage::public_document_key.size())),SQLITE_OK);
        QCOMPARE(sqlite3_exec(raw,"PRAGMA cipher_compatibility=4",nullptr,nullptr,nullptr),SQLITE_OK);
        sqlite3_stmt* query=nullptr;QCOMPARE(sqlite3_prepare_v2(raw,"SELECT opening FROM account",-1,&query,nullptr),SQLITE_OK);
        QCOMPARE(sqlite3_step(query),SQLITE_ROW);QCOMPARE(sqlite3_column_int64(query,0),123);sqlite3_finalize(query);
        QCOMPARE(sqlite3_exec(raw,"PRAGMA user_version=99",nullptr,nullptr,nullptr),SQLITE_OK);QCOMPARE(sqlite3_close(raw),SQLITE_OK);
        ImportModel unsupported(storage::document_gateway());run(unsupported,"open",{{"path",dir.path()+"/portable"}},false);
        QVERIFY(!unsupported.state().value("unlockRequired").toBool());QVERIFY(unsupported.error().contains("Unsupported document schema"));
        g=storage::document_gateway();
        g->create((dir.path()+"/protected").toStdString(),true,storage::public_document_key,"Synthetic",Currency("CAD"),"2026-10-25");g->close();
        QVERIFY_THROWS_EXCEPTION(app::UnlockRequired,g->open((dir.path()+"/protected").toStdString(),{}));
        g->open((dir.path()+"/protected").toStdString(),std::string(storage::public_document_key));g->close();
    }
    void qmlEditorNavigation() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        create(m,dir.path()+"/doc");
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-1\nPSynthetic\n^\n")},{"profile","DMY"}});
        QVERIFY(root->property("selected").toMap().contains("draft"));
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        payee->setProperty("text","Edited through QML");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(root->property("dirty").toBool());
        QVERIFY(!window->close());QVERIFY(window->isVisible());QVERIFY(m.state().value("active").toBool());
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        auto* stay=root->findChild<QObject*>("guardStay");QVERIFY(stay);QVERIFY(QMetaObject::invokeMethod(stay,"clicked"));
        QVERIFY(root->property("dirty").toBool());QVERIFY(!dialog->property("visible").toBool());
        auto* total=root->findChild<QQuickItem*>("durableTotal");QVERIFY(total);total->setProperty("text","invalid");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        QVERIFY(!window->close());auto* save=root->findChild<QObject*>("guardSave");QVERIFY(save);QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());QVERIFY(root->property("dirty").toBool());QVERIFY(m.state().value("active").toBool());
        total->setProperty("text","-1.00");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        auto* saveDraft=root->findChild<QObject*>("saveDraft");QVERIFY(saveDraft);QVERIFY(QMetaObject::invokeMethod(saveDraft,"clicked"));
        QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
        QTest::qWait(100);
        const auto image=window->grabWindow();QVERIFY(!image.isNull());
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_SCREENSHOT")) QVERIFY(image.save(qEnvironmentVariable("SPENTIT_TEST_SCREENSHOT")));
        payee->setProperty("text","Discard me");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(!window->close());auto* discard=root->findChild<QObject*>("guardDiscard");QVERIFY(discard);QVERIFY(QMetaObject::invokeMethod(discard,"clicked"));
        QTRY_VERIFY(!m.busy());QTRY_VERIFY(!window->isVisible());QVERIFY(!m.state().value("active").toBool());
        run(m,"open",{{"path",dir.path()+"/doc"}});
        const auto batch=m.state().value("pending").toMap().value("id");run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    ImportUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_model_tests.moc"
