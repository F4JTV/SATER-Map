/*
 * Screenshots.cpp - render the interface to PNG files and quit.
 *
 * A sample mission (three stations around Nice, a beacon, a few bearings
 * recorded), then: the desktop layout in the four themes, the phone layout's
 * four pages, and the main dialogs.  One step every 700 ms, so the QML has
 * settled (and tiles, when there is a network, have arrived) before each
 * grab.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Screenshots.h"

#include "app/AppController.h"
#include "app/AprsLink.h"
#include "app/Mission.h"
#include "aprs/Ax25.h"

#include <QCoreApplication>
#include <QDir>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

#include <functional>
#include <memory>

using namespace sater;

namespace {

void sampleMission(Mission *m)
{
    m->newMission();
    StationModel *s = m->stations();
    // Bearings on a point near Nice, each a degree or two off, F4AAA's read
    // on a compass (248 degrees magnetic, some 251 true).
    struct S { const char *call; double lat, lon, az, unc; const char *sig; bool magnetic; };
    const S list[] = {{"F4AAA", 43.7580, 7.4180, 248, 5, "S7", true},
                      {"F1BBB", 43.6620, 7.1500, 51, 4, "S9", false},
                      {"F5CCC", 43.6000, 7.3300, 326, 6, "S5", false}};
    for (const S &st : list) {
        const int r = m->addStation();
        s->setField(r, QStringLiteral("callsign"), QString::fromLatin1(st.call));
        s->setPosition(r, st.lat, st.lon);
        s->setField(r, QStringLiteral("magnetic"), st.magnetic);
        s->setField(r, QStringLiteral("azimuth"), st.az);
        s->setField(r, QStringLiteral("uncertainty"), st.unc);
        s->setField(r, QStringLiteral("signal"), QString::fromLatin1(st.sig));
        m->recordStation(r);
    }
    if (m->hasFix()) m->setBeacon(m->fixLat(), m->fixLon());
    else m->setBeacon(m->zoneLat(), m->zoneLon());
    m->setKilometers({{QStringLiteral("F4AAA"), 18.5}, {QStringLiteral("F1BBB"), 7.2}});
    // APRS: the operation's team, this station F4AAA, a bearing from F5CCC
    // heard by radio and one from a station outside the operation.
    m->setTeam({QStringLiteral("F4AAA"), QStringLiteral("F1BBB"), QStringLiteral("F5CCC")});
    m->aprs()->setMyCallsign(QStringLiteral("F4AAA-7"));
    m->aprs()->handlePacket(*aprs::parseTnc2("F5CCC-9>APZSAT,F1ZZZ-3*,WIDE2:!4336.00N/00719.80E\\000/000/326/965 SATER S5 U6.0"),
                            QStringLiteral("RF"));
    m->aprs()->handlePacket(*aprs::parseTnc2("F6XYZ-7>APDR16,WIDE1-1:=4341.20N/00715.10E>Mobile"), QStringLiteral("RF"));
    m->startTimer();
}

QQuickItem *findItem(QQuickWindow *w, const char *name)
{
    return w->findChild<QQuickItem *>(QLatin1String(name));
}

QObject *findObject(QQuickWindow *w, const char *name)
{
    return w->findChild<QObject *>(QLatin1String(name));
}

} // namespace

void screenshots::run(QQuickWindow *window, AppController *app, Mission *mission, const QString &dir)
{
    QDir().mkpath(dir);
    window->setVisibility(QWindow::Windowed);
    // SATER_SCREENSHOT_LANG=fr|en: the interface language for the run.
    const QString lang = qEnvironmentVariable("SATER_SCREENSHOT_LANG");
    if (!lang.isEmpty()) app->setLanguage(lang);
    sampleMission(mission);

    auto steps = std::make_shared<QList<std::function<void()>>>();
    auto grab = [window, dir](const QString &name) {
        return [window, dir, name]() { window->grabWindow().save(QDir(dir).filePath(name + QStringLiteral(".png"))); };
    };
    auto centre = [window]() {
        if (QObject *map = findObject(window, "mapView"))
            QMetaObject::invokeMethod(map, "centerOn", Q_ARG(double, 43.69), Q_ARG(double, 7.29), Q_ARG(double, 11));
    };

    // Desktop, four themes.
    const QStringList themes = {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("red"),
                                QStringLiteral("amber")};
    steps->append([window, centre]() { window->resize(1400, 860); });
    steps->append(centre);
    for (const QString &t : themes) {
        steps->append([app, t]() { app->setTheme(t); });
        steps->append(grab(QStringLiteral("desktop-") + t));
    }
    // Desktop dialogs.
    steps->append([app, mission]() {
        app->setTheme(QStringLiteral("light"));
        mission->setShowGrid(true);
    });
    steps->append(grab(QStringLiteral("desktop-grid")));
    steps->append([window, mission]() {
        mission->setShowGrid(false);
        QMetaObject::invokeMethod(window, "editStation", Q_ARG(QVariant, 0));
    });
    steps->append(grab(QStringLiteral("desktop-station-editor")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "stationEditor")) QMetaObject::invokeMethod(d, "close");
        QMetaObject::invokeMethod(window, "showHistory");
    });
    steps->append(grab(QStringLiteral("desktop-history")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "historyDialog")) QMetaObject::invokeMethod(d, "close");
        QMetaObject::invokeMethod(window, "setBeacon", Q_ARG(QVariant, QVariant()), Q_ARG(QVariant, QVariant()));
    });
    steps->append(grab(QStringLiteral("desktop-beacon")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "beaconDialog")) QMetaObject::invokeMethod(d, "close");
        QMetaObject::invokeMethod(window, "generatePdf");
    });
    steps->append(grab(QStringLiteral("desktop-report")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "reportDialog")) QMetaObject::invokeMethod(d, "close");
        if (QObject *d = findObject(window, "kilometersDialog")) QMetaObject::invokeMethod(d, "open");
    });
    steps->append(grab(QStringLiteral("desktop-kilometers")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "kilometersDialog")) QMetaObject::invokeMethod(d, "close");
        if (QObject *d = findObject(window, "preferencesDialog")) QMetaObject::invokeMethod(d, "open");
    });
    steps->append(grab(QStringLiteral("desktop-preferences")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "preferencesDialog")) QMetaObject::invokeMethod(d, "close");
        if (QObject *d = findObject(window, "aprsDialog")) QMetaObject::invokeMethod(d, "open");
    });
    steps->append(grab(QStringLiteral("desktop-aprs")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "aprsDialog")) QMetaObject::invokeMethod(d, "close");
        if (QObject *d = findObject(window, "aprsLogDialog")) QMetaObject::invokeMethod(d, "open");
    });
    steps->append(grab(QStringLiteral("desktop-aprs-log")));

    // Phone: the four pages, then the map in the night theme.
    steps->append([window]() {
        if (QObject *d = findObject(window, "aprsLogDialog")) QMetaObject::invokeMethod(d, "close");
        window->resize(393, 873);
    });
    steps->append(centre);
    const char *pages[] = {"map", "stations", "mission", "settings"};
    for (int i = 0; i < 4; ++i) {
        steps->append([window, i]() {
            if (QQuickItem *l = findItem(window, "phoneLayout")) l->setProperty("pageIndex", i);
        });
        steps->append(grab(QStringLiteral("phone-") + QLatin1String(pages[i])));
    }
    for (const QString &t : {QStringLiteral("red"), QStringLiteral("dark")}) {
        steps->append([window, app, t]() {
            app->setTheme(t);
            if (QQuickItem *l = findItem(window, "phoneLayout")) l->setProperty("pageIndex", 0);
        });
        steps->append(grab(QStringLiteral("phone-map-") + t));
    }
    steps->append([window, app]() {
        app->setTheme(QStringLiteral("light"));
        if (QQuickItem *l = findItem(window, "phoneLayout")) l->setProperty("pageIndex", 1);
        QMetaObject::invokeMethod(window, "editStation", Q_ARG(QVariant, 1));
    });
    steps->append(grab(QStringLiteral("phone-station-editor")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "stationEditor")) QMetaObject::invokeMethod(d, "close");
        if (QObject *m = findObject(window, "mainMenu")) QMetaObject::invokeMethod(m, "open");
    });
    steps->append(grab(QStringLiteral("phone-menu")));
    // The APRS settings on the phone: the top, then the radio cards.
    steps->append([window]() {
        if (QObject *m = findObject(window, "mainMenu")) QMetaObject::invokeMethod(m, "close");
        if (QObject *d = findObject(window, "aprsDialog")) QMetaObject::invokeMethod(d, "open");
    });
    steps->append(grab(QStringLiteral("phone-aprs")));
    steps->append([window]() {
        QObject *d = findObject(window, "aprsDialog");
        auto *flick = d ? d->property("contentItem").value<QQuickItem *>() : nullptr;
        if (flick) {
            const qreal bottom = flick->property("contentHeight").toReal() - flick->height();
            flick->setProperty("contentY", qMax<qreal>(0, bottom));
        }
    });
    steps->append(grab(QStringLiteral("phone-aprs-radio")));
    steps->append([window]() {
        if (QObject *d = findObject(window, "aprsDialog")) QMetaObject::invokeMethod(d, "close");
    });
    steps->append([mission, app]() {
        mission->aprs()->setMyCallsign(QString());
        mission->setTeam({});
        mission->newMission();
        mission->saveNow();
        app->setLanguage(QStringLiteral("auto"));
        app->setTheme(QStringLiteral("light"));
        QCoreApplication::quit();
    });

    auto *timer = new QTimer(window);
    auto index = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, window, [steps, index, timer]() {
        if (*index >= steps->size()) {
            timer->stop();
            return;
        }
        (*steps)[(*index)++]();
    });
    timer->start(700);
}
