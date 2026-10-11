// Card.qml - a raised panel for one group of controls, as AX25Chat's
// preference sections (the Python version's group boxes).  "title" puts a
// heading above the content; the content goes in the card's column.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: card
    property string title
    property real cardRadius: 10
    default property alias content: column.data
    padding: 12
    topPadding: title.length > 0 ? 8 : 12
    Material.elevation: window.flat ? 0 : 2
    Material.background: window.mc.card
    background: Rectangle {
        color: window.mc.card
        radius: card.cardRadius
        border.width: window.flat ? 1 : 0
        border.color: window.mc.edge
    }

    // The only child: the card takes its implicit size, and a card
    // stretched by its layout (the station table) stretches it too.
    ColumnLayout {
        id: column
        anchors.fill: parent
        spacing: 8
        Label {
            visible: card.title.length > 0
            text: card.title
            font.bold: true
            font.pixelSize: 14
            color: window.Material.accent
            Layout.fillWidth: true
        }
    }
}
