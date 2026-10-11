/*
 * ReportWriter.h - the PDF mission report.
 *
 * The Python version built it with reportlab; this one lays out the same
 * sections, tables and colours with QTextDocument and prints them through
 * QPdfWriter, which needs nothing outside QtGui on any platform.
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

} // namespace sater
