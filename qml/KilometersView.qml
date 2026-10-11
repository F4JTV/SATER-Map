// KilometersView.qml - the distance each station drove (the Python
// KilometersDialog): one field per station with a callsign, and the total.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 8
    property var values: ({})
    property var callsigns: []
    readonly property real total: {
        var t = 0
        for (var k in values) t += Number(values[k])
        return t
    }

    function load() {
        callsigns = mission.stations.callsigns()
        var v = {}
        for (var i = 0; i < callsigns.length; ++i) v[callsigns[i]] = mission.kilometersFor(callsigns[i])
        values = v
    }
    function save() { mission.setKilometers(values) }

    Label {
        visible: view.callsigns.length === 0
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: qsTr("No station defined.\nAdd stations with a callsign.")
    }
    Repeater {
        model: view.callsigns
        RowLayout {
            required property string modelData
            Layout.fillWidth: true
            spacing: 10
            Label { text: modelData + ":"; font.bold: true; Layout.preferredWidth: 110; elide: Text.ElideRight }
            NumberField {
                objectName: "kmField"
                Layout.fillWidth: true
                compact: !app.android
                from: 0; to: 9999; decimals: 1; step: 1
                suffix: " km"
                value: view.values[modelData] || 0
                onEdited: function(v) {
                    var n = view.values
                    n[modelData] = v
                    view.values = n
                }
            }
        }
    }
    Label {
        visible: view.callsigns.length > 0
        text: qsTr("Total: %1 km").arg(view.total.toFixed(1))
        font.bold: true
    }
}
