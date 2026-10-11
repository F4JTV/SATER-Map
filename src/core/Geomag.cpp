/*
 * Geomag.cpp - the World Magnetic Model, as NOAA's reference code
 * (geomag.c) computes it: Schmidt semi-normalised spherical harmonics to
 * degree 12, geodetic to geocentric coordinates and back.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Geomag.h"

#include "Tr.h"

#include <QTimeZone>
#include <QtMath>

#include <array>
#include <cmath>

namespace sater::geomag {

namespace {

constexpr int kMaxOrder = 12;

struct Coefficient {
    int n, m;
    double g, h, dg, dh;   // nT and nT/year
};

// WMM2025 (WMM.COF, released 11/13/2024): public domain.
constexpr Coefficient kWmm2025[] = {
    { 1,  0,  -29351.8,       0.0,   12.0,    0.0},
    { 1,  1,   -1410.8,    4545.4,    9.7,  -21.5},
    { 2,  0,   -2556.6,       0.0,  -11.6,    0.0},
    { 2,  1,    2951.1,   -3133.6,   -5.2,  -27.7},
    { 2,  2,    1649.3,    -815.1,   -8.0,  -12.1},
    { 3,  0,    1361.0,       0.0,   -1.3,    0.0},
    { 3,  1,   -2404.1,     -56.6,   -4.2,    4.0},
    { 3,  2,    1243.8,     237.5,    0.4,   -0.3},
    { 3,  3,     453.6,    -549.5,  -15.6,   -4.1},
    { 4,  0,     895.0,       0.0,   -1.6,    0.0},
    { 4,  1,     799.5,     278.6,   -2.4,   -1.1},
    { 4,  2,      55.7,    -133.9,   -6.0,    4.1},
    { 4,  3,    -281.1,     212.0,    5.6,    1.6},
    { 4,  4,      12.1,    -375.6,   -7.0,   -4.4},
    { 5,  0,    -233.2,       0.0,    0.6,    0.0},
    { 5,  1,     368.9,      45.4,    1.4,   -0.5},
    { 5,  2,     187.2,     220.2,    0.0,    2.2},
    { 5,  3,    -138.7,    -122.9,    0.6,    0.4},
    { 5,  4,    -142.0,      43.0,    2.2,    1.7},
    { 5,  5,      20.9,     106.1,    0.9,    1.9},
    { 6,  0,      64.4,       0.0,   -0.2,    0.0},
    { 6,  1,      63.8,     -18.4,   -0.4,    0.3},
    { 6,  2,      76.9,      16.8,    0.9,   -1.6},
    { 6,  3,    -115.7,      48.8,    1.2,   -0.4},
    { 6,  4,     -40.9,     -59.8,   -0.9,    0.9},
    { 6,  5,      14.9,      10.9,    0.3,    0.7},
    { 6,  6,     -60.7,      72.7,    0.9,    0.9},
    { 7,  0,      79.5,       0.0,   -0.0,    0.0},
    { 7,  1,     -77.0,     -48.9,   -0.1,    0.6},
    { 7,  2,      -8.8,     -14.4,   -0.1,    0.5},
    { 7,  3,      59.3,      -1.0,    0.5,   -0.8},
    { 7,  4,      15.8,      23.4,   -0.1,    0.0},
    { 7,  5,       2.5,      -7.4,   -0.8,   -1.0},
    { 7,  6,     -11.1,     -25.1,   -0.8,    0.6},
    { 7,  7,      14.2,      -2.3,    0.8,   -0.2},
    { 8,  0,      23.2,       0.0,   -0.1,    0.0},
    { 8,  1,      10.8,       7.1,    0.2,   -0.2},
    { 8,  2,     -17.5,     -12.6,    0.0,    0.5},
    { 8,  3,       2.0,      11.4,    0.5,   -0.4},
    { 8,  4,     -21.7,      -9.7,   -0.1,    0.4},
    { 8,  5,      16.9,      12.7,    0.3,   -0.5},
    { 8,  6,      15.0,       0.7,    0.2,   -0.6},
    { 8,  7,     -16.8,      -5.2,   -0.0,    0.3},
    { 8,  8,       0.9,       3.9,    0.2,    0.2},
    { 9,  0,       4.6,       0.0,   -0.0,    0.0},
    { 9,  1,       7.8,     -24.8,   -0.1,   -0.3},
    { 9,  2,       3.0,      12.2,    0.1,    0.3},
    { 9,  3,      -0.2,       8.3,    0.3,   -0.3},
    { 9,  4,      -2.5,      -3.3,   -0.3,    0.3},
    { 9,  5,     -13.1,      -5.2,    0.0,    0.2},
    { 9,  6,       2.4,       7.2,    0.3,   -0.1},
    { 9,  7,       8.6,      -0.6,   -0.1,   -0.2},
    { 9,  8,      -8.7,       0.8,    0.1,    0.4},
    { 9,  9,     -12.9,      10.0,   -0.1,    0.1},
    {10,  0,      -1.3,       0.0,    0.1,    0.0},
    {10,  1,      -6.4,       3.3,    0.0,    0.0},
    {10,  2,       0.2,       0.0,    0.1,   -0.0},
    {10,  3,       2.0,       2.4,    0.1,   -0.2},
    {10,  4,      -1.0,       5.3,   -0.0,    0.1},
    {10,  5,      -0.6,      -9.1,   -0.3,   -0.1},
    {10,  6,      -0.9,       0.4,    0.0,    0.1},
    {10,  7,       1.5,      -4.2,   -0.1,    0.0},
    {10,  8,       0.9,      -3.8,   -0.1,   -0.1},
    {10,  9,      -2.7,       0.9,   -0.0,    0.2},
    {10, 10,      -3.9,      -9.1,   -0.0,   -0.0},
    {11,  0,       2.9,       0.0,    0.0,    0.0},
    {11,  1,      -1.5,       0.0,   -0.0,   -0.0},
    {11,  2,      -2.5,       2.9,    0.0,    0.1},
    {11,  3,       2.4,      -0.6,    0.0,   -0.0},
    {11,  4,      -0.6,       0.2,    0.0,    0.1},
    {11,  5,      -0.1,       0.5,   -0.1,   -0.0},
    {11,  6,      -0.6,      -0.3,    0.0,   -0.0},
    {11,  7,      -0.1,      -1.2,   -0.0,    0.1},
    {11,  8,       1.1,      -1.7,   -0.1,   -0.0},
    {11,  9,      -1.0,      -2.9,   -0.1,    0.0},
    {11, 10,      -0.2,      -1.8,   -0.1,    0.0},
    {11, 11,       2.6,      -2.3,   -0.1,    0.0},
    {12,  0,      -2.0,       0.0,    0.0,    0.0},
    {12,  1,      -0.2,      -1.3,    0.0,   -0.0},
    {12,  2,       0.3,       0.7,   -0.0,    0.0},
    {12,  3,       1.2,       1.0,   -0.0,   -0.1},
    {12,  4,      -1.3,      -1.4,   -0.0,    0.1},
    {12,  5,       0.6,      -0.0,   -0.0,   -0.0},
    {12,  6,       0.6,       0.6,    0.1,   -0.0},
    {12,  7,       0.5,      -0.1,   -0.0,   -0.0},
    {12,  8,      -0.1,       0.8,    0.0,    0.0},
    {12,  9,      -0.4,       0.1,    0.0,   -0.0},
    {12, 10,      -0.2,      -1.0,   -0.1,   -0.0},
    {12, 11,      -1.3,       0.1,   -0.0,    0.0},
    {12, 12,      -0.7,       0.2,   -0.1,   -0.1},
};

using Grid = std::array<std::array<double, kMaxOrder + 1>, kMaxOrder + 1>;

// The coefficients, unnormalised once for all as geomag.c does: c[m][n]
// holds g(n,m), c[n][m-1] holds h(n,m); cd the same for their yearly change.
struct Model {
    Grid c{}, cd{}, k{};
    std::array<double, kMaxOrder + 1> fn{}, fm{};

    Model()
    {
        for (const Coefficient &e : kWmm2025) {
            c[e.m][e.n] = e.g;
            cd[e.m][e.n] = e.dg;
            if (e.m != 0) {
                c[e.n][e.m - 1] = e.h;
                cd[e.n][e.m - 1] = e.dh;
            }
        }
        Grid snorm{};
        snorm[0][0] = 1.0;
        for (int n = 1; n <= kMaxOrder; ++n) {
            snorm[0][n] = snorm[0][n - 1] * double(2 * n - 1) / double(n);
            int j = 2;
            for (int m = 0; m <= n; ++m) {
                k[m][n] = double((n - 1) * (n - 1) - m * m) / double((2 * n - 1) * (2 * n - 3));
                if (m > 0) {
                    const double flnmj = double((n - m + 1) * j) / double(n + m);
                    snorm[m][n] = snorm[m - 1][n] * std::sqrt(flnmj);
                    j = 1;
                    c[n][m - 1] *= snorm[m][n];
                    cd[n][m - 1] *= snorm[m][n];
                }
                c[m][n] *= snorm[m][n];
                cd[m][n] *= snorm[m][n];
            }
            fn[n] = double(n + 1);
            fm[n] = double(n);
        }
        k[1][1] = 0.0;
    }
};

const Model &model()
{
    static const Model m;
    return m;
}

} // namespace

Field compute(double lat, double lon, double altKm, double decimalYear)
{
    const Model &mod = model();
    // WGS 84 and the geomagnetic reference radius, km.
    constexpr double a = 6378.137, b = 6356.7523142, re = 6371.2;
    constexpr double a2 = a * a, b2 = b * b, c2 = a2 - b2, a4 = a2 * a2, b4 = b2 * b2, c4 = a4 - b4;

    const double dt = decimalYear - kModelEpoch;
    const double rlon = qDegreesToRadians(lon), rlat = qDegreesToRadians(lat);
    const double srlon = std::sin(rlon), crlon = std::cos(rlon);
    const double srlat = std::sin(rlat), crlat = std::cos(rlat);
    const double srlat2 = srlat * srlat, crlat2 = crlat * crlat;

    // Geodetic to spherical coordinates.
    const double q = std::sqrt(a2 - c2 * srlat2);
    const double q1 = altKm * q;
    const double q2 = ((q1 + a2) / (q1 + b2)) * ((q1 + a2) / (q1 + b2));
    const double ct = srlat / std::sqrt(q2 * crlat2 + srlat2);
    const double st = std::sqrt(1.0 - ct * ct);
    const double r2 = altKm * altKm + 2.0 * q1 + (a4 - c4 * srlat2) / (q * q);
    const double r = std::sqrt(r2);
    const double d = std::sqrt(a2 * crlat2 + b2 * srlat2);
    const double ca = (altKm + d) / r;
    const double sa = c2 * crlat * srlat / (r * d);

    std::array<double, kMaxOrder + 1> sp{}, cp{}, pp{};
    sp[0] = 0.0;
    cp[0] = 1.0;
    pp[0] = 1.0;
    sp[1] = srlon;
    cp[1] = crlon;
    for (int m = 2; m <= kMaxOrder; ++m) {
        sp[m] = sp[1] * cp[m - 1] + cp[1] * sp[m - 1];
        cp[m] = cp[1] * cp[m - 1] - sp[1] * sp[m - 1];
    }

    Grid p{}, dp{}, tc{};
    p[0][0] = 1.0;
    const double aor = re / r;
    double ar = aor * aor;
    double br = 0.0, bt = 0.0, bp = 0.0, bpp = 0.0;
    for (int n = 1; n <= kMaxOrder; ++n) {
        ar *= aor;
        for (int m = 0; m <= n; ++m) {
            // The associated Legendre functions and their derivatives.
            if (n == m) {
                p[m][n] = st * p[m - 1][n - 1];
                dp[m][n] = st * dp[m - 1][n - 1] + ct * p[m - 1][n - 1];
            } else if (n == 1 && m == 0) {
                p[m][n] = ct * p[m][n - 1];
                dp[m][n] = ct * dp[m][n - 1] - st * p[m][n - 1];
            } else if (n > 1 && n != m) {
                if (m > n - 2) {
                    p[m][n - 2] = 0.0;
                    dp[m][n - 2] = 0.0;
                }
                p[m][n] = ct * p[m][n - 1] - mod.k[m][n] * p[m][n - 2];
                dp[m][n] = ct * dp[m][n - 1] - st * p[m][n - 1] - mod.k[m][n] * dp[m][n - 2];
            }
            // The coefficients at the date.
            tc[m][n] = mod.c[m][n] + dt * mod.cd[m][n];
            if (m != 0) tc[n][m - 1] = mod.c[n][m - 1] + dt * mod.cd[n][m - 1];

            const double par = ar * p[m][n];
            double temp1, temp2;
            if (m == 0) {
                temp1 = tc[m][n] * cp[m];
                temp2 = tc[m][n] * sp[m];
            } else {
                temp1 = tc[m][n] * cp[m] + tc[n][m - 1] * sp[m];
                temp2 = tc[m][n] * sp[m] - tc[n][m - 1] * cp[m];
            }
            bt -= ar * temp1 * dp[m][n];
            bp += mod.fm[m] * temp2 * par;
            br += mod.fn[n] * temp1 * par;
            // At the geographic poles.
            if (st == 0.0 && m == 1) {
                pp[n] = n == 1 ? pp[n - 1] : ct * pp[n - 1] - mod.k[m][n] * pp[n - 2];
                bpp += mod.fm[m] * temp2 * ar * pp[n];
            }
        }
    }
    bp = st == 0.0 ? bpp : bp / st;

    // Back to geodetic: north, east, down.
    const double bx = -bt * ca - br * sa;
    const double by = bp;
    const double bz = bt * sa - br * ca;
    const double bh = std::sqrt(bx * bx + by * by);
    Field f;
    f.declination = qRadiansToDegrees(std::atan2(by, bx));
    f.inclination = qRadiansToDegrees(std::atan2(bz, bh));
    return f;
}

double decimalYear(const QDateTime &when)
{
    const QDateTime utc = when.toUTC();
    const int year = utc.date().year();
    const QDateTime start(QDate(year, 1, 1), QTime(0, 0), QTimeZone::utc());
    const QDateTime end(QDate(year + 1, 1, 1), QTime(0, 0), QTimeZone::utc());
    return year + double(start.msecsTo(utc)) / double(start.msecsTo(end));
}

QString declinationText(double declination)
{
    const QString value = QString::number(std::fabs(declination), 'f', 1);
    return declination >= 0 ? SATER_TR("%1° E").arg(value) : SATER_TR("%1° W").arg(value);
}

} // namespace sater::geomag
