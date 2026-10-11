// PhoneLayout.qml - the same mission on a phone: the map full screen, and
// the stations, the mission (timer, zone, history, kilometres) and the
// settings on pages of their own under a tab bar.  The menu (top right)
// holds what the desktop has in its menu bar.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: layout
    objectName: "phoneLayout"
    padding: 0
    property alias pageIndex: tabs.currentIndex

    // Android's back gesture: close what is open, then go back to the map.
    function back() {
        if (mainMenu.visible) { mainMenu.close(); return true }
        if (tabs.currentIndex !== 0) { tabs.currentIndex = 0; return true }
        return false
    }

    background: Rectangle { color: window.theme.background }
    header: ToolBar {
        Material.foreground: window.mc.header
        // Drawn here: Material's own is an elevation layer, empty on the
        // software renderer.
        background: Rectangle {
            implicitHeight: 52
            color: window.theme.primary
            Rectangle {
                anchors.top: parent.bottom
                width: parent.width
                height: 4
                z: 1
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(0, 0, 0, window.mc.shadow * 0.6) }
                    GradientStop { position: 1; color: "transparent" }
                }
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4
            spacing: 8
            Image {
                source: "qrc:/assets/logo.png"
                sourceSize.width: 30
                sourceSize.height: 30
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                smooth: true
                opacity: window.monochrome ? 0.55 : 1
            }
            Label {
                text: "SATER Map"
                font.bold: true
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            // APRS at a glance: green when a bearing can go out, amber when
            // a link is up (or coming) but cannot send, red when none is;
            // TX while the built-in modem keys the transmitter.
            AprsStatus {
                objectName: "headerAprs"
                readonly property var aprs: mission.aprs
                visible: aprs.active
                name: aprs.modemTransmitting ? "TX" : "APRS"
                state: aprs.canSend ? "connected"
                     : (aprs.isState !== "off" || aprs.kissState !== "off" || aprs.modemState !== "off") ? "connecting" : "off"
                detail: state === "off" ? "-" : ""
                showDetail: false
                TapHandler { onTapped: window.showAprs() }
            }
            // The mission clock, always in sight.
            Label {
                objectName: "headerClock"
                visible: mission.timerStarted
                text: mission.elapsedText
                font.family: "monospace"
                font.pixelSize: 16
                font.bold: true
                color: mission.timerRunning ? window.mc.header : window.colourOf("dim")
                TapHandler { onTapped: layout.pageIndex = 2 }
            }
            ToolButton {
                objectName: "menuButton"
                // Drawn rather than the U+2630 glyph, which the phone's fonts
                // may not have (learned on AX25Chat).
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    Icon { anchors.centerIn: parent; width: 22; height: 22; kind: "menu"; colour: window.mc.header }
                }
                onClicked: mainMenu.popup(this, width - mainMenu.width, height)
            }
        }
    }

    AppMenu {
        id: mainMenu
        objectName: "mainMenu"
        width: Math.min(layout.width - 24, 300)
        MenuItem { text: qsTr("Import JSON"); onTriggered: window.importJson() }
        MenuItem { text: qsTr("Export JSON"); onTriggered: window.exportJson() }
        MenuItem { text: qsTr("Export KML"); onTriggered: window.exportKml() }
        MenuItem { text: qsTr("Save HTML"); onTriggered: window.saveHtml() }
        MenuItem { text: qsTr("Export PNG"); onTriggered: window.exportPng() }
        MenuItem { text: qsTr("Generate PDF report"); onTriggered: window.generatePdf() }
        MenuSeparator {}
        MenuItem { text: qsTr("APRS packet log"); onTriggered: window.showAprsLog() }
        MenuSeparator {}
        MenuItem { text: qsTr("Manage presets"); onTriggered: window.managePresets() }
        MenuItem { text: qsTr("Load presets"); onTriggered: window.loadPresets() }
        MenuItem { text: qsTr("Save station as preset"); onTriggered: window.saveStationAsPreset(-1) }
        MenuSeparator {}
        MenuItem { text: qsTr("Set beacon position"); onTriggered: window.setBeacon() }
        MenuItem { text: qsTr("Clear beacon position"); enabled: mission.hasBeacon
                   onTriggered: { mission.clearBeacon(); window.showMessage(mission.statusMessage) } }
        MenuItem { text: qsTr("Centre on the intersection zone"); onTriggered: { tabs.currentIndex = 0; window.centreOnZone() } }
        MenuItem { text: qsTr("Centre on France"); onTriggered: { tabs.currentIndex = 0; window.centreFrance() } }
        MenuItem { text: qsTr("Download visible tiles"); onTriggered: { tabs.currentIndex = 0; window.downloadTiles() } }
        MenuSeparator {}
        MenuItem { text: qsTr("New mission"); onTriggered: window.newMission() }
        MenuItem { text: qsTr("About"); onTriggered: aboutDialog.open() }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        // ---- map ------------------------------------------------------------
        Item {
            MapPane {
                id: mapPane
                anchors.fill: parent
                phone: true
            }
            // The best estimate at a glance (or the zone when the estimate
            // is hidden), over the map's foot.
            Pane {
                id: zoneCard
                objectName: "zoneCard"
                readonly property bool estimate: mission.showEstimate && (mission.hasFix || mission.fixWarning.length > 0)
                visible: estimate || mission.zoneShown
                // Two children (the text, the tap area): the size is said.
                contentHeight: zoneColumn.implicitHeight
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                anchors.bottomMargin: 40
                padding: 10
                Material.elevation: window.flat ? 0 : 4
                Material.background: window.mc.card
                background: Rectangle { color: window.mc.card; radius: 10; opacity: 0.94
                                        border.width: window.flat ? 1 : 0; border.color: window.mc.edge }
                ColumnLayout {
                    id: zoneColumn
                    width: parent.width
                    spacing: 2
                    Label {
                        visible: zoneCard.estimate
                        text: qsTr("Best estimate")
                        font.pixelSize: 11
                        font.bold: true
                        font.capitalization: Font.AllUppercase
                        color: mission.estimateColor
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !zoneCard.estimate || mission.hasFix
                        text: zoneCard.estimate ? mission.fixText : mission.zoneText
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !zoneCard.estimate || mission.hasFix
                        text: zoneCard.estimate ? "MGRS: " + mission.fixMgrs : mission.mgrsText
                        font.family: "monospace"
                        font.bold: true
                        font.pixelSize: 14
                        color: window.colourOf("link")
                    }
                    Label {
                        objectName: "zoneCardWarning"
                        Layout.fillWidth: true
                        visible: zoneCard.estimate && mission.fixWarning.length > 0
                        text: mission.fixWarning
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        font.bold: true
                        color: window.colourOf("error")
                    }
                }
                MouseArea { anchors.fill: parent; onClicked: window.centreOnZone() }
            }
        }

        // ---- stations ---------------------------------------------------------
        Item {
            ListView {
                id: stationList
                objectName: "stationCards"
                anchors.fill: parent
                anchors.margins: 8
                spacing: 10
                clip: true
                model: mission.stations
                bottomMargin: 80
                delegate: StationCard { width: stationList.width }
                onCountChanged: if (count > 0) Qt.callLater(function() { stationList.positionViewAtIndex(stationList.count - 1, ListView.Contain) })
                onHeightChanged: returnToBounds()
                ScrollIndicator.vertical: ScrollIndicator {}
            }
            Label {
                anchors.centerIn: parent
                visible: stationList.count === 0
                width: parent.width - 48
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: 15
                color: window.colourOf("dim")
                text: qsTr("No station yet.\n\nAdd one with the + button, or press and hold the map where it stands.")
            }
            RoundButton {
                objectName: "addStationFab"
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 18
                width: 62
                height: 62
                background: Item {
                    SoftShadow { anchors.fill: parent; radius: width / 2; spread: 5; offsetY: 3; strength: window.mc.shadow }
                    Rectangle { anchors.fill: parent; radius: width / 2
                                color: parent.parent.down ? Qt.darker(window.Material.accent, 1.2) : window.Material.accent }
                }
                contentItem: Item {
                    Icon { anchors.centerIn: parent; width: 26; height: 26; kind: "plus"
                           colour: window.monochrome ? window.theme.background : "#ffffff" }
                }
                onClicked: mission.addStation()
            }
        }

        // ---- mission ------------------------------------------------------------
        Flickable {
            contentWidth: width
            contentHeight: missionColumn.implicitHeight + 24
            clip: true
            ScrollIndicator.vertical: ScrollIndicator {}
            ColumnLayout {
                id: missionColumn
                x: 8
                y: 8
                width: parent.width - 16
                spacing: 10
                TimerView { Layout.fillWidth: true; big: true }
                InfoView { Layout.fillWidth: true; big: true }
                Card {
                    title: qsTr("Beacon")
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.family: mission.hasBeacon ? "monospace" : ""
                        text: mission.hasBeacon
                              ? mission.beaconLat.toFixed(6) + ", " + mission.beaconLon.toFixed(6) + "\n"
                                + mission.dmsText(mission.beaconLat, mission.beaconLon) + "\n"
                                + "MGRS: " + mission.mgrs(mission.beaconLat, mission.beaconLon)
                              : qsTr("Not placed. Press and hold the map, or set it here.")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppButton { text: qsTr("Set"); Layout.fillWidth: true; onClicked: window.setBeacon() }
                        AppButton { text: qsTr("Clear"); Layout.fillWidth: true; enabled: mission.hasBeacon
                                 onClicked: { mission.clearBeacon(); window.showMessage(mission.statusMessage) } }
                    }
                }
                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Generate PDF report")
                    onClicked: window.generatePdf()
                }
            }
        }

        // ---- settings -------------------------------------------------------------
        Flickable {
            contentWidth: width
            contentHeight: settingsColumn.implicitHeight + 24
            clip: true
            ScrollIndicator.vertical: ScrollIndicator {}
            ColumnLayout {
                id: settingsColumn
                x: 8
                y: 8
                width: parent.width - 16
                spacing: 10
                MapSettingsView { Layout.fillWidth: true; compact: false }
                Card {
                    title: qsTr("Preferences")
                    Layout.fillWidth: true
                    PreferencesView { Layout.fillWidth: true }
                }
                Label {
                    Layout.topMargin: 6
                    text: "APRS"
                    font.bold: true
                    font.pixelSize: 16
                }
                AprsView { id: phoneAprs; objectName: "phoneAprsView"; Layout.fillWidth: true }
            }
        }
    }

    footer: TabBar {
        id: tabs
        objectName: "tabs"
        Material.elevation: window.flat ? 0 : 8
        background: Rectangle {
            color: window.mc.card
            Rectangle { width: parent.width; height: 1; color: window.mc.edge }
        }
        Repeater {
            model: [
                { "label": qsTr("Map"), "icon": "map" },
                { "label": qsTr("Stations"), "icon": "stations" },
                { "label": qsTr("Mission"), "icon": "mission" },
                { "label": qsTr("Settings"), "icon": "settings" }
            ]
            TabButton {
                id: tab
                required property var modelData
                required property int index
                height: 58
                contentItem: ColumnLayout {
                    spacing: 2
                    Icon {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        kind: tab.modelData.icon
                        colour: tab.checked ? window.Material.accent : window.Material.foreground
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: tab.modelData.label
                        font.pixelSize: 12
                        color: tab.checked ? window.Material.accent : window.Material.foreground
                        elide: Text.ElideRight
                        Layout.maximumWidth: tab.width - 4
                    }
                }
            }
        }
    }
}
