/*
 * ReportWriter.cpp - the PDF mission report.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ReportWriter.h"

#include "Geo.h"
#include "Fix.h"
#include "Geomag.h"
#include "Tr.h"

#include <algorithm>
#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QFont>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>
#include <QUrl>

namespace sater {

namespace {

// The document is laid out at 96 dpi: 1 cm = 37.8 px.
constexpr int kDpi = 96;
int cm(double v) { return int(v / 2.54 * kDpi + 0.5); }

QString esc(const QString &s) { return s.toHtmlEscaped(); }

const char *kOrange = "#e65c00";
const char *kBlue = "#003d80";

QString separator()
{
    // The orange/blue rule of the Python report (9 cm + 8 cm).
    return QStringLiteral("<table width=\"%1\" align=\"center\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
                          "<tr><td width=\"%2\" bgcolor=\"%4\" style=\"font-size:4pt\">&nbsp;</td>"
                          "<td width=\"%3\" bgcolor=\"%5\" style=\"font-size:4pt\">&nbsp;</td></tr></table>")
        .arg(cm(17))
        .arg(cm(9))
        .arg(cm(8))
        .arg(QLatin1String(kOrange), QLatin1String(kBlue));
}

QString section(const QString &title)
{
    return QStringLiteral("<p style=\"font-size:14pt; color:%1; margin-top:15px; margin-bottom:8px\"><b>%2</b></p>")
        .arg(QLatin1String(kOrange), esc(title));
}

// A two-column key/value table: bold keys on a tinted background.
QString keyValueTable(const QList<QPair<QString, QString>> &rows, const QString &keyBg, const QString &grid)
{
    QString h = QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"7\" border=\"1\" "
                               "style=\"border-collapse:collapse; border-style:solid; border-color:%2\">")
                    .arg(cm(15))
                    .arg(grid);
    for (const auto &r : rows) {
        h += QStringLiteral("<tr><td width=\"%1\" bgcolor=\"%2\" style=\"font-size:10pt\"><b>%3</b></td>"
                            "<td width=\"%4\" style=\"font-size:10pt\">%5</td></tr>")
                 .arg(cm(5))
                 .arg(keyBg, esc(r.first))
                 .arg(cm(10))
                 .arg(esc(r.second));
    }
    h += QStringLiteral("</table>");
    return h;
}

QString headerRow(const QStringList &cells, const QList<double> &widthsCm, const QString &bg, double fontPt)
{
    QString h = QStringLiteral("<tr>");
    for (int i = 0; i < cells.size(); ++i) {
        h += QStringLiteral("<td width=\"%1\" bgcolor=\"%2\" align=\"center\" style=\"font-size:%3pt; color:#ffffff\"><b>%4</b></td>")
                 .arg(cm(widthsCm.value(i, 3)))
                 .arg(bg)
                 .arg(fontPt)
                 .arg(esc(cells.at(i)));
    }
    return h + QStringLiteral("</tr>");
}

QString dmsLines(double lat, double lon)
{
    return esc(geo::ddToDmsString(lat, true)) + QStringLiteral("<br>") + esc(geo::ddToDmsString(lon, false));
}

QString fmtNum(double v)
{
    // Azimuths are whole degrees in the tables; uncertainties keep a
    // decimal when they have one (as Python's str()).
    if (v == double(qint64(v))) return QString::number(qint64(v));
    return QString::number(v, 'g', 10);
}

} // namespace

namespace {

// "248° ±5°", or for a compass reading "248° M ±5°" and the true bearing below.
QString azimuthCell(double azimuth, double uncertainty, bool magnetic, double declination)
{
    QString cell = QStringLiteral("%1°%2 ±%3°").arg(fmtNum(azimuth), magnetic ? QStringLiteral(" M") : QString(), fmtNum(uncertainty));
    if (magnetic) {
        cell += QStringLiteral("<br><span style=\"font-size:7pt; color:#555555\">%1</span>")
                    .arg(esc(SATER_TR("%1° true (declination %2)")
                                 .arg(QString::number(normalizeAzimuth(azimuth + declination), 'f', 1),
                                      geomag::declinationText(declination))));
    }
    return cell;
}

} // namespace

QString reportHtml(const ReportData &d)
{
    QString h;
    h += QStringLiteral("<html><body>");

    if (!d.logo.isNull()) {
        h += QStringLiteral("<p align=\"center\"><img src=\"sater://logo\" width=\"%1\" height=\"%1\"></p>").arg(cm(4));
    }
    h += separator();

    const QString title = d.title.isEmpty() ? SATER_TR("SATER mission report") : d.title;
    h += QStringLiteral("<p align=\"center\" style=\"font-size:20pt; color:#2c3e50; margin-top:15px; margin-bottom:10px\"><b>%1</b></p>")
             .arg(esc(title.toUpper()));
    const QString subject = d.subject.isEmpty() ? SATER_TR("Distress beacon search") : d.subject;
    h += QStringLiteral("<p align=\"center\" style=\"font-size:12pt; color:#7f8c8d; margin-bottom:20px\">%1</p>").arg(esc(subject));

    if (!d.comment.trimmed().isEmpty()) {
        h += section(SATER_TR("Comments / Notes"));
        QString c = esc(d.comment.trimmed());
        c.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        h += QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"8\" border=\"0\" bgcolor=\"#f5f5f5\">"
                            "<tr><td style=\"font-size:10pt\">%2</td></tr></table>")
                 .arg(cm(17))
                 .arg(c);
    }

    h += section(SATER_TR("Mission information"));
    h += keyValueTable({{SATER_TR("Start date:"), d.startDate},
                        {SATER_TR("Start time:"), d.startTime},
                        {SATER_TR("Duration:"), d.duration},
                        {SATER_TR("Number of stations:"), QString::number(d.stationCount)},
                        {SATER_TR("Number of records:"), QString::number(d.recordCount)}},
                       QStringLiteral("#fff3e6"), QLatin1String(kOrange));

    if (!d.stations.isEmpty()) {
        h += section(SATER_TR("Direction-finding stations"));
        const QList<double> w = {3, 6, 4, 2};
        h += QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"5\" border=\"1\" "
                            "style=\"border-collapse:collapse; border-style:solid; border-color:#bdc3c7\">")
                 .arg(cm(15));
        h += headerRow({SATER_TR("Callsign"), SATER_TR("Position (DMS)"), SATER_TR("Azimuth"), SATER_TR("Signal")},
                       w, QLatin1String(kOrange), 9);
        for (const Station &s : d.stations) {
            h += QStringLiteral("<tr><td width=\"%1\" style=\"font-size:9pt\">%2</td>"
                                "<td width=\"%3\" style=\"font-size:9pt\">%4</td>"
                                "<td width=\"%5\" align=\"center\" style=\"font-size:9pt\">%6</td>"
                                "<td width=\"%7\" align=\"center\" style=\"font-size:9pt\">%8</td></tr>")
                     .arg(cm(w[0]))
                     .arg(esc(s.callsign))
                     .arg(cm(w[1]))
                     .arg(dmsLines(s.lat, s.lon))
                     .arg(cm(w[2]))
                     .arg(azimuthCell(s.azimuth, s.uncertainty, s.magnetic, s.declination))
                     .arg(cm(w[3]))
                     .arg(esc(s.signal.isEmpty() ? QStringLiteral("S5") : s.signal));
        }
        h += QStringLiteral("</table>");
    }

    if (!d.kilometers.isEmpty()) {
        h += section(SATER_TR("Distance driven"));
        h += QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"5\" border=\"1\" "
                            "style=\"border-collapse:collapse; border-style:solid; border-color:#27ae60\">")
                 .arg(cm(10));
        h += headerRow({SATER_TR("Station"), SATER_TR("Distance")}, {6, 4}, QStringLiteral("#27ae60"), 10);
        double total = 0;
        for (const auto &p : d.kilometers) {
            h += QStringLiteral("<tr><td width=\"%1\" style=\"font-size:10pt\">%2</td>"
                                "<td width=\"%3\" align=\"right\" style=\"font-size:10pt\">%4 km</td></tr>")
                     .arg(cm(6))
                     .arg(esc(p.first))
                     .arg(cm(4))
                     .arg(p.second, 0, 'f', 1);
            total += p.second;
        }
        h += QStringLiteral("<tr><td bgcolor=\"#e8f8f0\" style=\"font-size:10pt\"><b>%1</b></td>"
                            "<td bgcolor=\"#e8f8f0\" align=\"right\" style=\"font-size:10pt\"><b>%2 km</b></td></tr>")
                 .arg(esc(SATER_TR("TOTAL")))
                 .arg(total, 0, 'f', 1);
        h += QStringLiteral("</table>");
    }

    if (d.estimate) {
        const auto &e = *d.estimate;
        h += section(SATER_TR("Best estimate"));
        h += keyValueTable({{SATER_TR("Coordinates (DD):"), QStringLiteral("%1, %2").arg(e.lat, 0, 'f', 6).arg(e.lon, 0, 'f', 6)},
                            {SATER_TR("Coordinates (DMS):"), geo::ddToDmsString(e.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(e.lon, false)},
                            {SATER_TR("MGRS:"), e.mgrs},
                            {SATER_TR("95 % ellipse:"), SATER_TR("%1 × %2 km, axis %3°").arg(e.major95Km, 0, 'f', 2)
                                                             .arg(e.minor95Km, 0, 'f', 2).arg(axisDegrees(e.axisDeg))},
                            {SATER_TR("50 % ellipse:"), QStringLiteral("%1 × %2 km").arg(e.major50Km, 0, 'f', 2).arg(e.minor50Km, 0, 'f', 2)},
                            {SATER_TR("Bearings:"), QString::number(e.bearingCount)},
                            {SATER_TR("Agreement:"), e.agreement}},
                           QStringLiteral("#f8e6ff"), QStringLiteral("#8e24aa"));
        h += QStringLiteral("<p style=\"font-size:8pt; color:#555555\">%1</p>")
                 .arg(esc(SATER_TR("Least-squares position of the bearings, each weighted by its uncertainty (the cone taken "
                                   "as a 95 % interval). If the bearings are free of gross errors, the beacon lies inside the "
                                   "95 % ellipse about 95 times out of 100 (somewhat less with only two bearings crossing at "
                                   "a sharp angle). Bearings read from magnetic north are corrected by the %1 declination.")
                              .arg(QString::fromLatin1(geomag::kModelName))));
    }

    if (d.zone) {
        const auto &z = *d.zone;
        h += section(SATER_TR("Intersection zone"));
        h += keyValueTable({{SATER_TR("Centre (DD):"), QStringLiteral("%1, %2").arg(z.lat, 0, 'f', 6).arg(z.lon, 0, 'f', 6)},
                            {SATER_TR("Centre (DMS):"), geo::ddToDmsString(z.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(z.lon, false)},
                            {SATER_TR("MGRS:"), z.mgrs.isEmpty() ? QStringLiteral("N/A") : z.mgrs},
                            {SATER_TR("Radius:"), QStringLiteral("%1 km").arg(z.radiusKm, 0, 'f', 2)},
                            {SATER_TR("Surface:"), QStringLiteral("%1 km²").arg(z.surfaceKm2, 0, 'f', 2)},
                            {SATER_TR("Intersection points:"), QString::number(z.pointCount)}},
                           QStringLiteral("#e6f3ff"), QLatin1String(kBlue));
    }

    if (d.beacon) {
        const auto &b = *d.beacon;
        h += section(SATER_TR("Distress beacon position"));
        h += keyValueTable({{SATER_TR("Coordinates (DD):"), QStringLiteral("%1, %2").arg(b.lat, 0, 'f', 6).arg(b.lon, 0, 'f', 6)},
                            {SATER_TR("Coordinates (DMS):"), geo::ddToDmsString(b.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(b.lon, false)},
                            {SATER_TR("MGRS:"), b.mgrs.isEmpty() ? QStringLiteral("N/A") : b.mgrs}},
                           QStringLiteral("#ffe6e6"), QStringLiteral("#ff0000"));
    }

    if (!d.mapImage.isNull()) {
        h += section(SATER_TR("Mission map"));
        // At most 17 x 12 cm, aspect kept (as the Python report).
        const double iw = d.mapImage.width(), ih = d.mapImage.height();
        const double maxW = cm(17), maxH = cm(12);
        double w, hgt;
        if (iw / ih > maxW / maxH) {
            w = maxW;
            hgt = maxW * ih / iw;
        } else {
            hgt = maxH;
            w = maxH * iw / ih;
        }
        h += QStringLiteral("<p align=\"center\"><img src=\"sater://map\" width=\"%1\" height=\"%2\"></p>")
                 .arg(int(w))
                 .arg(int(hgt));
    }

    if (!d.history.isEmpty()) {
        h += section(SATER_TR("Bearing history"));
        const QList<double> w = {3, 2, 3, 1.5, 5};
        h += QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"4\" border=\"1\" "
                            "style=\"border-collapse:collapse; border-style:solid; border-color:#bdc3c7\">")
                 .arg(cm(14.5));
        h += headerRow({SATER_TR("Date/Time"), SATER_TR("Station"), SATER_TR("Azimuth"), SATER_TR("Signal"), SATER_TR("Position")},
                       w, QLatin1String(kBlue), 8);
        const int from = std::max(0, int(d.history.size()) - 20);
        for (int i = from; i < d.history.size(); ++i) {
            const AzimuthRecord &r = d.history.at(i);
            h += QStringLiteral("<tr><td align=\"center\" style=\"font-size:8pt\">%1</td>"
                                "<td align=\"center\" style=\"font-size:8pt\">%2</td>"
                                "<td align=\"center\" style=\"font-size:8pt\">%3</td>"
                                "<td align=\"center\" style=\"font-size:8pt\">%4</td>"
                                "<td align=\"center\" style=\"font-size:8pt\">%5</td></tr>")
                     .arg(esc(r.timestamp), esc(r.callsign), azimuthCell(r.azimuth, r.uncertainty, r.magnetic, r.declination),
                          esc(r.signal.isEmpty() ? QStringLiteral("S5") : r.signal), dmsLines(r.lat, r.lon));
        }
        h += QStringLiteral("</table>");
        if (d.history.size() > 20) {
            h += QStringLiteral("<p style=\"font-size:10pt\"><i>%1</i></p>")
                     .arg(esc(SATER_TR("... and %1 more records").arg(d.history.size() - 20)));
        }
    }

    h += QStringLiteral("<p style=\"font-size:4pt\">&nbsp;</p><p style=\"font-size:4pt\">&nbsp;</p>");
    h += separator();
    const QString org = d.organization.trimmed();
    QString footer = SATER_TR("Report generated by SATER Map v%1 - %2")
                         .arg(d.appVersion, QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
    if (!org.isEmpty()) footer = org + QStringLiteral(" - ") + footer;
    h += QStringLiteral("<p align=\"center\" style=\"font-size:8pt; color:#95a5a6; margin-top:8px\">%1</p>").arg(esc(footer));
    h += QStringLiteral("</body></html>");
    return h;
}

namespace {

QByteArray xmpMetadata(const ReportData &d)
{
    auto x = [](const QString &s) { return s.toHtmlEscaped(); };
    QString bag;
    for (const QString &k : d.keywords.split(QLatin1Char(','), Qt::SkipEmptyParts))
        bag += QStringLiteral("<rdf:li>%1</rdf:li>").arg(x(k.trimmed()));
    // The packet header carries a byte order mark (U+FEFF) by definition.
    const QString xmp = QStringLiteral("<?xpacket begin=\"") + QChar(0xFEFF) + QStringLiteral(
        "\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
        " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "  <rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "    xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">\n"
        "   <dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">%1</rdf:li></rdf:Alt></dc:title>\n"
        "   <dc:creator><rdf:Seq><rdf:li>SATER Map</rdf:li></rdf:Seq></dc:creator>\n"
        "   <dc:description><rdf:Alt><rdf:li xml:lang=\"x-default\">%2</rdf:li></rdf:Alt></dc:description>\n"
        "   <dc:subject><rdf:Bag>%3</rdf:Bag></dc:subject>\n"
        "   <pdf:Keywords>%4</pdf:Keywords>\n"
        "   <pdf:Producer>SATER Map %5</pdf:Producer>\n"
        "   <xmp:CreatorTool>SATER Map</xmp:CreatorTool>\n"
        "  </rdf:Description>\n"
        " </rdf:RDF>\n"
        "</x:xmpmeta>\n"
        "<?xpacket end=\"w\"?>")
                            .arg(x(d.title), x(d.subject), bag, x(d.keywords), x(d.appVersion));
    return xmp.toUtf8();
}

} // namespace

bool writeReport(const ReportData &data, QIODevice *device, QString *error)
{
    if (!device || !device->isWritable()) {
        if (error) *error = SATER_TR("The file cannot be written.");
        return false;
    }

    QPdfWriter writer(device);
    writer.setResolution(kDpi);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 10, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(data.title);
    writer.setCreator(QStringLiteral("SATER Map"));
    writer.setDocumentXmpMetadata(xmpMetadata(data));

    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&writer);
    QFont font = doc.defaultFont();
    font.setPointSizeF(10);
    doc.setDefaultFont(font);
    doc.setDocumentMargin(0);
    const QRect paint = writer.pageLayout().paintRectPixels(kDpi);
    doc.setPageSize(QSizeF(paint.width(), paint.height()));
    if (!data.logo.isNull()) doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("sater://logo")), data.logo);
    if (!data.mapImage.isNull()) doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("sater://map")), data.mapImage);
    doc.setHtml(reportHtml(data));
    doc.print(&writer);
    return true;
}

} // namespace sater
