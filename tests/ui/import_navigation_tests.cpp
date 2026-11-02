#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QJSValue>
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
        auto request=args;if(action=="import" && !request.contains("account"))request["account"]=model.state().value("accountId");model.command(action,request);
        const auto id=spentit::diagnostics::Collector::instance().snapshot().active.id;
        if(model.diagnosticsEnabled())QTRY_VERIFY_WITH_TIMEOUT(spentit::diagnostics::Collector::instance().snapshot().latest.id==id && spentit::diagnostics::Collector::instance().snapshot().latest.delivered!=0,30000);
        else QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
    }
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}
    static QObject* object(QObject* root,const char* name){if(auto* found=root->findChild<QObject*>(QString::fromLatin1(name)))return found;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),QString::fromLatin1(name)):nullptr;}
    static bool click(QObject* root,const char* name) {
        auto* item=object(root,name);
        return item && QMetaObject::invokeMethod(item,"clicked");
    }
    static bool edit(QObject* root,const char* name,const QString& value) {
        auto* item=object(root,name);
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
    void categorySubmenuScrolls() {
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);window->show();QVERIFY(QTest::qWaitForWindowExposed(window));
        QVariantList categories;for(int i=0;i<80;++i)categories.push_back(QVariantMap{{"id",QString::number(i)},{"path",QString("Category %1").arg(i,2,10,QChar('0'))},{"assignable",true}});
        root->setProperty("s",QVariantMap{{"active",true},{"selectionCount",1},{"transfersSupported",true},{"categories",categories}});
        auto* context=object(root,"ledgerContext");QVERIFY(context);QVERIFY(QMetaObject::invokeMethod(context,"popup"));QTRY_VERIFY(context->property("visible").toBool());
        auto* menu=object(root,"ledgerCategoryMenu");QVERIFY(menu);
        // Hover the parent menu entry, as the owner requested.
        auto* content=qvariant_cast<QQuickItem*>(context->property("contentItem"));QVERIFY(content);
        QQuickItem* entry=nullptr;for(auto* item:content->childItems())for(auto* child:item->childItems())if(child->property("text").toString()=="Change category")entry=child;
        QVERIFY(entry);QTest::qWait(100);QTest::mouseMove(window,entry->mapToScene(QPointF(entry->width()/2,entry->height()/2)).toPoint());
        QTRY_VERIFY(menu->property("visible").toBool());QVERIFY(menu->property("height").toReal()<=420);
        auto* bar=object(root,"ledgerCategoryScrollBar");QVERIFY(bar);QVERIFY(bar->property("size").toReal()<1);QVERIFY(bar->property("interactive").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_MENU_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_MENU_SCREENSHOT")));
    }
    void categoryMenuSavesMixedRowsAndReopens() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.filePath("document");
        run(model,"create",{{"path",path},{"protected",false},{"name","Category account"},{"currency","CAD"}});
        auto import=[&](QString name){QFile f(dir.filePath(name+".qif"));if(!f.open(QIODevice::WriteOnly))return QString();f.write(("D1/1/2026\nT-1\nP"+name+"\nLBefore\n^\n").toUtf8());f.close();run(model,"import",{{"path",f.fileName()},{"profile","DMY"}});return model.state().value("selected").toMap().value("batch").toString();};
        const auto accepted=import("Accepted");QVERIFY(!accepted.isEmpty());run(model,"commit");
        const auto pending=import("Pending");QVERIFY(!pending.isEmpty());
        QMap<QString,QString> original;for(const auto& batch:{accepted,pending})original[batch]=model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString();
        run(model,"select",{{"row",model.occurrenceIndex(accepted,0)}});run(model,"select",{{"row",model.occurrenceIndex(pending,0)},{"toggle",true}});
        auto* menu=object(root,"ledgerCategoryMenu");QVERIFY(menu);QVERIFY(menu->property("enabled").toBool());
        auto* context=object(root,"ledgerContext");QVERIFY(context);QVERIFY(QMetaObject::invokeMethod(context,"popup"));QTRY_VERIFY(context->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu,"open"));QTRY_VERIFY(menu->property("visible").toBool());
        auto* choice=object(root,"ledgerCategoryChoice_0");QVERIFY(choice);
        QVERIFY(QMetaObject::invokeMethod(choice,"triggered"));QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        for(const auto& batch:{accepted,pending})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),QString("Uncategorized"));
        QCOMPARE(model.state().value("selectionCount").toInt(),2);
        run(model,"close");run(model,"open",{{"path",path}});
        for(const auto& batch:{accepted,pending})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),QString("Uncategorized"));
        run(model,"undo_transfer_assignment");
        for(const auto& batch:{accepted,pending})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),original[batch]);
    }
    void ledgerViewportSurvivesCategoryNavigation() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Viewport account"},{"currency","CAD"}});
        QFile file(dir.filePath("rows.qif"));QVERIFY(file.open(QIODevice::WriteOnly));QByteArray rows;
        for(int i=0;i<300;++i)rows+="D1/1/2026\nT-1\nPSynthetic "+QByteArray::number(i).rightJustified(3,'0')+"\n^\n";
        QCOMPARE(file.write(rows),rows.size());file.close();run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto account=model.state().value("accountId").toString();const auto batch=model.state().value("selected").toMap().value("batch").toString();
        run(model,"inspect",{{"batch",batch},{"ordinal",260}});
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto position=[&](int index){return QMetaObject::invokeMethod(ledger,"positionViewAtIndex",Q_ARG(int,index),Q_ARG(int,0));};
        auto snapshot=[&](){QMetaObject::invokeMethod(root,"rememberAccountViewport");return root->property("accountViewports").value<QJSValue>().toVariant().toMap().value(model.state().value("accountId").toString()).toMap();};
        auto first=[&](){int index=-1;QMetaObject::invokeMethod(ledger,"indexAt",Q_RETURN_ARG(int,index),Q_ARG(qreal,1),Q_ARG(qreal,ledger->property("contentY").toReal()+1));return index;};
        QTest::qWait(50);QVERIFY(position(model.occurrenceIndex(batch,260)));QTest::qWait(50);
        ledger->setProperty("contentY",ledger->property("contentY").toReal()+17);const auto saved=snapshot();QVERIFY(saved.value("index").toInt()>200);
        const auto selected=model.state().value("selected").toMap();
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());QCOMPARE(root->property("currentPage").toString(),"categories");
        QVERIFY(click(root,"newCategory"));QVERIFY(edit(root,"categoryName","New expense"));QVERIFY(QMetaObject::invokeMethod(root,"finishCategoryEdit"));
        QTRY_VERIFY(!model.busy() && !root->property("categoryDirty").toBool());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(click(root,"navigationBack"));QCOMPARE(root->property("currentPage").toString(),"ledger");
        QTRY_COMPARE(first(),saved.value("index").toInt());
        QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        QCOMPARE(model.state().value("selected").toMap().value("batch"),selected.value("batch"));QCOMPARE(model.state().value("selected").toMap().value("ordinal"),selected.value("ordinal"));
        QVERIFY(!root->property("editorClosed").toBool());
        // A category picked for a new transaction saves immediately, without
        // the old committed-only bulk preview or a jump to the top.
        run(model,"select",{{"row",model.occurrenceIndex(batch,260)}});
        QVERIFY(position(model.occurrenceIndex(batch,250)));QTest::qWait(50);
        const auto categoryView=snapshot();
        const auto categories=model.state().value("categories").toList();QVERIFY(!categories.isEmpty());
        QVariantMap category;for(const auto& c:categories)if(c.toMap().value("path")=="New expense")category=c.toMap();QVERIFY(!category.isEmpty());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseLedgerCategory",Q_ARG(QVariant,category)));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,260)).value("category").toString(),QString("New expense"));
        QTRY_COMPARE(first(),categoryView.value("index").toInt());
        QVERIFY(!object(root,"bulkCategoryDialog")->property("visible").toBool());
        QVERIFY(!object(root,"bulkDialog")->property("visible").toBool());
        QCOMPARE(model.state().value("selectionCount").toInt(),1);
        run(model,"undo_transfer_assignment");
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,260)).value("category").toString(),QString("Uncategorized"));
        run(model,"inspect",{{"batch",batch},{"ordinal",260}});QTest::qWait(50);
        QVERIFY(position(saved.value("index").toInt()));QTest::qWait(50);
        ledger->setProperty("contentY",ledger->property("contentY").toReal()+saved.value("offset").toReal());
        // Repeated visits must not overwrite the saved position from the hidden,
        // reset list; returning through the account button uses the same anchor.
        QVERIFY(click(root,"navigationForward"));QTRY_VERIFY(!model.busy());run(model,"category_list");
        QVERIFY(click(root,"accountNavigation"));QTRY_COMPARE(first(),saved.value("index").toInt());
        QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        // Keep independent positions when switching accounts, including while
        // leaving a non-ledger page, and through cross-account Back/Forward.
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());
        run(model,"create_account",{{"id","2"},{"name","Second viewport"},{"currency","CAD"},{"date","2026-01-01"}});
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto secondBatch=model.state().value("selected").toMap().value("batch").toString();
        run(model,"inspect",{{"batch",secondBatch},{"ordinal",40}});QTest::qWait(50);QVERIFY(position(model.occurrenceIndex(secondBatch,40)));QTest::qWait(50);
        const auto second=snapshot();
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,account)));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,QString("2"))));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),second.value("index").toInt());
        QVERIFY(click(root,"navigationBack"));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        QVERIFY(click(root,"navigationForward"));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),second.value("index").toInt());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,account)));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        // A vanished anchor and a shorter filtered ledger clamp to a valid end.
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());
        run(model,"query",{{"filter","Synthetic 00"},{"sort",0},{"descending",false}});
        QVERIFY(click(root,"navigationBack"));QTRY_VERIFY(first()>=0 && first()<20);
        QTRY_VERIFY(ledger->property("contentY").toReal()<=ledger->property("originY").toReal()+qMax(qreal(0),ledger->property("contentHeight").toReal()-ledger->height())+1);
        QCOMPARE(model.state().value("filter").toString(),"Synthetic 00");
        run(model,"close");QVERIFY(root->property("accountViewports").value<QJSValue>().toVariant().toMap().isEmpty());
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
        QVERIFY(click(root,"sidebarToggle"));QVERIFY(root->property("sidebarVisible").toBool());
        QVERIFY(click(root,"sidebarSettings"));QCOMPARE(root->property("currentPage").toString(),"settings");
        QCOMPARE(model.rowIdentity(model.selectedRowIndex()),identity);
        QVERIFY(click(root,"navigationBack"));QCOMPARE(root->property("currentPage").toString(),"ledger");
        QVERIFY(click(root,"navigationForward"));QCOMPARE(root->property("currentPage").toString(),"settings");
        QVERIFY(click(root,"navigationBack"));
        window->resize(1100,750);QTest::qWait(100);
        auto* sidebar=root->findChild<QQuickItem*>("accountSidebar");QVERIFY(sidebar);
        QVERIFY(sidebar->width()>=140);QVERIFY(sidebar->width()<=360);
        auto* settingsButton=root->findChild<QQuickItem*>("sidebarSettings");QVERIFY(settingsButton);
        const auto settingsCenter=settingsButton->mapToScene(QPointF(settingsButton->width()/2,settingsButton->height()/2));
        QVERIFY(settingsCenter.x()>0 && settingsCenter.x()<window->width());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,settingsCenter.toPoint());
        QTRY_COMPARE(root->property("currentPage").toString(),"settings");
        QVERIFY(click(root,"navigationBack"));
        window->resize(1440,850);QTest::qWait(100);
        QVERIFY(edit(root,"durablePayee","Unsaved"));QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"sidebarSettings"));
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(click(root,"guardStay"));QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(edit(root,"durableTotal","invalid"));QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardSave"));
        QTRY_VERIFY(!model.busy());QVERIFY(!model.error().isEmpty());
        QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardDiscard"));
        QCOMPARE(root->property("currentPage").toString(),"settings");QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Original");
        QVERIFY(click(root,"navigationBack"));
        QVERIFY(edit(root,"durablePayee","Saved navigation"));QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardSave"));
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
