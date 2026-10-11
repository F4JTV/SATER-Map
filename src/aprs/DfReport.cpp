/*
 * DfReport.cpp - APRS direction-finding reports.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "DfReport.h"

#include "core/MissionData.h"

#include <QRegularExpression>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace sater::aprs {

namespace {

// Beam width upper bound for Q = 1..9 (Q = 0: useless).
constexpr double kBeamWidth[10] = {360, 240, 120, 64, 32, 16, 8, 4, 2, 1};

bool digits(const QByteArray &s, int from, int count)
{
    if (from + count > s.size()) return false;
    for (int i = from; i < from + count; ++i) {
        if (s.at(i) < '0' || s.at(i) > '9') return false;
    }
    return true;
}

// "4903.50N" / "07201.75W"; spaces (position ambiguity) read as zeros.
std::optional<double> readCoordinate(QByteArray s, bool latitude)
{
    const int degDigits = latitude ? 2 : 3;
    if (s.size() != degDigits + 6) return std::nullopt;
    for (char &c : s) {
        if (c == ' ') c = '0';
    }
    if (!digits(s, 0, degDigits + 2) || s.at(degDigits + 2) != '.' || !digits(s, degDigits + 3, 2)) return std::nullopt;
    const char hemi = s.at(degDigits + 5);
    const double deg = s.left(degDigits).toDouble();
    const double min = s.mid(degDigits, 5).toDouble();
    if (min >= 60.0) return std::nullopt;
    double v = deg + min / 60.0;
    if (latitude) {
        if (hemi != 'N' && hemi != 'S') return std::nullopt;
        if (v > 90.0) return std::nullopt;
        if (hemi == 'S') v = -v;
    } else {
        if (hemi != 'E' && hemi != 'W') return std::nullopt;
        if (v > 180.0) return std::nullopt;
        if (hemi == 'W') v = -v;
    }
    return v;
}

QByteArray writeCoordinate(double v, bool latitude)
{
    const char hemi = latitude ? (v < 0 ? 'S' : 'N') : (v < 0 ? 'W' : 'E');
    double a = std::fabs(v);
    int deg = int(std::floor(a));
    // Hundredths of a minute, the carry into the degrees done by hand.
    long hundredths = std::lround((a - deg) * 6000.0);
    if (hundredths >= 6000) {
        hundredths -= 6000;
        ++deg;
    }
    const QString text = QStringLiteral("%1%2.%3%4")
                             .arg(deg, latitude ? 2 : 3, 10, QLatin1Char('0'))
                             .arg(hundredths / 100, 2, 10, QLatin1Char('0'))
                             .arg(hundredths % 100, 2, 10, QLatin1Char('0'))
                             .arg(QLatin1Char(hemi));
    return text.toLatin1();
}

// "DDHHMMz" (UTC), "DDHHMM/" (the sender's local time: read in this
// computer's, the operation being in one area) or "HHMMSSh" (UTC), dated
// from now.  Returned in UTC.
QDateTime readTimestamp(const QByteArray &t, const QDateTime &now)
{
    if (t.size() != 7 || !digits(t, 0, 6)) return {};
    const int a = t.mid(0, 2).toInt(), b = t.mid(2, 2).toInt(), c = t.mid(4, 2).toInt();
    if (t.at(6) == 'h') {
        const QDateTime utc = now.toUTC();
        QDateTime when(utc.date(), QTime(a, b, c), QTimeZone::utc());
        if (!when.isValid()) return {};
        if (when > utc.addSecs(3600)) when = when.addDays(-1);
        return when;
    }
    if (t.at(6) != 'z' && t.at(6) != '/') return {};
    const bool local = t.at(6) == '/';
    const QDateTime ref = local ? now.toLocalTime() : now.toUTC();
    const auto make = [&](const QDate &date) {
        return local ? QDateTime(date, QTime(b, c)) : QDateTime(date, QTime(b, c), QTimeZone::utc());
    };
    // Day of this month, or of the last one if it would be in the future.
    QDateTime when = make(QDate(ref.date().year(), ref.date().month(), a));
    if (!when.isValid() || when > ref.addSecs(3600)) {
        const QDate prev = ref.date().addMonths(-1);
        when = make(QDate(prev.year(), prev.month(), a));
    }
    return when.isValid() ? when.toUTC() : QDateTime();
}

} // namespace

int qualityFromUncertainty(double uncertaintyDeg)
{
    const double width = 2 * std::max(0.0, uncertaintyDeg);
    for (int q = 9; q >= 1; --q) {
        if (width < kBeamWidth[q]) return q;
    }
    return 0;
}

double uncertaintyFromQuality(int q)
{
    q = std::clamp(q, 1, 9);
    return std::max(1.0, kBeamWidth[q] / 2);
}

std::optional<DfReport> parseDfReport(const QString &source, const QByteArray &info, const QDateTime &now)
{
    if (info.isEmpty()) return std::nullopt;
    const char type = info.at(0);
    int pos = 1;
    DfReport r;
    if (type == '/' || type == '@') {
        r.time = readTimestamp(info.mid(1, 7), now);
        pos = 8;
    } else if (type != '!' && type != '=') {
        return std::nullopt;
    }
    // Uncompressed position: 8 + table + 9 + code.
    if (info.size() < pos + 19 + 15) return std::nullopt;
    const auto lat = readCoordinate(info.mid(pos, 8), true);
    const auto lon = readCoordinate(info.mid(pos + 9, 9), false);
    if (!lat || !lon) return std::nullopt;
    // The bearing field means a bearing only with the DF symbol (table /,
    // code \\)
    // (a car's course/speed followed by anything is not one).
    if (info.at(pos + 8) != '/' || info.at(pos + 18) != '\\') return std::nullopt;
    const int ext = pos + 19;
    // CSE/SPD then /BRG/NRQ.
    const QByteArray d = info.mid(ext, 15);
    if (d.at(3) != '/' || d.at(7) != '/' || d.at(11) != '/') return std::nullopt;
    if (!digits(d, 8, 3) || !digits(d, 12, 3)) return std::nullopt;
    const int bearing = d.mid(8, 3).toInt();
    // 000: an omni report (signal strength only), not a bearing.
    if (bearing < 1 || bearing > 360) return std::nullopt;
    r.source = normalizeCallsign(source);
    r.lat = *lat;
    r.lon = *lon;
    r.bearing = bearing == 360 ? 0.0 : double(bearing);
    r.hits = d.at(12) - '0';
    r.rangeCode = d.at(13) - '0';
    r.quality = d.at(14) - '0';
    if (r.hits == 0) {
        // "If N is 0, the NRQ value is meaningless": the default spread.
        r.rangeCode = 0;
        r.quality = 0;
    } else if (r.quality == 0) {
        return std::nullopt;   // "useless"
    } else {
        r.uncertainty = uncertaintyFromQuality(r.quality);
    }
    r.comment = QString::fromUtf8(info.mid(ext + 15)).trimmed();

    static const QRegularExpression sater(QStringLiteral("\\bSATER((?:\\s+\\S+)*)"));
    const QRegularExpressionMatch m = sater.match(r.comment);
    if (m.hasMatch()) {
        r.fromSaterMap = true;
        for (const QString &token : m.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (sater::signalLevels().contains(token)) {
                r.signal = token;
            } else if (token.startsWith(QLatin1Char('U'))) {
                bool ok = false;
                const double u = token.mid(1).toDouble(&ok);
                if (ok && u >= 0 && u <= 90) r.uncertainty = u;
            }
        }
    }
    return r;
}

std::optional<DfReport> parseDfReport(const Packet &packet, const QDateTime &now)
{
    if (packet.info.startsWith('}')) {
        // Third party (an internet gateway's relay): the inner packet.
        const auto inner = parseTnc2(packet.info.mid(1));
        if (!inner) return std::nullopt;
        return parseDfReport(*inner, now);
    }
    return parseDfReport(packet.source, packet.info, now);
}

QByteArray encodeDfReport(const DfReport &report, double rangeKm)
{
    const QDateTime when = (report.time.isValid() ? report.time : QDateTime::currentDateTimeUtc()).toUTC();
    int bearing = int(std::lround(normalizeAzimuth(report.bearing)));
    if (bearing == 0 || bearing == 360) bearing = 360;
    const double miles = std::max(1.0, rangeKm / 1.609344);
    const int range = std::clamp(int(std::ceil(std::log2(miles))), 0, 9);
    const int q = std::max(1, qualityFromUncertainty(report.uncertainty));

    QByteArray info = "/";
    info += when.toString(QStringLiteral("ddHHmm")).toLatin1() + 'z';
    info += writeCoordinate(report.lat, true);
    info += '/';
    info += writeCoordinate(report.lon, false);
    info += '\\';
    info += "000/000/";
    info += QByteArray::number(bearing).rightJustified(3, '0');
    info += '/';
    info += QByteArray::number(std::clamp(report.hits, 0, 9));
    info += QByteArray::number(range);
    info += QByteArray::number(q);
    info += " SATER";
    if (!report.signal.isEmpty()) info += ' ' + report.signal.toLatin1();
    info += " U" + QByteArray::number(report.uncertainty, 'f', 1);
    return info;
}

} // namespace sater::aprs
