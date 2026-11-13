#include "import_model.hpp"
#include <spentit/money.hpp>
#include <spentit/storage_document_gateway.hpp>
#include <sqlite3.h>
#include <QTest>
#include <QAccessible>
#include <QStyleHints>
#include <QScopeGuard>
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
#include "controls_style.hpp"
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
        for(int i=0;i<choices.size();++i)if(choices[i].toMap().value("id").toString()==id)return QMetaObject::invokeMethod(field,"selectIndex",Q_ARG(QVariant,QVariant(i)));
        return false;
    }
    void transactionRule(QObject* root,const QString& label,const QString& expression,const QString& payee,const QString& category={}){
        if(!object(root,"newRule")->property("enabled").toBool()){
            QVERIFY(click(root,"saveRules"));QTRY_VERIFY_WITH_TIMEOUT(!root->property("rulesDirty").toBool(),30000);
        }
        QVERIFY(click(root,"newRule"));QVERIFY(edit(root,"ruleLabel",label));QVERIFY(edit(root,"ruleExpression",expression));
        if(!payee.isEmpty())QVERIFY(edit(root,"rulePayeeTemplate",payee));
        if(!category.isEmpty())QVERIFY(pickRuleCategory(root,category));else QVERIFY(QMetaObject::invokeMethod(object(root,"ruleCategory"),"selectIndex",Q_ARG(QVariant,QVariant(0))));
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
    static bool showsText(QQuickItem* item,const QString& text){
        if(!item || !item->isVisible())return false;
        if(item->property("text").toString().contains(text))return true;
        for(auto* child:item->childItems())if(showsText(child,text))return true;
        return false;
    }
    static bool sameEdge(QQuickItem* a,QQuickItem* b){return a && b && a->isVisible() && b->isVisible() && qAbs(a->mapToScene(QPointF()).x()-b->mapToScene(QPointF()).x())<1.0;}
    static void writeQif(const QString& path,const QByteArray& bytes){QFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())qFatal("QIF fixture unavailable");}
    static QByteArray wise(){return "!Type:Bank\nD01/01/2026\nT-29.31\nPTfl Travel UK\nEWise Charges for: card\n$-0.14\nECARD-123\n$-29.17\n^\n";}
    void createDocument(Session& s,const QString& name){run(s.model,"create",{{"path",s.path()},{"protected",false},{"name",name},{"currency","CAD"}});QVERIFY(s.window);QVERIFY(QTest::qWaitForWindowExposed(s.window));QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);createCategory(s.model,s.root,"Test category");QVERIFY(click(s.root,"accountNavigation"));wait(s.model);}
    void setUserVersion(const QString& path,const char* version){
        sqlite3* db=nullptr;QCOMPARE(sqlite3_open(path.toUtf8().constData(),&db),SQLITE_OK);
        const auto key=storage::public_document_key;
        QCOMPARE(sqlite3_key(db,key.data(),static_cast<int>(key.size())),SQLITE_OK);
        QCOMPARE(sqlite3_exec(db,version,nullptr,nullptr,nullptr),SQLITE_OK);
        QCOMPARE(sqlite3_close(db),SQLITE_OK);
    }
private slots:
    void initTestCase(){if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("RuleUi");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());}
    void init(){QSettings().clear();qunsetenv("SPENTIT_RULE_CANCEL_WINDOW");qunsetenv("SPENTIT_RULE_REFRESH_FAIL");}
    void ledgerContextCreatesRuleWithoutOpeningEditor_data(){
        QTest::addColumn<bool>("dark");QTest::newRow("light")<<false;QTest::newRow("dark")<<true;
    }
    void ledgerContextCreatesRuleWithoutOpeningEditor(){
        QFETCH(bool,dark);
        const auto originalScheme=QGuiApplication::styleHints()->colorScheme();
        const auto restoreScheme=qScopeGuard([&]{QGuiApplication::styleHints()->setColorScheme(originalScheme);});
        QGuiApplication::styleHints()->setColorScheme(dark?Qt::ColorScheme::Dark:Qt::ColorScheme::Light);
        Session s;createDocument(s,"Ledger rule context");QVERIFY(s.root);
        const auto source=s.path("context.qif");
        writeQif(source,"!Type:Bank\nD01/01/2026\nT-1.00\nPFirst shop\n^\nD02/01/2026\nT-2.00\nPSecond shop\n^\n");
        run(s.model,"import",{{"path",source},{"profile","DMY"}});
        const auto batch=selected(s.model).value("batch").toString();
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(s.root->setProperty("editorClosed",true));
        auto openContext=[&](int ordinal){return QMetaObject::invokeMethod(s.root,"openLedgerContext",Q_ARG(QVariant,QVariant(s.model.occurrenceIndex(batch,ordinal))));};
        QTest::qWait(100);
        auto* row=qobject_cast<QQuickItem*>(object(s.root,"ledgerRow_"+QString::number(s.model.occurrenceIndex(batch,1))));QVERIFY(row);
        QTest::mouseClick(s.window,Qt::RightButton,Qt::NoModifier,row->mapToScene(QPointF(120,15)).toPoint());wait(s.model);
        QTRY_VERIFY(object(s.root,"ledgerContext")->property("visible").toBool());
        QVERIFY(s.root->property("editorClosed").toBool());
        QCOMPARE(selected(s.model).value("ordinal").toInt(),1);
        auto* action=object(s.root,"createRuleContext");QVERIFY(action);
        QVERIFY(action->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(action,"triggered"));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ledgerContext"),"close"));
        QCOMPARE(page(s.root),"rules");
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Second shop");
        QCOMPARE(object(s.root,"ruleExpression")->property("text").toString(),"^Second shop$");
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save, apply and back");
        QVERIFY(click(s.root,"discardRules"));QVERIFY(click(s.root,"accountNavigation"));
        run(s.model,"select",{{"row",s.model.occurrenceIndex(batch,0)}});
        run(s.model,"select",{{"row",s.model.occurrenceIndex(batch,1)},{"toggle",true}});
        QVERIFY(openContext(1));wait(s.model);
        QVERIFY(!action->property("enabled").toBool());
        QCOMPARE(s.model.state().value("selectionCount").toInt(),2);
        auto* theme=object(s.root,"applicationTheme");QVERIFY(theme);
        auto* content=action->property("contentItem").value<QObject*>();QVERIFY(content);
        QCOMPARE(content->property("color"),theme->property("disabled"));
        auto* submenu=object(s.root,"ledgerSubmenu_ledgerDuplicateMenu");QVERIFY(submenu);
        QVERIFY(!submenu->property("enabled").toBool());
        auto* arrow=submenu->property("arrow").value<QObject*>();QVERIFY(arrow);
        QCOMPARE(arrow->property("color"),theme->property("disabled"));
        QVERIFY(submenu->setProperty("highlighted",true));
        auto* background=submenu->property("background").value<QObject*>();QVERIFY(background);
        QCOMPARE(background->property("color").value<QColor>().alpha(),0);
        if(qEnvironmentVariableIsSet("SPENTIT_LEDGER_MENU_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_LEDGER_MENU_SCREENSHOT")+(dark?"-dark.png":"-light.png")));}
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ledgerContext"),"close"));
        run(s.model,"select",{{"row",s.model.occurrenceIndex(batch,0)}});
        QVERIFY(openContext(0));wait(s.model);
        QVERIFY(action->property("enabled").toBool());
        QCOMPARE(content->property("color"),theme->property("text"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ledgerContext"),"close"));
        const auto splitSource=s.path("split-context.qif");writeQif(splitSource,wise());
        run(s.model,"import",{{"path",splitSource},{"profile","DMY"}});
        const auto splitBatch=selected(s.model).value("batch").toString();
        QVERIFY(s.root->setProperty("editorClosed",true));
        QVERIFY(QMetaObject::invokeMethod(s.root,"openLedgerContext",Q_ARG(QVariant,QVariant(s.model.occurrenceIndex(splitBatch,0)))));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(action,"triggered"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ledgerContext"),"close"));
        QVERIFY(object(s.root,"ruleSplitDialog")->property("visible").toBool());
        QVERIFY(s.root->property("editorClosed").toBool());
        QVERIFY(click(s.root,"ruleSplitChoice_0"));wait(s.model);
        QCOMPARE(object(s.root,"ruleScope")->property("currentIndex").toInt(),1);
        QCOMPARE(object(s.root,"ruleExpression")->property("text").toString(),"^Wise Charges for: card$");
        QVERIFY(click(s.root,"discardRules"));QVERIFY(click(s.root,"accountNavigation"));
        QVERIFY(click(s.root,"addTransaction"));wait(s.model);
        QVERIFY(edit(s.root,"durablePayee","Manual only"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"durableTotal"),"accepted"));wait(s.model);
        const auto manual=selected(s.model);
        QVERIFY(QMetaObject::invokeMethod(s.root,"openLedgerContext",Q_ARG(QVariant,QVariant(s.model.occurrenceIndex(manual.value("batch").toString(),0)))));wait(s.model);
        QVERIFY(!action->property("enabled").toBool());
    }
    void ledgerCreatedRuleReturnsAfterSaveAndApply(){
        Session s;createDocument(s,"Ledger rule return");
        const auto source=s.path("return.qif");
        writeQif(source,"!Type:Bank\nD01/01/2026\nT-1.00\nPSecond shop\n^\n");
        run(s.model,"import",{{"path",source},{"profile","DMY"}});
        const auto batch=selected(s.model).value("batch").toString();
        QVERIFY(s.root->setProperty("editorClosed",true));
        QVERIFY(QMetaObject::invokeMethod(s.root,"openLedgerContext",Q_ARG(QVariant,QVariant(s.model.occurrenceIndex(batch,0)))));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"createRuleContext"),"triggered"));wait(s.model);
        QCOMPARE(page(s.root),"rules");
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save, apply and back");
        QVERIFY(click(s.root,"applyRules"));
        QCOMPARE(page(s.root),"rules");
        QCOMPARE(s.model.state().value("rules").toList().size(),0);
        QVERIFY(click(s.root,"discardRules"));
        QVERIFY(click(s.root,"newRule"));
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply rule");
        QVERIFY(click(s.root,"discardRules"));
        QVERIFY(click(s.root,"accountNavigation"));wait(s.model);
        QVERIFY(s.root->setProperty("editorClosed",true));
        QVERIFY(QMetaObject::invokeMethod(s.root,"openLedgerContext",Q_ARG(QVariant,QVariant(s.model.occurrenceIndex(batch,0)))));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"createRuleContext"),"triggered"));wait(s.model);
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save, apply and back");
        QVERIFY(edit(s.root,"rulePayeeTemplate","Shop renamed"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ruleCategory"),"selectIndex",Q_ARG(QVariant,QVariant(0))));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QTRY_COMPARE(page(s.root),"ledger");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Shop renamed");
        QCOMPARE(s.model.state().value("rules").toList().size(),1);
        QVERIFY(s.root->setProperty("editorClosed",false));
        QVERIFY(click(s.root,"editTransactionRule"));
        QCOMPARE(page(s.root),"rules");
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Apply rule");
        QVERIFY(edit(s.root,"ruleExpression","^Second shop extra$"));
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply rule");
        QVERIFY(click(s.root,"discardRules"));
        QTest::qWait(50);
        QCOMPARE(page(s.root),"rules");
        QVERIFY(click(s.root,"newRule"));
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply rule");
        QVERIFY(edit(s.root,"ruleLabel","Later rule"));
        QVERIFY(edit(s.root,"ruleExpression","^Later$"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ruleCategory"),"selectIndex",Q_ARG(QVariant,QVariant(0))));
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QTest::qWait(50);
        QCOMPARE(page(s.root),"rules");
        QCOMPARE(s.model.state().value("rules").toList().size(),2);
    }
    void discardNewThenRemoveSavedWithAccessibility(){
        QAccessible::setActive(true);
        Session s;createDocument(s,"Rule removal regression");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVERIFY(click(s.root,"newRule"));QVERIFY(edit(s.root,"ruleLabel","test"));
        QVERIFY(click(s.root,"saveRules"));
        QCOMPARE(object(s.root,"operationPhase")->property("text").toString(),"Choose an existing category.");
        QVERIFY(s.model.state().value("rules").toList().isEmpty());
        QCOMPARE(object(s.root,"operationPhase")->property("color"),object(s.root,"welcomeError")->property("color"));
        auto* ruleRow=qobject_cast<QQuickItem*>(object(s.root,"ruleRow_0"));
        auto* ruleEnabled=qobject_cast<QQuickItem*>(object(s.root,"ruleEnabled_0"));
        QVERIFY(ruleRow && ruleEnabled);
        QVERIFY(ruleEnabled->mapToItem(ruleRow,QPointF()).x()>=8);
        auto* divider=qobject_cast<QQuickItem*>(object(s.root,"ruleApplyDivider"));QVERIFY(divider && divider->isVisible());
        auto* save=qobject_cast<QQuickItem*>(object(s.root,"saveRules"));QVERIFY(save);
        auto* apply=qobject_cast<QQuickItem*>(object(s.root,"applyRules"));QVERIFY(apply);
        QTest::qWait(100);
        QVERIFY(divider->mapToScene(QPointF()).y()>save->mapToScene(QPointF(0,save->height())).y());
        QVERIFY(divider->mapToScene(QPointF()).y()<apply->mapToScene(QPointF()).y());
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_VALIDATION_SCREENSHOT"))QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_VALIDATION_SCREENSHOT")));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ruleCategory"),"selectIndex",Q_ARG(QVariant,QVariant(0))));
        QVERIFY(s.root->property("ruleValidationError").toString().isEmpty());
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QCOMPARE(s.model.state().value("rules").toList().size(),1);
        QVERIFY(click(s.root,"newRule"));
        QVERIFY(click(s.root,"openRule_0"));QVERIFY(click(s.root,"openRule_1"));
        QVERIFY(click(s.root,"discardRules"));
        QCOMPARE(s.root->property("ruleIndex").toInt(),0);
        auto* button=object(s.root,"openRule_0");QVERIFY(button);
        auto* accessible=QAccessible::queryAccessibleInterface(button);QVERIFY(accessible);
        QCOMPARE(accessible->text(QAccessible::Name),QString("test"));
        QVERIFY(click(s.root,"deleteRule_0"));QTest::qWait(100);
        QCOMPARE(accessible->text(QAccessible::Name),QString("test"));
        QCOMPARE(accessible->text(QAccessible::Description),QString("Saved rule"));
        QVERIFY(object(s.root,"deleteRuleDialog")->property("visible").toBool());
        QVERIFY(!click(s.root,"newRule"));
        QCOMPARE(s.model.state().value("rules").toList().size(),1);
        QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QVERIFY(s.model.state().value("rules").toList().isEmpty());
        QCOMPARE(s.model.state().value("ruleTombstones").toList().size(),1);
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});
        QVERIFY(s.model.state().value("rules").toList().isEmpty());
        QCOMPARE(s.model.state().value("ruleTombstones").toList().size(),1);
    }
    void deleteConfirmationSelectsRemainingDraft(){
        Session s;createDocument(s,"Delete confirmation");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"test","^test$","");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Neighbour","^other$","");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"newRule"));QVERIFY(edit(s.root,"ruleLabel","Unfinished draft"));
        QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(click(s.root,"deleteRule_0"));
        QVERIFY(object(s.root,"deleteRuleDialog")->property("modal").toBool());
        QVERIFY(!click(s.root,"openRule_2"));QVERIFY(!click(s.root,"newRule"));
        QVERIFY(!object(s.root,"openRule_0")->property("text").toString().contains("Deleted"));
        QVERIFY(click(s.root,"keepRule"));
        QCOMPARE(s.model.state().value("rules").toList().size(),2);
        QVERIFY(click(s.root,"deleteRule_0"));
        QTest::keyClick(s.window,Qt::Key_Escape);
        QTRY_VERIFY(!object(s.root,"deleteRuleDialog")->property("visible").toBool());
        QVERIFY(click(s.root,"deleteRule_0"));
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_DELETE_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_DELETE_SCREENSHOT")));}
        QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Unfinished draft");
        QVERIFY(object(s.root,"ruleEditor")->property("visible").toBool());
        QCOMPARE(s.model.state().value("rules").toList().size(),1);
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"discardRules"));
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Neighbour");
        QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QVERIFY(object(s.root,"newRule")->property("visible").toBool());
        QVERIFY(!object(s.root,"ruleEditor")->property("visible").toBool());
    }
    void workspaceUsabilityAndAccountPicker(){
        Session s;createDocument(s,"First account");
        const auto first=s.model.state().value("accountId").toString();
        run(s.model,"create_account",{{"id","2"},{"name","Second account"},{"currency","CAD"},{"date","2026-01-01"}});
        const auto source=s.path("shop.qif");writeQif(source,"!Type:Bank\nD01/01/2026\nT-12.00\nPShop\n^\n");
        run(s.model,"import",{{"path",source},{"profile","DMY"}});
        const auto batch=selected(s.model).value("batch");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        for(const auto* name:{"ruleSearch","showRemovedRules","ruleTransactionScope","previewThisRule","applyRules"})QVERIFY2(!object(s.root,name)->property("visible").toBool(),name);
        QVERIFY(object(s.root,"rulesEmpty")->property("visible").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_EMPTY_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_EMPTY_SCREENSHOT")));}
        transactionRule(s.root,"Shop","^Shop$","Shop result");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(!object(s.root,"saveRules")->property("visible").toBool());
        QVERIFY(click(s.root,"newRule"));QVERIFY(edit(s.root,"ruleLabel","Unfinished"));
        QVERIFY(!object(s.root,"newRule")->property("visible").toBool());
        QVERIFY(click(s.root,"openRule_0"));QVERIFY(edit(s.root,"rulePayeeTemplate","Changed shop"));
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QVERIFY(s.root->property("currentRulePreview").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_PREVIEW_SCREENSHOT")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_PREVIEW_SCREENSHOT")));}
        auto accounts=s.model.state().value("accounts").toList();int firstIndex=-1,secondIndex=-1;
        for(int i=0;i<accounts.size();++i){if(accounts[i].toMap().value("id").toString()==first)firstIndex=i;else secondIndex=i;}
        QVERIFY(firstIndex>=0 && secondIndex>=0);
        QVERIFY(choose(s.root,"rulesAccount",firstIndex));wait(s.model);
        QCOMPARE(page(s.root),"rules");QCOMPARE(s.root->property("ruleIndex").toInt(),0);
        QVERIFY(!s.root->property("currentRulePreview").toBool());QVERIFY(s.root->property("rulesDirty").toBool());
        QCOMPARE(object(s.root,"rulePayeeTemplate")->property("text").toString(),"Changed shop");
        QVERIFY(object(s.root,"openRule_1")->property("text").toString().contains("Unfinished"));
        QVERIFY(click(s.root,"previewThisRule"));wait(s.model);
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("changed").toInt(),0);
        QVERIFY(choose(s.root,"rulesAccount",secondIndex));wait(s.model);
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(s.model.state().value("rules").toList().size(),1);
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(s.model.state().value("canUndoRules").toBool());
        run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Changed shop");
        QVERIFY(click(s.root,"openRule_1"));QVERIFY(QMetaObject::invokeMethod(s.root,"undoEdit"));QVERIFY(object(s.root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(s.root,"confirmActionUndo"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Shop");
        QVERIFY(!s.model.state().value("canUndoRules").toBool());
        QVERIFY(click(s.root,"discardRules"));QVERIFY(click(s.root,"openRule_0"));
        QVERIFY(edit(s.root,"ruleSearch","Absent"));QVERIFY(object(s.root,"ruleSearchEmpty")->property("visible").toBool());
        QVERIFY(object(s.root,"ruleEditor")->property("visible").toBool());
        QVERIFY(click(s.root,"ruleSearchClear"));
        QVERIFY(setChecked(s.root,"ruleEnabled_0",false));
        QVERIFY(!object(s.root,"applyRules")->property("visible").toBool());
        QVERIFY(click(s.root,"discardRules"));
        QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(object(s.root,"ruleLabel")->property("visible").toBool());
        QVERIFY(object(s.root,"deleteRuleDialog")->property("visible").toBool());
        QVERIFY(click(s.root,"keepRule"));QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QVERIFY(object(s.root,"showRemovedRules")->property("visible").toBool());
        QVERIFY(setChecked(s.root,"showRemovedRules",true));
        const auto removed=s.model.state().value("ruleTombstones").toList().front().toMap().value("id").toString();
        QVERIFY(click(s.root,"copyRemoved_"+removed));
        QVERIFY(!object(s.root,"copyRemoved_"+removed)->property("visible").toBool());
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Shop");
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_EDIT_SCREENSHOT")){s.window->resize(720,560);QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_EDIT_SCREENSHOT")));}
    }
    void currentFormatShowsRulesAndOldFormatIsRejected(){
        Session current;createDocument(current,"Current rules");
        QVERIFY(current.model.state().value("rulesSupported").toBool());
        QVERIFY(object(current.root,"rulesNavigation")->property("visible").toBool());
        run(current.model,"close");QSettings().clear();
        Session legacy;createDocument(legacy,"Foreign version");
        run(legacy.model,"close");
        setUserVersion(legacy.path(),"PRAGMA user_version=2");
        QSettings().clear();
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
        run(s.model,"close",{},false); // A preparing import cannot expire its session.
        run(s.model,"close",{{"skipAudit",true}}); // Simulate interruption, then recover.
        run(s.model,"open",{{"path",s.path()}});
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
        QCOMPARE(object(s.root,"rulesGuardSave")->property("text").toString(),QString("Save all changes"));
        QCOMPARE(object(s.root,"rulesGuardDiscard")->property("text").toString(),QString("Discard all changes"));
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
    void sharedCategoryCompletion(){
        Session s;createDocument(s,"Shared category completion");
        QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);
        const auto heating=createCategory(s.model,s.root,"Utilities: Gas & Heating");
        createCategory(s.model,s.root,"Utilities: Water");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Completion","^Synthetic$","Synthetic",heating);
        auto* field=qobject_cast<QQuickItem*>(object(s.root,"ruleCategory"));QVERIFY(field);
        field->forceActiveFocus();
        QTRY_COMPARE(field->property("selectedText").toString(),QString("Utilities: Gas & Heating"));
        QTest::keyClick(s.window,Qt::Key_U);
        QCOMPARE(field->property("text").toString(),QString("u"));
        QTest::keyClick(s.window,Qt::Key_T);
        QCOMPARE(object(s.root,"ruleCategoryMatches")->property("count").toInt(),2);
        QVERIFY(field->hasActiveFocus());
        QVERIFY(edit(s.root,"ruleCategory","Gas &"));
        QCOMPARE(object(s.root,"ruleCategoryMatches")->property("count").toInt(),1);
        QTest::qWait(50);
        auto* choice=object(s.root,"inlineCategoryChoice_"+heating);QVERIFY(choice);
        QCOMPARE(choice->property("text").toString(),QString("Utilities: Gas & Heating"));
        // A drag within the already focused field selects only part of the text.
        const auto from=field->mapToScene(QPointF(8,field->height()/2)).toPoint();
        const auto to=from+QPoint(25,0);
        QTest::mousePress(s.window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(s.window,to,50);
        QTest::mouseRelease(s.window,Qt::LeftButton,Qt::NoModifier,to);
        QVERIFY(!field->property("selectedText").toString().isEmpty());
        QVERIFY(field->property("selectedText").toString().size()<field->property("text").toString().size());
        QTest::keyClick(s.window,Qt::Key_Return);
        QCOMPARE(s.root->property("ruleCategoryId").toString(),heating);
        QVERIFY(s.root->property("rulesDirty").toBool());
        QCOMPARE(s.model.state().value("rules").toList().size(),0);
        field->forceActiveFocus();
        QTRY_COMPARE(field->property("selectedText").toString(),QString("Utilities: Gas & Heating"));
        QVERIFY(edit(s.root,"ruleCategory","does not exist"));
        QCOMPARE(object(s.root,"ruleCategoryMatches")->property("count").toInt(),0);
        QTest::keyClick(s.window,Qt::Key_Return);
        QCOMPARE(s.root->property("ruleCategoryId").toString(),heating);
        QTest::keyClick(s.window,Qt::Key_Escape);
        QCOMPARE(field->property("text").toString(),QString("Utilities: Gas & Heating"));
        QVERIFY(edit(s.root,"ruleCategory",""));
        QCOMPARE(object(s.root,"ruleCategoryMatches")->property("count").toInt(),3);
        QVERIFY(!field->property("allowUncategorized").toBool());
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
        QVERIFY(QMetaObject::invokeMethod(s.root,"undoEdit"));QVERIFY(object(s.root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(s.root,"confirmActionUndo"));wait(s.model);
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
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"ruleCategory"),"selectIndex",Q_ARG(QVariant,QVariant(0))));QVERIFY(edit(s.root,"ruleMemoTemplate","Fee renamed"));
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
        QVERIFY(QMetaObject::invokeMethod(s.root,"undoEdit"));QVERIFY(object(s.root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(s.root,"confirmActionUndo"));wait(s.model);QCOMPARE(memo(),"Wise Charges for: card");
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
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply rule");
        QVERIFY(click(s.root,"applyRules"));wait(s.model);QTRY_VERIFY(s.model.state().value("canUndoRules").toBool());
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        run(s.model,"inspect",{{"batch",oldBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride old");
        QVERIFY(QMetaObject::invokeMethod(s.root,"undoEdit"));QVERIFY(object(s.root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(s.root,"confirmActionUndo"));wait(s.model);
        run(s.model,"inspect",{{"batch",newBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride new");
        QVERIFY(choose(s.root,"ruleTransactionScope",0));QVERIFY(click(s.root,"previewThisRule"));wait(s.model);QCOMPARE(summary(s.model),"2 transactions match. 0 already categorized. 0 handled by earlier rules. 2 eligible; 2 will change.");
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        run(s.model,"inspect",{{"batch",oldBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Travel");
        QVERIFY(QMetaObject::invokeMethod(s.root,"undoEdit"));QVERIFY(object(s.root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(s.root,"confirmActionUndo"));wait(s.model);QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride old");
        run(s.model,"inspect",{{"batch",newBatch},{"ordinal",0}});QCOMPARE(draftOf(s.model).value("payee").toString(),"Ride new");
    }
    void appliedSplitAndTransactionTargets() {
        Session s;createDocument(s,"Applied targets");
        QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);
        const auto category=createCategory(s.model,s.root,"Transport");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        splitMemoRule(s.root,"Fee","^Wise Charges for: card$",category,false);
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Merchant","^Tfl Travel UK$","",category);
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto path=s.path("split.qif");writeQif(path,wise());
        run(s.model,"import",{{"path",path},{"profile","DMY"}});
        if(page(s.root)!="ledger")QVERIFY(click(s.root,"accountNavigation"));
        const auto assignments=selected(s.model).value("ruleAssignments").toList();
        QCOMPARE(assignments.size(),2);
        QCOMPARE(assignments[0].toMap().value("scope").toString(),"split");
        QCOMPARE(assignments[1].toMap().value("scope").toString(),"transaction");
        QVERIFY(click(s.root,"editTransactionRule"));
        QVERIFY(object(s.root,"matchedRuleDialog")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"matchedRuleDialog"),"close"));
        if(qEnvironmentVariableIsSet("SPENTIT_RULE_TARGET_CAPTURE")){QTest::qWait(100);QVERIFY(s.window->grabWindow().save(qEnvironmentVariable("SPENTIT_RULE_TARGET_CAPTURE")));}
        // Only split 2 has no split-scope rule: Create opens its memo directly.
        QVERIFY(click(s.root,"transactionRuleAction"));wait(s.model);
        QCOMPARE(object(s.root,"ruleExpression")->property("text").toString(),"^CARD-123$");
        QCOMPARE(object(s.root,"ruleScope")->property("currentIndex").toInt(),1);
    }
    void ruleTargetChoices() {
        Session s;createDocument(s,"Rule targets");
        const QVariantMap split{{"amount","-1.00"},{"memo","Fee"},{"sourceOrdinal",0}};
        const QVariantMap second{{"amount","-9.00"},{"memo","Purchase"},{"sourceOrdinal",1}};
        const QVariantMap transactionRule{{"rule","transaction"},{"label","Merchant"},{"scope","transaction"},{"allocation",1},{"available",true}};
        const QVariantMap splitRule{{"rule","fee"},{"label","Fee"},{"scope","split"},{"allocation",0},{"available",true}};
        const QVariantMap secondRule{{"rule","purchase"},{"label","Purchase"},{"scope","split"},{"allocation",1},{"available",true}};
        auto check=[&](QVariantList splits,QVariantList assignments,int creates,int edits){
            s.root->setProperty("selected",QVariantMap{{"batch","synthetic"},{"draft",QVariantMap{{"splits",splits}}},{"ruleAssignments",assignments}});
            QVariant createResult,editResult;
            QVERIFY(QMetaObject::invokeMethod(s.root,"ruleCreateChoices",Q_RETURN_ARG(QVariant,createResult)));
            QVERIFY(QMetaObject::invokeMethod(s.root,"ruleChoices",Q_RETURN_ARG(QVariant,editResult)));
            auto list=[](const QVariant& value){return value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();};
            QCOMPARE(list(createResult).size(),creates);QCOMPARE(list(editResult).size(),edits);
            QCOMPARE(object(s.root,"transactionRuleAction")->property("enabled").toBool(),creates>0);
            QCOMPARE(object(s.root,"editTransactionRule")->property("enabled").toBool(),edits>0);
        };
        check({split},{},1,0);
        check({split},{transactionRule},0,1);
        check({split,second},{},3,0);
        check({split,second},{transactionRule},2,1);
        check({split,second},{splitRule},2,1);
        check({split,second},{transactionRule,splitRule},1,2);
        check({split,second},{transactionRule,splitRule,secondRule},0,3);
        auto deleted=splitRule;deleted["available"]=false;
        check({split,second},{transactionRule,deleted},2,1);
        auto generated=second;generated["sourceOrdinal"]=-1;
        check({split,generated},{transactionRule,splitRule},0,2);
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
        QVERIFY(!object(s.root,"transactionRuleAction")->property("enabled").toBool());
        QVERIFY(object(s.root,"editTransactionRule")->property("enabled").toBool());
        QVERIFY(!object(s.root,"ruleWinners"));
        auto* action=qobject_cast<QQuickItem*>(object(s.root,"transactionRuleAction"));
        auto* addSplit=qobject_cast<QQuickItem*>(object(s.root,"addSplit"));
        QVERIFY(action && addSplit);
        QTRY_VERIFY(action->mapToScene(QPointF()).x()<addSplit->mapToScene(QPointF()).x());
        QVERIFY(click(s.root,"editTransactionRule"));
        QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Synthetic Shop");
        QVERIFY(edit(s.root,"ruleExpression","^No longer matches$"));QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"accountNavigation"));run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(!object(s.root,"transactionRuleAction")->property("enabled").toBool());
        QVERIFY(object(s.root,"editTransactionRule")->property("enabled").toBool());
        QVERIFY(click(s.root,"editTransactionRule"));QCOMPARE(object(s.root,"ruleLabel")->property("text").toString(),"Synthetic Shop");
        QVERIFY(click(s.root,"deleteRule_1"));QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QVERIFY(click(s.root,"accountNavigation"));run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(object(s.root,"transactionRuleAction")->property("text").toString(),"Create rule…");
        const auto splitFile=s.path("split.qif");writeQif(splitFile,wise());
        run(s.model,"import",{{"path",splitFile},{"profile","DMY"}});
        QVERIFY(edit(s.root,"splitCategory_0","Transport"));
        QVERIFY(QMetaObject::invokeMethod(object(s.root,"splitCategory_0"),"acceptChoice"));
        QVERIFY(QMetaObject::invokeMethod(s.root,"saveEditor"));wait(s.model);
        const auto splitBefore=draftOf(s.model);
        run(s.model,"rules_prefill",{{"kind","payee"}});
        QCOMPARE(s.model.state().value("rulePrefill").toMap().value("scope").toString(),"transaction");
        QCOMPARE(s.model.state().value("rulePrefill").toMap().value("expression").toString(),"^Tfl Travel UK$");
        QVERIFY(s.model.state().value("rulePrefill").toMap().value("categoryId").toString().isEmpty());
        // Discard the unsaved prefill before choosing a split through Create.
        QVERIFY(click(s.root,"discardRules"));wait(s.model);
        QVERIFY(click(s.root,"accountNavigation"));
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
        QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(click(s.root,"confirmDeleteRule"));wait(s.model);
        QVERIFY(QMetaObject::invokeMethod(s.root,"previewRules"));wait(s.model);
        QCOMPARE(summary(s.model),"0 transactions match. 0 already categorized. 0 handled by earlier rules. 0 eligible; 0 will change.");
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("changed").toInt(),0);
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"2");
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
        QCOMPARE(object(s.root,"applyRules")->property("text").toString(),"Save and apply rule");
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
    void rulesListColumnsStayAligned(){
        Session s;createDocument(s,"Rule columns");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"First rule","^First$","First payee");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Second rule with a long name that should be cut off","^Second$","Second payee");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        transactionRule(s.root,"Third","^Third$","Third payee");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QVERIFY(click(s.root,"openRule_2"));
        auto item=[&](const QString& name){return qobject_cast<QQuickItem*>(object(s.root,name));};
        QVERIFY(!object(s.root,"ruleNumber_0"));
        auto* header=item("ruleListHeader");QVERIFY(header && header->isVisible());
        QCOMPARE(item("ruleColumnEnabled")->property("text").toString(),QString("Enabled"));
        QCOMPARE(item("ruleColumnName")->property("text").toString(),QString("Name"));
        QCOMPARE(item("ruleColumnType")->property("text").toString(),QString("Type"));
        QCOMPARE(item("ruleColumnCategory")->property("text").toString(),QString("Category"));
        QCOMPARE(item("ruleColumnOrder")->property("text").toString(),QString("Order"));
        QCOMPARE(item("rulePatternLabel")->property("text").toString(),QString("Pattern"));
        QCOMPARE(item("ruleEnabled_0")->property("text").toString(),QString());
        auto* enabledAccess=QAccessible::queryAccessibleInterface(item("ruleEnabled_0"));
        QVERIFY(enabledAccess);QCOMPARE(enabledAccess->text(QAccessible::Name),QString("Enabled"));
        QVERIFY(sameEdge(item("ruleColumnEnabled"),item("ruleEnabled_0")));
        QVERIFY(sameEdge(item("ruleColumnEnabled"),item("ruleEnabled_2")));
        QVERIFY(sameEdge(item("ruleColumnName"),item("openRule_0")));
        QVERIFY(sameEdge(item("openRule_0"),item("openRule_1")));
        QVERIFY(sameEdge(item("openRule_1"),item("openRule_2")));
        QVERIFY(qAbs(item("openRule_1")->width()-240)<1);
        auto* nameText=item("openRule_1")->property("contentItem").value<QQuickItem*>();
        QVERIFY(nameText && nameText->property("truncated").toBool());
        QVERIFY(sameEdge(item("ruleColumnType"),item("ruleType_0")));
        QVERIFY(sameEdge(item("ruleType_0"),item("ruleType_2")));
        QVERIFY(sameEdge(item("ruleColumnCategory"),item("ruleCategory_0")));
        QVERIFY(sameEdge(item("ruleCategory_0"),item("ruleCategory_2")));
        QCOMPARE(item("ruleType_0")->property("text").toString(),QString("Transaction · payee"));
        QVERIFY(!showsText(item("ruleRow_1"),"^Second$"));
        QVERIFY(!item("moveRuleUp_0")->isVisible());
        QVERIFY(item("moveRuleUpSpacer_0")->isVisible());
        QVERIFY(item("moveRuleUp_1")->isVisible());
        QVERIFY(!item("moveRuleUpSpacer_1")->isVisible());
        QVERIFY(qAbs(item("moveRuleUpSpacer_0")->width()-item("moveRuleUp_1")->width())<1);
        QVERIFY(!item("moveRuleDown_2")->isVisible());
        QVERIFY(item("moveRuleDownSpacer_2")->isVisible());
        QVERIFY(item("moveRuleDown_1")->isVisible());
        QVERIFY(qAbs(item("moveRuleDownSpacer_2")->width()-item("moveRuleDown_1")->width())<1);
        QVERIFY(sameEdge(item("ruleColumnOrder"),item("moveRuleUpSpacer_0")));
        QVERIFY(sameEdge(item("ruleColumnOrder"),item("moveRuleUp_1")));
        auto* row=item("ruleRow_2");auto* heading=item("selectedRuleHeading");
        QVERIFY(row && heading && heading->isVisible());
        const auto gap=heading->mapToScene(QPointF()).y()-(row->mapToScene(QPointF()).y()+row->height());
        QVERIFY2(gap>20 && gap<32,qPrintable(QString("gap=%1").arg(gap)));
        auto* payee=item("rulePayeeTemplate");auto* memo=item("ruleMemoTemplate");
        auto* category=item("ruleCategory");auto* expression=item("ruleExpression");auto* label=item("ruleLabel");
        QVERIFY(sameEdge(payee,memo));QVERIFY(sameEdge(payee,category));
        QVERIFY(sameEdge(payee,expression));QVERIFY(sameEdge(payee,label));
        QVERIFY(choose(s.root,"ruleScope",1));QCoreApplication::processEvents();
        QTRY_VERIFY(!item("rulePayeeTemplate")->isVisible());
        QVERIFY(sameEdge(item("ruleMemoTemplate"),item("ruleCategory")));
        QVERIFY(sameEdge(item("ruleMemoTemplate"),item("ruleExpression")));
        QVERIFY(sameEdge(item("ruleMemoTemplate"),item("ruleLabel")));
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));applySpentItControlsStyle();RuleUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "rule_ui_tests.moc"
