// MapSettingsView.qml - the Python version's "Map settings", "Intersection
// zone" and "UTM/MGRS grid" groups.  compact: the desktop's rows; else the
// phone's stacked cards.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: view
    property bool compact: false
    spacing: 8

    function setOffline(on) {
        mission.offline = on
        if (on) {
            var warning = mission.offlineCheck()
            if (warning.length > 0) window.alert(qsTr("Warning"), warning)
            else window.showMessage(mission.statusMessage)
        }
    }

    Card {
        title: qsTr("Map settings")
        Layout.fillWidth: true
        GridLayout {
            Layout.fillWidth: true
            columns: view.compact ? 6 : 2
            columnSpacing: 10
            rowSpacing: view.compact ? 4 : 10
            Label { text: qsTr("Length:") }
            NumberField {
                objectName: "lengthField"
                compact: view.compact
                Layout.preferredWidth: view.compact ? 76 : -1
                Layout.fillWidth: !view.compact
                from: 10; to: 500; step: 10
                suffix: " km"
                value: mission.azimuthLengthKm
                onEdited: function(v) { mission.azimuthLengthKm = v }
            }
            Label { text: qsTr("Background:") }
            AppComboBox {
                objectName: "providerBox"
                Layout.fillWidth: true
                model: mission.providerNames
                currentIndex: mission.providerIndex
                onActivated: mission.providerIndex = currentIndex
            }
            CheckBox {
                objectName: "offlineCheck"
                text: qsTr("Offline")
                checked: mission.offline
                onToggled: view.setOffline(checked)
                Layout.columnSpan: view.compact ? 1 : 2
            }
            AppButton {
                visible: view.compact
                text: qsTr("Download")
                flat: true
                onClicked: window.downloadTiles()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Download the visible tiles for offline use")
                ToolTip.delay: 600
            }
        }
        AppButton {
            visible: !view.compact
            Layout.fillWidth: true
            text: qsTr("Download visible tiles")
            onClicked: window.downloadTiles()
        }
        Label {
            visible: !view.compact
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Tiles stored: %1. Offline, the map shows only the stored tiles and those seen before.")
                  .arg(mission.tiles.storedTileCount)
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: 8

        Card {
            title: qsTr("Intersection zone")
            width: view.compact ? Math.max(380, (view.width - 8) * 0.62) : view.width
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                CheckBox {
                    objectName: "showZoneCheck"
                    text: qsTr("Show")
                    checked: mission.showIntersection
                    onToggled: mission.showIntersection = checked
                }
                Label { text: qsTr("Border:") }
                ColorButton { colour: mission.zoneBorderColor; tip: qsTr("Border colour")
                              onChosen: function(c) { mission.zoneBorderColor = c } }
                Label { text: qsTr("Fill:") }
                ColorButton { colour: mission.zoneFillColor; tip: qsTr("Fill colour")
                              onChosen: function(c) { mission.zoneFillColor = c } }
                Item { Layout.fillWidth: true; visible: !view.compact }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label { text: qsTr("Opacity:") }
                Slider {
                    objectName: "opacitySlider"
                    Layout.fillWidth: true
                    from: 0; to: 100; stepSize: 1
                    value: Math.round(mission.zoneOpacity * 100)
                    onMoved: mission.zoneOpacity = value / 100
                }
                Label { text: Math.round(mission.zoneOpacity * 100) + "%"; Layout.preferredWidth: 40 }
            }
            // The least-squares estimate and its confidence ellipses.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                CheckBox {
                    objectName: "showEstimateCheck"
                    text: qsTr("Best estimate")
                    checked: mission.showEstimate
                    onToggled: mission.showEstimate = checked
                }
                Label { text: qsTr("Colour:") }
                ColorButton { colour: mission.estimateColor; tip: qsTr("Colour of the estimate")
                              onChosen: function(c) { mission.estimateColor = c } }
                Item { Layout.fillWidth: true }
            }
        }

        Card {
            title: qsTr("UTM/MGRS grid")
            width: view.compact ? Math.max(180, view.width - 8 - Math.max(380, (view.width - 8) * 0.62)) : view.width
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                CheckBox {
                    objectName: "gridCheck"
                    text: qsTr("Show")
                    checked: mission.showGrid
                    onToggled: mission.showGrid = checked
                }
                Label { text: qsTr("Colour:") }
                ColorButton { colour: mission.gridColor; tip: qsTr("Grid colour")
                              onChosen: function(c) { mission.gridColor = c } }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
