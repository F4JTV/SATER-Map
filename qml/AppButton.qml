// AppButton.qml - a push button that looks like one in every theme and on
// every renderer.
//
// Material's own button background is drawn through a layer effect (its
// elevation shadow), which the software scene graph cannot draw: the button
// then has no background at all.  And Material paints it with the inherited
// Material.background, which the window sets for the themes, so it came out
// the colour of the panel it sits on.  This one draws its own: the theme's
// button colour (the accent when highlighted), a soft shadow under it.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control
    readonly property color fill: highlighted ? window.Material.accent : window.mc.button
    readonly property color ink: window.Material.foreground
    Material.foreground: highlighted && !flat ? (window.monochrome ? window.theme.background : "#ffffff") : window.Material.foreground

    background: Item {
        implicitWidth: 64
        implicitHeight: app.android ? 44 : 34
        SoftShadow {
            anchors.fill: parent
            visible: !control.flat && control.enabled
            radius: 5
            spread: 2.5
            offsetY: 1.5
            strength: window.mc.shadow * 0.7
        }
        Rectangle {
            anchors.fill: parent
            radius: 5
            color: control.flat
                   ? (control.down ? Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.14)
                      : control.hovered ? Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.07) : "transparent")
                   : !control.enabled ? window.mc.edge
                   : control.down ? Qt.darker(control.fill, 1.18)
                   : control.hovered ? Qt.lighter(control.fill, 1.06) : control.fill
            border.width: control.visualFocus ? 2 : 0
            border.color: window.Material.accent
        }
    }
}
