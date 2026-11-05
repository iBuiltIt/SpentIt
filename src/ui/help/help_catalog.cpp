#include "help/help_catalog.hpp"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <algorithm>

HelpCatalog::HelpCatalog(QObject* parent) : QObject(parent) {}

bool HelpCatalog::load(const QString& manifestPath) {
    loaded_ = false;
    sample_ = false;
    loadError_.clear();
    banner_.clear();
    topics_.clear();
    records_.clear();
    topicIndex_.clear();
    images_.clear();
    forgetMarks();
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        loadError_ = QStringLiteral("Help could not be opened.");
        emit catalogChanged();
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        loadError_ = QStringLiteral("Help could not be opened.");
        emit catalogChanged();
        return false;
    }
    const auto root = document.object();
    sample_ = root.value(QStringLiteral("sample")).toBool();
    banner_ = root.value(QStringLiteral("banner")).toString();
    const auto imageValues = root.value(QStringLiteral("images")).toArray();
    for (const auto& value : imageValues) {
        const auto image = value.toObject();
        const auto id = image.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1String("..")) || images_.contains(id)) {
            loadError_ = QStringLiteral("Help could not be opened.");
            emit catalogChanged();
            return false;
        }
        images_.insert(id, Image{image.value(QStringLiteral("alt")).toString(), QStringLiteral("qrc:/spentit/help/images/%1").arg(id)});
    }
    const auto topicValues = root.value(QStringLiteral("topics")).toArray();
    for (const auto& value : topicValues) {
        const auto object = value.toObject();
        Topic topic;
        topic.id = object.value(QStringLiteral("id")).toString();
        topic.title = object.value(QStringLiteral("title")).toString();
        topic.order = object.value(QStringLiteral("order")).toInt();
        topic.markdown = object.value(QStringLiteral("markdown")).toString();
        topic.plain = object.value(QStringLiteral("plain")).toString();
        for (const auto& keyword : object.value(QStringLiteral("keywords")).toArray())
            topic.keywords.push_back(keyword.toString());
        for (const auto& sectionValue : object.value(QStringLiteral("sections")).toArray()) {
            const auto section = sectionValue.toObject();
            topic.sections.push_back(Section{section.value(QStringLiteral("id")).toString(), section.value(QStringLiteral("title")).toString()});
        }
        if (topic.id.isEmpty() || topicIndex_.contains(topic.id)) {
            loadError_ = QStringLiteral("Help could not be opened.");
            emit catalogChanged();
            return false;
        }
        topicIndex_.insert(topic.id, static_cast<int>(records_.size()));
        records_.push_back(topic);
        QVariantMap summary;
        summary.insert(QStringLiteral("id"), topic.id);
        summary.insert(QStringLiteral("title"), topic.title);
        summary.insert(QStringLiteral("order"), topic.order);
        topics_.push_back(summary);
    }
    if (records_.isEmpty()) {
        loadError_ = QStringLiteral("Help could not be opened.");
        emit catalogChanged();
        return false;
    }
    loaded_ = true;
    emit catalogChanged();
    return true;
}

QVariantMap HelpCatalog::topic(const QString& id) const {
    const auto found = topicIndex_.constFind(id);
    if (found == topicIndex_.cend()) return {};
    const Topic& topic = records_.at(found.value());
    QVariantMap map;
    map.insert(QStringLiteral("id"), topic.id);
    map.insert(QStringLiteral("title"), topic.title);
    map.insert(QStringLiteral("order"), topic.order);
    map.insert(QStringLiteral("markdown"), topic.markdown);
    QVariantList sections;
    for (const Section& section : topic.sections) {
        QVariantMap sectionMap;
        sectionMap.insert(QStringLiteral("id"), section.id);
        sectionMap.insert(QStringLiteral("title"), section.title);
        sections.push_back(sectionMap);
    }
    map.insert(QStringLiteral("sections"), sections);
    return map;
}

QVariantMap HelpCatalog::rejected(const QString& message) const {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("kind"), QStringLiteral("rejected"));
    result.insert(QStringLiteral("message"), message);
    return result;
}

bool HelpCatalog::sectionExists(const QString& topicId, const QString& sectionId) const {
    const auto found = topicIndex_.constFind(topicId);
    if (found == topicIndex_.cend()) return false;
    const auto& sections = records_.at(found.value()).sections;
    return std::any_of(sections.cbegin(), sections.cend(), [&](const Section& section) { return section.id == sectionId; });
}

QString HelpCatalog::sectionTitle(const QString& topicId, const QString& sectionId) const {
    const auto found = topicIndex_.constFind(topicId);
    if (found == topicIndex_.cend()) return {};
    for (const Section& section : records_.at(found.value()).sections)
        if (section.id == sectionId) return section.title;
    return {};
}

QVariantMap HelpCatalog::resolveLink(const QString& fromTopic, const QString& url) const {
    const QString trimmed = url.trimmed();
    const QString lowered = trimmed.toLower();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1String("..")) || trimmed.startsWith(QLatin1Char('/')) || trimmed.startsWith(QLatin1Char('\\'))
        || trimmed.contains(QLatin1String("://")) || lowered.startsWith(QLatin1String("file:")) || lowered.startsWith(QLatin1String("http:"))
        || lowered.startsWith(QLatin1String("https:")) || lowered.startsWith(QLatin1String("javascript:")) || lowered.startsWith(QLatin1String("data:"))
        || lowered.startsWith(QLatin1String("qrc:")))
        return rejected(QStringLiteral("That link is not part of Help."));
    QString target = trimmed;
    QString fragment;
    const int hash = static_cast<int>(target.indexOf(QLatin1Char('#')));
    if (hash >= 0) {
        fragment = target.mid(hash + 1);
        target = target.left(hash);
        if (fragment.isEmpty() || fragment.contains(QLatin1Char('#')))
            return rejected(QStringLiteral("That link is not part of Help."));
    }
    const QString imagePrefix = QStringLiteral("help-image:");
    const QString topicPrefix = QStringLiteral("help:");
    if (target.startsWith(imagePrefix)) {
        const QString imageId = target.mid(imagePrefix.size());
        const auto image = images_.constFind(imageId);
        if (!fragment.isEmpty() || image == images_.cend())
            return rejected(QStringLiteral("That picture is not part of Help."));
        QVariantMap result;
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("kind"), QStringLiteral("image"));
        result.insert(QStringLiteral("imageId"), imageId);
        result.insert(QStringLiteral("source"), image.value().source);
        result.insert(QStringLiteral("alt"), image.value().alt);
        result.insert(QStringLiteral("message"), QString());
        return result;
    }
    QString topicId;
    if (target.startsWith(topicPrefix)) topicId = target.mid(topicPrefix.size());
    else if (target.isEmpty()) topicId = fromTopic;
    else return rejected(QStringLiteral("That link is not part of Help."));
    if (!topicIndex_.contains(topicId) || (!fragment.isEmpty() && !sectionExists(topicId, fragment)))
        return rejected(QStringLiteral("That link is not part of Help."));
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("kind"), fragment.isEmpty() ? QStringLiteral("topic") : QStringLiteral("section"));
    result.insert(QStringLiteral("topicId"), topicId);
    result.insert(QStringLiteral("sectionId"), fragment);
    result.insert(QStringLiteral("title"), fragment.isEmpty() ? records_.at(topicIndex_.value(topicId)).title : sectionTitle(topicId, fragment));
    result.insert(QStringLiteral("message"), QString());
    return result;
}

QVariantList HelpCatalog::search(const QString& query) const {
    const QStringList terms = query.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (terms.isEmpty()) return {};
    struct Hit { int score = 0; int order = 0; QVariantMap value; };
    QList<Hit> hits;
    for (const Topic& topic : records_) {
        const auto containsAll = [&](const QString& text) {
            return std::all_of(terms.cbegin(), terms.cend(), [&](const QString& term) { return text.contains(term); });
        };
        const QString title = topic.title.toLower();
        const QString keywords = topic.keywords.join(QLatin1Char(' ')).toLower();
        const QString plain = topic.plain.toLower();
        int score = 0;
        if (containsAll(title)) score = 300;
        else if (containsAll(keywords)) score = 200;
        else if (containsAll(plain)) score = 100;
        else continue;
        const int at = static_cast<int>(plain.indexOf(terms.front()));
        if (at > 0) score -= std::min(at, 50);
        QString before;
        QString match;
        QString after;
        if (at >= 0) {
            const int length = static_cast<int>(terms.front().size());
            const int start = std::max(0, at - 40);
            const int end = std::min(static_cast<int>(topic.plain.size()), at + length + 40);
            before = topic.plain.mid(start, at - start);
            match = topic.plain.mid(at, length);
            after = topic.plain.mid(at + length, end - (at + length));
        } else {
            before = topic.title;
        }
        QVariantMap value;
        value.insert(QStringLiteral("topicId"), topic.id);
        value.insert(QStringLiteral("title"), topic.title);
        value.insert(QStringLiteral("before"), before);
        value.insert(QStringLiteral("match"), match);
        value.insert(QStringLiteral("after"), after);
        value.insert(QStringLiteral("score"), score);
        hits.push_back(Hit{score, topic.order, value});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& left, const Hit& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.order < right.order;
    });
    QVariantList results;
    for (const Hit& hit : hits) results.push_back(hit.value);
    return results;
}

void HelpCatalog::forgetMarks() {
    marks_.clear();
    markedDocument_ = nullptr;
}

void HelpCatalog::clearStoredMarks(QTextDocument* document) {
    if (markedDocument_ != document) {
        marks_.clear();
        markedDocument_ = document;
        return;
    }
    for (const auto& mark : marks_) {
        if (mark.first < 0 || mark.second <= mark.first || mark.second > document->characterCount()) continue;
        QTextCursor cursor(document);
        cursor.setPosition(mark.first);
        cursor.setPosition(mark.second, QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setBackground(Qt::transparent);
        cursor.mergeCharFormat(format);
    }
    marks_.clear();
}

QTextDocument* HelpCatalog::textDocument(QObject* document) const {
    auto* quick = qobject_cast<QQuickTextDocument*>(document);
    return quick ? quick->textDocument() : nullptr;
}

QVariantMap HelpCatalog::markMatch(QObject* documentObject, const QString& term, int occurrence, const QColor& color) {
    QVariantMap result;
    result.insert(QStringLiteral("position"), -1);
    result.insert(QStringLiteral("count"), 0);
    result.insert(QStringLiteral("occurrence"), -1);
    QTextDocument* document = textDocument(documentObject);
    if (!document) return result;
    clearStoredMarks(document);
    if (term.isEmpty()) return result;
    int count = 0;
    QTextCursor scan(document);
    while (true) {
        scan = document->find(term, scan);
        if (scan.isNull()) break;
        ++count;
    }
    result.insert(QStringLiteral("count"), count);
    if (count == 0) return result;
    int index = occurrence % count;
    if (index < 0) index += count;
    QTextCursor cursor(document);
    for (int seen = 0; seen <= index; ++seen) {
        cursor = document->find(term, cursor);
        if (cursor.isNull()) return result;
    }
    QTextCharFormat format;
    format.setBackground(color);
    cursor.mergeCharFormat(format);
    marks_.push_back({static_cast<int>(cursor.selectionStart()), static_cast<int>(cursor.selectionEnd())});
    markedDocument_ = document;
    result.insert(QStringLiteral("position"), static_cast<int>(cursor.selectionStart()));
    result.insert(QStringLiteral("occurrence"), index);
    return result;
}

int HelpCatalog::sectionPosition(QObject* documentObject, const QString& title) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document || title.isEmpty()) return -1;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        if (block.text().trimmed() == title) return static_cast<int>(block.position());
    return -1;
}

int HelpCatalog::tableCount(QObject* documentObject) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document) return 0;
    int tables = 0;
    QList<QTextFrame*> frames{document->rootFrame()};
    while (!frames.isEmpty()) {
        QTextFrame* frame = frames.takeLast();
        if (qobject_cast<QTextTable*>(frame)) ++tables;
        for (auto child = frame->begin(); !child.atEnd(); ++child)
            if (auto* nested = child.currentFrame()) frames.push_back(nested);
    }
    return tables;
}

int HelpCatalog::listCount(QObject* documentObject) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document) return 0;
    QSet<QTextList*> lists;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        if (auto* list = block.textList()) lists.insert(list);
    return static_cast<int>(lists.size());
}

qreal HelpCatalog::readingGap(QObject* documentObject, const QString& phrase) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document || phrase.isEmpty()) return 0;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        if (block.text().contains(phrase)) return block.blockFormat().bottomMargin();
    return 0;
}

qreal HelpCatalog::headingGap(QObject* documentObject, const QString& title) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document || title.isEmpty()) return 0;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        if (block.text().trimmed() == title) return block.blockFormat().topMargin();
    return 0;
}

void HelpCatalog::relaxReading(QObject* documentObject) {
    // Qt Markdown keeps every block margin at zero, so paragraphs and headings
    // sit on top of each other. Give the article room without changing its words.
    if (relaxing_) return;
    QTextDocument* document = textDocument(documentObject);
    if (!document) return;
    relaxing_ = true;
    qreal size = document->defaultFont().pixelSize();
    if (size <= 0) size = 16;
    QList<QTextBlock> blocks;
    for (auto block = document->begin(); block != document->end(); block = block.next()) blocks.push_back(block);
    for (const QTextBlock& block : blocks) {
        QTextCursor cursor(document);
        cursor.setPosition(block.position());
        QTextBlockFormat format = block.blockFormat();
        const bool empty = block.text().trimmed().isEmpty() && !block.textList();
        const int heading = format.headingLevel();
        if (empty) {
            format.setTopMargin(0);
            format.setBottomMargin(0);
        } else if (heading == 1) {
            format.setTopMargin(size * 0.85);
            format.setBottomMargin(size * 0.45);
        } else if (heading > 1) {
            format.setTopMargin(size * 1.2);
            format.setBottomMargin(size * 0.4);
        } else if (block.textList()) {
            format.setTopMargin(size * 0.2);
            format.setBottomMargin(size * 0.35);
        } else {
            format.setTopMargin(0);
            format.setBottomMargin(size * 0.9);
        }
        if (!empty) format.setLineHeight(138, QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(format);
    }
    relaxing_ = false;
}

int HelpCatalog::imageCount(QObject* documentObject) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document) return 0;
    int images = 0;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        for (auto it = block.begin(); !it.atEnd(); ++it)
            if (it.fragment().charFormat().isImageFormat()) ++images;
    return images;
}

bool HelpCatalog::containsHref(QObject* documentObject, const QString& href) const {
    QTextDocument* document = textDocument(documentObject);
    if (!document) return false;
    for (auto block = document->begin(); block != document->end(); block = block.next())
        for (auto it = block.begin(); !it.atEnd(); ++it)
            if (it.fragment().charFormat().anchorHref() == href) return true;
    return false;
}
