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
#include <QJSValue>
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
    void run(ImportModel& m,const QString& action,const QVariantMap& args={},bool success=true){auto request=args;if(action=="import" && !request.contains("account"))request["account"]=m.state().value("accountId");m.command(action,request);wait(m,success);if(success && action=="import"){for(const auto& item:m.state().value("importCategoryDecisions").toList()){const auto d=item.toMap();run(m,"resolve_import_categories",{{"source",d.value("source")},{"selected",d.value("names")}});}}}
    static QVariantMap selected(const ImportModel& m){return m.state().value("selected").toMap();}
    void applyCategory(ImportModel& m,QObject* root){
        auto* field=qobject_cast<QQuickItem*>(object(root,"categoryName"));QVERIFY(field);
        field->forceActiveFocus();QTest::keyClick(qobject_cast<QQuickWindow*>(root),Qt::Key_Return);wait(m);
        QTRY_VERIFY(!root->property("categoryDirty").toBool());
        QVERIFY(!object(root,"categoryChangePreview"));
    }
    void menu(ImportModel& m,QObject* root,const QString& id,const QString& action){
        auto* list=qobject_cast<QQuickItem*>(object(root,"catalogList"));QVERIFY(list);
        auto cats=m.state().value("categories").toList();int index=-1;
        for(int i=0;i<cats.size();++i)if(cats[i].toMap().value("id").toString()==id){index=i;break;}
        QVERIFY(index>=0);
        // Search makes the target the sole row, independent of catalog ordering and scrolling.
        QVERIFY(edit(root,"catalogSearch",cats[index].toMap().value("path").toString()));QTest::qWait(30);
        auto point=list->mapToScene(QPointF(40,15)).toPoint();
        QTest::mouseClick(qobject_cast<QQuickWindow*>(root),Qt::RightButton,Qt::NoModifier,point);
        QTRY_VERIFY(object(root,"categoryContextMenu")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(root,action),"triggered"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"categoryContextMenu"),"close"));
    }
    QString createCategory(ImportModel& m,QObject* root,const QString& name){
        if(!click(root,"newCategory"))qFatal("Category creation unavailable");
        QTest::qWait(30);
        if(!edit(root,"categoryName",name))qFatal("Inline category field unavailable");
        applyCategory(m,root);return m.state().value("categorySelected").toMap().value("id").toString();
    }
    static QVariantList catalogRows(QObject* root){
        auto value=object(root,"catalogList")->property("model");
        if(value.metaType()==QMetaType::fromType<QJSValue>())value=value.value<QJSValue>().toVariant();
        return value.toList();
    }
    static QStringList catalogNames(QObject* root){QStringList names;for(const auto& row:catalogRows(root))if(!row.toMap().value("account").toBool())names.append(row.toMap().value("path").toString());return names;}
    void checkSelected(QObject* root,const QString& id){
        QTRY_COMPARE(root->property("categorySelection").toString(),id);
        QTest::qWait(30);
        const auto rows=catalogRows(root);auto* list=object(root,"catalogList");const auto index=list->property("currentIndex").toInt();
        QVERIFY(index>=0 && index<rows.size());QCOMPARE(rows[index].toMap().value("id").toString(),id);
        const auto y=index*30.0-list->property("contentY").toDouble();QVERIFY(y>=-1 && y+30<=list->property("height").toDouble()+1);
    }
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("CategoryUi");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());}
    void init(){QSettings().clear();}
    void openingRowEditing() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Opening edit"},{"currency","CAD"}});
        QVERIFY(!object(root,"openingSetup"));
        QTRY_VERIFY(object(root,"ledgerRow_0"));
        auto* row=qobject_cast<QQuickItem*>(object(root,"ledgerRow_0"));
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,row->mapToScene(QPointF(150,16)).toPoint());
        QTRY_VERIFY(object(root,"correctOpeningDialog")->property("visible").toBool());
        QVERIFY(edit(root,"correctOpeningDate","2026-01-01"));
        QVERIFY(edit(root,"correctOpeningAmount","25.00"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"correctOpeningDialog"),"accept"));wait(model);
        QCOMPARE(model.state().value("opening").toString(),QString("25.00"));
        const auto operation=model.state().value("undoableAccountOperation").toString();QVERIFY(!operation.isEmpty());
        run(model,"undo_account",{{"operation",model.newIdentity()},{"undoOf",operation}});
        QCOMPARE(model.state().value("opening").toString(),QString("0.00"));
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));file.write("D1/1/2026\nT-10\nPOpening test\n^\n");file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        QCOMPARE(model.state().value("reviewCount").toInt(),0);run(model,"commit");
        QVERIFY(QMetaObject::invokeMethod(root,"editOpening"));
        QTRY_VERIFY(object(root,"correctOpeningDialog")->property("visible").toBool());
        QVERIFY(edit(root,"correctOpeningDate","2026-01-01"));QVERIFY(edit(root,"correctOpeningAmount","50.00"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"correctOpeningDialog"),"accept"));wait(model);
        QCOMPARE(model.state().value("opening").toString(),QString("50.00"));
        run(model,"close");
    }
    void inlineCategoryCompletion() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Completion"},{"currency","CAD"}});
        QVERIFY(click(root,"categoryNavigation"));wait(model);QTest::qWait(50);
        const auto gas=createCategory(model,root,"Auto: Gas");
        const auto parking=createCategory(model,root,"Auto: Parking");
        createCategory(model,root,"Utils: Electricity, Gas & Heating");
        for(int i=0;i<12;++i)createCategory(model,root,QString("Synthetic %1").arg(i,2,10,QChar('0')));
        QVERIFY(click(root,"accountNavigation"));
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));file.write("D1/1/2026\nT-10\nPSynthetic completion\n^\n");file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        auto field=[&](const char* name){return qobject_cast<QQuickItem*>(object(root,name));};
        auto category=[&](){return field("splitCategory_0");};
        auto assigned=[&](){return selected(model).value("draft").toMap().value("splits").toList().front().toMap().value("categoryId").toString();};
        QVERIFY(category());QCOMPARE(category()->property("text").toString(),QString());
        QTRY_VERIFY(category()->width()>=root->property("categorySelectorWidth").toDouble()-1);
        field("durableDate")->forceActiveFocus();QTest::keyClick(window,Qt::Key_Tab);QVERIFY(field("durablePayee")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Tab);QVERIFY(field("durableTotal")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Tab);QVERIFY(category()->hasActiveFocus());
        QVERIFY(category()->findChild<QObject*>("categoryDropdown_0"));QTRY_VERIFY(category()->findChild<QObject*>("categoryDropdown_0")->property("visible").toBool());
        QCOMPARE(object(root,"categoryMatches_0")->property("count").toInt(),16);
        QVERIFY(object(root,"categoryDropdownScrollBar")->property("interactive").toBool());
        QTest::qWait(50);auto* bar=qobject_cast<QQuickItem*>(object(root,"categoryDropdownScrollBar"));QVERIFY(bar);
        QVERIFY(bar->property("size").toDouble()<1.0);
        const auto thumb=bar->mapToScene(QPointF(bar->width()/2,bar->height()*bar->property("size").toDouble()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,thumb);
        QTest::mouseMove(window,thumb+QPoint(0,80));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,thumb+QPoint(0,80));
        QVERIFY(object(root,"categoryMatches_0")->property("contentY").toDouble()>0);

        for(auto key:{Qt::Key_A,Qt::Key_U,Qt::Key_T,Qt::Key_O})QTest::keyClick(window,key,Qt::ShiftModifier);QCOMPARE(object(root,"categoryMatches_0")->property("count").toInt(),2);
        QCOMPARE(object(root,"categoryMatches_0")->property("currentIndex").toInt(),0);
        if(qEnvironmentVariableIsSet("SPENTIT_INLINE_CATEGORY_SCREENSHOT")){
            category()->setProperty("text","Utils: Electricity, Gas & Heating");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));
            QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INLINE_CATEGORY_SCREENSHOT")));
            window->resize(720,700);QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INLINE_CATEGORY_SCREENSHOT")+".narrow.png"));
            window->resize(1440,850);category()->setProperty("text","AUTO");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));QTest::qWait(100);
        }
        QTest::keyClick(window,Qt::Key_Down);QTest::keyClick(window,Qt::Key_Return);wait(model);
        QTRY_COMPARE(assigned(),parking);QTRY_VERIFY(field("durableMemo")->hasActiveFocus());
        QCOMPARE(category()->property("text").toString(),QString("Auto: Parking"));
        QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(category()->hasActiveFocus());
        category()->setProperty("text","auto: gAs");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));
        QCOMPARE(object(root,"categoryMatches_0")->property("count").toInt(),1);
        QTest::keyClick(window,Qt::Key_Tab);wait(model);QTRY_COMPARE(assigned(),gas);QTRY_VERIFY(field("durableMemo")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Backtab);QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(field("durableTotal")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(field("durablePayee")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(field("durableDate")->hasActiveFocus());
        category()->forceActiveFocus();category()->setProperty("text","no such category");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));
        QCOMPARE(object(root,"categoryMatches_0")->property("count").toInt(),0);
        QTest::keyClick(window,Qt::Key_Return);QVERIFY(category()->hasActiveFocus());QCOMPARE(assigned(),gas);QVERIFY(!model.busy());
        QTest::keyClick(window,Qt::Key_Escape);QCOMPARE(category()->property("text").toString(),QString("Auto: Gas"));
        category()->setProperty("text","");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));
        QTest::keyClick(window,Qt::Key_Tab);wait(model);QTRY_VERIFY(assigned().isEmpty());QTRY_VERIFY(field("durableMemo")->hasActiveFocus());
        QCOMPARE(category()->property("text").toString(),QString());
        QVERIFY(QMetaObject::invokeMethod(root,"undoEdit"));wait(model);QTRY_COMPARE(assigned(),gas);
        QTRY_VERIFY(field("durableMemo")->hasActiveFocus());QTest::qWait(50);category()->forceActiveFocus();category()->setProperty("text","Parking");QVERIFY(QMetaObject::invokeMethod(category(),"textEdited"));
        QTest::qWait(50);QVERIFY(pointer(window,root,"inlineCategoryChoice_"+parking));wait(model);QTRY_COMPARE(assigned(),parking);
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write("D2/1/2026\nT-10\nPSynthetic splits\nS\n$-5\nS\n$-5\n^\n");file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        QVERIFY(field("splitCategory_1"));field("splitCategory_1")->forceActiveFocus();
        QVERIFY(edit(root,"splitCategory_1","Gas"));QTest::keyClick(window,Qt::Key_Tab);wait(model);
        QTRY_VERIFY(field("editSplitMemo_1")->hasActiveFocus());
        QCOMPARE(selected(model).value("draft").toMap().value("splits").toList().last().toMap().value("categoryId").toString(),gas);
        QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(field("splitCategory_1")->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Backtab);QVERIFY(field("editSplitAmount_0")->hasActiveFocus());


    }
    void catalogPickerCurrentNamesAndArchive() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto path=dir.path()+"/document";run(model,"create",{{"path",path},{"protected",false},{"name","Synthetic categories"},{"currency","CAD"}});
        QVERIFY(model.state().value("categoriesSupported").toBool());QVERIFY(model.state().value("categories").toList().isEmpty());QCOMPARE(model.state().value("accountCategories").toList().size(),1);
        window->requestActivate();QTest::qWait(100);QVERIFY(pointer(window,root,"categoryNavigation"));QTRY_COMPARE(root->property("currentPage").toString(),"categories");wait(model);
        QVERIFY(catalogRows(root).front().toMap().value("account").toBool());
        auto* accountList=qobject_cast<QQuickItem*>(object(root,"catalogList"));QVERIFY(accountList);
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,accountList->mapToScene(QPointF(40,15)).toPoint());
        QTRY_VERIFY(object(root,"categoryContextMenu")->property("visible").toBool());
        QVERIFY(!object(root,"renameCategory")->property("enabled").toBool());QVERIFY(!object(root,"archiveCategory")->property("enabled").toBool());QVERIFY(!object(root,"deleteCategory")->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(root,"categoryContextMenu"),"close"));
        const auto group=createCategory(model,root,"Auto");QVERIFY(!group.isEmpty());
        const auto leaf=createCategory(model,root,"Auto: Parking");QVERIFY(!leaf.isEmpty());
        QVERIFY(model.categoryNameMatches("Auto: Parking","auto: parking"));QVERIFY(model.categoryNameMatches("Auto: Parking","AUTO",true));QVERIFY(!model.categoryNameMatches("Automobile","Auto",true));
        QVERIFY(click(root,"accountNavigation"));
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));const QByteArray bytes="D1/1/2026\nT-10\nPSynthetic parking\nLLegacy:Transport\n^\n";QCOMPARE(file.write(bytes),bytes.size());file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});const auto occurrence=model.rowIdentity(model.selectedRowIndex());
        auto allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();const auto allocationId=allocation.value("allocationId").toString();QVERIFY(!allocationId.isEmpty());
        QCOMPARE(allocation.value("categoryDisplay").toString(),"Legacy: Transport");
        QVERIFY(QMetaObject::invokeMethod(root,"openCategoryPicker",Q_ARG(QVariant,0)));QVERIFY(click(root,"chooseUncategorized"));wait(model);
        QCOMPARE(selected(model).value("draft").toMap().value("splits").toList().front().toMap().value("categoryDisplay").toString(),"Uncategorized");
        QVERIFY(QMetaObject::invokeMethod(root,"openCategoryPicker",Q_ARG(QVariant,0)));QVERIFY(edit(root,"unmappedCategoryText","Explicit review label"));QVERIFY(click(root,"chooseUnmapped"));wait(model);
        QCOMPARE(selected(model).value("draft").toMap().value("splits").toList().front().toMap().value("categoryDisplay").toString(),"Unmapped: Explicit review label");
        QVERIFY(QMetaObject::invokeMethod(root,"openCategoryPicker",Q_ARG(QVariant,0)));QTest::qWait(100);if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_PICKER_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_PICKER_SCREENSHOT")));QVERIFY(object(root,"pickCategory_"+group));QVERIFY(object(root,"pickCategory_"+group)->property("enabled").toBool());
        QVERIFY(edit(root,"categoryPickerSearch","auto: parking"));QVERIFY(QMetaObject::invokeMethod(object(root,"categoryPickerSearch"),"accepted"));wait(model);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("allocationId").toString(),allocationId);
        run(model,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(model,"commit");
        QVERIFY(click(root,"categoryNavigation"));wait(model);menu(model,root,leaf,"renameCategory");QTest::qWait(30);QVERIFY(edit(root,"categoryName","Transport: Parking"));applyCategory(model,root);
        QVERIFY(click(root,"accountNavigation"));run(model,"query",{{"filter","Transport: Parking"},{"projected",false}});QCOMPARE(model.rowCount(),1);
        QCOMPARE(model.data(model.index(0,0),Qt::UserRole).toMap().value("category").toString(),"Transport: Parking");run(model,"inspect",occurrence);QVERIFY(selected(model).value("source").toString().contains("LLegacy:Transport"));
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("category").toString(),"Auto: Parking");QCOMPARE(allocation.value("categoryDisplay").toString(),"Transport: Parking");
        QVERIFY(click(root,"categoryNavigation"));wait(model);menu(model,root,leaf,"archiveCategory");wait(model);
        QVERIFY(!model.state().value("categoryById").toMap().value(leaf).toMap().value("assignable").toBool());
        QVERIFY(click(root,"accountNavigation"));run(model,"inspect",occurrence);QVERIFY(edit(root,"durablePayee","Edited payee, retained category"));QVERIFY(QMetaObject::invokeMethod(object(root,"durablePayee"),"accepted"));wait(model);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("allocationId").toString(),allocationId);QVERIFY(allocation.value("categoryDisplay").toString().contains("Archived"));QCOMPARE(model.state().value("committed").toString(),"-10.00");
        QVERIFY(click(root,"categoryNavigation"));wait(model);object(root,"showArchivedCategories")->setProperty("checked",true);menu(model,root,leaf,"archiveCategory");wait(model);
        QVERIFY(model.state().value("categoryById").toMap().value(leaf).toMap().value("assignable").toBool());
        menu(model,root,leaf,"renameCategory");QTest::qWait(30);QVERIFY(edit(root,"categoryName","Parking"));applyCategory(model,root);
        QTest::qWait(100);if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_SCREENSHOT")));
        menu(model,root,leaf,"deleteCategory");wait(model,false);QVERIFY(model.error().contains("Transactions use this category",Qt::CaseInsensitive));
        QCOMPARE(object(root,"operationPhase")->property("text").toString(),model.error());
        int visibleErrors=0;
        for(auto* item:root->findChildren<QQuickItem*>())
            if(item->isVisible() && item->property("text").toString()==model.error())++visibleErrors;
        QCOMPARE(visibleErrors,1);
        // Modal validation owns the global error while open; dismissal restores
        // it to the bottom line without clearing the underlying failure.
        const auto error=model.error();
        for(const char* name:{"importAccountMapping","committedChangePreview","mergePreview","bulkDialog","reapplyDialog","closeAuditFailure","linkExclusionDialog","reviewDifference","statusDetails"}) {
            auto* dialog=object(root,name);QVERIFY(dialog);
            QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
            QTRY_VERIFY(dialog->property("visible").toBool());
            QCOMPARE(object(root,"operationPhase")->property("text").toString(),QString());
            int copies=0;for(auto* item:root->findChildren<QQuickItem*>())
                if(item->isVisible() && item->property("text").toString()==error)++copies;
            QCOMPARE(copies,1);
            QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
            QCOMPARE(object(root,"operationPhase")->property("text").toString(),error);
        }
        run(model,"close");run(model,"open",{{"path",path}});run(model,"inspect",occurrence);
        allocation=selected(model).value("draft").toMap().value("splits").toList().front().toMap();QCOMPARE(allocation.value("categoryId").toString(),leaf);QCOMPARE(allocation.value("categoryDisplay").toString(),"Parking");
        run(model,"close");auto gateway=storage::document_gateway();gateway->open(path.toStdString(),std::nullopt);const auto entry=gateway->repository().committed_entry({occurrence.value("batch").toString().toStdString(),static_cast<std::size_t>(occurrence.value("ordinal").toInt())});QCOMPARE(entry.current.draft.transaction.allocations().front().identity,allocationId.toStdString());QVERIFY(entry.current.draft.transaction.allocations().front().source_ordinal==std::optional<std::size_t>(0));
    }
    void explicitSortingAndRetainedSelection() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";QString middle;
        {
            ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
            run(model,"create",{{"path",path},{"protected",false},{"name","Category sorting"},{"currency","CAD"}});QVERIFY(click(root,"categoryNavigation"));wait(model);
            QCOMPARE(root->property("categorySort").toString(),"ascending");
            const auto z=createCategory(model,root,"Zulu");const auto a=createCategory(model,root,"alpha");middle=createCategory(model,root,"Middle");checkSelected(root,middle);
            QCOMPARE(catalogNames(root),QStringList({"Zulu","alpha","Middle"}));
            QVERIFY(click(root,"accountNavigation"));wait(model);
            QVERIFY(QMetaObject::invokeMethod(root,"traverseNavigation",Q_ARG(QVariant,-1)));wait(model);
            QCOMPARE(root->property("currentPage").toString(),"categories");QCOMPARE(catalogNames(root),QStringList({"alpha","Middle","Zulu"}));
            QVERIFY(QMetaObject::invokeMethod(root,"traverseNavigation",Q_ARG(QVariant,1)));wait(model);
            QVERIFY(click(root,"categoryNavigation"));wait(model);
            QCOMPARE(catalogNames(root),QStringList({"alpha","Middle","Zulu"}));
            QVERIFY(click(root,"categorySortAscending"));QCOMPARE(catalogNames(root),QStringList({"alpha","Middle","Zulu"}));checkSelected(root,middle);
            QVERIFY(click(root,"categorySortDescending"));QCOMPARE(catalogNames(root),QStringList({"Zulu","Middle","alpha"}));checkSelected(root,middle);
            const auto b=createCategory(model,root,"Beta");QCOMPARE(catalogNames(root),QStringList({"Zulu","Middle","alpha","Beta"}));checkSelected(root,b);QCOMPARE(root->property("categorySort").toString(),"descending");
            QVERIFY(click(root,"categorySortAdded"));QCOMPARE(catalogNames(root),QStringList({"Zulu","alpha","Middle","Beta"}));checkSelected(root,b);
            // A rename must retain original creation order, even when its spelling changes substantially.
            menu(model,root,z,"renameCategory");QTest::qWait(30);QVERIFY(edit(root,"categoryName","Aardvark"));applyCategory(model,root);QVERIFY(edit(root,"catalogSearch",""));
            QCOMPARE(catalogNames(root),QStringList({"Aardvark","alpha","Middle","Beta"}));checkSelected(root,z);
            for(int i=0;i<24;i++)createCategory(model,root,QString("Extra %1").arg(i,2,10,QChar('0')));
            const auto last=createCategory(model,root,"A new last entry");QCOMPARE(catalogRows(root).last().toMap().value("id").toString(),last);checkSelected(root,last);
            QVERIFY(click(root,"categorySortAscending"));QCOMPARE(catalogRows(root).first().toMap().value("id").toString(),last);checkSelected(root,last);
            QVERIFY(click(root,"categorySortDescending"));QCOMPARE(catalogRows(root).last().toMap().value("id").toString(),last);checkSelected(root,last);
            QVERIFY(edit(root,"catalogSearch","no match"));const auto filtered=createCategory(model,root,"Visible after save");checkSelected(root,filtered);QCOMPARE(catalogNames(root),QStringList({"Visible after save"}));
            QVERIFY(edit(root,"catalogSearch","alpha"));QCOMPARE(catalogNames(root),QStringList({"alpha"}));
            QCOMPARE(root->property("categorySort").toString(),"descending");
            run(model,"close");
        }
        {
            ImportModel model(storage::document_gateway());run(model,"open",{{"path",path}});QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
            QCOMPARE(root->property("categorySort").toString(),"ascending");QVERIFY(click(root,"categoryNavigation"));wait(model);
            QVERIFY(click(root,"categorySortAdded"));QCOMPARE(catalogNames(root)[2],QString("Middle"));QCOMPARE(catalogNames(root).first(),"Aardvark");QCOMPARE(catalogNames(root).last(),"Visible after save");
        }
    }
    void inlineEditingAndScroll() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Synthetic category guards"},{"currency","CAD"},{"defaultCategories",true}});
        QVERIFY(click(root,"categoryNavigation"));wait(model);
        QVERIFY(click(root,"newCategory"));QTest::qWait(50);auto* field=qobject_cast<QQuickItem*>(object(root,"categoryName"));QVERIFY(field);QVERIFY(field->hasActiveFocus());
        QVERIFY(edit(root,"categoryName","Invalid|Name"));applyInvalid(model,root,window);
        QCOMPARE(object(root,"categoryName")->property("text").toString(),"Invalid|Name");QVERIFY(root->property("categoryDirty").toBool());
        QTest::keyClick(window,Qt::Key_Escape);QVERIFY(!root->property("categoryDirty").toBool());
        QVERIFY(click(root,"newCategory"));QTest::qWait(30);QTest::keyClick(window,Qt::Key_Escape);QVERIFY(!root->property("categoryDirty").toBool());
        const auto unused=createCategory(model,root,"Disposable");
        QVERIFY(click(root,"newCategory"));QTest::qWait(30);QVERIFY(edit(root,"categoryName","disposable"));applyInvalid(model,root,window);QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(edit(root,"catalogSearch","does not match"));QVERIFY(click(root,"newCategory"));QTest::qWait(30);QVERIFY(edit(root,"categoryName","Focus saved"));
        QVERIFY(pointer(window,root,"catalogSearch"));QTest::qWait(50);wait(model);QTRY_VERIFY(!root->property("categoryDirty").toBool());
        QCOMPARE(model.state().value("categorySelected").toMap().value("name").toString(),"Focus saved");
        menu(model,root,unused,"renameCategory");QTest::qWait(30);QVERIFY(edit(root,"categoryName","Discarded rename"));QTest::keyClick(window,Qt::Key_Escape);
        QCOMPARE(model.state().value("categoryById").toMap().value(unused).toMap().value("name").toString(),"Disposable");
        QVERIFY(click(root,"newCategory"));QTest::qWait(30);QVERIFY(pointer(window,root,"catalogSearch"));QTest::qWait(30);QVERIFY(!root->property("categoryDirty").toBool());
        QVERIFY(click(root,"newCategory"));QTest::qWait(30);QVERIFY(edit(root,"categoryName","Invalid|Navigation"));QVERIFY(click(root,"accountNavigation"));wait(model,false);QCOMPARE(root->property("currentPage").toString(),"categories");QTest::qWait(30);QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(click(root,"newCategory"));QTest::qWait(30);QVERIFY(edit(root,"categoryName","Saved on navigation"));QVERIFY(click(root,"accountNavigation"));wait(model);QTRY_COMPARE(root->property("currentPage").toString(),"ledger");
        QCOMPARE(model.state().value("categorySelected").toMap().value("name").toString(),"Saved on navigation");
        QVERIFY(click(root,"categoryNavigation"));wait(model);
        menu(model,root,unused,"deleteCategory");wait(model);QVERIFY(model.state().value("categoryById").toMap().value(unused).toMap().value("deleted").toBool());
        QVERIFY(object(root,"showArchivedCategories")->setProperty("checked",true));
        QVERIFY(!catalogNames(root).contains("Disposable"));
        QVERIFY(pointer(window,root,"catalogSearchClear"));
        QCOMPARE(object(root,"catalogSearch")->property("text").toString(),QString());
        QVERIFY(object(root,"catalogSearch")->property("activeFocus").toBool());
        QVERIFY(!object(root,"catalogSearchClear")->property("visible").toBool());
        QVERIFY(catalogNames(root).contains("Amazon"));
        QVERIFY(!catalogNames(root).contains("Disposable"));
        QVERIFY(object(root,"showArchivedCategories")->setProperty("checked",false));
        QVERIFY(edit(root,"catalogSearch",""));
        for(int i=0;i<30;i++)createCategory(model,root,QString("Scroll %1").arg(i,2,10,QChar('0')));
        auto* list=object(root,"catalogList");QVERIFY(list->property("height").toDouble()>400);list->setProperty("contentY",0);QTest::qWait(50);
        auto* bar=qobject_cast<QQuickItem*>(object(root,"catalogScrollBar"));QVERIFY(bar);QVERIFY(bar->property("size").toDouble()<1);
        auto* thumb=qvariant_cast<QQuickItem*>(bar->property("contentItem"));QVERIFY(thumb);
        auto start=thumb->mapToScene(QPointF(thumb->width()/2,thumb->height()/2)).toPoint();auto end=start+QPoint(0,120);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,end,50);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,end);
        QTRY_VERIFY(list->property("contentY").toDouble()>20);
        QVERIFY(!object(root,"categoryUnsavedDialog"));QVERIFY(!object(root,"categoryChangePreview"));
        if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_SCREENSHOT")));
    }
private:
    void applyInvalid(ImportModel& model,QObject* root,QQuickWindow* window){
        qobject_cast<QQuickItem*>(object(root,"categoryName"))->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);wait(model,false);QTest::qWait(30);QCOMPARE(object(root,"operationPhase")->property("text").toString(),model.error());
    }

};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");CategoryUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "category_ui_tests.moc"
