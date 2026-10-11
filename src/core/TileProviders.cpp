/*
 * TileProviders.cpp - the map backgrounds.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "TileProviders.h"

namespace sater {

const QList<TileProvider> &tileProviders()
{
    static const QList<TileProvider> list = {
        // OpenStreetMap asks clients to stop using the a/b/c subdomains.
        {QStringLiteral("osm"), QStringLiteral("OpenStreetMap"),
         QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png"), {},
         QStringLiteral("© OpenStreetMap"), 19},
        {QStringLiteral("osm-fr"), QStringLiteral("OpenStreetMap France"),
         QStringLiteral("https://{s}.tile.openstreetmap.fr/osmfr/{z}/{x}/{y}.png"),
         {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
         QStringLiteral("© OpenStreetMap France"), 19},
        {QStringLiteral("topo"), QStringLiteral("OpenTopoMap"),
         QStringLiteral("https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png"),
         {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
         QStringLiteral("© OpenTopoMap"), 17},
        // Esri's Dark Gray Canvas, free of API keys like World Imagery
        // below: the base and its labels are two services.  Level 16 is the
        // most detailed (over Europe; less elsewhere), shown enlarged beyond.
        {QStringLiteral("esri-dark"), QStringLiteral("Esri Dark Gray"),
         QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/{z}/{y}/{x}"),
         {}, QStringLiteral("© Esri, HERE, Garmin, © OpenStreetMap"), 16,
         QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Reference/MapServer/tile/{z}/{y}/{x}")},
        {QStringLiteral("geoportail-plan"), QStringLiteral("Géoportail Plan"),
         QStringLiteral("https://data.geopf.fr/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&TILEMATRIXSET=PM"
                        "&LAYER=GEOGRAPHICALGRIDSYSTEMS.PLANIGNV2&STYLE=normal&FORMAT=image/png"
                        "&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}"),
         {}, QStringLiteral("© IGN Géoportail"), 18},
        {QStringLiteral("geoportail-sat"), QStringLiteral("Géoportail Satellite"),
         QStringLiteral("https://data.geopf.fr/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&TILEMATRIXSET=PM"
                        "&LAYER=ORTHOIMAGERY.ORTHOPHOTOS&STYLE=normal&FORMAT=image/jpeg"
                        "&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}"),
         {}, QStringLiteral("© IGN Géoportail"), 19},
        {QStringLiteral("esri-sat"), QStringLiteral("Esri Satellite"),
         QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"),
         {}, QStringLiteral("© Esri"), 19},
    };
    return list;
}

const TileProvider &tileProvider(const QString &key)
{
    const QString wanted = key == QLatin1String("cartodb-dark") ? QStringLiteral("esri-dark") : key;
    for (const TileProvider &p : tileProviders()) {
        if (p.key == wanted) return p;
    }
    return tileProviders().first();
}

namespace {

QString fill(QString url, const QStringList &subdomains, int z, int x, int y, int index)
{
    if (!subdomains.isEmpty()) {
        const int i = ((index % subdomains.size()) + subdomains.size()) % subdomains.size();
        url.replace(QLatin1String("{s}"), subdomains.at(i));
    }
    url.replace(QLatin1String("{r}"), QString());
    url.replace(QLatin1String("{z}"), QString::number(z));
    url.replace(QLatin1String("{x}"), QString::number(x));
    url.replace(QLatin1String("{y}"), QString::number(y));
    return url;
}

} // namespace

QString tileUrl(const TileProvider &p, int z, int x, int y, int index)
{
    return fill(p.urlTemplate, p.subdomains, z, x, y, index);
}

QString labelsUrl(const TileProvider &p, int z, int x, int y, int index)
{
    return p.labelsTemplate.isEmpty() ? QString() : fill(p.labelsTemplate, p.subdomains, z, x, y, index);
}

} // namespace sater
