#include "status_explanation.hpp"

namespace spentit {
namespace {

#include "status_explanation_data.inc"

bool matches(const QString& message, const PatternExplanation& pattern) {
    const QString prefix = QString::fromUtf8(pattern.prefix);
    const QString middle = QString::fromUtf8(pattern.middle);
    const QString suffix = QString::fromUtf8(pattern.suffix);
    if (!message.startsWith(prefix) || !message.endsWith(suffix)) return false;
    if (message.size() < prefix.size() + middle.size() + suffix.size()) return false;
    if (middle.isEmpty()) return message.size() > prefix.size() + suffix.size();
    const auto start = prefix.size();
    const auto end = message.size() - suffix.size();
    return message.indexOf(middle, start) >= start && message.indexOf(middle, start) < end;
}

}

QString statusExplanation(const QString& message) {
    const auto text = message.trimmed();
    if (text.isEmpty()) return {};
    for (const auto& entry : exactExplanations) {
        if (text == QString::fromUtf8(entry.message)) return QString::fromUtf8(entry.explanation);
    }
    for (const auto& entry : patternExplanations) {
        if (matches(text, entry)) return QString::fromUtf8(entry.explanation);
    }
    return {};
}

}