// SavePresetDialog.qml - "Save station as preset": which station, under
// which name (the Python QInputDialog pair).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("Save station as preset")
    preferredWidth: 420
    standardButtons: Dialog.Ok | Dialog.Cancel
    // Rows of the stations that have a callsign.
    property var rows: []

    function openFor(row) {
        var r = [], labels = []
        for (var i = 0; i < mission.stations.count; ++i) {
            var s = mission.stations.get(i)
            if (!s.valid) continue
            r.push(i)
            labels.push(s.callsign + " (" + s.latText + " " + s.lonText + ")")
        }
        rows = r
        stationBox.model = labels
        stationBox.currentIndex = Math.max(0, r.indexOf(row))
        nameField.text = qsTr("Preset %1").arg(mission.stations.get(r[stationBox.currentIndex]).callsign)
        open()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }
    onAccepted: {
        var name = nameField.text.trim()
        if (name.length === 0) return
        if (mission.saveStationAsPreset(rows[stationBox.currentIndex], name))
            window.alert(qsTr("Preset saved"), qsTr("Preset '%1' saved.\n\nFile: %2").arg(name).arg(mission.presets.filePath))
        else
            window.alert(qsTr("Error"), qsTr("Cannot save the preset.\n\nFile: %1\n\nCheck the write permissions.")
                         .arg(mission.presets.filePath))
    }

    ColumnLayout {
        width: parent.width
        spacing: 10
        Label { text: qsTr("Station to save:") }
        AppComboBox {
            id: stationBox
            Layout.fillWidth: true
            onActivated: nameField.text = qsTr("Preset %1").arg(mission.stations.get(dialog.rows[currentIndex]).callsign)
        }
        Label { text: qsTr("Name for this preset:") }
        TextField { id: nameField; objectName: "presetName"; Layout.fillWidth: true; onAccepted: dialog.accept() }
    }
}
