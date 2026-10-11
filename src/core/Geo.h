/*
 * Geo.h - coordinate conversions and direction-finding geometry.
 *
 * Every function here is a line-for-line port of the original Python
 * implementation (SATER Map 2.0, main.py), so a mission computed by either
 * version gives the same intersection zone.  The one deliberate difference
 * is MGRS: the Python code forgot the 500 km row-letter offset of the
 * even-numbered UTM zones (zone 32, east of 6 degrees E, among them), see
 * toMgrs().
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QList>
#include <QPointF>
#include <QString>

namespace sater::geo {

// A coordinate as degrees, minutes, seconds and hemisphere letter.
struct Dms {
    int deg = 0;
    int min = 0;
    double sec = 0.0;
    QChar dir = QLatin1Char('N');
};

// DMS to decimal degrees, rounded to 6 decimals (W and S are negative).
double dmsToDd(int deg, int min, double sec, QChar dir);

// Decimal degrees to DMS, seconds rounded to 2 decimals.  isLat picks N/S
// or E/W.  Unlike the Python version, a rounding carry (59.999" -> 60.00")
// is propagated into the minutes and degrees.
Dms ddToDms(double dd, bool isLat);

// "43°42'0.0"N" - seconds with one decimal, as the Python dd_to_dms_str().
QString ddToDmsString(double dd, bool isLat);

struct Utm {
    int zone = 0;
    QChar letter;
    double easting = 0.0;
    double northing = 0.0;
};

Utm toUtm(double lat, double lon);

// "32T LP 58989 40041".  Correct MGRS letters for odd and even zones.
QString toMgrs(double lat, double lon);

// The UTM zone and latitude band, "32T", as the map's coordinate popup.
QString utmZoneBand(double lat, double lon);

// Destination point at distKm along an initial bearing (spherical Earth,
// R = 6371 km).  Returns (lat, lon).
QPointF endpoint(double lat, double lon, double azimuthDeg, double distKm);

// Planar distance in km (111 km per degree, longitude scaled by the cosine
// of the mean latitude).
double distanceKm(double lat1, double lon1, double lat2, double lon2);

// On the same sphere as endpoint(): the great-circle distance in km, and the
// initial bearing from point 1 towards point 2 in degrees [0, 360).
double greatCircleKm(double lat1, double lon1, double lat2, double lon2);
double initialBearing(double lat1, double lon1, double lat2, double lon2);

// Where a bearing line from a station is taken from.
struct Bearing {
    double lat = 0.0;
    double lon = 0.0;
    double azimuth = 0.0;
};

// (lat, lon) of every pairwise intersection that lies ahead of both
// stations.  The lines run from each station to its endpoint at lengthKm,
// intersected as straight lines in (lon, lat) space.
QList<QPointF> intersections(const QList<Bearing> &bearings, double lengthKm);

struct Circle {
    bool valid = false;
    double lat = 0.0;
    double lon = 0.0;
    double radiusKm = 0.0;
    double surfaceKm2 = 0.0;
};

// The enclosing circle of the intersection points (the Python heuristic),
// with a 0.5 km minimum radius.
Circle uncertaintyCircle(const QList<QPointF> &points);

// ---- Web Mercator tiles -------------------------------------------------
struct TileXY {
    int x = 0;
    int y = 0;
};

TileXY latLonToTile(double lat, double lon, int zoom);

struct TileKey {
    int z = 0;
    int x = 0;
    int y = 0;
};

// Every tile covering the bounds, zoom by zoom, in the Python order.
QList<TileKey> tilesForBounds(double minLat, double minLon, double maxLat, double maxLon,
                              int minZoom, int maxZoom);

// Projection helpers, in pixels of a 256 px tile world at a (fractional)
// zoom.
double lonToWorldX(double lon, double zoom);
double latToWorldY(double lat, double zoom);
double worldXToLon(double x, double zoom);
double worldYToLat(double y, double zoom);

} // namespace sater::geo
