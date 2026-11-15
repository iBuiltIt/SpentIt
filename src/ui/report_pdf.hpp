#pragma once
#include <QColor>
#include <QRectF>
#include <QSizeF>
#include <QVariantMap>
#include <QVector>
#include <functional>
class QPainter;
class QIODevice;
namespace spentit::ui::pdf {
// Coordinates are points; money strings come unchanged from the report snapshot.
struct Mark {
    enum Kind { text, rectangle, line, pie } kind=text;
    QRectF box;
    QString label;
    QColor color=Qt::black;
    int size=10;
    bool bold=false;
    int start=0, span=0;
};
struct Page { QVector<Mark> marks; };
struct Document {
    QSizeF paper;
    QVector<Page> pages;
    QString title;
    double scale=1;
};
Document layout(const QVariantMap& report,const QVariantMap& options,const std::function<void()>& checkpoint={});
void paint(QPainter&,const Document&,int page);
QString preview(const Document&,int page);
void write(QIODevice&,const Document&,const std::function<void()>& checkpoint={});
void save(const QString& path,const Document&,const std::function<void()>& checkpoint={},const std::function<void()>& beforeCommit={});
QString suggestedFile(const QString& documentPath,const QString& reportName);
}
