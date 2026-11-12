#include "help/help_catalog.hpp"
#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QColor>
#include <QClipboard>
#include <QSignalSpy>
#include <QScopeGuard>
#include <functional>
#include <QElapsedTimer>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QStyleHints>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include "controls_style.hpp"
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QtTest/qtestwheel.h>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextLayout>
#include <QWindow>
using namespace spentit;
#ifdef Q_OS_MACOS
bool triggerNativeAbout();
#endif

class HelpUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    struct Session {
        QTemporaryDir dir;
        ImportModel model;
        QQmlApplicationEngine engine;
        QObject* root = nullptr;
        QQuickWindow* window = nullptr;
        explicit Session(HelpCatalog* catalog) : model(storage::document_gateway()) {
            engine.rootContext()->setContextProperty("workflow", &model);
            if (catalog) engine.rootContext()->setContextProperty("helpCatalog", catalog);
            engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
            if (!engine.rootObjects().isEmpty()) {
                root = engine.rootObjects().first();
                window = qobject_cast<QQuickWindow*>(root);
            }
        }
        QString path(const QString& name = "document") const { return dir.filePath(name); }
    };
    static QQuickItem* visual(QQuickItem* item, const QString& name) {
        if (!item) return nullptr;
        if (item->objectName() == name) return item;
        for (auto* child : item->childItems())
            if (auto* found = visual(child, name)) return found;
        return nullptr;
    }
    static QObject* object(QObject* root, const QString& name) {
        if (!root) return nullptr;
        if (auto* item = root->findChild<QObject*>(name)) return item;
        auto* window = qobject_cast<QQuickWindow*>(root);
        return window ? visual(window->contentItem(), name) : nullptr;
    }
    static QQuickWindow* namedWindow(const QString& name) {
        for (QWindow* window : QGuiApplication::allWindows())
            if (window->objectName() == name) return qobject_cast<QQuickWindow*>(window);
        return nullptr;
    }
    static bool click(QObject* root, const QString& name) {
        auto* item = object(root, name);
        return item && item->property("enabled").toBool() && item->property("visible").toBool() && QMetaObject::invokeMethod(item, "clicked");
    }
    static bool edit(QObject* root, const QString& name, const QString& value) {
        auto* item = object(root, name);
        return item && item->setProperty("text", value) && QMetaObject::invokeMethod(item, "textEdited");
    }
    void wait(ImportModel& model, bool success = true) {
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 30000);
        QVERIFY2(model.error().isEmpty() == success, qPrintable(model.error()));
    }
    void run(ImportModel& model, const QString& action, const QVariantMap& args = {}, bool success = true) {
        model.command(action, args);
        wait(model, success);
    }
    void createCategory(ImportModel& model, QObject* root, const QString& name) {
        QVERIFY(click(root, "newCategory"));
        QTRY_VERIFY(object(root, "categoryName"));
        QVERIFY(edit(root, "categoryName", name));
        auto* field = qobject_cast<QQuickItem*>(object(root, "categoryName"));
        QVERIFY(field);
        field->forceActiveFocus();
        QTest::keyClick(qobject_cast<QQuickWindow*>(root), Qt::Key_Return);
        wait(model);
        QTRY_VERIFY(!root->property("categoryDirty").toBool());
    }
    void createDocument(Session& session, const QString& name) {
        run(session.model, "create", {{"path", session.path()}, {"protected", false}, {"name", name}, {"currency", "CAD"}});
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QVERIFY(click(session.root, "categoryNavigation"));
        wait(session.model);
        createCategory(session.model, session.root, "Test category");
        QVERIFY(click(session.root, "accountNavigation"));
        wait(session.model);
    }
    static QObject* textDocument(QObject* article) {
        const QVariant value = article->property("textDocument");
        if (auto* quick = value.value<QQuickTextDocument*>()) return quick;
        return value.value<QObject*>();
    }
    static QQuickWindow* openHelp(QObject* root) {
        if (!QMetaObject::invokeMethod(root, "openHelp")) return nullptr;
        for (int attempt = 0; attempt < 50 && !namedWindow("helpWindow"); ++attempt) QTest::qWait(20);
        auto* help = namedWindow("helpWindow");
        if (!help) return nullptr;
        for (int attempt = 0; attempt < 50 && help->property("topicId").toString().isEmpty(); ++attempt) QTest::qWait(20);
        return help->property("topicId").toString().isEmpty() ? nullptr : help;
    }
    static void pressEscape(QWindow* window) {
        window->requestActivate();
        (void)QTest::qWaitForWindowActive(window, 200);
        QTest::keyClick(window, Qt::Key_Escape);
    }
    static QString focusName(QQuickWindow* window) {
        const auto* item = window->activeFocusItem();
        return item ? item->objectName() : QString();
    }
    static bool tabTo(QQuickWindow* window, const QString& name, bool reverse = false) {
        for (int i = 0; i < 100; ++i) {
            auto* focus = window->activeFocusItem();
            if (focus && focus->objectName() == name) {
                QTest::qWait(30); // Let a newly created ListView delegate finish layout.
                if (window->activeFocusItem() == focus) return true;
            }
            QTest::keyClick(window, reverse ? Qt::Key_Backtab : Qt::Key_Tab);
            QTest::qWait(5);
        }
        return false;
    }
private slots:
    void initTestCase() {
        applySpentItControlsStyle();
        QCoreApplication::setOrganizationName("SpentItTests");
        QCoreApplication::setApplicationName("HelpUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    }
    void init() { QSettings().clear(); }
    void aboutBeforeOpeningAndOfflineNotices_data() {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void aboutBeforeOpeningAndOfflineNotices() {
        QFETCH(bool, dark);
        const auto originalScheme = QGuiApplication::styleHints()->colorScheme();
        QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
        const auto restore = qScopeGuard([&] { QGuiApplication::styleHints()->setColorScheme(originalScheme); });
        if (dark && QGuiApplication::platformName() == "offscreen")
            QSKIP("Dark appearance requires the native platform.");
        QSettings().setValue("datasetPath", settings.filePath("missing.spentit"));
        Session session(nullptr);
        QVERIFY(session.root);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QTRY_VERIFY(!session.model.busy());
        QVERIFY(!session.model.state().value("active").toBool());
        auto* menu = object(session.root, "openAbout");
        QVERIFY(menu);
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa") {
            session.window->requestActivate();
            QTRY_VERIFY(session.window->isActive());
            QTest::qWait(50);
            QVERIFY(triggerNativeAbout());
        } else
#endif
        QVERIFY(QMetaObject::invokeMethod(menu, "triggered"));
        auto* about = namedWindow("aboutWindow");
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        QCOMPARE(about->modality(), Qt::ApplicationModal);
        QCOMPARE(about->transientParent(), session.window);
        auto* info = session.model.aboutInformation();
        QVERIFY(!info->version().isEmpty());
        QVERIFY(!info->build().isEmpty());
        QVERIFY(click(about, "copyVersionInformation"));
        QCOMPARE(QGuiApplication::clipboard()->text(), info->versionInformation());
        QVERIFY(!info->versionInformation().contains(settings.path()));
        const auto captureBase = qEnvironmentVariable("SPENTIT_ABOUT_CAPTURE");
        const auto capture = captureBase.isEmpty() ? QString() : captureBase + (dark ? "-dark" : "-light");
        if (!capture.isEmpty()) {
            QTest::qWait(150);
            QVERIFY(about->grabWindow().save(capture + "-main.png"));
            about->setWidth(440);
            QTest::qWait(50);
            about->setHeight(about->minimumHeight());
            QTest::qWait(50);
            QVERIFY(about->grabWindow().save(capture + "-narrow.png"));
        }
        auto* close = qobject_cast<QQuickItem*>(object(about, "aboutClose"));
        QVERIFY(close);
        QVERIFY(close->mapToScene(QPointF(0, close->height())).y() <= about->height());
        QVERIFY(tabTo(about, "aboutLicense"));
        QTest::keyClick(about, Qt::Key_Space);
        QCOMPARE(about->property("page").toInt(), 1);
        auto* text = object(about, "aboutNoticeText");
        QVERIFY(text);
        QVERIFY(text->property("text").toString().contains("© 2026 Stefan Wessels"));
        QVERIFY(text->property("text").toString().contains("final distribution license has not yet been selected"));
        QVERIFY(text->property("readOnly").toBool());
        if (!capture.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(about->grabWindow().save(capture + "-terms.png"));
        }
        QVERIFY(click(about, "aboutBack"));
        QVERIFY(click(about, "aboutNotices"));
        QCOMPARE(about->property("page").toInt(), 2);
        const auto rows = info->components();
        QVERIFY(rows.size() >= 6);
        for (const auto& value : rows) {
            const auto entry = value.toMap();
            QVERIFY(!entry.value("label").toString().isEmpty());
            QVERIFY(entry.value("text").toString().size() > 100);
        }
        int qtIndex = -1;
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i].toMap().value("label").toString().startsWith("Qt — ")) qtIndex = i;
        QVERIFY(qtIndex >= 0);
        QVERIFY(object(about, "aboutComponents")->setProperty("currentIndex", qtIndex));
        QTRY_VERIFY(text->property("text").toString().contains("GNU LESSER GENERAL PUBLIC LICENSE"));
        QVERIFY(text->property("text").toString().contains("GNU GENERAL PUBLIC LICENSE"));
        if (!capture.isEmpty()) {
            QTest::qWait(150);
            QVERIFY(about->grabWindow().save(capture + "-notices.png"));
            auto* picker = object(about, "aboutComponents");
            auto* popup = picker->property("popup").value<QObject*>();
            QVERIFY(popup);
            QVERIFY(QMetaObject::invokeMethod(popup, "open"));
            QTest::qWait(100);
            QVERIFY(about->grabWindow().save(capture + "-components.png"));
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        }
        // Escape returns from reading, then closes About, without touching the file.
        about->requestActivate();
        QTRY_VERIFY(about->isActive());
        QTest::keyClick(about, Qt::Key_Escape);
        QTRY_COMPARE(about->property("page").toInt(), 0);
        QTest::keyClick(about, Qt::Key_Escape);
        QTRY_VERIFY(!about->isVisible());
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void aboutPreservesUnsavedTransaction() {
        Session session(nullptr);
        QVERIFY(session.root);
        run(session.model, "create", {{"path", session.path()}, {"protected", false}, {"name", "About test"}, {"currency", "CAD"}});
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QVERIFY(click(session.root, "addTransaction"));
        wait(session.model);
        QVERIFY(edit(session.root, "durablePayee", "Unsaved payee"));
        QVERIFY(edit(session.root, "durableTotal", "1+2"));
        auto* field = qobject_cast<QQuickItem*>(object(session.root, "durableTotal"));
        QVERIFY(field);
        field->forceActiveFocus();
        const auto state = session.model.state();
        QSignalSpy completed(&session.model, &ImportModel::completed);
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa") QVERIFY(triggerNativeAbout());
        else
#endif
        QVERIFY(QMetaObject::invokeMethod(session.root, "openAbout"));
        auto* about = namedWindow("aboutWindow");
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        QVERIFY(click(about, "aboutLicense"));
        QVERIFY(click(about, "aboutClose"));
        QTRY_VERIFY(!about->isVisible());
        QTRY_VERIFY(field->hasActiveFocus());
        QCOMPARE(field->property("text").toString(), QString("1+2"));
        QCOMPARE(object(session.root, "durablePayee")->property("text").toString(), QString("Unsaved payee"));
        QVERIFY(session.root->property("dirty").toBool());
        QCOMPARE(session.model.state(), state);
        QCOMPARE(completed.count(), 0);
    }
    void keyboardHelp_data() {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<int>("width");
        QTest::newRow("light-wide") << false << 880;
        QTest::newRow("light-narrow") << false << 520;
        QTest::newRow("dark-wide") << true << 880;
        QTest::newRow("dark-narrow") << true << 520;
    }
    void keyboardHelp() {
        QFETCH(bool, dark); QFETCH(int, width);
        const auto originalScheme = QGuiApplication::styleHints()->colorScheme();
        QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
        const auto restore = qScopeGuard([&] { QGuiApplication::styleHints()->setColorScheme(originalScheme); });
        if (dark && QGuiApplication::platformName() == "offscreen")
            QSKIP("The offscreen platform does not implement color-scheme overrides; dark cases run natively.");
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        QSettings().setValue("datasetPath", settings.filePath("keyboard-missing.spentit"));
        Session session(&catalog);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        // Opening is setup; every interaction inside Help below uses keys only.
        auto* help = openHelp(session.root); QVERIFY(help);
        help->setWidth(width);
        help->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(help));
        QTRY_COMPARE(help->color(), QColor(dark ? "#171c22" : "#f6f7f9"));
        QVERIFY(tabTo(help, "helpNext"));
        QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        QVERIFY(tabTo(help, "helpBack", true));
        QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        QVERIFY(tabTo(help, "helpForward")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        QVERIFY(tabTo(help, "helpHome")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        QVERIFY(tabTo(help, "helpNext")); QTest::keyClick(help, Qt::Key_Space);
        QVERIFY(tabTo(help, "helpPrevious", true)); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        QVERIFY(tabTo(help, "helpTextSmaller")); QTest::keyClick(help, Qt::Key_Space);
        QVERIFY(help->property("textScale").toDouble() < 1);
        QVERIFY(tabTo(help, "helpTextReset")); QTest::keyClick(help, Qt::Key_Space);
        QVERIFY(tabTo(help, "helpTextLarger")); QTest::keyClick(help, Qt::Key_Space);
        QVERIFY(help->property("textScale").toDouble() > 1);
        QVERIFY(tabTo(help, "helpTextReset")); QTest::keyClick(help, Qt::Key_Space);
        QCOMPARE(help->property("textScale").toDouble(), 1.0);
        if (width < 720) {
            QVERIFY(tabTo(help, "helpContentsButton")); QTest::keyClick(help, Qt::Key_Space);
            QTRY_VERIFY(help->property("contentsOpen").toBool());
        }
        QVERIFY(tabTo(help, "helpTopic_sample-details")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        if (width < 720) {
            QVERIFY(tabTo(help, "helpContentsButton")); QTest::keyClick(help, Qt::Key_Space);
            QTest::keyClick(help, Qt::Key_Escape);
            QTRY_VERIFY(!help->property("contentsOpen").toBool());
            QVERIFY(help->isVisible());
            QVERIFY(tabTo(help, "helpContentsButton")); QTest::keyClick(help, Qt::Key_Space);
        }
        QVERIFY(tabTo(help, "helpHome")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        QVERIFY(tabTo(help, "helpSearch"));
        for (char key : QByteArray("second row")) QTest::keyClick(help, key);
        QTRY_COMPARE(help->property("results").toList().size(), 1);
        QTRY_COMPARE(object(help, "helpSearchResults")->property("count").toInt(), 1);
        QTest::qWait(100);
        QVERIFY(tabTo(help, "helpResult_sample-details"));
        QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        if (width < 720) QTRY_VERIFY(!help->property("contentsOpen").toBool());
        QVERIFY(tabTo(help, "helpHome")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        if (width < 720) {
            QVERIFY(tabTo(help, "helpContentsButton")); QTest::keyClick(help, Qt::Key_Space);
        }
        QVERIFY(tabTo(help, "helpSearch")); QTest::keyClick(help, Qt::Key_Return);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        if (width < 720) QTRY_VERIFY(!help->property("contentsOpen").toBool());
        QTest::keySequence(help, QKeySequence(QKeySequence::Find));
        QTRY_COMPARE(focusName(help), QString("helpFind"));
        for (char key : QByteArray("sample")) QTest::keyClick(help, key); QTest::keyClick(help, Qt::Key_Return);
        QVERIFY(help->property("findCount").toInt() > 1);
        QVERIFY(tabTo(help, "helpFindNext")); QTest::keyClick(help, Qt::Key_Space);
        QCOMPARE(help->property("findOccurrence").toInt(), 1);
        QVERIFY(tabTo(help, "helpFindPrevious", true)); QTest::keyClick(help, Qt::Key_Space);
        QCOMPARE(help->property("findOccurrence").toInt(), 0);
        QVERIFY(tabTo(help, "helpArticle"));
        auto* article = object(help, "helpArticle");
        auto* flick = object(help, "helpArticleFlick");
        QTest::keyClick(help, Qt::Key_Home);
        QCOMPARE(flick->property("contentY").toDouble(), 0.0);
        QTest::keyClick(help, Qt::Key_PageDown);
        QVERIFY(flick->property("contentY").toDouble() > 0);
        QTest::keyClick(help, Qt::Key_End);
        const auto bottom = flick->property("contentY").toDouble();
        QTest::keyClick(help, Qt::Key_PageUp);
        QVERIFY(flick->property("contentY").toDouble() < bottom);
        const auto captureDir = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
        if (!captureDir.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(help->grabWindow().save(QDir(captureDir).filePath(QString("keyboard-table-%1-%2.png").arg(dark ? "dark" : "light").arg(width))));
        }
        QTest::keyClick(help, Qt::Key_Home);
        QTest::keyClick(help, Qt::Key_Tab);
        QCOMPARE(focusName(help), QString("helpArticle"));
        QCOMPARE(article->property("keyboardLink").toInt(), 0);
        QTest::keyClick(help, Qt::Key_Tab);
        QCOMPARE(article->property("keyboardLink").toInt(), 1);
        QTest::keyClick(help, Qt::Key_Backtab);
        QCOMPARE(article->property("keyboardLink").toInt(), 0);
        QTest::keyClick(help, Qt::Key_Tab);
        QTest::keyClick(help, Qt::Key_Return);
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-start"));
        QVERIFY(tabTo(help, "helpArticle"));
        QTest::keyClick(help, Qt::Key_Home);
        QTest::keyClick(help, Qt::Key_Tab);
        QTest::keyClick(help, Qt::Key_Return);
        auto* picture = namedWindow("helpImageWindow");
        QTRY_VERIFY(picture && picture->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(picture));
        QVERIFY(tabTo(picture, "helpImageActual")); QTest::keyClick(picture, Qt::Key_Space);
        QCOMPARE(help->property("imageZoom").toDouble(), 1.0);
        QVERIFY(tabTo(picture, "helpImageZoomIn")); QTest::keyClick(picture, Qt::Key_Space);
        QVERIFY(help->property("imageZoom").toDouble() > 1);
        QVERIFY(tabTo(picture, "helpImageFlick"));
        QTest::keyClick(picture, Qt::Key_Right);
        QVERIFY(object(picture, "helpImageFlick")->property("contentX").toDouble() > 0);
        QVERIFY(tabTo(picture, "helpImageZoomOut")); QTest::keyClick(picture, Qt::Key_Space);
        QVERIFY(tabTo(picture, "helpImageFit", true)); QTest::keyClick(picture, Qt::Key_Space);
        QVERIFY(tabTo(picture, "helpImageFlick"));
        if (!captureDir.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(picture->grabWindow().save(QDir(captureDir).filePath(QString("keyboard-picture-%1-%2.png").arg(dark ? "dark" : "light").arg(width))));
        }
        QTest::keyClick(picture, Qt::Key_Escape);
        QTRY_VERIFY(!picture->isVisible());
        QTRY_COMPARE(focusName(help), QString("helpArticle"));
        QVERIFY(QTest::qWaitForWindowActive(help));
        QTest::keyClick(help, Qt::Key_Return);
        QTRY_VERIFY(picture->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(picture));
        QVERIFY(tabTo(picture, "helpImageClose")); QTest::keyClick(picture, Qt::Key_Space);
        QTRY_VERIFY(!picture->isVisible());
        QTRY_COMPARE(focusName(help), QString("helpArticle"));
        QVERIFY(QTest::qWaitForWindowActive(help));
        const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
        if (!output.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(help->grabWindow().save(QDir(output).filePath(QString("keyboard-%1-%2.png").arg(dark ? "dark" : "light").arg(width))));
        }
        QTest::keyClick(help, Qt::Key_Escape);
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void keyboardContextAndModal_data() {
        QTest::addColumn<bool>("reports");
        QTest::newRow("rules") << false;
        QTest::newRow("reports") << true;
    }
    void keyboardContextAndModal() {
        QFETCH(bool, reports);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        QSettings().setValue("datasetPath", settings.filePath("keyboard-missing.spentit"));
        Session session(&catalog);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        session.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(session.window));
        // The welcome screen starts its file probe on a 350ms timer; wait
        // before starting keyboard interaction with its temporarily disabled row.
        QTest::qWait(450);
        QTRY_VERIFY_WITH_TIMEOUT(!session.model.busy(), 30000);
        QVERIFY(tabTo(session.window, "welcomeHelp"));
        QTest::keyClick(session.window, Qt::Key_Space);
        auto* help = namedWindow("helpWindow"); QTRY_VERIFY(help && help->isVisible());
        QTRY_COMPARE(help->property("topicId").toString(), QString("create-open-file"));
        QVERIFY(QTest::qWaitForWindowActive(help));
        QVERIFY(tabTo(help, "helpTopic_messages-problems"));
        QTest::keyClick(help, Qt::Key_Space);
        QTRY_COMPARE(help->property("topicId").toString(), QString("messages-problems"));
        QVERIFY(tabTo(help, "helpClose")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_VERIFY(!help->isVisible());
        QTRY_COMPARE(focusName(session.window), QString("welcomeHelp"));
        // Synthetic setup only; the Help/decision interaction below is keyboard-only.
        createDocument(session, "Keyboard Help");
        if (reports) {
            QVERIFY(click(session.root, "reportsNavigation")); wait(session.model);
            QVERIFY(click(session.root, "newReport"));
        } else {
            QVERIFY(click(session.root, "rulesNavigation")); wait(session.model);
            QVERIFY(click(session.root, "newRule"));
            QVERIFY(edit(session.root, "ruleExpression", "^Keyboard help$"));
        }
        const QString stayName = reports ? "reportsGuardStay" : "rulesGuardStay";
        QVERIFY(click(session.root, "accountNavigation"));
        auto* dialog = qobject_cast<QQuickWindow*>(object(session.root, reports ? "reportsLeaveDialog" : "rulesLeaveDialog"));
        QVERIFY(dialog); QTRY_VERIFY(dialog->isVisible());
        dialog->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(dialog));
        QVERIFY(tabTo(dialog, stayName));
        const auto before = session.model.state();
        QTest::keySequence(dialog, QKeySequence(QKeySequence::HelpContents));
        QTRY_VERIFY(help->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(help));
        QTRY_COMPARE(help->property("topicId").toString(), QString(reports ? "answer-with-reports" : "use-rules"));
        QVERIFY(tabTo(help, "helpTextLarger")); QTest::keyClick(help, Qt::Key_Space);
        QVERIFY(tabTo(help, "helpClose")); QTest::keyClick(help, Qt::Key_Space);
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->modality(), Qt::ApplicationModal);
        if (!reports) {
            QVERIFY(session.root->property("rulesDirty").toBool());
            QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Keyboard help$"));
        } else {
            QVERIFY(object(session.root, "reportsWorkspace")->property("leaving").toBool());
        }
        QCOMPARE(session.model.state(), before);
        QTRY_COMPARE(focusName(dialog), stayName);
    }
    void missingCatalogStillLoadsTheWindow() {
        Session session(nullptr);
        QVERIFY(session.root);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
        QCOMPARE(session.root->property("helpLoadError").toString(), QString("Help could not be opened."));
        QVERIFY(!namedWindow("helpWindow"));
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void sampleViewerBeforeAFileIsOpen() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QVERIFY(!session.model.state().value("active").toBool());
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QVERIFY(QTest::qWaitForWindowExposed(help));
        QCOMPARE(help->modality(), Qt::NonModal);
        auto* banner = object(help, "helpBanner");
        QVERIFY(banner);
        QVERIFY(banner->property("visible").toBool());
        QVERIFY(banner->property("text").toString().contains("sample content"));
        auto* article = object(help, "helpArticle");
        QVERIFY(article);
        QTRY_VERIFY(article->property("text").toString().contains("1,234.56"));
        auto* document = textDocument(article);
        QVERIFY(document);
        int images = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "imageCount", Q_RETURN_ARG(int, images), Q_ARG(QObject*, document)));
        QVERIFY2(images >= 1, "The sample picture was not loaded into the article");
        bool href = false;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "containsHref", Q_RETURN_ARG(bool, href), Q_ARG(QObject*, document), Q_ARG(QString, QString("help:sample-details#wide-table"))));
        QVERIFY(href);
        auto* flick = object(help, "helpArticleFlick");
        QVERIFY(flick);
        QVERIFY(flick->setProperty("contentY", 30));
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help-image:sample-mark.png"))));
        auto* picture = namedWindow("helpImageWindow");
        QTRY_VERIFY(picture && picture->isVisible());
        auto* image = object(picture, "helpImage");
        QVERIFY(image);
        QTRY_VERIFY_WITH_TIMEOUT(image->property("status").toInt() == 1, 5000);
        QVERIFY(object(picture, "helpImageError")->property("visible").toBool() == false);
        const auto fitted = help->property("imageZoom").toDouble();
        QVERIFY(click(picture, "helpImageActual"));
        QCOMPARE(help->property("imageZoom").toDouble(), 1.0);
        QVERIFY(click(picture, "helpImageZoomIn"));
        QVERIFY(help->property("imageZoom").toDouble() > 1.0);
        QVERIFY(image->property("width").toDouble() > image->property("sourceSize").toSize().width());
        auto* imageFlick = qobject_cast<QQuickItem*>(object(picture, "helpImageFlick"));
        QVERIFY(imageFlick);
        imageFlick->forceActiveFocus();
        const auto beforePan = imageFlick->property("contentX").toDouble();
        QTest::keyClick(picture, Qt::Key_Right);
        QVERIFY(imageFlick->property("contentX").toDouble() >= beforePan);
        QTest::keyClick(picture, Qt::Key_Escape);
        QTRY_VERIFY2(!picture->isVisible(), qPrintable(QString("image-escape focus=%1")
            .arg(QGuiApplication::focusWindow() ? QGuiApplication::focusWindow()->objectName() : "none")));
        QVERIFY(help->isVisible());
        QCOMPARE(flick->property("contentY").toDouble(), 30.0);
        Q_UNUSED(fitted);
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:sample-details#wide-table"))));
        QTRY_COMPARE(help->property("topicId").toString(), QString("sample-details"));
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Wide table"));
        int tables = 0;
        auto* detailsDocument = textDocument(article);
        QVERIFY(QMetaObject::invokeMethod(&catalog, "tableCount", Q_RETURN_ARG(int, tables), Q_ARG(QObject*, detailsDocument)));
        QVERIFY(tables >= 1);
        QVariantMap marked;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "markMatch", Q_RETURN_ARG(QVariantMap, marked), Q_ARG(QObject*, detailsDocument), Q_ARG(QString, QString("lavender")), Q_ARG(int, 0), Q_ARG(QColor, QColor(Qt::yellow))));
        QVERIFY(marked.value("count").toInt() >= 1);
        const int position = marked.value("position").toInt();
        QVERIFY(QMetaObject::invokeMethod(article, "select", Q_ARG(int, position), Q_ARG(int, position + 8)));
        QCOMPARE(article->property("selectedText").toString(), QString("lavender"));
        bool stillLinked = false;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "containsHref", Q_RETURN_ARG(bool, stillLinked), Q_ARG(QObject*, detailsDocument), Q_ARG(QString, QString("help:sample-details#opening-notes"))));
        QVERIFY(stillLinked);
        auto* search = object(help, "helpSearch");
        QVERIFY(search);
        QVERIFY(search->setProperty("text", "second row"));
        if (help->property("query").toString() != "second row") QVERIFY(help->setProperty("query", "second row"));
        QTRY_COMPARE(help->property("results").toList().size(), 1);
        QCOMPARE(help->property("results").toList().front().toMap().value("topicId").toString(), QString("sample-details"));
        QVERIFY(search->setProperty("text", "zzzz"));
        if (help->property("query").toString() != "zzzz") QVERIFY(help->setProperty("query", "zzzz"));
        QTRY_VERIFY(object(help, "helpNoMatches")->property("visible").toBool());
        QCOMPARE(object(help, "helpNoMatches")->property("text").toString(), QString("No matches. Change the search or choose a topic."));
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("https://example.com"))));
        QCOMPARE(help->property("linkMessage").toString(), QString("That link is not part of Help."));
        auto* back = qobject_cast<QQuickItem*>(object(help, "helpBack"));
        QVERIFY(back);
        back->forceActiveFocus();
        QVERIFY2(back->hasFocus(), qPrintable(QString("focus=%1 active=%2 enabled=%3 window=%4")
            .arg(back->hasFocus()).arg(back->hasActiveFocus()).arg(back->isEnabled()).arg(help->isActive())));
        const auto smaller = article->property("font").value<QFont>().pixelSize();
        QVERIFY(click(help, "helpTextLarger"));
        QVERIFY(article->property("font").value<QFont>().pixelSize() > smaller);
        pressEscape(help);
        QTRY_VERIFY2(!help->isVisible(), qPrintable(QString("focus=%1").arg(QGuiApplication::focusWindow() ? QGuiApplication::focusWindow()->objectName() : "none")));
        QVERIFY(!session.model.state().value("active").toBool());
        QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
        QTRY_VERIFY(namedWindow("helpWindow") == help);
        QCOMPARE(help->property("topicId").toString(), QString("sample-details"));
    }
    void introductionBeforeAFileIsOpen() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        QVERIFY(!catalog.sample());
        QVERIFY(catalog.banner().isEmpty());
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QVERIFY(!session.model.state().value("active").toBool());
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QCOMPARE(help->modality(), Qt::NonModal);
        QCOMPARE(help->property("topicId").toString(), QString("understand-spending"));
        auto* banner = object(help, "helpBanner");
        QVERIFY(banner);
        QCOMPARE(banner->property("visible").toBool(), false);
        auto* article = object(help, "helpArticle");
        QVERIFY(article);
        QTRY_VERIFY(article->property("text").toString().contains("42.18"));
        QVERIFY(article->property("text").toString().simplified().contains("where did it all go?"));
        QVERIFY(article->property("text").toString().contains("Accept transactions"));
        QVERIFY(article->property("text").toString().contains("1,234.56"));
        qreal gap = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "readingGap", Q_RETURN_ARG(qreal, gap), Q_ARG(QObject*, textDocument(article)), Q_ARG(QString, QString("42.18"))));
        QVERIFY2(gap > 4.0, qPrintable(QString("paragraph gap %1").arg(gap)));
        qreal heading = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "headingGap", Q_RETURN_ARG(qreal, heading), Q_ARG(QObject*, textDocument(article)), Q_ARG(QString, QString("Start with your own accounts"))));
        QVERIFY2(heading > gap, qPrintable(QString("heading gap %1 paragraph gap %2").arg(heading).arg(gap)));
        int lists = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "listCount", Q_RETURN_ARG(int, lists), Q_ARG(QObject*, textDocument(article))));
        QVERIFY(lists >= 1);
        const auto wide = help->width();
        help->setWidth(1400);
        auto* flick = object(help, "helpArticleFlick");
        QVERIFY(flick);
        QTRY_VERIFY(article->property("width").toDouble() + 80 < flick->property("width").toDouble());
        help->setWidth(wide);
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void createOpenChapterReading_data() {
        QTest::addColumn<QString>("topic");
        QTest::addColumn<QStringList>("pictures");
        QTest::addColumn<QString>("section");
        QTest::addColumn<QString>("sectionTitle");
        QTest::newRow("create-open") << QString("create-open-file")
            << QStringList{"create-file-setup.png", "create-file-workspace.png", "create-file-unlock.png"}
            << QString("open-your-file") << QString("Open your file");
        QTest::newRow("accounts") << QString("set-up-accounts")
            << QStringList{"accounts-new.png", "accounts-opening.png", "accounts-menu.png"}
            << QString("choose-the-currency") << QString("Choose the currency");
        QTest::newRow("imports") << QString("bring-in-transactions")
            << QStringList{"import-destination.png", "import-mapping.png", "import-categories.png", "import-results.png", "import-undo.png"}
            << QString("undo-a-mistaken-import") << QString("Undo a mistaken import");
        QTest::newRow("transactions") << QString("find-transactions")
            << QStringList{"transactions-ledger.png", "transactions-search.png", "transactions-review.png"}
            << QString("find-a-purchase") << QString("Find a purchase");
        QTest::newRow("review") << QString("review-duplicates-transfers")
            << QStringList{"review-keep.png", "review-link.png", "review-linked.png", "review-difference.png"}
            << QString("when-the-amounts-differ") << QString("When the amounts differ");
        QTest::newRow("editing") << QString("add-correct-transactions")
            << QStringList{"editing-new.png", "editing-calendar.png", "editing-splits.png", "editing-mismatch.png", "change-history.png"}
            << QString("divide-a-purchase-into-splits") << QString("Divide a purchase into splits");
        QTest::newRow("categories") << QString("organize-categories")
            << QStringList{"categories-workspace.png", "categories-completion.png", "categories-menu.png", "categories-split.png"}
            << QString("change-selected-transactions") << QString("Change selected transactions");
        QTest::newRow("rules") << QString("use-rules")
            << QStringList{"rules-editor.png", "rules-preview.png", "rules-leave.png"}
            << QString("preview-save-and-apply") << QString("Preview, save and apply");
        QTest::newRow("acceptance") << QString("accept-transactions")
            << QStringList{"accept-ready.png", "accept-review.png", "accept-quit.png"}
            << QString("leave-them-new-and-return-later") << QString("Leave them new and return later");
        QTest::newRow("reports") << QString("answer-with-reports")
            << QStringList{"reports-editor.png", "reports-comparison.png", "reports-pie.png", "reports-transactions.png"}
            << QString("open-the-transactions-behind-an-amount") << QString("Open the transactions behind an amount");
        QTest::newRow("recovery") << QString("look-after-your-work")
            << QStringList{}
            << QString("keep-a-copy") << QString("Keep a copy of the file");
        QTest::newRow("where-to-find") << QString("where-to-find")
            << QStringList{}
            << QString("undo-and-problems") << QString("Undo and problems");
        QTest::newRow("keyboard-mouse") << QString("keyboard-mouse")
            << QStringList{}
            << QString("read-help") << QString("Read Help");
        QTest::newRow("search-examples") << QString("search-examples") << QStringList{}
            << QString("common-pattern-pieces") << QString("Common pattern pieces");
        QTest::newRow("words-used") << QString("words-used") << QStringList{}
            << QString("merge-and-link") << QString("Merge and Link");
        QTest::newRow("messages-problems") << QString("messages-problems") << QStringList{}
            << QString("search-and-rules") << QString("Search and rules");
    }
    void createOpenChapterReading() {
        QFETCH(QString, topic);
        QFETCH(QStringList, pictures);
        QFETCH(QString, section);
        QFETCH(QString, sectionTitle);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root);
        QVERIFY(help);
        QVERIFY(QTest::qWaitForWindowExposed(help));
        const QStringList order{"understand-spending", "create-open-file", "set-up-accounts",
                                "bring-in-transactions", "find-transactions", "review-duplicates-transfers", "add-correct-transactions", "organize-categories", "use-rules", "accept-transactions", "answer-with-reports", "look-after-your-work", "where-to-find", "keyboard-mouse", "search-examples", "words-used", "messages-problems"};
        const auto topicIndex = order.indexOf(topic);
        QVERIFY(topicIndex > 0);
        for (int index = 0; index < topicIndex; ++index)
            QVERIFY(QMetaObject::invokeMethod(help, "nextTopic"));
        QCOMPARE(help->property("topicId").toString(), topic);
        auto* article = object(help, "helpArticle");
        QVERIFY(article);
        auto* quick = qobject_cast<QQuickTextDocument*>(textDocument(article));
        QVERIFY(quick);
        int images = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "imageCount", Q_RETURN_ARG(int, images), Q_ARG(QObject*, quick)));
        QCOMPARE(images, pictures.size());
        if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
            QTest::qWait(150);
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE")));
        }
        QTRY_VERIFY2(quick->textDocument()->idealWidth() <= article->property("width").toDouble() + 1,
                     qPrintable(QString("Document width %1 exceeds article %2").arg(quick->textDocument()->idealWidth()).arg(article->property("width").toDouble())));
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:" + topic + "#" + section))));
        QTRY_COMPARE(help->property("currentSection").toString(), sectionTitle);
        auto* flick = object(help, "helpArticleFlick");
        for (const auto& name : pictures) {
            QTextBlock imageBlock;
            for (auto block = quick->textDocument()->begin(); block != quick->textDocument()->end(); block = block.next()) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const auto format = it.fragment().charFormat();
                    if (format.isImageFormat() && format.toImageFormat().name().endsWith(name)) {
                        imageBlock = block;
                        QVERIFY(format.isAnchor());
                        QCOMPARE(format.anchorHref(), QString("help-image:") + name);
                    }
                }
            }
            QVERIFY(imageBlock.isValid());
            const auto rect = quick->textDocument()->documentLayout()->blockBoundingRect(imageBlock);
            flick->setProperty("contentY", rect.top());
            QTest::qWait(50);
            const auto before = flick->property("contentY").toDouble();
            auto* articleItem = qobject_cast<QQuickItem*>(article);
            QVERIFY(articleItem);
            QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier,
                             articleItem->mapToScene(QPointF(80, rect.top() + 40)).toPoint());
            auto* picture = namedWindow("helpImageWindow");
            QTRY_VERIFY(picture && picture->isVisible());
            auto* image = object(picture, "helpImage");
            QTRY_COMPARE(image->property("status").toInt(), 1);
            QVERIFY(click(picture, "helpImageActual"));
            QCOMPARE(help->property("imageZoom").toDouble(), 1.0);
            QVERIFY(click(picture, "helpImageClose"));
            QTRY_VERIFY(!picture->isVisible());
            QCOMPARE(flick->property("contentY").toDouble(), before);
        }
        if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
            QTest::qWait(150);
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".images.png"));
        }
        if (topic == "search-examples") {
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:search-examples#common-pattern-pieces"))));
            QTRY_COMPARE(help->property("currentSection").toString(), QString("Common pattern pieces"));
            QVERIFY(quick->textDocument()->toPlainText().contains("SHOP[ -]123"));
            if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
                QTest::qWait(150);
                QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".regex.png"));
            }
        }
        if (topic == "accept-transactions") {
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:accept-transactions#choose-the-action-you-mean"))));
            QTRY_COMPARE(help->property("currentSection").toString(), QString("Choose the action you mean"));
            if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
                QTest::qWait(150);
                QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".table.png"));
            }
        }
        help->resize(500, 680);
        help->setProperty("textScale", 1.5);
        if (topic == "accept-transactions") {
            QTest::qWait(150);
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:accept-transactions#choose-the-action-you-mean"))));
        }
        if (topic == "search-examples") {
            QTest::qWait(150);
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:search-examples#common-pattern-pieces"))));
        }
        QTRY_VERIFY(quick->textDocument()->idealWidth() <= article->property("width").toDouble() + 1);
        if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
            QTest::qWait(150);
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".narrow.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(help, "previousTopic"));
        QCOMPARE(help->property("topicId").toString(), order.at(topicIndex - 1));
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void quickReferenceLookup_data() {
        QTest::addColumn<QString>("reference");
        QTest::addColumn<QString>("query");
        QTest::addColumn<QString>("destination");
        QTest::addColumn<QString>("section");
        QTest::addColumn<QString>("heading");
        QTest::newRow("where-to-find") << QString("where-to-find") << QString("quick reference")
            << QString("look-after-your-work") << QString("keep-a-copy") << QString("Keep a copy of the file");
        QTest::newRow("keyboard-mouse") << QString("keyboard-mouse") << QString("keyboard")
            << QString("find-transactions") << QString("select-without-editing") << QString("Select without editing");
        QTest::newRow("search-examples") << QString("search-examples") << QString("search examples")
            << QString("use-rules") << QString("keep-the-useful-part-of-a-paypal-description")
            << QString("Keep the useful part of a PayPal description");
        QTest::newRow("rules-pattern-reference") << QString("use-rules") << QString("regular expression")
            << QString("search-examples") << QString("common-pattern-pieces") << QString("Common pattern pieces");
        QTest::newRow("words-used") << QString("words-used") << QString("glossary")
            << QString("set-up-accounts") << QString("set-the-starting-balance") << QString("Set the starting balance");
        QTest::newRow("messages-problems") << QString("messages-problems") << QString("Unclosed search quote")
            << QString("search-examples") << QString("clear-a-search-or-fix-it") << QString("Clear a search or fix it");
        QTest::newRow("problem-open") << QString("messages-problems") << QString("already open")
            << QString("look-after-your-work") << QString("the-file-will-not-open") << QString("The file will not open");
        QTest::newRow("problem-splits") << QString("messages-problems") << QString("Splits add up")
            << QString("add-correct-transactions") << QString("divide-a-purchase-into-splits") << QString("Divide a purchase into splits");
        QTest::newRow("problem-report") << QString("messages-problems") << QString("Transactions changed")
            << QString("answer-with-reports") << QString("if-the-result-needs-attention") << QString("If the result needs attention");
    }
    void quickReferenceLookup() {
        QFETCH(QString, reference);
        QFETCH(QString, query);
        QFETCH(QString, destination);
        QFETCH(QString, section);
        QFETCH(QString, heading);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root); QVERIFY(help);
        QVERIFY(QTest::qWaitForWindowExposed(help));
        QVERIFY(edit(help, "helpSearch", query));
        const auto hits=catalog.search(query);
        int hitIndex=-1;
        for (int i=0; i<hits.size(); ++i)
            if (hits[i].toMap().value("topicId").toString()==reference) hitIndex=i;
        QVERIFY(hitIndex>=0);
        auto* results=qobject_cast<QQuickItem*>(object(help, "helpSearchResults")); QVERIFY(results);
        QTRY_COMPARE(results->property("count").toInt(), hits.size());
        QTRY_VERIFY(results->isVisible() && results->height()>0);
        // Scroll as a reader would. Teleporting to an estimated ListView index
        // can recycle a wrapped-height delegate before its layout settles.
        QQuickItem* result=nullptr;
        QPointF resultPoint;
        for (int step=0; step<100; ++step) {
            QTest::qWait(60);
            result=qobject_cast<QQuickItem*>(object(help,"helpResult_"+reference));
            if (result) {
                // The excerpt under the title must open the topic too, not only the title line.
                resultPoint=QPointF(result->width()/2, qMax(28.0, result->height()*0.72));
                if (results->contains(result->mapToItem(results,resultPoint))) break;
            }
            result=nullptr;
            const auto center=results->mapToScene(QPointF(results->width()/2,results->height()/2)).toPoint();
            QTest::wheelEvent(help,center,QPoint(0,-120));
            QTRY_VERIFY_WITH_TIMEOUT(!results->property("moving").toBool(), 3000);
        }
        QVERIFY(result);
        QVERIFY(results->contains(result->mapToItem(results,resultPoint)));
        QVERIFY(help->contentItem()->contains(result->mapToScene(resultPoint)));
        QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, result->mapToScene(resultPoint).toPoint());
        QTRY_COMPARE(help->property("topicId").toString(), reference);
        QVERIFY(edit(help, "helpSearch", ""));
        QTest::qWait(150);
        auto* article = qobject_cast<QQuickItem*>(object(help, "helpArticle")); QVERIFY(article);
        auto* quick = qobject_cast<QQuickTextDocument*>(textDocument(article)); QVERIFY(quick);
        auto* document = quick->textDocument();
        const QString target("help:" + destination + "#" + section);
        int position = -1;
        for (auto block = document->begin(); block != document->end(); block = block.next())
            for (auto it = block.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().anchorHref() == target)
                    position = it.fragment().position() + 1;
        QVERIFY(position >= 0);
        QRectF character;
        QVERIFY(QMetaObject::invokeMethod(article, "positionToRectangle", Q_RETURN_ARG(QRectF, character), Q_ARG(int, position)));
        const QPointF point(character.x() + 2, character.center().y());
        auto* flick = object(help, "helpArticleFlick"); QVERIFY(flick);
        flick->setProperty("contentY", qMax(0.0, point.y() - 100));
        QTest::qWait(150);
        QTRY_VERIFY(flick->property("contentHeight").toDouble() >= document->documentLayout()->documentSize().height() - 1);
        QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, article->mapToScene(point).toPoint());
        QTRY_COMPARE(help->property("topicId").toString(), destination);
        QTRY_COMPARE(help->property("currentSection").toString(), heading);
        QVERIFY(click(help, "helpBack"));
        QTRY_COMPARE(help->property("topicId").toString(), reference);
        QVERIFY(click(help, "helpHome"));
        QTRY_COMPARE(help->property("topicId").toString(), QString("understand-spending"));
        help->setProperty("contentsOpen", true);
        auto* contents = object(help, "helpContents"); QVERIFY(contents);
        int topicIndex=-1;
        const auto topics=catalog.topics();
        for (int i=0; i<topics.size(); ++i)
            if (topics[i].toMap().value("id").toString()==reference) topicIndex=i;
        QVERIFY(topicIndex>=0);
        QVERIFY(QMetaObject::invokeMethod(contents, "positionViewAtIndex", Q_ARG(int, topicIndex), Q_ARG(int, 1)));
        QTest::qWait(150);
        auto* topic = qobject_cast<QQuickItem*>(object(help, "helpTopic_" + reference)); QVERIFY(topic);
        const QPointF topicPoint(topic->width()/2,topic->height()/2);
        QVERIFY(qobject_cast<QQuickItem*>(contents)->contains(topic->mapToItem(qobject_cast<QQuickItem*>(contents),topicPoint)));
        QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier,topic->mapToScene(topicPoint).toPoint());
        QTRY_COMPARE(help->property("topicId").toString(), reference);
        QVERIFY(!session.model.state().value("active").toBool());
    }
    void articleVisibleAfterEnlargedPicture() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root);
        QVERIFY(help);
        help->resize(880, 480);
        QVERIFY(QTest::qWaitForWindowExposed(help));
        auto* article = qobject_cast<QQuickItem*>(object(help, "helpArticle"));
        QVERIFY(article);
        auto* flick = qobject_cast<QQuickItem*>(object(help, "helpArticleFlick"));
        QVERIFY(flick);
        auto choose = [&](const QString& id) {
            auto* contents = object(help, "helpContents");
            QVERIFY(contents);
            int topicIndex = -1;
            const auto topics = catalog.topics();
            for (int i = 0; i < topics.size(); ++i)
                if (topics[i].toMap().value("id").toString() == id) topicIndex = i;
            QVERIFY(topicIndex >= 0);
            QVERIFY(QMetaObject::invokeMethod(contents, "positionViewAtIndex", Q_ARG(int, topicIndex), Q_ARG(int, 1)));
            QTest::qWait(50);
            auto* button = qobject_cast<QQuickItem*>(object(help, "helpTopic_" + id));
            QVERIFY(button);
            QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier,
                             button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
            QTRY_COMPARE(help->property("topicId").toString(), id);
            QTest::qWait(100);
        };
        auto document = [&]() {
            auto* quick = qobject_cast<QQuickTextDocument*>(textDocument(article));
            return quick ? quick->textDocument() : nullptr;
        };
        auto assertTitleDrawn = [&](const QString& title) {
            auto* text = document();
            QVERIFY(text);
            QTextBlock block;
            for (auto it = text->begin(); it != text->end(); it = it.next())
                if (it.text().trimmed() == title) { block = it; break; }
            QVERIFY2(block.isValid(), qPrintable(title));
            const QRectF rect = text->documentLayout()->blockBoundingRect(block);
            const qreal contentY = flick->property("contentY").toDouble();
            QVERIFY2(rect.top() >= contentY - 1 && rect.bottom() <= contentY + flick->height() + 1,
                     qPrintable(QString("%1 at %2 is outside scroll %3 height %4")
                         .arg(title).arg(rect.top()).arg(contentY).arg(flick->height())));
            QVERIFY(!article->flags().testFlag(QQuickItem::ItemObservesViewport));
            const QImage screen = help->grabWindow();
            const qreal scale = screen.devicePixelRatio();
            const QPointF origin = article->mapToScene(rect.topLeft());
            const QRect pixels(qRound(origin.x()*scale), qRound(origin.y()*scale),
                qRound(qMin(rect.width(), 280.0)*scale), qRound(qMax(rect.height(), 12.0)*scale));
            const QImage band = screen.copy(pixels.intersected(screen.rect()));
            QVERIFY2(!band.isNull() && band.width() > 0 && band.height() > 0, qPrintable(title));
            qsizetype ink = 0;
            for (int y = 0; y < band.height(); ++y)
                for (int x = 0; x < band.width(); ++x)
                    if (qGray(band.pixel(x, y)) < 128) ++ink;
            if (ink <= 40) band.save("/tmp/spentit-help-missing-title.png");
            QVERIFY2(ink > 40, qPrintable(QString("%1 is not drawn (%2 dark pixels)").arg(title).arg(ink)));
        };
        auto openPicture = [&](const QString& fileName) {
            auto* text = document();
            QVERIFY(text);
            QTextBlock imageBlock;
            for (auto block = text->begin(); block != text->end(); block = block.next())
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const auto format = it.fragment().charFormat();
                    if (format.isImageFormat() && format.toImageFormat().name().endsWith(fileName))
                        imageBlock = block;
                }
            QVERIFY2(imageBlock.isValid(), qPrintable(fileName));
            const auto imageRect = text->documentLayout()->blockBoundingRect(imageBlock);
            flick->setProperty("contentY", qMax(0.0, imageRect.top() - 24));
            QTest::qWait(30);
            const QPointF clickPoint(qBound(8.0, imageRect.center().x(), article->width() - 8), imageRect.top() + 40);
            QString link;
            QVERIFY(QMetaObject::invokeMethod(article, "linkAt", Q_RETURN_ARG(QString, link),
                                              Q_ARG(qreal, clickPoint.x()), Q_ARG(qreal, clickPoint.y())));
            QVERIFY2(link == "help-image:" + fileName, qPrintable(link));
            const QPoint scenePoint = article->mapToScene(clickPoint).toPoint();
            QVERIFY(QRectF(0, 0, help->width(), help->height()).contains(scenePoint));
            QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, scenePoint);
            auto* picture = namedWindow("helpImageWindow");
            QTRY_VERIFY(picture && picture->isVisible());
            QTRY_VERIFY(picture->isActive());
            QVERIFY(click(picture, "helpImageClose"));
            QTRY_VERIFY(!picture->isVisible());
        };
        choose("add-correct-transactions");
        if (QTest::currentTestFailed()) return;
        openPicture("editing-new.png");
        if (QTest::currentTestFailed()) return;
        choose("organize-categories");
        if (QTest::currentTestFailed()) return;
        openPicture("categories-workspace.png");
        if (QTest::currentTestFailed()) return;
        choose("review-duplicates-transfers");
        if (QTest::currentTestFailed()) return;
        QVERIFY(flick->property("contentY").toDouble() < 1);
        assertTitleDrawn("Review duplicates and link transfers");
        if (QTest::currentTestFailed()) return;
        choose("add-correct-transactions");
        if (QTest::currentTestFailed()) return;
        const qreal bottom = qMax(0.0, flick->property("contentHeight").toDouble() - flick->height());
        QVERIFY(bottom > flick->height());
        flick->setProperty("contentY", bottom);
        QTest::qWait(20);
        (void)help->grabWindow();
        choose("understand-spending");
        if (QTest::currentTestFailed()) return;
        QVERIFY(flick->property("contentY").toDouble() < 1);
        assertTitleDrawn("Understand your spending");
    }
    void longChapterScrollRendering() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root); QVERIFY(help);
        help->resize(880, 680);
        QVERIFY(QTest::qWaitForWindowExposed(help));
        auto* article = qobject_cast<QQuickItem*>(object(help, "helpArticle")); QVERIFY(article);
        auto* flick = qobject_cast<QQuickItem*>(object(help, "helpArticleFlick")); QVERIFY(flick);
        auto choose = [&](const QString& id) {
            if (help->width() < 720) { help->setProperty("contentsOpen", true); QTest::qWait(40); }
            auto* contents = object(help, "helpContents"); QVERIFY(contents);
            int topicIndex = -1;
            const auto topics = catalog.topics();
            for (int i = 0; i < topics.size(); ++i)
                if (topics[i].toMap().value("id").toString() == id) topicIndex = i;
            QVERIFY(topicIndex >= 0);
            QVERIFY(QMetaObject::invokeMethod(contents, "positionViewAtIndex", Q_ARG(int, topicIndex), Q_ARG(int, 1)));
            QTest::qWait(50);
            auto* button = qobject_cast<QQuickItem*>(object(help, "helpTopic_" + id)); QVERIFY(button);
            QElapsedTimer elapsed; elapsed.start();
            QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier,
                             button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
            QTRY_COMPARE(help->property("topicId").toString(), id);
            QTest::qWait(100);
            qInfo() << "Help topic" << id << "ready in" << elapsed.elapsed() << "ms";
        };
        auto snapshot = [&]() {
            QTest::qWait(40);
            const QImage screen = help->grabWindow();
            const qreal scale = screen.devicePixelRatio();
            const auto top = flick->mapToScene(QPointF(2, 2));
            return screen.copy(QRect(qRound(top.x()*scale), qRound(top.y()*scale),
                qRound((qMin(article->width(), flick->width())-16)*scale), qRound((flick->height()-4)*scale)));
        };
        auto assertPainted = [&](const QImage& pixels) {
            QVERIFY(!pixels.isNull());
            qsizetype ink = 0;
            for (int y = 0; y < pixels.height(); ++y) for (int x = 0; x < pixels.width(); ++x)
                if (qGray(pixels.pixel(x,y)) < 128) ++ink;
            QVERIFY2(ink > pixels.width()*pixels.height()/100, "The chapter's initial viewport must visibly contain text");
        };
        auto comparePixels = [&](const QImage& actual, const QImage& expected) {
            QCOMPARE(actual.size(), expected.size());
            qsizetype changed = 0;
            for (int y = 0; y < actual.height(); ++y) for (int x = 0; x < actual.width(); ++x) {
                const auto a = actual.pixelColor(x,y), b = expected.pixelColor(x,y);
                if (qMax(qMax(qAbs(a.red()-b.red()), qAbs(a.green()-b.green())), qAbs(a.blue()-b.blue())) > 24) ++changed;
            }
            const double fraction = double(changed)/(actual.width()*actual.height());
            if (fraction > 0.005) {
                expected.save("/tmp/spentit-scroll-before.png"); actual.save("/tmp/spentit-scroll-after.png");
            }
            // Permit minor glyph raster changes, but not missing paragraphs.
            QVERIFY2(fraction <= 0.005, qPrintable(QString("%1% article pixels changed").arg(fraction*100)));
        };
        for (int cycle = 0; cycle < 2; ++cycle) {
            help->resize(cycle ? 500 : 880, 680);
            help->setProperty("textScale", cycle ? 1.5 : 1.0);
            choose("organize-categories");
            flick->setProperty("contentY", 0);
            const auto categories = snapshot();
            assertPainted(categories);
            choose("search-examples");
            flick->setProperty("contentY", 0);
            assertPainted(snapshot());
            const qreal maximum = qMax(0.0, flick->property("contentHeight").toDouble()-flick->height());
            QList<qreal> positions;
            QList<QImage> down;
            for (qreal y = 0; y < maximum; y += flick->height()*0.8) positions.push_back(y);
            positions.push_back(maximum);
            for (qreal y : positions) { flick->setProperty("contentY", y); down.push_back(snapshot()); }
            for (qsizetype i = positions.size()-1; i >= 0; --i) {
                flick->setProperty("contentY", positions[i]);
                comparePixels(snapshot(), down[i]);
                if (QTest::currentTestFailed()) return;
            }
            // Exercise actual wheel delivery in both directions as well as the
            // deterministic viewport sweep, then leave from a nonzero position.
            const auto center = flick->mapToScene(QPointF(flick->width()/2, flick->height()/2));
            QTest::wheelEvent(help, center, QPoint(0,-120));
            QTRY_VERIFY(flick->property("contentY").toDouble() > 0);
            QTest::wheelEvent(help, center, QPoint(0,120));
            QTRY_VERIFY(flick->property("contentY").toDouble() < 1);
            flick->setProperty("contentY", positions[positions.size()/2]);
            const auto savedY = flick->property("contentY").toDouble();
            const auto middle = snapshot();
            choose("organize-categories");
            comparePixels(snapshot(), categories);
            if (QTest::currentTestFailed()) return;
            choose("search-examples");
            QTRY_COMPARE(flick->property("contentY").toDouble(), savedY);
            comparePixels(snapshot(), middle);
            if (QTest::currentTestFailed()) return;
        }
    }

    void continuousManualWalkthrough_data() {
        QTest::addColumn<bool>("alternateFormat");
        QTest::newRow("guide-format") << false;
        QTest::newRow("day-first-comma-decimal") << true;
    }
    void continuousManualWalkthrough() {
        QFETCH(bool, alternateFormat);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 1000);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        const auto path = session.path("Household.spentit");
        auto amount = [&](QString value) {
            if (alternateFormat) value.replace(',', QChar(1)).replace('.', ',').replace(QChar(1), '.');
            return value;
        };
        auto date = [&](const QString& iso) { return alternateFormat ? iso.mid(8,2)+"/"+iso.mid(5,2)+"/"+iso.left(4) : iso; };
        auto enter = [&](const QString& name, bool success = true) {
            auto* field = qobject_cast<QQuickItem*>(object(session.root, name)); QVERIFY(field);
            field->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Return); wait(session.model, success);
        };
        auto mouseItem = [&](QQuickItem* item, bool twice = false) {
            QVERIFY2(item,"Missing UI item"); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
                if (!parent->property("contentY").isValid() || parent->property("contentHeight").toDouble() <= parent->height()) continue;
                const auto y = item->mapToItem(parent, QPointF(0, item->height()/2)).y();
                if (y < 0 || y > parent->height()) parent->setProperty("contentY", qBound(0.0,
                    parent->property("contentY").toDouble()+y-parent->height()/2,
                    parent->property("contentHeight").toDouble()-parent->height()));
            }
            QTest::qWait(150);
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(session.window->contentItem()->contains(point));
            if (twice) QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            else QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            wait(session.model);
        };
        auto mouse = [&](QString name) {
            if (name=="accountNavigation_"+session.model.state().value("accountId").toString()) name="accountNavigation";
            auto* item=qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name));
            mouseItem(item);
        };
        auto undoLatest = [&] {
            auto* ledger = qobject_cast<QQuickItem*>(object(session.root, "durableLedger"));
            if (!ledger) return false;
            ledger->forceActiveFocus();
            QTest::keySequence(session.window, QKeySequence::Undo);
            auto* confirm = object(session.root, "confirmActionUndo");
            if (!confirm || !confirm->property("visible").toBool()) return false;
            mouse("confirmActionUndo");
            return true;
        };
        std::function<QQuickItem*(QQuickItem*, const QString&)> byText = [&](QQuickItem* parent, const QString& text) -> QQuickItem* {
            if (parent->isVisible() && parent->property("text").toString() == text && parent->metaObject()->indexOfSignal("clicked()") >= 0) return parent;
            for (auto* child : parent->childItems()) if (auto* found = byText(child, text)) return found;
            return nullptr;
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(
                QString(alternateFormat ? "continuous-alternate-" : "continuous-guide-")+name+".png")));
        };
        // File-picker paths are submitted as model commands. Forms use injected
        // text/combo values, real QML handlers, pointer clicks and Enter keys.
        mouse("newDataset");
        QVERIFY(object(session.root, "datasetPath")->setProperty("text", path));
        run(session.model, "prepare_create", {{"path", path}});
        QVERIFY(edit(session.root, "initialAccountName", "Chequing"));
        QVERIFY(object(session.root, "defaultCategories")->property("checked").toBool());
        if (alternateFormat) {
            auto* format = object(session.root, "setupDateFormat"); QVERIFY(format);
            format->setProperty("currentIndex", 3);
            QVERIFY(QMetaObject::invokeMethod(format, "activated", Q_ARG(int, 3)));
        }
        QVERIFY(object(session.root, "newDatasetPassword")->setProperty("text", "synthetic-guide-password"));
        QVERIFY(object(session.root, "confirmDatasetPassword")->setProperty("text", "synthetic-guide-password"));
        mouse("createDataset");
        QVERIFY(session.model.state().value("active").toBool());
        if (alternateFormat) {
            mouse("sidebarSettings");
            auto* style = object(session.root, "amountFormatStyle"); QVERIFY(style);
            style->setProperty("currentIndex", 1);
            QVERIFY(QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 1))); wait(session.model);
            mouse("accountNavigation");
        }
        auto opening = [&](const QString& value) {
            mouseItem(qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_0")), true);
            QTRY_VERIFY(object(session.root, "correctOpeningDialog")->property("visible").toBool());
            QVERIFY(edit(session.root, "correctOpeningDate", date("2026-01-01")));
            QVERIFY(edit(session.root, "correctOpeningAmount", amount(value))); enter("correctOpeningAmount");
        };
        opening("1,000.00");
        mouse("newAccount"); QVERIFY(edit(session.root, "newAccountName", "Credit card")); enter("newAccountName");
        QCOMPARE(session.model.state().value("name").toString(), QString("Credit card"));
        opening("0.00");
        int sequence = 0;
        auto import = [&](const QByteArray& bytes, const QString& account) {
            QFile file(session.path("continuous-"+QString::number(++sequence)+".qif"));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
            run(session.model, "import", {{"path", file.fileName()}, {"account", account}});
        };
        import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\n", "2");
        QCOMPARE(session.model.state().value("balance").toString(), amount("-36.18"));
        import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket groceries\nLGroceries\n^\n", "2");
        QCOMPARE(session.model.state().value("balance").toString(), amount("-78.36"));
        mouse("reviewNavigation"); mouse("reviewYes");
        // A unique earliest import is selected automatically.
        QVERIFY(!object(session.root, "reviewSurvivor")->property("visible").toBool());
        QCOMPARE(session.model.state().value("balance").toString(), amount("-36.18"));
        mouse("accountNavigation_1");
        import("!Type:Bank\nD2026-01-28\nT-36.18\nPCredit card payment\n^\n", "1");
        mouse("accountNavigation_2");
        import("!Type:CCard\nD2026-01-29\nT36.18\nPPayment received\n^\n", "2");
        mouse("reviewNavigation"); mouse("reviewYes");
        QCOMPARE(session.model.state().value("transfers").toList().size(), 1);
        QCOMPARE(session.model.state().value("balance").toString(), amount("0.00"));
        mouse("categoryNavigation");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("categories"));
        createCategory(session.model, session.root, "Household");
        createCategory(session.model, session.root, "Income");
        mouse("accountNavigation_2"); mouse("addTransaction");
        QVERIFY(edit(session.root, "durableDate", date("2026-01-22")));
        QVERIFY(edit(session.root, "durablePayee", "Corner shop"));
        QVERIFY(edit(session.root, "durableTotal", amount("-20.00"))); enter("durableTotal");
        mouse("addSplit");
        QVERIFY(edit(session.root, "editSplitAmount_1", amount("-8.00"))); enter("editSplitAmount_1");
        QVERIFY(edit(session.root, "splitCategory_0", "Groceries")); enter("splitCategory_0");
        QVERIFY(edit(session.root, "splitCategory_1", "Household")); enter("splitCategory_1");
        const auto corner = session.model.state().value("selected").toMap();
        QCOMPARE(corner.value("draft").toMap().value("splits").toList().value(0).toMap().value("amount").toString(), amount("-12.00"));
        QCOMPARE(session.model.state().value("balance").toString(), amount("-20.00"));
        capture("split");
        QVERIFY(edit(session.root, "ledgerSearch", "Market")); enter("ledgerSearch");
        QCOMPARE(session.model.rowCount(), 2);
        QVERIFY(edit(session.root, "ledgerSearch", "\"Market")); enter("ledgerSearch", false);
        QCOMPARE(session.model.error(), QString("Unclosed search quote"));
        QCOMPARE(session.model.rowCount(), 2);
        QVERIFY(edit(session.root, "ledgerSearch", "")); enter("ledgerSearch");
        QCOMPARE(session.model.state().value("balance").toString(), amount("-20.00"));
        mouse("rulesNavigation"); mouse("newRule");
        QVERIFY(edit(session.root, "ruleLabel", "Market groceries"));
        QVERIFY(edit(session.root, "ruleExpression", "Market")); mouse("ruleWholeField");
        QVERIFY(edit(session.root, "ruleCategory", "Groceries")); enter("ruleCategory");
        mouse("previewThisRule");
        QCOMPARE(session.model.state().value("rulePreview").toMap().value("records").toList().size(), 0);
        mouse("saveRules");
        mouse("accountNavigation_1");
        import("!Type:Bank\nD2026-01-10\nT2000.00\nPPayday\nLIncome\n^\n", "1");
        QCOMPARE(session.model.state().value("balance").toString(), amount("2,963.82"));
        mouse("reviewNavigation"); mouse("commitImport");
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 0);
        mouse("accountNavigation_2");
        QCOMPARE(session.model.state().value("balance").toString(), amount("-20.00"));
        import("!Type:CCard\nD2026-02-15\nT-55.00\nPMarket\n^\nD2026-02-22\nT-15.00\nPHome supplies\nLHousehold\n^\n", "2");
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 2);
        QCOMPARE(session.model.state().value("balance").toString(), amount("-90.00"));
        // The saved rule must categorize the later import, not just its preview.
        bool categorized = false;
        for (int i=0; i<session.model.rowCount(); ++i) {
            const auto row = session.model.rowRecord(i);
            if (row.value("payee").toString()=="Market" && row.value("withdrawal").toString()==amount("55.00"))
                categorized = row.value("category").toString()=="Groceries";
        }
        QVERIFY(categorized);
        mouse("reportsNavigation"); mouse("newReport");
        auto* workspace = qobject_cast<QQuickItem*>(object(session.root, "reportsWorkspace")); QVERIFY(workspace);
        auto textClick = [&](const QString& text, bool twice=false) { mouseItem(byText(workspace, text), twice); };
        QVERIFY(edit(session.root, "reportName", "Household spending"));
        // Clear only the Categories checklist, then select the guide's two names.
        auto* groceries = byText(workspace, "Groceries"); QVERIFY(groceries);
        QQuickItem* categoryGroup = groceries;
        while (categoryGroup && !categoryGroup->property("choices").isValid()) categoryGroup=categoryGroup->parentItem();
        QVERIFY(categoryGroup); for(const auto* label:{"Clear expenses","Clear income"}){auto* clear=byText(categoryGroup,label);QVERIFY(clear);if(clear->isEnabled())mouseItem(clear);}
        textClick("Groceries"); textClick("Household");
        QVERIFY(edit(session.root, "reportStartDate0", date("2026-01-01")));
        QVERIFY(edit(session.root, "reportEndDate0", date("2026-01-31")));
        textClick("Add comparison");
        QVERIFY(edit(session.root, "reportStartDate1", date("2026-02-01")));
        QVERIFY(edit(session.root, "reportEndDate1", date("2026-02-28")));
        auto* graph = object(session.root, "reportGraph"); QVERIFY(graph);
        graph->setProperty("currentIndex", 2); QVERIFY(QMetaObject::invokeMethod(graph, "activated", Q_ARG(int, 2)));
        mouse("showReport");
        auto totals = [&]() { return session.model.state().value("reportResult").toMap().value("currencies").toList().value(0).toMap().value("totals").toList(); };
        QCOMPARE(totals(), (QVariantList{amount("56.18"), amount("70.00")}));
        capture("report");
        textClick(amount("48.18"), true);
        QTRY_VERIFY(session.model.state().value("reportLedgerActive").toBool());
        QCOMPARE(session.model.rowCount(), 3);
        const auto cornerIndex = session.model.occurrenceIndex(corner.value("batch").toString(), corner.value("ordinal").toInt());
        QVERIFY(cornerIndex>=0);
        QCOMPARE(session.model.rowRecord(cornerIndex).value("reportIncluded").toString(), amount("12.00"));
        auto* ledger = object(session.root, "durableLedger"); QVERIFY(ledger);
        QVERIFY(QMetaObject::invokeMethod(ledger, "positionViewAtIndex", Q_ARG(int, cornerIndex), Q_ARG(int, 1)));
        QTest::qWait(150);
        mouseItem(qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_"+QString::number(cornerIndex))), true);
        QVERIFY(edit(session.root, "durableDate", date("2026-02-22"))); enter("durableDate");
        QCOMPARE(totals(), (QVariantList{amount("36.18"), amount("90.00")}));
        QVERIFY(undoLatest());
        QCOMPARE(totals(), (QVariantList{amount("56.18"), amount("70.00")}));
        textClick("Back to report");
        const auto backup = session.path("Household-backup.spentit");
        QVERIFY(QMetaObject::invokeMethod(object(session.root, "fileCloseDataset"), "triggered"));
        wait(session.model);
        QVERIFY(QFile::copy(path, backup));
        QVERIFY(object(session.root, "datasetPath")->setProperty("text", backup));
        run(session.model, "open", {{"path", backup}}, false);
        QVERIFY(session.model.state().value("unlockRequired").toBool());
        QVERIFY(edit(session.root, "unlockPassword", "synthetic-guide-password")); mouse("openProtectedDataset");
        QCOMPARE(session.model.state().value("path").toString(), backup);
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 2);
        mouse("accountNavigation_1");
        QCOMPARE(session.model.state().value("balance").toString(), amount("2,963.82"));
        mouse("accountNavigation_2");
        QCOMPARE(session.model.state().value("balance").toString(), amount("-90.00"));
        mouse("reportsNavigation"); mouse("showReport");
        QCOMPARE(totals(), (QVariantList{amount("56.18"), amount("70.00")}));
        capture("restored");
    }
    void createOpenChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        QTemporaryDir guide("/tmp/SpentIt-guide-XXXXXX");
        QVERIFY(guide.isValid());
        const auto path = guide.filePath("Household.spentit");
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1100, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        QVERIFY(click(session.root, "newDataset"));
        wait(session.model);
        QVERIFY(object(session.root, "datasetPath")->setProperty("text", path));
        run(session.model, "prepare_create", {{"path", path}});
        QVERIFY(object(session.root, "initialAccountName")->setProperty("text", "Chequing"));
        QCOMPARE(object(session.root, "setupCurrency")->property("currentText").toString(), QString("CAD"));
        QCOMPARE(object(session.root, "setupDateFormat")->property("currentText").toString(), QString("YYYY-MM-DD"));
        QVERIFY(object(session.root, "defaultCategories")->property("checked").toBool());
        QVERIFY(object(session.root, "passwordProtection")->property("checked").toBool());
        auto* flick = object(session.root, "welcomeScreen")->property("contentItem").value<QObject*>();
        QVERIFY(flick);
        QTest::qWait(100);
        flick->setProperty("contentY", qMax(0.0, flick->property("contentHeight").toDouble() - flick->property("height").toDouble()));
        capture("create-file-setup");
        QVERIFY(object(session.root, "newDatasetPassword")->setProperty("text", "guide-example"));
        QVERIFY(object(session.root, "confirmDatasetPassword")->setProperty("text", "guide-example"));
        QVERIFY(click(session.root, "createDataset"));
        wait(session.model);
        QVERIFY(session.model.state().value("active").toBool());
        QCOMPARE(session.model.state().value("name").toString(), QString("Chequing"));
        session.window->resize(1440, 900);
        capture("create-file-workspace");
        session.window->resize(1100, 900);
        // The chapter's File > Close file route, followed by reopening the same file.
        QVERIFY(QMetaObject::invokeMethod(object(session.root, "fileCloseDataset"), "triggered"));
        wait(session.model);
        QTRY_VERIFY(session.model.state().value("unlockRequired").toBool());
        QTRY_VERIFY(object(session.root, "openProtectedDataset")->property("visible").toBool());
        flick->setProperty("contentY", 0);
        capture("create-file-unlock");
        QVERIFY(object(session.root, "unlockPassword")->setProperty("text", "wrong-example"));
        QVERIFY(click(session.root, "openProtectedDataset"));
        wait(session.model, false);
        QVERIFY(!session.model.state().value("active").toBool());
        QVERIFY(object(session.root, "welcomeError")->property("visible").toBool());
        QVERIFY(object(session.root, "unlockPassword")->setProperty("text", "guide-example"));
        QVERIFY(click(session.root, "openProtectedDataset"));
        wait(session.model);
        QVERIFY(session.model.state().value("active").toBool());
        QCOMPARE(session.model.state().value("name").toString(), QString("Chequing"));
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void accountsChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false}, {"name", "Chequing"}, {"currency", "CAD"}});
        auto mouse = [&](const QString& name, Qt::MouseButton button = Qt::LeftButton) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name));
            QVERIFY(item->isVisible());
            QTest::qWait(180); // Allow the popup transition to finish before hit testing.
            QTest::mouseClick(session.window, button, Qt::NoModifier,
                             item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        mouse("newAccount");
        QVERIFY(object(session.root, "newAccountName")->setProperty("text", "Credit card"));
        capture("accounts-new");
        auto* field = qobject_cast<QQuickItem*>(object(session.root, "newAccountName"));
        field->forceActiveFocus();
        QTest::keyClick(session.window, Qt::Key_Return);
        wait(session.model);
        QCOMPARE(session.model.state().value("name").toString(), QString("Credit card"));
        QCOMPARE(session.model.state().value("currency").toString(), QString("CAD"));
        QCOMPARE(session.model.state().value("opening").toString(), QString("0.00"));
        mouse("accountNavigation_1");
        wait(session.model);
        QCOMPARE(session.model.state().value("name").toString(), QString("Chequing"));
        QTest::qWait(100);
        auto* row = qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_0"));
        QVERIFY(row);
        QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          row->mapToScene(QPointF(160, row->height()/2)).toPoint());
        QTRY_VERIFY(object(session.root, "correctOpeningDialog")->property("visible").toBool());
        QVERIFY(object(session.root, "correctOpeningDate")->setProperty("text", "2026-01-01"));
        QVERIFY(object(session.root, "correctOpeningAmount")->setProperty("text", "1,000.00"));
        capture("accounts-opening");
        auto* amount = qobject_cast<QQuickItem*>(object(session.root, "correctOpeningAmount"));
        amount->forceActiveFocus();
        QTest::keyClick(session.window, Qt::Key_Return);
        wait(session.model);
        QCOMPARE(session.model.state().value("opening").toString(), QString("1,000.00"));
        QCOMPARE(session.model.state().value("openingDate").toString(), QString("2026-01-01"));
        mouse("accountNavigation", Qt::RightButton);
        QTRY_VERIFY(object(session.root, "accountContextMenu")->property("visible").toBool());
        capture("accounts-menu");
        mouse("reassignCurrency");
        QTRY_VERIFY(object(session.root, "currencyDialog")->property("visible").toBool());
        QVERIFY(object(session.root, "currencyWarning")->property("visible").toBool());
        QVERIFY(edit(session.root, "currencySearch", "USD"));
        mouse("confirmCurrency");
        wait(session.model);
        QCOMPARE(session.model.state().value("currency").toString(), QString("USD"));
        QCOMPARE(session.model.state().value("opening").toString(), QString("1,000.00"));
        mouse("accountNavigation", Qt::RightButton);
        mouse("undoAccountCommand"); mouse("confirmActionUndo");
        wait(session.model);
        QCOMPARE(session.model.state().value("currency").toString(), QString("CAD"));
        run(session.model,"close");run(session.model,"open",{{"path",session.path("Household.spentit")}});run(session.model,"select_account",{{"id","1"}});
        mouse("accountNavigation", Qt::RightButton);
        mouse("archiveAccount");
        wait(session.model);
        QCOMPARE(session.model.state().value("name").toString(), QString("Credit card"));
        mouse("archivedAccount_1");
        wait(session.model);
        QVERIFY(session.model.state().value("readOnly").toBool());
        mouse("archivedAccount_1", Qt::RightButton);
        mouse("reactivateAccount");
        wait(session.model);
        QVERIFY(!session.model.state().value("readOnly").toBool());
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void importChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Chequing"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        auto mouse = [&](const QString& name, Qt::MouseButton button = Qt::LeftButton) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name));
            QVERIFY(item->isVisible());
            QTest::qWait(180);
            QTest::mouseClick(session.window, button, Qt::NoModifier,
                item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        const auto incomePath = session.path("income.qif");
        QFile income(incomePath); QVERIFY(income.open(QIODevice::WriteOnly));
        income.write("!Type:Bank\nD2026-01-02\nT1500.00\nPSalary\n^\n"); income.close();
        // Supply the file-picker result without using the native file picker.
        run(session.model, "import", {{"path", incomePath}, {"account", ""}});
        auto* destination = object(session.root, "importDestinationDialog");
        QTRY_VERIFY(destination->property("visible").toBool());
        capture("import-destination");
        QVERIFY(QMetaObject::invokeMethod(destination, "accept"));
        wait(session.model);
        QCOMPARE(session.model.state().value("balance").toString(), QString("2,500.00"));
        auto* salary = qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_0"));
        QVERIFY(salary);
        QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier,
            salary->mapToScene(QPointF(160, 12)).toPoint());
        const auto path = session.path("household.qif");
        QFile source(path); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Account\nNCredit card\nTCCard\n^\n!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\n!Account\nNCash\nTCash\nB50.00\n^\n!Type:Cash\nD2026-01-18\nT-5.00\nPBakery\nLGroceries\n^\n"); source.close();
        run(session.model, "import", {{"path", path}, {"account", ""}});
        auto* mapping = object(session.root, "importAccountMapping");
        QTRY_VERIFY(mapping->property("visible").toBool());
        QCOMPARE(session.model.state().value("importPlan").toList().size(), 2);
        for (int index = 0; index < 2; ++index) {
            auto* choice = object(session.root, "sourceAccountChoice_" + QString::number(index));
            QVERIFY(choice);
            QVERIFY(choice->setProperty("currentIndex", 1));
            QVERIFY(QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 1)));
        }
        auto* cashOpening = object(session.root, "sourceOpeningBalance_1");
        QVERIFY(cashOpening->setProperty("currentIndex", 1));
        QVERIFY(QMetaObject::invokeMethod(cashOpening, "activated", Q_ARG(int, 1)));
        capture("import-mapping");
        mouse("confirmImportMappings");
        wait(session.model);
        QCOMPARE(session.model.state().value("accounts").toList().size(), 3);
        auto* categories = object(session.root, "importCategories");
        QTRY_VERIFY(categories->property("visible").toBool());
        capture("import-categories");
        mouse("applyImportCategories");
        wait(session.model);
        QTRY_VERIFY(!categories->property("visible").toBool());
        QCOMPARE(session.model.state().value("importCategoryDecisions").toList().size(), 0);
        // Account creation order follows the source sections.
        run(session.model, "select_account", {{"id", "2"}});
        QCOMPARE(session.model.state().value("name").toString(), QString("Credit card"));
        QCOMPARE(session.model.state().value("balance").toString(), QString("-36.18"));
        // Opening is the day before 2026-01-15, so newest-first order is refund, purchase, opening.
        auto* purchase = qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_1"));
        QVERIFY(purchase);
        QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier,
            purchase->mapToScene(QPointF(160, 12)).toPoint());
        capture("import-results");
        // Reimport the same file: explicit mappings remain required for multiple accounts.
        run(session.model, "import", {{"path", path}, {"account", ""}});
        QTRY_VERIFY(mapping->property("visible").toBool());
        for (int index = 0; index < 2; ++index) {
            auto* choice = object(session.root, "sourceAccountChoice_" + QString::number(index));
            const int choiceIndex = index + 3; // Choose, New, Chequing, Credit card, Cash.
            QVERIFY(choice->setProperty("currentIndex", choiceIndex));
            QVERIFY(QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, choiceIndex)));
        }
        mouse("confirmImportMappings"); wait(session.model);
        QCOMPARE(session.model.state().value("balance").toString(), QString("-36.18"));
        // Undo the duplicate-only import first, then preview the original multi-account import.
        mouse("accountNavigation", Qt::RightButton); mouse("undoLastImport"); wait(session.model);
        QTRY_VERIFY(object(session.root, "importUndoConfirm")->property("visible").toBool());
        mouse("confirmImportUndo"); wait(session.model);
        QCOMPARE(session.model.state().value("balance").toString(), QString("-36.18"));
        mouse("accountNavigation", Qt::RightButton); mouse("undoLastImport"); wait(session.model);
        QTRY_VERIFY(object(session.root, "importUndoConfirm")->property("visible").toBool());
        QCOMPARE(session.model.state().value("importUndoPreview").toMap().value("transactions").toInt(), 3);
        capture("import-undo");
        mouse("confirmImportUndo"); wait(session.model);
        QCOMPARE(session.model.state().value("balance").toString(), QString("0.00"));
        run(session.model, "select_account", {{"id", "1"}});
        QCOMPARE(session.model.state().value("balance").toString(), QString("2,500.00"));
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void transactionsChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        auto mouse = [&](const QString& name, Qt::KeyboardModifiers modifiers = Qt::NoModifier, bool twice = false) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name));
            QVERIFY(item->isVisible());
            QTest::qWait(180);
            const auto point = item->mapToScene(QPointF(100 < item->width() ? 100 : item->width()/2, 12)).toPoint();
            if (twice) QTest::mouseDClick(session.window, Qt::LeftButton, modifiers, point);
            else QTest::mouseClick(session.window, Qt::LeftButton, modifiers, point);
            wait(session.model);
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        QFile source(session.path("card.qif")); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nMGroceries for the week\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\n"); source.close();
        run(session.model, "import", {{"path", source.fileName()}, {"account", "1"}});
        QTRY_VERIFY(object(session.root, "importCategories")->property("visible").toBool());
        mouse("applyImportCategories");
        QTRY_VERIFY(!object(session.root, "importCategories")->property("visible").toBool());
        run(session.model, "commit", {{"ignoreWarnings", true}});
        session.root->setProperty("editorClosed", true);
        run(session.model, "query", {{"sort", 0}, {"descending", false}});
        mouse("historyNavigation");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("history"));
        QTRY_VERIFY(object(session.root, "historyRow_0"));
        capture("change-history");
        mouse("navigationBack");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("ledger"));
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        mouse("ledgerRow_1");
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 1);
        QVERIFY(session.root->property("editorClosed").toBool());
        mouse("ledgerRow_2", Qt::ShiftModifier);
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 2);
        mouse("ledgerRow_2", Qt::ControlModifier);
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 1);
        mouse("ledgerRow_1", Qt::NoModifier, true);
        QTRY_VERIFY(object(session.root, "durableMemo")->property("visible").toBool());
        QVERIFY(!object(session.root, "showEntryHistory"));
        QVERIFY(!object(session.root, "removeCommitted"));
        QVERIFY(object(session.root, "durableMemo")->property("text").toString().contains("Groceries for the week"));
        mouse("ledgerRow_1", Qt::NoModifier, true);
        mouse("splitToggle_2");
        capture("transactions-ledger");
        QVERIFY(edit(session.root, "ledgerSearch", "Market"));
        mouse("applyFilter");
        QCOMPARE(session.model.rowCount(), 2);
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        capture("transactions-search");
        mouse("ledgerColumn_1");
        QCOMPARE(session.model.state().value("sort").toInt(), 1);
        mouse("clearFilter");
        QCOMPARE(session.model.rowCount(), 4);
        QFile duplicate(session.path("possible-repeat.qif")); QVERIFY(duplicate.open(QIODevice::WriteOnly));
        duplicate.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket groceries\nLGroceries\n^\n"); duplicate.close();
        run(session.model, "import", {{"path", duplicate.fileName()}, {"account", "1"}});
        session.root->setProperty("editorClosed", true);
        mouse("reviewNavigation");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("review"));
        QTRY_VERIFY(session.model.state().value("reviewCount").toInt() > 0);
        capture("transactions-review");
        mouse("navigationBack");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("ledger"));
        mouse("navigationForward");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("review"));
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void searchExamplesWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        QFile source(session.path("card.qif")); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nMGroceries for the week\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\n"); source.close();
        run(session.model, "import", {{"path", source.fileName()}, {"account", "1"}});
        QTRY_VERIFY(object(session.root, "importCategories")->property("visible").toBool());
        const auto decision = session.model.state().value("importCategoryDecisions").toList().front().toMap();
        run(session.model, "resolve_import_categories", {{"source", decision.value("source")}, {"selected", decision.value("names")}});
        QTRY_VERIFY(!object(session.root, "importCategories")->property("visible").toBool());
        session.root->setProperty("editorClosed", true);
        auto search = [&](const QString& query, int count, bool success = true) {
            QVERIFY(edit(session.root, "ledgerSearch", query));
            auto* field = qobject_cast<QQuickItem*>(object(session.root, "ledgerSearch")); QVERIFY(field);
            field->forceActiveFocus();
            QTest::keyClick(session.window, Qt::Key_Return);
            wait(session.model, success);
            QCOMPARE(session.model.rowCount(), count);
            QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        };
        search("Market", 2);
        search("Market groceries", 2);
        search("\"Corner shop\"", 1);
        search("category:Groceries period:2026 Market", 2);
        search("category-exact:Groceries", 3);
        search("period:2026-01-15", 1);
        search("period:2026-01-15..2026-01-20 Market", 2);
        search("category:Household", 1);
        QCOMPARE(session.model.state().value("withdrawals").toString(), QString("-20.00"));
        search("regex:\"^Market$\"", 1);
        search("period:2026 regex:\"^Market\"", 2);
        search("QIF: period:2026 Market", 2);
        search("regex:\"(\"", 2, false);
        search("period:dec-jan", 2, false);
        auto* field = qobject_cast<QQuickItem*>(object(session.root, "ledgerSearch")); QVERIFY(field);
        field->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Escape); wait(session.model);
        QCOMPARE(session.model.rowCount(), 4);
        QVERIFY(field->property("text").toString().isEmpty());
    }
    void recoveryChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        const auto original = session.path("Household.spentit");
        const auto backup = session.path("Household-backup-2026-02-28.spentit");
        const auto recovered = session.path("Household-recovered.spentit");
        run(session.model, "create", {{"path", original}, {"protected", true},
            {"password", "synthetic-guide-password"}, {"confirmation", "synthetic-guide-password"},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        auto mouse = [&](const QString& name) {
            QTest::qWait(120);
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(session.window->contentItem()->contains(point));
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            QTRY_VERIFY_WITH_TIMEOUT(!session.model.busy(), 30000);
        };
        mouse("categoryNavigation");
        createCategory(session.model, session.root, "Groceries");
        mouse("accountNavigation");
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        QFile source(session.path("card.qif")); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\n"); source.close();
        run(session.model, "import", {{"path", source.fileName()}, {"account", "1"}});
        run(session.model, "commit");
        run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        run(session.model, "transfer_manual", {{"link", "guide-payment"}, {"operation", "guide-payment-create"},
            {"sequence", session.model.state().value("transferSequence")}, {"destination", "1"},
            {"date", "2026-01-28"}, {"amount", "-36.18"}, {"payee", "Credit card payment"}});
        QCOMPARE(session.model.state().value("balance").toString(), QString("963.82"));
        const auto newCount = session.model.state().value("newTransactions").toInt();
        QVERIFY(newCount > 0);
        mouse("reportsNavigation"); mouse("newReport");
        QVERIFY(edit(session.root, "reportName", "Household spending"));
        QVERIFY(edit(session.root, "reportStartDate0", "2026-01-01"));
        QVERIFY(edit(session.root, "reportEndDate0", "2026-01-31"));
        // The report form can extend below the viewport; its existing helper emits clicked.
        QVERIFY(click(session.root, "showReport")); wait(session.model);
        const auto totals = session.model.state().value("reportResult").toMap().value("currencies").toList();
        QCOMPARE(totals.size(), 1);
        QCOMPARE(totals.front().toMap().value("totals").toList(), (QVariantList{"36.18"}));
        mouse("sidebarSettings");
        {
            auto* settingsView = qobject_cast<QQuickItem*>(object(session.root, "activeSettings"));
            QVERIFY(settingsView);
            auto* flick = settingsView->property("contentItem").value<QQuickItem*>();
            QVERIFY(flick);
            auto* passwordField = qobject_cast<QQuickItem*>(object(session.root, "currentPassword"));
            QVERIFY(passwordField);
            QVERIFY(passwordField->isVisible());
            auto* changePassword = qobject_cast<QQuickItem*>(object(session.root, "changePassword"));
            QVERIFY(changePassword);
            QVERIFY(changePassword->isVisible());
            QVERIFY(!changePassword->isEnabled());
            const double extra = flick->property("contentHeight").toDouble() - flick->height();
            if (extra > 0) flick->setProperty("contentY", extra);
            QTest::qWait(200);
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (!output.isEmpty())
                QVERIFY(session.window->grabWindow().save(QDir(output).filePath("settings.png")));
            flick->setProperty("contentY", 0);
        }
        mouse("amountFormatting");
        QVERIFY(!session.model.state().value("amountFormatting").toBool());
        mouse("amountFormatting");
        QVERIFY(session.model.state().value("amountFormatting").toBool());
        mouse("rememberWindowPosition");
        QVERIFY(QMetaObject::invokeMethod(object(session.root, "fileCloseDataset"), "triggered"));
        wait(session.model);
        QVERIFY(!session.model.state().value("active").toBool());
        QVERIFY(QFile::copy(original, backup));
        run(session.model, "open", {{"path", backup}}, false);
        QVERIFY(session.model.state().value("unlockRequired").toBool());
        run(session.model, "unlock", {{"path", backup}, {"password", "synthetic-guide-password"}});
        QCOMPARE(session.model.state().value("newTransactions").toInt(), newCount);
        run(session.model, "select_account", {{"id", "2"}});
        QCOMPARE(session.model.state().value("balance").toString(), QString("963.82"));
        run(session.model, "select_account", {{"id", "1"}});
        QCOMPARE(session.model.state().value("balance").toString(), QString("0.00"));
        QVERIFY(QMetaObject::invokeMethod(object(session.root, "fileCloseDataset"), "triggered"));
        wait(session.model);
        QVERIFY(QFile::copy(backup, recovered));
        run(session.model, "unlock", {{"path", recovered}, {"password", "synthetic-guide-password"}});
        QCOMPARE(session.model.state().value("path").toString(), recovered);
        QCOMPARE(session.model.state().value("newTransactions").toInt(), newCount);
        mouse("reportsNavigation");
        QVERIFY(click(session.root, "showReport")); wait(session.model);
        QCOMPARE(session.model.state().value("reportResult").toMap().value("currencies").toList(), totals);
        // Lookup entry from a problem, followed by a linked task-specific explanation.
        auto* help = openHelp(session.root); QVERIFY(help);
        QVERIFY(edit(help, "helpSearch", "successful close"));
        QTRY_VERIFY(object(help, "helpResult_look-after-your-work"));
        QTest::qWait(150);
        auto* result = qobject_cast<QQuickItem*>(object(help, "helpResult_look-after-your-work")); QVERIFY(result);
        QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, result->mapToScene(QPointF(result->width()/2, 12)).toPoint());
        QTRY_COMPARE(help->property("topicId").toString(), QString("look-after-your-work"));
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:bring-in-transactions#when-something-needs-attention"))));
        QCOMPARE(help->property("topicId").toString(), QString("bring-in-transactions"));
        QVERIFY(edit(help, "helpSearch", ""));
        help->setProperty("contentsOpen", true);
        auto* contents = object(help, "helpContents"); QVERIFY(contents);
        QVERIFY(QMetaObject::invokeMethod(contents, "positionViewAtEnd"));
        QTest::qWait(150);
        auto* topic = qobject_cast<QQuickItem*>(object(help, "helpTopic_look-after-your-work")); QVERIFY(topic);
        QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, topic->mapToScene(QPointF(topic->width()/2, topic->height()/2)).toPoint());
        QTRY_COMPARE(help->property("topicId").toString(), QString("look-after-your-work"));
    }
    void reportsChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        auto* workspace = qobject_cast<QQuickItem*>(object(session.root, "reportsWorkspace")); QVERIFY(workspace);
        std::function<QQuickItem*(QQuickItem*, const QString&)> byText = [&](QQuickItem* parent, const QString& text) -> QQuickItem* {
            if (parent->isVisible() && parent->property("text").toString() == text && parent->metaObject()->indexOfSignal("clicked()") >= 0) return parent;
            for (auto* child : parent->childItems()) if (auto* found = byText(child, text)) return found;
            return nullptr;
        };
        auto mouseItem = [&](QQuickItem* item, bool twice = false) {
            QVERIFY2(item,"Missing UI item"); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
                if (!parent->property("contentY").isValid() || parent->property("contentHeight").toDouble() <= parent->height()) continue;
                const auto y = item->mapToItem(parent, QPointF(0, item->height()/2)).y();
                if (y < 0 || y > parent->height()) parent->setProperty("contentY", qBound(0.0,
                    parent->property("contentY").toDouble() + y - parent->height()/2,
                    parent->property("contentHeight").toDouble() - parent->height()));
            }
            QTest::qWait(150);
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(session.window->contentItem()->contains(point));
            if (twice) QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            else QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            wait(session.model);
        };
        auto mouse = [&](const QString& name) { mouseItem(qobject_cast<QQuickItem*>(object(session.root, name))); };
        auto textClick = [&](const QString& text, bool twice = false) { mouseItem(byText(workspace, text), twice); };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(200);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        mouse("categoryNavigation");
        for (const auto& name : {"Groceries", "Household", "Income"}) createCategory(session.model, session.root, name);
        QString incomeId;
        for (const auto& item : session.model.state().value("categories").toList()) {
            const auto category = item.toMap();
            if (category.value("path").toString() == QString("Income")) incomeId = category.value("id").toString();
        }
        QVERIFY(!incomeId.isEmpty());
        mouse("categoryIncome_" + incomeId);
        QTRY_VERIFY(!session.root->property("categorySaving").toBool());
        mouse("accountNavigation");
        QFile card(session.path("card.qif")); QVERIFY(card.open(QIODevice::WriteOnly));
        card.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\nD2026-02-15\nT-55.00\nPMarket\nLGroceries\n^\nD2026-02-22\nT-15.00\nPHome supplies\nLHousehold\n^\n"); card.close();
        run(session.model, "import", {{"path", card.fileName()}, {"account", "1"}});
        const auto cardBatch = session.model.state().value("selected").toMap().value("batch");
        run(session.model, "commit");
        run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        QFile income(session.path("income.qif")); QVERIFY(income.open(QIODevice::WriteOnly));
        income.write("!Type:Bank\nD2026-01-10\nT2000.00\nPPayday\nLIncome\n^\n"); income.close();
        run(session.model, "import", {{"path", income.fileName()}, {"account", "2"}});
        run(session.model, "transfer_manual", {{"link", "guide-card-payment"}, {"operation", "guide-create-payment"},
            {"sequence", session.model.state().value("transferSequence")}, {"destination", "1"},
            {"date", "2026-01-28"}, {"amount", "-36.18"}, {"payee", "Credit card payment"}});
        mouse("reportsNavigation"); mouse("newReport");
        QVERIFY(edit(session.root, "reportName", "Household spending"));
        textClick("Uncategorized"); textClick("Income");
        QVERIFY(edit(session.root, "reportStartDate0", "2026-01-01"));
        QVERIFY(edit(session.root, "reportEndDate0", "2026-01-31"));
        // Exercise the actual shared picker beside the report's From field.
        auto* date = qobject_cast<QQuickItem*>(object(session.root, "reportStartDate0")); QVERIFY(date);
        QQuickItem* picker = nullptr;
        for (auto* sibling : date->parentItem()->childItems())
            if (sibling->property("field").value<QObject*>() == date) picker = sibling;
        QVERIFY(picker); mouseItem(picker);
        // Resolve the keys from this picker, not the ledger's separate calendar.
        auto* keys = qobject_cast<QQuickItem*>(picker->findChild<QObject*>("calendarKeys")); QVERIFY(keys);
        keys->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Right); QTest::keyClick(session.window, Qt::Key_Return);
        QCOMPARE(date->property("text").toString(), QString("2026-01-02"));
        QVERIFY(edit(session.root, "reportStartDate0", "2026-01-01"));
        textClick("Add comparison");
        QTRY_VERIFY(object(session.root, "reportStartDate1"));
        QVERIFY(edit(session.root, "reportStartDate1", "2026-02-01"));
        QVERIFY(edit(session.root, "reportEndDate1", "2026-02-28"));
        auto* graph = qobject_cast<QQuickItem*>(object(session.root, "reportGraph")); QVERIFY(graph);
        // The chart's current index follows the draft, so arrow keys cannot move it.
        // Open the list and choose the visible entry.
        auto chooseChart = [&](const QString& label) {
            mouseItem(graph);
            QTest::qWait(200);
            std::function<QQuickItem*(QQuickItem*)> labeled = [&](QQuickItem* parent) -> QQuickItem* {
                if (!parent || !parent->isVisible()) return nullptr;
                if (parent != graph && parent->property("text").toString() == label) return parent;
                for (auto* child : parent->childItems()) if (auto* found = labeled(child)) return found;
                return nullptr;
            };
            QQuickItem* choice = nullptr;
            for (QWindow* candidate : QGuiApplication::allWindows()) {
                auto* quick = qobject_cast<QQuickWindow*>(candidate);
                if (!quick || !quick->isVisible()) continue;
                choice = labeled(quick->contentItem());
                if (choice) break;
            }
            QVERIFY2(choice, qPrintable(label));
            mouseItem(choice);
        };
        chooseChart(QStringLiteral("Bar — grouped periods"));
        QCOMPARE(graph->property("currentIndex").toInt(), 2);
        auto* expenses = qobject_cast<QQuickItem*>(object(session.root, "reportExpensesGroup"));
        auto* incomeGroup = qobject_cast<QQuickItem*>(object(session.root, "reportIncomeGroup"));
        QVERIFY(expenses);
        QVERIFY(incomeGroup);
        for (auto* item : {expenses, incomeGroup}) {
            for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
                if (!parent->property("contentY").isValid() || parent->property("contentHeight").toDouble() <= parent->height()) continue;
                const auto y = item->mapToItem(parent, QPointF(0, item->height()/2)).y();
                if (y < 0 || y > parent->height()) parent->setProperty("contentY", qBound(0.0,
                    parent->property("contentY").toDouble() + y - parent->height()/2,
                    parent->property("contentHeight").toDouble() - parent->height()));
            }
        }
        QTest::qWait(150);
        QVERIFY(session.window->contentItem()->contains(expenses->mapToScene(QPointF(8, expenses->height()/2)).toPoint()));
        QVERIFY(session.window->contentItem()->contains(incomeGroup->mapToScene(QPointF(8, incomeGroup->height()/2)).toPoint()));
        capture("reports-editor");
        mouse("showReport");
        QTRY_COMPARE(workspace->property("mode").toString(), QString("result"));
        auto currency = [&]() { return session.model.state().value("reportResult").toMap().value("currencies").toList().front().toMap(); };
        QCOMPARE(currency().value("totals").toList(), (QVariantList{"56.18", "70.00"}));
        capture("reports-comparison");
        textClick("48.18", true);
        QTRY_VERIFY(session.model.state().value("reportLedgerActive").toBool());
        QCOMPARE(session.model.rowCount(), 3);
        const auto splitIndex = session.model.occurrenceIndex(cardBatch.toString(), 2);
        QVERIFY(splitIndex >= 0);
        QCOMPARE(session.model.rowRecord(splitIndex).value("reportIncluded").toString(), QString("12.00"));
        QCOMPARE(session.model.rowRecord(splitIndex).value("reportAllocations").toStringList().size(), 1);
        run(session.model, "inspect", {{"batch", cardBatch}, {"ordinal", 2}});
        QTRY_VERIFY(object(session.root, "editSplitAmount_1"));
        capture("reports-transactions");
        QVERIFY(edit(session.root, "durableDate", "2026-02-22"));
        auto* dateEdit = qobject_cast<QQuickItem*>(object(session.root, "durableDate")); QVERIFY(dateEdit);
        dateEdit->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Return); wait(session.model);
        QVERIFY(!session.model.rowRecord(session.model.occurrenceIndex(cardBatch.toString(), 2)).value("reportMatches").toBool());
        QCOMPARE(currency().value("totals").toList(), (QVariantList{"36.18", "90.00"}));
        run(session.model, "undo_edit");
        QCOMPARE(currency().value("totals").toList(), (QVariantList{"56.18", "70.00"}));
        textClick("Back to report"); textClick("Back to Reports");
        chooseChart(QStringLiteral("Pie"));
        QCOMPARE(graph->property("currentIndex").toInt(), 3);
        mouse("showReport");
        QTRY_COMPARE(workspace->property("mode").toString(), QString("result"));
        QCOMPARE(currency().value("totals").toList(), (QVariantList{"56.18", "70.00"}));
        capture("reports-pie");
        textClick("Back to Reports");
        QVERIFY(edit(session.root, "reportName", "Unfinished report name"));
        mouse("accountNavigation");
        auto* leave = namedWindow("reportsLeaveDialog"); QVERIFY(leave);
        QTRY_VERIFY(leave->isVisible());
        QVERIFY(click(leave, "reportsGuardStay"));
        QVERIFY(workspace->property("dirty").toBool());
        mouse("accountNavigation");
        QVERIFY(click(leave, "reportsGuardDiscard"));
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("ledger"));
    }
    void acceptanceChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        const auto path = session.path("Household.spentit");
        run(session.model, "create", {{"path", path}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        auto mouse = [&](const QString& name) {
            QTest::qWait(120);
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(session.window->contentItem()->contains(point));
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            wait(session.model);
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(180);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        mouse("categoryNavigation");
        createCategory(session.model, session.root, "Groceries");
        createCategory(session.model, session.root, "Household");
        mouse("accountNavigation");
        QFile card(session.path("card.qif")); QVERIFY(card.open(QIODevice::WriteOnly));
        card.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\n"); card.close();
        run(session.model, "import", {{"path", card.fileName()}, {"account", "1"}});
        run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        QFile income(session.path("income.qif")); QVERIFY(income.open(QIODevice::WriteOnly));
        income.write("!Type:Bank\nD2026-01-10\nT2000.00\nPPayday\n^\n"); income.close();
        run(session.model, "import", {{"path", income.fileName()}, {"account", "2"}});
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 4);
        mouse("reviewNavigation");
        QCOMPARE(session.model.state().value("reviewCount").toInt(), 0);
        capture("accept-ready");
        mouse("commitImport");
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 0);
        QCOMPARE(session.model.state().value("balance").toString(), QString("3,000.00"));
        run(session.model, "select_account", {{"id", "1"}});
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        QCOMPARE(session.model.state().value("committed").toString(), QString("-56.18"));
        // A later export has one valid purchase and one unreadable amount.
        QFile later(session.path("later.qif")); QVERIFY(later.open(QIODevice::WriteOnly));
        later.write("!Type:CCard\nD2026-01-23\nT-5.00\nPCoffee\n^\nD2026-01-24\nTbad\nPAmount to check\n^\n"); later.close();
        run(session.model, "import", {{"path", later.fileName()}, {"account", "1"}});
        QCOMPARE(session.model.state().value("importFailures").toInt(), 1);
        mouse("reviewNavigation");
        mouse("commitImport");
        QVERIFY(session.model.state().value("canIgnoreWarnings").toBool());
        QCOMPARE(object(session.root, "acceptAnyway")->property("text").toString(), QString("Accept anyway"));
        capture("accept-review");
        mouse("acceptAnyway");
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 0);
        QCOMPARE(session.model.state().value("importFailures").toInt(), 1);
        QCOMPARE(session.model.state().value("balance").toString(), QString("-61.18"));
        // Keep another saved purchase new through quitting and reopening.
        QFile next(session.path("next.qif")); QVERIFY(next.open(QIODevice::WriteOnly));
        next.write("!Type:CCard\nD2026-01-25\nT-3.00\nPBus fare\n^\n"); next.close();
        run(session.model, "import", {{"path", next.fileName()}, {"account", "1"}});
        session.window->close();
        QTRY_VERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        capture("accept-quit");
        mouse("quitCommitCancel");
        QVERIFY(session.model.state().value("active").toBool());
        session.window->close();
        QTRY_VERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        mouse("quitCommitNo");
        QTRY_VERIFY(!session.model.state().value("active").toBool());
        ImportModel reopened(storage::document_gateway());
        run(reopened, "open", {{"path", path}});
        QCOMPARE(reopened.state().value("newTransactions").toInt(), 1);
        run(reopened, "select_account", {{"id", "1"}});
        QCOMPARE(reopened.state().value("balance").toString(), QString("-64.18"));
        QCOMPARE(reopened.state().value("importFailures").toInt(), 1);
    }

    void rulesChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        run(session.model, "select_account", {{"id", "1"}});
        auto mouse = [&](QObject* root, QQuickWindow* window, const QString& name) {
            QTest::qWait(120);
            auto* item = qobject_cast<QQuickItem*>(object(root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(window->contentItem()->contains(point));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
            wait(session.model);
        };
        auto mainClick = [&](const QString& name) { mouse(session.root, session.window, name); };
        auto capture = [&](QQuickWindow* window, const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if(output.isEmpty()) return;
            QTest::qWait(180);
            QVERIFY(window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        mainClick("categoryNavigation");
        createCategory(session.model, session.root, "Groceries");
        createCategory(session.model, session.root, "Household");
        mainClick("accountNavigation");
        QFile source(session.path("card.qif")); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\n"); source.close();
        run(session.model, "import", {{"path", source.fileName()}, {"account", "1"}});
        mainClick("rulesNavigation"); mainClick("newRule");
        QVERIFY(edit(session.root, "ruleLabel", "Market groceries"));
        QVERIFY(edit(session.root, "ruleExpression", "Market"));
        mainClick("ruleWholeField");
        auto* category = qobject_cast<QQuickItem*>(object(session.root, "ruleCategory")); QVERIFY(category);
        category->forceActiveFocus(); QVERIFY(edit(session.root, "ruleCategory", "Groceries"));
        QTest::keyClick(session.window, Qt::Key_Return); wait(session.model);
        QCOMPARE(session.root->property("ruleCategoryId").toString().isEmpty(), false);
        capture(session.window, "rules-editor");
        mainClick("previewThisRule");
        QCOMPARE(session.model.state().value("rulePreview").toMap().value("records").toList().size(), 1);
        auto* scroll = object(session.root, "rulesPage"); QVERIFY(scroll);
        auto* flick = scroll->property("contentItem").value<QObject*>(); QVERIFY(flick);
        QTRY_VERIFY(object(session.root, "ruleMatchPayee_0"));
        QTest::qWait(200); // Let the preview rows participate in the ScrollView layout.
        flick->setProperty("contentY", qMax(0.0, flick->property("contentHeight").toDouble() - flick->property("height").toDouble()));
        auto* match = qobject_cast<QQuickItem*>(object(session.root, "ruleMatchPayee_0")); QVERIFY(match);
        QTRY_VERIFY(match->mapToScene(QPointF(0, match->height())).y() < session.window->height() - 40);
        capture(session.window, "rules-preview");
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"applyRules"),"clicked"));wait(session.model);
        QVERIFY(!session.root->property("rulesDirty").toBool());
        QVERIFY(session.model.state().value("canUndoRules").toBool());
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"undoRules"),"clicked"));QTRY_VERIFY(object(session.root,"actionUndoConfirm")->property("visible").toBool()); mainClick("confirmActionUndo");
        QVERIFY(!session.model.state().value("canUndoRules").toBool());
        mainClick("previewThisRule");
        QCOMPARE(session.model.state().value("rulePreview").toMap().value("records").toList().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"applyRules"),"clicked"));wait(session.model);
        mainClick("previewThisRule");
        QCOMPARE(session.model.state().value("rulePreview").toMap().value("records").toList().size(), 0);
        flick->setProperty("contentY", 0);
        QVERIFY(edit(session.root, "ruleLabel", "Market groceries revised"));
        mainClick("accountNavigation");
        auto* leave = namedWindow("rulesLeaveDialog"); QVERIFY(leave);
        QTRY_VERIFY(leave->isVisible());
        QCOMPARE(leave->modality(), Qt::ApplicationModal);
        capture(leave, "rules-leave");
        mouse(leave, leave, "rulesGuardStay");
        QTRY_VERIFY(!leave->isVisible());
        QVERIFY(session.root->property("rulesDirty").toBool());
        mainClick("accountNavigation");
        QTRY_VERIFY(leave->isVisible());
        mouse(leave, leave, "rulesGuardDiscard");
        QTRY_COMPARE(session.root->property("currentPage").toString(), QString("ledger"));
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
    }
    void categoriesChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
        run(session.model, "select_account", {{"id", "1"}});
        auto mouse = [&](const QString& name, Qt::MouseButton button = Qt::LeftButton, bool twice = false) {
            QTest::qWait(180);
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            const auto point = item->mapToScene(QPointF(qMin(100.0, item->width()/2), qMin(15.0, item->height()/2))).toPoint();
            if(twice) QTest::mouseDClick(session.window, button, Qt::NoModifier, point);
            else QTest::mouseClick(session.window, button, Qt::NoModifier, point);
            wait(session.model);
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if(output.isEmpty())return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        auto enter = [&](const QString& name) {
            auto* field=qobject_cast<QQuickItem*>(object(session.root,name)); QVERIFY(field);
            field->forceActiveFocus(); QTest::keyClick(session.window,Qt::Key_Return);wait(session.model);
        };
        auto undoLatest = [&] {
            auto* ledger = qobject_cast<QQuickItem*>(object(session.root, "durableLedger"));
            if (!ledger) return false;
            ledger->forceActiveFocus();
            QTest::keySequence(session.window, QKeySequence::Undo);
            auto* confirm = object(session.root, "confirmActionUndo");
            if (!confirm || !confirm->property("visible").toBool()) return false;
            mouse("confirmActionUndo");
            return true;
        };
        mouse("categoryNavigation");
        createCategory(session.model,session.root,"Groceries");
        createCategory(session.model,session.root,"Household");
        createCategory(session.model,session.root,"Food: Meals out");
        createCategory(session.model,session.root,"Income");
        QString incomeId;
        for (const auto& item : session.model.state().value("categories").toList()) {
            const auto category = item.toMap();
            if (category.value("path").toString() == QString("Income")) incomeId = category.value("id").toString();
        }
        QVERIFY(!incomeId.isEmpty());
        mouse("categoryIncome_" + incomeId);
        QTRY_VERIFY(!session.root->property("categorySaving").toBool());
        mouse("categorySortAscending");
        capture("categories-workspace");
        // Manage a category through its real row context menu.
        QVERIFY(edit(session.root,"catalogSearch","Food: Meals out"));
        mouse("catalogList",Qt::RightButton);
        QTRY_VERIFY(object(session.root,"categoryContextMenu")->property("visible").toBool());
        mouse("archiveCategory");
        mouse("showArchivedCategories");
        mouse("catalogList",Qt::RightButton);
        QVERIFY(object(session.root,"archiveCategory"));
        QTRY_COMPARE(object(session.root,"archiveCategory")->property("text").toString(),QString("Unarchive"));
        mouse("archiveCategory");
        mouse("catalogList",Qt::RightButton);mouse("renameCategory");
        QVERIFY(edit(session.root,"categoryName","Food: Eating out"));enter("categoryName");
        mouse("accountNavigation");
        QFile source(session.path("card.qif"));QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\nD2026-01-22\nT-20.00\nPCorner shop\nSGroceries\n$-12.00\nSHousehold\n$-8.00\n^\n");source.close();
        run(session.model,"import",{{"path",source.fileName()},{"account","1"}});
        session.root->setProperty("editorClosed",true);
        run(session.model,"query",{{"sort",0},{"descending",false}});
        mouse("ledgerRow_1",Qt::LeftButton,true);
        auto* category=qobject_cast<QQuickItem*>(object(session.root,"splitCategory_0"));QVERIFY(category);
        category->forceActiveFocus();QVERIFY(edit(session.root,"splitCategory_0","gro"));
        QTRY_VERIFY(category->findChild<QObject*>("categoryDropdown_0"));
        QTRY_VERIFY(category->findChild<QObject*>("categoryDropdown_0")->property("visible").toBool());
        capture("categories-completion");enter("splitCategory_0");
        const auto savedSplits=session.model.state().value("selected").toMap().value("draft").toMap().value("splits").toList();
        QVERIFY(!savedSplits.isEmpty());
        QCOMPARE(savedSplits.front().toMap().value("category").toString(),QString("Groceries"));
        mouse("ledgerRow_1",Qt::LeftButton,true);
        {
            // Open the menu toward the right, so Change category has room to cascade.
            auto* row = qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_1"));
            QVERIFY(row);
            const auto menuPoint = row->mapToScene(QPointF(qMax(100.0, row->width() - 80), 15)).toPoint();
            QTest::mouseClick(session.window, Qt::RightButton, Qt::NoModifier, menuPoint);
            wait(session.model);
        }
        QTRY_VERIFY(object(session.root,"ledgerContext")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"ledgerCategoryMenu"),"open"));
        QTRY_VERIFY(object(session.root,"ledgerCategoryMenu")->property("visible").toBool());
        // The cursor can rest on Resolve duplicate and open that cascade over Change category.
        auto* duplicateMenu = object(session.root, "ledgerDuplicateMenu");
        if (duplicateMenu && duplicateMenu->property("visible").toBool())
            QVERIFY(QMetaObject::invokeMethod(duplicateMenu, "close"));
        QTest::qWait(200);
        capture("categories-menu");
        mouse("ledgerCategoryChoice_0");
        QVERIFY(session.model.state().value("canUndoTransferAssignment").toBool());
        QVERIFY(undoLatest());
        mouse("ledgerRow_3",Qt::RightButton);
        QTRY_VERIFY(object(session.root,"ledgerContext")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"ledgerCategoryMenu"),"open"));
        mouse("ledgerCategoryChoice_0");
        QTRY_VERIFY(object(session.root,"bulkCategoryDialog")->property("visible").toBool());
        capture("categories-split");mouse("ledgerCategorySplit_1");
        QVERIFY(undoLatest());
        QCOMPARE(session.model.state().value("balance").toString(),QString("-56.18"));
    }
    void editingChapterWalkthrough() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        auto mouse = [&](const QString& name) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            QTest::qWait(180);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
            wait(session.model);
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        auto enter = [&](const QString& name) {
            auto* field = qobject_cast<QQuickItem*>(object(session.root, name)); QVERIFY(field);
            field->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Return); wait(session.model);
        };
        mouse("categoryNavigation");
        createCategory(session.model, session.root, "Household");
        mouse("accountNavigation");
        QFile source(session.path("card.qif")); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\nLGroceries\n^\nD2026-01-20\nT6.00\nPMarket refund\nLGroceries\n^\n"); source.close();
        run(session.model, "import", {{"path", source.fileName()}, {"account", "1"}});
        QTRY_VERIFY(object(session.root, "importCategories")->property("visible").toBool());
        mouse("applyImportCategories");
        session.root->setProperty("editorClosed", true);
        mouse("addTransaction");
        QVERIFY(edit(session.root, "durableDate", "2026-01-22"));
        QVERIFY(edit(session.root, "durablePayee", "Corner shop"));
        QVERIFY(edit(session.root, "durableTotal", "-20.00"));
        QVERIFY(edit(session.root, "durableMemo", "Groceries and cleaning supplies"));
        capture("editing-new");
        mouse("transactionCalendar");
        QTRY_VERIFY(object(object(session.root,"transactionCalendar"), "calendarPopup")->property("visible").toBool());
        capture("editing-calendar");
        auto* keys = qobject_cast<QQuickItem*>(object(object(session.root,"transactionCalendar"), "calendarKeys")); QVERIFY(keys);
        keys->forceActiveFocus(); QTest::keyClick(session.window, Qt::Key_Right); QTest::keyClick(session.window, Qt::Key_Return);
        QCOMPARE(object(session.root, "durableDate")->property("text").toString(), QString("2026-01-23"));
        QVERIFY(edit(session.root, "durableDate", "2026-01-22"));
        enter("durableTotal");
        QVERIFY(!session.model.state().value("selected").toMap().value("newManual").toBool());
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        mouse("addSplit");
        QTRY_VERIFY(object(session.root, "editSplitAmount_1"));
        QVERIFY(edit(session.root, "editSplitAmount_1", "-8.00"));
        enter("editSplitAmount_1");
        auto category = [&](int index, const QString& text) {
            const auto name = "splitCategory_" + QString::number(index);
            QVERIFY(edit(session.root, name, text)); enter(name);
        };
        category(0, "Groceries"); category(1, "Household");
        auto splits = session.model.state().value("selected").toMap().value("draft").toMap().value("splits").toList();
        QCOMPARE(splits.size(), 2);
        QCOMPARE(splits[0].toMap().value("amount").toString(), QString("-12.00"));
        QCOMPARE(splits[1].toMap().value("amount").toString(), QString("-8.00"));
        capture("editing-splits");
        QVERIFY(edit(session.root, "editSplitAmount_0", "-10.00"));
        mouse("addTransaction");
        QTRY_VERIFY(object(session.root, "splitMismatchDialog")->property("visible").toBool());
        capture("editing-mismatch");
        mouse("splitKeepEditing");
        QVERIFY(edit(session.root, "editSplitAmount_0", "-12.00")); enter("editSplitAmount_0");
        QVERIFY(edit(session.root, "durableMemo", "Unfinished note"));
        mouse("addTransaction");
        QTRY_VERIFY(object(session.root, "unsavedDialog")->property("visible").toBool());
        mouse("guardStay");
        auto* memo = qobject_cast<QQuickItem*>(object(session.root, "durableMemo")); memo->forceActiveFocus();
        QTest::keyClick(session.window, Qt::Key_Escape); wait(session.model);
        QCOMPARE(object(session.root, "durableMemo")->property("text").toString(), QString("Groceries and cleaning supplies"));
        QVERIFY(edit(session.root, "durablePayee", "Corner shop corrected")); enter("durablePayee");
        run(session.model, "undo_edit");
        QCOMPARE(session.model.state().value("selected").toMap().value("draft").toMap().value("payee").toString(), QString("Corner shop"));
    }
    void reviewChapterWalkthrough_data() {
        QTest::addColumn<bool>("difference");
        QTest::newRow("merge-link") << false;
        QTest::newRow("difference") << true;
    }
    void reviewChapterWalkthrough() {
        QFETCH(bool, difference);
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false},
            {"name", difference ? "Chequing" : "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        auto mouse = [&](const QString& name) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name));
            QVERIFY2(item, qPrintable(name)); QVERIFY2(item->isVisible(),qPrintable(item->objectName())); QVERIFY2(item->isEnabled(),qPrintable(item->objectName()));
            QTest::qWait(180);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
            wait(session.model);
        };
        auto capture = [&](const QString& name) {
            const auto output = qEnvironmentVariable("SPENTIT_GUIDE_CAPTURE");
            if (output.isEmpty()) return;
            QTest::qWait(150);
            QVERIFY(session.window->grabWindow().save(QDir(output).filePath(name + ".png")));
        };
        auto undoLatest = [&] {
            auto* questions = qobject_cast<QQuickItem*>(object(session.root, "reviewQuestions"));
            if (!questions) return false;
            questions->forceActiveFocus();
            QTest::keySequence(session.window, QKeySequence::Undo);
            auto* confirm = object(session.root, "confirmActionUndo");
            if (!confirm || !confirm->property("visible").toBool()) return false;
            mouse("confirmActionUndo");
            return true;
        };
        int sequence = 0;
        auto import = [&](const QByteArray& bytes, const QString& account) {
            QFile file(session.path("review-" + QString::number(++sequence) + ".qif"));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
            run(session.model, "import", {{"path", file.fileName()}, {"account", account}});
        };
        if (!difference) {
            // The chapter pair has one earlier import, so Merge keeps it without asking.
            // This picture needs two new copies from the same later import, which still asks.
            run(session.model, "close");
            run(session.model, "create", {{"path", session.path("keep-picture.spentit")}, {"protected", false},
                {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
            run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket statement\n^\n", "1");
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket groceries\n^\nD2026-01-15\nT-42.18\nPMarket\n^\n", "1");
            mouse("reviewNavigation");
            const auto keepQuestions = session.model.state().value("reviewQuestions").toList();
            QVERIFY(!keepQuestions.isEmpty());
            const auto keepRows = keepQuestions.front().toMap().value("rows").toList();
            int market = -1, groceries = -1;
            QString sameOrder;
            for (int i = 0; i < keepRows.size(); ++i) {
                const auto row = keepRows.at(i).toMap();
                if (row.value("payee").toString() == QString("Market groceries")) {
                    groceries = i;
                    sameOrder = row.value("importOrder").toString();
                }
            }
            for (int i = 0; i < keepRows.size(); ++i) {
                const auto row = keepRows.at(i).toMap();
                if (row.value("payee").toString() == QString("Market") && row.value("importOrder").toString() == sameOrder) market = i;
            }
            QStringList described;
            for (const auto& value : keepRows) {
                const auto row = value.toMap();
                described << row.value("payee").toString() + " / " + row.value("importOrder").toString();
            }
            QVERIFY2(market >= 0 && groceries >= 0, qPrintable(described.join(" | ")));
            QTRY_VERIFY(object(session.root, "reviewRowSelect_" + QString::number(market)));
            mouse("reviewRowSelect_" + QString::number(market));
            mouse("reviewRowSelect_" + QString::number(groceries));
            auto* merge = object(session.root, "reviewYes");
            QTRY_VERIFY(merge && merge->property("enabled").toBool());
            mouse("reviewYes");
            auto* keep = object(session.root, "reviewSurvivor");
            QVERIFY(keep);
            QTRY_VERIFY(keep->property("visible").toBool());
            capture("review-keep");
            QVERIFY(QMetaObject::invokeMethod(keep, "reject"));
            QTRY_VERIFY(!keep->property("visible").toBool());
            run(session.model, "close");
            run(session.model, "create", {{"path", session.path("Household.spentit")}, {"protected", false}, {"overwrite", true},
                {"name", "Credit card"}, {"currency", "CAD"}, {"defaultCategories", false}});
            run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\n^\nD2026-01-20\nT6.00\nPMarket refund\n^\n", "1");
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket groceries\n^\n", "1");
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            mouse("reviewNavigation");
            QTRY_VERIFY(object(session.root, "reviewYes")); mouse("reviewYes");
            // The unique earliest import is the automatic survivor.
            QVERIFY(!object(session.root, "reviewSurvivor")->property("visible").toBool());
            QCOMPARE(session.model.state().value("balance").toString(), QString("-36.18"));
            QVERIFY(undoLatest());
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            QTRY_VERIFY(object(session.root, "reviewNo")); mouse("reviewNo");
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 0);
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            QVERIFY(undoLatest());
            QTRY_VERIFY(object(session.root, "reviewYes")); mouse("reviewYes");
            QVERIFY(!object(session.root, "reviewSurvivor")->property("visible").toBool());
            run(session.model, "create_account", {{"id", "2"}, {"name", "Chequing"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
            run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "1,000.00"}});
            import("!Type:Bank\nD2026-01-28\nT-36.18\nPCredit card payment\n^\n", "2");
            run(session.model, "select_account", {{"id", "1"}});
            import("!Type:CCard\nD2026-01-29\nT36.18\nPPayment received\n^\n", "1");
            mouse("reviewNavigation");
            QTRY_VERIFY(object(session.root, "reviewLinkCandidate_1"));
            capture("review-link");
            mouse("reviewYes");
            QCOMPARE(session.model.state().value("balance").toString(), QString("0.00"));
            QCOMPARE(session.model.state().value("transfers").toList().size(), 1);
            mouse("accountNavigation");
            session.root->setProperty("editorClosed", true);
            run(session.model, "query", {{"sort", 0}, {"descending", false}});
            QTRY_VERIFY(object(session.root, "ledgerTransferArrow_3"));
            capture("review-linked");
            mouse("ledgerTransferArrow_3");
            QTRY_COMPARE(session.model.state().value("name").toString(), QString("Chequing"));
            QCOMPARE(session.model.state().value("balance").toString(), QString("963.82"));
        } else {
            import("!Type:Bank\nD2026-01-28\nT-37.18\nPCredit card payment\nL[Credit card]\n^\n", "1");
            run(session.model, "create_account", {{"id", "2"}, {"name", "Credit card"}, {"currency", "CAD"}, {"date", "2026-01-01"}});
            import("!Type:CCard\nD2026-01-29\nT36.18\nPPayment received\n^\n", "2");
            mouse("reviewNavigation");
            QTRY_VERIFY(object(session.root, "reviewYes")); mouse("reviewYes");
            QTRY_VERIFY(object(session.root, "reviewDifference")->property("visible").toBool());
            QCOMPARE(session.model.state().value("reviewLinkPreview").toMap().value("common").toString(), QString("36.18"));
            QCOMPARE(session.model.state().value("reviewLinkPreview").toMap().value("remainder").toString(), QString("1.00"));
            capture("review-difference");
            mouse("reviewDifferenceNo");
            QTRY_VERIFY(!object(session.root, "reviewDifference")->property("visible").toBool());
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 1);
            mouse("reviewYes");
            QTRY_VERIFY(object(session.root, "reviewDifference")->property("visible").toBool());
            mouse("reviewDifferenceYes");
            QCOMPARE(session.model.state().value("balance").toString(), QString("36.18"));
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 0);
            QVERIFY(undoLatest());
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 1);
        }
        QGuiApplication::styleHints()->unsetColorScheme();
    }
    void contextHelpPreservesWorkspaceAndEditor() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        session.window->resize(1280, 900);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto mouse = [&](QObject* owner, const QString& name) {
            auto* item = qobject_cast<QQuickItem*>(object(owner, name));
            QVERIFY2(item, qPrintable(name));
            QTRY_VERIFY(item->isVisible() && item->isEnabled() && item->width() > 0);
            QTest::qWait(50);
            auto* surface = item->window(); QVERIFY(surface);
            const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
            QVERIFY(QRect(QPoint(), surface->size()).contains(point));
            QTest::mouseClick(surface, Qt::LeftButton, Qt::NoModifier, point);
        };
        mouse(session.root, "welcomeHelp");
        auto* help = namedWindow("helpWindow"); QVERIFY(help);
        QTRY_COMPARE(help->property("topicId").toString(), QString("create-open-file"));
        QVERIFY(!session.model.state().value("active").toBool());
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        createDocument(session, "Context help");
        // Every instantiated dialog carries a fixed target validated by the catalog.
        int dialogs = 0;
        for (auto* candidate : session.root->findChildren<QObject*>()) {
            const auto target = candidate->property("helpTarget");
            if (!target.isValid()) continue;
            ++dialogs;
            QVERIFY2(catalog.resolveLink("", target.toString()).value("ok").toBool(), qPrintable(target.toString()));
        }
        QCOMPARE(dialogs, 34);
        const QList<QPair<QString, QString>> pages{{"ledger", "find-transactions"}, {"review", "review-duplicates-transfers"},
            {"categories", "organize-categories"}, {"rules", "use-rules"}, {"reports", "answer-with-reports"},
            {"history", "add-correct-transactions"}, {"settings", "look-after-your-work"}};
        for (const auto& page : pages) {
            QVERIFY(session.root->setProperty("currentPage", page.first));
            const auto before = session.model.state();
            mouse(session.root, "workspaceHelp");
            QTRY_COMPARE(help->property("topicId").toString(), page.second);
            QCOMPARE(session.model.state(), before);
            QCOMPARE(session.root->property("currentPage").toString(), page.first);
            QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        }
        QVERIFY(session.root->setProperty("currentPage", "ledger"));
        mouse(session.root, "addTransaction"); wait(session.model);
        QVERIFY(edit(session.root, "durablePayee", "Unsaved purchase"));
        auto* payee = qobject_cast<QQuickItem*>(object(session.root, "durablePayee")); QVERIFY(payee);
        payee->forceActiveFocus();
        const auto before = session.model.state();
        mouse(session.root, "workspaceHelp");
        QTRY_COMPARE(help->property("topicId").toString(), QString("add-correct-transactions"));
        QCOMPARE(payee->property("text").toString(), QString("Unsaved purchase"));
        QVERIFY(session.root->property("dirty").toBool());
        QCOMPARE(session.model.state(), before);
        // General Help retains the current topic; unknown targets leave navigation available.
        QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
        QCOMPARE(help->property("topicId").toString(), QString("add-correct-transactions"));
        QVERIFY(QMetaObject::invokeMethod(session.root, "openContextHelp", Q_ARG(QVariant, QVariant("help:missing"))));
        QVERIFY(!help->property("linkMessage").toString().isEmpty());
        QVERIFY(object(help, "helpSearch"));
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        QTRY_VERIFY(payee->hasActiveFocus());
        QCOMPARE(session.model.state(), before);
        QVERIFY(edit(session.root, "durableTotal", "-1 - 2"));
        auto* amount = qobject_cast<QQuickItem*>(object(session.root, "durableTotal")); QVERIFY(amount);
        amount->forceActiveFocus();
        mouse(session.root, "workspaceHelp");
        QCOMPARE(amount->property("text").toString(), QString("-1 - 2"));
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        QTRY_VERIFY(amount->hasActiveFocus());
        QCOMPARE(amount->property("text").toString(), QString("-1 - 2"));
        // A real click in the unsaved decision opens its section without answering it.
        mouse(session.root, "sidebarSettings");
        auto* dialog = object(session.root, "unsavedDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        mouse(session.root, "unsavedDialogHelp");
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Correct an entry"));
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(session.root->property("dirty").toBool());
        QCOMPARE(session.model.state(), before);
        if (qEnvironmentVariableIsSet("SPENTIT_CONTEXT_CAPTURE")) {
            QVERIFY(session.window->grabWindow().save(qEnvironmentVariable("SPENTIT_CONTEXT_CAPTURE") + "/unsaved.png"));
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_CONTEXT_CAPTURE") + "/help.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        mouse(session.root, "guardStay");
        QVERIFY(session.root->property("dirty").toBool());
    }
    void contextHelpPreservesCategoryDraft() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        createDocument(session, "Category draft Help");
        QVERIFY(click(session.root, "categoryNavigation")); wait(session.model);
        QVERIFY(click(session.root, "newCategory"));
        QTRY_VERIFY(object(session.root, "categoryName"));
        QVERIFY(edit(session.root, "categoryName", "Unfinished category"));
        auto* field = qobject_cast<QQuickItem*>(object(session.root, "categoryName")); QVERIFY(field);
        field->forceActiveFocus();
        const auto before = session.model.state();
        auto* button = qobject_cast<QQuickItem*>(object(session.root, "workspaceHelp")); QVERIFY(button);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
        auto* help = namedWindow("helpWindow"); QVERIFY(help);
        QTRY_COMPARE(help->property("topicId").toString(), QString("organize-categories"));
        QTest::qWait(100);
        QVERIFY(session.root->property("categoryDirty").toBool());
        QCOMPARE(session.model.state(), before);
        QCOMPARE(field->property("text").toString(), QString("Unfinished category"));
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        QTRY_VERIFY(field->hasActiveFocus());
        if (QGuiApplication::platformName() == "cocoa")
            QTRY_COMPARE(QGuiApplication::focusWindow(), session.window);
        QTest::qWait(100);
        QVERIFY(session.root->property("categoryDirty").toBool());
        QCOMPARE(session.model.state(), before);
        // Native macOS menus have no QQuickWindow for QTest pointer events.
        // Exercise the real QML menu on the offscreen backend; native checks
        // above cover the workspace action and focus return.
        if (QGuiApplication::platformName() != "cocoa") {
            auto* menu = object(session.root, "helpMenu"); QVERIFY(menu);
            QVERIFY(QMetaObject::invokeMethod(menu, "open"));
            QTRY_VERIFY(menu->property("visible").toBool());
            QTest::qWait(100);
            QVERIFY(session.root->property("categoryDirty").toBool());
            QCOMPARE(session.model.state(), before);
            auto* action = qobject_cast<QQuickItem*>(object(session.root, "openScreenHelp")); QVERIFY(action);
            QVERIFY(action->window());
            QTest::mouseClick(action->window(), Qt::LeftButton, Qt::NoModifier,
                action->mapToScene(QPointF(action->width()/2, action->height()/2)).toPoint());
            QTRY_VERIFY(help->isVisible());
            QTRY_COMPARE(help->property("topicId").toString(), QString("organize-categories"));
            QTest::qWait(100);
            QVERIFY(session.root->property("categoryDirty").toBool());
            QCOMPARE(session.model.state(), before);
            QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        }
        // Explicit Enter still saves after reading Help.
        QTRY_VERIFY(field->hasActiveFocus());
        QTest::keyClick(session.window, Qt::Key_Return); wait(session.model);
        QTRY_VERIFY(!session.root->property("categoryDirty").toBool());
    }
    void contextHelpDialogAndErrorActions() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        createDocument(session, "Context dialogs");
        auto mouse = [&](const QString& name) {
            auto* item = qobject_cast<QQuickItem*>(object(session.root, name)); QVERIFY2(item, qPrintable(name));
            QTRY_VERIFY(item->isVisible() && item->isEnabled()); QTest::qWait(50);
            QTest::mouseClick(item->window(), Qt::LeftButton, Qt::NoModifier,
                item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        };
        auto* rename = object(session.root, "renameAccountDialog"); QVERIFY(rename);
        QVERIFY(QMetaObject::invokeMethod(rename, "open"));
        QVERIFY(edit(session.root, "renameAccountName", "Still unsaved"));
        auto before = session.model.state();
        mouse("renameAccountDialogHelp");
        auto* help = namedWindow("helpWindow"); QVERIFY(help);
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Rename or put an account aside"));
        QVERIFY(rename->property("visible").toBool());
        QCOMPARE(object(session.root, "renameAccountName")->property("text").toString(), QString("Still unsaved"));
        QCOMPARE(session.model.state(), before);
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        QTest::keySequence(session.window, QKeySequence(QKeySequence::HelpContents));
        QTRY_VERIFY(help->isVisible());
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Rename or put an account aside"));
        QVERIFY(rename->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        QVERIFY(QMetaObject::invokeMethod(rename, "close"));
        QVERIFY(click(session.root, "rulesNavigation")); wait(session.model);
        QVERIFY(click(session.root, "newRule"));
        QVERIFY(edit(session.root, "ruleExpression", "^Unsaved$"));
        QVERIFY(click(session.root, "accountNavigation"));
        auto* rules = qobject_cast<QQuickWindow*>(object(session.root, "rulesLeaveDialog")); QVERIFY(rules);
        QTRY_VERIFY(rules->isVisible());
        mouse("rulesGuardHelp");
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Keep and change your rules"));
        QCOMPARE(help->transientParent(), rules);
        QCOMPARE(rules->modality(), Qt::ApplicationModal);
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Unsaved$"));
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        mouse("rulesGuardStay");
        QVERIFY(click(session.root, "discardRules"));
        QVERIFY(click(session.root, "reportsNavigation")); wait(session.model);
        QVERIFY(click(session.root, "newReport"));
        auto* reports = object(session.root, "reportsWorkspace"); QVERIFY(reports);
        const auto drafts = reports->property("drafts").value<QJSValue>().toVariant();
        QVERIFY(click(session.root, "accountNavigation"));
        auto* reportDialog = qobject_cast<QQuickWindow*>(object(session.root, "reportsLeaveDialog")); QVERIFY(reportDialog);
        QTRY_VERIFY(reportDialog->isVisible());
        mouse("reportsGuardHelp");
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Keep and change your reports"));
        QCOMPARE(help->transientParent(), reportDialog);
        QCOMPARE(reportDialog->modality(), Qt::ApplicationModal);
        QCOMPARE(reports->property("drafts").value<QJSValue>().toVariant(), drafts);
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss")); mouse("reportsGuardStay");
        // An actual invalid search produces an error. Help must not rerun it.
        QVERIFY(QMetaObject::invokeMethod(reports, "load"));
        QVERIFY(click(session.root, "accountNavigation")); wait(session.model);
        run(session.model, "query", {{"filter", "\"unclosed"}}, false);
        const auto error = session.model.error(); QVERIFY(!error.isEmpty());
        before = session.model.state();
        mouse("statusHelp");
        QTRY_COMPARE(help->property("topicId").toString(), QString("messages-problems"));
        QCOMPARE(session.model.error(), error); QCOMPARE(session.model.state(), before);
        QVERIFY(!session.model.busy());
        QVERIFY(QMetaObject::invokeMethod(help, "dismiss"));
        run(session.model, "close");
        // Use the welcome field so its delayed probe and the tested path agree.
        QVERIFY(edit(session.root, "datasetPath", session.path("missing-file.spentit")));
        QTRY_VERIFY(!session.model.busy() && !session.model.error().isEmpty());
        before = session.model.state();
        const auto openError = session.model.error();
        mouse("welcomeErrorHelp");
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Opening a file"));
        QCOMPARE(session.model.state(), before); QCOMPARE(session.model.error(), openError);
    }
    void helpBesideRulesDialog() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        Session session(&catalog);
        createDocument(session, "Help beside rules");
        QVERIFY(click(session.root, "rulesNavigation"));
        wait(session.model);
        QVERIFY(click(session.root, "newRule"));
        QVERIFY(edit(session.root, "ruleExpression", "^Sample help$"));
        QVERIFY(session.root->property("rulesDirty").toBool());
        if (QGuiApplication::platformName() == "cocoa") {
            auto* expression = qobject_cast<QQuickItem*>(object(session.root, "ruleExpression"));
            QVERIFY(expression);
            expression->forceActiveFocus();
            const auto original = expression->property("text").toString();
            QTest::keyClick(session.window, Qt::Key_Z);
            QCOMPARE(expression->property("text").toString(), original + QString("z"));
            QVERIFY(expression->setProperty("text", original));
            QVERIFY(QMetaObject::invokeMethod(expression, "textEdited"));
        }
        QVERIFY(click(session.root, "accountNavigation"));
        QTRY_VERIFY(object(session.root, "rulesLeaveDialog")->property("visible").toBool());
        auto* dialog = qobject_cast<QQuickWindow*>(object(session.root, "rulesLeaveDialog"));
        QVERIFY(dialog);
        QCOMPARE(dialog->modality(), Qt::ApplicationModal);
        auto* stay = qobject_cast<QQuickItem*>(object(session.root, "rulesGuardStay"));
        QVERIFY(stay);
        stay->forceActiveFocus();
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QCOMPARE(dialog->modality(), Qt::ApplicationModal);
        QCOMPARE(help->transientParent(), dialog);
        QVERIFY(dialog->isVisible());
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Sample help$"));
        QCOMPARE(session.root->property("currentPage").toString(), QString("rules"));
        if (QGuiApplication::platformName() == "cocoa") {
            auto* expression = qobject_cast<QQuickItem*>(object(session.root, "ruleExpression"));
            QVERIFY(expression);
            expression->forceActiveFocus();
            const auto during = expression->property("text").toString();
            QTest::keyClick(session.window, Qt::Key_Z);
            QCOMPARE(expression->property("text").toString(), during);
            auto* larger = qobject_cast<QQuickItem*>(object(help, "helpTextLarger"));
            QVERIFY(larger);
            const auto scaleBefore = help->property("textScale").toDouble();
            const QPoint pos = larger->mapToScene(QPointF(larger->width() / 2.0, larger->height() / 2.0)).toPoint();
            QTest::mouseClick(help, Qt::LeftButton, Qt::NoModifier, pos);
            QTRY_VERIFY(help->property("textScale").toDouble() > scaleBefore + 0.05);
            QVERIFY(help->close());
            QTRY_VERIFY(!help->isVisible());
            QVERIFY(dialog->isVisible());
            QCOMPARE(expression->property("text").toString(), during);
            QTRY_VERIFY2(stay->hasFocus(), "Focus did not return to Stay");
            QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
            QTRY_VERIFY(help->isVisible());
            QCOMPARE(help->transientParent(), dialog);
        }
        // Escape is Stay on the dialog. Do not send it to Help while the dialog
        // is open: offscreen delivers that key to the dialog.
        QVERIFY(dialog->isVisible());
        QVERIFY(help->isVisible());
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Sample help$"));
        QVERIFY(click(session.root, "rulesGuardStay"));
        QTRY_VERIFY(!dialog->isVisible());
        QVERIFY(help->isVisible());
        QCOMPARE(help->transientParent(), session.window);
        QCOMPARE(session.root->property("currentPage").toString(), QString("rules"));
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Sample help$"));
    }
    void helpAlreadyOpenWhenRulesDialogAppears() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        Session session(&catalog);
        createDocument(session, "Help then rules dialog");
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QVERIFY(click(session.root, "rulesNavigation"));
        wait(session.model);
        QVERIFY(click(session.root, "newRule"));
        QVERIFY(edit(session.root, "ruleExpression", "^Already open$"));
        QVERIFY(click(session.root, "accountNavigation"));
        auto* dialog = qobject_cast<QQuickWindow*>(object(session.root, "rulesLeaveDialog"));
        QTRY_VERIFY(dialog && dialog->isVisible());
        QVERIFY(help->isVisible());
        QCOMPARE(dialog->modality(), Qt::ApplicationModal);
        QCOMPARE(help->transientParent(), dialog);
        QVERIFY(click(session.root, "rulesGuardStay"));
        QTRY_VERIFY(!dialog->isVisible());
        QVERIFY(help->isVisible());
        QCOMPARE(help->transientParent(), session.window);
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Already open$"));
    }
    void helpBesideReportsDialog() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        Session session(&catalog);
        createDocument(session, "Help beside reports");
        QVERIFY(click(session.root, "reportsNavigation"));
        wait(session.model);
        QVERIFY(click(session.root, "newReport"));
        QVERIFY(click(session.root, "accountNavigation"));
        auto* dialog = qobject_cast<QQuickWindow*>(object(session.root, "reportsLeaveDialog"));
        QTRY_VERIFY(dialog && dialog->isVisible());
        QCOMPARE(dialog->modality(), Qt::ApplicationModal);
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QCOMPARE(help->transientParent(), dialog);
        QVERIFY(help->isVisible());
        QVERIFY(dialog->isVisible());
        QVERIFY(click(session.root, "reportsGuardStay"));
        QTRY_VERIFY(!dialog->isVisible());
        QVERIFY(help->isVisible());
        QCOMPARE(help->transientParent(), session.window);
        QCOMPARE(session.root->property("currentPage").toString(), QString("reports"));
    }
    void quitClosesHelpBeforeTheQuestion() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        run(session.model, "create", {{"path", session.path()}, {"protected", false},
            {"name", "Quit Help"}, {"currency", "CAD"}, {"defaultCategories", false}});
        run(session.model, "correct_opening", {{"date", "2026-01-01"}, {"amount", "0.00"}});
        QFile card(session.path("card.qif"));
        QVERIFY(card.open(QIODevice::WriteOnly));
        card.write("!Type:Bank\nD2026-01-15\nT-4.00\nPMarket\n^\n");
        card.close();
        run(session.model, "import", {{"path", card.fileName()}, {"account", "1"}});
        QCOMPARE(session.model.state().value("newTransactions").toInt(), 1);
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        const auto topic = help->property("topicId").toString();
        QVERIFY(!topic.isEmpty());
        session.window->close();
        QTRY_VERIFY(!help->isVisible());
        QTRY_VERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        QVERIFY(session.model.state().value("active").toBool());
        QVERIFY(click(session.root, "quitCommitCancel"));
        QTRY_VERIFY(!object(session.root, "quitCommitDialog")->property("visible").toBool());
        QVERIFY(!help->isVisible());
        QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
        QTRY_VERIFY(help->isVisible());
        QCOMPARE(help->property("topicId").toString(), topic);
        session.window->close();
        QTRY_VERIFY(!help->isVisible());
        QTRY_VERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        QVERIFY(click(session.root, "quitCommitDialogHelp"));
        QTRY_VERIFY(help->isVisible());
        QTRY_COMPARE(help->property("topicId").toString(), QString("accept-transactions"));
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Leave them new and return later"));
        QVERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        session.window->close();
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(object(session.root, "quitCommitDialog")->property("visible").toBool());
        QVERIFY(session.model.state().value("active").toBool());
        QVERIFY(click(session.root, "quitCommitCancel"));
    }
    void standardCloseKeyClosesHelp() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help/manifest.json")));
        Session session(&catalog);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root);
        QVERIFY2(help, qPrintable(session.root->property("helpLoadError").toString()));
        QVERIFY(help->setProperty("width", 500));
        QVERIFY(help->setProperty("contentsOpen", true));
        auto* search = qobject_cast<QQuickItem*>(object(help, "helpSearch"));
        QVERIFY(search);
        search->forceActiveFocus();
        help->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(help));
        QTest::keySequence(help, QKeySequence(QKeySequence::Close));
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(!session.model.state().value("active").toBool());

        createDocument(session, "Close Help");
        QVERIFY(click(session.root, "rulesNavigation"));
        wait(session.model);
        QVERIFY(click(session.root, "newRule"));
        QVERIFY(edit(session.root, "ruleExpression", "^Close help$"));
        QVERIFY(click(session.root, "accountNavigation"));
        auto* dialog = qobject_cast<QQuickWindow*>(object(session.root, "rulesLeaveDialog"));
        QTRY_VERIFY(dialog && dialog->isVisible());
        QVERIFY(QMetaObject::invokeMethod(session.root, "openHelp"));
        QTRY_VERIFY(help->isVisible());
        QCOMPARE(help->transientParent(), dialog);
        help->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(help));
        QTest::keySequence(help, QKeySequence(QKeySequence::Close));
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(dialog->isVisible());
        QVERIFY(session.root->property("rulesDirty").toBool());
        QCOMPARE(object(session.root, "ruleExpression")->property("text").toString(), QString("^Close help$"));
    }
};

QTEST_MAIN(HelpUiTests)
#include "help_ui_tests.moc"
