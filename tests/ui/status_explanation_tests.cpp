#include "status_explanation.hpp"
#include <QFile>
#include <QRegularExpression>
#include <QTest>
using namespace spentit;

class StatusExplanationTests : public QObject {
    Q_OBJECT
private slots:
    void everyDraftSentenceHasItsExplanation();
    void placeholdersAndUnlistedMessages();
};

void StatusExplanationTests::everyDraftSentenceHasItsExplanation() {
    QFile file(QStringLiteral(SPENTIT_STATUS_EXPLANATIONS));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto text = QString::fromUtf8(file.readAll());
    const auto parts = text.split(QStringLiteral("\n## "));
    QVERIFY(parts.size() > 2);
    int count = 0;
    for (int i = 1; i < parts.size(); ++i) {
        const auto lines = parts.at(i).trimmed().split('\n');
        const auto title = lines.first().trimmed();
        QString body;
        for (int line = 1; line < lines.size(); ++line) {
            const auto piece = lines.at(line).trimmed();
            if (!piece.isEmpty()) body += (body.isEmpty() ? QString() : QStringLiteral(" ")) + piece;
        }
        QVERIFY2(!title.isEmpty() && !body.isEmpty(), qPrintable(title));
        QString sample = title;
        sample.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QStringLiteral("99"));
        QCOMPARE(statusExplanation(sample), body);
        ++count;
    }
    QCOMPARE(count, 130);
}

void StatusExplanationTests::placeholdersAndUnlistedMessages() {
    QVERIFY(statusExplanation(QString()).isEmpty());
    QVERIFY(statusExplanation(QStringLiteral("Dataset file not found or inaccessible")).isEmpty());
    QVERIFY(statusExplanation(QStringLiteral("Enter a password.")).isEmpty());
    const auto currency = statusExplanation(QStringLiteral("Statement currency USD does not match the target account; no conversion was applied."));
    QVERIFY(currency.contains(QStringLiteral("did not convert")));
    QCOMPARE(statusExplanation(QStringLiteral("Statement currency is missing or unsupported; no conversion was applied")),
        statusExplanation(QStringLiteral("Statement currency is missing or unsupported; no conversion was applied")));
    QVERIFY(!statusExplanation(QStringLiteral("Statement currency is missing or unsupported; no conversion was applied")).contains(QStringLiteral("account you chose")));
    QVERIFY(statusExplanation(QStringLiteral("Pattern compilation failed at byte 4 (code 2)")).contains(QStringLiteral("ignore the code")));
    QVERIFY(statusExplanation(QStringLiteral("Rule matching failed (code 7)")).contains(QStringLiteral("Nothing was categorized")));
    QVERIFY(statusExplanation(QStringLiteral("Rule matching failed (code )")).isEmpty());
}

QTEST_GUILESS_MAIN(StatusExplanationTests)
#include "status_explanation_tests.moc"
