/*
 * StationModel.h - the direction-finding stations of the mission.
 *
 * One row per station, as the rows of the Python StationsTableWidget.  The
 * position is kept in decimal degrees at full precision; the DMS roles are
 * views of it, and writing one of them rebuilds the position.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "core/MissionData.h"

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>

class StationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList signalLevels READ signalLevels CONSTANT)
    Q_PROPERTY(QStringList palette READ palette CONSTANT)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        CallsignRole,
        LatRole,
        LonRole,
        AzimuthRole,
        UncertaintyRole,
        ColorRole,
        VisibleRole,
        SignalRole,
        NotesRole,
        LatDegRole,
        LatMinRole,
        LatSecRole,
        LatDirRole,
        LonDegRole,
        LonMinRole,
        LonSecRole,
        LonDirRole,
        LatTextRole,     // 43°42'0.0"N
        LonTextRole,
        ValidRole,       // has a callsign
        HasBearingRole,  // signal is not S0
        MagneticRole,    // the azimuth is read from magnetic north
        DeclinationRole, // WMM declination at the station, degrees east
        TrueAzimuthRole, // the azimuth from true north
    };

    explicit StationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_stations.size()); }
    QStringList signalLevels() const { return sater::signalLevels(); }
    QStringList palette() const { return sater::stationColors(); }

    const QList<sater::Station> &stations() const { return m_stations; }
    // The stations with a callsign (what Python's get_all_stations() gave).
    QList<sater::Station> validStations() const;
    void setStations(const QList<sater::Station> &stations);

    // ---- QML ---------------------------------------------------------------
    // Append an empty row (next colour of the palette); returns its row.
    Q_INVOKABLE int add();
    Q_INVOKABLE int addAt(double lat, double lon);
    int addStation(const sater::Station &s);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void clear();
    // Set one field by role name ("callsign", "azimuth", ...).
    Q_INVOKABLE void setField(int row, const QString &name, const QVariant &value);
    Q_INVOKABLE void setLatDms(int row, int deg, int min, double sec, const QString &dir);
    Q_INVOKABLE void setLonDms(int row, int deg, int min, double sec, const QString &dir);
    Q_INVOKABLE void setPosition(int row, double lat, double lon);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int rowOfId(const QString &id) const;
    Q_INVOKABLE int rowOfCallsign(const QString &callsign) const;
    // Callsigns of the stations that have one, in table order.
    Q_INVOKABLE QStringList callsigns() const;

    // The date the declinations are computed for (decimal year): the
    // mission's start, or now.
    void setDeclinationYear(double year);
    double declinationYear() const { return m_declinationYear; }
    // New stations read their bearings from magnetic north.
    void setMagneticByDefault(bool on) { m_magneticByDefault = on; }
    bool magneticByDefault() const { return m_magneticByDefault; }

signals:
    void countChanged();
    // Anything changed: rows, positions, bearings, colours, visibility.
    void stationsChanged();

private:
    void changed(int row, const QList<int> &roles = {});
    // Recompute the declination of a station; true when it moved.
    bool updateDeclination(sater::Station &s) const;
    QHash<QString, int> m_roleByName;
    QList<sater::Station> m_stations;
    double m_declinationYear = 0.0;
    bool m_magneticByDefault = false;
};
