#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QGuiApplication>
using namespace spentit;
class ReportUiTests:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    void run(ImportModel& m,const QString& action,const QVariantMap& args={}){m.command(action,args);QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));}
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}
    static QObject* object(QObject* root,const char* name){if(auto* found=root->findChild<QObject*>(QString::fromLatin1(name)))return found;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),QString::fromLatin1(name)):nullptr;}
    static bool click(QObject* root,const char* name){auto* item=object(root,name);return item && QMetaObject::invokeMethod(item,"clicked");}
    static QVariant value(const QVariant& v){return v.metaType()==QMetaType::fromType<QJSValue>()?v.value<QJSValue>().toVariant():v;}
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("SpentItReportsTests");QCoreApplication::setApplicationName("Reports");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());}
    void reportsAndCalendar(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Synthetic checking"},{"currency","CAD"}});
        QFile source(dir.filePath("report.qif"));QVERIFY(source.open(QIODevice::WriteOnly));source.write("D2026-01-01\nT-100\nPSynthetic purchase\n^\nD2026-01-02\nT20\nPSynthetic refund\n^\n");source.close();
        run(model,"import",{{"path",source.fileName()},{"account",model.state().value("accountId")}});
        QCOMPARE(model.state().value("newTransactions").toLongLong(),2);
        QVERIFY(click(root,"reportsNavigation"));QTRY_VERIFY(!model.busy());QCOMPARE(root->property("currentPage").toString(),"reports");
        auto* workspace=object(root,"reportsWorkspace");QVERIFY(workspace);QVERIFY(workspace->property("workflow").value<QObject*>()==&model);
        QVERIFY(click(root,"newReport"));QVERIFY(workspace->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(workspace,"setField",Q_ARG(QVariant,QVariant("name")),Q_ARG(QVariant,QVariant("Synthetic spending"))));
        // A typed boundary switches the range to Custom and preserves the other boundary.
        QTRY_VERIFY(object(root,"reportStartDate0"));auto* reportStart=object(root,"reportStartDate0");
        QVERIFY(reportStart->setProperty("text","2026-01-01"));QVERIFY(QMetaObject::invokeMethod(reportStart,"textEdited"));
        QCOMPARE(value(workspace->property("current")).toMap().value("periods").toList().front().toMap().value("preset").toString(),"custom");
        QVERIFY(click(root,"showReport"));QTRY_VERIFY_WITH_TIMEOUT(!model.busy() && workspace->property("mode").toString()=="result",30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        auto result=model.state().value("reportResult").toMap();auto currency=result.value("currencies").toList().front().toMap();QCOMPARE(currency.value("totals").toList().front().toString(),"80.00");
        run(model,"report_drill",{{"currency",0},{"period",0},{"row",-1}});QCOMPARE(model.state().value("reportDetails").toMap().value("rows").toList().size(),2);
        QVERIFY(QMetaObject::invokeMethod(workspace,"back"));QVERIFY(QMetaObject::invokeMethod(workspace,"back"));
        const auto id=model.state().value("reports").toList().front().toMap().value("id");
        QVERIFY(click(root,"newReport"));QVERIFY(QMetaObject::invokeMethod(workspace,"setField",Q_ARG(QVariant,QVariant("name")),Q_ARG(QVariant,QVariant(""))));
        QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,QVariant("ledger"))));QVERIFY(workspace->property("leaving").toBool());QVERIFY(QMetaObject::invokeMethod(workspace,"stay"));QCOMPARE(root->property("currentPage").toString(),"reports");
        // Save selected existing report while the unrelated incomplete draft stays.
        QVERIFY(workspace->setProperty("selectedIndex",0));QVERIFY(QMetaObject::invokeMethod(workspace,"setField",Q_ARG(QVariant,QVariant("name")),Q_ARG(QVariant,QVariant("Renamed"))));
        QVERIFY(QMetaObject::invokeMethod(workspace,"save",Q_ARG(QVariant,QVariant(true)),Q_ARG(QVariant,QVariant(false))));QTRY_VERIFY(!model.busy());QVERIFY(!model.error().isEmpty());
        QCOMPARE(model.state().value("reports").toList().front().toMap().value("name").toString(),"Synthetic spending");
        QVERIFY(click(root,"saveReport"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());QVERIFY(workspace->property("dirty").toBool());
        QCOMPARE(model.state().value("reports").toList().size(),1);QCOMPARE(model.state().value("reports").toList().front().toMap().value("name").toString(),"Renamed");
        QVERIFY(QMetaObject::invokeMethod(workspace,"load"));QVERIFY(!workspace->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,QVariant("ledger"))));QTRY_VERIFY(!model.busy());
        run(model,"begin_manual");auto* field=object(root,"durableDate");auto* calendar=object(root,"transactionCalendar");QVERIFY(field && calendar);QVERIFY(field->setProperty("text","2024-01-31"));
        const auto before=model.state().value("newTransactions").toLongLong();
        QVERIFY(QMetaObject::invokeMethod(calendar,"clicked"));QCOMPARE(calendar->property("cursor").toString(),"2024-01-31");
        QVERIFY(QMetaObject::invokeMethod(calendar,"browse",Q_ARG(QVariant,QVariant("2024-02-29"))));QCOMPARE(field->property("text").toString(),"2024-01-31");
        QVERIFY(QMetaObject::invokeMethod(calendar,"choose",Q_ARG(QVariant,QVariant("2024-02-29"))));QCOMPARE(field->property("text").toString(),"2024-02-29");QVERIFY(root->property("dirty").toBool());QVERIFY(!model.busy());QCOMPARE(model.state().value("newTransactions").toLongLong(),before);
        QCOMPARE(model.calendarMove("2024-01-31",0,1),"2024-02-29");QCOMPARE(model.calendarMove("9999-12-31",1),"9999-12-31");QCOMPARE(model.calendarMove("0001-01-01",-1),"0001-01-01");
        QVERIFY(model.calendarPage("2026-02-29").contains("error"));
        // Keyboard selection changes the draft only; Return must not reach Save.
        QTRY_VERIFY(!calendar->findChild<QObject*>("calendarPopup")->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(calendar,"clicked"));QTRY_VERIFY(calendar->findChild<QObject*>("calendarKeys")->property("activeFocus").toBool());
        auto* popup=calendar->findChild<QObject*>("calendarPopup");QVERIFY(popup);QVERIFY(popup->property("x").toReal()>=0 && popup->property("y").toReal()>=0);
        QVERIFY(popup->property("x").toReal()+popup->property("width").toReal()<=window->width());QVERIFY(popup->property("y").toReal()+popup->property("height").toReal()<=window->height());
        QTest::qWait(50);window->grabWindow().save("/tmp/spentit-calendar.png");
        QTest::keyClick(window,Qt::Key_Right);QTest::keyClick(window,Qt::Key_Return);QCOMPARE(field->property("text").toString(),"2024-03-01");QVERIFY(!model.busy());QCOMPARE(model.state().value("newTransactions").toLongLong(),before);
        // Escape backs out of year/month selection before closing, without editing.
        QVERIFY(QMetaObject::invokeMethod(calendar,"clicked"));calendar->setProperty("view",2);QTRY_VERIFY(calendar->findChild<QObject*>("calendarKeys")->property("activeFocus").toBool());QTest::keyClick(window,Qt::Key_Escape);QCOMPARE(calendar->property("view").toInt(),0);QTest::keyClick(window,Qt::Key_Escape);QCOMPARE(field->property("text").toString(),"2024-03-01");QVERIFY(root->property("dirty").toBool());
        run(model,"cancel_manual");run(model,"report_show",{{"id",id}});QVERIFY(!model.state().value("reportResult").toMap().value("stale").toBool());
        run(model,"correct_opening",{{"date","2026-01-01"},{"amount","100"}});QVERIFY(model.state().value("reportResult").toMap().value("stale").toBool());
        run(model,"close");run(model,"open",{{"path",dir.filePath("document")}});QCOMPARE(model.state().value("reports").toList().size(),1);
        run(model,"report_show",{{"id",id}});QCOMPARE(model.state().value("reportResult").toMap().value("currencies").toList().front().toMap().value("totals").toList().front().toString(),"80.00");
        // Multiple separate charts, including negative pie handling.
        auto definitions=model.state().value("reports").toList();auto definition=definitions[0].toMap();definition["graph"]="pie";
        auto periods=definition.value("periods").toList();periods.push_back(QVariantMap{{"preset","custom"},{"start","2026-01-02"},{"end","2026-01-02"}});definition["periods"]=periods;definitions[0]=definition;
        run(model,"report_save",{{"revision",model.state().value("reportRevision")},{"reports",definitions}});run(model,"report_show",{{"id",id}});
        auto totals=model.state().value("reportResult").toMap().value("currencies").toList().front().toMap().value("totals").toList();QCOMPARE(totals.size(),2);QCOMPARE(totals[1].toString(),"-20.00");
        // Capture the real QML result using only synthetic transactions.
        root->setProperty("currentPage","reports");workspace->setProperty("mode","result");window->resize(1200,800);window->show();QTest::qWait(150);window->grabWindow().save("/tmp/spentit-report-result.png");
        workspace->setProperty("mode","workspace");QVERIFY(QMetaObject::invokeMethod(workspace,"load"));QTest::qWait(100);window->grabWindow().save("/tmp/spentit-report-workspace.png");
        for(const auto& warning:warnings){const auto list=qvariant_cast<QList<QQmlError>>(warning.front());for(const auto& item:list)qWarning().noquote()<<item.toString();}
        QCOMPARE(warnings.size(),qsizetype(0));
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");ReportUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "report_ui_tests.moc"
