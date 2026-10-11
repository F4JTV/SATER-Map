// TimerView.qml - the mission timer: an LCD-style display, Start, Stop,
// Reset, and the time the mission started (the Python MissionTimer).
// Start after Stop carries on from the same start time: the display is the
// time since the mission began.  big: the phone's layout, the display on
// a line of its own above the buttons.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Card {
    id: card
    title: qsTr("Mission timer")
    property bool big: false

    component Lcd: Rectangle {
        radius: 4
        color: window.mc.lcdBg
        Label {
            objectName: "elapsedLabel"
            anchors.centerIn: parent
            text: mission.elapsedText
            color: window.mc.lcdText
            font.family: "monospace"
            font.pixelSize: card.big ? 40 : 24
            font.bold: true
        }
    }
    component TimerButton: RoundButton {
        id: tb
        property string iconKind
        property string tip
        Layout.preferredWidth: card.big ? -1 : 52
        Layout.fillWidth: card.big
        Layout.preferredHeight: card.big ? 52 : 40
        background: Item {
            SoftShadow { anchors.fill: parent; radius: 6; spread: 2.5; offsetY: 1.5; strength: tb.enabled ? window.mc.shadow * 0.7 : 0 }
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: !tb.enabled ? window.mc.edge : tb.down ? Qt.darker(window.mc.button, 1.18) : window.mc.button
            }
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: card.big ? 22 : 16
                height: width
                kind: tb.iconKind
                colour: tb.enabled ? window.Material.foreground : window.Material.hintTextColor
            }
        }
        ToolTip.visible: tip.length > 0 && hovered
        ToolTip.text: tip
        ToolTip.delay: 600
    }
    function reset() {
        if (mission.timerStarted && card.big)
            window.confirm(qsTr("Reset"), qsTr("Reset the mission timer?"), function() { mission.resetTimer() })
        else mission.resetTimer()
    }

    // Desktop: one row, as the Python widget.
    RowLayout {
        visible: !card.big
        Layout.fillWidth: true
        spacing: 8
        Lcd { Layout.fillWidth: true; Layout.preferredHeight: 38; Layout.minimumWidth: 110 }
        TimerButton { objectName: "timerStart"; iconKind: "play"; tip: qsTr("Start"); enabled: !mission.timerRunning; onClicked: mission.startTimer() }
        TimerButton { objectName: "timerStop"; iconKind: "stop"; tip: qsTr("Stop"); enabled: mission.timerRunning; onClicked: mission.stopTimer() }
        TimerButton { objectName: "timerReset"; iconKind: "reset"; tip: qsTr("Reset"); onClicked: card.reset() }
        Label {
            Layout.preferredWidth: 96
            text: mission.startText
            color: window.colourOf("dim")
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    // Phone: the display, then the buttons.
    Lcd { visible: card.big; Layout.fillWidth: true; Layout.preferredHeight: 64 }
    RowLayout {
        visible: card.big
        Layout.fillWidth: true
        spacing: 10
        TimerButton { iconKind: "play"; tip: qsTr("Start"); enabled: !mission.timerRunning; onClicked: mission.startTimer() }
        TimerButton { iconKind: "stop"; tip: qsTr("Stop"); enabled: mission.timerRunning; onClicked: mission.stopTimer() }
        TimerButton { iconKind: "reset"; tip: qsTr("Reset"); onClicked: card.reset() }
    }
    Label {
        visible: card.big && mission.startText.length > 0
        text: mission.startText
        color: window.colourOf("dim")
        font.pixelSize: 13
    }
}
