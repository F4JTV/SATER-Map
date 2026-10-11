// NorthButton.qml - the north a bearing is read from: T, true north (a map,
// a GPS heading), or M, magnetic north (a compass), which the application
// corrects by the declination.  Click to switch.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

DirButton {
    id: nb
    property bool magnetic: false
    property real declination: 0
    signal switched()

    objectName: "northButton"
    text: magnetic ? qsTr("M") : qsTr("T")
    Material.foreground: magnetic ? window.Material.accent : window.Material.foreground
    onClicked: switched()
    hoverEnabled: true
    ToolTip.visible: hovered && !app.android
    ToolTip.delay: 600
    ToolTip.text: magnetic
                  ? qsTr("Magnetic north (compass): corrected by the declination, %1. Click for true north.")
                        .arg(mission.declinationText(declination))
                  : qsTr("True north (map, GPS). Click if the bearing was read on a compass.")
}
