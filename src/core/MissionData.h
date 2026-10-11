/*
 * MissionData.h - the records a mission is made of.
 *
 * Field names and meanings follow the Python version's dataclasses
 * (Station, AzimuthRecord, StationPreset) so the JSON files stay
 * interchangeable between the two versions.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <cmath>
#include <optional>

namespace sater {

// The S-meter scale offered for each bearing.  "S0" means the station is
// on the map but heard nothing: no bearing line, no part in the zone.
inline const QStringList &signalLevels()
{
    static const QStringList levels = {
        QStringLiteral("S0"), QStringLiteral("S1"), QStringLiteral("S2"), QStringLiteral("S3"),
        QStringLiteral("S4"), QStringLiteral("S5"), QStringLiteral("S6"), QStringLiteral("S7"),
        QStringLiteral("S8"), QStringLiteral("S9"), QStringLiteral("S9+10"), QStringLiteral("S9+20"),
        QStringLiteral("S9+30")};
    return levels;
}

// The colours new stations get, in turn.
inline const QStringList &stationColors()
{
    static const QStringList colors = {
        QStringLiteral("#e74c3c"), QStringLiteral("#3498db"), QStringLiteral("#2ecc71"),
        QStringLiteral("#9b59b6"), QStringLiteral("#f39c12"), QStringLiteral("#1abc9c"),
        QStringLiteral("#e91e63"), QStringLiteral("#795548")};
    return colors;
}

// An 8-character identifier, as str(uuid.uuid4())[:8] in Python.
QString newStationId();

// A bearing in [0, 360).
inline double normalizeAzimuth(double a)
{
    a = std::fmod(a, 360.0);
    return a < 0 ? a + 360.0 : a;
}

struct Station {
    QString id;                 // "station_id" in JSON
    QString callsign;
    double lat = 0.0;
    double lon = 0.0;
    double azimuth = 0.0;
    double uncertainty = 5.0;
    QString color = QStringLiteral("#e74c3c");
    bool visible = true;
    QString signal = QStringLiteral("S5");
    QString notes;
    // The azimuth as read: from magnetic north (a compass) or from true
    // north.  declination is the WMM value at the station on the mission's
    // date, kept up to date by the station model; maps and computations use
    // trueAzimuth().  (Not in the Python JSON keys: see MissionIO.)
    bool magnetic = false;
    double declination = 0.0;

    // A station without a callsign is a row still being filled in: it is
    // not drawn, exported or used for the zone (as in Python).
    bool isValid() const { return !callsign.trimmed().isEmpty(); }
    bool hasBearing() const { return signal != QLatin1String("S0"); }
    double trueAzimuth() const { return magnetic ? normalizeAzimuth(azimuth + declination) : azimuth; }
};

struct AzimuthRecord {
    QString timestamp;          // "dd/MM/yyyy HH:mm:ss"
    QString callsign;
    double azimuth = 0.0;       // as read (see Station)
    double uncertainty = 0.0;
    double lat = 0.0;
    double lon = 0.0;
    QString signal = QStringLiteral("S5");
    bool magnetic = false;
    double declination = 0.0;   // when it was recorded

    double trueAzimuth() const { return magnetic ? normalizeAzimuth(azimuth + declination) : azimuth; }
};

struct StationPreset {
    QString name;
    QString callsign;
    double lat = 0.0;
    double lon = 0.0;
    QString color = QStringLiteral("#e74c3c");
    double defaultUncertainty = 5.0;
};

struct DisplaySettings {
    QString gridColor = QStringLiteral("#00ff00");
    QString zoneBorderColor = QStringLiteral("#f44336");
    QString zoneFillColor = QStringLiteral("#ffeb3b");
    double zoneOpacity = 0.3;
    QString estimateColor = QStringLiteral("#d500f9");   // the least-squares ellipse
};

// The kilometres each station drove, in the order they were entered.
using KilometerList = QList<QPair<QString, double>>;

struct MissionDocument {
    QString version;
    double azimuthLengthKm = 100.0;
    QList<Station> stations;
    QList<AzimuthRecord> history;
    KilometerList kilometers;
    std::optional<QPair<double, double>> beacon;   // (lat, lon)
    QDateTime missionStart;                          // invalid when not started
    QString missionDuration = QStringLiteral("00:00:00");
    std::optional<DisplaySettings> settings;
    // The operation's callsigns whose APRS bearings are taken in (a callsign
    // without SSID stands for all its SSIDs).  Not in version 2's keys.
    QStringList team;
};

} // namespace sater
