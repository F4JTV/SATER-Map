/*
 * HistoryModel.h - the bearings recorded during the mission.
 *
 * Kept in the order they were taken; presented newest first, as the
 * Python HistoryDialog table.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "core/MissionData.h"

#include <QAbstractListModel>
#include <QList>

class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        TimestampRole = Qt::UserRole + 1,
        CallsignRole,
        AzimuthRole,
        UncertaintyRole,
        SignalRole,
        LatRole,
        LonRole,
        DmsRole,
        MagneticRole,
        TrueAzimuthRole,
    };

    explicit HistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_records.size()); }
    const QList<sater::AzimuthRecord> &records() const { return m_records; }
    void setRecords(const QList<sater::AzimuthRecord> &records);
    void append(const sater::AzimuthRecord &record);

    // Rows as shown (newest first).
    Q_INVOKABLE void removeRows(const QList<int> &rows);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void historyChanged();

private:
    int indexOfRow(int row) const { return int(m_records.size()) - 1 - row; }
    QList<sater::AzimuthRecord> m_records;
};
