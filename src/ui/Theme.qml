import QtQuick

// Semantic colours shared by the desktop workspaces and completion popups.
// Unknown system appearance uses the light palette.
QtObject {
    readonly property bool dark: Application.styleHints.colorScheme === Qt.Dark
    readonly property color window: dark ? "#171c22" : "#f6f7f9"
    readonly property color surface: dark ? "#222a33" : "#ffffff"
    readonly property color alternate: dark ? "#28323d" : "#f1f4f6"
    readonly property color row: dark ? "#222a33" : "#e6ebf0"
    readonly property color rowAlternate: dark ? "#28323d" : "#ffffff"
    readonly property color text: dark ? "#e7edf3" : "#18232d"
    readonly property color muted: dark ? "#acbac8" : "#52636a"
    readonly property color disabled: dark ? "#83909e" : "#707b85"
    readonly property color border: dark ? "#536373" : "#b8c0c7"
    readonly property color divider: dark ? "#36424e" : "#dce1e5"
    readonly property color button: dark ? "#303d4a" : "#e9edf0"
    readonly property color highlight: dark ? "#315f82" : "#2678c9"
    readonly property color highlightedText: "#ffffff"
    readonly property color selection: dark ? "#304f69" : "#c7e3fb"
    readonly property color selectionPressed: dark ? "#3b5c77" : "#afd3f2"
    readonly property color focus: dark ? "#79b9ed" : "#2870a8"
    readonly property color draft: dark ? "#3d3524" : "#fff6de"
    readonly property color draftAlternate: dark ? "#49402c" : "#fffdf2"
    readonly property color changed: dark ? "#463b24" : "#fff2cf"
    readonly property color duplicate: dark ? "#f0a1bf" : "#a13661"
    // Ledger attention. Teal is a payment shown twice. Indigo was merged. Green is linked.
    // These stay readable on the new-transaction yellow and are separate from warning and error.
    readonly property color transferDuplicate: dark ? "#7dccc6" : "#0b6e6a"
    readonly property color merged: dark ? "#d2c4f8" : "#5b3f9e"
    readonly property color linked: dark ? "#8fdfa8" : "#1f7a3a"
    readonly property color error: dark ? "#ff9999" : "#a02020"
    readonly property color warning: dark ? "#e9be70" : "#855100"
    readonly property color scrollThumb: dark ? "#718396" : "#94a3b0"
    readonly property color scrollHover: dark ? "#93a8bc" : "#73879a"
    readonly property color scrollPressed: dark ? "#b3c6d8" : "#52687b"
    readonly property color scrollTrack: dark ? "#26313c" : "#e9edf0"
}
