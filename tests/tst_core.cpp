/*
 * tst_core.cpp - geometry, file formats and the mission, checked against
 * the Python version.
 *
 * tests/data/python_vectors.json is produced by gen_python_vectors.py,
 * which runs the functions of the original main.py: DMS, UTM, MGRS,
 * bearing endpoints, intersections and the zone circle of 60 random
 * missions.  The C++ port must give the same numbers - except MGRS in the
 * even UTM zones, where the Python version is wrong and the reference is
 * an independent MGRS library ("mgrs_ref").
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/AprsLink.h"
#include "app/Mission.h"
#include "aprs/AprsIsClient.h"
#include "aprs/Ax25.h"
#include "aprs/DfReport.h"
#include "aprs/Kiss.h"
#include "aprs/KissTcpClient.h"
#include "aprs/ModemSettings.h"
#ifdef SATER_MODEM
#include "aprs/ModemPort.h"
#endif
#include "core/Analysis.h"
#include "core/Exporters.h"
#include "core/Fix.h"
#include "core/Geo.h"
#include "core/Geomag.h"
#include "core/MissionIO.h"
#include "core/ReportWriter.h"
#include "core/TileProviders.h"
#include "i18n/Translator.h"
#include "map/MapView.h"
#include "map/TileManager.h"

#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDiskCache>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimeZone>
#include <QXmlStreamReader>

#if defined(SATER_MODEM) && defined(Q_OS_LINUX)
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#define SATER_MODEM_TEST 1
#endif

#include <random>

using namespace sater;

namespace {

// ---- the least-squares fix: helpers -------------------------------------------

// Normal deviates from a fixed seed, the same on every compiler (the
// standard distributions differ between libraries).
struct Gauss {
    std::mt19937 rng;
    explicit Gauss(unsigned seed) : rng(seed) {}
    double operator()()
    {
        const double u1 = (rng() + 1.0) / 4294967297.0, u2 = (rng() + 1.0) / 4294967297.0;
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2 * M_PI * u2);
    }
};

QList<BearingObservation> bearingsTo(const QList<QPointF> &stations, QPointF target, double sigma)
{
    QList<BearingObservation> out;
    for (const QPointF &st : stations)
        out.append({st.x(), st.y(), geo::initialBearing(st.x(), st.y(), target.x(), target.y()), sigma});
    return out;
}

double chi2At(const QList<BearingObservation> &obs, double lat, double lon)
{
    double c = 0;
    for (const BearingObservation &o : obs) {
        double r = geo::initialBearing(o.lat, o.lon, lat, lon) - o.azimuth;
        r = std::remainder(r, 360.0);
        c += (r / o.sigma) * (r / o.sigma);
    }
    return c;
}

// Is the point inside the ellipse centred on the fix?
bool insideEllipse(const FixResult &f, const ConfidenceEllipse &e, QPointF point)
{
    const double d = geo::greatCircleKm(f.lat, f.lon, point.x(), point.y());
    const double az = qDegreesToRadians(geo::initialBearing(f.lat, f.lon, point.x(), point.y()) - e.orientationDeg);
    const double along = d * std::cos(az), across = d * std::sin(az);
    return (along / e.semiMajorKm) * (along / e.semiMajorKm) + (across / e.semiMinorKm) * (across / e.semiMinorKm) <= 1.0;
}

QJsonObject vectors()
{
    static QJsonObject v = [] {
        QFile f(QStringLiteral(SATER_TEST_DATA "/python_vectors.json"));
        if (!f.open(QIODevice::ReadOnly)) return QJsonObject();
        return QJsonDocument::fromJson(f.readAll()).object();
    }();
    return v;
}

QString stripZoneZero(QString s)
{
    if (s.startsWith(QLatin1Char('0'))) s.remove(0, 1);
    return s;
}

// Same grid zone and square, and the metres within 1: the reference
// library uses a longer UTM series, which can move a value sitting on a
// metre boundary to the next one.
bool sameMgrs(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char(' ')), pb = b.split(QLatin1Char(' '));
    if (pa.size() != 4 || pb.size() != 4) return false;
    if (pa.at(0) != pb.at(0) || pa.at(1) != pb.at(1)) return false;
    return std::abs(pa.at(2).toInt() - pb.at(2).toInt()) <= 1 && std::abs(pa.at(3).toInt() - pb.at(3).toInt()) <= 1;
}

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("SATER_Map_Test"));
        QCoreApplication::setApplicationName(QStringLiteral("SATER_Map_Test"));
        QVERIFY2(!vectors().isEmpty(), "python_vectors.json not found");
    }

    // The declination against pygeomag, an independent port of NOAA's WMM
    // code, on the WMM2025 coefficients (tests/data/gen_wmm_vectors.py).
    void declinationMatchesWmm()
    {
        QFile f(QStringLiteral(SATER_TEST_DATA "/wmm_vectors.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(doc.value(QStringLiteral("model")).toString(), QStringLiteral("WMM-2025"));
        int checked = 0;
        for (const QJsonValue &v : doc.value(QStringLiteral("vectors")).toArray()) {
            const QJsonObject o = v.toObject();
            const geomag::Field g = geomag::compute(o.value(QStringLiteral("lat")).toDouble(), o.value(QStringLiteral("lon")).toDouble(),
                                                    o.value(QStringLiteral("alt_km")).toDouble(), o.value(QStringLiteral("year")).toDouble());
            const double dec = o.value(QStringLiteral("dec")).toDouble(), inc = o.value(QStringLiteral("inc")).toDouble();
            QVERIFY2(std::fabs(g.declination - dec) < 1e-6,
                     qPrintable(QStringLiteral("%1 %2 %3: %4 vs %5").arg(o.value(QStringLiteral("lat")).toDouble())
                                    .arg(o.value(QStringLiteral("lon")).toDouble()).arg(o.value(QStringLiteral("year")).toDouble())
                                    .arg(g.declination, 0, 'f', 8).arg(dec, 0, 'f', 8)));
            QVERIFY(std::fabs(g.inclination - inc) < 1e-6);
            ++checked;
        }
        QVERIFY(checked > 200);
        // Nice, autumn 2026: a little over 3 degrees east.
        const double nice = geomag::declination(43.7102, 7.2620, 2026.78);
        QVERIFY(nice > 3.1 && nice < 3.4);
        QCOMPARE(geomag::decimalYear(QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::utc())), 2026.0);
        QVERIFY(std::fabs(geomag::decimalYear(QDateTime(QDate(2026, 7, 2), QTime(12, 0), QTimeZone::utc())) - 2026.5) < 1e-9);
    }

    // Bearings aimed exactly at a point give that point back, from two
    // stations or five, and agree perfectly.
    void fixFromExactBearings()
    {
        const QPointF target(43.80, 7.25);
        const QList<QPointF> stations = {{43.70, 7.10}, {43.65, 7.35}, {43.90, 7.40}, {43.95, 7.05}, {43.60, 7.20}};
        for (int n : {2, 3, 5}) {
            const FixResult f = computeFix(bearingsTo(stations.mid(0, n), target, bearingSigma(5)), 100);
            QVERIFY(f.valid);
            QCOMPARE(int(f.problem), int(FixResult::Problem::None));
            QVERIFY2(geo::greatCircleKm(f.lat, f.lon, target.x(), target.y()) < 0.001, qPrintable(QString::number(n)));
            QVERIFY(f.chi2 < 1e-9);
            QVERIFY(f.consistent);
            QCOMPARE(f.suspect, -1);
            QCOMPARE(f.degreesOfFreedom, n - 2);
            QVERIFY(f.ellipse95.semiMajorKm >= f.ellipse95.semiMinorKm);
            QVERIFY(f.ellipse95.semiMajorKm > f.ellipse50.semiMajorKm);
        }
    }

    // With noisy bearings the fix is the minimum of the weighted squares:
    // no point a few metres around fits better.
    void fixIsTheLeastSquaresMinimum()
    {
        Gauss g(7);
        const QPointF target(43.80, 7.25);
        const QList<QPointF> stations = {{43.70, 7.10}, {43.65, 7.35}, {43.90, 7.40}, {43.95, 7.05}};
        for (int trial = 0; trial < 50; ++trial) {
            QList<BearingObservation> obs = bearingsTo(stations, target, 2.0);
            for (BearingObservation &o : obs) o.azimuth += 2.0 * g();
            const FixResult f = computeFix(obs, 100);
            QVERIFY(f.valid);
            const double best = chi2At(obs, f.lat, f.lon);
            QVERIFY(std::fabs(best - f.chi2) < 1e-6 * std::max(1.0, best));
            for (int k = 0; k < 8; ++k) {
                const QPointF q = geo::endpoint(f.lat, f.lon, k * 45.0, 0.005);
                QVERIFY2(chi2At(obs, q.x(), q.y()) >= best - 1e-9, qPrintable(QString::number(trial)));
            }
        }
    }

    // The ellipses mean what they say: over many simulated missions the
    // beacon lies inside the 95 % ellipse 95 times out of 100 (two
    // bearings, no disagreement to grow it), and at least that with four.
    void fixEllipseCoverage()
    {
        Gauss g(2025);
        const QPointF target(43.80, 7.25);
        struct Case { QList<QPointF> stations; double lo95, hi95, lo50, hi50; };
        const QList<Case> cases = {
            {{{43.70, 7.10}, {43.66, 7.33}}, 0.93, 0.97, 0.46, 0.54},
            {{{43.70, 7.10}, {43.66, 7.33}, {43.90, 7.40}, {43.95, 7.05}}, 0.94, 1.00, 0.46, 1.00},
        };
        for (const Case &c : cases) {
            const int trials = 2000;
            int in95 = 0, in50 = 0, valid = 0;
            for (int t = 0; t < trials; ++t) {
                QList<BearingObservation> obs = bearingsTo(c.stations, target, 1.5);
                for (BearingObservation &o : obs) o.azimuth += 1.5 * g();
                const FixResult f = computeFix(obs, 100);
                if (!f.valid) continue;
                ++valid;
                in95 += insideEllipse(f, f.ellipse95, target);
                in50 += insideEllipse(f, f.ellipse50, target);
            }
            QVERIFY(valid > trials * 0.99);
            const double r95 = double(in95) / valid, r50 = double(in50) / valid;
            QVERIFY2(r95 >= c.lo95 && r95 <= c.hi95, qPrintable(QStringLiteral("95 %: %1").arg(r95)));
            QVERIFY2(r50 >= c.lo50 && r50 <= c.hi50, qPrintable(QStringLiteral("50 %: %1").arg(r50)));
        }
    }

    // One wrong bearing among five is caught and named; among three the
    // disagreement is caught, but which one is wrong cannot be proven.
    void fixFindsTheWrongBearing()
    {
        const QPointF target(43.80, 7.25);
        const QList<QPointF> stations = {{43.70, 7.10}, {43.65, 7.35}, {43.90, 7.40}, {43.95, 7.05}, {43.60, 7.20}};
        QList<BearingObservation> obs = bearingsTo(stations, target, bearingSigma(5));
        obs[3].azimuth += 14.0;
        FixResult f = computeFix(obs, 100);
        QVERIFY(f.valid);
        QVERIFY(!f.consistent);
        QCOMPARE(f.suspect, 3);
        QVERIFY(f.suspectConfirmed);
        // Without it, the others agree.
        obs.removeAt(3);
        QVERIFY(computeFix(obs, 100).consistent);

        // (Three stations, none in line with another through the beacon:
        // two that face each other across it give one line, and the third
        // bearing alone places the fix along it - its error cannot show.)
        QList<BearingObservation> three = bearingsTo({{43.70, 7.10}, {43.65, 7.35}, {43.95, 7.30}}, target, bearingSigma(5));
        three[2].azimuth -= 16.0;
        f = computeFix(three, 100);
        QVERIFY(f.valid);
        QVERIFY(!f.consistent);
        // Three bearings: one degree of freedom, every normalised deviation
        // the same size - no station is named.
        QCOMPARE(f.suspect, -1);
        QVERIFY(!f.suspectConfirmed);
        // Bearings within their cones agree.
        QList<BearingObservation> fine = bearingsTo(stations, target, bearingSigma(5));
        fine[0].azimuth += 3.0;
        fine[2].azimuth -= 2.0;
        QVERIFY(computeFix(fine, 100).consistent);
    }

    // Two nearly parallel bearings that cross far away do not throw the
    // starting point off: sound bearings give a fix, not "points away".
    // (A case found by the review: the mean of the crossings started 47 km
    // off, and a good bearing was refused.)
    void fixStartsNearTheCrossings()
    {
        const double sigma = 2.551;
        const FixResult f = computeFix({{43.4632, 7.7044, 278.37, sigma}, {43.7483, 7.3722, 181.19, sigma},
                                        {43.3767, 7.5322, 319.40, sigma}, {43.2828, 7.6172, 319.69, sigma}}, 100);
        QVERIFY(f.valid);
        QVERIFY(f.consistent);
        QVERIFY2(geo::greatCircleKm(f.lat, f.lon, 43.5036, 7.3710) < 0.05, qPrintable(QStringLiteral("%1 %2").arg(f.lat).arg(f.lon)));

        // Over many clean random missions (3 to 6 stations, 8 to 40 km from
        // the beacon), almost none is refused.
        Gauss g(4242);
        std::mt19937 rng(99);
        auto uniform = [&](double a, double b) { return a + (b - a) * (rng() / 4294967295.0); };
        int refused = 0, runs = 0;
        for (int n = 3; n <= 6; ++n) {
            for (int t = 0; t < 500; ++t) {
                const QPointF target(43.5 + uniform(-0.2, 0.2), 7.3 + uniform(-0.2, 0.2));
                QList<BearingObservation> obs;
                for (int i = 0; i < n; ++i) {
                    const QPointF st = geo::endpoint(target.x(), target.y(), uniform(0, 360), uniform(8, 40));
                    obs.append({st.x(), st.y(), geo::initialBearing(st.x(), st.y(), target.x(), target.y()) + sigma * g(), sigma});
                }
                ++runs;
                refused += !computeFix(obs, 100).valid;
            }
        }
        QVERIFY2(refused <= runs / 500, qPrintable(QStringLiteral("%1 of %2 refused").arg(refused).arg(runs)));
        QCOMPARE(axisDegrees(179.6), 0);
        QCOMPARE(axisDegrees(179.4), 179);
        QCOMPARE(axisDegrees(0.2), 0);
    }

    // No fix where none makes sense: parallel or diverging bearings, a
    // solution beyond the bearing lines, a bearing pointing away from it.
    void fixRefusesWhatDoesNotCross()
    {
        FixResult f = computeFix({{43.6, 7.0, 90, 2}, {43.7, 7.0, 90, 2}}, 100);
        QVERIFY(!f.valid);
        QCOMPARE(int(f.problem), int(FixResult::Problem::Parallel));
        f = computeFix({{43.7, 7.0, 270, 2}, {43.7, 7.3, 90, 2}}, 100);
        QVERIFY(!f.valid);
        QCOMPARE(int(f.problem), int(FixResult::Problem::Parallel));
        f = computeFix({{43.7, 7.0, 0, 2}}, 100);
        QCOMPARE(int(f.problem), int(FixResult::Problem::TooFewBearings));

        const QPointF far(44.30, 7.25);
        f = computeFix(bearingsTo({{43.70, 7.10}, {43.65, 7.35}}, far, 2), 40);
        QVERIFY(!f.valid);
        QCOMPARE(int(f.problem), int(FixResult::Problem::OutOfRange));
        QCOMPARE(f.problemIndex, 1);
        QVERIFY(computeFix(bearingsTo({{43.70, 7.10}, {43.65, 7.35}}, far, 2), 100).valid);

        const QPointF target(43.80, 7.25);
        QList<BearingObservation> obs = bearingsTo({{43.70, 7.10}, {43.65, 7.35}, {43.90, 7.40}}, target, 2);
        obs[2].azimuth += 180.0;
        f = computeFix(obs, 100);
        QVERIFY(!f.valid);
        QCOMPARE(int(f.problem), int(FixResult::Problem::Behind));
        QCOMPARE(f.problemIndex, 2);

        QVERIFY(std::fabs(chiSquare99(1) - 6.635) < 1e-3);
        QVERIFY(std::fabs(chiSquare99(25) - 44.314) < 0.05);
        QVERIFY(std::fabs(bearingSigma(5) - 2.551) < 1e-3);
        QCOMPARE(bearingSigma(0), bearingSigma(1));
    }

    void dmsMatchesPython()
    {
        int checked = 0;
        for (const QJsonValue &v : vectors().value(QStringLiteral("dms")).toArray()) {
            const QJsonObject o = v.toObject();
            for (const bool isLat : {true, false}) {
                const double dd = o.value(isLat ? QStringLiteral("lat") : QStringLiteral("lon")).toDouble();
                const QJsonArray py = o.value(isLat ? QStringLiteral("lat_dms") : QStringLiteral("lon_dms")).toArray();
                const geo::Dms d = geo::ddToDms(dd, isLat);
                if (py.at(2).toDouble() >= 60.0) continue; // the Python 60" carry bug
                QCOMPARE(d.deg, py.at(0).toInt());
                QCOMPARE(d.min, py.at(1).toInt());
                QVERIFY2(std::fabs(d.sec - py.at(2).toDouble()) < 0.011, qPrintable(QString::number(dd)));
                QCOMPARE(QString(d.dir), py.at(3).toString());
                const QString text = geo::ddToDmsString(dd, isLat);
                QCOMPARE(text, o.value(isLat ? QStringLiteral("lat_str") : QStringLiteral("lon_str")).toString());
                ++checked;
            }
        }
        QVERIFY(checked > 90);
        for (const QJsonValue &v : vectors().value(QStringLiteral("dms_to_dd")).toArray()) {
            const QJsonObject o = v.toObject();
            const double dd = geo::dmsToDd(o.value(QStringLiteral("d")).toInt(), o.value(QStringLiteral("m")).toInt(),
                                           o.value(QStringLiteral("s")).toDouble(), o.value(QStringLiteral("h")).toString().at(0));
            QVERIFY(std::fabs(dd - o.value(QStringLiteral("dd")).toDouble()) < 1e-9);
        }
    }

    void dmsCarry()
    {
        // 43.9999999 gave 43°59'60.0" in Python; here 44°0'0.0".
        QCOMPARE(geo::ddToDmsString(43.9999999, true), QStringLiteral("44°0'0.0\"N"));
        QCOMPARE(geo::ddToDmsString(-7.25, false), QStringLiteral("7°15'0.0\"W"));
    }

    void utmMatchesPython()
    {
        for (const QJsonValue &v : vectors().value(QStringLiteral("utm")).toArray()) {
            const QJsonObject o = v.toObject();
            const double lat = o.value(QStringLiteral("lat")).toDouble(), lon = o.value(QStringLiteral("lon")).toDouble();
            const geo::Utm u = geo::toUtm(lat, lon);
            QCOMPARE(u.zone, o.value(QStringLiteral("zone")).toInt());
            QCOMPARE(QString(u.letter), o.value(QStringLiteral("letter")).toString());
            QVERIFY(std::fabs(u.easting - o.value(QStringLiteral("e")).toDouble()) < 1e-6);
            QVERIFY(std::fabs(u.northing - o.value(QStringLiteral("n")).toDouble()) < 1e-6);
        }
    }

    void mgrsIsRight()
    {
        int even = 0;
        for (const QJsonValue &v : vectors().value(QStringLiteral("utm")).toArray()) {
            const QJsonObject o = v.toObject();
            if (!o.contains(QStringLiteral("mgrs_ref"))) continue;
            const double lat = o.value(QStringLiteral("lat")).toDouble(), lon = o.value(QStringLiteral("lon")).toDouble();
            const QString mine = geo::toMgrs(lat, lon);
            QVERIFY2(sameMgrs(mine, stripZoneZero(o.value(QStringLiteral("mgrs_ref")).toString())),
                     qPrintable(mine + QStringLiteral(" / ") + o.value(QStringLiteral("mgrs_ref")).toString()));
            if (o.value(QStringLiteral("zone")).toInt() % 2 == 1) {
                QCOMPARE(mine, o.value(QStringLiteral("mgrs")).toString()); // Python right in odd zones
            } else {
                ++even;
            }
        }
        QVERIFY(even > 10);
        // Nice: zone 32, where the Python version said "32T LJ".
        QCOMPARE(geo::toMgrs(43.7, 7.25), QStringLiteral("32T LP 58989 40041"));
        QCOMPARE(geo::toMgrs(43.2965, 5.3698), QStringLiteral("31T FH 92230 96468"));
    }

    void endpointsMatchPython()
    {
        for (const QJsonValue &v : vectors().value(QStringLiteral("endpoint")).toArray()) {
            const QJsonObject o = v.toObject();
            const QPointF e = geo::endpoint(o.value(QStringLiteral("lat")).toDouble(), o.value(QStringLiteral("lon")).toDouble(),
                                            o.value(QStringLiteral("az")).toDouble(), o.value(QStringLiteral("dist")).toDouble());
            QVERIFY(std::fabs(e.x() - o.value(QStringLiteral("elat")).toDouble()) < 1e-9);
            QVERIFY(std::fabs(e.y() - o.value(QStringLiteral("elon")).toDouble()) < 1e-9);
        }
    }

    void zonesMatchPython()
    {
        int withCircle = 0;
        for (const QJsonValue &v : vectors().value(QStringLiteral("scenarios")).toArray()) {
            const QJsonObject o = v.toObject();
            QList<Station> stations;
            int i = 0;
            for (const QJsonValue &sv : o.value(QStringLiteral("stations")).toArray()) {
                const QJsonObject so = sv.toObject();
                Station s;
                s.callsign = QStringLiteral("ST%1").arg(i++);
                s.lat = so.value(QStringLiteral("lat")).toDouble();
                s.lon = so.value(QStringLiteral("lon")).toDouble();
                s.azimuth = so.value(QStringLiteral("az")).toDouble();
                stations.append(s);
            }
            const ZoneResult z = computeZone(stations, o.value(QStringLiteral("length")).toDouble());
            const QJsonArray pts = o.value(QStringLiteral("intersections")).toArray();
            QCOMPARE(z.points.size(), pts.size());
            for (int k = 0; k < pts.size(); ++k) {
                QVERIFY(std::fabs(z.points.at(k).x() - pts.at(k).toArray().at(0).toDouble()) < 1e-9);
                QVERIFY(std::fabs(z.points.at(k).y() - pts.at(k).toArray().at(1).toDouble()) < 1e-9);
            }
            const QJsonValue circle = o.value(QStringLiteral("circle"));
            QCOMPARE(z.circle.valid, circle.isArray());
            if (circle.isArray()) {
                const QJsonArray c = circle.toArray();
                QVERIFY(std::fabs(z.circle.lat - c.at(0).toDouble()) < 1e-9);
                QVERIFY(std::fabs(z.circle.lon - c.at(1).toDouble()) < 1e-9);
                QVERIFY(std::fabs(z.circle.radiusKm - c.at(2).toDouble()) < 1e-9);
                QVERIFY(std::fabs(z.circle.surfaceKm2 - c.at(3).toDouble()) < 1e-6);
                QVERIFY(sameMgrs(geo::toMgrs(z.circle.lat, z.circle.lon), stripZoneZero(o.value(QStringLiteral("mgrs_ref")).toString())));
                ++withCircle;
            }
        }
        QVERIFY(withCircle > 50);
    }

    void zoneRules()
    {
        // S0, hidden and nameless stations take no part (Python rules).
        Station a; a.callsign = QStringLiteral("A"); a.lat = 43.6; a.lon = 7.0; a.azimuth = 45;
        Station b; b.callsign = QStringLiteral("B"); b.lat = 43.6; b.lon = 7.3; b.azimuth = 315;
        QVERIFY(computeZone({a, b}, 100).circle.valid);
        Station s0 = b; s0.signal = QStringLiteral("S0");
        QCOMPARE(computeZone({a, s0}, 100).bearingCount, 1);
        Station hidden = b; hidden.visible = false;
        QVERIFY(!computeZone({a, hidden}, 100).circle.valid);
        Station nameless = b; nameless.callsign.clear();
        QVERIFY(!computeZone({a, nameless}, 100).circle.valid);
        // Bearings pointing away from each other do not cross ahead.
        Station c = a; c.azimuth = 225;
        Station d = b; d.azimuth = 135;
        QVERIFY(computeZone({c, d}, 100).points.isEmpty());
    }

    void tilesMatchPython()
    {
        for (const QJsonValue &v : vectors().value(QStringLiteral("tiles")).toArray()) {
            const QJsonObject o = v.toObject();
            const auto t = geo::tilesForBounds(o.value(QStringLiteral("minLat")).toDouble(), o.value(QStringLiteral("minLon")).toDouble(),
                                               o.value(QStringLiteral("maxLat")).toDouble(), o.value(QStringLiteral("maxLon")).toDouble(),
                                               o.value(QStringLiteral("z1")).toInt(), o.value(QStringLiteral("z2")).toInt());
            QCOMPARE(int(t.size()), o.value(QStringLiteral("count")).toInt());
            const QJsonArray first = o.value(QStringLiteral("first")).toArray(), last = o.value(QStringLiteral("last")).toArray();
            QCOMPARE(t.first().z, first.at(0).toInt());
            QCOMPARE(t.first().x, first.at(1).toInt());
            QCOMPARE(t.first().y, first.at(2).toInt());
            QCOMPARE(t.last().z, last.at(0).toInt());
            QCOMPARE(t.last().x, last.at(1).toInt());
            QCOMPARE(t.last().y, last.at(2).toInt());
        }
    }

    void projectionRoundTrip()
    {
        for (double z : {2.0, 6.0, 10.5, 17.0}) {
            for (double lat : {-60.0, 0.0, 43.7, 70.0}) {
                const double y = geo::latToWorldY(lat, z);
                QVERIFY(std::fabs(geo::worldYToLat(y, z) - lat) < 1e-9);
                const double x = geo::lonToWorldX(7.25, z);
                QVERIFY(std::fabs(geo::worldXToLon(x, z) - 7.25) < 1e-9);
            }
        }
    }

    void pythonJsonImports()
    {
        // A file as the Python export_json() writes it.
        const QByteArray py = R"({
  "version": "2.0.0",
  "azimuth_length_km": 150,
  "stations": [
    {"callsign": "F1ABC", "lat": 43.7, "lon": 7.25, "azimuth": 45, "uncertainty": 5.0,
     "color": "#e74c3c", "visible": true, "signal": "S7", "notes": "", "station_id": "1a2b3c4d"},
    {"callsign": "F2XYZ", "lat": 43.65, "lon": 7.45, "azimuth": 300, "uncertainty": 7.5,
     "color": "#3498db", "visible": false, "signal": "S0", "notes": "toit", "station_id": "9f8e7d6c"}
  ],
  "history": [
    {"timestamp": "10/10/2026 18:30:01", "callsign": "F1ABC", "azimuth": 45, "uncertainty": 5.0,
     "lat": 43.7, "lon": 7.25, "signal": "S7"}
  ],
  "kilometers": {"F1ABC": 12.5},
  "beacon": {"lat": 43.71, "lon": 7.3},
  "mission_start": "2026-10-10T18:00:00.123456",
  "mission_duration": "00:30:00",
  "settings": {"grid_color": "#00ff00", "zone_border_color": "#f44336", "zone_fill_color": "#ffeb3b", "zone_opacity": 0.45}
})";
        MissionDocument doc;
        QString err;
        QVERIFY2(io::missionFromJson(py, &doc, &err), qPrintable(err));
        QCOMPARE(doc.azimuthLengthKm, 150.0);
        QCOMPARE(doc.stations.size(), 2);
        QCOMPARE(doc.stations.at(1).signal, QStringLiteral("S0"));
        QCOMPARE(doc.stations.at(1).visible, false);
        QCOMPARE(doc.stations.at(1).id, QStringLiteral("9f8e7d6c"));
        QCOMPARE(doc.history.size(), 1);
        QCOMPARE(doc.kilometers.size(), 1);
        QVERIFY(doc.beacon.has_value());
        QVERIFY(doc.missionStart.isValid());
        QCOMPARE(doc.missionStart.time().msec(), 123);
        QCOMPARE(doc.settings->zoneOpacity, 0.45);

        // And what we write has exactly the keys the Python import takes:
        // Station(**d) and AzimuthRecord(**h) reject any other.
        const QJsonObject out = QJsonDocument::fromJson(io::missionToJson(doc)).object();
        const QSet<QString> stationKeys = {"callsign", "lat", "lon", "azimuth", "uncertainty", "color",
                                           "visible", "signal", "notes", "station_id"};
        for (const QJsonValue &s : out.value(QStringLiteral("stations")).toArray()) {
            const QStringList k = s.toObject().keys();
            QCOMPARE(QSet<QString>(k.begin(), k.end()), stationKeys);
        }
        const QSet<QString> recordKeys = {"timestamp", "callsign", "azimuth", "uncertainty", "lat", "lon", "signal"};
        for (const QJsonValue &h : out.value(QStringLiteral("history")).toArray()) {
            const QStringList k = h.toObject().keys();
            QCOMPARE(QSet<QString>(k.begin(), k.end()), recordKeys);
        }
        for (const char *key : {"version", "azimuth_length_km", "stations", "history", "kilometers", "beacon",
                                "mission_start", "mission_duration", "settings"})
            QVERIFY2(out.contains(QLatin1String(key)), key);
        QCOMPARE(out.value(QStringLiteral("beacon")).toObject().value(QStringLiteral("lon")).toDouble(), 7.3);
    }

    // A bearing read on a compass: the file keeps version 2's keys with the
    // true azimuth (so version 2 draws it in the right place), and the
    // reading with its declination in a block version 2 does not read.
    void magneticBearingsInJson()
    {
        MissionDocument doc;
        doc.version = QStringLiteral("3.0.0");
        Station a;
        a.id = QStringLiteral("aaaa1111");
        a.callsign = QStringLiteral("F4AAA");
        a.lat = 43.70;
        a.lon = 7.10;
        a.azimuth = 358.5;
        a.magnetic = true;
        a.declination = 3.2;
        Station b = a;
        b.id = QStringLiteral("bbbb2222");
        b.callsign = QStringLiteral("F1BBB");
        b.azimuth = 40;
        b.magnetic = false;
        doc.stations = {a, b};
        AzimuthRecord r;
        r.callsign = a.callsign;
        r.azimuth = a.azimuth;
        r.magnetic = true;
        r.declination = a.declination;
        doc.history = {r, AzimuthRecord()};

        const QByteArray json = io::missionToJson(doc);
        const QJsonObject out = QJsonDocument::fromJson(json).object();
        const QJsonArray stations = out.value(QStringLiteral("stations")).toArray();
        QVERIFY(std::fabs(stations.at(0).toObject().value(QStringLiteral("azimuth")).toDouble() - 1.7) < 1e-9);
        QCOMPARE(stations.at(1).toObject().value(QStringLiteral("azimuth")).toDouble(), 40.0);
        QVERIFY(std::fabs(out.value(QStringLiteral("history")).toArray().at(0).toObject().value(QStringLiteral("azimuth")).toDouble() - 1.7) < 1e-9);
        const QSet<QString> stationKeys = {"callsign", "lat", "lon", "azimuth", "uncertainty", "color",
                                           "visible", "signal", "notes", "station_id"};
        for (const QJsonValue &v : stations) {
            const QStringList k = v.toObject().keys();
            QCOMPARE(QSet<QString>(k.begin(), k.end()), stationKeys);
        }
        const QJsonObject ext = out.value(QStringLiteral("sater_map_3")).toObject();
        QCOMPARE(ext.value(QStringLiteral("stations")).toObject().keys(), QStringList{QStringLiteral("aaaa1111")});
        QCOMPARE(ext.value(QStringLiteral("history")).toObject().keys(), QStringList{QStringLiteral("0")});

        MissionDocument back;
        QVERIFY(io::missionFromJson(json, &back, nullptr));
        QCOMPARE(back.stations.size(), 2);
        QVERIFY(back.stations.at(0).magnetic);
        QCOMPARE(back.stations.at(0).azimuth, 358.5);
        QCOMPARE(back.stations.at(0).declination, 3.2);
        QVERIFY(!back.stations.at(1).magnetic);
        QCOMPARE(back.stations.at(1).azimuth, 40.0);
        QVERIFY(back.history.at(0).magnetic);
        QCOMPARE(back.history.at(0).azimuth, 358.5);
        QVERIFY(!back.history.at(1).magnetic);

        // The CSV gives the reading, its north and the true azimuth.
        const QString csv = QString::fromUtf8(io::historyToCsv({r}));
        QVERIFY2(csv.contains(QStringLiteral(";358.5;")), qPrintable(csv));
        QVERIFY2(csv.trimmed().endsWith(QStringLiteral(";1.7")), qPrintable(csv));
    }

    // The mission corrects a magnetic reading by the declination of the
    // station's place on the mission's date, everywhere the bearing is used.
    void magneticBearingInTheMission()
    {
        Mission m;
        m.newMission();
        StationModel *s = m.stations();
        const int a = m.addStationAt(43.70, 7.10), b = m.addStationAt(43.66, 7.33);
        s->setField(a, QStringLiteral("callsign"), QStringLiteral("F4AAA"));
        s->setField(b, QStringLiteral("callsign"), QStringLiteral("F1BBB"));
        s->setField(a, QStringLiteral("azimuth"), 40);
        s->setField(b, QStringLiteral("azimuth"), 330);
        QVERIFY(m.hasFix());
        const double lat0 = m.fixLat(), lon0 = m.fixLon();
        const double dec = s->get(a).value(QStringLiteral("declination")).toDouble();
        QVERIFY2(dec > 2.5 && dec < 4.5, qPrintable(QString::number(dec)));   // Alpes-Maritimes, 2025-2030
        QCOMPARE(dec, m.declinationAt(43.70, 7.10));
        // Read on a compass: the same number now means a bearing turned
        // east by the declination - the estimate moves.
        s->setField(a, QStringLiteral("magnetic"), true);
        QVERIFY(s->get(a).value(QStringLiteral("magnetic")).toBool());
        QVERIFY(std::fabs(s->get(a).value(QStringLiteral("trueAzimuth")).toDouble() - (40 + dec)) < 1e-9);
        QVERIFY(m.hasFix());
        QVERIFY(geo::greatCircleKm(lat0, lon0, m.fixLat(), m.fixLon()) > 0.3);
        // The same as typing the true bearing.
        Mission t;
        t.newMission();
        const int ta = t.addStationAt(43.70, 7.10), tb = t.addStationAt(43.66, 7.33);
        t.stations()->setField(ta, QStringLiteral("callsign"), QStringLiteral("F4AAA"));
        t.stations()->setField(tb, QStringLiteral("callsign"), QStringLiteral("F1BBB"));
        t.stations()->setField(ta, QStringLiteral("azimuth"), 40 + dec);
        t.stations()->setField(tb, QStringLiteral("azimuth"), 330);
        QVERIFY(geo::greatCircleKm(t.fixLat(), t.fixLon(), m.fixLat(), m.fixLon()) < 0.001);
        QVERIFY(std::fabs(t.zoneLat() - m.zoneLat()) < 1e-9);
        // The history keeps the reading and how it was read.
        m.recordStation(a);
        QVERIFY(m.history()->records().constLast().magnetic);
        QCOMPARE(m.history()->records().constLast().azimuth, 40.0);
        // New stations read from magnetic north when asked to.
        m.setMagneticByDefault(true);
        const int c = m.addStation();
        QVERIFY(s->get(c).value(QStringLiteral("magnetic")).toBool());
        m.setMagneticByDefault(false);
        // The mission's date sets the declination (it grows by about 0.1 degree a year).
        MissionDocument doc = m.document();
        doc.missionStart = QDateTime(QDate(2029, 12, 1), QTime(12, 0), QTimeZone::utc());
        m.applyDocument(doc, true);
        const double later = s->get(s->rowOfCallsign(QStringLiteral("F4AAA"))).value(QStringLiteral("declination")).toDouble();
        QVERIFY2(later > dec, qPrintable(QStringLiteral("%1 %2").arg(later).arg(dec)));
        QCOMPARE(later, geomag::declination(43.70, 7.10, geomag::decimalYear(doc.missionStart)));
        // An import (the timer left alone) also takes its own date.
        m.resetTimer();
        doc.missionStart = QDateTime(QDate(2027, 3, 1), QTime(12, 0), QTimeZone::utc());
        m.applyDocument(doc, false);
        QVERIFY(!m.timerStarted());
        QCOMPARE(s->get(s->rowOfCallsign(QStringLiteral("F4AAA"))).value(QStringLiteral("declination")).toDouble(),
                 geomag::declination(43.70, 7.10, geomag::decimalYear(doc.missionStart)));
    }

    void presetsRoundTrip()
    {
        const QByteArray py = R"([{"name": "Mont Agel", "callsign": "F4AAA", "lat": 43.76, "lon": 7.42,
                                   "color": "#2ecc71", "default_uncertainty": 3.0}])";
        bool ok = false;
        const auto list = io::presetsFromJson(py, &ok);
        QVERIFY(ok);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().defaultUncertainty, 3.0);
        const auto again = io::presetsFromJson(io::presetsToJson(list), &ok);
        QCOMPARE(again.first().name, QStringLiteral("Mont Agel"));
    }

    void kmlIsWellFormed()
    {
        ExportContext c;
        Station a; a.callsign = QStringLiteral("F1<A>"); a.lat = 43.6; a.lon = 7.0; a.azimuth = 45;
        Station b; b.callsign = QStringLiteral("B"); b.lat = 43.6; b.lon = 7.3; b.azimuth = 315; b.uncertainty = 0;
        Station s0; s0.callsign = QStringLiteral("C"); s0.lat = 43.5; s0.lon = 7.1; s0.signal = QStringLiteral("S0");
        c.stations = {a, b, s0};
        c.zone = computeZone(c.stations, 100);
        c.beacon = QPair<double, double>(43.7, 7.15);
        const QByteArray kml = exportKml(c);
        QXmlStreamReader xml(kml);
        int placemarks = 0;
        while (!xml.atEnd()) {
            if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("Placemark")) ++placemarks;
        }
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        // 3 stations + 2 lines + 1 cone (b has no uncertainty) + beacon + zone centre
        QCOMPARE(placemarks, 8);
        QVERIFY(kml.contains("ff3c4ce7")); // #e74c3c as aabbggrr
    }

    // The exports draw a compass bearing from true north, say what was
    // read, and carry the estimate with its ellipses.
    void exportsCarryTheEstimate()
    {
        ExportContext c;
        Station a; a.callsign = QStringLiteral("A"); a.lat = 43.70; a.lon = 7.10; a.azimuth = 40;
        a.magnetic = true; a.declination = 3.2;
        Station b; b.callsign = QStringLiteral("B"); b.lat = 43.66; b.lon = 7.33; b.azimuth = 330;
        Station d; d.callsign = QStringLiteral("D"); d.lat = 43.95; d.lon = 7.30; d.azimuth = 200;
        c.stations = {a, b, d};
        c.zone = computeZone(c.stations, 100);
        c.fix = computeStationFix(c.stations, 100);
        QVERIFY(c.fix->valid);
        c.fixNote = QStringLiteral("A note <&>");
        const QByteArray kml = exportKml(c);
        QXmlStreamReader xml(kml);
        int placemarks = 0;
        QString firstLine;
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement) continue;
            if (xml.name() == QLatin1String("Placemark")) ++placemarks;
            if (xml.name() == QLatin1String("coordinates") && firstLine.isEmpty() && placemarks == 2) firstLine = xml.readElementText();
        }
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        // 3 stations + 3 lines + 3 cones + zone centre + estimate + 2 ellipses
        QCOMPARE(placemarks, 13);
        QVERIFY(kml.contains("Read on a compass: 40° magnetic, declination 3.2° E"));
        QVERIFY(kml.contains("A note &lt;&amp;&gt;"));
        // Station A's line ends where its true bearing (43.2°) leads.
        const QPointF end = geo::endpoint(a.lat, a.lon, 43.2, 100);
        QVERIFY2(firstLine.contains(pyNumber(end.y()) + QLatin1Char(',') + pyNumber(end.x())), qPrintable(firstLine));

        const QByteArray html = exportStandaloneHtml(c);
        QVERIFY(html.contains("var estimate={"));
        QVERIFY(html.contains("e95: [["));
        QVERIFY(html.contains("\"azText\":\"43.2° (40°M)\""));
        QVERIFY(html.contains("\"azimuth\":43.2"));

        ReportData r;
        r.stations = c.stations;
        ReportData::Estimate e;
        e.lat = c.fix->lat;
        e.lon = c.fix->lon;
        e.mgrs = geo::toMgrs(e.lat, e.lon);
        e.bearingCount = 3;
        e.agreement = QStringLiteral("agree");
        r.estimate = e;
        const QString report = reportHtml(r);
        QVERIFY(report.contains(QStringLiteral("Best estimate")));
        QVERIFY(report.contains(QStringLiteral("40° M ±5°")));
        QVERIFY(report.contains(QStringLiteral("43.2° true (declination 3.2° E)")));
    }

    void htmlExport()
    {
        ExportContext c;
        Station a; a.callsign = QStringLiteral("A'B"); a.lat = 43.6; a.lon = 7.0; a.azimuth = 45;
        Station b; b.callsign = QStringLiteral("B"); b.lat = 43.6; b.lon = 7.3; b.azimuth = 315;
        c.stations = {a, b};
        c.zone = computeZone(c.stations, 100);
        c.providerKey = QStringLiteral("geoportail-plan");
        const QByteArray html = exportStandaloneHtml(c);
        QVERIFY(html.contains("leaflet@1.9.4"));
        QVERIFY(html.contains("data.geopf.fr"));
        QVERIFY(html.contains("\"callsign\":\"A'B\""));
        QVERIFY(html.contains("var intersection={"));
        // A background with a separate labels layer exports both layers.
        c.providerKey = QStringLiteral("esri-dark");
        const QByteArray dark = exportStandaloneHtml(c);
        QVERIFY(dark.contains("World_Dark_Gray_Base"));
        QVERIFY(dark.contains("World_Dark_Gray_Reference"));
        QVERIFY(!html.contains("World_Dark_Gray_Reference"));
    }

    // No background needs an API key; the CARTO one, which now does, is gone
    // and a map left on it opens on the dark background that replaces it.
    void tileProviderList()
    {
        QStringList keys;
        for (const TileProvider &p : tileProviders()) {
            keys << p.key;
            QVERIFY2(!p.urlTemplate.contains(QLatin1String("cartocdn")), qPrintable(p.key));
            QVERIFY2(!p.urlTemplate.contains(QLatin1String("key=")), qPrintable(p.key));
        }
        QVERIFY(!keys.contains(QStringLiteral("cartodb-dark")));
        QCOMPARE(tileProvider(QStringLiteral("cartodb-dark")).key, QStringLiteral("esri-dark"));
        QCOMPARE(tileProvider(QStringLiteral("nonsense")).key, QStringLiteral("osm"));
        const TileProvider &dark = tileProvider(QStringLiteral("esri-dark"));
        QCOMPARE(tileUrl(dark, 13, 4261, 2992),
                 QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/13/2992/4261"));
        QCOMPARE(labelsUrl(dark, 13, 4261, 2992),
                 QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Reference/MapServer/tile/13/2992/4261"));
        QVERIFY(labelsUrl(tileProvider(QStringLiteral("osm")), 1, 0, 0).isEmpty());
    }

    // The report's pages: every section whole on one page (a heading with
    // its table, the heading with the map), nothing below the text area;
    // only a table taller than a page is cut, between rows.
    void pdfReportPages()
    {
        ReportData d;
        d.title = QStringLiteral("Rapport de mission SATER");
        d.comment = QStringLiteral("Test");
        for (int i = 0; i < 9; ++i) {
            Station s;
            s.callsign = QStringLiteral("F4AA%1").arg(i);
            s.lat = 43.6 + 0.01 * i;
            s.lon = 7.2;
            s.azimuth = 40 + i;
            s.magnetic = i % 2;
            s.declination = 3.2;
            d.stations.append(s);
        }
        d.stationCount = int(d.stations.size());
        for (const Station &s : d.stations) d.kilometers.append({s.callsign, 12.5});
        ReportData::Estimate e;
        e.agreement = QStringLiteral("The bearings agree within their uncertainty.");
        d.estimate = e;
        d.zone = ReportData::Zone{};
        d.beacon = ReportData::Beacon{};
        d.mapImage = QImage(1600, 1100, QImage::Format_RGB32);
        d.mapImage.fill(Qt::darkCyan);
        for (int i = 0; i < 30; ++i) {
            AzimuthRecord r;
            r.timestamp = QStringLiteral("12/10/2026 06:%1:00").arg(i, 2, 10, QLatin1Char('0'));
            r.callsign = QStringLiteral("F4AA%1").arg(i % 9);
            r.azimuth = 100 + i;
            r.magnetic = true;
            r.lat = 43.6;
            r.lon = 7.2;
            d.history.append(r);
        }
        d.logo = QImage(QStringLiteral(":/assets/logo.png"));

        const auto check = [](const QList<ReportPiece> &pieces, double pageHeight, const QString &cut) {
            QMap<QString, QSet<int>> pages;
            int last = 0;
            for (const ReportPiece &p : pieces) {
                QVERIFY2(p.top >= 0 && p.top + p.height <= pageHeight + 0.5,
                         qPrintable(QStringLiteral("%1 on page %2: %3..%4 of %5").arg(p.section).arg(p.page)
                                        .arg(p.top).arg(p.top + p.height).arg(pageHeight)));
                QVERIFY(p.page >= last);
                last = p.page;
                pages[p.section].insert(p.page);
            }
            for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
                if (it.key() == cut) continue;
                QVERIFY2(it.value().size() == 1, qPrintable(it.key() + QStringLiteral(" is cut between pages")));
            }
        };
        double pageHeight = 0;
        QList<ReportPiece> pieces = reportLayout(d, &pageHeight);
        QVERIFY(pageHeight > 900);
        check(pieces, pageHeight, QString());
        QVERIFY(pieces.last().page >= 2);   // three pages at least
        // The map and its heading together.
        int mapPieces = 0;
        for (const ReportPiece &p : pieces) mapPieces += p.section == QStringLiteral("Mission map");
        QCOMPARE(mapPieces, 2);

        // Sixty stations: their table is cut, between rows, every piece
        // inside its page; the other sections stay whole.
        for (int i = 9; i < 60; ++i) {
            Station s = d.stations.first();
            s.callsign = QStringLiteral("F5B%1").arg(i);
            d.stations.append(s);
        }
        pieces = reportLayout(d, &pageHeight);
        check(pieces, pageHeight, QStringLiteral("Direction-finding stations"));
        QSet<int> stationPages;
        for (const ReportPiece &p : pieces) {
            if (p.section == QStringLiteral("Direction-finding stations")) stationPages.insert(p.page);
        }
        QVERIFY(stationPages.size() >= 2);

        // And the PDF itself, with its page count.
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QString err;
        QVERIFY(writeReport(d, &buf, &err));
        QCOMPARE(int(bytes.count("/Type /Page\n")) + int(bytes.count("/Type /Page\r")) + int(bytes.count("/Type /Page ")) > 0, true);
        const QString out = qEnvironmentVariable("SATER_TEST_OUT");
        if (!out.isEmpty()) {
            QFile f(out + QStringLiteral("/report-long.pdf"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        }
    }

    void pdfReport()
    {
        ReportData d;
        d.title = QStringLiteral("Rapport de mission SATER");
        d.subject = QStringLiteral("Radiogoniométrie");
        d.keywords = QStringLiteral("SATER, ADRASEC");
        d.comment = QStringLiteral("Line 1\nLine 2 <b>not bold</b>");
        Station a; a.callsign = QStringLiteral("F1ABC"); a.lat = 43.6; a.lon = 7.0; a.azimuth = 45;
        d.stations = {a, a};
        d.stationCount = 2;
        d.kilometers = {{QStringLiteral("F1ABC"), 12.5}, {QStringLiteral("F2XYZ"), 3}};
        ReportData::Zone z; z.lat = 43.7; z.lon = 7.2; z.radiusKm = 1.2; z.surfaceKm2 = 4.5; z.mgrs = QStringLiteral("32T LP"); z.pointCount = 3;
        d.zone = z;
        for (int i = 0; i < 25; ++i) {
            AzimuthRecord r; r.timestamp = QStringLiteral("10/10/2026 18:%1:00").arg(i, 2, 10, QLatin1Char('0'));
            r.callsign = QStringLiteral("F1ABC"); r.azimuth = i; r.lat = 43.6; r.lon = 7.0;
            d.history.append(r);
        }
        d.mapImage = QImage(1200, 800, QImage::Format_RGB32);
        d.mapImage.fill(Qt::darkCyan);
        d.logo = QImage(QStringLiteral(":/assets/logo.png"));
        QVERIFY(!d.logo.isNull());
        d.organization = QStringLiteral("ADRASEC 06");
        d.appVersion = QStringLiteral("3.0.0");
        const QString html = reportHtml(d);
        QVERIFY(html.contains(QStringLiteral("RAPPORT DE MISSION SATER")));
        QVERIFY(html.contains(QStringLiteral("&lt;b&gt;not bold&lt;/b&gt;")));
        QVERIFY(html.contains(QStringLiteral("... and 5 more records")));

        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QString err;
        QVERIFY(writeReport(d, &buf, &err));
        QVERIFY(bytes.startsWith("%PDF"));
        QVERIFY(bytes.size() > 20000);
        const QString out = qEnvironmentVariable("SATER_TEST_OUT");
        if (!out.isEmpty()) {
            QFile f(out + QStringLiteral("/report.pdf"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        }
    }

    void frenchCatalog()
    {
        CatalogTranslator t;
        QVERIFY(t.size() > 50);
        QCOMPARE(t.translate("SaterMap", "Time;Callsign;Azimuth;Uncertainty;Signal;Lat_DD;Lon_DD;Lat_DMS;Lon_DMS;North;True_azimuth"),
                 QStringLiteral("Heure;Indicatif;Azimut;Incertitude;Signal;Lat_DD;Lon_DD;Lat_DMS;Lon_DMS;Nord;Azimut_geographique"));
        QVERIFY(t.translate("Whatever", "no such string").isEmpty());
    }

    void missionWorkflow()
    {
        QTemporaryDir tmp;
        Mission m;
        m.newMission();
        const int r1 = m.addStation();
        const int r2 = m.addStation();
        auto *st = m.stations();
        QCOMPARE(st->count(), 2);
        QCOMPARE(st->get(1).value(QStringLiteral("stationColor")).toString(), QStringLiteral("#3498db"));
        QVERIFY(!m.hasZone());
        st->setField(r1, QStringLiteral("callsign"), QStringLiteral("f1abc"));
        QCOMPARE(st->get(r1).value(QStringLiteral("callsign")).toString(), QStringLiteral("F1ABC"));
        st->setLatDms(r1, 43, 36, 0, QStringLiteral("N"));
        st->setLonDms(r1, 7, 0, 0, QStringLiteral("E"));
        st->setField(r1, QStringLiteral("azimuth"), 45);
        st->setField(r2, QStringLiteral("callsign"), QStringLiteral("F2XYZ"));
        st->setPosition(r2, 43.6, 7.3);
        QSignalSpy zone(&m, &Mission::zoneChanged);
        st->setField(r2, QStringLiteral("azimuth"), 315);
        QVERIFY(zone.count() >= 1);
        QVERIFY(m.hasZone());
        QCOMPARE(m.zonePointCount(), 1);
        QVERIFY(m.zoneText().startsWith(QStringLiteral("Intersection: ")));
        QVERIFY(m.mgrsText().startsWith(QStringLiteral("MGRS: 32T ")));
        // S0: the line and the zone go away.
        st->setField(r2, QStringLiteral("signal"), QStringLiteral("S0"));
        QVERIFY(!m.hasZone());
        QCOMPARE(m.zoneText(), QStringLiteral("Intersection zone: -"));
        st->setField(r2, QStringLiteral("signal"), QStringLiteral("S9+10"));
        QVERIFY(m.hasZone());

        m.recordStation(r1);
        m.recordStation(r2);
        QCOMPARE(m.history()->count(), 2);
        QCOMPARE(m.history()->data(m.history()->index(0), HistoryModel::CallsignRole).toString(), QStringLiteral("F2XYZ"));
        m.history()->removeRows({0});
        QCOMPARE(m.history()->count(), 1);

        m.setKilometers({{QStringLiteral("F2XYZ"), 3.0}, {QStringLiteral("F1ABC"), 12.54}, {QStringLiteral("NONE"), 0.0}});
        QCOMPARE(m.kilometers().size(), 2);
        QCOMPARE(m.kilometers().first().toMap().value(QStringLiteral("callsign")).toString(), QStringLiteral("F1ABC"));
        QCOMPARE(m.totalKilometers(), 15.5);

        m.setBeacon(43.7, 7.15);
        QVERIFY(m.hasBeacon());
        m.startTimer();
        QVERIFY(m.timerStarted());

        // JSON out and back in.
        const QUrl file = QUrl::fromLocalFile(tmp.filePath(QStringLiteral("mission.json")));
        QVERIFY(m.exportJson(file));
        m.newMission();
        QCOMPARE(st->count(), 0);
        QVERIFY(m.importJson(file));
        QCOMPARE(st->count(), 2);
        QVERIFY(m.hasZone());
        QVERIFY(m.hasBeacon());
        QCOMPARE(m.history()->count(), 1);

        // The other exports.
        QVERIFY(m.exportKml(QUrl::fromLocalFile(tmp.filePath(QStringLiteral("m.kml")))));
        QVERIFY(m.saveHtml(QUrl::fromLocalFile(tmp.filePath(QStringLiteral("m.html")))));
        QVERIFY(m.exportHistoryCsv(QUrl::fromLocalFile(tmp.filePath(QStringLiteral("h.csv")))));
        QVERIFY(m.generatePdf(QUrl::fromLocalFile(tmp.filePath(QStringLiteral("m.pdf"))), QString(), QString(), QString(), QString()));
        QVERIFY(QFileInfo(tmp.filePath(QStringLiteral("m.pdf"))).size() > 10000);

        // Presets.
        m.presets()->removeRows({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
        QVERIFY(m.presets()->save());
        QVERIFY(m.saveStationAsPreset(0, QStringLiteral("Home")));
        m.presets()->reload();
        QCOMPARE(m.presets()->count(), 1);
        m.loadPresets({0});
        QCOMPARE(st->count(), 3);
        QCOMPARE(st->get(2).value(QStringLiteral("azimuth")).toDouble(), 0.0);

        // Autosave and restore in a fresh mission object, a row still
        // without a callsign included, with its compass reading.
        const int blank = m.addStation();
        m.stations()->setField(blank, QStringLiteral("magnetic"), true);
        m.stations()->setField(blank, QStringLiteral("azimuth"), 300);
        m.startTimer();
        m.saveNow();
        Mission again;
        QCOMPARE(again.stations()->count(), 4);
        QVERIFY(again.stations()->get(3).value(QStringLiteral("magnetic")).toBool());
        QCOMPARE(again.stations()->get(3).value(QStringLiteral("azimuth")).toDouble(), 300.0);
        again.stations()->remove(3);
        QVERIFY(again.hasBeacon());
        QVERIFY(again.timerRunning());
        again.newMission();
        again.saveNow();
    }

    void mapRendering()
    {
        Mission m;
        m.newMission();
        const int a = m.addStationAt(43.6, 7.0);
        const int b = m.addStationAt(43.6, 7.3);
        m.stations()->setField(a, QStringLiteral("callsign"), QStringLiteral("A"));
        m.stations()->setField(a, QStringLiteral("azimuth"), 45);
        m.stations()->setField(b, QStringLiteral("callsign"), QStringLiteral("B"));
        m.stations()->setField(b, QStringLiteral("azimuth"), 315);
        m.setOffline(true);  // no network in the test
        MapView view;
        view.setSize(QSizeF(800, 600));
        view.setMission(&m);
        view.centerOn(43.65, 7.15, 11);
        // Projection round trip through the view.
        const QPointF p = view.toScreen(43.6, 7.0);
        const QPointF c = view.toCoordinate(p.x(), p.y());
        QVERIFY(std::fabs(c.x() - 43.6) < 1e-9 && std::fabs(c.y() - 7.0) < 1e-9);
        QCOMPARE(view.toScreen(43.65, 7.15), QPointF(400, 300));
        // The estimate, the zone and its point can be hit (two bearings:
        // the estimate is their crossing point, and comes first).
        const QPointF zc = view.toScreen(m.zoneLat(), m.zoneLon());
        QCOMPARE(view.hitTest(zc.x(), zc.y()).value(QStringLiteral("kind")).toString(), QStringLiteral("estimate"));
        {
            // The estimate's cross is drawn in its colour (magenta by
            // default; blended with its white edge where it straddles pixels).
            const QImage shown = view.renderImage(1.0);
            const QPointF fc = view.toScreen(m.fixLat(), m.fixLon());
            int hits = 0;
            for (int d = 5; d <= 10; ++d) {
                const QColor px = shown.pixelColor(int(std::round(fc.x())) + d, int(std::round(fc.y())));
                hits += px.red() > 150 && px.green() < 160 && px.blue() > 180;
            }
            QVERIFY2(hits >= 4, qPrintable(QString::number(hits)));
        }
        m.setShowEstimate(false);
        QCOMPARE(view.hitTest(zc.x(), zc.y()).value(QStringLiteral("kind")).toString(), QStringLiteral("point"));
        m.setShowEstimate(true);
        // Station A's line runs north-east in its colour.
        const QImage img = view.renderImage(1.0);
        QCOMPARE(img.size(), QSize(800, 600));
        const QPointF mid = view.toScreen(43.6 + 0.02, 7.0 + 0.0277);
        bool red = false;
        for (int dx = -3; dx <= 3 && !red; ++dx) {
            for (int dy = -3; dy <= 3 && !red; ++dy) {
                const QColor px = img.pixelColor(int(mid.x()) + dx, int(mid.y()) + dy);
                red = px.red() > 150 && px.green() < 140 && px.blue() < 140;
            }
        }
        QVERIFY2(red, "no red bearing line where expected");
        const QString out = qEnvironmentVariable("SATER_TEST_OUT");
        if (!out.isEmpty()) img.save(out + QStringLiteral("/map.png"));
        // Zoom keeps the point under the cursor.
        const QPointF before = view.toCoordinate(100, 100);
        view.zoomAround(1, 100, 100, false);
        QCOMPARE(view.zoom(), 12.0);
        const QPointF after = view.toCoordinate(100, 100);
        QVERIFY(std::fabs(before.x() - after.x()) < 1e-9 && std::fabs(before.y() - after.y()) < 1e-9);
        m.newMission();
        m.setOffline(false);
        m.saveNow();
    }
    // SATER_BENCH_TILES=<tiles folder>: time the painting of a phone-sized
    // view (393x851 at 2.75 device pixels per point) with its tiles.
    // MapView::paint() runs on the scene graph's render thread (Windows,
    // Android, most Linux GPUs), with the GUI thread blocked: the tiles it
    // asks for must still be loaded from the GUI thread, where the network
    // manager lives.
    void tilesRequestedFromRenderThread()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        TileManager tiles;
        tiles.setTilesDir(dir.path());
        const QString onDisk = tiles.tilePath(QStringLiteral("osm"), 3, 4, 2);
        QVERIFY(QDir().mkpath(QFileInfo(onDisk).absolutePath()));
        QImage img(256, 256, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        QVERIFY(img.save(onDisk, "PNG"));

        QTest::failOnWarning(QRegularExpression(QStringLiteral("thread|Cannot create children")));
        QSignalSpy arrived(&tiles, &TileManager::tileArrived);
        QThread *render = QThread::create([&tiles]() {
            tiles.beginFrame();
            tiles.tile(QStringLiteral("osm"), 3, 4, 2, true);   // on disk
            tiles.tile(QStringLiteral("osm"), 3, 5, 2, true);   // missing: the (offline) network path
        });
        render->start();
        QVERIFY(render->wait(5000));
        delete render;
        QTRY_VERIFY(!tiles.peek(QStringLiteral("osm"), 3, 4, 2).isNull());
        QVERIFY(arrived.count() >= 1);
        QTest::qWait(100);   // the missing tile's reply finishes
    }

    // Esri Dark Gray comes as two layers, the map and its place names: both
    // are fetched and the names drawn over the map, on screen and in the
    // tiles folder.  The two answers are put in the HTTP cache first, so the
    // test runs without a network.
    void tileWithLabelsLayer()
    {
        const TileProvider &dark = tileProvider(QStringLiteral("esri-dark"));
        const int z = 13, x = 4261, y = 2992;
        QImage base(256, 256, QImage::Format_RGB32);
        base.fill(QColor(30, 30, 30));
        QImage labels(256, 256, QImage::Format_ARGB32);
        labels.fill(Qt::transparent);
        {
            QPainter p(&labels);
            p.fillRect(QRect(100, 100, 50, 20), QColor(240, 240, 240));
        }
        auto png = [](const QImage &img) {
            QByteArray bytes;
            QBuffer b(&bytes);
            b.open(QIODevice::WriteOnly);
            img.save(&b, "PNG");
            return bytes;
        };
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cacheDir.isEmpty()) cacheDir = QDir::tempPath() + QStringLiteral("/SATER_Map");
        {
            QNetworkDiskCache cache;
            cache.setCacheDirectory(cacheDir + QStringLiteral("/http-tiles"));
            const QList<QPair<QString, QByteArray>> answers = {{tileUrl(dark, z, x, y), png(base)},
                                                                {labelsUrl(dark, z, x, y), png(labels)}};
            for (const auto &a : answers) {
                QNetworkCacheMetaData md;
                md.setUrl(QUrl(a.first));
                md.setSaveToDisk(true);
                md.setRawHeaders({{"Content-Type", "image/png"}});
                md.setExpirationDate(QDateTime::currentDateTimeUtc().addDays(1));
                QNetworkCacheMetaData::AttributesMap attributes;
                attributes.insert(QNetworkRequest::HttpStatusCodeAttribute, 200);
                md.setAttributes(attributes);
                QIODevice *dev = cache.prepare(md);
                QVERIFY(dev);
                dev->write(a.second);
                cache.insert(dev);
            }
        }
        QTemporaryDir dir;
        TileManager tiles;
        tiles.setTilesDir(dir.path());
        // On screen (offline: from the cache only).
        tiles.beginFrame();
        QVERIFY(tiles.tile(QStringLiteral("esri-dark"), z, x, y, true).isNull());
        QTRY_VERIFY(!tiles.peek(QStringLiteral("esri-dark"), z, x, y).isNull());
        const QImage shown = tiles.peek(QStringLiteral("esri-dark"), z, x, y);
        QCOMPARE(QColor(shown.pixel(10, 10)), QColor(30, 30, 30));
        QCOMPARE(QColor(shown.pixel(120, 110)), QColor(240, 240, 240));
        // Into the tiles folder: one image holding both.
        QSignalSpy finished(&tiles, &TileManager::downloadFinished);
        tiles.startDownload(QStringLiteral("esri-dark"), {{z, x, y}});
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(tiles.downloadSaved(), 1);
        const QImage stored(tiles.tilePath(QStringLiteral("esri-dark"), z, x, y));
        QVERIFY(!stored.isNull());
        QCOMPARE(QColor(stored.pixel(10, 10)), QColor(30, 30, 30));
        QCOMPARE(QColor(stored.pixel(120, 110)), QColor(240, 240, 240));
    }

    // ---- APRS --------------------------------------------------------------

    // The examples of the APRS 1.0.1 specification (chapter 7) read as DF
    // reports; what SATER Map writes reads back the same.
    void aprsDfReports()
    {
        using namespace sater::aprs;
        const QDateTime now(QDate(2026, 10, 11), QTime(12, 0), QTimeZone::utc());
        auto r = parseDfReport(QStringLiteral("F4AAA-7"), "=4903.50N/07201.75W\\088/036/270/729 no timestamp", now);
        QVERIFY(r);
        QCOMPARE(r->source, QStringLiteral("F4AAA-7"));
        QVERIFY(std::fabs(r->lat - (49 + 3.50 / 60)) < 1e-12);
        QVERIFY(std::fabs(r->lon + (72 + 1.75 / 60)) < 1e-12);
        QCOMPARE(r->bearing, 270.0);
        QCOMPARE(r->hits, 7);
        QCOMPARE(r->rangeCode, 2);
        QCOMPARE(r->quality, 9);
        QCOMPARE(r->uncertainty, 1.0);
        QVERIFY(!r->fromSaterMap);
        QVERIFY(!r->time.isValid());
        r = parseDfReport(QStringLiteral("F1BBB"), "/092345z4903.50N/07201.75W\\000/000/270/729", now);
        QVERIFY(r);
        QCOMPARE(r->time, QDateTime(QDate(2026, 10, 9), QTime(23, 45), QTimeZone::utc()));
        // "DDHHMM/" is the sender's local time, read as this computer's.
        r = parseDfReport(QStringLiteral("F1BBB"), "/111130/4903.50N/07201.75W\\000/000/270/729", now);
        QVERIFY(r);
        QCOMPARE(r->time, QDateTime(QDate(2026, 10, 11), QTime(11, 30)).toUTC());
        // N = 0: the NRQ says nothing, the default spread.
        r = parseDfReport(QStringLiteral("F1BBB"), "!4903.50N/07201.75W\\000/000/270/000", now);
        QVERIFY(r);
        QCOMPARE(r->bearing, 270.0);
        QCOMPARE(r->uncertainty, DfReport().uncertainty);
        // Not DF reports: no bearing field, an omni report (bearing 000),
        // a useless quality, a plain position.
        QVERIFY(!parseDfReport(QStringLiteral("X"), "=4903.50N/07201.75W>088/036 comment", now));
        // A car's course and speed followed by numbers: not the DF symbol.
        QVERIFY(!parseDfReport(QStringLiteral("X"), "!4345.48N/00725.08E>088/036/270/729 Mobile", now));
        QVERIFY(!parseDfReport(QStringLiteral("X"), "!4345.48N\\00725.08E\\088/036/270/729", now));
        QVERIFY(!parseDfReport(QStringLiteral("X"), "!4903.50N/07201.75W\\000/000/000/729", now));
        QVERIFY(!parseDfReport(QStringLiteral("X"), "!4903.50N/07201.75W\\000/000/270/720", now));
        QVERIFY(!parseDfReport(QStringLiteral("X"), "!4903.50N/07201.75W-Home", now));
        QVERIFY(!parseDfReport(QStringLiteral("X"), ":F4AAA    :hello{1", now));
        // 360 is north.
        QCOMPARE(parseDfReport(QStringLiteral("X"), "!4903.50N/07201.75W\\000/000/360/965", now)->bearing, 0.0);

        // Written by SATER Map: the DF symbol, true bearing, N = 9 (manual),
        // Q from the cone, R from the bearing length, the comment.
        DfReport out;
        out.lat = 43.7580;
        out.lon = 7.4180;
        out.bearing = 251.4;
        out.uncertainty = 5.0;
        out.signal = QStringLiteral("S9+10");
        out.time = QDateTime(QDate(2026, 10, 11), QTime(10, 23, 41), QTimeZone::utc());
        const QByteArray info = encodeDfReport(out, 100);
        // R = 6: 100 km is 62 miles, under 2^6.
        QCOMPARE(info, QByteArray("/111023z4345.48N/00725.08E\\000/000/251/965 SATER S9+10 U5.0"));
        r = parseDfReport(QStringLiteral("F4AAA"), info, now);
        QVERIFY(r);
        QVERIFY(r->fromSaterMap);
        QCOMPARE(r->signal, QStringLiteral("S9+10"));
        QCOMPARE(r->uncertainty, 5.0);
        QCOMPARE(r->bearing, 251.0);
        QVERIFY(std::fabs(r->lat - 43.7580) < 0.01 / 60 && std::fabs(r->lon - 7.4180) < 0.01 / 60);
        QCOMPARE(r->time, QDateTime(QDate(2026, 10, 11), QTime(10, 23), QTimeZone::utc()));
        // North written 360, south and west hemispheres, minutes rounding up.
        out.bearing = 359.7;
        out.lat = -33.99999;
        out.lon = -0.5;
        const QByteArray south = encodeDfReport(out, 10);
        QVERIFY2(south.contains("3400.00S/00030.00W\\000/000/360/9"), south.constData());
        QCOMPARE(qualityFromUncertainty(0.2), 9);
        QCOMPARE(qualityFromUncertainty(5), 5);
        QCOMPARE(qualityFromUncertainty(8), 4);
        QCOMPARE(qualityFromUncertainty(30), 3);   // 60° wide, under 64
        QCOMPARE(qualityFromUncertainty(40), 2);
        QCOMPARE(qualityFromUncertainty(200), 0);
    }

    // AX.25 frames and KISS: a frame built here decodes back to the same
    // packet, in pieces as a TCP stream delivers it, escapes included; the
    // text form is APRS-IS's.
    void aprsFramesAndKiss()
    {
        using namespace sater::aprs;
        Packet p;
        p.source = QStringLiteral("F4AAA-7");
        p.destination = QStringLiteral("APZSAT");
        p.path = {QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-1")};
        p.info = QByteArray("/111023z4345.48N/00725.08E\\000/000/251/975 SATER S7 U5.0") + char(0xC0) + char(0xDB);
        const QByteArray frame = encodeFrame(p);
        QVERIFY(!frame.isEmpty());
        QCOMPARE(frame.size(), 4 * 7 + 2 + p.info.size());
        QCOMPARE(uchar(frame.at(13)) & 0x01, 0);   // source not last: digipeaters follow
        QCOMPARE(uchar(frame.at(27)) & 0x01, 1);
        const QByteArray kiss = kissEncode(frame);
        QCOMPARE(kiss.count(char(0xC0)), 2);       // the data's C0 is escaped
        KissDecoder dec;
        QList<QByteArray> frames;
        for (int i = 0; i < kiss.size(); i += 5) frames += dec.feed(kiss.mid(i, 5));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.first(), frame);
        auto back = decodeFrame(frames.first());
        QVERIFY(back);
        QCOMPARE(back->source, p.source);
        QCOMPARE(back->destination, p.destination);
        QCOMPARE(back->path, p.path);
        QCOMPARE(back->info, p.info);
        // A repeated frame: the last digipeater that handled it is starred.
        p.path = {QStringLiteral("F1ZZZ-3*"), QStringLiteral("WIDE2-1")};
        back = decodeFrame(encodeFrame(p));
        QVERIFY(back->toTnc2().startsWith(QStringLiteral("F4AAA-7>APZSAT,F1ZZZ-3*,WIDE2-1:/111023z")));
        // Text form, and a third-party packet relayed by a gateway.
        const auto line = parseTnc2("F5GW>APDW17,TCPIP*,qAC,T2FRA:}F4AAA-7>APZSAT,TCPIP,F5GW*:!4903.50N/07201.75W\\000/000/270/965\r\n");
        QVERIFY(line);
        QCOMPARE(line->source, QStringLiteral("F5GW"));
        const auto df = parseDfReport(*line);
        QVERIFY(df);
        QCOMPARE(df->source, QStringLiteral("F4AAA-7"));
        // Invalid addresses and non-UI frames are refused.
        Packet bad = p;
        bad.source = QStringLiteral("TOOLONGCALL");
        QVERIFY(encodeFrame(bad).isEmpty());
        bad.source = QStringLiteral("F4AAA-16");
        QVERIFY(encodeFrame(bad).isEmpty());
        QByteArray notUi = frame;
        notUi[28] = char(0x13);
        QVERIFY(!decodeFrame(notUi));
        QVERIFY(!decodeFrame(QByteArray(10, 'x')));
        QCOMPARE(normalizeCallsign(QStringLiteral(" f4aaa-0 ")), QStringLiteral("F4AAA"));
        QVERIFY(isValidCallsign(QStringLiteral("F4AAA-15")));
        QVERIFY(!isValidCallsign(QStringLiteral("F4 AAA")));
        // KISS commands other than data are dropped; garbage between frames too.
        QByteArray stream = QByteArray("junk") + char(0xC0) + char(0x01) + "x" + char(0xC0) + kissEncode(frame, 1);
        QList<int> ports;
        frames = KissDecoder().feed(stream, &ports);
        QCOMPARE(frames.size(), 1);
        QCOMPARE(ports, QList<int>{1});
    }

    // APRS-IS against a local server: the login with the filter, the
    // server's answer, a packet in, a packet out, the filter changed.
    void aprsIsClient()
    {
        using namespace sater::aprs;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        AprsIsClient client;
        client.setSoftware(QStringLiteral("SATER_Map"), QStringLiteral("3.0.0"));
        client.setServer(QStringLiteral("127.0.0.1"), server.serverPort());
        client.setLogin(QStringLiteral("f4jtv-9"), 12345);
        client.setFilterCallsigns({QStringLiteral("F4AAA"), QStringLiteral("F1BBB-7")});
        QSignalSpy received(&client, &AprsPort::packetReceived);
        client.setEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections() || server.waitForNewConnection(50));
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        QTRY_VERIFY(peer->canReadLine() || peer->waitForReadyRead(50));
        QCOMPARE(peer->readLine(), QByteArray("user F4JTV-9 pass 12345 vers SATER_Map 3.0.0 filter b/F4AAA*/F1BBB-7\r\n"));
        QVERIFY(!client.canSend());   // not before the server says so
        peer->write("# aprsc 2.1\r\n# logresp F4JTV-9 verified, server T2TEST\r\n");
        QTRY_VERIFY(client.canSend());
        QVERIFY(client.detail().contains(QStringLiteral("T2TEST")));
        peer->write("F4AAA-7>APZSAT,TCPIP*,qAC,T2TEST:/111023z4345.48N/00725.08E\\000/000/251/965 SATER S7 U5.0\r\n");
        QTRY_COMPARE(received.count(), 1);
        const auto in = received.first().first().value<Packet>();
        QCOMPARE(in.source, QStringLiteral("F4AAA-7"));
        QVERIFY(parseDfReport(in));
        Packet out;
        out.source = QStringLiteral("F4JTV-9");
        out.destination = QStringLiteral("APZSAT");
        out.path = {QStringLiteral("WIDE1-1")};   // replaced by TCPIP* on APRS-IS
        out.info = "/111023z4345.48N/00725.08E\\000/000/251/965";
        QVERIFY(client.send(out));
        QTRY_VERIFY(peer->canReadLine() || peer->waitForReadyRead(50));
        QCOMPARE(peer->readLine(), QByteArray("F4JTV-9>APZSAT,TCPIP*:/111023z4345.48N/00725.08E\\000/000/251/965\r\n"));
        client.setFilterCallsigns({QStringLiteral("F4AAA"), QStringLiteral("F5CCC")});
        QTRY_VERIFY(peer->canReadLine() || peer->waitForReadyRead(50));
        QCOMPARE(peer->readLine(), QByteArray("#filter b/F4AAA*/F5CCC*\r\n"));
        // Receive only (no passcode): logged in, but nothing goes out.
        peer->write("# logresp F4JTV-9 unverified, server T2TEST\r\n");
        QTRY_VERIFY(!client.canSend());
        QVERIFY(!client.send(out));
        peer->close();
        QTRY_COMPARE(int(client.state()), int(AprsPort::State::Off));
        client.setEnabled(false);
    }

    // A KISS TNC on TCP (Dire Wolf's port 8001): frames in, in pieces;
    // frames out with the radio path.
    void kissTcpClient()
    {
        using namespace sater::aprs;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        KissTcpClient client;
        client.setServer(QStringLiteral("127.0.0.1"), server.serverPort());
        client.setPath({QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-2")});
        QSignalSpy received(&client, &AprsPort::packetReceived);
        client.setEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections() || server.waitForNewConnection(50));
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        QTRY_VERIFY(client.canSend());
        Packet in;
        in.source = QStringLiteral("F1BBB");
        in.destination = QStringLiteral("APZSAT");
        in.path = {QStringLiteral("F5DIG*"), QStringLiteral("WIDE2-1")};
        in.info = "!4339.72N/00709.00E\\000/000/051/965 SATER S9 U4.0";
        const QByteArray bytes = kissEncode(encodeFrame(in));
        peer->write(bytes.left(9));
        peer->flush();
        QTest::qWait(30);
        peer->write(bytes.mid(9));
        QTRY_COMPARE(received.count(), 1);
        const auto got = received.first().first().value<Packet>();
        QCOMPARE(got.toTnc2(), in.toTnc2());
        Packet out = in;
        out.source = QStringLiteral("F4JTV-9");
        QVERIFY(client.send(out));
        QByteArray sent;
        QTRY_VERIFY((sent += peer->readAll()).count(char(0xC0)) >= 2);
        const auto frames = KissDecoder().feed(sent);
        QCOMPARE(frames.size(), 1);
        const auto decoded = decodeFrame(frames.first());
        QVERIFY(decoded);
        QCOMPARE(decoded->path, (QStringList{QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-2")}));
        QCOMPARE(decoded->source, QStringLiteral("F4JTV-9"));
        client.setEnabled(false);
        QCOMPARE(int(client.state()), int(AprsPort::State::Off));
    }

    // The operation's team: a report from one of its callsigns moves that
    // station and gives it the bearing; others are left out; the same
    // report heard twice (RF and APRS-IS) counts once.  This station's
    // bearing goes out as a DF report.
    void aprsTeamBearings()
    {
        using namespace sater::aprs;
        Mission m;
        m.newMission();
        AprsLink *aprs = m.aprs();
        QVERIFY(aprs);
        m.setTeam({QStringLiteral("f4aaa"), QStringLiteral("F1BBB-7"), QStringLiteral("not a call"), QStringLiteral("F4AAA")});
        QCOMPARE(m.team(), (QStringList{QStringLiteral("F4AAA"), QStringLiteral("F1BBB-7")}));
        QCOMPARE(m.teamEntryFor(QStringLiteral("F4AAA-9")), QStringLiteral("F4AAA"));
        QCOMPARE(m.teamEntryFor(QStringLiteral("F1BBB")), QString());
        QCOMPARE(m.teamEntryFor(QStringLiteral("F1BBB-7")), QStringLiteral("F1BBB-7"));
        QCOMPARE(aprs->aprsIs()->filter(), QStringLiteral("b/F4AAA*/F1BBB-7"));

        auto packet = [](const char *line) { return *parseTnc2(line); };
        QSignalSpy received(&m, &Mission::bearingReceived);
        aprs->handlePacket(packet("F4AAA-9>APZSAT,WIDE1-1:!4345.48N/00725.08E\\000/000/251/965 SATER S7 U5.0"), QStringLiteral("RF"));
        QCOMPARE(received.count(), 1);
        // The sender's own row (F4AAA lists every SSID of the callsign).
        QCOMPARE(m.stations()->rowOfCallsign(QStringLiteral("F4AAA")), -1);
        int row = m.stations()->rowOfCallsign(QStringLiteral("F4AAA-9"));
        QVERIFY(row >= 0);
        QVariantMap st = m.stations()->get(row);
        QCOMPARE(st.value(QStringLiteral("azimuth")).toDouble(), 251.0);
        QCOMPARE(st.value(QStringLiteral("uncertainty")).toDouble(), 5.0);
        QCOMPARE(st.value(QStringLiteral("signal")).toString(), QStringLiteral("S7"));
        QVERIFY(!st.value(QStringLiteral("magnetic")).toBool());
        QVERIFY(std::fabs(st.value(QStringLiteral("lat")).toDouble() - (43 + 45.48 / 60)) < 1e-9);
        QCOMPARE(m.history()->count(), 1);
        // The same report by APRS-IS: already in.
        aprs->handlePacket(packet("F4AAA-9>APZSAT,TCPIP*,qAC,T2X:!4345.48N/00725.08E\\000/000/251/965 SATER S7 U5.0"), QStringLiteral("APRS-IS"));
        QCOMPARE(m.history()->count(), 1);
        // Not in the operation, or not a bearing: nothing changes.
        aprs->handlePacket(packet("F9XYZ>APZSAT:!4345.48N/00725.08E\\000/000/100/965"), QStringLiteral("RF"));
        aprs->handlePacket(packet("F4AAA-9>APDR15:=4345.48N/00725.08E>Mobile"), QStringLiteral("RF"));
        QCOMPARE(m.stations()->count(), 1);
        QCOMPARE(m.history()->count(), 1);
        // A second team member (another program: no SATER fields, Q gives
        // the cone); with two bearings the estimate appears.
        aprs->handlePacket(packet("F1BBB-7>APX219:/111020z4339.72N/00709.00E\\000/000/051/925"), QStringLiteral("RF"));
        row = m.stations()->rowOfCallsign(QStringLiteral("F1BBB-7"));
        QVERIFY(row >= 0);
        QCOMPARE(m.stations()->get(row).value(QStringLiteral("uncertainty")).toDouble(), 8.0);   // Q = 5: under 16° wide
        QCOMPARE(aprs->bearingsReceived(), 2);
        QVERIFY(m.hasFix());
        QCOMPARE(aprs->log().size(), 5);
        QCOMPARE(aprs->log().first().toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("bearing"));
        // Two SSIDs of one callsign are two teams: two rows.  A row made
        // beforehand without SSID becomes the first one heard.
        m.setTeam(m.team() << QStringLiteral("F5CCC"));
        const int made = m.stations()->addAt(43.6, 7.2);
        m.stations()->setField(made, QStringLiteral("callsign"), QStringLiteral("F5CCC"));
        const int countBefore = m.stations()->count();
        aprs->handlePacket(packet("F5CCC-1>APZSAT:!4345.48N/00725.08E\\000/000/251/965 SATER S7 U5.0"), QStringLiteral("RF"));
        aprs->handlePacket(packet("F5CCC-2>APZSAT:!4340.00N/00710.00E\\000/000/120/965 SATER S5 U5.0"), QStringLiteral("RF"));
        QCOMPARE(m.stations()->count(), countBefore + 1);
        QCOMPARE(m.stations()->rowOfCallsign(QStringLiteral("F5CCC-1")), made);
        QCOMPARE(m.stations()->get(made).value(QStringLiteral("azimuth")).toDouble(), 251.0);
        row = m.stations()->rowOfCallsign(QStringLiteral("F5CCC-2"));
        QVERIFY(row >= 0 && row != made);
        QCOMPARE(m.stations()->get(row).value(QStringLiteral("azimuth")).toDouble(), 120.0);
        QCOMPARE(m.stations()->rowOfCallsign(QStringLiteral("F5CCC")), -1);

        // Sending: this station's row, its callsign, a KISS TNC on a local port.
        const int own = m.stations()->addAt(43.70, 7.25);
        m.stations()->setField(own, QStringLiteral("callsign"), QStringLiteral("F4AAA"));
        m.stations()->setField(own, QStringLiteral("azimuth"), 75.0);
        m.stations()->setField(own, QStringLiteral("signal"), QStringLiteral("S7"));
        QVERIFY(!aprs->sendBearing(m.stations()->rowOfCallsign(QStringLiteral("F4AAA"))));   // no callsign yet
        aprs->setMyCallsign(QStringLiteral("f4aaa-7"));
        QVERIFY(aprs->isMine(QStringLiteral("F4AAA")));
        QVERIFY(aprs->isMine(QStringLiteral("F4AAA-7")));
        QVERIFY(!aprs->isMine(QStringLiteral("F4AAA-9")));
        QVERIFY(!aprs->isMine(QStringLiteral("F1BBB-7")));
        QVERIFY(!aprs->sendBearing(m.stations()->rowOfCallsign(QStringLiteral("F4AAA"))));   // no connection
        QTcpServer tnc;
        QVERIFY(tnc.listen(QHostAddress::LocalHost));
        aprs->setKissHost(QStringLiteral("127.0.0.1"));
        aprs->setKissPort(tnc.serverPort());
        aprs->setRfPath(QStringLiteral("wide1-1, wide2-1"));
        aprs->setKissEnabled(true);
        QTRY_VERIFY(tnc.hasPendingConnections() || tnc.waitForNewConnection(50));
        QTcpSocket *peer = tnc.nextPendingConnection();
        QTRY_VERIFY(aprs->canSend());
        const int mine = m.stations()->rowOfCallsign(QStringLiteral("F4AAA"));
        m.stations()->setField(mine, QStringLiteral("magnetic"), true);   // read on a compass: sent from true north
        const double trueAz = m.stations()->get(mine).value(QStringLiteral("trueAzimuth")).toDouble();
        const int historyBefore = m.history()->count();
        QVERIFY(aprs->sendBearing(mine));
        QCOMPARE(m.history()->count(), historyBefore + 1);
        QByteArray bytes;
        QTRY_VERIFY((bytes += peer->readAll()).count(char(0xC0)) >= 2);
        const auto sent = decodeFrame(KissDecoder().feed(bytes).first());
        QVERIFY(sent);
        QCOMPARE(sent->source, QStringLiteral("F4AAA-7"));
        QCOMPARE(sent->destination, QStringLiteral("APZSAT"));
        QCOMPARE(sent->path, (QStringList{QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-1")}));
        const auto df = parseDfReport(*sent);
        QVERIFY(df);
        QCOMPARE(df->bearing, double(qRound(trueAz)));
        QCOMPARE(df->signal, QStringLiteral("S7"));
        QCOMPARE(aprs->log().first().toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("sent"));
        // Our own report back from a digipeater (we are in the team: F4AAA):
        // this station keeps its compass reading, the history its one line.
        Packet echoed = *sent;
        echoed.path = {QStringLiteral("F5DIG-3*"), QStringLiteral("WIDE2-1")};
        aprs->handlePacket(echoed, QStringLiteral("RF"));
        QCOMPARE(m.history()->count(), historyBefore + 1);
        QVERIFY(m.stations()->get(mine).value(QStringLiteral("magnetic")).toBool());
        QVERIFY(aprs->log().first().toMap().value(QStringLiteral("text")).toString().contains(QStringLiteral("our own")));
        // F4AAA without SSID is another station than this one (F4AAA-7):
        // its report is taken in, on a row of its own - this station's
        // row, named F4AAA, keeps its compass reading, and is not taken
        // over by F4AAA-9 either.
        const int rowsBefore = m.stations()->count();
        aprs->handlePacket(packet("F4AAA>APZSAT:!4341.00N/00715.00E\\000/000/030/965 SATER S6 U4.0"), QStringLiteral("RF"));
        QCOMPARE(m.history()->count(), historyBefore + 2);
        QCOMPARE(m.stations()->count(), rowsBefore + 1);
        QCOMPARE(m.stations()->get(rowsBefore).value(QStringLiteral("azimuth")).toDouble(), 30.0);
        aprs->handlePacket(packet("F4AAA-9>APZSAT:!4342.00N/00716.00E\\000/000/040/965 SATER S6 U4.0"), QStringLiteral("RF"));
        QCOMPARE(m.stations()->get(mine).value(QStringLiteral("callsign")).toString(), QStringLiteral("F4AAA"));
        QVERIFY(m.stations()->get(mine).value(QStringLiteral("magnetic")).toBool());
        QCOMPARE(m.stations()->count(), rowsBefore + 1);   // F4AAA-9 had its row already
        QCOMPARE(m.stations()->get(m.stations()->rowOfCallsign(QStringLiteral("F4AAA-9"))).value(QStringLiteral("azimuth")).toDouble(), 40.0);
        aprs->setKissEnabled(false);
        aprs->setMyCallsign(QString());

        // The team list is saved with the mission (in version 3's block).
        const QJsonObject json = QJsonDocument::fromJson(io::missionToJson(m.document())).object();
        QCOMPARE(json.value(QStringLiteral("sater_map_3")).toObject().value(QStringLiteral("team")).toArray().size(), 3);
        MissionDocument back;
        QVERIFY(io::missionFromJson(io::missionToJson(m.document()), &back, nullptr));
        QCOMPARE(back.team, m.team());
        // A new mission is a new operation: no callsign carried over.
        m.newMission();
        QVERIFY(m.team().isEmpty());
    }

    // What the review of the APRS code found: a server that refuses the
    // login says why, and is not tried again at once; a line longer than
    // any server sends is dropped to its end, not read from its middle;
    // every digipeater up to the star is marked repeated in the frame.
    void aprsRobustness()
    {
        using namespace sater::aprs;
        Packet p;
        p.source = QStringLiteral("F4AAA-7");
        p.destination = QStringLiteral("APZSAT");
        p.path = {QStringLiteral("F5DIG-3"), QStringLiteral("WIDE2*"), QStringLiteral("WIDE3-1")};
        p.info = "!4345.48N/00725.08E\\000/000/251/965";
        const QByteArray frame = encodeFrame(p);
        QCOMPARE(uchar(frame.at(2 * 7 + 6)) & 0x80, 0x80);
        QCOMPARE(uchar(frame.at(3 * 7 + 6)) & 0x80, 0x80);
        QCOMPARE(uchar(frame.at(4 * 7 + 6)) & 0x80, 0);
        QCOMPARE(decodeFrame(frame)->path, p.path);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        {
            AprsIsClient client;
            client.setServer(QStringLiteral("127.0.0.1"), server.serverPort());
            client.setLogin(QStringLiteral("F4JTV-9"), 12345);
            QSignalSpy notes(&client, &AprsPort::note);
            client.setEnabled(true);
            QTRY_VERIFY(server.hasPendingConnections() || server.waitForNewConnection(50));
            QTcpSocket *peer = server.nextPendingConnection();
            QTRY_VERIFY(peer->canReadLine() || peer->waitForReadyRead(50));
            peer->readLine();
            peer->write("# aprsc 2.1.14-g5e22b37\r\n# Invalid username format\r\n");
            QTRY_VERIFY(client.detail().contains(QStringLiteral("Invalid username format")));
            QVERIFY(!client.canSend());
            QVERIFY(notes.count() >= 1);
            peer->close();
            QTRY_COMPARE(int(client.state()), int(AprsPort::State::Off));
            QVERIFY(client.detail().contains(QStringLiteral("Invalid username format")));
            client.setEnabled(false);
        }
        {
            AprsIsClient client;
            client.setServer(QStringLiteral("127.0.0.1"), server.serverPort());
            client.setLogin(QStringLiteral("F4JTV-9"), 12345);
            QSignalSpy received(&client, &AprsPort::packetReceived);
            client.setEnabled(true);
            QTRY_VERIFY(server.hasPendingConnections() || server.waitForNewConnection(50));
            QTcpSocket *peer = server.nextPendingConnection();
            QTRY_VERIFY(peer->canReadLine() || peer->waitForReadyRead(50));
            peer->readLine();
            peer->write("# logresp F4JTV-9 verified, server T2TEST\r\n");
            QTRY_VERIFY(client.canSend());
            peer->write(QByteArray(1100, 'x'));
            peer->flush();
            QTest::qWait(150);
            peer->write("F4AAA-7>APZSAT:!4345.48N/00725.08E\\000/000/090/965\r\n");
            peer->write("F1BBB>APZSAT:!4345.48N/00725.08E\\000/000/180/965\r\n");
            QTRY_COMPARE(received.count(), 1);
            QTest::qWait(50);
            QCOMPARE(received.count(), 1);
            QCOMPARE(received.first().first().value<Packet>().source, QStringLiteral("F1BBB"));
            client.setEnabled(false);
        }
    }

    // The built-in modem's settings and the direwolf.conf made from them.
    void modemConfiguration()
    {
        using namespace sater::aprs;
        ModemSettings s;
        s.audioIn = QStringLiteral("plughw:CARD=Device,DEV=0");
        s.audioOut = QString();         // the same card
        s.ptt = QStringLiteral("CM108");
        s.gpio = 3;
        s.txDelayMs = 305;
        QString text = modemConfigText(QStringLiteral("f4aaa-7"), s);
        QVERIFY2(text.contains(QStringLiteral("\nADEVICE  plughw:CARD=Device,DEV=0\n")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("\nMYCALL   F4AAA-7\n")));
        QVERIFY(text.contains(QStringLiteral("\nMODEM    1200\n")));
        QVERIFY(text.contains(QStringLiteral("\nPTT      CM108 3\n")));
        QVERIFY(text.contains(QStringLiteral("\nTXDELAY  30\n")));
        QVERIFY(text.contains(QStringLiteral("\nTXTAIL   10\n")));
        QVERIFY(text.contains(QStringLiteral("\nARATE    %1\n").arg(modemSampleRate())));
        s = ModemSettings();
        text = modemConfigText(QString(), s);
        QVERIFY(text.contains(QStringLiteral("#ADEVICE default")));
        QVERIFY(text.contains(QStringLiteral("\nMYCALL   NOCALL\n")));
        QVERIFY(text.contains(QStringLiteral("# No PTT line")));
        s.ptt = QStringLiteral("rts");
        QVERIFY(modemConfigText(QString(), s).contains(QStringLiteral("PTT      %1 RTS").arg(defaultSerialPort())));
        s.ptt = QStringLiteral("dtr");
        s.pttDevice = QStringLiteral("/dev/ttyACM0");
        QVERIFY(modemConfigText(QString(), s).contains(QStringLiteral("PTT      /dev/ttyACM0 DTR")));
        s.ptt = QStringLiteral("gpio");
        QCOMPARE(s.normalized().ptt, QStringLiteral("none"));
        s.gpio = 12;
        s.txDelayMs = 99999;
        QCOMPARE(s.normalized().gpio, 8);
        QCOMPARE(s.normalized().txDelayMs, 2550);

        const auto cards = parseAsoundCards(QStringLiteral(
            " 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n"
            "                      HDA Intel PCH at 0xf7f10000 irq 32\n"
            " 1 [Device         ]: USB-Audio - USB Audio Device\n"
            "                      C-Media Electronics Inc. USB Audio Device at usb-0000:00:14.0-2, full speed\n"));
        QCOMPARE(cards.size(), 2);
        QCOMPARE(cards.at(1).value, QStringLiteral("plughw:CARD=Device"));
        QCOMPARE(cards.at(1).label, QStringLiteral("USB Audio Device  (plughw:CARD=Device)"));
        QCOMPARE(audioDevices().first().value, QStringLiteral("default"));

        // Through the mission's APRS settings.
        Mission m;
        AprsLink *aprs = m.aprs();
#ifdef SATER_MODEM
        QVERIFY(aprs->modemAvailable());
#else
        QVERIFY(!aprs->modemAvailable());
#endif
        QVERIFY(!aprs->modemEnabled());
        aprs->setModemAudioIn(QStringLiteral("plughw:CARD=Device,DEV=0"));
        QCOMPARE(aprs->modemAudioOut(), QStringLiteral("plughw:CARD=Device,DEV=0"));   // follows the input
        aprs->setModemAudioOut(QStringLiteral("default"));
        aprs->setModemAudioIn(QStringLiteral("default"));
        QCOMPARE(aprs->modemAudioOut(), QStringLiteral("default"));
        aprs->setModemPtt(QStringLiteral("cm108"));
        aprs->setModemTxDelay(1234);
        QCOMPARE(aprs->modemPtt(), QStringLiteral("cm108"));
        QCOMPARE(aprs->modemTxDelay(), 1230);
        QVERIFY(aprs->modemAudioDevices().size() >= 1);
        aprs->setModemPtt(QStringLiteral("none"));
        aprs->setModemTxDelay(300);
    }

#ifdef SATER_MODEM_TEST
    // The built-in modem end to end without a sound card: a DF report
    // goes through Dire Wolf's modulator into a file (ALSA's file plugin),
    // that audio is played to the receiver on its standard input, and the
    // report comes back out of the demodulator.
    void modemRoundTrip()
    {
        using namespace sater::aprs;
        // Dire Wolf keeps 29 characters of a device name: a short folder.
        QTemporaryDir dir(QStringLiteral("/tmp/smXXXXXX"));
        QVERIFY(dir.isValid());
        const auto conf = [&](const QString &name, const QString &adevice) {
            const QString path = dir.filePath(name);
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return QString();
            f.write(QStringLiteral("ADEVICE %1\nARATE 44100\nACHANNELS 1\nCHANNEL 0\nMYCALL F4AAA-7\nMODEM 1200\n"
                                   "TXDELAY 30\nTXTAIL 10\n").arg(adevice).toUtf8());
            return path;
        };
        ModemPort modem;
        modem.setPath({QStringLiteral("WIDE1-1")});

        // A sound card that is not there: Dire Wolf's reason, no crash.
        QSignalSpy notes(&modem, &AprsPort::note);
        modem.setConfigFile(conf(QStringLiteral("bad.conf"), QStringLiteral("plughw:CARD=NoSuchCard,DEV=0")));
        modem.setEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(modem.state() == AprsPort::State::Off && modem.idle(), 10000);
        QVERIFY(!modem.detail().isEmpty());
        QVERIFY(notes.count() >= 1);
        modem.setEnabled(false);

        // Transmit into a file.
        const QString raw = dir.filePath(QStringLiteral("t"));
        modem.setConfigFile(conf(QStringLiteral("tx.conf"), QStringLiteral("null file:FILE=%1").arg(raw)));
        modem.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 10000);
        QVERIFY2(modem.detail().contains(QStringLiteral("1200")), qPrintable(modem.detail()));
        DfReport r;
        r.lat = 43.758;
        r.lon = 7.418;
        r.bearing = 251;
        r.uncertainty = 5;
        r.signal = QStringLiteral("S7");
        r.time = QDateTime(QDate(2026, 10, 11), QTime(10, 23), QTimeZone::utc());
        Packet out;
        out.source = QStringLiteral("F4AAA-7");
        out.destination = QStringLiteral("APZSAT");
        out.info = encodeDfReport(r, 50);
        QSignalSpy keyed(&modem, &ModemPort::transmittingChanged);
        QVERIFY(modem.canSend());
        QVERIFY(modem.send(out));
        QTRY_VERIFY_WITH_TIMEOUT(keyed.count() >= 2 && !modem.transmitting() && modem.queuedFrames() == 0, 15000);
        modem.setEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(modem.idle(), 10000);
        QFile rawFile(raw);
        QVERIFY(rawFile.open(QIODevice::ReadOnly));
        const QByteArray audio = rawFile.readAll();
        // The frame at 1200 baud with 300 ms of flags: well over half a second.
        QVERIFY2(audio.size() > 44100, qPrintable(QString::number(audio.size())));

        // Receive it from standard input, then silence, at the pace of the
        // audio, while the demodulator works.
        int fds[2];
        QCOMPARE(::pipe(fds), 0);
        ::fcntl(fds[1], F_SETFL, ::fcntl(fds[1], F_GETFL) | O_NONBLOCK);
        const int savedStdin = ::dup(0);
        ::dup2(fds[0], 0);
        ::close(fds[0]);
        std::atomic<bool> feeding{true};
        std::thread feeder;
        struct Cleanup {
            std::atomic<bool> &feeding;
            std::thread &feeder;
            int writeEnd, saved;
            ~Cleanup()
            {
                feeding = false;
                if (feeder.joinable()) feeder.join();
                ::close(writeEnd);
                ::dup2(saved, 0);
                ::close(saved);
            }
        } cleanup{feeding, feeder, fds[1], savedStdin};

        QSignalSpy received(&modem, &AprsPort::packetReceived);
        modem.setConfigFile(conf(QStringLiteral("rx.conf"), QStringLiteral("stdin null")));
        modem.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 10000);
        feeder = std::thread([&feeding, audio, fd = fds[1]] {
            const QByteArray silence(8820, '\0');   // 0.1 s
            const auto put = [&](const QByteArray &bytes) {
                qsizetype done = 0;
                while (done < bytes.size() && feeding) {
                    const ssize_t n = ::write(fd, bytes.constData() + done, size_t(bytes.size() - done));
                    if (n > 0) done += n;
                    else if (n < 0 && errno != EAGAIN && errno != EINTR) return;
                    else std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            };
            for (int i = 0; i < 3; ++i) put(silence);
            put(audio);
            while (feeding) {
                put(silence);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        QTRY_VERIFY_WITH_TIMEOUT(received.count() >= 1, 20000);
        const Packet in = received.first().first().value<Packet>();
        QCOMPARE(in.source, QStringLiteral("F4AAA-7"));
        QCOMPARE(in.destination, QStringLiteral("APZSAT"));
        QCOMPARE(in.path, QStringList{QStringLiteral("WIDE1-1")});
        const auto df = parseDfReport(in);
        QVERIFY(df);
        QCOMPARE(df->bearing, 251.0);
        QCOMPARE(df->signal, QStringLiteral("S7"));
        QCOMPARE(df->uncertainty, 5.0);
        modem.setEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(modem.idle(), 10000);
    }

    // The modem stopped and started again around transmissions: no
    // transmit thread left behind (it would key the radio with a closed
    // sound card and take the next frames), nothing sent after the stop,
    // new settings waiting for the end of a transmission, at once after a
    // failure; the port destroyed while the core starts does not abort.
    void modemStopsCleanly()
    {
        using namespace sater::aprs;
        const auto threads = []() { return int(QDir(QStringLiteral("/proc/self/task")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size()); };
        QTemporaryDir dir(QStringLiteral("/tmp/smXXXXXX"));
        QVERIFY(dir.isValid());
        const auto conf = [&](const QString &name, const QString &adevice) {
            const QString path = dir.filePath(name);
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return QString();
            f.write(QStringLiteral("ADEVICE %1\nARATE 44100\nACHANNELS 1\nCHANNEL 0\nMYCALL F4AAA-7\nMODEM 1200\n"
                                   "TXDELAY 30\nTXTAIL 10\n").arg(adevice).toUtf8());
            return path;
        };
        Packet out;
        out.source = QStringLiteral("F4AAA-7");
        out.destination = QStringLiteral("APZSAT");
        out.info = QByteArray(":F1BBB    :") + QByteArray(180, 'x');   // about 1.5 s on the air
        const QString good = conf(QStringLiteral("tx.conf"), QStringLiteral("null file:FILE=%1").arg(dir.filePath(QStringLiteral("t"))));

        const int before = threads();
        {
            ModemPort modem;
            modem.setConfigFile(good);
            modem.setEnabled(true);
            QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 10000);
            const int running = threads();
            QVERIFY(running > before);
            // Stopped in the middle of a transmission, more frames queued.
            for (int i = 0; i < 3; ++i) QVERIFY(modem.send(out));
            QTRY_VERIFY_WITH_TIMEOUT(modem.transmitting(), 10000);
            modem.setEnabled(false);
            QTRY_VERIFY_WITH_TIMEOUT(modem.idle(), 15000);
            QTRY_COMPARE_WITH_TIMEOUT(threads(), before, 5000);
            QVERIFY(!modem.transmitting());
            // Started again: as many threads as the first time, and the
            // frames left in the queue are not sent.
            QSignalSpy keyed(&modem, &ModemPort::transmittingChanged);
            modem.setEnabled(true);
            QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(threads(), running, 5000);
            QTest::qWait(1500);
            QCOMPARE(keyed.count(), 0);
            QCOMPARE(modem.queuedFrames(), 0);

            // New settings during a transmission: after it.
            QVERIFY(modem.send(out));
            QTRY_VERIFY_WITH_TIMEOUT(modem.transmitting(), 10000);
            ModemSettings changed = modem.settings();
            changed.txTailMs += 10;
            modem.setSettings(changed);
            QCOMPARE(int(modem.state()), int(AprsPort::State::Connected));   // still on the air
            QTRY_VERIFY_WITH_TIMEOUT(!modem.transmitting(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(modem.state() != AprsPort::State::Connected || !modem.idle(), 5000);
            QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 15000);
            QTRY_COMPARE_WITH_TIMEOUT(threads(), running, 5000);

            // Off after a failure: the next settings are tried at once.
            modem.setEnabled(false);
            QTRY_VERIFY_WITH_TIMEOUT(modem.idle(), 15000);
            modem.setConfigFile(conf(QStringLiteral("bad.conf"), QStringLiteral("plughw:CARD=NoSuchCard")));
            modem.setEnabled(true);
            QTRY_VERIFY_WITH_TIMEOUT(modem.state() == AprsPort::State::Off && modem.idle(), 10000);
            modem.setConfigFile(good);
            changed.txTailMs += 10;
            QElapsedTimer t;
            t.start();
            modem.setSettings(changed);
            QTRY_COMPARE_WITH_TIMEOUT(int(modem.state()), int(AprsPort::State::Connected), 4000);   // the first retry is 5 s away
            QVERIFY(t.elapsed() < 4000);
            modem.setEnabled(false);
            QTRY_VERIFY_WITH_TIMEOUT(modem.idle(), 15000);
        }
        QTRY_COMPARE_WITH_TIMEOUT(threads(), before, 5000);

        // Destroyed while the core starts: stopped, not aborted.
        QElapsedTimer t;
        t.start();
        {
            ModemPort modem;
            modem.setConfigFile(good);
            modem.setEnabled(true);
        }
        QVERIFY(t.elapsed() < 10000);
        QTRY_COMPARE_WITH_TIMEOUT(threads(), before, 5000);
        // And the core is free for the next one.
        ModemPort again;
        again.setConfigFile(good);
        again.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(int(again.state()), int(AprsPort::State::Connected), 10000);
        again.setEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(again.idle(), 15000);
    }
#endif

    void paintBenchmark()
    {
        const QString dir = qEnvironmentVariable("SATER_BENCH_TILES");
        if (dir.isEmpty()) QSKIP("SATER_BENCH_TILES not set");
        Mission m;
        m.newMission();
        m.setTilesDir(dir);
        m.setOffline(true);
        m.setProvider(QStringLiteral("osm"));
        for (int i = 0; i < 4; ++i) {
            const int r = m.addStationAt(43.6 + 0.05 * i, 7.2 + 0.04 * i);
            m.stations()->setField(r, QStringLiteral("callsign"), QStringLiteral("S%1").arg(i));
            m.stations()->setField(r, QStringLiteral("azimuth"), 30 + 70 * i);
        }
        m.setShowGrid(true);
        MapView view;
        view.setSize(QSizeF(393, 851));
        view.setMission(&m);
        view.centerOn(43.69, 7.29, 12);
        view.renderImage(1.0);               // asks for the tiles
        QTest::qWait(1500);                  // let them all load and decode
        view.renderImage(1.0);
        QTest::qWait(500);
        QElapsedTimer t;
        t.start();
        const int n = 20;
        for (int i = 0; i < n; ++i) {
            view.centerOn(43.69 + i * 0.0005, 7.29, 12);
            view.renderImage(2.75);
        }
        qInfo("phone view, 2.75 dppx: %.1f ms per frame", double(t.elapsed()) / n);
        t.restart();
        for (int i = 0; i < n; ++i) {
            view.centerOn(43.69 + i * 0.0005, 7.29, 12);
            view.renderImage(1.0);
        }
        qInfo("phone view, 1 dppx: %.1f ms per frame", double(t.elapsed()) / n);
        m.setShowGrid(false);
        t.restart();
        for (int i = 0; i < n; ++i) {
            view.centerOn(43.69 + i * 0.0005, 7.29, 12);
            view.renderImage(2.75);
        }
        qInfo("phone view, 2.75 dppx, no grid: %.1f ms per frame", double(t.elapsed()) / n);
        m.setShowIntersection(false);
        for (int r = 0; r < m.stations()->count(); ++r) m.stations()->setField(r, QStringLiteral("uncertainty"), 0);
        t.restart();
        for (int i = 0; i < n; ++i) {
            view.centerOn(43.69 + i * 0.0005, 7.29, 12);
            view.renderImage(2.75);
        }
        qInfo("phone view, 2.75 dppx, no grid, no cones, no zone: %.1f ms per frame", double(t.elapsed()) / n);
        m.stations()->clear();
        t.restart();
        for (int i = 0; i < n; ++i) {
            view.centerOn(43.69 + i * 0.0005, 7.29, 12);
            view.renderImage(2.75);
        }
        qInfo("phone view, 2.75 dppx, tiles only: %.1f ms per frame", double(t.elapsed()) / n);
        m.setShowIntersection(true);
        m.newMission();
        m.setOffline(false);
        m.setShowGrid(false);
        m.setTilesDir(QString());
        m.saveNow();
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
