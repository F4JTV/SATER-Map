/*
 * HistoryModel.cpp - the bearings recorded during the mission.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "HistoryModel.h"

#include "core/Geo.h"

#include <algorithm>

HistoryModel::HistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_records.size());
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {
        {TimestampRole, "timestamp"},
        {CallsignRole, "callsign"},
        {AzimuthRole, "azimuth"},
        {UncertaintyRole, "uncertainty"},
        {SignalRole, "signal"},
        {LatRole, "lat"},
        {LonRole, "lon"},
        {DmsRole, "dms"},
        {MagneticRole, "magnetic"},
        {TrueAzimuthRole, "trueAzimuth"},
    };
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    const int i = indexOfRow(index.row());
    if (i < 0 || i >= m_records.size()) return {};
    const sater::AzimuthRecord &r = m_records.at(i);
    switch (role) {
    case TimestampRole: return r.timestamp;
    case CallsignRole: return r.callsign;
    case AzimuthRole: return r.azimuth;
    case UncertaintyRole: return r.uncertainty;
    case SignalRole: return r.signal.isEmpty() ? QStringLiteral("S5") : r.signal;
    case LatRole: return r.lat;
    case LonRole: return r.lon;
    case DmsRole:
        return sater::geo::ddToDmsString(r.lat, true) + QLatin1Char(' ') + sater::geo::ddToDmsString(r.lon, false);
    case MagneticRole: return r.magnetic;
    case TrueAzimuthRole: return r.trueAzimuth();
    default: return {};
    }
}

void HistoryModel::setRecords(const QList<sater::AzimuthRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countChanged();
    emit historyChanged();
}

void HistoryModel::append(const sater::AzimuthRecord &record)
{
    // Newest first: the new record is row 0.
    beginInsertRows(QModelIndex(), 0, 0);
    m_records.append(record);
    endInsertRows();
    emit countChanged();
    emit historyChanged();
}

void HistoryModel::removeRows(const QList<int> &rows)
{
    if (rows.isEmpty()) return;
    QList<int> indices;
    for (int row : rows) {
        const int i = indexOfRow(row);
        if (i >= 0 && i < m_records.size() && !indices.contains(i)) indices.append(i);
    }
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    beginResetModel();
    for (int i : indices) m_records.removeAt(i);
    endResetModel();
    emit countChanged();
    emit historyChanged();
}

void HistoryModel::clear()
{
    if (m_records.isEmpty()) return;
    beginResetModel();
    m_records.clear();
    endResetModel();
    emit countChanged();
    emit historyChanged();
}
