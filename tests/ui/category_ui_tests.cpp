#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFont>
using namespace spentit;
class CategoryUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}
    static QObject* object(QObject* root,const QString& name){if(auto* item=root->findChild<QObject*>(name))return item;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),name):nullptr;}
    static bool click(QObject* root,const QString& name){auto* item=object(root,name);return item && item->property("enabled").toBool() && item->property("visible").toBool() && QMetaObject::invokeMethod(item,"clicked");}
    static bool edit(QObject* root,const QString& name,const QString& value){auto* item=object(root,name);return item && item->setProperty("text",value) && QMetaObject::invokeMethod(item,"textEdited");}
    static bool pointer(QQuickWindow* window,QObject* root,const QString& name){auto* item=qobject_cast<QQuickItem*>(object(root,name));if(!item || !item->isEnabled() || !item->isVisible())return false;const auto p=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();if(!QRect(0,0,window->width(),window->height()).contains(p))return false;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p);return true;}
    void wait(ImportModel& m,bool success=true){QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty()==success,qPrintable(m.error()));}
    void run(ImportModel& m,const QString& action,const QVariantMap& args={},bool success=true){m.command(action,args);wait(m,success);}
    static QVariantMap selected(const ImportModel& m){return m.state().value("selected").toMap();}
    void applyCategory(ImportModel& m,QObject* root){QVERIFY(click(root,"saveCategory"));wait(m);QVERIFY(object(root,"categoryChangePreview")->property("visible").toBool());QVERIFY(click(root,"applyCategoryChange"));wait(m);}
    QString createCategory(ImportModel& m,QObject* root,const QString& name){
        if(!click(root,"newCategory") || !edit(root,"categoryName",name))qFatal("Category form unavailable");
        applyCategory(m,root);return m.state().value("categorySelected").toMap().value("id").toString();
    }
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("CategoryUi");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());}
    void init(){QSettings().clear();}
    void catalogPickerCurrentNamesAndArchive() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto path=dir.path()+"/document";run(model,"create",{{"path",path},{"protected",false},{"name","Synthetic categories"},{"currency","CAD"}});
        QVERIFY(model.state().value("categoriesSupported").toBool());QVERIFY(model.state().value("categories").toList().isEmpty());
        window->requestActivate();QTest::qWait(100);QVERIFY(pointer(window,root,"categoryNavigation"));QTRY_COMPARE(root->property("currentPage").toString(),"categories");wait(model);
        const auto group=createCategory(model,root,"Auto");QVERIFY(!group.isEmpty());
        const auto leaf=createCategory(model,root,"Auto: Parking");QVERIFY(!leaf.isEmpty());
        QVERIFY(model.categoryNameMatches("Auto: Parking","auto: parking"));QVERIFY(model.categoryNameMatches("Auto: Parking","AUTO",true));QVERIFY(!model.categoryNameMatches("Automobile","Auto",true));
        QVERIFY(click(root,"accountNavigation"));
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));const QByteArray bytes="D1/1/2026\nT-10\nPSynthetic parking\nLLegacy:Transport\n^\n";QCOMPARE(file.write(bytes),bytes.size());file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});const auto occurrence=model.rowIdentity(model.selectedRowIndex());
        auto allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();const auto allocationId=allocation.value("allocationId").toString();QVERIFY(!allocationId.isEmpty());
        QCOMPARE(allocation.value("categoryDisplay").toString(),"Unmapped: Legacy:Transport");
        QVERIFY(click(root,"splitCategory_0"));QVERIFY(click(root,"chooseUncategorized"));QVERIFY(click(root,"saveDraft"));wait(model);
        QCOMPARE(selected(model).value("draft").toMap().value("splits").toList().front().toMap().value("categoryDisplay").toString(),"Uncategorized");
        QVERIFY(click(root,"splitCategory_0"));QVERIFY(edit(root,"unmappedCategoryText","Explicit review label"));QVERIFY(click(root,"chooseUnmapped"));QVERIFY(click(root,"saveDraft"));wait(model);
        QCOMPARE(selected(model).value("draft").toMap().value("splits").toList().front().toMap().value("categoryDisplay").toString(),"Unmapped: Explicit review label");
        QVERIFY(click(root,"splitCategory_0"));QTest::qWait(100);if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_PICKER_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_PICKER_SCREENSHOT")));QVERIFY(object(root,"pickCategory_"+group));QVERIFY(object(root,"pickCategory_"+group)->property("enabled").toBool());
        QVERIFY(edit(root,"categoryPickerSearch","auto: parking"));QVERIFY(QMetaObject::invokeMethod(object(root,"categoryPickerSearch"),"accepted"));QVERIFY(click(root,"saveDraft"));wait(model);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("allocationId").toString(),allocationId);
        run(model,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(model,"commit");
        QVERIFY(click(root,"categoryNavigation"));wait(model);run(model,"category_select",{{"id",leaf}});QVERIFY(click(root,"renameCategory"));QVERIFY(edit(root,"categoryName","Transport: Parking"));applyCategory(model,root);
        QVERIFY(click(root,"accountNavigation"));run(model,"query",{{"filter","Transport: Parking"},{"projected",false}});QCOMPARE(model.rowCount(),1);
        QCOMPARE(model.data(model.index(0,0),Qt::UserRole).toMap().value("category").toString(),"Transport: Parking");run(model,"inspect",occurrence);QVERIFY(selected(model).value("source").toString().contains("LLegacy:Transport"));
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("category").toString(),"Auto: Parking");QCOMPARE(allocation.value("categoryDisplay").toString(),"Transport: Parking");
        QVERIFY(click(root,"categoryNavigation"));wait(model);run(model,"category_select",{{"id",leaf}});QVERIFY(click(root,"archiveCategory"));wait(model);QVERIFY(click(root,"applyCategoryChange"));wait(model);
        QVERIFY(!model.state().value("categoryById").toMap().value(leaf).toMap().value("assignable").toBool());
        QVERIFY(click(root,"accountNavigation"));run(model,"inspect",occurrence);QVERIFY(click(root,"editCommitted"));QVERIFY(edit(root,"durablePayee","Edited payee, retained category"));QVERIFY(click(root,"saveDraft"));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("allocationId").toString(),allocationId);QVERIFY(allocation.value("categoryDisplay").toString().contains("Archived"));QCOMPARE(model.state().value("committed").toString(),"-10.00");
        QVERIFY(click(root,"categoryNavigation"));wait(model);run(model,"category_select",{{"id",leaf}});QVERIFY(click(root,"archiveCategory"));wait(model);QVERIFY(click(root,"applyCategoryChange"));wait(model);
        QVERIFY(model.state().value("categoryById").toMap().value(leaf).toMap().value("assignable").toBool());
        run(model,"category_select",{{"id",leaf}});QVERIFY(click(root,"renameCategory"));QVERIFY(edit(root,"categoryName","Parking"));applyCategory(model,root);
        QTest::qWait(100);if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_SCREENSHOT")));
        QVERIFY(click(root,"deleteCategory"));wait(model,false);QVERIFY(model.error().contains("retained assignments",Qt::CaseInsensitive));
        run(model,"close");run(model,"open",{{"path",path}});run(model,"inspect",occurrence);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("categoryDisplay").toString(),"Parking");
        run(model,"close");auto gateway=storage::document_gateway();gateway->open(path.toStdString(),std::nullopt);const auto entry=gateway->repository().committed_entry({occurrence.value("batch").toString().toStdString(),static_cast<std::size_t>(occurrence.value("ordinal").toInt())});QCOMPARE(entry.current.draft.transaction.allocations().front().identity,allocationId.toStdString());QVERIFY(entry.current.draft.transaction.allocations().front().source_ordinal==std::optional<std::size_t>(0));
    }
    void invalidCatalogEditGuardAndUnusedDeletion() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Synthetic category guards"},{"currency","CAD"}});QVERIFY(click(root,"categoryNavigation"));wait(model);QVERIFY(click(root,"newCategory"));QVERIFY(edit(root,"categoryName","Invalid/Name"));QVERIFY(click(root,"accountNavigation"));QVERIFY(object(root,"categoryUnsavedDialog")->property("visible").toBool());QVERIFY(click(root,"categoryGuardSave"));wait(model,false);QVERIFY(root->property("categoryDirty").toBool());QCOMPARE(root->property("currentPage").toString(),"categories");QVERIFY(model.state().value("categories").toList().isEmpty());
        QVERIFY(click(root,"accountNavigation"));QVERIFY(click(root,"categoryGuardStay"));QVERIFY(root->property("categoryDirty").toBool());QVERIFY(click(root,"accountNavigation"));QVERIFY(click(root,"categoryGuardDiscard"));QCOMPARE(root->property("currentPage").toString(),"ledger");
        QVERIFY(click(root,"categoryNavigation"));wait(model);const auto unused=createCategory(model,root,"Disposable");QVERIFY(click(root,"deleteCategory"));wait(model);QVERIFY(click(root,"applyCategoryChange"));wait(model);QVERIFY(model.state().value("categoryById").toMap().value(unused).toMap().value("deleted").toBool());
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");CategoryUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "category_ui_tests.moc"
