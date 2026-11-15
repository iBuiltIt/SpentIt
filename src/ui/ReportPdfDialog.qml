import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    objectName: "reportPdfDialog"
    required property var workflow
    readonly property var pdfState: workflow.state.reportPdf || {}
    property bool ready: false
    property string destination: ""
    property string failure: ""
    property bool saved: false
    title: "Save report as PDF"
    modal: true
    anchors.centerIn: parent
    width: Math.min(960, parent.width - 32)
    height: Math.min(860, parent.height - 32)
    closePolicy: workflow.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    function rebuild() {
        ready = false; saved = false; failure = ""
        workflow.command("report_pdf_layout", {paper: paper.currentText, orientation: orientation.currentText, layout: layout.currentText})
    }
    function start() {
        destination = ""; paper.currentIndex = 0; orientation.currentIndex = 0; layout.currentIndex = 0
        open(); rebuild()
    }
    function save(overwrite) {
        failure = ""
        workflow.command("report_pdf_save", {path: destination, overwrite: overwrite})
    }
    onClosed: {
        ready = false
        if (!workflow.busy) workflow.command("report_pdf_clear")
    }
    Connections {
        target: root.workflow
        function onCompleted(action, success) {
            if (!root.visible || !action.startsWith("report_pdf_")) return
            root.failure = success ? "" : root.workflow.error
            if (action === "report_pdf_layout" && success) {
                root.ready = true
                if (!root.destination) root.destination = root.pdfState.path
            }
            if (action === "report_pdf_save" && success) root.saved = true
        }
    }
    FileDialog {
        id: browse
        objectName: "reportPdfBrowseDialog"
        title: "Choose PDF destination"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PDF files (*.pdf)"]
        defaultSuffix: "pdf"
        // Replace is confirmed when Save is pressed in the preview dialog.
        options: FileDialog.DontConfirmOverwrite
        onAccepted: { root.destination = root.workflow.localPath(selectedFile); root.saved = false }
    }
    Dialog {
        id: replace
        objectName: "reportPdfReplaceDialog"
        title: "Replace PDF?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(500, root.width - 40)
        contentItem: ColumnLayout {
            Label { text: "Replace the existing file?\n" + root.destination; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Cancel"; onClicked: replace.close() }
                Button { objectName: "reportPdfReplace"; text: "Replace"; onClicked: { replace.close(); root.save(true) } }
            }
        }
    }
    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            enabled: !root.workflow.busy
            Label { text: "Paper" }
            ComboBox { id: paper; objectName: "reportPdfPaper"; model: ["Letter", "A4"]; onActivated: root.rebuild() }
            Label { text: "Orientation" }
            ComboBox { id: orientation; objectName: "reportPdfOrientation"; model: ["Automatic", "Portrait", "Landscape"]; onActivated: root.rebuild() }
            ComboBox { id: layout; objectName: "reportPdfLayout"; model: ["Readable pages", "Fit on one page"]; Layout.fillWidth: true; onActivated: root.rebuild() }
        }
        Rectangle {
            color: "#cbd0d5"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Image {
                objectName: "reportPdfPreview"
                anchors.fill: parent
                anchors.margins: 8
                source: root.ready ? root.pdfState.image || "" : ""
                fillMode: Image.PreserveAspectFit
                cache: false
                Accessible.role: Accessible.Graphic
                Accessible.name: "Report page preview"
            }
            BusyIndicator { anchors.centerIn: parent; running: root.workflow.busy }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Button { objectName: "reportPdfPrevious"; text: "Previous page"; enabled: root.ready && !root.workflow.busy && root.pdfState.page > 0; onClicked: root.workflow.command("report_pdf_page", {page: root.pdfState.page - 1}) }
            Label { objectName: "reportPdfPageCount"; text: root.ready ? "Page " + (root.pdfState.page + 1) + " of " + root.pdfState.count : "Preparing preview…" }
            Button { objectName: "reportPdfNext"; text: "Next page"; enabled: root.ready && !root.workflow.busy && root.pdfState.page + 1 < root.pdfState.count; onClicked: root.workflow.command("report_pdf_page", {page: root.pdfState.page + 1}) }
        }
        Label { visible: root.ready && !!root.pdfState.smallText; text: "Fitting this report on one page makes the text small. Choose Readable pages for larger text."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Label { text: "Save to" }
            TextField { objectName: "reportPdfDestination"; text: root.destination; readOnly: true; selectByMouse: true; Layout.fillWidth: true }
            Button { objectName: "reportPdfBrowse"; text: "Browse…"; enabled: !root.workflow.busy && root.ready; onClicked: { browse.selectedFile = root.workflow.fileUrl(root.destination); browse.open() } }
        }
        Label { text: "The PDF is not password-protected."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { objectName: "reportPdfFeedback"; text: root.failure || (root.saved ? "PDF saved." : ""); visible: text.length > 0; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: root.workflow.busy ? "Cancel operation" : root.saved ? "Close" : "Cancel"; enabled: !root.workflow.busy || root.workflow.canCancel; onClicked: { if (root.workflow.busy) root.workflow.requestCancel(); else root.close() } }
            Button { text: "Preview"; visible: !root.ready && !root.workflow.busy; onClicked: root.rebuild() }
            Button { objectName: "reportPdfSave"; text: "Save"; enabled: root.ready && !root.workflow.busy && !root.saved; onClicked: { if (root.workflow.pdfFileExists(root.destination)) replace.open(); else root.save(false) } }
        }
    }
}
