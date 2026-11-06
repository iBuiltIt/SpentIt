#pragma once

#include <QColor>
#include <QObject>
#include <QPair>
#include <QVariantList>
#include <QVariantMap>

class QTextDocument;

// Offline Help manifest, search, and the link allowlist. Reading Help does not
// open a financial file or run a financial command.
class HelpCatalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY catalogChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY catalogChanged)
    Q_PROPERTY(bool sample READ sample NOTIFY catalogChanged)
    Q_PROPERTY(QString banner READ banner NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList topics READ topics NOTIFY catalogChanged)
public:
    explicit HelpCatalog(QObject* parent = nullptr);
    bool load(const QString& manifestPath);
    bool loaded() const { return loaded_; }
    QString loadError() const { return loadError_; }
    bool sample() const { return sample_; }
    QString banner() const { return banner_; }
    QVariantList topics() const { return topics_; }

    Q_INVOKABLE QVariantMap topic(const QString& id) const;
    Q_INVOKABLE QVariantMap resolveLink(const QString& fromTopic, const QString& url) const;
    Q_INVOKABLE QVariantList search(const QString& query) const;
    Q_INVOKABLE void forgetMarks();
    Q_INVOKABLE QVariantMap markMatch(QObject* document, const QString& term, int occurrence, const QColor& color);
    Q_INVOKABLE int sectionPosition(QObject* document, const QString& title) const;
    Q_INVOKABLE int tableCount(QObject* document) const;
    Q_INVOKABLE int imageCount(QObject* document) const;
    Q_INVOKABLE int listCount(QObject* document) const;
    Q_INVOKABLE qreal readingGap(QObject* document, const QString& phrase) const;
    Q_INVOKABLE qreal headingGap(QObject* document, const QString& title) const;
    Q_INVOKABLE void relaxReading(QObject* document, qreal availableWidth);
    Q_INVOKABLE void prepareArticleRendering(QObject* article, QObject* document);
    Q_INVOKABLE bool containsHref(QObject* document, const QString& href) const;
signals:
    void catalogChanged();
private:
    struct Section {
        QString id;
        QString title;
    };
    struct Topic {
        QString id;
        QString title;
        int order = 0;
        QStringList keywords;
        QString markdown;
        QString plain;
        QList<Section> sections;
    };
    struct Image {
        QString alt;
        QString source;
    };
    QTextDocument* textDocument(QObject* document) const;
    bool sectionExists(const QString& topicId, const QString& sectionId) const;
    QString sectionTitle(const QString& topicId, const QString& sectionId) const;
    QVariantMap rejected(const QString& message) const;
    void clearStoredMarks(QTextDocument* document);

    bool loaded_ = false;
    bool sample_ = false;
    QString loadError_;
    QString banner_;
    QVariantList topics_;
    QList<Topic> records_;
    QHash<QString, int> topicIndex_;
    QHash<QString, Image> images_;
    QTextDocument* markedDocument_ = nullptr;
    QList<QPair<int, int>> marks_;
    bool relaxing_ = false;
};
