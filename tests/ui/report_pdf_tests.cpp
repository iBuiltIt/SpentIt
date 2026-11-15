#include "report_pdf.hpp"
#include <QTest>
#include <QFile>
#include <QBuffer>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <cstdlib>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <csignal>
#endif
#include <stdexcept>
using namespace spentit::ui::pdf;
namespace {
QVariantMap fixture(int rows=5,int periods=2,QString graph="bar") {
    QVariantList ranges,totals,items;
    for(int p=0;p<periods;++p){ranges.push_back(QVariantMap{{"label",QString("Comparison %1").arg(p)},{"start","2026-01-01"},{"end","2026-12-31"}});totals.push_back("123,456.78");}
    for(int r=0;r<rows;++r){QVariantList cells;for(int p=0;p<periods;++p)cells.push_back(QVariantMap{{"amount",r==0?"-1,234.56":"123.45"},{"value",r==0?-123456:12345}});
        items.push_back(QVariantMap{{"name",r==1?"Long category: household maintenance and occasional repairs / 家庭 / café":QString("Category %1").arg(r)},{"cells",cells},{"hue",(r*41)%360}});}
    QVariantMap currency{{"currency","CAD"},{"rows",items},{"totals",totals},{"minimum",-123456},{"maximum",12345},{"minimumLabel","-1,234.56"},{"maximumLabel","123.45"}};
    return {{"name","Synthetic spending report"},{"accountNames",QStringList{"Checking","Savings"}},{"periods",ranges},{"currencies",QVariantList{currency}},{"graph",graph},{"stale",false}};
}
class FailingDevice:public QIODevice {
public: qint64 budget=100;FailingDevice(){open(QIODevice::WriteOnly);}
protected:qint64 readData(char*,qint64)override{return -1;}
    qint64 writeData(const char*,qint64 size)override{if(size>budget){setErrorString("Synthetic disk full");return -1;}budget-=size;return size;}
};
}
class ReportPdfTests:public QObject {
    Q_OBJECT
private slots:
    void pagination() {
        QCOMPARE(layout(fixture(),{}).pages.size(),1);
        auto d=layout(fixture(90,8),{});QVERIFY(d.pages.size()>4);QVERIFY(d.paper.width()>d.paper.height());QCOMPARE(d.scale,1.0);
        int occurrences=0,headings=0;
        for(const auto& page:d.pages)for(const auto& mark:page.marks){if(mark.label=="90. Category 89")++occurrences;if(mark.label=="Category")++headings;QVERIFY(mark.box.bottom()<d.paper.height()-35);}
        QCOMPARE(occurrences,3);QVERIFY(headings>=4);
        auto one=layout(fixture(90,8),{{"layout","Fit on one page"}});QCOMPARE(one.pages.size(),1);QVERIFY(one.scale<.8);
        QVERIFY(preview(d,0).startsWith("data:image/png;base64,"));QVERIFY_THROWS_EXCEPTION(std::runtime_error,preview(d,-1));
        auto a4=layout(fixture(1,1,"none"),{{"paper","A4"},{"orientation","Portrait"}});QCOMPARE(a4.paper,QSizeF(595,842));QCOMPARE(a4.pages.size(),1);
    }
    void semantics() {
        auto data=fixture(5,2,"pie");auto second=data.value("currencies").toList()[0].toMap();second["currency"]="USD";auto currencies=data.value("currencies").toList();currencies.push_back(second);data["currencies"]=currencies;
        data["warning"]="Imported records need repair.";
        const auto d=layout(data,{});QString all;int pies=0;for(const auto& p:d.pages)for(const auto& m:p.marks){all+=m.label+"\n";pies+=m.kind==Mark::pie;}
        QVERIFY(all.contains("CAD"));QVERIFY(all.contains("USD"));QVERIFY(all.contains("-1,234.56"));QVERIFY(all.contains("Imported records need repair."));QVERIFY(all.contains("negative amounts"));QCOMPARE(pies,0);
        data["stale"]=true;QVERIFY_THROWS_EXCEPTION(std::runtime_error,layout(data,{}));
    }
    void safeSave() {
        QTemporaryDir dir;const auto path=dir.filePath("report.pdf");QFile old(path);QVERIFY(old.open(QIODevice::WriteOnly));old.write("original");old.close();
        const auto d=layout(fixture(),{});
        int checkpoints=0;QVERIFY_THROWS_EXCEPTION(std::runtime_error,save(path,d,[&]{if(++checkpoints==2)throw std::runtime_error("Cancelled");}));
        QVERIFY(old.open(QIODevice::ReadOnly));QCOMPARE(old.readAll(),QByteArray("original"));old.close();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,save(path,d,{},[]{throw std::runtime_error("Commit cancelled");}));
        QVERIFY(old.open(QIODevice::ReadOnly));QCOMPARE(old.readAll(),QByteArray("original"));old.close();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,save(dir.filePath("missing/report.pdf"),d));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,save(dir.filePath("database.spentit"),d));
        save(path,d);QVERIFY(old.open(QIODevice::ReadOnly));QVERIFY(old.readAll().startsWith("%PDF-"));old.close();
        // Failure in QIODevice is tested separately from the QSaveFile commit path.
        FailingDevice failed;QVERIFY_THROWS_EXCEPTION(std::runtime_error,write(failed,d));QCOMPARE(failed.errorString(),QString("Synthetic disk full"));
        QCOMPARE(QFileInfo(suggestedFile(dir.filePath("book.spentit"),"../Bad: name")).absolutePath(),dir.path());
        QVERIFY(suggestedFile(dir.filePath("book.spentit"),"CON").endsWith("_CON.pdf"));
    }
    void interruptionAndWriteFailure() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir;const auto path=dir.filePath("report.pdf");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("original");file.close();
        for(const auto& mode:{QString("--pdf-before"),QString("--pdf-full"),QString("--pdf-after")}) {
            QProcess child;child.start(QCoreApplication::applicationFilePath(),{mode,path});QVERIFY(child.waitForFinished(30000));
            QCOMPARE(child.exitCode(),mode=="--pdf-before"?73:mode=="--pdf-full"?75:74);
            QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();file.close();
            if(mode=="--pdf-after")QVERIFY(bytes.startsWith("%PDF-"));else QCOMPARE(bytes,QByteArray("original"));
        }
        // A normal retry replaces the original after the failed/cancelled attempts.
        save(path,layout(fixture(),{}));
#else
        QSKIP("POSIX interruption and file-size-limit checks are not available on this host.");
#endif
    }
    void visualFixtures() {
        const auto path=qEnvironmentVariable("SPENTIT_PDF_FIXTURES");if(path.isEmpty())return;QVERIFY(QDir().mkpath(path));
        for(const auto& graph:{QString("none"),QString("bar"),QString("grouped_bar"),QString("pie")}) {
            auto data=fixture(15,3,graph);
            if(graph=="pie") {auto c=data["currencies"].toList()[0].toMap();auto rows=c["rows"].toList();rows.removeFirst();c["rows"]=rows;data["currencies"]=QVariantList{c};}
            const auto d=layout(data,{});save(path+"/"+graph+".pdf",d);
        }
        save(path+"/wide.pdf",layout(fixture(90,8,"grouped_bar"),{}));
        save(path+"/fit.pdf",layout(fixture(90,8,"grouped_bar"),{{"layout","Fit on one page"}}));
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
#ifdef Q_OS_UNIX
    const auto args=app.arguments();
    if(args.size()==3 && args[1].startsWith("--pdf-")) {
        const auto document=layout(fixture(),{});
        if(args[1]=="--pdf-before")save(args[2],document,{},[]{std::_Exit(73);});
        if(args[1]=="--pdf-after"){save(args[2],document);std::_Exit(74);}
        if(args[1]=="--pdf-full") {
            struct rlimit limit{128,128};if(setrlimit(RLIMIT_FSIZE,&limit)!=0)return 76;
            std::signal(SIGXFSZ,SIG_IGN);
            try{save(args[2],document);}catch(const std::exception&){return 75;}return 77;
        }
    }
#endif
    ReportPdfTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "report_pdf_tests.moc"
