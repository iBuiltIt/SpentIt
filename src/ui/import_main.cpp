#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QFont>
#include <QSettings>
#include <QTemporaryDir>
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    app.setOrganizationName("SpentIt");app.setApplicationName("Durable import experiment");
    app.setFont(QFont("Helvetica Neue",13));
    QQuickStyle::setStyle("Basic");
    std::unique_ptr<QTemporaryDir> smokeSettings;
    if(app.arguments().contains("--smoke")) {
        smokeSettings=std::make_unique<QTemporaryDir>();
        if(!smokeSettings->isValid()) return 2;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,smokeSettings->path());
        QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,smokeSettings->path());
    }
    ImportModel model(spentit::storage::document_gateway());
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);
    engine.load(QUrl("qrc:/spentit/ImportMain.qml"));if(engine.rootObjects().isEmpty())return 1;
    if(app.arguments().contains("--smoke"))QTimer::singleShot(500,&app,[&]{engine.rootObjects().first()->setProperty("allowClose",true);app.quit();});
    return app.exec();
}
