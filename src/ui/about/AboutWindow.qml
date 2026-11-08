import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: about
    objectName: "aboutWindow"
    required property var information
    required property var mainWindow
    property int page: 0
    property var returnItem: null
    property bool copied: false
    signal dismissed()
    transientParent: mainWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    title: page === 0 ? "About SpentIt" : page === 1 ? "License and terms" : "Third-party software"
    width: page === 0 ? 500 : 780
    height: page === 0 ? minimumHeight : 640
    minimumWidth: 440
    minimumHeight: page === 0 ? Math.max(450, mainContent.implicitHeight + footer.implicitHeight + 64) : 400
    palette: mainWindow.palette
    color: palette.window
    visible: false

    function present(item) {
        if (!visible) {
            returnItem = item
            page = 0
            copied = false
            x = mainWindow.x + (mainWindow.width - width) / 2
            y = mainWindow.y + (mainWindow.height - height) / 2
        }
        show()
        raise()
        requestActivate()
        closeButton.forceActiveFocus()
    }
    function back() {
        const previous = page
        page = 0
        Qt.callLater(function() {
            if (previous === 1) licenseButton.forceActiveFocus()
            else noticesButton.forceActiveFocus()
        })
    }
    function showPage(next) {
        page = next
        Qt.callLater(function() {
            if (next === 2) components.forceActiveFocus()
            else licenseText.forceActiveFocus()
        })
    }
    onClosing: dismissed()
    Shortcut {
        sequence: "Escape"
        onActivated: { if (about.page === 0) about.close(); else about.back() }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16
        ColumnLayout {
            id: mainContent
            visible: about.page === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/spentit/images/SpentIt_" + (Qt.styleHints.colorScheme === Qt.ColorScheme.Dark ? "dark" : "light") + "_256.png"
                sourceSize: Qt.size(256, 256)
                Layout.preferredWidth: 80; Layout.preferredHeight: 80
                Accessible.ignored: true
            }
            Label { text: "SpentIt"; font.pixelSize: 28; font.bold: true; Layout.alignment: Qt.AlignHCenter }
            Label { text: "Where did my money go?"; Layout.alignment: Qt.AlignHCenter }
            Label {
                objectName: "aboutVersion"
                text: "Version " + information.version + " · Build " + information.build
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
            }
            Button {
                objectName: "copyVersionInformation"
                text: about.copied ? "Copied" : "Copy version information"
                Layout.alignment: Qt.AlignHCenter
                onClicked: { information.copyVersionInformation(); about.copied = true }
            }
            Label { text: "© 2026 Stefan Wessels"; Layout.alignment: Qt.AlignHCenter }
            Label {
                text: "Provided as is, without warranty. See License and terms."
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
            }
            Label {
                text: "Built with Qt, used under LGPL v3."
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Button { id: licenseButton; objectName: "aboutLicense"; text: "License and terms…"; onClicked: about.showPage(1) }
                Button { id: noticesButton; objectName: "aboutNotices"; text: "Third-party software…"; onClicked: about.showPage(2) }
            }
        }
        Label {
            visible: about.page !== 0
            text: about.title; font.bold: true; font.pixelSize: 20
            Layout.fillWidth: true; wrapMode: Text.Wrap
        }
        ComboBox {
            id: components; objectName: "aboutComponents"
            visible: about.page === 2
            Layout.fillWidth: true
            model: information.components
            textRole: "label"
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: components.width
                text: modelData.label
                font: components.font
                topPadding: 4; bottomPadding: 4
                implicitHeight: Math.max(28, contentItem.implicitHeight + topPadding + bottomPadding)
                highlighted: components.highlightedIndex === index
            }
            Accessible.name: "Third-party component"
            onCurrentIndexChanged: noticeScroll.contentItem.contentY = 0
        }
        ScrollView {
            id: noticeScroll
            visible: about.page !== 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            TextArea {
                id: licenseText; objectName: "aboutNoticeText"
                text: about.page === 1 ? information.terms : ((components.model[components.currentIndex] || {}).text || "")
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                readOnly: true; selectByMouse: true; activeFocusOnTab: true
                Accessible.name: about.page === 1 ? "License and terms" : "Third-party notice"
                background: Rectangle { color: about.palette.base; border.color: licenseText.activeFocus ? about.palette.highlight : about.palette.mid }
                onTextChanged: { cursorPosition = 0; noticeScroll.contentItem.contentY = 0 }
            }
        }
        RowLayout {
            id: footer
            Layout.fillWidth: true
            Button { objectName: "aboutBack"; text: "Back"; visible: about.page !== 0; onClicked: about.back() }
            Item { Layout.fillWidth: true }
            Button { id: closeButton; objectName: "aboutClose"; text: "Close"; onClicked: about.close() }
        }
    }
}
