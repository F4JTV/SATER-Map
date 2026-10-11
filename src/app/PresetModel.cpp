/*
 * PresetModel.cpp - stations' usual positions, kept between missions.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PresetModel.h"

#include "core/Geo.h"
#include "core/MissionIO.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

PresetModel::PresetModel(const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_filePath(filePath)
{
    reload();
}

int PresetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_presets.size());
}

QHash<int, QByteArray> PresetModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {CallsignRole, "callsign"},
        {LatRole, "lat"},
        {LonRole, "lon"},
        {ColorRole, "presetColor"},
        {UncertaintyRole, "defaultUncertainty"},
        {DescriptionRole, "description"},
    };
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_presets.size()) return {};
    const sater::StationPreset &p = m_presets.at(index.row());
    switch (role) {
    case NameRole: return p.name;
    case CallsignRole: return p.callsign;
    case LatRole: return p.lat;
    case LonRole: return p.lon;
    case ColorRole: return p.color;
    case UncertaintyRole: return p.defaultUncertainty;
    case DescriptionRole:
        return QStringLiteral("%1 (%2) - %3 %4")
            .arg(p.name, p.callsign, sater::geo::ddToDmsString(p.lat, true), sater::geo::ddToDmsString(p.lon, false));
    default: return {};
    }
}

void PresetModel::reload()
{
    QList<sater::StationPreset> list;
    QFile f(m_filePath);
    if (f.open(QIODevice::ReadOnly)) list = sater::io::presetsFromJson(f.readAll());
    beginResetModel();
    m_presets = list;
    endResetModel();
    emit countChanged();
}

bool PresetModel::save()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QString err;
    if (!sater::io::writeFile(m_filePath, sater::io::presetsToJson(m_presets), &err)) {
        m_lastError = err;
        return false;
    }
    m_lastError.clear();
    return true;
}

sater::StationPreset PresetModel::fromMap(const QVariantMap &m)
{
    sater::StationPreset p;
    p.name = m.value(QStringLiteral("name")).toString().trimmed();
    p.callsign = m.value(QStringLiteral("callsign")).toString().trimmed().toUpper();
    p.lat = m.value(QStringLiteral("lat")).toDouble();
    p.lon = m.value(QStringLiteral("lon")).toDouble();
    const QColor c(m.value(QStringLiteral("color"), QStringLiteral("#e74c3c")).toString());
    p.color = c.isValid() ? c.name() : QStringLiteral("#e74c3c");
    p.defaultUncertainty = std::clamp(m.value(QStringLiteral("defaultUncertainty"), 5.0).toDouble(), 0.0, 30.0);
    return p;
}

QVariantMap PresetModel::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_presets.size()) return m;
    const sater::StationPreset &p = m_presets.at(row);
    m.insert(QStringLiteral("name"), p.name);
    m.insert(QStringLiteral("callsign"), p.callsign);
    m.insert(QStringLiteral("lat"), p.lat);
    m.insert(QStringLiteral("lon"), p.lon);
    m.insert(QStringLiteral("color"), p.color);
    m.insert(QStringLiteral("defaultUncertainty"), p.defaultUncertainty);
    return m;
}

void PresetModel::add(const QVariantMap &fields)
{
    append(fromMap(fields));
}

void PresetModel::append(const sater::StationPreset &p)
{
    const int row = int(m_presets.size());
    beginInsertRows(QModelIndex(), row, row);
    m_presets.append(p);
    endInsertRows();
    emit countChanged();
}

void PresetModel::update(int row, const QVariantMap &fields)
{
    if (row < 0 || row >= m_presets.size()) return;
    m_presets[row] = fromMap(fields);
    emit dataChanged(index(row), index(row));
}

void PresetModel::removeRows(const QList<int> &rows)
{
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    beginResetModel();
    for (int r : sorted) {
        if (r >= 0 && r < m_presets.size()) m_presets.removeAt(r);
    }
    endResetModel();
    emit countChanged();
}
