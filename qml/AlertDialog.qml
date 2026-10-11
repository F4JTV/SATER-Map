// AlertDialog.qml - a message, or a yes/no question (question: true).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

AppDialog {
    id: dialog
    property alias text: label.text
    property bool question: false
    preferredWidth: 460
    standardButtons: question ? (Dialog.Yes | Dialog.No) : Dialog.Ok

    Label {
        id: label
        objectName: "alertText"
        width: parent.width
        wrapMode: Text.Wrap
        font.pixelSize: 14
    }
}
