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
    void init(){QSettings().clear();}
    void workspaceUsability(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);
        auto* workspace=object(root,"reportsWorkspace");QVERIFY(workspace);
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        QVERIFY(click(root,"reportsNavigation"));QTRY_VERIFY(!model.busy());
        auto visible=[&](const char* name){auto* item=object(root,name);return item && item->property("visible").toBool();};
        auto drafts=[&]{return value(workspace->property("drafts")).toList();};
        auto field=[&](const QString& name,const QVariant& v){return QMetaObject::invokeMethod(workspace,"setField",Q_ARG(QVariant,name),Q_ARG(QVariant,v));};
        QVERIFY(!visible("reportSearch"));QVERIFY(!visible("duplicateReport"));QVERIFY(visible("reportFeedbackHelp"));
        QVERIFY(click(root,"newReport"));QVERIFY(field("name","First report"));
        QCOMPARE(drafts().size(),1);QVERIFY(!visible("duplicateReport"));QVERIFY(!visible("deleteReport"));
        QCOMPARE(object(root,"newReport")->property("text").toString(),"New report");
        QVERIFY(!object(root,"newReport")->property("enabled").toBool());
        QVERIFY(click(root,"newReport"));QCOMPARE(drafts().size(),1);
        auto* nameItem=qobject_cast<QQuickItem*>(object(root,"reportName"));QVERIFY(nameItem);nameItem->forceActiveFocus();
        QVERIFY(click(root,"saveReport"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());
        QVERIFY(!nameItem->hasActiveFocus());
        auto* accountSearch=object(root,"reportChoiceSearch_accounts");QVERIFY(accountSearch);
        accountSearch->setProperty("text","Checking");QVERIFY(click(root,"reportChoiceSearch_accountsClear"));
        QCOMPARE(accountSearch->property("text").toString(),"");
        QVERIFY(!visible("saveReport"));QVERIFY(visible("duplicateReport"));
        QCOMPARE(object(root,"showReport")->property("text").toString(),"Show report");
        // Duplicate copies the selected edits but never saves its source.
        QVERIFY(field("name","Edited first"));QVERIFY(click(root,"duplicateReport"));
        QCOMPARE(drafts().size(),2);QCOMPARE(drafts()[1].toMap().value("name").toString(),"Edited first copy");
        QCOMPARE(model.state().value("reports").toList()[0].toMap().value("name").toString(),"First report");
        QVERIFY(field("name","")); // unrelated incomplete new report
        workspace->setProperty("selectedIndex",0);
        QVERIFY(!visible("duplicateReport"));
        QCOMPARE(object(root,"showReport")->property("text").toString(),"Save and show");
        QVERIFY(click(root,"saveReport"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());
        QCOMPARE(drafts()[1].toMap().value("name").toString(),"");
        QVERIFY(workspace->property("dirty").toBool());
        auto* search=object(root,"reportSearch");search->setProperty("text","no matching report");
        QTRY_VERIFY(visible("reportNoMatches"));QCOMPARE(workspace->property("selectedIndex").toInt(),0);
        QVERIFY(visible("reportName"));
        QVERIFY(click(root,"reportSearchClear"));QCOMPARE(search->property("text").toString(),"");
        // Named cancellation, Escape and confirmed selected-only deletion.
        QVERIFY(click(root,"deleteReport"));auto* dialog=object(root,"deleteReportDialog");QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());QVERIFY(dialog->property("modal").toBool());
        QCOMPARE(dialog->property("reportName").toString(),"Edited first");
        QVERIFY(click(root,"cancelDeleteReport"));QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(model.state().value("reports").toList().size(),1);QCOMPARE(drafts().size(),2);
        QVERIFY(click(root,"deleteReport"));QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(click(root,"deleteReport"));QVERIFY(click(root,"confirmDeleteReport"));QTRY_VERIFY(!model.busy());
        QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QCOMPARE(model.state().value("reports").toList().size(),0);
        QCOMPARE(drafts().size(),1);QCOMPARE(workspace->property("selectedIndex").toInt(),0);
        QCOMPARE(drafts()[0].toMap().value("name").toString(),"");
        QVERIFY(click(root,"discardReport"));QVERIFY(drafts().isEmpty());QVERIFY(!workspace->property("dirty").toBool());
        // Final deletion is durable and does not resurrect on reload.
        run(model,"report_load");QVERIFY(drafts().isEmpty());
        QVERIFY(click(root,"newReport"));QVERIFY(field("name","Chart sizing"));
        window->resize(1000,800);window->show();QTest::qWait(100);
        auto* date=qobject_cast<QQuickItem*>(object(root,"reportStartDate0"));QVERIFY(date);
        date->forceActiveFocus();QVERIFY(QMetaObject::invokeMethod(date,"select",Q_ARG(int,3),Q_ARG(int,4)));
        QTest::keyClick(window,Qt::Key_4);
        QCOMPARE(date->property("text").toString(),"2024-01-01");
        QCOMPARE(date->property("cursorPosition").toInt(),4);
        date->setProperty("cursorPosition",0);
        for(char c:QByteArray("2023"))QTest::keyClick(window,Qt::Key(c));
        QCOMPARE(date->property("text").toString(),"2023-01-01");
        QCOMPARE(date->property("cursorPosition").toInt(),4);
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        // Explicit selection also supports replacing the entire value.
        QVERIFY(QMetaObject::invokeMethod(date,"selectAll"));
        for(char c:QByteArray("2022-02-03"))QTest::keyClick(window,Qt::Key(c));
        QCOMPARE(date->property("text").toString(),"2022-02-03");
        auto* graph=object(root,"reportGraph");QVERIFY(graph);
        auto* content=graph->property("contentItem").value<QObject*>();QVERIFY(content);
        QVERIFY(content->property("width").toDouble()>=content->property("implicitWidth").toDouble());
        if(qEnvironmentVariableIsSet("SPENTIT_REPORT_UI_CAPTURE")){
            auto* scroll=object(root,"reportsEditorScroll");QVERIFY(scroll);
            auto* flick=scroll->property("contentItem").value<QObject*>();QVERIFY(flick);
            flick->setProperty("contentY",flick->property("contentHeight").toDouble()-flick->property("height").toDouble());
            QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REPORT_UI_CAPTURE")));
        }
        auto* theme=object(root,"applicationTheme");QVERIFY(theme);
        auto* rename=object(root,"reportRename");auto* remove=object(root,"reportDelete");
        QVERIFY(rename && remove);QVERIFY(rename->property("enabled").toBool());
        auto* renameText=rename->property("contentItem").value<QObject*>();QVERIFY(renameText);
        auto* removeText=remove->property("contentItem").value<QObject*>();QVERIFY(removeText);
        QCOMPARE(renameText->property("color"),theme->property("text"));
        QVERIFY(remove->setProperty("enabled",false));
        QCOMPARE(removeText->property("color"),theme->property("disabled"));
        QVERIFY(remove->setProperty("highlighted",true));
        QCOMPARE(remove->property("background").value<QObject*>()->property("color").value<QColor>().alpha(),0);
        for(const auto& warning:warnings){const auto list=qvariant_cast<QList<QQmlError>>(warning.front());for(const auto& item:list)qWarning().noquote()<<item.toString();}
        QVERIFY(warnings.isEmpty());
    }
    void importProgress_data(){QTest::addColumn<bool>("uncategorized");QTest::newRow("selected")<<false;QTest::newRow("uncategorized")<<true;}
    void importProgress(){
        QFETCH(bool,uncategorized);
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.filePath("doc")},{"protected",false},{"name","Checking"},{"currency","CAD"},{"dateFormat","YYYY-MM-DD"}});
        QFile file(dir.filePath("import.qif"));QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("!Account\nNCard\nTCCard\n^\n!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLNovel category\n^\n!Account\nNCash\nTCash\n^\n!Type:Cash\nD2026-01-18\nT-5.00\nPBakery\nLNovel category\n^\n");file.close();
        run(model,"import",{{"path",file.fileName()},{"account",""}});
        auto* mapping=object(root,"importAccountMapping");QVERIFY(mapping);QTRY_VERIFY(mapping->property("visible").toBool());
        for(const char* name:{"sourceAccountChoice_0","sourceAccountChoice_1"}){QTRY_VERIFY(object(root,name));auto* choice=object(root,name);choice->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,1)));}
        auto* calendar=object(root,"sourceOpeningCalendar_0");QVERIFY(calendar);
        QVERIFY(QMetaObject::invokeMethod(calendar,"choose",Q_ARG(QVariant,QVariant("2024-02-29"))));
        auto mappings=[&]{return value(mapping->property("mappings")).toList();};
        QCOMPARE(mappings()[0].toMap().value("date").toString(),QString("2024-02-29"));QVERIFY(!model.busy());
        QVERIFY(QMetaObject::invokeMethod(mapping,"update",Q_ARG(QVariant,0),Q_ARG(QVariant,"date"),Q_ARG(QVariant,"bad-date")));
        auto* progress=object(root,"importProgressDialog");QVERIFY(progress);
        QVERIFY(click(root,"confirmImportMappings"));QVERIFY(model.busy());QVERIFY(progress->property("visible").toBool());QVERIFY(!mapping->property("visible").toBool());
        QTRY_VERIFY(!model.busy());QVERIFY(!model.error().isEmpty());QTRY_VERIFY(mapping->property("visible").toBool());QVERIFY(!progress->property("visible").toBool());
        QCOMPARE(mappings()[0].toMap().value("date").toString(),QString("bad-date"));QVERIFY(mappings()[1].toMap().value("create").toBool());
        QVERIFY(QMetaObject::invokeMethod(calendar,"choose",Q_ARG(QVariant,QVariant("2024-02-29"))));
        bool cancelMapping=true;
        const auto mappingCancelConnection=connect(&model,&ImportModel::changed,this,[&]{if(cancelMapping && model.busy()){cancelMapping=false;click(root,"cancelImportProgress");}});
        QVERIFY(click(root,"confirmImportMappings"));QVERIFY(progress->property("visible").toBool());
        disconnect(mappingCancelConnection);QVERIFY(model.cancellationRequested());
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QTRY_VERIFY(mapping->property("visible").toBool());
        QCOMPARE(mappings()[0].toMap().value("date").toString(),QString("2024-02-29"));
        QCOMPARE(model.state().value("accounts").toList().size(),1);
        QVERIFY(click(root,"confirmImportMappings"));QVERIFY(progress->property("visible").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        auto* categories=object(root,"importCategories");QTRY_VERIFY(categories->property("visible").toBool());QVERIFY(!progress->property("visible").toBool());
        // Request at admission, before the worker starts, to exercise rollback deterministically.
        bool cancelOnce=true;
        const auto cancelConnection=connect(&model,&ImportModel::changed,this,[&]{if(cancelOnce && model.busy()){cancelOnce=false;click(root,"cancelImportProgress");}});
        QVERIFY(click(root,uncategorized?"uncategorizedImport":"applyImportCategories"));QVERIFY(progress->property("visible").toBool());QVERIFY(!categories->property("visible").toBool());
        disconnect(cancelConnection);
        QVERIFY(model.cancellationRequested());QVERIFY(!object(root,"cancelImportProgress")->property("enabled").toBool());
        QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QTRY_VERIFY(!progress->property("visible").toBool());
        QVERIFY(!model.state().value("importCategoryDecisions").toList().isEmpty());QTRY_VERIFY(categories->property("visible").toBool());
        QVERIFY(click(root,uncategorized?"uncategorizedImport":"applyImportCategories"));QVERIFY(progress->property("visible").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QVERIFY(model.state().value("importCategoryDecisions").toList().isEmpty());QVERIFY(!progress->property("visible").toBool());
        QVERIFY(!object(root,"groupOpeningDateCalendar")); // retired staged import opening
        for(const char* name:{"correctOpeningCalendar","transferDateCalendar"}){auto* picker=object(root,name);QVERIFY(picker);auto* field=picker->property("field").value<QObject*>();QVERIFY(field);QVERIFY(QMetaObject::invokeMethod(picker,"choose",Q_ARG(QVariant,QVariant("2024-02-29"))));QCOMPARE(field->property("text").toString(),QString("2024-02-29"));QVERIFY(!model.busy());}
        for(const auto& warning:warnings)qWarning()<<warning;QCOMPARE(warnings.size(),0);
    }
    void incomeGroups(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(QTest::qWaitForWindowExposed(window));auto* workspace=object(root,"reportsWorkspace");
        run(model,"create",{{"path",dir.filePath("doc")},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        QStringList ids;
        for(const auto* name:{"Zulu expense","Alpha income","Middle expense"}) {
            run(model,"preview_category",{{"action","create"},{"name",name},{"revision",model.state().value("catalogRevision")}});
            ids.append(model.state().value("categorySelected").toMap().value("id").toString());
        }
        run(model,"preview_category",{{"action","set_income"},{"id",ids[1]},{"income",true},{"revision",model.state().value("catalogRevision")}});
        QVERIFY(click(root,"reportsNavigation"));QTRY_VERIFY(!model.busy());QVERIFY(click(root,"newReport"));
        auto selected=[&]{return value(workspace->property("current")).toMap().value("categories").toStringList();};
        auto* expenses=qobject_cast<QQuickItem*>(object(root,"reportExpensesGroup"));auto* income=qobject_cast<QQuickItem*>(object(root,"reportIncomeGroup"));QVERIFY(expenses);QVERIFY(income);
        QCOMPARE(expenses->property("checkState").toInt(),int(Qt::Checked));QCOMPARE(income->property("checkState").toInt(),int(Qt::Checked));
        // Actual key activation tests two-state group action and tri-state feedback.
        expenses->forceActiveFocus();QTest::keyClick(window,Qt::Key_Space);QCOMPARE(selected(),QStringList({"",ids[1]}));
        QCOMPARE(expenses->property("checkState").toInt(),int(Qt::Unchecked));
        QVERIFY(QMetaObject::invokeMethod(workspace,"check",Q_ARG(QVariant,"categories"),Q_ARG(QVariant,ids[0]),Q_ARG(QVariant,true)));
        QCOMPARE(expenses->property("checkState").toInt(),int(Qt::PartiallyChecked));
        if(qEnvironmentVariableIsSet("SPENTIT_INCOME_REPORT_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INCOME_REPORT_SCREENSHOT")));}
        auto* search=object(root,"reportChoiceSearch_categories");QVERIFY(search);search->setProperty("text","Zulu");
        auto* filteredGroup=qobject_cast<QQuickItem*>(object(root,"reportExpensesGroup"));QVERIFY(filteredGroup);
        filteredGroup->forceActiveFocus();QTest::keyClick(window,Qt::Key_Space);QCOMPARE(selected().size(),4);QCOMPARE(filteredGroup->property("checkState").toInt(),int(Qt::Checked));
        search->setProperty("text","");income=qobject_cast<QQuickItem*>(object(root,"reportIncomeGroup"));QVERIFY(income);
        auto* flick=object(root,"reportChoiceScroll_categories")->property("contentItem").value<QObject*>();QVERIFY(flick);
        QTRY_VERIFY(flick->property("contentHeight").toDouble()>flick->property("height").toDouble());QTest::qWait(50);
        flick->setProperty("contentY",flick->property("contentHeight").toDouble()-flick->property("height").toDouble());QTest::qWait(50);
        const auto incomePoint=income->mapToScene(QPointF(income->width()/2,income->height()/2)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,incomePoint);QVERIFY(!selected().contains(ids[1]));QVERIFY(selected().contains(""));
        if(qEnvironmentVariableIsSet("SPENTIT_INCOME_REPORT_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INCOME_REPORT_SCREENSHOT")+".income.png"));}
        QVERIFY(click(root,"clearReportExpenses"));QCOMPARE(selected(),QStringList({""}));
        QVERIFY(QMetaObject::invokeMethod(workspace,"checkGroup",Q_ARG(QVariant,true),Q_ARG(QVariant,true)));
        QVERIFY(QMetaObject::invokeMethod(workspace,"checkGroup",Q_ARG(QVariant,false),Q_ARG(QVariant,true)));
        search->setProperty("text","Zulu");
        QVERIFY(click(root,"clearReportIncome"));QVERIFY(!selected().contains(ids[1]));QVERIFY(selected().contains(ids[0]));QVERIFY(selected().contains(""));
        QVERIFY(click(root,"reportChoiceSearch_categoriesClear"));QCOMPARE(search->property("text").toString(),"");
        auto* bar=qobject_cast<QQuickItem*>(object(root,"reportChoiceBar_categories"));QVERIFY(bar);
        QVERIFY(bar->property("interactive").toBool());QVERIFY(bar->isVisible());
        auto* thumb=bar->property("contentItem").value<QQuickItem*>();QVERIFY(thumb);
        QTest::qWait(50);
        const auto thumbPoint=thumb->mapToScene(QPointF(thumb->width()/2,thumb->height()/2)).toPoint();
        if(qEnvironmentVariableIsSet("SPENTIT_INCOME_REPORT_SCREENSHOT")){QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INCOME_REPORT_SCREENSHOT")+".thumb.png"));}
        QTest::mouseMove(window,thumbPoint);QVERIFY(thumb->isVisible());
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,thumbPoint);
        QVERIFY(bar->property("pressed").toBool());
        QTest::mouseMove(window,bar->mapToScene(QPointF(bar->width()/2,2)).toPoint(),50);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,bar->mapToScene(QPointF(bar->width()/2,2)).toPoint());
        QTRY_VERIFY(flick->property("contentY").toDouble()<1);
        const auto savedSelection=selected();QVERIFY(QMetaObject::invokeMethod(workspace,"save",Q_ARG(QVariant,false),Q_ARG(QVariant,false)));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        // Reclassification changes grouping, never the saved report's explicit selection.
        run(model,"preview_category",{{"action","set_income"},{"id",ids[0]},{"income",true},{"revision",model.state().value("catalogRevision")}});
        auto unchanged=selected();auto original=savedSelection;unchanged.sort();original.sort();QCOMPARE(unchanged,original);QCOMPARE(object(root,"reportIncomeGroup")->property("checkState").toInt(),int(Qt::PartiallyChecked));
        run(model,"report_load");auto stored=model.state().value("reports").toList().front().toMap().value("categories").toStringList();auto expected=savedSelection;stored.sort();expected.sort();QCOMPARE(stored,expected);
        // A group action applies exactly that group, including colon names independently.
        QVERIFY(QMetaObject::invokeMethod(workspace,"checkGroup",Q_ARG(QVariant,true),Q_ARG(QVariant,false)));
        QVERIFY(!selected().contains(ids[0]) && !selected().contains(ids[1]) && selected().contains(ids[2]) && selected().contains(""));
        QVERIFY(warnings.isEmpty());
    }
    void editableReportLedger(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* workspace=object(root,"reportsWorkspace");
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        auto import=[&](const char* name,const QByteArray& bytes){QFile file(dir.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();run(model,"import",{{"path",file.fileName()},{"account",model.state().value("accountId")}});};
        import("one.qif","D2026-01-01\nT-100\nPFirst synthetic\n^\nD2026-02-01\nT-7\nPOutside period\n^\nD2026-01-03\nT-30\nPSynthetic splits\nS\n$-10\nS\n$-20\n^\n");
        const auto first=model.state().value("selected").toMap().value("batch");
        run(model,"commit");
        run(model,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});
        import("two.qif","D2026-01-02\nT-50\nPSecond synthetic\n^\n");
        const auto second=model.state().value("selected").toMap().value("batch");
        QVariantList categoryIds{""};
        for(int i=0;i<18;++i){
            run(model,"preview_category",{{"action","create"},{"name",QString("Synthetic category %1").arg(i+1)},{"revision",model.state().value("catalogRevision")}});
            categoryIds.push_back(model.state().value("categorySelected").toMap().value("id"));
        }
        QVariantMap definition{{"id","comparison"},{"name","Synthetic comparison"},{"graph","grouped_bar"},{"accounts",QVariantList{"1","2"}},{"categories",categoryIds},
            {"periods",QVariantList{QVariantMap{{"preset","custom"},{"start","2026-01-01"},{"end","2026-01-31"}},QVariantMap{{"preset","custom"},{"start","2026-01-02"},{"end","2026-01-31"}}}}};
        run(model,"report_save",{{"revision",model.state().value("reportRevision")},{"reports",QVariantList{definition}}});
        run(model,"report_show",{{"id","comparison"}});root->setProperty("currentPage","reports");
        auto* window=qobject_cast<QQuickWindow*>(root);window->resize(1300,850);window->show();QTest::qWait(100);
        auto* table=object(root,"reportCategoryTable");QVERIFY(table);QCOMPARE(table->property("height").toInt(),64+20*34+16);
        QVERIFY(object(root,"reportGraphs")->property("height").toInt()>260);
        window->grabWindow().save("/tmp/spentit-report-grouped.png");
        run(model,"report_drill",{{"currency",0},{"period",0},{"row",0}});
        QCOMPARE(model.rowCount(),3);QVERIFY(model.state().value("reportLedgerActive").toBool());QCOMPARE(workspace->property("mode").toString(),"detail");
        QTRY_VERIFY(object(root,"durableLedger")->property("visible").toBool());
        QCOMPARE(model.rowRecord(0).value("reportAccount").toString(),"Checking");
        QTest::qWait(50);window->grabWindow().save("/tmp/spentit-report-ledger.png");
        const auto splitRow=model.rowRecord(model.occurrenceIndex(first.toString(),2));QCOMPARE(splitRow.value("reportIncluded").toString(),"30.00");QCOMPARE(splitRow.value("reportAllocations").toStringList().size(),2);
        // Unsaved report edits use the same Save / Discard / Stay navigation guard.
        run(model,"inspect",{{"batch",first},{"ordinal",0}});
        auto* payee=object(root,"durablePayee");QVERIFY(payee);payee->setProperty("text","Unsaved correction");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(QMetaObject::invokeMethod(workspace,"back"));QVERIFY(object(root,"unsavedDialog")->property("visible").toBool());QVERIFY(click(root,"guardStay"));QCOMPARE(workspace->property("mode").toString(),"detail");
        QVERIFY(QMetaObject::invokeMethod(root,"discardEditor",Q_ARG(QVariant,QVariant())));
        // The current account is Savings; inspecting Checking routes the existing editor.
        run(model,"inspect",{{"batch",first},{"ordinal",0}});QCOMPARE(model.state().value("accountId").toString(),"1");
        auto edit=model.state().value("selected").toMap().value("draft").toMap();edit["date"]="2026-02-02";
        run(model,"accept_edit",edit);QCOMPARE(model.rowCount(),3);
        auto totals=model.state().value("reportResult").toMap().value("currencies").toList().front().toMap().value("totals").toList();QCOMPARE(totals.front().toString(),"80.00");
        const auto moved=model.rowRecord(model.occurrenceIndex(first.toString(),0));QVERIFY(!moved.value("reportMatches").toBool());
        run(model,"undo_edit");QCOMPARE(model.rowCount(),3);QCOMPARE(model.state().value("reportDetails").toMap().value("total").toString(),"180.00");
        // Scope cannot broaden with ordinary ledger search, and source searches work.
        run(model,"query",{{"filter","Outside"}});QCOMPARE(model.rowCount(),0);run(model,"query",{{"filter","QIF: Second"}});QCOMPARE(model.rowCount(),1);run(model,"query",{{"filter",""}});QCOMPARE(model.rowCount(),3);
        run(model,"preview_category",{{"action","create"},{"name","Excluded synthetic"},{"revision",model.state().value("catalogRevision")}});
        const auto category=model.state().value("categorySelected").toMap().value("id");QVERIFY(!category.toString().isEmpty());
        run(model,"inspect",{{"batch",first},{"ordinal",2}});
        const auto allocation=model.state().value("selected").toMap().value("draft").toMap().value("splits").toList().front().toMap().value("allocationId");
        run(model,"release_transfer_batch",{{"category",category},{"allocation",allocation}});
        QCOMPARE(model.rowRecord(model.occurrenceIndex(first.toString(),2)).value("reportIncluded").toString(),"20.00");QCOMPARE(model.state().value("reportDetails").toMap().value("total").toString(),"170.00");
        run(model,"undo_transfer_assignment");QCOMPARE(model.state().value("reportDetails").toMap().value("total").toString(),"180.00");
        run(model,"inspect",{{"batch",second},{"ordinal",0}});QCOMPARE(model.state().value("accountId").toString(),"2");
        edit=model.state().value("selected").toMap().value("draft").toMap();edit["payee"]="Corrected second";run(model,"save",edit);
        QCOMPARE(model.rowRecord(model.occurrenceIndex(second.toString(),0)).value("payee").toString(),"Corrected second");
        run(model,"select",{{"row",0}});run(model,"select",{{"row",model.occurrenceIndex(second.toString(),0)},{"toggle",true}});
        model.command("rename_payee_selection",{{"account",model.state().value("accountId")},{"generation",model.state().value("ledgerEditGeneration")},{"selected",model.state().value("selection")},{"payee","Must not apply"}});
        QTRY_VERIFY(!model.busy());QVERIFY(model.error().contains("one account"));QCOMPARE(model.rowRecord(model.occurrenceIndex(second.toString(),0)).value("payee").toString(),"Corrected second");
        run(model,"report_leave");QVERIFY(!model.state().value("reportLedgerActive").toBool());QCOMPARE(model.state().value("accountId").toString(),"2");
        run(model,"close");run(model,"open",{{"path",dir.filePath("document")}});QCOMPARE(model.state().value("reports").toList().front().toMap().value("graph").toString(),"grouped_bar");
        for(const auto& warning:warnings){const auto list=qvariant_cast<QList<QQmlError>>(warning.front());for(const auto& item:list)qWarning().noquote()<<item.toString();}
        QCOMPARE(warnings.size(),qsizetype(0));
    }
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
        QVERIFY(QMetaObject::invokeMethod(workspace,"back"));QTRY_VERIFY(!model.busy());QVERIFY(QMetaObject::invokeMethod(workspace,"back"));
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
