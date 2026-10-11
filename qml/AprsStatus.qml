// AprsStatus.qml - one APRS connection at a glance: a dot (green
// connected, amber connecting, red off with an error, grey off) and a name.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

RowLayout {
    id: status
    property string name
    property string state: "off"       // off, connecting, connected
    property string detail
    property bool showDetail: true
    spacing: 6
    Rectangle {
        Layout.preferredWidth: 10
        Layout.preferredHeight: 10
        radius: 5
        color: status.state === "connected" ? window.colourOf("clear")
             : status.state === "connecting" ? window.colourOf("busy")
             : status.detail.length > 0 ? window.colourOf("error") : window.colourOf("dim")
    }
    Label {
        text: status.name
        font.bold: true
        font.pixelSize: 12
    }
    Label {
        visible: status.showDetail
        Layout.fillWidth: true
        text: status.state === "off" && status.detail.length === 0 ? qsTr("off") : status.detail
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.pixelSize: 12
        color: window.colourOf("dim")
    }
}
