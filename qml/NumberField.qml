// NumberField.qml - a compact number entry for the station table and the
// dialogs (the Python QSpinBox/QDoubleSpinBox without buttons): typing,
// arrow keys and the mouse wheel change the value; "edited" reports each
// valid value as it is typed.  While the field has the focus the text is
// the user's; the model's value is shown again when it loses it.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

TextField {
    id: field
    property real value: 0
    property real from: 0
    property real to: 100
    property int decimals: 0
    property real step: 1
    property bool wrap: false          // 359 + 1 -> 0 (azimuths)
    property string prefix: ""
    property string suffix: ""
    property bool compact: true
    signal edited(real newValue)

    implicitWidth: 56
    implicitHeight: compact ? 30 : 44
    leftPadding: compact ? 4 : 10
    rightPadding: compact ? 4 : 10
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: compact ? 13 : 16
    horizontalAlignment: compact ? TextInput.AlignHCenter : TextInput.AlignLeft
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    inputMethodHints: decimals > 0 ? Qt.ImhFormattedNumbersOnly : Qt.ImhDigitsOnly
    // As many decimals as the field shows (6 for decimal degrees), so the
    // value it displays can be edited and a GPS position pasted.
    validator: RegularExpressionValidator {
        regularExpression: new RegExp("[-+±]?\\d{0,5}"
                                      + (field.decimals > 0 ? "([.,]\\d{0," + field.decimals + "})?" : "")
                                      + "[°'\"]?(\\s?km)?")
    }
    background: Rectangle {
        radius: 4
        color: window.mc.field
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? window.Material.accent : window.mc.fieldEdge
    }

    function format(v) {
        return prefix + (decimals > 0 ? Number(v).toFixed(decimals) : Math.round(v).toString()) + suffix
    }
    function parse(t) {
        var s = t.replace(prefix, "").replace(suffix, "").replace(/[±°'"]|km/g, "").replace(",", ".").trim()
        if (s.length === 0 || s === "-" || s === "+" || s === ".") return NaN
        return Number(s)
    }
    function clampValue(v) {
        if (wrap) {
            var span = to - from + (decimals > 0 ? 0 : 1)
            while (v > to) v -= span
            while (v < from) v += span
            return v
        }
        return Math.max(from, Math.min(to, v))
    }
    function stepBy(n) {
        var cur = parse(text)
        if (isNaN(cur)) cur = value
        var v = clampValue(cur + n * step)
        text = format(v)
        edited(v)
    }

    Component.onCompleted: text = format(value)
    onValueChanged: if (!activeFocus) text = format(value)
    onActiveFocusChanged: {
        if (activeFocus) {
            selectAll()
        } else {
            var v = parse(text)
            if (!isNaN(v) && clampValue(v) !== value) edited(clampValue(v))
            text = format(value)
        }
    }
    onTextEdited: {
        var v = parse(text)
        if (!isNaN(v) && v >= from && v <= to) edited(v)
    }
    onAccepted: {
        var v = parse(text)
        if (!isNaN(v)) edited(clampValue(v))
        text = format(clampValue(isNaN(v) ? value : v))
    }
    Keys.onUpPressed: stepBy(1)
    Keys.onDownPressed: stepBy(-1)
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_PageUp) { stepBy(10); event.accepted = true }
        else if (event.key === Qt.Key_PageDown) { stepBy(-10); event.accepted = true }
    }

    WheelHandler {
        enabled: field.enabled && !app.android
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) {
            field.forceActiveFocus()
            field.stepBy(event.angleDelta.y > 0 ? 1 : -1)
        }
    }
}
