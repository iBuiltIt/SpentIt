#pragma once
#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>
#include <QTest>
#include <QVariantMap>

// UI cases that exercise manual Review pin an unsaved choice to off. The
// product default is on. ObserveAutomaticReviewDefault leaves the unsaved
// choice alone so a case can see that default.
inline bool& observeAutomaticReviewDefault() {
    static bool value=false;
    return value;
}
struct ObserveAutomaticReviewDefault {
    ObserveAutomaticReviewDefault() { observeAutomaticReviewDefault()=true; }
    ~ObserveAutomaticReviewDefault() { observeAutomaticReviewDefault()=false; }
};

template<typename Model>
QString pinUnsavedAutomaticReviewOff(Model& model) {
    if(observeAutomaticReviewDefault()) return {};
    const auto path=model.state().value("path").toString();
    if(path.isEmpty() || !model.state().value("active").toBool()) return {};
    const QFileInfo file(path);
    const auto canonical=file.canonicalFilePath();
    const auto location=canonical.isEmpty()?file.absoluteFilePath():canonical;
    const auto group=QStringLiteral("LedgerSort/")+QString::fromLatin1(QCryptographicHash::hash(location.toUtf8(),QCryptographicHash::Sha256).toHex());
    QSettings settings;
    const auto pin=[&](const char* kind,const char* leaf)->QString {
        if(settings.contains(group+"/"+leaf)) return {};
        model.command(QStringLiteral("automatic_review_settings"),QVariantMap{{QStringLiteral("kind"),QString::fromLatin1(kind)},{QStringLiteral("enabled"),false}});
        if(!QTest::qWaitFor([&]{return !model.busy();},30000)) return QStringLiteral("Saving the automatic matching setting timed out.");
        settings.sync();
        return model.error();
    };
    if(const auto error=pin("merge","AutoMerge");!error.isEmpty()) return error;
    return pin("link","AutoLink");
}
