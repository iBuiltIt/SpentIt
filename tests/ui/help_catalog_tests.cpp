#include "help/help_catalog.hpp"
#include <QTest>

class HelpCatalogTests : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(catalog.topics().size(), 8);
        QCOMPARE(catalog.topics().front().toMap().value("id").toString(), QStringLiteral("understand-spending"));
        QCOMPARE(catalog.topics().front().toMap().value("title").toString(), QStringLiteral("Understand your spending"));
        const auto refund = catalog.search(QStringLiteral("refund"));
        QCOMPARE(refund.size(), 6);
        QCOMPARE(refund.front().toMap().value("topicId").toString(), QStringLiteral("understand-spending"));
        const auto commit = catalog.search(QStringLiteral("commit"));
        QCOMPARE(commit.size(), 1);
        QCOMPARE(commit.front().toMap().value("topicId").toString(), QStringLiteral("understand-spending"));
        const auto setup = catalog.search(QStringLiteral("unlock"));
        QCOMPARE(setup.size(), 1);
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
        QVERIFY(catalog.search(QStringLiteral("sample content")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(HelpCatalogTests)
#include "help_catalog_tests.moc"
