/*
 * Exporters.h - KML and stand-alone HTML (Leaflet) exports.
 *
 * Ports of the Python export_kml() and _standalone_html(): same layers,
 * same styles, same popups; plus the least-squares estimate and its
 * ellipses, and bearings read from magnetic north drawn from true north.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "Analysis.h"
#include "MissionData.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace sater {

struct ExportContext {
    QList<Station> stations;                       // all of them; invalid ones are skipped
    double azimuthLengthKm = 100.0;
    std::optional<QPair<double, double>> beacon;
    ZoneResult zone;
    // The least-squares estimate when it is shown, and what to say of the
    // bearings' agreement (empty when they agree).
    std::optional<FixResult> fix;
    QString fixNote;
    DisplaySettings display;
    QString providerKey = QStringLiteral("osm");
    // The view the HTML page opens on.
    bool hasView = false;
    double centerLat = 43.7;
    double centerLon = 7.25;
    int zoom = 10;
    QString missionStartText;                      // "dd/MM/yyyy HH:mm" or "Not started"
    QString missionDurationText = QStringLiteral("00:00:00");
    int recordCount = 0;
};

// Number as Python's repr(): the shortest text that reads back the same.
QString pyNumber(double v);

QByteArray exportKml(const ExportContext &ctx);
QByteArray exportStandaloneHtml(const ExportContext &ctx);

} // namespace sater
