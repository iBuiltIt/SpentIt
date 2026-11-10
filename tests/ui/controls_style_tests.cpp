#include "controls_style.hpp"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>

class ControlsStyleTests : public QObject {
    Q_OBJECT
private slots:
    void disabledButtonsFade() {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls
            ApplicationWindow {
                Button { objectName: "off"; text: "Import"; enabled: false }
                Button { objectName: "on"; text: "Cancel" }
                ToolButton { objectName: "toolOff"; text: "Back"; enabled: false }
                Button { objectName: "kept"; text: "Day"; enabled: false; opacity: 0.55 }
                ComboBox { objectName: "combo"; model: ["Choose an account"] }
            }
        )", QUrl());
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        QVERIFY(root);
        QCOMPARE(warnings.count(), 0);
        auto* off = root->findChild<QQuickItem*>("off");
        auto* on = root->findChild<QQuickItem*>("on");
        auto* toolOff = root->findChild<QQuickItem*>("toolOff");
        auto* kept = root->findChild<QQuickItem*>("kept");
        auto* combo = root->findChild<QQuickItem*>("combo");
        QVERIFY(off);
        QVERIFY(on);
        QVERIFY(toolOff);
        QVERIFY(kept);
        QVERIFY(combo);
        QVERIFY(qAbs(off->opacity() - 0.4) < 0.001);
        QVERIFY(qAbs(on->opacity() - 1.0) < 0.001);
        QVERIFY(qAbs(toolOff->opacity() - 0.4) < 0.001);
        QVERIFY(qAbs(kept->opacity() - 0.55) < 0.001);
        QVERIFY(qAbs(combo->opacity() - 1.0) < 0.001);
        off->setProperty("enabled", true);
        on->setProperty("enabled", false);
        QVERIFY(qAbs(off->opacity() - 1.0) < 0.001);
        QVERIFY(qAbs(on->opacity() - 0.4) < 0.001);
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    applySpentItControlsStyle();
    ControlsStyleTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "controls_style_tests.moc"
