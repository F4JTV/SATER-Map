// StationCard.qml - one station on the phone: the bearing first (large,
// with -/+ steps), signal and uncertainty, the record button; callsign,
// colour and visibility on top; the position below, opened in the editor.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: card
    required property int index
    required property string callsign
    required property string latText
    required property string lonText
    required property string signal
    required property real azimuth
    required property real uncertainty
    required property string stationColor
    required property bool shown
    required property bool magnetic
    required property real declination
    // See StationTable: a callsign changed elsewhere is shown here too.
    onCallsignChanged: if (!callField.activeFocus) callField.text = callsign
    readonly property var stations: mission.stations

    padding: 12
    Material.elevation: window.flat ? 0 : 2
    Material.background: window.mc.card
    background: Rectangle {
        color: window.mc.card
        radius: 12
        border.width: window.flat ? 1 : 0
        border.color: window.mc.edge
        Rectangle {
            width: 6
            height: parent.height - 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 3
            radius: 3
            color: card.stationColor
        }
    }

    function step(delta) {
        var v = (Math.round(card.azimuth) + delta) % 360
        if (v < 0) v += 360
        stations.setField(card.index, "azimuth", v)
        app.haptic()
    }

    ColumnLayout {
        x: 6
        width: parent.width - 6
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ColorButton {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                colour: card.stationColor
                onChosen: function(c) { card.stations.setField(card.index, "stationColor", c) }
            }
            AppField {
                id: callField
                objectName: "callsignField"
                Layout.fillWidth: true
                font.pixelSize: 18
                font.bold: true
                color: card.stationColor
                hint: qsTr("Callsign")
                Component.onCompleted: text = card.callsign
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextEdited: {
                    var up = text.toUpperCase()
                    if (up !== text) { var c = cursorPosition; text = up; cursorPosition = c }
                    card.stations.setField(card.index, "callsign", text)
                }
            }
            Switch {
                objectName: "visibleSwitch"
                checked: card.shown
                onToggled: card.stations.setField(card.index, "shown", checked)
            }
            ToolButton {
                contentItem: Item {
                    implicitWidth: 22
                    implicitHeight: 22
                    Icon { anchors.centerIn: parent; width: 20; height: 20; kind: "more"; colour: window.Material.foreground }
                }
                onClicked: cardMenu.popup(this, width - cardMenu.width, height)
                AppMenu {
                    id: cardMenu
                    // Another station's bearing (read out by telephone): out
                    // by APRS as an object named after it.  This station's
                    // own has its button on the card.
                    MenuItem {
                        objectName: "relayItem"
                        visible: mission.aprs.myCallsign.length > 0 && !mission.aprs.isMine(card.callsign)
                        height: visible ? implicitHeight : 0
                        enabled: card.signal !== "S0" && card.callsign.trim().length > 0
                        text: qsTr("Send by APRS")
                        onTriggered: {
                            mission.aprs.sendBearing(card.index)
                            window.showMessage(mission.statusMessage)
                            app.haptic()
                        }
                    }
                    MenuItem { text: qsTr("Edit the position"); onTriggered: window.editStation(card.index) }
                    MenuItem {
                        text: qsTr("Show on the map")
                        onTriggered: {
                            var s = card.stations.get(card.index)
                            window.viewLat = s.lat
                            window.viewLon = s.lon
                            if (window.mapView) window.mapView.centerOn(s.lat, s.lon, Math.max(window.mapView.zoom, 12))
                            layoutLoader.item.pageIndex = 0
                        }
                    }
                    MenuItem { text: qsTr("Save as preset"); onTriggered: window.saveStationAsPreset(card.index) }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Delete")
                        onTriggered: window.confirm(qsTr("Confirmation"), qsTr("Delete station %1?").arg(card.callsign),
                                                    function() { card.stations.remove(card.index) })
                    }
                }
            }
        }

        // The bearing: what changes most during a mission.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            component StepButton: RoundButton {
                property int delta
                Layout.preferredWidth: 46
                Layout.preferredHeight: 46
                padding: 0
                font.pixelSize: 15
                font.bold: true
                text: (delta > 0 ? "+" : "−") + Math.abs(delta)
                autoRepeat: true
                autoRepeatDelay: 450
                autoRepeatInterval: 90
                background: Item {
                    SoftShadow { anchors.fill: parent; radius: width / 2; spread: 2; offsetY: 1.5; strength: window.mc.shadow * 0.6 }
                    Rectangle { anchors.fill: parent; radius: width / 2
                                color: parent.parent.down ? Qt.darker(window.mc.button, 1.2) : window.mc.button }
                }
                onClicked: card.step(delta)
            }
            StepButton { delta: -10 }
            StepButton { delta: -1 }
            NumberField {
                objectName: "azimuthField"
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                compact: false
                horizontalAlignment: TextInput.AlignHCenter
                font.pixelSize: 28
                font.bold: true
                from: 0; to: 359; wrap: true
                suffix: "°"
                value: card.azimuth
                onEdited: function(v) { card.stations.setField(card.index, "azimuth", v) }
            }
            StepButton { delta: 1 }
            StepButton { delta: 10 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            AppComboBox {
                objectName: "signalBox"
                Layout.preferredWidth: 96
                Layout.minimumWidth: 88
                model: card.stations.signalLevels
                currentIndex: Math.max(0, card.stations.signalLevels.indexOf(card.signal))
                onActivated: card.stations.setField(card.index, "signal", currentText)
            }
            NumberField {
                Layout.preferredWidth: 84
                Layout.minimumWidth: 76
                compact: false
                horizontalAlignment: TextInput.AlignHCenter
                from: 0; to: 30; decimals: 1; step: 0.5
                prefix: "±"; suffix: "°"
                value: card.uncertainty
                onEdited: function(v) { card.stations.setField(card.index, "uncertainty", v) }
            }
            NorthButton {
                compact: false
                Layout.preferredWidth: 44
                magnetic: card.magnetic
                declination: card.declination
                onSwitched: { card.stations.setField(card.index, "magnetic", !card.magnetic); app.haptic() }
            }
            AppButton {
                id: recordButton
                objectName: "recordButton"
                Layout.fillWidth: true
                sidePadding: 8
                highlighted: true
                Material.accent: window.Material.accent
                onClicked: { mission.recordStation(card.index); window.showMessage(mission.statusMessage); app.haptic() }
                contentItem: Item {
                    implicitWidth: recordRow.implicitWidth
                    implicitHeight: recordRow.implicitHeight
                    Row {
                        id: recordRow
                        anchors.centerIn: parent
                        spacing: 6
                        Icon { kind: "record"; width: 18; height: 18; anchors.verticalCenter: parent.verticalCenter
                               colour: window.monochrome ? window.theme.background : "#ffffff" }
                        // The word when it fits in the room the button's
                        // padding leaves (a narrow phone in French: the
                        // icon alone).
                        Label { id: recordLabel; text: qsTr("Record"); font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                                visible: recordButton.availableWidth >= implicitWidth + 18 + recordRow.spacing
                                color: window.monochrome ? window.theme.background : "#ffffff" }
                    }
                }
            }
        }

        // The position: tap to edit.  On this station's own card, the
        // bearing goes out by APRS from here.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: card.latText + "   " + card.lonText
                font.family: "monospace"
                font.pixelSize: 13
                color: window.colourOf("dim")
                elide: Text.ElideRight
                MouseArea { anchors.fill: parent; onClicked: window.editStation(card.index) }
            }
            AppButton {
                objectName: "sendButton"
                // myCallsign read first: the binding follows it.
                visible: mission.aprs.myCallsign.length > 0 && mission.aprs.isMine(card.callsign)
                enabled: card.signal !== "S0"
                highlighted: mission.aprs.canSend
                onClicked: {
                    mission.aprs.sendBearing(card.index)
                    window.showMessage(mission.statusMessage)
                    app.haptic()
                }
                contentItem: RowLayout {
                    spacing: 6
                    Icon { kind: "send"; Layout.preferredWidth: 16; Layout.preferredHeight: 16
                           colour: mission.aprs.canSend && !window.monochrome ? "#ffffff" : window.Material.foreground }
                    Label { text: qsTr("Send"); font.bold: true
                            color: mission.aprs.canSend && !window.monochrome ? "#ffffff" : window.Material.foreground }
                }
            }
        }
    }
}
