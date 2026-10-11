/*
 * Exporters.cpp - KML and stand-alone HTML (Leaflet) exports.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Exporters.h"

#include "Geo.h"
#include "Geomag.h"
#include "TileProviders.h"
#include "Tr.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>

namespace sater {

QString pyNumber(double v)
{
    QString s = QString::number(v, 'g', QLocale::FloatingPointShortest);
    // Python keeps large/small numbers in plain notation down to 1e-4 and
    // up to 1e16; we only print coordinates and distances, which never
    // reach the exponent form, but guard against it anyway.
    if (s.contains(QLatin1Char('e'))) s = QString::number(v, 'f', 8);
    return s;
}

namespace {

QString xml(const QString &s)
{
    return s.toHtmlEscaped();
}

// "#rrggbb" to KML's aabbggrr.
QString kmlColor(const QString &hex, const QString &alpha = QStringLiteral("ff"))
{
    QString h = hex.trimmed();
    if (h.startsWith(QLatin1Char('#'))) h = h.mid(1);
    if (h.size() < 6) h = h.leftJustified(6, QLatin1Char('0'));
    const QString r = h.mid(0, 2), g = h.mid(2, 2), b = h.mid(4, 2);
    return (alpha + b + g + r).toLower();
}

QString jsString(const QString &s)
{
    // A JSON string is a valid JavaScript string literal.
    const QByteArray j = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(j.mid(1, j.size() - 2));
}

} // namespace

QByteArray exportKml(const ExportContext &ctx)
{
    QString kml;
    kml += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
        "<Document>\n");
    kml += QStringLiteral("<name>%1</name>\n").arg(xml(SATER_TR("SATER - Search mission")));
    kml += QStringLiteral("<description>%1</description>\n\n").arg(xml(SATER_TR("SATER Map export")));
    kml += QStringLiteral(
        "<!-- Station style -->\n"
        "<Style id=\"stationStyle\">\n"
        "    <IconStyle>\n"
        "        <color>ff0000ff</color>\n"
        "        <scale>1.0</scale>\n"
        "        <Icon><href>http://maps.google.com/mapfiles/kml/paddle/red-circle.png</href></Icon>\n"
        "    </IconStyle>\n"
        "    <LabelStyle>\n"
        "        <scale>0.8</scale>\n"
        "    </LabelStyle>\n"
        "</Style>\n\n"
        "<!-- Beacon style -->\n"
        "<Style id=\"beaconStyle\">\n"
        "    <IconStyle>\n"
        "        <color>ff0000ff</color>\n"
        "        <scale>1.5</scale>\n"
        "        <Icon><href>http://maps.google.com/mapfiles/kml/shapes/target.png</href></Icon>\n"
        "    </IconStyle>\n"
        "    <LabelStyle>\n"
        "        <color>ff0000ff</color>\n"
        "        <scale>1.0</scale>\n"
        "    </LabelStyle>\n"
        "</Style>\n\n"
        "<!-- Bearing style -->\n"
        "<Style id=\"azimuthStyle\">\n"
        "    <LineStyle>\n"
        "        <color>ff0000ff</color>\n"
        "        <width>2</width>\n"
        "    </LineStyle>\n"
        "    <PolyStyle>\n"
        "        <color>400000ff</color>\n"
        "    </PolyStyle>\n"
        "</Style>\n\n");

    kml += QStringLiteral("<Folder>\n<name>%1</name>\n").arg(xml(SATER_TR("Stations")));
    const double len = ctx.azimuthLengthKm;
    for (const Station &s : ctx.stations) {
        if (!s.isValid() || !s.visible) continue;
        const QString color = kmlColor(s.color);
        const QString signal = s.signal.isEmpty() ? QStringLiteral("S5") : s.signal;

        // From true north; a compass reading is said as read too.
        const double azimuth = s.trueAzimuth();
        const QString azText = s.magnetic ? QString::number(azimuth, 'f', 1) : pyNumber(azimuth);
        const QString readText = s.magnetic ? QStringLiteral("\n") + SATER_TR("Read on a compass: %1° magnetic, declination %2")
                                                                      .arg(pyNumber(s.azimuth), geomag::declinationText(s.declination))
                                            : QString();
        kml += QStringLiteral("<Placemark>\n<name>%1</name>\n<description>%2</description>\n")
                   .arg(xml(s.callsign),
                        xml(SATER_TR("Signal: %1\nAzimuth: %2° ±%3°\nPosition: %4, %5")
                                .arg(signal, azText, pyNumber(s.uncertainty))
                                .arg(s.lat, 0, 'f', 6)
                                .arg(s.lon, 0, 'f', 6)
                            + readText));
        kml += QStringLiteral(
                   "<Style>\n"
                   "    <IconStyle>\n"
                   "        <color>%1</color>\n"
                   "        <scale>1.0</scale>\n"
                   "        <Icon><href>http://maps.google.com/mapfiles/kml/paddle/wht-circle.png</href></Icon>\n"
                   "    </IconStyle>\n"
                   "    <LabelStyle>\n"
                   "        <color>%1</color>\n"
                   "        <scale>0.8</scale>\n"
                   "    </LabelStyle>\n"
                   "</Style>\n"
                   "<Point><coordinates>%2,%3,0</coordinates></Point>\n"
                   "</Placemark>\n")
                   .arg(color, pyNumber(s.lon), pyNumber(s.lat));

        if (!s.hasBearing()) continue;
        const QPointF end = geo::endpoint(s.lat, s.lon, azimuth, len);
        kml += QStringLiteral("<Placemark>\n<name>%1</name>\n<description>%2</description>\n")
                   .arg(xml(SATER_TR("%1 - Azimuth %2°").arg(s.callsign, azText)),
                        xml(SATER_TR("Azimuth: %1° ±%2°").arg(azText, pyNumber(s.uncertainty)) + readText));
        kml += QStringLiteral(
                   "<Style>\n"
                   "    <LineStyle>\n"
                   "        <color>%1</color>\n"
                   "        <width>2</width>\n"
                   "    </LineStyle>\n"
                   "</Style>\n"
                   "<LineString>\n"
                   "<tessellate>1</tessellate>\n"
                   "<coordinates>%2,%3,0 %4,%5,0</coordinates>\n"
                   "</LineString>\n"
                   "</Placemark>\n")
                   .arg(color, pyNumber(s.lon), pyNumber(s.lat), pyNumber(end.y()), pyNumber(end.x()));

        if (s.uncertainty > 0) {
            const QPointF l = geo::endpoint(s.lat, s.lon, azimuth - s.uncertainty, len);
            const QPointF r = geo::endpoint(s.lat, s.lon, azimuth + s.uncertainty, len);
            kml += QStringLiteral("<Placemark>\n<name>%1</name>\n")
                       .arg(xml(SATER_TR("%1 - Uncertainty").arg(s.callsign)));
            kml += QStringLiteral(
                       "<Style>\n"
                       "    <LineStyle>\n"
                       "        <color>%1</color>\n"
                       "        <width>1</width>\n"
                       "    </LineStyle>\n"
                       "    <PolyStyle>\n"
                       "        <color>%2</color>\n"
                       "    </PolyStyle>\n"
                       "</Style>\n"
                       "<Polygon>\n"
                       "<tessellate>1</tessellate>\n"
                       "<outerBoundaryIs><LinearRing>\n"
                       "<coordinates>%3,%4,0 %5,%6,0 %7,%8,0 %3,%4,0</coordinates>\n"
                       "</LinearRing></outerBoundaryIs>\n"
                       "</Polygon>\n"
                       "</Placemark>\n")
                       .arg(kmlColor(s.color, QStringLiteral("80")), kmlColor(s.color, QStringLiteral("40")),
                            pyNumber(s.lon), pyNumber(s.lat), pyNumber(l.y()), pyNumber(l.x()),
                            pyNumber(r.y()), pyNumber(r.x()));
        }
    }
    kml += QStringLiteral("</Folder>\n");

    if (ctx.beacon) {
        const double bl = ctx.beacon->first, bn = ctx.beacon->second;
        kml += QStringLiteral("<Folder>\n<name>%1</name>\n<Placemark>\n<name>%2</name>\n<description>%3</description>\n")
                   .arg(xml(SATER_TR("Distress beacon")), xml(SATER_TR("🎯 BEACON")),
                        xml(SATER_TR("Estimated position of the distress beacon\nCoordinates: %1, %2")
                                .arg(bl, 0, 'f', 6)
                                .arg(bn, 0, 'f', 6)));
        kml += QStringLiteral(
                   "<Style>\n"
                   "    <IconStyle>\n"
                   "        <color>ff0000ff</color>\n"
                   "        <scale>1.5</scale>\n"
                   "        <Icon><href>http://maps.google.com/mapfiles/kml/shapes/target.png</href></Icon>\n"
                   "    </IconStyle>\n"
                   "    <LabelStyle>\n"
                   "        <color>ff0000ff</color>\n"
                   "        <scale>1.2</scale>\n"
                   "    </LabelStyle>\n"
                   "</Style>\n"
                   "<Point><coordinates>%1,%2,0</coordinates></Point>\n"
                   "</Placemark>\n"
                   "</Folder>\n")
                   .arg(pyNumber(bn), pyNumber(bl));
    }

    if (ctx.zone.circle.valid) {
        const geo::Circle &c = ctx.zone.circle;
        kml += QStringLiteral("<Folder>\n<name>%1</name>\n<Placemark>\n<name>%2</name>\n<description>%3</description>\n")
                   .arg(xml(SATER_TR("Intersection zone")), xml(SATER_TR("Zone centre")),
                        xml(SATER_TR("Radius: %1 km").arg(c.radiusKm, 0, 'f', 2)));
        kml += QStringLiteral(
                   "<Style>\n"
                   "    <IconStyle>\n"
                   "        <color>%1</color>\n"
                   "        <scale>1.2</scale>\n"
                   "        <Icon><href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href></Icon>\n"
                   "    </IconStyle>\n"
                   "</Style>\n"
                   "<Point><coordinates>%2,%3,0</coordinates></Point>\n"
                   "</Placemark>\n"
                   "</Folder>\n")
                   .arg(kmlColor(ctx.display.zoneBorderColor), pyNumber(c.lon), pyNumber(c.lat));
    }

    if (ctx.fix && ctx.fix->valid) {
        const FixResult &f = *ctx.fix;
        const QString color = kmlColor(ctx.display.estimateColor);
        QString description = SATER_TR("Least squares on %1 bearings\nCoordinates: %2, %3\nMGRS: %4\n"
                                       "95 % ellipse: %5 × %6 km, axis %7°\n50 % ellipse: %8 × %9 km")
                                  .arg(f.degreesOfFreedom + 2)
                                  .arg(f.lat, 0, 'f', 6)
                                  .arg(f.lon, 0, 'f', 6)
                                  .arg(geo::toMgrs(f.lat, f.lon))
                                  .arg(2 * f.ellipse95.semiMajorKm, 0, 'f', 2)
                                  .arg(2 * f.ellipse95.semiMinorKm, 0, 'f', 2)
                                  .arg(axisDegrees(f.ellipse95.orientationDeg))
                                  .arg(2 * f.ellipse50.semiMajorKm, 0, 'f', 2)
                                  .arg(2 * f.ellipse50.semiMinorKm, 0, 'f', 2);
        if (!ctx.fixNote.isEmpty()) description += QStringLiteral("\n\n") + ctx.fixNote;
        kml += QStringLiteral("<Folder>\n<name>%1</name>\n<Placemark>\n<name>%1</name>\n<description>%2</description>\n")
                   .arg(xml(SATER_TR("Best estimate")), xml(description));
        kml += QStringLiteral(
                   "<Style>\n"
                   "    <IconStyle>\n"
                   "        <color>%1</color>\n"
                   "        <scale>1.2</scale>\n"
                   "        <Icon><href>http://maps.google.com/mapfiles/kml/shapes/cross-hairs.png</href></Icon>\n"
                   "    </IconStyle>\n"
                   "</Style>\n"
                   "<Point><coordinates>%2,%3,0</coordinates></Point>\n"
                   "</Placemark>\n")
                   .arg(color, pyNumber(f.lon), pyNumber(f.lat));
        const struct { const ConfidenceEllipse *e; QString name; QString fill; } rings[] = {
            {&f.ellipse95, SATER_TR("95 % ellipse"), kmlColor(ctx.display.estimateColor, QStringLiteral("30"))},
            {&f.ellipse50, SATER_TR("50 % ellipse"), kmlColor(ctx.display.estimateColor, QStringLiteral("00"))},
        };
        for (const auto &ring : rings) {
            QStringList coords;
            const QList<QPointF> pts = ellipseOutline(f.lat, f.lon, *ring.e, 72);
            for (const QPointF &pt : pts) coords << QStringLiteral("%1,%2,0").arg(pyNumber(pt.y()), pyNumber(pt.x()));
            coords << coords.first();
            kml += QStringLiteral(
                       "<Placemark>\n<name>%1</name>\n"
                       "<Style>\n"
                       "    <LineStyle>\n"
                       "        <color>%2</color>\n"
                       "        <width>2</width>\n"
                       "    </LineStyle>\n"
                       "    <PolyStyle>\n"
                       "        <color>%3</color>\n"
                       "    </PolyStyle>\n"
                       "</Style>\n"
                       "<Polygon>\n"
                       "<tessellate>1</tessellate>\n"
                       "<outerBoundaryIs><LinearRing>\n"
                       "<coordinates>%4</coordinates>\n"
                       "</LinearRing></outerBoundaryIs>\n"
                       "</Polygon>\n"
                       "</Placemark>\n")
                       .arg(xml(ring.name), color, ring.fill, coords.join(QLatin1Char(' ')));
        }
        kml += QStringLiteral("</Folder>\n");
    }

    kml += QStringLiteral("</Document>\n</kml>");
    return kml.toUtf8();
}

QByteArray exportStandaloneHtml(const ExportContext &ctx)
{
    const TileProvider &provider = tileProvider(ctx.providerKey);

    QJsonArray stations;
    for (const Station &s : ctx.stations) {
        if (!s.isValid()) continue;
        QJsonObject o;
        o.insert(QStringLiteral("callsign"), s.callsign);
        o.insert(QStringLiteral("lat"), s.lat);
        o.insert(QStringLiteral("lon"), s.lon);
        // Drawn from true north; the popup says what was read.
        o.insert(QStringLiteral("azimuth"), s.trueAzimuth());
        o.insert(QStringLiteral("azText"), s.magnetic ? QStringLiteral("%1° (%2°M)").arg(QString::number(s.trueAzimuth(), 'f', 1), pyNumber(s.azimuth))
                                                      : pyNumber(s.azimuth) + QStringLiteral("°"));
        o.insert(QStringLiteral("uncertainty"), s.uncertainty);
        o.insert(QStringLiteral("color"), s.color);
        o.insert(QStringLiteral("visible"), s.visible);
        o.insert(QStringLiteral("signal"), s.signal);
        o.insert(QStringLiteral("notes"), s.notes);
        o.insert(QStringLiteral("station_id"), s.id);
        stations.append(o);
    }

    QString beaconJs = QStringLiteral("null");
    if (ctx.beacon) {
        beaconJs = QStringLiteral("{lat: %1, lon: %2}").arg(pyNumber(ctx.beacon->first), pyNumber(ctx.beacon->second));
    }
    QString zoneJs = QStringLiteral("null");
    if (ctx.zone.circle.valid) {
        QJsonArray pts;
        for (const QPointF &p : ctx.zone.points) pts.append(QJsonArray{p.x(), p.y()});
        zoneJs = QStringLiteral("{\n    centerLat: %1,\n    centerLon: %2,\n    radiusKm: %3,\n    points: %4,\n"
                                "    borderColor: %5,\n    fillColor: %6,\n    opacity: %7\n}")
                     .arg(pyNumber(ctx.zone.circle.lat), pyNumber(ctx.zone.circle.lon), pyNumber(ctx.zone.circle.radiusKm),
                          QString::fromUtf8(QJsonDocument(pts).toJson(QJsonDocument::Compact)),
                          jsString(ctx.display.zoneBorderColor), jsString(ctx.display.zoneFillColor),
                          pyNumber(ctx.display.zoneOpacity));
    }

    QString estimateJs = QStringLiteral("null");
    if (ctx.fix && ctx.fix->valid) {
        const FixResult &f = *ctx.fix;
        auto ring = [&](const ConfidenceEllipse &e) {
            QJsonArray pts;
            for (const QPointF &pt : ellipseOutline(f.lat, f.lon, e, 72)) pts.append(QJsonArray{pt.x(), pt.y()});
            return QString::fromUtf8(QJsonDocument(pts).toJson(QJsonDocument::Compact));
        };
        estimateJs = QStringLiteral("{\n    lat: %1,\n    lon: %2,\n    e95: %3,\n    e50: %4,\n    color: %5,\n"
                                    "    mgrs: %6,\n    ellipse: %7,\n    note: %8\n}")
                         .arg(pyNumber(f.lat), pyNumber(f.lon), ring(f.ellipse95), ring(f.ellipse50),
                              jsString(ctx.display.estimateColor), jsString(geo::toMgrs(f.lat, f.lon)),
                              jsString(SATER_TR("95 % ellipse: %1 × %2 km, axis %3°")
                                           .arg(2 * f.ellipse95.semiMajorKm, 0, 'f', 2)
                                           .arg(2 * f.ellipse95.semiMinorKm, 0, 'f', 2)
                                           .arg(axisDegrees(f.ellipse95.orientationDeg))),
                              jsString(ctx.fixNote));
    }

    const double cLat = ctx.hasView ? ctx.centerLat : 43.7;
    const double cLon = ctx.hasView ? ctx.centerLon : 7.25;
    const int zoom = ctx.hasView ? ctx.zoom : 10;

    QString subdomains;
    if (!provider.subdomains.isEmpty()) subdomains = QStringLiteral("    subdomains: %1,\n").arg(jsString(provider.subdomains.join(QString())));

    // Strings the page shows, translated here.
    QJsonObject t;
    t.insert(QStringLiteral("signal"), SATER_TR("Signal:"));
    t.insert(QStringLiteral("az"), SATER_TR("Az:"));
    t.insert(QStringLiteral("zoneCentre"), SATER_TR("Zone centre"));
    t.insert(QStringLiteral("radius"), SATER_TR("Radius:"));
    t.insert(QStringLiteral("zone"), SATER_TR("Zone:"));
    t.insert(QStringLiteral("beacon"), SATER_TR("🎯 DISTRESS BEACON"));
    t.insert(QStringLiteral("estimate"), SATER_TR("Best estimate"));
    const QString tJson = QString::fromUtf8(QJsonDocument(t).toJson(QJsonDocument::Compact));

    QString html;
    html += QStringLiteral(
        "<!DOCTYPE html>\n"
        "<html><head><meta charset=\"utf-8\">\n"
        "<title>SATER Map</title>\n"
        "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\"/>\n"
        "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>\n"
        "<style>\n"
        "html,body,#map{height:100%;margin:0}\n"
        ".info{position:absolute;top:10px;right:10px;z-index:1000;background:#fff;padding:10px;border-radius:5px;"
        "box-shadow:0 2px 10px rgba(0,0,0,.2);max-width:300px;font-family:sans-serif}\n"
        ".marker-label{\n"
        "    display:inline-block;\n"
        "    background:rgba(255,255,255,0.95);\n"
        "    border:2px solid;\n"
        "    border-radius:4px;\n"
        "    padding:4px 8px;\n"
        "    font-size:12px;\n"
        "    font-weight:bold;\n"
        "    white-space:nowrap;\n"
        "    line-height:1.2;\n"
        "    box-shadow: 0 2px 5px rgba(0,0,0,0.3);\n"
        "}\n"
        "@keyframes beacon-pulse {\n"
        "    0% { box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000; transform: scale(1); }\n"
        "    50% { box-shadow: 0 0 20px #ff0000, 0 0 40px #ff0000, 0 0 60px #ff3333; transform: scale(1.1); }\n"
        "    100% { box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000; transform: scale(1); }\n"
        "}\n"
        ".beacon-pulse {\n"
        "    animation: beacon-pulse 1.5s ease-in-out infinite;\n"
        "}\n"
        "</style>\n"
        "</head><body><div id=\"map\"></div>\n"
        "<div class=\"info\">\n"
        "<h3>&#128752;&#65039; SATER Map</h3>\n");
    html += QStringLiteral("<p><b>%1</b> %2<br><b>%3</b> %4</p>\n")
                .arg(SATER_TR("Mission:").toHtmlEscaped(), ctx.missionStartText.toHtmlEscaped(),
                     SATER_TR("Duration:").toHtmlEscaped(), ctx.missionDurationText.toHtmlEscaped());
    html += QStringLiteral("<div id=\"list\"></div>\n");
    html += QStringLiteral("<p><b>%1</b> %2</p>\n").arg(SATER_TR("Records:").toHtmlEscaped()).arg(ctx.recordCount);
    html += QStringLiteral("<div id=\"intersection-info\"></div>\n</div>\n<script>\n");
    html += QStringLiteral("var T=%1;\n").arg(tJson);
    html += QStringLiteral("var stations=%1;\n").arg(QString::fromUtf8(QJsonDocument(stations).toJson(QJsonDocument::Compact)));
    html += QStringLiteral("var len=%1;\n").arg(pyNumber(ctx.azimuthLengthKm));
    html += QStringLiteral("var beacon=%1;\n").arg(beaconJs);
    html += QStringLiteral("var intersection=%1;\n").arg(zoneJs);
    html += QStringLiteral("var estimate=%1;\n").arg(estimateJs);
    html += QStringLiteral("var map=L.map('map').setView([%1, %2], %3);\n").arg(pyNumber(cLat), pyNumber(cLon)).arg(zoom);
    html += QStringLiteral("L.tileLayer(%1, {\n    attribution: %2,\n    maxZoom: %3,\n%4}).addTo(map);\n")
                .arg(jsString(provider.urlTemplate), jsString(provider.attribution))
                .arg(provider.maxZoom)
                .arg(subdomains);
    if (!provider.labelsTemplate.isEmpty()) {
        // The place names of a background served without them.
        html += QStringLiteral("L.tileLayer(%1, {\n    maxZoom: %2,\n%3}).addTo(map);\n")
                    .arg(jsString(provider.labelsTemplate))
                    .arg(provider.maxZoom)
                    .arg(subdomains);
    }
    html += QStringLiteral(R"JS(
function calcEnd(lat,lon,az,d){
    var R=6371,lat1=lat*Math.PI/180,lon1=lon*Math.PI/180,b=az*Math.PI/180;
    var lat2=Math.asin(Math.sin(lat1)*Math.cos(d/R)+Math.cos(lat1)*Math.sin(d/R)*Math.cos(b));
    var lon2=lon1+Math.atan2(Math.sin(b)*Math.sin(d/R)*Math.cos(lat1),Math.cos(d/R)-Math.sin(lat1)*Math.sin(lat2));
    return[lat2*180/Math.PI,lon2*180/Math.PI];
}
function esc(s){return String(s).replace(/[&<>"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];});}

var html=[];
stations.forEach(function(s){
    if(!s.visible) return;
    var labelHtml = '<div class="marker-label" style="border-color:'+s.color+';color:'+s.color+';">'+esc(s.callsign)+'</div>';
    var icon = L.divIcon({
        className: '',
        html: labelHtml,
        iconSize: null,
        iconAnchor: [-8, 14]
    });
    var signalStr = s.signal || 'S5';
    L.marker([s.lat,s.lon],{icon:icon})
        .bindPopup('<b>'+esc(s.callsign)+'</b><br>'+T.signal+' '+signalStr+'<br>'+T.az+' '+esc(s.azText)+' ±'+s.uncertainty+'°')
        .addTo(map);
    if(signalStr !== 'S0') {
        var e=calcEnd(s.lat,s.lon,s.azimuth,len);
        L.polyline([[s.lat,s.lon],e],{color:s.color,weight:3,dashArray:'10,5'}).addTo(map);
        if(s.uncertainty>0){
            var eL=calcEnd(s.lat,s.lon,s.azimuth-s.uncertainty,len);
            var eR=calcEnd(s.lat,s.lon,s.azimuth+s.uncertainty,len);
            L.polygon([[s.lat,s.lon],eL,eR],{color:s.color,fillColor:s.color,fillOpacity:0.15,weight:1}).addTo(map);
        }
    }
    html.push('<div><span style="color:'+s.color+'">&#9679;</span> '+esc(s.callsign)+' - '+esc(s.azText)+' ±'+s.uncertainty+'° ('+signalStr+')</div>');
});

if(intersection) {
    L.circle([intersection.centerLat, intersection.centerLon], {
        radius: intersection.radiusKm * 1000,
        color: intersection.borderColor,
        fillColor: intersection.fillColor,
        fillOpacity: intersection.opacity,
        weight: 2
    }).addTo(map);
    intersection.points.forEach(function(p) {
        L.circleMarker([p[0], p[1]], {
            radius: 5,
            color: intersection.borderColor,
            fillColor: intersection.borderColor,
            fillOpacity: 1,
            weight: 2
        }).addTo(map);
    });
    L.circleMarker([intersection.centerLat, intersection.centerLon], {
        radius: 8,
        color: '#000',
        fillColor: intersection.fillColor,
        fillOpacity: 1,
        weight: 2
    }).bindPopup('<b>'+T.zoneCentre+'</b><br>'+intersection.centerLat.toFixed(6)+', '+intersection.centerLon.toFixed(6)+'<br>'+T.radius+' '+intersection.radiusKm.toFixed(2)+' km').addTo(map);
    document.getElementById('intersection-info').innerHTML = '<p><b>'+T.zone+'</b> '+intersection.centerLat.toFixed(4)+', '+intersection.centerLon.toFixed(4)+'<br>'+T.radius+' '+intersection.radiusKm.toFixed(2)+' km</p>';
}

if(estimate) {
    L.polygon(estimate.e95, {color: estimate.color, fillColor: estimate.color, fillOpacity: 0.12, weight: 2}).addTo(map);
    L.polygon(estimate.e50, {color: estimate.color, fill: false, weight: 1.5, dashArray: '6,4'}).addTo(map);
    var estimateText = '<b>'+T.estimate+'</b><br>'+estimate.lat.toFixed(6)+', '+estimate.lon.toFixed(6)
        +'<br>MGRS: '+esc(estimate.mgrs)+'<br>'+esc(estimate.ellipse)
        +(estimate.note ? '<br><span style="color:#b32626">'+esc(estimate.note)+'</span>' : '');
    L.circleMarker([estimate.lat, estimate.lon], {radius: 6, color: '#fff', fillColor: estimate.color, fillOpacity: 1, weight: 2})
        .bindPopup(estimateText).addTo(map);
    document.getElementById('intersection-info').innerHTML += '<p>'+estimateText+'</p>';
}

if(beacon) {
    var beaconIcon = L.divIcon({
        className: 'beacon-marker',
        html: '<div class="beacon-pulse" style="background-color: #ff0000; border: 3px solid #fff; border-radius: 50%; width: 20px; height: 20px;"></div>',
        iconSize: [20, 20],
        iconAnchor: [10, 10]
    });
    L.marker([beacon.lat, beacon.lon], {icon: beaconIcon, zIndexOffset: 1000})
        .bindPopup('<b style="color: #ff0000;">'+T.beacon+'</b><br>DD: '+beacon.lat.toFixed(6)+', '+beacon.lon.toFixed(6))
        .addTo(map);
}

document.getElementById('list').innerHTML=html.join('');
</script></body></html>
)JS");
    return html.toUtf8();
}

} // namespace sater
