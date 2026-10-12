// AprsView.qml - APRS for the operation: this station, the operation's
// callsigns (only their bearings are put on the map), the connections
// (APRS-IS, a KISS TNC on the network, the modem built into the program
// when there is one), the packet log.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: view
    readonly property var aprs: mission.aprs
    property bool big: app.android
    spacing: 10

    function addCallsign() {
        if (view.aprs.addTeamCallsign(teamField.text)) teamField.text = ""
        else if (teamField.text.length > 0) window.showMessage(qsTr("Not a valid callsign, or already in the list."))
    }

    Card {
        title: qsTr("This station")
        Layout.fillWidth: true
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Label { text: qsTr("Callsign:") }
            AppField {
                objectName: "aprsCallsign"
                Layout.fillWidth: true
                hint: "F4ABC-7"
                text: view.aprs.myCallsign
                font.bold: true
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onEditingFinished: view.aprs.myCallsign = text
            }
            Label { text: qsTr("APRS-IS passcode:") }
            AppField {
                objectName: "aprsPasscode"
                Layout.fillWidth: true
                hint: qsTr("none: receive only")
                text: view.aprs.passcode
                inputMethodHints: Qt.ImhDigitsOnly
                validator: RegularExpressionValidator { regularExpression: /-?\d{0,5}/ }
                onEditingFinished: view.aprs.passcode = text
            }
            Label { text: qsTr("Radio path:") }
            AppField {
                objectName: "aprsPath"
                Layout.fillWidth: true
                text: view.aprs.rfPath
                hint: "WIDE1-1,WIDE2-1"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onEditingFinished: view.aprs.rfPath = text
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Bearings go out under this callsign, from the station of the same callsign in the list. The passcode lets it send on APRS-IS; the path is the digipeaters asked for by radio.")
        }
    }

    Card {
        title: qsTr("Operation callsigns")
        Layout.fillWidth: true
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: mission.team
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    objectName: "teamChip"
                    height: 30
                    width: chipRow.implicitWidth + 16
                    radius: 15
                    color: window.mc.field
                    border.width: 1
                    border.color: window.mc.fieldEdge
                    RowLayout {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 2
                        Label { text: modelData; font.bold: true; font.pixelSize: 13; leftPadding: 4 }
                        IconButton {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            padding: 4
                            iconKind: "close"
                            iconSize: 12
                            tip: qsTr("Remove from the operation")
                            onClicked: view.aprs.removeTeamCallsign(index)
                        }
                    }
                }
            }
            Label {
                visible: mission.team.length === 0
                text: qsTr("No callsign yet: no bearing is taken in.")
                color: window.colourOf("error")
                font.pixelSize: 12
                height: 30
                verticalAlignment: Text.AlignVCenter
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            AppField {
                id: teamField
                objectName: "teamField"
                Layout.fillWidth: true
                hint: "F4ABC"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onTextEdited: {
                    var up = text.toUpperCase()
                    if (up !== text) { var c = cursorPosition; text = up; cursorPosition = c }
                }
                onAccepted: view.addCallsign()
            }
            AppButton {
                objectName: "teamAdd"
                // Never narrower than its word: the field gives way.
                Layout.minimumWidth: implicitWidth
                text: qsTr("Add")
                onClicked: view.addCallsign()
            }
        }
        AppButton {
            objectName: "teamFromTable"
            Layout.fillWidth: true
            flat: true
            text: qsTr("Add the stations' callsigns")
            onClicked: {
                var n = view.aprs.addTeamFromStations()
                window.showMessage(qsTr("%1 callsign(s) added").arg(n))
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Only the bearings of these callsigns are put on the map; a callsign without SSID stands for all its SSIDs. A station not yet in the table is added.")
        }
    }

    Card {
        title: qsTr("APRS-IS (internet)")
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Switch {
                objectName: "aprsIsSwitch"
                checked: view.aprs.isEnabled
                onToggled: view.aprs.isEnabled = checked
            }
            AprsStatus {
                Layout.fillWidth: true
                name: "APRS-IS"
                state: view.aprs.isState
                detail: view.aprs.isDetail
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 8
            Label { text: qsTr("Server:") }
            AppField {
                Layout.fillWidth: true
                text: view.aprs.isHost
                hint: "rotate.aprs2.net"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
                onEditingFinished: view.aprs.isHost = text
            }
            Label { text: qsTr("Port:") }
            NumberField {
                Layout.preferredWidth: 76
                compact: false
                from: 1; to: 65535
                value: view.aprs.isPort
                onEdited: function(v) { view.aprs.isPort = v }
            }
        }
    }

    Card {
        title: qsTr("Radio: KISS TNC on the network")
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Switch {
                objectName: "kissSwitch"
                checked: view.aprs.kissEnabled
                onToggled: view.aprs.kissEnabled = checked
            }
            AprsStatus {
                Layout.fillWidth: true
                name: "KISS"
                state: view.aprs.kissState
                detail: view.aprs.kissDetail
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 8
            Label { text: qsTr("Host:") }
            AppField {
                Layout.fillWidth: true
                text: view.aprs.kissHost
                hint: "127.0.0.1"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
                onEditingFinished: view.aprs.kissHost = text
            }
            Label { text: qsTr("Port:") }
            NumberField {
                Layout.preferredWidth: 76
                compact: false
                from: 1; to: 65535
                value: view.aprs.kissPort
                onEdited: function(v) { view.aprs.kissPort = v }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Dire Wolf listens on port 8001, UZ7HO Soundmodem on 8100.")
        }
    }

    Card {
        objectName: "modemCard"
        visible: view.aprs.modemAvailable
        title: qsTr("Radio: built-in modem")
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Switch {
                objectName: "modemSwitch"
                checked: view.aprs.modemEnabled
                onToggled: view.aprs.modemEnabled = checked
            }
            AprsStatus {
                Layout.fillWidth: true
                name: qsTr("Modem")
                state: view.aprs.modemState
                detail: view.aprs.modemDetail
            }
            // The transmitter keyed.
            Rectangle {
                visible: view.aprs.modemTransmitting
                Layout.preferredWidth: txLabel.implicitWidth + 12
                Layout.preferredHeight: 20
                radius: 4
                color: window.colourOf("error")
                Label { id: txLabel; anchors.centerIn: parent; text: "TX"; font.bold: true; font.pixelSize: 12; color: "#ffffff" }
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            // On a phone the system routes the audio to the USB sound card
            // (or the headset jack) by itself: nothing to choose.
            Label { text: qsTr("Sound card:"); visible: !app.android }
            AppComboBox {
                id: cardBox
                objectName: "modemSoundCard"
                visible: !app.android
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                // The default's name in the interface's language, now.
                model: view.aprs.modemAudioDevices().map(function(d) {
                    return d.value === "default" ? { value: d.value, label: qsTr("System default") } : d
                })
                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(view.aprs.modemAudioIn))
                onActivated: {
                    view.aprs.modemAudioIn = currentValue
                    view.aprs.modemAudioOut = currentValue
                }
            }
            Label { text: qsTr("PTT:") }
            AppComboBox {
                id: pttBox
                objectName: "modemPtt"
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: [
                    { value: "none", label: qsTr("VOX (no PTT line)") },
                    { value: "cm108", label: qsTr("USB sound card GPIO (CM108)") },
                    { value: "rts", label: qsTr("Serial adapter, RTS line") },
                    { value: "dtr", label: qsTr("Serial adapter, DTR line") }
                ]
                currentIndex: Math.max(0, indexOfValue(view.aprs.modemPtt))
                onActivated: view.aprs.modemPtt = currentValue
            }
            Label { text: qsTr("GPIO:"); visible: view.aprs.modemPtt === "cm108" }
            NumberField {
                visible: view.aprs.modemPtt === "cm108"
                Layout.preferredWidth: 96
                compact: false
                from: 1; to: 8
                value: view.aprs.modemGpio
                onEdited: function(v) { view.aprs.modemGpio = v }
            }
            Label { text: qsTr("Serial port:"); visible: serialField.visible }
            AppField {
                id: serialField
                visible: !app.android && (view.aprs.modemPtt === "rts" || view.aprs.modemPtt === "dtr")
                Layout.fillWidth: true
                text: view.aprs.modemPttDevice
                hint: Qt.platform.os === "windows" ? "COM3" : "/dev/ttyUSB0"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onEditingFinished: view.aprs.modemPttDevice = text
            }
            Label { text: qsTr("TX delay:") }
            NumberField {
                objectName: "modemTxDelay"
                Layout.preferredWidth: 96
                compact: false
                from: 0; to: 1000; step: 10
                suffix: " ms"
                value: view.aprs.modemTxDelay
                onEdited: function(v) { view.aprs.modemTxDelay = v }
            }
            Label { text: qsTr("TX tail:") }
            NumberField {
                Layout.preferredWidth: 96
                compact: false
                from: 0; to: 500; step: 10
                suffix: " ms"
                value: view.aprs.modemTxTail
                onEdited: function(v) { view.aprs.modemTxTail = v }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: app.android
                  ? qsTr("1200 baud APRS through the radio's audio interface: a USB sound card (Digirig and the like) or the headset jack. The transmitter is keyed by the interface's VOX, the sound card's GPIO or a USB serial adapter; Android asks once for the USB device and for the microphone.")
                  : qsTr("1200 baud APRS through the radio's audio interface (a USB sound card such as a Digirig). The transmitter is keyed by the interface's VOX, the sound card's GPIO or the RTS/DTR line of a serial adapter.")
        }
    }

    // Android: APRS goes on minimised or with the phone locked (a
    // foreground service); battery saving must leave it its network.
    Card {
        objectName: "backgroundCard"
        visible: app.android
        title: qsTr("In the background")
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: app.backgroundAllowed ? window.colourOf("dim") : window.colourOf("error")
            text: app.backgroundAllowed
                  ? qsTr("APRS keeps working when SATER Map is minimised or the phone locked, as long as a connection is on (a notification says so).")
                  : qsTr("Battery saving may cut the network when the phone is locked: allow SATER Map to run in the background.")
        }
        AppButton {
            visible: !app.backgroundAllowed
            Layout.fillWidth: true
            text: qsTr("Allow in the background")
            onClicked: app.allowBackground()
        }
    }

    AppButton {
        objectName: "aprsLogButton"
        Layout.fillWidth: true
        text: qsTr("Packet log (%1)").arg(view.aprs.log.length)
        onClicked: window.showAprsLog()
    }
}
