// AprsLogDialog.qml - the APRS packets and events, newest first: the
// bearings taken in, those left out (not in the operation), those sent.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    objectName: "aprsLogDialog"
    title: qsTr("APRS packet log")
    preferredWidth: 760
    fillHeight: true
    standardButtons: Dialog.Close

    function kindText(kind) {
        switch (kind) {
        case "bearing": return qsTr("Bearing")
        case "ignored": return qsTr("Left out")
        case "sent": return qsTr("Sent")
        case "note": return qsTr("Note")
        default: return qsTr("Other")
        }
    }
    function kindColour(kind) {
        switch (kind) {
        case "bearing": return window.colourOf("clear")
        case "sent": return window.Material.accent
        case "ignored": return window.colourOf("busy")
        case "note": return window.colourOf("link")
        default: return window.colourOf("dim")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("%1 bearing(s) taken in").arg(mission.aprs.bearingsReceived)
                color: window.colourOf("dim")
            }
            AppButton { text: qsTr("Clear"); flat: true; onClicked: mission.aprs.clearLog() }
        }
        ListView {
            id: list
            objectName: "aprsLogList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: mission.aprs.log
            ScrollBar.vertical: ScrollBar {}
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Nothing received yet.")
                color: window.colourOf("dim")
            }
            delegate: ColumnLayout {
                required property var modelData
                width: list.width - 12
                spacing: 0
                RowLayout {
                    spacing: 8
                    Label { text: modelData.time; font.family: "monospace"; font.pixelSize: 12; color: window.colourOf("dim") }
                    Label { text: dialog.kindText(modelData.kind); font.bold: true; font.pixelSize: 12; color: dialog.kindColour(modelData.kind) }
                }
                Label {
                    Layout.fillWidth: true
                    text: modelData.text
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    font.family: "monospace"
                    font.pixelSize: 12
                }
            }
        }
    }
}
