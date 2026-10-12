/*
 * DfReport.h - APRS direction-finding reports.
 *
 * The APRS DF report (APRS 1.0.1, chapter 7) is a position report followed
 * by the course/speed extension and an 8-byte bearing field:
 *
 *     /111023z4345.48N/00725.08E\000/000/251/965 SATER S7 U5.0
 *     |       |                 ||       |   |   `- comment
 *     |       position          ||       |   `- N, R, Q
 *     timestamp (UTC)           ||       `- bearing, degrees from true
 *                               ||          north, 001-360 (360 north)
 *                               |`- course/speed: 000/000, a fixed station
 *                               `- symbol: DF station (table '/', code '\')
 *
 * N is 9 for a manual report; the range is 2^R miles; Q is the accuracy,
 * the beam width: 9 under 1 degree, 8 under 2, 7 under 4, 6 under 8, 5 under
 * 16, 4 under 32, 3 under 64, 2 under 120, 1 under 240, 0 useless.  Other
 * APRS programs (Xastir, APRSIS32...) plot such a report as a bearing line.
 * SATER Map adds to the comment what the format has no field for: the
 * S-meter reading and the exact uncertainty ("SATER S7 U5.0").
 *
 * The bearing of another station (one without APRS, heard by telephone)
 * goes out as an APRS object (chapter 11) named after that station, sent
 * from this station's callsign, with the same symbol and fields:
 *
 *     ;F4XYZ-7  *111023z4345.48N/00725.08E\000/000/251/965 SATER S7 U5.0
 *
 * The specification defines BRG/NRQ for DF reports only: for an object it
 * is the comment (which "may contain any appropriate APRS data").  Other
 * programs show the object at the station's position; SATER Map reads the
 * bearing from it.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "Ax25.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace sater::aprs {

struct DfReport {
    QString source;              // the station that sent it
    QString object;              // an object: the station the bearing is of
                                 // (its name); empty: the sender's own
    double lat = 0.0;
    double lon = 0.0;
    double bearing = 0.0;        // true, [0, 360)
    int hits = 9;                // N (9: manual)
    int rangeCode = 6;           // R: 2^R miles
    int quality = 5;             // Q
    double uncertainty = 8.0;    // half the beam width, degrees
    QString signal;              // "S7"; empty when not given
    bool fromSaterMap = false;   // the comment carried SATER's fields
    QDateTime time;              // when taken (UTC), from the timestamp; invalid if none
    QString comment;             // what follows the bearing field

    // The station the bearing was taken by.
    QString station() const { return object.isEmpty() ? source : object; }
};

// Can name an APRS object: 1 to 9 printable ASCII characters (spaces
// inside), no | or ~ (kept by the network), not starting with a space.
bool isValidObjectName(const QString &name);

// The DF report in an APRS information field, if it is one: a position
// report, or a live object (object set to its name).  now dates a
// "DDHHMMz" timestamp (day of the month, hours, minutes).
std::optional<DfReport> parseDfReport(const QString &source, const QByteArray &info,
                                      const QDateTime &now = QDateTime::currentDateTimeUtc());
// The same from a packet, a third-party one ("}SRC>DEST:...") unwrapped.
std::optional<DfReport> parseDfReport(const Packet &packet, const QDateTime &now = QDateTime::currentDateTimeUtc());

// The information field of a DF report: timestamp from report.time (now if
// invalid), the uncertainty sets Q, rangeKm sets R, the signal goes into
// the SATER comment.
QByteArray encodeDfReport(const DfReport &report, double rangeKm);
// The same as a live object named report.object (a valid object name, or
// an empty result).
QByteArray encodeDfObject(const DfReport &report, double rangeKm);

// Q from the half-width of the cone, and back (the upper bound's half).
int qualityFromUncertainty(double uncertaintyDeg);
double uncertaintyFromQuality(int q);

} // namespace sater::aprs
