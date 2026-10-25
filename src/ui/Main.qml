import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280; height: 840
    minimumWidth: 940; minimumHeight: 600
    visible: true
    title: "SpentIt · Synthetic sheet prototype"
    color: "#f6f7f9"
    property bool sidebarVisible: true
    property bool closingApproved: false
    property string previousTitle: ""
    font.family: "Helvetica Neue"
    font.pixelSize: 13
    palette.buttonText: "#243b45"
    palette.highlight: "#126d65"
    palette.button: "#edf1f3"
    palette.window: "#f6f7f9"
    onClosing: function(close) {
        if (!closingApproved) { close.accepted = false; sheet.requestClose() }
    }
    Shortcut { sequence: "Ctrl+B"; onActivated: window.sidebarVisible = !window.sidebarVisible }
    Connections {
        target: sheet
        function onAskNavigation() { navigation.open() }
        function onCloseAllowed() { window.closingApproved = true; window.close() }
        function onChanged() {
            if (window.previousTitle !== sheet.title) { search.text = ""; window.previousTitle = sheet.title }
        }
        function onReveal(index, offset) {
            if (index < 0) return
            Qt.callLater(function() {
                ledger.positionViewAtIndex(index, offset >= 0 ? ListView.Beginning : ListView.Contain)
                if (offset >= 0) ledger.contentY += offset
            })
        }
    }
    Dialog {
        id: navigation
        objectName: "navigationDialog"
        title: "Leave this edit?"
        width: 570
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        contentItem: ColumnLayout {
            spacing: 18
            Label { text: "Save changes to this synthetic session, discard them, or stay here." }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Stay"; onClicked: { navigation.close(); sheet.resolve("stay") } }
                Button { text: "Discard"; onClicked: { navigation.close(); sheet.resolve("discard") } }
                Button { text: "Save"; highlighted: true; onClicked: { navigation.close(); sheet.resolve("save") } }
            }
        }
    }
    header: ToolBar {
        height: 64
        background: Rectangle { color: "#ffffff"; border.color: "#dbe2e6" }
        RowLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 10
            ToolButton { text: "☰"; Accessible.name: "Toggle sidebar"; onClicked: window.sidebarVisible = !window.sidebarVisible }
            ToolButton { text: "‹"; Accessible.name: "Back"; enabled: sheet.canBack && !sheet.busy; onClicked: sheet.back() }
            ToolButton { text: "›"; Accessible.name: "Forward"; enabled: sheet.canForward && !sheet.busy; onClicked: sheet.forward() }
            Label { text: "SpentIt"; font.pixelSize: 20; font.bold: true; color: "#126d65" }
            Label { text: "/  " + sheet.title; color: "#52636a"; Layout.fillWidth: true }
            TextField {
                id: search; objectName: "searchField"
                Layout.preferredWidth: 260
                placeholderText: "⌕  Literal text filter · Enter"
                maximumLength: 256
                enabled: !sheet.busy
                onAccepted: { ledger.captureViewport(); sheet.filter(text) }
            }
            ToolButton { text: "Clear"; enabled: !sheet.busy; onClicked: { search.text = ""; sheet.filter("") } }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 34; color: "#eaf2ef"
            Label { anchors.centerIn: parent; text: "SYNTHETIC DATA ONLY  ·  Session saves are temporary  ·  No bank files or document storage"; color: "#2e6258"; font.pixelSize: 12 }
        }
        SplitView {
            Layout.fillWidth: true; Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 5; implicitHeight: 5; color: SplitHandle.hovered ? "#87b5ac" : "#dde4e7" }
            Rectangle {
                visible: window.sidebarVisible
                SplitView.preferredWidth: 244; SplitView.minimumWidth: 210; SplitView.maximumWidth: window.width * 0.38
                color: "#eef2f4"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 12
                    Label { text: "WORKSPACE"; font.pixelSize: 11; font.letterSpacing: 2; color: "#70818a" }
                    SplitView {
                        Layout.fillHeight: true; Layout.fillWidth: true; orientation: Qt.Vertical
                        handle: Rectangle { implicitHeight: 5; color: SplitHandle.hovered ? "#87b5ac" : "#d3dce1" }
                        ScrollView {
                            SplitView.preferredHeight: 300; SplitView.minimumHeight: 145
                            clip: true
                            ColumnLayout {
                                width: parent.width; spacing: 10
                                Label { text: "Accounts"; font.bold: true; font.pixelSize: 16 }
                                Repeater {
                                    model: ["Everyday checking", "Reserve account"]
                                    ColumnLayout {
                                        required property int index
                                        required property string modelData
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Button {
                                            Layout.fillWidth: true
                                            text: parent.modelData; highlighted: sheet.title === parent.modelData
                                            enabled: !sheet.busy
                                            onClicked: { ledger.captureViewport(); sheet.navigate(parent.index) }
                                        }
                                        ToolButton {
                                            Layout.fillWidth: true
                                            text: sheet.accountBalances[parent.index] || "Loading…"
                                            font.pixelSize: 10
                                            enabled: !sheet.busy
                                            onClicked: sheet.toggleAccountBalance(parent.index)
                                            Accessible.name: "Toggle " + parent.modelData + " balance"
                                        }
                                    }
                                }
                                Label { text: "CAD · synthetic accounts"; color: "#70818a"; font.pixelSize: 11 }
                            }
                        }
                        ScrollView {
                            SplitView.minimumHeight: 110; clip: true
                            ColumnLayout {
                                width: parent.width; spacing: 12
                                Label { text: "Reports"; font.bold: true; font.pixelSize: 16 }
                                Label { text: "Report creation is outside\nthis prototype."; color: "#70818a"; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#d3dce1" }
                    Label { text: "Settings · Categories · Rules"; color: "#70818a"; font.pixelSize: 11 }
                    Label { text: "Editors planned separately"; color: "#70818a"; font.pixelSize: 11 }
                }
            }
            ColumnLayout {
                SplitView.fillWidth: true; spacing: 0
                RowLayout {
                    Layout.fillWidth: true; Layout.margins: 20
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: sheet.title; font.pixelSize: 26; font.bold: true; color: "#1e343d" }
                        Label { text: "Account activity  /  CAD  /  " + sheet.selectedCount + " selected"; color: "#70818a" }
                    }
                    Button { text: sheet.balance + "  ⇄"; enabled: !sheet.busy; onClicked: sheet.toggleBalance(); Accessible.name: "Toggle committed and projected balance" }
                }
                Label { Layout.leftMargin: 20; Layout.bottomMargin: 10; text: "Balance column: chronological projection including drafts. Double-click a row to edit."; color: "#70818a"; font.pixelSize: 11 }
                Rectangle {
                    Layout.fillWidth: true; height: 38; color: "#e9eef1"
                    Row {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 18
                        Repeater {
                            model: ["Date", "Payee", "Category", "Withdrawal", "Deposit", "Balance"]
                            Button {
                                required property int index
                                required property string modelData
                                width: (parent.width * [0.14,0.24,0.17,0.15,0.14,0.16][index]); height: 38
                                flat: true; text: modelData + (sheet.sortColumn === index ? (sheet.descending ? " ↓" : " ↑") : "")
                                enabled: !sheet.busy
                                onClicked: sheet.sortBy(index)
                            }
                        }
                    }
                }
                ListView {
                    id: ledger; objectName: "ledger"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: sheet; clip: true; reuseItems: true; cacheBuffer: 300
                    spacing: 1; focus: true
                    ScrollBar.vertical: ScrollBar {}
                    function captureViewport() {
                        let i = indexAt(10, contentY + 2)
                        let item = itemAtIndex(i)
                        if (item) sheet.rememberViewport(item.transactionId, contentY - item.y)
                    }
                    onMovementEnded: captureViewport()
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
                            sheet.moveSelection(event.key === Qt.Key_Down ? 1 : -1, (event.modifiers & Qt.ShiftModifier) !== 0); event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (sheet.selectedId) sheet.startEdit(sheet.selectedId); event.accepted = true
                        }
                    }
                    delegate: Rectangle {
                        id: row
                        required property int transactionId
                        required property string dateText
                        required property string payeeText
                        required property string categoryText
                        required property string withdrawalText
                        required property string depositText
                        required property string balanceText
                        required property bool isDraft
                        required property bool isSelected
                        required property bool isExpanded
                        required property string splitText
                        width: ledger.width
                        height: 48 + (details.visible ? details.implicitHeight + 20 : 0) + (editorLoader.active ? editorLoader.height : 0)
                        color: isSelected ? "#e0efeb" : "#ffffff"
                        Rectangle { width: 3; height: 48; color: row.isDraft ? "#ca8b31" : "transparent" }
                        MouseArea {
                            anchors.top: parent.top; width: parent.width; height: 48
                            enabled: !sheet.busy
                            onClicked: function(mouse) { ledger.forceActiveFocus(); sheet.select(row.transactionId, (mouse.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0, (mouse.modifiers & Qt.ShiftModifier) !== 0) }
                            onDoubleClicked: sheet.startEdit(row.transactionId)
                        }
                        Row {
                            x: 12; width: parent.width - 30; height: 48
                            Label { width: parent.width * 0.14; height: 48; verticalAlignment: Text.AlignVCenter; text: row.dateText; color: "#536771"; font.pixelSize: 12 }
                            Column {
                                width: parent.width * 0.24; anchors.verticalCenter: parent.verticalCenter
                                Label { width: parent.width - 8; text: row.payeeText; font.bold: row.isSelected; elide: Text.ElideRight; color: "#243b45" }
                                Label { text: row.isDraft ? "DRAFT · uncommitted" : "COMMITTED · fixture"; font.pixelSize: 9; color: row.isDraft ? "#94621a" : "#87969c" }
                            }
                            ToolButton { width: parent.width * 0.17; height: 48; text: (row.isExpanded ? "▾ " : "▸ ") + row.categoryText; onClicked: sheet.expand(row.transactionId); Accessible.name: "Expand allocations" }
                            Label { width: parent.width * 0.15; height: 48; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight; rightPadding: 14; text: row.withdrawalText; color: "#536771"; font.family: "Menlo"; font.pixelSize: 12 }
                            Label { width: parent.width * 0.14; height: 48; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight; rightPadding: 14; text: row.depositText; color: "#126d65"; font.family: "Menlo"; font.pixelSize: 12 }
                            Label { width: parent.width * 0.16; height: 48; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight; rightPadding: 4; text: row.balanceText; color: "#243b45"; font.family: "Menlo"; font.pixelSize: 12 }
                        }
                        Label {
                            id: details; x: 28; y: 58; width: parent.width - 50
                            visible: row.isExpanded; text: row.splitText; wrapMode: Text.WordWrap; color: "#52636a"; lineHeight: 1.6
                        }
                        Loader {
                            id: editorLoader
                            x: 10; y: 48 + (details.visible ? details.implicitHeight + 20 : 0); width: parent.width - 20
                            active: sheet.editingId === row.transactionId
                            sourceComponent: inlineEditor
                        }
                    }
                    Label { anchors.centerIn: parent; visible: ledger.count === 0 && !sheet.busy; text: "No matching transactions"; color: "#70818a" }
                }
                Rectangle {
                    Layout.fillWidth: true; height: 42; color: "#e9eef1"
                    Label { anchors.centerIn: parent; text: sheet.totals; color: "#243b45"; font.pixelSize: 12 }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; height: sheet.error ? 54 : 30; color: sheet.error ? "#fde8df" : "#ffffff"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                BusyIndicator { running: sheet.busy; visible: sheet.busy; implicitWidth: 24; implicitHeight: 24 }
                Label { Layout.fillWidth: true; text: sheet.error || (sheet.busy ? "Working in background…" : sheet.metrics); color: sheet.error ? "#9c3e26" : "#70818a"; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                Label { text: "Prototype  /  Session only"; color: "#70818a"; font.pixelSize: 11 }
            }
        }
    }
    Component {
        id: inlineEditor
        Rectangle {
            implicitHeight: editLayout.implicitHeight + 32
            color: "#edf5f2"; radius: 8; border.color: "#b4d1c8"
            ColumnLayout {
                id: editLayout
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                spacing: 10
                Label { text: "EDIT TRANSACTION  #" + sheet.editingId + (sheet.dirty ? "  ·  Unsaved" : ""); color: "#126d65"; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Payee" }
                    TextField { objectName: "editPayee"; Layout.fillWidth: true; text: sheet.edit.payee || ""; maximumLength: 200; enabled: !sheet.busy; onTextEdited: sheet.setField("payee",text); onAccepted: sheet.save() }
                    Label { text: "Signed total CAD" }
                    TextField { objectName: "editTotal"; Layout.preferredWidth: 135; text: sheet.edit.total || ""; maximumLength: 40; enabled: !sheet.busy; onTextEdited: sheet.setField("total",text); onAccepted: sheet.save() }
                }
                CheckBox { text: "Include a second allocation"; checked: sheet.edit.addAllocation || false; enabled: !sheet.busy && !sheet.edit.hasSplit; onToggled: sheet.setField("addAllocation", checked ? "true" : "false") }
                Label { text: "Allocation 1 is the remainder. Edit allocation 2 below; other amounts stay fixed."; color: "#52636a"; font.pixelSize: 11 }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { Layout.fillWidth: true; placeholderText: "Category"; text: sheet.edit.category || ""; maximumLength: 200; enabled: !sheet.busy && sheet.edit.addAllocation; onTextEdited: sheet.setField("category",text); onAccepted: sheet.save() }
                    TextField { Layout.fillWidth: true; placeholderText: "Allocation memo"; text: sheet.edit.memo || ""; maximumLength: 500; enabled: !sheet.busy && sheet.edit.addAllocation; onTextEdited: sheet.setField("memo",text); onAccepted: sheet.save() }
                    TextField { objectName: "editAllocation"; Layout.preferredWidth: 135; placeholderText: "Signed CAD"; text: sheet.edit.allocation || ""; maximumLength: 40; enabled: !sheet.busy && sheet.edit.addAllocation; onTextEdited: sheet.setField("allocation",text); onAccepted: sheet.save() }
                }
                RowLayout {
                    Label { text: "Note" }
                    TextField { Layout.fillWidth: true; text: sheet.edit.note || ""; maximumLength: 2000; enabled: !sheet.busy; onTextEdited: sheet.setField("note",text); onAccepted: sheet.save() }
                }
                RowLayout {
                    CheckBox { text: "Explicitly allow mixed signs"; checked: sheet.edit.mixed || false; enabled: !sheet.busy; onToggled: sheet.setMixed(checked) }
                    Item { Layout.fillWidth: true }
                    Label { text: "Original CAD " + (sheet.rowInfo(sheet.editingId).original || ""); color: "#52636a"; font.pixelSize: 11 }
                    Button { text: "Cancel"; enabled: !sheet.busy; onClicked: sheet.cancel() }
                    Button { objectName: "saveEdit"; text: "OK · Save session"; highlighted: true; enabled: !sheet.busy; onClicked: sheet.save() }
                }
            }
        }
    }
}
