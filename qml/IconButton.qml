// IconButton.qml - a tool button showing a drawn Icon, with an optional
// text beside it and a tooltip.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ToolButton {
    id: button
    property string iconKind
    property string tip
    property real iconSize: 18
    property color iconColour: enabled ? Material.foreground : Material.hintTextColor
    display: text.length > 0 ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
    padding: 6
    focusPolicy: Qt.TabFocus

    contentItem: RowLayout {
        spacing: 6
        Icon {
            Layout.alignment: Qt.AlignVCenter | (button.text.length > 0 ? Qt.AlignLeft : Qt.AlignHCenter)
            Layout.preferredWidth: button.iconSize
            Layout.preferredHeight: button.iconSize
            kind: button.iconKind
            colour: button.iconColour
        }
        Label {
            visible: button.text.length > 0
            text: button.text
            font: button.font
            color: button.iconColour
            elide: Text.ElideRight
            Layout.fillWidth: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.text: tip
    ToolTip.delay: 600
}
