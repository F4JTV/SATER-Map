/*
 * StationModel.cpp - the direction-finding stations of the mission.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "StationModel.h"

#include "core/Geo.h"
#include "core/Geomag.h"

#include <algorithm>
#include <QColor>
#include <QDateTime>

#include <cmath>

using sater::Station;

StationModel::StationModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_declinationYear(sater::geomag::decimalYear(QDateTime::currentDateTimeUtc()))
{
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it) m_roleByName.insert(QString::fromLatin1(it.value()), it.key());
}

int StationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_stations.size());
}

QHash<int, QByteArray> StationModel::roleNames() const
{
    return {
        {IdRole, "stationId"},
        {CallsignRole, "callsign"},
        {LatRole, "lat"},
        {LonRole, "lon"},
        {AzimuthRole, "azimuth"},
        {UncertaintyRole, "uncertainty"},
        {ColorRole, "stationColor"},
        {VisibleRole, "shown"},
        {SignalRole, "signal"},
        {NotesRole, "notes"},
        {LatDegRole, "latDeg"},
        {LatMinRole, "latMin"},
        {LatSecRole, "latSec"},
        {LatDirRole, "latDir"},
        {LonDegRole, "lonDeg"},
        {LonMinRole, "lonMin"},
        {LonSecRole, "lonSec"},
        {LonDirRole, "lonDir"},
        {LatTextRole, "latText"},
        {LonTextRole, "lonText"},
        {ValidRole, "valid"},
        {HasBearingRole, "hasBearing"},
        {MagneticRole, "magnetic"},
        {DeclinationRole, "declination"},
        {TrueAzimuthRole, "trueAzimuth"},
    };
}

QVariant StationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_stations.size()) return {};
    const Station &s = m_stations.at(index.row());
    switch (role) {
    case IdRole: return s.id;
    case CallsignRole: return s.callsign;
    case LatRole: return s.lat;
    case LonRole: return s.lon;
    case AzimuthRole: return s.azimuth;
    case UncertaintyRole: return s.uncertainty;
    case ColorRole: return s.color;
    case VisibleRole: return s.visible;
    case SignalRole: return s.signal;
    case NotesRole: return s.notes;
    case LatDegRole: return sater::geo::ddToDms(s.lat, true).deg;
    case LatMinRole: return sater::geo::ddToDms(s.lat, true).min;
    case LatSecRole: return sater::geo::ddToDms(s.lat, true).sec;
    case LatDirRole: return QString(sater::geo::ddToDms(s.lat, true).dir);
    case LonDegRole: return sater::geo::ddToDms(s.lon, false).deg;
    case LonMinRole: return sater::geo::ddToDms(s.lon, false).min;
    case LonSecRole: return sater::geo::ddToDms(s.lon, false).sec;
    case LonDirRole: return QString(sater::geo::ddToDms(s.lon, false).dir);
    case LatTextRole: return sater::geo::ddToDmsString(s.lat, true);
    case LonTextRole: return sater::geo::ddToDmsString(s.lon, false);
    case ValidRole: return s.isValid();
    case HasBearingRole: return s.hasBearing();
    case MagneticRole: return s.magnetic;
    case DeclinationRole: return s.declination;
    case TrueAzimuthRole: return s.trueAzimuth();
    default: return {};
    }
}

bool StationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) return false;
    const QByteArray name = roleNames().value(role);
    if (name.isEmpty()) return false;
    setField(index.row(), QString::fromLatin1(name), value);
    return true;
}

QList<Station> StationModel::validStations() const
{
    QList<Station> out;
    for (const Station &s : m_stations) {
        if (s.isValid()) out.append(s);
    }
    return out;
}

void StationModel::setStations(const QList<Station> &stations)
{
    beginResetModel();
    m_stations = stations;
    for (Station &s : m_stations) {
        if (s.id.isEmpty()) s.id = sater::newStationId();
        updateDeclination(s);
    }
    endResetModel();
    emit countChanged();
    emit stationsChanged();
}

int StationModel::add()
{
    Station s;
    s.id = sater::newStationId();
    s.color = sater::stationColors().at(m_stations.size() % sater::stationColors().size());
    s.magnetic = m_magneticByDefault;
    return addStation(s);
}

int StationModel::addAt(double lat, double lon)
{
    Station s;
    s.id = sater::newStationId();
    s.color = sater::stationColors().at(m_stations.size() % sater::stationColors().size());
    s.magnetic = m_magneticByDefault;
    s.lat = std::clamp(lat, -90.0, 90.0);
    s.lon = std::clamp(lon, -180.0, 180.0);
    return addStation(s);
}

int StationModel::addStation(const Station &station)
{
    Station s = station;
    if (s.id.isEmpty()) s.id = sater::newStationId();
    updateDeclination(s);
    const int row = int(m_stations.size());
    beginInsertRows(QModelIndex(), row, row);
    m_stations.append(s);
    endInsertRows();
    emit countChanged();
    emit stationsChanged();
    return row;
}

void StationModel::remove(int row)
{
    if (row < 0 || row >= m_stations.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_stations.removeAt(row);
    endRemoveRows();
    emit countChanged();
    emit stationsChanged();
}

void StationModel::clear()
{
    if (m_stations.isEmpty()) return;
    beginResetModel();
    m_stations.clear();
    endResetModel();
    emit countChanged();
    emit stationsChanged();
}

void StationModel::changed(int row, const QList<int> &roles)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, roles);
    emit stationsChanged();
}

void StationModel::setField(int row, const QString &name, const QVariant &value)
{
    if (row < 0 || row >= m_stations.size()) return;
    Station &s = m_stations[row];
    const int role = m_roleByName.value(name, -1);
    switch (role) {
    case CallsignRole: {
        const QString v = value.toString().trimmed().toUpper();
        if (v == s.callsign) return;
        s.callsign = v;
        changed(row, {CallsignRole, ValidRole});
        return;
    }
    case LatRole: setPosition(row, value.toDouble(), s.lon); return;
    case LonRole: setPosition(row, s.lat, value.toDouble()); return;
    case AzimuthRole: {
        double v = std::fmod(value.toDouble(), 360.0);
        if (v < 0) v += 360.0;
        if (qFuzzyCompare(v + 1, s.azimuth + 1)) return;
        s.azimuth = v;
        changed(row, {AzimuthRole, TrueAzimuthRole});
        return;
    }
    case MagneticRole:
        if (value.toBool() == s.magnetic) return;
        s.magnetic = value.toBool();
        changed(row, {MagneticRole, TrueAzimuthRole});
        return;
    case UncertaintyRole: {
        const double v = std::clamp(value.toDouble(), 0.0, 30.0);
        if (qFuzzyCompare(v + 1, s.uncertainty + 1)) return;
        s.uncertainty = v;
        changed(row, {UncertaintyRole});
        return;
    }
    case ColorRole: {
        const QColor c(value.toString());
        if (!c.isValid() || c.name() == s.color) return;
        s.color = c.name();
        changed(row, {ColorRole});
        return;
    }
    case VisibleRole:
        if (value.toBool() == s.visible) return;
        s.visible = value.toBool();
        changed(row, {VisibleRole});
        return;
    case SignalRole: {
        const QString v = value.toString();
        if (!sater::signalLevels().contains(v) || v == s.signal) return;
        s.signal = v;
        changed(row, {SignalRole, HasBearingRole});
        return;
    }
    case NotesRole:
        if (value.toString() == s.notes) return;
        s.notes = value.toString();
        changed(row, {NotesRole});
        return;
    default:
        return;
    }
}

void StationModel::setLatDms(int row, int deg, int min, double sec, const QString &dir)
{
    if (row < 0 || row >= m_stations.size()) return;
    deg = std::clamp(deg, 0, 90);
    min = std::clamp(min, 0, 59);
    sec = std::clamp(sec, 0.0, 59.99);
    const QChar d = dir.isEmpty() ? QLatin1Char('N') : dir.at(0).toUpper();
    setPosition(row, std::clamp(sater::geo::dmsToDd(deg, min, sec, d), -90.0, 90.0), m_stations.at(row).lon);
}

void StationModel::setLonDms(int row, int deg, int min, double sec, const QString &dir)
{
    if (row < 0 || row >= m_stations.size()) return;
    deg = std::clamp(deg, 0, 180);
    min = std::clamp(min, 0, 59);
    sec = std::clamp(sec, 0.0, 59.99);
    const QChar d = dir.isEmpty() ? QLatin1Char('E') : dir.at(0).toUpper();
    setPosition(row, m_stations.at(row).lat, std::clamp(sater::geo::dmsToDd(deg, min, sec, d), -180.0, 180.0));
}

void StationModel::setPosition(int row, double lat, double lon)
{
    if (row < 0 || row >= m_stations.size()) return;
    Station &s = m_stations[row];
    lat = std::clamp(lat, -90.0, 90.0);
    lon = std::clamp(lon, -180.0, 180.0);
    if (lat == s.lat && lon == s.lon) return;
    s.lat = lat;
    s.lon = lon;
    updateDeclination(s);
    changed(row, {LatRole, LonRole, LatDegRole, LatMinRole, LatSecRole, LatDirRole, LonDegRole, LonMinRole,
                  LonSecRole, LonDirRole, LatTextRole, LonTextRole, DeclinationRole, TrueAzimuthRole});
}

bool StationModel::updateDeclination(Station &s) const
{
    const double d = sater::geomag::declination(s.lat, s.lon, m_declinationYear);
    if (d == s.declination) return false;
    s.declination = d;
    return true;
}

void StationModel::setDeclinationYear(double year)
{
    if (year == m_declinationYear) return;
    m_declinationYear = year;
    bool any = false;
    for (Station &s : m_stations) any |= updateDeclination(s);
    if (!any) return;
    emit dataChanged(index(0), index(int(m_stations.size()) - 1), {DeclinationRole, TrueAzimuthRole});
    emit stationsChanged();
}

QVariantMap StationModel::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_stations.size()) return m;
    const QHash<int, QByteArray> roles = roleNames();
    const QModelIndex idx = index(row);
    for (auto it = roles.begin(); it != roles.end(); ++it) m.insert(QString::fromLatin1(it.value()), data(idx, it.key()));
    return m;
}

int StationModel::rowOfId(const QString &id) const
{
    for (int i = 0; i < m_stations.size(); ++i) {
        if (m_stations.at(i).id == id) return i;
    }
    return -1;
}

int StationModel::rowOfCallsign(const QString &callsign) const
{
    for (int i = 0; i < m_stations.size(); ++i) {
        if (m_stations.at(i).callsign == callsign) return i;
    }
    return -1;
}

QStringList StationModel::callsigns() const
{
    QStringList out;
    for (const Station &s : m_stations) {
        if (s.isValid()) out.append(s.callsign);
    }
    return out;
}
