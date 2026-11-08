#include "about_info.hpp"
#include "about_build.hpp"
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>

AboutInfo::AboutInfo(QObject* parent) : QObject(parent) {
    Q_INIT_RESOURCE(about_notices);
}
QString AboutInfo::version() const { return QStringLiteral(SPENTIT_VERSION); }
QString AboutInfo::build() const { return QStringLiteral(SPENTIT_BUILD); }
QString AboutInfo::versionInformation() const {
    return QStringLiteral("SpentIt %1\nBuild %2\nQt %3\n%4 (%5)")
        .arg(version(), build(), QString::fromLatin1(qVersion()),
             QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
}
namespace {
QByteArray resource(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}
QString AboutInfo::terms() const {
    return QString::fromUtf8(resource(QStringLiteral(":/spentit/about/terms.txt")));
}
QVariantList AboutInfo::components() const {
    return QJsonDocument::fromJson(resource(QStringLiteral(":/spentit/about/components.json"))).array().toVariantList();
}
void AboutInfo::copyVersionInformation() const {
    QGuiApplication::clipboard()->setText(versionInformation());
}
