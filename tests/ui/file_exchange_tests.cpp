#include "file_exchange_codec.hpp"
#include "import_model.hpp"
#include <spentit/import_store.hpp>
#include <spentit/storage_document_gateway.hpp>
#ifndef _WIN32
#include "../storage/fault_vfs.hpp"
#include <sys/wait.h>
#endif
#include <QTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QSignalSpy>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <climits>
using namespace spentit;
using namespace spentit::storage;
namespace ex=spentit::ui::exchange;
class FileExchangeTests:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}

    static void category(ImportStore& store,const char* id,const char* name,bool income=false){app::CategoryRequest request{app::CategoryAction::create,id,name,""};store.apply_category_change(std::string("create-")+id,request,store.category_catalog().revision);if(income){request={app::CategoryAction::set_income,id,"",""};request.income=true;store.apply_category_change(std::string("income-")+id,request,store.category_catalog().revision);}}
    static void definitions(ImportStore& store){
        category(store,"food","Food");category(store,"wage","Wages",true);
        if(store.rules_supported()){app::RuleDefinitionImage r;r.identity="rule";r.label="Same name";r.expression="Shop";r.actions.category="food";r.actions.memo_template="Bought ${0}";app::OperationContext context;store.save_rules("save-rules",{{r}},0,context);}
        store.save_reports({0,{{"report","Same name","grouped_bar",{1},{"food",""},{{"this_year","",""},{"custom","2025-01-01","2025-12-31"}}}}});
    }
    static QVariantMap choose(bool rules=true){return {{"rules",rules?QVariantList{0}:QVariantList{}},{"reports",QVariantList{0}},{"categories",QVariantList{0,1}}};}
    static imports::DraftRow manual(const char* date,const char* payee,std::int64_t amount){const Currency cad("CAD");return {date,payee,"",Transaction::manual(Money(amount,cad),{{"","",Money(amount,cad)}})};}
    void run(ImportModel& model,const QString& command,const QVariantMap& args={}){model.command(command,args);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));}
private slots:
    void initTestCase(){
#ifndef _WIN32
        fault_vfs::install();
#endif
        QCoreApplication::setOrganizationName("SpentItExchangeTests");QCoreApplication::setApplicationName("Exchange");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());}
    void roundTrip(){
        QTemporaryDir dir;const Currency cad("CAD");auto source=ImportStore::create(dir.filePath("source").toStdString(),"test","Checking",cad,"2026-01-01");definitions(source);
        const auto bytes=ex::encode(source,source.rules_supported(),true,true);const auto doc=ex::decode(bytes);
        auto dest=ImportStore::create(dir.filePath("dest").toStdString(),"test","Checking",cad,"2026-01-01");dest.create_manual("manual",manual("2026-02-01","New",100));
        const auto head=dest.action_token().head;const auto token=dest.action_token();auto input=choose(source.rules_supported());const auto request=ex::plan(dest,doc,input,token);app::OperationContext context;
        dest.import_definitions("exchange",request,context);QVERIFY(dest.definition_import_completed("exchange",request));
        QCOMPARE(dest.category_catalog().categories.size(),std::size_t(2));QCOMPARE(dest.report_catalog().definitions.size(),std::size_t(1));QCOMPARE(dest.action_token().head,head);QCOMPARE(dest.balance().units(),std::int64_t(100));
        if(dest.rules_supported()){QCOMPARE(dest.rule_image().ordered.size(),std::size_t(1));QVERIFY(dest.rule_image().ordered.front().identity!="rule");}
        dest.import_definitions("exchange",request,context);QCOMPARE(dest.report_catalog().definitions.size(),std::size_t(1));
        auto conflict=request;conflict.reports->definitions[0].name="different";QVERIFY_THROWS_EXCEPTION(std::exception,dest.import_definitions("exchange",conflict,context));
        const auto again=ex::plan(dest,doc,input,dest.action_token());dest.import_definitions("again",again,context);QCOMPARE(dest.report_catalog().definitions.size(),std::size_t(2));if(dest.rules_supported())QCOMPARE(dest.rule_image().ordered.size(),std::size_t(2));
        QCOMPARE(dest.category_catalog().categories.size(),std::size_t(2));dest.certify({});
        const auto noCats=ex::decode(ex::encode(source,source.rules_supported(),true,false));auto missing=ImportStore::create(dir.filePath("missing").toStdString(),"test","Different",cad,"2026-01-01");
        QVariantMap only{{"rules",QVariantList{}},{"reports",QVariantList{0}},{"categories",QVariantList{}}};
        QVERIFY_THROWS_EXCEPTION(std::exception,ex::plan(missing,noCats,only,missing.action_token()));
        category(missing,"mapped","Dining");only["categoryMappings"]=QVariantMap{{"food","mapped"}};only["accountMappings"]=QVariantMap{{"1","1"}};
        const auto mapped=ex::plan(missing,noCats,only,missing.action_token());missing.import_definitions("mapped",mapped,context);QCOMPARE(missing.report_catalog().definitions[0].categories[1],std::string("mapped"));
        auto wrong=doc;wrong["version"]=2;QVERIFY_THROWS_EXCEPTION(std::exception,ex::decode(QJsonDocument(wrong).toJson()));QVERIFY_THROWS_EXCEPTION(std::exception,ex::decode("{"));
        auto duplicated=bytes;duplicated.replace("\"version\": 1","\"version\": 9, \"version\": 1");QVERIFY_THROWS_EXCEPTION(std::exception,ex::decode(duplicated));
        auto invalidUnicode=bytes;invalidUnicode.replace("Same name","\\ud800");QVERIFY_THROWS_EXCEPTION(std::exception,ex::decode(invalidUnicode));
        category(missing,"wage","Wages",false);auto income=choose(false);income["reports"]=QVariantList{};income["categories"]=QVariantList{1};
        // Category export order is catalog order; find Wages explicitly.
        int wage=-1;const auto categories=doc.value("categories").toArray();for(int i=0;i<categories.size();++i)if(categories[i].toObject()["name"]=="Wages")wage=i;QVERIFY(wage>=0);income["categories"]=QVariantList{wage};
        QVERIFY_THROWS_EXCEPTION(std::exception,ex::plan(missing,doc,income,missing.action_token()));
        income["incomeChoices"]=QVariantMap{{QString::number(wage),2}};auto change=ex::plan(missing,doc,income,missing.action_token());missing.import_definitions("income",change,context);QVERIFY(missing.category_catalog().categories.back().income);
        QVERIFY_THROWS_EXCEPTION(std::exception,ex::plan(dest,doc,input,token));
    }
    void atomicFaults(){
        QTemporaryDir dir;const Currency cad("CAD");auto source=ImportStore::create(dir.filePath("source").toStdString(),"test","Checking",cad,"2026-01-01");definitions(source);const auto doc=ex::decode(ex::encode(source,source.rules_supported(),true,true));
        auto dest=ImportStore::create(dir.filePath("dest").toStdString(),"test","Checking",cad,"2026-01-01");auto request=ex::plan(dest,doc,choose(source.rules_supported()),dest.action_token());const auto token=dest.action_token();app::OperationContext context;
        dest.observer([](ImportPhase phase){if(phase==ImportPhase::before_commit)throw std::runtime_error("fault");});QVERIFY_THROWS_EXCEPTION(std::exception,dest.import_definitions("fault",request,context));dest.observer({});QCOMPARE(dest.category_catalog().categories.size(),std::size_t(0));QVERIFY(dest.report_catalog().definitions.empty());QCOMPARE(dest.action_token(),token);
        if(dest.rules_supported())QVERIFY(dest.rule_image().ordered.empty());
        app::OperationContext cancelled;cancelled.request();QVERIFY_THROWS_EXCEPTION(app::OperationCancelled,dest.import_definitions("cancelled",request,cancelled));QVERIFY(dest.category_catalog().categories.empty());
        if(request.rules){auto invalid=request;invalid.rules->ordered.back().expression="[";QVERIFY_THROWS_EXCEPTION(std::exception,dest.import_definitions("invalid",invalid,context));QVERIFY(dest.category_catalog().categories.empty());}
        dest.observer([](ImportPhase phase){if(phase==ImportPhase::after_commit)throw std::runtime_error("lost reply");});QVERIFY_THROWS_EXCEPTION(std::exception,dest.import_definitions("lost",request,context));dest.observer({});QVERIFY(dest.definition_import_completed("lost",request));dest.import_definitions("lost",request,context);QCOMPARE(dest.report_catalog().definitions.size(),std::size_t(1));dest.certify({});
    }
    void ioAndCrashRecovery(){
#ifdef _WIN32
        QSKIP("The fault VFS and process-exit harness are POSIX-only.");
#else
        QTemporaryDir dir;const Currency cad("CAD");QJsonObject doc;bool withRules=false;
        {auto source=ImportStore::create(dir.filePath("source").toStdString(),"test","Checking",cad,"2026-01-01");definitions(source);withRules=source.rules_supported();doc=ex::decode(ex::encode(source,withRules,true,true));}
        int writes=0;
        {auto dest=ImportStore::create(dir.filePath("measure").toStdString(),"test","Checking",cad,"2026-01-01");const auto request=ex::plan(dest,doc,choose(withRules),dest.action_token());app::OperationContext context;fault_vfs::arm(INT_MAX);dest.import_definitions("measure",request,context);writes=fault_vfs::seen;fault_vfs::disarm();}
        QVERIFY(writes>2);
        int serial=0;
        for(int error:{SQLITE_IOERR_WRITE,SQLITE_FULL})for(int point:{1,writes/2,writes}){
            const auto path=dir.filePath("fault-"+QString::number(serial++)).toStdString();bool saved=false;
            {auto dest=ImportStore::create(path,"test","Checking",cad,"2026-01-01");const auto request=ex::plan(dest,doc,choose(withRules),dest.action_token());app::OperationContext context;fault_vfs::arm(point,error);try{dest.import_definitions("io",request,context);}catch(const std::exception&){}const auto fired=fault_vfs::fired;fault_vfs::disarm();QVERIFY(fired);saved=dest.definition_import_completed("io",request);QCOMPARE(dest.category_catalog().categories.size(),saved?std::size_t(2):std::size_t(0));QCOMPARE(dest.report_catalog().definitions.size(),saved?std::size_t(1):std::size_t(0));if(withRules)QCOMPARE(dest.rule_image().ordered.size(),saved?std::size_t(1):std::size_t(0));dest.certify({});}
            {auto reopened=ImportStore::open(path,"test");QCOMPARE(reopened.report_catalog().definitions.size(),saved?std::size_t(1):std::size_t(0));reopened.certify({});}
        }
        for(const auto boundary:{ImportPhase::before_commit,ImportPhase::after_commit}){
            const auto path=dir.filePath("crash-"+QString::number(serial++)).toStdString();{auto dest=ImportStore::create(path,"test","Checking",cad,"2026-01-01");}
            const auto child=fork();QVERIFY(child>=0);
            if(child==0){try{auto dest=ImportStore::open(path,"test");const auto request=ex::plan(dest,doc,choose(withRules),dest.action_token());dest.observer([&](ImportPhase phase){if(phase==boundary)_exit(71);});app::OperationContext context;dest.import_definitions("crash",request,context);_exit(72);}catch(...){_exit(73);}}
            int status=0;QVERIFY(waitpid(child,&status,0)==child);QVERIFY(WIFEXITED(status));QCOMPARE(WEXITSTATUS(status),71);
            auto reopened=ImportStore::open(path,"test");const bool saved=boundary==ImportPhase::after_commit;QCOMPARE(reopened.category_catalog().categories.size(),saved?std::size_t(2):std::size_t(0));QCOMPARE(reopened.report_catalog().definitions.size(),saved?std::size_t(1):std::size_t(0));if(withRules)QCOMPARE(reopened.rule_image().ordered.size(),saved?std::size_t(1):std::size_t(0));reopened.certify({});
            if(saved){const auto backup=dir.filePath("backup").toStdString();reopened.backup_to(backup,"test");auto restored=ImportStore::open(backup,"test");QCOMPARE(restored.report_catalog().definitions,reopened.report_catalog().definitions);restored.certify({});}
        }
#endif
    }
    void qifTextRoundTrip(){
        QCOMPARE(app::qif_export_text("Café\tshop\r\nnext\nline"),std::string("Café shop next line"));
        const std::string allControls="one\x01\x7f\xc2\x85\xe2\x80\xa8\xe2\x80\xa9two";
        QCOMPARE(app::qif_export_text(allControls),std::string("one two"));
        QCOMPARE(app::qif_export_text(std::string("a\0b",3)),std::string("a b"));
        QVERIFY_THROWS_EXCEPTION(std::exception,app::qif_export_text(std::string("\xff",1)));
        QTemporaryDir dir;const Currency cad("CAD");auto store=ImportStore::create(dir.filePath("text").toStdString(),"test","Checking",cad,"2026-01-01");
        // Tabs and embedded CR are legal source text under the existing QIF
        // profile. CRLF field delimiters themselves must never enter values.
        const std::string bytes="!Type:Bank\r\nD1/2/2026\r\nT-12.34\r\nPCafé\tshop\rbranch\r\nMfirst\rsecond\tline\r\nS\r\nEsplit\tnote\rcontinued\r\n$-12.34\r\n^\r\n";
        store.ingest("text",bytes,qif::DateOrder::mdy);
        const auto before=store.row({"text",0});QVERIFY(before.draft);
        QCOMPARE(before.source.payee,std::string("Café\tshop\rbranch"));QCOMPARE(before.draft->payee,before.source.payee);
        QCOMPARE(before.source.memo,std::string("first\rsecond\tline"));QCOMPARE(before.draft->memo,before.source.memo);
        QCOMPARE(before.draft->transaction.allocations()[0].memo,std::string("split\tnote\rcontinued"));
        const auto token=store.action_token();app::OperationContext context;
        const auto exported=app::export_qif(store,{{1,"2026-01-01","2026-12-31"}},true,true,context);
        QVERIFY(exported.find('\t')==std::string::npos);QVERIFY(exported.find('\r')==std::string::npos);
        const auto doc=qif::index_document(exported);QVERIFY(!doc.diagnostic);const auto parsed=qif::parse_account(exported,doc,0,qif::DateOrder::mdy,cad);
        QVERIFY(parsed.valid());QCOMPARE(parsed.records.size(),std::size_t(1));const auto& row=parsed.records.front();
        QCOMPARE(row.payee,std::string("Café shop branch"));QCOMPARE(row.memo,std::string("first second line"));QCOMPARE(row.source_allocations[0].memo,std::string("split note continued"));QCOMPARE(row.transaction->effective_total().units(),std::int64_t(-1234));
        const auto after=store.row({"text",0});QCOMPARE(after.draft->payee,before.draft->payee);QCOMPARE(after.draft->memo,before.draft->memo);QCOMPARE(after.draft->transaction.allocations()[0].memo,before.draft->transaction.allocations()[0].memo);QCOMPARE(store.action_token(),token);
        // An ordinary CRLF QIF has no control characters inserted into its values.
        const auto clean=qif::parse("!Type:Bank\r\nD1/2/2026\r\nT-1\r\nPClean payee\r\nMClean memo\r\n^\r\n",qif::DateOrder::mdy,cad);QVERIFY(clean.valid());QCOMPARE(clean.records[0].payee,std::string("Clean payee"));QCOMPARE(clean.records[0].memo,std::string("Clean memo"));
        auto names=ImportStore::create(dir.filePath("names").toStdString(),"test","One\tAccount",cad,"2026-01-01");names.create_account(2,"One Account",cad,"2026-01-01");
        QVERIFY_THROWS_EXCEPTION(std::exception,app::export_qif(names,{{1,"2026-01-01","2026-01-01"},{2,"2026-01-01","2026-01-01"}},false,true,context));
    }
    void qifContents(){
        QTemporaryDir dir;const Currency cad("CAD");auto store=ImportStore::create(dir.filePath("doc").toStdString(),"test","Checking",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(10000,cad));category(store,"food","Food");category(store,"unused","Unused private category");
        store.ingest("before","D1/1/2026\nT-10\nPBefore\n^\n",qif::DateOrder::dmy);store.accept_all("accept");
        store.ingest("current","D2/1/2026\nT-12\nPOriginal\nSFood\n$-15\nSFood\n$3\n^\n",qif::DateOrder::dmy);
        auto row=store.row({"current",0});auto edited=*row.draft;edited.payee="Current edited";store.edit_transaction("edit",{"current",0},0,{},edited);
        app::OperationContext context;auto all=app::qif_export_accounts(store,context);QCOMPARE(all.front().first,std::string("2026-01-01"));QCOMPARE(all.front().last,std::string("2026-01-02"));
        const auto qif=app::export_qif(store,{{1,"2026-01-02","2026-01-02"}},true,true,context);QVERIFY(qif.find("B90.00\n")!=std::string::npos);QVERIFY(qif.find("Current edited")!=std::string::npos);QVERIFY(qif.find("Original")==std::string::npos);QVERIFY(qif.find("PBefore")==std::string::npos);QVERIFY(qif.find("Unused private category")==std::string::npos);
        const auto doc=qif::index_document(qif);QVERIFY(!doc.diagnostic);QCOMPARE(doc.accounts.size(),std::size_t(1));QCOMPARE(*doc.accounts[0].reported_balance,std::string("90.00"));
        const auto section=doc.sections.front();const auto parsed=qif::parse(std::string_view(qif).substr(section.begin,section.end-section.begin),qif::DateOrder::mdy,cad);QCOMPARE(parsed.records.size(),std::size_t(1));QVERIFY(!parsed.records.front().diagnostic);
        const auto plain=app::export_qif(store,{{1,"2026-01-01","2026-01-02"}},false,false,context);QVERIFY(plain.find("\nB")==std::string::npos);QVERIFY(plain.find("Food")==std::string::npos);QVERIFY(plain.find("$-15.00")!=std::string::npos);
        store.create_account(2,"Savings",cad,"2026-01-01");store.select_account(1);store.ingest("transfer","D3/1/2026\nT-5\nPTransfer\nL[Old Savings export]\n^\n",qif::DateOrder::dmy);auto transfer=store.row({"transfer",0});app::TransferRequest request{};request.action=app::TransferAction::from_draft;request.link="link";request.origin=imports::Occurrence{"transfer",0};request.allocation=transfer.draft->transaction.allocations()[0].identity;request.destination=2;request.expected_sequence=store.transfer_sequence();store.apply_transfer("transfer-create",request);
        const auto withTransfer=app::export_qif(store,{{1,"2026-01-01","2026-01-03"},{2,"2026-01-01","2026-01-03"}},false,false,context);QVERIFY(withTransfer.find("L[Savings]")!=std::string::npos);QVERIFY(withTransfer.find("L[Checking]")!=std::string::npos);QCOMPARE(store.account().identity,std::int64_t(1));
        const auto full=app::export_qif(store,{{1,"2026-01-01","2026-01-03"},{2,"2026-01-01","2026-01-03"}},true,true,context);
        auto restored=ImportStore::create(dir.filePath("qif-restored").toStdString(),"test","Unused",cad,"2026-01-01");
        app::OperationContext capture,resume,resolve;
        restored.prepare_import_group("qif-roundtrip",full,qif::DateOrder::mdy,{{0,2,app::AccountCreation{2,"Checking",cad,"2026-01-01"},true},{1,3,app::AccountCreation{3,"Savings",cad,"2026-01-01"},true}},capture);
        restored.resume_import_group("qif-roundtrip",resume);for(const auto& missing:restored.missing_import_categories())restored.resolve_import_categories(missing.source,missing.names,resolve);
        restored.select_account(2);QCOMPARE(restored.balance().units(),store.balance().units());QCOMPARE(restored.current_ledger().size(),store.current_ledger().size());restored.select_account(3);QCOMPARE(restored.balance().units(),std::int64_t(500));QCOMPARE(restored.current_ledger().size(),std::size_t(1));restored.certify({});
        app::OperationContext cancelled;cancelled.request();QVERIFY_THROWS_EXCEPTION(app::OperationCancelled,app::export_qif(store,{{1,"2026-01-01","2026-01-03"}},true,true,cancelled));QCOMPARE(store.account().identity,std::int64_t(1));
        const auto path=dir.filePath("out.qif");ex::publish(path,QByteArray::fromStdString(qif),dir.filePath("doc"),true,context);QFile output(path);QVERIFY(output.open(QIODevice::ReadOnly));QCOMPARE(output.readAll(),QByteArray::fromStdString(qif));output.close();
        QVERIFY_THROWS_EXCEPTION(app::OperationCancelled,ex::publish(path,"partial",dir.filePath("doc"),true,cancelled));QVERIFY(output.open(QIODevice::ReadOnly));QCOMPARE(output.readAll(),QByteArray::fromStdString(qif));
        QVERIFY_THROWS_EXCEPTION(std::exception,ex::publish(dir.filePath("doc"),"bad",dir.filePath("doc"),true,context));
        auto archived=ImportStore::create(dir.filePath("archived").toStdString(),"test","Archived",cad,"2026-01-01");archived.archive_account("archive");archived.release_account_selection();QVERIFY(!archived.account_selected());
        const auto choices=app::qif_export_accounts(archived,context);QCOMPARE(choices.size(),std::size_t(1));QVERIFY(choices.front().account.archived);QVERIFY(!archived.account_selected());
        QVERIFY(!app::export_qif(archived,{{1,"2026-01-01","2026-01-01"}},true,true,context).empty());QVERIFY(!archived.account_selected());
    }
    void menuAndDialogs(){
        QSettings().clear();QTemporaryDir dir;ImportModel model(document_gateway());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().empty());auto* root=engine.rootObjects().front();
        auto find=[&](const char* name)->QObject*{if(auto* item=root->findChild<QObject*>(name))return item;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),QString::fromLatin1(name)):nullptr;};
        for(const auto* name:{"fileQifExport","fileSpentItExport","fileSpentItImport","fileBackup"}){QVERIFY(find(name));QVERIFY(!find(name)->property("enabled").toBool());}
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        for(const auto* name:{"fileQifExport","fileSpentItExport","fileSpentItImport","fileBackup"})QVERIFY(find(name)->property("enabled").toBool());
        QFile imported(dir.filePath("input.qif"));QVERIFY(imported.open(QIODevice::WriteOnly));imported.write("!Type:Bank\nD2026-02-01\nT-12.34\nPMerchant\tbranch\nMMemo\rcontinued\n^\n");imported.close();run(model,"import",{{"path",imported.fileName()},{"account","1"},{"profile","MDY"}});QCOMPARE(model.state().value("balance").toString(),"-12.34");
        run(model,"create_account",{{"id","2"},{"name","Other"},{"currency","CAD"},{"date","2026-01-01"}});run(model,"select_account",{{"id","1"}});

        QVERIFY(QMetaObject::invokeMethod(find("fileQifExport"),"triggered"));QTRY_VERIFY(!model.busy());QVERIFY(model.error().isEmpty());QTRY_VERIFY(find("qifExportDialog")->property("visible").toBool());QVERIFY(find("qifIncludeOpening")->property("checked").toBool());QVERIFY(find("qifExportSave")->property("enabled").toBool());
        QTRY_VERIFY(find("qifAccount_0"));QTRY_VERIFY(find("qifAccount_1"));
        for(const auto* name:{"qifAccount_0","qifAccount_1"}){QVERIFY(find(name)->property("checked").toBool());find(name)->setProperty("checked",false);QVERIFY(QMetaObject::invokeMethod(find(name),"clicked"));}
        QVERIFY(!find("qifExportSave")->property("enabled").toBool());
        for(const auto* name:{"qifAccount_0","qifAccount_1"}){find(name)->setProperty("checked",true);QVERIFY(QMetaObject::invokeMethod(find(name),"clicked"));}QTRY_VERIFY(find("qifExportSave")->property("enabled").toBool());QTest::qWait(100);if(auto* window=qobject_cast<QQuickWindow*>(root))window->grabWindow().save("/tmp/spentit-qif-export.png");
        auto* picker=find("exchangeSaveDialog");QVERIFY(picker);QCOMPARE(picker->property("fileMode").toInt(),2);
        // Exercise the typed filename control deterministically on every platform.
        picker->setProperty("options",8); // QFileDialogOptions::DontUseNativeDialog
        picker->setProperty("currentFolder",QUrl::fromLocalFile(dir.path()));
        QVERIFY(QMetaObject::invokeMethod(find("qifExportSave"),"clicked"));
        QTRY_VERIFY(picker->property("visible").toBool());
        QCOMPARE(picker->property("fileMode").toInt(),2);
        auto* filename=find("fileNameTextField");QVERIFY(filename);
        filename->setProperty("text","via-picker.qif");
        QVERIFY(QMetaObject::invokeMethod(filename,"textEdited"));
        QVERIFY(QMetaObject::invokeMethod(filename,"editingFinished"));
        QObject* impl=filename;while(impl && !impl->inherits("QQuickFileDialogImpl"))impl=impl->parent();
        QVERIFY(impl);
        QVERIFY(QMetaObject::invokeMethod(impl,"accept"));
        QVERIFY(model.busy());QVERIFY(find("qifExportFeedback")->property("visible").toBool());QVERIFY(!find("qifExportSave")->property("enabled").toBool());
        QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QVERIFY(QFileInfo::exists(dir.filePath("via-picker.qif")));
        QFile qifOutput(dir.filePath("via-picker.qif"));QVERIFY(qifOutput.open(QIODevice::ReadOnly));const auto exportedBytes=qifOutput.readAll();QVERIFY(exportedBytes.contains("PMerchant branch\nMMemo continued\n"));QCOMPARE(exportedBytes.count("!Account\n"),2);
        QMetaObject::invokeMethod(find("exchangeResultDialog"),"close");
        QVERIFY(QMetaObject::invokeMethod(find("fileQifExport"),"triggered"));QTRY_VERIFY(!model.busy());QTRY_VERIFY(find("qifExportDialog")->property("visible").toBool());
        const auto selectDestination=[&](const QString& name){
            auto* filenameField=find("fileNameTextField");if(!filenameField)return false;
            filenameField->setProperty("text",name);
            if(!QMetaObject::invokeMethod(filenameField,"textEdited") || !QMetaObject::invokeMethod(filenameField,"editingFinished"))return false;
            auto* dialogImpl=filenameField;while(dialogImpl && !dialogImpl->inherits("QQuickFileDialogImpl"))dialogImpl=dialogImpl->parent();
            return dialogImpl && QMetaObject::invokeMethod(dialogImpl,"accept");
        };
        QVERIFY(QMetaObject::invokeMethod(find("qifExportSave"),"clicked"));QTRY_VERIFY(picker->property("visible").toBool());QVERIFY(selectDestination("bare-name"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QVERIFY(QFileInfo::exists(dir.filePath("bare-name.qif")));QMetaObject::invokeMethod(find("exchangeResultDialog"),"close");
        // A rejected destination stays visible beside the export controls.
        QVERIFY(QMetaObject::invokeMethod(find("fileQifExport"),"triggered"));QTRY_VERIFY(!model.busy());QTRY_VERIFY(find("qifExportDialog")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(find("qifExportSave"),"clicked"));QTRY_VERIFY(picker->property("visible").toBool());QVERIFY(selectDestination("wrong.txt"));QTRY_VERIFY(!model.busy());QVERIFY(!model.error().isEmpty());QVERIFY(find("qifExportFeedback")->property("visible").toBool());QVERIFY(find("qifExportSave")->property("enabled").toBool());QVERIFY(!QFileInfo::exists(dir.filePath("wrong.txt")));QMetaObject::invokeMethod(find("qifExportDialog"),"close");
        QVERIFY(QMetaObject::invokeMethod(find("fileSpentItExport"),"triggered"));QTRY_VERIFY(find("definitionExportDialog")->property("visible").toBool());QVERIFY(find("definitionExportSave")->property("enabled").toBool());
        for(const auto* name:{"exportRules","exportReports","exportCategories"})QVERIFY(find(name)->property("checked").toBool());
        for(const auto* name:{"exportRules","exportReports","exportCategories"})find(name)->setProperty("checked",false);QVERIFY(!find("definitionExportSave")->property("enabled").toBool());QMetaObject::invokeMethod(find("definitionExportDialog"),"close");
        QVERIFY(QMetaObject::invokeMethod(find("fileSpentItExport"),"triggered"));QTRY_VERIFY(find("definitionExportDialog")->property("visible").toBool());
        for(const auto* name:{"exportRules","exportReports","exportCategories"})QVERIFY(find(name)->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(find("definitionExportSave"),"clicked"));QTRY_VERIFY(picker->property("visible").toBool());QVERIFY(selectDestination("via-picker"));QVERIFY(model.busy());QVERIFY(find("definitionExportFeedback")->property("visible").toBool());QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QVERIFY(QFileInfo::exists(dir.filePath("via-picker.spentit.json")));QMetaObject::invokeMethod(find("exchangeResultDialog"),"close");
        run(model,"exchange_export",{{"path",dir.filePath("empty.spentit.json")},{"categories",true}});QMetaObject::invokeMethod(find("exchangeResultDialog"),"close");run(model,"exchange_read",{{"path",dir.filePath("empty.spentit.json")}});QTRY_VERIFY(find("definitionImportDialog")->property("visible").toBool());QVERIFY(!find("definitionImportSave")->property("enabled").toBool());
        QMetaObject::invokeMethod(find("definitionImportDialog"),"close");
        {auto source=ImportStore::create(dir.filePath("source").toStdString(),"test","Checking",Currency("CAD"),"2026-01-01");definitions(source);QFile file(dir.filePath("definitions.spentit.json"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(ex::encode(source,source.rules_supported(),true,true));}
        run(model,"exchange_read",{{"path",dir.filePath("definitions.spentit.json")}});QTRY_VERIFY(find("definitionImportDialog")->property("visible").toBool());QVERIFY(find("definitionImportSave")->property("enabled").toBool());
        QTest::qWait(150);if(auto* window=qobject_cast<QQuickWindow*>(root))window->grabWindow().save("/tmp/spentit-definition-import.png");
        QVERIFY(QMetaObject::invokeMethod(find("definitionImportSave"),"clicked"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));QVERIFY(!find("definitionImportDialog")->property("visible").toBool());QMetaObject::invokeMethod(find("exchangeResultDialog"),"close");
        QCOMPARE(model.state().value("reports").toList().size(),1);run(model,"close");QCOMPARE(warnings.size(),0);
    }
};
int main(int argc,char** argv){QQuickStyle::setStyle("Basic");QGuiApplication app(argc,argv);FileExchangeTests tests;return QTest::qExec(&tests,argc,argv);}
#include "file_exchange_tests.moc"
