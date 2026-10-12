// AppDialog.qml - the modal dialog every window of the application uses:
// centred, as wide as it asks up to the window's width, scrolling when the
// content is taller than the window (a phone in landscape).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Dialog {
    id: dialog
    property real preferredWidth: 440
    // Take the whole height available (lists, tables).
    property bool fillHeight: false
    default property alias content: holder.data

    parent: Overlay.overlay
    modal: true
    focus: true
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(parent.width - 16, preferredWidth)
    height: fillHeight ? parent.height - 24 : Math.min(implicitHeight, parent.height - 24)
    padding: 16
    topPadding: 8
    Material.background: window.mc.card
    // Drawn here: Material's own is an elevation layer, empty on the
    // software renderer.
    background: Item {
        // Material's title bar (Qt 6.5 and later) rounds its corners with
        // background.radius: an Item has none, and each dialog warned
        // "Unable to assign [undefined] to double" at start.
        readonly property real radius: 8
        SoftShadow { anchors.fill: parent; radius: 8; spread: 12; offsetY: 6; strength: window.mc.shadow }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: window.mc.card
            border.width: window.flat ? 1 : 0
            border.color: window.mc.edge
        }
    }

    // The standard buttons in the interface's language: Qt names them once,
    // in its own language, when they are made.
    function retranslateButtons() {
        var names = [[Dialog.Ok, qsTr("OK")], [Dialog.Cancel, qsTr("Cancel")], [Dialog.Close, qsTr("Close")],
                     [Dialog.Yes, qsTr("Yes")], [Dialog.No, qsTr("No")]]
        for (var i = 0; i < names.length; ++i) {
            var b = dialog.standardButton(names[i][0])
            if (b) b.text = names[i][1]
        }
    }
    onAboutToShow: retranslateButtons()
    // Qt 6.4 (Ubuntu 24.04): the layouts of a closed dialog do not follow
    // what changed meanwhile (the language: "Add" became "Ajouter") and
    // keep the old sizes once it opens.  A width change makes them lay
    // out again.
    onOpened: {
        holder.width = flick.width - 1
        holder.width = Qt.binding(function() { return flick.width })
    }

    contentItem: Flickable {
        id: flick
        clip: true
        contentWidth: width
        contentHeight: dialog.fillHeight ? height : holder.childrenRect.height
        implicitHeight: holder.childrenRect.height
        boundsBehavior: Flickable.StopAtBounds
        interactive: !dialog.fillHeight && contentHeight > height
        ScrollIndicator.vertical: ScrollIndicator {}
        Item {
            id: holder
            width: flick.width
            height: dialog.fillHeight ? flick.height : childrenRect.height
        }
    }
}
