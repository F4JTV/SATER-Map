// DmsEditor.qml - a position typed in decimal degrees or in degrees,
// minutes and seconds (the two tabs of the Python beacon dialog).  lat and
// lon always hold the position in decimal degrees.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: editor
    property real lat: 0
    property real lon: 0
    property bool big: app.android
    spacing: 8

    // Fill the fields from lat/lon (call after setting them).
    function reload() {
        var a = mission.toDms(lat, true), o = mission.toDms(lon, false)
        latDeg.value = a.deg; latMin.value = a.min; latSec.value = a.sec; latDir.text = a.dir
        lonDeg.value = o.deg; lonMin.value = o.min; lonSec.value = o.sec; lonDir.text = o.dir
        latDd.value = lat; lonDd.value = lon
        for (var f of [latDeg, latMin, latSec, lonDeg, lonMin, lonSec, latDd, lonDd]) f.text = f.format(f.value)
    }
    function fromDms() {
        lat = mission.fromDms(latDeg.value, latMin.value, latSec.value, latDir.text)
        lon = mission.fromDms(lonDeg.value, lonMin.value, lonSec.value, lonDir.text)
        latDd.value = lat; lonDd.value = lon
    }
    function fromDd() {
        lat = latDd.value; lon = lonDd.value
        var a = mission.toDms(lat, true), o = mission.toDms(lon, false)
        latDeg.value = a.deg; latMin.value = a.min; latSec.value = a.sec; latDir.text = a.dir
        lonDeg.value = o.deg; lonMin.value = o.min; lonSec.value = o.sec; lonDir.text = o.dir
    }

    TabBar {
        id: tabs
        Layout.fillWidth: true
        // The full name where it fits, the short one on a narrow phone
        // (in French the full one takes twice the room).
        component FormatTab: TabButton {
            id: tab
            property string full
            property string brief
            TextMetrics { id: fullMetrics; font: tab.font; text: tab.full }
            // The bar's share, not the tab's own width: the bar sizes its
            // tabs from their text.
            text: fullMetrics.advanceWidth <= tabs.width / tabs.count - tab.leftPadding - tab.rightPadding ? full : brief
        }
        FormatTab { full: qsTr("Degrees Minutes Seconds"); brief: qsTr("DMS") }
        FormatTab { full: qsTr("Decimal Degrees"); brief: qsTr("Decimal") }
    }

    StackLayout {
        currentIndex: tabs.currentIndex
        Layout.fillWidth: true

        GridLayout {
            columns: 5
            columnSpacing: 6
            rowSpacing: 8
            Label { text: qsTr("Latitude:") }
            NumberField { id: latDeg; objectName: "latDeg"; compact: !editor.big; from: 0; to: 90; suffix: "°"; Layout.fillWidth: true
                          onEdited: function(v) { value = v; editor.fromDms() } }
            NumberField { id: latMin; objectName: "latMin"; compact: !editor.big; from: 0; to: 59; suffix: "'"; Layout.fillWidth: true
                          onEdited: function(v) { value = v; editor.fromDms() } }
            NumberField { id: latSec; objectName: "latSec"; compact: !editor.big; from: 0; to: 59.99; decimals: 2; step: 0.1; suffix: "\""
                          Layout.fillWidth: true; Layout.preferredWidth: 70
                          onEdited: function(v) { value = v; editor.fromDms() } }
            DirButton {
                id: latDir
                objectName: "latDir"
                text: "N"
                compact: !editor.big
                onClicked: { text = text === "N" ? "S" : "N"; editor.fromDms() }
            }
            Label { text: qsTr("Longitude:") }
            NumberField { id: lonDeg; objectName: "lonDeg"; compact: !editor.big; from: 0; to: 180; suffix: "°"; Layout.fillWidth: true
                          onEdited: function(v) { value = v; editor.fromDms() } }
            NumberField { id: lonMin; objectName: "lonMin"; compact: !editor.big; from: 0; to: 59; suffix: "'"; Layout.fillWidth: true
                          onEdited: function(v) { value = v; editor.fromDms() } }
            NumberField { id: lonSec; objectName: "lonSec"; compact: !editor.big; from: 0; to: 59.99; decimals: 2; step: 0.1; suffix: "\""
                          Layout.fillWidth: true; Layout.preferredWidth: 70
                          onEdited: function(v) { value = v; editor.fromDms() } }
            DirButton {
                id: lonDir
                objectName: "lonDir"
                text: "E"
                compact: !editor.big
                onClicked: { text = text === "E" ? "W" : "E"; editor.fromDms() }
            }
        }

        GridLayout {
            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            Label { text: qsTr("Latitude (DD):") }
            NumberField { id: latDd; objectName: "latDd"; compact: !editor.big; from: -90; to: 90; decimals: 6; step: 0.0001
                          Layout.fillWidth: true; onEdited: function(v) { value = v; editor.fromDd() } }
            Label { text: qsTr("Longitude (DD):") }
            NumberField { id: lonDd; objectName: "lonDd"; compact: !editor.big; from: -180; to: 180; decimals: 6; step: 0.0001
                          Layout.fillWidth: true; onEdited: function(v) { value = v; editor.fromDd() } }
        }
    }
}
