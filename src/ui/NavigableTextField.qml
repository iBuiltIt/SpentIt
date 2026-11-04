import QtQuick
import QtQuick.Controls

TextField {
    id: field

    // Forward first so derived fields can retain their own Keys.onPressed
    // handlers (completion, Escape and session Undo).
    Keys.forwardTo: [boundaryKeys]
    Item {
        id: boundaryKeys
        Keys.onPressed: function(event) {
            if ((event.key !== Qt.Key_Home && event.key !== Qt.Key_End)
                    || (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)))
                return
            const position = event.key === Qt.Key_Home ? 0 : field.length
            if (event.modifiers & Qt.ShiftModifier)
                field.moveCursorSelection(position, TextInput.SelectCharacters)
            else
                field.cursorPosition = position
            event.accepted = true
        }
    }
}
