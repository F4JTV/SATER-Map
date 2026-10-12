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
#include <QBuffer>
#include <QDateTime>
#include <QFont>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QUrl>

#include <memory>

namespace sater {

namespace {

// The report is laid out at 96 dpi: 1 cm = 37.8 units.
constexpr int kDpi = 96;
double cm(double v) { return v / 2.54 * kDpi; }

QString esc(const QString &s) { return s.toHtmlEscaped(); }

const char *kOrange = "#e65c00";
const char *kBlue = "#003d80";

// One element of a part: text (HTML), a table that may be cut between its
// rows when it is taller than a page, a picture, the emblem, the rule.
struct Element {
    enum Kind { Html, Table, Image, Rule } kind = Html;
    QString html;                       // Html; Table: what precedes the rows
    QString header;                     // Table: the header row, on every piece
    QStringList rows;                   // Table
    QString close = QStringLiteral("</table>");
    QImage image;                       // Image
    QSizeF maxSize;                     // Image, units
};

// A part of the report: a heading and what it introduces, kept on one page.
struct Part {
    QString name;
    QList<Element> elements;
};

Element html(const QString &h) { Element e; e.html = h; return e; }

QString heading(const QString &title)
{
    return QStringLiteral("<p style=\"font-size:14pt; color:%1; margin-bottom:6px\"><b>%2</b></p>")
        .arg(QLatin1String(kOrange), esc(title));
}

QString tableOpen(const QString &width, int padding, const QString &grid)
{
    // Widths in percent of the text width: a length in pixels is scaled by
    // the screen's resolution (by 4/3 on Android, past the right margin).
    return QStringLiteral("<table width=\"%1\" cellspacing=\"0\" cellpadding=\"%2\" border=\"1\" "
                          "style=\"border-collapse:collapse; border-style:solid; border-color:%3\">")
        .arg(width).arg(padding).arg(grid);
}

// A two-column key/value table: bold keys on a tinted background.
Element keyValueTable(const QList<QPair<QString, QString>> &rows, const QString &keyBg, const QString &grid)
{
    Element e;
    e.kind = Element::Table;
    e.html = tableOpen(QStringLiteral("100%"), 6, grid);
    for (const auto &r : rows) {
        e.rows << QStringLiteral("<tr><td width=\"34%\" bgcolor=\"%1\" style=\"font-size:10pt\"><b>%2</b></td>"
                                 "<td width=\"66%\" style=\"font-size:10pt\">%3</td></tr>")
                      .arg(keyBg, esc(r.first), esc(r.second));
    }
    return e;
}

QString headerRow(const QStringList &cells, const QList<int> &percents, const QString &bg, double fontPt)
{
    QString h = QStringLiteral("<tr>");
    for (int i = 0; i < cells.size(); ++i) {
        h += QStringLiteral("<td width=\"%1%\" bgcolor=\"%2\" align=\"center\" style=\"font-size:%3pt; color:#ffffff\"><b>%4</b></td>")
                 .arg(percents.value(i, 20))
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

QList<Part> reportParts(const ReportData &d)
{
    QList<Part> parts;

    // The emblem, the rule, the title.
    {
        Part p;
        p.name = QStringLiteral("Title");
        if (!d.logo.isNull()) {
            Element logo;
            logo.kind = Element::Image;
            logo.image = d.logo;
            logo.maxSize = QSizeF(cm(4), cm(4));
            p.elements << logo;
        }
        Element rule;
        rule.kind = Element::Rule;
        p.elements << rule;
        const QString title = d.title.isEmpty() ? SATER_TR("SATER mission report") : d.title;
        const QString subject = d.subject.isEmpty() ? SATER_TR("Distress beacon search") : d.subject;
        p.elements << html(QStringLiteral("<p align=\"center\" style=\"font-size:20pt; color:#2c3e50; margin-top:12px; margin-bottom:8px\"><b>%1</b></p>"
                                          "<p align=\"center\" style=\"font-size:12pt; color:#7f8c8d\">%2</p>")
                               .arg(esc(title.toUpper()), esc(subject)));
        parts << p;
    }

    if (!d.comment.trimmed().isEmpty()) {
        QString c = esc(d.comment.trimmed());
        c.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        parts << Part{SATER_TR("Comments / Notes"),
                      {html(heading(SATER_TR("Comments / Notes"))
                            + QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"8\" border=\"0\" bgcolor=\"#f5f5f5\">"
                                             "<tr><td style=\"font-size:10pt\">%1</td></tr></table>").arg(c))}};
    }

    {
        Element t = keyValueTable({{SATER_TR("Start date:"), d.startDate},
                                   {SATER_TR("Start time:"), d.startTime},
                                   {SATER_TR("Duration:"), d.duration},
                                   {SATER_TR("Number of stations:"), QString::number(d.stationCount)},
                                   {SATER_TR("Number of records:"), QString::number(d.recordCount)}},
                                  QStringLiteral("#fff3e6"), QLatin1String(kOrange));
        t.html.prepend(heading(SATER_TR("Mission information")));
        parts << Part{SATER_TR("Mission information"), {t}};
    }

    if (!d.stations.isEmpty()) {
        const QList<int> w = {18, 36, 32, 14};
        Element t;
        t.kind = Element::Table;
        t.html = heading(SATER_TR("Direction-finding stations")) + tableOpen(QStringLiteral("100%"), 5, QStringLiteral("#bdc3c7"));
        t.header = headerRow({SATER_TR("Callsign"), SATER_TR("Position (DMS)"), SATER_TR("Azimuth"), SATER_TR("Signal")},
                             w, QLatin1String(kOrange), 9);
        for (const Station &s : d.stations) {
            t.rows << QStringLiteral("<tr><td width=\"%1%\" style=\"font-size:9pt\">%2</td>"
                                     "<td width=\"%3%\" style=\"font-size:9pt\">%4</td>"
                                     "<td width=\"%5%\" align=\"center\" style=\"font-size:9pt\">%6</td>"
                                     "<td width=\"%7%\" align=\"center\" style=\"font-size:9pt\">%8</td></tr>")
                          .arg(w[0])
                          .arg(esc(s.callsign))
                          .arg(w[1])
                          .arg(dmsLines(s.lat, s.lon))
                          .arg(w[2])
                          .arg(azimuthCell(s.azimuth, s.uncertainty, s.magnetic, s.declination))
                          .arg(w[3])
                          .arg(esc(s.signal.isEmpty() ? QStringLiteral("S5") : s.signal));
        }
        parts << Part{SATER_TR("Direction-finding stations"), {t}};
    }

    if (!d.kilometers.isEmpty()) {
        Element t;
        t.kind = Element::Table;
        t.html = heading(SATER_TR("Distance driven")) + tableOpen(QStringLiteral("60%"), 5, QStringLiteral("#27ae60"));
        t.header = headerRow({SATER_TR("Station"), SATER_TR("Distance")}, {60, 40}, QStringLiteral("#27ae60"), 10);
        double total = 0;
        for (const auto &p : d.kilometers) {
            t.rows << QStringLiteral("<tr><td width=\"60%\" style=\"font-size:10pt\">%1</td>"
                                     "<td width=\"40%\" align=\"right\" style=\"font-size:10pt\">%2 km</td></tr>")
                          .arg(esc(p.first))
                          .arg(p.second, 0, 'f', 1);
            total += p.second;
        }
        t.rows << QStringLiteral("<tr><td bgcolor=\"#e8f8f0\" style=\"font-size:10pt\"><b>%1</b></td>"
                                 "<td bgcolor=\"#e8f8f0\" align=\"right\" style=\"font-size:10pt\"><b>%2 km</b></td></tr>")
                      .arg(esc(SATER_TR("TOTAL")))
                      .arg(total, 0, 'f', 1);
        parts << Part{SATER_TR("Distance driven"), {t}};
    }

    if (d.estimate) {
        const auto &e = *d.estimate;
        Element t = keyValueTable({{SATER_TR("Coordinates (DD):"), QStringLiteral("%1, %2").arg(e.lat, 0, 'f', 6).arg(e.lon, 0, 'f', 6)},
                                   {SATER_TR("Coordinates (DMS):"), geo::ddToDmsString(e.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(e.lon, false)},
                                   {SATER_TR("MGRS:"), e.mgrs},
                                   {SATER_TR("95 % ellipse:"), SATER_TR("%1 × %2 km, axis %3°").arg(e.major95Km, 0, 'f', 2)
                                                                    .arg(e.minor95Km, 0, 'f', 2).arg(axisDegrees(e.axisDeg))},
                                   {SATER_TR("50 % ellipse:"), QStringLiteral("%1 × %2 km").arg(e.major50Km, 0, 'f', 2).arg(e.minor50Km, 0, 'f', 2)},
                                   {SATER_TR("Bearings:"), QString::number(e.bearingCount)},
                                   {SATER_TR("Agreement:"), e.agreement}},
                                  QStringLiteral("#f8e6ff"), QStringLiteral("#8e24aa"));
        t.html.prepend(heading(SATER_TR("Best estimate")));
        Element note = html(QStringLiteral("<p style=\"font-size:8pt; color:#555555; margin-top:6px\">%1</p>")
                                .arg(esc(SATER_TR("Least-squares position of the bearings, each weighted by its uncertainty (the cone taken "
                                                  "as a 95 % interval). If the bearings are free of gross errors, the beacon lies inside the "
                                                  "95 % ellipse about 95 times out of 100 (somewhat less with only two bearings crossing at "
                                                  "a sharp angle). Bearings read from magnetic north are corrected by the %1 declination.")
                                             .arg(QString::fromLatin1(geomag::kModelName)))));
        parts << Part{SATER_TR("Best estimate"), {t, note}};
    }

    if (d.zone) {
        const auto &z = *d.zone;
        Element t = keyValueTable({{SATER_TR("Centre (DD):"), QStringLiteral("%1, %2").arg(z.lat, 0, 'f', 6).arg(z.lon, 0, 'f', 6)},
                                   {SATER_TR("Centre (DMS):"), geo::ddToDmsString(z.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(z.lon, false)},
                                   {SATER_TR("MGRS:"), z.mgrs.isEmpty() ? QStringLiteral("N/A") : z.mgrs},
                                   {SATER_TR("Radius:"), QStringLiteral("%1 km").arg(z.radiusKm, 0, 'f', 2)},
                                   {SATER_TR("Surface:"), QStringLiteral("%1 km²").arg(z.surfaceKm2, 0, 'f', 2)},
                                   {SATER_TR("Intersection points:"), QString::number(z.pointCount)}},
                                  QStringLiteral("#e6f3ff"), QLatin1String(kBlue));
        t.html.prepend(heading(SATER_TR("Intersection zone")));
        parts << Part{SATER_TR("Intersection zone"), {t}};
    }

    if (d.beacon) {
        const auto &b = *d.beacon;
        Element t = keyValueTable({{SATER_TR("Coordinates (DD):"), QStringLiteral("%1, %2").arg(b.lat, 0, 'f', 6).arg(b.lon, 0, 'f', 6)},
                                   {SATER_TR("Coordinates (DMS):"), geo::ddToDmsString(b.lat, true) + QLatin1Char(' ') + geo::ddToDmsString(b.lon, false)},
                                   {SATER_TR("MGRS:"), b.mgrs.isEmpty() ? QStringLiteral("N/A") : b.mgrs}},
                                  QStringLiteral("#ffe6e6"), QStringLiteral("#ff0000"));
        t.html.prepend(heading(SATER_TR("Distress beacon position")));
        parts << Part{SATER_TR("Distress beacon position"), {t}};
    }

    if (!d.mapImage.isNull()) {
        Element map;
        map.kind = Element::Image;
        map.image = d.mapImage;
        // At most 17 x 12 cm, aspect kept (as the Python report).
        map.maxSize = QSizeF(cm(17), cm(12));
        parts << Part{SATER_TR("Mission map"), {html(heading(SATER_TR("Mission map"))), map}};
    }

    if (!d.history.isEmpty()) {
        const QList<int> w = {21, 14, 25, 10, 30};
        Element t;
        t.kind = Element::Table;
        t.html = heading(SATER_TR("Bearing history")) + tableOpen(QStringLiteral("100%"), 4, QStringLiteral("#bdc3c7"));
        t.header = headerRow({SATER_TR("Date/Time"), SATER_TR("Station"), SATER_TR("Azimuth"), SATER_TR("Signal"), SATER_TR("Position")},
                             w, QLatin1String(kBlue), 8);
        const int from = std::max(0, int(d.history.size()) - 20);
        for (int i = from; i < d.history.size(); ++i) {
            const AzimuthRecord &r = d.history.at(i);
            t.rows << QStringLiteral("<tr><td width=\"%1%\" align=\"center\" style=\"font-size:8pt\">%2</td>"
                                     "<td width=\"%3%\" align=\"center\" style=\"font-size:8pt\">%4</td>"
                                     "<td width=\"%5%\" align=\"center\" style=\"font-size:8pt\">%6</td>"
                                     "<td width=\"%7%\" align=\"center\" style=\"font-size:8pt\">%8</td>"
                                     "<td width=\"%9%\" align=\"center\" style=\"font-size:8pt\">%10</td></tr>")
                          .arg(w[0]).arg(esc(r.timestamp))
                          .arg(w[1]).arg(esc(r.callsign))
                          .arg(w[2]).arg(azimuthCell(r.azimuth, r.uncertainty, r.magnetic, r.declination))
                          .arg(w[3]).arg(esc(r.signal.isEmpty() ? QStringLiteral("S5") : r.signal))
                          .arg(w[4]).arg(dmsLines(r.lat, r.lon));
        }
        Part p{SATER_TR("Bearing history"), {t}};
        if (d.history.size() > 20) {
            p.elements << html(QStringLiteral("<p style=\"font-size:10pt; margin-top:4px\"><i>%1</i></p>")
                                   .arg(esc(SATER_TR("... and %1 more records").arg(d.history.size() - 20))));
        }
        parts << p;
    }

    {
        Part p;
        p.name = QStringLiteral("Footer");
        Element rule;
        rule.kind = Element::Rule;
        p.elements << rule;
        const QString org = d.organization.trimmed();
        QString footer = SATER_TR("Report generated by SATER Map v%1 - %2")
                             .arg(d.appVersion, QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
        if (!org.isEmpty()) footer = org + QStringLiteral(" - ") + footer;
        p.elements << html(QStringLiteral("<p align=\"center\" style=\"font-size:8pt; color:#95a5a6; margin-top:6px\">%1</p>").arg(esc(footer)));
        parts << p;
    }
    return parts;
}

// What lands on a page: a block of text (a table, or a piece of one), a
// picture or the rule, at y in the page's text area.
struct Placed {
    int page = 0;
    double y = 0;
    double height = 0;
    QString name;
    std::shared_ptr<QTextDocument> doc;
    QImage image;
    QRectF imageRect;
    bool rule = false;
};

class Layout
{
public:
    // device: the PDF writer (its fonts), width and height of the text area.
    Layout(QPaintDevice *device, double width, double height)
        : m_device(device), m_width(width), m_height(height)
    {
    }

    QList<Placed> run(const QList<Part> &parts)
    {
        for (const Part &part : parts) place(part);
        return m_placed;
    }
    int pages() const { return m_page + 1; }

private:
    std::shared_ptr<QTextDocument> document(const QString &body) const
    {
        auto doc = std::make_shared<QTextDocument>();
        doc->documentLayout()->setPaintDevice(m_device);
        QFont font = doc->defaultFont();
        font.setPointSizeF(10);
        doc->setDefaultFont(font);
        doc->setDocumentMargin(0);
        doc->setTextWidth(m_width);
        doc->setHtml(QStringLiteral("<html><body>") + body + QStringLiteral("</body></html>"));
        return doc;
    }

    std::shared_ptr<QTextDocument> table(const Element &e, int from, int to, bool withLead) const
    {
        QString h = withLead ? e.html : e.html.mid(e.html.indexOf(QStringLiteral("<table")));
        h += e.header;
        for (int i = from; i < to; ++i) h += e.rows.at(i);
        return document(h + e.close);
    }

    static constexpr double kRuleHeight = 5;

    QSizeF imageSize(const Element &e, double room) const
    {
        const double iw = e.image.width(), ih = e.image.height();
        double w = std::min(e.maxSize.width(), m_width), h = std::min(e.maxSize.height(), room);
        if (iw / ih > w / h) h = w * ih / iw;
        else w = h * iw / ih;
        return QSizeF(w, h);
    }

    // Height of an element laid out whole.
    double height(const Element &e, std::shared_ptr<QTextDocument> *doc) const
    {
        switch (e.kind) {
        case Element::Rule: return kRuleHeight;
        case Element::Image: return imageSize(e, m_height).height();
        case Element::Html: *doc = document(e.html); return (*doc)->size().height();
        case Element::Table: *doc = table(e, 0, int(e.rows.size()), true); return (*doc)->size().height();
        }
        return 0;
    }

    void newPage()
    {
        ++m_page;
        m_y = 0;
    }

    void put(const QString &name, const Element &e, double h, std::shared_ptr<QTextDocument> doc)
    {
        Placed p;
        p.page = m_page;
        p.y = m_y;
        p.height = h;
        p.name = name;
        if (e.kind == Element::Rule) {
            p.rule = true;
        } else if (e.kind == Element::Image) {
            const QSizeF s = imageSize(e, std::min(m_height, h));
            p.image = e.image;
            p.imageRect = QRectF((m_width - s.width()) / 2, m_y, s.width(), s.height());
        } else {
            p.doc = std::move(doc);
        }
        m_placed << p;
        m_y += h;
    }

    void place(const Part &part)
    {
        // The part whole: on this page if it fits, else at the top of the next.
        QList<std::shared_ptr<QTextDocument>> docs;
        QList<double> heights;
        double total = 0;
        for (const Element &e : part.elements) {
            std::shared_ptr<QTextDocument> doc;
            heights << height(e, &doc);
            docs << doc;
            total += heights.last();
        }
        const double gap = m_y > 0 ? cm(0.45) : 0;
        // A part taller than a page is cut anyway: it starts here.
        if (m_y > 0 && m_y + gap + total > m_height && total <= m_height) newPage();
        else m_y += gap;
        if (total <= m_height - m_y) {
            for (int i = 0; i < part.elements.size(); ++i) put(part.name, part.elements.at(i), heights.at(i), docs.at(i));
            return;
        }
        // Taller than a page: its tables are cut between rows, the header
        // row repeated on each page; nothing else is ever cut.
        for (int i = 0; i < part.elements.size(); ++i) {
            const Element &e = part.elements.at(i);
            if (e.kind != Element::Table) {
                if (m_y > 0 && m_y + heights.at(i) > m_height) newPage();
                put(part.name, e, heights.at(i), docs.at(i));
                continue;
            }
            int from = 0;
            bool lead = true;
            while (from < e.rows.size()) {
                // As many rows as fit here (at least one on an empty page).
                int to = from + 1;
                std::shared_ptr<QTextDocument> best = table(e, from, to, lead);
                if (m_y > 0 && m_y + best->size().height() > m_height) {
                    newPage();
                    continue;
                }
                while (to < e.rows.size()) {
                    auto more = table(e, from, to + 1, lead);
                    if (m_y + more->size().height() > m_height) break;
                    best = more;
                    ++to;
                }
                put(part.name, e, best->size().height(), best);
                from = to;
                lead = false;
                if (from < e.rows.size()) newPage();
            }
        }
    }

    QPaintDevice *m_device;
    double m_width;
    double m_height;
    int m_page = 0;
    double m_y = 0;
    QList<Placed> m_placed;
};

// The text area of the writer's pages, less the band of the page numbers.
QRectF textArea(QPdfWriter &writer)
{
    const QRect paint = writer.pageLayout().paintRectPixels(kDpi);
    return QRectF(0, 0, paint.width(), paint.height() - cm(0.8));
}

void setUp(QPdfWriter &writer)
{
    writer.setResolution(kDpi);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 10, 15, 12), QPageLayout::Millimeter);
}

} // namespace

QString reportHtml(const ReportData &d)
{
    QString h = QStringLiteral("<html><body>");
    for (const Part &p : reportParts(d)) {
        for (const Element &e : p.elements) {
            if (e.kind == Element::Html) h += e.html;
            else if (e.kind == Element::Table) h += e.html + e.header + e.rows.join(QString()) + e.close;
        }
    }
    return h + QStringLiteral("</body></html>");
}

QList<ReportPiece> reportLayout(const ReportData &data, double *pageHeight)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buffer);
    setUp(writer);
    const QRectF area = textArea(writer);
    if (pageHeight) *pageHeight = area.height();
    Layout layout(&writer, area.width(), area.height());
    QList<ReportPiece> pieces;
    for (const Placed &p : layout.run(reportParts(data))) pieces.append({p.name, p.page, p.y, p.height});
    return pieces;
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
    setUp(writer);
    writer.setTitle(data.title);
    writer.setCreator(QStringLiteral("SATER Map"));
    writer.setDocumentXmpMetadata(xmpMetadata(data));

    const QRectF area = textArea(writer);
    Layout layout(&writer, area.width(), area.height());
    const QList<Placed> placed = layout.run(reportParts(data));
    const int pages = layout.pages();

    QPainter painter;
    if (!painter.begin(&writer)) {
        if (error) *error = SATER_TR("The file cannot be written.");
        return false;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QFont pageFont = painter.font();
    pageFont.setPointSizeF(8);
    int page = 0;
    const auto pageNumber = [&]() {
        painter.save();
        painter.setFont(pageFont);
        painter.setPen(QColor(0x95, 0xa5, 0xa6));
        const QRectF band(0, area.height(), area.width(), cm(0.8));
        painter.drawText(band, Qt::AlignRight | Qt::AlignBottom, SATER_TR("Page %1 of %2").arg(page + 1).arg(pages));
        painter.restore();
    };
    for (const Placed &p : placed) {
        while (page < p.page) {
            pageNumber();
            writer.newPage();
            ++page;
        }
        if (p.rule) {
            // The orange/blue rule of the Python report (9 cm + 8 cm).
            const double w = std::min(cm(17), area.width()), x = (area.width() - w) / 2;
            painter.fillRect(QRectF(x, p.y, w * 9 / 17, p.height), QColor(QLatin1String(kOrange)));
            painter.fillRect(QRectF(x + w * 9 / 17, p.y, w * 8 / 17, p.height), QColor(QLatin1String(kBlue)));
        } else if (!p.image.isNull()) {
            painter.drawImage(p.imageRect, p.image);
        } else if (p.doc) {
            painter.save();
            painter.translate(0, p.y);
            p.doc->drawContents(&painter, QRectF(0, 0, area.width(), p.height + 1));
            painter.restore();
        }
    }
    pageNumber();
    painter.end();
    return true;
}

} // namespace sater
