// BeaconDialog.qml - where the distress beacon is (the Python "Set beacon
// position" dialog): decimal degrees or DMS, or the centre of the
// intersection zone.  The beacon can also be dragged on the map.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("Distress beacon position")
    preferredWidth: 480
    standardButtons: Dialog.Ok | Dialog.Cancel

    function openAt(lat, lon) {
        if (lat !== undefined) {
            position.lat = lat
            position.lon = lon
        } else if (mission.hasBeacon) {
            position.lat = mission.beaconLat
            position.lon = mission.beaconLon
        } else {
            // The Python default (Nice).
            position.lat = 43.7
            position.lon = 7.25
        }
        position.reload()
        open()
    }
    onAccepted: {
        mission.setBeacon(position.lat, position.lon)
        window.showMessage(mission.statusMessage)
    }

    ColumnLayout {
        width: parent.width
        spacing: 10
        Label { text: qsTr("Enter the beacon's coordinates:"); font.bold: true }
        DmsEditor { id: position; Layout.fillWidth: true }
        AppButton {
            objectName: "useEstimate"
            Layout.fillWidth: true
            visible: mission.hasFix
            text: qsTr("Use the best estimate")
            onClicked: {
                position.lat = mission.fixLat
                position.lon = mission.fixLon
                position.reload()
            }
        }
        AppButton {
            objectName: "useZoneCentre"
            Layout.fillWidth: true
            visible: mission.hasZone
            text: qsTr("Use the centre of the intersection zone")
            onClicked: {
                position.lat = mission.zoneLat
                position.lon = mission.zoneLon
                position.reload()
            }
        }
        AppButton {
            Layout.fillWidth: true
            flat: true
            visible: window.mapView !== null
            text: qsTr("Use the centre of the map")
            onClicked: {
                position.lat = window.mapView.centerLat
                position.lon = window.mapView.centerLon
                position.reload()
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.italic: true
            font.pixelSize: 12
            text: qsTr("The beacon can also be dragged on the map.")
        }
    }
}
