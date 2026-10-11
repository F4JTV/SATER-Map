// ColorPicker.qml - choose a colour: the station palette, common colours,
// or a hex code.  Touch-sized, the same on every platform.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: picker
    title: qsTr("Choose a colour")
    preferredWidth: 360
    standardButtons: Dialog.Ok | Dialog.Cancel

    property color current: "#e74c3c"
    property var callback: null
    readonly property var swatches: [
        // The station colours of the Python version first.
        "#e74c3c", "#3498db", "#2ecc71", "#9b59b6", "#f39c12", "#1abc9c", "#e91e63", "#795548",
        "#f44336", "#ff9800", "#ffeb3b", "#cddc39", "#4caf50", "#00bcd4", "#2196f3", "#3f51b5",
        "#673ab7", "#9c27b0", "#607d8b", "#000000", "#ffffff", "#9e9e9e", "#00ff00", "#ff00ff"
    ]

    function choose(initial, onChosen) {
        current = initial
        hexField.text = String(current)
        callback = onChosen
        open()
    }
    onAccepted: if (callback) callback(String(current))

    ColumnLayout {
        width: parent.width
        spacing: 12
        GridLayout {
            columns: 8
            columnSpacing: 6
            rowSpacing: 6
            Layout.alignment: Qt.AlignHCenter
            Repeater {
                model: picker.swatches
                Rectangle {
                    required property string modelData
                    width: Math.min(34, (picker.availableWidth - 7 * 6) / 8)
                    height: width
                    radius: 5
                    color: modelData
                    border.width: Qt.colorEqual(picker.current, modelData) ? 3 : 1
                    border.color: Qt.colorEqual(picker.current, modelData) ? window.Material.accent : "#808080"
                    TapHandler {
                        onTapped: {
                            picker.current = parent.modelData
                            hexField.text = parent.modelData
                        }
                        onDoubleTapped: { picker.current = parent.modelData; picker.accept() }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                width: 44
                height: 32
                radius: 5
                color: picker.current
                border.color: "#808080"
            }
            AppField {
                id: hexField
                objectName: "hexField"
                Layout.fillWidth: true
                hint: "#rrggbb"
                font.family: "monospace"
                validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                onTextEdited: {
                    var t = text.startsWith("#") ? text : "#" + text
                    if (t.length === 7) picker.current = t
                }
            }
        }
    }
}
