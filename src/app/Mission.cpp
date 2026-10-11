/*
 * Mission.cpp - the mission: stations, bearings, zone, beacon, timer, files.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Mission.h"

#include "AprsLink.h"

#include "core/Exporters.h"
#include "aprs/Ax25.h"
#include "core/Geo.h"
#include "core/Geomag.h"
#include "core/MissionIO.h"
#include "core/ReportWriter.h"
#include "core/TileProviders.h"
#include "core/Tr.h"
#include "map/MapView.h"
#include "map/TileManager.h"

#include <algorithm>
#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <cmath>

using namespace sater;

namespace {

QString defaultDataDir()
{
#if defined(Q_OS_ANDROID)
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    // The Python version's folder: %LOCALAPPDATA%\SATER_Map on Windows,
    // $XDG_CONFIG_HOME/SATER_Map on Linux (presets live there).
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/SATER_Map");
#endif
}

bool writableDir(const QString &dir)
{
    if (!QDir().mkpath(dir)) return false;
    QFile probe(dir + QStringLiteral("/.write_test"));
    if (!probe.open(QIODevice::WriteOnly)) return false;
    probe.close();
    probe.remove();
    return true;
}

QString defaultTilesDir(const QString &dataDir)
{
#if defined(Q_OS_ANDROID)
    return dataDir + QStringLiteral("/tiles");
#else
    // Next to the program, as the Python version (its installer made that
    // folder writable): tiles already downloaded there are kept.
    const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/tiles");
    if (QFileInfo(beside).isDir() && writableDir(beside)) return beside;
#if defined(Q_OS_WIN)
    return dataDir + QStringLiteral("/tiles");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/SATER_Map/tiles");
#endif
#endif
}

QString validColor(const QString &c, const QString &fallback)
{
    const QColor col(c);
    return col.isValid() ? col.name() : fallback;
}

} // namespace

Mission::Mission(QObject *parent)
    : QObject(parent)
    , m_dataDir(defaultDataDir())
{
    QDir().mkpath(m_dataDir);
    m_stations = new StationModel(this);
    m_history = new HistoryModel(this);
    m_presets = new PresetModel(m_dataDir + QStringLiteral("/station_presets.json"), this);
    m_tiles = new TileManager(this);

    connect(m_stations, &StationModel::stationsChanged, this, [this]() {
        recompute();
        scheduleSave();
    });
    connect(m_history, &HistoryModel::historyChanged, this, &Mission::scheduleSave);
    connect(m_tiles, &TileManager::tilesDirChanged, this, &Mission::tilesDirChanged);

    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &Mission::timerChanged);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &Mission::saveNow);

    loadSettings();
    updateDeclinationYear();
    restoreAutosave();
    recompute();
    // After the restore: the team list is the mission's.
    m_aprs = new AprsLink(this, this);
    m_status = SATER_TR("Ready");
}

Mission::~Mission()
{
    if (m_saveTimer.isActive()) saveNow();
    saveSettings();
}

QString Mission::version() const
{
    return QCoreApplication::applicationVersion();
}

// ---- settings -------------------------------------------------------------------

void Mission::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("map"));
    m_lengthKm = std::clamp(s.value(QStringLiteral("azimuthLengthKm"), 100.0).toDouble(), 10.0, 500.0);
    m_showIntersection = s.value(QStringLiteral("showIntersection"), true).toBool();
    m_showGrid = s.value(QStringLiteral("showGrid"), false).toBool();
    m_showEstimate = s.value(QStringLiteral("showEstimate"), true).toBool();
    m_display.estimateColor = validColor(s.value(QStringLiteral("estimateColor")).toString(), m_display.estimateColor);
    m_display.gridColor = validColor(s.value(QStringLiteral("gridColor")).toString(), m_display.gridColor);
    m_display.zoneBorderColor = validColor(s.value(QStringLiteral("zoneBorderColor")).toString(), m_display.zoneBorderColor);
    m_display.zoneFillColor = validColor(s.value(QStringLiteral("zoneFillColor")).toString(), m_display.zoneFillColor);
    m_display.zoneOpacity = std::clamp(s.value(QStringLiteral("zoneOpacity"), 0.3).toDouble(), 0.0, 1.0);
    m_provider = tileProvider(s.value(QStringLiteral("provider"), QStringLiteral("osm")).toString()).key;
    m_offline = s.value(QStringLiteral("offline"), false).toBool();
    QString tilesDir = s.value(QStringLiteral("tilesDir")).toString();
    s.endGroup();
    m_organization = s.value(QStringLiteral("report/organization"), QStringLiteral("ADRASEC 06")).toString();
    m_stations->setMagneticByDefault(s.value(QStringLiteral("stations/magneticByDefault"), false).toBool());
    if (tilesDir.isEmpty()) tilesDir = defaultTilesDir(m_dataDir);
    m_tiles->setTilesDir(tilesDir);
}

void Mission::saveSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("map"));
    s.setValue(QStringLiteral("azimuthLengthKm"), m_lengthKm);
    s.setValue(QStringLiteral("showIntersection"), m_showIntersection);
    s.setValue(QStringLiteral("showGrid"), m_showGrid);
    s.setValue(QStringLiteral("showEstimate"), m_showEstimate);
    s.setValue(QStringLiteral("estimateColor"), m_display.estimateColor);
    s.setValue(QStringLiteral("gridColor"), m_display.gridColor);
    s.setValue(QStringLiteral("zoneBorderColor"), m_display.zoneBorderColor);
    s.setValue(QStringLiteral("zoneFillColor"), m_display.zoneFillColor);
    s.setValue(QStringLiteral("zoneOpacity"), m_display.zoneOpacity);
    s.setValue(QStringLiteral("provider"), m_provider);
    s.setValue(QStringLiteral("offline"), m_offline);
    s.endGroup();
    s.setValue(QStringLiteral("report/organization"), m_organization);
    s.setValue(QStringLiteral("stations/magneticByDefault"), m_stations->magneticByDefault());
}

QString Mission::tilesDir() const
{
    return m_tiles->tilesDir();
}

void Mission::setTilesDir(const QString &dir)
{
    QString d = dir.trimmed();
    if (d.startsWith(QLatin1String("file:"))) d = QUrl(d).toLocalFile();
    if (d.isEmpty()) d = defaultTilesDir(m_dataDir);
    m_tiles->setTilesDir(d);
    QSettings().setValue(QStringLiteral("map/tilesDir"), dir.trimmed().isEmpty() ? QString() : d);
}

// ---- map settings -----------------------------------------------------------------

void Mission::setAzimuthLengthKm(double km)
{
    km = std::clamp(std::round(km), 10.0, 500.0);
    if (km == m_lengthKm) return;
    m_lengthKm = km;
    emit displayChanged();
    recompute();
    saveSettings();
    scheduleSave();
}

void Mission::setShowIntersection(bool on)
{
    if (on == m_showIntersection) return;
    m_showIntersection = on;
    emit displayChanged();
    emit zoneChanged();
    emit mapContentChanged();
    saveSettings();
}

void Mission::setShowGrid(bool on)
{
    if (on == m_showGrid) return;
    m_showGrid = on;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
}

void Mission::setGridColor(const QString &c)
{
    const QString v = validColor(c, m_display.gridColor);
    if (v == m_display.gridColor) return;
    m_display.gridColor = v;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
    scheduleSave();
}

void Mission::setZoneBorderColor(const QString &c)
{
    const QString v = validColor(c, m_display.zoneBorderColor);
    if (v == m_display.zoneBorderColor) return;
    m_display.zoneBorderColor = v;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
    scheduleSave();
}

void Mission::setZoneFillColor(const QString &c)
{
    const QString v = validColor(c, m_display.zoneFillColor);
    if (v == m_display.zoneFillColor) return;
    m_display.zoneFillColor = v;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
    scheduleSave();
}

void Mission::setZoneOpacity(double o)
{
    o = std::clamp(o, 0.0, 1.0);
    if (qFuzzyCompare(o + 1, m_display.zoneOpacity + 1)) return;
    m_display.zoneOpacity = o;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
    scheduleSave();
}

void Mission::setProvider(const QString &key)
{
    const QString k = tileProvider(key).key;
    if (k == m_provider) return;
    m_provider = k;
    emit providerChanged();
    emit mapContentChanged();
    saveSettings();
}

int Mission::providerIndex() const
{
    const auto &list = tileProviders();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).key == m_provider) return i;
    }
    return 0;
}

void Mission::setProviderIndex(int index)
{
    const auto &list = tileProviders();
    if (index >= 0 && index < list.size()) setProvider(list.at(index).key);
}

QStringList Mission::providerNames() const
{
    QStringList out;
    for (const TileProvider &p : tileProviders()) out.append(p.name);
    return out;
}

QString Mission::providerName() const
{
    return tileProvider(m_provider).name;
}

QString Mission::attribution() const
{
    return tileProvider(m_provider).attribution;
}

int Mission::maxZoom() const
{
    return tileProvider(m_provider).maxZoom;
}

void Mission::setOffline(bool on)
{
    if (on == m_offline) return;
    m_offline = on;
    emit providerChanged();
    emit mapContentChanged();
    saveSettings();
}

QString Mission::offlineCheck()
{
    const QString dir = m_tiles->tilesDir() + QLatin1Char('/') + m_provider;
    if (!QFileInfo(dir).isDir()) {
        return SATER_TR("No tiles downloaded for '%1'.\n\nExpected folder:\n%2\n\n"
                        "Use 'Download visible tiles' to download tiles.")
            .arg(providerName(), QDir::toNativeSeparators(dir));
    }
    const int n = m_tiles->storedTilesFor(m_provider);
    if (n == 0) {
        return SATER_TR("The folder exists but contains no tiles:\n%1\n\n"
                        "Use 'Download visible tiles' to download tiles.")
            .arg(QDir::toNativeSeparators(dir));
    }
    showStatus(SATER_TR("Offline mode: %1 tiles available for %2").arg(n).arg(m_provider));
    return QString();
}

// ---- the zone ----------------------------------------------------------------------

void Mission::recompute()
{
    m_zone = computeZone(m_stations->stations(), m_lengthKm);
    m_fix = computeStationFix(m_stations->stations(), m_lengthKm, &m_fixRows);
    emit zoneChanged();
    emit mapContentChanged();
}

void Mission::updateDeclinationYear()
{
    const QDateTime when = m_missionStart.isValid() ? m_missionStart : QDateTime::currentDateTimeUtc();
    m_stations->setDeclinationYear(geomag::decimalYear(when));
}

double Mission::declinationAt(double lat, double lon) const
{
    return geomag::declination(lat, lon, m_stations->declinationYear());
}

QString Mission::declinationText(double declination) const
{
    return geomag::declinationText(declination);
}

QString Mission::declinationModel() const
{
    return QString::fromLatin1(geomag::kModelName);
}

// ---- APRS team ---------------------------------------------------------------------

void Mission::setTeam(const QStringList &team)
{
    QStringList clean;
    for (const QString &c : team) {
        const QString n = aprs::normalizeCallsign(c);
        if (!n.isEmpty() && aprs::isValidCallsign(n) && !clean.contains(n)) clean.append(n);
    }
    if (clean == m_team) return;
    m_team = clean;
    emit teamChanged();
    scheduleSave();
}

QString Mission::teamEntryFor(const QString &source) const
{
    const QString src = aprs::normalizeCallsign(source);
    const QString base = src.section(QLatin1Char('-'), 0, 0);
    for (const QString &entry : m_team) {
        // An entry with an SSID is that station only; without, all its SSIDs.
        if (entry == src || (!entry.contains(QLatin1Char('-')) && entry == base)) return entry;
    }
    return QString();
}

int Mission::applyReceivedBearing(const QString &callsign, double lat, double lon, double trueAzimuth,
                                  double uncertainty, const QString &signal, const QDateTime &when)
{
    // The row of the sender itself: two SSIDs of one callsign are two
    // teams.  A row named without SSID, made beforehand for that callsign,
    // becomes the first SSID heard (and the next one gets a row of its own).
    const QString src = aprs::normalizeCallsign(callsign);
    const QString base = src.section(QLatin1Char('-'), 0, 0);
    int row = -1, bare = -1;
    for (int i = 0; i < m_stations->count(); ++i) {
        const QString c = aprs::normalizeCallsign(m_stations->stations().at(i).callsign);
        // Never this station's own row (named without SSID, it stands for
        // this station whatever its SSID): its bearing is read here.
        if (m_aprs && m_aprs->isMine(c)) continue;
        if (c == src) {
            row = i;
            break;
        }
        if (bare < 0 && base != src && c == base) bare = i;
    }
    if (row < 0 && bare >= 0) {
        row = bare;
        m_stations->setField(row, QStringLiteral("callsign"), src);
    }
    if (row < 0) {
        row = m_stations->addAt(lat, lon);
        m_stations->setField(row, QStringLiteral("callsign"), src);
    }
    m_stations->setPosition(row, lat, lon);
    // APRS bearings are from true north.
    m_stations->setField(row, QStringLiteral("magnetic"), false);
    m_stations->setField(row, QStringLiteral("azimuth"), trueAzimuth);
    m_stations->setField(row, QStringLiteral("uncertainty"), uncertainty);
    if (!signal.isEmpty()) m_stations->setField(row, QStringLiteral("signal"), signal);
    else if (!m_stations->stations().at(row).hasBearing()) m_stations->setField(row, QStringLiteral("signal"), QStringLiteral("S5"));
    m_stations->setField(row, QStringLiteral("shown"), true);

    const Station &s = m_stations->stations().at(row);
    AzimuthRecord r;
    r.timestamp = (when.isValid() ? when.toLocalTime() : QDateTime::currentDateTime()).toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    r.callsign = s.callsign;
    r.azimuth = s.azimuth;
    r.uncertainty = s.uncertainty;
    r.lat = s.lat;
    r.lon = s.lon;
    r.signal = s.signal;
    m_history->append(r);
    showStatus(SATER_TR("APRS bearing from %1: %2° ±%3° (%4)")
                   .arg(s.callsign)
                   .arg(qRound(s.azimuth))
                   .arg(s.uncertainty, 0, 'f', 1)
                   .arg(s.signal));
    emit bearingReceived(s.callsign, s.azimuth, s.uncertainty);
    return row;
}

void Mission::setMagneticByDefault(bool on)
{
    if (on == m_stations->magneticByDefault()) return;
    m_stations->setMagneticByDefault(on);
    emit displayChanged();
    saveSettings();
}

void Mission::setShowEstimate(bool on)
{
    if (on == m_showEstimate) return;
    m_showEstimate = on;
    emit displayChanged();
    emit zoneChanged();
    emit mapContentChanged();
    saveSettings();
}

void Mission::setEstimateColor(const QString &c)
{
    const QString v = validColor(c, m_display.estimateColor);
    if (v == m_display.estimateColor) return;
    m_display.estimateColor = v;
    emit displayChanged();
    emit mapContentChanged();
    saveSettings();
    scheduleSave();
}

QString Mission::fixMgrs() const
{
    return m_fix.valid ? geo::toMgrs(m_fix.lat, m_fix.lon) : QString();
}

QString Mission::fixText() const
{
    if (!m_fix.valid) return QStringLiteral("-");
    const double major = fixMajorKm(), minor = fixMinorKm();
    return SATER_TR("%1, %2\n95 %: %3 × %4 km, axis %5° (%6 km²)")
        .arg(m_fix.lat, 0, 'f', 6)
        .arg(m_fix.lon, 0, 'f', 6)
        .arg(major, 0, 'f', major < 10 ? 2 : 1)
        .arg(minor, 0, 'f', minor < 10 ? 2 : 1)
        .arg(axisDegrees(m_fix.ellipse95.orientationDeg))
        .arg(M_PI * m_fix.ellipse95.semiMajorKm * m_fix.ellipse95.semiMinorKm, 0, 'f', 2);
}

QString Mission::fixWarning() const
{
    const QList<Station> &all = m_stations->stations();
    auto callsignOf = [&](int bearing) {
        const int row = m_fixRows.value(bearing, -1);
        return row >= 0 && row < all.size() ? all.at(row).callsign : QString();
    };
    switch (m_fix.problem) {
    case FixResult::Problem::TooFewBearings:
        return QString();
    case FixResult::Problem::Parallel:
        return SATER_TR("No estimate: the bearings do not cross.");
    case FixResult::Problem::Behind:
        return SATER_TR("No estimate: the bearing of %1 points away from where the others cross.")
            .arg(callsignOf(m_fix.problemIndex));
    case FixResult::Problem::OutOfRange:
        return SATER_TR("No estimate: the bearings cross more than %1 km from %2 (the bearing length).")
            .arg(m_lengthKm, 0, 'f', 0)
            .arg(callsignOf(m_fix.problemIndex));
    case FixResult::Problem::None:
        break;
    }
    if (m_fix.consistent) return QString();
    if (m_fix.suspect < 0) {
        return SATER_TR("The three bearings disagree beyond their uncertainty: one of them is wrong, and three "
                        "cannot tell which. Check them, or take a fourth bearing.");
    }
    const int row = m_fixRows.value(m_fix.suspect, -1);
    const double cone = row >= 0 && row < all.size() ? all.at(row).uncertainty : 0.0;
    if (m_fix.suspectConfirmed) {
        return SATER_TR("%1 disagrees with the other bearings: %2° away from where they cross, for a cone of ±%3°. "
                        "Check it, or hide the station.")
            .arg(callsignOf(m_fix.suspect))
            .arg(std::fabs(m_fix.suspectOffset), 0, 'f', 1)
            .arg(cone, 0, 'f', 1);
    }
    return SATER_TR("The bearings disagree beyond their uncertainty, and leaving one out is not enough: more than "
                    "one may be wrong. %1 is the furthest off (%2°).")
        .arg(callsignOf(m_fix.suspect))
        .arg(std::fabs(m_fix.suspectOffset), 0, 'f', 1);
}

QString Mission::zoneMgrs() const
{
    return m_zone.circle.valid ? geo::toMgrs(m_zone.circle.lat, m_zone.circle.lon) : QString();
}

QString Mission::zoneText() const
{
    if (!m_showIntersection || m_zone.bearingCount < 2) return SATER_TR("Intersection zone: -");
    if (m_zone.points.isEmpty()) return SATER_TR("No intersection");
    const geo::Circle &c = m_zone.circle;
    if (!c.valid) return SATER_TR("Cannot compute");
    if (m_zone.points.size() == 1) {
        return SATER_TR("Intersection: %1, %2\nSurface: %3 km²")
            .arg(c.lat, 0, 'f', 6)
            .arg(c.lon, 0, 'f', 6)
            .arg(c.surfaceKm2, 0, 'f', 2);
    }
    return SATER_TR("Centre: %1, %2\nRadius: %3 km | Surface: %4 km²\n(%5 points)")
        .arg(c.lat, 0, 'f', 6)
        .arg(c.lon, 0, 'f', 6)
        .arg(c.radiusKm, 0, 'f', 2)
        .arg(c.surfaceKm2, 0, 'f', 2)
        .arg(m_zone.points.size());
}

QString Mission::mgrsText() const
{
    if (!zoneShown()) return QStringLiteral("MGRS: -");
    return QStringLiteral("MGRS: ") + zoneMgrs();
}

// ---- stations ----------------------------------------------------------------------

int Mission::addStation()
{
    const int row = m_stations->add();
    showStatus(SATER_TR("Station added (%1)").arg(m_stations->count()));
    return row;
}

int Mission::addStationAt(double lat, double lon)
{
    const int row = m_stations->addAt(lat, lon);
    showStatus(SATER_TR("Station added (%1)").arg(m_stations->count()));
    return row;
}

void Mission::clearStations()
{
    m_stations->clear();
}

void Mission::recordStation(int row)
{
    if (row < 0 || row >= m_stations->count()) return;
    const Station &s = m_stations->stations().at(row);
    if (!s.isValid()) {
        showStatus(SATER_TR("Enter the station's callsign first."));
        return;
    }
    AzimuthRecord r;
    r.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    r.callsign = s.callsign;
    r.azimuth = s.azimuth;
    r.magnetic = s.magnetic;
    r.declination = s.declination;
    r.uncertainty = s.uncertainty;
    r.lat = s.lat;
    r.lon = s.lon;
    r.signal = s.signal;
    m_history->append(r);
    showStatus(SATER_TR("Bearing recorded: %1 %2° (%3)").arg(s.callsign).arg(s.azimuth).arg(s.signal)
               + (s.magnetic ? SATER_TR(", magnetic") : QString()));
}

// ---- beacon ---------------------------------------------------------------------------

void Mission::setBeacon(double lat, double lon)
{
    moveBeacon(lat, lon);
    showStatus(SATER_TR("Beacon position: %1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
}

void Mission::moveBeacon(double lat, double lon)
{
    m_hasBeacon = true;
    m_beaconLat = std::clamp(lat, -90.0, 90.0);
    m_beaconLon = std::clamp(lon, -180.0, 180.0);
    emit beaconChanged();
    emit mapContentChanged();
    scheduleSave();
}

void Mission::clearBeacon()
{
    if (m_hasBeacon) {
        m_hasBeacon = false;
        emit beaconChanged();
        emit mapContentChanged();
        scheduleSave();
    }
    showStatus(SATER_TR("Beacon position cleared"));
}

// ---- kilometres ----------------------------------------------------------------------------

QVariantList Mission::kilometers() const
{
    QVariantList out;
    for (const auto &p : m_km) {
        QVariantMap m;
        m.insert(QStringLiteral("callsign"), p.first);
        m.insert(QStringLiteral("km"), p.second);
        out.append(m);
    }
    return out;
}

double Mission::totalKilometers() const
{
    double t = 0;
    for (const auto &p : m_km) t += p.second;
    return t;
}

double Mission::kilometersFor(const QString &callsign) const
{
    for (const auto &p : m_km) {
        if (p.first == callsign) return p.second;
    }
    return 0.0;
}

void Mission::setKilometers(const QVariantMap &values)
{
    // Keep the station order for the report.
    KilometerList list;
    const QStringList order = m_stations->callsigns();
    QStringList keys = values.keys();
    std::stable_sort(keys.begin(), keys.end(), [&order](const QString &a, const QString &b) {
        const int ia = order.indexOf(a), ib = order.indexOf(b);
        return (ia < 0 ? 1 << 20 : ia) < (ib < 0 ? 1 << 20 : ib);
    });
    for (const QString &k : keys) {
        const double v = values.value(k).toDouble();
        if (v > 0) list.append({k, std::round(v * 10) / 10});
    }
    m_km = list;
    emit kilometersChanged();
    scheduleSave();
}

// ---- timer -------------------------------------------------------------------------------------

void Mission::startTimer()
{
    if (m_timerRunning) return;
    if (!m_missionStart.isValid()) {
        m_missionStart = QDateTime::currentDateTime();
        updateDeclinationYear();
    }
    m_timerRunning = true;
    m_tick.start();
    emit timerChanged();
    scheduleSave();
}

void Mission::stopTimer()
{
    if (!m_timerRunning) return;
    m_elapsedSecs = m_missionStart.secsTo(QDateTime::currentDateTime());
    m_timerRunning = false;
    m_tick.stop();
    emit timerChanged();
    scheduleSave();
}

void Mission::resetTimer()
{
    m_timerRunning = false;
    m_tick.stop();
    m_missionStart = QDateTime();
    m_elapsedSecs = 0;
    updateDeclinationYear();
    emit timerChanged();
    scheduleSave();
}

QString Mission::elapsedText() const
{
    // As the Python MissionTimer: the time since the mission started; Stop
    // freezes the display, Start resumes from the same start time.
    qint64 secs = m_elapsedSecs;
    if (m_timerRunning && m_missionStart.isValid()) secs = m_missionStart.secsTo(QDateTime::currentDateTime());
    secs = std::max<qint64>(0, secs);
    return QStringLiteral("%1:%2:%3")
        .arg(secs / 3600, 2, 10, QLatin1Char('0'))
        .arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0'));
}

QString Mission::startText() const
{
    if (!m_missionStart.isValid()) return QString();
    return SATER_TR("Start: %1").arg(m_missionStart.toString(QStringLiteral("HH:mm:ss")));
}

// ---- whole mission ---------------------------------------------------------------------------------

void Mission::newMission()
{
    m_stations->clear();
    m_history->clear();
    m_km.clear();
    emit kilometersChanged();
    m_hasBeacon = false;
    emit beaconChanged();
    // A new mission is a new operation: its callsigns are entered anew.
    setTeam({});
    resetTimer();
    recompute();
    scheduleSave();
    showStatus(SATER_TR("New mission"));
}

MissionDocument Mission::document() const
{
    MissionDocument d;
    d.version = version();
    d.azimuthLengthKm = m_lengthKm;
    d.stations = m_stations->validStations();
    d.history = m_history->records();
    d.kilometers = m_km;
    if (m_hasBeacon) d.beacon = QPair<double, double>(m_beaconLat, m_beaconLon);
    d.missionStart = m_missionStart;
    d.missionDuration = elapsedText();
    d.settings = m_display;
    d.team = m_team;
    return d;
}

void Mission::applyDocument(const MissionDocument &doc, bool includeTimer)
{
    m_restoring = true;
    m_lengthKm = std::clamp(std::round(doc.azimuthLengthKm), 10.0, 500.0);
    if (includeTimer) {
        // The declinations of the mission's date, before the stations come in.
        m_missionStart = doc.missionStart;
        updateDeclinationYear();
    } else if (doc.missionStart.isValid()) {
        // An import leaves the timer alone, but its compass bearings were
        // read on its own date.
        m_stations->setDeclinationYear(geomag::decimalYear(doc.missionStart));
    }
    m_stations->setStations(doc.stations);
    m_history->setRecords(doc.history);
    m_km = doc.kilometers;
    emit kilometersChanged();
    m_hasBeacon = doc.beacon.has_value();
    if (doc.beacon) {
        m_beaconLat = doc.beacon->first;
        m_beaconLon = doc.beacon->second;
    }
    emit beaconChanged();
    // The autosave is the operation itself.  An imported file brings its
    // callsigns; one without (version 2, or saved without a team) leaves
    // the operation's as they are.
    if (includeTimer || !doc.team.isEmpty()) setTeam(doc.team);
    if (doc.settings) {
        m_display.gridColor = validColor(doc.settings->gridColor, m_display.gridColor);
        m_display.zoneBorderColor = validColor(doc.settings->zoneBorderColor, m_display.zoneBorderColor);
        m_display.zoneFillColor = validColor(doc.settings->zoneFillColor, m_display.zoneFillColor);
        m_display.zoneOpacity = std::clamp(doc.settings->zoneOpacity, 0.0, 1.0);
        m_display.estimateColor = validColor(doc.settings->estimateColor, m_display.estimateColor);
    }
    if (includeTimer) {
        m_missionStart = doc.missionStart;
        emit timerChanged();
    }
    emit displayChanged();
    m_restoring = false;
    recompute();
    saveSettings();
    scheduleSave();
}

// ---- autosave -------------------------------------------------------------------------------------

void Mission::scheduleSave()
{
    if (m_restoring) return;
    m_saveTimer.start();
}

void Mission::saveNow()
{
    m_saveTimer.stop();
    // What only the autosave keeps: the rows still without a callsign (with
    // their north, in the same block as the others) and the timer state.
    MissionDocument doc = document();
    doc.stations = m_stations->stations();
    QJsonObject root = QJsonDocument::fromJson(io::missionToJson(doc)).object();
    root.insert(QStringLiteral("timer_running"), m_timerRunning);
    root.insert(QStringLiteral("elapsed_seconds"), double(m_elapsedSecs));
    QString err;
    io::writeFile(m_dataDir + QStringLiteral("/autosave.json"), QJsonDocument(root).toJson(), &err);
}

void Mission::restoreAutosave()
{
    QByteArray bytes;
    if (!io::readFile(m_dataDir + QStringLiteral("/autosave.json"), &bytes, nullptr)) return;
    MissionDocument doc;
    if (!io::missionFromJson(bytes, &doc, nullptr)) return;
    const QJsonObject root = QJsonDocument::fromJson(bytes).object();
    applyDocument(doc, true);
    m_elapsedSecs = qint64(root.value(QStringLiteral("elapsed_seconds")).toDouble());
    if (root.value(QStringLiteral("timer_running")).toBool() && m_missionStart.isValid()) {
        m_timerRunning = true;
        m_tick.start();
    }
    m_saveTimer.stop();
    emit timerChanged();
}

// ---- presets ---------------------------------------------------------------------------------------

void Mission::loadPresets(const QList<int> &rows)
{
    int n = 0;
    for (int r : rows) {
        if (r < 0 || r >= m_presets->count()) continue;
        const StationPreset p = m_presets->preset(r);
        Station s;
        s.callsign = p.callsign;
        s.lat = p.lat;
        s.lon = p.lon;
        s.azimuth = 0;
        s.uncertainty = p.defaultUncertainty;
        s.color = p.color;
        s.visible = true;
        s.signal = QStringLiteral("S5");
        s.magnetic = m_stations->magneticByDefault();
        m_stations->addStation(s);
        ++n;
    }
    showStatus(SATER_TR("%1 preset(s) loaded").arg(n));
}

bool Mission::saveStationAsPreset(int stationRow, const QString &name)
{
    if (stationRow < 0 || stationRow >= m_stations->count() || name.trimmed().isEmpty()) return false;
    const Station &s = m_stations->stations().at(stationRow);
    StationPreset p;
    p.name = name.trimmed();
    p.callsign = s.callsign;
    p.lat = s.lat;
    p.lon = s.lon;
    p.color = s.color;
    p.defaultUncertainty = s.uncertainty;
    m_presets->reload();
    m_presets->append(p);
    if (!m_presets->save()) {
        m_lastError = m_presets->lastError();
        return false;
    }
    return true;
}

// ---- files ---------------------------------------------------------------------------------------------

QString Mission::pathOf(const QUrl &url) const
{
    if (url.isLocalFile()) return url.toLocalFile();
    if (url.scheme().isEmpty()) return url.toString();
    return url.toString(); // content:// on Android
}

QString Mission::suggestedFileName(const QString &stem, const QString &suffix, bool withTime) const
{
    QString name = stem;
    if (withTime) name += QLatin1Char('_') + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm"));
    return name + QLatin1Char('.') + suffix;
}

QUrl Mission::documentsUrl(const QString &fileName) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    return QUrl::fromLocalFile(fileName.isEmpty() ? dir : dir + QLatin1Char('/') + fileName);
}

QString Mission::displayPath(const QUrl &url) const
{
    if (url.isLocalFile()) return QDir::toNativeSeparators(url.toLocalFile());
    // content://.../document/primary%3ADocuments%2Fsater.json
    const QString last = QUrl::fromPercentEncoding(url.toString().section(QLatin1Char('/'), -1).toUtf8());
    return last.section(QLatin1Char(':'), -1);
}

bool Mission::writeOut(const QUrl &url, const QByteArray &data, const QString &statusFormat)
{
    const QString path = pathOf(url);
    QString err;
    if (!io::writeFile(path, data, &err)) {
        m_lastError = err;
        emit message(SATER_TR("Error"), SATER_TR("Cannot write %1:\n%2").arg(path, err));
        return false;
    }
    showStatus(statusFormat.arg(displayPath(url)));
    return true;
}

bool Mission::exportJson(const QUrl &url)
{
    return writeOut(url, io::missionToJson(document()), SATER_TR("JSON: %1"));
}

bool Mission::importJson(const QUrl &url)
{
    QByteArray bytes;
    QString err;
    if (!io::readFile(pathOf(url), &bytes, &err)) {
        m_lastError = err;
        emit message(SATER_TR("Error"), err);
        return false;
    }
    MissionDocument doc;
    if (!io::missionFromJson(bytes, &doc, &err)) {
        m_lastError = err;
        emit message(SATER_TR("Error"), err);
        return false;
    }
    // As the Python import: the timer is left alone.
    applyDocument(doc, false);
    showStatus(SATER_TR("Imported: %1 stations").arg(m_stations->count()));
    return true;
}

namespace {

ExportContext exportContext(const Mission *m, const MapView *view)
{
    ExportContext c;
    c.stations = m->stations()->validStations();
    c.azimuthLengthKm = m->azimuthLengthKm();
    if (m->hasBeacon()) c.beacon = QPair<double, double>(m->beaconLat(), m->beaconLon());
    c.zone = m->zone();
    if (m->fixShown()) {
        c.fix = m->fix();
        c.fixNote = m->fixWarning();
    }
    c.display = m->display();
    c.providerKey = m->provider();
    if (view) {
        c.hasView = true;
        c.centerLat = view->centerLat();
        c.centerLon = view->centerLon();
        c.zoom = int(std::lround(view->zoom()));
    }
    c.missionStartText = m->timerStarted() ? QString() : SATER_TR("Not started");
    c.missionDurationText = m->elapsedText();
    c.recordCount = m->history()->count();
    return c;
}

} // namespace

bool Mission::exportKml(const QUrl &url)
{
    return writeOut(url, sater::exportKml(exportContext(this, m_view)), SATER_TR("KML: %1"));
}

bool Mission::saveHtml(const QUrl &url)
{
    ExportContext c = exportContext(this, m_view);
    if (m_missionStart.isValid()) c.missionStartText = m_missionStart.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    return writeOut(url, sater::exportStandaloneHtml(c), SATER_TR("Saved: %1"));
}

bool Mission::exportPng(const QUrl &url)
{
    if (!m_view) {
        emit message(SATER_TR("Error"), SATER_TR("The map is not loaded yet."));
        return false;
    }
    const QImage img = m_view->renderImage();
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return writeOut(url, bytes, SATER_TR("PNG exported: %1"));
}

bool Mission::exportHistoryCsv(const QUrl &url)
{
    return writeOut(url, io::historyToCsv(m_history->records()), SATER_TR("History exported: %1"));
}

bool Mission::generatePdf(const QUrl &url, const QString &title, const QString &subject, const QString &keywords,
                          const QString &comment)
{
    ReportData d;
    d.title = title.trimmed().isEmpty() ? SATER_TR("SATER mission report") : title.trimmed();
    d.subject = subject.trimmed();
    d.keywords = keywords.trimmed();
    d.comment = comment.trimmed();
    if (m_missionStart.isValid()) {
        d.startDate = m_missionStart.toString(QStringLiteral("dd/MM/yyyy"));
        d.startTime = m_missionStart.toString(QStringLiteral("HH:mm:ss"));
    }
    d.duration = elapsedText();
    d.stations = m_stations->validStations();
    d.stationCount = int(d.stations.size());
    d.recordCount = m_history->count();
    d.history = m_history->records();
    d.kilometers = m_km;
    if (m_hasBeacon) {
        ReportData::Beacon b;
        b.lat = m_beaconLat;
        b.lon = m_beaconLon;
        b.mgrs = geo::toMgrs(m_beaconLat, m_beaconLon);
        d.beacon = b;
    }
    if (fixShown()) {
        ReportData::Estimate e;
        e.lat = m_fix.lat;
        e.lon = m_fix.lon;
        e.mgrs = geo::toMgrs(e.lat, e.lon);
        e.major95Km = 2 * m_fix.ellipse95.semiMajorKm;
        e.minor95Km = 2 * m_fix.ellipse95.semiMinorKm;
        e.axisDeg = m_fix.ellipse95.orientationDeg;
        e.major50Km = 2 * m_fix.ellipse50.semiMajorKm;
        e.minor50Km = 2 * m_fix.ellipse50.semiMinorKm;
        e.bearingCount = m_fix.degreesOfFreedom + 2;
        if (m_fix.degreesOfFreedom == 0) e.agreement = SATER_TR("Two bearings: their agreement cannot be checked.");
        else if (m_fix.consistent) e.agreement = SATER_TR("The bearings agree within their uncertainty.");
        else e.agreement = fixWarning();
        d.estimate = e;
    }
    if (m_zone.circle.valid) {
        ReportData::Zone z;
        z.lat = m_zone.circle.lat;
        z.lon = m_zone.circle.lon;
        z.radiusKm = m_zone.circle.radiusKm;
        z.surfaceKm2 = m_zone.circle.surfaceKm2;
        z.mgrs = geo::toMgrs(z.lat, z.lon);
        z.pointCount = int(m_zone.points.size());
        d.zone = z;
    }
    if (m_view) d.mapImage = m_view->renderImage();
    d.logo = QImage(QStringLiteral(":/assets/logo.png"));
    d.organization = m_organization;
    d.appVersion = version();

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QString err;
    if (!writeReport(d, &buf, &err)) {
        m_lastError = err;
        emit message(SATER_TR("Error"), SATER_TR("Error while generating the PDF"));
        return false;
    }
    buf.close();
    return writeOut(url, bytes, SATER_TR("PDF: %1"));
}

// ---- tiles -------------------------------------------------------------------------------------------------

QVariantMap Mission::prepareTileDownload()
{
    QVariantMap r;
    m_pendingTiles.clear();
    if (!m_view) {
        r.insert(QStringLiteral("ok"), false);
        r.insert(QStringLiteral("error"), SATER_TR("The map is not loaded yet."));
        return r;
    }
    const QVariantMap b = m_view->bounds();
    const int zoom = int(std::lround(b.value(QStringLiteral("zoom")).toDouble()));
    const TileProvider &p = tileProvider(m_provider);
    const int minZ = std::max(1, zoom - 2);
    const int maxZ = std::min(p.maxZoom, zoom + 2);
    const QList<geo::TileKey> keys = geo::tilesForBounds(
        b.value(QStringLiteral("minLat")).toDouble(), b.value(QStringLiteral("minLon")).toDouble(),
        b.value(QStringLiteral("maxLat")).toDouble(), b.value(QStringLiteral("maxLon")).toDouble(), minZ, maxZ);

    const QString folder = m_tiles->tilesDir() + QLatin1Char('/') + m_provider;
    if (!writableDir(folder)) {
        r.insert(QStringLiteral("ok"), false);
        r.insert(QStringLiteral("error"),
                 SATER_TR("Cannot write in the folder:\n%1\n\nCheck the write permissions.").arg(QDir::toNativeSeparators(folder)));
        return r;
    }
    for (const geo::TileKey &k : keys) m_pendingTiles.append({k.z, {k.x, k.y}});
    m_pendingProvider = m_provider;
    r.insert(QStringLiteral("ok"), !keys.isEmpty());
    r.insert(QStringLiteral("provider"), p.name);
    r.insert(QStringLiteral("folder"), QDir::toNativeSeparators(folder));
    r.insert(QStringLiteral("count"), int(keys.size()));
    r.insert(QStringLiteral("minZoom"), minZ);
    r.insert(QStringLiteral("maxZoom"), maxZ);
    return r;
}

void Mission::startTileDownload()
{
    QList<TileManager::Key> keys;
    for (const auto &t : m_pendingTiles) keys.append({t.first, t.second.first, t.second.second});
    m_tiles->startDownload(m_pendingProvider, keys);
}

// ---- helpers ------------------------------------------------------------------------------------------------

QVariantMap Mission::toDms(double dd, bool isLat) const
{
    const geo::Dms d = geo::ddToDms(dd, isLat);
    return {{QStringLiteral("deg"), d.deg},
            {QStringLiteral("min"), d.min},
            {QStringLiteral("sec"), d.sec},
            {QStringLiteral("dir"), QString(d.dir)}};
}

double Mission::fromDms(int deg, int min, double sec, const QString &dir) const
{
    return geo::dmsToDd(deg, min, sec, dir.isEmpty() ? QLatin1Char('N') : dir.at(0));
}

QString Mission::dmsText(double lat, double lon) const
{
    return geo::ddToDmsString(lat, true) + QLatin1Char(' ') + geo::ddToDmsString(lon, false);
}

QString Mission::mgrs(double lat, double lon) const
{
    return geo::toMgrs(lat, lon);
}

QString Mission::utmZone(double lat, double lon) const
{
    return geo::utmZoneBand(lat, lon);
}

void Mission::showStatus(const QString &message)
{
    m_status = message;
    emit statusMessageChanged();
}

QVariantMap Mission::savedView() const
{
    QSettings s;
    // As the Python version at start: France, zoom 6.
    return {{QStringLiteral("lat"), s.value(QStringLiteral("map/viewLat"), 46.6).toDouble()},
            {QStringLiteral("lon"), s.value(QStringLiteral("map/viewLon"), 2.5).toDouble()},
            {QStringLiteral("zoom"), s.value(QStringLiteral("map/viewZoom"), 6).toDouble()}};
}

void Mission::saveView(double lat, double lon, double zoom)
{
    QSettings s;
    s.setValue(QStringLiteral("map/viewLat"), lat);
    s.setValue(QStringLiteral("map/viewLon"), lon);
    s.setValue(QStringLiteral("map/viewZoom"), std::round(zoom));
}

void Mission::retranslate()
{
    emit zoneChanged();
    emit timerChanged();
    if (m_status == QLatin1String("Ready") || m_status == QCoreApplication::translate("SaterMap", "Ready")
        || m_status.isEmpty())
        m_status = SATER_TR("Ready");
    emit statusMessageChanged();
}

void Mission::setOrganization(const QString &org)
{
    if (org == m_organization) return;
    m_organization = org;
    emit organizationChanged();
    saveSettings();
}

void Mission::setPrimaryView(MapView *view)
{
    m_view = view;
}
