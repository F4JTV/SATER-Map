// PresetsDialog.qml - the station presets (the Python PresetManagerDialog):
// add, edit, delete, and load the selected ones as new stations.  Changes
// are saved when the dialog is accepted.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: loading ? qsTr("Load presets") : qsTr("Station presets")
    preferredWidth: 640
    fillHeight: true
    standardButtons: Dialog.Cancel
    property bool loading: false
    property var selected: ({})
    property int selectedCount: 0

    function openFor(load) {
        loading = load
        mission.presets.reload()
        selected = ({})
        selectedCount = 0
        open()
    }
    function toggle(row) {
        // A copy: the same object assigned back would not notify.
        var s = Object.assign({}, selected)
        if (s[row]) delete s[row]
        else s[row] = true
        selected = s
        selectedCount = Object.keys(s).length
    }
    function rows() { return Object.keys(selected).map(Number).sort(function(a, b) { return a - b }) }
    function save() {
        if (!mission.presets.save())
            window.alert(qsTr("Error"), qsTr("Cannot save the presets.\n\nFile: %1\n\nCheck the write permissions.")
                         .arg(mission.presets.filePath))
    }
    onRejected: mission.presets.reload()   // forget unsaved edits

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ListView {
            id: list
            objectName: "presetList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: mission.presets
            spacing: 2
            ScrollBar.vertical: ScrollBar {}
            Label {
                anchors.centerIn: parent
                width: parent.width - 40
                visible: list.count === 0
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No preset yet: add the usual positions of your stations.")
                color: window.colourOf("dim")
            }
            delegate: Rectangle {
                id: item
                required property int index
                required property string description
                required property string presetColor
                width: list.width - 10
                height: 44
                radius: 4
                color: dialog.selected[index] ? Qt.rgba(window.Material.accent.r, window.Material.accent.g, window.Material.accent.b, 0.25)
                                              : (index % 2 ? window.mc.rowAlt : "transparent")
                MouseArea {
                    anchors.fill: parent
                    onClicked: dialog.toggle(item.index)
                    onDoubleClicked: presetEditDialog.openFor(item.index)
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10
                    CheckBox {
                        checked: !!dialog.selected[item.index]
                        onToggled: dialog.toggle(item.index)
                    }
                    Rectangle { width: 14; height: 14; radius: 7; color: item.presetColor }
                    Label { text: item.description; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            AppButton { text: qsTr("Add"); onClicked: presetEditDialog.openFor(-1) }
            AppButton {
                text: qsTr("Edit")
                enabled: dialog.selectedCount === 1
                onClicked: presetEditDialog.openFor(dialog.rows()[0])
            }
            AppButton {
                text: qsTr("Delete")
                enabled: dialog.selectedCount > 0
                onClicked: window.confirm(qsTr("Confirmation"), qsTr("Delete %1 preset(s)?").arg(dialog.selectedCount),
                                          function() {
                                              mission.presets.removeRows(dialog.rows())
                                              dialog.selected = ({})
                                              dialog.selectedCount = 0
                                              dialog.save()
                                          })
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("%1 selected").arg(dialog.selectedCount)
                color: window.colourOf("dim")
            }
            AppButton {
                objectName: "loadSelection"
                highlighted: true
                text: qsTr("Load selection")
                enabled: dialog.selectedCount > 0
                onClicked: {
                    dialog.save()
                    mission.loadPresets(dialog.rows())
                    window.showMessage(mission.statusMessage)
                    dialog.close()
                }
            }
            AppButton {
                visible: !dialog.loading
                text: qsTr("Save")
                onClicked: { dialog.save(); dialog.close() }
            }
        }
    }
}
