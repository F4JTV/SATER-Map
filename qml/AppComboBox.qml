// AppComboBox.qml - a drop-down list drawn as the entry fields are (a
// rounded box), the same on every renderer; Material's own box is a layer
// effect the software scene graph does not draw.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ComboBox {
    id: control
    property bool compact: false
    implicitHeight: compact ? 30 : (app.android ? 44 : 34)
    topInset: 0
    bottomInset: 0
    leftPadding: compact ? 6 : 10
    // The choice shown cut with "…" when longer than the box (Material's
    // own is a text field, which clips it: "Géographique (carte," on a
    // phone in French).
    contentItem: Label {
        text: control.displayText
        font: control.font
        color: control.enabled ? control.Material.foreground : control.Material.hintTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 4
        color: window.mc.field
        border.width: control.visualFocus || control.down ? 2 : 1
        border.color: control.visualFocus || control.down ? window.Material.accent : window.mc.fieldEdge
    }
    popup.background: Rectangle {
        radius: 4
        color: window.mc.card
        border.width: 1
        border.color: window.mc.edge
    }
}
