// ColorButton.qml - a swatch that opens the colour picker (the Python
// version's coloured QPushButton + QColorDialog).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

AbstractButton {
    id: button
    property color colour: "#e74c3c"
    property string tip
    signal chosen(string colour)
    implicitWidth: 28
    implicitHeight: 24
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: 4
        color: button.colour
        border.width: button.activeFocus || button.hovered ? 2 : 1
        border.color: button.activeFocus ? window.Material.accent : "#666666"
    }
    onClicked: window.chooseColor(colour, function(c) { button.chosen(c) })

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.text: tip
    ToolTip.delay: 600
}
