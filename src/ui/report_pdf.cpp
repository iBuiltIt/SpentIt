#include "report_pdf.hpp"
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QTextLayout>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPageLayout>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spentit::ui::pdf {
namespace {
constexpr double margin=36;
class CheckedDevice final : public QIODevice {
public:
    explicit CheckedDevice(QIODevice& target):target_(target){open(QIODevice::WriteOnly);}
    bool failed=false;
    bool isSequential() const override{return true;}
protected:
    qint64 readData(char*,qint64) override{return -1;}
    qint64 writeData(const char* data,qint64 count) override {
        const auto written=target_.write(data,count);if(written!=count)failed=true;return written;
    }
private:QIODevice& target_;
};
QFont font(int size,bool bold=false) { QFont f=QGuiApplication::font();f.setPixelSize(size);f.setBold(bold);return f; }
double textLayout(QTextLayout& layout,double width) {
    QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
    layout.beginLayout();double y=0;
    while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(width);line.setPosition({0,y});y+=line.height();}
    layout.endLayout();return y;
}
QString lines(QString text){return text.replace('\n',QChar::LineSeparator);}
double height(const QString& text,double width,int size=10,bool bold=false) {
    QTextLayout layout(lines(text),font(size,bold));return std::max(16.0,textLayout(layout,width)+8);
}
struct Builder {
    Document doc;
    double width=0,y=margin,limit=0;
    bool fit=false;
    qsizetype count=0;
    std::function<void()> check;
    void mark(Mark m) {
        if(++count>300000)throw std::runtime_error("This report is too large for PDF. Choose fewer categories or comparisons.");
        doc.pages.last().marks.push_back(std::move(m));
    }
    void text(const QString& s,QRectF r,int size=10,bool bold=false) {mark({Mark::text,r,s,Qt::black,size,bold});}
    void page() {
        if(check)check();
        if(doc.pages.size()>=1000)throw std::runtime_error("This report needs more than 1,000 pages. Choose fewer categories or comparisons.");
        doc.pages.push_back({});y=margin;
        const double h=height(doc.title,width,16,true);
        text(doc.title,{margin,y,width,h},16,true);y+=h+8;
    }
    void need(double h) {
        if(check)check();
        if(!fit && y+h>limit)page();
        if(!fit && y+h>limit)throw std::runtime_error("A report label is too long to fit on a page. Shorten it or choose a larger layout.");
    }
    void paragraph(const QString& s,int size=10,bool bold=false) {
        const double h=height(s,width,size,bold);need(h);text(s,{margin,y,width,h},size,bold);y+=h;
    }
    void row(const QStringList& cells,const QVector<double>& widths,bool heading=false,bool stripe=false) {
        double h=24;for(qsizetype i=0;i<cells.size();++i)h=std::max(h,height(cells[i],widths[i]-12,10,heading));
        need(h);double x=margin;
        if(heading||stripe)mark({Mark::rectangle,{margin,y,width,h},{},heading?QColor("#e3eaf0"):QColor("#f3f5f7")});
        for(qsizetype i=0;i<cells.size();++i){text(cells[i],{x+6,y,widths[i]-12,h},10,heading);x+=widths[i];}
        y+=h;
    }
};
QString periodLabel(const QVariantList& periods,int p) {
    const auto item=periods[p].toMap();return QString::number(p+1)+". "+item.value("label").toString()+"\n"+item.value("start").toString()+"\n"+item.value("end").toString();
}
void charts(Builder& b,const QVariantMap& currency,const QVariantList& periods,const QString& graph,int onlyPeriod=-1) {
    if(graph=="none")return;
    const auto rows=currency.value("rows").toList();
    const bool grouped=graph=="grouped_bar";
    if(!grouped && onlyPeriod<0 && periods.size()>1 && rows.size()<=20) {
        const double gap=18, panel=(b.width-gap)/2;
        for(int first=0;first<periods.size();first+=2) {
            QVector<Builder> panels;double h=0;
            for(int p=first;p<std::min(first+2,static_cast<int>(periods.size()));++p){
                Builder child;child.width=panel;child.fit=true;child.y=0;child.check=b.check;child.doc.pages.push_back({});
                charts(child,currency,periods,graph,p);h=std::max(h,child.y);panels.push_back(std::move(child));
            }
            b.need(h);
            for(int i=0;i<panels.size();++i)for(auto mark:panels[i].doc.pages[0].marks){mark.box.translate(i*(panel+gap),b.y);b.mark(std::move(mark));}
            b.y+=h;
        }
        return;
    }

    for(int period=onlyPeriod<0?0:onlyPeriod;period<(onlyPeriod<0?(grouped?1:periods.size()):onlyPeriod+1);++period) {
        const auto title=grouped?QString("Period comparison"):periodLabel(periods,period);
        bool negative=false;double total=0;
        for(const auto& item:rows){const double v=item.toMap().value("cells").toList()[period].toMap().value("value").toDouble();negative|=v<0;total+=v;}
        if(graph=="pie" && negative){b.paragraph(title+"\nThis period has negative amounts. Choose Bar to display them.");continue;}
        if(rows.isEmpty() || (graph=="pie" && total==0)){b.paragraph(title+"\nNo amounts in this period.");continue;}
        // Charts stay intact. Category and period keys refer to the complete table below.
        const int capacity=(graph=="pie" || onlyPeriod>=0)?static_cast<int>(rows.size()):std::max(1,static_cast<int>((b.width-50)/(grouped?static_cast<double>(periods.size())*6+10:20)));
        const int blocks=(static_cast<int>(rows.size())+capacity-1)/capacity;
        const int chunk=(static_cast<int>(rows.size())+blocks-1)/blocks;
        for(int first=0;first<rows.size();first+=chunk) {
            const int last=std::min(static_cast<int>(rows.size()),first+chunk);
            QString label=currency.value("currency").toString()+" - "+title;
            if(graph!="pie")label+="\nShared scale: "+currency.value("minimumLabel").toString()+" to "+currency.value("maximumLabel").toString();
            label+=graph=="pie"?"\nCategory colours match the table":"\nCategory numbers match the table";
            if(grouped)label+="\nBars: period 1 (darkest) to "+QString::number(periods.size())+" (lightest)";
            const double heading=height(label,b.width,10,true);
            const double chartHeight=graph=="pie"?140:110,blockHeight=chartHeight+34;
            b.need(heading+blockHeight);b.text(label,{margin,b.y,b.width,heading},10,true);b.y+=heading;
            const double top=b.y;
            if(graph=="pie") {
                int angle=90*16,used=0;
                for(int r=0;r<rows.size();++r){const auto row=rows[r].toMap();const auto v=row.value("cells").toList()[period].toMap().value("value").toDouble();
                    const int span=r+1==rows.size()?5760-used:static_cast<int>(std::round(v/total*5760));used+=span;
                    b.mark({Mark::pie,{margin+b.width/2-70,top,140,140},{},QColor::fromHsl(row.value("hue").toInt(),153,122),10,false,angle,-span});angle-=span;
                }
            } else {
                const double low=currency.value("minimum").toDouble(),high=currency.value("maximum").toDouble();
                const double range=high-low==0?1:high-low;
                const auto yy=[&](double v){return top+chartHeight-(v-low)/range*chartHeight;};
                const double step=b.width/(last-first);const int n=grouped?static_cast<int>(periods.size()):1;
                b.mark({Mark::line,{margin,yy(0),b.width,0},{},QColor("#888888")});
                for(int r=first;r<last;++r){const auto row=rows[r].toMap();const auto cells=row.value("cells").toList();
                    for(int p=0;p<n;++p){const double v=cells[grouped?p:period].toMap().value("value").toDouble();const double edge=yy(v);
                        b.mark({Mark::rectangle,{margin+(r-first)*step+step*.1+p*step*.8/n,std::min(edge,yy(0)),step*.8/n-1,std::abs(edge-yy(0))},{},QColor::fromHsl(row.value("hue").toInt(),153,n==1?122:92+static_cast<int>(66.0*p/(n-1)))});
                    }
                    b.text(QString::number(r+1),{margin+(r-first)*step,top+chartHeight+2,step,20},9);
                }
            }
            b.y+=blockHeight;
        }
        if(grouped){b.paragraph("Bars run left to right in period order, from darker to lighter.");for(int p=0;p<periods.size();++p)b.paragraph(periodLabel(periods,p));}
    }
}
}
Document layout(const QVariantMap& report,const QVariantMap& options,const std::function<void()>& checkpoint) {
    if(report.isEmpty() || report.value("stale").toBool())throw std::runtime_error("Refresh the report before saving a PDF.");
    const auto periods=report.value("periods").toList();
    if(periods.isEmpty() || periods.size()>32)throw std::runtime_error("Show a report before saving a PDF.");
    const auto paper=options.value("paper","Letter").toString(),orientation=options.value("orientation","Automatic").toString(),mode=options.value("layout","Readable pages").toString();
    if((paper!="Letter" && paper!="A4") || (orientation!="Automatic" && orientation!="Portrait" && orientation!="Landscape") || (mode!="Readable pages" && mode!="Fit on one page"))throw std::runtime_error("Choose a PDF paper size and layout.");
    Builder b;b.check=checkpoint;b.fit=mode=="Fit on one page";b.doc.paper=paper=="A4"?QSizeF(595,842):QSizeF(612,792);
    if(orientation=="Landscape" || (orientation=="Automatic" && periods.size()>3))b.doc.paper.transpose();
    b.width=b.doc.paper.width()-2*margin;b.limit=b.doc.paper.height()-margin-22;
    // Amount columns must accommodate exact formatted amounts without clipping.
    double amountWidth=110;
    const auto currencies=report.value("currencies").toList();
    const QFontMetricsF amountMetrics(font(10)),totalMetrics(font(10,true));
    for(const auto& c:currencies){
        for(const auto& r:c.toMap().value("rows").toList()) {
            if(checkpoint)checkpoint();
            for(const auto& cell:r.toMap().value("cells").toList())
                amountWidth=std::max(amountWidth,amountMetrics.horizontalAdvance(cell.toMap().value("amount").toString())+16);
        }
        for(const auto& total:c.toMap().value("totals").toList())
            amountWidth=std::max(amountWidth,totalMetrics.horizontalAdvance(total.toString())+16);
    }
    if(b.fit)b.width=std::max(b.width,180+amountWidth*static_cast<double>(periods.size()));
    const int capacity=std::max(1,static_cast<int>((b.width-180)/amountWidth));
    if(capacity<2 && periods.size()>1)throw std::runtime_error("These amounts need more room. Choose Landscape or Fit on one page.");
    b.doc.title=report.value("name").toString();b.page();
    b.paragraph("Accounts: "+report.value("accountNames").toStringList().join(", "));
    b.paragraph("Net spending: outflows are positive; refunds reduce spending; income is negative.");
    if(!report.value("warning").toString().isEmpty())b.paragraph(report.value("warning").toString());
    if(currencies.isEmpty())b.paragraph("No accounts selected.");
    for(const auto& item:currencies) {
        const auto currency=item.toMap();b.paragraph(currency.value("currency").toString(),13,true);
        charts(b,currency,periods,report.value("graph").toString());
        int next=1;
        do {
            QVector<int> columns{0};while(next<periods.size() && columns.size()<capacity)columns.push_back(next++);
            if(b.doc.pages.last().marks.size()>2)b.y+=12;
            const double categoryWidth=b.width-amountWidth*static_cast<double>(columns.size());
            QVector<double> widths{categoryWidth};QStringList headings{"Category"};
            for(int p:columns){widths.push_back(amountWidth);headings.push_back(periodLabel(periods,p));}
            double headerHeight=24;for(int i=0;i<headings.size();++i)headerHeight=std::max(headerHeight,height(headings[i],widths[i]-12,10,true));
            const auto header=[&]{b.paragraph(currency.value("currency").toString()+" - category totals",11,true);b.row(headings,widths,true);};
            double firstRowHeight=24;
            const auto tableRows=currency.value("rows").toList();
            if(!tableRows.isEmpty())firstRowHeight=std::max(24.0,height("1. "+tableRows[0].toMap().value("name").toString(),categoryWidth-12));
            b.need(headerHeight+height(currency.value("currency").toString()+" - category totals",b.width,11,true)+24+firstRowHeight);header();
            QStringList totals{"Total"};for(int p:columns)totals.push_back(currency.value("totals").toList()[p].toString());b.row(totals,widths,true);
            const auto rows=currency.value("rows").toList();
            for(int r=0;r<rows.size();++r){const auto row=rows[r].toMap();QStringList cells{QString::number(r+1)+". "+row.value("name").toString()};for(int p:columns)cells.push_back(row.value("cells").toList()[p].toMap().value("amount").toString());
                double h=24;for(int i=0;i<cells.size();++i)h=std::max(h,height(cells[i],widths[i]-12));
                if(!b.fit && b.y+h>b.limit){b.page();header();}
                const double oldY=b.y;const auto oldPage=b.doc.pages.size();b.row(cells,widths,false,r%2==0);
                if(oldPage!=b.doc.pages.size())throw std::runtime_error("A category name is too long for the page. Shorten it or choose Fit on one page.");
                b.mark({Mark::rectangle,{margin+1,oldY+8,3,8},{},QColor::fromHsl(row.value("hue").toInt(),153,122)});
            }
            if(next<periods.size() && !b.fit)b.page();
        } while(next<periods.size());
    }
    if(b.fit)b.doc.scale=std::min({1.0,(b.doc.paper.width()-2*margin)/b.width,(b.doc.paper.height()-2*margin-22)/(b.y-margin)});
    return b.doc;
}
void paint(QPainter& p,const Document& d,int page) {
    if(page<0 || page>=d.pages.size())throw std::runtime_error("Choose an available PDF page.");
    p.save();p.fillRect(QRectF(QPointF(0,0),d.paper),Qt::white);p.setRenderHint(QPainter::Antialiasing);
    p.translate(margin,margin);p.scale(d.scale,d.scale);p.translate(-margin,-margin);
    for(const auto& m:d.pages[page].marks){p.setPen(m.color);p.setBrush(m.color);
        switch(m.kind){case Mark::text:{QTextLayout layout(lines(m.label),font(m.size,m.bold));const double h=textLayout(layout,m.box.width());layout.draw(&p,m.box.topLeft()+QPointF(0,(m.box.height()-h)/2));break;}
        case Mark::rectangle:p.fillRect(m.box,m.color);break;
        case Mark::line:p.drawLine(m.box.topLeft(),m.box.bottomRight());break;
        case Mark::pie:p.setPen(Qt::NoPen);p.drawPie(m.box,m.start,m.span);break;}
    }
    p.restore();p.setFont(font(9));p.setPen(QColor("#555555"));p.drawText(QRectF(margin,d.paper.height()-margin,d.paper.width()-2*margin,18),Qt::AlignRight,QString("Page %1 of %2").arg(page+1).arg(d.pages.size()));
}
QString preview(const Document& d,int page) {
    const double scale=1.5;QImage image((d.paper*scale).toSize(),QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);painter.scale(scale,scale);paint(painter,d,page);painter.end();
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);if(!image.save(&buffer,"PNG"))throw std::runtime_error("Could not create the PDF preview.");
    return "data:image/png;base64,"+QString::fromLatin1(bytes.toBase64());
}
void write(QIODevice& device,const Document& d,const std::function<void()>& checkpoint) {
    if(d.pages.isEmpty())throw std::runtime_error("Preview the report before saving it.");
    CheckedDevice checked(device);
    {
    QPdfWriter writer(&checked);writer.setResolution(72);writer.setPageLayout(QPageLayout(QPageSize(d.paper,QPageSize::Point),QPageLayout::Portrait,QMarginsF(0,0,0,0),QPageLayout::Point));writer.setTitle(d.title);writer.setCreator("SpentIt");
    QPainter painter;if(!painter.begin(&writer))throw std::runtime_error("Could not start the PDF. Choose another destination and try again.");
    for(int i=0;i<d.pages.size();++i){if(checkpoint)checkpoint();if(i && !writer.newPage())throw std::runtime_error("Could not write a PDF page. Check available disk space and try again.");paint(painter,d,i);}
    if(!painter.end())throw std::runtime_error("Could not finish the PDF. Check available disk space and try again.");
    }
    if(checked.failed)throw std::runtime_error("Could not write the PDF. Check available disk space and try again.");
}
void save(const QString& path,const Document& d,const std::function<void()>& checkpoint,const std::function<void()>& beforeCommit) {
    if(!path.endsWith(".pdf",Qt::CaseInsensitive))throw std::runtime_error("Choose a filename ending in .pdf.");
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))throw std::runtime_error("Could not save the PDF. Choose a writable folder and try again.");
    write(file,d,checkpoint);
    if(file.error()!=QFileDevice::NoError)throw std::runtime_error("Could not write the PDF. Check available disk space and try again.");
    if(checkpoint)checkpoint();if(beforeCommit)beforeCommit();
    if(!file.commit())throw std::runtime_error("Could not finish saving the PDF. Check available disk space and try again.");
}
QString suggestedFile(const QString& documentPath,const QString& reportName) {
    QString name=reportName;name.replace(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]"),"_");name=name.trimmed().left(120);while(name.endsWith('.')||name.endsWith(' '))name.chop(1);
    if(name.isEmpty())name="Report";
    if(QRegularExpression("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())name.prepend('_');
    QDir dir=QFileInfo(documentPath).absoluteDir();if(documentPath.isEmpty() || !dir.exists() || !QFileInfo(dir.absolutePath()).isWritable())dir=QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    return dir.filePath(name+".pdf");
}
}
