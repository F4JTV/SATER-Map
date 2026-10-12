// HistoryView.qml - the recorded bearings, newest first (the Python
// HistoryDialog): select rows to delete them, export to CSV, clear all.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 8
    property var selected: ({})
    property int selectedCount: 0

    function toggle(row) {
        // A copy: the same object assigned back would not notify.
        var s = Object.assign({}, selected)
        if (s[row]) delete s[row]
        else s[row] = true
        selected = s
        selectedCount = Object.keys(s).length
    }
    function clearSelection() { selected = ({}); selectedCount = 0 }

    Connections {
        target: mission.history
        function onHistoryChanged() { view.clearSelection() }
    }

    readonly property bool narrow: width < 620

    // Header (wide only: on a phone each record is a two-line card).
    RowLayout {
        visible: !view.narrow
        Layout.fillWidth: true
        spacing: 6
        component Head: Label { font.bold: true; font.pixelSize: 12; color: window.colourOf("dim") }
        Head { text: qsTr("Time"); Layout.preferredWidth: 140 }
        Head { text: qsTr("Callsign"); Layout.preferredWidth: 80 }
        Head { text: qsTr("Azimuth"); Layout.preferredWidth: 60 }
        Head { text: "±"; Layout.preferredWidth: 46 }
        Head { text: qsTr("Signal"); Layout.preferredWidth: 50 }
        Head { text: qsTr("Position"); Layout.fillWidth: true }
    }

    ListView {
        id: list
        objectName: "historyList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        clip: true
        model: mission.history
        ScrollBar.vertical: ScrollBar {}
        Label {
            anchors.centerIn: parent
            visible: list.count === 0
            text: qsTr("No bearing recorded yet.")
            color: window.colourOf("dim")
        }
        delegate: Rectangle {
            id: rec
            required property int index
            required property string timestamp
            required property string callsign
            required property real azimuth
            required property bool magnetic
            // "248°", or "248°M" read from magnetic north.
            readonly property string azimuthText: Math.round(azimuth) + "°" + (magnetic ? "M" : "")
            required property real uncertainty
            required property string signal
            required property real lat
            required property real lon
            required property string dms
            width: list.width - 10
            height: view.narrow ? 54 : 30
            color: view.selected[index] ? Qt.rgba(window.Material.accent.r, window.Material.accent.g, window.Material.accent.b, 0.25)
                                        : (index % 2 ? window.mc.rowAlt : "transparent")
            radius: 3
            MouseArea { anchors.fill: parent; onClicked: view.toggle(rec.index) }
            RowLayout {
                visible: !view.narrow
                anchors.fill: parent
                anchors.leftMargin: 4
                spacing: 6
                Label { text: rec.timestamp; Layout.preferredWidth: 140; font.pixelSize: 12 }
                Label { text: rec.callsign; Layout.preferredWidth: 80; font.bold: true; elide: Text.ElideRight }
                Label { text: rec.azimuthText; Layout.preferredWidth: 60 }
                Label { text: "±" + rec.uncertainty.toFixed(1) + "°"; Layout.preferredWidth: 46 }
                Label { text: rec.signal; Layout.preferredWidth: 50 }
                Label { text: rec.lat.toFixed(6) + ", " + rec.lon.toFixed(6) + "   " + rec.dms
                        Layout.fillWidth: true; font.pixelSize: 12; elide: Text.ElideRight }
            }
            ColumnLayout {
                visible: view.narrow
                anchors.fill: parent
                anchors.margins: 4
                spacing: 0
                Label { text: rec.callsign + "   " + rec.azimuthText + " ±" + rec.uncertainty.toFixed(1) + "°   " + rec.signal
                        font.bold: true }
                Label { text: rec.timestamp + "   " + rec.dms; font.pixelSize: 12; color: window.colourOf("dim")
                        elide: Text.ElideRight; Layout.fillWidth: true }
            }
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: 6
        AppButton {
            objectName: "deleteSelected"
            text: qsTr("Delete selection")
            enabled: view.selectedCount > 0
            onClicked: mission.history.removeRows(Object.keys(view.selected).map(Number))
        }
        AppButton {
            text: qsTr("Export CSV")
            enabled: list.count > 0
            onClicked: window.exportHistory()
        }
        AppButton {
            text: qsTr("Clear all")
            enabled: list.count > 0
            onClicked: window.confirm(qsTr("Confirmation"), qsTr("Clear the whole history?"),
                                      function() { mission.history.clear() })
        }
    }
}
