#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QTest>
#include <QJSValue>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFont>
#include <QStyleHints>
#include <QPalette>
#include <cmath>

class NavigationTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    void run(ImportModel& model,const QString& action,const QVariantMap& args={}) {
        auto request=args;if(action=="import" && !request.contains("account"))request["account"]=model.state().value("accountId");model.command(action,request);
        const auto id=spentit::diagnostics::Collector::instance().snapshot().active.id;
        if(model.diagnosticsEnabled())QTRY_VERIFY_WITH_TIMEOUT(spentit::diagnostics::Collector::instance().snapshot().latest.id==id && spentit::diagnostics::Collector::instance().snapshot().latest.delivered!=0,30000);
        else QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        if(action=="import")for(const auto& item:model.state().value("importCategoryDecisions").toList()){const auto d=item.toMap();run(model,"resolve_import_categories",{{"source",d.value("source")},{"selected",d.value("names")}});}
    }
    static QQuickItem* visual(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;}
    static QObject* object(QObject* root,const char* name){if(auto* found=root->findChild<QObject*>(QString::fromLatin1(name)))return found;auto* window=qobject_cast<QQuickWindow*>(root);return window?visual(window->contentItem(),QString::fromLatin1(name)):nullptr;}
    static bool click(QObject* root,const char* name) {
        auto* item=object(root,name);
        return item && QMetaObject::invokeMethod(item,"clicked");
    }
    static bool edit(QObject* root,const char* name,const QString& value) {
        auto* item=object(root,name);
        return item && item->setProperty("text",value) && QMetaObject::invokeMethod(item,"textEdited");
    }
    // Qt 6.12 exposes a nullary Menu.popup invokable. Qt 6.8 exposes popup only
    // as a QML function, so the same call is evaluated there.
    static bool showPopup(QObject* menu) {
        if(!menu) return false;
        if(QMetaObject::invokeMethod(menu,"popup")) return true;
        QQmlExpression expression(qmlContext(menu),menu,QStringLiteral("popup()"));
        expression.evaluate();
        return !expression.hasError();
    }
private slots:
    void initTestCase() {
        if(qEnvironmentVariableIsSet("SPENTIT_TEST_DIAGNOSTICS"))QVERIFY(spentit::diagnostics::Collector::instance().enable());
        QCoreApplication::setOrganizationName("SpentItTests");QCoreApplication::setApplicationName("Navigation");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());QSettings().clear();
    }
    void init() { QSettings().clear(); } // Never reopen a previous test's removed temporary dataset.
    void activityAnimation_data() {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light")<<false;QTest::newRow("dark")<<true;
    }
    void activityAnimation() {
        QFETCH(bool,dark);
        QGuiApplication::styleHints()->setColorScheme(dark?Qt::ColorScheme::Dark:Qt::ColorScheme::Light);
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        auto* indicator=window->findChild<QQuickItem*>("activityIndicator");QVERIFY(indicator);
        QVERIFY(!indicator->property("running").toBool());QVERIFY(!indicator->isVisible());
        // Isolate the real toolbar control for a sustained visual animation test.
        // No document or simulated financial operation is needed.
        indicator->setParentItem(window->contentItem());indicator->setPosition(QPointF(40,40));
        indicator->setSize(QSizeF(36,36));indicator->setProperty("running",true);
        window->show();QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(indicator->isVisible());QTest::qWait(120);
        const auto frame=[&]{const auto image=window->grabWindow();const auto scale=image.devicePixelRatio();return image.copy(QRect(40*scale,40*scale,36*scale,36*scale));};
        const auto first=frame();QVERIFY(!first.isNull());
        QTest::qWait(220);const auto second=frame();QVERIFY(first!=second);
        QTest::qWait(220);const auto third=frame();QVERIFY(second!=third);
        if(qEnvironmentVariableIsSet("SPENTIT_ACTIVITY_CAPTURE")) {
            QVERIFY(first.save(QString("/tmp/spentit-activity-%1-1.png").arg(dark?"dark":"light")));
            QVERIFY(second.save(QString("/tmp/spentit-activity-%1-2.png").arg(dark?"dark":"light")));
        }
        indicator->setProperty("running",false);QTRY_VERIFY(!indicator->isVisible());
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void textBoundaries_data() {
        QTest::addColumn<QString>("type");
        for(const auto* type:{"NavigableTextField","ClearableSearchField","CategoryCompletion"})
            QTest::newRow(type)<<QString::fromLatin1(type);
    }
    void textBoundaries() {
        QFETCH(QString,type);
        QQmlApplicationEngine engine;
        QQmlComponent component(&engine);
        component.setData(("import QtQuick\nimport QtQuick.Controls\n"+type+
            " { width: 300; height: 40; property bool escaped: false; "
            "Keys.onEscapePressed: escaped = true }").toUtf8(),QUrl("qrc:/spentit/BoundaryTest.qml"));
        std::unique_ptr<QObject> field(component.create());
        QVERIFY2(field,qPrintable(component.errorString()));
        auto* item=qobject_cast<QQuickItem*>(field.get());QVERIFY(item);
        QQuickWindow window;window.resize(400,100);item->setParentItem(window.contentItem());window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));item->forceActiveFocus();
        QTRY_VERIFY(item->hasActiveFocus());QCoreApplication::processEvents();
        const auto reset=[&](QString text="alpha beta",int position=5) {
            field->setProperty("text",text);field->setProperty("cursorPosition",position);
        };
        reset();
        QTest::keyClick(&window,Qt::Key_Home);QCOMPARE(field->property("cursorPosition").toInt(),0);
        QTest::keyClick(&window,Qt::Key_End);QCOMPARE(field->property("cursorPosition").toInt(),10);
        reset();
        QTest::keyClick(&window,Qt::Key_Home,Qt::ShiftModifier);
        QCOMPARE(field->property("selectedText").toString(),"alpha");
        QTest::keyClick(&window,Qt::Key_End,Qt::ShiftModifier);
        QCOMPARE(field->property("selectedText").toString()," beta");
        QTest::keyClick(&window,Qt::Key_Home,Qt::ShiftModifier);
        QCOMPARE(field->property("selectedText").toString(),"alpha");
        QTest::keyClick(&window,Qt::Key_Backspace);
        QCOMPARE(field->property("text").toString()," beta");
        reset();QTest::keyClick(&window,Qt::Key_End,Qt::ShiftModifier);
        QTest::keyClick(&window,Qt::Key_Exclam);QCOMPARE(field->property("text").toString(),"alpha!");
        reset();QTest::keyClick(&window,Qt::Key_Home,Qt::ShiftModifier);
        QTest::keyClick(&window,Qt::Key_End);
        QCOMPARE(field->property("selectedText").toString(),QString());
        QCOMPARE(field->property("cursorPosition").toInt(),10);
        reset("",0);QTest::keyClick(&window,Qt::Key_End,Qt::ShiftModifier);
        QTest::keyClick(&window,Qt::Key_Home);QCOMPARE(field->property("cursorPosition").toInt(),0);
        reset();QTest::keyClick(&window,Qt::Key_Left);QCOMPARE(field->property("cursorPosition").toInt(),4);
        QTest::keyClick(&window,Qt::Key_Escape);QVERIFY(field->property("escaped").toBool());
        item->setParentItem(nullptr);
    }
    void appearanceSwitching() {
        const bool native=QGuiApplication::platformName()!="offscreen";
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Theme account"},{"currency","CAD"}});
        auto* theme=object(root,"applicationTheme");QVERIFY(theme);
        const auto contrast=[](QColor a,QColor b) {
            const auto luminance=[](QColor c) {
                const auto channel=[](double v){return v<=0.04045?v/12.92:std::pow((v+0.055)/1.055,2.4);};
                return 0.2126*channel(c.redF())+0.7152*channel(c.greenF())+0.0722*channel(c.blueF());
            };
            const double x=luminance(a),y=luminance(b);return (std::max(x,y)+0.05)/(std::min(x,y)+0.05);
        };
        // Exercise both transitions on the same loaded workspace, including inherited controls.
        for(auto scheme:native ? QList{Qt::ColorScheme::Light,Qt::ColorScheme::Dark,Qt::ColorScheme::Light} : QList{Qt::ColorScheme::Light}) {
            if(native)QGuiApplication::styleHints()->setColorScheme(scheme);
            QTRY_COMPARE(theme->property("dark").toBool(),scheme==Qt::ColorScheme::Dark);
            const auto colour=[&](const char* role){return theme->property(role).value<QColor>();};
            QTRY_COMPARE(window->color(),colour("window"));
            auto* search=object(root,"ledgerSearch");QVERIFY(search);
            QTRY_COMPARE(search->property("color").value<QColor>(),colour("text"));
            auto* row=object(root,"ledgerRow_0");QVERIFY(row);
            QTRY_COMPARE(row->property("color").value<QColor>(),colour("row"));
            auto* balance=object(root,("accountBalance_"+model.state().value("accountId").toString()).toUtf8().constData());QVERIFY(balance);
            QCOMPARE(balance->property("color").value<QColor>(),colour("text"));
            auto* account=object(root,"accountNavigation");QVERIFY(account);
            auto* accountBackground=account->property("background").value<QObject*>();QVERIFY(accountBackground);
            auto* accountText=account->property("contentItem").value<QObject*>();QVERIFY(accountText);
            QVERIFY(contrast(accountText->property("color").value<QColor>(),accountBackground->property("color").value<QColor>())>=4.5);
            for(const char* background:{"window","surface","alternate","row","rowAlternate","selection","selectionPressed","draft","draftAlternate","changed","button"})
                QVERIFY2(contrast(colour("text"),colour(background))>=4.5,background);
            QVERIFY(contrast(colour("highlightedText"),colour("highlight"))>=4.5);
            for(const char* foreground:{"muted","error","warning"})
                QVERIFY2(contrast(colour(foreground),colour("surface"))>=4.5,foreground);
            // Check the controls' actual rendered colour properties, not just theme pairs.
            QVERIFY(click(root,"sidebarSettings"));
            auto* combo=qobject_cast<QQuickItem*>(object(root,"amountFormatStyle"));QVERIFY(combo);
            auto* popup=combo->property("popup").value<QObject*>();QVERIFY(popup);
            combo->forceActiveFocus();
            QVERIFY(QMetaObject::invokeMethod(popup,"open"));
            QTest::keyClick(window,Qt::Key_Down);
            QTRY_COMPARE(combo->property("highlightedIndex").toInt(),1);
            QTRY_VERIFY(object(root,"comboChoice_1"));
            auto* choice=object(root,"comboChoice_1");
            QVERIFY(choice->property("highlighted").toBool());
            const auto checkControl=[&](QObject* control) {
                auto* text=control->property("contentItem").value<QObject*>();
                auto* background=control->property("background").value<QObject*>();
                return text && background && contrast(text->property("color").value<QColor>(),background->property("color").value<QColor>())>=4.5;
            };
            QVERIFY(checkControl(choice));
            QVERIFY(choice->setProperty("down",true));QVERIFY(checkControl(choice));
            QVERIFY(choice->setProperty("down",false));
            auto* menuItem=object(root,"addTransactionContext");QVERIFY(menuItem);
            QVERIFY(menuItem->setProperty("highlighted",true));QVERIFY(checkControl(menuItem));
            QVERIFY(menuItem->setProperty("down",true));QVERIFY(checkControl(menuItem));
            QVERIFY(menuItem->setProperty("down",false));
            QVERIFY(menuItem->setProperty("highlighted",false));
            auto* settingsButton=object(root,"sidebarSettings");QVERIFY(checkControl(settingsButton));
            if(qEnvironmentVariableIsSet("SPENTIT_THEME_CAPTURE")) {
                QTest::qWait(100);
                QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_THEME_CAPTURE")+(scheme==Qt::ColorScheme::Dark?"-dark.png":"-light.png")));
            }
            QVERIFY(QMetaObject::invokeMethod(popup,"close"));
            QVERIFY(QMetaObject::invokeMethod(account,"clicked"));
        }
        if(native)QGuiApplication::styleHints()->unsetColorScheme();
    }
    void ledgerStripes_data() {
        QTest::addColumn<bool>("accepted");
        QTest::newRow("committed")<<true;
        QTest::newRow("uncommitted")<<false;
    }
    void ledgerStripes() {
        QFETCH(bool,accepted);
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Stripe account"},{"currency","CAD"}});
        QFile file(dir.filePath("rows.qif"));QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("!Type:Bank\n");
        for(int i=0;i<6;++i)file.write(("D1/1/2026\nT-10\nPRow "+QString::number(i)+"\nSExpense\n$-12\nSRefund\n$2\n^\n").toUtf8());
        file.close();run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        if(accepted)run(model,"commit",{{"ignoreWarnings",true}});
        root->setProperty("editorClosed",true);
        auto* theme=object(root,"applicationTheme");QVERIFY(theme);
        const bool native=QGuiApplication::platformName()!="offscreen";
        for(auto scheme:native?QList{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}:QList{Qt::ColorScheme::Light}) {
            if(native)QGuiApplication::styleHints()->setColorScheme(scheme);
            QTRY_COMPARE(theme->property("dark").toBool(),scheme==Qt::ColorScheme::Dark);
            for(bool descending:{false,true}) {
                run(model,"query",{{"filter",""},{"sort",1},{"descending",descending}});
                auto* ledger=qobject_cast<QQuickItem*>(object(root,"durableLedger"));QVERIFY(ledger);
                QVERIFY(QMetaObject::invokeMethod(ledger,"positionViewAtBeginning"));
                for(int i=0;i<4;++i) {
                    QTRY_VERIFY(object(root,qPrintable("ledgerRow_"+QString::number(i))));
                    auto* row=object(root,qPrintable("ledgerRow_"+QString::number(i)));
                    const bool draft=model.rowRecord(i).value("status").toString()=="Draft";
                    if(i>0)QCOMPARE(draft,!accepted); // Opening balance sorts first in ascending order.
                    QTRY_COMPARE(row->property("color").value<QColor>(),theme->property(draft?(i%2?"draftAlternate":"draft"):(i%2?"rowAlternate":"row")).value<QColor>());
                }
            }
            auto* toggle=qobject_cast<QQuickItem*>(object(root,"splitToggle_0"));QVERIFY(toggle);
            QCOMPARE(toggle->x(),32.0);
            auto* label=toggle->property("contentItem").value<QObject*>();QVERIFY(label);
            QCOMPARE(label->property("horizontalAlignment").toInt(),int(Qt::AlignLeft));
            auto* background=toggle->property("background").value<QObject*>();QVERIFY(background);
            QTest::mouseMove(window,QPoint(2,2));
            QTRY_COMPARE(background->property("color").value<QColor>().alpha(),0);
            toggle->setProperty("down",true);
            QCOMPARE(background->property("color").value<QColor>(),theme->property("selectionPressed").value<QColor>());
            toggle->setProperty("down",false);
            QTest::mouseMove(window,toggle->mapToScene(QPointF(10,10)).toPoint());
            QTRY_COMPARE(background->property("color").value<QColor>(),theme->property("selection").value<QColor>());
            toggle->forceActiveFocus(Qt::TabFocusReason);
            QTRY_VERIFY(toggle->property("visualFocus").toBool());
            auto* border=background->property("border").value<QObject*>();QVERIFY(border);
            QCOMPARE(border->property("width").toInt(),1);
            qobject_cast<QQuickItem*>(object(root,"durableLedger"))->forceActiveFocus(Qt::MouseFocusReason);
            QTest::mouseMove(window,QPoint(2,2));
            QVERIFY(QMetaObject::invokeMethod(toggle,"clicked"));
            QTRY_VERIFY(object(root,"splitPreview_0")->property("height").toReal()>0);
            if(qEnvironmentVariableIsSet("SPENTIT_STRIPE_CAPTURE")) {
                QTest::qWait(100);
                QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_STRIPE_CAPTURE")+(accepted?"-committed":"-uncommitted")+(scheme==Qt::ColorScheme::Dark?"-dark.png":"-light.png")));
            }
            QVERIFY(QMetaObject::invokeMethod(toggle,"clicked"));
            run(model,"query",{{"filter","Row 0"},{"sort",1}});
            QTRY_VERIFY(object(root,"ledgerRow_0"));
            QCOMPARE(object(root,"ledgerRow_0")->property("color").value<QColor>(),theme->property(accepted?"row":"draft").value<QColor>());
        }
        if(native)QGuiApplication::styleHints()->unsetColorScheme();
    }
    void manualTransactionEntry() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto path=dir.filePath("document");run(model,"create",{{"path",path},{"protected",false},{"name","Manual account"},{"currency","CAD"}});
        run(model,"query",{{"filter","no matching rows"}});
        QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(model.state().value("selected").toMap().value("newManual").toBool());QVERIFY(model.selectedRowIndex()>=0);
        QCOMPARE(object(root,"durableTotal")->property("text").toString(),QString("0.00"));
        QVERIFY(!object(root,"sourceToggle")->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"discardEditor",Q_ARG(QVariant,QVariant())));QTRY_VERIFY(!model.busy());
        QCOMPARE(model.state().value("newTransactions").toInt(),0);
        auto* context=object(root,"addTransactionContext");QVERIFY(context);QVERIFY(QMetaObject::invokeMethod(context,"triggered"));QTRY_VERIFY(!model.busy());
        QVERIFY(edit(root,"durableTotal","10/3"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"durableTotal"),"accepted"));QTRY_VERIFY(!model.busy());
        QVERIFY(model.state().value("selected").toMap().value("newManual").toBool());
        QCOMPARE(model.state().value("newTransactions").toInt(),0);
        QCOMPARE(object(root,"durableTotal")->property("text").toString(),QString("10/3"));
        QVERIFY(!root->property("splitEditMessage").toString().isEmpty());
        QVERIFY(edit(root,"durablePayee","Typed purchase"));QVERIFY(edit(root,"durableTotal","-9.34-(12*0.25)"));QVERIFY(edit(root,"durableDate","2026-01-15"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"durableTotal"),"accepted"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(object(root,"durableTotal")->property("text").toString(),QString("-12.34"));
        const auto batch=model.state().value("selected").toMap().value("batch").toString();
        QVERIFY(!model.state().value("selected").toMap().value("newManual").toBool());QCOMPARE(model.state().value("newTransactions").toInt(),1);QVERIFY(model.selectedRowIndex()>=0);
        run(model,"query",{{"filter",""}});run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});
        QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(!model.busy());QCOMPARE(object(root,"durableDate")->property("text").toString(),QString("2026-01-15"));
        QVERIFY(edit(root,"durablePayee","Not saved"));QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(object(root,"unsavedDialog")->property("visible").toBool());
        QVERIFY(click(root,"guardStay"));QVERIFY(model.state().value("selected").toMap().value("newManual").toBool());
        QVERIFY(click(root,"addTransaction"));QVERIFY(click(root,"guardDiscard"));QTRY_VERIFY(!model.busy() && model.state().value("selected").toMap().value("newManual").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"discardEditor",Q_ARG(QVariant,QVariant())));QTRY_VERIFY(!model.busy());
        run(model,"close");run(model,"open",{{"path",path}});run(model,"inspect",{{"batch",batch},{"ordinal",0}});
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),QString("Typed purchase"));
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("total").toString(),QString("-12.34"));
        QVERIFY(model.state().value("selected").toMap().value("ruleMatchError").toString().isEmpty());
        const auto before=model.selectedRowIndex();QVERIFY(edit(root,"durableDate","2027-01-15"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"durableDate"),"accepted"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(model.selectedRowIndex()>before);QVERIFY(!root->property("editorClosed").toBool());
        QVERIFY(edit(root,"durablePayee","Saved before adding"));QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(object(root,"unsavedDialog")->property("visible").toBool());
        QVERIFY(click(root,"guardSave"));QTRY_VERIFY(!model.busy() && model.state().value("selected").toMap().value("newManual").toBool());
        QCOMPARE(object(root,"durableDate")->property("text").toString(),QString("2027-01-15"));
        QVERIFY(QMetaObject::invokeMethod(root,"discardEditor",Q_ARG(QVariant,QVariant())));QTRY_VERIFY(!model.busy());
        run(model,"inspect",{{"batch",batch},{"ordinal",0}});QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),QString("Saved before adding"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"deleteSelectedContext"),"triggered"));
        QTRY_VERIFY(object(root,"deleteSelectionDialog")->property("visible").toBool());QVERIFY(click(root,"confirmDeleteSelection"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(model.occurrenceIndex(batch,0),-1);QCOMPARE(model.state().value("newTransactions").toInt(),0);
        run(model,"undo_edit");QVERIFY(model.occurrenceIndex(batch,0)>=0);
        QVERIFY(QMetaObject::invokeMethod(object(root,"deleteSelectedContext"),"triggered"));
        QTRY_VERIFY(object(root,"deleteSelectionDialog")->property("visible").toBool());
        QVERIFY(click(root,"confirmDeleteSelection"));QTRY_VERIFY(!model.busy());
        run(model,"close");run(model,"open",{{"path",path}});QCOMPARE(model.occurrenceIndex(batch,0),-1);

    }
    void diagnosticSemanticPrivacy() {
        if(!spentit::diagnostics::Collector::instance().enabled())QSKIP("Opt-in diagnostics test");
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        run(model,"create",{{"path",dir.path()+"/CANARY-path"},{"protected",true},{"password","CANARY-password"},{"confirmation","CANARY-password"},{"name","CANARY-account"},{"currency","CAD"}});
        root->setProperty("currentPage","settings");QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));
        auto observation=spentit::diagnostics::Collector::instance().snapshot();QCOMPARE(observation.ui.surface,spentit::diagnostics::Surface::settings);
        QVERIFY(observation.ui.time!=0);
        QVERIFY((observation.ui.control_visible & (1ULL<<1))==0);
        QVERIFY((observation.ui.control_enabled & (1ULL<<1))==0);
        root->setProperty("dirty",true);QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));QVERIFY(spentit::diagnostics::Collector::instance().snapshot().ui.dirty);
        window->hide();QVERIFY(QMetaObject::invokeMethod(root,"publishDiagnostics"));QVERIFY(!spentit::diagnostics::Collector::instance().snapshot().ui.native_exposed);
        QVERIFY(spentit::diagnostics::Collector::instance().json().find("CANARY")==std::string::npos);
        const auto generation=model.diagnosticGeneration();model.observeUi({{"generation",generation-1},{"surface","CANARY-payee"},{"ready",true}});
        QCOMPARE(spentit::diagnostics::Collector::instance().snapshot().ui.surface,spentit::diagnostics::Surface::settings);
    }
    void sidebarAccountDrag() {
        QTemporaryDir dir;const auto path=dir.filePath("order-document");
        const auto order=[](ImportModel& model){QStringList ids;for(const auto& value:model.state().value("sidebarAccounts").toList())ids.push_back(value.toMap().value("id").toString());return ids;};
        {
            ImportModel model(spentit::storage::document_gateway());
            QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
            QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
            run(model,"create",{{"path",path},{"protected",false},{"name","Checking"},{"currency","CAD"}});
            run(model,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});
            run(model,"create_account",{{"id","3"},{"name","Card"},{"currency","CAD"},{"date","2026-01-01"}});
            window->show();QVERIFY(QTest::qWaitForWindowExposed(window));QTest::qWait(100);
            QCOMPARE(order(model),(QStringList{"1","2","3"}));
            const auto point=[&](const char* name,int y){auto* row=qobject_cast<QQuickItem*>(object(root,name));return row->mapToScene(QPointF(50,y)).toPoint();};
            auto start=point("accountRow_3",16);auto target=point("accountRow_1",2);
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,target,50);
            QTRY_VERIFY(object(root,"accountDragPreview")->property("visible").toBool());
            QVERIFY(object(root,"accountDropIndicator")->property("visible").toBool());
            QCOMPARE(root->property("accountDropIndex").toInt(),0);
            QVERIFY(qAbs(qobject_cast<QQuickItem*>(object(root,"accountDropIndicator"))->mapToScene(QPointF(0,0)).y()-(point("accountRow_1",0).y()-1.5))<1);
            if(qEnvironmentVariableIsSet("SPENTIT_ACCOUNT_DRAG_CAPTURE")){QTest::qWait(100);QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_ACCOUNT_DRAG_CAPTURE")));}
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,target);
            QTRY_COMPARE(order(model),(QStringList{"3","1","2"}));
            QCOMPARE(model.state().value("accountId").toString(),QString("3"));
            QTest::qWait(50);
            start=point("accountRow_3",16);target=point("accountRow_2",50);
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,target,50);
            QTRY_VERIFY(object(root,"accountDragPreview")->property("visible").toBool());
            QTest::keyClick(window,Qt::Key_Escape);
            QTRY_VERIFY(!object(root,"accountDragPreview")->property("visible").toBool());
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,target);
            QCOMPARE(order(model),(QStringList{"3","1","2"}));
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,QPoint(600,300),50);
            QVERIFY(!object(root,"accountDropIndicator")->property("visible").toBool());
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,QPoint(600,300));
            QCOMPARE(order(model),(QStringList{"3","1","2"}));
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point("accountRow_3",16));
            target=point("accountRow_2",60);QTest::mouseMove(window,target,50);
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,target);
            QTRY_COMPARE(order(model),(QStringList{"1","2","3"}));QTest::qWait(50);
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point("accountRow_3",16));
            target=point("accountRow_1",2);QTest::mouseMove(window,target,50);
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,target);
            QTRY_COMPARE(order(model),(QStringList{"3","1","2"}));QTest::qWait(50);
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point("accountRow_1",16));
            QTRY_COMPARE(model.state().value("accountId").toString(),QString("1"));QTRY_VERIFY(!model.busy());
            QCOMPARE(order(model),(QStringList{"3","1","2"}));
            QVERIFY(!model.reorderAccounts({"1","1","2"}));QVERIFY(!model.error().isEmpty());
            QCOMPARE(order(model),(QStringList{"3","1","2"}));
            run(model,"close");
        }
        {
            ImportModel model(spentit::storage::document_gateway());run(model,"open",{{"path",path}});
            QCOMPARE(order(model),(QStringList{"3","1","2"}));
            run(model,"create_account",{{"id","4"},{"name","New"},{"currency","CAD"},{"date","2026-01-01"}});
            QCOMPARE(order(model),(QStringList{"3","1","2","4"}));
            run(model,"close");run(model,"create",{{"path",dir.filePath("other")},{"protected",false},{"name","Other"},{"currency","CAD"}});
            QCOMPARE(order(model),(QStringList{"1"}));run(model,"close");
            run(model,"open",{{"path",path}});QCOMPARE(order(model),(QStringList{"3","1","2","4"}));run(model,"close");
        }
    }
    void firstSplitTransferNavigation() {
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const QVariantMap first{{"counterpartAccount","2"}},second{{"counterpartAccount","3"}};
        auto state=model.state();
        state["transferNavigation"]=QVariantMap{{"synthetic:0",QVariantMap{{"z",first},{"a",second}}}};
        QVERIFY(root->setProperty("s",state));
        const auto destination=[&](const QStringList& ids) {
            QVariantList splits;for(const auto& id:ids)splits.push_back(QVariantMap{{"allocationId",id}});
            QVariant result;const QVariant record=QVariantMap{{"batch","synthetic"},{"ordinal",0},{"splits",splits}};
            if(!QMetaObject::invokeMethod(root,"firstLedgerTransfer",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,record)))return QVariantMap{{"error",true}};
            return result.metaType()==QMetaType::fromType<QJSValue>()?result.value<QJSValue>().toVariant().toMap():result.toMap();
        };
        QCOMPARE(destination({"fee","z","a"}),first);
        QCOMPARE(destination({"fee","a","z"}),second);
        QCOMPARE(destination({"z"}),first);
        QVERIFY(destination({"fee"}).isEmpty());
        QVERIFY(destination({}).isEmpty());
    }
    void newAccountScroll_data() {
        QTest::addColumn<int>("accounts");
        QTest::newRow("short-list")<<1;
        QTest::newRow("scrolling-list")<<16;
    }
    void newAccountScroll() {
        QFETCH(int,accounts);
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        run(model,"create",{{"path",dir.filePath("accounts")},{"protected",false},{"name","First"},{"currency","CAD"}});
        for(int i=2;i<=accounts;++i)run(model,"create_account",{{"id",QString::number(i)},{"name",QString("Account %1").arg(i)},{"currency","CAD"},{"date","2026-01-01"}});
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* scroll=qobject_cast<QQuickItem*>(object(root,"accountScroll"));QVERIFY(scroll);
        auto* flick=scroll->property("contentItem").value<QObject*>();QVERIFY(flick);
        auto* field=qobject_cast<QQuickItem*>(object(root,"newAccountName"));QVERIFY(field);
        const auto bounded=[&](){const auto y=flick->property("contentY").toReal();return y>=-1 && y<=qMax(qreal(0),flick->property("contentHeight").toReal()-flick->property("height").toReal())+1;};
        const auto visible=[&](QQuickItem* item){const auto y=item->mapToItem(scroll,QPointF(0,0)).y();return y>=-1 && y+item->height()<=scroll->height()+1;};
        for(bool save:{false,true}) {
            QVERIFY(click(root,"newAccount"));QTRY_VERIFY(field->hasActiveFocus());
            QTRY_VERIFY(bounded());QTRY_VERIFY(visible(field));
            if(accounts==1){QTRY_COMPARE(flick->property("contentY").toReal(),qreal(0));QVERIFY(visible(qobject_cast<QQuickItem*>(object(root,"accountRow_1"))));}
            if(save){field->setProperty("text","Added account");QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!model.busy() && !root->property("addingAccount").toBool());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));}
            else {QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!root->property("addingAccount").toBool());}
            QTRY_VERIFY(bounded());
        }
        QCOMPARE(model.state().value("accounts").toList().size(),accounts+1);
    }
    void sidebarAccountDragScroll() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        run(model,"create",{{"path",dir.filePath("scroll")},{"protected",false},{"name","First"},{"currency","CAD"}});
        for(int i=2;i<=16;++i)run(model,"create_account",{{"id",QString::number(i)},{"name",QString("Account %1").arg(i)},{"currency","CAD"},{"date","2026-01-01"}});
        window->show();QVERIFY(QTest::qWaitForWindowExposed(window));QTest::qWait(100);
        auto* scroll=qobject_cast<QQuickItem*>(object(root,"accountScroll"));QVERIFY(scroll);
        auto* flick=scroll->property("contentItem").value<QObject*>();QVERIFY(flick);
        auto* first=qobject_cast<QQuickItem*>(object(root,"accountRow_1"));QVERIFY(first);
        const auto start=first->mapToScene(QPointF(50,16)).toPoint();
        const auto edge=scroll->mapToScene(QPointF(50,scroll->height()-12)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,edge,50);
        QTRY_VERIFY(flick->property("contentY").toReal()>0);
        QTRY_VERIFY_WITH_TIMEOUT(flick->property("contentY").toReal()>=flick->property("contentHeight").toReal()-flick->property("height").toReal()-1,10000);
        auto* last=qobject_cast<QQuickItem*>(object(root,"accountRow_16"));QVERIFY(last);
        const auto target=last->mapToScene(QPointF(50,last->height()-2)).toPoint();
        QTest::mouseMove(window,target,50);QCOMPARE(root->property("accountDropIndex").toInt(),16);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,target);
        QTRY_COMPARE(model.state().value("sidebarAccounts").toList().last().toMap().value("id").toString(),QString("1"));
        QCOMPARE(model.state().value("accountId").toString(),QString("16"));run(model,"close");
    }
    void accountSortSurvivesRestart() {
        QTemporaryDir dir;const auto path=dir.filePath("sort-document");
        const auto checkSort=[](ImportModel& model,int column,bool descending){QCOMPARE(model.state().value("sort").toInt(),column);QCOMPARE(model.state().value("descending").toBool(),descending);};
        {
            ImportModel model(spentit::storage::document_gateway());
            run(model,"create",{{"path",path},{"protected",false},{"name","First"},{"currency","CAD"}});checkSort(model,0,true);
            run(model,"opening",{{"date","2025-12-31"},{"amount","0.00"}});
            QFile file(dir.filePath("sort.qif"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("D1/1/2026\nT-1\nPAlpha\n^\nD2/1/2026\nT-2\nPZulu\n^\n");file.close();
            run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});QCOMPARE(model.rowRecord(0).value("payee").toString(),QString("Zulu"));
            run(model,"query",{{"sort",1},{"descending",true}});checkSort(model,1,true);QCOMPARE(model.rowRecord(0).value("payee").toString(),QString("Zulu"));
            QSettings preferences;preferences.sync();const auto keys=preferences.allKeys().filter("LedgerSort/");QCOMPARE(keys.size(),1);
            QCOMPARE(preferences.value(keys.first()).toList(),(QVariantList{1,true})); // saved before quitting
            run(model,"create_account",{{"id","2"},{"name","Second"},{"currency","CAD"},{"date","2026-01-01"}});checkSort(model,0,true);
            run(model,"query",{{"sort",0},{"descending",true}});checkSort(model,0,true);
            run(model,"select_account",{{"id","1"}});checkSort(model,1,true);
            run(model,"select_account",{{"id","2"}});checkSort(model,0,true);
            run(model,"query",{{"sort",0},{"descending",false}});checkSort(model,0,false);
            run(model,"close");
        }
        {
            ImportModel model(spentit::storage::document_gateway());run(model,"open",{{"path",path}});
            run(model,"select_account",{{"id","1"}});checkSort(model,1,true);QCOMPARE(model.rowRecord(0).value("payee").toString(),QString("Zulu"));
            run(model,"rename_account",{{"name","Renamed"}});checkSort(model,1,true);
            run(model,"select_account",{{"id","2"}});checkSort(model,0,false);
            run(model,"query",{{"sort",4},{"descending",true}});run(model,"close");
            run(model,"create",{{"path",dir.filePath("other")},{"protected",false},{"name","Other"},{"currency","CAD"}});checkSort(model,0,true);run(model,"close");
            run(model,"open",{{"path",path}});run(model,"select_account",{{"id","2"}});checkSort(model,4,true);
            run(model,"close");
            // Replacing a file is a new set of accounts, not an old preference.
            run(model,"create",{{"path",path},{"overwrite",true},{"protected",false},{"name","Replacement"},{"currency","CAD"}});checkSort(model,0,true);run(model,"close");
        }
    }
    void categorySubmenuScrolls() {
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);window->show();QVERIFY(QTest::qWaitForWindowExposed(window));
        QVariantList categories;for(int i=0;i<80;++i)categories.push_back(QVariantMap{{"id",QString::number(i)},{"path",QString("Category %1").arg(i,2,10,QChar('0'))},{"assignable",true}});
        root->setProperty("s",QVariantMap{{"active",true},{"selectionCount",1},{"transfersSupported",true},{"categories",categories}});
        auto* context=object(root,"ledgerContext");QVERIFY(context);QVERIFY(showPopup(context));QTRY_VERIFY(context->property("visible").toBool());
        auto* menu=object(root,"ledgerCategoryMenu");QVERIFY(menu);
        // Hover the parent menu entry, as the owner requested.
        auto* content=qvariant_cast<QQuickItem*>(context->property("contentItem"));QVERIFY(content);
        QQuickItem* entry=nullptr;for(auto* item:content->childItems())for(auto* child:item->childItems())if(child->property("text").toString()=="Change category")entry=child;
        QVERIFY(entry);QTest::qWait(100);QTest::mouseMove(window,entry->mapToScene(QPointF(entry->width()/2,entry->height()/2)).toPoint());
        QTRY_VERIFY(menu->property("visible").toBool());QVERIFY(menu->property("height").toReal()<=420);
        auto* bar=object(root,"ledgerCategoryScrollBar");QVERIFY(bar);QVERIFY(bar->property("size").toReal()<1);QVERIFY(bar->property("interactive").toBool());
        if(qEnvironmentVariableIsSet("SPENTIT_CATEGORY_MENU_SCREENSHOT"))QVERIFY(window->grabWindow().save(qEnvironmentVariable("SPENTIT_CATEGORY_MENU_SCREENSHOT")));
    }
    void acceptedManualTransferAssignment() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();const auto path=dir.filePath("document");
        run(model,"create",{{"path",path},{"protected",false},{"name","Checking"},{"currency","CAD"}});
        QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(!model.busy());QVERIFY(edit(root,"durablePayee","Manual transfer"));QVERIFY(edit(root,"durableTotal","-10.00"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"durableTotal"),"accepted"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        const auto batch=model.state().value("selected").toMap().value("batch").toString();QVERIFY(!batch.isEmpty());run(model,"commit");
        run(model,"create_account",{{"id","2"},{"name","Savings"},{"currency","CAD"},{"date","2026-01-01"}});run(model,"select_account",{{"id","1"}});
        run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});
        run(model,"assign_transfer_batch",{{"destination","2"},{"sequence",model.state().value("transferSequence")}});
        QVERIFY(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString().contains("Savings"));
        // Ordinary assignment Undo belongs to this session.
        QVERIFY(model.state().value("canUndoTransferAssignment").toBool());
        run(model,"undo_transfer_assignment");
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),QString("Uncategorized"));
        run(model,"select",{{"row",model.occurrenceIndex(batch,0)}});
        run(model,"assign_transfer_batch",{{"destination","2"},{"sequence",model.state().value("transferSequence")}});
        QVERIFY(model.state().value("canUndoTransferAssignment").toBool());
        const auto saved=model.rowRecord(model.occurrenceIndex(batch,0));
        QVERIFY(saved.value("category").toString().contains("Savings"));
        QCOMPARE(saved.value("status").toString(),QString("Committed"));
        run(model,"close");run(model,"open",{{"path",path}});run(model,"select_account",{{"id","1"}});
        // Successful close retains the financial state, but expires ordinary Undo.
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)),saved);
        QVERIFY(!model.state().value("canUndoTransferAssignment").toBool());
        model.command("undo_transfer_assignment");QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY(model.error().contains("Later changes prevent transfer assignment Undo"));
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)),saved);
    }
    void categoryMenuSavesMixedRowsAndReopens() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* root=engine.rootObjects().first();
        const auto path=dir.filePath("document");
        run(model,"create",{{"path",path},{"protected",false},{"name","Category account"},{"currency","CAD"}});
        auto import=[&](QString name){QFile f(dir.filePath(name+".qif"));if(!f.open(QIODevice::WriteOnly))return QString();f.write(("D1/1/2026\nT-1\nP"+name+"\nLBefore\n^\n").toUtf8());f.close();run(model,"import",{{"path",f.fileName()},{"profile","DMY"}});return model.state().value("selected").toMap().value("batch").toString();};
        const auto accepted=import("Accepted");QVERIFY(!accepted.isEmpty());run(model,"commit");
        const auto pending=import("Pending");QVERIFY(!pending.isEmpty());
        QVERIFY(click(root,"addTransaction"));QTRY_VERIFY(!model.busy());
        QVERIFY(edit(root,"durablePayee","Manual category test"));
        QVERIFY(QMetaObject::invokeMethod(object(root,"durableTotal"),"accepted"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        const auto manualBatch=model.state().value("selected").toMap().value("batch").toString();QVERIFY(!manualBatch.isEmpty());
        auto* manualContext=object(root,"ledgerContext");QVERIFY(manualContext);QVERIFY(showPopup(manualContext));QTRY_VERIFY(manualContext->property("visible").toBool());
        auto* manualMenu=object(root,"ledgerCategoryMenu");QVERIFY(manualMenu);
        QVERIFY(QMetaObject::invokeMethod(manualMenu,"open"));QTRY_VERIFY(manualMenu->property("visible").toBool());
        QObject* beforeChoice=nullptr;
        for(int i=0;i<20;++i){auto* candidate=object(root,("ledgerCategoryChoice_"+QString::number(i)).toUtf8().constData());if(candidate && candidate->property("text").toString()=="Before"){beforeChoice=candidate;break;}}
        QVERIFY(beforeChoice);QVERIFY(QMetaObject::invokeMethod(beforeChoice,"triggered"));QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(model.rowRecord(model.occurrenceIndex(manualBatch,0)).value("category").toString(),QString("Before"));
        QMap<QString,QString> original;for(const auto& batch:{accepted,pending,manualBatch})original[batch]=model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString();
        run(model,"select",{{"row",model.occurrenceIndex(accepted,0)}});run(model,"select",{{"row",model.occurrenceIndex(pending,0)},{"toggle",true}});
        run(model,"select",{{"row",model.occurrenceIndex(manualBatch,0)},{"toggle",true}});
        auto* menu=object(root,"ledgerCategoryMenu");QVERIFY(menu);QVERIFY(menu->property("enabled").toBool());
        auto* context=object(root,"ledgerContext");QVERIFY(context);QVERIFY(showPopup(context));QTRY_VERIFY(context->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu,"open"));QTRY_VERIFY(menu->property("visible").toBool());
        auto* choice=object(root,"ledgerCategoryChoice_0");QVERIFY(choice);
        QVERIFY(QMetaObject::invokeMethod(choice,"triggered"));QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        for(const auto& batch:{accepted,pending,manualBatch})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),QString("Uncategorized"));
        QCOMPARE(model.state().value("selectionCount").toInt(),3);
        QVERIFY(model.state().value("canUndoTransferAssignment").toBool());
        run(model,"undo_transfer_assignment");
        for(const auto& batch:{accepted,pending,manualBatch})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)).value("category").toString(),original[batch]);
        // Reapply through the menu so the changed mixed selection becomes the baseline.
        run(model,"select",{{"row",model.occurrenceIndex(accepted,0)}});
        run(model,"select",{{"row",model.occurrenceIndex(pending,0)},{"toggle",true}});
        run(model,"select",{{"row",model.occurrenceIndex(manualBatch,0)},{"toggle",true}});
        QVERIFY(showPopup(context));QTRY_VERIFY(context->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu,"open"));QTRY_VERIFY(menu->property("visible").toBool());
        choice=object(root,"ledgerCategoryChoice_0");QVERIFY(choice);
        QVERIFY(QMetaObject::invokeMethod(choice,"triggered"));QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(model.state().value("canUndoTransferAssignment").toBool());
        QMap<QString,QVariantMap> saved;
        for(const auto& batch:{accepted,pending,manualBatch}){
            saved[batch]=model.rowRecord(model.occurrenceIndex(batch,0));
            QCOMPARE(saved[batch].value("category").toString(),QString("Uncategorized"));
        }
        QCOMPARE(saved[accepted].value("status").toString(),QString("Committed"));
        QVERIFY(saved[pending].value("status").toString()!="Committed");
        QVERIFY(saved[manualBatch].value("status").toString()!="Committed");
        run(model,"close");run(model,"open",{{"path",path}});
        for(const auto& batch:{accepted,pending,manualBatch})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)),saved[batch]);
        QVERIFY(!model.state().value("canUndoTransferAssignment").toBool());
        model.command("undo_transfer_assignment");QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
        QVERIFY(model.error().contains("Later changes prevent transfer assignment Undo"));
        for(const auto& batch:{accepted,pending,manualBatch})QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,0)),saved[batch]);
    }
    void ledgerViewportSurvivesCategoryNavigation() {
        QTemporaryDir dir;ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));
        run(model,"create",{{"path",dir.filePath("document")},{"protected",false},{"name","Viewport account"},{"currency","CAD"}});
        QFile file(dir.filePath("rows.qif"));QVERIFY(file.open(QIODevice::WriteOnly));QByteArray rows;
        for(int i=0;i<300;++i)rows+="D1/1/2026\nT-1\nPSynthetic "+QByteArray::number(i).rightJustified(3,'0')+"\n^\n";
        QCOMPARE(file.write(rows),rows.size());file.close();run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto account=model.state().value("accountId").toString();const auto batch=model.state().value("selected").toMap().value("batch").toString();
        run(model,"inspect",{{"batch",batch},{"ordinal",260}});
        auto* ledger=root->findChild<QQuickItem*>("durableLedger");QVERIFY(ledger);
        auto position=[&](int index){return QMetaObject::invokeMethod(ledger,"positionViewAtIndex",Q_ARG(int,index),Q_ARG(int,0));};
        auto snapshot=[&](){QMetaObject::invokeMethod(root,"rememberAccountViewport");return root->property("accountViewports").value<QJSValue>().toVariant().toMap().value(model.state().value("accountId").toString()).toMap();};
        auto first=[&](){int index=-1;QMetaObject::invokeMethod(ledger,"indexAt",Q_RETURN_ARG(int,index),Q_ARG(qreal,1),Q_ARG(qreal,ledger->property("contentY").toReal()+1));return index;};
        QTest::qWait(50);QVERIFY(position(model.occurrenceIndex(batch,260)));QTest::qWait(50);
        ledger->setProperty("contentY",ledger->property("contentY").toReal()+17);const auto saved=snapshot();QVERIFY(saved.value("index").toInt()>200);
        const auto selected=model.state().value("selected").toMap();
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());QCOMPARE(root->property("currentPage").toString(),"categories");
        QVERIFY(click(root,"newCategory"));QVERIFY(edit(root,"categoryName","New expense"));QVERIFY(QMetaObject::invokeMethod(root,"finishCategoryEdit"));
        QTRY_VERIFY(!model.busy() && !root->property("categoryDirty").toBool());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QVERIFY(click(root,"navigationBack"));QCOMPARE(root->property("currentPage").toString(),"ledger");
        QTRY_COMPARE(first(),saved.value("index").toInt());
        QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        QCOMPARE(model.state().value("selected").toMap().value("batch"),selected.value("batch"));QCOMPARE(model.state().value("selected").toMap().value("ordinal"),selected.value("ordinal"));
        QVERIFY(!root->property("editorClosed").toBool());
        // A category picked for a new transaction saves immediately, without
        // the old committed-only bulk preview or a jump to the top.
        run(model,"select",{{"row",model.occurrenceIndex(batch,260)}});
        QVERIFY(position(model.occurrenceIndex(batch,250)));QTest::qWait(50);
        const auto categoryView=snapshot();
        const auto categories=model.state().value("categories").toList();QVERIFY(!categories.isEmpty());
        QVariantMap category;for(const auto& c:categories)if(c.toMap().value("path")=="New expense")category=c.toMap();QVERIFY(!category.isEmpty());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseLedgerCategory",Q_ARG(QVariant,category)));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,260)).value("category").toString(),QString("New expense"));
        QTRY_COMPARE(first(),categoryView.value("index").toInt());
        QVERIFY(!object(root,"bulkCategoryDialog")->property("visible").toBool());
        QVERIFY(!object(root,"bulkDialog")->property("visible").toBool());
        QCOMPARE(model.state().value("selectionCount").toInt(),1);
        run(model,"undo_transfer_assignment");
        QCOMPARE(model.rowRecord(model.occurrenceIndex(batch,260)).value("category").toString(),QString("Uncategorized"));
        run(model,"inspect",{{"batch",batch},{"ordinal",260}});QTest::qWait(50);
        QVERIFY(position(saved.value("index").toInt()));QTest::qWait(50);
        ledger->setProperty("contentY",ledger->property("contentY").toReal()+saved.value("offset").toReal());
        // Repeated visits must not overwrite the saved position from the hidden,
        // reset list; returning through the account button uses the same anchor.
        QVERIFY(click(root,"navigationForward"));QTRY_VERIFY(!model.busy());run(model,"category_list");
        QVERIFY(click(root,"accountNavigation"));QTRY_COMPARE(first(),saved.value("index").toInt());
        QTRY_VERIFY(qAbs(snapshot().value("offset").toReal()-saved.value("offset").toReal())<2);
        // Keep independent positions when switching accounts, including while
        // leaving a non-ledger page, and through cross-account Back/Forward.
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());
        run(model,"create_account",{{"id","2"},{"name","Second viewport"},{"currency","CAD"},{"date","2026-01-01"}});
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        const auto secondBatch=model.state().value("selected").toMap().value("batch").toString();
        run(model,"inspect",{{"batch",secondBatch},{"ordinal",40}});QTest::qWait(50);QVERIFY(position(model.occurrenceIndex(secondBatch,40)));QTest::qWait(50);
        const auto second=snapshot();
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,account)));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,QString("2"))));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),second.value("index").toInt());
        QVERIFY(click(root,"navigationBack"));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        QVERIFY(click(root,"navigationForward"));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),second.value("index").toInt());
        QVERIFY(QMetaObject::invokeMethod(root,"chooseAccount",Q_ARG(QVariant,account)));QTRY_VERIFY(!model.busy());QTRY_COMPARE(first(),saved.value("index").toInt());
        // A vanished anchor and a shorter filtered ledger clamp to a valid end.
        QVERIFY(click(root,"categoryNavigation"));QTRY_VERIFY(!model.busy());
        run(model,"query",{{"filter","Synthetic 00"},{"sort",0},{"descending",false}});
        QVERIFY(click(root,"navigationBack"));QTRY_VERIFY(first()>=0 && first()<20);
        QTRY_VERIFY(ledger->property("contentY").toReal()<=ledger->property("originY").toReal()+qMax(qreal(0),ledger->property("contentHeight").toReal()-ledger->height())+1);
        QCOMPARE(model.state().value("filter").toString(),"Synthetic 00");
        run(model,"close");QVERIFY(root->property("accountViewports").value<QJSValue>().toVariant().toMap().isEmpty());
    }
    void guardedNavigationAndIsolation() {
        QTemporaryDir dir;
        ImportModel model(spentit::storage::document_gateway());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
        engine.load(QUrl("qrc:/spentit/ImportMain.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* root=engine.rootObjects().first();auto* window=qobject_cast<QQuickWindow*>(root);QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto path=dir.path()+"/document";
        run(model,"create",{{"path",path},{"protected",false},{"name","Synthetic navigation"},{"currency","CAD"}});
        QFile file(dir.path()+"/source.qif");QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes="D1/1/2026\nT-1\nPOriginal\n^\n";QCOMPARE(file.write(bytes),bytes.size());file.close();
        run(model,"import",{{"path",file.fileName()},{"profile","DMY"}});
        if(model.diagnosticsEnabled()) {
            const auto record=model.rowIdentity(0);
            model.command("select",{{"row",0}});
            const auto selectedOperation=spentit::diagnostics::Collector::instance().snapshot().active.id;
            model.command("history");
            model.command("inspect",record);const auto deferred=spentit::diagnostics::Collector::instance().snapshot().deferred;
            model.command("inspect",record);
            QVERIFY(deferred!=0);QVERIFY(spentit::diagnostics::Collector::instance().snapshot().deferred!=deferred);
            auto observation=spentit::diagnostics::Collector::instance().snapshot();
            QCOMPARE(observation.outcomes[deferred%32].result,spentit::diagnostics::Result::superseded);
            QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),30000);
            QVERIFY(spentit::diagnostics::Collector::instance().snapshot().latest.id!=selectedOperation);
            QTRY_COMPARE(spentit::diagnostics::Collector::instance().snapshot().cleanup,0ULL);
        }
        const auto identity=model.rowIdentity(model.selectedRowIndex());
        QCOMPARE(root->property("currentPage").toString(),"ledger");
        const auto title=[&]{return root->findChild<QObject*>("workspaceTitle")->property("text").toString();};
        const auto ledgerTitle=model.state().value("name").toString()+" · "+model.state().value("currency").toString();
        QCOMPARE(title(),ledgerTitle);
        QCOMPARE(root->findChild<QObject*>("addTransaction")->property("text").toString(),QString("+ New transaction"));
        auto* toggle=root->findChild<QQuickItem*>("sidebarToggle");QVERIFY(toggle);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,toggle->mapToScene(QPointF(toggle->width()/2,toggle->height()/2)).toPoint());
        QTRY_VERIFY(!root->property("sidebarVisible").toBool());
        QVERIFY(click(root,"sidebarToggle"));QVERIFY(root->property("sidebarVisible").toBool());
        QVERIFY(click(root,"sidebarSettings"));QCOMPARE(root->property("currentPage").toString(),"settings");QCOMPARE(title(),QString("Settings"));
        QCOMPARE(model.rowIdentity(model.selectedRowIndex()),identity);
        QVERIFY(click(root,"navigationBack"));QCOMPARE(root->property("currentPage").toString(),"ledger");QCOMPARE(title(),ledgerTitle);
        QVERIFY(click(root,"navigationForward"));QCOMPARE(root->property("currentPage").toString(),"settings");QCOMPARE(title(),QString("Settings"));
        QVERIFY(click(root,"navigationBack"));
        window->resize(1100,750);QTest::qWait(100);
        auto* sidebar=root->findChild<QQuickItem*>("accountSidebar");QVERIFY(sidebar);
        QVERIFY(sidebar->width()>=140);QVERIFY(sidebar->width()<=360);
        auto* settingsButton=root->findChild<QQuickItem*>("sidebarSettings");QVERIFY(settingsButton);
        const auto settingsCenter=settingsButton->mapToScene(QPointF(settingsButton->width()/2,settingsButton->height()/2));
        QVERIFY(settingsCenter.x()>0 && settingsCenter.x()<window->width());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,settingsCenter.toPoint());
        QTRY_COMPARE(root->property("currentPage").toString(),"settings");
        QVERIFY(click(root,"navigationBack"));
        window->resize(1440,850);QTest::qWait(100);
        QVERIFY(edit(root,"durablePayee","Unsaved"));QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"sidebarSettings"));
        auto* dialog=root->findChild<QObject*>("unsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(click(root,"guardStay"));QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(edit(root,"durableTotal","invalid"));QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardSave"));
        QTRY_VERIFY(!model.busy());QVERIFY(!root->property("splitEditMessage").toString().isEmpty());
        QCOMPARE(root->property("currentPage").toString(),"ledger");QVERIFY(root->property("dirty").toBool());
        QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardDiscard"));
        QCOMPARE(root->property("currentPage").toString(),"settings");QCOMPARE(title(),QString("Settings"));QVERIFY(!root->property("dirty").toBool());
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Original");
        QVERIFY(click(root,"navigationBack"));
        QVERIFY(edit(root,"durablePayee","Saved navigation"));QVERIFY(click(root,"sidebarSettings"));QVERIFY(click(root,"guardSave"));
        QTRY_VERIFY(!model.busy());QVERIFY2(model.error().isEmpty(),qPrintable(model.error()));
        QCOMPARE(root->property("currentPage").toString(),"settings");QCOMPARE(title(),QString("Settings"));
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Saved navigation");
        QVERIFY(QMetaObject::invokeMethod(object(root,"fileCloseDataset"),"triggered"));QTRY_VERIFY(!model.busy());QVERIFY(!model.state().value("active").toBool());
        QCOMPARE(root->property("currentPage").toString(),"ledger");QCOMPARE(root->property("navigationIndex").toInt(),0);
        run(model,"create",{{"path",dir.path()+"/second"},{"protected",false},{"name","Other dataset"},{"currency","CAD"}});
        QVERIFY(model.state().value("selected").toMap().isEmpty());QCOMPARE(root->property("navigationIndex").toInt(),0);
        QVERIFY(!root->findChild<QObject*>("navigationBack")->property("enabled").toBool());
        run(model,"close");run(model,"open",{{"path",path}});
        run(model,"inspect",identity);
        QCOMPARE(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(),"Saved navigation");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    NavigationTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "import_navigation_tests.moc"
