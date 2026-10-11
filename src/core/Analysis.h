/*
 * Analysis.h - the intersection zone of a set of stations.
 *
 * As the Python _do_update_map(): only visible stations with a callsign
 * and a signal other than S0 take part; two of them at least are needed.
 * The bearings are taken from true north (a magnetic reading corrected by
 * the declination).
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "Fix.h"
#include "Geo.h"
#include "MissionData.h"

#include <QList>
#include <QPointF>

namespace sater {

struct ZoneResult {
    int bearingCount = 0;          // stations taking part
    QList<QPointF> points;         // (lat, lon) of each intersection
    geo::Circle circle;            // valid when points is not empty
};

inline ZoneResult computeZone(const QList<Station> &stations, double lengthKm)
{
    ZoneResult r;
    QList<geo::Bearing> bearings;
    for (const Station &s : stations) {
        if (!s.isValid() || !s.visible || !s.hasBearing()) continue;
        bearings.append({s.lat, s.lon, s.trueAzimuth()});
    }
    r.bearingCount = int(bearings.size());
    if (bearings.size() >= 2) {
        r.points = geo::intersections(bearings, lengthKm);
        r.circle = geo::uncertaintyCircle(r.points);
    }
    return r;
}

// The least-squares fix of the same stations (Fix.h); rows: for each
// bearing of the fix, the station's index in the list.
inline FixResult computeStationFix(const QList<Station> &stations, double lengthKm, QList<int> *rows = nullptr)
{
    QList<BearingObservation> obs;
    if (rows) rows->clear();
    for (int i = 0; i < stations.size(); ++i) {
        const Station &s = stations.at(i);
        if (!s.isValid() || !s.visible || !s.hasBearing()) continue;
        obs.append({s.lat, s.lon, s.trueAzimuth(), bearingSigma(s.uncertainty)});
        if (rows) rows->append(i);
    }
    return computeFix(obs, lengthKm);
}

} // namespace sater
