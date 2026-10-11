/*
 * Mission.h - the mission: stations, bearings, zone, beacon, timer, files.
 *
 * Everything the Python MainWindow held that is not a widget: the station
 * table's content, the history, the kilometres, the beacon, the mission
 * timer, the map settings, and the imports/exports.  Both interfaces
 * (desktop and phone layouts) work on this one object.
 *
 * The mission is saved on every change (autosave.json in the app data
 * folder) and restored at start: Android may stop the app in the
 * background at any time.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "HistoryModel.h"
#include "PresetModel.h"
#include "StationModel.h"
#include "core/Analysis.h"
#include "map/MapView.h"
#include "map/TileManager.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>


class AprsLink;

class Mission : public QObject
{
    Q_OBJECT
    Q_PROPERTY(StationModel *stations READ stations CONSTANT)
    Q_PROPERTY(HistoryModel *history READ history CONSTANT)
    Q_PROPERTY(PresetModel *presets READ presets CONSTANT)
    Q_PROPERTY(TileManager *tiles READ tiles CONSTANT)
    Q_PROPERTY(AprsLink *aprs READ aprs CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

    // ---- map settings -------------------------------------------------------
    Q_PROPERTY(double azimuthLengthKm READ azimuthLengthKm WRITE setAzimuthLengthKm NOTIFY displayChanged)
    Q_PROPERTY(bool showIntersection READ showIntersection WRITE setShowIntersection NOTIFY displayChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY displayChanged)
    Q_PROPERTY(QString gridColor READ gridColor WRITE setGridColor NOTIFY displayChanged)
    Q_PROPERTY(QString zoneBorderColor READ zoneBorderColor WRITE setZoneBorderColor NOTIFY displayChanged)
    Q_PROPERTY(QString zoneFillColor READ zoneFillColor WRITE setZoneFillColor NOTIFY displayChanged)
    Q_PROPERTY(double zoneOpacity READ zoneOpacity WRITE setZoneOpacity NOTIFY displayChanged)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(int providerIndex READ providerIndex WRITE setProviderIndex NOTIFY providerChanged)
    Q_PROPERTY(QStringList providerNames READ providerNames CONSTANT)
    Q_PROPERTY(QString providerName READ providerName NOTIFY providerChanged)
    Q_PROPERTY(QString attribution READ attribution NOTIFY providerChanged)
    Q_PROPERTY(int maxZoom READ maxZoom NOTIFY providerChanged)
    Q_PROPERTY(bool offline READ offline WRITE setOffline NOTIFY providerChanged)

    // ---- the zone (Python's "Informations" box) ------------------------------
    Q_PROPERTY(bool hasZone READ hasZone NOTIFY zoneChanged)
    Q_PROPERTY(bool zoneShown READ zoneShown NOTIFY zoneChanged)
    Q_PROPERTY(int zonePointCount READ zonePointCount NOTIFY zoneChanged)
    Q_PROPERTY(double zoneLat READ zoneLat NOTIFY zoneChanged)
    Q_PROPERTY(double zoneLon READ zoneLon NOTIFY zoneChanged)
    Q_PROPERTY(double zoneRadiusKm READ zoneRadiusKm NOTIFY zoneChanged)
    Q_PROPERTY(double zoneSurfaceKm2 READ zoneSurfaceKm2 NOTIFY zoneChanged)
    Q_PROPERTY(QString zoneMgrs READ zoneMgrs NOTIFY zoneChanged)
    Q_PROPERTY(QString zoneText READ zoneText NOTIFY zoneChanged)
    Q_PROPERTY(QString mgrsText READ mgrsText NOTIFY zoneChanged)

    // ---- the least-squares estimate (Fix.h) ------------------------------------
    Q_PROPERTY(bool showEstimate READ showEstimate WRITE setShowEstimate NOTIFY displayChanged)
    Q_PROPERTY(QString estimateColor READ estimateColor WRITE setEstimateColor NOTIFY displayChanged)
    Q_PROPERTY(bool hasFix READ hasFix NOTIFY zoneChanged)
    Q_PROPERTY(bool fixShown READ fixShown NOTIFY zoneChanged)
    Q_PROPERTY(double fixLat READ fixLat NOTIFY zoneChanged)
    Q_PROPERTY(double fixLon READ fixLon NOTIFY zoneChanged)
    Q_PROPERTY(QString fixMgrs READ fixMgrs NOTIFY zoneChanged)
    // Full axes of the 95 % ellipse, km, and its major axis's azimuth.
    Q_PROPERTY(double fixMajorKm READ fixMajorKm NOTIFY zoneChanged)
    Q_PROPERTY(double fixMinorKm READ fixMinorKm NOTIFY zoneChanged)
    Q_PROPERTY(double fixAxisDeg READ fixAxisDeg NOTIFY zoneChanged)
    Q_PROPERTY(QString fixText READ fixText NOTIFY zoneChanged)
    // Why there is no estimate, or which bearing disagrees; empty if none.
    Q_PROPERTY(QString fixWarning READ fixWarning NOTIFY zoneChanged)

    // ---- north reference ----------------------------------------------------------
    // New stations read their bearings from magnetic north.
    Q_PROPERTY(bool magneticByDefault READ magneticByDefault WRITE setMagneticByDefault NOTIFY displayChanged)
    Q_PROPERTY(QString declinationModel READ declinationModel CONSTANT)

    // ---- APRS -------------------------------------------------------------------------
    // The operation's callsigns: their bearings received by APRS are put on
    // the map (see AprsLink).
    Q_PROPERTY(QStringList team READ team WRITE setTeam NOTIFY teamChanged)

    // ---- the beacon -------------------------------------------------------------
    Q_PROPERTY(bool hasBeacon READ hasBeacon NOTIFY beaconChanged)
    Q_PROPERTY(double beaconLat READ beaconLat NOTIFY beaconChanged)
    Q_PROPERTY(double beaconLon READ beaconLon NOTIFY beaconChanged)

    // ---- kilometres ----------------------------------------------------------------
    Q_PROPERTY(QVariantList kilometers READ kilometers NOTIFY kilometersChanged)
    Q_PROPERTY(double totalKilometers READ totalKilometers NOTIFY kilometersChanged)

    // ---- the mission timer ---------------------------------------------------------
    Q_PROPERTY(bool timerRunning READ timerRunning NOTIFY timerChanged)
    Q_PROPERTY(bool timerStarted READ timerStarted NOTIFY timerChanged)
    Q_PROPERTY(QString elapsedText READ elapsedText NOTIFY timerChanged)
    Q_PROPERTY(QString startText READ startText NOTIFY timerChanged)

    // ---- misc -----------------------------------------------------------------------
    Q_PROPERTY(QString organization READ organization WRITE setOrganization NOTIFY organizationChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString tilesDir READ tilesDir WRITE setTilesDir NOTIFY tilesDirChanged)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    explicit Mission(QObject *parent = nullptr);
    ~Mission() override;

    StationModel *stations() const { return m_stations; }
    HistoryModel *history() const { return m_history; }
    PresetModel *presets() const { return m_presets; }
    TileManager *tiles() const { return m_tiles; }
    AprsLink *aprs() const { return m_aprs; }
    QString version() const;

    double azimuthLengthKm() const { return m_lengthKm; }
    void setAzimuthLengthKm(double km);
    bool showIntersection() const { return m_showIntersection; }
    void setShowIntersection(bool on);
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool on);
    QString gridColor() const { return m_display.gridColor; }
    void setGridColor(const QString &c);
    QString zoneBorderColor() const { return m_display.zoneBorderColor; }
    void setZoneBorderColor(const QString &c);
    QString zoneFillColor() const { return m_display.zoneFillColor; }
    void setZoneFillColor(const QString &c);
    double zoneOpacity() const { return m_display.zoneOpacity; }
    void setZoneOpacity(double o);
    QString provider() const { return m_provider; }
    void setProvider(const QString &key);
    int providerIndex() const;
    void setProviderIndex(int index);
    QStringList providerNames() const;
    QString providerName() const;
    QString attribution() const;
    int maxZoom() const;
    bool offline() const { return m_offline; }
    void setOffline(bool on);

    bool hasZone() const { return m_zone.circle.valid; }
    bool zoneShown() const { return m_showIntersection && m_zone.circle.valid; }
    int zonePointCount() const { return int(m_zone.points.size()); }
    double zoneLat() const { return m_zone.circle.lat; }
    double zoneLon() const { return m_zone.circle.lon; }
    double zoneRadiusKm() const { return m_zone.circle.radiusKm; }
    double zoneSurfaceKm2() const { return m_zone.circle.surfaceKm2; }
    QString zoneMgrs() const;
    QString zoneText() const;
    QString mgrsText() const;
    const sater::ZoneResult &zone() const { return m_zone; }

    bool showEstimate() const { return m_showEstimate; }
    void setShowEstimate(bool on);
    QString estimateColor() const { return m_display.estimateColor; }
    void setEstimateColor(const QString &c);
    bool hasFix() const { return m_fix.valid; }
    bool fixShown() const { return m_showEstimate && m_fix.valid; }
    double fixLat() const { return m_fix.lat; }
    double fixLon() const { return m_fix.lon; }
    QString fixMgrs() const;
    double fixMajorKm() const { return 2 * m_fix.ellipse95.semiMajorKm; }
    double fixMinorKm() const { return 2 * m_fix.ellipse95.semiMinorKm; }
    double fixAxisDeg() const { return m_fix.ellipse95.orientationDeg; }
    QString fixText() const;
    QString fixWarning() const;
    const sater::FixResult &fix() const { return m_fix; }
    // For each bearing of the fix, the station's row.
    const QList<int> &fixRows() const { return m_fixRows; }

    QStringList team() const { return m_team; }
    void setTeam(const QStringList &team);
    // The team entry a packet's source belongs to ("F4AAA" for F4AAA-7 when
    // the list says F4AAA), empty if none.
    QString teamEntryFor(const QString &source) const;
    // A bearing received from the team: the station (made if needed) moved
    // there with this bearing, and recorded in the history.  Returns its row.
    int applyReceivedBearing(const QString &callsign, double lat, double lon, double trueAzimuth,
                             double uncertainty, const QString &signal, const QDateTime &when);

    bool magneticByDefault() const { return m_stations->magneticByDefault(); }
    void setMagneticByDefault(bool on);
    QString declinationModel() const;
    const sater::DisplaySettings &display() const { return m_display; }

    bool hasBeacon() const { return m_hasBeacon; }
    double beaconLat() const { return m_beaconLat; }
    double beaconLon() const { return m_beaconLon; }

    QVariantList kilometers() const;
    double totalKilometers() const;

    bool timerRunning() const { return m_timerRunning; }
    bool timerStarted() const { return m_missionStart.isValid(); }
    QString elapsedText() const;
    QString startText() const;

    QString organization() const { return m_organization; }
    void setOrganization(const QString &org);
    QString statusMessage() const { return m_status; }
    QString tilesDir() const;
    void setTilesDir(const QString &dir);
    QString dataDir() const { return m_dataDir; }

    // The map the PNG and PDF exports draw (the one on screen).
    void setPrimaryView(MapView *view);
    MapView *primaryView() const { return m_view; }

    // ---- stations -----------------------------------------------------------
    Q_INVOKABLE int addStation();
    Q_INVOKABLE int addStationAt(double lat, double lon);
    Q_INVOKABLE void clearStations();
    // Store the station's current bearing in the history.
    Q_INVOKABLE void recordStation(int row);

    // ---- beacon ---------------------------------------------------------------
    Q_INVOKABLE void setBeacon(double lat, double lon);
    Q_INVOKABLE void moveBeacon(double lat, double lon);   // drag: no status message
    Q_INVOKABLE void clearBeacon();

    // ---- kilometres -------------------------------------------------------------
    Q_INVOKABLE double kilometersFor(const QString &callsign) const;
    // map callsign -> km; zero entries are dropped (as Python).
    Q_INVOKABLE void setKilometers(const QVariantMap &values);

    // ---- timer ------------------------------------------------------------------
    Q_INVOKABLE void startTimer();
    Q_INVOKABLE void stopTimer();
    Q_INVOKABLE void resetTimer();

    // ---- whole mission -------------------------------------------------------------
    // Stations, history, kilometres, beacon and timer cleared.
    Q_INVOKABLE void newMission();

    // ---- presets -------------------------------------------------------------------
    // Add the presets (rows of the preset model) as new stations.
    Q_INVOKABLE void loadPresets(const QList<int> &rows);
    Q_INVOKABLE bool saveStationAsPreset(int stationRow, const QString &name);

    // ---- files (QML hands file:// or content:// URLs) -------------------------------
    Q_INVOKABLE bool exportJson(const QUrl &url);
    Q_INVOKABLE bool importJson(const QUrl &url);
    Q_INVOKABLE bool exportKml(const QUrl &url);
    Q_INVOKABLE bool saveHtml(const QUrl &url);
    Q_INVOKABLE bool exportPng(const QUrl &url);
    Q_INVOKABLE bool exportHistoryCsv(const QUrl &url);
    Q_INVOKABLE bool generatePdf(const QUrl &url, const QString &title, const QString &subject,
                                 const QString &keywords, const QString &comment);
    Q_INVOKABLE QString lastError() const { return m_lastError; }
    // A default file name with the current time ("sater_map_20261010_1830.png").
    Q_INVOKABLE QString suggestedFileName(const QString &stem, const QString &suffix, bool withTime) const;
    // A file URL in the documents folder for a file name (desktop dialogs
    // open there).
    Q_INVOKABLE QUrl documentsUrl(const QString &fileName = QString()) const;
    Q_INVOKABLE QUrl fileUrl(const QString &path) const { return QUrl::fromLocalFile(path); }
    // A URL the dialogs return, as the user should read it: the local path,
    // or the file name of an Android document.
    Q_INVOKABLE QString displayPath(const QUrl &url) const;

    // ---- tiles ---------------------------------------------------------------------------
    // What "Download visible tiles" would fetch for the primary map view:
    // {ok, error, provider, folder, count, minZoom, maxZoom}.
    Q_INVOKABLE QVariantMap prepareTileDownload();
    Q_INVOKABLE void startTileDownload();
    // Warning text when offline mode has no tiles for the provider, else
    // empty (and a status message).
    Q_INVOKABLE QString offlineCheck();

    // ---- coordinate helpers for QML -----------------------------------------------------
    Q_INVOKABLE QVariantMap toDms(double dd, bool isLat) const;
    Q_INVOKABLE double fromDms(int deg, int min, double sec, const QString &dir) const;
    Q_INVOKABLE QString dmsText(double lat, double lon) const;
    Q_INVOKABLE QString mgrs(double lat, double lon) const;
    Q_INVOKABLE QString utmZone(double lat, double lon) const;
    // The magnetic declination there on the mission's date, degrees east.
    Q_INVOKABLE double declinationAt(double lat, double lon) const;
    // "3.2° E", "0.4° W".
    Q_INVOKABLE QString declinationText(double declination) const;

    Q_INVOKABLE void showStatus(const QString &message);
    // The map view at the last exit: {lat, lon, zoom}, France at first.
    Q_INVOKABLE QVariantMap savedView() const;
    Q_INVOKABLE void saveView(double lat, double lon, double zoom);
    // The language changed: the texts made here are asked for again.
    Q_INVOKABLE void retranslate();

    // Saved state: written now (also done by itself after each change).
    Q_INVOKABLE void saveNow();

    // For the tests and the autosave.
    sater::MissionDocument document() const;
    void applyDocument(const sater::MissionDocument &doc, bool includeTimer);

signals:
    void displayChanged();
    void providerChanged();
    void zoneChanged();
    void beaconChanged();
    void kilometersChanged();
    void timerChanged();
    void organizationChanged();
    void statusMessageChanged();
    void tilesDirChanged();
    void teamChanged();
    // A bearing came in by APRS (for a notice on the screen).
    void bearingReceived(const QString &callsign, double azimuth, double uncertainty);
    // Something drawn on the map changed.
    void mapContentChanged();
    // Show a message box (title, text) - errors and confirmations.
    void message(const QString &title, const QString &text);

private:
    void recompute();
    // The declinations are those of the mission's date: its start, or now.
    void updateDeclinationYear();
    void scheduleSave();
    void loadSettings();
    void saveSettings() const;
    void restoreAutosave();
    QString pathOf(const QUrl &url) const;
    bool writeOut(const QUrl &url, const QByteArray &data, const QString &statusFormat);

    StationModel *m_stations = nullptr;
    HistoryModel *m_history = nullptr;
    PresetModel *m_presets = nullptr;
    TileManager *m_tiles = nullptr;
    AprsLink *m_aprs = nullptr;
    QPointer<MapView> m_view;

    double m_lengthKm = 100.0;
    bool m_showIntersection = true;
    bool m_showGrid = false;
    sater::DisplaySettings m_display;
    QString m_provider = QStringLiteral("osm");
    bool m_offline = false;

    sater::ZoneResult m_zone;
    sater::FixResult m_fix;
    QList<int> m_fixRows;
    bool m_showEstimate = true;
    bool m_hasBeacon = false;
    double m_beaconLat = 0.0;
    double m_beaconLon = 0.0;
    sater::KilometerList m_km;
    QStringList m_team;

    QDateTime m_missionStart;
    qint64 m_elapsedSecs = 0;
    bool m_timerRunning = false;
    QTimer m_tick;

    QString m_organization = QStringLiteral("ADRASEC 06");
    QString m_status;
    QString m_lastError;
    QString m_dataDir;
    QTimer m_saveTimer;
    bool m_restoring = false;

    QList<QPair<int, QPair<int, int>>> m_pendingTiles;   // z, (x, y)
    QString m_pendingProvider;
};
