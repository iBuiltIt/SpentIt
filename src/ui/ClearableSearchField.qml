import QtQuick
import QtQuick.Controls

TextField {
    id: field
    property string clearObjectName: objectName + "Clear"
    property bool hasAppliedFilter: false
    signal clearRequested()
    function clearSearch() {
        clear()
        forceActiveFocus()
    }
    rightPadding: clearButton.visible ? clearButton.width + 8 : 8
    ToolButton {
        id: clearButton
        objectName: field.clearObjectName
        text: "×"
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: Math.min(28, parent.height - 4)
        visible: field.text.length > 0 || field.hasAppliedFilter
        enabled: !field.readOnly
        Accessible.name: "Clear search"
        ToolTip.text: "Clear search"
        ToolTip.visible: hovered
        onClicked: field.clearRequested()
    }
}
