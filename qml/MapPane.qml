// MapPane.qml - the map and what sits on it: the stations' labels and the
// beacon (dragged to move them, tapped for their details), the zoom
// buttons, the scale and the attribution, and the popups the map opens
// (coordinates of a point, the zone, an intersection point).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import SaterMap.Native

Item {
    id: pane
    property bool phone: false
    property alias map: mapView
    // The context's "mission", under a name MapView's own "mission"
    // property does not hide.
    readonly property QtObject theMission: mission
    clip: true

    MapView {
        id: mapView
        objectName: "mapView"
        anchors.fill: parent
        mission: pane.theMission
        primary: true
        backgroundColor: window.mc.mapBg
        tileTint: window.mc.tint
        tileDim: window.mc.dim
        Component.onCompleted: {
            centerOn(window.viewLat, window.viewLon, window.viewZoom)
            window.mapView = mapView
        }
        Component.onDestruction: if (window.mapView === mapView) window.mapView = null
        onViewChanged: {
            window.viewLat = centerLat
            window.viewLon = centerLon
            window.viewZoom = Math.round(zoom)
        }
        onTapped: function(x, y) {
            var hit = hitTest(x, y)
            if (hit.kind === "point") {
                infoPopup.show(x, y, qsTr("Intersection %1").arg(hit.index + 1),
                               coordinateLines(hit.lat, hit.lon), "")
            } else if (hit.kind === "estimate") {
                infoPopup.show(x, y, qsTr("Best estimate"),
                               qsTr("DD: %1, %2").arg(mission.fixLat.toFixed(6)).arg(mission.fixLon.toFixed(6)) + "\n"
                               + qsTr("DMS: %1").arg(mission.dmsText(mission.fixLat, mission.fixLon)) + "\n"
                               + "MGRS: " + mission.fixMgrs + "\n"
                               + qsTr("95 % ellipse: %1 × %2 km, axis %3°").arg(mission.fixMajorKm.toFixed(2))
                                     .arg(mission.fixMinorKm.toFixed(2)).arg(Math.round(mission.fixAxisDeg) % 180)
                               + (mission.fixWarning.length > 0 ? "\n\n" + mission.fixWarning : ""), "")
            } else if (hit.kind === "zone") {
                infoPopup.show(x, y, qsTr("Intersection zone"),
                               qsTr("Centre DD: %1, %2").arg(mission.zoneLat.toFixed(6)).arg(mission.zoneLon.toFixed(6)) + "\n"
                               + qsTr("Centre DMS: %1").arg(mission.dmsText(mission.zoneLat, mission.zoneLon)) + "\n"
                               + qsTr("Radius: %1 km").arg(mission.zoneRadiusKm.toFixed(2)) + "\n"
                               + qsTr("Surface: %1 km²").arg(mission.zoneSurfaceKm2.toFixed(2)) + "\n"
                               + qsTr("Points: %1").arg(mission.zonePointCount) + "\n"
                               + "MGRS: " + mission.zoneMgrs, "")
            } else {
                infoPopup.close()
            }
        }
        onContextRequested: function(x, y, lat, lon) {
            app.haptic()
            contextPopup.lat = lat
            contextPopup.lon = lon
            contextPopup.place(x, y)
        }
    }

    function coordinateLines(lat, lon) {
        return "DD: " + lat.toFixed(6) + ", " + lon.toFixed(6) + "\n"
             + "DMS: " + mission.dmsText(lat, lon)
    }

    // ---- station labels -------------------------------------------------------
    Repeater {
        id: markers
        model: mission.stations
        delegate: Item {
            id: marker
            required property int index
            required property string callsign
            required property real lat
            required property real lon
            required property string stationColor
            required property bool shown
            required property bool valid
            required property real azimuth
            required property real uncertainty
            required property string signal
            visible: valid && shown
            // The label's left edge sits on the station, its middle on the
            // line (Leaflet's iconAnchor [0, 12]).
            readonly property point at: { mapView.revision; return mapView.toScreen(lat, lon) }
            x: at.x
            y: at.y - height / 2
            width: label.width
            height: label.height
            z: drag.active ? 10 : 1

            SoftShadow {
                anchors.fill: label
                radius: 4
                spread: 3
                offsetY: 2
                strength: window.mc.shadow
            }
            Rectangle {
                id: label
                width: callText.implicitWidth + 20
                height: callText.implicitHeight + (pane.phone ? 12 : 8)
                radius: 4
                color: window.theme.material === Material.Light ? Qt.rgba(1, 1, 1, 0.95) : window.mc.card
                border.width: 2
                border.color: marker.stationColor
                Text {
                    id: callText
                    anchors.centerIn: parent
                    text: marker.callsign
                    color: marker.stationColor
                    font.bold: true
                    font.pixelSize: pane.phone ? 14 : 12
                }
            }

            // Dragging moves the station; its line and the zone follow.
            property point grabStart
            DragHandler {
                id: drag
                target: null
                dragThreshold: 4
                onActiveChanged: if (active) marker.grabStart = marker.at
                onTranslationChanged: {
                    if (!active) return
                    var p = mapView.toCoordinate(marker.grabStart.x + translation.x, marker.grabStart.y + translation.y)
                    mission.stations.setPosition(marker.index, p.x, p.y)
                }
            }
            TapHandler {
                onTapped: infoPopup.show(marker.x, marker.y + marker.height / 2,
                                         marker.callsign + "  (" + marker.signal + ")",
                                         pane.coordinateLines(marker.lat, marker.lon) + "\n"
                                         + qsTr("Azimuth: %1° ±%2°").arg(marker.azimuth).arg(marker.uncertainty),
                                         "station:" + marker.index)
            }
            HoverHandler { cursorShape: Qt.SizeAllCursor }
        }
    }

    // ---- the beacon: a pulsing red disc ---------------------------------------------
    Item {
        id: beacon
        objectName: "beaconMarker"
        visible: mission.hasBeacon
        readonly property point at: { mapView.revision; return mapView.toScreen(mission.beaconLat, mission.beaconLon) }
        x: at.x - width / 2
        y: at.y - height / 2
        width: 44
        height: 44
        z: 20

        Rectangle {
            id: glow
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: width / 2
            color: "transparent"
            border.width: 6
            border.color: Qt.rgba(1, 0, 0, 0.45)
            SequentialAnimation on scale {
                running: beacon.visible
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 2.1; duration: 750; easing.type: Easing.InOutSine }
                NumberAnimation { from: 2.1; to: 1.0; duration: 750; easing.type: Easing.InOutSine }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: 10
            color: "#ff0000"
            border.width: 3
            border.color: "#ffffff"
        }
        property point grabStart
        DragHandler {
            target: null
            dragThreshold: 4
            onActiveChanged: if (active) beacon.grabStart = beacon.at
            onTranslationChanged: {
                if (!active) return
                var p = mapView.toCoordinate(beacon.grabStart.x + translation.x, beacon.grabStart.y + translation.y)
                mission.moveBeacon(p.x, p.y)
            }
        }
        TapHandler {
            onTapped: infoPopup.show(beacon.at.x, beacon.at.y, qsTr("DISTRESS BEACON"),
                                     pane.coordinateLines(mission.beaconLat, mission.beaconLon) + "\n"
                                     + "MGRS: " + mission.mgrs(mission.beaconLat, mission.beaconLon) + "\n"
                                     + qsTr("Drag to move"), "beacon")
        }
        HoverHandler { cursorShape: Qt.SizeAllCursor }
    }

    // ---- controls ----------------------------------------------------------------------
    component MapButton: RoundButton {
        id: mb
        property string iconKind
        property string tip
        implicitWidth: pane.phone ? 48 : 38
        implicitHeight: implicitWidth
        padding: 0
        background: Item {
            SoftShadow { anchors.fill: parent; radius: width / 2; spread: 3; offsetY: 2; strength: window.mc.shadow }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: mb.down ? Qt.darker(window.mc.card, 1.15) : window.mc.card
                border.width: 1
                border.color: window.mc.edge
            }
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: pane.phone ? 22 : 18
                height: width
                kind: mb.iconKind
                colour: window.Material.foreground
            }
        }
        ToolTip.visible: tip.length > 0 && hovered
        ToolTip.text: tip
        ToolTip.delay: 600
    }

    Column {
        id: zoomControls
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 6
        MapButton { objectName: "zoomInButton"; iconKind: "plus"; tip: qsTr("Zoom in"); onClicked: mapView.zoomIn() }
        MapButton { objectName: "zoomOutButton"; iconKind: "minus"; tip: qsTr("Zoom out"); onClicked: mapView.zoomOut() }
        MapButton { iconKind: "zone"; tip: qsTr("Centre on the intersection zone"); visible: mission.hasZone
                    onClicked: window.centreOnZone() }
    }

    // Phone: the background, offline mode and grid at hand.
    MapButton {
        objectName: "layersButton"
        visible: pane.phone
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        iconKind: "layers"
        onClicked: layersMenu.popup(this, 0, height)
        AppMenu {
            id: layersMenu
            Repeater {
                model: mission.providerNames
                MenuItem {
                    required property int index
                    required property string modelData
                    text: modelData
                    checkable: true
                    checked: mission.providerIndex === index
                    onTriggered: mission.providerIndex = index
                }
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Offline")
                checkable: true
                checked: mission.offline
                onTriggered: {
                    mission.offline = checked
                    if (checked) {
                        var warning = mission.offlineCheck()
                        if (warning.length > 0) window.alert(qsTr("Warning"), warning)
                        else window.showMessage(mission.statusMessage)
                    }
                }
            }
            MenuItem {
                text: qsTr("UTM/MGRS grid")
                checkable: true
                checked: mission.showGrid
                onTriggered: mission.showGrid = checked
            }
            MenuItem { text: qsTr("Download visible tiles"); onTriggered: window.downloadTiles() }
        }
    }

    // Offline badge.
    Label {
        visible: mission.offline
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 10
        text: qsTr("Offline")
        padding: 6
        leftPadding: 12
        rightPadding: 12
        font.pixelSize: 12
        font.bold: true
        color: "#ffffff"
        background: Rectangle { radius: 12; color: Qt.rgba(0.6, 0.1, 0.1, 0.85) }
    }

    // Scale bar (bottom left): a round length near 100 px.
    Item {
        id: scale
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        anchors.bottomMargin: pane.phone ? 10 : 8
        width: barWidth
        height: 22
        readonly property real mpp: { mapView.revision; return mapView.metersPerPixel }
        readonly property real meters: {
            var target = mpp * 110
            var p = Math.pow(10, Math.floor(Math.log(target) / Math.LN10))
            var n = target / p
            return (n >= 5 ? 5 : n >= 2 ? 2 : 1) * p
        }
        readonly property real barWidth: mpp > 0 ? meters / mpp : 100
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(1, 1, 1, 0.75)
            border.color: "#333333"
            border.width: 2
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 2; color: Qt.rgba(1, 1, 1, 0.75) }
        }
        Label {
            anchors.centerIn: parent
            text: scale.meters >= 1000 ? (scale.meters / 1000) + " km" : scale.meters + " m"
            font.pixelSize: 11
            color: "#333333"
        }
    }

    Label {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: mission.attribution
        font.pixelSize: 10
        color: "#333333"
        padding: 2
        leftPadding: 5
        rightPadding: 5
        background: Rectangle { color: Qt.rgba(1, 1, 1, 0.8) }
    }

    // ---- popups ----------------------------------------------------------------------------
    // Details of what was tapped (Leaflet's popups).
    Popup {
        id: infoPopup
        objectName: "infoPopup"
        property string heading
        property string body
        property string target
        padding: 12
        // As wide as the text or the row of buttons, whichever is wider, up
        // to the map's width; past that the buttons go on a second line.
        width: Math.min(pane.width - 20, Math.max(220, infoText.implicitWidth + 24, infoFlow.needed + 24))
        background: Item {
            SoftShadow { anchors.fill: parent; radius: 8; spread: 6; offsetY: 3; strength: window.mc.shadow }
            Rectangle { anchors.fill: parent; radius: 8; color: window.mc.card; border.width: 1; border.color: window.mc.edge }
        }
        function show(px, py, h, b, t) {
            heading = h
            body = b
            target = t
            x = Math.max(10, Math.min(pane.width - width - 10, px - width / 2))
            y = Math.max(10, Math.min(pane.height - implicitHeight - 10, py + 14))
            open()
        }
        contentItem: ColumnLayout {
            spacing: 6
            Label { text: infoPopup.heading; font.bold: true; font.pixelSize: 14
                    color: infoPopup.target === "beacon" ? "#ff3333" : window.Material.foreground }
            Label { id: infoText; text: infoPopup.body; font.family: "monospace"; font.pixelSize: 12 }
            Flow {
                id: infoFlow
                visible: infoPopup.target.length > 0
                Layout.fillWidth: true
                spacing: 4
                readonly property bool station: infoPopup.target.startsWith("station:")
                readonly property bool beacon: infoPopup.target === "beacon"
                // What the buttons shown take side by side.
                readonly property real needed: copyButton.implicitWidth
                    + (station ? infoRecord.implicitWidth + stationEdit.implicitWidth + 2 * spacing : 0)
                    + (beacon ? beaconEdit.implicitWidth + spacing : 0)
                AppButton {
                    id: infoRecord
                    visible: infoPopup.target.startsWith("station:")
                    text: qsTr("Record")
                    flat: true
                    onClicked: { mission.recordStation(parseInt(infoPopup.target.split(":")[1])); window.showMessage(mission.statusMessage); infoPopup.close() }
                }
                AppButton {
                    id: stationEdit
                    visible: infoPopup.target.startsWith("station:")
                    text: qsTr("Edit")
                    flat: true
                    onClicked: { window.editStation(parseInt(infoPopup.target.split(":")[1])); infoPopup.close() }
                }
                AppButton {
                    id: beaconEdit
                    visible: infoPopup.target === "beacon"
                    text: qsTr("Edit")
                    flat: true
                    onClicked: { window.setBeacon(); infoPopup.close() }
                }
                AppButton {
                    id: copyButton
                    text: qsTr("Copy")
                    flat: true
                    onClicked: { app.copyToClipboard(infoPopup.heading + "\n" + infoPopup.body); infoPopup.close() }
                }
            }
        }
    }

    // Right click / long press: the coordinates of the point, and what can
    // be put there.
    Popup {
        id: contextPopup
        objectName: "contextPopup"
        property real lat
        property real lon
        padding: 12
        width: Math.min(pane.width - 20, Math.max(250, ctxText.implicitWidth + 24))
        background: Item {
            SoftShadow { anchors.fill: parent; radius: 8; spread: 6; offsetY: 3; strength: window.mc.shadow }
            Rectangle { anchors.fill: parent; radius: 8; color: window.mc.card; border.width: 1; border.color: window.mc.edge }
        }
        function place(px, py) {
            x = Math.max(10, Math.min(pane.width - width - 10, px - width / 2))
            y = Math.max(10, Math.min(pane.height - implicitHeight - 10, py + 14))
            open()
        }
        contentItem: ColumnLayout {
            spacing: 4
            Label { text: qsTr("Coordinates"); font.bold: true; font.pixelSize: 14 }
            Label {
                id: ctxText
                font.family: "monospace"
                font.pixelSize: 12
                text: pane.coordinateLines(contextPopup.lat, contextPopup.lon) + "\n"
                      + "UTM: " + mission.utmZone(contextPopup.lat, contextPopup.lon) + "\n"
                      + "MGRS: " + mission.mgrs(contextPopup.lat, contextPopup.lon)
            }
            Flow {
                Layout.fillWidth: true
                spacing: 2
                AppButton {
                    objectName: "placeBeaconHere"
                    text: qsTr("Beacon here")
                    flat: true
                    onClicked: { mission.setBeacon(contextPopup.lat, contextPopup.lon); window.showMessage(mission.statusMessage); contextPopup.close() }
                }
                AppButton {
                    objectName: "addStationHere"
                    text: qsTr("Station here")
                    flat: true
                    onClicked: {
                        var row = mission.addStationAt(contextPopup.lat, contextPopup.lon)
                        contextPopup.close()
                        window.editStation(row)
                    }
                }
                AppButton {
                    text: qsTr("Copy")
                    flat: true
                    onClicked: { app.copyToClipboard(ctxText.text); contextPopup.close() }
                }
            }
        }
    }
}
