// Main.qml - the window: themes, the desktop or phone layout, the menu
// actions and the dialogs both layouts share.
//
// A window at least 1000 px wide gets the desktop layout (the Python
// version's control panel beside the map, menu bar, status bar); a
// narrower one, the phone layout (full-screen map, pages in tabs).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    visible: true
    width: 1400
    height: 860
    minimumWidth: 360
    minimumHeight: 560
    title: "SATER Map v" + app.version
    // Full screen on the phone (the system bars are hidden by the C++ side
    // as well; Qt shows them again for a window that is not full screen).
    visibility: app.android ? Window.FullScreen : Window.Maximized

    // ---- themes -----------------------------------------------------------
    // AX25Chat's four looks, same Material palettes and status colours:
    // light and dark for daytime, red for the night (every colour a shade of
    // red, so the eyes keep their dark adaptation), amber for dim light.
    // "map" adds what the map and the panels need.
    readonly property var themes: ({
        "light": { "material": Material.Light, "primary": "#3f51b5", "accent": "#e91e63",
                   "background": "#fafafa", "foreground": "#212121",
                   "colours": { "clear": "#1a7f37", "busy": "#9a6700", "error": "#b32626",
                                "dim": "#6e7681", "offline": "#6e7681", "link": "#2980b9" },
                   "map": { "card": "#ffffff", "button": "#e3e6ea", "edge": "#d0d7de", "field": "#ffffff", "fieldEdge": "#b8c0c8",
                            "header": "#ffffff", "mapBg": "#dddddd", "tint": "transparent", "dim": 0.0,
                            "lcdBg": "#1a1a2e", "lcdText": "#00ff00", "rowAlt": "#f3f5f8",
                            "toast": "#323232", "toastText": "#ffffff", "shadow": 0.30 } },
        "dark":  { "material": Material.Dark, "primary": "#263238", "accent": "#26c6da",
                   "background": "#121212", "foreground": "#e6edf3",
                   "colours": { "clear": "#3fb950", "busy": "#d29922", "error": "#f85149",
                                "dim": "#8b949e", "offline": "#8b949e", "link": "#58a6ff" },
                   "map": { "card": "#1e1f22", "button": "#2d333b", "edge": "#30363d", "field": "#16181b", "fieldEdge": "#3d444d",
                            "header": "#e6edf3", "mapBg": "#2b2b2b", "tint": "transparent", "dim": 0.0,
                            "lcdBg": "#0d1117", "lcdText": "#3fb950", "rowAlt": "#24262a",
                            "toast": "#e6edf3", "toastText": "#121212", "shadow": 0.60 } },
        "red":   { "material": Material.Dark, "primary": "#2b0000", "accent": "#ff2a2a",
                   "background": "#0a0000", "foreground": "#ff3b3b",
                   "colours": { "clear": "#ff5252", "busy": "#c92e2e", "error": "#ff1a1a",
                                "dim": "#7a2424", "offline": "#5c1f1f", "link": "#ff7070" },
                   "map": { "card": "#170202", "button": "#2b0505", "edge": "#2a0505", "field": "#0d0000", "fieldEdge": "#4a0c0c",
                            "header": "#ff3b3b", "mapBg": "#1a0000", "tint": "#ff3030", "dim": 0.45,
                            "lcdBg": "#0d0000", "lcdText": "#ff3b3b", "rowAlt": "#1f0404",
                            "toast": "#2b0000", "toastText": "#ff3b3b", "shadow": 0.70 } },
        "amber": { "material": Material.Dark, "primary": "#3d2a00", "accent": "#ffb000",
                   "background": "#100c00", "foreground": "#ffb84d",
                   "colours": { "clear": "#ffd166", "busy": "#e69500", "error": "#ff6a00",
                                "dim": "#8a6a2a", "offline": "#5c4a1f", "link": "#ffd166" },
                   "map": { "card": "#1a1403", "button": "#2e2306", "edge": "#2e2306", "field": "#100c00", "fieldEdge": "#4a3a10",
                            "header": "#ffb84d", "mapBg": "#1a1400", "tint": "#ffb000", "dim": 0.35,
                            "lcdBg": "#100c00", "lcdText": "#ffb84d", "rowAlt": "#211903",
                            "toast": "#3d2a00", "toastText": "#ffb84d", "shadow": 0.70 } }
    })
    readonly property string themeName: themes[app.theme] ? app.theme : "light"
    readonly property var theme: themes[themeName]
    readonly property var mc: theme.map
    // Night themes: everything in one hue, the station colours included.
    readonly property bool monochrome: themeName === "red" || themeName === "amber"
    Material.theme: theme.material
    Material.primary: theme.primary
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    color: theme.background

    readonly property var colours: theme.colours
    function colourOf(key) { return colours[key] !== undefined ? colours[key] : colours["dim"] }
    // Shadows are drawn by the GPU; the software renderer used for the
    // headless screenshots has none.
    readonly property bool flat: app.flatRendering

    // Desktop layout: the control panel (760 px at least) beside the map.
    readonly property bool wide: width >= 1000
    // The map view, kept across a layout change (phone rotated, window
    // resized) and from one run to the next.
    readonly property var startView: mission.savedView()
    property real viewLat: startView.lat
    property real viewLon: startView.lon
    property real viewZoom: startView.zoom
    // Written a moment after the map stops moving, not at every frame.
    Timer {
        id: viewSaver
        interval: 1500
        onTriggered: mission.saveView(window.viewLat, window.viewLon, window.viewZoom)
    }
    onViewLatChanged: viewSaver.restart()
    onViewLonChanged: viewSaver.restart()
    onViewZoomChanged: viewSaver.restart()
    property var mapView: null      // the MapView on screen, set by MapPane

    // ---- layout -------------------------------------------------------------
    menuBar: wide ? desktopMenu : null
    footer: wide ? statusBar : null

    Loader {
        id: layoutLoader
        anchors.fill: parent
        sourceComponent: window.wide ? desktopComponent : phoneComponent
    }
    Component { id: desktopComponent; DesktopLayout {} }
    Component { id: phoneComponent; PhoneLayout {} }

    MenuBar {
        id: desktopMenu
        visible: window.wide
        AppMenu {
            title: qsTr("&File")
            Action { text: qsTr("&Save HTML"); shortcut: "Ctrl+S"; onTriggered: window.saveHtml() }
            Action { text: qsTr("Export &PNG"); onTriggered: window.exportPng() }
            Action { text: qsTr("Export &KML"); onTriggered: window.exportKml() }
            Action { text: qsTr("Export &JSON"); onTriggered: window.exportJson() }
            Action { text: qsTr("&Import JSON"); onTriggered: window.importJson() }
            MenuSeparator {}
            Action { text: qsTr("Generate &PDF report"); shortcut: "Ctrl+P"; onTriggered: window.generatePdf() }
            MenuSeparator {}
            Action { text: qsTr("&New mission"); onTriggered: window.newMission() }
            Action { text: qsTr("&Quit"); shortcut: "Ctrl+Q"; onTriggered: Qt.quit() }
        }
        AppMenu {
            title: qsTr("&Stations")
            Action { text: qsTr("&Manage presets"); onTriggered: window.managePresets() }
            Action { text: qsTr("&Load presets"); onTriggered: window.loadPresets() }
            MenuSeparator {}
            Action { text: qsTr("&Save station as preset"); onTriggered: window.saveStationAsPreset(-1) }
        }
        AppMenu {
            title: qsTr("&Edit")
            Action { text: qsTr("Centre on &France"); onTriggered: window.centreFrance() }
            Action { text: qsTr("Centre on the intersection &zone"); onTriggered: window.centreOnZone() }
            MenuSeparator {}
            Action { text: qsTr("Set &beacon position"); onTriggered: window.setBeacon() }
            Action { text: qsTr("Clear beacon position"); onTriggered: mission.clearBeacon() }
            MenuSeparator {}
            Action { text: qsTr("&Download visible tiles"); onTriggered: window.downloadTiles() }
            MenuSeparator {}
            Action { text: qsTr("P&references"); onTriggered: preferencesDialog.open() }
        }
        AppMenu {
            title: qsTr("&APRS")
            Action { text: qsTr("&Operation and connections"); onTriggered: window.showAprs() }
            Action { text: qsTr("&Packet log"); onTriggered: window.showAprsLog() }
        }
        AppMenu {
            title: qsTr("&Help")
            Action { text: qsTr("&About"); onTriggered: aboutDialog.open() }
        }
    }

    ToolBar {
        id: statusBar
        visible: window.wide
        height: 30
        Material.elevation: 0
        Material.background: window.mc.card
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 14
            Label {
                objectName: "statusLabel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter
                text: mission.statusMessage
                elide: Text.ElideRight
                font.pixelSize: 13
            }
            // The APRS connections that are on; a click opens their settings.
            AprsStatus {
                visible: mission.aprs.isEnabled
                name: "APRS-IS"
                state: mission.aprs.isState
                detail: mission.aprs.isDetail
                showDetail: false
                TapHandler { onTapped: window.showAprs() }
            }
            AprsStatus {
                visible: mission.aprs.kissEnabled
                name: "KISS"
                state: mission.aprs.kissState
                detail: mission.aprs.kissDetail
                showDetail: false
                TapHandler { onTapped: window.showAprs() }
            }
            AprsStatus {
                visible: mission.aprs.modemAvailable && mission.aprs.modemEnabled
                name: mission.aprs.modemTransmitting ? "TX" : qsTr("Modem")
                state: mission.aprs.modemState
                detail: mission.aprs.modemDetail
                showDetail: false
                TapHandler { onTapped: window.showAprs() }
            }
        }
    }

    // ---- actions (menu bar and phone menu) ------------------------------------
    function showMessage(text) {
        if (window.wide) mission.showStatus(text)
        else toast.show(text)
    }
    function alert(title, text) {
        messageDialog.title = title
        messageDialog.text = text
        messageDialog.open()
    }
    function confirm(title, text, onYes) {
        confirmDialog.title = title
        confirmDialog.text = text
        confirmDialog.onYes = onYes
        confirmDialog.open()
    }

    // One file dialog for every import and export.
    property string filePurpose: ""
    function chooseFile(purpose, save, name, filter) {
        filePurpose = purpose
        fileDialog.fileMode = save ? FileDialog.SaveFile : FileDialog.OpenFile
        fileDialog.nameFilters = [filter, qsTr("All files (*)")]
        fileDialog.defaultSuffix = name.length > 0 ? name.split(".").pop() : ""
        fileDialog.currentFolder = mission.documentsUrl()
        if (save) fileDialog.selectedFile = mission.documentsUrl(name)
        fileDialog.open()
    }
    FileDialog {
        id: fileDialog
        objectName: "fileDialog"
        title: window.filePurpose === "import" ? qsTr("Import JSON") : qsTr("Save as")
        onAccepted: {
            var url = selectedFile
            switch (window.filePurpose) {
            case "html": mission.saveHtml(url); break
            case "png": mission.exportPng(url); break
            case "kml": mission.exportKml(url); break
            case "json": mission.exportJson(url); break
            case "import": mission.importJson(url); break
            case "csv": mission.exportHistoryCsv(url); break
            case "pdf":
                if (mission.generatePdf(url, reportDialog.reportTitle, reportDialog.reportSubject,
                                        reportDialog.reportKeywords, reportDialog.reportComment))
                    window.alert(qsTr("Success"), qsTr("PDF report generated:\n%1").arg(mission.displayPath(url)))
                break
            }
            if (!window.wide && mission.statusMessage.length > 0) toast.show(mission.statusMessage)
        }
    }

    function saveHtml() { chooseFile("html", true, "sater_map.html", qsTr("HTML (*.html)")) }
    function exportPng() {
        chooseFile("png", true, mission.suggestedFileName("sater_map", "png", true), qsTr("PNG (*.png)"))
    }
    function exportKml() { chooseFile("kml", true, "sater.kml", qsTr("KML (*.kml)")) }
    function exportJson() { chooseFile("json", true, "sater.json", qsTr("JSON (*.json)")) }
    function importJson() { chooseFile("import", false, "", qsTr("JSON (*.json)")) }
    function exportHistory() { chooseFile("csv", true, "historique_releves.csv", qsTr("CSV (*.csv)")) }
    function generatePdf() { reportDialog.open() }
    function pdfDetailsReady() {
        chooseFile("pdf", true, mission.suggestedFileName("rapport_mission", "pdf", true), qsTr("PDF (*.pdf)"))
    }

    function newMission() {
        confirm(qsTr("New mission"),
                qsTr("Clear the stations, the history, the kilometres, the beacon and the timer?"),
                function() { mission.newMission() })
    }
    function clearStations() {
        if (mission.stations.count === 0) return
        confirm(qsTr("Confirmation"), qsTr("Delete all stations?"), function() { mission.clearStations() })
    }

    function managePresets() { presetsDialog.openFor(false) }
    function loadPresets() {
        mission.presets.reload()
        if (mission.presets.count === 0) {
            alert(qsTr("No preset"), qsTr("No station preset saved.\n\nUse 'Stations > Manage presets' to create some."))
            return
        }
        presetsDialog.openFor(true)
    }
    function saveStationAsPreset(row) {
        if (mission.stations.count === 0) {
            alert(qsTr("No station"), qsTr("Add a station to save as a preset first."))
            return
        }
        if (mission.stations.callsigns().length === 0) {
            alert(qsTr("No valid station"), qsTr("Fill in at least the callsign of a station."))
            return
        }
        savePresetDialog.openFor(row)
    }

    function centreFrance() { if (mapView) mapView.centerOn(46.6796, 3.0761, 6) }
    function centreOnZone() {
        // On the best estimate when there is one, else on the zone.
        if (mission.fixShown) {
            if (mapView) mapView.centerOn(mission.fixLat, mission.fixLon, Math.min(12, mapView.maxZoom))
            return
        }
        if (!mission.hasZone) {
            alert(qsTr("Information"), qsTr("No intersection zone computed.\nAdd at least 2 stations with bearings to create an intersection."))
            return
        }
        if (mapView) mapView.centerOn(mission.zoneLat, mission.zoneLon, Math.min(12, mapView.maxZoom))
    }
    function setBeacon(lat, lon) {
        if (lat !== undefined) beaconDialog.openAt(lat, lon)
        else beaconDialog.openAt(undefined, undefined)
    }
    function downloadTiles() { tileDialog.prepare() }
    function editStation(row) { stationEditor.openFor(row) }
    function chooseColor(initial, onChosen) { colorPicker.choose(initial, onChosen) }

    // ---- dialogs --------------------------------------------------------------------
    AlertDialog { id: messageDialog; objectName: "messageDialog" }
    AlertDialog {
        id: confirmDialog
        objectName: "confirmDialog"
        property var onYes: null
        question: true
        onAccepted: if (onYes) onYes()
    }
    BeaconDialog { id: beaconDialog; objectName: "beaconDialog" }
    PresetsDialog { id: presetsDialog; objectName: "presetsDialog" }
    PresetEditDialog { id: presetEditDialog; objectName: "presetEditDialog" }
    SavePresetDialog { id: savePresetDialog; objectName: "savePresetDialog" }
    ReportDialog { id: reportDialog; objectName: "reportDialog"; onDetailsAccepted: window.pdfDetailsReady() }
    TileDownloadDialog { id: tileDialog; objectName: "tileDialog" }
    AboutDialog { id: aboutDialog; objectName: "aboutDialog" }
    StationEditor { id: stationEditor; objectName: "stationEditor" }
    ColorPicker { id: colorPicker; objectName: "colorPicker" }
    AppDialog {
        id: historyDialog
        objectName: "historyDialog"
        title: qsTr("Bearing history")
        preferredWidth: 760
        fillHeight: true
        standardButtons: Dialog.Close
        HistoryView { anchors.fill: parent }
    }
    AppDialog {
        id: kilometersDialog
        objectName: "kilometersDialog"
        title: qsTr("Distance driven")
        preferredWidth: 380
        standardButtons: Dialog.Ok | Dialog.Cancel
        KilometersView { id: kmView; width: parent.width }
        onAboutToShow: kmView.load()
        onAccepted: kmView.save()
    }
    AppDialog {
        id: preferencesDialog
        objectName: "preferencesDialog"
        title: qsTr("Preferences")
        preferredWidth: 520
        standardButtons: Dialog.Close
        PreferencesView { width: parent.width }
    }
    AppDialog {
        id: aprsDialog
        objectName: "aprsDialog"
        title: "APRS"
        preferredWidth: 620
        standardButtons: Dialog.Close
        AprsView { width: parent.width }
    }
    AprsLogDialog { id: aprsLogDialog }
    function showAprs() { aprsDialog.open() }
    function showAprsLog() { aprsLogDialog.open() }
    function showHistory() { historyDialog.open() }
    function showKilometers() { kilometersDialog.open() }

    Connections {
        target: mission
        function onMessage(title, text) { window.alert(title, text) }
        // A team's bearing came in by APRS.
        function onBearingReceived(callsign, azimuth, uncertainty) {
            window.showMessage(qsTr("Bearing received from %1: %2° ±%3°")
                               .arg(callsign).arg(Math.round(azimuth) % 360).arg(uncertainty.toFixed(1)))
            app.haptic()
        }
    }
    Connections {
        target: mission.tiles
        function onDownloadFinished(saved, failed) { tileDialog.finished(saved, failed) }
    }

    Toast { id: toast; objectName: "toast" }

    // The phone's back gesture closes what is open before the application.
    // Android only: on the desktop, closing the window must close it.
    onClosing: function(close) {
        if (!app.android) return
        if (layoutLoader.item && layoutLoader.item.back && layoutLoader.item.back()) close.accepted = false
    }
}
