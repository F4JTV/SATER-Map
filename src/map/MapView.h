/*
 * MapView.h - the slippy map with the mission drawn on it.
 *
 * Replaces the Leaflet page of the Python version (QtWebEngine does not
 * exist on Android).  Tiles are Web Mercator 256 px tiles from
 * TileManager; on top of them, in Leaflet's order and styles: the
 * UTM/MGRS grid, each station's uncertainty cone and dashed bearing line,
 * the intersection zone and its points.
 *
 * The station labels and the beacon are QML items placed with toScreen()
 * so they can be dragged and tapped; renderImage() paints them too, for
 * the PNG and PDF exports.
 *
 * Gestures: drag to pan (with inertia), wheel / double-click / pinch to
 * zoom, right-click or long press for the coordinates of a point.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "core/Fix.h"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QPolygonF>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QVariantMap>

class Mission;
class TileManager;

class MapView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(Mission *mission READ mission WRITE setMission NOTIFY missionChanged)
    Q_PROPERTY(double centerLat READ centerLat WRITE setCenterLat NOTIFY viewChanged)
    Q_PROPERTY(double centerLon READ centerLon WRITE setCenterLon NOTIFY viewChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(int minZoom READ minZoom CONSTANT)
    Q_PROPERTY(int maxZoom READ maxZoom NOTIFY maxZoomChanged)
    // Bumped on every view change: bind marker positions to it.
    Q_PROPERTY(int revision READ revision NOTIFY viewChanged)
    // Metres per screen pixel at the centre (for the scale bar).
    Q_PROPERTY(double metersPerPixel READ metersPerPixel NOTIFY viewChanged)
    Q_PROPERTY(bool primary READ primary WRITE setPrimary NOTIFY primaryChanged)
    // Shown where no tile is loaded yet.
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY styleChanged)
    // Night use: the tiles are multiplied by this colour (red keeps dark
    // adaptation) and darkened by tileDim (0..1).  Overlays are not.
    Q_PROPERTY(QColor tileTint READ tileTint WRITE setTileTint NOTIFY styleChanged)
    Q_PROPERTY(double tileDim READ tileDim WRITE setTileDim NOTIFY styleChanged)

public:
    explicit MapView(QQuickItem *parent = nullptr);
    ~MapView() override;

    Mission *mission() const { return m_mission; }
    void setMission(Mission *mission);
    double centerLat() const { return m_lat; }
    void setCenterLat(double lat);
    double centerLon() const { return m_lon; }
    void setCenterLon(double lon);
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    int minZoom() const { return 2; }
    int maxZoom() const;
    int revision() const { return m_revision; }
    double metersPerPixel() const;
    bool primary() const { return m_primary; }
    void setPrimary(bool on);
    QColor backgroundColor() const { return m_background; }
    void setBackgroundColor(const QColor &c);
    QColor tileTint() const { return m_tint; }
    void setTileTint(const QColor &c);
    double tileDim() const { return m_dim; }
    void setTileDim(double d);

    // Screen position (item coordinates) of a coordinate.
    Q_INVOKABLE QPointF toScreen(double lat, double lon) const;
    // Coordinate under a screen position: QPointF(lat, lon).
    Q_INVOKABLE QPointF toCoordinate(double x, double y) const;
    Q_INVOKABLE void centerOn(double lat, double lon, double zoom = -1);
    // Zoom by delta levels keeping the point (x, y) still.
    Q_INVOKABLE void zoomAround(double delta, double x, double y, bool animated = true);
    Q_INVOKABLE void zoomIn() { zoomAround(1, width() / 2, height() / 2); }
    Q_INVOKABLE void zoomOut() { zoomAround(-1, width() / 2, height() / 2); }
    // {minLat, maxLat, minLon, maxLon, zoom}
    Q_INVOKABLE QVariantMap bounds() const;
    // What lies under (x, y): {kind: "point", index, lat, lon} for an
    // intersection point, {kind: "zone"} inside the zone, or {}.
    Q_INVOKABLE QVariantMap hitTest(double x, double y) const;

    // The current view as an image, markers and attribution included.
    QImage renderImage(qreal scale = 0) const;

    void paint(QPainter *painter) override;

signals:
    void missionChanged();
    void viewChanged();
    void maxZoomChanged();
    void primaryChanged();
    void styleChanged();
    // A tap / click that did not move the map.
    void tapped(double x, double y);
    // Right click or long press: the coordinates of the point.
    void contextRequested(double x, double y, double lat, double lon);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void touchEvent(QTouchEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct View {
        double lat = 46.6;
        double lon = 2.5;
        double zoom = 6;
        double width = 0;
        double height = 0;
    };
    View view() const;
    static QPointF project(const View &v, double lat, double lon);
    static QPointF unproject(const View &v, double x, double y);
    void render(QPainter *p, const View &v, bool forExport) const;
    void drawTiles(QPainter *p, const View &v) const;
    void drawGrid(QPainter *p, const View &v) const;
    void drawOverlays(QPainter *p, const View &v) const;
    void drawEstimate(QPainter *p, const View &v) const;
    QPolygonF estimateOutline(const View &v, const sater::ConfidenceEllipse &e) const;
    void drawMarkers(QPainter *p, const View &v) const;
    void drawAttribution(QPainter *p, const View &v) const;

    void setView(double lat, double lon, double zoom);
    void panBy(double dx, double dy);
    void beginPress(QPointF pos);
    void movePress(QPointF pos);
    void endPress(QPointF pos);
    void startInertia();
    void snapZoom(QPointF anchor);
    void applyAnchoredZoom(double zoom);
    void contentChanged();

    QPointer<Mission> m_mission;
    QPointer<TileManager> m_tiles;
    double m_lat = 46.6;
    double m_lon = 2.5;
    double m_zoom = 6;
    int m_revision = 0;
    bool m_primary = false;
    QColor m_background = QColor(0xdd, 0xdd, 0xdd);
    QColor m_tint;
    double m_dim = 0.0;
    bool m_pinchEnded = false;

    // Gesture state.
    bool m_pressed = false;
    bool m_dragging = false;
    bool m_longPressed = false;
    QPointF m_pressPos;
    QPointF m_lastPos;
    QElapsedTimer m_pressClock;
    QTimer m_longPress;
    QList<QPair<qint64, QPointF>> m_track;     // recent positions for inertia
    QTimer m_inertia;
    QPointF m_velocity;                          // px per ms
    QElapsedTimer m_inertiaClock;
    bool m_pinching = false;
    double m_pinchStartDist = 0;
    double m_pinchStartZoom = 0;
    QPointF m_pinchAnchorCoord;                  // (lat, lon) under the fingers
    QPointF m_lastPinchCenter;
    QVariantAnimation m_zoomAnim;
    QPointF m_zoomAnchorScreen;
    QPointF m_zoomAnchorCoord;
    qint64 m_lastTapTime = -1;
    QPointF m_lastTapPos;
    QElapsedTimer m_clock;
    double m_wheelAccum = 0;
};
