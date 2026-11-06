import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtCore

// Sample or manual reading. This window does not change a file, a selection,
// or a decision dialog. While a decision dialog is open, modalOwner is that
// dialog so Help can be used without leaving the dialog's protection.
Window {
    id: root
    objectName: "helpWindow"
    required property var catalog
    property var mainWindow: null
    property var modalOwner: null
    signal helpClosed()
    title: "SpentIt Help"
    width: 880
    height: 680
    minimumWidth: 480
    minimumHeight: 360
    modality: Qt.NonModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    color: theme.window
    Theme { id: theme }
    palette.window: theme.window
    palette.windowText: theme.text
    palette.base: theme.surface
    palette.text: theme.text
    palette.button: theme.button
    palette.buttonText: theme.text
    palette.highlight: theme.highlight
    palette.highlightedText: theme.highlightedText

    property string topicId: ""
    property string currentSection: ""
    property string linkMessage: ""
    property string query: ""
    property var results: []
    property var history: []
    property int historyIndex: -1
    property var scrollPositions: ({})
    property bool contentsOpen: false
    property real textScale: 1
    property int findOccurrence: -1
    property int findCount: 0
    property string matchStatus: ""
    property real savedContentY: 0
    readonly property bool narrow: width < 720

    Settings {
        id: settings
        category: "help"
        property real textScale: 1
        property int windowWidth: 880
        property int windowHeight: 680
    }

    function owner() { return modalOwner || mainWindow }
    function syncOwner() {
        // A decision dialog stays application-modal. Help is its transient child
        // only while that dialog is open, then returns to the main window before
        // the dialog hides so closing the dialog does not close Help.
        const next = owner() || null
        transientParent = next
        imageWindow.transientParent = next
    }
    onModalOwnerChanged: syncOwner()

    function clampToScreen() {
        const area = screen ? screen.availableGeometry : null
        if (!area) return
        width = Math.max(minimumWidth, Math.min(width, area.width))
        height = Math.max(minimumHeight, Math.min(height, area.height))
        if (x < area.x || x + width > area.x + area.width)
            x = area.x + Math.max(0, (area.width - width) / 2)
        if (y < area.y || y + height > area.y + area.height)
            y = area.y + Math.max(0, (area.height - height) / 2)
    }
    Component.onCompleted: {
        textScale = Math.max(0.8, Math.min(1.8, settings.textScale))
        width = settings.windowWidth
        height = settings.windowHeight
        clampToScreen()
        ensureTopic()
    }
    // close() can be ignored while this window is a transient child of an
    // application-modal dialog. Hiding still leaves that dialog in place.
    property bool helpHiding: false
    function dismiss() {
        if (helpHiding) return
        helpHiding = true
        if (imageWindow.visible) imageWindow.dismiss()
        // A transient child of an application-modal dialog can ignore close()
        // and a plain hide. Drop that relationship first, then hide. The dialog
        // itself stays open.
        transientParent = null
        visible = false
        helpHiding = false
    }
    function rememberPlacement() {
        settings.textScale = textScale
        settings.windowWidth = Math.round(width)
        settings.windowHeight = Math.round(height)
    }
    onClosing: function(close) {
        close.accepted = true
        dismiss()
    }
    onVisibleChanged: {
        if (visible) ensureTopic()
        else { rememberPlacement(); helpClosed() }
    }
    Shortcut {
        objectName: "helpEscape"
        sequences: ["Escape"]
        context: Qt.WindowShortcut
        enabled: !imageWindow.visible
        onActivated: root.dismiss()
    }
    Shortcut { sequences: [StandardKey.Find]; context: Qt.WindowShortcut; onActivated: findField.forceActiveFocus() }

    function topicIndex() {
        if (!catalog) return -1
        for (let i = 0; i < catalog.topics.length; ++i)
            if (catalog.topics[i].id === topicId) return i
        return -1
    }
    function ensureTopic() {
        if (!catalog || !catalog.loaded || history.length > 0) return
        if (catalog.topics.length === 0) return
        history = [catalog.topics[0].id]
        historyIndex = 0
        showTopic(catalog.topics[0].id, false)
    }
    function rememberScroll() {
        if (!topicId) return
        let next = Object.assign({}, scrollPositions)
        next[topicId] = articleFlick.contentY
        scrollPositions = next
    }
    function showTopic(id, recordHistory) {
        if (!catalog) return
        const topic = catalog.topic(id)
        if (!topic || !topic.id) { linkMessage = "That link is not part of Help."; return }
        rememberScroll()
        if (recordHistory) {
            let trimmed = history.slice(0, historyIndex + 1)
            trimmed.push(id)
            history = trimmed
            historyIndex = trimmed.length - 1
        }
        topicId = id
        currentSection = ""
        linkMessage = ""
        findOccurrence = -1
        findCount = 0
        matchStatus = ""
        catalog.forgetMarks()
        article.text = topic.markdown
        // Run after setText: Qt chooses its viewport optimization there.
        catalog.prepareArticleRendering(article, article.textDocument)
        Qt.callLater(restoreScroll)
    }
    function restoreScroll() {
        const saved = scrollPositions[topicId]
        articleFlick.contentY = saved === undefined ? 0 : saved
    }
    function back() { if (historyIndex > 0) { historyIndex -= 1; showTopic(history[historyIndex], false) } }
    function forward() { if (historyIndex >= 0 && historyIndex < history.length - 1) { historyIndex += 1; showTopic(history[historyIndex], false) } }
    function home() { if (catalog && catalog.topics.length > 0) showTopic(catalog.topics[0].id, true) }
    function previousTopic() { const index = topicIndex(); if (index > 0) showTopic(catalog.topics[index - 1].id, true) }
    function nextTopic() { const index = topicIndex(); if (catalog && index >= 0 && index < catalog.topics.length - 1) showTopic(catalog.topics[index + 1].id, true) }
    function revealSection(title) {
        currentSection = title || ""
        const position = catalog.sectionPosition(article.textDocument, title)
        if (position < 0) return
        article.cursorPosition = position
        const rect = article.cursorRectangle
        if (rect.y < articleFlick.contentY || rect.y + 24 > articleFlick.contentY + articleFlick.height)
            articleFlick.contentY = Math.max(0, rect.y - 24)
    }
    function followLink(url) {
        const result = catalog.resolveLink(topicId, url)
        if (!result.ok) { linkMessage = result.message; return }
        linkMessage = ""
        if (result.kind === "image") { openImage(result); return }
        if (result.topicId !== topicId) showTopic(result.topicId, true)
        if (result.sectionId) Qt.callLater(function() { revealSection(result.title) })
    }
    function openImage(result) {
        savedContentY = articleFlick.contentY
        imageError = ""
        imageAlt = result.alt || "Picture"
        imageZoom = 1
        image.source = result.source
        imageWindow.title = imageAlt
        imageWindow.transientParent = owner()
        imageWindow.show()
        imageWindow.requestActivate()
        Qt.callLater(fitImage)
    }
    function findStep(step) {
        const term = findField.text
        if (!term) { matchStatus = ""; return }
        const start = findOccurrence < 0 ? 0 : findOccurrence + step
        const marked = catalog.markMatch(article.textDocument, term, start, theme.selection)
        findCount = marked.count
        findOccurrence = marked.occurrence
        if (marked.count === 0) { matchStatus = "No matches in this topic."; return }
        matchStatus = "Match " + (marked.occurrence + 1) + " of " + marked.count
        if (marked.position < 0) return
        article.cursorPosition = marked.position
        const rect = article.cursorRectangle
        if (rect.y < articleFlick.contentY || rect.y > articleFlick.contentY + articleFlick.height - 24)
            articleFlick.contentY = Math.max(0, rect.y - 24)
    }
    function changeTextScale(next) {
        textScale = Math.max(0.8, Math.min(1.8, next))
    }
    function toggleContents() {
        contentsOpen = !contentsOpen
        if (contentsOpen) searchField.forceActiveFocus()
        else article.forceActiveFocus()
    }
    function escapeText(value) {
        return String(value).replace(/&/g, "&amp;").replace(/</g, "&lt;")
    }

    component HelpButton: Button {
        id: control
        font.pixelSize: 13
        focusPolicy: Qt.StrongFocus
        background: Rectangle {
            color: control.down ? theme.selectionPressed : theme.button
            border.color: control.activeFocus ? theme.focus : theme.border
            border.width: control.activeFocus ? 2 : 1
            radius: 3
        }
        contentItem: Text {
            text: control.text
            color: control.enabled ? theme.text : theme.disabled
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        // Escape reaches the focused control even when this window is not the
        // key window. Close the picture first. A decision dialog keeps its own
        // Escape, which means Stay.
        Keys.onEscapePressed: imageWindow.visible ? imageWindow.dismiss() : root.dismiss()
        Flow {
            Layout.fillWidth: true
            spacing: 6
            HelpButton { objectName: "helpContentsButton"; text: "Contents"; visible: root.narrow; onClicked: root.toggleContents(); Accessible.name: "Contents" }
            HelpButton { objectName: "helpBack"; text: "Back"; enabled: root.historyIndex > 0; onClicked: root.back(); Accessible.name: "Back" }
            HelpButton { objectName: "helpForward"; text: "Forward"; enabled: root.historyIndex >= 0 && root.historyIndex < root.history.length - 1; onClicked: root.forward(); Accessible.name: "Forward" }
            HelpButton { objectName: "helpHome"; text: "Home"; onClicked: root.home(); Accessible.name: "Home" }
            HelpButton { objectName: "helpPrevious"; text: "Previous topic"; enabled: root.topicIndex() > 0; onClicked: root.previousTopic(); Accessible.name: "Previous topic" }
            HelpButton { objectName: "helpNext"; text: "Next topic"; enabled: root.catalog && root.topicIndex() >= 0 && root.topicIndex() < root.catalog.topics.length - 1; onClicked: root.nextTopic(); Accessible.name: "Next topic" }
            HelpButton { objectName: "helpTextSmaller"; text: "Smaller text"; onClicked: root.changeTextScale(root.textScale - 0.1); Accessible.name: "Smaller text" }
            HelpButton { objectName: "helpTextLarger"; text: "Larger text"; onClicked: root.changeTextScale(root.textScale + 0.1); Accessible.name: "Larger text" }
            HelpButton { objectName: "helpTextReset"; text: "Reset text size"; onClicked: root.changeTextScale(1); Accessible.name: "Reset text size" }
        }
        Label {
            objectName: "helpBanner"
            visible: root.catalog && root.catalog.sample && root.catalog.banner.length > 0
            text: root.catalog ? root.catalog.banner : ""
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: theme.text
            Accessible.name: text
        }
        Label {
            objectName: "helpLinkMessage"
            visible: text.length > 0
            text: root.linkMessage
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: theme.error
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                id: contentsPane
                objectName: "helpContentsPane"
                visible: !root.narrow || root.contentsOpen
                z: root.narrow ? 2 : 0
                width: root.narrow ? Math.min(300, parent.width - 8) : 260
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                color: theme.surface
                border.color: theme.border
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    TextField {
                        id: searchField
                        objectName: "helpSearch"
                        placeholderText: "Search Help"
                        Accessible.name: "Search Help"
                        Layout.fillWidth: true
                        selectByMouse: true
                        onTextChanged: root.query = text
                        onAccepted: if (root.results.length > 0) root.showTopic(root.results[0].topicId, true)
                    }
                    Label {
                        objectName: "helpNoMatches"
                        visible: root.query.trim().length > 0 && root.results.length === 0
                        text: "No matches. Change the search or choose a topic."
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        color: theme.text
                    }
                    ListView {
                        objectName: "helpContents"
                        visible: root.query.trim().length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.catalog ? root.catalog.topics : []
                        delegate: ItemDelegate {
                            id: topicDelegate
                            width: ListView.view.width
                            palette.highlightedText: theme.text
                            background: Rectangle {
                                color: topicDelegate.highlighted ? theme.selection : topicDelegate.hovered ? theme.alternate : "transparent"
                                border.color: topicDelegate.activeFocus ? theme.focus : "transparent"
                            }
                            text: modelData.title
                            highlighted: modelData.id === root.topicId
                            objectName: "helpTopic_" + modelData.id
                            Accessible.name: modelData.title
                            onClicked: { root.showTopic(modelData.id, true); if (root.narrow) root.contentsOpen = false }
                        }
                    }
                    ListView {
                        objectName: "helpSearchResults"
                        visible: root.query.trim().length > 0 && root.results.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.results
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            objectName: "helpResult_" + modelData.topicId
                            Accessible.name: modelData.title
                            onClicked: { root.showTopic(modelData.topicId, true); if (root.narrow) root.contentsOpen = false }
                            contentItem: Column {
                                spacing: 2
                                Label { text: modelData.title; color: theme.text; font.bold: true; width: parent.width; wrapMode: Text.Wrap }
                                TextEdit {
                                    width: parent.width
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    color: theme.text
                                    textFormat: TextEdit.RichText
                                    text: root.escapeText(modelData.before) + "<b>" + root.escapeText(modelData.match) + "</b>" + root.escapeText(modelData.after)
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.left: root.narrow ? parent.left : contentsPane.right
                anchors.leftMargin: root.narrow ? 0 : 12
                spacing: 8
                Flickable {
                    id: articleFlick
                    objectName: "helpArticleFlick"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: width
                    contentHeight: article.contentHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    TextEdit {
                        id: article
                        objectName: "helpArticle"
                        // Keep a reading line short enough to take in one piece,
                        // even when the window is wide. Qt Markdown also leaves
                        // paragraph margins at zero; relaxReading puts them back.
                        width: Math.min(articleFlick.width, Math.max(240, Math.round(font.pixelSize * 36)))
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.MarkdownText
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: Math.round(16 * root.textScale)
                        color: theme.text
                        selectionColor: theme.highlight
                        selectedTextColor: theme.highlightedText
                        Accessible.name: "Help article"
                        activeFocusOnTab: true
                        onLinkActivated: function(link) { root.followLink(link) }
                        onWidthChanged: if (root.catalog && length > 0) root.catalog.relaxReading(textDocument, width)
                        onTextChanged: if (root.catalog) root.catalog.relaxReading(textDocument, width)
                        onFontChanged: if (root.catalog && length > 0) root.catalog.relaxReading(textDocument, width)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: findField
                        objectName: "helpFind"
                        placeholderText: "Find in this topic"
                        Accessible.name: "Find in this topic"
                        Layout.fillWidth: true
                        selectByMouse: true
                        onAccepted: root.findStep(1)
                    }
                    HelpButton { objectName: "helpFindPrevious"; text: "Previous match"; onClicked: root.findStep(-1); Accessible.name: "Previous match" }
                    HelpButton { objectName: "helpFindNext"; text: "Next match"; onClicked: root.findStep(1); Accessible.name: "Next match" }
                }
                Label {
                    objectName: "helpMatchStatus"
                    visible: text.length > 0
                    text: root.matchStatus
                    color: theme.text
                    Layout.fillWidth: true
                }
            }
        }
    }

    property string imageAlt: ""
    property string imageError: ""
    property real imageZoom: 1
    function fitImage() {
        if (image.sourceSize.width <= 0 || imageFlick.width <= 0 || imageFlick.height <= 0) return
        imageZoom = Math.min(imageFlick.width / image.sourceSize.width, imageFlick.height / image.sourceSize.height)
        imageFlick.contentX = 0
        imageFlick.contentY = 0
    }
    function actualImage() { imageZoom = 1 }
    Window {
        id: imageWindow
        objectName: "helpImageWindow"
        width: 720
        height: 520
        minimumWidth: 320
        minimumHeight: 240
        modality: Qt.NonModal
        color: theme.window
        title: root.imageAlt || "Picture"
        property bool imageHiding: false
        function dismiss() {
            if (imageHiding) return
            imageHiding = true
            transientParent = null
            visible = false
            imageHiding = false
        }
        Shortcut { sequences: ["Escape"]; context: Qt.WindowShortcut; onActivated: imageWindow.dismiss() }
        onClosing: function(close) { close.accepted = true; imageWindow.dismiss() }
        onVisibleChanged: if (!visible) {
            articleFlick.contentY = root.savedContentY
            article.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            Keys.onEscapePressed: imageWindow.dismiss()
            Flickable {
                id: imageFlick
                objectName: "helpImageFlick"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: Math.max(width, image.width)
                contentHeight: Math.max(height, image.height)
                boundsBehavior: Flickable.StopAtBounds
                focus: true
                Accessible.name: "Picture view"
                Keys.onEscapePressed: imageWindow.dismiss()
                Keys.onLeftPressed: contentX = Math.max(0, contentX - 40)
                Keys.onRightPressed: contentX = Math.min(contentWidth - width, contentX + 40)
                Keys.onUpPressed: contentY = Math.max(0, contentY - 40)
                Keys.onDownPressed: contentY = Math.min(contentHeight - height, contentY + 40)
                Image {
                    id: image
                    objectName: "helpImage"
                    width: Math.max(1, sourceSize.width * root.imageZoom)
                    height: Math.max(1, sourceSize.height * root.imageZoom)
                    fillMode: Image.PreserveAspectFit
                    Accessible.name: root.imageAlt
                    onStatusChanged: root.imageError = status === Image.Error ? "This picture could not be opened." : ""
                }
            }
            Label { visible: root.imageError.length > 0; text: root.imageError; color: theme.error; wrapMode: Text.Wrap; Layout.fillWidth: true; objectName: "helpImageError" }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                HelpButton { objectName: "helpImageFit"; text: "Fit"; onClicked: root.fitImage(); Accessible.name: "Fit" }
                HelpButton { objectName: "helpImageActual"; text: "Actual size"; onClicked: root.actualImage(); Accessible.name: "Actual size" }
                HelpButton { objectName: "helpImageZoomIn"; text: "Zoom in"; onClicked: root.imageZoom = Math.min(8, root.imageZoom * 1.25); Accessible.name: "Zoom in" }
                HelpButton { objectName: "helpImageZoomOut"; text: "Zoom out"; onClicked: root.imageZoom = Math.max(0.1, root.imageZoom / 1.25); Accessible.name: "Zoom out" }
                HelpButton { objectName: "helpImageClose"; text: "Close"; onClicked: imageWindow.dismiss(); Accessible.name: "Close picture" }
            }
        }
    }

    onQueryChanged: results = catalog ? catalog.search(query) : []
}
