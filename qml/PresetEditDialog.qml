// PresetEditDialog.qml - one preset: name, callsign, position, default
// uncertainty, colour (the Python PresetEditDialog).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: row < 0 ? qsTr("New preset") : qsTr("Edit preset")
    preferredWidth: 480
    standardButtons: Dialog.Ok | Dialog.Cancel
    property int row: -1
    property string colour: "#e74c3c"

    function openFor(r) {
        row = r
        var p = r >= 0 ? mission.presets.get(r) : { "name": "", "callsign": "", "lat": 0, "lon": 0,
                                                    "color": "#e74c3c", "defaultUncertainty": 5 }
        nameField.text = p.name
        callField.text = p.callsign
        position.lat = p.lat
        position.lon = p.lon
        position.reload()
        uncField.value = p.defaultUncertainty
        colour = p.color
        open()
    }
    onAccepted: {
        if (nameField.text.trim().length === 0 || callField.text.trim().length === 0) {
            window.alert(qsTr("Preset"), qsTr("A preset needs a name and a callsign."))
            return
        }
        var fields = { "name": nameField.text, "callsign": callField.text, "lat": position.lat, "lon": position.lon,
                       "color": colour, "defaultUncertainty": uncField.value }
        if (row < 0) mission.presets.add(fields)
        else mission.presets.update(row, fields)
    }

    ColumnLayout {
        width: parent.width
        spacing: 10
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Label { text: qsTr("Name:") }
            AppField { id: nameField; Layout.fillWidth: true; hint: qsTr("E.g. Command post") }
            Label { text: qsTr("Callsign:") }
            AppField {
                id: callField
                Layout.fillWidth: true
                hint: qsTr("E.g. FXXXX")
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onTextEdited: {
                    var up = text.toUpperCase()
                    if (up !== text) { var c = cursorPosition; text = up; cursorPosition = c }
                }
            }
        }
        DmsEditor { id: position; Layout.fillWidth: true }
        AppButton {
            Layout.fillWidth: true
            flat: true
            visible: window.mapView !== null
            text: qsTr("Use the centre of the map")
            onClicked: {
                position.lat = window.mapView.centerLat
                position.lon = window.mapView.centerLon
                position.reload()
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Label { text: qsTr("Default uncertainty:") }
            NumberField { id: uncField; Layout.fillWidth: true; compact: !app.android; from: 0; to: 30; decimals: 1
                          step: 0.5; suffix: "°"; onEdited: function(v) { value = v } }
            Label { text: qsTr("Colour:") }
            ColorButton { colour: dialog.colour; Layout.preferredWidth: 60; onChosen: function(c) { dialog.colour = c } }
        }
    }
}
