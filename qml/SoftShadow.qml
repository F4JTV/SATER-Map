// SoftShadow.qml - a drop shadow made of stacked translucent rounded
// rectangles. Plain rectangles draw the same on every renderer (GPU or
// software) and on every Qt 6, which a shader effect does not.
//
// Placed behind a rectangle of the same size and radius; the part under the
// rectangle is hidden by it, the edge fades out over "spread".
//
// This file is part of SATER Map (from AX25Chat).
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    id: shadow
    property real radius: 8
    property real spread: 6       // how far the shadow reaches past the edge
    property real offsetY: 3      // light from above: the shadow falls down
    property real strength: 0.3   // opacity where the layers all overlap
    property color colour: "#000000"
    readonly property int layers: 5

    Repeater {
        model: shadow.layers
        Rectangle {
            readonly property real grow: shadow.spread * (index + 1) / shadow.layers
            x: -grow
            y: -grow + shadow.offsetY
            width: shadow.width + 2 * grow
            height: shadow.height + 2 * grow
            radius: shadow.radius + grow
            color: Qt.rgba(shadow.colour.r, shadow.colour.g, shadow.colour.b, shadow.strength / shadow.layers)
        }
    }
}
