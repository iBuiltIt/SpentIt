#include <spentit/diagnostics.hpp>
#include <QQuickWindow>
#include <iostream>
#ifdef SPENTIT_DIAGNOSTICS
#include "diagnostic_transport.hpp"
#endif
#include "import_model.hpp"
#include "help/help_catalog.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "controls_style.hpp"
#include <QTimer>
#include <QFont>
#include <QFileInfo>
#include <QSettings>
#include <QVariantMap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QIcon>
#include <QStyleHints>
#ifdef Q_OS_MACOS
#include <QEvent>
#include <QEventLoop>
#include <QFileOpenEvent>
#include <QWindow>
#endif
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
void reportLaunchError(const QString& message) {
    std::cerr<<message.toStdString()<<'\n';
#ifdef Q_OS_WIN
    MessageBoxW(nullptr,reinterpret_cast<const wchar_t*>(message.utf16()),L"SpentIt",MB_OK|MB_ICONWARNING);
#endif
}
#ifdef Q_OS_MACOS
void raiseSpentItWindow(QObject* root) {
    auto* window=qobject_cast<QWindow*>(root);
    if(!window) return;
    auto states=window->windowStates();
    if(states.testFlag(Qt::WindowMinimized)) {states.setFlag(Qt::WindowMinimized,false);window->setWindowStates(states);}
    if(!window->isVisible()) window->show();
    window->raise();
    window->requestActivate();
}
class MacDocumentOpen final : public QObject {
public:
    MacDocumentOpen(ImportModel* model, QObject** window, QObject* parent):QObject(parent),model_(model),window_(window) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if(event->type()!=QEvent::FileOpen || watched!=qApp) return QObject::eventFilter(watched,event);
        const auto* opened=static_cast<const QFileOpenEvent*>(event);
        auto path=opened->file();
        if(path.isEmpty()) path=opened->url().toLocalFile();
        if(path.isEmpty()) return true;
        const auto kind=model_->launchFileKind(path);
        if(kind==QLatin1String("statement")) {
            if(model_->deliverLaunchStatement(path)!=ImportModel::LaunchDelivery::ignored) raiseSpentItWindow(window_?*window_:nullptr);
        } else if(kind==QLatin1String("document")) {
            if(model_->deliverLaunchDocument(path)!=ImportModel::LaunchDelivery::ignored) raiseSpentItWindow(window_?*window_:nullptr);
        } else {
            model_->refuseStatement(QStringLiteral("type"));
            raiseSpentItWindow(window_?*window_:nullptr);
        }
        return true;
    }
private:
    ImportModel* model_;
    QObject** window_;
};
#endif
}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    QString launchError;
    const auto launchPath=ImportModel::launchDocumentPath(app.arguments(),&launchError);
    if(!launchError.isEmpty()) {reportLaunchError(launchError);return 2;}
    QString statementError;
    const auto statementPath=ImportModel::launchStatementPath(app.arguments(),&statementError);
    if(!statementError.isEmpty()) {reportLaunchError(statementError);return 2;}
    const bool diagnosticsRequested=app.arguments().contains("--diagnostics");
    if(diagnosticsRequested && !spentit::diagnostics::Collector::instance().enable()){std::cerr<<"Diagnostics unsupported in this build.\n";return 2;}
#ifdef SPENTIT_DIAGNOSTICS
    DiagnosticTransport transport;
    if(diagnosticsRequested){if(!transport.start()){std::cerr<<"Diagnostics endpoint unavailable; permissions were not relaxed.\n";return 2;}std::cerr<<"Diagnostics endpoint: "<<transport.endpoint().toStdString()<<'\n';}
#endif
    const auto diagnosticStartup=spentit::diagnostics::now();
    app.setOrganizationName("SpentIt");app.setApplicationName("Durable import experiment");
    // Keep the existing settings identity so the remembered document survives.
    app.setApplicationDisplayName("SpentIt");
    app.setDesktopFileName("SpentIt");
    const auto updateIcon = [] {
        const QString variant = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
            ? QStringLiteral("dark") : QStringLiteral("light");
        QIcon icon;
        for (const int size : {16, 32, 64, 128, 256, 512, 1024})
            icon.addFile(QStringLiteral(":/spentit/images/SpentIt_%1_%2.png").arg(variant).arg(size));
        QGuiApplication::setWindowIcon(icon);
    };
    updateIcon();
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &app, updateIcon);
    app.setFont(QFont("Helvetica Neue",13));
    applySpentItControlsStyle();
    std::unique_ptr<QTemporaryDir> smokeSettings;
    const bool smoke=app.arguments().contains("--smoke");
    // Package verification. Same create command as the Create file button.
    const bool smokeCreate=app.arguments().contains("--smoke-create");
    if((!launchPath.isEmpty() || !statementPath.isEmpty()) && (smoke || smokeCreate)) {reportLaunchError(QStringLiteral("SpentIt cannot open a file during a package check."));return 2;}
    if(smoke || smokeCreate) {
        smokeSettings=std::make_unique<QTemporaryDir>();
        if(!smokeSettings->isValid()) return 2;
        if(smokeCreate) smokeSettings->setAutoRemove(false);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,smokeSettings->path());
        QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,smokeSettings->path());
        if(!smokeCreate) QSettings().setValue("datasetPath",smokeSettings->path()+"/missing.spentit");
    }
    ImportModel model(spentit::storage::document_gateway((QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/verification").toStdString()));
    if(!launchPath.isEmpty()) model.setLaunchDocument(launchPath);
    if(!statementPath.isEmpty()) model.deliverLaunchStatement(statementPath);
#ifdef Q_OS_MACOS
    // Finder delivers a double-clicked file while NSApplication finishes launching.
    // That can be after QML would already open the remembered file. Hold that open
    // until the file arrives, or until the event loop shows that none did.
    QObject* root=nullptr;
    MacDocumentOpen documentOpen(&model,&root,nullptr);
    if(launchPath.isEmpty() && !smoke && !smokeCreate) {
        model.holdLaunch();
        app.installEventFilter(&documentOpen);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    } else app.installEventFilter(&documentOpen);
#endif
    app.setApplicationVersion(model.aboutInformation()->version());
    HelpCatalog helpCatalog;
    helpCatalog.load(QStringLiteral(":/spentit/help/manifest.json"));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("workflow",&model);
    engine.rootContext()->setContextProperty("helpCatalog",&helpCatalog);
    {spentit::diagnostics::Span qmlSpan(spentit::diagnostics::Code::qml_creation);
    engine.load(QUrl("qrc:/spentit/ImportMain.qml"));if(engine.rootObjects().isEmpty())return 1;}
#ifdef Q_OS_MACOS
    root=engine.rootObjects().first();
#endif
    if(diagnosticsRequested) {
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if(window) {
            auto frameGeneration=std::make_shared<std::atomic<qulonglong>>(UINT64_MAX);
            auto frameRevision=std::make_shared<std::atomic<qulonglong>>(UINT64_MAX);
            QObject::connect(window,&QQuickWindow::beforeSynchronizing,window,[frameGeneration,frameRevision]{
                const auto snapshot=spentit::diagnostics::Collector::instance().snapshot(UINT64_MAX);
                frameGeneration->store(snapshot.ui.interaction_ready && snapshot.ui.generation==snapshot.view?snapshot.view:UINT64_MAX);
                frameRevision->store(snapshot.ui.revision);
            },Qt::DirectConnection);
            QObject::connect(window,&QQuickWindow::frameSwapped,window,[frameGeneration,frameRevision]{
                spentit::diagnostics::Collector::instance().frame(frameGeneration->load(),frameRevision->load());
            },Qt::DirectConnection);
        }
    }
    if(smokeCreate) {
        const QString path=smokeSettings->path()+"/smoke.spentit";
        QObject::connect(&model,&ImportModel::completed,&app,[&](const QString& action,bool success){
            if(action!="create") return;
            if(!success || !QFileInfo::exists(path) || QFileInfo(path).size()<=0) {
                std::cerr<<"Create failed: "<<model.error().toStdString()<<'\n';
                app.exit(1);
                return;
            }
            std::cout<<"Created "<<path.toStdString()<<'\n'<<std::flush;
            engine.rootObjects().first()->setProperty("allowClose",true);
            app.quit();
        });
        auto* wait=new QTimer(&app);
        wait->setInterval(50);
        QObject::connect(wait,&QTimer::timeout,&app,[wait,&model,path]{
            if(model.busy()) return;
            wait->stop();
            QVariantMap args;
            args.insert("path",path);
            args.insert("name","SpentIt");
            args.insert("currency","CAD");
            args.insert("protected",false);
            args.insert("password","");
            args.insert("confirmation","");
            args.insert("overwrite",false);
            args.insert("dateFormat","YYYY-MM-DD");
            args.insert("defaultCategories",true);
            model.command("create",args);
        });
        wait->start();
        QTimer::singleShot(30000,&app,[&]{std::cerr<<"Create did not finish.\n";app.exit(1);});
    } else if(smoke) QTimer::singleShot(diagnosticsRequested?1500:500,&app,[&]{engine.rootObjects().first()->setProperty("allowClose",true);app.quit();});
#ifdef Q_OS_MACOS
    if(model.launchHeld()) QTimer::singleShot(0,&model,[&model]{model.releaseLaunchHold();});
#endif
    spentit::diagnostics::Collector::instance().metric(spentit::diagnostics::Code::startup,spentit::diagnostics::now()-diagnosticStartup,0,false);
    return app.exec();
}
