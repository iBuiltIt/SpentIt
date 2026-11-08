#pragma once
#include <QObject>
#include <QVariantList>

// Read-only application metadata; no document, storage or financial commands.
class AboutInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString build READ build CONSTANT)
    Q_PROPERTY(QString versionInformation READ versionInformation CONSTANT)
    Q_PROPERTY(QString terms READ terms CONSTANT)
    Q_PROPERTY(QVariantList components READ components CONSTANT)
public:
    explicit AboutInfo(QObject* parent = nullptr);
    QString version() const;
    QString build() const;
    QString versionInformation() const;
    QString terms() const;
    QVariantList components() const;
    Q_INVOKABLE void copyVersionInformation() const;
};
