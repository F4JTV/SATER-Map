/*
 * PresetModel.h - stations' usual positions, kept between missions.
 *
 * Stored in station_presets.json in the user data folder - the same file
 * as the Python version on Windows (%LOCALAPPDATA%\SATER_Map) and Linux
 * (~/.config/SATER_Map), so existing presets carry over.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "core/MissionData.h"

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>

class PresetModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        CallsignRole,
        LatRole,
        LonRole,
        ColorRole,
        UncertaintyRole,
        DescriptionRole,   // "Name (CALL) - 43°42'0.0"N 7°15'0.0"E"
    };

    explicit PresetModel(const QString &filePath, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_presets.size()); }
    QString filePath() const { return m_filePath; }
    const QList<sater::StationPreset> &presets() const { return m_presets; }
    sater::StationPreset preset(int row) const { return m_presets.value(row); }

    // Read the file again (discards unsaved changes).
    Q_INVOKABLE void reload();
    // Write the file; false (and lastError set) on failure.
    Q_INVOKABLE bool save();
    Q_INVOKABLE QString lastError() const { return m_lastError; }

    // The edit dialog works with a map of the fields.
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void add(const QVariantMap &fields);
    Q_INVOKABLE void update(int row, const QVariantMap &fields);
    Q_INVOKABLE void removeRows(const QList<int> &rows);
    void append(const sater::StationPreset &p);

signals:
    void countChanged();

private:
    static sater::StationPreset fromMap(const QVariantMap &m);
    QString m_filePath;
    QString m_lastError;
    QList<sater::StationPreset> m_presets;
};
