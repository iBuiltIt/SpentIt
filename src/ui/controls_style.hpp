#pragma once

#include <QQuickStyle>
#include <QString>

// Basic is the fallback for every control this style does not replace.
// Call this before loading any QML that imports Qt Quick Controls.
inline void applySpentItControlsStyle() {
    QQuickStyle::setStyle(QStringLiteral("SpentItControls"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));
}
