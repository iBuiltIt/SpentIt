#pragma once
#include <spentit/file_exchange.hpp>
#include <QJsonObject>
#include <QVariantMap>
namespace spentit::ui::exchange {
QByteArray encode(app::ImportRepository&,bool rules,bool reports,bool categories);
QJsonObject decode(const QByteArray&);
QVariantMap preview(app::ImportRepository&,const QJsonObject&);
app::DefinitionImport plan(app::ImportRepository&,const QJsonObject&,const QVariantMap&,const app::ActionToken&);
void publish(const QString& path,const QByteArray&,const QString& activePath,bool qif,const app::OperationContext&);
}
