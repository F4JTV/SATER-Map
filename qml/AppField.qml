// AppField.qml - a text field whose placeholder is only an example.
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
    property string hint: ""
    placeholderText: length === 0 && !activeFocus ? hint : ""
    selectByMouse: true
}
