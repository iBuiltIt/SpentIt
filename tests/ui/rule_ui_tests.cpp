#include "import_model.hpp"
#include <spentit/money.hpp>
#include <spentit/storage_document_gateway.hpp>
#include "../storage/import_schema5_fixture.hpp"
#include <QTest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QJSValue>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFont>
#include <QGuiApplication>
#include <sqlite3.h>
using namespace spentit;
class RuleUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    struct Session {
        QTemporaryDir dir;
        ImportModel model;
        QQmlApplicationEngine engine;
        QObject* root=nullptr;
        QQuickWindow* window=nullptr;
        Session():model(storage::document_gateway()){
            engine.rootContext()->setContextProperty("workflow",&model);
            engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
            if(!engine.rootObjects().isEmpty()){root=engine.rootObjects().first();window=qobject_cast<QQuickWindow*>(root);}
        }
        QString path(const QString& name="document") const {return dir.filePath(name);}
    };
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}
    static QObject* object(QObject* root,const QString& name){if(auto* item=root->findChild<QObject*>(name))return item;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),name):nullptr;}
    static bool click(QObject* root,const QString& name){auto* item=object(root,name);return item && item->property("enabled").toBool() && item->property("visible").toBool() && QMetaObject::invokeMethod(item,"clicked");}
    static bool edit(QObject* root,const QString& name,const QString& value){auto* item=object(root,name);return item && item->setProperty("text",value) && QMetaObject::invokeMethod(item,"textEdited");}
    static bool choose(QObject* root,const QString& name,int index){auto* item=object(root,name);return item && item->setProperty("currentIndex",index) && QMetaObject::invokeMethod(item,"activated",Q_ARG(int,index));}
    static bool setChecked(QObject* root,const QString& name,bool on){auto* item=object(root,name);if(!item || !item->property("visible").toBool() || !item->property("enabled").toBool())return false;if(item->property("checked").toBool()==on)return true;return item->setProperty("checked",on) && QMetaObject::invokeMethod(item,"clicked");}
    void wait(ImportModel& m,bool success=true){QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty()==success,qPrintable(m.error()));}
    void run(ImportModel& m,const QString& action,const QVariantMap& args={},bool success=true){auto request=args;if(action=="import" && !request.contains("account"))request["account"]=m.state().value("accountId");m.command(action,request);wait(m,success);}
    static QVariantMap selected(const ImportModel& m){return m.state().value("selected").toMap();}
    static QVariantMap draftOf(const ImportModel& m){return selected(m).value("draft").toMap();}
    static QString page(QObject* root){return root->property("currentPage").toString();}
    void applyCategory(ImportModel& m,QObject* root){auto* field=qobject_cast<QQuickItem*>(object(root,"categoryName"));QVERIFY(field);field->forceActiveFocus();QTest::keyClick(qobject_cast<QQuickWindow*>(root),Qt::Key_Return);wait(m);QTRY_VERIFY(!root->property("categoryDirty").toBool());}
    QString createCategory(ImportModel& m,QObject* root,const QString& name){
        if(!click(root,"newCategory") || !edit(root,"categoryName",name))qFatal("Category form unavailable");
        applyCategory(m,root);return m.state().value("categorySelected").toMap().value("id").toString();
    }
    bool pickRuleCategory(QObject* root,const QString& id){
        auto* field=object(root,"ruleCategory");if(!field)return false;
        const auto value=field->property("model");
        const auto choices=value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();
        for(int i=0;i<choices.size();++i)if(choices[i].toMap().value("id").toString()==id)return choose(root,"ruleCategory",i);
        return false;
    }
    void transactionRule(QObject* root,const QString& label,const QString& expression,const QString& payee,const QString& category={}){
        if(!object(root,"newRule")->property("enabled").toBool()){
            QVERIFY(click(root,"saveRules"));QTRY_VERIFY_WITH_TIMEOUT(!root->property("rulesDirty").toBool(),30000);
        }
        QVERIFY(click(root,"newRule"));QVERIFY(edit(root,"ruleLabel",label));QVERIFY(edit(root,"ruleExpression",expression));
        if(!payee.isEmpty())QVERIFY(edit(root,"rulePayeeTemplate",payee));
        if(!category.isEmpty())QVERIFY(pickRuleCategory(root,category));else QVERIFY(choose(root,"ruleCategory",1));
    }
    void splitMemoRule(QObject* root,const QString& label,const QString& expression,const QString& category,bool inherit){
        if(!object(root,"newRule")->property("enabled").toBool()){
            QVERIFY(click(root,"saveRules"));QTRY_VERIFY_WITH_TIMEOUT(!root->property("rulesDirty").toBool(),30000);
        }
        QVERIFY(click(root,"newRule"));QVERIFY(edit(root,"ruleLabel",label));QVERIFY(edit(root,"ruleExpression",expression));
        QVERIFY(choose(root,"ruleScope",1));QCoreApplication::processEvents();QVERIFY(choose(root,"ruleInput",1));
        if(inherit){for(int n=0;n<20 && object(root,"ruleInherit") && !object(root,"ruleInherit")->property("visible").toBool();++n)QTest::qWait(20);QVERIFY(setChecked(root,"ruleInherit",true));}
        else if(!category.isEmpty())QVERIFY(pickRuleCategory(root,category));
    }
    static QString summary(const ImportModel& m){return m.state().value("rulePreview").toMap().value("summaryText").toString();}
    static void writeQif(const QString& path,const QByteArray& bytes){QFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())qFatal("QIF fixture unavailable");}
    static QByteArray wise(){return "!Type:Bank\nD01/01/2026\nT-29.31\nPTfl Travel UK\nEWise Charges for: card\n$-0.14\nECARD-123\n$-29.17\n^\n";}
    void createDocument(Session& s,const QString& name){run(s.model,"create",{{"path",s.path()},{"protected",false},{"name",name},{"currency","CAD"}});QVERIFY(s.window);QVERIFY(QTest::qWaitForWindowExposed(s.window));QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);createCategory(s.model,s.root,"Test category");QVERIFY(click(s.root,"accountNavigation"));wait(s.model);}
    void writeSchema5(const QString& path){
        QFile created(path);QVERIFY(created.open(QIODevice::WriteOnly));created.close();
        sqlite3* db=nullptr;QCOMPARE(sqlite3_open(path.toUtf8().constData(),&db),SQLITE_OK);
        const auto key=storage::public_document_key;
        QCOMPARE(sqlite3_key(db,key.data(),static_cast<int>(key.size())),SQLITE_OK);
        QCOMPARE(sqlite3_exec(db,"PRAGMA cipher_compatibility=4; PRAGMA cipher_memory_security=ON",nullptr,nullptr,nullptr),SQLITE_OK);
        QCOMPARE(sqlite3_exec(db,schema5_import_fixture,nullptr,nullptr,nullptr),SQLITE_OK);
        sqlite3_stmt* insert=nullptr;QCOMPARE(sqlite3_prepare_v2(db,"INSERT INTO account VALUES(1,'Synthetic',?,'2026-08-01',0,0,0,0)",-1,&insert,nullptr),SQLITE_OK);
        const auto definition=Currency("CAD").definition_id();
        QCOMPARE(sqlite3_bind_text(insert,1,definition.data(),static_cast<int>(definition.size()),SQLITE_TRANSIENT),SQLITE_OK);
        QCOMPARE(sqlite3_step(insert),SQLITE_DONE);QCOMPARE(sqlite3_finalize(insert),SQLITE_OK);QCOMPARE(sqlite3_close(db),SQLITE_OK);
    }
private slots:
    void initTestCase(){if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("RuleUi");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());}
    void init(){QSettings().clear();qunsetenv("SPENTIT_RULE_CANCEL_WINDOW");qunsetenv("SPENTIT_RULE_REFRESH_FAIL");}
    void currentFormatShowsRulesAndOldFormatIsRejected(){
        Session current;createDocument(current,"Current rules");
        QVERIFY(current.model.state().value("rulesSupported").toBool());
        QVERIFY(object(current.root,"rulesNavigation")->property("visible").toBool());
        run(current.model,"close");QSettings().clear();
        Session legacy;writeSchema5(legacy.path());
        run(legacy.model,"open",{{"path",legacy.path()}},false);
        QVERIFY(!legacy.model.state().value("active").toBool());
        QVERIFY(legacy.model.error().contains("This dataset format is not supported"));
    }
    void saveDiscardStayAndDraftPrecedesRules(){
        Session s;createDocument(s,"Synthetic rule guards");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);QCOMPARE(page(s.root),"rules");
        transactionRule(s.root,"Guard","^Guard","Kept");
        QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QVERIFY(click(s.root,"rulesGuardStay"));
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"accountNavigation"));QVERIFY(click(s.root,"rulesGuardDiscard"));wait(s.model);
        QCOMPARE(page(s.root),"ledger");QVERIFY(!s.root->property("rulesDirty").toBool());QCOMPARE(s.model.state().value("ruleRevision").toString(),"0");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Guard","^Guard","Kept");
        QVERIFY(click(s.root,"accountNavigation"));QVERIFY(click(s.root,"rulesGuardSave"));wait(s.model);
        QCOMPARE(page(s.root),"ledger");QVERIFY(!s.root->property("rulesDirty").toBool());QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);QVERIFY(click(s.root,"newRule"));
        QVERIFY(edit(s.root,"durablePayee","Temporary payee"));
        QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(object(s.root,"unsavedDialog")->property("visible").toBool());
        QVERIFY(!object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"guardDiscard"));
        QVERIFY(object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"rulesGuardStay"));
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(edit(s.root,"durablePayee","Temporary payee"));
        QVERIFY(QMetaObject::invokeMethod(s.window,"close"));
        QVERIFY(object(s.root,"unsavedDialog")->property("visible").toBool());
        QVERIFY(!object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"guardDiscard"));
        QVERIFY(object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"rulesGuardStay"));
        QVERIFY(s.window->isVisible());QVERIFY(s.model.state().value("active").toBool());
    }
    void previewApplyBlankPayeeCommitAndSourceQuote(){
        Session s;createDocument(s,"Split first import");
        QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);
        const auto transport=createCategory(s.model,s.root,"Travel: Transport");
        const auto fees=createCategory(s.model,s.root,"Fees");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Travel","^Tfl","Shared payee must stay",transport);
        splitMemoRule(s.root,"Fee","^Wise",fees,false);
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto file=s.path("split.qif");writeQif(file,wise());run(s.model,"import",{{"path",file},{"profile","DMY"}});
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        const auto splits=draftOf(s.model).value("splits").toList();QCOMPARE(splits.size(),2);
        QCOMPARE(splits[0].toMap().value("categoryDisplay").toString(),"Fees");QCOMPARE(splits[1].toMap().value("categoryDisplay").toString(),"Travel: Transport");
        QCOMPARE(splits[0].toMap().value("amount").toString(),"-0.14");QCOMPARE(splits[1].toMap().value("amount").toString(),"-29.17");
        const auto batch=selected(s.model).value("batch");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("matched").toInt(),0);
        QVERIFY(!object(s.root,"replaceManualFields"));QVERIFY(!object(s.root,"ruleInherit"));
        run(s.model,"commit");run(s.model,"close");run(s.model,"open",{{"path",s.path()}});run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(draftOf(s.model).value("splits").toList()[0].toMap().value("categoryDisplay").toString(),"Fees");
    }
    void missingImportCategoryDialogSurvivesRestart(){
        Session s;createDocument(s,"Missing categories");
        const auto file=s.path("categories.qif");writeQif(file,"!Type:Bank\nD1/1/2026\nT-1\nPSynthetic\nLMissing One\n^\nD2/1/2026\nT-2\nPOther\nLMissing Two\n^\n");
        run(s.model,"import",{{"path",file},{"profile","DMY"}});
        QTRY_VERIFY(object(s.root,"importCategories")->property("visible").toBool());
        QCOMPARE(s.model.state().value("importCategoryDecisions").toList().size(),1);
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"importCategories"),"close"));
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        QTRY_VERIFY(object(s.root,"importCategories")->property("visible").toBool());
        QVERIFY(!s.model.state().value("startupImportCleanup").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_SCREENSHOT"))QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_SCREENSHOT")));
        QVERIFY(click(s.root,"uncategorizedImport"));wait(s.model);
        QTRY_VERIFY(!object(s.root,"importCategories")->property("visible").toBool());
        QVERIFY(s.model.state().value("importCategoryDecisions").toList().empty());
        run(s.model,"commit");
    }
    void simplePatternsAndWholeFieldKey(){
        Session s;createDocument(s,"Pattern assistance");QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Synthetic rule","^STARBUCKS$","Coffee");
        auto expression=[&](){return object(s.root,"ruleExpression")->property("text").toString();};
        auto whole=[&](){return object(s.root,"ruleWholeField")->property("checked").toBool();};
        QVERIFY(whole());QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"STARBUCKS");
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^STARBUCKS$");
        QVERIFY(edit(s.root,"ruleExpression","^\\QAMZN Mktp CA*TEST\\E$"));QVERIFY(whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"\\QAMZN Mktp CA*TEST\\E");
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^\\QAMZN Mktp CA*TEST\\E$");
        QVERIFY(edit(s.root,"ruleExpression","^(?:\\QShop (West)*\\E)$"));QVERIFY(whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"\\QShop (West)*\\E");
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^\\QShop (West)*\\E$");
        QVERIFY(edit(s.root,"ruleExpression","Amazon|AMZN"));QVERIFY(!whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^(?:Amazon|AMZN)$");
        QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"Amazon|AMZN");
        QVERIFY(edit(s.root,"ruleExpression","^Amazon|AMZN$"));QVERIFY(!whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^(?:^Amazon|AMZN$)$");
        QVERIFY(edit(s.root,"ruleExpression","^[A|B]$"));QVERIFY(whole());
        QVERIFY(edit(s.root,"ruleExpression","^cost\\$"));QVERIFY(!whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^cost\\$$");
        QVERIFY(edit(s.root,"ruleExpression","^(Amazon|AMZN)$"));QVERIFY(whole());
        QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"(Amazon|AMZN)");
        QVERIFY(setChecked(s.root,"ruleWholeField",true));QCOMPARE(expression(),"^(Amazon|AMZN)$");
        QVERIFY(setChecked(s.root,"ruleCaseSensitive",true));QCOMPARE(expression(),"(?-i)^(Amazon|AMZN)$");
        QVERIFY(setChecked(s.root,"ruleWholeField",false));QCOMPARE(expression(),"(?-i)(Amazon|AMZN)");
        QVERIFY(edit(s.root,"ruleExpression","\\Qunfinished"));QVERIFY(setChecked(s.root,"ruleWholeField",true));QVERIFY(!whole());QCOMPARE(expression(),"\\Qunfinished");
        QVERIFY(edit(s.root,"ruleExpression","^STARBUCKS$"));
        QVERIFY(!object(s.root,"ruleAssistNote")->property("visible").toBool());
        auto* key=object(s.root,"rulePatternKey");QVERIFY(key);QVERIFY(key->property("text").toString().contains("\\Q…\\E literal text"));
        QTRY_VERIFY(key->property("lineCount").toInt()>=1 && key->property("lineCount").toInt()<=2);
        s.window->resize(900,850);QTRY_COMPARE(key->property("lineCount").toInt(),2);
        if(qEnvironmentVariableIsSet("SPENTIT_PATTERN_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_PATTERN_SCREENSHOT")));}
    }
    void newRuleInLongListIsVisible(){
        Session s;createDocument(s,"Long rule list");
        QVariantList definitions;
        for(int i=0;i<25;i++)definitions.push_back(QVariantMap{
            {"id",s.model.newIdentity()},{"enabled",true},{"label",QString("Synthetic rule %1").arg(i)},
            {"scope","transaction"},{"input","payee"},{"expression",QString("^Synthetic %1$").arg(i)},
            {"payeeTemplate","Synthetic result"},{"categoryId",s.model.state().value("categories").toList().front().toMap().value("id")}});
        run(s.model,"rules_save",{{"rules",definitions}});
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVERIFY(click(s.root,"newRule"));QTest::qWait(100);
        QCOMPARE(s.root->property("ruleIndex").toInt(),25);
        auto* row=qobject_cast<QQuickItem*>(object(s.root,"ruleRow_25"));QVERIFY(row);
        auto* name=qobject_cast<QQuickItem*>(object(s.root,"ruleLabel"));QVERIFY(name);
        auto* apply=qobject_cast<QQuickItem*>(object(s.root,"applyRules"));QVERIFY(apply);
        QVERIFY(row->mapToScene(QPointF(0,0)).y()>=0);
        QVERIFY(name->mapToScene(QPointF(0,0)).y()>row->mapToScene(QPointF(0,0)).y());
        if(qEnvironmentVariableIsSet("SPENTIT_LONG_RULE_SCREENSHOT"))QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_LONG_RULE_SCREENSHOT")));
        QVERIFY2(apply->mapToScene(QPointF(0,0)).y()+apply->height()<s.window->height(),qPrintable(QString("row=%1 name=%2 apply=%3 height=%4").arg(row->mapToScene(QPointF(0,0)).y()).arg(name->mapToScene(QPointF(0,0)).y()).arg(apply->mapToScene(QPointF(0,0)).y()).arg(s.window->height())));
        QVERIFY(!object(s.root,"newRule")->property("enabled").toBool());
        QVERIFY(!object(s.root,"allRules"));QVERIFY(!object(s.root,"previewRules"));
        if(qEnvironmentVariableIsSet("SPENTIT_LONG_RULE_SCREENSHOT"))QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_LONG_RULE_SCREENSHOT")));
    }
    void independentDraftAndSelectedSaveApply(){
        Session s;createDocument(s,"Independent rule editing");
        const auto source=s.path("rides.qif");
        writeQif(source,"!Type:Bank\nD01/01/2026\nT-12.00\nPRide North\n^\n");
        run(s.model,"import",{{"path",source},{"profile","DMY"}});run(s.model,"commit");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Ride","^Ride South$","Travel");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Other","^Other$","Other result");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"newRule"));
        QVERIFY(edit(s.root,"ruleLabel","Unfinished draft"));
        QVERIFY(edit(s.root,"ruleExpression","^Ride North$"));
        QVERIFY(!object(s.root,"newRule")->property("enabled").toBool());
        QVERIFY(object(s.root,"ruleRow_2")->property("changed").toBool());
        QVERIFY(object(s.root,"ruleRow_2")->property("isNew").toBool());
        QVERIFY(edit(s.root,"ruleSearch","Ride"));
        QVERIFY(!object(s.root,"ruleRow_1")->property("visible").toBool());
        QVERIFY(object(s.root,"ruleRow_0")->property("visible").toBool());
        QVERIFY(click(s.root,"ruleSearchClear"));
        QCOMPARE(object(s.root,"ruleSearch")->property("text").toString(),QString());
        QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(edit(s.root,"ruleExpression","^Ride"));
        QVERIFY(object(s.root,"ruleRow_0")->property("changed").toBool());
        // No Preview step: save and apply only the edited saved rule.
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QTRY_VERIFY(s.model.state().value("canUndoRules").toBool());
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        QCOMPARE(s.model.state().value("rules").toList().size(),2);
        QCOMPARE(s.model.state().value("rules").toList()[0].toMap().value("expression").toString(),"^Ride");
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(object(s.root,"ruleRow_2")->property("isNew").toBool());
        QVERIFY(!object(s.root,"ruleRow_0")->property("changed").toBool());
        QVERIFY(click(s.root,"openRule_1"));QVERIFY(edit(s.root,"ruleLabel","Discard this change"));
        QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),1);
        QCOMPARE(s.root->property("unsavedRuleCount").toInt(),2);
        QTRY_VERIFY(object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QTest::qWait(50);
        auto* dialog=qobject_cast<QQuickWindow*>(object(s.root,"rulesLeaveDialog"));
        QVERIFY(dialog);
        QCOMPARE(dialog->modality(),Qt::ApplicationModal);
        QVERIFY(dialog->flags().testFlag(Qt::WindowTitleHint));
        auto* content=qobject_cast<QQuickItem*>(object(s.root,"rulesLeaveContent"));
        QVERIFY(content);
        QTRY_COMPARE(dialog->height(),qRound(content->implicitHeight()+40));
        QCOMPARE(content->y(),20.0);
        QCOMPARE(dialog->transientParent(),s.window);
        if(QGuiApplication::platformName()=="cocoa")QVERIFY(dialog->frameMargins().top()>0);
        const auto position=dialog->position();
        dialog->setPosition(position+QPoint(60,40));
        QCOMPARE(dialog->position(),position+QPoint(60,40));
        dialog->close();
        QTRY_VERIFY(!dialog->isVisible());
        QCOMPARE(page(s.root),"rules");
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"accountNavigation"));
        QTRY_VERIFY(dialog->isVisible());
        dialog->requestActivate();
        QTest::qWait(50);
        QTest::keyClick(dialog,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->isVisible());
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"accountNavigation"));
        QTRY_VERIFY(dialog->isVisible());
        QCOMPARE(object(s.root,"rulesGuardSave")->property("text").toString(),QString("Save"));
        QCOMPARE(object(s.root,"rulesGuardDiscard")->property("text").toString(),QString("Discard"));
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_SCREENSHOT"))QVERIFY(dialog->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_SCREENSHOT")));
        QVERIFY(click(s.root,"rulesGuardStay"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),1);
        QVERIFY(click(s.root,"accountNavigation"));QVERIFY(click(s.root,"rulesGuardDiscard"));wait(s.model);
        QCOMPARE(page(s.root),"ledger");
        QVERIFY(!dialog->isVisible());
        // Qt quit closes even hidden windows that still have a native handle.
        // A dismissed confirmation must not veto that close.
        QVERIFY(dialog->close());
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        const auto appliedBatch=selected(s.model).value("batch");
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        run(s.model,"inspect",{{"batch",appliedBatch},{"ordinal",0}});
        QCOMPARE(s.model.state().value("rules").toList().size(),2);
        QCOMPARE(s.model.state().value("rules").toList()[0].toMap().value("expression").toString(),"^Ride");
        QCOMPARE(s.model.state().value("rules").toList()[1].toMap().value("label").toString(),"Other");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
    }
    void quitAfterDiscardedRule(){
        if(!qEnvironmentVariableIsSet("SPENTIT_QUIT_RULE_CHILD")){
            QProcess child;
            auto environment=QProcessEnvironment::systemEnvironment();
            environment.insert("SPENTIT_QUIT_RULE_CHILD","1");
            child.setProcessEnvironment(environment);
            child.start(QCoreApplication::applicationFilePath(),{"quitAfterDiscardedRule"});
            QVERIFY(child.waitForStarted());
            QVERIFY2(child.waitForFinished(30000),"Quit did not terminate the test process");
            const auto output=child.readAllStandardOutput()+child.readAllStandardError();
            QCOMPARE(child.exitStatus(),QProcess::NormalExit);
            QVERIFY2(child.exitCode()==0,output.constData());
            return;
        }
        Session s;createDocument(s,"Quit after rule discard");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Discard before quit","^Synthetic$","Synthetic");
        QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(click(s.root,"rulesGuardDiscard"));wait(s.model);
        QCOMPARE(page(s.root),"ledger");
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout,&QTimer::timeout,[]{QCoreApplication::exit(87);});
        timeout.start(15000);
        QTimer::singleShot(0,[]{QCoreApplication::quit();});
        QCOMPARE(QGuiApplication::exec(),0);
        QVERIFY(!s.model.state().value("active").toBool());
    }
    void saveAllLeaveIsAtomicAndSelectsInvalidRule(){
        Session s;createDocument(s,"Save all rules");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"First","^First$","First result");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Second","^Second$","Second result");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto revision=s.model.state().value("ruleRevision");
        QVERIFY(click(s.root,"openRule_0"));QVERIFY(edit(s.root,"ruleLabel","First changed"));
        QVERIFY(click(s.root,"openRule_1"));QVERIFY(edit(s.root,"ruleExpression","["));
        QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),1);
        QVERIFY(click(s.root,"rulesGuardSave"));wait(s.model,false);
        QTRY_COMPARE(s.root->property("ruleIndex").toInt(),1);
        QCOMPARE(s.model.state().value("ruleRevision"),revision);
        QCOMPARE(s.model.state().value("rules").toList()[0].toMap().value("label").toString(),"First");
        QVERIFY(!object(s.root,"rulesLeaveDialog")->property("visible").toBool());
        QVERIFY(edit(s.root,"ruleExpression","^Second changed$"));
        QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(click(s.root,"rulesGuardSave"));wait(s.model);
        QTRY_COMPARE(page(s.root),"ledger");
        QVERIFY(!s.root->property("rulesDirty").toBool());
        QCOMPARE(s.model.state().value("rules").toList()[0].toMap().value("label").toString(),"First changed");
        QCOMPARE(s.model.state().value("rules").toList()[1].toMap().value("expression").toString(),"^Second changed$");
        QVERIFY(!s.model.state().value("canUndoRules").toBool());
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Saved new","^New$","New result");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(object(s.root,"newRule")->property("enabled").toBool());
        QVERIFY(!object(s.root,"ruleRow_2")->property("isNew").toBool());
    }
    void restoredTextCanBeRuledAgain_data(){
        QTest::addColumn<bool>("committed");
        QTest::newRow("new")<<false;QTest::newRow("committed")<<true;
    }
    void restoredTextCanBeRuledAgain(){
        QFETCH(bool,committed);
        Session s;createDocument(s,"Restore rule regression");
        const auto path=s.path("restore.qif");
        writeQif(path,"!Type:Bank\nD01/01/2026\nT-3.50\nPParking original\nMOriginal memo\n^\n");
        run(s.model,"import",{{"path",path},{"profile","DMY"}});
        const auto batch=selected(s.model).value("batch");
        if(committed)run(s.model,"commit");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        if(s.root->property("ruleIndex").toInt()<0 && object(s.root,"openRule_0"))QVERIFY(click(s.root,"openRule_0"));
        transactionRule(s.root,"Parking","^Parking","Parking renamed");
        QVERIFY(edit(s.root,"ruleMemoTemplate","Rule memo"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking renamed");
        auto restore=[&]{
            QVERIFY(click(s.root,"accountNavigation"));wait(s.model);
            run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
            QVERIFY(QMetaObject::invokeMethod(s.root,"restoreImportedValues"));
            QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
            QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking original");
            QCOMPARE(draftOf(s.model).value("memo").toString(),"Original memo");
        };
        restore();
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        if(s.root->property("ruleIndex").toInt()<0 && object(s.root,"openRule_0"))QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);
        QVERIFY(summary(s.model).contains("1 eligible; 1 will change"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking renamed");
        QCOMPARE(draftOf(s.model).value("memo").toString(),"Rule memo");
        QVERIFY(click(s.root,"undoRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking original");
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking renamed");
        // Explicit typing after Restore, even typing the original value, stays manual.
        QVERIFY(click(s.root,"accountNavigation"));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(s.root,"restoreImportedValues"));
        QVERIFY(edit(s.root,"durablePayee","Parking original"));
        QVERIFY(edit(s.root,"durableMemo","My memo"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        if(s.root->property("ruleIndex").toInt()<0 && object(s.root,"openRule_0"))QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking original");
        QCOMPARE(draftOf(s.model).value("memo").toString(),"My memo");
        restore(); // Also release manual protection through explicit Restore.
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        if(s.root->property("ruleIndex").toInt()<0 && object(s.root,"openRule_0"))QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Parking renamed");
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
    }
    void restoredSplitMemoCanBeRuledAgain(){
        Session s;createDocument(s,"Restore split memo");
        const auto path=s.path("split.qif");writeQif(path,wise());
        run(s.model,"import",{{"path",path},{"profile","DMY"}});
        const auto batch=selected(s.model).value("batch");run(s.model,"commit");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        splitMemoRule(s.root,"Fee","Wise Charges",{},false);
        QVERIFY(choose(s.root,"ruleCategory",1));QVERIFY(edit(s.root,"ruleMemoTemplate","Fee renamed"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        auto memo=[&]{return draftOf(s.model).value("splits").toList()[0].toMap().value("memo").toString();};
        QCOMPARE(memo(),"Fee renamed");
        QVERIFY(click(s.root,"accountNavigation"));wait(s.model);
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(QMetaObject::invokeMethod(s.root,"restoreImportedValues"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        QCOMPARE(memo(),"Wise Charges for: card");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);QCOMPARE(memo(),"Fee renamed");
        QVERIFY(click(s.root,"undoRules"));wait(s.model);QCOMPARE(memo(),"Wise Charges for: card");
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});QCOMPARE(memo(),"Wise Charges for: card");
    }
    void accountScopeSaveAndApplyUndo(){
        Session s;createDocument(s,"Account rule scopes");
        const auto old=s.path("old.qif"),fresh=s.path("new.qif");
        writeQif(old,"!Type:Bank\nD01/01/2026\nT-1.00\nPRide old\n^\n");run(s.model,"import",{{"path",old},{"profile","DMY"}});
        const auto oldBatch=selected(s.model).value("batch").toString();run(s.model,"commit");
        writeQif(fresh,"!Type:Bank\nD02/01/2026\nT-2.00\nPRide new\n^\n");run(s.model,"import",{{"path",fresh},{"profile","DMY"}});
        const auto newBatch=selected(s.model).value("batch").toString();
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);transactionRule(s.root,"Ride","^Ride","Travel");
        QCOMPARE(object(s.root,"ruleTransactionScope")->property("currentIndex").toInt(),0);
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QCOMPARE(summary(s.model),"2 transactions match. 0 already categorized. 0 handled by earlier rules. 2 eligible; 2 will change.");
        QVERIFY(choose(s.root,"ruleTransactionScope",1));QVERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QCOMPARE(summary(s.model),"1 transactions match. 0 already categorized. 0 handled by earlier rules. 1 eligible; 1 will change.");
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply");
        QVERIFY(click(s.root,"applyRules"));wait(s.model);QTRY_VERIFY(s.model.state().value("canUndoRules").toBool());
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        run(s.model,"inspect",{{"batch",oldBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride old");
        QVERIFY(click(s.root,"undoRules"));wait(s.model);
        run(s.model,"inspect",{{"batch",newBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride new");
        QVERIFY(choose(s.root,"ruleTransactionScope",0));QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QCOMPARE(summary(s.model),"2 transactions match. 0 already categorized. 0 handled by earlier rules. 2 eligible; 2 will change.");
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        run(s.model,"inspect",{{"batch",oldBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        QVERIFY(click(s.root,"undoRules"));wait(s.model);QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride old");
        run(s.model,"inspect",{{"batch",newBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride new");
    }
    void focusedCreationPreviewAndAttribution(){
        Session s;createDocument(s,"Focused rule flow");
        QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);
        const auto category=createCategory(s.model,s.root,"Transport");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Other rule","^Other","Other result");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("simple.qif");writeQif(qif,"!Type:Bank\nD01/01/2026\nT-12.00\nPSynthetic Shop\n^\n");
        run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        QVERIFY(edit(s.root,"splitCategory_0","Transport"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"splitCategory_0"),"acceptChoice"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        const auto original=draftOf(s.model);
        QCOMPARE(original.value("splits").toList()[0].toMap().value("categoryId").toString(),category);
        if(page(s.root)!="ledger")QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(object(s.root,"transactionRuleAction")->property("text").toString(),"Create rule…");
        QVERIFY(click(s.root,"transactionRuleAction"));wait(s.model);
        QVERIFY(object(s.root,"ruleEditor")->property("visible").toBool());
        QCOMPARE(s.root->property("ruleCategoryId").toString(),category);
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Synthetic Shop");
        QVERIFY(!object(s.root,"previewRules"));
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_SCREENSHOT")));}
        QVERIFY(edit(s.root,"rulePayeeTemplate","Shop"));
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);
        auto result=s.model.state().value("rulePreview").toMap();
        QCOMPARE(result.value("evaluated").toInt(),1);QVERIFY(result.value("operation").toString().isEmpty());
        QVERIFY(!summary(s.model).contains("Other rule"));QCOMPARE(draftOf(s.model),original);
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(object(s.root,"ruleEditor")->property("visible").toBool());QVERIFY(s.root->property("ruleIndex").toInt()>=0);
        QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(edit(s.root,"splitCategory_0",""));QVERIFY(QMetaObject::invokeMethod(object(s.root,"splitCategory_0"),"acceptChoice"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);QVERIFY(click(s.root,"openRule_1"));
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QVERIFY(!s.model.state().value("rulePreview").toMap().value("operation").toString().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QVERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);
        QVERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        const auto batch=selected(s.model).value("batch").toString();
        QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(object(s.root,"transactionRuleAction")->property("text").toString(),"Edit rule…");
        QCOMPARE(object(s.root,"ruleWinners")->property("text").toString(),QString::fromUtf8("Payee assigned by “Synthetic Shop”\nCategory assigned by “Synthetic Shop”"));
        QTest::qWait(30);
        auto* action=qobject_cast<QQuickItem*>(object(s.root,"transactionRuleAction"));
        auto* attribution=qobject_cast<QQuickItem*>(object(s.root,"ruleWinners"));
        QVERIFY(action && attribution);
        QVERIFY(action->x()<attribution->x());
        QCOMPARE(action->x(),0.0);
        QVERIFY(click(s.root,"transactionRuleAction"));
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Synthetic Shop");
        QVERIFY(edit(s.root,"ruleExpression","^No longer matches$"));QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"accountNavigation"));run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(object(s.root,"transactionRuleAction")->property("text").toString(),"Edit rule…");
        QVERIFY(click(s.root,"transactionRuleAction"));QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Synthetic Shop");
        QVERIFY(click(s.root,"deleteRule_1"));QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"accountNavigation"));run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(object(s.root,"transactionRuleAction")->property("text").toString(),"Create rule…");
        const auto splitFile=s.path("split.qif");writeQif(splitFile,wise());
        run(s.model,"import",{{"path",splitFile},{"profile","DMY"}});
        QVERIFY(edit(s.root,"splitCategory_0","Transport"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"splitCategory_0"),"acceptChoice"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        const auto splitBefore=draftOf(s.model);
        QVERIFY(click(s.root,"transactionRuleAction"));QVERIFY(object(s.root,"ruleSplitDialog")->property("visible").toBool());
        QVERIFY(click(s.root,"ruleSplitChoice_0"));wait(s.model);
        QCOMPARE(object(s.root,"ruleScope")->property("currentIndex").toInt(),1);
        QCOMPARE(object(s.root,"ruleInput")->property("currentIndex").toInt(),1);
        QCOMPARE(s.root->property("ruleCategoryId").toString(),category);
        QCOMPARE(object(s.root,"ruleExpression")->property("text").toString(),"^Wise Charges for: card$");
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);
        QCOMPARE(draftOf(s.model),splitBefore);
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("records").toList().size(),0);
    }
    void selectedSavePreservesUnrelatedReordering(){
        Session s;createDocument(s,"Independent rule order");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"First","^First","First result");
        transactionRule(s.root,"Second","^Second","Second result");
        transactionRule(s.root,"Third","^Third","Third result");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"moveRuleDown_0"));
        QVERIFY(click(s.root,"moveRuleDown_1"));
        // Local order is Second, Third, First. Editing Second must not save First's move.
        QVERIFY(click(s.root,"openRule_0"));QVERIFY(edit(s.root,"ruleLabel","Second changed"));
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto saved=s.model.state().value("rules").toList();
        QCOMPARE(saved[0].toMap().value("label").toString(),"First");
        QCOMPARE(saved[1].toMap().value("label").toString(),"Second changed");
        QCOMPARE(saved[2].toMap().value("label").toString(),"Third");
        QVERIFY(object(s.root,"openRule_0")->property("text").toString().startsWith("Second changed"));
        QVERIFY(object(s.root,"openRule_2")->property("text").toString().startsWith("First"));
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),0);
        QVERIFY(click(s.root,"rulesGuardStay"));
        QVERIFY(click(s.root,"openRule_2"));
        QVERIFY(click(s.root,"discardRules"));
        QVERIFY(!s.root->property("rulesDirty").toBool());
        QVERIFY(object(s.root,"openRule_0")->property("text").toString().startsWith("First"));
        QCOMPARE(s.model.state().value("rules").toList(),saved);
    }
    void savedRulesNavigationAndEnabledChanges(){
        Session s;createDocument(s,"Rule navigation regression");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"First","^First","First result");
        transactionRule(s.root,"Second","^Second","Second result");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QCOMPARE(s.root->property("ruleIndex").toInt(),-1);
        QVERIFY(!object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(setChecked(s.root,"ruleEnabled_0",false));
        QVERIFY(setChecked(s.root,"ruleEnabled_1",false));
        QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(!s.model.state().value("rules").toList()[0].toMap().value("enabled").toBool());
        QVERIFY(s.model.state().value("rules").toList()[1].toMap().value("enabled").toBool());
        QVERIFY(click(s.root,"accountNavigation"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),0);
        QVERIFY(click(s.root,"rulesGuardDiscard"));wait(s.model);
        QVERIFY(!s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVERIFY(!object(s.root,"ruleEnabled_0")->property("checked").toBool());
        QVERIFY(object(s.root,"ruleEnabled_1")->property("checked").toBool());
    }
    void removedRuleKeepsEarlierAssignments(){
        Session s;createDocument(s,"Synthetic obsolete rule");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Echo","^Tfl","Tfl Travel UK");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("wise.qif");writeQif(qif,wise());
        run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        if(page(s.root)!="rules"){QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);}
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QCOMPARE(summary(s.model),"0 transactions match. 0 already categorized. 0 handled by earlier rules. 0 eligible; 0 will change.");
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("changed").toInt(),0);
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
    }
    void enabledPreviewOnlyShowsMatchingRows(){
        Session s;createDocument(s,"Matching rows");
        const auto qif=s.path("matches.qif");writeQif(qif,"!Type:Bank\nD01/01/2026\nT-1.00\nPShop\n^\nD02/01/2026\nT-2.00\nPCoffee\n^\nD03/01/2026\nT-3.00\nPRide\n^\nD04/01/2026\nT-4.00\nPUnrelated\n^\n");
        run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Shop","^Shop$","Shop result");
        transactionRule(s.root,"Coffee","^Coffee$","Coffee result");
        transactionRule(s.root,"Ride","^Ride$","Ride result");
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);QCOMPARE(summary(s.model),"3 transactions match. 0 already categorized. 0 handled by earlier rules. 3 eligible; 3 will change.");
        QVERIFY(setChecked(s.root,"ruleEnabled_0",false));QVERIFY(setChecked(s.root,"ruleEnabled_1",false));
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);QCOMPARE(summary(s.model),"1 transactions match. 0 already categorized. 0 handled by earlier rules. 1 eligible; 1 will change.");
        const auto rows=s.model.state().value("rulePreview").toMap().value("records").toList();
        QCOMPARE(rows.size(),1);QCOMPARE(rows[0].toMap().value("payee").toString(),"Ride");
        QCOMPARE(object(s.root,"ruleMatches")->property("count").toInt(),1);
        QVERIFY(!object(s.root,"ruleApplyHelp")->property("visible").toBool());
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QVERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_MATCH_COMPACT_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_MATCH_COMPACT_SCREENSHOT")));}
        QVERIFY(setChecked(s.root,"ruleEnabled_2",false));QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QCOMPARE(summary(s.model),"0 transactions match. 0 already categorized. 0 handled by earlier rules. 0 eligible; 0 will change.");QVERIFY(!object(s.root,"ruleMatches")->property("visible").toBool());
    }
    void largeMatchListIsVirtualized(){
        Session s;createDocument(s,"Large match list");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVariantList rows;rows.reserve(100000);
        for(int i=0;i<100000;++i)rows.push_back(QVariantMap{{"date","2026-08-10"},{"payee",i==99999?QString("Synthetic\nmatch 99999"):QString("Synthetic match %1").arg(i)},{"amount","-1.00 CAD"}});
        auto state=s.model.state();state["rulePreview"]=QVariantMap{{"label","All enabled rules · all transactions in this account"},{"summaryText","100000 matching transactions"},{"records",rows},{"matched",100000}};
        QElapsedTimer timer;timer.start();QVERIFY(s.root->setProperty("s",state));
        QVariant previewKey;QVERIFY(QMetaObject::invokeMethod(s.root,"rulesPreviewKey",Q_RETURN_ARG(QVariant,previewKey)));
        QVERIFY(s.root->setProperty("rulePreviewKey",previewKey));QTest::qWait(100);
        auto* list=object(s.root,"ruleMatches");QVERIFY(list);QCOMPARE(list->property("count").toInt(),100000);
        auto* content=qobject_cast<QQuickItem*>(list->property("contentItem").value<QObject*>());QVERIFY(content);
        QVERIFY(content->childItems().size()<100);
        QVERIFY(QMetaObject::invokeMethod(list,"positionViewAtEnd"));QTest::qWait(100);
        QVERIFY(content->childItems().size()<100);
        auto* last=object(s.root,"ruleMatchPayee_99999");QVERIFY(last);
        QCOMPARE(last->property("text").toString(),"Synthetic match 99999");QCOMPARE(last->property("lineCount").toInt(),1);
        qInfo()<<"100000 match rows rendered and scrolled in"<<timer.elapsed()<<"ms; visible items"<<content->childItems().size();
        if(qEnvironmentVariableIsSet("SPENTIT_MATCH_SCREENSHOT"))QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_MATCH_SCREENSHOT")));
    }
    void busyPreviewIgnoresSave(){
        Session s;createDocument(s,"Synthetic busy rules");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Kept","^Tfl","Kept");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("wise.qif");writeQif(qif,wise());run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        const auto pending=s.model.state().value("pending").toMap();
        s.model.command("rules_preview",{{"unsaved",false},{"replaceManual",false},{"batch",pending.value("id")},{"batchRevision",pending.value("revision").toString()}});
        QVERIFY(s.model.busy());
        s.model.command("rules_save",{{"rules",QVariantList{}}});
        wait(s.model);
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QCOMPARE(s.model.state().value("rules").toList().size(),qsizetype(1));
    }
    void cancelPreviewLeavesRulesAndDrafts(){
        Session s;createDocument(s,"Synthetic cancel rules");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Kept","^Tfl","Kept");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("wise.qif");writeQif(qif,wise());run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        const auto pending=s.model.state().value("pending").toMap();
        qputenv("SPENTIT_RULE_CANCEL_WINDOW","1");
        s.model.command("rules_preview",{{"unsaved",false},{"replaceManual",false},{"batch",pending.value("id")},{"batchRevision",pending.value("revision").toString()}});
        QVERIFY(s.model.canCancel());s.model.requestCancel();
        wait(s.model,false);
        QVERIFY(s.model.error().contains("Operation cancelled"));QVERIFY(s.model.error().contains("unchanged"));
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        QVERIFY(s.model.state().value("active").toBool());
    }
    void applyRefreshFailureClosesWithDurableReceipt(){
        Session s;createDocument(s,"Synthetic refresh rules");
        const auto qif=s.path("wise.qif");writeQif(qif,"!Type:Bank\nD1/1/2026\nT-1\nPTfl Travel UK\n^\n");run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Echoed","^Tfl","Echoed");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto batch=s.model.state().value("pending").toMap().value("id").toString();QVERIFY(!batch.isEmpty());
        if(page(s.root)!="rules"){QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);}
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QTRY_VERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        qputenv("SPENTIT_RULE_REFRESH_FAIL","1");
        QVERIFY(click(s.root,"applyRules"));wait(s.model,false);
        const auto id=s.model.state().value("ruleReceipt").toMap().value("operation").toString();
        const auto error=s.model.error();
        QVERIFY(!id.isEmpty());
        if(s.model.diagnosticsEnabled()) {
            const auto observation=spentit::diagnostics::Collector::instance().snapshot();
            QCOMPARE(observation.latest.persistence,spentit::diagnostics::Persistence::confirmed_saved);
            QCOMPARE(observation.latest.presentation,spentit::diagnostics::Presentation::failed);
        }
        QVERIFY(error.contains("were saved"));QVERIFY(!error.contains(id));QVERIFY(error.contains("before making more changes"));
        QVERIFY(!error.contains("rollback",Qt::CaseInsensitive));
        QVERIFY(!s.model.state().value("active").toBool());
        qunsetenv("SPENTIT_RULE_REFRESH_FAIL");
        run(s.model,"open",{{"path",s.path()}});
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Echoed");
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");RuleUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "rule_ui_tests.moc"
