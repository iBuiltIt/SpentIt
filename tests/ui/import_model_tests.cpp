#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
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
#include <sqlite3.h>
using namespace spentit;
// Delegates every real storage operation; only caller acknowledgement and
// outcome-query failures are injected. These tests never replace durability.
class OutcomeGateway final : public app::DocumentGateway, public app::ImportRepository {
public:
    std::unique_ptr<app::DocumentGateway> inner=storage::document_gateway();
    enum Fault { none, before, after, unknown } fault=none;
    bool attempted=false,cancelAfterOpen=false;
    app::ImportRepository& repo() const { return inner->repository(); }
    void create(const std::filesystem::path& p,bool mode,std::string_view pw,std::string_view name,const Currency& c,std::string_view day) override {inner->create(p,mode,pw,name,c,day);}
    void open(const std::filesystem::path& p,std::optional<std::string> pw) override {inner->open(p,std::move(pw));}
    void open_controlled(const std::filesystem::path& p,std::optional<std::string> pw,const app::OperationContext& c) override {inner->open_controlled(p,std::move(pw),c);if(cancelAfterOpen)c.request();}
    void close() override {inner->close();}
    void backup(const std::filesystem::path& p) override {inner->backup(p);}
    app::ImportRepository& repository() override {return *this;}
    app::Account account() const override {return repo().account();}
    void opening(std::int64_t r,std::string_view d,const Money& m) override {repo().opening(r,d,m);}
    std::vector<app::BatchInfo> batches() const override {return repo().batches();}
    app::BatchInfo ingest(std::string_view id,std::string_view b,qif::DateOrder p) override {return repo().ingest(id,b,p);}
    std::string evidence(std::string_view b) const override {return repo().evidence(b);}
    app::RowView row(const imports::Occurrence& o) const override {return repo().row(o);}
    void save(const imports::Occurrence& o,std::int64_t r,const imports::DraftRow& d) override {repo().save(o,r,d);}
    void decide(const imports::Occurrence& o,std::int64_t r,const imports::Decision& d) override {repo().decide(o,r,d);}
    void discard(std::string_view b,std::int64_t r) override {repo().discard(b,r);}
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
    Money committed_balance() const override {return repo().committed_balance();}
    std::optional<Money> projected_balance() const override {return repo().projected_balance();}
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
        model.command(action,args);QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty()==success,qPrintable(action+": "+model.error()));
    }
    void create(ImportModel& m,const QString& path) {
        run(m,"create",{{"path",path},{"protected",false},{"name","Synthetic test"},{"currency","CAD"}});
    }
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("ImportUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    }
    void init() {QSettings().clear();}
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
        for(const auto phase:{"Opening dataset","Verifying dataset integrity","Verifying import evidence","Verifying category history","Verifying committed change history"}) {
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
            if(fault==OutcomeGateway::after) {QCOMPARE(m.state().value("receipt").toMap().value("change").toString(),"1.00");QCOMPARE(m.state().value("committed").toString(),"1.00");}
            if(fault==OutcomeGateway::unknown) {QVERIFY(!m.state().value("active").toBool());QVERIFY(m.error().contains("uncertain"));}
            injected->fault=OutcomeGateway::none;
            if(m.state().value("active").toBool())run(m,"close");
            run(m,"open",{{"path",path}});
            if(fault==OutcomeGateway::before) run(m,"commit");
            QCOMPARE(m.state().value("committed").toString(),"1.00");QCOMPARE(m.rowCount(),2);
            QCOMPARE(m.state().value("receipt").toMap().value("included").toInt(),1);
        }
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
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");QCOMPARE(m.state().value("receipt").toMap().value("change").toString(),"20.00");
        QCOMPARE(m.state().value("receipt").toMap().value("included").toInt(),2);
        QCOMPARE(m.state().value("receipt").toMap().value("excluded").toInt(),0);
        QVERIFY(m.state().value("frozen").toBool());
        run(m,"opening",{{"date","2026-08-01"},{"amount","200.00"}},false);
        QCOMPARE(m.state().value("committed").toString(),"120.00");
        run(m,"close");run(m,"open",{{"path",path}});
        QCOMPARE(m.state().value("receipt").toMap().value("change").toString(),"20.00");
        run(m,"import",{{"path",source},{"profile","DMY"}});QCOMPARE(selected(m).value("decision").toInt(),2);
        run(m,"commit");QCOMPARE(m.state().value("committed").toString(),"120.00");
        QCOMPARE(m.state().value("receipt").toMap().value("included").toInt(),0);
        QCOMPARE(m.state().value("receipt").toMap().value("excluded").toInt(),2);
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
        const auto invalid=fixture(dir.path(),"D1/1/2026\nT1\nL[Other]\n^\n");run(m,"import",{{"path",invalid},{"profile","DMY"}});
        QVERIFY(!m.state().value("projectionAvailable").toBool());QCOMPARE(m.rowCount(),0);run(m,"commit",{},false);
        QVERIFY(!selected(m).value("diagnostic").toString().isEmpty());run(m,"discard");QCOMPARE(m.state().value("committed").toString(),"0.00");
        run(m,"import",{{"path",fixture(dir.path(),QByteArray(1,static_cast<char>(0xff)))},{"profile","DMY"}});
        QVERIFY(selected(m).value("source").toString().contains("hex"));QVERIFY(!selected(m).contains("draft"));run(m,"discard");
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
        QVERIFY(!unsupported.state().value("unlockRequired").toBool());QVERIFY(unsupported.error().contains("Unsupported document schema"));
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
            run(m,"query",{{"projected",false}});QCOMPARE(m.rowCount(),1);
            QCOMPARE(m.state().value("deposits").toString(),"100.00");
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
        run(m,"discard");QCOMPARE(m.state().value("projected").toString(),"209.00");
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
        QTest::qWait(100);ledger->setProperty("contentY",0);QTest::qWait(100);
        const auto point=ledger->mapToScene(QPointF(100,17)).toPoint();
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,point);
        QTRY_VERIFY(!m.busy());QCOMPARE(selected(m).value("ordinal").toInt(),0);
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
        QCOMPARE(m.state().value("sort").toInt(),0);QVERIFY(!m.state().value("descending").toBool());
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
        QVERIFY(click("clearFilter"));QVERIFY(click("guardSave"));QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());
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
        run(m,"query",{{"projected",true},{"filter","Row 0"},{"sort",1},{"descending",true}});
        QTRY_VERIFY(item("splitPreview_0") && item("splitPreview_0")->height()>0);
        QCOMPARE(payee->property("text").toString(),"Unsaved editor");
        run(m,"query",{{"projected",true},{"filter","Row 1"}});
        QTRY_VERIFY(item("splitPreview_1") && item("splitPreview_1")->height()>0);
        QVERIFY(QMetaObject::invokeMethod(item("splitToggle_1"),"clicked"));
        QTRY_COMPARE(item("splitPreview_1")->height(),0);
        // Closing clears view-only identities, including when the next dataset uses the same ordinals.
        run(m,"close");create(m,dir.path()+"/other");
        run(m,"import",{{"path",fixture(dir.path(),bytes)},{"profile","DMY"}});
        run(m,"inspect",{{"batch",selected(m).value("batch")},{"ordinal",79}});
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));QTest::qWait(100);
        QTRY_VERIFY(item("splitPreview_0"));QCOMPARE(item("splitPreview_0")->height(),0);
    }
    void inlineSourceCopy() {
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
        auto splits=edit.value("splits").toList();auto split=splits[0].toMap();split["amount"]="-11.00";splits[0]=split;edit["splits"]=splits;
        run(m,"save",edit);
        auto* toggle=root->findChild<QObject*>("sourceToggle");QVERIFY(toggle);toggle->setProperty("checked",true);
        QTRY_VERIFY(visualItem(window->contentItem(),"copySource_payee_-1"));
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"copySource_payee_-1"),"clicked"));
        QCOMPARE(payee->property("text").toString(),"Original");QVERIFY(root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Corrected");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"copySource_total_-1"),"clicked"));
        QCOMPARE(total->property("text").toString(),"-10.00");
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());
        QVERIFY(!m.error().isEmpty());QVERIFY(root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("total").toString(),"-11.00");
        QVERIFY(QMetaObject::invokeMethod(visualItem(window->contentItem(),"copySource_amount_0"),"clicked"));
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());
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
        QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());QCOMPARE(selected(m).value("ordinal").toInt(),0);
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
        QTRY_VERIFY(visualItem(window->contentItem(),"copySource_payee_-1"));
        QVERIFY(!visualItem(window->contentItem(),"copySource_payee_-1")->property("enabled").toBool());
        QTest::qWait(100);
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
        QTRY_VERIFY(!m.busy());QVERIFY(!m.error().isEmpty());QVERIFY(root->property("dirty").toBool());QVERIFY(m.state().value("active").toBool());
        total->setProperty("text","-1.00");QVERIFY(QMetaObject::invokeMethod(total,"textEdited"));
        auto* saveDraft=root->findChild<QObject*>("saveDraft");QVERIFY(saveDraft);QVERIFY(QMetaObject::invokeMethod(saveDraft,"clicked"));
        QTRY_VERIFY(!m.busy());QVERIFY(m.error().isEmpty());QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
        QTest::qWait(100);
        const auto image=window->grabWindow();QVERIFY(!image.isNull());
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_SCREENSHOT")) QVERIFY(image.save(qEnvironmentVariable("SPENTIT_TEST_SCREENSHOT")));
        payee->setProperty("text","Discard me");QVERIFY(QMetaObject::invokeMethod(payee,"textEdited"));
        QVERIFY(!window->close());auto* discard=root->findChild<QObject*>("guardDiscard");QVERIFY(discard);QVERIFY(QMetaObject::invokeMethod(discard,"clicked"));
        QTRY_VERIFY(!m.busy());QTRY_VERIFY(!window->isVisible());QVERIFY(!m.state().value("active").toBool());
        run(m,"open",{{"path",dir.path()+"/doc"}});
        const auto batch=m.state().value("pending").toMap().value("id");run(m,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(selected(m).value("draft").toMap().value("payee").toString(),"Edited through QML");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    ImportUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_model_tests.moc"
