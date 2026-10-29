#include "import_model.hpp"
#include <spentit/money.hpp>
#include <spentit/storage_document_gateway.hpp>
#include "../storage/import_schema5_fixture.hpp"
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
    void run(ImportModel& m,const QString& action,const QVariantMap& args={},bool success=true){m.command(action,args);wait(m,success);}
    static QVariantMap selected(const ImportModel& m){return m.state().value("selected").toMap();}
    static QVariantMap draftOf(const ImportModel& m){return selected(m).value("draft").toMap();}
    static QString page(QObject* root){return root->property("currentPage").toString();}
    void applyCategory(ImportModel& m,QObject* root){auto* field=qobject_cast<QQuickItem*>(object(root,"categoryName"));QVERIFY(field);field->forceActiveFocus();QTest::keyClick(qobject_cast<QQuickWindow*>(root),Qt::Key_Return);wait(m);QTRY_VERIFY(!root->property("categoryDirty").toBool());}
    QString createCategory(ImportModel& m,QObject* root,const QString& name){
        if(!click(root,"newCategory") || !edit(root,"categoryName",name))qFatal("Category form unavailable");
        applyCategory(m,root);return m.state().value("categorySelected").toMap().value("id").toString();
    }
    bool pickRuleCategory(QObject* root,const QString& id){
        if(!click(root,"ruleCategory"))return false;
        for(int n=0;n<50 && !object(root,"pickCategory_"+id);++n)QTest::qWait(20);
        return click(root,"pickCategory_"+id);
    }
    void transactionRule(QObject* root,const QString& label,const QString& expression,const QString& payee,const QString& category={}){
        QVERIFY(click(root,"newRule"));QVERIFY(edit(root,"ruleLabel",label));QVERIFY(edit(root,"ruleExpression",expression));
        if(!payee.isEmpty())QVERIFY(edit(root,"rulePayeeTemplate",payee));
        if(!category.isEmpty())QVERIFY(pickRuleCategory(root,category));
    }
    void splitMemoRule(QObject* root,const QString& label,const QString& expression,const QString& category,bool inherit){
        QVERIFY(click(root,"newRule"));QVERIFY(edit(root,"ruleLabel",label));QVERIFY(edit(root,"ruleExpression",expression));
        QVERIFY(choose(root,"ruleScope",1));QCoreApplication::processEvents();QVERIFY(choose(root,"ruleInput",1));
        if(inherit){for(int n=0;n<20 && object(root,"ruleInherit") && !object(root,"ruleInherit")->property("visible").toBool();++n)QTest::qWait(20);QVERIFY(setChecked(root,"ruleInherit",true));}
        else if(!category.isEmpty())QVERIFY(pickRuleCategory(root,category));
    }
    static QString summary(const ImportModel& m){return m.state().value("rulePreview").toMap().value("summaryText").toString();}
    static void writeQif(const QString& path,const QByteArray& bytes){QFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())qFatal("QIF fixture unavailable");}
    static QByteArray wise(){return "!Type:Bank\nD01/01/2026\nT-29.31\nPTfl Travel UK\nSUnmapped Fee\nEWise Charges for: card\n$-0.14\nSUnmapped Travel\nECARD-123\n$-29.17\n^\n";}
    void createDocument(Session& s,const QString& name){run(s.model,"create",{{"path",s.path()},{"protected",false},{"name",name},{"currency","CAD"}});QVERIFY(s.window);QVERIFY(QTest::qWaitForWindowExposed(s.window));}
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
        QVERIFY(object(s.root,"rulesUnsavedDialog")->property("visible").toBool());
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
        QVERIFY(!object(s.root,"rulesUnsavedDialog")->property("visible").toBool());
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"guardDiscard"));
        QVERIFY(object(s.root,"rulesUnsavedDialog")->property("visible").toBool());
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"rulesGuardStay"));
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(edit(s.root,"durablePayee","Temporary payee"));
        QVERIFY(QMetaObject::invokeMethod(s.window,"close"));
        QVERIFY(object(s.root,"unsavedDialog")->property("visible").toBool());
        QVERIFY(!object(s.root,"rulesUnsavedDialog")->property("visible").toBool());
        QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"guardDiscard"));
        QVERIFY(object(s.root,"rulesUnsavedDialog")->property("visible").toBool());
        QCOMPARE(page(s.root),"rules");QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"rulesGuardStay"));
        QVERIFY(s.window->isVisible());QVERIFY(s.model.state().value("active").toBool());
    }
    void previewApplyBlankPayeeCommitAndSourceQuote(){
        Session s;createDocument(s,"Synthetic rule preview");
        QVERIFY(click(s.root,"categoryNavigation"));wait(s.model);
        const auto transport=createCategory(s.model,s.root,"Travel: Transport");
        const auto fees=createCategory(s.model,s.root,"Fees");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Alpha","^Tfl","Alpha",transport);
        transactionRule(s.root,"Beta","^Tfl","Beta");
        splitMemoRule(s.root,"Fee","^Wise",fees,false);
        splitMemoRule(s.root,"Trip","^CARD-","",true);
        QVERIFY(click(s.root,"saveRules"));wait(s.model);QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        const auto qif=s.path("wise.qif");writeQif(qif,wise());
        run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        QVERIFY(object(s.root,"createRuleFromMemo_0")->property("enabled").toBool());
        QCOMPARE(object(s.root,"createRuleFromMemo_0")->property("text").toString(),"Create rule from this split memo");
        if(page(s.root)!="rules"){QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);}
        QTRY_VERIFY(object(s.root,"moveRuleUp_1"));
        QVERIFY(click(s.root,"moveRuleUp_1"));QVERIFY(s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"previewRules"));wait(s.model);
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("label").toString(),"Unsaved rules");
        QVERIFY(object(s.root,"rulePreviewStatus")->property("text").toString().contains("Unsaved rules"));
        QVERIFY(summary(s.model).contains("applied · This rule will assign the field · Beta"));
        QVERIFY(summary(s.model).contains("competing · Also matches and disagrees; shadowed by the earlier field winner · Alpha"));
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        QVERIFY(!object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(click(s.root,"discardRules"));wait(s.model);
        QVERIFY(!s.root->property("rulesDirty").toBool());
        QCOMPARE(s.model.state().value("rules").toList().at(0).toMap().value("label").toString(),"Alpha");
        QCOMPARE(s.model.state().value("rules").toList().at(1).toMap().value("label").toString(),"Beta");
        QVERIFY(click(s.root,"previewRules"));wait(s.model);
        QCOMPARE(s.model.state().value("rulePreview").toMap().value("label").toString(),"Saved ruleset revision 1");
        QTRY_VERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        const auto applied=draftOf(s.model);const auto splits=applied.value("splits").toList();
        QCOMPARE(applied.value("payee").toString(),"Alpha");QCOMPARE(splits.size(),2);
        QCOMPARE(splits.at(0).toMap().value("categoryDisplay").toString(),"Fees");
        QCOMPARE(splits.at(1).toMap().value("categoryDisplay").toString(),"Travel: Transport");
        QCOMPARE(splits.at(0).toMap().value("amount").toString(),"-0.14");
        QCOMPARE(splits.at(1).toMap().value("amount").toString(),"-29.17");
        const auto batch=selected(s.model).value("batch").toString();
        run(s.model,"close");run(s.model,"open",{{"path",s.path()}});run(s.model,"inspect",{{"batch",batch},{"ordinal",0}});
        const auto reloaded=draftOf(s.model).value("splits").toList();
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Alpha");
        QCOMPARE(reloaded.at(0).toMap().value("categoryDisplay").toString(),"Fees");
        QCOMPARE(reloaded.at(1).toMap().value("categoryDisplay").toString(),"Travel: Transport");
        QCOMPARE(reloaded.at(0).toMap().value("amount").toString(),"-0.14");
        QCOMPARE(reloaded.at(1).toMap().value("amount").toString(),"-29.17");
        QVERIFY(edit(s.root,"durablePayee",""));QVERIFY(click(s.root,"saveDraft"));wait(s.model);
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        QVERIFY(!object(s.root,"replaceManualFields")->property("checked").toBool());
        QVERIFY(click(s.root,"previewRules"));wait(s.model);
        QVERIFY(summary(s.model).contains("manual_protected"));
        QTRY_VERIFY(object(s.root,"applyRules")->property("enabled").toBool());
        QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"");
        const auto kept=draftOf(s.model).value("splits").toList();
        QCOMPARE(kept.at(0).toMap().value("amount").toString(),"-0.14");
        QCOMPARE(kept.at(1).toMap().value("amount").toString(),"-29.17");
        run(s.model,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(s.model,"commit");
        QVERIFY(!object(s.root,"applyRules")->property("visible").toBool());
        QVERIFY(object(s.root,"rulesCommittedNotice")->property("visible").toBool());
        QCOMPARE(object(s.root,"rulesCommittedNotice")->property("text").toString(),"Committed records are not changed by Apply.");
        const auto before=s.model.state().value("ruleRevision").toString();
        QVERIFY(click(s.root,"accountNavigation"));QCOMPARE(page(s.root),"ledger");
        QVERIFY(click(s.root,"createRuleFromPayee"));wait(s.model);
        QCOMPARE(object(s.root,"ruleExpression")->property("text").toString(),"^(?:\\QTfl Travel UK\\E)$");
        QVERIFY(s.root->property("rulesDirty").toBool());QCOMPARE(s.model.state().value("ruleRevision").toString(),before);
        QVERIFY(edit(s.root,"rulePayeeTemplate","From source"));QVERIFY(click(s.root,"saveRules"));wait(s.model);
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"2");
        bool quoted=false;
        for(const auto& value:s.model.state().value("rules").toList()){
            const auto expression=value.toMap().value("expression").toString();
            QVERIFY(expression!="^(?:\\QAlpha\\E)$");
            if(expression=="^(?:\\QTfl Travel UK\\E)$")quoted=true;
        }
        QVERIFY(quoted);
        QVERIFY(!s.root->property("rulesDirty").toBool());
        QVERIFY(click(s.root,"reapplyRules"));wait(s.model);
        QVERIFY(object(s.root,"reapplyDialog")->property("visible").toBool());
        const auto reapply=s.model.state().value("reapplyPreview").toMap();
        QVERIFY(reapply.value("evaluated").toInt()>=1);
        QCOMPARE(reapply.value("changed").toInt(),0);
        QVERIFY(reapply.value("impact").toString().contains("Account balance is unchanged."));
        QVERIFY(object(s.root,"rulesCommittedNotice")->property("visible").toBool());
        QCOMPARE(object(s.root,"rulesCommittedNotice")->property("text").toString(),"Committed records are not changed by Apply.");
        QVERIFY(click(s.root,"applyReapply"));wait(s.model);
        QCOMPARE(s.model.state().value("reapplyReceipt").toMap().value("summary").toString(),"No committed rows were changed");
        QVERIFY(!object(s.root,"reapplyDialog")->property("visible").toBool());
    }
    void obsoleteDerivedPayeeRemoval(){
        Session s;createDocument(s,"Synthetic obsolete rule");
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Echo","^Tfl","Tfl Travel UK");
        QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("wise.qif");writeQif(qif,wise());
        run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        if(page(s.root)!="rules"){QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);}
        QVERIFY(click(s.root,"previewRules"));wait(s.model);QVERIFY(click(s.root,"applyRules"));wait(s.model);
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
        QVERIFY(click(s.root,"deleteRule_0"));QVERIFY(click(s.root,"previewRules"));wait(s.model);
        QVERIFY(summary(s.model).contains("Derived value will be cleared and the source value restored, or the previous rule mark replaced"));
        QVERIFY(summary(s.model).contains("Tfl Travel UK -> Tfl Travel UK"));
        QCOMPARE(s.model.state().value("ruleRevision").toString(),"1");
        QCOMPARE(draftOf(s.model).value("payee").toString(),"Tfl Travel UK");
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
        QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);
        transactionRule(s.root,"Echoed","^Tfl","Echoed");QVERIFY(click(s.root,"saveRules"));wait(s.model);
        const auto qif=s.path("wise.qif");writeQif(qif,wise());run(s.model,"import",{{"path",qif},{"profile","DMY"}});
        const auto batch=s.model.state().value("pending").toMap().value("id").toString();QVERIFY(!batch.isEmpty());
        if(page(s.root)!="rules"){QVERIFY(click(s.root,"rulesNavigation"));wait(s.model);}
        QVERIFY(click(s.root,"previewRules"));wait(s.model);
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
        QVERIFY(error.contains("was saved durably"));QVERIFY(error.contains(id));QVERIFY(error.contains("do not apply it again"));
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
