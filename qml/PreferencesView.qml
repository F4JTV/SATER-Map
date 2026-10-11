// PreferencesView.qml - theme, language, the organisation named in the
// PDF report, the tiles folder (desktop), keeping the screen on (phone).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout {
    id: prefs
    spacing: 10

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 10

        Label { text: qsTr("Theme:") }
        AppComboBox {
            objectName: "themeBox"
            Layout.fillWidth: true
            model: [qsTr("Light"), qsTr("Dark"), qsTr("Red (night)"), qsTr("Amber")]
            readonly property var keys: ["light", "dark", "red", "amber"]
            currentIndex: Math.max(0, keys.indexOf(app.theme))
            onActivated: app.theme = keys[currentIndex]
        }

        Label { text: qsTr("Language:") }
        AppComboBox {
            objectName: "languageBox"
            Layout.fillWidth: true
            // Each language under its own name.
            model: [qsTr("Automatic"), "English", "Français"]
            readonly property var keys: ["auto", "en", "fr"]
            currentIndex: Math.max(0, keys.indexOf(app.language))
            onActivated: app.language = keys[currentIndex]
        }

        Label { text: qsTr("Organisation:") }
        AppField {
            objectName: "organizationField"
            Layout.fillWidth: true
            text: mission.organization
            hint: "ADRASEC 06"
            onEditingFinished: mission.organization = text
        }

        Label { text: qsTr("New stations:") }
        AppComboBox {
            objectName: "northDefaultBox"
            Layout.fillWidth: true
            model: [qsTr("True north"), qsTr("Magnetic north (compass)")]
            currentIndex: mission.magneticByDefault ? 1 : 0
            onActivated: mission.magneticByDefault = currentIndex === 1
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 12
        color: window.colourOf("dim")
        text: qsTr("Red keeps the eyes used to the dark; amber suits dim light. The organisation is printed at the foot of the PDF report.")
              + "\n" + qsTr("A bearing read on a compass is corrected by the magnetic declination at the station (%1).").arg(mission.declinationModel)
    }

    Switch {
        visible: app.android
        Layout.fillWidth: true
        text: qsTr("Keep the screen on")
        checked: app.keepScreenOn
        onToggled: app.keepScreenOn = checked
        contentItem: Text {
            text: parent.text
            font: parent.font
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
            color: window.Material.foreground
            leftPadding: parent.indicator.width + parent.spacing
        }
    }

    // The tiles folder: the desktop only (Android keeps them in the
    // application's own storage).
    ColumnLayout {
        visible: !app.android
        Layout.fillWidth: true
        spacing: 4
        Label { text: qsTr("Tiles folder:") }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: tilesField
                objectName: "tilesDirField"
                Layout.fillWidth: true
                text: mission.tilesDir
                readOnly: true
                font.pixelSize: 12
            }
            AppButton { text: qsTr("Change..."); onClicked: folderDialog.open() }
            AppButton { text: qsTr("Default"); flat: true; onClicked: mission.tilesDir = "" }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: window.colourOf("dim")
            textFormat: Text.PlainText
            text: qsTr("Tiles in <provider>/<z>/<x>/<y>.png, as SATER Map 2: point it at the old \"tiles\" folder to use the tiles already downloaded.")
        }
    }
    FolderDialog {
        id: folderDialog
        title: qsTr("Tiles folder")
        currentFolder: mission.fileUrl(mission.tilesDir)
        onAccepted: mission.tilesDir = selectedFolder
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 12
        color: window.colourOf("dim")
        text: qsTr("Mission and presets folder: %1").arg(mission.dataDir)
    }
}
