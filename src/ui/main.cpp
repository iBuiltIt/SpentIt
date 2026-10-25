#include "sheet_model.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QThreadPool>
#include <cstdio>
#include <limits>
#include <QFont>
#include <QQuickItem>
#include <QElapsedTimer>
#include <algorithm>
#include <vector>
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("SpentIt synthetic prototype");
    app.setFont(QFont("Helvetica Neue", 13));
    QQuickStyle::setStyle("Basic");
    int count = 100000;
    const auto args = app.arguments();
    if (args.contains("--rows")) {
        bool ok = false; const auto pos = args.indexOf("--rows");
        if (pos + 1 >= args.size()) return 2;
        count = args[pos+1].toInt(&ok); if (!ok || count < 1 || count > std::numeric_limits<int>::max() / 2) return 2;
    }
    SheetModel sheet(count);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("sheet", &sheet);
    engine.load(QUrl("qrc:/spentit/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (args.contains("--smoke") || args.contains("--scroll-benchmark")) {
        auto* ready = new QTimer(&app);
        ready->setInterval(50);
        QObject::connect(ready, &QTimer::timeout, &app, [&, ready] {
            if (sheet.busy()) return;
            ready->stop();
            if (!sheet.error().isEmpty()) { app.exit(4); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            if (args.contains("--scroll-benchmark")) {
                auto* ledger = window->findChild<QQuickItem*>("ledger");
                auto* timer = new QTimer(&app); timer->setInterval(16);
                auto elapsed = std::make_shared<QElapsedTimer>(); elapsed->start();
                auto intervals = std::make_shared<std::vector<double>>();
                auto frames = std::make_shared<std::vector<double>>();
                auto frameClock = std::make_shared<QElapsedTimer>(); frameClock->start();
                QObject::connect(window, &QQuickWindow::frameSwapped, &app, [frames,frameClock] {
                    frames->push_back(double(frameClock->nsecsElapsed()) / 1000000.0); frameClock->restart();
                });
                QObject::connect(timer, &QTimer::timeout, &app, [&,ledger,timer,elapsed,intervals,frames,tick=0]() mutable {
                    intervals->push_back(double(elapsed->nsecsElapsed()) / 1000000.0); elapsed->restart();
                    ledger->setProperty("contentY",ledger->property("contentY").toDouble() + 120.0);
                    if (++tick == 100) sheet.sortBy(1);
                    if (tick < 300) return;
                    timer->stop();
                    auto report = [](const char* label, std::vector<double> values) {
                        if (values.empty()) { std::printf("%s unavailable\n",label); return; }
                        std::sort(values.begin(),values.end());
                        std::printf("%s samples=%zu p95_ms=%.2f max_ms=%.2f\n",label,values.size(),values[values.size()*95/100],values.back());
                    };
                    report("gui_timer",*intervals); report("frame_interval",*frames);
                    std::printf("%s\n",qPrintable(sheet.metrics())); app.exit(0);
                });
                timer->start();
            } else {
                if (args.contains("--edit")) { sheet.startEdit(1); sheet.expand(1); }
                QTimer::singleShot(300, &app, [&,window] {
                const auto pos = args.indexOf("--screenshot");
                if (pos >= 0 && pos + 1 < args.size() && !window->grabWindow().save(args[pos+1])) { app.exit(3); return; }
                std::printf("%s\n", qPrintable(sheet.metrics())); app.exit(0);
            });
            }
        });
        ready->start();
        QTimer::singleShot(120000, &app, [&] { app.exit(5); });
    }
    const int result = app.exec();
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
