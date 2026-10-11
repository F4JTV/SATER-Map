// DesktopLayout.qml - the Python version's main window: the control panel
// on the left (mission timer, station table, map settings, zone, grid,
// information), the map on the right, a splitter between them.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SplitView {
    id: layout
    objectName: "desktopLayout"
    orientation: Qt.Horizontal

    // Nothing to close with a back gesture on the desktop.
    function back() { return false }

    handle: Rectangle {
        implicitWidth: 6
        color: SplitHandle.pressed ? window.Material.accent : SplitHandle.hovered ? window.mc.edge : window.theme.background
    }

    Pane {
        id: panel
        objectName: "controlPanel"
        // The station table needs 760 px; the map keeps at least 300 px of
        // what is left (Main.qml switches to the phone layout below 1000 px).
        SplitView.preferredWidth: Math.min(Math.max(800, layout.width * 0.46), layout.width - 300)
        SplitView.minimumWidth: 760
        padding: 8
        background: Rectangle { color: window.theme.background }

        // The station table takes the height the other parts leave; on a
        // short screen (a laptop at 768 px) it keeps 150 px and the panel
        // scrolls instead of cutting off its bottom.
        Flickable {
            id: panelFlick
            objectName: "panelFlick"
            anchors.fill: parent
            clip: true
            contentWidth: width
            contentHeight: Math.max(height, timerView.implicitHeight + addRow.implicitHeight + 150
                                    + mapSettings.implicitHeight + infoView.implicitHeight + 4 * panelColumn.spacing)
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height + 1
            ScrollBar.vertical: ScrollBar { policy: panelFlick.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

            ColumnLayout {
                id: panelColumn
                width: panelFlick.width - (panelFlick.interactive ? 10 : 0)
                height: panelFlick.contentHeight
                spacing: 8

                TimerView { id: timerView; Layout.fillWidth: true }

                RowLayout {
                    id: addRow
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton {
                        objectName: "addStationButton"
                        Layout.fillWidth: true
                        text: qsTr("Add a station")
                        icon.width: 0
                        onClicked: mission.addStation()
                        contentItem: RowLayout {
                            spacing: 8
                            Item { Layout.fillWidth: true }
                            Icon { kind: "plus"; colour: window.Material.foreground; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                            Label { text: qsTr("Add a station"); font.bold: true }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    AppButton {
                        objectName: "clearStationsButton"
                        Layout.fillWidth: true
                        onClicked: window.clearStations()
                        contentItem: RowLayout {
                            spacing: 8
                            Item { Layout.fillWidth: true }
                            Icon { kind: "trash"; colour: window.Material.foreground; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                            Label { text: qsTr("Clear all"); font.bold: true }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                StationTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 150
                }

                MapSettingsView { id: mapSettings; Layout.fillWidth: true; compact: true }

                InfoView { id: infoView; Layout.fillWidth: true }
            }
        }
    }

    MapPane {
        id: mapPane
        SplitView.fillWidth: true
        SplitView.minimumWidth: 200
        phone: false
    }
}
