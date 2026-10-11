// StationTable.qml - the stations as the Python StationsTableWidget: one
// row each, every field edited in place.  Columns: callsign, latitude and
// longitude in degrees/minutes/seconds, signal, azimuth, the north it is
// read from, uncertainty, colour, visible, record the bearing, delete.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Card {
    id: table
    padding: 6
    topPadding: 6

    // Column widths (the callsign takes what is left).
    // (16: the north the azimuth is read from, shown after the azimuth.)
    readonly property var widths: [0, 38, 32, 46, 30, 42, 32, 46, 30, 76, 54, 54, 28, 28, 30, 30, 28]
    readonly property int spacingX: 3

    component HeaderCell: Label {
        property int column
        Layout.preferredWidth: table.widths[column]
        Layout.fillWidth: column === 0
        Layout.minimumWidth: column === 0 ? 70 : table.widths[column]
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 12
        font.bold: true
        elide: Text.ElideRight
        color: window.colourOf("dim")
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: table.spacingX
        HeaderCell { column: 0; text: qsTr("Callsign") }
        HeaderCell { column: 1; text: "Lat °" }
        HeaderCell { column: 2; text: "Lat '" }
        HeaderCell { column: 3; text: "Lat \"" }
        HeaderCell { column: 4; text: qsTr("N/S") }
        HeaderCell { column: 5; text: "Lon °" }
        HeaderCell { column: 6; text: "Lon '" }
        HeaderCell { column: 7; text: "Lon \"" }
        HeaderCell { column: 8; text: qsTr("E/W") }
        HeaderCell { column: 9; text: qsTr("Signal") }
        HeaderCell { column: 10; text: qsTr("Azimuth") }
        HeaderCell { column: 16; text: qsTr("Ref") }
        HeaderCell { column: 11; text: "±" }
        Item { Layout.preferredWidth: table.widths[12]; Layout.preferredHeight: 16
               Icon { anchors.centerIn: parent; width: 14; height: 14; kind: "edit"; colour: window.colourOf("dim") } }
        Item { Layout.preferredWidth: table.widths[13]; Layout.preferredHeight: 16
               Icon { anchors.centerIn: parent; width: 14; height: 14; kind: "eye"; colour: window.colourOf("dim") } }
        Item { Layout.preferredWidth: table.widths[14]; Layout.preferredHeight: 16
               Icon { anchors.centerIn: parent; width: 14; height: 14; kind: "record"; colour: window.colourOf("dim") } }
        Item { Layout.preferredWidth: table.widths[15]; Layout.preferredHeight: 16
               Icon { anchors.centerIn: parent; width: 14; height: 14; kind: "trash"; colour: window.colourOf("dim") } }
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: window.mc.edge }

    ListView {
        id: list
        objectName: "stationList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 60
        clip: true
        spacing: 2
        model: mission.stations
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }


        Label {
            anchors.centerIn: parent
            visible: list.count === 0
            width: parent.width - 40
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("No station yet. \"Add a station\", or right-click the map: \"Station here\".")
            color: window.colourOf("dim")
        }

        delegate: Rectangle {
            id: row
            required property int index
            required property string callsign
            required property int latDeg
            required property int latMin
            required property real latSec
            required property string latDir
            required property int lonDeg
            required property int lonMin
            required property real lonSec
            required property string lonDir
            required property string signal
            required property real azimuth
            required property real uncertainty
            required property string stationColor
            required property bool shown
            required property bool magnetic
            required property real declination
            width: list.width - (list.ScrollBar.vertical.visible ? 10 : 0)
            height: 34
            color: index % 2 ? window.mc.rowAlt : "transparent"
            radius: 3
            readonly property var stations: mission.stations
            // The callsign field is set once, so typing does not move its
            // cursor; a change made elsewhere (station editor) is shown here.
            onCallsignChanged: if (!callField.activeFocus) callField.text = callsign

            function setLat(d, m, s, h) { stations.setLatDms(index, d, m, s, h) }
            function setLon(d, m, s, h) { stations.setLonDms(index, d, m, s, h) }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                spacing: table.spacingX

                AppField {
                    id: callField
                    objectName: "callsignField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 70
                    Layout.preferredHeight: 30
                    topPadding: 0
                    bottomPadding: 0
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 13
                    font.bold: true
                    color: row.stationColor
                    hint: qsTr("Callsign")
                    // Upper case as it is typed, the cursor left where it was
                    // (only this field changes a row's callsign).
                    Component.onCompleted: text = row.callsign
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                    selectByMouse: true
                    onTextEdited: {
                        var up = text.toUpperCase()
                        if (up !== text) { var c = cursorPosition; text = up; cursorPosition = c }
                        row.stations.setField(row.index, "callsign", text)
                    }
                    background: Rectangle {
                        radius: 4
                        color: window.mc.field
                        border.width: callField.activeFocus ? 2 : 1
                        border.color: callField.activeFocus ? window.Material.accent : window.mc.fieldEdge
                    }
                }
                NumberField { Layout.preferredWidth: table.widths[1]; from: 0; to: 90; value: row.latDeg
                              onEdited: function(v) { row.setLat(v, row.latMin, row.latSec, row.latDir) } }
                NumberField { Layout.preferredWidth: table.widths[2]; from: 0; to: 59; value: row.latMin
                              onEdited: function(v) { row.setLat(row.latDeg, v, row.latSec, row.latDir) } }
                NumberField { Layout.preferredWidth: table.widths[3]; from: 0; to: 59.99; decimals: 1; step: 0.1; value: row.latSec
                              onEdited: function(v) { row.setLat(row.latDeg, row.latMin, v, row.latDir) } }
                DirButton { objectName: "rowLatDir"; Layout.preferredWidth: table.widths[4]; Layout.preferredHeight: 30; text: row.latDir
                            onClicked: row.setLat(row.latDeg, row.latMin, row.latSec, row.latDir === "N" ? "S" : "N") }
                NumberField { Layout.preferredWidth: table.widths[5]; from: 0; to: 180; value: row.lonDeg
                              onEdited: function(v) { row.setLon(v, row.lonMin, row.lonSec, row.lonDir) } }
                NumberField { Layout.preferredWidth: table.widths[6]; from: 0; to: 59; value: row.lonMin
                              onEdited: function(v) { row.setLon(row.lonDeg, v, row.lonSec, row.lonDir) } }
                NumberField { Layout.preferredWidth: table.widths[7]; from: 0; to: 59.99; decimals: 1; step: 0.1; value: row.lonSec
                              onEdited: function(v) { row.setLon(row.lonDeg, row.lonMin, v, row.lonDir) } }
                DirButton { Layout.preferredWidth: table.widths[8]; Layout.preferredHeight: 30; text: row.lonDir
                            onClicked: row.setLon(row.lonDeg, row.lonMin, row.lonSec, row.lonDir === "E" ? "W" : "E") }
                AppComboBox {
                    objectName: "signalBox"
                    compact: true
                    Layout.preferredWidth: table.widths[9]
                    Layout.preferredHeight: 30
                    font.pixelSize: 12
                    model: row.stations.signalLevels
                    currentIndex: Math.max(0, row.stations.signalLevels.indexOf(row.signal))
                    onActivated: row.stations.setField(row.index, "signal", currentText)
                    leftPadding: 4
                    rightPadding: 0
                    topInset: 0
                    bottomInset: 0
                }
                NumberField { objectName: "azimuthField"; Layout.preferredWidth: table.widths[10]; from: 0; to: 359; wrap: true
                              suffix: "°"; value: row.azimuth; font.bold: true
                              onEdited: function(v) { row.stations.setField(row.index, "azimuth", v) } }
                NorthButton {
                    Layout.preferredWidth: table.widths[16]
                    Layout.preferredHeight: 30
                    magnetic: row.magnetic
                    declination: row.declination
                    onSwitched: row.stations.setField(row.index, "magnetic", !row.magnetic)
                }
                NumberField { Layout.preferredWidth: table.widths[11]; from: 0; to: 30; decimals: 1; step: 0.5
                              prefix: "±"; suffix: "°"; value: row.uncertainty
                              onEdited: function(v) { row.stations.setField(row.index, "uncertainty", v) } }
                ColorButton { Layout.preferredWidth: table.widths[12]; Layout.preferredHeight: 24
                              colour: row.stationColor; tip: qsTr("Colour")
                              onChosen: function(c) { row.stations.setField(row.index, "stationColor", c) } }
                CheckBox {
                    Layout.preferredWidth: table.widths[13]
                    Layout.preferredHeight: 30
                    padding: 0
                    checked: row.shown
                    onToggled: row.stations.setField(row.index, "shown", checked)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Visible on the map")
                    ToolTip.delay: 600
                }
                IconButton {
                    objectName: "recordButton"
                    Layout.preferredWidth: table.widths[14]
                    Layout.preferredHeight: 30
                    padding: 4
                    iconKind: "record"
                    iconSize: 16
                    tip: qsTr("Record this bearing in the history")
                    onClicked: mission.recordStation(row.index)
                }
                IconButton {
                    objectName: "deleteButton"
                    Layout.preferredWidth: table.widths[15]
                    Layout.preferredHeight: 30
                    padding: 4
                    iconKind: "trash"
                    iconSize: 16
                    tip: qsTr("Delete the station")
                    onClicked: row.stations.remove(row.index)
                }
            }
            // Double click on the row's free space: the full editor.
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onDoubleTapped: window.editStation(row.index)
            }
        }
    }
}
