/*
 * Fix.h - the most likely position of the beacon, from the bearings.
 *
 * The intersection zone of the Python version is the circle around every
 * crossing of two bearing lines: one wrong bearing, or two lines nearly
 * parallel, and it grows to tens of kilometres.  Here the position is the
 * weighted least-squares solution of all the bearings at once (as Stansfield
 * set it out for direction finding): the point from which the bearings, as
 * seen from each station, differ least from those measured, each difference
 * counted against that station's uncertainty.  Its covariance gives the
 * confidence ellipses (50 %, 95 %), and a chi-square test tells whether the
 * bearings agree with one another within their uncertainties - if not,
 * which one disagrees.
 *
 * Bearings are great circles on the sphere of geo::endpoint() (R = 6371 km),
 * the lines drawn on the map.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QList>
#include <QPointF>

#include <cmath>

namespace sater {

struct BearingObservation {
    double lat = 0.0;
    double lon = 0.0;
    double azimuth = 0.0;      // from true north, degrees
    double sigma = 2.5;        // standard deviation of the bearing, degrees
};

// The cone a station draws (+/- uncertainty) taken as the 95 % interval of
// its bearing; at least 1 degree, so that no bearing is taken as exact.
double bearingSigma(double uncertaintyDeg);

struct ConfidenceEllipse {
    double semiMajorKm = 0.0;
    double semiMinorKm = 0.0;
    double orientationDeg = 0.0;   // azimuth of the major axis, [0, 180)
};

struct FixResult {
    enum class Problem {
        None,
        TooFewBearings,    // fewer than two
        Parallel,          // the bearings do not cross
        Behind,            // the bearing of `problemIndex` points away from the solution
        OutOfRange,        // the solution lies beyond the bearing length from `problemIndex`
    };
    bool valid = false;
    Problem problem = Problem::TooFewBearings;
    int problemIndex = -1;

    double lat = 0.0;
    double lon = 0.0;
    ConfidenceEllipse ellipse50;
    ConfidenceEllipse ellipse95;

    // Bearing from each station to the solution minus its measured bearing,
    // degrees in (-180, 180], in the order of the observations.
    QList<double> residuals;
    double chi2 = 0.0;
    int degreesOfFreedom = 0;      // bearings - 2
    // False when the bearings disagree beyond their uncertainties (99 %
    // chi-square test; needs three bearings).  The ellipses then grow by
    // the factor the disagreement calls for.
    bool consistent = true;
    // The bearing that disagrees most (-1: none; always -1 with three
    // bearings, where the test can only say that they disagree).  Confirmed
    // when, without it, the others agree.
    int suspect = -1;
    bool suspectConfirmed = false;
    // How far the suspect bearing is from the others: the bearing from its
    // station to where the others cross (confirmed), or to the solution.
    double suspectOffset = 0.0;
};

// maxRangeKm: the length of the bearing lines; a solution farther than that
// from a station is not on its line as drawn.
FixResult computeFix(const QList<BearingObservation> &bearings, double maxRangeKm);

// The 99 % point of the chi-square distribution with dof degrees of freedom.
double chiSquare99(int dof);

// An ellipse's axis in whole degrees, [0, 180) (179.6 is 0, not 180).
inline int axisDegrees(double orientationDeg)
{
    return int(std::lround(orientationDeg)) % 180;
}

// The outline of an ellipse centred on (lat, lon), as (lat, lon) points.
QList<QPointF> ellipseOutline(double lat, double lon, const ConfidenceEllipse &e, int points = 72);

} // namespace sater
