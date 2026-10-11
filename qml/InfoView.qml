// InfoView.qml - the Python "Information" box: the best estimate (least
// squares, with its 95 % ellipse and any disagreement between the bearings),
// the intersection zone, their MGRS references, the history and kilometre
// buttons, the counts.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Card {
    id: info
    title: qsTr("Information")
    property bool big: false

    // The least-squares estimate (Fix.h), then the intersection zone of
    // the Python version.
    Label {
        visible: mission.showEstimate
        text: qsTr("Best estimate")
        font.pixelSize: info.big ? 13 : 11
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: mission.estimateColor
    }
    Label {
        objectName: "fixLabel"
        visible: mission.showEstimate
        Layout.fillWidth: true
        text: mission.fixText
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: info.big ? 15 : 13
    }
    Label {
        objectName: "fixMgrsLabel"
        visible: mission.fixShown
        Layout.fillWidth: true
        text: "MGRS: " + mission.fixMgrs
        font.family: "monospace"
        font.pixelSize: info.big ? 15 : 13
        font.bold: true
        color: window.colourOf("link")
        TapHandler {
            onLongPressed: { app.copyToClipboard(mission.fixMgrs); window.showMessage(qsTr("MGRS copied")) }
        }
    }
    Label {
        objectName: "fixWarning"
        visible: mission.showEstimate && mission.fixWarning.length > 0
        Layout.fillWidth: true
        text: mission.fixWarning
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: info.big ? 14 : 12
        font.bold: true
        color: window.colourOf("error")
    }
    Label {
        visible: mission.showEstimate && mission.showIntersection
        Layout.topMargin: 4
        text: qsTr("Intersection zone")
        font.pixelSize: info.big ? 13 : 11
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: window.colourOf("dim")
    }
    Label {
        objectName: "zoneLabel"
        Layout.fillWidth: true
        text: mission.zoneText
        wrapMode: Text.Wrap
        font.pixelSize: info.big ? 15 : 13
    }
    Label {
        objectName: "mgrsLabel"
        Layout.fillWidth: true
        text: mission.mgrsText
        wrapMode: Text.Wrap
        font.family: "monospace"
        font.pixelSize: info.big ? 15 : 13
        font.bold: mission.zoneShown
        color: window.colourOf("link")
        TapHandler {
            enabled: mission.zoneShown
            onLongPressed: { app.copyToClipboard(mission.zoneMgrs); window.showMessage(qsTr("MGRS copied")) }
        }
    }
    Label {
        visible: mission.zoneShown
        Layout.fillWidth: true
        text: "DMS: " + mission.dmsText(mission.zoneLat, mission.zoneLon)
        font.family: "monospace"
        font.pixelSize: info.big ? 14 : 12
        color: window.colourOf("dim")
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        AppButton {
            objectName: "historyButton"
            Layout.fillWidth: true
            onClicked: window.showHistory()
            contentItem: RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                Icon { kind: "history"; colour: window.Material.foreground; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                Label { text: qsTr("History"); font.bold: true }
                Item { Layout.fillWidth: true }
            }
        }
        AppButton {
            objectName: "kilometersButton"
            Layout.fillWidth: true
            onClicked: window.showKilometers()
            contentItem: RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                Icon { kind: "car"; colour: window.Material.foreground; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                Label { text: qsTr("Kilometres"); font.bold: true }
                Item { Layout.fillWidth: true }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label {
            objectName: "recordCount"
            text: qsTr("%1 records").arg(mission.history.count)
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Tiles: %1").arg(mission.tiles.storedTileCount)
            color: window.colourOf("dim")
        }
        Label {
            visible: mission.totalKilometers > 0
            text: "  " + qsTr("%1 km driven").arg(mission.totalKilometers.toFixed(1))
            color: window.colourOf("dim")
        }
    }
}
