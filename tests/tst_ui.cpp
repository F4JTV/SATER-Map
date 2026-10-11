/*
 * tst_ui.cpp - the real QML, headless, under real pointer events.
 *
 * Loads qml/Main.qml from the source tree with the application's objects,
 * then: both layouts, every control inside a 360 px wide phone, the
 * dialogs, the map gestures (drag, wheel, right click, "Station here"),
 * dragging a station's label, the French interface.  Any QML warning
 * (a TypeError, a broken binding) fails the test.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/AppController.h"
#include "app/AprsLink.h"
#include "app/Mission.h"
#include "aprs/Ax25.h"
#include "map/MapView.h"
#include "map/TileManager.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QPointingDevice>
#include <QTest>

#include <cmath>
#include <functional>

namespace {

QStringList g_warnings;
QtMessageHandler g_previous = nullptr;

void collect(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        // Noise of the headless platform, not of the program.
        if (!msg.contains(QLatin1String("XDG_RUNTIME_DIR")) && !msg.contains(QLatin1String("propagateSizeHints")))
            g_warnings.append(msg);
    }
    if (g_previous) g_previous(type, ctx, msg);
}

} // namespace

class TestUi : public QObject
{
    Q_OBJECT

    QQmlApplicationEngine *m_engine = nullptr;
    AppController *m_app = nullptr;
    Mission *m_mission = nullptr;
    QQuickWindow *m_window = nullptr;

    // By object name, through the visual tree too: the delegates of a
    // ListView have no QObject parent to be found by.
    static QQuickItem *byName(QQuickItem *root, const QString &name)
    {
        if (!root) return nullptr;
        if (root->objectName() == name) return root;
        for (QQuickItem *c : root->childItems()) {
            if (QQuickItem *f = byName(c, name)) return f;
        }
        return nullptr;
    }
    QQuickItem *item(const char *name) const
    {
        if (auto *i = m_window->findChild<QQuickItem *>(QLatin1String(name))) return i;
        return byName(m_window->contentItem(), QLatin1String(name));
    }
    QObject *object(const char *name) const { return m_window->findChild<QObject *>(QLatin1String(name)); }
    // Every item of that name in the visual tree (list delegates included).
    void allByName(QQuickItem *root, const QString &name, QList<QQuickItem *> *out) const
    {
        if (!root) return;
        if (root->objectName() == name) out->append(root);
        for (QQuickItem *c : root->childItems()) allByName(c, name, out);
    }
    bool callsignShown(const QString &callsign) const
    {
        QList<QQuickItem *> fields;
        allByName(m_window->contentItem(), QStringLiteral("callsignField"), &fields);
        for (QQuickItem *f : fields) {
            if (f->property("text").toString() == callsign) return true;
        }
        return false;
    }
    QPoint centre(QQuickItem *i) const
    {
        return i->mapToScene(QPointF(i->width() / 2, i->height() / 2)).toPoint();
    }
    void resize(int w, int h)
    {
        m_window->resize(w, h);
        QTRY_COMPARE(m_window->width(), w);
        QTest::qWait(150);
    }
    void noWarnings()
    {
        const QStringList w = g_warnings;
        g_warnings.clear();
        QVERIFY2(w.isEmpty(), qPrintable(w.join(QLatin1Char('\n'))));
    }
    // Every visible control of the window lies inside it (a long French
    // label must wrap, not push its card past the edge of a phone).
    void checkFits(const QString &where)
    {
        QStringList problems;
        // The visual tree: list delegates (the station cards) have no
        // QObject parent and findChildren() would not see them.
        QList<QQuickItem *> all;
        const std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
            for (QQuickItem *c : item->childItems()) {
                all.append(c);
                collect(c);
            }
        };
        collect(m_window->contentItem());
        for (QQuickItem *i : all) {
            if (!i->isVisible() || i->width() <= 0 || !i->inherits("QQuickControl")) continue;
            // Only what is on screen: items of hidden pages are invisible,
            // items scrolled away are clipped by their Flickable.
            bool shown = true;
            for (QQuickItem *p = i; p; p = p->parentItem()) {
                if (!p->isVisible() || p->opacity() == 0) {
                    shown = false;
                    break;
                }
            }
            if (!shown) continue;
            const QString name = QString::fromLatin1(i->metaObject()->className()) + QStringLiteral(" \"")
                                 + (i->property("text").toString().isEmpty() ? i->objectName() : i->property("text").toString())
                                 + QLatin1Char('"');
            // A button's text and icon inside the button: what they need
            // against the room its padding leaves (the padding of Material
            // 3, Qt 6.5 and later, is not that of Material 2).
            if (i->inherits("QQuickAbstractButton")) {
                auto *content = i->property("contentItem").value<QQuickItem *>();
                const qreal room = i->property("availableWidth").toReal();
                if (content && content->isVisible() && content->implicitWidth() > room + 1)
                    problems << QStringLiteral("%1 needs %2 px for its content, has %3").arg(name).arg(content->implicitWidth()).arg(room);
            }
            const QRectF r = i->mapRectToScene(QRectF(0, 0, i->width(), i->height()));
            // Inside a horizontally clipped parent (the station list never
            // scrolls sideways) - report it.
            if (r.left() < -1 || r.right() > m_window->width() + 1)
                problems << QStringLiteral("%1 spans %2..%3 in a %4 px window").arg(name).arg(r.left()).arg(r.right()).arg(m_window->width());
        }
        problems.removeDuplicates();
        if (!problems.isEmpty()) QFAIL(qPrintable(where + QStringLiteral(":\n    ") + problems.join(QStringLiteral("\n    "))));
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("SATER_Map_Test"));
        QCoreApplication::setApplicationName(QStringLiteral("SATER_Map_Test"));
        QCoreApplication::setApplicationVersion(QStringLiteral(SATER_VERSION));
        QQuickStyle::setStyle(QStringLiteral("Material"));
        g_previous = qInstallMessageHandler(collect);

        qmlRegisterType<MapView>("SaterMap.Native", 1, 0, "MapView");
        qmlRegisterUncreatableType<Mission>("SaterMap.Native", 1, 0, "Mission", QString());
        qmlRegisterUncreatableType<StationModel>("SaterMap.Native", 1, 0, "StationModel", QString());
        qmlRegisterUncreatableType<HistoryModel>("SaterMap.Native", 1, 0, "HistoryModel", QString());
        qmlRegisterUncreatableType<PresetModel>("SaterMap.Native", 1, 0, "PresetModel", QString());
        qmlRegisterUncreatableType<TileManager>("SaterMap.Native", 1, 0, "TileManager", QString());
        qmlRegisterUncreatableType<AprsLink>("SaterMap.Native", 1, 0, "AprsLink", QStringLiteral("From mission"));

        m_app = new AppController(this);
        m_app->setLanguage(QStringLiteral("en"));
        m_app->setTheme(QStringLiteral("light"));
        m_mission = new Mission(this);
        m_mission->newMission();
        m_mission->setOffline(true); // no network in the test
        m_engine = new QQmlApplicationEngine(this);
        m_app->setEngine(m_engine);
        connect(m_app, &AppController::languageChanged, m_mission, &Mission::retranslate);
        m_engine->rootContext()->setContextProperty(QStringLiteral("app"), m_app);
        m_engine->rootContext()->setContextProperty(QStringLiteral("mission"), m_mission);
        m_engine->load(QUrl::fromLocalFile(QStringLiteral(SATER_QML_DIR "/Main.qml")));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(m_window);
        m_window->setVisibility(QWindow::Windowed);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        resize(1400, 860);
        noWarnings();
    }

    void desktopLayout()
    {
        QVERIFY(item("desktopLayout"));
        QVERIFY(!item("phoneLayout"));
        QQuickItem *add = item("addStationButton");
        QVERIFY(add);
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(add));
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(add));
        QTRY_COMPARE(m_mission->stations()->count(), 2);
        // Fill them as a user would, through the model the table edits.
        StationModel *s = m_mission->stations();
        s->setField(0, QStringLiteral("callsign"), QStringLiteral("F4AAA"));
        s->setPosition(0, 43.758, 7.418);
        s->setField(0, QStringLiteral("azimuth"), 248);
        s->setField(1, QStringLiteral("callsign"), QStringLiteral("F1BBB"));
        s->setPosition(1, 43.662, 7.15);
        s->setField(1, QStringLiteral("azimuth"), 38);
        QVERIFY(m_mission->hasZone());
        // The record button of the first row.
        QQuickItem *list = item("stationList");
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 2);
        // The rows are made at the next polish, before the next frame.
        QTRY_VERIFY(item("recordButton"));
        QQuickItem *record = item("recordButton");
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(record));
        QTRY_COMPARE(m_mission->history()->count(), 1);
        // The best estimate is shown with its MGRS reference.
        QVERIFY(m_mission->hasFix());
        QTRY_VERIFY(item("fixMgrsLabel") && item("fixMgrsLabel")->isVisible());
        QVERIFY(item("fixLabel")->property("text").toString().contains(QStringLiteral("95 %")));
        QVERIFY(!item("fixWarning")->isVisible());
        // The north switch of a row: magnetic, then true again; the
        // estimate moves by the declination's worth.
        {
            QList<QQuickItem *> norths;
            allByName(item("stationList"), QStringLiteral("northButton"), &norths);
            QCOMPARE(norths.size(), 2);
            QQuickItem *north = nullptr;
            for (QQuickItem *n : norths) {
                if (!north || n->mapToScene(QPointF(0, 0)).y() < north->mapToScene(QPointF(0, 0)).y()) north = n;
            }
            QCOMPARE(north->property("text").toString(), QStringLiteral("T"));
            const double lat0 = m_mission->fixLat(), lon0 = m_mission->fixLon();
            QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(north));
            QTRY_VERIFY(m_mission->stations()->get(0).value(QStringLiteral("magnetic")).toBool());
            QTRY_COMPARE(north->property("text").toString(), QStringLiteral("M"));
            QVERIFY(std::fabs(m_mission->fixLat() - lat0) + std::fabs(m_mission->fixLon() - lon0) > 1e-4);
            QCOMPARE(north->height(), item("azimuthField")->height());
            QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(north));
            QTRY_VERIFY(!m_mission->stations()->get(0).value(QStringLiteral("magnetic")).toBool());
            QVERIFY(std::fabs(m_mission->fixLat() - lat0) < 1e-9);
        }
        // The N/S switch fills the height of the row, as the fields do (its
        // background included: Material keeps insets around it).
        QQuickItem *dir = item("rowLatDir");
        QQuickItem *az = item("azimuthField");
        QVERIFY(dir && az);
        QCOMPARE(dir->height(), az->height());
        auto *dirBg = dir->property("background").value<QQuickItem *>();
        QVERIFY(dirBg);
        QCOMPARE(dirBg->height(), dir->height());
        // The callsign hint is gone while the field has the focus or holds
        // text (Material would float it over the frame, half cut).
        QList<QQuickItem *> calls;
        allByName(item("stationList"), QStringLiteral("callsignField"), &calls);
        QVERIFY(!calls.isEmpty());
        QQuickItem *call = calls.first();
        QVERIFY(!call->property("text").toString().isEmpty());
        QCOMPARE(call->property("placeholderText").toString(), QString());
        const QString saved = call->property("text").toString();
        call->setProperty("text", QString());
        QVERIFY(!call->property("placeholderText").toString().isEmpty());
        call->forceActiveFocus();
        QTRY_VERIFY(call->hasActiveFocus());
        QCOMPARE(call->property("placeholderText").toString(), QString());
        call->setProperty("text", saved);
        m_window->contentItem()->forceActiveFocus();
        // The narrowest desktop layout: the panel and the map side by side,
        // the map whole inside the window.
        resize(1000, 800);
        QTRY_VERIFY(item("desktopLayout"));
        QQuickItem *map = item("mapView");
        QVERIFY(map);
        QTRY_VERIFY(map->width() >= 200);
        const QRectF mapRect = map->mapRectToScene(QRectF(0, 0, map->width(), map->height()));
        QVERIFY2(mapRect.right() <= m_window->width() + 1,
                 qPrintable(QStringLiteral("map spans %1..%2").arg(mapRect.left()).arg(mapRect.right())));
        // A short screen: the panel scrolls, the table keeps 150 px, and the
        // buttons at the bottom stay within reach.
        resize(1000, 640);
        {
            QQuickItem *flick = item("panelFlick");
            QVERIFY(flick);
            QTRY_VERIFY(flick->property("interactive").toBool());
            QQuickItem *content = flick->property("contentItem").value<QQuickItem *>();
            QQuickItem *history = item("historyButton");
            QVERIFY(content && history);
            const QRectF r = history->mapRectToItem(content, QRectF(0, 0, history->width(), history->height()));
            QVERIFY2(r.bottom() <= flick->property("contentHeight").toDouble() + 0.5, qPrintable(QString::number(r.bottom())));
            QVERIFY(item("stationList")->height() > 60);
        }
        resize(999, 800);
        QTRY_VERIFY(item("phoneLayout"));
        resize(1400, 860);
        QTRY_VERIFY(item("desktopLayout"));
        noWarnings();
    }

    void mapGestures()
    {
        auto *map = qobject_cast<MapView *>(item("mapView"));
        QVERIFY(map);
        map->centerOn(43.7, 7.3, 11);
        QTest::qWait(50);
        const QPoint c = centre(map);
        // Drag: the map follows the pointer.
        const double lon0 = map->centerLon();
        QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, c);
        for (int i = 1; i <= 10; ++i) QTest::mouseMove(m_window, c + QPoint(i * 12, 0), 10);
        QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, c + QPoint(120, 0), 400);
        QTRY_VERIFY(map->centerLon() < lon0 - 0.01);
        // Wheel: one notch, one zoom level, around the pointer.
        const double z0 = map->zoom();
        QWheelEvent wheel(QPointF(map->width() / 2, map->height() / 2), map->mapToGlobal(QPointF(map->width() / 2, map->height() / 2)),
                          QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(map, &wheel);
        QTRY_COMPARE(map->zoom(), z0 + 1);
        // Right click: the coordinates, and "Station here".
        QTest::mouseClick(m_window, Qt::RightButton, Qt::NoModifier, c + QPoint(-60, 40));
        QObject *ctx = object("contextPopup");
        QVERIFY(ctx);
        QTRY_VERIFY(ctx->property("opened").toBool());
        QQuickItem *here = item("addStationHere");
        QVERIFY(here);
        const int before = m_mission->stations()->count();
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(here));
        QTRY_COMPARE(m_mission->stations()->count(), before + 1);
        QObject *editor = object("stationEditor");
        QTRY_VERIFY(editor->property("opened").toBool());
        auto *editorItem = editor->property("contentItem").value<QQuickItem *>();
        QVERIFY(editorItem);
        // Decimal degrees take the six decimals they show (a GPS position).
        QList<QQuickItem *> dd;
        allByName(editorItem, QStringLiteral("latDd"), &dd);
        QCOMPARE(dd.size(), 1);
        dd.first()->setProperty("text", QStringLiteral("43.712345"));
        QVERIFY(dd.first()->property("acceptableInput").toBool());
        dd.first()->setProperty("text", QStringLiteral("43,7123"));
        QVERIFY(dd.first()->property("acceptableInput").toBool());
        dd.first()->setProperty("text", QStringLiteral("43.7123456"));
        QVERIFY(!dd.first()->property("acceptableInput").toBool());
        // The callsign given in the editor shows in the table row as well
        // (the row's field is set once, when the row is made).
        QList<QQuickItem *> call;
        allByName(editorItem, QStringLiteral("editorCallsign"), &call);
        QCOMPARE(call.size(), 1);
        call.first()->setProperty("text", QStringLiteral("F9ZZZ"));
        QMetaObject::invokeMethod(editor, "accept");
        // Not just closed: gone, its exit transition over (a modal
        // dialog's dimmer takes the input until then).
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(m_mission->stations()->get(before).value(QStringLiteral("callsign")).toString(), QStringLiteral("F9ZZZ"));
        QTRY_VERIFY(callsignShown(QStringLiteral("F9ZZZ")));
        m_mission->stations()->remove(before);
        noWarnings();
    }

    void touchGestures()
    {
        auto *map = qobject_cast<MapView *>(item("mapView"));
        map->centerOn(43.7, 7.3, 11);
        QTest::qWait(50);
        static QPointingDevice *touch = QTest::createTouchDevice();
        const QPoint c = centre(map);

        // A finger held still: the coordinates of the point.
        QObject *ctx = object("contextPopup");
        QSignalSpy longPress(map, &MapView::contextRequested);
        QTest::touchEvent(m_window, touch).press(0, c);
        QTRY_COMPARE_WITH_TIMEOUT(longPress.count(), 1, 1500);
        QTRY_VERIFY(ctx->property("opened").toBool());
        QTest::touchEvent(m_window, touch).release(0, c);
        QTest::qWait(50);
        QVERIFY2(ctx->property("opened").toBool(), "the popup closed when the finger lifted");
        QMetaObject::invokeMethod(ctx, "close");
        QTRY_VERIFY(!ctx->property("visible").toBool());

        // Two fingers moving apart: zoom in, settled on a whole level.
        const double z0 = map->zoom();
        QPoint a = c - QPoint(40, 0), b = c + QPoint(40, 0);
        QTest::touchEvent(m_window, touch).press(0, a).press(1, b);
        for (int i = 1; i <= 10; ++i) {
            QTest::touchEvent(m_window, touch).move(0, a - QPoint(i * 4, 0)).move(1, b + QPoint(i * 4, 0));
            QTest::qWait(10);
        }
        // 80 px apart -> 160: one level.
        QTest::touchEvent(m_window, touch).release(0, a - QPoint(40, 0)).release(1, b + QPoint(40, 0));
        QTRY_COMPARE(map->zoom(), z0 + 1);

        // One finger: a pan.
        const double lat0 = map->centerLat();
        QTest::touchEvent(m_window, touch).press(0, c);
        for (int i = 1; i <= 10; ++i) {
            QTest::touchEvent(m_window, touch).move(0, c + QPoint(0, i * 10));
            QTest::qWait(10);
        }
        QTest::touchEvent(m_window, touch).release(0, c + QPoint(0, 100));
        QTRY_VERIFY(map->centerLat() > lat0);
        noWarnings();
    }

    void dragStationLabel()
    {
        auto *map = qobject_cast<MapView *>(item("mapView"));
        map->centerOn(43.72, 7.29, 11);
        QTest::qWait(100);
        const QPointF at = map->toScreen(43.662, 7.15);
        // The label starts at the station and runs right: press inside it.
        const QPoint p = map->mapToScene(at + QPointF(15, 0)).toPoint();
        const double lat0 = m_mission->stations()->get(1).value(QStringLiteral("lat")).toDouble();
        QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, p);
        for (int i = 1; i <= 8; ++i) QTest::mouseMove(m_window, p + QPoint(0, -i * 8), 10);
        QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, p + QPoint(0, -64), 10);
        const double lat1 = m_mission->stations()->get(1).value(QStringLiteral("lat")).toDouble();
        QVERIFY2(lat1 > lat0 + 0.01, qPrintable(QStringLiteral("%1 -> %2").arg(lat0).arg(lat1)));
        noWarnings();
    }

    void dialogsOpen()
    {
        const char *dialogs[] = {"historyDialog", "kilometersDialog", "preferencesDialog", "aboutDialog",
                                 "aprsDialog", "aprsLogDialog"};
        for (const char *name : dialogs) {
            QObject *d = object(name);
            QVERIFY2(d, name);
            QMetaObject::invokeMethod(d, "open");
            QTRY_VERIFY(d->property("opened").toBool());
            QMetaObject::invokeMethod(d, "close");
            QTRY_VERIFY(!d->property("opened").toBool());
        }
        QMetaObject::invokeMethod(m_window, "setBeacon", Q_ARG(QVariant, QVariant()), Q_ARG(QVariant, QVariant()));
        QObject *beacon = object("beaconDialog");
        QTRY_VERIFY(beacon->property("opened").toBool());
        QMetaObject::invokeMethod(beacon, "accept");
        QTRY_VERIFY(m_mission->hasBeacon());
        QMetaObject::invokeMethod(m_window, "generatePdf");
        QObject *report = object("reportDialog");
        QTRY_VERIFY(report->property("opened").toBool());
        QMetaObject::invokeMethod(report, "close");
        // The file dialog (Qt's own on this platform) opens for an export.
        QMetaObject::invokeMethod(m_window, "exportJson");
        QObject *files = object("fileDialog");
        QVERIFY(files);
        QTRY_VERIFY(files->property("visible").toBool());
        QMetaObject::invokeMethod(files, "reject");
        QTRY_VERIFY(!files->property("visible").toBool());
        QMetaObject::invokeMethod(m_window, "managePresets");
        QObject *presets = object("presetsDialog");
        QTRY_VERIFY(presets->property("opened").toBool());
        QMetaObject::invokeMethod(presets, "close");
        QTRY_VERIFY(!presets->property("opened").toBool());
        noWarnings();
    }

    void phoneLayoutFits()
    {
        resize(360, 740);
        QTRY_VERIFY(item("phoneLayout"));
        QVERIFY(!item("desktopLayout"));
        QQuickItem *phone = item("phoneLayout");
        for (int page = 0; page < 4; ++page) {
            phone->setProperty("pageIndex", page);
            QTest::qWait(150);
            checkFits(QStringLiteral("phone page %1").arg(page));
        }
        // The back gesture: from a page to the map, then out.
        phone->setProperty("pageIndex", 2);
        QVariant handled;
        QMetaObject::invokeMethod(phone, "back", Q_RETURN_ARG(QVariant, handled));
        QVERIFY(handled.toBool());
        QCOMPARE(phone->property("pageIndex").toInt(), 0);
        QMetaObject::invokeMethod(phone, "back", Q_RETURN_ARG(QVariant, handled));
        QVERIFY(!handled.toBool());
        // The station cards: the step buttons move the bearing.
        phone->setProperty("pageIndex", 1);
        QTest::qWait(150);
        const double az0 = m_mission->stations()->get(0).value(QStringLiteral("azimuth")).toDouble();
        QQuickItem *cards = item("stationCards");
        QVERIFY(cards);
        QQuickItem *plusTen = nullptr;
        std::function<void(QQuickItem *)> find = [&](QQuickItem *i) {
            if (plusTen) return;
            if (i->property("delta").isValid() && i->property("delta").toInt() == 10 && i->isVisible()) {
                plusTen = i;
                return;
            }
            for (QQuickItem *c : i->childItems()) find(c);
        };
        find(cards);
        QVERIFY(plusTen);
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(plusTen));
        QTRY_COMPARE(m_mission->stations()->get(0).value(QStringLiteral("azimuth")).toDouble(), std::fmod(az0 + 10, 360.0));

        // This station's card gets a Send button once its callsign is set
        // as the APRS callsign; with no connection the bearing stays here.
        AprsLink *aprs = m_mission->aprs();
        const QString own = m_mission->stations()->get(0).value(QStringLiteral("callsign")).toString();
        auto sendButtons = [&]() {
            QList<QQuickItem *> all, shown;
            allByName(cards, QStringLiteral("sendButton"), &all);
            for (QQuickItem *b : all) {
                if (b->isVisible()) shown.append(b);
            }
            return shown;
        };
        QVERIFY(sendButtons().isEmpty());
        aprs->setMyCallsign(own + QStringLiteral("-7"));
        QTRY_COMPARE(sendButtons().size(), 1);
        const int records = m_mission->history()->count();
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(sendButtons().first()));
        QTRY_VERIFY(m_mission->statusMessage().contains(QStringLiteral("could not be sent")));
        QCOMPARE(m_mission->history()->count(), records);
        aprs->setMyCallsign(QString());
        QTRY_VERIFY(sendButtons().isEmpty());

        // A team's bearing comes in: its card shows it.
        m_mission->setTeam({own});
        aprs->handlePacket(*sater::aprs::parseTnc2(QByteArray(own.toLatin1()) + "-9>APZSAT:!4345.48N/00725.08E\\000/000/123/965 SATER S8 U3.0"),
                           QStringLiteral("RF"));
        QTRY_COMPARE(m_mission->stations()->get(0).value(QStringLiteral("azimuth")).toDouble(), 123.0);
        m_mission->setTeam({});
        noWarnings();
    }

    // The APRS settings: the operation's callsigns added and removed.
    void aprsSettings()
    {
        QObject *dialog = object("aprsDialog");
        QVERIFY(dialog);
        QMetaObject::invokeMethod(dialog, "open");
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *content = dialog->property("contentItem").value<QQuickItem *>();
        QList<QQuickItem *> fields, adds;
        allByName(content, QStringLiteral("teamField"), &fields);
        allByName(content, QStringLiteral("teamAdd"), &adds);
        QCOMPARE(fields.size(), 1);
        QCOMPARE(adds.size(), 1);
        fields.first()->setProperty("text", QStringLiteral("F5XYZ"));
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centre(adds.first()));
        QTRY_COMPARE(m_mission->team(), QStringList{QStringLiteral("F5XYZ")});
        QCOMPARE(fields.first()->property("text").toString(), QString());
        QList<QQuickItem *> chips;
        QTRY_VERIFY((chips.clear(), allByName(content, QStringLiteral("teamChip"), &chips), chips.size() == 1));
        m_mission->setTeam({});
        // The built-in modem's card, where the program has one; its PTT
        // choice follows the setting.
        AprsLink *aprs = m_mission->aprs();
        QList<QQuickItem *> cards, ptt;
        allByName(content, QStringLiteral("modemCard"), &cards);
        QCOMPARE(cards.size(), 1);
        QCOMPARE(cards.first()->isVisible(), aprs->modemAvailable());
        allByName(content, QStringLiteral("modemPtt"), &ptt);
        QCOMPARE(ptt.size(), 1);
        aprs->setModemPtt(QStringLiteral("cm108"));
        QTRY_COMPARE(ptt.first()->property("currentIndex").toInt(), 1);
        aprs->setModemPtt(QStringLiteral("none"));
        QTRY_COMPARE(ptt.first()->property("currentIndex").toInt(), 0);
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        noWarnings();
    }

    void frenchFits()
    {
        m_app->setLanguage(QStringLiteral("fr"));
        QTest::qWait(150);
        m_mission->setShowIntersection(false);
        QCOMPARE(m_mission->zoneText(), QStringLiteral("Zone d'intersection: -"));
        m_mission->setShowIntersection(true);
        // The first station is this one: its card shows the Send button.
        AprsLink *aprs = m_mission->aprs();
        aprs->setMyCallsign(m_mission->stations()->get(0).value(QStringLiteral("callsign")).toString());
        const auto dialogs = [this](const QString &where) {
            const char *names[] = {"stationEditor", "beaconDialog", "reportDialog", "kilometersDialog", "aprsDialog"};
            for (const char *name : names) {
                QObject *d = object(name);
                if (QLatin1String(name) == QLatin1String("stationEditor"))
                    QMetaObject::invokeMethod(d, "openFor", Q_ARG(QVariant, 0));
                else if (QLatin1String(name) == QLatin1String("beaconDialog"))
                    QMetaObject::invokeMethod(d, "openAt", Q_ARG(QVariant, QVariant()), Q_ARG(QVariant, QVariant()));
                else
                    QMetaObject::invokeMethod(d, "open");
                QTRY_VERIFY(d->property("opened").toBool());
                QTest::qWait(150);
                checkFits(QStringLiteral("French %1 %2").arg(where, QLatin1String(name)));
                QMetaObject::invokeMethod(d, "close");
                QTRY_VERIFY(!d->property("opened").toBool());
            }
        };
        // Phones of several widths: a button's text that fits on one may
        // not on the next (the record button's label shows when it fits).
        for (const int w : {360, 393, 412, 430}) {
            resize(w, 860);
            QQuickItem *phone = nullptr;
            QTRY_VERIFY((phone = item("phoneLayout")));
            for (int page = 0; page < 4; ++page) {
                phone->setProperty("pageIndex", page);
                QTest::qWait(150);
                checkFits(QStringLiteral("French phone %1 px page %2").arg(w).arg(page));
            }
            if (w == 360) dialogs(QStringLiteral("phone"));
        }
        // The station cards across the widths a phone can have: the record
        // button shows its word only where it fits.
        for (int w = 340; w <= 560; w += 6) {
            resize(w, 860);
            QQuickItem *phone = nullptr;
            QTRY_VERIFY((phone = item("phoneLayout")));
            phone->setProperty("pageIndex", 1);
            QTest::qWait(40);
            checkFits(QStringLiteral("French phone %1 px, stations").arg(w));
        }
        resize(1400, 860);
        QTRY_VERIFY(item("desktopLayout"));
        QTest::qWait(150);
        checkFits(QStringLiteral("French desktop"));
        dialogs(QStringLiteral("desktop"));
        aprs->setMyCallsign(QString());
        resize(360, 740);
        QTRY_VERIFY(item("phoneLayout"));
        m_app->setLanguage(QStringLiteral("en"));
        resize(1400, 860);
        QTRY_VERIFY(item("desktopLayout"));
        noWarnings();
    }

    void cleanupTestCase()
    {
        m_mission->newMission();
        m_mission->setOffline(false);
        m_mission->saveNow();
        m_app->setLanguage(QStringLiteral("auto"));
        delete m_engine;
        m_engine = nullptr;
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
