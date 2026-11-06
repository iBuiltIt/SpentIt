#include "help/help_catalog.hpp"
#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QColor>
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
#include <QQuickStyle>
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
            << QStringList{"editing-new.png", "editing-calendar.png", "editing-splits.png", "editing-mismatch.png"}
            << QString("divide-a-purchase-into-splits") << QString("Divide a purchase into splits");
        QTest::newRow("categories") << QString("organize-categories")
            << QStringList{"categories-workspace.png", "categories-completion.png", "categories-menu.png", "categories-split.png"}
            << QString("change-selected-transactions") << QString("Change selected transactions");
        QTest::newRow("rules") << QString("use-rules")
            << QStringList{"rules-editor.png", "rules-preview.png", "rules-leave.png"}
            << QString("preview-save-and-apply") << QString("Preview, save and apply");
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
                                "bring-in-transactions", "find-transactions", "review-duplicates-transfers", "add-correct-transactions", "organize-categories", "use-rules"};
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
        if (topic == "use-rules") {
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:use-rules#common-pattern-pieces"))));
            QTRY_COMPARE(help->property("currentSection").toString(), QString("Common pattern pieces"));
            QVERIFY(quick->textDocument()->toPlainText().contains("${1} from eBay"));
            if (qEnvironmentVariableIsSet("SPENTIT_GUIDE_READING_CAPTURE")) {
                QTest::qWait(150);
                QVERIFY(help->grabWindow().save(qEnvironmentVariable("SPENTIT_GUIDE_READING_CAPTURE") + ".regex.png"));
            }
        }
        help->resize(500, 680);
        help->setProperty("textScale", 1.5);
        if (topic == "use-rules") {
            QTest::qWait(150);
            QVERIFY(QMetaObject::invokeMethod(help, "followLink", Q_ARG(QVariant, QVariant("help:use-rules#common-pattern-pieces"))));
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
            choose("use-rules");
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
            choose("use-rules");
            QTRY_COMPARE(flick->property("contentY").toDouble(), savedY);
            comparePixels(snapshot(), middle);
            if (QTest::currentTestFailed()) return;
        }
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
        mouse("undoAccountCommand");
        wait(session.model);
        QCOMPARE(session.model.state().value("currency").toString(), QString("CAD"));
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
        auto* purchase = qobject_cast<QQuickItem*>(object(session.root, "ledgerRow_2"));
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
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        mouse("ledgerRow_1");
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 1);
        QVERIFY(session.root->property("editorClosed").toBool());
        mouse("ledgerRow_2", Qt::ShiftModifier);
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 2);
        mouse("ledgerRow_2", Qt::ControlModifier);
        QCOMPARE(session.model.state().value("selectionCount").toInt(), 1);
        mouse("ledgerRow_1", Qt::NoModifier, true);
        QTRY_VERIFY(object(session.root, "showEntryHistory")->property("visible").toBool());
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
            QVERIFY2(item, qPrintable(name)); QVERIFY(item->isVisible()); QVERIFY(item->isEnabled());
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
        mainClick("applyRules");
        QVERIFY(!session.root->property("rulesDirty").toBool());
        QVERIFY(session.model.state().value("canUndoRules").toBool());
        QCOMPARE(session.model.state().value("balance").toString(), QString("-56.18"));
        mainClick("undoRules");
        QVERIFY(!session.model.state().value("canUndoRules").toBool());
        mainClick("previewThisRule");
        QCOMPARE(session.model.state().value("rulePreview").toMap().value("records").toList().size(), 1);
        mainClick("applyRules");
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
            QVERIFY2(item, qPrintable(name)); QVERIFY(item->isVisible()); QVERIFY(item->isEnabled());
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
        mouse("categoryNavigation");
        createCategory(session.model,session.root,"Groceries");
        createCategory(session.model,session.root,"Household");
        createCategory(session.model,session.root,"Food: Meals out");
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
        mouse("ledgerRow_1",Qt::RightButton);
        QTRY_VERIFY(object(session.root,"ledgerContext")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"ledgerCategoryMenu"),"open"));
        QTRY_VERIFY(object(session.root,"ledgerCategoryMenu")->property("visible").toBool());
        capture("categories-menu");
        mouse("ledgerCategoryChoice_0");
        QVERIFY(session.model.state().value("canUndoTransferAssignment").toBool());
        mouse("undoTransferAssignment");
        mouse("ledgerRow_3",Qt::RightButton);
        QTRY_VERIFY(object(session.root,"ledgerContext")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(object(session.root,"ledgerCategoryMenu"),"open"));
        mouse("ledgerCategoryChoice_0");
        QTRY_VERIFY(object(session.root,"bulkCategoryDialog")->property("visible").toBool());
        capture("categories-split");mouse("ledgerCategorySplit_1");
        mouse("undoTransferAssignment");
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
            QVERIFY2(item, qPrintable(name)); QVERIFY(item->isVisible()); QVERIFY(item->isEnabled());
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
        QTRY_VERIFY(object(session.root, "calendarPopup")->property("visible").toBool());
        capture("editing-calendar");
        auto* keys = qobject_cast<QQuickItem*>(object(session.root, "calendarKeys")); QVERIFY(keys);
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
            QVERIFY2(item, qPrintable(name)); QVERIFY(item->isVisible()); QVERIFY(item->isEnabled());
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
        int sequence = 0;
        auto import = [&](const QByteArray& bytes, const QString& account) {
            QFile file(session.path("review-" + QString::number(++sequence) + ".qif"));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
            run(session.model, "import", {{"path", file.fileName()}, {"account", account}});
        };
        if (!difference) {
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket\n^\nD2026-01-20\nT6.00\nPMarket refund\n^\n", "1");
            import("!Type:CCard\nD2026-01-15\nT-42.18\nPMarket groceries\n^\n", "1");
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            mouse("reviewNavigation");
            QTRY_VERIFY(object(session.root, "reviewYes")); mouse("reviewYes");
            QTRY_VERIFY(object(session.root, "reviewSurvivor")->property("visible").toBool());
            capture("review-keep");
            mouse("reviewKeep_0");
            QCOMPARE(session.model.state().value("balance").toString(), QString("-36.18"));
            mouse("reviewUndo");
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            QTRY_VERIFY(object(session.root, "reviewNo")); mouse("reviewNo");
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 0);
            QCOMPARE(session.model.state().value("balance").toString(), QString("-78.36"));
            mouse("reviewUndo");
            QTRY_VERIFY(object(session.root, "reviewYes")); mouse("reviewYes");
            QTRY_VERIFY(object(session.root, "reviewSurvivor")->property("visible").toBool()); mouse("reviewKeep_0");
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
            mouse("reviewUndo");
            QCOMPARE(session.model.state().value("reviewCount").toInt(), 1);
        }
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
