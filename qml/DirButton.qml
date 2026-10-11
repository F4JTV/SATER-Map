// DirButton.qml - the N/S or E/W switch of a position, drawn like the
// number fields beside it and as tall as they are.
//
// Material's Button keeps 6 px free above and below its background
// (topInset and bottomInset, room for its shadow): in a row of 30 px fields
// the box came out 18 px tall.  The insets are 0 here, so the box fills the
// height the row gives it.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

AppButton {
    id: db
    property bool compact: true      // the height of a compact NumberField
    flat: true
    padding: 0
    sidePadding: 0
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    implicitWidth: 44
    implicitHeight: compact ? 30 : 44
    font.pixelSize: compact ? 13 : 16
    font.bold: true
    focusPolicy: Qt.TabFocus
    background: Rectangle {
        radius: 4
        color: db.down ? window.mc.edge : db.hovered ? Qt.lighter(window.mc.field, 1.08) : window.mc.field
        border.width: db.activeFocus ? 2 : 1
        border.color: db.activeFocus ? window.Material.accent : window.mc.fieldEdge
    }
}
