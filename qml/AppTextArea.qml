// AppTextArea.qml - several lines of text in a box drawn like the fields.
//
// Material 3 (Qt 6.5 and later) reserves room above an outlined text area
// for its floating label: its own frame starts below the top inset while
// the text starts higher, so a frame drawn here (the theme's field colour)
// cut through the first line, and its placeholder, which places itself,
// sat on the frame.  Insets and padding are set here, the same on every
// Qt, and the example in the empty box is a label of our own where the
// text will start (see AppField.qml for why only in the empty box).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

TextArea {
    id: area
    property string hint: ""
    wrapMode: TextEdit.Wrap
    selectByMouse: true
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    topPadding: 8
    bottomPadding: 8
    leftPadding: 10
    rightPadding: 10
    background: Rectangle {
        radius: 4
        color: window.mc.field
        border.width: area.activeFocus ? 2 : 1
        border.color: area.activeFocus ? window.Material.accent : window.mc.fieldEdge
        Label {
            objectName: "textAreaHint"
            visible: area.length === 0 && !area.activeFocus
            x: area.leftPadding
            y: area.topPadding
            width: parent.width - area.leftPadding - area.rightPadding
            text: area.hint
            font: area.font
            color: area.Material.hintTextColor
            elide: Text.ElideRight
        }
    }
}
