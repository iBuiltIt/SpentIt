#include "help/help_catalog.hpp"
#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QStyleHints>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextFragment>
#include <QWindow>
using namespace spentit;

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
        if (!click(root, "newCategory") || !edit(root, "categoryName", name)) qFatal("Category form unavailable");
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
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        QCoreApplication::setOrganizationName("SpentItTests");
        QCoreApplication::setApplicationName("HelpUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    }
    void init() { QSettings().clear(); }
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
    void createOpenChapterReading() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(":/spentit/help-manual/manifest.json")));
        Session session(&catalog);
        QVERIFY(session.window);
        QVERIFY(QTest::qWaitForWindowExposed(session.window));
        auto* help = openHelp(session.root);
        QVERIFY(help);
        QVERIFY(QTest::qWaitForWindowExposed(help));
        QVERIFY(QMetaObject::invokeMethod(help, "nextTopic"));
        QCOMPARE(help->property("topicId").toString(), QString("create-open-file"));
        auto* article = object(help, "helpArticle");
        QVERIFY(article);
        auto* quick = qobject_cast<QQuickTextDocument*>(textDocument(article));
        QVERIFY(quick);
        int images = 0;
        QVERIFY(QMetaObject::invokeMethod(&catalog, "imageCount", Q_RETURN_ARG(int, images), Q_ARG(QObject*, quick)));
        QCOMPARE(images, 3);
        if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
            QTest::qWait(150);
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE")));
        }
        QTRY_VERIFY2(quick->textDocument()->idealWidth() <= article->property("width").toDouble() + 1,
                     qPrintable(QString("Document width %1 exceeds article %2").arg(quick->textDocument()->idealWidth()).arg(article->property("width").toDouble())));
        QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:create-open-file#open-your-file"))));
        QTRY_COMPARE(help->property("currentSection").toString(), QString("Open your file"));
        auto* flick = object(help, "helpArticleFlick");
        for (const auto& name : {"create-file-setup.png", "create-file-workspace.png", "create-file-unlock.png"}) {
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
        help->resize(500, 680);
        help->setProperty("textScale", 1.5);
        QTRY_VERIFY(quick->textDocument()->idealWidth() <= article->property("width").toDouble() + 1);
        if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
            QTest::qWait(150);
            QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".narrow.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(help, "previousTopic"));
        QCOMPARE(help->property("topicId").toString(), QString("understand-spending"));
        QVERIFY(!session.model.state().value("active").toBool());
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
};

QTEST_MAIN(HelpUiTests)
#include "help_ui_tests.moc"
