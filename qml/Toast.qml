// Toast.qml - a short message at the bottom of the window (the phone's
// stand-in for the desktop status bar).  As Orange TV Remote's.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

Rectangle {
    id: toast
    property alias text: toastLabel.text
    z: 1000
    parent: Overlay.overlay
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 84
    width: Math.min((parent ? parent.width : 400) - 32, toastLabel.implicitWidth + 32)
    height: toastLabel.implicitHeight + 20
    radius: 10
    color: window.mc.toast
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 180 } }

    Label {
        id: toastLabel
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.parent ? toast.parent.width - 64 : 300)
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: window.mc.toastText
        font.pixelSize: 14
    }
    Timer { id: toastTimer; interval: 4000; onTriggered: toast.opacity = 0 }
    TapHandler { onTapped: toast.opacity = 0 }

    function show(message) {
        if (!message || message.length === 0) return
        toastLabel.text = message
        opacity = 1
        toastTimer.restart()
    }
}
