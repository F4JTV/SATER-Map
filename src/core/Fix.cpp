/*
 * Fix.cpp - the least-squares position from the bearings.
 *
 * Gauss-Newton on the sphere: at the current estimate, each station's
 * bearing towards it is compared with the measured one; moving the estimate
 * sideways by x km, as seen from a station at angular distance c, turns that
 * bearing by x / (R sin c) radians.  The weights are 1/sigma^2, so a far
 * station, whose angle error spreads wider on the ground, counts for less
 * in kilometres - Stansfield's estimator.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Fix.h"

#include "Geo.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sater {

namespace {

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kScale95 = 2.4477468306808166;   // sqrt(-2 ln 0.05): 2-D Gaussian, 95 %
constexpr double kScale50 = 1.1774100225154747;   // sqrt(-2 ln 0.5): 50 %
constexpr int kMaxIterations = 60;

// Degrees in (-180, 180].
double wrap180(double a)
{
    a = std::fmod(a + 180.0, 360.0);
    if (a <= 0) a += 360.0;
    return a - 180.0;
}

struct Normal {
    // A = sum w J J^T (symmetric: aa ab / ab bb), g = sum w J r, chi2 = sum w r^2;
    // J per km of (east, north), r in radians.
    double aa = 0, ab = 0, bb = 0, gE = 0, gN = 0, chi2 = 0;
    QList<double> residuals;   // degrees
    QList<double> distances;   // km
    bool ok = true;
};

Normal evaluate(const QList<BearingObservation> &obs, double lat, double lon)
{
    Normal n;
    for (const BearingObservation &o : obs) {
        const double d = geo::greatCircleKm(o.lat, o.lon, lat, lon);
        const double rDeg = wrap180(geo::initialBearing(o.lat, o.lon, lat, lon) - o.azimuth);
        n.residuals.append(rDeg);
        n.distances.append(d);
        const double s = kEarthRadiusKm * std::sin(d / kEarthRadiusKm);
        if (s < 1e-6) {
            // On the station itself: its bearing says nothing there.
            n.ok = false;
            continue;
        }
        // The direction the great circle from the station travels at the
        // estimate; a step to its right turns the station's bearing clockwise.
        const double gamma = qDegreesToRadians(geo::initialBearing(lat, lon, o.lat, o.lon) + 180.0);
        const double jE = std::cos(gamma) / s, jN = -std::sin(gamma) / s;
        const double sigma = qDegreesToRadians(std::max(o.sigma, 1e-3));
        const double w = 1.0 / (sigma * sigma);
        const double r = qDegreesToRadians(rDeg);
        n.aa += w * jE * jE;
        n.ab += w * jE * jN;
        n.bb += w * jN * jN;
        n.gE += w * jE * r;
        n.gN += w * jN * r;
        n.chi2 += w * r * r;
    }
    return n;
}

ConfidenceEllipse ellipseOf(double cEE, double cEN, double cNN, double scale)
{
    // Eigenvalues of the 2x2 covariance, km^2; the major axis's azimuth.
    const double mean = (cEE + cNN) / 2, half = (cEE - cNN) / 2;
    const double root = std::sqrt(half * half + cEN * cEN);
    const double l1 = std::max(0.0, mean + root), l2 = std::max(0.0, mean - root);
    const double fromEast = 0.5 * std::atan2(2 * cEN, cEE - cNN);   // angle of the major axis from east
    double azimuth = 90.0 - qRadiansToDegrees(fromEast);
    azimuth = std::fmod(azimuth, 180.0);
    if (azimuth < 0) azimuth += 180.0;
    ConfidenceEllipse e;
    e.semiMajorKm = scale * std::sqrt(l1);
    e.semiMinorKm = scale * std::sqrt(l2);
    e.orientationDeg = azimuth;
    return e;
}

// A starting point: where the bearing rays (not the lines behind the
// stations) cross, in a plane around the stations - the median of the
// crossings within the bearing length.  (Two nearly parallel bearings cross
// hundreds of kilometres away; with a mean, that one crossing dragged the
// start so far off that a sound bearing seemed to point away from it.)
double median(QList<double> v)
{
    std::sort(v.begin(), v.end());
    const int n = int(v.size());
    return n % 2 ? v.at(n / 2) : (v.at(n / 2 - 1) + v.at(n / 2)) / 2;
}

bool startingPoint(const QList<BearingObservation> &obs, double maxRangeKm, double *lat, double *lon)
{
    double lat0 = 0, lon0 = 0;
    for (const BearingObservation &o : obs) {
        lat0 += o.lat;
        lon0 += o.lon;
    }
    lat0 /= obs.size();
    lon0 /= obs.size();
    const double kmPerDeg = kEarthRadiusKm * M_PI / 180.0;
    const double cosLat = std::cos(qDegreesToRadians(lat0));
    struct Ray { double e, n, ue, un; };
    QList<Ray> rays;
    for (const BearingObservation &o : obs) {
        const double a = qDegreesToRadians(o.azimuth);
        rays.append({(o.lon - lon0) * cosLat * kmPerDeg, (o.lat - lat0) * kmPerDeg, std::sin(a), std::cos(a)});
    }
    QList<double> nearE, nearN, allE, allN;
    for (int i = 0; i < rays.size(); ++i) {
        for (int j = i + 1; j < rays.size(); ++j) {
            const Ray &p = rays.at(i), &q = rays.at(j);
            const double det = p.ue * (-q.un) - p.un * (-q.ue);
            if (std::fabs(det) < 1e-9) continue;
            const double de = q.e - p.e, dn = q.n - p.n;
            const double t = (de * (-q.un) - dn * (-q.ue)) / det;   // km along each ray
            const double u = (p.ue * dn - p.un * de) / det;
            if (t <= 0 || u <= 0) continue;
            const double e = p.e + t * p.ue, n = p.n + t * p.un;
            allE.append(e);
            allN.append(n);
            if (t <= maxRangeKm && u <= maxRangeKm) {
                nearE.append(e);
                nearN.append(n);
            }
        }
    }
    if (allE.isEmpty()) return false;
    // Only far crossings: start there all the same (the solution will be
    // found beyond the bearing length and refused as such).
    const bool near = !nearE.isEmpty();
    *lat = lat0 + median(near ? nearN : allN) / kmPerDeg;
    *lon = lon0 + median(near ? nearE : allE) / (kmPerDeg * cosLat);
    return true;
}

FixResult solve(const QList<BearingObservation> &obs, double maxRangeKm, bool lookForSuspect)
{
    FixResult f;
    if (obs.size() < 2) return f;   // TooFewBearings
    double lat = 0, lon = 0;
    if (!startingPoint(obs, maxRangeKm, &lat, &lon)) {
        f.problem = FixResult::Problem::Parallel;
        return f;
    }

    Normal n = evaluate(obs, lat, lon);
    // A bearing pointing away from where the others cross (a reading taken
    // 180 degrees off, an east/west slip): no least squares can mend it.
    int away = -1;
    for (int i = 0; i < obs.size(); ++i) {
        if (std::fabs(n.residuals.at(i)) > 90.0 && (away < 0 || std::fabs(n.residuals.at(i)) > std::fabs(n.residuals.at(away))))
            away = i;
    }
    if (away >= 0) {
        f.problem = FixResult::Problem::Behind;
        f.problemIndex = away;
        return f;
    }
    for (int it = 0; it < kMaxIterations; ++it) {
        const double det = n.aa * n.bb - n.ab * n.ab;
        const double trace = n.aa + n.bb;
        if (!(det > trace * trace * 1e-12)) {
            f.problem = FixResult::Problem::Parallel;
            return f;
        }
        // delta = -A^-1 g, km east and north.
        double dE = -(n.bb * n.gE - n.ab * n.gN) / det;
        double dN = -(-n.ab * n.gE + n.aa * n.gN) / det;
        double step = std::hypot(dE, dN);
        // Halve the step until the fit improves (far from the solution the
        // linearisation can overshoot).
        bool improved = false;
        double newLat = lat, newLon = lon;
        Normal next;
        for (int halving = 0; halving < 30; ++halving) {
            const QPointF p = geo::endpoint(lat, lon, qRadiansToDegrees(std::atan2(dE, dN)), step);
            next = evaluate(obs, p.x(), p.y());
            if (next.ok && next.chi2 <= n.chi2) {
                newLat = p.x();
                newLon = p.y();
                improved = true;
                break;
            }
            step /= 2;
        }
        if (!improved) break;   // at the minimum, to rounding
        lat = newLat;
        lon = newLon;
        n = next;
        if (step < 1e-6) break; // 1 mm
    }
    if (!n.ok) {
        f.problem = FixResult::Problem::Parallel;
        return f;
    }

    f.lat = lat;
    f.lon = lon;
    f.residuals = n.residuals;
    f.chi2 = n.chi2;
    f.degreesOfFreedom = int(obs.size()) - 2;

    // A bearing pointing away from the solution, or a solution beyond the
    // drawn lines: no fix.
    int worst = -1;
    for (int i = 0; i < obs.size(); ++i) {
        if (std::fabs(n.residuals.at(i)) > 90.0 && (worst < 0 || std::fabs(n.residuals.at(i)) > std::fabs(n.residuals.at(worst))))
            worst = i;
    }
    if (worst >= 0) {
        f.problem = FixResult::Problem::Behind;
        f.problemIndex = worst;
        return f;
    }
    worst = -1;
    for (int i = 0; i < obs.size(); ++i) {
        if (n.distances.at(i) > maxRangeKm && (worst < 0 || n.distances.at(i) > n.distances.at(worst))) worst = i;
    }
    if (worst >= 0) {
        f.problem = FixResult::Problem::OutOfRange;
        f.problemIndex = worst;
        return f;
    }

    // Covariance = A^-1, grown by the disagreement of the bearings when they
    // disagree more than their uncertainties allow (a posteriori variance).
    const double det = n.aa * n.bb - n.ab * n.ab;
    const double cEE = n.bb / det, cNN = n.aa / det, cEN = -n.ab / det;
    double factor = 1.0;
    if (f.degreesOfFreedom > 0) {
        factor = std::max(1.0, f.chi2 / f.degreesOfFreedom);
        f.consistent = f.chi2 <= chiSquare99(f.degreesOfFreedom);
    }
    const double grow = std::sqrt(factor);
    f.ellipse50 = ellipseOf(cEE, cEN, cNN, kScale50 * grow);
    f.ellipse95 = ellipseOf(cEE, cEN, cNN, kScale95 * grow);
    f.valid = true;
    f.problem = FixResult::Problem::None;

    // With three bearings there is one degree of freedom: their normalised
    // deviations all come out the same size, and the largest only points at
    // the bearing that weighs least on the fix.  No suspect is named then.
    if (!f.consistent && lookForSuspect && obs.size() >= 4) {
        // Without which bearing do the others agree best?
        double best = std::numeric_limits<double>::infinity();
        for (int i = 0; i < obs.size(); ++i) {
            QList<BearingObservation> others = obs;
            others.removeAt(i);
            const FixResult without = solve(others, maxRangeKm, false);
            if (!without.valid) continue;
            if (without.chi2 < best) {
                best = without.chi2;
                f.suspect = i;
                f.suspectOffset = wrap180(geo::initialBearing(obs.at(i).lat, obs.at(i).lon, without.lat, without.lon)
                                          - obs.at(i).azimuth);
            }
        }
        f.suspectConfirmed = f.suspect >= 0 && best <= chiSquare99(int(obs.size()) - 3);
        if (f.suspect < 0) {
            // No single bearing explains it: the largest deviation for its
            // uncertainty, not confirmed.
            double most = -1;
            for (int i = 0; i < obs.size(); ++i) {
                const double z = std::fabs(n.residuals.at(i)) / std::max(obs.at(i).sigma, 1e-3);
                if (z > most) {
                    most = z;
                    f.suspect = i;
                    f.suspectOffset = n.residuals.at(i);
                }
            }
        }
    }
    return f;
}

} // namespace

double bearingSigma(double uncertaintyDeg)
{
    return std::max(uncertaintyDeg, 1.0) / 1.959963984540054;
}

double chiSquare99(int dof)
{
    static const double table[] = {6.6349, 9.2103, 11.3449, 13.2767, 15.0863, 16.8119, 18.4753, 20.0902,
                                   21.6660, 23.2093, 24.7250, 26.2170, 27.6882, 29.1412, 30.5779, 31.9999,
                                   33.4087, 34.8053, 36.1909, 37.5662};
    if (dof < 1) return 0.0;
    if (dof <= 20) return table[dof - 1];
    // Wilson-Hilferty.
    const double k = dof, z = 2.3263478740408408;
    const double t = 1.0 - 2.0 / (9.0 * k) + z * std::sqrt(2.0 / (9.0 * k));
    return k * t * t * t;
}

FixResult computeFix(const QList<BearingObservation> &bearings, double maxRangeKm)
{
    return solve(bearings, maxRangeKm, true);
}

QList<QPointF> ellipseOutline(double lat, double lon, const ConfidenceEllipse &e, int points)
{
    QList<QPointF> out;
    const double theta = qDegreesToRadians(e.orientationDeg);
    for (int i = 0; i < points; ++i) {
        const double t = 2 * M_PI * i / points;
        const double along = e.semiMajorKm * std::cos(t), across = e.semiMinorKm * std::sin(t);
        const double east = along * std::sin(theta) + across * std::cos(theta);
        const double north = along * std::cos(theta) - across * std::sin(theta);
        const double d = std::hypot(east, north);
        out.append(d > 0 ? geo::endpoint(lat, lon, qRadiansToDegrees(std::atan2(east, north)), d) : QPointF(lat, lon));
    }
    return out;
}

} // namespace sater
