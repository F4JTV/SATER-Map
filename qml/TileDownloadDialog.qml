// TileDownloadDialog.qml - "Download visible tiles": what would be fetched
// (the view, two zoom levels below and above it), then the progress, then
// the result - the Python confirmation, QProgressDialog and summary.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("Download the tiles")
    preferredWidth: 480
    closePolicy: Popup.NoAutoClose
    // ask -> running -> done
    property string stage: "ask"
    property var plan: ({})
    property string resultText: ""
    readonly property var tiles: mission.tiles

    standardButtons: stage === "ask" ? (Dialog.Yes | Dialog.No) : stage === "running" ? Dialog.Cancel : Dialog.Ok

    function prepare() {
        plan = mission.prepareTileDownload()
        if (!plan.ok) {
            window.alert(qsTr("Error"), plan.error ? plan.error : qsTr("No tile to download."))
            return
        }
        stage = "ask"
        open()
    }
    function finished(saved, failed) {
        if (stage !== "running") return
        var text = qsTr("Downloaded: %1\nFailed: %2").arg(tiles.downloadDone - failed).arg(failed)
        if (failed > 0 && tiles.lastError.length > 0) text += "\n\n" + qsTr("Last error:\n%1").arg(tiles.lastError)
        resultText = text
        stage = "done"
        if (!visible) open()
    }
    onAccepted: {
        if (stage === "ask") {
            stage = "running"
            mission.startTileDownload()
            // Up again for the progress, or for the result when every tile
            // was already there (the download is then over at once).
            Qt.callLater(function() { if (dialog.stage !== "ask") dialog.open() })
        }
    }
    onRejected: {
        if (stage === "running") tiles.cancelDownload()
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            visible: dialog.stage === "ask"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: qsTr("Background: %1\nFolder: %2\nTiles: %3\nZoom: %4-%5\n\nContinue?")
                  .arg(dialog.plan.provider).arg(dialog.plan.folder).arg(dialog.plan.count)
                  .arg(dialog.plan.minZoom).arg(dialog.plan.maxZoom)
        }
        Label {
            visible: dialog.stage === "ask" && dialog.plan.count > 2000
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: window.colourOf("busy")
            text: qsTr("That is a lot of tiles: tile servers ask for moderation. Zoom in on the area of operation first.")
        }

        ColumnLayout {
            visible: dialog.stage === "running"
            Layout.fillWidth: true
            Label { text: qsTr("Downloading...") }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, dialog.tiles.downloadTotal)
                value: dialog.tiles.downloadDone
            }
            Label {
                text: qsTr("%1 / %2 (%3 failed)").arg(dialog.tiles.downloadDone).arg(dialog.tiles.downloadTotal)
                      .arg(dialog.tiles.downloadFailed)
                color: window.colourOf("dim")
            }
        }

        Label {
            visible: dialog.stage === "done"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: dialog.resultText
        }
    }
}
