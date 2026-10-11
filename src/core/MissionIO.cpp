/*
 * MissionIO.cpp - reading and writing missions, presets and history.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "MissionIO.h"

#include "Geo.h"
#include "Tr.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

namespace sater {

QString newStationId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

namespace io {

namespace {

double num(const QJsonValue &v, double fallback = 0.0)
{
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok ? d : fallback;
    }
    return fallback;
}

QString str(const QJsonValue &v, const QString &fallback = QString())
{
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    return fallback;
}

// Python's datetime.isoformat() writes microseconds; Qt reads at most
// milliseconds.
QDateTime parseIso(const QString &text)
{
    QString t = text.trimmed();
    static const QRegularExpression frac(QStringLiteral("(\\.\\d{3})\\d+"));
    t.replace(frac, QStringLiteral("\\1"));
    QDateTime dt = QDateTime::fromString(t, Qt::ISODateWithMs);
    if (!dt.isValid()) dt = QDateTime::fromString(t, Qt::ISODate);
    return dt;
}

} // namespace

QJsonObject stationToJson(const Station &s)
{
    // Exactly the Station dataclass fields: the Python import calls
    // Station(**data) and rejects any other key.  The azimuth is the true
    // one, so version 2 draws a magnetic reading where it belongs; the
    // reading itself is in the "sater_map_3" block (northToJson).
    QJsonObject o;
    o.insert(QStringLiteral("callsign"), s.callsign);
    o.insert(QStringLiteral("lat"), s.lat);
    o.insert(QStringLiteral("lon"), s.lon);
    o.insert(QStringLiteral("azimuth"), s.trueAzimuth());
    o.insert(QStringLiteral("uncertainty"), s.uncertainty);
    o.insert(QStringLiteral("color"), s.color);
    o.insert(QStringLiteral("visible"), s.visible);
    o.insert(QStringLiteral("signal"), s.signal);
    o.insert(QStringLiteral("notes"), s.notes);
    o.insert(QStringLiteral("station_id"), s.id);
    return o;
}

Station stationFromJson(const QJsonObject &o)
{
    Station s;
    s.callsign = str(o.value(QStringLiteral("callsign")));
    s.lat = num(o.value(QStringLiteral("lat")));
    s.lon = num(o.value(QStringLiteral("lon")));
    s.azimuth = num(o.value(QStringLiteral("azimuth")));
    s.uncertainty = num(o.value(QStringLiteral("uncertainty")), 5.0);
    s.color = str(o.value(QStringLiteral("color")), QStringLiteral("#e74c3c"));
    s.visible = o.value(QStringLiteral("visible")).toBool(true);
    s.signal = str(o.value(QStringLiteral("signal")), QStringLiteral("S5"));
    if (!signalLevels().contains(s.signal)) s.signal = QStringLiteral("S5");
    s.notes = str(o.value(QStringLiteral("notes")));
    s.id = str(o.value(QStringLiteral("station_id")));
    if (s.id.isEmpty()) s.id = newStationId();
    return s;
}

QJsonObject recordToJson(const AzimuthRecord &r)
{
    // AzimuthRecord(**data) in Python: the same keys only, true azimuth.
    QJsonObject o;
    o.insert(QStringLiteral("timestamp"), r.timestamp);
    o.insert(QStringLiteral("callsign"), r.callsign);
    o.insert(QStringLiteral("azimuth"), r.trueAzimuth());
    o.insert(QStringLiteral("uncertainty"), r.uncertainty);
    o.insert(QStringLiteral("lat"), r.lat);
    o.insert(QStringLiteral("lon"), r.lon);
    o.insert(QStringLiteral("signal"), r.signal);
    return o;
}

AzimuthRecord recordFromJson(const QJsonObject &o)
{
    AzimuthRecord r;
    r.timestamp = str(o.value(QStringLiteral("timestamp")));
    r.callsign = str(o.value(QStringLiteral("callsign")));
    r.azimuth = num(o.value(QStringLiteral("azimuth")));
    r.uncertainty = num(o.value(QStringLiteral("uncertainty")));
    r.lat = num(o.value(QStringLiteral("lat")));
    r.lon = num(o.value(QStringLiteral("lon")));
    r.signal = str(o.value(QStringLiteral("signal")), QStringLiteral("S5"));
    return r;
}

namespace {

// A magnetic reading: what was read, and the declination it was corrected by.
QJsonObject magneticReading(double azimuth, double declination)
{
    QJsonObject o;
    o.insert(QStringLiteral("north"), QStringLiteral("magnetic"));
    o.insert(QStringLiteral("azimuth"), azimuth);
    o.insert(QStringLiteral("declination"), declination);
    return o;
}

const QString kExtension = QStringLiteral("sater_map_3");

} // namespace

QByteArray missionToJson(const MissionDocument &doc)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), doc.version);
    root.insert(QStringLiteral("azimuth_length_km"), doc.azimuthLengthKm);

    QJsonArray stations;
    for (const Station &s : doc.stations) stations.append(stationToJson(s));
    root.insert(QStringLiteral("stations"), stations);

    QJsonArray history;
    for (const AzimuthRecord &r : doc.history) history.append(recordToJson(r));
    root.insert(QStringLiteral("history"), history);

    QJsonObject km;
    for (const auto &p : doc.kilometers) km.insert(p.first, p.second);
    root.insert(QStringLiteral("kilometers"), km);

    if (doc.beacon) {
        QJsonObject b;
        b.insert(QStringLiteral("lat"), doc.beacon->first);
        b.insert(QStringLiteral("lon"), doc.beacon->second);
        root.insert(QStringLiteral("beacon"), b);
    } else {
        root.insert(QStringLiteral("beacon"), QJsonValue::Null);
    }

    if (doc.missionStart.isValid()) {
        root.insert(QStringLiteral("mission_start"),
                    doc.missionStart.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")));
    } else {
        root.insert(QStringLiteral("mission_start"), QJsonValue::Null);
    }
    root.insert(QStringLiteral("mission_duration"), doc.missionDuration);

    const DisplaySettings ds = doc.settings.value_or(DisplaySettings());
    QJsonObject settings;
    settings.insert(QStringLiteral("grid_color"), ds.gridColor);
    settings.insert(QStringLiteral("zone_border_color"), ds.zoneBorderColor);
    settings.insert(QStringLiteral("zone_fill_color"), ds.zoneFillColor);
    settings.insert(QStringLiteral("zone_opacity"), ds.zoneOpacity);
    root.insert(QStringLiteral("settings"), settings);

    // What version 2 has no key for, in a block it does not read: the
    // bearings read from magnetic north (station id or history index).
    QJsonObject ext;
    QJsonObject stationReadings, historyReadings;
    for (const Station &s : doc.stations) {
        if (s.magnetic) stationReadings.insert(s.id, magneticReading(s.azimuth, s.declination));
    }
    for (int i = 0; i < doc.history.size(); ++i) {
        const AzimuthRecord &r = doc.history.at(i);
        if (r.magnetic) historyReadings.insert(QString::number(i), magneticReading(r.azimuth, r.declination));
    }
    if (!stationReadings.isEmpty()) ext.insert(QStringLiteral("stations"), stationReadings);
    if (!historyReadings.isEmpty()) ext.insert(QStringLiteral("history"), historyReadings);
    ext.insert(QStringLiteral("estimate_color"), ds.estimateColor);
    if (!doc.team.isEmpty()) ext.insert(QStringLiteral("team"), QJsonArray::fromStringList(doc.team));
    root.insert(kExtension, ext);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool missionFromJson(const QByteArray &bytes, MissionDocument *doc, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument jd = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = pe.errorString();
        return false;
    }
    if (!jd.isObject()) {
        if (error) *error = SATER_TR("The file does not contain a SATER Map mission.");
        return false;
    }
    const QJsonObject root = jd.object();

    MissionDocument d;
    d.version = str(root.value(QStringLiteral("version")));
    if (root.contains(QStringLiteral("azimuth_length_km")))
        d.azimuthLengthKm = num(root.value(QStringLiteral("azimuth_length_km")), 100.0);

    const QJsonArray stations = root.value(QStringLiteral("stations")).toArray();
    for (const QJsonValue &v : stations) {
        if (v.isObject()) d.stations.append(stationFromJson(v.toObject()));
    }
    const QJsonArray history = root.value(QStringLiteral("history")).toArray();
    for (const QJsonValue &v : history) {
        if (v.isObject()) d.history.append(recordFromJson(v.toObject()));
    }
    const QJsonObject km = root.value(QStringLiteral("kilometers")).toObject();
    for (auto it = km.begin(); it != km.end(); ++it) d.kilometers.append({it.key(), num(it.value())});

    const QJsonValue beacon = root.value(QStringLiteral("beacon"));
    if (beacon.isObject()) {
        const QJsonObject b = beacon.toObject();
        d.beacon = QPair<double, double>(num(b.value(QStringLiteral("lat"))), num(b.value(QStringLiteral("lon"))));
    }
    const QJsonValue start = root.value(QStringLiteral("mission_start"));
    if (start.isString()) d.missionStart = parseIso(start.toString());
    d.missionDuration = str(root.value(QStringLiteral("mission_duration")), QStringLiteral("00:00:00"));

    if (root.value(QStringLiteral("settings")).isObject()) {
        const QJsonObject s = root.value(QStringLiteral("settings")).toObject();
        DisplaySettings ds;
        ds.gridColor = str(s.value(QStringLiteral("grid_color")), ds.gridColor);
        ds.zoneBorderColor = str(s.value(QStringLiteral("zone_border_color")), ds.zoneBorderColor);
        ds.zoneFillColor = str(s.value(QStringLiteral("zone_fill_color")), ds.zoneFillColor);
        ds.zoneOpacity = num(s.value(QStringLiteral("zone_opacity")), ds.zoneOpacity);
        d.settings = ds;
    }

    // Magnetic readings (written by version 3): back to what was read.
    const QJsonObject ext = root.value(kExtension).toObject();
    auto applyReading = [](const QJsonValue &v, double *azimuth, bool *magnetic, double *declination) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("north")).toString() != QLatin1String("magnetic")) return;
        *magnetic = true;
        *azimuth = normalizeAzimuth(num(o.value(QStringLiteral("azimuth")), *azimuth));
        *declination = num(o.value(QStringLiteral("declination")));
    };
    const QJsonObject stationReadings = ext.value(QStringLiteral("stations")).toObject();
    for (Station &s : d.stations) {
        if (stationReadings.contains(s.id)) applyReading(stationReadings.value(s.id), &s.azimuth, &s.magnetic, &s.declination);
    }
    const QJsonObject historyReadings = ext.value(QStringLiteral("history")).toObject();
    for (int i = 0; i < d.history.size(); ++i) {
        AzimuthRecord &r = d.history[i];
        const QString key = QString::number(i);
        if (historyReadings.contains(key)) applyReading(historyReadings.value(key), &r.azimuth, &r.magnetic, &r.declination);
    }
    for (const QJsonValue &v : ext.value(QStringLiteral("team")).toArray()) {
        const QString c = v.toString().trimmed().toUpper();
        if (!c.isEmpty() && !d.team.contains(c)) d.team.append(c);
    }
    if (ext.contains(QStringLiteral("estimate_color"))) {
        DisplaySettings ds = d.settings.value_or(DisplaySettings());
        ds.estimateColor = str(ext.value(QStringLiteral("estimate_color")), ds.estimateColor);
        d.settings = ds;
    }
    *doc = d;
    return true;
}

QByteArray presetsToJson(const QList<StationPreset> &presets)
{
    QJsonArray list;
    for (const StationPreset &p : presets) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), p.name);
        o.insert(QStringLiteral("callsign"), p.callsign);
        o.insert(QStringLiteral("lat"), p.lat);
        o.insert(QStringLiteral("lon"), p.lon);
        o.insert(QStringLiteral("color"), p.color);
        o.insert(QStringLiteral("default_uncertainty"), p.defaultUncertainty);
        list.append(o);
    }
    return QJsonDocument(list).toJson(QJsonDocument::Indented);
}

QList<StationPreset> presetsFromJson(const QByteArray &bytes, bool *ok)
{
    QList<StationPreset> result;
    QJsonParseError pe;
    const QJsonDocument jd = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !jd.isArray()) {
        if (ok) *ok = false;
        return result;
    }
    for (const QJsonValue &v : jd.array()) {
        const QJsonObject o = v.toObject();
        StationPreset p;
        p.name = str(o.value(QStringLiteral("name")));
        p.callsign = str(o.value(QStringLiteral("callsign")));
        p.lat = num(o.value(QStringLiteral("lat")));
        p.lon = num(o.value(QStringLiteral("lon")));
        p.color = str(o.value(QStringLiteral("color")), QStringLiteral("#e74c3c"));
        p.defaultUncertainty = num(o.value(QStringLiteral("default_uncertainty")), 5.0);
        result.append(p);
    }
    if (ok) *ok = true;
    return result;
}

QByteArray historyToCsv(const QList<AzimuthRecord> &history)
{
    // The columns of version 2, then the north the azimuth was read from
    // and the azimuth from true north.
    QString out = SATER_TR("Time;Callsign;Azimuth;Uncertainty;Signal;Lat_DD;Lon_DD;Lat_DMS;Lon_DMS;North;True_azimuth");
    out += QLatin1Char('\n');
    for (const AzimuthRecord &r : history) {
        out += QStringLiteral("%1;%2;%3;%4;%5;%6;%7;%8;%9;%10;%11\n")
                   .arg(r.timestamp, r.callsign)
                   .arg(r.azimuth)
                   .arg(r.uncertainty)
                   .arg(r.signal)
                   .arg(r.lat, 0, 'g', 15)
                   .arg(r.lon, 0, 'g', 15)
                   .arg(geo::ddToDmsString(r.lat, true), geo::ddToDmsString(r.lon, false))
                   .arg(r.magnetic ? SATER_TR("magnetic") : SATER_TR("true"))
                   .arg(QString::number(r.trueAzimuth(), 'f', 1));
    }
    return out.toUtf8();
}

namespace {

bool isUri(const QString &s)
{
    return s.startsWith(QLatin1String("content:"), Qt::CaseInsensitive);
}

} // namespace

bool writeFile(const QString &pathOrUri, const QByteArray &data, QString *error)
{
    if (isUri(pathOrUri)) {
        // Android's storage framework: QFile understands content:// URIs.
        QFile f(pathOrUri);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error) *error = f.errorString();
            return false;
        }
        const bool ok = f.write(data) == data.size();
        if (!ok && error) *error = f.errorString();
        f.close();
        return ok;
    }
    QSaveFile f(pathOrUri);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(data);
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool readFile(const QString &pathOrUri, QByteArray *data, QString *error)
{
    QFile f(pathOrUri);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    *data = f.readAll();
    return true;
}

} // namespace io
} // namespace sater
