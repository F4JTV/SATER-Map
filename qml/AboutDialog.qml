// AboutDialog.qml - the program, its version, what it is for.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("About")
    preferredWidth: 440
    standardButtons: Dialog.Ok

    ColumnLayout {
        width: parent.width
        spacing: 8
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/assets/logo.png"
            sourceSize.width: 96
            sourceSize.height: 96
            opacity: window.monochrome ? 0.6 : 1
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "SATER Map"
            font.pixelSize: 22
            font.bold: true
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Version %1").arg(app.version)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: mission.organization.length > 0
            text: mission.organization
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Direction finding for the search of distress beacons (SATER missions).")
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: window.mc.edge }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            text: qsTr("Free software (GPL-3.0-or-later). Qt %1.").arg(app.qtVersion)
                  + "\n" + qsTr("Map data: OpenStreetMap contributors, OpenTopoMap, IGN Géoplateforme, Esri.")
        }
    }
}
