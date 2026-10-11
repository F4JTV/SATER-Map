// AppMenu.qml - a menu whose panel is drawn here rather than by Material's
// elevation layer, which the software scene graph leaves empty (the items
// then float over the page with nothing behind them).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Menu {
    background: Item {
        implicitWidth: 220
        implicitHeight: 40
        SoftShadow { anchors.fill: parent; radius: 6; spread: 6; offsetY: 3; strength: window.mc.shadow }
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: window.mc.card
            border.width: 1
            border.color: window.mc.edge
        }
    }
}
