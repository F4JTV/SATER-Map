/*
 * Geo.cpp - coordinate conversions and direction-finding geometry.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Geo.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sater::geo {

namespace {

double roundTo(double v, int decimals)
{
    const double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

constexpr double kEarthRadiusKm = 6371.0;

} // namespace

double dmsToDd(int deg, int min, double sec, QChar dir)
{
    double dd = deg + (min / 60.0) + (sec / 3600.0);
    const QChar d = dir.toUpper();
    if (d == QLatin1Char('W') || d == QLatin1Char('S')) dd = -dd;
    return roundTo(dd, 6);
}

Dms ddToDms(double dd, bool isLat)
{
    const bool positive = dd >= 0;
    dd = std::fabs(dd);
    Dms r;
    r.deg = int(dd);
    r.min = int((dd - r.deg) * 60);
    r.sec = roundTo((dd - r.deg - r.min / 60.0) * 3600, 2);
    // Carry a rounded 60" into the minutes (the Python code printed 60.0").
    if (r.sec >= 60.0) {
        r.sec -= 60.0;
        if (r.sec < 0.005) r.sec = 0.0;
        ++r.min;
    }
    if (r.min >= 60) {
        r.min -= 60;
        ++r.deg;
    }
    if (isLat) r.dir = positive ? QLatin1Char('N') : QLatin1Char('S');
    else r.dir = positive ? QLatin1Char('E') : QLatin1Char('W');
    return r;
}

QString ddToDmsString(double dd, bool isLat)
{
    const Dms d = ddToDms(dd, isLat);
    // printf rounds the exact binary value, ties to even, as Python's
    // format() does ("49.25" -> "49.2"); QString::number rounds ties up.
    char sec[32];
    std::snprintf(sec, sizeof sec, "%.1f", d.sec);
    return QStringLiteral("%1°%2'%3\"%4")
        .arg(d.deg)
        .arg(d.min)
        .arg(QLatin1String(sec))
        .arg(d.dir);
}

Utm toUtm(double lat, double lon)
{
    int zone = int((lon + 180) / 6) + 1;

    if (56 <= lat && lat < 64 && 3 <= lon && lon < 12) {
        zone = 32;
    } else if (72 <= lat && lat < 84) {
        if (0 <= lon && lon < 9) zone = 31;
        else if (9 <= lon && lon < 21) zone = 33;
        else if (21 <= lon && lon < 33) zone = 35;
        else if (33 <= lon && lon < 42) zone = 37;
    }

    static const char letters[] = "CDEFGHJKLMNPQRSTUVWXX";
    QChar letter = QLatin1Char('Z');
    if (-80 <= lat && lat <= 84) letter = QLatin1Char(letters[int((lat + 80) / 8)]);

    const double a = 6378137.0;
    const double f = 1 / 298.257223563;
    const double k0 = 0.9996;

    const double e = std::sqrt(f * (2 - f));
    const double e2 = e * e;
    const double ep2 = e2 / (1 - e2);

    const double lon0 = qDegreesToRadians(double((zone - 1) * 6 - 180 + 3));
    const double latRad = qDegreesToRadians(lat);
    const double lonRad = qDegreesToRadians(lon);

    const double N = a / std::sqrt(1 - e2 * std::pow(std::sin(latRad), 2));
    const double T = std::pow(std::tan(latRad), 2);
    const double C = ep2 * std::pow(std::cos(latRad), 2);
    const double A = (lonRad - lon0) * std::cos(latRad);

    const double M = a * ((1 - e2 / 4 - 3 * std::pow(e2, 2) / 64 - 5 * std::pow(e2, 3) / 256) * latRad
                          - (3 * e2 / 8 + 3 * std::pow(e2, 2) / 32 + 45 * std::pow(e2, 3) / 1024) * std::sin(2 * latRad)
                          + (15 * std::pow(e2, 2) / 256 + 45 * std::pow(e2, 3) / 1024) * std::sin(4 * latRad)
                          - (35 * std::pow(e2, 3) / 3072) * std::sin(6 * latRad));

    const double easting = k0 * N * (A + (1 - T + C) * std::pow(A, 3) / 6
                                     + (5 - 18 * T + std::pow(T, 2) + 72 * C - 58 * ep2) * std::pow(A, 5) / 120)
                           + 500000;
    double northing = k0 * (M + N * std::tan(latRad) * (std::pow(A, 2) / 2
                                                         + (5 - T + 9 * C + 4 * std::pow(C, 2)) * std::pow(A, 4) / 24
                                                         + (61 - 58 * T + std::pow(T, 2) + 600 * C - 330 * ep2) * std::pow(A, 6) / 720));
    if (lat < 0) northing += 10000000;

    Utm u;
    u.zone = zone;
    u.letter = letter;
    u.easting = easting;
    u.northing = northing;
    return u;
}

QString toMgrs(double lat, double lon)
{
    const Utm u = toUtm(lat, lon);

    int set = u.zone % 6;
    if (set == 0) set = 6;

    static const char *colLetters[] = {"ABCDEFGH", "JKLMNPQR", "STUVWXYZ", "ABCDEFGH", "JKLMNPQR", "STUVWXYZ"};
    int colIdx = int(u.easting / 100000) - 1;
    colIdx = std::clamp(colIdx, 0, 7);
    const QChar colLetter = QLatin1Char(colLetters[set - 1][colIdx]);

    // The row letters cycle every 2000 km; even zones start 5 letters
    // further ("F" instead of "A").  The Python version left the offset
    // out, which put every even-zone reference 500 km off.
    static const char rowLetters[] = "ABCDEFGHJKLMNPQRSTUV";
    const int rowOffset = (u.zone % 2 == 0) ? 5 : 0;
    const int rowIdx = (int(u.northing / 100000) + rowOffset) % 20;
    const QChar rowLetter = QLatin1Char(rowLetters[rowIdx]);

    const int e100k = int(std::fmod(u.easting, 100000.0));
    const int n100k = int(std::fmod(u.northing, 100000.0));

    return QStringLiteral("%1%2 %3%4 %5 %6")
        .arg(u.zone)
        .arg(u.letter)
        .arg(colLetter)
        .arg(rowLetter)
        .arg(e100k, 5, 10, QLatin1Char('0'))
        .arg(n100k, 5, 10, QLatin1Char('0'));
}

QString utmZoneBand(double lat, double lon)
{
    // As the Leaflet popup: the zone from the longitude alone.
    const int zone = int(std::floor((lon + 180) / 6)) + 1;
    static const char letters[] = "CDEFGHJKLMNPQRSTUVWXX";
    const int idx = int(std::floor((lat + 80) / 8));
    const QChar letter = (idx >= 0 && idx < 21) ? QLatin1Char(letters[idx]) : QLatin1Char('?');
    return QStringLiteral("%1%2").arg(zone).arg(letter);
}

QPointF endpoint(double lat, double lon, double azimuthDeg, double distKm)
{
    const double R = kEarthRadiusKm;
    const double lat1 = qDegreesToRadians(lat);
    const double lon1 = qDegreesToRadians(lon);
    const double brng = qDegreesToRadians(azimuthDeg);

    const double lat2 = std::asin(std::sin(lat1) * std::cos(distKm / R)
                                  + std::cos(lat1) * std::sin(distKm / R) * std::cos(brng));
    const double lon2 = lon1 + std::atan2(std::sin(brng) * std::sin(distKm / R) * std::cos(lat1),
                                          std::cos(distKm / R) - std::sin(lat1) * std::sin(lat2));
    return QPointF(qRadiansToDegrees(lat2), qRadiansToDegrees(lon2));
}

double greatCircleKm(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = qDegreesToRadians(lat1), p2 = qDegreesToRadians(lat2);
    const double dp = p2 - p1, dl = qDegreesToRadians(lon2 - lon1);
    const double h = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2 * kEarthRadiusKm * std::asin(std::min(1.0, std::sqrt(h)));
}

double initialBearing(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = qDegreesToRadians(lat1), p2 = qDegreesToRadians(lat2);
    const double dl = qDegreesToRadians(lon2 - lon1);
    const double y = std::sin(dl) * std::cos(p2);
    const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    const double b = qRadiansToDegrees(std::atan2(y, x));
    return b < 0 ? b + 360.0 : b;
}

double distanceKm(double lat1, double lon1, double lat2, double lon2)
{
    const double dlat = (lat2 - lat1) * 111.0;
    const double dlon = (lon2 - lon1) * 111.0 * std::cos(qDegreesToRadians((lat1 + lat2) / 2));
    return std::sqrt(dlat * dlat + dlon * dlon);
}

namespace {

bool lineIntersection(QPointF p1, QPointF p2, QPointF p3, QPointF p4, QPointF *out)
{
    const double x1 = p1.x(), y1 = p1.y();
    const double x2 = p2.x(), y2 = p2.y();
    const double x3 = p3.x(), y3 = p3.y();
    const double x4 = p4.x(), y4 = p4.y();

    const double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::fabs(denom) < 1e-10) return false;

    const double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    *out = QPointF(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
    return true;
}

} // namespace

QList<QPointF> intersections(const QList<Bearing> &bearings, double lengthKm)
{
    QList<QPointF> result;
    for (int i = 0; i < bearings.size(); ++i) {
        for (int j = i + 1; j < bearings.size(); ++j) {
            const Bearing &s1 = bearings.at(i);
            const Bearing &s2 = bearings.at(j);

            const QPointF end1 = endpoint(s1.lat, s1.lon, s1.azimuth, lengthKm);
            const QPointF end2 = endpoint(s2.lat, s2.lon, s2.azimuth, lengthKm);

            QPointF inter;
            if (!lineIntersection(QPointF(s1.lon, s1.lat), QPointF(end1.y(), end1.x()),
                                  QPointF(s2.lon, s2.lat), QPointF(end2.y(), end2.x()), &inter)) {
                continue;
            }
            const double intLon = inter.x();
            const double intLat = inter.y();

            // The intersection must lie ahead of both stations.
            const double dir1 = std::atan2(intLon - s1.lon, intLat - s1.lat);
            const double az1 = qDegreesToRadians(s1.azimuth);
            const double dir2 = std::atan2(intLon - s2.lon, intLat - s2.lat);
            const double az2 = qDegreesToRadians(s2.azimuth);

            double diff1 = std::fabs(dir1 - az1);
            double diff2 = std::fabs(dir2 - az2);
            if (diff1 > M_PI) diff1 = 2 * M_PI - diff1;
            if (diff2 > M_PI) diff2 = 2 * M_PI - diff2;

            if (diff1 < M_PI / 2 && diff2 < M_PI / 2) {
                if (-90 <= intLat && intLat <= 90 && -180 <= intLon && intLon <= 180) {
                    result.append(QPointF(intLat, intLon));
                }
            }
        }
    }
    return result;
}

namespace {

// (lat, lon, radius km) - the Python smallest_enclosing_circle() heuristic.
void enclosingCircle(const QList<QPointF> &points, double *cLat, double *cLon, double *radius)
{
    if (points.isEmpty()) {
        *cLat = *cLon = *radius = 0;
        return;
    }
    if (points.size() == 1) {
        *cLat = points.at(0).x();
        *cLon = points.at(0).y();
        *radius = 0.5;
        return;
    }
    if (points.size() == 2) {
        const QPointF p1 = points.at(0), p2 = points.at(1);
        const double lat = (p1.x() + p2.x()) / 2;
        const double lon = (p1.y() + p2.y()) / 2;
        *cLat = lat;
        *cLon = lon;
        *radius = std::max(distanceKm(lat, lon, p1.x(), p1.y()), 0.5);
        return;
    }

    double maxDist = 0;
    QPointF p1 = points.at(0), p2 = points.at(1);
    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            const double d = distanceKm(points.at(i).x(), points.at(i).y(), points.at(j).x(), points.at(j).y());
            if (d > maxDist) {
                maxDist = d;
                p1 = points.at(i);
                p2 = points.at(j);
            }
        }
    }

    double centerLat = (p1.x() + p2.x()) / 2;
    double centerLon = (p1.y() + p2.y()) / 2;
    double r = maxDist / 2;

    for (const QPointF &p : points) {
        const double d = distanceKm(centerLat, centerLon, p.x(), p.y());
        if (d > r) {
            const double newR = (r + d) / 2;
            const double ratio = d > 0 ? (newR - r) / d : 0;
            centerLat = centerLat + ratio * (p.x() - centerLat);
            centerLon = centerLon + ratio * (p.y() - centerLon);
            r = newR;
        }
    }
    for (const QPointF &p : points) {
        const double d = distanceKm(centerLat, centerLon, p.x(), p.y());
        if (d > r) r = d;
    }

    *cLat = centerLat;
    *cLon = centerLon;
    *radius = std::max(r, 0.5);
}

} // namespace

Circle uncertaintyCircle(const QList<QPointF> &points)
{
    Circle c;
    if (points.isEmpty()) return c;
    enclosingCircle(points, &c.lat, &c.lon, &c.radiusKm);
    c.surfaceKm2 = M_PI * c.radiusKm * c.radiusKm;
    c.valid = true;
    return c;
}

TileXY latLonToTile(double lat, double lon, int zoom)
{
    const double latRad = qDegreesToRadians(lat);
    const double n = std::pow(2.0, zoom);
    TileXY t;
    t.x = int((lon + 180.0) / 360.0 * n);
    t.y = int((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n);
    return t;
}

QList<TileKey> tilesForBounds(double minLat, double minLon, double maxLat, double maxLon,
                              int minZoom, int maxZoom)
{
    QList<TileKey> tiles;
    for (int z = minZoom; z <= maxZoom; ++z) {
        const TileXY a = latLonToTile(minLat, minLon, z); // x_min, y_max
        const TileXY b = latLonToTile(maxLat, maxLon, z); // x_max, y_min
        const int n = 1 << z;
        const int xMin = std::max(0, a.x), xMax = std::min(n - 1, b.x);
        const int yMin = std::max(0, b.y), yMax = std::min(n - 1, a.y);
        for (int x = xMin; x <= xMax; ++x) {
            for (int y = yMin; y <= yMax; ++y) tiles.append({z, x, y});
        }
    }
    return tiles;
}

double lonToWorldX(double lon, double zoom)
{
    return (lon + 180.0) / 360.0 * 256.0 * std::pow(2.0, zoom);
}

double latToWorldY(double lat, double zoom)
{
    const double clamped = std::clamp(lat, -85.05112878, 85.05112878);
    const double r = qDegreesToRadians(clamped);
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / M_PI) / 2.0 * 256.0 * std::pow(2.0, zoom);
}

double worldXToLon(double x, double zoom)
{
    return x / (256.0 * std::pow(2.0, zoom)) * 360.0 - 180.0;
}

double worldYToLat(double y, double zoom)
{
    const double n = M_PI - 2.0 * M_PI * y / (256.0 * std::pow(2.0, zoom));
    return qRadiansToDegrees(std::atan(0.5 * (std::exp(n) - std::exp(-n))));
}

} // namespace sater::geo
