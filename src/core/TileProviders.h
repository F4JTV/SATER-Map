/*
 * TileProviders.h - the map backgrounds.
 *
 * The keys are the Python version's TILE_PROVIDERS keys: they name the
 * folders under tiles/, so tiles downloaded with SATER Map 2.0 (Python)
 * are found again.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace sater {

struct TileProvider {
    QString key;
    QString name;
    QString urlTemplate;     // Leaflet syntax: {s} {z} {x} {y} {r}
    QStringList subdomains;
    QString attribution;
    int maxZoom = 19;
    // A transparent layer of place names drawn over the tiles, for the
    // backgrounds served without them (same syntax); empty for the others.
    // A tile stored in the tiles folder is the two layers in one image.
    QString labelsTemplate = {};
};

const QList<TileProvider> &tileProviders();
// The provider for key, OpenStreetMap when the key is unknown.  The keys of
// backgrounds withdrawn since lead to their replacement (cartodb-dark: CARTO
// now asks for an API key; the dark background is esri-dark).
const TileProvider &tileProvider(const QString &key);
// The tile URL, the subdomain picked by rotating on index.
QString tileUrl(const TileProvider &p, int z, int x, int y, int index = 0);
// The URL of the labels tile (empty when the provider has none).
QString labelsUrl(const TileProvider &p, int z, int x, int y, int index = 0);

} // namespace sater
