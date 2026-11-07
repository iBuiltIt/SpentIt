#include <QElapsedTimer>
#include <QThread>
#include <QScreen>
#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QSignalSpy>
#include <QMetaProperty>
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
#include <QRegularExpression>
#include <QAccessible>
#include <QJSValue>
#include <QStyleHints>
#include <sqlite3.h>
using namespace spentit;
// Opt-in synthetic-only tracing. Forward every event through the original
// handler/native bridge: observing notifications must not suppress the crash path.
class SplitAccessibilityTrace {
    inline static SplitAccessibilityTrace* current=nullptr;
    QAccessible::UpdateHandler previous=nullptr;
    int statusNames=0,otherNames=0,announcements=0;
    static void observe(QAccessibleEvent* event) {
        auto* self=current;
        if(event->type()==QAccessible::NameChanged) {
            if(event->object() && event->object()->objectName()=="operationPhase")++self->statusNames;
            else ++self->otherNames;
        }
        if(event->type()==QAccessible::Announcement)++self->announcements;
        QAccessible::installUpdateHandler(self->previous);
        QAccessible::updateAccessibility(event);
        QAccessible::installUpdateHandler(observe);
    }
public:
    SplitAccessibilityTrace() {
        if(qEnvironmentVariableIsSet("SPENTIT_SPLIT_AX_TRACE")){current=this;previous=QAccessible::installUpdateHandler(observe);}
    }
    ~SplitAccessibilityTrace() {
        if(current!=this)return;
        QAccessible::installUpdateHandler(previous);current=nullptr;
        qInfo()<<"Accessibility trace: status name changes"<<statusNames<<"other name changes"<<otherNames<<"announcements"<<announcements<<"active"<<QAccessible::isActive();
    }
};
// Delegates every real storage operation; only caller acknowledgement and
// outcome-query failures are injected. These tests never replace durability.
class OutcomeGateway final : public app::DocumentGateway, public app::ImportRepository {
public:
    std::unique_ptr<app::DocumentGateway> inner=storage::document_gateway();
    enum Fault { none, before, after, unknown } fault=none;
    bool attempted=false,cancelAfterOpen=false,failAccountRefresh=false,failAccountSelection=false;
    QString groupFaultAction;bool groupInjected=false,cancelGroupResume=false;
    app::ImportRepository& repo() const { return inner->repository(); }
    bool failReviewRefresh=false;
    bool delayClose=false,failClose=false;
    void check_file(const app::OperationContext& context) override {inner->check_file(context);}
    void close_verified(const app::OperationContext& context) override {
        if(failClose)throw std::runtime_error("Injected close failure");
        if(delayClose){context.checkpoint("Checking file");QThread::msleep(1000);}
        inner->close_verified(context);
    }
    app::ReviewSheet review_sheet() const override {if(failReviewRefresh && attempted)throw std::runtime_error("Injected Review refresh failure");return repo().review_sheet();}
    app::ReviewReceipt merge_review_batch(std::string_view id,const std::vector<app::ReviewAnswer>& answers,const app::OperationContext& context) override {const bool lose=groupBefore("review_batch");auto result=repo().merge_review_batch(id,answers,context);if(lose)throw std::runtime_error("Injected lost batch acknowledgement");return result;}
    std::optional<app::ReviewReceipt> review_outcome(std::string_view id) const override {if(attempted && fault==unknown && groupFaultAction=="review_batch")throw app::OperationOutcomeUnknown();return repo().review_outcome(id);}

    void create(const std::filesystem::path& p,bool mode,std::string_view pw,std::string_view name,const Currency& c,std::string_view day) override {inner->create(p,mode,pw,name,c,day);}
    void open(const std::filesystem::path& p,std::optional<std::string> pw) override {inner->open(p,std::move(pw));}
    void open_controlled(const std::filesystem::path& p,std::optional<std::string> pw,const app::OperationContext& c) override {inner->open_controlled(p,std::move(pw),c);if(cancelAfterOpen)c.request();}
    void close() override {inner->close();}
    void backup(const std::filesystem::path& p) override {inner->backup(p);}
    void backup_controlled(const std::filesystem::path& p,const app::OperationContext& context) override {inner->backup_controlled(p,context);}
    app::ImportRepository& repository() override {return *this;}
    std::int64_t next_account_identity() const override{return repo().next_account_identity();}
    qif::Result parsed_source(std::string_view id,const std::function<void()>& checkpoint={}) const override{return repo().parsed_source(id,checkpoint);}
    app::ImportRepairProgress repair_progress(std::string_view id) const override{return repo().repair_progress(id);}
    std::vector<app::MissingImportCategories> missing_import_categories() const override {return repo().missing_import_categories();}
    void resolve_import_categories(std::string_view source,const std::vector<std::string>& names,app::OperationContext& c) override {repo().resolve_import_categories(source,names,c);}
    std::vector<app::ImportGroup> import_groups() const override{return repo().import_groups();}
    std::optional<app::ImportGroup> import_group_outcome(std::string_view id) const override {if(attempted && fault==unknown && !groupFaultAction.isEmpty())throw std::runtime_error("Injected unknown import outcome");return repo().import_group_outcome(id);}
    bool groupBefore(const QString& action){if(groupFaultAction!=action || groupInjected)return false;attempted=true;groupInjected=true;if(fault==before)throw std::runtime_error("Injected before import operation");return fault==after || fault==unknown;}
    app::ImportGroup prepare_import_group(std::string_view id,std::string_view bytes,qif::DateOrder profile,const std::vector<app::ImportAccountMapping>& mappings,app::OperationContext& context,std::string_view label="qif") override {const bool lose=groupBefore("capture");auto result=repo().prepare_import_group(id,bytes,profile,mappings,context,label);if(lose)throw std::runtime_error("Injected lost capture acknowledgement");return result;}
    app::ImportGroup resume_import_group(std::string_view id,app::OperationContext& context) override {if(cancelGroupResume)context.request();const bool lose=groupBefore("resume");auto result=repo().resume_import_group(id,context);if(lose)throw std::runtime_error("Injected lost resume acknowledgement");return result;}
    void review_import_balance(std::string_view id,std::size_t source,std::int64_t revision,app::ReportedBalanceChoice choice,std::string_view date) override {const bool lose=groupBefore("balance");repo().review_import_balance(id,source,revision,choice,date);if(lose)throw std::runtime_error("Injected lost balance acknowledgement");}
    bool cleanup_incomplete_imports(std::string_view id={}) override {return repo().cleanup_incomplete_imports(id);}
    bool startup_import_cleanup() const override {return repo().startup_import_cleanup();}
    void abort_import_group(std::string_view id) override {const bool lose=groupBefore("abort");repo().abort_import_group(id);if(lose)throw std::runtime_error("Injected lost Abort acknowledgement");}
    app::Account account() const override {return repo().account();}
    std::vector<app::AccountAlias> account_aliases() const override {return repo().account_aliases();}
    std::optional<std::int64_t> resolve_account_name(std::string_view name) const override {return repo().resolve_account_name(name);}
    std::vector<app::Account> accounts() const override {if(attempted && failAccountRefresh)throw std::runtime_error("Injected account refresh failure");return repo().accounts();}
    void select_account(std::int64_t id) override {if(attempted && failAccountSelection)throw std::runtime_error("Injected selection failure");repo().select_account(id);}
    app::Account create_account(std::int64_t id,std::string_view name,const Currency& c,std::string_view date) override {
        attempted=true;if(fault==before)throw std::runtime_error("Injected failure before account creation");
        auto result=repo().create_account(id,name,c,date);
        if(fault==after || fault==unknown)throw std::runtime_error("Injected lost account acknowledgement");return result;
    }
    std::optional<app::AccountCreation> account_creation_outcome(std::int64_t id) const override {
        if(attempted && fault==unknown)throw std::runtime_error("Injected account outcome failure");return repo().account_creation_outcome(id);
    }
    bool transfers_supported() const override {return true;}
    std::int64_t transfer_sequence() const override {return repo().transfer_sequence();}
    std::vector<app::TransferView> transfers() const override {return repo().transfers();}
    app::TransferPreview preview_transfer(const app::TransferRequest& request) const override {return repo().preview_transfer(request);}
    app::TransferReceipt apply_transfer(std::string_view operation,const app::TransferRequest& request) override {
        attempted=true;if(fault==before)throw std::runtime_error("Injected failure before transfer");
        const bool already=repo().transfer_outcome(operation).has_value();auto result=repo().apply_transfer(operation,request);if(!already && (fault==after || fault==unknown))throw std::runtime_error("Injected lost transfer acknowledgement");return result;
    }
    std::optional<app::TransferReceipt> transfer_outcome(std::string_view operation) const override {
        if(attempted && fault==unknown)throw std::runtime_error("Injected transfer outcome failure");return repo().transfer_outcome(operation);
    }
    std::vector<app::TransferJournalEntry> transfer_journal() const override {return repo().transfer_journal();}
    void opening(std::int64_t r,std::string_view d,const Money& m) override {repo().opening(r,d,m);}
    std::vector<app::BatchInfo> batches() const override {return repo().batches();}
    app::BatchInfo ingest(std::string_view id,std::string_view b,qif::DateOrder p) override {return repo().ingest(id,b,p);}
    std::string evidence(std::string_view b) const override {return repo().evidence(b);}
    app::RowView row(const imports::Occurrence& o) const override {return repo().row(o);}
    app::ReviewReceipt edit_transaction(std::string_view op,const imports::Occurrence& id,std::int64_t revision,const std::optional<app::EntryToken>& token,const imports::DraftRow& draft) override {return repo().edit_transaction(op,id,revision,token,draft);}
    bool can_undo_transaction_edit(std::string_view op) const override {return repo().can_undo_transaction_edit(op);}
    app::ReviewReceipt undo_transaction_edit(std::string_view op,std::string_view edit) override {return repo().undo_transaction_edit(op,edit);}
    void save(const imports::Occurrence& o,std::int64_t r,const imports::DraftRow& d) override {repo().save(o,r,d);}
    void decide(const imports::Occurrence& o,std::int64_t r,const imports::Decision& d) override {repo().decide(o,r,d);}
    void discard(std::string_view b,std::int64_t r) override {repo().discard(b,r);}
    bool transaction_accepted(const imports::Occurrence& id) const override{return repo().transaction_accepted(id);}
    app::CommittedEntry committed_entry(const imports::Occurrence& id) const override{return repo().committed_entry(id);}
    app::ReviewReceipt answer_review(std::string_view op,const app::ReviewAnswer& a) override{return repo().answer_review(op,a);}
    app::ReviewReceipt undo_review(std::string_view op) override{return repo().undo_review(op);}
    app::AcceptanceReview acceptance_review() const override{return repo().acceptance_review();}
    app::AcceptanceReceipt accept_all(std::string_view op,bool ignore=false) override {
        attempted=true;if(fault==before)throw std::runtime_error("Injected failure before commit");
        auto receipt=repo().accept_all(op,ignore);if(fault==after || fault==unknown)throw std::runtime_error("Injected lost commit acknowledgement");return receipt;
    }
    std::optional<app::AcceptanceReceipt> acceptance_outcome(std::string_view op) const override {if(attempted && fault==unknown)throw std::runtime_error("Injected outcome lookup failure");return repo().acceptance_outcome(op);}
    app::ImportResult import_result(std::string_view id) const override{return repo().import_result(id);}
    std::vector<app::UndoableImport> undoable_imports() const override{return repo().undoable_imports();}
    app::RollbackReview review_import_undo(std::string_view source) const override{return repo().review_import_undo(source);}
    app::RollbackReceipt undo_import(std::string_view op,std::string_view source,std::int64_t token) override{const bool lose=groupBefore("undo_import");auto result=repo().undo_import(op,source,token);if(lose)throw std::runtime_error("Injected lost import undo acknowledgement");return result;}
    std::optional<app::RollbackReceipt> import_undo_outcome(std::string_view op,std::string_view source) const override{if(attempted && fault==unknown && groupFaultAction=="undo_import")throw app::OperationOutcomeUnknown();return repo().import_undo_outcome(op,source);}
    app::RollbackReview rollback_review() const override{return repo().rollback_review();}
    app::RollbackReceipt rollback_account(std::string_view op,std::int64_t token) override{return repo().rollback_account(op,token);}
    std::optional<app::RollbackReceipt> rollback_outcome(std::string_view op) const override{return repo().rollback_outcome(op);}
    app::Receipt commit(std::string_view b,std::int64_t r,std::string_view op) override {
        attempted=true;
        if(fault==before) throw std::runtime_error("Injected failure before commit");
        auto receipt=repo().commit(b,r,op);
        if(fault==after || fault==unknown) throw std::runtime_error("Injected lost commit acknowledgement");
        return receipt;
    }
    std::optional<app::Receipt> outcome(std::string_view op) const override {
        if(attempted && fault==unknown) throw std::runtime_error("Injected outcome lookup failure");
        return repo().outcome(op);
    }
    std::vector<app::LedgerRow> ledger(bool p) const override {return repo().ledger(p);}
    std::vector<app::LedgerRow> current_ledger() const override {return repo().current_ledger();}
    Money balance() const override {return repo().balance();}
    std::vector<std::pair<std::int64_t,Money>> balances() const override {return repo().balances();}
    Money committed_balance() const override {return repo().committed_balance();}
    std::optional<Money> projected_balance() const override {return repo().projected_balance();}
    std::vector<app::AccountBalance> account_balances() const override {return repo().account_balances();}
};
class ImportUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QQuickItem* visualItem(QQuickItem* parent,const QString& name) {
        if(parent->objectName()==name) return parent;
        for(auto* child:parent->childItems()) if(auto* found=visualItem(child,name)) return found;
        return nullptr;
    }
    static QVariantMap selected(const ImportModel& m) { return m.state().value("selected").toMap(); }
    static QString fixture(const QString& dir,const QByteArray& bytes) {
        const auto path=dir+"/synthetic.qif";QFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())qFatal("Fixture failed");return path;
    }
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {
        auto request=args;if(action=="import" && !request.contains("account"))request["account"]=model.state().value("accountId");model.command(action,request);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty()==success,qPrintable(action+": "+model.error()));
        if(success && action=="resolve_import_categories" && model.diagnosticsEnabled()) {
            const auto observed=diagnostics::Collector::instance().snapshot();
            QCOMPARE(observed.latest.action,diagnostics::Action::resolve_import_categories);
            QCOMPARE(observed.latest.result,diagnostics::Result::succeeded);
            QCOMPARE(observed.latest.persistence,diagnostics::Persistence::confirmed_saved);
            QVERIFY(observed.latest.delivered!=0);
            QVERIFY(observed.metrics[static_cast<unsigned>(diagnostics::Code::import_category_resolution)].calls>0);
        }
        if(success && (action=="import" || action=="map_import"))for(const auto& item:model.state().value("importCategoryDecisions").toList()){const auto d=item.toMap();run(model,"resolve_import_categories",{{"source",d.value("source")},{"selected",d.value("names")}});}
    }
    void create(ImportModel& m,const QString& path) {
        run(m,"create",{{"path",path},{"protected",false},{"name","Synthetic test"},{"currency","CAD"}});
    }
private slots:
    void importRoutingAndLedgerCleanup() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/document");
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Other"},{"currency","CAD"},{"date","2026-01-01"}});
        const auto unnamed=fixture(dir.path(),"!Type:Bank\nD2026-02-01\nT-1\nPAnonymous\n^\n");
        run(m,"import",{{"path",unnamed},{"account",""}});QVERIFY(m.state().value("importDestination").toBool());QCOMPARE(m.rowCount(),1);
        run(m,"confirm_import_destination",{{"account","1"}});QCOMPARE(m.state().value("accountId").toString(),"1");QCOMPARE(m.state().value("balance").toString(),"-1.00");
        auto named=fixture(dir.path(),"!Account\nNOther\nTBank\n^\n!Type:Bank\nD2026-02-02\nT-2\nPNamed\n^\n");
        run(m,"import",{{"path",named},{"account","1"}});QCOMPARE(m.state().value("importPlan").toList().size(),1);run(m,"cancel_import_mapping");
        run(m,"import",{{"path",named},{"account",""}});QVERIFY(m.state().value("importPlan").toList().isEmpty());QCOMPARE(m.state().value("accountId").toString(),"2");QCOMPARE(m.state().value("balance").toString(),"-2.00");
        named=fixture(dir.path(),"!Account\nNUnknown\nTBank\n^\n!Type:Bank\nD2026-02-03\nT-3\nPUnknown\n^\n");run(m,"import",{{"path",named},{"account",""}});QCOMPARE(m.state().value("importPlan").toList().size(),1);run(m,"cancel_import_mapping");
        const auto ofx=QFileInfo(QString::fromUtf8(SPENTIT_REVIEW_FIXTURE)).absoluteDir().filePath("../statements/bank-sgml.ofx");
        run(m,"import",{{"path",ofx},{"account",""}});QCOMPARE(m.state().value("importPlan").toList().size(),1);
        run(m,"map_import",{{"mappings",QVariantList{QVariantMap{{"source",0},{"account","1"},{"create",false}}}}});
        run(m,"import",{{"path",ofx},{"account",""}});QVERIFY(m.state().value("importPlan").toList().isEmpty());QCOMPARE(m.state().value("accountId").toString(),"1");QVERIFY(m.state().value("importFeedback").toString().contains("already present"));
        run(m,"import",{{"path",ofx},{"account","2"}});QCOMPARE(m.state().value("importPlan").toList().size(),1);run(m,"cancel_import_mapping");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QCOMPARE(window->title(),"SpentIt — "+m.state().value("displayPath").toString());
        QVERIFY(!window->findChild<QObject*>("resumeImport"));QVERIFY(!window->findChild<QObject*>("viewSourceFile"));QVERIFY(!window->findChild<QObject*>("reviewRecord"));QVERIFY(!window->findChild<QObject*>("bulkRemove"));
        QVERIFY(window->findChild<QObject*>("importStatement"));QVERIFY(window->findChild<QObject*>("commitImport"));QVERIFY(window->findChild<QObject*>("discardImport"));QVERIFY(window->findChild<QObject*>("settingsBackup"));
        run(m,"close");run(m,"probe",{{"path",QDir::homePath()+"/spentit-missing-title-fixture.spentit"}},false);QCOMPARE(m.state().value("displayPath").toString(),"~/spentit-missing-title-fixture.spentit");
    }
    void ledgerDuplicateReview() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPOlder A\n^\nD2026-02-01\nT-10\nPOlder B\n^\nD2026-02-01\nT-2\nPNeighbour\n^\n")}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPNew download\n^\n")}});
        const auto questions=m.state().value("reviewQuestions").toList();QCOMPARE(questions.size(),1);
        QVariantMap fresh,old;for(const auto& v:questions[0].toMap().value("rows").toList()){const auto r=v.toMap();if(r.value("payee")=="New download")fresh=r;else if(r.value("payee")=="Older A")old=r;}
        QVERIFY(!fresh.isEmpty());QCOMPARE(fresh.value("duplicateCandidates").toList().size(),2);
        run(m,"select",{{"row",m.occurrenceIndex(fresh.value("batch").toString(),fresh.value("ordinal").toInt())}});
        QTRY_COMPARE(window->property("duplicateOriginKey").toString(),fresh.value("key").toString());
        auto click=[&](const char* name){auto* item=visualItem(window->contentItem(),QString::fromUtf8(name));if(!item)return false;return QMetaObject::invokeMethod(item,"clicked");};
        QTRY_VERIFY(visualItem(window->contentItem(),"ledgerDuplicateMerge"));QVERIFY(!visualItem(window->contentItem(),"ledgerDuplicateMerge")->isEnabled());
        run(m,"select",{{"row",m.occurrenceIndex(old.value("batch").toString(),old.value("ordinal").toInt())}});
        QTRY_COMPARE(window->property("duplicateCandidateKey").toString(),old.value("key").toString());
        if(!qEnvironmentVariableIsEmpty("SPENTIT_LEDGER_PREVIEW")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_LEDGER_PREVIEW")));}
        // Revealing a filtered-out candidate is explicit and clears that filter.
        run(m,"query",{{"filter","payee:New"}});
        QVERIFY(click("ledgerDuplicateShow"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(m.state().value("filter").toString().isEmpty());
        QVERIFY(m.occurrenceIndex(old.value("batch").toString(),old.value("ordinal").toInt())>=0);
        QVERIFY(click("ledgerDuplicateSeparate"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("reviewQuestions").toList()[0].toMap().value("rows").toList().size(),2);QCOMPARE(m.rowCount(),5);QCOMPARE(m.state().value("balance").toString(),"-32.00");
        QVERIFY(click("ledgerReviewUndo"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QCOMPARE(m.state().value("reviewQuestions").toList()[0].toMap().value("rows").toList().size(),3);
        // Reset the origin explicitly, choose a pair, and merge without a survivor dialog.
        QVERIFY(QMetaObject::invokeMethod(window,"showDuplicateInLedger",Q_ARG(QVariant,fresh),Q_ARG(QVariant,fresh)));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        run(m,"select",{{"row",m.occurrenceIndex(old.value("batch").toString(),old.value("ordinal").toInt())}});
        QVERIFY(click("ledgerDuplicateMerge"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QVERIFY(!window->findChild<QObject*>("reviewSurvivor")->property("visible").toBool());QCOMPARE(m.rowCount(),4);QCOMPARE(m.state().value("balance").toString(),"-22.00");QVERIFY(m.occurrenceIndex(old.value("batch").toString(),old.value("ordinal").toInt())>=0);QCOMPARE(m.occurrenceIndex(fresh.value("batch").toString(),fresh.value("ordinal").toInt()),-1);
        QVERIFY(click("ledgerReviewUndo"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QCOMPARE(m.rowCount(),5);
        QVERIFY(QMetaObject::invokeMethod(window,"showDuplicateInLedger",Q_ARG(QVariant,fresh),Q_ARG(QVariant,fresh)));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY(click("ledgerDuplicateNone"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.rowCount(),5);
    }
    void ledgerDuplicateViewport() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);window->resize(1200,650);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        QByteArray records;for(int i=1;i<=60;++i)records+="D2026-02-01\nT-"+QByteArray::number(i)+"\nPOlder "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),records)}});run(m,"commit");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-20\nPDownloaded again\n^\n")}});
        QVariantMap fresh,old;for(const auto& v:m.state().value("reviewQuestions").toList()[0].toMap().value("rows").toList()){const auto r=v.toMap();if(r.value("committed").toBool())old=r;else fresh=r;}
        QVERIFY(QMetaObject::invokeMethod(window,"showDuplicateInLedger",Q_ARG(QVariant,fresh),Q_ARG(QVariant,fresh)));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QTest::qWait(100);
        auto* ledger=visualItem(window->contentItem(),"durableLedger");QVERIFY(ledger);QVERIFY(ledger->property("contentY").toReal()>0);
        auto* merge=visualItem(window->contentItem(),"ledgerDuplicateMerge");QVERIFY(merge && merge->isEnabled());QVERIFY(QMetaObject::invokeMethod(merge,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        const auto survivor=m.occurrenceIndex(old.value("batch").toString(),old.value("ordinal").toInt());QCOMPARE(m.state().value("selectionIndex").toInt(),survivor);
        const auto name="ledgerRow_"+QString::number(survivor);QTRY_VERIFY(visualItem(ledger,name));
        QTRY_VERIFY(visualItem(ledger,name)->y()>=ledger->property("contentY").toReal()-1);
        QTRY_VERIFY(visualItem(ledger,name)->y()+visualItem(ledger,name)->height()<=ledger->property("contentY").toReal()+ledger->height()+1);
        auto* undo=visualItem(window->contentItem(),"ledgerReviewUndo");QVERIFY(undo);QVERIFY(QMetaObject::invokeMethod(undo,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QCOMPARE(m.state().value("reviewCount").toInt(),1);
    }
    void batchReviewOutcomes() {
        for(int mode=0;mode<4;++mode){
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* fault=gateway.get();ImportModel m(std::move(gateway));create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
            run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPA\n^\nD2026-02-01\nT-20\nPA\n^\n")}});run(m,"commit");run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPB\n^\nD2026-02-01\nT-20\nPB\n^\n")}});
            QVariantList groups;for(const auto& value:m.state().value("reviewQuestions").toList()){const auto q=value.toMap();groups.push_back(QVariantMap{{"question",q.value("key")},{"generation",m.state().value("reviewGeneration")},{"selected",q.value("rows")}});}QCOMPARE(groups.size(),2);
            fault->groupFaultAction="review_batch";fault->fault=mode==0?OutcomeGateway::before:mode==2?OutcomeGateway::unknown:OutcomeGateway::after;fault->failReviewRefresh=mode==3;
            run(m,"review_batch",{{"groups",groups}},mode==1);
            if(mode==0){QVERIFY(m.state().value("active").toBool());QCOMPARE(m.state().value("balance").toString(),"-60.00");}
            if(mode==1){QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"-30.00");}
            if(mode>=2)QVERIFY(!m.state().value("active").toBool());
            if(mode==3)QVERIFY(m.error().contains("saved"));
            fault->fault=OutcomeGateway::none;fault->failReviewRefresh=false;fault->groupFaultAction.clear();if(m.state().value("active").toBool())run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});QCOMPARE(m.state().value("balance").toString(),mode==0?"-60.00":"-30.00");run(m,"close");
        }
    }
    void batchReviewSurvivorSelection() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        for(int pass=0;pass<2;++pass){const QByteArray record="D2026-02-01\nT-10\nP"+QByteArray::number(pass)+" synthetic\n^\n";run(m,"import",{{"path",fixture(dir.path(),record+record)}});}
        QCOMPARE(m.state().value("reviewCount").toInt(),1);QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));QTest::qWait(100);
        auto* group=visualItem(window->contentItem(),"reviewGroupSelect_0");QVERIFY(group);QVERIFY(!group->isEnabled());
        for(int i=0;i<2;++i){auto* row=visualItem(window->contentItem(),"reviewRowSelect_"+QString::number(i));QVERIFY(row);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,row->mapToScene(QPointF(row->width()/2,row->height()/2)).toPoint());}
        QTRY_VERIFY(group->isEnabled());const auto clickGroup=[&]{QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,group->mapToScene(QPointF(group->width()/2,group->height()/2)).toPoint());};
        clickGroup();auto* survivor=window->findChild<QObject*>("reviewSurvivor");QVERIFY(survivor);QTRY_VERIFY(survivor->property("visible").toBool());QCOMPARE(window->property("reviewBatch").value<QJSValue>().toVariant().toMap().size(),0);
        QVERIFY(QMetaObject::invokeMethod(survivor,"reject"));QTRY_VERIFY(!survivor->property("visible").toBool());QTRY_VERIFY(!group->property("checked").toBool());
        clickGroup();QTRY_VERIFY(survivor->property("visible").toBool());QTRY_VERIFY(visualItem(window->contentItem(),"reviewKeep_0"));QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewKeep_0"),"clicked"));QTRY_VERIFY(group->property("checked").toBool());QCOMPARE(m.state().value("balance").toString(),"-40.00");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"mergeSelectedMatches"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),"-30.00");QCOMPARE(m.state().value("reviewCount").toInt(),1);run(m,"review_undo");QCOMPARE(m.state().value("balance").toString(),"-40.00");run(m,"close");
    }
    void batchReviewWorkspace() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        for(int pass=0;pass<2;++pass){QString bytes="!Type:Bank\n";for(int i=1;i<=40;++i)bytes+="D2026-02-01\nT-"+QString::number(i)+"\nP"+QString::number(pass)+" synthetic\n^\n";run(m,"import",{{"path",fixture(dir.path(),bytes.toUtf8())}});if(!pass)run(m,"commit");}
        QCOMPARE(m.state().value("reviewCount").toInt(),40);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));
        auto* status=visualItem(window->contentItem(),"workspaceStatus");auto* list=visualItem(window->contentItem(),"reviewQuestions");QVERIFY(status && list);QTest::qWait(30);const auto statusPosition=status->mapToScene(QPointF());const auto listPosition=list->mapToScene(QPointF());const auto listHeight=list->height();
        const auto select=[&](int index){const auto q=m.state().value("reviewQuestions").toList().at(index).toMap();QVERIFY(QMetaObject::invokeMethod(window,"selectReviewGroup",Q_ARG(QVariant,QVariant(q)),Q_ARG(QVariant,q.value("rows"))));};
        if(qEnvironmentVariableIsSet("SPENTIT_REVIEW_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REVIEW_SCREENSHOT")));}
        select(0);select(1);QCOMPARE(window->property("reviewBatch").value<QJSValue>().toVariant().toMap().size(),2);
        // A read-only inspection refresh must retain group intent.
        const auto row=m.state().value("reviewQuestions").toList().at(0).toMap().value("rows").toList().at(0).toMap();run(m,"inspect",{{"batch",row.value("batch")},{"ordinal",row.value("ordinal")}});
        QCOMPARE(window->property("reviewBatch").value<QJSValue>().toVariant().toMap().size(),2);
        QVERIFY(QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,10),Q_ARG(int,0)));QTest::qWait(50);
        int anchorIndex=-1;QVERIFY(QMetaObject::invokeMethod(list,"indexAt",Q_RETURN_ARG(int,anchorIndex),Q_ARG(qreal,1),Q_ARG(qreal,list->property("contentY").toDouble()+1)));QVERIFY(anchorIndex>=2);const auto anchorKey=m.state().value("reviewQuestions").toList().at(anchorIndex).toMap().value("key");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"mergeSelectedMatches"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),38);QCOMPARE(m.state().value("balance").toString(),"-1,637.00");
        QTest::qWait(50);int restoredIndex=-1;QVERIFY(QMetaObject::invokeMethod(list,"indexAt",Q_RETURN_ARG(int,restoredIndex),Q_ARG(qreal,1),Q_ARG(qreal,list->property("contentY").toDouble()+1)));QVERIFY(restoredIndex>=0);QCOMPARE(m.state().value("reviewQuestions").toList().at(restoredIndex).toMap().value("key"),anchorKey);
        QTRY_COMPARE(status->mapToScene(QPointF()),statusPosition);QCOMPARE(list->mapToScene(QPointF()),listPosition);QCOMPARE(list->height(),listHeight);
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS")){const auto observed=diagnostics::Collector::instance().snapshot();QCOMPARE(observed.latest.action,diagnostics::Action::review_batch);QCOMPARE(observed.latest.persistence,diagnostics::Persistence::confirmed_saved);qInfo()<<"Batch UI worker_us"<<observed.latest.ended-observed.latest.started<<"delivery_us"<<observed.latest.delivered-observed.latest.ended;}
        run(m,"review_undo");QCOMPARE(m.state().value("reviewCount").toInt(),40);QCOMPARE(m.state().value("balance").toString(),"-1,640.00");
        select(0);QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>("reviewSheet"),"close"));QCOMPARE(window->property("reviewBatch").value<QJSValue>().toVariant().toMap().size(),0);
        run(m,"review_batch",{{"groups",QVariantList{}}},false);QVERIFY(!m.error().isEmpty());QCOMPARE(status->mapToScene(QPointF()),statusPosition);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"rulesNavigation"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(status->isVisible());QCOMPARE(status->mapToScene(QPointF()),statusPosition);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));select(0);run(m,"commit",{{"ignoreWarnings",true}});QCOMPARE(window->property("reviewBatch").value<QJSValue>().toVariant().toMap().size(),0);
        run(m,"close");
    }
    void unifiedReviewSheet() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPFirst\n^\n")}});run(m,"commit");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPSecond\n^\n")}});
        QCOMPARE(window->property("currentPage").toString(),"ledger");
        QCOMPARE(m.state().value("reviewCount").toInt(),1);const auto question=m.state().value("reviewQuestions").toList().at(0).toMap();const auto rows=question.value("rows").toList();QCOMPARE(rows.size(),2);
        auto* button=visualItem(window->contentItem(),"reviewNavigation");QVERIFY(button && button->isVisible());QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto* dialog=window->findChild<QObject*>("reviewSheet");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_REVIEW_WORKSPACE_CAPTURE")){QTest::qWait(150);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REVIEW_WORKSPACE_CAPTURE")));}
        QVERIFY(QMetaObject::invokeMethod(window,"inspectReviewTransaction",Q_ARG(QVariant,rows.last())));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QCOMPARE(window->property("currentPage").toString(),"review");
        QVERIFY(visualItem(window->contentItem(),"transactionEditor")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
        const auto row=rows.last().toMap();run(m,"inspect",{{"batch",row.value("batch")},{"ordinal",row.value("ordinal")}});QCOMPARE(m.state().value("reviewCount").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"commitImport"),"clicked"));QTRY_VERIFY(dialog->property("visible").toBool());QCOMPARE(m.state().value("reviewCount").toInt(),1);
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewYes"));QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"-10.00");
        QVERIFY(visualItem(window->contentItem(),"reviewUndo")->isEnabled());QTest::keySequence(window,QKeySequence::Undo);QTRY_COMPARE(m.state().value("reviewCount").toInt(),1);
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewNo"));QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNo"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"-20.00");
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});QCOMPARE(m.state().value("reviewCount").toInt(),0);
    }
    void linkExclusions() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPTax\n^\n")}});
        run(m,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-03\nT10\nPCRA\n^\n")}});QCOMPARE(m.state().value("reviewCount").toInt(),1);
        const QVariantMap rule{{"identity","test-exclusion"},{"field","payee"},{"expression","cra"},{"account","0"},{"regex",false}};
        run(m,"link_exclusion_preview",rule);QCOMPARE(m.state().value("linkExclusionPreview").toMap().value("comparisons").toList().size(),1);QCOMPARE(m.state().value("reviewCount").toInt(),1);
        run(m,"link_exclusion_save");QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.state().value("linkExclusions").toList().size(),1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"rulesNavigation"),"clicked"));QTRY_COMPARE(window->property("currentPage").toString(),"rules");QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        auto removed=rule;removed["removed"]=true;run(m,"link_exclusion_preview",removed);QCOMPARE(m.state().value("linkExclusionPreview").toMap().value("comparisons").toList().size(),1);run(m,"link_exclusion_save");QCOMPARE(m.state().value("reviewCount").toInt(),1);
        run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});QCOMPARE(m.state().value("reviewCount").toInt(),1);
    }
    void reviewMultipleLinkCandidates_data() {
        QTest::addColumn<QString>("firstDate");
        QTest::addColumn<QString>("secondDate");
        QTest::addColumn<bool>("ambiguousSplits");
        QTest::addColumn<bool>("preselected");
        QTest::newRow("unique-date-and-split")<<"2026-02-01"<<"2026-02-02"<<false<<true;
        QTest::newRow("no-exact-date")<<"2026-02-02"<<"2026-02-03"<<false<<false;
        QTest::newRow("tied-date")<<"2026-02-01"<<"2026-02-01"<<false<<false;
        QTest::newRow("ambiguous-split")<<"2026-02-01"<<"2026-02-02"<<true<<true;
    }
    void reviewMultipleLinkCandidates() {
        QFETCH(QString,firstDate);QFETCH(QString,secondDate);QFETCH(bool,ambiguousSplits);QFETCH(bool,preselected);
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"import",{{"path",fixture(dir.path(),("D"+firstDate+"\nT9\nPFirst\nS\n$-1\nS\n$10\n^\nD"+secondDate+(ambiguousSplits?"\nT8\nPSecond\nS\n$10\nS\n$10\nS\n$-12\n^\n":"\nT8\nPSecond\nS\n$10\nS\n$-2\n^\n")+"D2026-03-01\nT30\nPAnother incoming\n^\n").toUtf8())}});
        run(m,"commit");run(m,"select_account",{{"id","1"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPOutgoing\n^\nD2026-03-01\nT-30\nPAnother transfer\n^\n")}});
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_2"));
        QCOMPARE(visualItem(window->contentItem(),"reviewLinkCandidate_1")->property("checked").toBool(),preselected);
        QVERIFY(!visualItem(window->contentItem(),"reviewLinkCandidate_2")->property("checked").toBool());
        QCOMPARE(visualItem(window->contentItem(),"reviewYes")->isEnabled(),preselected);
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewSplit_1"));
        if(preselected)QCOMPARE(visualItem(window->contentItem(),"reviewSplit_1")->property("currentIndex").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewLinkCandidate_2"),"clicked"));
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewYes")->isEnabled());
        QVERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_2")->property("checked").toBool());
        QVERIFY(!visualItem(window->contentItem(),"reviewLinkCandidate_1")->property("checked").toBool());
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewSplit_2")->isVisible());
        QCOMPARE(visualItem(window->contentItem(),"reviewSplit_2")->property("currentIndex").toInt(),ambiguousSplits?-1:0);
        if(ambiguousSplits){QCOMPARE(visualItem(window->contentItem(),"reviewSplit_2")->property("displayText").toString(),QString("Select split"));return;}
        QVERIFY(!visualItem(window->contentItem(),"reviewSplit_1")->isVisible());
        auto* split=visualItem(window->contentItem(),"reviewSplit_2");
        QVERIFY(QMetaObject::invokeMethod(split,"activated",Q_ARG(int,1)));
        QCOMPARE(split->property("currentIndex").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewLinkCandidate_1"),"clicked"));
        QCOMPARE(visualItem(window->contentItem(),"reviewSplit_1")->property("currentIndex").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewLinkCandidate_2"),"clicked"));
        QCOMPARE(split->property("currentIndex").toInt(),0);
        if(qEnvironmentVariableIsSet("SPENTIT_REVIEW_SELECTION_CAPTURE")){QTest::qWait(150);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REVIEW_SELECTION_CAPTURE")));}
        const auto linkedRows=m.state().value("reviewQuestions").toList().first().toMap().value("rows").toList();
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewYes"),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        // Both editor endpoints use the actual relationship, including the accepted
        // imported split whose stored category is still empty.
        for(const int index:{0,2}) {
            const auto row=linkedRows[index].toMap();
            run(m,"select_account",{{"id",row.value("account")}});
            run(m,"inspect",{{"batch",row.value("batch")},{"ordinal",row.value("ordinal")}});
            const auto splits=selected(m).value("draft").toMap().value("splits").toList();
            const auto transfer=splits.first().toMap();
            QCOMPARE(transfer.value("categoryDisplay").toString(),index==0?QString("Savings [Account]"):QString("Synthetic test [Account]"));
            QVERIFY(transfer.value("transferAmount").toBool());
            QCOMPARE(transfer.value("category").toString(),row.value("splits").toList().first().toMap().value("category").toString());
            if(index==2){
                QVERIFY(QMetaObject::invokeMethod(window,"navigate",Q_ARG(QVariant,QVariant("ledger"))));
                QTRY_VERIFY(visualItem(window->contentItem(),"splitCategory_0"));
                QCOMPARE(visualItem(window->contentItem(),"splitCategory_0")->property("text").toString(),QString("Synthetic test [Account]"));
                QVERIFY(!visualItem(window->contentItem(),"splitRemainder_0")->isVisible());
                if(qEnvironmentVariableIsSet("SPENTIT_TRANSFER_EDITOR_CAPTURE")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_TRANSFER_EDITOR_CAPTURE")));}
            }
        }
    }
    void unifiedReviewLinkDifference() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        run(m,"create",{{"path",dir.path()+"/doc"},{"protected",false},{"name","Checking"},{"currency","CAD"},{"defaultCategories",true}});run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10.40\nPTransfer\nL[Wise]\n^\n")}});
        run(m,"create_account",{{"id","2"},{"name","Wise"},{"currency","CAD"},{"date","2026-01-01"}});run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT10.00\nPTransfer\n^\n")}});
        QCOMPARE(m.state().value("reviewCount").toInt(),1);QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_1"));QTRY_VERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_1")->property("checked").toBool());QTRY_VERIFY(visualItem(window->contentItem(),"reviewYes")->isEnabled());QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        auto* popup=window->findChild<QObject*>("reviewDifference");QVERIFY(popup);QTRY_VERIFY(popup->property("visible").toBool());QCOMPARE(m.state().value("reviewLinkPreview").toMap().value("common").toString(),"10.00");QCOMPARE(m.state().value("reviewLinkPreview").toMap().value("remainder").toString(),"0.40");
        if(qEnvironmentVariableIsSet("SPENTIT_REVIEW_CAPTURE")){QTest::qWait(150);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REVIEW_CAPTURE")));}
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewDifferenceNo"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(!popup->property("visible").toBool());QCOMPARE(m.state().value("reviewCount").toInt(),1);
        QTRY_VERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_1"));QTRY_VERIFY(visualItem(window->contentItem(),"reviewLinkCandidate_1")->property("checked").toBool());QTRY_VERIFY(visualItem(window->contentItem(),"reviewYes")->isEnabled());QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QTRY_VERIFY(popup->property("visible").toBool());
        auto* category=visualItem(window->contentItem(),"reviewRemainderCategory");QVERIFY(category);QVERIFY(category->property("count").toInt()>1);category->setProperty("currentIndex",1);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewDifferenceYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"10.00");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewUndo"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("reviewCount").toInt(),1);
    }
    void undoSelectedImport() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        auto* last=window->findChild<QObject*>("undoLastImport");QVERIFY(last);QVERIFY(!last->property("enabled").toBool());
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT1\nPFirst\n^\n")}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-02\nT2\nPWrong account\n^\n")}});
        QCOMPARE(m.state().value("undoableImports").toList().size(),2);QVERIFY(last->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(last,"triggered"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        auto* confirm=window->findChild<QObject*>("importUndoConfirm");QVERIFY(confirm);QTRY_VERIFY(confirm->property("visible").toBool());
        QCOMPARE(m.state().value("importUndoPreview").toMap().value("transactions").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(confirm,"close"));QCOMPARE(m.state().value("balance").toString(),"3.00");
        QVERIFY(QMetaObject::invokeMethod(last,"triggered"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QTRY_VERIFY(confirm->property("visible").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_UNDO_CAPTURE")){QTest::qWait(150);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_UNDO_CAPTURE")));}
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>("confirmImportUndo"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),"1.00");QCOMPARE(m.state().value("undoableImports").toList().size(),1);
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-03\nT3\nPThird\n^\n")}});
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>("chooseImportUndo"),"clicked"));QTRY_VERIFY(window->findChild<QObject*>("importUndoPicker")->property("visible").toBool());
        QCOMPARE(window->findChild<QObject*>("importUndoList")->property("count").toInt(),2);
        QTRY_VERIFY(visualItem(window->contentItem(),"importUndoChoice_1"));
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"importUndoChoice_1"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QTRY_VERIFY(confirm->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>("confirmImportUndo"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),"3.00");
        run(m,"commit");QVERIFY(!last->property("enabled").toBool());QVERIFY(m.state().value("undoableImports").toList().isEmpty());
    }
    void importUndoOutcomes() {
        for(int mode=0;mode<4;++mode){
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* fault=gateway.get();ImportModel m(std::move(gateway));create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
            run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT1\nPFirst\n^\n")}});
            const auto source=m.state().value("undoableImports").toList().first().toMap().value("source");run(m,"preview_import_undo",{{"source",source}});
            const auto preview=m.state().value("importUndoPreview").toMap();
            fault->groupFaultAction="undo_import";fault->fault=mode==0?OutcomeGateway::before:mode==2?OutcomeGateway::unknown:OutcomeGateway::after;fault->failReviewRefresh=mode==3;
            run(m,"undo_import",{{"source",source},{"token",preview.value("token")}},mode==1);
            if(mode==0){QVERIFY(m.state().value("active").toBool());QCOMPARE(m.state().value("balance").toString(),"1.00");}
            if(mode==1){QCOMPARE(m.state().value("balance").toString(),"0.00");QVERIFY(m.state().value("undoableImports").toList().isEmpty());}
            if(mode>=2)QVERIFY(!m.state().value("active").toBool());
            if(mode==3)QVERIFY(m.error().contains("undone"));
        }
    }
    void datasetAcceptanceAndRollback() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT1\nPFirst\n^\nD2026-02-02\nTbad\nPFailure\n^\n")}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-03\nT2\nPSecond\n^\n")}});
        QCOMPARE(m.state().value("newTransactions").toInt(),2);QCOMPARE(m.state().value("importFailures").toInt(),1);
        run(m,"create_account",{{"id","2"},{"name","Other"},{"currency","USD"},{"date","2026-01-01"}});run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-04\nT3\nPThird\n^\n")}});
        auto* commit=visualItem(window->contentItem(),"commitImport");QVERIFY(commit && commit->property("enabled").toBool());QVERIFY(QMetaObject::invokeMethod(commit,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QTRY_VERIFY(window->findChild<QObject*>("reviewSheet")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewIgnoreCommit"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("newTransactions").toInt(),0);QCOMPARE(m.state().value("importFailures").toInt(),1);QCOMPARE(m.state().value("balance").toString(),"3.00");
        run(m,"select_account",{{"id","1"}});QCOMPARE(m.state().value("balance").toString(),"3.00");
        run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});QCOMPARE(m.state().value("balance").toString(),"3.00");QCOMPARE(m.state().value("importFailures").toInt(),0);
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("newTransactions").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"3.00");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-05\nT4\nPNew\n^\n")}});QCOMPARE(m.state().value("newTransactions").toInt(),1);
        window->close();QTRY_VERIFY(window->findChild<QObject*>("quitCommitDialog")->property("visible").toBool());
        auto* cancel=visualItem(window->contentItem(),"quitCommitCancel");QVERIFY(cancel);QVERIFY(QMetaObject::invokeMethod(cancel,"clicked"));QVERIFY(m.state().value("active").toBool());
        window->close();QTRY_VERIFY(window->findChild<QObject*>("quitCommitDialog")->property("visible").toBool());auto* no=visualItem(window->contentItem(),"quitCommitNo");QVERIFY(no);QVERIFY(QMetaObject::invokeMethod(no,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy() && !m.state().value("active").toBool(),30000);
        ImportModel reopened(storage::document_gateway());run(reopened,"open",{{"path",path}});QCOMPARE(reopened.state().value("newTransactions").toInt(),1);QCOMPARE(reopened.state().value("balance").toString(),"7.00");
    }
    void warningAndQuitAcceptance() {
        QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* injected=gateway.get();ImportModel m(std::move(gateway));
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nTbad\nPRepair\n^\n")}});
        auto edit=selected(m).value("draft").toMap();edit["total"]="1.00";auto splits=edit.value("splits").toList();auto split=splits[0].toMap();split["amount"]="1.00";splits[0]=split;edit["splits"]=splits;run(m,"save",edit);
        QVERIFY(m.state().value("canIgnoreWarnings").toBool());QVERIFY(!m.state().value("acceptanceWarnings").toList().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"commitImport"),"clicked"));QTRY_VERIFY(window->findChild<QObject*>("reviewSheet")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewIgnoreCommit"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("newTransactions").toInt(),0);QCOMPARE(m.state().value("balance").toString(),"1.00");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-02\nT2\nPNext\n^\n")}});injected->fault=OutcomeGateway::before;
        window->close();QTRY_VERIFY(window->findChild<QObject*>("quitCommitDialog")->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"quitCommitYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(!m.error().isEmpty());QVERIFY(m.state().value("active").toBool());QCOMPARE(m.state().value("newTransactions").toInt(),1);QVERIFY(window->isVisible());
        injected->fault=OutcomeGateway::none;window->close();QTRY_VERIFY(window->findChild<QObject*>("quitCommitDialog")->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"quitCommitYes"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy() && !m.state().value("active").toBool(),30000);
        ImportModel reopened(storage::document_gateway());run(reopened,"open",{{"path",dir.path()+"/doc"}});QCOMPARE(reopened.state().value("newTransactions").toInt(),0);QCOMPARE(reopened.state().value("balance").toString(),"3.00");
    }
    void automaticImportDates() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        create(m,dir.path()+"/doc");
        auto* button=engine.rootObjects().first()->findChild<QObject*>("importStatement");
        QVERIFY(button);QVERIFY(button->property("enabled").toBool());
        const auto source=fixture(dir.path(),"D08/10/2026\nT1\n^\n");
        run(m,"import",{{"path",source}});
        QCOMPARE(m.state().value("importDateQuestion").toMap().value("example").toString(),QString("08/10/2026"));
        auto* dialog=engine.rootObjects().first()->findChild<QObject*>("importDateOrderDialog");
        QVERIFY(dialog);QVERIFY(dialog->property("visible").toBool());
        auto* confirm=engine.rootObjects().first()->findChild<QObject*>("confirmImportDate");
        QVERIFY(confirm);QVERIFY(!confirm->property("enabled").toBool());
        run(m,"cancel_import_date");
        QVERIFY(m.state().value("importDateQuestion").toMap().isEmpty());
        run(m,"import",{{"path",source}});
        fixture(dir.path(),"D12/31/2026\nT99\n^\n");
        run(m,"confirm_import_date",{{"profile","DMY"}});
        QVERIFY(m.state().value("importDateQuestion").toMap().isEmpty());
        QVERIFY(selected(m).value("source").toString().contains("08/10/2026"));
        QCOMPARE(selected(m).value("draft").toMap().value("date").toString(),QString("2026-10-08"));
        run(m,"close");create(m,dir.path()+"/other");
        run(m,"import",{{"path",source}});
        QVERIFY(m.state().value("importDateQuestion").toMap().isEmpty());
        QCOMPARE(selected(m).value("draft").toMap().value("date").toString(),QString("2026-12-31"));
        run(m,"close");create(m,dir.path()+"/iso");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-10-08\nT1\n^\n")}});
        QVERIFY(m.state().value("importDateQuestion").toMap().isEmpty());
        QCOMPARE(selected(m).value("draft").toMap().value("date").toString(),QString("2026-10-08"));
        run(m,"close");create(m,dir.path()+"/ofx");
        const auto ofx=QFileInfo(QString::fromUtf8(SPENTIT_REVIEW_FIXTURE)).absoluteDir().filePath("../statements/bank-sgml.ofx");
        run(m,"import",{{"path",ofx}});
        QVERIFY(m.state().value("importDateQuestion").toMap().isEmpty());
        QVERIFY(!m.state().value("importPlan").toList().isEmpty());
        run(m,"cancel_import_mapping");
        run(m,"close");create(m,dir.path()+"/conflict");
        run(m,"import",{{"path",fixture(dir.path(),"D20/10/2026\nT1\n^\nD10/20/2026\nT1\n^\n")}},false);
        QVERIFY(m.error().contains("conflicting date orders"));
    }
    void transferOutcomeRecovery() {
        for(const auto fault:{OutcomeGateway::before,OutcomeGateway::after,OutcomeGateway::unknown}) {
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));
            const auto path=dir.path()+"/doc";create(m,path);run(m,"create_account",{{"id","2"},{"name","Visa"},{"currency","CAD"},{"date","2026-01-01"}});run(m,"select_account",{{"id","1"}});seam->fault=fault;
            run(m,"transfer_manual",{{"link","lost-ack"},{"sequence",m.state().value("transferSequence")},{"operation","create-transfer"},{"destination","2"},{"date","2026-01-02"},{"amount","-100.00"}},fault==OutcomeGateway::after);
            if(fault==OutcomeGateway::unknown){QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("could not be confirmed"));}else run(m,"close");
            seam->fault=OutcomeGateway::none;seam->attempted=false;run(m,"open",{{"path",path}});QCOMPARE(m.state().value("transfers").toList().size(),fault==OutcomeGateway::before?0:1);
        }
    }
    void qifTransferResolution() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.path()+"/doc";create(m,path);run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Visa"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"select_account",{{"id","1"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2/1/2026\nT-101\nSFee\n$-1\nS[Old Visa export]\n$-100\n^\n")},{"profile","DMY"}});
        QVERIFY(selected(m).value("diagnostic").toString().contains("Choose a destination for this transfer"));
        QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().last().toMap().value("transferDestination").toString(),QString("Old Visa export"));
        run(m,"commit",{},false);QVERIFY(m.error().contains("Resolve every account"));
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto visual=[](auto&& self,QQuickItem* item)->QQuickItem*{if(item->objectName()=="splitCategory_1")return item;for(auto* child:item->childItems())if(auto* found=self(self,child))return found;return nullptr;};
        QTRY_VERIFY(visual(visual,window->contentItem()));
        auto* category=visual(visual,window->contentItem());category->forceActiveFocus();
        category->setProperty("text","Visa");QVERIFY(QMetaObject::invokeMethod(category,"textEdited"));
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("transfers").toList().size(),1);
        const auto source=selected(m);
        const auto sourceBatch=source.value("batch").toString();
        const auto savedSplits=source.value("draft").toMap().value("splits").toList();
        QCOMPARE(savedSplits.last().toMap().value("categoryDisplay").toString(),QString("Visa [Account]"));
        QVERIFY(savedSplits.last().toMap().value("transferAmount").toBool());
        const auto navigation=m.state().value("transferNavigation").toMap().value(sourceBatch+":0").toMap();
        QCOMPARE(navigation.size(),1);
        QVERIFY(!navigation.contains(savedSplits.first().toMap().value("allocationId").toString()));
        const auto transferAllocation=savedSplits.last().toMap().value("allocationId").toString();
        QCOMPARE(navigation.value(transferAllocation).toMap().value("arrow").toString(),QString("←"));
        QVERIFY(QMetaObject::invokeMethod(root,"toggleRecord",Q_ARG(QVariant,sourceBatch),Q_ARG(QVariant,0)));
        QTRY_VERIFY(visualItem(window->contentItem(),"splitToggle_0"));
        const auto rowArrowName="ledgerTransferArrow_"+QString::number(m.occurrenceIndex(sourceBatch,0));
        QTRY_VERIFY(visualItem(window->contentItem(),rowArrowName));
        auto* rowArrow=visualItem(window->contentItem(),rowArrowName);
        QTRY_VERIFY(rowArrow->isVisible());
        QCOMPARE(rowArrow->property("text").toString(),QString("←"));
        const auto destinationValue=rowArrow->property("destination");
        const auto rowDestination=destinationValue.metaType()==QMetaType::fromType<QJSValue>()?destinationValue.value<QJSValue>().toVariant().toMap():destinationValue.toMap();
        QCOMPARE(rowDestination,navigation.value(transferAllocation).toMap());
        QVERIFY(rowDestination.value("description").toString().contains("Visa"));
        auto* toggle=visualItem(window->contentItem(),"splitToggle_0");
        QVERIFY(toggle->mapToScene(QPointF(0,0)).x()>=rowArrow->mapToScene(QPointF(rowArrow->width(),0)).x());
        QCOMPARE(toggle->x(),32.0);
        QTest::mouseMove(window,QPoint(2,2));
        auto* arrowBackground=rowArrow->property("background").value<QObject*>();QVERIFY(arrowBackground);
        QTRY_COMPARE(arrowBackground->property("color").value<QColor>().alpha(),0);
        auto* splitBackground=toggle->property("background").value<QObject*>();QVERIFY(splitBackground);
        QTRY_COMPARE(splitBackground->property("color").value<QColor>().alpha(),0);
        if(qEnvironmentVariableIsSet("SPENTIT_SPLIT_ARROW_CAPTURE")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_SPLIT_ARROW_CAPTURE")));}
        QVERIFY(QMetaObject::invokeMethod(rowArrow,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QCOMPARE(selected(m).value("batch").toString(),rowDestination.value("counterpartBatch").toString());
        run(m,"select_account",{{"id","1"}});
        QTRY_VERIFY(visualItem(window->contentItem(),"splitToggle_0"));
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"splitToggle_0"),"clicked"));
        const auto arrowName="splitTransferArrow_"+transferAllocation;
        QTRY_VERIFY(visualItem(window->contentItem(),arrowName));
        QTRY_VERIFY(visualItem(window->contentItem(),arrowName)->isVisible());
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),arrowName),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QCOMPARE(selected(m).value("batch").toString(),navigation.value(transferAllocation).toMap().value("counterpartBatch").toString());
        QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().first().toMap().value("categoryDisplay").toString(),QString("Synthetic test [Account]"));
        run(m,"select_account",{{"id","2"}});run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"select_account",{{"id","1"}});run(m,"commit",{{"ignoreWarnings",true}});QCOMPARE(m.state().value("committed").toString(),QString("-101.00"));
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("transfers").toList().size(),1);
        run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("projected").toString(),QString("100.00"));QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
    }
    void transferWorkflow() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","200.00"}});
        run(m,"create_account",{{"id","2"},{"name","Visa"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"select_account",{{"id","1"}});
        const auto transfer=[&](QString action,QVariantMap args={},bool success=true){args["link"]="test-link";args["sequence"]=m.state().value("transferSequence");args["operation"]=m.newIdentity();run(m,action,args,success);};
        transfer("transfer_manual",{{"destination","2"},{"date","2026-01-02"},{"amount","-(1200/3)/4"},{"payee","Payment"}});
        QCOMPARE(m.state().value("committed").toString(),QString("200.00"));QCOMPARE(m.state().value("projected").toString(),QString("100.00"));
        transfer("transfer_commit",{},false);transfer("transfer_accept");transfer("transfer_commit");
        run(m,"query",{{"filter","Payment"},{"projected",true}});
        const auto link=m.state().value("transfers").toList().first().toMap();
        QVERIFY(QMetaObject::invokeMethod(root,"showCounterpart",Q_ARG(QVariant,link)));QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));QVERIFY(selected(m).value("localTransfer").toBool());
        QCOMPARE(m.state().value("committed").toString(),QString("0.00"));QVERIFY(m.state().value("transfers").toList().first().toMap().value("protected").toBool());
        transfer("transfer_accept");transfer("transfer_commit");
        const auto source=fixture(dir.path(),"D3/1/2026\nT100\nPBank payment\nL[Checking]\n^\n");
        run(m,"import",{{"path",source},{"profile","DMY"}});const auto batch=selected(m).value("batch");
        run(m,"transfer_candidates",{{"link","test-link"}});QCOMPARE(m.state().value("transferCandidates").toList().size(),1);
        transfer("transfer_match",{{"batch",batch},{"ordinal",0},{"allocation","source-0"}});
        QCOMPARE(m.state().value("projected").toString(),QString("100.00"));QVERIFY(m.state().value("transfers").toList().first().toMap().value("evidenced").toBool());
        run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});QVERIFY(!m.state().value("transfers").toList().first().toMap().value("proposed").toBool());
        run(m,"import",{{"path",source},{"profile","DMY"}});
        transfer("transfer_match",{{"allocation","source-0"}});run(m,"commit");
        QCOMPARE(m.state().value("committed").toString(),QString("100.00"));QCOMPARE(m.state().value("newTransactions").toInt(),0);
        run(m,"select_account",{{"id","1"}});QCOMPARE(m.state().value("filter").toString(),QString("Payment"));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("transferDialog"),"open"));QTest::qWait(100);
        QVERIFY(root->findChild<QObject*>("transferDialog")->property("visible").toBool());
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);if(qEnvironmentVariableIsSet("SPENTIT_TRANSFERS_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_TRANSFERS_SCREENSHOT")));
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
        QVERIFY(QMetaObject::invokeMethod(root,"mergeSelected"));
        QCOMPARE(root->property("mergeHint").toString(),QString("Select one or two splits, then select Link"));
        run(m,"transfer_manual",{{"link","clear-link"},{"operation","create-clear"},{"sequence",m.state().value("transferSequence")},{"destination","2"},{"date","2026-01-04"},{"amount","-1.00"}});
        run(m,"inspect",{{"batch","~transfer-clear-link-origin"},{"ordinal",0}});
        run(m,"transfer_clear",{{"link","clear-link"},{"operation","clear"},{"sequence",m.state().value("transferSequence")}});
        QVERIFY(m.state().value("active").toBool());QVERIFY(selected(m).isEmpty());QCOMPARE(m.state().value("projected").toString(),QString("100.00"));
    }
    void contextCategoryTransfer() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        create(m,dir.path()+"/doc");
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Wise"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"select_account",{{"id","1"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2/2/2026\nT-100\nPTransfer\n^\nD3/2/2026\nT-7\nPOther\n^\n")},{"profile","DMY"}});
        const auto batch=selected(m).value("batch").toString();
        run(m,"inspect",{{"batch",batch},{"ordinal",1}});
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});
        const auto choices=root->property("ledgerCategoryChoices").value<QJSValue>().toVariant().toList();int wise=-1,current=-1;
        for(int i=0;i<choices.size();++i){const auto c=choices[i].toMap();if(c.value("accountId").toString()=="2")wise=i;if(c.value("accountId").toString()=="1")current=i;}
        QVERIFY(wise>=0);QVERIFY(current>=0);
        auto choose=[&](int index){return QMetaObject::invokeMethod(root,"chooseLedgerCategory",Q_ARG(QVariant,choices[index]));};
        run(m,"select",{{"row",m.occurrenceIndex(batch,1)},{"toggle",true}});
        QVERIFY(choose(wise));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("transfers").toList().size(),2);
        QCOMPARE(m.rowRecord(m.occurrenceIndex(batch,0)).value("category").toString(),QString("Wise [Account]"));
        for(int count=1;count<=2;++count){
            run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});if(count==2)run(m,"select",{{"row",m.occurrenceIndex(batch,1)},{"toggle",true}});
            QVERIFY(choose(0));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
            QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
            QCOMPARE(m.rowRecord(m.occurrenceIndex(batch,0)).value("category").toString(),QString("Uncategorized"));
            run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("balance").toString(),count==1?QString("7.00"):QString("0.00"));
            run(m,"undo_transfer_assignment");QCOMPARE(m.state().value("balance").toString(),QString("107.00"));
            run(m,"select_account",{{"id","1"}});QCOMPARE(m.rowRecord(m.occurrenceIndex(batch,0)).value("category").toString(),QString("Wise [Account]"));
        }
        run(m,"select_account",{{"id","2"}});
        QCOMPARE(m.state().value("balance").toString(),QString("107.00"));
        QVERIFY(m.state().value("canUndoTransferAssignment").toBool());
        auto* undo=root->findChild<QObject*>("undoTransferAssignment");QVERIFY(undo);QVERIFY(undo->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(undo,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),QString("0.00"));
        run(m,"select_account",{{"id","1"}});QCOMPARE(m.state().value("balance").toString(),QString("-107.00"));
        run(m,"import",{{"path",fixture(dir.path(),"D4/2/2026\nT-7\nPSplit\nSFirst\n$-3\nSSecond\n$-4\n^\n")},{"profile","DMY"}});
        const auto splitBatch=selected(m).value("batch").toString();
        run(m,"select",{{"row",m.occurrenceIndex(splitBatch,0)}});
        QVERIFY(choose(wise));
        auto* dialog=root->findChild<QObject*>("bulkCategoryDialog");QVERIFY(dialog);QVERIFY(dialog->property("visible").toBool());
        auto* splitChoice=visualItem(qobject_cast<QQuickWindow*>(root)->contentItem(),"ledgerCategorySplit_1");QVERIFY(splitChoice);
        QVERIFY(QMetaObject::invokeMethod(splitChoice,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("balance").toString(),QString("4.00"));
        run(m,"undo_transfer_assignment");QCOMPARE(m.state().value("balance").toString(),QString("0.00"));
    }
    void transferManagementUi() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* shortcut=root->findChild<QObject*>("mergeShortcut");QVERIFY(shortcut);
        QCOMPARE(shortcut->property("sequence").toString(),QString("Alt+L"));
        QVERIFY(root->findChild<QObject*>("mergeSelection"));
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Visa"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"select_account",{{"id","1"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2/2/2026\nT-100\nPChecking pay\n^\n")},{"profile","DMY"}});
        const auto checkingBatch=selected(m).value("batch").toString();QVERIFY(!checkingBatch.isEmpty());
        const auto chooseImported=[&]{
            run(m,"select",{{"row",m.occurrenceIndex(selected(m).value("batch").toString(),selected(m).value("ordinal").toInt())}});
        };
        chooseImported();
        QCOMPARE(m.state().value("selectionCount").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(root,"mergeSelected"));
        QCOMPARE(root->property("mergeHint").toString(),QString("Choose the other split, then select Link again"));
        run(m,"select_account",{{"id","2"}});
        run(m,"import",{{"path",fixture(dir.path(),"D3/2/2026\nT100\nPVisa pay\n^\n")},{"profile","DMY"}});
        chooseImported();
        QVERIFY(QMetaObject::invokeMethod(root,"mergeSelected"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("transfers").toList().size(),1);
        bool merged=false;for(const auto& item:m.state().value("transferJournal").toList())if(item.toMap().value("action").toString()=="Merge")merged=true;
        QVERIFY(merged);
        auto* mergePreview=root->findChild<QObject*>("mergePreview");QVERIFY(mergePreview);QVERIFY(!mergePreview->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("transferDialog"),"open"));
        auto* dialog=root->findChild<QObject*>("transferDialog");QVERIFY(dialog);QVERIFY(dialog->property("visible").toBool());
        const auto repeaterItem=[&](const char* name,int index)->QQuickItem*{
            auto* repeater=root->findChild<QObject*>(name);if(!repeater)return nullptr;QQuickItem* item=nullptr;
            QMetaObject::invokeMethod(repeater,"itemAt",Q_RETURN_ARG(QQuickItem*,item),Q_ARG(int,index));return item;
        };
        QTRY_VERIFY(repeaterItem("transferJournalRepeater",0));
        QVERIFY(repeaterItem("transferJournalRepeater",0)->property("text").toString().contains("Merge"));
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,"ledger")));
        const auto findArrow=[&](const QString& name)->QQuickItem* {
            const auto find=[](auto&& self,QQuickItem* item,const QString& key)->QQuickItem* {
                if(item->objectName()==key)return item;
                for(auto* child:item->childItems())if(auto* found=self(self,child,key))return found;
                return nullptr;
            };
            return find(find,qobject_cast<QQuickWindow*>(root)->contentItem(),name);
        };
        const auto visaBatch=selected(m).value("batch").toString();
        const auto incomingName="ledgerTransferArrow_"+QString::number(m.occurrenceIndex(visaBatch,0));
        QTRY_VERIFY(findArrow(incomingName));
        QTRY_VERIFY(findArrow(incomingName)->isVisible());
        QCOMPARE(findArrow(incomingName)->property("text").toString(),QString("→"));
        if(qEnvironmentVariableIsSet("SPENTIT_TRANSFER_CAPTURE")){QTest::qWait(150);QVERIFY(qobject_cast<QQuickWindow*>(root)->grabWindow().save(qEnvironmentVariable("SPENTIT_TRANSFER_CAPTURE")));}
        QVERIFY(QMetaObject::invokeMethod(findArrow(incomingName),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QCOMPARE(m.state().value("accountId").toString(),QString("1"));
        QCOMPARE(selected(m).value("batch").toString(),checkingBatch);
        QCOMPARE(m.state().value("selectionCount").toInt(),1);
        const auto outgoingName="ledgerTransferArrow_"+QString::number(m.occurrenceIndex(checkingBatch,0));
        QTRY_VERIFY(findArrow(outgoingName));
        QTRY_VERIFY(findArrow(outgoingName)->isVisible());
        QCOMPARE(findArrow(outgoingName)->property("text").toString(),QString("←"));
        QVERIFY(QMetaObject::invokeMethod(findArrow(outgoingName),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QCOMPARE(selected(m).value("batch").toString(),visaBatch);
        QCOMPARE(m.state().value("selectionCount").toInt(),1);
        run(m,"select_account",{{"id","1"}});run(m,"commit");
        run(m,"inspect",{{"batch",checkingBatch},{"ordinal",0}});
        QVERIFY(selected(m).value("committed").toBool());QVERIFY(selected(m).value("canCorrect").toBool());QVERIFY(!selected(m).value("editable").toBool());
        auto dated=selected(m).value("draft").toMap();dated["kind"]="correct";dated["date"]="2026-11-20";
        run(m,"preview_change",dated);
        QVERIFY(m.state().value("changePreview").toMap().value("balanceImpact").toString().contains("remains"));
        QCOMPARE(m.state().value("committed").toString(),QString("-100.00"));
        auto* changeDialog=root->findChild<QObject*>("committedChangePreview");QVERIFY(changeDialog);QTRY_VERIFY(changeDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("cancelCommittedChange"),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QTRY_VERIFY(!changeDialog->property("visible").toBool());
        run(m,"import",{{"path",fixture(dir.path(),"D5/2/2026\nT-7\nPUnlinked\n^\n")},{"profile","DMY"}});
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto visual=[](auto&& self,QQuickItem* item,const QString& name)->QQuickItem*{if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=self(self,child,name))return found;return nullptr;};
        QTRY_VERIFY(visual(visual,window->contentItem(),"splitCategory_0"));
        auto* category=visual(visual,window->contentItem(),"splitCategory_0");category->forceActiveFocus();
        category->setProperty("text","Visa");QVERIFY(QMetaObject::invokeMethod(category,"textEdited"));
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        int generated=0,named=0;
        for(const auto& item:m.state().value("transfers").toList()){
            const auto batch=item.toMap().value("counterpartBatch").toString();
            if(batch.startsWith("~transfer-"))++generated;else ++named;
        }
        QCOMPARE(generated,1);QCOMPARE(named,1);
        bool retargeted=false;for(const auto& item:m.state().value("transferJournal").toList())if(item.toMap().value("action").toString()=="Change destination")retargeted=true;
        QVERIFY(retargeted);
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("transferDialog"),"open"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        bool listed=false;const auto journal=root->findChild<QObject*>("transferJournalRepeater");QVERIFY(journal);
        for(int i=0;i<journal->property("count").toInt();++i)if(auto* item=repeaterItem("transferJournalRepeater",i))listed=listed||item->property("text").toString().contains("Change destination");
        QVERIFY(listed);
    }
    void committedMergePreview() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.path()+"/doc";
        run(m,"create",{{"path",path},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"create_account",{{"id","2"},{"name","Visa"},{"currency","CAD"},{"date","2026-01-01"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});run(m,"select_account",{{"id","1"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2/2/2026\nT-100\nPChecking pay\n^\n")},{"profile","DMY"}});
        run(m,"transfer_from_draft",{{"operation","retire-link-op"},{"sequence",m.state().value("transferSequence")},{"link","retire-link"},{"destination","2"},{"allocation","source-0"}});
        run(m,"select_account",{{"id","2"}});
        run(m,"transfer_accept",{{"operation","accept-visa"},{"sequence",m.state().value("transferSequence")},{"link","retire-link"}});
        run(m,"transfer_commit",{{"operation","commit-visa"},{"sequence",m.state().value("transferSequence")},{"link","retire-link"}});
        QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
        run(m,"import",{{"path",fixture(dir.path(),"D3/2/2026\nT100\nPVisa pay\n^\n")},{"profile","DMY"}});
        const auto visa=selected(m).value("batch").toString();QVERIFY(!visa.isEmpty());
        const auto merge=[&](const char* operation){
            run(m,"transfer_merge",{{"operation",operation},{"sequence",m.state().value("transferSequence")},{"link","retire-link"},
                {"leftAccount","2"},{"leftBatch","~transfer-retire-link-destination"},{"leftOrdinal",0},{"leftAllocation","generated"},
                {"rightAccount","2"},{"rightBatch",visa},{"rightOrdinal",0},{"rightAllocation","source-0"}});
        };
        merge("merge-generated");
        auto* dialog=root->findChild<QObject*>("mergePreview");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        const auto note=root->findChild<QObject*>("mergePreviewNote")->property("text").toString();
        QVERIFY(note.contains("Visa"));QVERIFY(note.contains("Checking"));
        QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("cancelMerge"),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(m.state().value("transferMergePreview").toMap().value("id").toString().isEmpty());
        QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
        merge("merge-generated-again");
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("confirmMerge"),"clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(m.state().value("committed").toString(),QString("0.00"));
        run(m,"select_account",{{"id","1"}});QCOMPARE(m.state().value("committed").toString(),QString("0.00"));
    }
    void accountOutcomeRecovery() {
        for(const auto fault:{OutcomeGateway::before,OutcomeGateway::after,OutcomeGateway::unknown}) {
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));
            const auto path=dir.path()+"/doc";create(m,path);seam->fault=fault;
            run(m,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}},fault==OutcomeGateway::after);
            if(fault==OutcomeGateway::unknown){QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("could not be confirmed"));}
            else {QVERIFY(m.state().value("active").toBool());run(m,"close");}
            seam->fault=OutcomeGateway::none;seam->attempted=false;run(m,"open",{{"path",path}});
            QCOMPARE(m.state().value("accounts").toList().size(),fault==OutcomeGateway::before?1:2);
        }
        QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));
        const auto path=dir.path()+"/doc";create(m,path);seam->failAccountRefresh=true;
        run(m,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}},false);
        QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("created"));
        seam->failAccountRefresh=false;run(m,"open",{{"path",path}});QCOMPARE(m.state().value("accounts").toList().size(),2);
        seam->failAccountSelection=true;
        run(m,"create_account",{{"id","3"},{"name","Another"},{"currency","CAD"},{"date","2026-01-01"}},false);
        QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("created"));
        seam->failAccountSelection=false;run(m,"open",{{"path",path}});QCOMPARE(m.state().value("accounts").toList().size(),3);
    }
    void accounts() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        const auto source=fixture(dir.path(),"D1/5/2026\nT-10\nPSynthetic\n^\n");
        run(m,"import",{{"path",source},{"profile","DMY"}});const auto original=selected(m).value("batch").toString();
        run(m,"create_account",{{"id","2"},{"name","Japan"},{"currency","JPY"},{"date","2026-01-01"}});
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));QCOMPARE(m.state().value("accounts").toList().size(),2);
        QCOMPARE(m.state().value("currency").toString(),QString("JPY"));QVERIFY(selected(m).isEmpty());QCOMPARE(m.rowCount(),1);
        run(m,"opening",{{"date","2026-01-01"},{"amount","500"}});run(m,"import",{{"path",source},{"profile","DMY"}});
        QCOMPARE(m.state().value("projected").toString(),QString("490"));
        root->setProperty("dirty",true);
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,QString("1"))));
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardStay"),"clicked"));QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,QString("1"))));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardDiscard"),"clicked"));QTRY_VERIFY(!m.busy());
        QCOMPARE(m.state().value("accountId").toString(),QString("1"));QCOMPARE(m.state().value("projected").toString(),QString("90.00"));
        QCOMPARE(m.state().value("pending").toMap().value("id").toString(),original);QCOMPARE(selected(m).value("batch").toString(),original);
        QVERIFY(QMetaObject::invokeMethod(root,"traverseNavigation",Q_ARG(QVariant,-1)));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QVERIFY(QMetaObject::invokeMethod(root,"traverseNavigation",Q_ARG(QVariant,1)));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("accountId").toString(),QString("1"));
        run(m,"commit");run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("committed").toString(),QString("490"));QCOMPARE(m.state().value("newTransactions").toInt(),0);
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("committed").toString(),QString("90.00"));
        run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("committed").toString(),QString("490"));
        if(qEnvironmentVariableIsSet("SPENTIT_ACCOUNTS_SCREENSHOT")){QTest::qWait(150);auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_ACCOUNTS_SCREENSHOT")));}
        auto* add=root->findChild<QObject*>("newAccount");QVERIFY(add);QVERIFY(QMetaObject::invokeMethod(add,"clicked"));
        auto* name=root->findChild<QObject*>("newAccountName");QVERIFY(name);QTRY_VERIFY(name->property("visible").toBool());name->setProperty("text","Savings");
        QVERIFY(QMetaObject::invokeMethod(name,"accepted"));QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("accounts").toList().size(),3);QCOMPARE(m.state().value("currency").toString(),QString("CAD"));QCOMPARE(m.state().value("opening").toString(),QString("0.00"));QVERIFY(!m.state().value("confirmed").toBool());
    }
    void linkedDeletionDialog() {
        QTemporaryDir dir;const auto path=dir.path()+"/doc";
        {const Currency cad("CAD");auto seed=storage::document_gateway();seed->create(path.toStdString(),true,"test","Checking",cad,"2026-01-01");auto& store=seed->repository();store.create_account(2,"Savings",cad,"2026-01-01");store.ingest("out","!Type:Bank\nD01/01/2026\nT-10\nPPayment\n^\n",qif::DateOrder::dmy);
        const auto row=store.row({"out",0});app::TransferRequest r{};r.action=app::TransferAction::retarget;r.mode=1;r.link="link";r.kept="kept";r.expected_sequence=store.transfer_sequence();r.batch_revision=store.batches().front().revision;
        r.left=transfers::AllocationFact{{1,{"out",0},row.draft->transaction.allocations().front().identity},Money(-1000,cad),row.draft->date,transfers::State::draft,true};r.generated=transfers::Endpoint{2,{"~transfer-link",0},"generated"};r.generated_currency=cad.definition_id();app::OperationContext c;store.assign_transfer_batch("assign",{r},c);store.accept_all("accept",true);}
        ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        run(m,"unlock",{{"path",path},{"password","test"}});run(m,"select",{{"mode","all"}});
        auto* dialog=root->findChild<QObject*>("deleteSelectionDialog");auto* keep=root->findChild<QObject*>("keepDeletionCounterparts");auto* both=root->findChild<QObject*>("removeDeletionCounterparts");auto* confirm=root->findChild<QObject*>("confirmDeleteSelection");QVERIFY(dialog && keep && both && confirm);
        const auto preview=[&]{QVERIFY(QMetaObject::invokeMethod(root,"removeSelected"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QTRY_VERIFY(dialog->property("visible").toBool());QVERIFY(keep->property("checked").toBool());QCOMPARE(m.state().value("deletionPreview").toMap().value("links").toInt(),1);};
        preview();QVERIFY(QMetaObject::invokeMethod(dialog,"close"));QCOMPARE(m.state().value("balance").toString(),QString("-10.00"));
        preview();QVERIFY(QMetaObject::invokeMethod(confirm,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),QString("0.00"));run(m,"undo_edit");QCOMPARE(m.state().value("balance").toString(),QString("-10.00"));
        run(m,"select_account",{{"id","1"}});run(m,"select",{{"mode","all"}});preview();both->setProperty("checked",true);QVERIFY(!keep->property("checked").toBool());QVERIFY(QMetaObject::invokeMethod(confirm,"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("balance").toString(),QString("0.00"));run(m,"undo_edit");QCOMPARE(m.state().value("balance").toString(),QString("-10.00"));run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("balance").toString(),QString("10.00"));run(m,"close");
    }
    void selectionShortcuts() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        const auto keys=[&](bool clear){QSignalSpy completed(&m,&ImportModel::completed);ledger->forceActiveFocus();if(auto* row=visualItem(window->contentItem(),"ledgerRow_0"))row->forceActiveFocus();QTRY_VERIFY(window->activeFocusItem()==ledger || (window->activeFocusItem() && window->activeFocusItem()->property("ledgerSelectionRow").toBool()));QTest::keyClick(window,Qt::Key_A,clear?Qt::ControlModifier|Qt::ShiftModifier:Qt::ControlModifier);QTRY_VERIFY(!completed.empty());QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());};
        keys(false);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),0); // Opening only.
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-1\nPKeep one\n^\nD2/1/2026\nT-2\nPKeep two\n^\nD3/1/2026\nT-3\nPOther\n^\n")},{"profile","DMY"}});
        const auto batch=selected(m).value("batch").toString();
        keys(false);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),3);
        for(const auto& id:m.state().value("selection").toList())QVERIFY(!id.toMap().value("batch").toString().isEmpty());
        auto* search=root->findChild<QQuickItem*>("ledgerSearch");auto* apply=root->findChild<QObject*>("applyFilter");auto* clear=root->findChild<QObject*>("clearFilter");QVERIFY(search && apply && clear);
        search->setProperty("text","Keep");QVERIFY(QMetaObject::invokeMethod(apply,"clicked"));QTRY_VERIFY(!m.busy());
        keys(false);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),2);
        QVERIFY(QMetaObject::invokeMethod(clear,"clicked"));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),2);
        search->setProperty("text","Other");QVERIFY(QMetaObject::invokeMethod(apply,"clicked"));QTRY_VERIFY(!m.busy());
        keys(true);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),0);
        QVERIFY(QMetaObject::invokeMethod(clear,"clicked"));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),0);
        search->setProperty("text","No matches");QVERIFY(QMetaObject::invokeMethod(apply,"clicked"));QTRY_VERIFY(!m.busy());
        keys(false);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),0);
        QVERIFY(QMetaObject::invokeMethod(clear,"clicked"));QTRY_VERIFY(!m.busy());
        run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        payee->setProperty("text","Unsaved name");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));QVERIFY(root->property("dirty").toBool());
        keys(false);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),3);
        keys(true);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),0);
        QCOMPARE(payee->property("text").toString(),"Unsaved name");QVERIFY(root->property("dirty").toBool());QCOMPARE(selected(m).value("ordinal").toInt(),0);
        auto* unsaved=root->findChild<QObject*>("unsavedDialog");QVERIFY(unsaved);QVERIFY(!unsaved->property("visible").toBool());
        payee->forceActiveFocus();QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        QCOMPARE(payee->property("selectedText").toString(),"Unsaved name");QCOMPARE(m.state().value("selectionCount").toInt(),0);
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier|Qt::ShiftModifier);QCOMPARE(m.state().value("selectionCount").toInt(),0);
        search->forceActiveFocus();search->setProperty("text","Search text");QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        QCOMPARE(search->property("selectedText").toString(),"Search text");QCOMPARE(m.state().value("selectionCount").toInt(),0);
        ledger->forceActiveFocus();QVERIFY(QMetaObject::invokeMethod(unsaved,"open"));QTRY_VERIFY(unsaved->property("visible").toBool());
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);QCOMPARE(m.state().value("selectionCount").toInt(),0);
        QVERIFY(QMetaObject::invokeMethod(unsaved,"close"));QTRY_VERIFY(!unsaved->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(root,"discardEditor",Q_ARG(QVariant,QVariant())));
        payee->setProperty("text","Saved name");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY(m.state().value("canUndoEdit").toBool());
        keys(false);keys(true);QVERIFY(m.state().value("canUndoEdit").toBool());run(m,"undo_edit");QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Keep one");run(m,"close");
    }
    void largeSelection() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        create(m,dir.path()+"/doc");QVERIFY(QTest::qWaitForWindowExposed(window));
        QByteArray rows="!Type:Bank\n";
        for(int i=0;i<4100;++i)rows+="D1/1/2026\nT-"+QByteArray::number(i+1)+"\nPRow "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),rows)},{"profile","DMY"}});
        const auto batch=selected(m).value("batch").toString();
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});
        run(m,"select",{{"row",m.occurrenceIndex(batch,4098)},{"range",true}});
        QCOMPARE(m.state().value("selectionCount").toInt(),4099);
        run(m,"select",{{"row",m.occurrenceIndex(batch,4099)},{"toggle",true}});
        QCOMPARE(m.state().value("selectionCount").toInt(),4100);
        const auto ids=m.state().value("selection").toList();
        run(m,"query",{{"filter","Row 4099"},{"projected",true}});
        run(m,"query",{{"filter",""},{"projected",true},{"restoreSelectionIds",ids},{"sort",1},{"descending",true}});
        QCOMPARE(m.state().value("selectionCount").toInt(),4100);
        auto* ledger=window->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);ledger->forceActiveFocus();QTRY_VERIFY(window->activeFocusItem()==ledger || (window->activeFocusItem() && window->activeFocusItem()->property("ledgerSelectionRow").toBool()));
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier|Qt::ShiftModifier);QTRY_COMPARE(m.state().value("selectionCount").toInt(),0);QTRY_VERIFY(!m.busy());
        ledger->forceActiveFocus();QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);QTRY_COMPARE(m.state().value("selectionCount").toInt(),4100);QTRY_VERIFY(!m.busy());
        run(m,"close");
    }
    void bulkInteractions() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);if(!m.state().value("bulkSupported").toBool())QSKIP("Bulk format is not enabled");run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        const auto source=fixture(dir.path(),"D1/5/2026\nT-10.00\nPSplit\nSFood\nERefund\n$2.00\nSTravel\nEFare\n$-12.00\n^\nD2/5/2026\nT20.00\nPIncome\n^\n");
        run(m,"import",{{"path",source},{"profile","DMY"}});const auto batch=m.state().value("batches").toList().front().toMap().value("id").toString();
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});run(m,"preview_bulk",{{"kind","remove"}},false);QVERIFY(m.state().value("active").toBool());
        run(m,"commit");run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});run(m,"select",{{"row",m.occurrenceIndex(batch,1)},{"range",true}});
        QCOMPARE(m.state().value("selectionCount").toInt(),2);
        auto* search=root->findChild<QQuickItem*>("ledgerSearch");QVERIFY(search);
        auto* applyFilter=root->findChild<QObject*>("applyFilter");auto* clearFilter=root->findChild<QObject*>("clearFilter");QVERIFY(applyFilter && clearFilter);
        search->setProperty("text","Income");QVERIFY(QMetaObject::invokeMethod(applyFilter,"clicked"));QTRY_VERIFY(!m.busy());
        QVERIFY(QMetaObject::invokeMethod(clearFilter,"clicked"));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),2);
        search->setProperty("text","Income");QVERIFY(QMetaObject::invokeMethod(applyFilter,"clicked"));QTRY_VERIFY(!m.busy());
        QVERIFY(QMetaObject::invokeMethod(root,"selectLedgerRow",Q_ARG(QVariant,0),Q_ARG(QVariant,0)));QTRY_VERIFY(!m.busy());
        QVERIFY(QMetaObject::invokeMethod(clearFilter,"clicked"));QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),1);QCOMPARE(m.state().value("selection").toList()[0].toMap().value("ordinal").toInt(),1);
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);ledger->forceActiveFocus();QTest::keyClick(window,Qt::Key_End);QTRY_VERIFY(!m.busy());QTest::keyClick(window,Qt::Key_Up,Qt::ShiftModifier);QTRY_VERIFY(!m.busy());QCOMPARE(m.state().value("selectionCount").toInt(),2);
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});run(m,"select",{{"row",m.occurrenceIndex(batch,1)},{"range",true}});
        run(m,"preview_bulk",{{"kind","category"},{"category",""}});QVERIFY(root->findChild<QObject*>("bulkDialog")->property("visible").toBool());
        run(m,"cancel_bulk");QCOMPARE(m.state().value("committed").toString(),"10.00");
        run(m,"preview_bulk",{{"kind","category"},{"category",""}});run(m,"apply_bulk",{{"preview",m.state().value("bulkPreview").toMap().value("id")}});QCOMPARE(m.state().value("committed").toString(),"10.00");
        run(m,"preview_bulk",{{"kind","remove"}});QTest::qWait(100);
        if(qEnvironmentVariableIsSet("SPENTIT_BULK_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_BULK_SCREENSHOT")));
        run(m,"apply_bulk",{{"preview",m.state().value("bulkPreview").toMap().value("id")}});const auto operation=m.state().value("bulkReceipt").toMap().value("operation");
        QCOMPARE(m.state().value("committed").toString(),"0.00");QCOMPARE(m.state().value("selectionCount").toInt(),0);
        run(m,"entry_history",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(m.state().value("entryHistory").toList().front().toMap().value("bulkGroup"),operation);
        run(m,"preview_bulk",{{"kind","undo"},{"operation",operation}});run(m,"apply_bulk",{{"preview",m.state().value("bulkPreview").toMap().value("id")}});
        QCOMPARE(m.state().value("committed").toString(),"10.00");
        run(m,"inspect",{{"batch",batch},{"ordinal",0}});auto allocations=selected(m).value("draft").toMap().value("splits").toList();QCOMPARE(allocations.size(),2);QCOMPARE(allocations[0].toMap().value("amount").toString(),"2.00");QCOMPARE(allocations[1].toMap().value("memo").toString(),"Fare");
        run(m,"close");run(m,"open",{{"path",path}});run(m,"entry_history",{{"batch",batch},{"ordinal",0}});
        QVERIFY(m.state().value("entryHistory").toList().isEmpty());QCOMPARE(m.state().value("committed").toString(),"10.00");
    }
    void combinedSearchAndSelection() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/doc");
        const auto source=fixture(dir.path(),"D1/5/2026\nT-1.00\nPCoffee Shop\nMOriginal memo\n^\nD2/6/2026\nT-2.00\nPCoffee Other\n^\nD1/7/2026\nT3.00\nPIncome\n^\n");
        run(m,"import",{{"path",source},{"profile","DMY"}});
        run(m,"query",{{"filter","period:2026-05-01..2026-06-30 coffee"},{"projected",true}});
        QCOMPARE(m.rowCount(),2);QCOMPARE(m.state().value("withdrawals").toString(),"-3.00");
        run(m,"select",{{"row",0}});run(m,"select",{{"row",1},{"range",true}});QCOMPARE(m.state().value("selectionCount").toInt(),2);
        run(m,"select",{{"row",0},{"toggle",true}});QCOMPARE(m.state().value("selectionCount").toInt(),1);
        run(m,"query",{{"filter","regex:\"(\""},{"projected",true}},false);
        QVERIFY(m.state().value("active").toBool());QCOMPARE(m.rowCount(),2);QCOMPARE(m.state().value("withdrawals").toString(),"-3.00");
        run(m,"query",{{"filter","period:02/03/2026"},{"projected",true}},false);QVERIFY(m.state().value("active").toBool());QCOMPARE(m.rowCount(),2);
        run(m,"query",{{"filter","QIF: \"Original memo\""},{"projected",true}});QCOMPARE(m.rowCount(),1);
        run(m,"query",{{"filter","QIF:"},{"projected",true}});QCOMPARE(m.rowCount(),3);
        bool requested=false;auto connection=connect(&m,&ImportModel::changed,&m,[&]{if(m.canCancel()&&!requested){requested=true;m.requestCancel();}});
        run(m,"query",{{"filter","coffee"},{"projected",true}},false);disconnect(connection);QVERIFY(requested);QVERIFY(m.state().value("active").toBool());QCOMPARE(m.rowCount(),3);
        run(m,"query",{{"projected",true}});QCOMPARE(m.rowCount(),4);
        run(m,"close");create(m,dir.path()+"/second");QCOMPARE(m.state().value("selectionCount").toInt(),0);
    }
    void initTestCase() {
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("ImportUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    }
    void init() {QSettings().clear();QSettings().setValue("datasetPath",settings.path()+"/missing.spentit");QTest::failOnWarning(QRegularExpression(".*DelegateModel::cancel.*"));}
    void windowPositionPreference() {
        ImportModel m(storage::document_gateway());QQuickWindow window;window.resize(400,300);window.show();QTRY_VERIFY(window.isVisible());
        const auto area=window.screen()->availableGeometry();const QPoint first=area.topLeft()+QPoint(40,50);
        window.setFramePosition(first);QTRY_COMPARE(window.framePosition(),first);
        QVERIFY(!m.rememberWindowPosition());QVERIFY(m.saveWindowPosition(&window));QVERIFY(!QSettings().contains("Window/position"));
        m.setRememberWindowPosition(true);QVERIFY(m.saveWindowPosition(&window));QCOMPARE(QSettings().value("Window/position").toPoint(),first);
        // No-op saves must leave the preferences file untouched.
        QSettings preferences;preferences.sync();const auto modified=QFileInfo(preferences.fileName()).lastModified();
        QVERIFY(m.saveWindowPosition(&window));QCOMPARE(QFileInfo(preferences.fileName()).lastModified(),modified);
        const QPoint second=first+QPoint(20,20);window.setFramePosition(second);QTRY_COMPARE(window.framePosition(),second);
        QVERIFY(m.saveWindowPosition(&window));QCOMPARE(QSettings().value("Window/position").toPoint(),second);
        ImportModel reopened(storage::document_gateway());QVERIFY(reopened.rememberWindowPosition());QQuickWindow restored;restored.resize(400,300);reopened.restoreWindowPosition(&restored);restored.show();QTRY_COMPARE(restored.framePosition(),second);
        reopened.setRememberWindowPosition(false);restored.setFramePosition(first);QVERIFY(reopened.saveWindowPosition(&restored));QCOMPARE(QSettings().value("Window/position").toPoint(),second);
        restored.setFramePosition(first);reopened.restoreWindowPosition(&restored);QCOMPARE(restored.framePosition(),first);
        reopened.setRememberWindowPosition(true);QSettings().setValue("Window/position",QPoint(100000,100000));reopened.restoreWindowPosition(&restored);
        QVERIFY(restored.screen()->availableGeometry().contains(restored.framePosition()));
        QSettings().remove("Window");
    }
    void windowGeometryRestoration() {
        ImportModel m(storage::document_gateway());m.setRememberWindowPosition(true);
        QQuickWindow window;window.resize(400,300);m.restoreWindowPosition(&window);QTRY_VERIFY(window.isVisible());
        const auto area=window.screen()->availableGeometry();const QSize tall(420,std::min(600,area.height()-80));
        window.resize(tall);window.setFramePosition(area.topLeft()+QPoint(20,30));QTest::qWait(50);
        QVERIFY(m.saveWindowPosition(&window));QCOMPARE(QSettings().value("Window/size").toSize(),tall);
        QQuickWindow restored;restored.resize(200,200);m.restoreWindowPosition(&restored);QTRY_COMPARE(restored.size(),tall);
        QTRY_COMPARE(restored.framePosition(),window.framePosition());
        restored.showMaximized();QTRY_COMPARE(restored.windowState(),Qt::WindowMaximized);QTest::qWait(50);
        QVERIFY(m.saveWindowPosition(&restored));QCOMPARE(QSettings().value("Window/size").toSize(),tall);
        QQuickWindow maximized;m.restoreWindowPosition(&maximized);QTRY_COMPARE(maximized.windowState(),Qt::WindowMaximized);
        maximized.showNormal();QTRY_COMPARE(maximized.windowState(),Qt::WindowNoState);QTRY_COMPARE(maximized.size(),tall);
        QTest::qWait(50);maximized.showMinimized();QTest::qWait(50);QVERIFY(m.saveWindowPosition(&maximized));
        QQuickWindow visible;m.restoreWindowPosition(&visible);QVERIFY(visible.windowState()!=Qt::WindowMinimized);
        if(QGuiApplication::platformName()=="offscreen") {
            visible.showFullScreen();QTRY_COMPARE(visible.windowState(),Qt::WindowFullScreen);QTest::qWait(50);QVERIFY(m.saveWindowPosition(&visible));
            QQuickWindow fullScreen;m.restoreWindowPosition(&fullScreen);QTRY_COMPARE(fullScreen.windowState(),Qt::WindowFullScreen);
            fullScreen.showNormal();QTRY_COMPARE(fullScreen.size(),tall);
        }
        m.setRememberWindowPosition(false);const auto previous=QSettings().value("Window/size");visible.resize(250,250);QVERIFY(m.saveWindowPosition(&visible));QCOMPARE(QSettings().value("Window/size"),previous);
    }
    void windowPositionSettingsAndQuit() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/doc");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        window->setProperty("currentPage","settings");auto* option=visualItem(window->contentItem(),"rememberWindowPosition");QVERIFY(option);QVERIFY(!option->property("checked").toBool());
        option->setProperty("checked",true);QVERIFY(QMetaObject::invokeMethod(option,"toggled"));QVERIFY(m.rememberWindowPosition());
        const QPoint position=window->screen()->availableGeometry().topLeft()+QPoint(15,20);window->setFramePosition(position);QTRY_COMPARE(window->framePosition(),position);
        QVERIFY(QMetaObject::invokeMethod(window,"beginQuit",Q_ARG(QVariant,QVariant(false))));QCOMPARE(QSettings().value("Window/position").toPoint(),position);QVERIFY(window->isVisible());QTRY_VERIFY(!m.busy() && !m.state().value("active").toBool());
    }
    void cachedStartupTiming() {
        QTemporaryDir dir;const auto path=dir.path()+"/doc";int count=qEnvironmentVariableIntValue("SPENTIT_STARTUP_ROWS");if(count<=0)count=120;
        {ImportModel seed(storage::document_gateway());create(seed,path);run(seed,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        QByteArray records;for(int i=0;i<count;++i)records+="D"+QDate(2026,2,1).addDays(i).toString(Qt::ISODate).toUtf8()+"\nT-1.00\nPSynthetic "+QByteArray::number(i)+"\n^\n";
        run(seed,"import",{{"path",fixture(dir.path(),records)}});run(seed,"commit");run(seed,"close");}
        ImportModel m(storage::document_gateway((dir.path()+"/approval").toStdString()));QElapsedTimer timer;timer.start();
        run(m,"open",{{"path",path}});const auto first=timer.elapsed();QCOMPARE(m.rowCount(),count+1);QCOMPARE(m.state().value("balance").toString(),QString::number(-count)+".00");
        timer.restart();run(m,"close");const auto closing=timer.elapsed();timer.restart();run(m,"open",{{"path",path}});const auto cached=timer.elapsed();
        QCOMPARE(m.rowCount(),count+1);QCOMPARE(m.state().value("balance").toString(),QString::number(-count)+".00");QVERIFY(!m.state().value("reviewReady").toBool());
        timer.restart();run(m,"review_load");const auto review=timer.elapsed();QVERIFY(m.state().value("reviewReady").toBool());QCOMPARE(m.state().value("reviewCount").toInt(),0);
        qInfo()<<"Startup synthetic rows"<<count<<"bytes"<<QFileInfo(path).size()<<"full-open-ms"<<first<<"audit-close-ms"<<closing<<"approved-open-ms"<<cached<<"deferred-review-ms"<<review;
        run(m,"close");
    }
    void visibleShutdownProgress() {
        QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();gateway->delayClose=true;
        ImportModel m(std::move(gateway));create(m,dir.path()+"/doc");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* dialog=window->findChild<QObject*>("shutdownProgressDialog");QVERIFY(dialog);
        QVERIFY(!dialog->property("visible").toBool());
        QSignalSpy completed(&m,&ImportModel::completed);
        window->close();QVERIFY(window->isVisible());QVERIFY(m.busy());
        QVERIFY(dialog->property("visible").toBool());QVERIFY(dialog->property("modal").toBool());
        QCOMPARE(dialog->property("title").toString(),QString("Closing SpentIt…"));
        QCOMPARE(dialog->property("standardButtons").toInt(),0);
        auto* phase=window->findChild<QObject*>("operationPhase");QVERIFY(phase);
        QTRY_COMPARE(phase->property("text").toString(),QString("Checking file"));
        QVERIFY(window->isVisible());QVERIFY(m.busy());
        auto* indicator=window->findChild<QObject*>("activityIndicator");QVERIFY(indicator);
        QVERIFY(indicator->property("running").toBool());QVERIFY(indicator->property("visible").toBool());
        QVERIFY(!window->findChild<QObject*>("importStatement")->property("enabled").toBool());
        QCOMPARE(window->findChild<QObject*>("shutdownProgressMessage")->property("text").toString(),phase->property("text").toString());
        auto* spinner=window->findChild<QQuickItem*>("shutdownActivityIndicator");QVERIFY(spinner);
        QVERIFY(spinner->property("running").toBool());QVERIFY(spinner->isVisible());
        QTest::keyClick(window,Qt::Key_Escape);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,QPoint(8,8));
        QVERIFY(dialog->property("visible").toBool());
        if(QGuiApplication::platformName()!=QStringLiteral("offscreen")) {
            QTest::qWait(100);
            const auto frame=[&]{const auto image=window->grabWindow();const auto scale=image.devicePixelRatio();const auto pos=spinner->mapToScene(QPointF());return image.copy(QRectF(pos.x()*scale,pos.y()*scale,spinner->width()*scale,spinner->height()*scale).toAlignedRect());};
            const auto first=frame();QVERIFY(!first.isNull());QTest::qWait(150);
            QVERIFY(first!=frame());
        }
        window->close();QVERIFY(window->isVisible());QCOMPARE(completed.count(),0);
        QTRY_VERIFY(!m.busy());QTRY_VERIFY(!window->isVisible());QCOMPARE(completed.count(),1);
        QVERIFY(!m.state().value("active").toBool());
    }
    void cachedStartupReviewAndQuit() {
        QTemporaryDir dir;const auto path=dir.path()+"/doc";
        ImportModel m(storage::document_gateway((dir.path()+"/approval").toStdString()));
        create(m,path);run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPA\n^\n")}});run(m,"commit");
        run(m,"import",{{"path",fixture(dir.path(),"D2026-02-01\nT-10\nPB\n^\n")}});
        QCOMPARE(m.state().value("reviewCount").toInt(),1);run(m,"close");run(m,"open",{{"path",path}});
        QVERIFY(!m.state().value("reviewReady").toBool());QCOMPARE(m.state().value("balance").toString(),"-20.00");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QMetaObject::invokeMethod(window,"requestCommit"));QTRY_VERIFY(!m.busy() && m.state().value("reviewReady").toBool());
        QTRY_COMPARE(window->property("currentPage").toString(),QString("review"));QCOMPARE(m.state().value("reviewCount").toInt(),1);QCOMPARE(m.state().value("newTransactions").toInt(),1);
        run(m,"close");run(m,"open",{{"path",path}});QVERIFY(!m.state().value("reviewReady").toBool());
        auto* review=visualItem(window->contentItem(),"reviewNavigation");QVERIFY(review);QVERIFY(QMetaObject::invokeMethod(review,"clicked"));QTRY_VERIFY(!m.busy() && m.state().value("reviewReady").toBool());QCOMPARE(m.state().value("reviewCount").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(window,"beginQuit",Q_ARG(QVariant,QVariant(false))));QVERIFY(window->isVisible());
        QVERIFY(m.busy());QVERIFY(window->findChild<QObject*>("activityIndicator")->property("running").toBool());
        QCOMPARE(window->findChild<QObject*>("operationPhase")->property("text").toString(),QString("Closing file…"));
        window->close();QVERIFY(window->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy() && !m.state().value("active").toBool(),30000);
        QVERIFY(QDir(dir.path()+"/approval").entryList({"*.verified"},QDir::Files).isEmpty());
    }
    void closingApprovalFailure() {
        QTemporaryDir dir;const auto cache=dir.path()+"/blocked";QFile file(cache);QVERIFY(file.open(QIODevice::WriteOnly));file.write("occupied");file.close();
        auto gateway=std::make_unique<OutcomeGateway>();gateway->failClose=true;ImportModel m(std::move(gateway));create(m,dir.path()+"/doc");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QMetaObject::invokeMethod(window,"beginQuit",Q_ARG(QVariant,QVariant(false))));QVERIFY(window->isVisible());
        QVERIFY(m.busy());QVERIFY(window->findChild<QObject*>("activityIndicator")->property("running").toBool());
        QCOMPARE(window->findChild<QObject*>("operationPhase")->property("text").toString(),QString("Closing file…"));
        window->close();QVERIFY(window->isVisible());
        QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());QVERIFY(m.state().value("active").toBool());QVERIFY(window->isVisible());
        QTRY_VERIFY(!window->findChild<QObject*>("shutdownProgressDialog")->property("visible").toBool());
        QVERIFY(window->findChild<QObject*>("closeAuditFailure")->property("visible").toBool());
        auto* quit=window->findChild<QObject*>("closeAuditQuitAnyway");QVERIFY(quit);
        QVERIFY(QMetaObject::invokeMethod(quit,"clicked"));QVERIFY(window->isVisible());QVERIFY(m.busy());
        QVERIFY(window->findChild<QObject*>("shutdownProgressDialog")->property("visible").toBool());
        QTRY_VERIFY(!m.busy() && !m.state().value("active").toBool());QTRY_VERIFY(!window->isVisible());
    }
    void startupDataset_data() {
        QTest::addColumn<QString>("kind");
        for(const auto* kind:{"public","protected","missing","invalid","locked"})QTest::newRow(kind)<<QString(kind);
    }
    void startupDataset() {
        QFETCH(QString,kind);
        QTemporaryDir dir;const auto path=dir.path()+"/startup.spentit";
        ImportModel owner(storage::document_gateway());
        if(kind=="public" || kind=="protected" || kind=="locked") {
            run(owner,"create",{{"path",path},{"name","Startup test"},{"currency","CAD"},{"protected",kind=="protected"},{"password","synthetic"},{"confirmation","synthetic"}});
            if(kind!="locked")run(owner,"close");
        } else if(kind=="invalid") {
            QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("not a dataset");file.close();
        }
        QSettings().setValue("datasetPath",path);
        ImportModel m(storage::document_gateway());
        QSignalSpy startupCommands(&m,&ImportModel::completed);
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* close=root->findChild<QObject*>("fileCloseDataset");QVERIFY(close);
        QTRY_VERIFY(!root->property("startupPending").toBool());QTRY_VERIFY(!m.busy());
        QCOMPARE(startupCommands.size(),1);
        QCOMPARE(startupCommands.front()[0].toString(),QString("open"));
        if(kind!="public") {
            QVERIFY(!m.state().value("active").toBool());QVERIFY(!close->property("enabled").toBool());
            QVERIFY(root->findChild<QObject*>("welcomeScreen")->property("visible").toBool());
            if(kind=="protected") {QVERIFY(m.state().value("unlockRequired").toBool());QVERIFY(m.error().isEmpty());}
            else QVERIFY(!m.error().isEmpty() || m.state().value("unlockRequired").toBool());
            return;
        }
        QVERIFY2(m.state().value("active").toBool(),qPrintable(m.error()));
        QVERIFY(!root->findChild<QObject*>("welcomeScreen")->property("visible").toBool());
        root->setProperty("dirty",true);QVERIFY(QMetaObject::invokeMethod(close,"triggered"));
        QTRY_VERIFY(root->findChild<QObject*>("unsavedDialog")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardStay"),"clicked"));QVERIFY(m.state().value("active").toBool());
        QVERIFY(QMetaObject::invokeMethod(close,"triggered"));
        QSignalSpy lifecycle(&m,&ImportModel::completed);
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardDiscard"),"clicked"));
        QTRY_VERIFY(!m.state().value("active").toBool());
        // Closing schedules a probe. Its retained canOpen flag is not a new
        // completion, and a fixed sleep can click Open while that probe is busy.
        const auto probed=[&]{for(const auto& event:lifecycle)if(event[0].toString()=="probe")return true;return false;};
        QTRY_VERIFY_WITH_TIMEOUT(probed(),30000);
        if(m.diagnosticsEnabled()) {
            const auto observed=diagnostics::Collector::instance().snapshot();
            QCOMPARE(observed.latest.action,diagnostics::Action::probe);
            QVERIFY(observed.latest.delivered!=0);
        }
        QVERIFY(m.state().value("canOpen").toBool());QVERIFY(!m.state().value("active").toBool());
        QVERIFY(root->findChild<QObject*>("welcomeScreen")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("openPublicDataset"),"clicked"));
        QTRY_VERIFY(m.state().value("active").toBool());
    }

    void configuredDatesAndCategories() {
        QTemporaryDir dir;const auto path=dir.path()+"/dates.spentit";
        ImportModel m(storage::document_gateway());
        run(m,"create",{{"path",path},{"name","Dates"},{"currency","CAD"},{"protected",false},{"dateFormat","DD/MM/YYYY"},{"defaultCategories",true}});
        QCOMPARE(m.state().value("dateFormat").toString(),QString("DD/MM/YYYY"));QCOMPARE(m.state().value("categories").toList().size(),46);
        run(m,"opening",{{"date","28/10/2026"},{"amount","0.00"}});
        QCOMPARE(m.state().value("openingDate").toString(),QString("28/10/2026"));
        QFile source(dir.path()+"/dates.qif");QVERIFY(source.open(QIODevice::WriteOnly));source.write("D10/11/2026\nT-1.00\nPSynthetic date\n^\n");source.close();
        run(m,"import",{{"path",source.fileName()},{"profile","MDY"}});
        auto selected=m.state().value("selected").toMap();auto draft=selected.value("draft").toMap();QCOMPARE(draft.value("date").toString(),QString("11/10/2026"));
        draft["date"]="29/02/2026";run(m,"save",draft,false);QVERIFY(!m.error().isEmpty());
        draft["date"]="12/10/2026";run(m,"save",draft);run(m,"commit");
        run(m,"query",{{"filter","period:12/10/2026"},{"projected",false}});QCOMPARE(m.rowCount(),1);QCOMPARE(m.rowRecord(0).value("date").toString(),QString("12/10/2026"));
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("dateFormat").toString(),QString("DD/MM/YYYY"));
        run(m,"close");auto gateway=storage::document_gateway();gateway->open(path.toStdString(),std::nullopt);QCOMPARE(gateway->repository().ledger(false).front().draft.date,std::string("2026-10-12"));
    }
    void welcomeFlow() {
        QTemporaryDir dir;const auto missing=dir.path()+"/missing.spentit";
        QSettings().setValue("datasetPath",missing);
        ImportModel m(storage::document_gateway());
        QCOMPARE(QFileInfo(m.suggestedPath()).fileName(),QString("spentit.spentit"));
        QCOMPARE(m.datasetFilename("Stefan.0001"),QString("Stefan.0001.spentit"));
        QCOMPARE(m.datasetFilename("Stefan.SPENTIT"),QString("Stefan.SPENTIT"));
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        QTRY_VERIFY(m.error().contains("not found"));QVERIFY(!m.state().value("active").toBool());
        auto find=[&](const char* name){return root->findChild<QObject*>(name);};
        auto click=[&](const char* name){return QMetaObject::invokeMethod(find(name),"clicked");};
        QVERIFY(!find("openPublicDataset")->property("visible").toBool());
        QVERIFY(!find("openProtectedDataset")->property("visible").toBool());
        auto* logo=find("welcomeLogo");QVERIFY(logo);
        QTRY_COMPARE(logo->property("status").toInt(),1); // Image.Ready
        // Cocoa supports application-local appearance overrides; offscreen does not.
        if(QGuiApplication::platformName() == "cocoa") {
            auto* hints=QGuiApplication::styleHints();
            hints->setColorScheme(Qt::ColorScheme::Dark);
            QTRY_VERIFY(logo->property("source").toUrl().path().endsWith("SpentIt_dark_256.png"));
            QTRY_COMPARE(logo->property("status").toInt(),1);
            if(qEnvironmentVariableIsSet("SPENTIT_WELCOME_DARK_SCREENSHOT")){QTest::qWait(100);QVERIFY(qobject_cast<QQuickWindow*>(root)->grabWindow().save(qEnvironmentVariable("SPENTIT_WELCOME_DARK_SCREENSHOT")));}
            hints->setColorScheme(Qt::ColorScheme::Light);
            QTRY_VERIFY(logo->property("source").toUrl().path().endsWith("SpentIt_light_256.png"));
            QTRY_COMPARE(logo->property("status").toInt(),1);
            hints->unsetColorScheme();
        }
        if(qEnvironmentVariableIsSet("SPENTIT_WELCOME_SCREENSHOT")){QTest::qWait(100);QVERIFY(qobject_cast<QQuickWindow*>(root)->grabWindow().save(qEnvironmentVariable("SPENTIT_WELCOME_SCREENSHOT")));}
        QVERIFY(click("newDataset"));QTRY_VERIFY(!m.busy());
        QCOMPARE(find("initialAccountName")->property("text").toString(),QString("SpentIt"));
        QCOMPARE(find("setupCurrency")->property("currentText").toString(),QString("CAD"));
        QVERIFY(find("passwordProtection")->property("checked").toBool());
        QVERIFY(find("defaultCategories")->property("checked").toBool());
        QCOMPARE(find("setupDateFormat")->property("currentText").toString(),QString("YYYY-MM-DD"));
        find("setupCurrency")->setProperty("currentIndex",m.currencies().indexOf("USD"));QVERIFY(QMetaObject::invokeMethod(find("setupCurrency"),"activated",Q_ARG(int,m.currencies().indexOf("USD"))));
        QCOMPARE(find("setupDateFormat")->property("currentText").toString(),QString("MM/DD/YYYY"));
        find("setupDateFormat")->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(find("setupDateFormat"),"activated",Q_ARG(int,1)));
        find("setupCurrency")->setProperty("currentIndex",m.currencies().indexOf("CAD"));QVERIFY(QMetaObject::invokeMethod(find("setupCurrency"),"activated",Q_ARG(int,m.currencies().indexOf("CAD"))));
        QCOMPARE(find("setupDateFormat")->property("currentText").toString(),QString("YYYYMMDD"));
        auto* popup=find("setupCurrency")->property("popup").value<QObject*>();QVERIFY(popup);QVERIFY(QMetaObject::invokeMethod(popup,"open"));
        auto* bar=popup->findChild<QQuickItem*>("comboScrollBar");QVERIFY(bar);QTRY_VERIFY(bar->isVisible() && bar->height()>30);
        QVERIFY(bar->property("interactive").toBool());QVERIFY(bar->width()>=16);
        const auto oldPosition=bar->property("position").toDouble();
        const auto thumb=bar->property("visualPosition").toDouble()+bar->property("visualSize").toDouble()/2;
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,bar->mapToScene(QPointF(bar->width()/2,bar->height()*thumb)).toPoint());
        QTest::mouseMove(window,bar->mapToScene(QPointF(bar->width()/2,bar->height()*0.8)).toPoint(),30);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,bar->mapToScene(QPointF(bar->width()/2,bar->height()*0.8)).toPoint());
        QTRY_VERIFY(bar->property("position").toDouble()!=oldPosition);
        if(qEnvironmentVariableIsSet("SPENTIT_SETUP_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_SETUP_SCREENSHOT")));}
        QVERIFY(QMetaObject::invokeMethod(popup,"close"));
        if(qEnvironmentVariableIsSet("SPENTIT_SETUP_SCREENSHOT")){
            auto* flick=find("welcomeScreen")->property("contentItem").value<QObject*>();QVERIFY(flick);
            flick->setProperty("contentY",std::max(0.0,flick->property("contentHeight").toDouble()-flick->property("height").toDouble()));
            QTest::qWait(200);QVERIFY(qobject_cast<QQuickWindow*>(root)->grabWindow().save(qEnvironmentVariable("SPENTIT_SETUP_SCREENSHOT")));
        }
        const auto path=dir.path()+"/created.spentit";find("datasetPath")->setProperty("text",path);
        QVERIFY(click("createDataset"));QTRY_VERIFY(!m.busy());QVERIFY(m.error().contains("Enter a password"));QVERIFY(!QFile::exists(path));
        find("newDatasetPassword")->setProperty("text","synthetic-password");find("confirmDatasetPassword")->setProperty("text","synthetic-password");
        QVERIFY(click("createDataset"));QTRY_VERIFY(!m.busy());QVERIFY2(m.state().value("active").toBool(),qPrintable(m.error()));
        run(m,"close");QTRY_VERIFY(m.state().value("unlockRequired").toBool());
        QVERIFY(m.error().isEmpty());QVERIFY(find("openProtectedDataset")->property("visible").toBool());
        find("unlockPassword")->setProperty("text","wrong");QVERIFY(QMetaObject::invokeMethod(find("unlockPassword"),"accepted"));QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());QVERIFY(!m.state().value("active").toBool());
        find("unlockPassword")->setProperty("text","synthetic-password");QVERIFY(QMetaObject::invokeMethod(find("unlockPassword"),"accepted"));QTRY_VERIFY(!m.busy());QVERIFY(m.state().value("active").toBool());
        run(m,"close");QTRY_VERIFY(m.state().value("unlockRequired").toBool());
        QVERIFY(click("newDataset"));QTRY_VERIFY(!m.busy());find("datasetPath")->setProperty("text",path);find("passwordProtection")->setProperty("checked",false);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto before=file.readAll();file.close();
        QVERIFY(click("createDataset"));QTRY_VERIFY(!m.busy());QVERIFY(m.state().value("overwriteRequired").toBool());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),before);file.close();
        QVERIFY(QMetaObject::invokeMethod(find("overwriteDatasetDialog"),"reject"));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),before);file.close();
        QVERIFY(click("createDataset"));QTRY_VERIFY(!m.busy());QVERIFY(click("confirmOverwriteDataset"));QTRY_VERIFY(!m.busy());
        QVERIFY2(m.state().value("active").toBool(),qPrintable(m.error()));QCOMPARE(m.state().value("name").toString(),QString("SpentIt"));
        run(m,"close");QTRY_VERIFY(m.state().value("canOpen").toBool());QVERIFY(!m.state().value("unlockRequired").toBool());
        QVERIFY(!m.state().value("active").toBool());QVERIFY(click("openPublicDataset"));QTRY_VERIFY(!m.busy());QVERIFY(m.state().value("active").toBool());
    }
    void cancellationBoundaries() {
        QTemporaryDir dir; const auto path=dir.path()+"/doc";
        ImportModel m(storage::document_gateway());create(m,path);run(m,"close");
        const auto startup=QSettings().value("datasetPath");
        bool requested=false;
        auto cancel=connect(&m,&ImportModel::changed,&m,[&]{if(m.canCancel() && !requested){requested=true;m.requestCancel();}});
        run(m,"open",{{"path",path}},false);
        QVERIFY(requested);QVERIFY(m.error().contains("cancelled"));QVERIFY(!m.state().value("active").toBool());
        QCOMPARE(QSettings().value("datasetPath"),startup);
        disconnect(cancel);run(m,"open",{{"path",path}});
        const auto source=fixture(dir.path(),"D1/1/2026\nT1.00\nPSynthetic\n^\n");
        requested=false;
        cancel=connect(&m,&ImportModel::changed,&m,[&]{if(m.canCancel() && !requested){requested=true;m.requestCancel();}});
        run(m,"import",{{"path",source},{"profile","DMY"}},false);
        QVERIFY(requested);QVERIFY(m.state().value("batches").toList().isEmpty());
        disconnect(cancel);run(m,"import",{{"path",source},{"profile","DMY"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        auto edit=selected(m).value("draft").toMap();edit["total"]="2.00";
        auto splits=edit.value("splits").toList();auto allocation=splits[0].toMap();allocation["amount"]="2.00";splits[0]=allocation;edit["splits"]=splits;run(m,"save",edit);
        const auto saved=selected(m);const auto pending=m.state().value("pending");
        requested=false;cancel=connect(&m,&ImportModel::changed,&m,[&]{if(m.canCancel() && !requested){requested=true;m.requestCancel();}});
        run(m,"import",{{"path",source},{"profile","DMY"}},false);disconnect(cancel);
        QCOMPARE(selected(m),saved);QCOMPARE(m.state().value("pending"),pending);QCOMPARE(m.state().value("projected").toString(),"2.00");
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("projected").toString(),"2.00");
        bool sealed=false;
        auto late=connect(&m,&ImportModel::changed,&m,[&]{if(m.busy()){sealed=true;QVERIFY(!m.canCancel());m.requestCancel();QVERIFY(!m.cancellationRequested());}});
        run(m,"commit");disconnect(late);QVERIFY(sealed);QCOMPARE(m.state().value("committed").toString(),"2.00");
    }
    void backupCancellation() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/one");
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        bool requested=false;
        const auto cancel=connect(&m,&ImportModel::changed,&m,[&]{if(m.canCancel() && !requested){requested=true;m.requestCancel();}});
        run(m,"backup",{{"path",dir.path()+"/cancelled"}},false);disconnect(cancel);
        QVERIFY(requested);QVERIFY(m.error().contains("Backup cancelled"));QVERIFY(m.error().contains(".partial"));
        QVERIFY(m.state().value("active").toBool());QCOMPARE(m.state().value("committed").toString(),"100.00");
        QVERIFY(!QFileInfo::exists(dir.path()+"/cancelled"));
        run(m,"backup",{{"path",dir.path()+"/retry"}});run(m,"close");run(m,"open",{{"path",dir.path()+"/retry"}});
        QCOMPARE(m.state().value("committed").toString(),"100.00");
    }
    void cancelledOpenActivation() {
        QTemporaryDir dir;const auto path=dir.path()+"/doc";
        auto gateway=std::make_unique<OutcomeGateway>();auto* injected=gateway.get();
        ImportModel m(std::move(gateway));create(m,path);run(m,"close");
        QSettings().setValue("datasetPath",dir.path()+"/previous");const auto prior=QSettings().value("datasetPath");
        injected->cancelAfterOpen=true;run(m,"open",{{"path",path}},false);
        QVERIFY(!m.state().value("active").toBool());QCOMPARE(QSettings().value("datasetPath"),prior);
        QVERIFY(m.error().contains("cancelled"));
        {auto independent=storage::document_gateway();independent->open(path.toStdString(),std::nullopt);independent->close();}
        injected->cancelAfterOpen=false;run(m,"open",{{"path",path}});QVERIFY(m.state().value("active").toBool());
    }
    void controlledStorageOpen() {
        QTemporaryDir dir;const auto path=(std::filesystem::canonical(dir.path().toStdString())/"doc").string();
        {auto store=storage::ImportStore::create(path,"synthetic","Account",Currency("CAD"),"2026-01-01");
         store.ingest("one","D1/1/2026\nT1.00\nPSynthetic\n^\n",qif::DateOrder::dmy);}
        QFile file(QString::fromStdString(path));QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        for(const auto phase:{"Opening file","Checking file","Checking imported transactions","Verifying category history","Verifying baseline and session changes"}) {
            app::OperationContext context;bool reached=false;
            context.progress=[&](std::string_view value){if(value==phase){reached=true;context.request();}};
            QVERIFY_THROWS_EXCEPTION(app::OperationCancelled,storage::ImportStore::open(path,"synthetic",&context));
            QVERIFY(reached);
            {auto store=storage::ImportStore::open(path,"synthetic");QCOMPARE(store.batches().size(),std::size_t(1));}
            QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        }
    }
    void commitOutcomeRecovery() {
        for(auto fault:{OutcomeGateway::before,OutcomeGateway::after,OutcomeGateway::unknown}) {
            QTemporaryDir dir;const auto path=dir.path()+"/doc";
            auto gateway=std::make_unique<OutcomeGateway>();auto* injected=gateway.get();
            ImportModel m(std::move(gateway));create(m,path);
            const auto source=fixture(dir.path(),"D1/1/2026\nT1.00\nPSynthetic\n^\n");
            run(m,"import",{{"path",source},{"profile","DMY"}});
            run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
            injected->fault=fault;run(m,"commit",{},fault==OutcomeGateway::after);
            if(fault==OutcomeGateway::before) {QVERIFY(m.state().value("active").toBool());QCOMPARE(m.state().value("committed").toString(),"0.00");QVERIFY(!m.state().value("pending").toMap().isEmpty());}
            if(fault==OutcomeGateway::after) {QCOMPARE(m.state().value("newTransactions").toInt(),0);QCOMPARE(m.state().value("committed").toString(),"1.00");}
            if(fault==OutcomeGateway::unknown) {QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("could not confirm"));}
            injected->fault=OutcomeGateway::none;
            if(m.state().value("active").toBool())run(m,"close");
            run(m,"open",{{"path",path}});
            if(fault==OutcomeGateway::before) run(m,"commit");
            QCOMPARE(m.state().value("committed").toString(),"1.00");QCOMPARE(m.rowCount(),2);
            QCOMPARE(m.state().value("newTransactions").toInt(),0);
        }
    }
    void groupOutcomeRecovery() {
        for(const auto& action:{QString("capture"),QString("resume"),QString("balance"),QString("abort")})for(const auto fault:{OutcomeGateway::before,OutcomeGateway::after,OutcomeGateway::unknown}) {
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));const auto path=dir.path()+"/doc";create(m,path);
            const auto file=fixture(dir.path(),"!Account\nNSynthetic source\nTBank\nB10\n^\n!Type:Bank\nD1/1/2026\nT10\n^\n");run(m,"import",{{"path",file},{"profile","DMY"}});
            QVariantMap mapping{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}};QVariantMap args{{"mappings",QVariantList{mapping}}};
            if(action=="balance" || action=="abort")run(m,"map_import",args);
            seam->fault=fault;seam->groupFaultAction=action;QString command="map_import";
            if(action=="balance"){command="group_balance";args={{"group",m.state().value("importGroup").toMap().value("id")},{"source",0},{"revision",1},{"choice",1},{"date","2026-01-01"}};}
            if(action=="abort"){command="group_abort";args={{"group",m.state().value("importGroup").toMap().value("id")}};}
            run(m,command,args,fault==OutcomeGateway::after || (action=="balance" && fault==OutcomeGateway::before));
            if(m.diagnosticsEnabled() && action=="resume" && fault==OutcomeGateway::before) {
                const auto observed=diagnostics::Collector::instance().snapshot();
                QCOMPARE(observed.latest.result,diagnostics::Result::failed);
                QCOMPARE(observed.latest.persistence,diagnostics::Persistence::confirmed_saved);
                QCOMPARE(observed.latest.presentation,diagnostics::Presentation::model_applied);
            }
            if(fault==OutcomeGateway::unknown)QVERIFY(!m.state().value("active").toBool());
            else if(fault==OutcomeGateway::before && action=="capture"){QCOMPARE(m.state().value("accounts").toList().size(),1);seam->fault=OutcomeGateway::none;run(m,"map_import",args);}
            if(m.state().value("active").toBool())run(m,"close");
            seam->fault=OutcomeGateway::none;seam->groupFaultAction.clear();seam->attempted=false;run(m,"open",{{"path",path}});
            QCOMPARE(m.state().value("accounts").toList().size(),((action=="abort" && fault!=OutcomeGateway::before) || (action=="capture" && fault==OutcomeGateway::unknown) || (action=="resume" && fault==OutcomeGateway::before))?1:2);
        }
        for(bool selectionFailure:{false,true}) {
            QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));const auto path=dir.path()+"/doc";create(m,path);
            run(m,"import",{{"path",fixture(dir.path(),"!Account\nNSynthetic source\nTBank\n^\n!Type:Bank\nD1/1/2026\nT1\n^\n")},{"profile","DMY"}});seam->groupFaultAction="capture";seam->failAccountRefresh=!selectionFailure;seam->failAccountSelection=selectionFailure;
            run(m,"map_import",{{"mappings",QVariantList{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}}}},false);QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("saved"));
            seam->attempted=false;seam->failAccountRefresh=false;seam->failAccountSelection=false;run(m,"open",{{"path",path}});QCOMPARE(m.state().value("accounts").toList().size(),selectionFailure?1:2);
        }
    }
    void groupedCancellationAndMappingPersistence() {
        QTemporaryDir dir;auto gateway=std::make_unique<OutcomeGateway>();auto* seam=gateway.get();ImportModel m(std::move(gateway));const auto path=dir.path()+"/document";create(m,path);
        run(m,"create_account",{{"id","2"},{"name","JPY workspace"},{"currency","JPY"},{"date","2026-01-01"}});
        const auto file=fixture(dir.path(),"!Account\nNSynthetic retained\nTBank\n^\n!Type:Bank\nD1/1/2026\nT1\n^\n");run(m,"import",{{"path",file},{"profile","DMY"}});QCOMPARE(m.state().value("importPlan").toList()[0].toMap().value("currency").toString(),"CAD");
        seam->cancelGroupResume=true;run(m,"map_import",{{"mappings",QVariantList{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}}}},false);QVERIFY(m.error().contains("Import cancelled"));QVERIFY(!m.state().value("pending").toMap().value("processingIncomplete").toBool());QCOMPARE(m.state().value("accounts").toList().size(),2);
        seam->cancelGroupResume=false;run(m,"import",{{"path",file},{"profile","DMY"}});run(m,"map_import",{{"mappings",QVariantList{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}}}});QCOMPARE(m.state().value("accounts").toList().size(),3);run(m,"group_abort",{{"group",m.state().value("importGroup").toMap().value("id")}});QCOMPARE(m.state().value("accounts").toList().size(),2);
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QByteArray list;for(int i=0;i<20;++i)list+="!Account\nNSynthetic list "+QByteArray::number(i)+"\nTBank\n^\n";run(m,"import",{{"path",fixture(dir.path(),list)},{"profile","DMY"}});
        QTRY_VERIFY(visualItem(window->contentItem(),"sourceAccountChoice_0"));auto* choice=visualItem(window->contentItem(),"sourceAccountChoice_0");choice->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,1)));
        QTRY_VERIFY(visualItem(window->contentItem(),"sourceCurrency_0"));auto* currency=visualItem(window->contentItem(),"sourceCurrency_0");const int yen=static_cast<int>(m.currencies().indexOf("JPY"));currency->setProperty("currentIndex",yen);QVERIFY(QMetaObject::invokeMethod(currency,"activated",Q_ARG(int,yen)));
        auto* view=visualItem(window->contentItem(),"sourceAccountMappingList");QVERIFY(view);QVERIFY(QMetaObject::invokeMethod(view,"positionViewAtIndex",Q_ARG(int,19),Q_ARG(int,0)));QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(view,"positionViewAtIndex",Q_ARG(int,0),Q_ARG(int,0)));QTRY_VERIFY(visualItem(window->contentItem(),"sourceAccountChoice_0"));QCOMPARE(visualItem(window->contentItem(),"sourceAccountChoice_0")->property("currentIndex").toInt(),1);QCOMPARE(visualItem(window->contentItem(),"sourceCurrency_0")->property("currentText").toString(),"JPY");run(m,"cancel_import_mapping");
    }
    void mappedTransferBlockers() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);create(m,dir.path()+"/doc");
        QByteArray input="!Account\nNA\nTBank\n^\n!Type:Bank\n";
        for(int i=0;i<41;++i)input+="D2026-01-01\nT-1\nPOrdinary\n^\n";
        for(int i=0;i<73;++i)input+="D2026-01-02\nT-10\nL[B]\n^\n";
        input+="!Account\nNB\nTBank\n^\n!Type:Bank\nD2026-01-03\nT10\nL[A]\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),input)},{"profile","DMY"}});
        QVariantList mappings{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}},QVariantMap{{"source",1},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}};run(m,"map_import",{{"mappings",mappings}});
        QCOMPARE(m.state().value("importGroup").toMap().value("unresolvedTransfers").toInt(),0);
        const auto questions=m.state().value("reviewQuestions").toList();int waiting=0;QVariantMap target;
        for(const auto& value:questions){const auto q=value.toMap();if(!q.value("waitingLink").toString().isEmpty()){++waiting;target=q.value("rows").toList().first().toMap();}}
        QCOMPARE(waiting,74);QVERIFY(!m.state().value("acceptanceWarnings").toList().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"reviewNavigation"),"clicked"));
        QTRY_COMPARE(window->property("currentPage").toString(),"review");
        auto* list=visualItem(window->contentItem(),"reviewQuestions");QVERIFY(list);QVERIFY(QMetaObject::invokeMethod(list,"positionViewAtEnd"));QTRY_VERIFY(list->property("contentY").toDouble()>20);
        QVERIFY(QMetaObject::invokeMethod(window,"inspectReviewTransaction",Q_ARG(QVariant,target)));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QCOMPARE(window->property("currentPage").toString(),"review");QVERIFY(visualItem(window->contentItem(),"transactionEditor")->isVisible());
    }

    void mappingOpeningInvalidReport() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/doc");
        run(m,"import",{{"path",fixture(dir.path(),"!Account\nNNew account\nTBank\nBinvalid\n^\n!Type:Bank\nDinvalid\nT10\n^\n")},{"profile","DMY"}});
        const auto plan=m.state().value("importPlan").toList();QCOMPARE(plan[0].toMap().value("openingDate").toString(),"1900-01-01");QVERIFY(plan[0].toMap().value("offerReportedOpening").toBool());
        QVariantMap mapping{{"source",0},{"create",true},{"currency","CAD"},{"date","1900-01-01"},{"useReportedOpening",true}};run(m,"map_import",{{"mappings",QVariantList{mapping}}},false);QCOMPARE(m.state().value("accounts").toList().size(),1);
        mapping["useReportedOpening"]=false;run(m,"map_import",{{"mappings",QVariantList{mapping}}});QCOMPARE(m.state().value("opening").toString(),"0.00");QVERIFY(m.state().value("groupOpeningFixed").toBool());QVERIFY(m.state().value("importGroup").toMap().value("unresolved").toInt()>0);
    }
    void mappingOpeningChoices() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);create(m,dir.path()+"/doc");run(m,"opening",{{"date","2000-01-01"},{"amount","7.00"}});
        const QByteArray input="!Account\nNNew account\nTBank\nB100\n^\n!Type:Bank\nD2010-01-01\nT10\n^\nD2009-01-01\nT20\n^\n!Account\nNEmpty account\nTBank\nB0\n^\n!Type:Bank\n!Account\nNExisting export\nTBank\n^\n!Type:Bank\nD2001-01-01\nT1\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),input)},{"profile","DMY"}});const auto plan=m.state().value("importPlan").toList();QCOMPARE(plan.size(),3);QCOMPARE(plan[0].toMap().value("openingDate").toString(),"2009-01-01");QCOMPARE(plan[1].toMap().value("openingDate").toString(),"1900-01-01");QVERIFY(plan[0].toMap().value("offerReportedOpening").toBool());QVERIFY(!plan[1].toMap().value("offerReportedOpening").toBool());QVERIFY(!plan[2].toMap().value("offerReportedOpening").toBool());
        auto* dialog=window->findChild<QObject*>("importAccountMapping");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());auto mappings=dialog->property("mappings").toList();QCOMPARE(mappings[0].toMap().value("date").toString(),"2009-01-01");QVERIFY(!mappings[0].toMap().value("useReportedOpening").toBool());
        for(int i=0;i<3;++i){auto item=mappings[i].toMap();item["create"]=i<2;item["account"]=i<2?"new":"1";mappings[i]=item;}dialog->setProperty("mappings",mappings);
        auto* balance=visualItem(window->contentItem(),"sourceOpeningBalance_0");QTRY_VERIFY(balance && balance->isVisible());QCOMPARE(balance->property("count").toInt(),2);QCOMPARE(balance->property("currentIndex").toInt(),0);balance->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(balance,"activated",Q_ARG(int,1)));
        if(qEnvironmentVariableIsSet("SPENTIT_MAPPING_CAPTURE")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_MAPPING_CAPTURE")));}
        run(m,"map_import",{{"mappings",dialog->property("mappings")}});QCOMPARE(m.state().value("opening").toString(),"100.00");QCOMPARE(m.state().value("openingDate").toString(),"2009-01-01");QCOMPARE(m.state().value("balance").toString(),"130.00");QVERIFY(!m.state().value("groupCreated").toBool());QVERIFY(!visualItem(window->contentItem(),"openingSetup"));QCOMPARE(m.state().value("importGroup").toMap().value("unresolved").toInt(),0);
        run(m,"select_account",{{"id","3"}});QCOMPARE(m.state().value("opening").toString(),"0.00");QCOMPARE(m.state().value("openingDate").toString(),"1900-01-01");QVERIFY(!m.state().value("groupCreated").toBool());
        run(m,"select_account",{{"id","1"}});QCOMPARE(m.state().value("opening").toString(),"7.00");QCOMPARE(m.state().value("openingDate").toString(),"2000-01-01");run(m,"commit");run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("balance").toString(),"130.00");
    }
    void mappedTransferImport() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);create(m,dir.path()+"/doc");
        run(m,"import",{{"path",fixture(dir.path(),"!Type:Cat\nNA\nE\n^\nNFees\nE\n^\n!Account\nNA\nTBank\n^\n!Type:Bank\nD2026-01-02\nT-10.25\nSB\n$-10\nSFees\n$-0.25\n^\nD2026-01-02\nT-10.25\nSB\n$-10\nSFees\n$-0.25\n^\n!Account\nNB\nTBank\n^\n!Type:Bank\nD2026-01-02\nT10\nL[A]\n^\nD2026-01-02\nT10\nL[A]\n^\n")},{"profile","DMY"}});
        QVariantList mappings{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}},QVariantMap{{"source",1},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}};
        run(m,"map_import",{{"mappings",mappings}});
        QCOMPARE(m.state().value("accountCategories").toList().size(),3);
        auto group=m.state().value("importGroup").toMap();QCOMPARE(group.value("unresolvedTransfers").toInt(),0);QCOMPARE(group.value("unresolved").toInt(),2);QCOMPARE(group.value("blockers").toList().size(),2);QCOMPARE(m.state().value("transfers").toList().size(),2);
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"showImportBlockers"),"clicked"));
        auto* dialog=root->findChild<QObject*>("importBlockersDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
        run(m,"group_resolve_transfers",{{"group",group.value("id")}});QCOMPARE(m.state().value("transfers").toList().size(),2);
        for(int i=0;i<2;++i){run(m,"select_account",{{"id",QString::number(i+2)}});run(m,"group_balance",{{"group",group.value("id")},{"source",i},{"revision",m.state().value("pending").toMap().value("revision")},{"choice",4},{"date","2026-01-01"}});}
        QVERIFY(m.state().value("importGroup").toMap().value("ready").toBool());run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"20.00");
        run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("committed").toString(),"-20.50");QCOMPARE(m.state().value("newTransactions").toInt(),0);
    }
    void multiAccountImport() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);create(m,path);
        const auto file=fixture(dir.path(),"!Type:Cat\nNFood\nE\nDImported source description\n^\nNUnused/Category\nI\n^\n!Account\nNSynthetic wallet\nTCash\nB50\n^\n!Type:Cash\nD2026-01-01\nTbad\nPSynthetic shop\rRepeated text\rTnot-an-amount\nLFood/Class\n^\n!Account\nNSynthetic card\nTCCard\nB-10\n^\n!Type:CCard\nD2026-01-02\nT-10\n^\n");
        run(m,"import",{{"path",file},{"profile","DMY"}});QCOMPARE(m.state().value("importPlan").toList().size(),2);QCOMPARE(m.state().value("accounts").toList().size(),1);
        auto* popup=root->findChild<QObject*>("importAccountMapping");QVERIFY(popup);QTRY_VERIFY(popup->property("visible").toBool());
        QVariantList mappings{QVariantMap{{"source",0},{"create",true},{"currency","CAD"},{"date","2026-01-01"}},QVariantMap{{"source",1},{"create",true},{"currency","CAD"},{"date","2026-01-01"}}};
        for(int i=0;i<2;++i){const auto name="sourceAccountChoice_"+QString::number(i);QTRY_VERIFY(visualItem(window->contentItem(),name));auto* choice=visualItem(window->contentItem(),name);QVERIFY(choice->setProperty("currentIndex",1));QVERIFY(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,1)));}
        if(qEnvironmentVariableIsSet("SPENTIT_GROUP_MAPPING_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_GROUP_MAPPING_SCREENSHOT")));}
        QTRY_VERIFY(visualItem(window->contentItem(),"confirmImportMappings"));QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"confirmImportMappings"),"clicked"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("accounts").toList().size(),3);QVERIFY(m.state().value("importPlan").toList().isEmpty());
        QTRY_VERIFY(visualItem(window->contentItem(),"groupRepairProgress"));QVERIFY(visualItem(window->contentItem(),"groupRepairProgress")->property("text").toString().contains("0 / 1"));QCOMPARE(visualItem(window->contentItem(),"durableDate")->property("text").toString(),QString("2026-01-01"));
        if(qEnvironmentVariableIsSet("SPENTIT_GROUP_REVIEW_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_GROUP_REVIEW_SCREENSHOT")));}
        QCOMPARE(m.state().value("categories").toList().size(),0);
        QCOMPARE(m.state().value("importGroup").toMap().value("categories").toList().size(),2);
        auto* categoryReview=root->findChild<QObject*>("reviewImportedCategories");QVERIFY(categoryReview);QVERIFY(QMetaObject::invokeMethod(categoryReview,"clicked"));
        auto* categoryDialog=root->findChild<QObject*>("importCategoriesDialog");QVERIFY(categoryDialog);QTRY_VERIFY(categoryDialog->property("visible").toBool());
        QTRY_COMPARE(root->findChild<QObject*>("importCategoryList")->property("count").toInt(),2);
        if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_IMPORT_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_IMPORT_SCREENSHOT")));}
        QVERIFY(QMetaObject::invokeMethod(categoryDialog,"close"));
        for(const auto& item:m.state().value("importCategoryDecisions").toList()){const auto d=item.toMap();run(m,"resolve_import_categories",{{"source",d.value("source")},{"selected",d.value("names")}});}
        QCOMPARE(visualItem(window->contentItem(),"durablePayee")->property("text").toString(),QString("Synthetic shop\rRepeated text\rTnot-an-amount"));
        auto group=m.state().value("importGroup").toMap();QCOMPARE(group.value("repairs").toInt(),1);QCOMPARE(group.value("amountRepairs").toInt(),1);QCOMPARE(group.value("done").toInt(),0);
        auto edit=selected(m).value("draft").toMap();edit["total"]="10.00";auto parts=edit.value("splits").toList();auto part=parts[0].toMap();part["amount"]="10.00";parts[0]=part;edit["splits"]=parts;
        run(m,"save",edit);run(m,"decide");QCOMPARE(m.state().value("importGroup").toMap().value("done").toInt(),1);
        QVERIFY(m.state().value("groupOpeningFixed").toBool());QVERIFY(!visualItem(window->contentItem(),"openingSetup"));QCOMPARE(m.state().value("opening").toString(),"0.00");run(m,"commit");
        QCOMPARE(m.state().value("categories").toList().size(),2);QCOMPARE(m.state().value("committed").toString(),"10.00");run(m,"select_account",{{"id","3"}});QCOMPARE(m.state().value("committed").toString(),"-10.00");QCOMPARE(m.state().value("newTransactions").toInt(),0);
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("categories").toList().size(),2);run(m,"select_account",{{"id","2"}});QCOMPARE(m.state().value("committed").toString(),"10.00");
        run(m,"select_account",{{"id","1"}});QFile duplicateFile(file);QVERIFY(duplicateFile.open(QIODevice::ReadOnly));auto duplicateBytes=duplicateFile.readAll();duplicateFile.close();duplicateBytes.replace("Synthetic wallet","Abort wallet");duplicateBytes.replace("Synthetic card","Abort card");run(m,"import",{{"path",fixture(dir.path(),duplicateBytes)},{"profile","DMY"}});run(m,"map_import",{{"mappings",mappings}});group=m.state().value("importGroup").toMap();
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>("discardImport"),"triggered"));auto* discard=root->findChild<QObject*>("discardImportDialog");QVERIFY(discard);QTRY_VERIFY(discard->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(discard,"accepted"));QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(m.state().value("accountId").toString(),"4");QCOMPARE(m.state().value("accounts").toList().size(),5);QCOMPARE(m.state().value("nextAccountId").toString(),"6");
    }
    void unknownAmountRepair() {
        QTemporaryDir dir;const auto path=dir.path()+"/document";ImportModel m(storage::document_gateway());create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        const auto file=fixture(dir.path(),"!Type:Cash\nD1/1/2026\nTbad\nLFood/Family\n^\n");
        run(m,"import",{{"path",file},{"profile","DMY"}});
        QVERIFY(selected(m).value("repair").toBool());QCOMPARE(selected(m).value("original").toString(),"Unavailable");
        QVERIFY(selected(m).value("sourceClasses").toString().contains("Family"));
        const auto sourcePart=selected(m).value("draft").toMap().value("splits").toList()[0].toMap();QCOMPARE(sourcePart.value("categoryDisplay").toString(),"Unmapped: Food");QCOMPARE(sourcePart.value("sourceOrdinal").toInt(),0);QCOMPARE(sourcePart.value("categoryOwner").toString(),"source");
        if(m.state().value("rulesSupported").toBool())run(m,"rules_prefill",{{"kind","memo"},{"allocation",0}});
        auto edit=selected(m).value("draft").toMap();QVERIFY(edit.value("total").toString().isEmpty());edit["total"]="-10.00";
        auto splits=edit.value("splits").toList();QCOMPARE(splits.size(),1);auto allocation=splits[0].toMap();QVERIFY(allocation.value("amount").toString().isEmpty());allocation["amount"]="-10.00";splits[0]=allocation;edit["splits"]=splits;
        run(m,"save",edit);
        QCOMPARE(selected(m).value("draft").toMap().value("splits").toList()[0].toMap().value("categoryOwner").toString(),QString("source"));
        run(m,"commit",{},false);run(m,"decide");run(m,"commit");
        QCOMPARE(m.state().value("committed").toString(),"90.00");run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("committed").toString(),"90.00");
    }
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
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");QCOMPARE(m.state().value("newTransactions").toInt(),0);
        QVERIFY(m.state().value("receipt").toMap().value("summary").toString().contains("2 new transactions"));

        QVERIFY(m.state().value("frozen").toBool());
        run(m,"opening",{{"date","2026-08-01"},{"amount","200.00"}},false);
        QCOMPARE(m.state().value("committed").toString(),"120.00");
        run(m,"close");run(m,"open",{{"path",path}});
        QCOMPARE(m.state().value("newTransactions").toInt(),0);
        run(m,"import",{{"path",source},{"profile","DMY"}});QCOMPARE(selected(m).value("decision").toInt(),2);
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");
        QCOMPARE(m.state().value("newTransactions").toInt(),0);
        QVERIFY(m.state().value("importOutcome").toString().contains("2 already present"));
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
    void splitMemoBeforeCategory() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"!Type:Bank\nD2026-01-02\nT-10\nPSynthetic\nEFirst\nSFood\n$-12\nSRefund\nESecond\n$2\n^\n")},{"profile","DMY"}});
        QVERIFY(!m.state().value("pending").toMap().value("blocked").toBool());
        const auto splits=selected(m).value("draft").toMap().value("splits").toList();QCOMPARE(splits.size(),2);
        QCOMPARE(splits[0].toMap().value("memo").toString(),"First");QCOMPARE(splits[1].toMap().value("memo").toString(),"Second");
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"-10.00");
        run(m,"close");run(m,"open",{{"path",path}});QCOMPARE(m.state().value("committed").toString(),"-10.00");
    }
    void structuralDiagnostics() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/doc");
        run(m,"import",{{"path",fixture(dir.path(),"!Type:Bank\nD2026-01-01\nT-10\nPSecretSynthetic\rT999\nSSecretCategory\n^\n")},{"profile","DMY"}});
        const auto diagnostic=m.state().value("pending").toMap().value("diagnostics").toString();
        QVERIFY(diagnostic.contains("source lines 2–6"));
        QVERIFY(diagnostic.contains("error at line 6"));
        QVERIFY(diagnostic.contains("missing its amount"));
        QVERIFY(!diagnostic.contains("Secret") && !diagnostic.contains("999"));
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"0.00");QVERIFY(m.state().value("importFailures").toInt()>0);
        run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});
    }
    void recordRepair() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());create(m,dir.path()+"/doc");
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D31/02/2026\nT-10\nPDate repair\n^\nD1/3/2026\nT-20\nSFood\n$-19\n^\n")},{"profile","DMY"}});
        QVERIFY(selected(m).value("repair").toBool());QVERIFY(selected(m).value("editable").toBool());
        for(const auto& field:selected(m).value("sourceFields").toList())
            QVERIFY(field.toMap().value("field").toString()!="date");
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));QTest::qWait(100);
        if(qEnvironmentVariableIsSet("SPENTIT_REPAIR_SCREENSHOT")) QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_REPAIR_SCREENSHOT")));
        auto edit=selected(m).value("draft").toMap();edit["date"]="2026-02-28";
        run(m,"save",edit);QVERIFY(!selected(m).value("repair").toBool());
        QVERIFY(QMetaObject::invokeMethod(window,"restoreImportedValues"));
        QCOMPARE(window->findChild<QObject*>("durableDate")->property("text").toString(),QString());
        QVERIFY(QMetaObject::invokeMethod(window,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());
        QCOMPARE(selected(m).value("draft").toMap().value("date").toString(),"2026-02-28");
        QVERIFY(QMetaObject::invokeMethod(window,"loadEditor"));
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
        const auto invalid=fixture(dir.path(),"D1/1/2026\nT1\nL[Other\n^\n");m.command("import",{{"path",invalid},{"profile","DMY"},{"account",m.state().value("accountId")}});QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(m.error().isEmpty());
        for(const auto& item:m.state().value("importCategoryDecisions").toList())run(m,"resolve_import_categories",{{"source",item.toMap().value("source")},{"selected",QVariantList{}}});
        QVERIFY(!m.state().value("projectionAvailable").toBool());
        QCOMPARE(m.rowCount(),1);QCOMPARE(m.rowRecord(0).value("payee").toString(),QString("Opening Balance"));
        QCOMPARE(m.state().value("balance").toString(),QString("0.00"));run(m,"commit");QVERIFY(m.state().value("importFailures").toInt()>0);
        QVERIFY(!selected(m).value("diagnostic").toString().isEmpty());run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});QCOMPARE(m.state().value("committed").toString(),"0.00");
        run(m,"import",{{"path",fixture(dir.path(),QByteArray(1,static_cast<char>(0xff)))},{"profile","DMY"}});
        QVERIFY(selected(m).value("source").toString().contains("Original bytes"));QVERIFY(!selected(m).contains("draft"));run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});
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
        QVERIFY(!unsupported.state().value("unlockRequired").toBool());QVERIFY(unsupported.error().contains("This dataset format is not supported"));
        g=storage::document_gateway();
        g->create((dir.path()+"/protected").toStdString(),true,storage::public_document_key,"Synthetic",Currency("CAD"),"2026-10-25");g->close();
        QVERIFY_THROWS_EXCEPTION(app::UnlockRequired,g->open((dir.path()+"/protected").toStdString(),{}));
        g->open((dir.path()+"/protected").toStdString(),std::string(storage::public_document_key));g->close();
    }
    void cachedViewsRemainCurrent() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());create(m,dir.path()+"/one");
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        const auto file=fixture(dir.path(),"D2/1/2026\nT-10\nPFirst\nLFood\n^\nD3/1/2026\nT20\nPSecond\nLIncome\n^\n");
        run(m,"import",{{"path",file},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        for(int i=0;i<3;++i) {
            run(m,"query",{{"projected",false}});QCOMPARE(m.rowCount(),3);
            QCOMPARE(m.state().value("deposits").toString(),"120.00");
            QCOMPARE(m.state().value("balance").toString(),"110.00");
            run(m,"query",{{"projected",true},{"filter","First"},{"sort",5},{"descending",true}});
            QCOMPARE(m.rowCount(),1);QCOMPARE(m.state().value("withdrawals").toString(),"-10.00");
            QCOMPARE(m.data(m.index(0),Qt::UserRole).toMap().value("balance").toString(),"90.00");
        }
        run(m,"opening",{{"date","2026-01-01"},{"amount","200.00"}});
        QCOMPARE(m.state().value("committed").toString(),"200.00");
        QCOMPARE(m.data(m.index(0),Qt::UserRole).toMap().value("balance").toString(),"190.00");
        auto edit=selected(m).value("draft").toMap();edit["total"]="-11.00";
        run(m,"save",edit,false);QCOMPARE(m.state().value("projected").toString(),"210.00");
        auto splits=edit.value("splits").toList();auto split=splits[0].toMap();split["amount"]="-11.00";splits[0]=split;edit["splits"]=splits;
        run(m,"save",edit);QCOMPARE(m.state().value("withdrawals").toString(),"-11.00");
        QCOMPARE(m.data(m.index(0),Qt::UserRole).toMap().value("balance").toString(),"189.00");
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"209.00");
        QVERIFY(!selected(m).value("editable").toBool());
        QVERIFY(selected(m).value("candidates").toList().isEmpty());
        run(m,"query",{{"projected",false},{"filter","First"}});QCOMPARE(m.state().value("withdrawals").toString(),"-11.00");
        run(m,"import",{{"path",file},{"profile","DMY"}}); // Identical evidence is excluded.
        QCOMPARE(m.state().value("projected").toString(),"209.00");
        run(m,"decide");QCOMPARE(m.state().value("projected").toString(),"199.00");
        run(m,"query",{{"projected",true},{"filter","First"}});QCOMPARE(m.rowCount(),2);
        run(m,"decide",{{"duplicate",true},{"batch",batch},{"ordinal",0}});
        QCOMPARE(m.rowCount(),1);QCOMPARE(m.state().value("projected").toString(),"209.00");
        run(m,"discard",{{"token",m.state().value("rollback").toMap().value("token")}});QCOMPARE(m.state().value("projected").toString(),"209.00");
        run(m,"close");create(m,dir.path()+"/two");
        QCOMPARE(m.rowCount(),1);QVERIFY(selected(m).isEmpty());QCOMPARE(m.state().value("committed").toString(),"0.00");
        run(m,"close");run(m,"open",{{"path",dir.path()+"/one"}});
        QCOMPARE(m.state().value("committed").toString(),"209.00");QCOMPARE(m.rowCount(),3);
    }
    void inlineRecycling() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        QByteArray bytes;
        for(int i=0;i<80;++i) bytes+="D1/1/2026\nT-1\nPRow "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        // This interaction fixture uses chronological row positions.
        run(m,"query",{{"sort",0},{"descending",false}});
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto* editor=root->findChild<QQuickItem*>("transactionEditor");QVERIFY(editor);
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        QVERIFY(QTest::qWaitForWindowExposed(window));QTRY_VERIFY(editor->property("inlineRow").toBool());
        payee->setProperty("text","Survives recycling");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        ledger->setProperty("contentY",2000);QTest::qWait(100);
        ledger->setProperty("contentY",0);QTest::qWait(100);
        QCOMPARE(payee->property("text").toString(),"Survives recycling");QVERIFY(root->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Survives recycling");
        // Qt pointer dispatch on a visible ledger row opens that occurrence.
        run(m,"inspect",{{"batch",selected(m).value("batch")},{"ordinal",1}});
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));QTest::qWait(100);
        const auto point=ledger->mapToScene(QPointF(100,17)).toPoint();
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,point);
        QTRY_VERIFY(!m.busy());QCOMPARE(selected(m).value("ordinal").toInt(),0);
    }
    void accessibilityAndKeyboard() {
        QAccessible::setActive(true);
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2025-12-31"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-10\nPAccessible first\nSFood\n$-12\nSRefund\n$2\n^\nD2/1/2026\nT20\nPAccessible second\n^\n")},{"profile","DMY"}});
        // This interaction fixture uses chronological row positions.
        run(m,"query",{{"sort",0},{"descending",false}});
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto* search=root->findChild<QQuickItem*>("ledgerSearch");QVERIFY(search);
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        auto* list=QAccessible::queryAccessibleInterface(ledger);QVERIFY(list);
        QCOMPARE(list->role(),QAccessible::List);QVERIFY(list->text(QAccessible::Name).startsWith("Transactions in"));
        auto* field=QAccessible::queryAccessibleInterface(payee);QVERIFY(field);
        QCOMPARE(field->role(),QAccessible::EditableText);QCOMPARE(field->text(QAccessible::Name),QString("Payee"));
        ledger->forceActiveFocus();QTRY_VERIFY(ledger->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_End);QTRY_VERIFY(!m.busy());
        QCOMPARE(m.state().value("selectionIndex").toInt(),m.rowCount()-1);
        QTest::keyClick(window,Qt::Key_Home);QTRY_VERIFY(!m.busy());
        QTest::keyClick(window,Qt::Key_Down);QTRY_VERIFY(!m.busy());
        QTest::keyClick(window,Qt::Key_Down);QTRY_VERIFY(!m.busy());
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());
        QCOMPARE(selected(m).value("ordinal").toInt(),1);
        QTRY_VERIFY(visualItem(window->contentItem(),"ledgerRow_2"));
        auto* row=QAccessible::queryAccessibleInterface(visualItem(window->contentItem(),"ledgerRow_2"));QVERIFY(row);
        QCOMPARE(row->role(),QAccessible::ListItem);QVERIFY(row->text(QAccessible::Name).contains("Accessible second"));QVERIFY(row->text(QAccessible::Name).contains("CAD"));QVERIFY(row->state().selected);
        payee->forceActiveFocus();QTest::keyClick(window,Qt::Key_Tab);
        auto* total=root->findChild<QQuickItem*>("durableTotal");QVERIFY(total);QTRY_VERIFY(total->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Backtab);QTRY_VERIFY(payee->hasActiveFocus());
        for(char c:QByteArray(" changed"))QTest::keyClick(window,c);QVERIFY(root->property("dirty").toBool());
        // Escape from a modal guard must retain the edit and return focus.
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("sidebarSettings"),"clicked"));
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        auto* guardSave=root->findChild<QQuickItem*>("guardSave");QVERIFY(guardSave);
        QAccessibleInterface* guardAccessible=nullptr;
        QTRY_VERIFY(guardAccessible=QAccessible::queryAccessibleInterface(guardSave));
        QTRY_VERIFY(!guardAccessible->state().invisible);
        QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!dialog->property("visible").toBool());QVERIFY(root->property("dirty").toBool());
        QTRY_VERIFY(payee->hasActiveFocus());
        QTRY_VERIFY(guardAccessible->state().invisible);
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QVERIFY(!root->property("dirty").toBool());
        search->forceActiveFocus();for(char c:QByteArray("no-result"))QTest::keyClick(window,c);QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QCOMPARE(m.rowCount(),0);
        QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!m.busy());QCOMPARE(m.rowCount(),3);QVERIFY(search->hasActiveFocus());
        if(qEnvironmentVariableIsSet("SPENTIT_ACCESSIBILITY_HOLD"))QTest::qWait(240000);
    }
    void searchReturnNavigation() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        QByteArray bytes;
        for(int i=0;i<80;++i) bytes+="D1/1/2026\nT-1\nPRow "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto* search=root->findChild<QQuickItem*>("ledgerSearch");QVERIFY(search);
        auto click=[&](const char* name){return QMetaObject::invokeMethod(root->findChild<QObject*>(name),"clicked");};
        QTest::qWait(100);ledger->setProperty("contentY",1000);QTest::qWait(100);
        int beforeIndex=-1;
        QVERIFY(QMetaObject::invokeMethod(ledger,"indexAt",Q_RETURN_ARG(int,beforeIndex),Q_ARG(qreal,1),Q_ARG(qreal,ledger->property("contentY").toDouble()+1)));
        const auto beforeIdentity=m.rowIdentity(beforeIndex);
        search->setProperty("text","No matching transaction");QVERIFY(click("applyFilter"));QTRY_VERIFY(!m.busy());
        QCOMPARE(m.rowCount(),0);QCOMPARE(selected(m).value("ordinal").toInt(),0);
        QVERIFY(QMetaObject::invokeMethod(root,"query",Q_ARG(QVariant,QVariant(1)),Q_ARG(QVariant,QVariant(true))));QTRY_VERIFY(!m.busy());
        QCOMPARE(m.state().value("sort").toInt(),1);
        // Inspecting whole-file evidence must not count as choosing a search result.
        run(m,"evidence",{{"batch",batch}});
        QVERIFY(click("clearFilter"));QTRY_VERIFY(!m.busy());QTest::qWait(100);
        QCOMPARE(selected(m).value("ordinal").toInt(),0);QCOMPARE(m.rowCount(),81);
        QCOMPARE(m.state().value("sort").toInt(),0);QVERIFY(m.state().value("descending").toBool());
        int afterIndex=-1;
        QVERIFY(QMetaObject::invokeMethod(ledger,"indexAt",Q_RETURN_ARG(int,afterIndex),Q_ARG(qreal,1),Q_ARG(qreal,ledger->property("contentY").toDouble()+1)));
        QCOMPARE(m.rowIdentity(afterIndex),beforeIdentity);
        search->setProperty("text","Row 7");QVERIFY(click("applyFilter"));QTRY_VERIFY(!m.busy());
        run(m,"inspect",{{"batch",batch},{"ordinal", 70}});
        QVERIFY(click("clearFilter"));QTRY_VERIFY(!m.busy());QTest::qWait(100);
        QCOMPARE(selected(m).value("ordinal").toInt(),70);QCOMPARE(m.rowCount(),81);
        QVERIFY(m.selectedRowIndex()>=0);
        auto* editor=root->findChild<QQuickItem*>("transactionEditor");QVERIFY(editor);
        const auto editorTop=editor->mapToItem(ledger,QPointF(0,0)).y();
        QVERIFY(editorTop>=0 && editorTop<ledger->height());
        QCOMPARE(m.rowIdentity(m.selectedRowIndex()).value("ordinal").toInt(),70);
        QCOMPARE(m.occurrenceIndex(batch.toString(),70),m.selectedRowIndex());
        QVERIFY(m.rowIdentity(-1).isEmpty());QCOMPARE(m.occurrenceIndex("missing",0),-1);
        // A dirty edit must resolve before clearing a filter.
        search->setProperty("text","Row 7");QVERIFY(click("applyFilter"));QTRY_VERIFY(!m.busy());
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        payee->setProperty("text","Unsaved search edit");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(click("clearFilter"));auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());QVERIFY(click("guardStay"));
        QCOMPARE(search->property("text").toString(),"Row 7");QVERIFY(root->property("dirty").toBool());
        auto* total=root->findChild<QQuickItem*>("durableTotal");QVERIFY(total);
        total->setProperty("text","bad");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        QVERIFY(click("clearFilter"));QVERIFY(click("guardSave"));QTRY_VERIFY(!m.busy());QVERIFY(!root->property("splitEditMessage").toString().isEmpty());
        QCOMPARE(search->property("text").toString(),"Row 7");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click("clearFilter"));QVERIFY(click("guardDiscard"));QTRY_VERIFY(!m.busy());
        QCOMPARE(search->property("text").toString(),"");QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Row 70");
        search->setProperty("text","Row 7");QVERIFY(click("applyFilter"));QTRY_VERIFY(!m.busy());
        payee->setProperty("text","Saved on search return");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(click("clearFilter"));QVERIFY(click("guardSave"));QTRY_VERIFY(!m.busy());
        QTRY_COMPARE(search->property("text").toString(),"");QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Saved on search return");
        search->setProperty("text","Row 7");QVERIFY(click("applyFilter"));QTRY_VERIFY(!m.busy());
        run(m,"close");create(m,dir.path()+"/other");
        QVERIFY(root->property("searchReturn").isNull());QCOMPARE(search->property("text").toString(),"");
    }
    void readOnlySplitViews() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        QByteArray bytes;
        for(int i=0;i<80;++i) bytes+="D1/1/2026\nT-10\nPRow "+QByteArray::number(i)+"\nSExpense\nECharge\n$-12\nSRefund\nEAdjustment\n$2\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        // This interaction fixture uses chronological row positions.
        run(m,"query",{{"sort",0},{"descending",false}});
        const auto batch=selected(m).value("batch");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // Keep the editor offscreen so several other split rows are visible.
        run(m,"inspect",{{"batch",batch},{"ordinal",79}});
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        payee->setProperty("text","Unsaved editor");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));QTest::qWait(100);
        auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
        QTRY_VERIFY(item("splitToggle_0"));
        auto* toggle=item("splitToggle_0");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,toggle->mapToScene(QPointF(toggle->width()/2,17)).toPoint());
        QTRY_VERIFY(item("splitPreview_0") && item("splitPreview_0")->height()>0);
        QTRY_VERIFY(item("splitToggle_1"));
        QVERIFY(QMetaObject::invokeMethod(item("splitToggle_1"),"clicked"));
        QTRY_VERIFY(item("splitPreview_1") && item("splitPreview_1")->height()>0);
        if(qEnvironmentVariableIsSet("SPENTIT_SPLITS_SCREENSHOT")) QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_SPLITS_SCREENSHOT")));
        QCOMPARE(selected(m).value("ordinal").toInt(),79);
        QCOMPARE(payee->property("text").toString(),"Unsaved editor");QVERIFY(root->property("dirty").toBool());
        QCOMPARE(m.state().value("projected").toString(),"-800.00");
        const auto splits=m.data(m.index(0,0),Qt::UserRole).toMap().value("splits").toList();
        QCOMPARE(splits[0].toMap().value("amount").toString(),"-12.00");
        QCOMPARE(splits[1].toMap().value("amount").toString(),"2.00");
        ledger->setProperty("contentY",2000);QTest::qWait(100);
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));QTest::qWait(100);
        QTRY_VERIFY(item("splitPreview_0") && item("splitPreview_0")->height()>0);
        run(m,"query",{{"projected",true},{"filter","\"Row 0\""},{"sort",1},{"descending",true}});
        QTRY_VERIFY(item("splitPreview_0") && item("splitPreview_0")->height()>0);
        QCOMPARE(payee->property("text").toString(),"Unsaved editor");
        run(m,"query",{{"projected",true},{"filter","\"Row 1\""}});
        QTRY_VERIFY(item("splitPreview_1") && item("splitPreview_1")->height()>0);
        QVERIFY(QMetaObject::invokeMethod(item("splitToggle_1"),"clicked"));
        QTRY_COMPARE(item("splitPreview_1")->height(),0);
        // Closing clears view-only identities, including when the next dataset uses the same ordinals.
        run(m,"close");create(m,dir.path()+"/other");
        run(m,"query",{{"sort",0},{"descending",false}});
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        run(m,"inspect",{{"batch",selected(m).value("batch")},{"ordinal",79}});
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));QTest::qWait(100);
        QTRY_VERIFY(item("splitPreview_0"));QCOMPARE(item("splitPreview_0")->height(),0);
    }
    void inlineSourceRestore() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        const QByteArray bytes="D2/1/2026\nT-10\nPOriginal\nMSource memo\nSFood\n$-10\n^\nD3/1/2026\nT20\nPSecond\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        const auto evidence=selected(m).value("source");
        auto* editor=root->findChild<QQuickItem*>("transactionEditor");QVERIFY(editor);
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        auto* total=root->findChild<QQuickItem*>("durableTotal");QVERIFY(total);
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(editor->property("inlineRow").toBool());
        QCOMPARE(m.selectedRowIndex(),1); // Opening balance precedes the import.
        auto edit=selected(m).value("draft").toMap();edit["payee"]="Corrected";edit["total"]="-11.00";
        auto splits=edit.value("splits").toList();auto split=splits[0].toMap();split["amount"]="-11.00";splits[0]=split;auto extra=split;extra["allocationId"]="";extra["amount"]="0.00";splits.push_back(extra);edit["splits"]=splits;
        run(m,"save",edit);QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().size(),2);
        auto* toggle=root->findChild<QQuickItem*>("sourceToggle");QVERIFY(toggle);
        auto* scroll=root->findChild<QObject*>("editorScroll");QVERIFY(scroll);
        auto* flick=qvariant_cast<QObject*>(scroll->property("contentItem"));QVERIFY(flick);
        flick->setProperty("contentY",toggle->y());QTest::qWait(100);
        auto* indicator=qvariant_cast<QQuickItem*>(toggle->property("indicator"));QVERIFY(indicator);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,indicator->mapToScene(QPointF(indicator->width()/2,indicator->height()/2)).toPoint());
        QTRY_VERIFY(toggle->property("checked").toBool());
        auto* restore=visualItem(window->contentItem(),"restoreImportedValues");QVERIFY(restore);
        QTRY_VERIFY(restore->isVisible());
        QVERIFY(restore->mapToItem(qobject_cast<QQuickItem*>(scroll),QPointF(restore->width(),0)).x()<=qobject_cast<QQuickItem*>(scroll)->width()-24);
        QVERIFY(!visualItem(window->contentItem(),"copySource_payee_-1"));
        QVERIFY(QMetaObject::invokeMethod(restore,"clicked"));
        QCOMPARE(payee->property("text").toString(),"Original");QVERIFY(root->property("dirty").toBool());
        QCOMPARE(total->property("text").toString(),"-10.00");
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Corrected");
        QTest::keyClick(window,Qt::Key_Escape);
        QCOMPARE(payee->property("text").toString(),"Corrected");QVERIFY(!root->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(restore,"clicked"));
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());
        QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().size(),1);
        QCOMPARE(selected(m).value("source"),evidence);
        QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),"-10.00");
        // Identity follows sorting; filtering the row out retains an accessible editor.
        run(m,"query",{{"sort",1},{"descending",true},{"projected",true}});
        QTRY_VERIFY(editor->property("inlineRow").toBool());
        QCOMPARE(m.data(m.index(m.selectedRowIndex()),Qt::UserRole).toMap().value("ordinal").toInt(),0);
        run(m,"query",{{"filter","Second"},{"projected",true}});
        QCOMPARE(m.selectedRowIndex(),-1);QTRY_VERIFY(!editor->property("inlineRow").toBool());
        QCOMPARE(editor->parentItem()->objectName(),"reviewPanel");
        run(m,"query",{{"projected",true}});QTRY_VERIFY(editor->property("inlineRow").toBool());
        // A failed navigation save cannot switch to another occurrence.
        total->setProperty("text","bad");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        QVERIFY(QMetaObject::invokeMethod(root,"inspectRecord",Q_ARG(QVariant,batch),Q_ARG(QVariant,QVariant(1))));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardSave"),"clicked"));
        QTRY_VERIFY(!m.busy());QVERIFY(!root->property("splitEditMessage").toString().isEmpty());QCOMPARE(selected(m).value("ordinal").toInt(),0);
        QVERIFY(root->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"inspectRecord",Q_ARG(QVariant,batch),Q_ARG(QVariant,QVariant(1))));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardDiscard"),"clicked"));
        QTRY_VERIFY(!m.busy());QCOMPARE(selected(m).value("ordinal").toInt(),1);
        QCOMPARE(payee->property("text").toString(),"Second");
        run(m,"close");run(m,"open",{{"path",dir.path()+"/doc"}});
        run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(selected(m).value("source"),evidence);
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Original");
        run(m,"commit");QVERIFY(!selected(m).value("editable").toBool());
        toggle->setProperty("checked",true);
        QTRY_VERIFY(visualItem(window->contentItem(),"restoreImportedValues"));
        QVERIFY(visualItem(window->contentItem(),"restoreImportedValues")->property("enabled").toBool());
        flick->setProperty("contentY",toggle->y());QTest::qWait(100);
        if(qEnvironmentVariableIsSet("SPENTIT_INLINE_SCREENSHOT")) QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_INLINE_SCREENSHOT")));
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
        QTRY_VERIFY(!m.busy());QVERIFY(!root->property("splitEditMessage").toString().isEmpty());QVERIFY(root->property("dirty").toBool());QVERIFY(m.state().value("active").toBool());
        total->setProperty("text","-1.00");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));
        QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
        QTest::qWait(100);
        const auto image=window->grabWindow();QVERIFY(!image.isNull());
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_SCREENSHOT")) QVERIFY(image.save(qEnvironmentVariable("SPENTIT_TEST_SCREENSHOT")));
        payee->setProperty("text","Discard me");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(!window->close());auto* discard=root->findChild<QObject*>("guardDiscard");QVERIFY(discard);QVERIFY(QMetaObject::invokeMethod(discard,"clicked"));
        QTRY_VERIFY(!m.busy());QTRY_VERIFY(root->findChild<QObject*>("quitCommitDialog")->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"quitCommitNo"),"clicked"));QTRY_VERIFY(!window->isVisible());QTRY_VERIFY(!m.busy() && !m.state().value("active").toBool());
        run(m,"open",{{"path",dir.path()+"/doc"}});
        const auto batch=m.state().value("pending").toMap().value("id");run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
    }
    void accountDialogKeyboard_data() {
        QTest::addColumn<int>("key");
        QTest::newRow("return")<<int(Qt::Key_Return);
        QTest::newRow("keypad-enter")<<int(Qt::Key_Enter);
    }
    void accountDialogKeyboard() {
        QFETCH(int,key);
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* dialog=root->findChild<QObject*>("renameAccountDialog");
        auto* field=root->findChild<QQuickItem*>("renameAccountName");QVERIFY(dialog && field);
        const auto original=m.state().value("name").toString();
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
        QTRY_VERIFY(dialog->property("opened").toBool());field->setProperty("text","Canceled rename");field->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(m.state().value("name").toString(),original);
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","Keyboard name");field->forceActiveFocus();
        QTest::keyClick(window,Qt::Key(key));
        QTRY_COMPARE(m.state().value("name").toString(),QString("Keyboard name"));
        QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(!dialog->property("visible").toBool());
        // The same command validation rejects a blank name submitted by keyboard.
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text"," ");field->forceActiveFocus();QTest::keyClick(window,Qt::Key(key));
        QTRY_VERIFY(!m.error().isEmpty());QCOMPARE(m.state().value("name").toString(),QString("Keyboard name"));
        auto* currency=root->findChild<QObject*>("currencyDialog");
        auto* search=root->findChild<QQuickItem*>("currencySearch");QVERIFY(currency && search);
        QVERIFY(QMetaObject::invokeMethod(currency,"open"));QTRY_VERIFY(currency->property("opened").toBool());
        search->setProperty("text","INVALID");search->forceActiveFocus();QTest::keyClick(window,Qt::Key(key));
        QVERIFY(currency->property("visible").toBool());QVERIFY(!m.busy());
        search->setProperty("text","JPY");QTest::keyClick(window,Qt::Key(key));
        QTRY_COMPARE(m.state().value("currency").toString(),QString("JPY"));QTRY_VERIFY(!m.busy());
        QVERIFY(m.error().isEmpty());QVERIFY(!currency->property("visible").toBool());
        run(m,"create_account",{{"id","2"},{"name","Opening keyboard test"},{"currency","JPY"},{"date","2026-01-01"}});
        run(m,"opening",{{"date","2026-01-01"},{"amount","0"}});
        auto* opening=root->findChild<QObject*>("correctOpeningDialog");
        auto* date=root->findChild<QQuickItem*>("correctOpeningDate");
        auto* amount=root->findChild<QQuickItem*>("correctOpeningAmount");QVERIFY(opening && date && amount);
        for(int i=1;i<=2;++i) {
            QVERIFY(QMetaObject::invokeMethod(opening,"open"));QTRY_VERIFY(opening->property("opened").toBool());
            date->setProperty("text","2026-01-01");amount->setProperty("text",QString::number(i));
            (i==1?date:amount)->forceActiveFocus();QTest::keyClick(window,Qt::Key(key));
            QTRY_COMPARE(m.state().value("opening").toString(),QString::number(i));
            QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(!opening->property("visible").toBool());
        }
    }

    void accountContextMenu() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        run(m,"create_account",{{"id","2"},{"name","Second"},{"currency","CAD"},{"date","2026-01-01"}});
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* menu=root->findChild<QObject*>("accountContextMenu");QVERIFY(menu);
        QQuickItem* first=nullptr;
        QTRY_VERIFY(first=visualItem(window->contentItem(),"accountNavigation_1"));
        auto* add=root->findChild<QQuickItem*>("newAccount");QVERIFY(add);
        QVERIFY(add->mapToScene(QPointF()).y()<first->mapToScene(QPointF()).y());
        root->setProperty("dirty",true);
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,first->mapToScene(QPointF(first->width()/2,first->height()/2)).toPoint());
        QTRY_VERIFY(root->findChild<QObject*>("unsavedDialog")->property("visible").toBool());
        QVERIFY(!menu->property("visible").toBool());
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardStay"),"clicked"));
        root->setProperty("dirty",false);
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,first->mapToScene(QPointF(first->width()/2,first->height()/2)).toPoint());
        QTRY_VERIFY(menu->property("visible").toBool());
        QCOMPARE(m.state().value("accountId").toString(),QString("1"));
        auto* rename=root->findChild<QObject*>("renameAccount");QVERIFY(rename);
        QVERIFY(QMetaObject::invokeMethod(rename,"triggered"));
        auto* dialog=root->findChild<QObject*>("renameAccountDialog");QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        root->findChild<QObject*>("renameAccountName")->setProperty("text","Renamed first");
        QVERIFY(QMetaObject::invokeMethod(menu,"close"));
        QVERIFY(QMetaObject::invokeMethod(dialog,"accepted"));
        QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("name").toString(),QString("Renamed first"));
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
        run(m,"select_account",{{"id","2"}});
        QCOMPARE(m.state().value("name").toString(),QString("Second"));
        // Existing empty-account deletion still permits removing the extra account,
        // while refusing to remove the only remaining account.
        run(m,"delete_account",{{"operation",m.newIdentity()}});
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(m.state().value("accounts").toList().size(),1);
        run(m,"delete_account",{{"operation",m.newIdentity()}},false);
        QCOMPARE(m.state().value("accounts").toList().size(),1);
        QVERIFY(!m.error().isEmpty());
        run(m,"archive_account",{{"operation",m.newIdentity()}});
        QQuickItem* archived=nullptr;
        QTRY_VERIFY(archived=visualItem(window->contentItem(),"archivedAccount_1"));
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,archived->mapToScene(QPointF(archived->width()/2,archived->height()/2)).toPoint());
        QTRY_VERIFY(menu->property("visible").toBool());
        QCOMPARE(m.state().value("accountId").toString(),QString("1"));
        QVERIFY(!root->findChild<QObject*>("archiveAccount")->property("visible").toBool());
        auto* reactivate=root->findChild<QObject*>("reactivateAccount");QVERIFY(reactivate->property("visible").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_ACCOUNT_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_ACCOUNT_SCREENSHOT")));}
        QVERIFY(QMetaObject::invokeMethod(reactivate,"triggered"));
        QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QVERIFY(!m.state().value("archived").toBool());
    }

    void selectionDoesNotFlash() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");run(m,"opening",{{"date","2025-12-31"},{"amount","0.00"}});
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-1\nPFirst\n^\nD2/1/2026\nT-2\nPSecond\n^\nD3/1/2026\nT-3\nPThird\n^\n")},{"profile","DMY"}});
        const auto batch=selected(m).value("batch").toString();run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(QTest::qWaitForWindowExposed(window));auto* editor=root->findChild<QQuickItem*>("transactionEditor");auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(editor && payee);QTRY_VERIFY(editor->property("inlineRow").toBool());
        const auto parent=editor->parentItem();const auto text=payee->property("text");
        QSignalSpy enabled(editor,&QQuickItem::enabledChanged),resets(&m,&QAbstractItemModel::modelReset),changes(&m,&ImportModel::changed),completed(&m,&ImportModel::completed);
        std::vector<std::unique_ptr<QSignalSpy>> sidebar;
        for(const char* name:{"newAccount","importStatement","reviewNavigation","categoryNavigation","rulesNavigation","historyNavigation","sidebarSettings"}){
            auto* button=root->findChild<QQuickItem*>(name);QVERIFY(button);QVERIFY(button->isEnabled());sidebar.push_back(std::make_unique<QSignalSpy>(button,&QQuickItem::enabledChanged));
        }
        bool sawBusy=false;connect(&m,&ImportModel::changed,this,[&]{sawBusy|=m.busy();});
        const auto clickRow=[&](int ordinal){auto* row=visualItem(window->contentItem(),"ledgerRow_"+QString::number(m.occurrenceIndex(batch,ordinal)));if(!row)return false;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,row->mapToScene(QPointF(100,17)).toPoint());return true;};
        QVERIFY(clickRow(0));QCOMPARE(changes.count(),0);QCOMPARE(completed.count(),0);
        QVERIFY(clickRow(1));QCOMPARE(m.state().value("selectionIndex").toInt(),m.occurrenceIndex(batch,1));QVERIFY(!sawBusy);QCOMPARE(completed.count(),1);
        QCOMPARE(selected(m).value("ordinal").toInt(),0);QCOMPARE(editor->parentItem(),parent);QCOMPARE(payee->property("text"),text);QCOMPARE(enabled.count(),0);QCOMPARE(resets.count(),0);
        for(const auto& spy:sidebar)QCOMPARE(spy->count(),0);
        const auto count=changes.count();QVERIFY(clickRow(1));QCOMPARE(changes.count(),count);QCOMPARE(completed.count(),1);
        // The synchronous choice must be the anchor for subsequent worker-based ranges.
        run(m,"select",{{"row",m.occurrenceIndex(batch,2)},{"range",true}});QCOMPARE(m.state().value("selectionCount").toInt(),2);
        run(m,"select",{{"row",m.occurrenceIndex(batch,2)}});QCOMPARE(m.state().value("selectionCount").toInt(),1);
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)},{"range",true}});QCOMPARE(m.state().value("selectionCount").toInt(),3);
        run(m,"select",{{"row",m.occurrenceIndex(batch,0)}});
        // Repeated selection after entering a search still marks the chosen result.
        const auto beforeSearch=completed.count();run(m,"select",{{"row",m.occurrenceIndex(batch,0)},{"firstSearchSelection",true}});QCOMPARE(completed.count(),beforeSearch+1);
        enabled.clear();for(const auto& spy:sidebar)spy->clear();sawBusy=false;
        payee->setProperty("text","Accepted edit");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));
        QVERIFY(m.busy());QVERIFY(!editor->isEnabled());QVERIFY(!root->findChild<QQuickItem*>("sidebarSettings")->isEnabled());
        QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(sawBusy);QCOMPARE(enabled.count(),2);for(const auto& spy:sidebar)QCOMPARE(spy->count(),2);
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Accepted edit");
    }
    void repeatedSplitRestoreEnter_data() {
        QTest::addColumn<bool>("committed");
        QTest::newRow("new")<<false;
        QTest::newRow("committed")<<true;
    }
    void repeatedSplitRestoreEnter() {
        QFETCH(bool,committed);
        QAccessible::setActive(true);
        SplitAccessibilityTrace trace;
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);
        QByteArray bytes="D1/1/2026\nT-7.35\nPSynthetic split purchase\nMFixture memo\nS\nEFixture fee\n$-0.04\nS\nEFixture purchase\n$-7.31\n^\n";
        // Keep Review rebuilding while queued progress changes update the status label.
        for(int i=0;i<250;++i)bytes+="D2/1/2026\nT-2\nPFixture "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        if(committed)run(m,"commit");
        run(m,"review_load");run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto original=selected(m).value("source");
        auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
        auto* source=item("sourceToggle");QVERIFY(source);source->setProperty("checked",true);
        auto* phase=item("operationPhase");QVERIFY(phase);QSignalSpy updates(phase,phase->metaObject()->property(phase->metaObject()->indexOfProperty("text")).notifySignal());QVERIFY(updates.isValid());
        const auto splits=[&]{return selected(m).value("draft").toMap().value("splits").toList();};
        const auto waitSaved=[&]{QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QVERIFY(!root->property("dirty").toBool());};
        const int cycles=qEnvironmentVariableIntValue("SPENTIT_SPLIT_REPRO_CYCLES")>0?qMin(100,qEnvironmentVariableIntValue("SPENTIT_SPLIT_REPRO_CYCLES")):12;
        for(int cycle=0;cycle<cycles;++cycle) {
            QVERIFY(QMetaObject::invokeMethod(item("addSplit"),"clicked"));
            QTRY_VERIFY(item("editSplitAmount_2"));QTest::qWait(100);auto* amount=item("editSplitAmount_2");
            amount->forceActiveFocus();QTRY_VERIFY(amount->hasActiveFocus());
            QTest::keySequence(window,QKeySequence::SelectAll);QTest::keyClick(window,Qt::Key_Minus);QTest::keyClick(window,Qt::Key_1);QTest::keyClick(window,Qt::Key_Return);waitSaved();
            QCOMPARE(splits().size(),3);QCOMPARE(splits()[0].toMap().value("amount").toString(),QString("-0.04"));QCOMPARE(splits()[1].toMap().value("amount").toString(),QString("-6.31"));QCOMPARE(splits()[2].toMap().value("amount").toString(),QString("-1.00"));
            QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),QString("-7.35"));
            if(cycle%3==0) {
                QVERIFY(QMetaObject::invokeMethod(root,"restoreImportedValues"));
                QTest::keyClick(window,Qt::Key_Return);waitSaved();
            } else if(cycle%3==1) {
                run(m,"undo_edit");
            } else {
                run(m,"select",{{"row",m.occurrenceIndex(batch.toString(),0)}});
                run(m,"restore_imported_selection",{{"account",m.state().value("accountId")},{"generation",m.state().value("ledgerEditGeneration")},{"selected",QVariantList{QVariantMap{{"batch",batch},{"ordinal",0}}}}});
            }
            QCOMPARE(splits().size(),2);QCOMPARE(splits()[0].toMap().value("amount").toString(),QString("-0.04"));QCOMPARE(splits()[1].toMap().value("amount").toString(),QString("-7.31"));
            QCOMPARE(selected(m).value("source"),original);
            if(cycle%4==3){run(m,"close");run(m,"open",{{"path",path}});run(m,"review_load");run(m,"inspect",{{"batch",batch},{"ordinal",0}});}
        }
        QVERIFY(updates.count()>cycles);
        qInfo()<<"Completed split/restore cycles:"<<cycles<<"status text changes:"<<updates.count();
        run(m,"close");run(m,"open",{{"path",path}});run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(splits().size(),2);QCOMPARE(selected(m).value("source"),original);
    }
    void existingSplitRemainder() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-10.00\nPSynthetic purchase\nS\nEFee\n$-0.25\nS\nEPurchase\n$-9.75\n^\n")},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
        auto saved=[&](int i){return selected(m).value("draft").toMap().value("splits").toList()[i].toMap().value("amount").toString();};
        auto type=[&](int i,const QString& value){auto* field=item("editSplitAmount_"+QString::number(i));QVERIFY(field);field->setProperty("text",value);QVERIFY(QMetaObject::invokeMethod(field,"textEdited"));};
        auto enter=[&]{QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));};
        QCOMPARE(root->property("automaticSplitIndex").toInt(),1);
        QVERIFY(item("splitRemainder_1")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(item("addSplit"),"clicked"));QTRY_VERIFY(item("editSplitAmount_2"));
        type(2,"-8.00");enter();QCOMPARE(saved(0),"-0.25");QCOMPARE(saved(1),"-1.75");QCOMPARE(saved(2),"-8.00");
        const auto identity=selected(m).value("draft").toMap().value("remainderAllocation").toString();QVERIFY(!identity.isEmpty());
        run(m,"close");run(m,"open",{{"path",path}});run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(root->property("automaticSplitIndex").toInt(),1); // Never jumps to the now larger manually entered split.
        run(m,"commit");type(0,"-0.50");enter();QCOMPARE(saved(1),"-1.50");
        QVERIFY(QMetaObject::invokeMethod(item("removeSplit_0"),"clicked"));enter();
        QCOMPARE(root->property("automaticSplitIndex").toInt(),0);QCOMPARE(saved(0),"-2.00");
        run(m,"undo_edit");QCOMPARE(root->property("automaticSplitIndex").toInt(),1);QCOMPARE(saved(0),"-0.50");
        type(1,"-1.00");enter();QVERIFY(root->property("dirty").toBool());QCOMPARE(saved(1),"-1.50");
        type(2,"-8.50");enter();QVERIFY(selected(m).value("draft").toMap().value("manualSplitAmounts").toBool());
        run(m,"close");run(m,"open",{{"path",path}});run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(root->property("automaticSplitIndex").toInt(),-1);QCOMPARE(saved(1),"-1.00");
    }
    void removedInitialRemainder() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        create(m,dir.path()+"/doc");
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT-10\nPSynthetic removal\nS\n$-1\nS\n$-9\n^\n")},{"profile","DMY"}});
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
        QCOMPARE(root->property("automaticSplitIndex").toInt(),1);
        QVERIFY(QMetaObject::invokeMethod(item("removeSplit_1"),"clicked"));
        QCOMPARE(root->property("automaticSplitIndex").toInt(),-1);QVERIFY(root->property("manualSplitAmounts").toBool());
        item("durableTotal")->setProperty("text","-1");QVERIFY(QMetaObject::invokeMethod(item("durableTotal"),"textEdited"));
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QVERIFY(selected(m).value("draft").toMap().value("manualSplitAmounts").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().size(),1);
        run(m,"undo_edit");QCOMPARE(root->property("automaticSplitIndex").toInt(),1);
    }
    void amountFormatting() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);window->show();
        const auto path=dir.path()+"/doc";create(m,path);
        QVERIFY(m.state().value("amountFormatting").toBool());
        QCOMPARE(m.state().value("amountStyle").toString(),"comma-dot");
        const auto definition=QString::fromStdString(Currency("CAD").definition_id());
        run(m,"correct_opening",{{"date","2025-12-31"},{"amount","16,782.00+1,876.00"}});
        QCOMPARE(m.state().value("opening").toString(),"18,658.00");
        QCOMPARE(m.rowRecord(0).value("balance").toString(),"18,658.00");
        run(m,"amount_format",{{"enabled",true},{"style","dot-comma"}});
        QCOMPARE(m.state().value("opening").toString(),"18.658,00");
        QCOMPARE(m.rowRecord(0).value("balance").toString(),"18.658,00");
        QCOMPARE(m.amountValue("1.000,25+2,75",definition).value("amount").toString(),"1.003,00");
        QVERIFY(!m.amountValue("1.23",definition).value("valid").toBool());
        auto result=m.splitAmounts("-10.000,00",{"ignored","-1.876,00"},true,definition,0);
        QVERIFY(result.value("balanced").toBool());
        QCOMPARE(result.value("amounts").toList()[0].toString(),"-8.124,00");
        run(m,"begin_manual");auto manual=selected(m).value("draft").toMap();
        manual["total"]="-1.000,00-3,00";auto splits=manual.value("splits").toList();auto split=splits[0].toMap();split["amount"]="-1003,00";splits[0]=split;manual["splits"]=splits;
        run(m,"save_manual",manual);
        QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),"-1.003,00");
        QCOMPARE(m.state().value("balance").toString(),"17.655,00");run(m,"undo_manual");
        QCOMPARE(m.state().value("balance").toString(),"18.658,00");
        run(m,"correct_opening",{{"date","2025-12-31"},{"amount","18.658,00+1"}});
        QCOMPARE(m.state().value("opening").toString(),"18.659,00");
        run(m,"close");run(m,"open",{{"path",path}});
        QCOMPARE(m.state().value("amountStyle").toString(),"dot-comma");
        QCOMPARE(m.state().value("opening").toString(),"18.659,00");
        run(m,"amount_format",{{"enabled",false},{"style","dot-comma"}});
        QCOMPARE(m.state().value("opening").toString(),"18659.00");
        QCOMPARE(m.amountValue("1000.25+2.75",definition).value("amount").toString(),"1003.00");
        QVERIFY(!m.amountValue("1,000.25",definition).value("valid").toBool());
        run(m,"close");run(m,"open",{{"path",path}});
        QVERIFY(!m.state().value("amountFormatting").toBool());
        QCOMPARE(m.state().value("amountStyle").toString(),"dot-comma");
        run(m,"amount_format",{{"enabled",true},{"style","space-comma"}});
        QCOMPARE(m.state().value("opening").toString(),"18 659,00");
        QCOMPARE(m.amountValue("1 000,25 + 2,75",definition).value("amount").toString(),"1 003,00");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"sidebarSettings"),"clicked"));
        QTRY_COMPARE(window->property("currentPage").toString(),"settings");
        auto* checkbox=visualItem(window->contentItem(),"amountFormatting");auto* style=visualItem(window->contentItem(),"amountFormatStyle");
        QVERIFY(checkbox);QVERIFY(style);QVERIFY(checkbox->property("checked").toBool());
        QCOMPARE(style->property("currentIndex").toInt(),2);
        if(qEnvironmentVariableIsSet("SPENTIT_AMOUNT_CAPTURE")){QTest::qWait(150);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_AMOUNT_CAPTURE")));}
        checkbox->setProperty("checked",false);QVERIFY(QMetaObject::invokeMethod(checkbox,"toggled"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(m.error().isEmpty());
        QVERIFY(!m.state().value("amountFormatting").toBool());QVERIFY(!style->property("enabled").toBool());
        checkbox->setProperty("checked",true);QVERIFY(QMetaObject::invokeMethod(checkbox,"toggled"));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(m.error().isEmpty());
        style->setProperty("currentIndex",0);QVERIFY(QMetaObject::invokeMethod(style,"activated",Q_ARG(int,0)));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY(m.error().isEmpty());
        QCOMPARE(m.state().value("amountStyle").toString(),"comma-dot");
    }
    void amountExpressions() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2025-12-31"},{"amount","(10/3)*3"}});
        QCOMPARE(m.state().value("opening").toString(),"10.00");
        run(m,"correct_opening",{{"date","2025-12-31"},{"amount","-9.34+(12*0.25)"}});
        QCOMPARE(m.state().value("opening").toString(),"-6.34");
        run(m,"close");run(m,"open",{{"path",path}});
        QCOMPARE(m.state().value("opening").toString(),"-6.34");
        const auto definition=QString::fromStdString(Currency("CAD").definition_id());
        auto result=m.splitAmounts("-10*2",{"ignored","-9.34+(12*0.25)"},true,definition,0);
        QVERIFY(result.value("valid").toBool());QVERIFY(result.value("balanced").toBool());
        QCOMPARE(result.value("total").toString(),"-20.00");
        QCOMPARE(result.value("amounts").toList()[0].toString(),"-13.66");
        QCOMPARE(result.value("amounts").toList()[1].toString(),"-6.34");
        QVERIFY(!m.splitAmounts("10",{"ignored","10/3"},true,definition,0).value("valid").toBool());
    }
    void remainderSelection() {
        ImportModel m(storage::document_gateway());const auto definition=QString::fromStdString(Currency("CAD").definition_id());
        auto part=[](const QString& amount,bool transfer=false){return QVariantMap{{"amount",amount},{"transferAmount",transfer}};};
        QCOMPARE(m.chooseSplitRemainder("-10",{part("-1"),part("-9")},definition),1);
        QCOMPARE(m.chooseSplitRemainder("-10",{part("50"),part("-30"),part("-30")},definition),1);
        QCOMPARE(m.chooseSplitRemainder("0",{part("10"),part("-10")},definition),0);
        QCOMPARE(m.chooseSplitRemainder("-10",{part("-1"),part("-9",true)},definition),0);
        QCOMPARE(m.chooseSplitRemainder("-10",{part("-10",true)},definition),-1);
        QCOMPARE(m.chooseSplitRemainder("-92233720368547758.08",{part("-92233720368547758.08"),part("0")},definition),0);
        QCOMPARE(m.chooseSplitRemainder("-10",{part("invalid")},definition),-1);
        auto result=m.splitAmounts("-10",{"-0.25","ignored","-2"},true,definition,1);
        QVERIFY(result.value("balanced").toBool());QCOMPARE(result.value("amounts").toList()[1].toString(),"-7.75");
        QVERIFY(!m.splitAmounts("-10",{"-10"},true,definition,1).value("valid").toBool());
    }
    void splitAmountEditing() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"import",{{"path",fixture(dir.path(),"D1/1/2026\nT1000\nPSplits\n^\n")},{"profile","DMY"}});
        const auto batch=selected(m).value("batch");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
        auto press=[&](const QString& name){auto* control=item(name);QVERIFY(control);QVERIFY(QMetaObject::invokeMethod(control,"clicked"));};
        auto type=[&](int index,const QString& value){auto* field=item("editSplitAmount_"+QString::number(index));QVERIFY(field);field->setProperty("text",value);QVERIFY(QMetaObject::invokeMethod(field,"textEdited"));};
        auto enter=[&]{QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));};
        auto saved=[&](int index){return selected(m).value("draft").toMap().value("splits").toList()[index].toMap().value("amount").toString();};
        press("addSplit");QTRY_VERIFY(item("editSplitAmount_1"));type(1,"(1200/3)-100");enter();QCOMPARE(saved(0),"700.00");QCOMPARE(saved(1),"300.00");
        press("addSplit");QTRY_VERIFY(item("editSplitAmount_2"));type(2,"300*3");enter();QCOMPARE(saved(0),"-200.00");
        // A manual first amount prevents saving a mismatch; Keep editing retains it.
        type(0,"0.20");enter();QVERIFY(root->property("dirty").toBool());QCOMPARE(saved(0),"-200.00");
        QVERIFY(!window->close());auto* dialog=root->findChild<QObject*>("splitMismatchDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        press("splitKeepEditing");QVERIFY(window->isVisible());QCOMPARE(item("editSplitAmount_0")->property("text").toString(),"0.20");QVERIFY(root->property("dirty").toBool());
        // Reconcile manually and persist without changing the total.
        type(1,"99.80");enter();QCOMPARE(saved(0),"0.20");QVERIFY(selected(m).value("draft").toMap().value("manualSplitAmounts").toBool());
        run(m,"close");run(m,"open",{{"path",path}});run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QVERIFY(root->property("manualSplitAmounts").toBool());run(m,"commit");
        press("addSplit");QTRY_VERIFY(item("editSplitAmount_3"));type(3,"12");enter();QCOMPARE(saved(0),"0.20");QCOMPARE(selected(m).value("draft").toMap().value("splits").toList().size(),3);
        // Closing the editor asks to change the total, then saves and closes it.
        QVERIFY(QMetaObject::invokeMethod(root,"toggleRecord",Q_ARG(QVariant,batch),Q_ARG(QVariant,0)));
        QTRY_VERIFY(dialog->property("visible").toBool());press("splitChangeAmount");QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),"1,012.00");QCOMPARE(saved(0),"0.20");QVERIFY(root->property("editorClosed").toBool());
        run(m,"undo_edit");QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),"1,000.00");QCOMPARE(saved(0),"0.20");QVERIFY(root->property("manualSplitAmounts").toBool());
        // Exact arithmetic, including a transient sum larger than int64.
        const auto definition=selected(m).value("draft").toMap().value("currencyDefinition").toString();
        auto result=m.splitAmounts("1000",{"0","1100"},true,definition);QVERIFY(result.value("valid").toBool());QCOMPARE(result.value("amounts").toList()[0].toString(),"-100.00");
        result=m.splitAmounts("92233720368547758.07",{"0","92233720368547758.07","0.01"},true,definition);QVERIFY(result.value("valid").toBool());QCOMPARE(result.value("amounts").toList()[0].toString(),"-0.01");
        QVERIFY(!m.splitAmounts("0",{"0","92233720368547758.07","0.02"},true,definition).value("valid").toBool());
        QVERIFY(!m.splitAmounts("1000",{"0","1.001"},true,definition).value("valid").toBool());
    }
    void immediateEditor() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);run(m,"opening",{{"date","2025-12-31"},{"amount","0.00"}});
        QByteArray bytes;for(int i=0;i<100;++i)bytes+="D1/1/2026\nT-10\nPRow "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});const auto batch=selected(m).value("batch").toString();
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");auto* editor=root->findChild<QQuickItem*>("transactionEditor");auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(ledger && editor && payee);
        QVERIFY(QTest::qWaitForWindowExposed(window));run(m,"inspect",{{"batch",batch},{"ordinal",40}});QTest::qWait(100);
        QTRY_VERIFY(editor->property("inlineRow").toBool());const auto originalParent=editor->parentItem();
        run(m,"select",{{"row",m.occurrenceIndex(batch,41)}});
        QCOMPARE(editor->parentItem(),originalParent);QVERIFY(editor->property("inlineRow").toBool());
        QCOMPARE(selected(m).value("ordinal").toInt(),40);
        // Toggle the same row closed and open using actual double-clicks.
        auto* row=originalParent->parentItem();const auto point=row->mapToScene(QPointF(100,17)).toPoint();
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,point);QTRY_VERIFY(!m.busy());QTRY_VERIFY(!editor->isVisible());QCOMPARE(row->height(),34.0);
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,point);QTRY_VERIFY(!m.busy());QTRY_VERIFY(editor->isVisible());QTRY_VERIFY(editor->property("inlineRow").toBool());
        payee->setProperty("text","Row 40 Stefan");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));payee->forceActiveFocus();payee->setProperty("cursorPosition",13);
        const auto y=editor->mapToScene(QPointF(0,0)).y();QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());
        QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QTRY_COMPARE(selected(m).value("draft").toMap().value("payee").toString(),QString("Row 40 Stefan"));
        QVERIFY(m.state().value("canUndoEdit").toBool());run(m,"check_file");QVERIFY(m.state().value("canUndoEdit").toBool());
        QTRY_VERIFY(payee->hasActiveFocus());QTRY_VERIFY(qAbs(editor->mapToScene(QPointF(0,0)).y()-y)<2);QCOMPARE(payee->property("cursorPosition").toInt(),13);
        QCOMPARE(m.state().value("selectionIndex").toInt(),m.occurrenceIndex(batch,40));QVERIFY(m.state().value("canUndoEdit").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_EDITOR_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_EDITOR_SCREENSHOT")));
        payee->setProperty("text","Second edit");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());
        QTest::keySequence(window,QKeySequence::Undo);QTRY_VERIFY(!m.busy());QTRY_COMPARE(payee->property("text").toString(),QString("Row 40 Stefan"));
        QTest::keySequence(window,QKeySequence::Undo);QTRY_VERIFY(!m.busy());QTRY_COMPARE(payee->property("text").toString(),QString("Row 40"));QVERIFY(!m.state().value("canUndoEdit").toBool());
        auto* total=root->findChild<QQuickItem*>("durableTotal");QVERIFY(total);total->setProperty("text","-11.001");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));total->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY(!root->property("splitEditMessage").toString().isEmpty());QVERIFY(root->property("dirty").toBool());QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),QString("-10.00"));
        QTRY_VERIFY(total->hasActiveFocus());QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!root->property("dirty").toBool());QCOMPARE(total->property("text").toString(),QString("-10.00"));
        payee->setProperty("text","Persistent edit");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));payee->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());
        run(m,"close");run(m,"open",{{"path",path}});run(m,"inspect",{{"batch",batch},{"ordinal",40}});
        QCOMPARE(payee->property("text").toString(),QString("Persistent edit"));QVERIFY(!m.state().value("canUndoEdit").toBool());
        run(m,"commit");payee->setProperty("text","Committed edit");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));payee->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),QString("Committed edit"));
        run(m,"undo_edit");QCOMPARE(payee->property("text").toString(),QString("Persistent edit"));
        auto* date=root->findChild<QQuickItem*>("durableDate");QVERIFY(date);date->setProperty("text","2026-02-01");QVERIFY(QMetaObject::invokeMethod(date,"textEdited"));date->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QTRY_VERIFY(date->hasActiveFocus());QTRY_VERIFY(date->mapToItem(ledger,QPointF(0,0)).y()>=0 && date->mapToItem(ledger,QPointF(0,date->height())).y()<=ledger->height());QCOMPARE(selected(m).value("ordinal").toInt(),40);
    }
    void ledgerInteractions() {
        QTemporaryDir dir;ImportModel m(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&m);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        const auto path=dir.path()+"/doc";create(m,path);
        run(m,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        const auto source=fixture(dir.path(),"D1/5/2026\nT-10.00\nPSynthetic\n^\n");
        run(m,"import",{{"path",source},{"profile","DMY"}});
        // This interaction fixture uses chronological row positions.
        run(m,"query",{{"sort",0},{"descending",false}});
        const auto batch=selected(m).value("batch").toString();
        QCOMPARE(m.state().value("committed").toString(),QString("100.00"));
        QCOMPARE(m.state().value("projected").toString(),QString("90.00"));
        QVariantMap checking;
        for(const auto& value:m.state().value("accounts").toList()) if(value.toMap().value("id").toString()=="1") checking=value.toMap();
        QVERIFY(!checking.isEmpty());
        QCOMPARE(checking.value("balance").toString(),QString("90.00"));
        QCOMPARE(m.state().value("balance").toString(),QString("90.00"));
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto shown=[&](const char* name){return visualItem(window->contentItem(),QString::fromLatin1(name));};
        QQuickItem* balance=nullptr;
        QTRY_VERIFY(balance=shown("accountBalance_1"));
        QTRY_COMPARE(balance->property("text").toString(),QString("Balance CAD 90.00"));
        QVERIFY(!root->findChild<QQuickItem*>("projectedLedger"));
        run(m,"create_account",{{"id","2"},{"name","Japan"},{"currency","JPY"},{"date","2026-01-01"}});
        QCOMPARE(m.state().value("accountId").toString(),QString("2"));
        checking=QVariantMap{};
        for(const auto& value:m.state().value("accounts").toList()) if(value.toMap().value("id").toString()=="1") checking=value.toMap();
        QCOMPARE(checking.value("balance").toString(),QString("90.00"));
        QTRY_VERIFY(balance=shown("accountBalance_1"));
        QTRY_COMPARE(balance->property("text").toString(),QString("Balance CAD 90.00"));
        run(m,"select_account",{{"id","1"}});
        QTRY_VERIFY(balance=shown("accountBalance_1"));
        QTRY_COMPARE(balance->property("text").toString(),QString("Balance CAD 90.00"));
        const int draftRow=m.occurrenceIndex(batch,0);QVERIFY(draftRow>=0);
        run(m,"select",{{"row",draftRow}});
        auto* payee=root->findChild<QQuickItem*>("durablePayee");QVERIFY(payee);
        payee->setProperty("text","Unsaved bulk");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(root->property("dirty").toBool());
        auto* bulkDialog=root->findChild<QObject*>("bulkDialog");auto* reapplyDialog=root->findChild<QObject*>("reapplyDialog");
        QVERIFY(bulkDialog && reapplyDialog);
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("deleteSelectedContext"),"triggered"));
        auto* unsaved=root->findChild<QObject*>("unsavedDialog");QVERIFY(unsaved);QTRY_VERIFY(unsaved->property("visible").toBool());
        QVERIFY(!bulkDialog->property("visible").toBool());QVERIFY(!reapplyDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardStay"),"clicked"));
        QTRY_VERIFY(!unsaved->property("visible").toBool());
        QCOMPARE(root->property("currentPage").toString(),QString("ledger"));QVERIFY(root->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("rulesNavigation"),"clicked"));
        QTRY_VERIFY(unsaved->property("visible").toBool());
        QVERIFY(!bulkDialog->property("visible").toBool());QVERIFY(!reapplyDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("guardStay"),"clicked"));
        QTRY_VERIFY(!unsaved->property("visible").toBool());
        QCOMPARE(root->property("currentPage").toString(),QString("ledger"));
        QVERIFY(QMetaObject::invokeMethod(root,"loadEditor"));QVERIFY(!root->property("dirty").toBool());
        auto* dateCol=shown("ledgerColumn_0");
        auto* payeeCol=shown("ledgerColumn_1");
        auto* categoryCol=shown("ledgerColumn_2");
        auto* withdrawalCol=shown("ledgerColumn_3");
        auto* depositCol=shown("ledgerColumn_4");
        auto* balanceCol=shown("ledgerColumn_5");
        QVERIFY(dateCol && payeeCol && categoryCol && withdrawalCol && depositCol && balanceCol);
        QTRY_VERIFY(payeeCol->width()>dateCol->width());
        QTRY_VERIFY(payeeCol->width()>categoryCol->width());
        QTRY_VERIFY(payeeCol->width()>withdrawalCol->width());
        QTRY_VERIFY(payeeCol->width()>depositCol->width());
        QTRY_VERIFY(payeeCol->width()>balanceCol->width());
        run(m,"commit");
        QByteArray rows;
        for(int i=0;i<40;++i) rows+="D2/1/2026\nT-1.00\nPRow "+QByteArray::number(i)+"\n^\n";
        run(m,"import",{{"path",fixture(dir.path(),rows)},{"profile","DMY"}});run(m,"commit");
        const int last=m.rowCount()-1;QVERIFY(last>40);
        run(m,"select",{{"row",last}});run(m,"select",{{"row",last-1},{"range",true}});
        QCOMPARE(m.state().value("selectionCount").toInt(),2);
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto* bar=root->findChild<QQuickItem*>("ledgerScrollBar");QVERIFY(bar);QTRY_VERIFY(bar->isVisible());QVERIFY(bar->width()>=16);QVERIFY(bar->property("interactive").toBool());
        const auto oldPosition=bar->property("position").toDouble();
        const auto thumb=bar->property("visualPosition").toDouble()+bar->property("visualSize").toDouble()/2;
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,bar->mapToScene(QPointF(bar->width()/2,bar->height()*thumb)).toPoint());
        const double destination=oldPosition>0.3?0.15:0.85;
        QTest::mouseMove(window,bar->mapToScene(QPointF(bar->width()/2,bar->height()*destination)).toPoint(),30);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,bar->mapToScene(QPointF(bar->width()/2,bar->height()*destination)).toPoint());
        QTRY_VERIFY(bar->property("position").toDouble()!=oldPosition);

        auto visibleRow=[&]{
            int index=-1;const double y=ledger->property("contentY").toDouble();
            if(!QMetaObject::invokeMethod(ledger,"indexAt",Q_RETURN_ARG(int,index),Q_ARG(double,1.0),Q_ARG(double,y+1))) return -1;
            return index;
        };
        QTest::qWait(100);ledger->setProperty("contentY",ledger->property("originY").toDouble()+450);QTest::qWait(50);
        QVERIFY(ledger->property("contentY").toDouble()>300);
        const int anchorRow=visibleRow();QVERIFY2(anchorRow>2,qPrintable(QString("anchor %1 y %2 origin %3 count %4").arg(anchorRow).arg(ledger->property("contentY").toDouble()).arg(ledger->property("originY").toDouble()).arg(ledger->property("count").toInt())));
        const auto anchorId=m.rowIdentity(anchorRow);QVERIFY(!anchorId.isEmpty());
        const auto sameAnchor=[&]{return visibleRow()==m.occurrenceIndex(anchorId.value("batch").toString(),anchorId.value("ordinal").toInt());};
        QVERIFY(QMetaObject::invokeMethod(root,"chooseLedgerCategory",Q_ARG(QVariant,(QVariantMap{{"id",""},{"path","Uncategorized"}}))));
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(),30000);QVERIFY2(m.error().isEmpty(),qPrintable(m.error()));
        QTRY_VERIFY(!bulkDialog->property("visible").toBool());
        QTRY_VERIFY2(sameAnchor(),qPrintable(QString("apply row %1 y %2").arg(visibleRow()).arg(ledger->property("contentY").toDouble())));
        QVERIFY(ledger->property("contentY").toDouble()>0);
        window->resize(720,560);QTest::qWait(100);
        QCOMPARE(window->width(),720);QCOMPARE(window->height(),560);
        for(const char* name:{"sidebarToggle","navigationBack","navigationForward"}) {
            auto* item=root->findChild<QQuickItem*>(name);QVERIFY(item);QVERIFY(item->isVisible());
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
            QVERIFY2(point.x()>=0 && point.x()<window->width() && point.y()>=0 && point.y()<window->height(),name);
        }
        auto* scroll=root->findChild<QQuickItem*>("ledgerScroll");QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentWidth").toDouble()>=1080);
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    ImportUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_model_tests.moc"
