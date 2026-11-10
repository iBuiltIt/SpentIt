import QtQuick.Controls.Basic as Basic

// Same unavailable treatment as Button.qml. An instance opacity binding
// replaces this one.
Basic.ToolButton {
    opacity: enabled ? 1 : 0.4
}
