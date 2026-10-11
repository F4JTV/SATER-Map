/*
 * Geomag.h - the magnetic declination, from the World Magnetic Model.
 *
 * A bearing read on a compass is measured from magnetic north; the map
 * needs it from true north.  The difference, the declination, comes from
 * WMM2025 (NOAA NCEI and the British Geological Survey, public domain),
 * valid from 2025.0 to 2030.0: about 3 degrees east in the south of France,
 * growing by some 0.1 degree a year.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDateTime>
#include <QString>

namespace sater::geomag {

inline constexpr double kModelEpoch = 2025.0;
inline constexpr double kModelEnd = 2030.0;
inline constexpr const char *kModelName = "WMM2025";

struct Field {
    double declination = 0.0;   // degrees, east positive
    double inclination = 0.0;   // degrees, down positive
};

// The field at a geodetic latitude/longitude (degrees), an altitude above
// the WGS 84 ellipsoid (km) and a date in decimal years.  Dates outside the
// model's life span are extrapolated (less accurately).
Field compute(double lat, double lon, double altKm, double decimalYear);

// The declination at ground level, degrees east.
inline double declination(double lat, double lon, double decimalYear)
{
    return compute(lat, lon, 0.0, decimalYear).declination;
}

// 2026-10-11 12:00 UTC -> 2026.777...
double decimalYear(const QDateTime &when);

// "3.2° E", "0.4° W" (translated).
QString declinationText(double declination);

} // namespace sater::geomag
