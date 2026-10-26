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
// Inject lost acknowledgement after real storage has durably applied the operation.
class ChangeFaultGateway final : public app::DocumentGateway, public app::ImportRepository {
    std::unique_ptr<app::DocumentGateway> inner=storage::document_gateway();
    app::ImportRepository& repo() const {return inner->repository();}
public:
    bool loseAcknowledgement=false,unknownOutcome=false,failBefore=false,failRefresh=false,refreshBroken=false;
    void create(const std::filesystem::path& p,bool mode,std::string_view pw,std::string_view name,const Currency& c,std::string_view day) override {inner->create(p,mode,pw,name,c,day);}
    void open(const std::filesystem::path& p,std::optional<std::string> pw) override {refreshBroken=false;inner->open(p,std::move(pw));}
    void open_controlled(const std::filesystem::path& p,std::optional<std::string> pw,const app::OperationContext& c) override {refreshBroken=false;inner->open_controlled(p,std::move(pw),c);}
    void close() override {inner->close();}
    void backup(const std::filesystem::path& p) override {inner->backup(p);}
    app::ImportRepository& repository() override {return *this;}
    app::Account account() const override {return repo().account();}
    void opening(std::int64_t r,std::string_view d,const Money& m) override {repo().opening(r,d,m);}
    std::vector<app::BatchInfo> batches() const override {return repo().batches();}
    app::BatchInfo ingest(std::string_view id,std::string_view b,qif::DateOrder p) override {return repo().ingest(id,b,p);}
    app::BatchInfo ingest_controlled(std::string_view id,std::string_view b,qif::DateOrder p,app::OperationContext& c) override {return repo().ingest_controlled(id,b,p,c);}
    app::BatchInfo resume(std::string_view id,std::int64_t r,app::OperationContext& c) override {return repo().resume(id,r,c);}
    std::string evidence(std::string_view b) const override {return repo().evidence(b);}
    app::RowView row(const imports::Occurrence& o) const override {return repo().row(o);}
    void save(const imports::Occurrence& o,std::int64_t r,const imports::DraftRow& d) override {repo().save(o,r,d);}
    void decide(const imports::Occurrence& o,std::int64_t r,const imports::Decision& d) override {repo().decide(o,r,d);}
    void discard(std::string_view b,std::int64_t r) override {repo().discard(b,r);}
    app::Receipt commit(std::string_view b,std::int64_t r,std::string_view op) override {return repo().commit(b,r,op);}
    std::optional<app::Receipt> outcome(std::string_view op) const override {return repo().outcome(op);}
    std::optional<app::Receipt> durable_outcome(std::string_view op) const override {return repo().durable_outcome(op);}
    std::vector<app::LedgerRow> ledger(bool p) const override {return repo().ledger(p);}
    Money committed_balance() const override {return repo().committed_balance();}
    std::optional<Money> projected_balance() const override {return repo().projected_balance();}
    bool committed_changes_supported() const override {return repo().committed_changes_supported();}
    app::CommittedEntry committed_entry(const imports::Occurrence& id) const override {if(refreshBroken)throw std::runtime_error("Injected post-save refresh failure");return repo().committed_entry(id);}
    std::vector<app::CommittedHistoryItem> committed_history(const imports::Occurrence& id) const override {return repo().committed_history(id);}
    std::vector<app::CommittedChangeSummary> committed_changes(std::int64_t before,std::size_t limit) const override {return repo().committed_changes(before,limit);}
    app::ChangePreview preview_committed_change(const app::ChangeRequest& r) const override {return repo().preview_committed_change(r);}
    app::ChangeReceipt apply_committed_change(std::string_view op,const app::ChangeRequest& r,const app::EntryToken& t,std::int64_t sequence) override {
        if(failBefore)throw std::runtime_error("Injected failure before apply");
        auto result=repo().apply_committed_change(op,r,t,sequence);refreshBroken=failRefresh;if(loseAcknowledgement)throw std::runtime_error("Injected lost acknowledgement");return result;
    }
    app::ChangeReceipt apply_committed_change_with_catalog(std::string_view op,const app::ChangeRequest& r,const app::EntryToken& t,std::int64_t sequence,std::int64_t catalog) override {
        if(failBefore)throw std::runtime_error("Injected failure before apply");
        auto result=repo().apply_committed_change_with_catalog(op,r,t,sequence,catalog);refreshBroken=failRefresh;if(loseAcknowledgement)throw std::runtime_error("Injected lost acknowledgement");return result;
    }
    std::optional<app::ChangeReceipt> committed_change_outcome(std::string_view op) const override {
        if(unknownOutcome)throw std::runtime_error("Injected unavailable outcome");return repo().committed_change_outcome(op);
    }
};
class CommittedUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QQuickItem* visual(QQuickItem* item,const QString& name) {
        if(item->objectName()==name)return item;
        for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;
        return nullptr;
    }
    static QObject* object(QObject* root,const QString& name) {
        if(auto* item=root->findChild<QObject*>(name))return item;
        if(auto* window=qobject_cast<QQuickWindow*>(root))return visual(window->contentItem(),name);
        return nullptr;
    }
    static bool click(QObject* root,const QString& name) {auto* item=object(root,name);return item && item->property("enabled").toBool() && item->property("visible").toBool() && QMetaObject::invokeMethod(item,"clicked");}
    static bool pointerClick(QQuickWindow* window,QObject* root,const QString& name) {
        auto* item=qobject_cast<QQuickItem*>(object(root,name));if(!item || !item->isEnabled() || !item->isVisible())return false;
        const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();
        if(!QRect(0,0,window->width(),window->height()).contains(position))return false;
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,position);return true;
    }
    static bool edit(QObject* root,const char* name,const QString& value) {auto* item=object(root,QString::fromLatin1(name));return item && item->setProperty("text",value) && QMetaObject::invokeMethod(item,"textEdited");}
    void wait(ImportModel& model,bool success=true) {QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty()==success,qPrintable(model.error()));}
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {model.command(action,args);wait(model,success);}
    static QVariantMap selected(const ImportModel& model){return model.state().value("selected").toMap();}
    QString prepare(ImportModel& model,const QString& dir) {
        run(model,"create",{{"path",dir+"/document"},{"protected",false},{"name","Synthetic corrections"},{"currency","CAD"}});
        run(model,"opening",{{"date","2026-01-01"},{"amount","100.00"}});
        QFile file(dir+"/source.qif");if(!file.open(QIODevice::WriteOnly))qFatal("Cannot write synthetic fixture");
        const QByteArray source="D1/1/2026\nT-29.31\nPSynthetic purchase\nSFees\n$-0.14\nSTransport\n$-29.17\n^\nD2/1/2026\nT10.00\nPSynthetic deposit\n^\n";
        if(file.write(source)!=source.size())qFatal("Cannot write synthetic fixture");file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});run(model,"commit");
        return file.fileName();
    }
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("CommittedUi");QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    }
    void init(){QSettings().clear();}
    void correctionGuardsRemovalAndUndo() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        prepare(model,dir.path());const auto id=model.rowIdentity(model.selectedRowIndex());
        QVERIFY(model.state().value("committedChangesSupported").toBool());QCOMPARE(model.state().value("committed").toString(),"80.69");
        QVERIFY(selected(model).value("committed").toBool());QVERIFY(!selected(model).value("editable").toBool());
        QVERIFY(root->findChild<QObject*>("durableTotal")->property("readOnly").toBool());
        QTest::qWait(100);QVERIFY(pointerClick(window,root,"editCommitted"));QVERIFY(root->property("committedEditing").toBool());
        QVERIFY(edit(root,"durableTotal","-30.00"));QVERIFY(edit(root,"editSplitAmount_1","-29.86"));
        QVERIFY(click(root,"saveDraft"));wait(model);
        auto* preview=root->findChild<QObject*>("committedChangePreview");QVERIFY(preview);QVERIFY(preview->property("visible").toBool());
        QCOMPARE(root->findChild<QObject*>("durableTotal")->property("text").toString(),"-30.00");QVERIFY(root->property("dirty").toBool());
        QCOMPARE(model.state().value("committed").toString(),"80.69");
        QVERIFY(click(root,"cancelCommittedChange"));wait(model);QVERIFY(root->property("dirty").toBool());
        QCOMPARE(root->findChild<QObject*>("durableTotal")->property("text").toString(),"-30.00");
        QVERIFY(click(root,"saveDraft"));wait(model);
        QTest::qWait(100);if(qEnvironmentVariableIsSet("SPENTIT_CORRECTION_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CORRECTION_SCREENSHOT")));
        QVERIFY(pointerClick(window,root,"applyCommittedChange"));wait(model);QCOMPARE(model.state().value("committed").toString(),"80.00");
        const auto correction=model.state().value("changeReceipt").toMap().value("operation").toString();QVERIFY(!correction.isEmpty());
        QCOMPARE(selected(model).value("original").toString(),"-29.31");QVERIFY(!root->property("dirty").toBool());
        QVERIFY(click(root,"editCommitted"));QVERIFY(edit(root,"durablePayee","Corrected merchant"));
        QVERIFY(click(root,"settingsNavigation"));QVERIFY(click(root,"guardSave"));wait(model);QVERIFY(preview->property("visible").toBool());
        QVERIFY(click(root,"cancelCommittedChange"));wait(model);QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"settingsNavigation"));QVERIFY(click(root,"guardSave"));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model);
        QCOMPARE(root->property("currentPage").toString(),"settings");QVERIFY(!root->property("dirty").toBool());
        QVERIFY(click(root,"navigationBack"));QVERIFY(click(root,"showEntryHistory"));wait(model);
        bool blocked=false;for(const auto& value:model.state().value("entryHistory").toList()) {const auto h=value.toMap();if(h.value("operation").toString()==correction)blocked=!h.value("undoEligible").toBool() && !h.value("undoReason").toString().isEmpty();}QVERIFY(blocked);
        QVERIFY(click(root,"accountNavigation"));QVERIFY(click(root,"removeCommitted"));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model);
        QCOMPARE(model.state().value("committed").toString(),"110.00");QVERIFY(selected(model).value("removed").toBool());QCOMPARE(model.selectedRowIndex(),-1);
        const auto removal=model.state().value("changeReceipt").toMap().value("operation").toString();
        QVERIFY(click(root,"showEntryHistory"));wait(model);QVERIFY(!model.state().value("history").toList().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(root,"revealHistoryOperation",Q_ARG(QVariant,removal)));QTest::qWait(100);
        QTRY_VERIFY(object(root,"undo_"+removal));QVERIFY(click(root,"undo_"+removal));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model);
        QCOMPARE(model.state().value("committed").toString(),"80.00");QVERIFY(!selected(model).value("removed").toBool());
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
        run(model,"backup",{{"path",dir.path()+"/backup"}});run(model,"close");run(model,"open",{{"path",dir.path()+"/backup"}});run(model,"inspect",id);
        QCOMPARE(model.state().value("committed").toString(),"80.00");QCOMPARE(selected(model).value("original").toString(),"-29.31");
        run(model,"entry_history",id);QVERIFY(model.state().value("entryHistory").toList().size()>=5);
    }
    void staleBalancePreviewDoesNotOverwrite() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());prepare(model,dir.path());
        const auto original=model.rowIdentity(model.selectedRowIndex());
        QFile incoming(dir.path()+"/later.qif");QVERIFY(incoming.open(QIODevice::WriteOnly));
        const QByteArray bytes="D3/1/2026\nT5.00\nPUnrelated later movement\n^\n";QCOMPARE(incoming.write(bytes),bytes.size());incoming.close();
        run(model,"import",{{"path",incoming.fileName()},{"profile","DMY"}});run(model,"inspect",original);
        auto change=selected(model).value("draft").toMap();change["kind"]="correct";change["payee"]="Stale proposal";
        run(model,"preview_change",change);const auto preview=model.state().value("changePreview").toMap().value("id");
        run(model,"commit");QCOMPARE(model.state().value("committed").toString(),"85.69");
        run(model,"apply_change",{{"preview",preview}},false);
        QCOMPARE(model.state().value("committed").toString(),"85.69");
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Synthetic purchase");
        run(model,"history");QVERIFY(model.state().value("history").toList().isEmpty());
    }
    void lostAcknowledgementAndQuarantine() {
        QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));prepare(model,dir.path());
        const auto id=model.rowIdentity(model.selectedRowIndex());
        auto change=selected(model).value("draft").toMap();change["kind"]="correct";change["payee"]="Recovered acknowledgement";
        run(model,"preview_change",change);fault->loseAcknowledgement=true;
        run(model,"apply_change",{{"preview",model.state().value("changePreview").toMap().value("id")}});
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Recovered acknowledgement");
        change["payee"]="Saved despite unknown outcome";run(model,"preview_change",change);fault->unknownOutcome=true;
        run(model,"apply_change",{{"preview",model.state().value("changePreview").toMap().value("id")}},false);
        QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("uncertain"));
        fault->unknownOutcome=false;fault->loseAcknowledgement=false;run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Saved despite unknown outcome");
        run(model,"history");QCOMPARE(model.state().value("history").toList().size(),2);QCOMPARE(model.state().value("committed").toString(),"80.69");
    }
    void failedApplyRetainsBufferAndKnownSaveIsReported() {
        QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        prepare(model,dir.path());const auto id=model.rowIdentity(model.selectedRowIndex());
        QVERIFY(click(root,"editCommitted"));QVERIFY(edit(root,"durablePayee","Preserved proposal"));QVERIFY(click(root,"saveDraft"));wait(model);
        fault->failBefore=true;QVERIFY(click(root,"applyCommittedChange"));wait(model,false);
        QVERIFY(root->property("dirty").toBool());QCOMPARE(root->findChild<QObject*>("durablePayee")->property("text").toString(),"Preserved proposal");
        QVERIFY(root->findChild<QObject*>("committedChangePreview")->property("visible").toBool());QCOMPARE(model.state().value("committed").toString(),"80.69");
        QVERIFY(click(root,"cancelCommittedChange"));wait(model);fault->failBefore=false;fault->failRefresh=true;
        QVERIFY(click(root,"saveDraft"));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model,false);
        QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("saved durably"));QVERIFY(!root->findChild<QObject*>("committedChangePreview")->property("visible").toBool());QVERIFY(!model.state().value("changeReceipt").toMap().value("operation").toString().isEmpty());
        fault->failRefresh=false;run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Preserved proposal");run(model,"history");QCOMPARE(model.state().value("history").toList().size(),1);
    }
    void removedReimportRequiresReview() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();const auto file=prepare(model,dir.path());
        QVERIFY(click(root,"removeCommitted"));wait(model);QVERIFY(click(root,"applyCommittedChange"));wait(model);QCOMPARE(model.state().value("committed").toString(),"110.00");
        run(model,"import",{{"path",file},{"profile","DMY"}});QCOMPARE(selected(model).value("decision").toInt(),0);
        const auto candidates=selected(model).value("candidates").toList();QVERIFY(!candidates.isEmpty());QVERIFY(candidates.front().toMap().value("removed").toBool());
        auto* chooser=root->findChild<QObject*>("duplicateCandidate");QVERIFY(chooser);chooser->setProperty("currentIndex",0);
        QCOMPARE(root->findChild<QObject*>("excludeDuplicate")->property("text").toString(),"Keep removed");
        QVERIFY(click(root,"excludeDuplicate"));wait(model);QCOMPARE(selected(model).value("decision").toInt(),2);
        QCOMPARE(model.state().value("projected").toString(),"110.00");run(model,"commit");QCOMPARE(model.state().value("committed").toString(),"110.00");
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");CommittedUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "committed_ui_tests.moc"
