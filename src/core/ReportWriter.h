/*
 * ReportWriter.h - the PDF mission report.
 *
 * The Python version built it with reportlab; this one lays out the same
 * sections, tables and colours with QTextDocument and paints them through
 * QPdfWriter, which needs nothing outside QtGui on any platform.  Each
 * section (a heading and its table, the map) is laid out on its own and
 * kept on one page: it starts a new page when it does not fit on this one,
 * and only a table taller than a whole page is cut, between rows, with its
 * header row repeated.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "MissionData.h"

#include <QImage>
#include <QIODevice>
#include <QList>
#include <QString>

#include <optional>

namespace sater {

struct ReportData {
    QString title;
    QString subject;
    QString keywords;
    QString comment;
    QString startDate = QStringLiteral("N/A");
    QString startTime = QStringLiteral("N/A");
    QString duration = QStringLiteral("00:00:00");
    int stationCount = 0;
    int recordCount = 0;
    QList<Station> stations;
    KilometerList kilometers;
    struct Zone {
        double lat = 0, lon = 0, radiusKm = 0, surfaceKm2 = 0;
        QString mgrs;
        int pointCount = 0;
    };
    std::optional<Zone> zone;
    // The least-squares estimate (Fix.h).
    struct Estimate {
        double lat = 0, lon = 0;
        QString mgrs;
        double major95Km = 0, minor95Km = 0, axisDeg = 0;   // full axes
        double major50Km = 0, minor50Km = 0;
        int bearingCount = 0;
        QString agreement;      // the bearings agree, or which one does not
    };
    std::optional<Estimate> estimate;
    struct Beacon {
        double lat = 0, lon = 0;
        QString mgrs;
    };
    std::optional<Beacon> beacon;
    QImage mapImage;
    QList<AzimuthRecord> history;
    QImage logo;
    QString organization;       // "ADRASEC 06" in the footer
    QString appVersion;
};

// The report as HTML for QTextDocument (exposed for the tests).
QString reportHtml(const ReportData &data);

// Write the PDF to device (opened for writing).  False on failure.
bool writeReport(const ReportData &data, QIODevice *device, QString *error);

// Where each piece of the report lands (for the tests): the section it
// belongs to, its page (from 0), top and height in the text area of the
// page, in 1/96 inch.  pageHeight: the height of that text area.
struct ReportPiece {
    QString section;
    int page = 0;
    double top = 0;
    double height = 0;
};
QList<ReportPiece> reportLayout(const ReportData &data, double *pageHeight = nullptr);

} // namespace sater
