#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include "controls_style.hpp"
#include <QFont>
#include <QJSValue>
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
    void close_verified(const app::OperationContext& c) override {inner->close_verified(c);}
    void check_file(const app::OperationContext& c) override {inner->check_file(c);}
    void backup(const std::filesystem::path& p) override {inner->backup(p);}
    app::ImportRepository& repository() override {return *this;}
    app::Account account() const override {return repo().account();}
    void opening(std::int64_t r,std::string_view d,const Money& m) override {repo().opening(r,d,m);}
    std::vector<app::BatchInfo> batches() const override {return repo().batches();}
    app::BatchInfo ingest(std::string_view id,std::string_view b,qif::DateOrder p) override {return repo().ingest(id,b,p);}
    app::BatchInfo ingest_controlled(std::string_view id,std::string_view b,qif::DateOrder p,app::OperationContext& c) override {return repo().ingest_controlled(id,b,p,c);}
    std::vector<app::ImportGroup> import_groups() const override {return repo().import_groups();}
    std::optional<app::ImportGroup> import_group_outcome(std::string_view id) const override {return repo().import_group_outcome(id);}
    app::ImportGroup prepare_import_group(std::string_view id,std::string_view bytes,qif::DateOrder order,const std::vector<app::ImportAccountMapping>& mappings,app::OperationContext& c,std::string_view label) override {return repo().prepare_import_group(id,bytes,order,mappings,c,label);}
    app::ImportGroup resume_import_group(std::string_view id,app::OperationContext& c) override {return repo().resume_import_group(id,c);}
    bool cleanup_incomplete_imports(std::string_view source={}) override {return repo().cleanup_incomplete_imports(source);}
    std::vector<app::MissingImportCategories> missing_import_categories() const override {return repo().missing_import_categories();}
    void resolve_import_categories(std::string_view source,const std::vector<std::string>& names,app::OperationContext& c) override {repo().resolve_import_categories(source,names,c);}
    app::BatchInfo resume(std::string_view id,std::int64_t r,app::OperationContext& c) override {return repo().resume(id,r,c);}
    std::string evidence(std::string_view b) const override {return repo().evidence(b);}
    app::RowView row(const imports::Occurrence& o) const override {return repo().row(o);}
    app::ReviewReceipt edit_transaction(std::string_view op,const imports::Occurrence& id,std::int64_t revision,const std::optional<app::EntryToken>& token,const imports::DraftRow& draft) override {
        if(failBefore)throw std::runtime_error("Injected failure before edit");auto result=repo().edit_transaction(op,id,revision,token,draft);refreshBroken=failRefresh;if(loseAcknowledgement)throw std::runtime_error("Injected lost acknowledgement");return result;
    }
    std::int64_t ledger_edit_generation() const override {return repo().ledger_edit_generation();}
    app::ReviewReceipt edit_transactions(std::string_view op,const app::LedgerSelectionEdit& request,const app::OperationContext& context) override {
        if(failBefore)throw std::runtime_error("Injected failure before selection edit");auto result=repo().edit_transactions(op,request,context);refreshBroken=failRefresh;if(loseAcknowledgement)throw std::runtime_error("Injected lost acknowledgement");return result;
    }
    bool can_undo_transaction_edit(std::string_view op) const override {return repo().can_undo_transaction_edit(op);}
    app::ReviewReceipt undo_transaction_edit(std::string_view op,std::string_view edit) override {return repo().undo_transaction_edit(op,edit);}
    std::optional<app::ReviewReceipt> review_outcome(std::string_view op) const override {if(unknownOutcome)throw std::runtime_error("Injected unavailable edit outcome");return repo().review_outcome(op);}
    void save(const imports::Occurrence& o,std::int64_t r,const imports::DraftRow& d) override {repo().save(o,r,d);}
    void decide(const imports::Occurrence& o,std::int64_t r,const imports::Decision& d) override {repo().decide(o,r,d);}
    void discard(std::string_view b,std::int64_t r) override {repo().discard(b,r);}
    app::AcceptanceReview acceptance_review() const override {return repo().acceptance_review();}
    app::AcceptanceReceipt accept_all(std::string_view op,bool ignore=false) override {return repo().accept_all(op,ignore);}
    std::optional<app::AcceptanceReceipt> acceptance_outcome(std::string_view op) const override {return repo().acceptance_outcome(op);}
    bool transaction_accepted(const imports::Occurrence& id) const override {return repo().transaction_accepted(id);}
    app::RollbackReview rollback_review() const override {return repo().rollback_review();}
    app::ImportResult import_result(std::string_view id) const override {return repo().import_result(id);}
    app::Receipt commit(std::string_view b,std::int64_t r,std::string_view op) override {return repo().commit(b,r,op);}
    std::optional<app::Receipt> outcome(std::string_view op) const override {return repo().outcome(op);}
    std::optional<app::Receipt> durable_outcome(std::string_view op) const override {return repo().durable_outcome(op);}
    std::vector<app::LedgerRow> ledger(bool p) const override {if(refreshBroken)throw std::runtime_error("Injected ledger refresh failure");return repo().ledger(p);}
    Money committed_balance() const override {return repo().committed_balance();}
    std::optional<Money> projected_balance() const override {return repo().projected_balance();}
    std::vector<app::AccountBalance> account_balances() const override {return repo().account_balances();}
    bool committed_changes_supported() const override {return repo().committed_changes_supported();}
    bool bulk_supported() const override {return repo().bulk_supported();}
    std::string bulk_group(std::string_view op) const override {return repo().bulk_group(op);}
    app::BulkPreview preview_bulk(const app::BulkRequest& request) const override {return repo().preview_bulk(request);}
    app::BulkReceipt apply_bulk(std::string_view op,const app::BulkPreview& preview) override {
        if(failBefore)throw std::runtime_error("Injected failure before bulk apply");
        auto result=repo().apply_bulk(op,preview);refreshBroken=failRefresh;if(loseAcknowledgement)throw std::runtime_error("Injected lost bulk acknowledgement");return result;
    }
    std::optional<app::BulkReceipt> bulk_outcome(std::string_view op) const override {if(unknownOutcome)throw std::runtime_error("Injected unavailable bulk outcome");return repo().bulk_outcome(op);}
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
    app::AccountHistoryPage account_change_history(std::int64_t account,std::int64_t before,std::size_t limit,const std::optional<imports::Occurrence>& focus) const override {
        return repo().account_change_history(account,before,limit,focus);
    }
    std::string action_display_label(const app::ActionSummary& action) const override {return repo().action_display_label(action);}
    std::vector<app::HistoryFieldChange> action_field_changes(const app::ActionSummary& action) const override {return repo().action_field_changes(action);}
    app::ConnectedUndoPreview preview_connected_undo(std::string_view id,const app::ActionToken& token) const override {return repo().preview_connected_undo(id,token);}
    app::ActionReceipt undo_connected(std::string_view operation,const app::ConnectedUndoPreview& preview) override {return repo().undo_connected(operation,preview);}
    std::optional<app::ActionReceipt> undo_connected_outcome(std::string_view operation,const app::ConnectedUndoPreview& preview) const override {return repo().undo_connected_outcome(operation,preview);}
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
    void run(ImportModel& model,const QString& action,const QVariantMap& args={},bool success=true) {auto request=args;if(action=="import" && !request.contains("account"))request["account"]=model.state().value("accountId");model.command(action,request);wait(model,success);if(success && action=="import"){for(const auto& item:model.state().value("importCategoryDecisions").toList()){const auto d=item.toMap();run(model,"resolve_import_categories",{{"source",d.value("source")},{"selected",d.value("names")}});}}}
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
    void selectedRenameRestoreAndUndo(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(QTest::qWaitForWindowExposed(window));
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);prepare(model,dir.path());
        const auto oldBatch=selected(model).value("batch");
        QFile file(dir.path()+"/new.qif");QVERIFY(file.open(QIODevice::WriteOnly));file.write("D3/1/2026\nT-2\nPSynthetic new\n^\n");file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto newBatch=selected(model).value("batch");
        run(model,"query",{{"filter","Synthetic"}});QCOMPARE(model.rowCount(),3);
        run(model,"select",{{"row",0}});run(model,"select",{{"row",2},{"range",true}});QCOMPARE(model.state().value("selectionCount").toInt(),3);
        auto open=[&](const char* action){auto* item=object(root,QString::fromLatin1(action));QVERIFY(item);QVERIFY(item->property("enabled").toBool());QVERIFY(QMetaObject::invokeMethod(item,"triggered"));QTRY_VERIFY(object(root,"selectedEditDialog")->property("visible").toBool());};
        open("renamePayeeSelection");QVERIFY(object(root,"selectedPayee")->setProperty("text","APPLE"));QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        QCOMPARE(model.rowCount(),0);QVERIFY(model.state().value("canUndoEdit").toBool());QCOMPARE(model.state().value("editUndoLabel").toString(),"Undo Rename payee…");
        run(model,"query",{{"filter","regex:\"^APPLE$\""}});QCOMPARE(model.rowCount(),3);
        open("restoreImportedSelection");QVERIFY(root->property("restoringSelection").toBool());QCOMPARE(root->property("selectedEditCount").toInt(),3);
        QVERIFY(click(root,"cancelSelectedEdit"));QCOMPARE(model.rowCount(),3);
        open("restoreImportedSelection");
        if(qEnvironmentVariableIsSet("SPENTIT_SELECTION_SCREENSHOT")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_SELECTION_SCREENSHOT")));}
        QVERIFY(click(root,"confirmSelectedEdit"));wait(model);QCOMPARE(model.rowCount(),0);
        QCOMPARE(model.state().value("editUndoLabel").toString(),"Undo Restore imported values…");
        // Native Undo shortcut reverses the entire selection even after it leaves the filter.
        qobject_cast<QQuickItem*>(object(root,"durableLedger"))->forceActiveFocus();
        QTest::keySequence(window,QKeySequence::Undo);QVERIFY(object(root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(root,"confirmActionUndo"));wait(model);QTRY_COMPARE(model.rowCount(),3);
        QCOMPARE(model.state().value("editUndoLabel").toString(),"Undo Rename payee…");
        QVERIFY(click(root,"undoEdit"));QVERIFY(object(root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(root,"confirmActionUndo"));wait(model);QCOMPARE(model.rowCount(),0);
        run(model,"query",{{"filter",""}});
        run(model,"inspect",{{"batch",oldBatch},{"ordinal",0}});QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Synthetic purchase");
        run(model,"inspect",{{"batch",newBatch},{"ordinal",0}});QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Synthetic new");
        open("renamePayeeSelection");QCOMPARE(root->property("selectedEditCount").toInt(),1);QVERIFY(object(root,"selectedPayee")->setProperty("text","Single rename"));QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        run(model,"close");run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",{{"batch",newBatch},{"ordinal",0}});
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Single rename");QVERIFY(!model.state().value("canUndoEdit").toBool());
        QCOMPARE(warnings.size(),0);
    }
    void selectedSetMemo(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(QTest::qWaitForWindowExposed(window));
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Synthetic memos"},{"currency","CAD"}});
        run(model,"opening",{{"date","2026-01-01"},{"amount","0.00"}});
        QFile file(dir.path()+"/memos.qif");QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray source=
            "D1/1/2026\nT-10.00\nPAlpha\nMBank note\nSFees\nESplit note\n$-4.00\nSFood\nESecond note\n$-6.00\n^\n"
            "D2/1/2026\nT-3.00\nPBlank\n^\n"
            "D3/1/2026\nT-4.00\nPKept\nMKeep me\n^\n"
            "D4/1/2026\nT-5.00\nPSpaces\nM   \n^\n";
        QCOMPARE(file.write(source),source.size());file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});run(model,"commit");
        QString batch;for(int row=0;row<model.rowCount();++row){const auto record=model.rowRecord(row);if(record.value("payee").toString()=="Alpha"){batch=record.value("batch").toString();break;}}
        QVERIFY(!batch.isEmpty());
        auto record=[&](int ordinal){return model.rowRecord(model.occurrenceIndex(batch,ordinal));};
        auto memo=[&](int ordinal){return record(ordinal).value("memo").toString();};
        auto splitMemo=[&](int ordinal,int split){return record(ordinal).value("splits").toList().at(split).toMap().value("memo").toString();};
        QCOMPARE(memo(0),QString("Bank note"));QCOMPARE(splitMemo(0,0),QString("Split note"));QCOMPARE(splitMemo(0,1),QString("Second note"));
        QCOMPARE(memo(1),QString());QCOMPARE(memo(2),QString("Keep me"));QCOMPARE(memo(3),QString("   "));
        run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});
        for(int ordinal=1;ordinal<4;++ordinal)run(model,"select",{{"row",model.occurrenceIndex(batch,ordinal)},{"toggle",true}});
        QCOMPARE(model.state().value("selectionCount").toInt(),4);
        auto* item=object(root,"setMemoSelection");QVERIFY(item);QVERIFY(item->property("enabled").toBool());QCOMPARE(item->property("text").toString(),"Set memo…");
        QVERIFY(QMetaObject::invokeMethod(item,"triggered"));QTRY_VERIFY(object(root,"selectedEditDialog")->property("visible").toBool());
        QCOMPARE(object(root,"selectedEditDialog")->property("title").toString(),"Set memo");
        QCOMPARE(root->property("selectedMemoCount").toInt(),2);
        QCOMPARE(object(root,"overwriteExistingMemos")->property("visible").toBool(),true);
        QCOMPARE(object(root,"overwriteExistingMemos")->property("text").toString(),"Overwrite 2 existing memos");
        QCOMPARE(object(root,"overwriteExistingMemos")->property("checked").toBool(),false);
        QCOMPARE(object(root,"selectedMemo")->property("text").toString(),QString());
        QVERIFY(!object(root,"confirmSelectedEdit")->property("enabled").toBool());
        QVERIFY(object(root,"selectedMemo")->setProperty("text","Shared"));
        QVERIFY(object(root,"confirmSelectedEdit")->property("enabled").toBool());
        QCOMPARE(object(root,"confirmSelectedEdit")->property("text").toString(),"Set memo");
        QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        QCOMPARE(memo(0),QString("Bank note"));QCOMPARE(memo(1),QString("Shared"));QCOMPARE(memo(2),QString("Keep me"));QCOMPARE(memo(3),QString("Shared"));
        QCOMPARE(splitMemo(0,0),QString("Split note"));QCOMPARE(splitMemo(0,1),QString("Second note"));
        QCOMPARE(record(1).value("memoOwner").toString(),"manual");QCOMPARE(record(0).value("memoOwner").toString(),"source");
        QCOMPARE(model.state().value("editUndoLabel").toString(),"Undo Set memo…");
        QVERIFY(click(root,"undoEdit"));QVERIFY(object(root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(root,"confirmActionUndo"));wait(model);
        QCOMPARE(memo(1),QString());QCOMPARE(memo(3),QString("   "));
        QVERIFY(QMetaObject::invokeMethod(object(root,"setMemoSelection"),"triggered"));QTRY_VERIFY(object(root,"selectedEditDialog")->property("visible").toBool());
        QVERIFY(object(root,"overwriteExistingMemos")->setProperty("checked",true));
        QVERIFY(object(root,"selectedMemo")->setProperty("text","Shared"));
        QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        for(int ordinal:{0,1,2,3}) QCOMPARE(memo(ordinal),QString("Shared"));
        QCOMPARE(splitMemo(0,0),QString("Split note"));QCOMPARE(record(2).value("splits").toList().front().toMap().value("memo").toString(),"Keep me");
        QVERIFY(click(root,"undoEdit"));QVERIFY(click(root,"confirmActionUndo"));wait(model);
        QVERIFY(QMetaObject::invokeMethod(object(root,"setMemoSelection"),"triggered"));QTRY_VERIFY(object(root,"selectedEditDialog")->property("visible").toBool());
        QVERIFY(object(root,"overwriteExistingMemos")->setProperty("checked",true));
        QVERIFY(object(root,"selectedMemo")->setProperty("text",""));
        QCOMPARE(object(root,"confirmSelectedEdit")->property("text").toString(),"Clear memos");
        QVERIFY(object(root,"confirmSelectedEdit")->property("enabled").toBool());
        QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        QCOMPARE(memo(0),QString());QCOMPARE(memo(2),QString());QCOMPARE(memo(1),QString());QCOMPARE(memo(3),QString("   "));
        QCOMPARE(record(0).value("memoOwner").toString(),"manual");QCOMPARE(record(3).value("memoOwner").toString(),"source");
        QVERIFY(click(root,"undoEdit"));QVERIFY(click(root,"confirmActionUndo"));wait(model);
        QCOMPARE(memo(0),QString("Bank note"));QCOMPARE(memo(2),QString("Keep me"));
        run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});
        QVERIFY(QMetaObject::invokeMethod(object(root,"setMemoSelection"),"triggered"));QTRY_VERIFY(object(root,"selectedEditDialog")->property("visible").toBool());
        QCOMPARE(object(root,"selectedMemo")->property("text").toString(),"Bank note");
        QCOMPARE(object(root,"overwriteExistingMemos")->property("text").toString(),"Overwrite 1 existing memo");
        QCOMPARE(object(root,"confirmSelectedEdit")->property("enabled").toBool(),false);
        QVERIFY(click(root,"cancelSelectedEdit"));QCOMPARE(memo(0),QString("Bank note"));
        QCOMPARE(warnings.size(),0);
    }
    void selectionEditRetainsViewport(){
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.path()+"/document"},{"protected",false},{"name","Viewport"},{"currency","CAD"}});
        QFile file(dir.path()+"/rows.qif");QVERIFY(file.open(QIODevice::WriteOnly));QByteArray rows;
        for(int i=0;i<150;++i)rows+="D1/1/2026\nT-1\nPSynthetic "+QByteArray::number(i).rightJustified(3,'0')+"\n^\n";
        file.write(rows);file.close();run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto batch=selected(model).value("batch").toString();run(model,"inspect",{{"batch",batch},{"ordinal",100}});
        run(model,"select",{{"row",model.occurrenceIndex(batch,100)}});run(model,"select",{{"row",model.occurrenceIndex(batch,102)},{"range",true}});
        auto* ledger=qobject_cast<QQuickItem*>(object(root,"durableLedger"));QVERIFY(ledger);
        auto snapshot=[&](){QMetaObject::invokeMethod(root,"rememberAccountViewport");return root->property("accountViewports").value<QJSValue>().toVariant().toMap().value(model.state().value("accountId").toString()).toMap();};
        QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtIndex",Q_ARG(int,model.occurrenceIndex(batch,90)),Q_ARG(int,0)));QTest::qWait(80);
        ledger->setProperty("contentY",ledger->property("contentY").toReal()+11);const auto saved=snapshot();
        QVERIFY(QMetaObject::invokeMethod(object(root,"renamePayeeSelection"),"triggered"));QVERIFY(object(root,"selectedPayee")->setProperty("text","Renamed"));QVERIFY(click(root,"confirmSelectedEdit"));wait(model);
        QTRY_COMPARE(snapshot().value("anchor"),saved.value("anchor"));QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        QCOMPARE(model.state().value("selectionCount").toInt(),3);
        QVERIFY(click(root,"undoEdit"));QVERIFY(object(root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(root,"confirmActionUndo"));wait(model);
        QTRY_COMPARE(snapshot().value("anchor"),saved.value("anchor"));QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        QCOMPARE(model.state().value("selectionCount").toInt(),3);
    }
    void selectedEditAcknowledgement(){
        for(int mode=0;mode<4;++mode){
            QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));prepare(model,dir.path());
            const auto id=selected(model);const QVariantMap args{{"account",model.state().value("accountId")},{"generation",model.state().value("ledgerEditGeneration")},{"selected",QVariantList{QVariantMap{{"batch",id.value("batch")},{"ordinal",0}}}},{"payee","Renamed"}};
            fault->failBefore=mode==0;fault->loseAcknowledgement=mode==1 || mode==2;fault->unknownOutcome=mode==2;fault->failRefresh=mode==3;
            run(model,"rename_payee_selection",args,mode==1);
            fault->failBefore=false;fault->loseAcknowledgement=false;fault->unknownOutcome=false;fault->failRefresh=false;
            run(model,"close");run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",{{"batch",id.value("batch")},{"ordinal",0}});
            QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),mode==0?"Synthetic purchase":"Renamed");
        }
    }
    void addThenRemoveEmptySplit() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        prepare(model,dir.path());
        run(model,"inspect",{{"batch",selected(model).value("batch")},{"ordinal",1}});
        const auto before=selected(model).value("draft").toMap();
        const auto count=before.value("splits").toList().size();QCOMPARE(count,1);
        auto* remove=object(root,"removeSplit_0");QVERIFY(remove);
        QVERIFY(!remove->property("visible").toBool());QVERIFY(!remove->property("enabled").toBool());
        // Even a queued/direct activation must not remove the final allocation.
        QVERIFY(QMetaObject::invokeMethod(remove,"clicked"));
        QVERIFY(!root->property("dirty").toBool());
        QVERIFY(object(root,"editSplitAmount_0"));
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        QVERIFY(click(root,"addSplit"));QVERIFY(root->property("dirty").toBool());
        QVERIFY(remove->property("visible").toBool());
        QVERIFY(object(root,"removeSplit_"+QString::number(count)));
        QVERIFY(click(root,"removeSplit_"+QString::number(count)));
        QVERIFY(root->property("dirty").toBool());
        QTRY_VERIFY(!object(root,"removeSplit_"+QString::number(count)));
        QVERIFY(!remove->property("visible").toBool());
        QCOMPARE(warnings.count(),0);
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));wait(model);
        QCOMPARE(selected(model).value("draft").toMap().value("splits"),before.value("splits"));
        QCOMPARE(model.state().value("committed").toString(),QString("80.69"));
        QVERIFY(!root->property("dirty").toBool());QCOMPARE(warnings.count(),0);
    }
    void bulkRecoveryOutcomes() {
        for(int mode=0;mode<4;++mode){
            QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));prepare(model,dir.path());
            if(!model.state().value("bulkSupported").toBool())QSKIP("Bulk format is not enabled");
            const auto batch=model.state().value("batches").toList().front().toMap().value("id").toString();
            run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});run(model,"select",{{"row",model.occurrenceIndex(batch,1)},{"toggle",true}});
            run(model,"preview_bulk",{{"kind","remove"}});const auto id=model.state().value("bulkPreview").toMap().value("id");
            fault->failBefore=mode==0;fault->loseAcknowledgement=mode==1||mode==2;fault->unknownOutcome=mode==2;fault->failRefresh=mode==3;
            run(model,"apply_bulk",{{"preview",id}},mode==1);
            if(mode==0){QVERIFY(model.state().value("active").toBool());QCOMPARE(model.state().value("committed").toString(),"80.69");}
            if(mode==1){QVERIFY(model.state().value("active").toBool());QCOMPARE(model.state().value("committed").toString(),"100.00");}
            if(mode==2){QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("uncertain"));}
            if(mode==3){QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("saved"));}
            fault->failBefore=false;fault->loseAcknowledgement=false;fault->unknownOutcome=false;fault->failRefresh=false;
            if(model.state().value("active").toBool())run(model,"close");run(model,"open",{{"path",dir.path()+"/document"}});
            QCOMPARE(model.state().value("committed").toString(),mode==0?"80.69":"100.00");
        }
    }
    void correctionGuardsRemovalAndUndo() {
        QTemporaryDir dir;ImportModel model(storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        prepare(model,dir.path());const auto id=model.rowIdentity(model.selectedRowIndex());
        QVERIFY(model.state().value("committedChangesSupported").toBool());QCOMPARE(model.state().value("committed").toString(),"80.69");
        run(model,"history");
        const auto prepared=model.state().value("history").toList();QCOMPARE(prepared.size(),2);
        QCOMPARE(prepared.at(0).toMap().value("label").toString(),"Accept transactions");
        QCOMPARE(prepared.at(1).toMap().value("label").toString(),"Import file");
        QVERIFY(selected(model).value("committed").toBool());QVERIFY(!selected(model).value("editable").toBool());
        QVERIFY(!root->findChild<QObject*>("durableTotal")->property("readOnly").toBool());
        QVERIFY(edit(root,"durableTotal","-30.00"));QVERIFY(edit(root,"editSplitAmount_1","-29.86"));
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));wait(model);
        QCOMPARE(model.state().value("committed").toString(),"80.00");
        run(model,"history");const auto amountEdit=model.state().value("history").toList().front().toMap();
        QCOMPARE(amountEdit.value("label").toString(),"Edit transactions");QVERIFY(!amountEdit.value("id").toString().isEmpty());
        QCOMPARE(selected(model).value("original").toString(),"-29.31");QVERIFY(!root->property("dirty").toBool());
        QVERIFY(edit(root,"durablePayee","Corrected merchant"));
        QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardStay"));QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardSave"));wait(model);
        QCOMPARE(root->property("currentPage").toString(),"settings");QVERIFY(!root->property("dirty").toBool());
        QVERIFY(click(root,"navigationBack"));QVERIFY(click(root,"historyNavigation"));wait(model);QTRY_VERIFY(object(root,"historyRow_0"));QVERIFY(click(root,"historyRow_0"));
        QCOMPARE(root->property("currentPage").toString(),"history");
        QTRY_VERIFY(object(root,"historyUndo"));
        QVERIFY(object(root,"historyShowLedger"));
        QVERIFY(!object(root,"historyShowFull_0"));
        QVERIFY(!object(root,"historyBalance"));
        QVERIFY(click(root,"historyShowLedger"));wait(model);
        QCOMPARE(root->property("currentPage").toString(),"ledger");
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
        QVERIFY(click(root,"historyNavigation"));wait(model);
        QVERIFY(click(root,"historyRow_1"));
        QTRY_VERIFY(object(root,"historyBalance"));
        QTRY_VERIFY(object(root,"historyUndo"));
        QCOMPARE(object(root,"historyUndo")->property("text").toString(),"Undo");
        QVERIFY(click(root,"historyUndo"));
        QTRY_VERIFY(object(root,"connectedUndoConfirm")->property("visible").toBool());
        QTRY_VERIFY(object(root,"connectedUndoLine_0"));
        const auto newestLine=object(root,"connectedUndoLine_0")->property("text").toString();
        QVERIFY(newestLine.contains("Payee:"));QVERIFY(newestLine.contains("→"));QVERIFY(newestLine.contains("Corrected merchant"));
        QVERIFY(!object(root,"connectedUndoLine_2"));
        QVERIFY(click(root,"cancelConnectedUndo"));
        QTRY_VERIFY(!object(root,"connectedUndoConfirm")->property("visible").toBool());
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
        QVERIFY(click(root,"accountNavigation"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"deleteSelectedContext"),"triggered"));
        QTRY_VERIFY(object(root,"deleteSelectionDialog")->property("visible").toBool());
        QVERIFY(click(root,"confirmDeleteSelection"));wait(model);
        QCOMPARE(model.state().value("committed").toString(),"110.00");QCOMPARE(model.selectedRowIndex(),-1);
        QVERIFY(click(root,"historyNavigation"));wait(model);QTRY_VERIFY(object(root,"historyRow_0"));QVERIFY(click(root,"historyRow_0"));
        QTRY_VERIFY(object(root,"historyUndo"));QVERIFY(click(root,"historyUndo"));
        QTRY_VERIFY(object(root,"actionUndoConfirm")->property("visible").toBool());QVERIFY(click(root,"confirmActionUndo"));wait(model);
        QCOMPARE(model.state().value("committed").toString(),"80.00");
        run(model,"inspect",id);QVERIFY(!selected(model).value("removed").toBool());
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
        const auto checking=model.state().value("accountId");
        QVERIFY(click(root,"historyNavigation"));wait(model);QVERIFY(click(root,"historyRow_0"));QTRY_VERIFY(object(root,"historyOpen"));
        run(model,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});
        QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(!object(root,"historyOpen"));
        QVERIFY(click(root,"historyNavigation"));wait(model);QVERIFY(object(root,"historyEmpty")->property("visible").toBool());
        QVERIFY(!object(root,"historyBack"));
        run(model,"select_account",{{"id",checking}});
        run(model,"inspect",id);
        QCOMPARE(model.state().value("committed").toString(),"80.00");QCOMPARE(selected(model).value("original").toString(),"-29.31");
        run(model,"history");const auto retained=model.state().value("history").toList();QCOMPARE(retained.size(),4);
        QCOMPARE(retained.at(0).toMap().value("label").toString(),"Edit transactions");
        QCOMPARE(retained.at(1).toMap().value("label").toString(),"Edit transactions");
        QCOMPARE(retained.at(2).toMap().value("label").toString(),"Accept transactions");
        QCOMPARE(retained.at(3).toMap().value("label").toString(),"Import file");
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
        run(model,"close");run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);
        run(model,"history");QVERIFY(model.state().value("history").toList().isEmpty());
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Corrected merchant");
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
        run(model,"history");const auto retained=model.state().value("history").toList();QCOMPARE(retained.size(),4);
        QCOMPARE(retained.at(0).toMap().value("label").toString(),"Accept transactions");
        QCOMPARE(retained.at(1).toMap().value("label").toString(),"Import file");
        QCOMPARE(retained.at(2).toMap().value("label").toString(),"Accept transactions");
        QCOMPARE(retained.at(3).toMap().value("label").toString(),"Import file");
        for(const auto& value:retained){const auto row=value.toMap();QVERIFY(!row.value("summary").toString().contains("Stale proposal"));for(const auto& member:row.value("members").toList())QVERIFY(!member.toMap().value("payee").toString().contains("Stale proposal"));}
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
        QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("could not confirm"));
        fault->unknownOutcome=false;fault->loseAcknowledgement=false;run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Saved despite unknown outcome");
        run(model,"history");QCOMPARE(model.state().value("history").toList().size(),4);QCOMPARE(model.state().value("committed").toString(),"80.69");
    }
    void editAcknowledgementAndQuarantine() {
        QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));prepare(model,dir.path());
        const auto id=model.rowIdentity(model.selectedRowIndex());auto change=selected(model).value("draft").toMap();change["payee"]="Recovered edit";
        fault->loseAcknowledgement=true;run(model,"accept_edit",change);QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Recovered edit");
        change["payee"]="Unknown acknowledgement";fault->unknownOutcome=true;run(model,"accept_edit",change,false);QVERIFY(!model.state().value("active").toBool());
        fault->unknownOutcome=false;fault->loseAcknowledgement=false;run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Unknown acknowledgement");QVERIFY(!model.state().value("canUndoEdit").toBool());run(model,"history");QCOMPARE(model.state().value("history").toList().size(),4);
    }
    void failedApplyRetainsBufferAndKnownSaveIsReported() {
        QTemporaryDir dir;auto gateway=std::make_unique<ChangeFaultGateway>();auto* fault=gateway.get();ImportModel model(std::move(gateway));
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        prepare(model,dir.path());const auto id=model.rowIdentity(model.selectedRowIndex());
        QVERIFY(edit(root,"durablePayee","Preserved proposal"));
        fault->failBefore=true;QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));wait(model,false);
        QVERIFY(root->property("dirty").toBool());QCOMPARE(root->findChild<QObject*>("durablePayee")->property("text").toString(),"Preserved proposal");
        QCOMPARE(model.state().value("committed").toString(),"80.69");
        fault->failBefore=false;fault->failRefresh=true;
        QVERIFY(QMetaObject::invokeMethod(root,"saveEditor"));wait(model,false);
        QVERIFY(!model.state().value("active").toBool());QVERIFY(model.error().contains("saved"));
        fault->failRefresh=false;run(model,"open",{{"path",dir.path()+"/document"}});run(model,"inspect",id);
        QCOMPARE(selected(model).value("draft").toMap().value("payee").toString(),"Preserved proposal");run(model,"history");QCOMPARE(model.state().value("history").toList().size(),3);
    }
    void removedReimportRequiresReview_data() {
        QTest::addColumn<bool>("keepRemoved");
        QTest::newRow("keep removed")<<true;
        QTest::newRow("include separately")<<false;
    }
    void removedReimportRequiresReview() {
        QFETCH(bool,keepRemoved);
        QTemporaryDir dir;ImportModel model(storage::document_gateway());QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();const auto file=prepare(model,dir.path());
        const auto removedBatch=selected(model).value("batch").toString();const auto removedOrdinal=selected(model).value("ordinal").toInt();
        run(model,"select",{{"row",model.occurrenceIndex(removedBatch,removedOrdinal)}});
        QVERIFY(QMetaObject::invokeMethod(object(root,"deleteSelectedContext"),"triggered"));
        QTRY_VERIFY(object(root,"deleteSelectionDialog")->property("visible").toBool());
        QVERIFY(click(root,"confirmDeleteSelection"));wait(model);QCOMPARE(model.state().value("committed").toString(),"110.00");
        run(model,"import",{{"path",file},{"profile","DMY"}});QCOMPARE(selected(model).value("decision").toInt(),0);
        const auto candidates=selected(model).value("candidates").toList();QVERIFY(!candidates.isEmpty());QVERIFY(candidates.front().toMap().value("removed").toBool());
        QVERIFY(!root->findChild<QObject*>("includeSeparate")->property("visible").toBool());
        QVERIFY(!root->findChild<QObject*>("excludeDuplicate")->property("visible").toBool());
        run(model,"review_load");root->setProperty("currentPage","review");
        QTRY_VERIFY(object(root,"reviewCorrect"));
        QVERIFY(click(root,"reviewCorrect"));wait(model);
        QCOMPARE(root->property("currentPage").toString(),"review");
        QVERIFY(root->property("reviewInspecting").toBool());
        QTRY_VERIFY(root->findChild<QObject*>("includeSeparate")->property("visible").toBool());
        auto* chooser=root->findChild<QObject*>("duplicateCandidate");QVERIFY(chooser);chooser->setProperty("currentIndex",0);
        QCOMPARE(root->findChild<QObject*>("excludeDuplicate")->property("text").toString(),"Keep removed");
        QVERIFY(click(root,keepRemoved?"excludeDuplicate":"includeSeparate"));wait(model);QCOMPARE(selected(model).value("decision").toInt(),keepRemoved?2:1);
        const auto expected=keepRemoved?QString("110.00"):QString("80.69");
        QCOMPARE(model.state().value("projected").toString(),expected);run(model,"commit");QCOMPARE(model.state().value("committed").toString(),expected);
    }
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));applySpentItControlsStyle();CommittedUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "committed_ui_tests.moc"
