// AppField.qml - a text field whose placeholder is only an example, and
// which shows the beginning of a long text when it is not being edited.
//
// Since Qt 6.5 the Material style turns the placeholder into a label that
// floats over the top border as soon as the field has the focus or holds
// text; on the 30 px fields of the station table it lands on the frame,
// half cut, over the callsign being typed.  Every field here is already
// named by the label or the column header next to it: the hint shows in an
// empty field without the focus, and nowhere else.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

TextField {
    id: field
    property string hint: ""
    placeholderText: length === 0 && !activeFocus ? hint : ""
    selectByMouse: true
    // A text longer than the field shows its beginning, not its end (where
    // the cursor is left after the text was set), until it is edited.
    Component.onCompleted: if (!activeFocus) ensureVisible(0)
    onActiveFocusChanged: if (!activeFocus) { cursorPosition = 0; ensureVisible(0) }
    onTextChanged: if (!activeFocus) { cursorPosition = 0; ensureVisible(0) }
}
