// StationEditor.qml - every field of one station in a dialog: callsign,
// position (DMS or decimal degrees, or the centre of the map), bearing and
// the north it is read from, uncertainty, signal, colour, visibility, notes.  Used on the phone (the
// station cards) and on the desktop (double click on a row, "Station
// here" on the map).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("Station")
    preferredWidth: 480
    standardButtons: Dialog.Ok | Dialog.Cancel
    property int row: -1
    property string colour: "#e74c3c"

    function openFor(r) {
        row = r
        var s = mission.stations.get(r)
        if (!s.stationId) return
        callField.text = s.callsign
        position.lat = s.lat
        position.lon = s.lon
        position.reload()
        azField.value = Math.round(s.azimuth)
        uncField.value = s.uncertainty
        signalBox.currentIndex = Math.max(0, mission.stations.signalLevels.indexOf(s.signal))
        colour = s.stationColor
        visibleCheck.checked = s.shown
        northBox.currentIndex = s.magnetic ? 1 : 0
        notesField.text = s.notes
        open()
        if (s.callsign.length === 0) callField.forceActiveFocus()
    }
    onAccepted: {
        var st = mission.stations
        st.setField(row, "callsign", callField.text)
        st.setPosition(row, position.lat, position.lon)
        st.setField(row, "azimuth", azField.value)
        st.setField(row, "magnetic", northBox.currentIndex === 1)
        st.setField(row, "uncertainty", uncField.value)
        st.setField(row, "signal", signalBox.currentText)
        st.setField(row, "stationColor", colour)
        st.setField(row, "shown", visibleCheck.checked)
        st.setField(row, "notes", notesField.text)
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ColorButton {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                colour: dialog.colour
                onChosen: function(c) { dialog.colour = c }
            }
            AppField {
                id: callField
                objectName: "editorCallsign"
                Layout.fillWidth: true
                hint: qsTr("Callsign")
                font.bold: true
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onTextEdited: {
                    var up = text.toUpperCase()
                    if (up !== text) { var c = cursorPosition; text = up; cursorPosition = c }
                }
            }
            CheckBox { id: visibleCheck; text: qsTr("Visible") }
        }

        DmsEditor { id: position; Layout.fillWidth: true }
        AppButton {
            Layout.fillWidth: true
            flat: true
            text: qsTr("Use the centre of the map")
            enabled: window.mapView !== null
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
            Label { text: qsTr("Azimuth:") }
            NumberField { id: azField; Layout.fillWidth: true; compact: !app.android; from: 0; to: 359; wrap: true; suffix: "°"
                          onEdited: function(v) { value = v } }
            Label { text: qsTr("North:") }
            AppComboBox {
                id: northBox
                objectName: "editorNorth"
                Layout.fillWidth: true
                model: [qsTr("True (map, GPS)"), qsTr("Magnetic (compass)")]
            }
            Item { width: 1; height: 1; visible: northBox.currentIndex === 1 }
            Label {
                objectName: "editorDeclination"
                visible: northBox.currentIndex === 1
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: window.colourOf("dim")
                readonly property real declination: mission.declinationAt(position.lat, position.lon)
                text: qsTr("Declination here: %1 (%2), true bearing %3°.")
                          .arg(mission.declinationText(declination)).arg(mission.declinationModel)
                          .arg(((azField.value + declination) % 360 + 360) % 360 > 359.5 ? "0"
                               : (((azField.value + declination) % 360 + 360) % 360).toFixed(0))
            }
            Label { text: qsTr("Uncertainty:") }
            NumberField { id: uncField; Layout.fillWidth: true; compact: !app.android; from: 0; to: 30; decimals: 1; step: 0.5
                          prefix: "±"; suffix: "°"; onEdited: function(v) { value = v } }
            Label { text: qsTr("Signal:") }
            AppComboBox { id: signalBox; Layout.fillWidth: true; model: mission.stations.signalLevels }
            Label { text: qsTr("Notes:") }
            TextField { id: notesField; Layout.fillWidth: true }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("S0: the station is on the map but hears nothing - no bearing line, no part in the zone.")
        }
        // Save the changes and send the bearing: this station's own as a DF
        // report, another's (read out by telephone) as an object named
        // after it.
        AppButton {
            id: sendButton
            objectName: "editorSend"
            Layout.fillWidth: true
            visible: mission.aprs.myCallsign.length > 0 && callField.text.trim().length > 0
            // myCallsign read first: the binding follows it.
            readonly property bool own: mission.aprs.myCallsign.length > 0 && mission.aprs.isMine(callField.text)
            highlighted: mission.aprs.canSend
            text: qsTr("OK and send by APRS")
            onClicked: {
                var r = dialog.row
                dialog.accept()
                mission.aprs.sendBearing(r)
                window.showMessage(mission.statusMessage)
            }
        }
        Label {
            objectName: "editorRelayNote"
            Layout.fillWidth: true
            visible: sendButton.visible && !sendButton.own
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Sent as an APRS object named %1, from %2.")
                      .arg(callField.text.trim()).arg(mission.aprs.myCallsign)
        }
    }
}
