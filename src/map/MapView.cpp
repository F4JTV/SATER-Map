/*
 * MapView.cpp - the slippy map with the mission drawn on it.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "MapView.h"

#include "TileManager.h"
#include "app/Mission.h"
#include "core/Geo.h"
#include "core/TileProviders.h"

#include <algorithm>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QtMath>

#include <cmath>

using namespace sater;

namespace {

constexpr double kEarthR = 6371000.0;          // Leaflet's L.CRS.Earth.R
constexpr int kLongPressMs = 600;
constexpr double kDragThreshold = 6.0;         // px before a press becomes a pan
constexpr double kMaxLat = 85.05112878;

double wrapLon(double lon)
{
    while (lon < -180.0) lon += 360.0;
    while (lon >= 180.0) lon -= 360.0;
    return lon;
}

QColor withAlpha(const QColor &c, double a)
{
    QColor out = c;
    out.setAlphaF(float(std::clamp(a, 0.0, 1.0)));
    return out;
}

// Leaflet's dashArray (pixels) for a pen of the given width.
QVector<qreal> dashes(double on, double off, double width)
{
    return {on / width, off / width};
}

// The view rectangle with a margin, to clip what is drawn: QPainter's
// antialiased rasterizer walks a whole shape, and a 100 km cone at zoom 14
// is tens of thousands of pixels wide - clipped first, it costs what is
// on the screen only.
QRectF viewClip(double w, double h)
{
    return QRectF(-8, -8, w + 16, h + 16);
}

// Sutherland-Hodgman, against an axis-aligned rectangle.
QPolygonF clipPolygon(const QPolygonF &poly, const QRectF &r)
{
    QPolygonF out = poly;
    for (int edge = 0; edge < 4 && !out.isEmpty(); ++edge) {
        auto inside = [&](const QPointF &p) {
            switch (edge) {
            case 0: return p.x() >= r.left();
            case 1: return p.x() <= r.right();
            case 2: return p.y() >= r.top();
            default: return p.y() <= r.bottom();
            }
        };
        auto cross = [&](const QPointF &a, const QPointF &b) {
            double t;
            switch (edge) {
            case 0: t = (r.left() - a.x()) / (b.x() - a.x()); break;
            case 1: t = (r.right() - a.x()) / (b.x() - a.x()); break;
            case 2: t = (r.top() - a.y()) / (b.y() - a.y()); break;
            default: t = (r.bottom() - a.y()) / (b.y() - a.y()); break;
            }
            return a + (b - a) * t;
        };
        const QPolygonF in = out;
        out.clear();
        for (int i = 0; i < in.size(); ++i) {
            const QPointF cur = in.at(i), prev = in.at((i + in.size() - 1) % in.size());
            const bool ci = inside(cur), pi = inside(prev);
            if (ci) {
                if (!pi) out << cross(prev, cur);
                out << cur;
            } else if (pi) {
                out << cross(prev, cur);
            }
        }
    }
    return out;
}

// Liang-Barsky.  On return a and b are the visible part; *startT is how far
// along the original segment (0..1) the visible part starts.
bool clipLine(QPointF *a, QPointF *b, const QRectF &r, double *startT)
{
    const double x0 = a->x(), y0 = a->y(), dx = b->x() - x0, dy = b->y() - y0;
    double t0 = 0, t1 = 1;
    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {x0 - r.left(), r.right() - x0, y0 - r.top(), r.bottom() - y0};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0) return false;
            continue;
        }
        const double t = q[i] / p[i];
        if (p[i] < 0) t0 = std::max(t0, t);
        else t1 = std::min(t1, t);
        if (t0 > t1) return false;
    }
    *b = QPointF(x0 + t1 * dx, y0 + t1 * dy);
    *a = QPointF(x0 + t0 * dx, y0 + t0 * dy);
    if (startT) *startT = t0;
    return true;
}

// A dashed line from a to b, clipped to r, the dashes keeping their place
// along the whole line (so they do not slide when the map moves).
void drawDashedLine(QPainter *p, QPen pen, QPointF a, QPointF b, const QRectF &r)
{
    const double length = QLineF(a, b).length();
    double t0 = 0;
    QPointF ca = a, cb = b;
    if (!clipLine(&ca, &cb, r, &t0)) return;
    const QVector<qreal> pattern = pen.dashPattern();
    double period = 0;
    for (qreal v : pattern) period += v;
    if (period > 0) pen.setDashOffset(std::fmod(t0 * length / pen.widthF(), period));
    p->setPen(pen);
    p->drawLine(ca, cb);
}

// A closed dashed outline, clipped to r.  QPainter starts the dash pattern
// again at each line of a drawLines() call, so the dashes are cut here,
// along the whole outline (they stay in place when the map moves), and
// drawn as plain flat-capped lines: QPainter's fast path, where a round cap
// would take its general path stroker.  Each dash is lengthened by half the
// pen width at both ends, the length Leaflet's round caps give it.
void drawDashedRing(QPainter *p, QPen pen, const QPolygonF &ring, const QRectF &r)
{
    const double w = pen.widthF();
    const QVector<qreal> pattern = pen.dashPattern();
    double period = 0;
    for (qreal v : pattern) period += v * w;
    QVector<QLineF> out;
    const int n = int(ring.size());
    double along = 0;
    for (int i = 0; i < n; ++i) {
        const QPointF a = ring.at(i), b = ring.at((i + 1) % n);
        const double length = QLineF(a, b).length();
        QPointF ca = a, cb = b;
        if (length <= 0 || !clipLine(&ca, &cb, r, nullptr)) {
            along += length;
            continue;
        }
        if (pattern.size() < 2 || period <= 0) {
            out.append(QLineF(ca, cb));
            along += length;
            continue;
        }
        const double end = along + length;
        for (double base = std::floor(along / period) * period - period; base - w / 2 < end; base += period) {
            double on = base;
            for (int j = 0; j + 1 < pattern.size(); j += 2) {
                const double d0 = std::max(on - w / 2, along);
                const double d1 = std::min(on + pattern.at(j) * w + w / 2, end);
                on += (pattern.at(j) + pattern.at(j + 1)) * w;
                if (d1 <= d0) continue;
                QPointF pa = a + (b - a) * ((d0 - along) / length);
                QPointF pb = a + (b - a) * ((d1 - along) / length);
                if (clipLine(&pa, &pb, r, nullptr)) out.append(QLineF(pa, pb));
            }
        }
        along = end;
    }
    pen.setStyle(Qt::SolidLine);
    pen.setCapStyle(Qt::FlatCap);
    p->setPen(pen);
    p->drawLines(out);
}

} // namespace

MapView::MapView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptTouchEvents(true);
    setAntialiasing(true);
    setOpaquePainting(true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
    setFillColor(m_background);
    // Qt 6 always paints a QQuickPaintedItem into a CPU image (the
    // framebuffer-object targets of Qt 5 are ignored), uploaded as a texture
    // at each frame.  What keeps a pan smooth on a phone is drawing only
    // what is on the screen, on QPainter's fast paths (see drawOverlays).

    m_clock.start();

    m_longPress.setSingleShot(true);
    m_longPress.setInterval(kLongPressMs);
    connect(&m_longPress, &QTimer::timeout, this, [this]() {
        if (!m_pressed || m_dragging || m_pinching) return;
        m_longPressed = true;
        const QPointF c = toCoordinate(m_pressPos.x(), m_pressPos.y());
        emit contextRequested(m_pressPos.x(), m_pressPos.y(), c.x(), c.y());
    });

    m_inertia.setInterval(16);
    connect(&m_inertia, &QTimer::timeout, this, [this]() {
        const double dt = double(std::max<qint64>(1, m_inertiaClock.restart()));
        panBy(m_velocity.x() * dt, m_velocity.y() * dt);
        m_velocity *= std::exp(-dt / 325.0);
        if (std::hypot(m_velocity.x(), m_velocity.y()) < 0.02) m_inertia.stop();
    });

    m_zoomAnim.setDuration(200);
    m_zoomAnim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_zoomAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { applyAnchoredZoom(v.toDouble()); });
}

MapView::~MapView() = default;

// ---- properties ----------------------------------------------------------------

void MapView::setMission(Mission *mission)
{
    if (mission == m_mission) return;
    if (m_mission) disconnect(m_mission, nullptr, this, nullptr);
    if (m_tiles) disconnect(m_tiles, nullptr, this, nullptr);
    m_mission = mission;
    m_tiles = mission ? mission->tiles() : nullptr;
    if (m_mission) {
        connect(m_mission, &Mission::mapContentChanged, this, &MapView::contentChanged);
        connect(m_mission, &Mission::providerChanged, this, [this]() {
            emit maxZoomChanged();
            if (m_zoom > maxZoom()) setZoom(maxZoom());
            update();
        });
        if (m_primary) m_mission->setPrimaryView(this);
    }
    if (m_tiles) connect(m_tiles, &TileManager::tileArrived, this, [this]() { update(); });
    emit missionChanged();
    emit maxZoomChanged();
    update();
}

void MapView::setPrimary(bool on)
{
    if (on == m_primary) return;
    m_primary = on;
    if (on && m_mission) m_mission->setPrimaryView(this);
    emit primaryChanged();
}

void MapView::setBackgroundColor(const QColor &c)
{
    if (c == m_background) return;
    m_background = c;
    setFillColor(c);
    emit styleChanged();
    update();
}

void MapView::setTileTint(const QColor &c)
{
    if (c == m_tint) return;
    m_tint = c;
    emit styleChanged();
    update();
}

void MapView::setTileDim(double d)
{
    d = std::clamp(d, 0.0, 1.0);
    if (qFuzzyCompare(d + 1, m_dim + 1)) return;
    m_dim = d;
    emit styleChanged();
    update();
}

int MapView::maxZoom() const
{
    return m_mission ? m_mission->maxZoom() : 19;
}

void MapView::setCenterLat(double lat)
{
    setView(lat, m_lon, m_zoom);
}

void MapView::setCenterLon(double lon)
{
    setView(m_lat, lon, m_zoom);
}

void MapView::setZoom(double zoom)
{
    setView(m_lat, m_lon, zoom);
}

void MapView::setView(double lat, double lon, double zoom)
{
    lat = std::clamp(lat, -kMaxLat, kMaxLat);
    lon = wrapLon(lon);
    zoom = std::clamp(zoom, double(minZoom()), double(maxZoom()));
    if (lat == m_lat && lon == m_lon && zoom == m_zoom) return;
    m_lat = lat;
    m_lon = lon;
    m_zoom = zoom;
    ++m_revision;
    emit viewChanged();
    update();
}

void MapView::contentChanged()
{
    update();
}

void MapView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        ++m_revision;
        emit viewChanged();
    }
}

double MapView::metersPerPixel() const
{
    const double world = 256.0 * std::pow(2.0, m_zoom);
    return 2 * M_PI * 6378137.0 * std::cos(qDegreesToRadians(m_lat)) / world;
}

// ---- projection ----------------------------------------------------------------

MapView::View MapView::view() const
{
    View v;
    v.lat = m_lat;
    v.lon = m_lon;
    v.zoom = m_zoom;
    v.width = width();
    v.height = height();
    return v;
}

QPointF MapView::project(const View &v, double lat, double lon)
{
    const double x = geo::lonToWorldX(lon, v.zoom) - geo::lonToWorldX(v.lon, v.zoom) + v.width / 2;
    const double y = geo::latToWorldY(lat, v.zoom) - geo::latToWorldY(v.lat, v.zoom) + v.height / 2;
    return QPointF(x, y);
}

QPointF MapView::unproject(const View &v, double x, double y)
{
    const double wx = geo::lonToWorldX(v.lon, v.zoom) + (x - v.width / 2);
    const double wy = geo::latToWorldY(v.lat, v.zoom) + (y - v.height / 2);
    return QPointF(geo::worldYToLat(wy, v.zoom), geo::worldXToLon(wx, v.zoom));
}

QPointF MapView::toScreen(double lat, double lon) const
{
    return project(view(), lat, lon);
}

QPointF MapView::toCoordinate(double x, double y) const
{
    return unproject(view(), x, y);
}

void MapView::centerOn(double lat, double lon, double zoom)
{
    m_inertia.stop();
    m_zoomAnim.stop();
    setView(lat, lon, zoom < 0 ? m_zoom : zoom);
}

void MapView::zoomAround(double delta, double x, double y, bool animated)
{
    m_inertia.stop();
    const double from = m_zoomAnim.state() == QAbstractAnimation::Running ? m_zoomAnim.endValue().toDouble() : m_zoom;
    double target = std::round(from + delta);
    target = std::clamp(target, double(minZoom()), double(maxZoom()));
    m_zoomAnim.stop();
    m_zoomAnchorScreen = QPointF(x, y);
    m_zoomAnchorCoord = toCoordinate(x, y);
    if (!animated) {
        applyAnchoredZoom(target);
        return;
    }
    if (std::fabs(target - m_zoom) < 1e-9) return;
    m_zoomAnim.setStartValue(m_zoom);
    m_zoomAnim.setEndValue(target);
    m_zoomAnim.start();
}

void MapView::applyAnchoredZoom(double z)
{
    // Keep the anchor coordinate under the anchor point while zooming.
    z = std::clamp(z, double(minZoom()), double(maxZoom()));
    const double ax = geo::lonToWorldX(m_zoomAnchorCoord.y(), z);
    const double ay = geo::latToWorldY(m_zoomAnchorCoord.x(), z);
    const double cx = ax - (m_zoomAnchorScreen.x() - width() / 2);
    const double cy = ay - (m_zoomAnchorScreen.y() - height() / 2);
    setView(geo::worldYToLat(cy, z), geo::worldXToLon(cx, z), z);
}

void MapView::snapZoom(QPointF anchor)
{
    const double target = std::clamp(std::round(m_zoom), double(minZoom()), double(maxZoom()));
    if (std::fabs(target - m_zoom) < 1e-6) return;
    m_zoomAnim.stop();
    m_zoomAnchorScreen = anchor;
    m_zoomAnchorCoord = toCoordinate(anchor.x(), anchor.y());
    m_zoomAnim.setStartValue(m_zoom);
    m_zoomAnim.setEndValue(target);
    m_zoomAnim.start();
}

void MapView::panBy(double dx, double dy)
{
    const double z = m_zoom;
    const double world = 256.0 * std::pow(2.0, z);
    const double cx = geo::lonToWorldX(m_lon, z) - dx;
    const double cy = std::clamp(geo::latToWorldY(m_lat, z) - dy, 0.0, world);
    setView(geo::worldYToLat(cy, z), geo::worldXToLon(cx, z), z);
}

QVariantMap MapView::bounds() const
{
    const View v = view();
    const QPointF nw = unproject(v, 0, 0);
    const QPointF se = unproject(v, v.width, v.height);
    return {{QStringLiteral("minLat"), se.x()},
            {QStringLiteral("maxLat"), nw.x()},
            {QStringLiteral("minLon"), nw.y()},
            {QStringLiteral("maxLon"), se.y()},
            {QStringLiteral("zoom"), m_zoom}};
}

QVariantMap MapView::hitTest(double x, double y) const
{
    if (!m_mission) return {};
    const View v = view();
    const QVariantMap estimate = {{QStringLiteral("kind"), QStringLiteral("estimate")}};
    if (m_mission->fixShown()) {
        const QPointF c = project(v, m_mission->fixLat(), m_mission->fixLon());
        if (std::hypot(c.x() - x, c.y() - y) <= 12) return estimate;
    }
    const ZoneResult &zone = m_mission->zone();
    for (int i = 0; m_mission->zoneShown() && i < zone.points.size(); ++i) {
        const QPointF p = project(v, zone.points.at(i).x(), zone.points.at(i).y());
        if (std::hypot(p.x() - x, p.y() - y) <= 9) {
            return {{QStringLiteral("kind"), QStringLiteral("point")},
                    {QStringLiteral("index"), i},
                    {QStringLiteral("lat"), zone.points.at(i).x()},
                    {QStringLiteral("lon"), zone.points.at(i).y()}};
        }
    }
    if (m_mission->zoneShown()) {
        const QPointF c = project(v, zone.circle.lat, zone.circle.lon);
        const double r = zone.circle.radiusKm * 1000.0 / (metersPerPixel() > 0 ? metersPerPixel() : 1);
        if (std::hypot(c.x() - x, c.y() - y) <= std::max(r, 4.0)) return {{QStringLiteral("kind"), QStringLiteral("zone")}};
    }
    if (m_mission->fixShown() && estimateOutline(v, m_mission->fix().ellipse95).containsPoint(QPointF(x, y), Qt::OddEvenFill))
        return estimate;
    return {};
}

// ---- painting --------------------------------------------------------------------

void MapView::paint(QPainter *painter)
{
    render(painter, view(), false);
}

QImage MapView::renderImage(qreal scale) const
{
    if (scale <= 0) scale = window() ? std::max<qreal>(1.0, window()->effectiveDevicePixelRatio()) : 1.0;
    const View v = view();
    const QSize size(std::max(1, int(std::ceil(v.width * scale))), std::max(1, int(std::ceil(v.height * scale))));
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(m_background);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(scale, scale);
    render(&p, v, true);
    p.end();
    return img;
}

void MapView::render(QPainter *p, const View &v, bool forExport) const
{
    p->fillRect(QRectF(0, 0, v.width, v.height), m_background);
    drawTiles(p, v);
    if (m_mission && m_mission->showGrid()) drawGrid(p, v);
    drawOverlays(p, v);
    if (forExport) {
        drawMarkers(p, v);
        drawAttribution(p, v);
    }
}

void MapView::drawTiles(QPainter *p, const View &v) const
{
    if (!m_tiles || !m_mission) return;
    const QString provider = m_mission->provider();
    const bool offline = m_mission->offline();
    const int providerMax = tileProvider(provider).maxZoom;

    const int tz = std::clamp(int(std::floor(v.zoom + 0.5)), 0, providerMax);
    const double scale = std::pow(2.0, v.zoom - tz);
    const double tilePx = 256.0 * scale;
    const int n = 1 << tz;
    const double cx = geo::lonToWorldX(v.lon, tz) * scale;
    const double cy = geo::latToWorldY(v.lat, tz) * scale;
    const double ox = v.width / 2 - cx;   // screen x of world x = 0
    const double oy = v.height / 2 - cy;

    const int tx0 = int(std::floor(-ox / tilePx));
    const int tx1 = int(std::floor((v.width - ox) / tilePx));
    const int ty0 = std::max(0, int(std::floor(-oy / tilePx)));
    const int ty1 = std::min(n - 1, int(std::floor((v.height - oy) / tilePx)));

    const_cast<TileManager *>(m_tiles.data())->beginFrame();
    p->save();
    // Tiles land pixel for pixel only at a whole zoom on a 1:1 screen; a
    // phone shows them scaled up two or three times, smoothly as a browser
    // does, or the map labels come out blocky.
    const QTransform device = p->combinedTransform();
    const double deviceScale = std::hypot(device.m11(), device.m12());
    const bool smooth = std::fabs(scale * deviceScale - 1.0) > 1e-3;
    p->setRenderHint(QPainter::Antialiasing, false);

    // Centre tiles first: they are requested (and so loaded) first.
    struct Item { int tx, ty; double d; };
    QList<Item> items;
    const double ctx = (v.width / 2 - ox) / tilePx, cty = (v.height / 2 - oy) / tilePx;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) items.append({tx, ty, std::hypot(tx + 0.5 - ctx, ty + 0.5 - cty)});
    }
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.d < b.d; });

    for (const Item &it : items) {
        const int wx = ((it.tx % n) + n) % n;
        const double x0 = std::round(ox + it.tx * tilePx), x1 = std::round(ox + (it.tx + 1) * tilePx);
        const double y0 = std::round(oy + it.ty * tilePx), y1 = std::round(oy + (it.ty + 1) * tilePx);
        const QRectF target(x0, y0, x1 - x0, y1 - y0);
        QImage img = const_cast<TileManager *>(m_tiles.data())->tile(provider, tz, wx, it.ty, offline);
        if (!img.isNull()) {
            p->setRenderHint(QPainter::SmoothPixmapTransform, smooth);
            p->drawImage(target, img);
            continue;
        }
        // While it loads, a parent tile scaled up stands in.
        for (int dz = 1; dz <= 6 && tz - dz >= 0; ++dz) {
            const QImage parent = m_tiles->peek(provider, tz - dz, wx >> dz, it.ty >> dz);
            if (parent.isNull()) continue;
            const int span = 1 << dz;
            const double sub = 256.0 / span;
            const QRectF source((wx % span) * sub, (it.ty % span) * sub, sub, sub);
            p->setRenderHint(QPainter::SmoothPixmapTransform, true);
            p->drawImage(target, parent, source);
            break;
        }
    }

    if (m_tint.isValid() && m_tint.alpha() > 0) {
        p->setCompositionMode(QPainter::CompositionMode_Multiply);
        p->fillRect(QRectF(0, 0, v.width, v.height), m_tint);
        p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    if (m_dim > 0) p->fillRect(QRectF(0, 0, v.width, v.height), withAlpha(Qt::black, m_dim));
    p->restore();
}

void MapView::drawGrid(QPainter *p, const View &v) const
{
    // The Python "UTM/MGRS grid": a latitude/longitude graticule whose
    // spacing follows the zoom, labelled with degrees and the UTM zone.
    const QColor color(m_mission->gridColor());
    const double luminance = (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0;
    const QColor labelBg = luminance > 0.5 ? QColor(0, 0, 0, 191) : QColor(40, 40, 40, 217);

    const double zoom = v.zoom;
    double spacing;
    if (zoom < 8) spacing = 6;
    else if (zoom < 10) spacing = 1;
    else if (zoom < 12) spacing = 0.5;
    else if (zoom < 14) spacing = 0.1;
    else spacing = 0.05;

    const QPointF nw = unproject(v, 0, 0);
    const QPointF se = unproject(v, v.width, v.height);
    const double north = nw.x(), south = se.x(), west = nw.y(), east = se.y();
    const long iLat0 = long(std::floor(south / spacing)), iLat1 = long(std::ceil(north / spacing));
    const long iLon0 = long(std::floor(west / spacing)), iLon1 = long(std::ceil(east / spacing));
    if ((iLat1 - iLat0) > 400 || (iLon1 - iLon0) > 400) return;
    const double minLat = iLat0 * spacing, maxLat = iLat1 * spacing;
    const double minLon = iLon0 * spacing, maxLon = iLon1 * spacing;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(withAlpha(color, 0.6), 1);
    p->setPen(pen);
    for (long i = iLon0; i <= iLon1; ++i) {
        const double lon = i * spacing;
        p->drawLine(project(v, minLat, lon), project(v, maxLat, lon));
    }
    for (long i = iLat0; i <= iLat1; ++i) {
        const double lat = i * spacing;
        p->drawLine(project(v, lat, minLon), project(v, lat, maxLon));
    }

    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(11);
    p->setFont(font);
    const QFontMetricsF fm(font);
    static const char letters[] = "CDEFGHJKLMNPQRSTUVWXX";

    auto label = [&](const QString &text, QPointF anchor, bool centered) {
        const double w = fm.horizontalAdvance(text) + 10;
        const double h = fm.height() + 4;
        QRectF r = centered ? QRectF(anchor.x() - w / 2, anchor.y(), w, h) : QRectF(anchor.x(), anchor.y() - h / 2, w, h);
        p->setPen(Qt::NoPen);
        p->setBrush(labelBg);
        p->drawRoundedRect(r, 3, 3);
        // text-shadow: 1px 1px 2px rgba(0,0,0,0.8)
        p->setPen(QColor(0, 0, 0, 204));
        p->drawText(r.translated(1, 1), Qt::AlignCenter, text);
        p->setPen(color);
        p->drawText(r, Qt::AlignCenter, text);
    };

    const double labelLat = north - (north - south) * 0.02;
    for (long i = iLon0; i <= iLon1; ++i) {
        const double lon = i * spacing;
        label(QStringLiteral("%1°").arg(lon, 0, 'f', 2), project(v, labelLat, lon), true);
    }
    const double centerLon = (west + east) / 2;
    const int zone = int(std::floor((centerLon + 180) / 6)) + 1;
    const double labelLon = west + (east - west) * 0.02;
    for (long i = iLat0; i <= iLat1; ++i) {
        const double lat = i * spacing;
        const int li = int(std::floor((lat + 80) / 8));
        const QChar letter = (li >= 0 && li < 21) ? QLatin1Char(letters[li]) : QLatin1Char('?');
        label(QStringLiteral("%1%2 %3°").arg(zone).arg(letter).arg(lat, 0, 'f', 2), project(v, lat, labelLon), false);
    }
    p->restore();
}

void MapView::drawOverlays(QPainter *p, const View &v) const
{
    if (!m_mission) return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const double len = m_mission->azimuthLengthKm();

    const QRectF clip = viewClip(v.width, v.height);

    // Cones and bearing lines, station by station (Leaflet's order).
    for (const Station &s : m_mission->stations()->stations()) {
        if (!s.isValid() || !s.visible || !s.hasBearing()) continue;
        const QColor color(s.color);
        const QPointF origin = project(v, s.lat, s.lon);
        const double azimuth = s.trueAzimuth();
        const QPointF endLatLon = geo::endpoint(s.lat, s.lon, azimuth, len);
        const QPointF end = project(v, endLatLon.x(), endLatLon.y());

        // Leaflet draws the line first and the cone over it.
        QPen line(withAlpha(color, 0.9), 3, Qt::CustomDashLine, Qt::FlatCap, Qt::RoundJoin);
        line.setDashPattern(dashes(10, 5, 3));
        p->setBrush(Qt::NoBrush);
        drawDashedLine(p, line, origin, end, clip);

        if (s.uncertainty > 0) {
            const QPointF l = geo::endpoint(s.lat, s.lon, azimuth - s.uncertainty, len);
            const QPointF r = geo::endpoint(s.lat, s.lon, azimuth + s.uncertainty, len);
            const QPointF pl = project(v, l.x(), l.y()), pr = project(v, r.x(), r.y());
            // The fill clipped to the view; the two sides drawn as lines,
            // so the clipping edges do not show.
            const QPolygonF cone = clipPolygon(QPolygonF({origin, pl, pr}), clip);
            if (cone.size() >= 3) {
                // Filled without antialiasing (several times faster on
                // large areas): the antialiased sides are drawn over it.
                p->setRenderHint(QPainter::Antialiasing, false);
                p->setPen(Qt::NoPen);
                p->setBrush(withAlpha(color, 0.15));
                p->drawPolygon(cone);
                p->setRenderHint(QPainter::Antialiasing, true);
            }
            // Flat caps: QPainter strokes straight lines with square or flat
            // caps on its fast path; round caps go through the general
            // path stroker, several times slower on a phone screen.
            p->setPen(QPen(withAlpha(color, 0.5), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
            p->setBrush(Qt::NoBrush);
            for (const QLineF &side : {QLineF(origin, pl), QLineF(origin, pr), QLineF(pl, pr)}) {
                QPointF a = side.p1(), b = side.p2();
                if (clipLine(&a, &b, clip, nullptr)) p->drawLine(a, b);
            }
        }
    }

    // The intersection zone: Leaflet's L.circle, an ellipse in Mercator.
    if (m_mission->zoneShown()) {
        const ZoneResult &zone = m_mission->zone();
        const double lat = zone.circle.lat, lng = zone.circle.lon;
        const double d = M_PI / 180;
        const double latR = (zone.circle.radiusKm * 1000.0 / kEarthR) / d;
        const QPointF top = project(v, lat + latR, lng);
        const QPointF bottom = project(v, lat - latR, lng);
        const QPointF c = (top + bottom) / 2;
        const double lat2 = unproject(v, c.x(), c.y()).x();
        const double lngR = std::acos((std::cos(latR * d) - std::sin(lat * d) * std::sin(lat2 * d))
                                      / (std::cos(lat * d) * std::cos(lat2 * d))) / d;
        double rx = std::isnan(lngR) ? 0 : c.x() - project(v, lat2, lng - lngR).x();
        double ry = c.y() - top.y();
        rx = std::max(rx, 1.0);
        ry = std::max(ry, 1.0);

        const QColor border(m_mission->zoneBorderColor());
        const QColor fill(m_mission->zoneFillColor());
        // Drawn as a polygon rather than with drawEllipse(): clipped to the
        // view (a large zone at a high zoom is far bigger than the screen),
        // and its dashed border stroked as straight flat-capped lines, which
        // QPainter rasterizes on its fast path.
        const double perimeter = 2 * M_PI * std::max(rx, ry);
        const int n = std::clamp(int(perimeter / 4), 48, 4096);
        QPolygonF ring;
        ring.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double a = 2 * M_PI * i / n;
            ring << QPointF(c.x() + rx * std::cos(a), c.y() + ry * std::sin(a));
        }
        const QPolygonF area = clipPolygon(ring, clip);
        if (area.size() >= 3) {
            p->setPen(Qt::NoPen);
            p->setBrush(withAlpha(fill, m_mission->zoneOpacity()));
            p->drawPolygon(area);
        }
        QPen pen(border, 2, Qt::CustomDashLine, Qt::FlatCap, Qt::MiterJoin);
        pen.setDashPattern(dashes(5, 5, 2));
        p->setBrush(Qt::NoBrush);
        drawDashedRing(p, pen, ring, clip);

        p->setPen(QPen(border, 2));
        p->setBrush(border);
        for (const QPointF &pt : zone.points) p->drawEllipse(project(v, pt.x(), pt.y()), 6, 6);
    }
    p->restore();
    if (m_mission->fixShown()) drawEstimate(p, v);
}

QPolygonF MapView::estimateOutline(const View &v, const ConfidenceEllipse &e) const
{
    // The ellipse on the ground (km along its axes), then on the screen.
    const FixResult &f = m_mission->fix();
    const double kmPerDeg = kEarthR / 1000.0 * M_PI / 180.0;
    const double cosLat = std::max(1e-6, std::cos(qDegreesToRadians(f.lat)));
    const double theta = qDegreesToRadians(e.orientationDeg);
    const double mpp = metersPerPixel() > 0 ? metersPerPixel() : 1;
    const double perimeter = 2 * M_PI * e.semiMajorKm * 1000.0 / mpp;
    const int n = std::clamp(int(perimeter / 4), 48, 4096);
    QPolygonF ring;
    ring.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double t = 2 * M_PI * i / n;
        const double along = e.semiMajorKm * std::cos(t), across = e.semiMinorKm * std::sin(t);
        const double east = along * std::sin(theta) + across * std::cos(theta);
        const double north = along * std::cos(theta) - across * std::sin(theta);
        ring << project(v, f.lat + north / kmPerDeg, f.lon + east / (kmPerDeg * cosLat));
    }
    return ring;
}

void MapView::drawEstimate(QPainter *p, const View &v) const
{
    // The least-squares estimate: the 95 % ellipse filled and outlined, the
    // 50 % one dashed, a cross on the point.
    const FixResult &f = m_mission->fix();
    const QColor color(m_mission->estimateColor());
    const QRectF clip = viewClip(v.width, v.height);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const QPolygonF outer = estimateOutline(v, f.ellipse95);
    const QPolygonF area = clipPolygon(outer, clip);
    if (area.size() >= 3) {
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(color, 0.12));
        p->drawPolygon(area);
    }
    p->setBrush(Qt::NoBrush);
    drawDashedRing(p, QPen(color, 2, Qt::SolidLine, Qt::FlatCap), outer, clip);
    QPen half(color, 1.5, Qt::CustomDashLine, Qt::FlatCap);
    half.setDashPattern(dashes(6, 4, 1.5));
    drawDashedRing(p, half, estimateOutline(v, f.ellipse50), clip);

    const QPointF c = project(v, f.lat, f.lon);
    if (clip.contains(c)) {
        const double arm = 11;
        const QLineF lines[] = {QLineF(c.x() - arm, c.y(), c.x() - 3, c.y()), QLineF(c.x() + 3, c.y(), c.x() + arm, c.y()),
                                QLineF(c.x(), c.y() - arm, c.x(), c.y() - 3), QLineF(c.x(), c.y() + 3, c.x(), c.y() + arm)};
        p->setPen(QPen(QColor(255, 255, 255, 230), 4.5, Qt::SolidLine, Qt::FlatCap));
        p->drawLines(lines, 4);
        p->setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap));
        p->drawLines(lines, 4);
        p->setPen(QPen(Qt::white, 1.5));
        p->setBrush(color);
        p->drawEllipse(c, 3, 3);
    }
    p->restore();
}

void MapView::drawMarkers(QPainter *p, const View &v) const
{
    if (!m_mission) return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QFont font = p->font();
    font.setPixelSize(12);
    font.setBold(true);
    p->setFont(font);
    const QFontMetricsF fm(font);

    for (const Station &s : m_mission->stations()->stations()) {
        if (!s.isValid() || !s.visible) continue;
        const QColor color(s.color);
        const QPointF pt = project(v, s.lat, s.lon);
        // .marker-label: 3px 8px padding, 2px border, anchor [0, 12].
        const double w = fm.horizontalAdvance(s.callsign) + 16 + 4;
        const double h = fm.height() + 6 + 4;
        const QRectF r(pt.x(), pt.y() - 12, w, h);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 60));
        p->drawRoundedRect(r.translated(0, 2), 4, 4);
        p->setPen(QPen(color, 2));
        p->setBrush(QColor(255, 255, 255, 242));
        p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        p->setPen(color);
        p->drawText(r, Qt::AlignCenter, s.callsign);
    }

    if (m_mission->hasBeacon()) {
        const QPointF c = project(v, m_mission->beaconLat(), m_mission->beaconLon());
        QRadialGradient glow(c, 22);
        glow.setColorAt(0.4, QColor(255, 0, 0, 160));
        glow.setColorAt(1.0, QColor(255, 0, 0, 0));
        p->setPen(Qt::NoPen);
        p->setBrush(glow);
        p->drawEllipse(c, 22, 22);
        p->setPen(QPen(Qt::white, 3));
        p->setBrush(QColor(255, 0, 0));
        p->drawEllipse(c, 8.5, 8.5);
    }
    p->restore();
}

void MapView::drawAttribution(QPainter *p, const View &v) const
{
    if (!m_mission) return;
    const QString text = QStringLiteral("SATER Map | ") + m_mission->attribution();
    QFont font = p->font();
    font.setPixelSize(11);
    font.setBold(false);
    p->save();
    p->setFont(font);
    const QFontMetricsF fm(font);
    const double w = fm.horizontalAdvance(text) + 10, h = fm.height() + 2;
    const QRectF r(v.width - w, v.height - h, w, h);
    p->fillRect(r, QColor(255, 255, 255, 204));
    p->setPen(QColor(0x33, 0x33, 0x33));
    p->drawText(r, Qt::AlignCenter, text);
    p->restore();
}

// ---- gestures ----------------------------------------------------------------------

void MapView::beginPress(QPointF pos)
{
    m_inertia.stop();
    m_zoomAnim.stop();
    m_pressed = true;
    m_dragging = false;
    m_longPressed = false;
    m_pressPos = pos;
    m_lastPos = pos;
    m_track.clear();
    m_track.append({m_clock.elapsed(), pos});
    m_longPress.start();
}

void MapView::movePress(QPointF pos)
{
    if (!m_pressed) return;
    if (!m_dragging && QLineF(pos, m_pressPos).length() < kDragThreshold) return;
    if (!m_dragging) {
        m_dragging = true;
        m_longPress.stop();
        // Start from the press point: the threshold is not a jump.
        m_lastPos = m_pressPos;
    }
    panBy(pos.x() - m_lastPos.x(), pos.y() - m_lastPos.y());
    m_lastPos = pos;
    const qint64 now = m_clock.elapsed();
    m_track.append({now, pos});
    while (m_track.size() > 2 && now - m_track.first().first > 100) m_track.removeFirst();
}

void MapView::endPress(QPointF pos)
{
    if (!m_pressed) return;
    m_pressed = false;
    m_longPress.stop();
    if (m_longPressed) return;
    if (m_dragging) {
        startInertia();
        return;
    }
    emit tapped(pos.x(), pos.y());
}

void MapView::startInertia()
{
    const qint64 now = m_clock.elapsed();
    if (m_track.size() < 2 || now - m_track.last().first > 50) return;
    const auto &a = m_track.first();
    const auto &b = m_track.last();
    const double dt = double(b.first - a.first);
    if (dt <= 0) return;
    m_velocity = (b.second - a.second) / dt;
    if (std::hypot(m_velocity.x(), m_velocity.y()) < 0.3) return;
    m_inertiaClock.start();
    m_inertia.start();
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::RightButton) {
        const QPointF c = toCoordinate(event->position().x(), event->position().y());
        emit contextRequested(event->position().x(), event->position().y(), c.x(), c.y());
        event->accept();
        return;
    }
    beginPress(event->position());
    m_longPress.stop(); // a mouse has a right button for that
    event->accept();
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    movePress(event->position());
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) endPress(event->position());
    event->accept();
}

void MapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        zoomAround(event->modifiers() & Qt::ShiftModifier ? -1 : 1, event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::wheelEvent(QWheelEvent *event)
{
    m_wheelAccum += event->angleDelta().y();
    while (std::fabs(m_wheelAccum) >= 120) {
        const int step = m_wheelAccum > 0 ? 1 : -1;
        m_wheelAccum -= step * 120;
        zoomAround(step, event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::touchEvent(QTouchEvent *event)
{
    const QList<QEventPoint> &pts = event->points();
    int down = 0;
    for (const QEventPoint &pt : pts) {
        if (pt.state() != QEventPoint::Released) ++down;
    }

    if (event->type() == QEvent::TouchCancel) {
        m_pressed = m_dragging = m_pinching = m_pinchEnded = false;
        m_longPress.stop();
        event->accept();
        return;
    }

    if (down >= 2 && pts.size() >= 2) {
        const QPointF a = pts.at(0).position(), b = pts.at(1).position();
        const QPointF center = (a + b) / 2;
        const double dist = std::max(1.0, QLineF(a, b).length());
        if (!m_pinching) {
            m_pinching = true;
            m_pressed = false;
            m_dragging = false;
            m_longPress.stop();
            m_inertia.stop();
            m_zoomAnim.stop();
            m_pinchStartDist = dist;
            m_pinchStartZoom = m_zoom;
            m_pinchAnchorCoord = toCoordinate(center.x(), center.y());
        }
        m_lastPinchCenter = center;
        const double z = std::clamp(m_pinchStartZoom + std::log2(dist / m_pinchStartDist), double(minZoom()),
                                    double(maxZoom()));
        // The point first under the fingers follows their middle.
        const double ax = geo::lonToWorldX(m_pinchAnchorCoord.y(), z);
        const double ay = geo::latToWorldY(m_pinchAnchorCoord.x(), z);
        setView(geo::worldYToLat(ay - (center.y() - height() / 2), z),
                geo::worldXToLon(ax - (center.x() - width() / 2), z), z);
        event->accept();
        return;
    }

    if (m_pinching) {
        // One finger lifted: the pinch is over; ignore the other finger
        // until it lifts too.
        m_pinching = false;
        m_pinchEnded = down > 0;
        snapZoom(m_lastPinchCenter);
        event->accept();
        return;
    }

    if (pts.isEmpty()) {
        event->accept();
        return;
    }
    const QEventPoint &pt = pts.first();
    if (m_pinchEnded) {
        if (pt.state() == QEventPoint::Released || down == 0) m_pinchEnded = false;
        event->accept();
        return;
    }
    switch (pt.state()) {
    case QEventPoint::Pressed:
        forceActiveFocus(Qt::MouseFocusReason);
        beginPress(pt.position());
        break;
    case QEventPoint::Updated:
        movePress(pt.position());
        break;
    case QEventPoint::Released: {
        const bool wasTap = m_pressed && !m_dragging && !m_longPressed;
        const QPointF pos = pt.position();
        if (wasTap) {
            const qint64 now = m_clock.elapsed();
            if (m_lastTapTime >= 0 && now - m_lastTapTime < 300 && QLineF(pos, m_lastTapPos).length() < 30) {
                // Double tap: zoom in there.
                m_pressed = false;
                m_longPress.stop();
                m_lastTapTime = -1;
                zoomAround(1, pos.x(), pos.y());
                break;
            }
            m_lastTapTime = now;
            m_lastTapPos = pos;
        }
        endPress(pos);
        break;
    }
    default:
        break;
    }
    event->accept();
}

void MapView::keyPressEvent(QKeyEvent *event)
{
    const double step = 80;
    switch (event->key()) {
    case Qt::Key_Left: panBy(step, 0); break;
    case Qt::Key_Right: panBy(-step, 0); break;
    case Qt::Key_Up: panBy(0, step); break;
    case Qt::Key_Down: panBy(0, -step); break;
    case Qt::Key_Plus:
    case Qt::Key_Equal: zoomIn(); break;
    case Qt::Key_Minus: zoomOut(); break;
    default:
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }
    event->accept();
}
