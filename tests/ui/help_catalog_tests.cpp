#include "help/help_catalog.hpp"
#include <QTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

class HelpCatalogTests : public QObject {
    Q_OBJECT
private slots:
    void excerptsKeepWholeTokens() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = QStringLiteral("Earlier omitted words ");
        const QString left = QString(65, QLatin1Char('a')) + QString::fromUtf8("é");
        const QString right = QString::fromUtf8("e\u0301") + QString(65, QLatin1Char('z'));
        const QString body = prefix + left + " needle " + right + " Later omitted words";
        QJsonObject topic{{"id", "test"}, {"title", "Title"}, {"plain", body}, {"order", 1},
                          {"keywords", QJsonArray{"alias"}}};
        QFile file(directory.filePath("manifest.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"topics", QJsonArray{topic}}}).toJson());
        file.close();
        HelpCatalog catalog;
        QVERIFY(catalog.load(file.fileName()));
        auto hit = catalog.search("NEEDLE").front().toMap();
        QCOMPARE(hit.value("before").toString(), left + " ");
        QCOMPARE(hit.value("match").toString(), QString("needle"));
        QCOMPARE(hit.value("after").toString(), " " + right);
        hit = catalog.search("Earlier").front().toMap();
        QVERIFY(hit.value("before").toString().isEmpty());
        QVERIFY(hit.value("after").toString().endsWith(left));
        hit = catalog.search("words").front().toMap();
        QCOMPARE(hit.value("match").toString(), QString("words"));
        hit = catalog.search("Later omitted").front().toMap();
        QCOMPARE(hit.value("after").toString(), QString(" omitted words"));
        hit = catalog.search("alias").front().toMap();
        QCOMPARE(hit.value("before").toString(), QString("Title"));
        QVERIFY(hit.value("match").toString().isEmpty());
    }
    void searchPrefersTitlesAndRejectsOutsideLinks() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(SPENTIT_HELP_MANIFEST)));
        QVERIFY(catalog.loaded());
        QVERIFY(catalog.sample());
        QCOMPARE(catalog.topics().size(), 2);
        const auto lavender = catalog.search(QStringLiteral("lavender marker"));
        QCOMPARE(lavender.size(), 2);
        QVERIFY(lavender.front().toMap().value("match").toString().size() > 0);
        const auto phrase = catalog.search(QStringLiteral("second row"));
        QCOMPARE(phrase.size(), 1);
        QCOMPARE(phrase.front().toMap().value("topicId").toString(), QStringLiteral("sample-details"));
        const auto titled = catalog.search(QStringLiteral("sample start"));
        QVERIFY(titled.size() >= 1);
        QCOMPARE(titled.front().toMap().value("topicId").toString(), QStringLiteral("sample-start"));
        QVERIFY(catalog.search(QStringLiteral("zzzz")).isEmpty());
        QVERIFY(catalog.search(QString()).isEmpty());
        const auto section = catalog.resolveLink(QStringLiteral("sample-start"), QStringLiteral("help:sample-details#wide-table"));
        QVERIFY(section.value("ok").toBool());
        QCOMPARE(section.value("kind").toString(), QStringLiteral("section"));
        QCOMPARE(section.value("title").toString(), QStringLiteral("Wide table"));
        const auto image = catalog.resolveLink(QStringLiteral("sample-start"), QStringLiteral("help-image:sample-mark.png"));
        QVERIFY(image.value("ok").toBool());
        QCOMPARE(image.value("source").toString(), QStringLiteral("qrc:/spentit/help/images/sample-mark.png"));
        for (const char* rejected : {"https://example.com", "file:///tmp/x", "/tmp/x", "../secret.png", "javascript:alert(1)", "help:missing#wide-table", "help-image:missing.png"}) {
            const auto result = catalog.resolveLink(QStringLiteral("sample-start"), QString::fromLatin1(rejected));
            QVERIFY2(!result.value("ok").toBool(), rejected);
            QVERIFY(!result.value("message").toString().isEmpty());
        }
    }
    void introductionIsTheShippedTopic() {
        HelpCatalog catalog;
        QVERIFY(catalog.load(QStringLiteral(SPENTIT_HELP_MANUAL_MANIFEST)));
        QVERIFY(catalog.loaded());
        QVERIFY(!catalog.sample());
        QVERIFY(catalog.banner().isEmpty());
        QCOMPARE(catalog.topics().size(), 17);
        QCOMPARE(catalog.topics().front().toMap().value("id").toString(), QStringLiteral("understand-spending"));
        QCOMPARE(catalog.topics().front().toMap().value("title").toString(), QStringLiteral("Understand your spending"));
        const auto refund = catalog.search(QStringLiteral("refund"));
        QCOMPARE(refund.size(), 10);
        QCOMPARE(refund.front().toMap().value("topicId").toString(), QStringLiteral("understand-spending"));
        const auto commit = catalog.search(QStringLiteral("commit"));
        QCOMPARE(commit.size(), 2);
        QCOMPARE(commit.front().toMap().value("topicId").toString(), QStringLiteral("understand-spending"));
        const auto setup = catalog.search(QStringLiteral("unlock"));
        QCOMPARE(setup.size(), 2);
        QCOMPARE(setup.front().toMap().value("topicId").toString(), QStringLiteral("create-open-file"));
        const auto accounts = catalog.search(QStringLiteral("reassign"));
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(accounts.front().toMap().value("topicId").toString(), QStringLiteral("set-up-accounts"));
        const auto importing = catalog.search(QStringLiteral("QFX"));
        QCOMPARE(importing.size(), 1);
        QCOMPARE(importing.front().toMap().value("topicId").toString(), QStringLiteral("bring-in-transactions"));
        const auto ledger = catalog.search(QStringLiteral("ledger"));
        QVERIFY(!ledger.isEmpty());
        QCOMPARE(ledger.front().toMap().value("topicId").toString(), QStringLiteral("find-transactions"));
        const auto linking = catalog.search(QStringLiteral("unlink"));
        QCOMPARE(linking.size(), 1);
        QCOMPARE(linking.front().toMap().value("topicId").toString(), QStringLiteral("review-duplicates-transfers"));
        const auto arithmetic = catalog.search(QStringLiteral("arithmetic"));
        QCOMPARE(arithmetic.front().toMap().value("topicId").toString(), QStringLiteral("add-correct-transactions"));
        const auto unarchive = catalog.search(QStringLiteral("unarchive"));
        QVERIFY(!unarchive.isEmpty());
        QCOMPARE(unarchive.front().toMap().value("topicId").toString(), QStringLiteral("organize-categories"));
        const auto patterns = catalog.search(QStringLiteral("regular expression"));
        QVERIFY(!patterns.isEmpty());
        QCOMPARE(patterns.front().toMap().value("topicId").toString(), QStringLiteral("use-rules"));
        const auto acceptance = catalog.search(QStringLiteral("acceptance"));
        QVERIFY(!acceptance.isEmpty());
        QCOMPARE(acceptance.front().toMap().value("topicId").toString(), QStringLiteral("accept-transactions"));
        const auto reports = catalog.search(QStringLiteral("grouped periods"));
        QVERIFY(!reports.isEmpty());
        QCOMPARE(reports.front().toMap().value("topicId").toString(), QStringLiteral("answer-with-reports"));
        const auto recovery = catalog.search(QStringLiteral("disk full"));
        QVERIFY(!recovery.isEmpty());
        QCOMPARE(recovery.front().toMap().value("topicId").toString(), QStringLiteral("look-after-your-work"));
        const auto problem = catalog.search(QStringLiteral("Unclosed search quote"));
        QCOMPARE(problem.size(), 1);
        QCOMPARE(problem.front().toMap().value("topicId").toString(), QStringLiteral("messages-problems"));
        QVERIFY(catalog.search(QStringLiteral("sample content")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(HelpCatalogTests)
#include "help_catalog_tests.moc"
