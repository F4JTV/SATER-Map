/*
 * main.cpp - SATER Map: direction finding for SATER missions (search for
 * distress beacons), with the bearings of several stations on a map and
 * the zone where they cross.  One Qt Quick interface for Windows, Linux
 * and Android; it lays itself out for a desktop window or a phone.
 *
 * SATER_SCREENSHOT_DIR=<dir> renders the interface in each theme, at
 * desktop and phone size, saves PNGs there and quits (used to check the
 * layouts without a phone).
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/AppController.h"
#include "app/AprsLink.h"
#include "app/Mission.h"
#include "core/Tr.h"
#include "map/MapView.h"
#include "map/TileManager.h"
#include "platform/AndroidUi.h"
#include "platform/Screenshots.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif

int main(int argc, char **argv)
{
#ifndef Q_OS_ANDROID
    // A denser Material on the desktop: the station table has 16 columns.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_VARIANT"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
#endif
    QGuiApplication app(argc, argv);
    // The settings and data folder names of the Python version.
    app.setOrganizationName(QStringLiteral("SATER_Map"));
    app.setApplicationName(QStringLiteral("SATER_Map"));
    app.setApplicationDisplayName(QStringLiteral("SATER Map"));
    app.setApplicationVersion(QStringLiteral(SATER_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    qmlRegisterType<MapView>("SaterMap.Native", 1, 0, "MapView");
    qmlRegisterUncreatableType<Mission>("SaterMap.Native", 1, 0, "Mission", QStringLiteral("Use the mission object"));
    qmlRegisterUncreatableType<StationModel>("SaterMap.Native", 1, 0, "StationModel", QStringLiteral("From mission"));
    qmlRegisterUncreatableType<HistoryModel>("SaterMap.Native", 1, 0, "HistoryModel", QStringLiteral("From mission"));
    qmlRegisterUncreatableType<PresetModel>("SaterMap.Native", 1, 0, "PresetModel", QStringLiteral("From mission"));
    qmlRegisterUncreatableType<TileManager>("SaterMap.Native", 1, 0, "TileManager", QStringLiteral("From mission"));
    qmlRegisterUncreatableType<AprsLink>("SaterMap.Native", 1, 0, "AprsLink", QStringLiteral("From mission"));

    // Screenshots run on a sample mission: never on the user's own data.
    if (!qEnvironmentVariableIsEmpty("SATER_SCREENSHOT_DIR")) QStandardPaths::setTestModeEnabled(true);

    AppController controller;
    Mission mission;

#ifdef Q_OS_ANDROID
    // Full screen, as AX25Chat: the bars are hidden now and again each time
    // the application comes back.
    androidui::hideSystemBars();
    androidui::installServices();
#endif
    // APRS goes on minimised or with the phone locked (Android): a
    // foreground service while a connection is on, with the microphone
    // type while the built-in modem is on and may listen.  Looked at again
    // each time the application comes back (the system may have stopped
    // the service meanwhile).
    AprsLink *aprs = mission.aprs();
    const auto keepRunning = [aprs](bool check) {
        bool microphone = aprs->modemAvailable() && aprs->modemEnabled();
#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        microphone = microphone && qApp->checkPermission(QMicrophonePermission{}) == Qt::PermissionStatus::Granted;
#endif
        androidui::keepRunning(aprs->active(), microphone, SATER_TR("SATER Map: APRS on"),
                               SATER_TR("Bearings keep going out and coming in with the screen off."),
                               SATER_TR("APRS in the background"), check);
    };
    // Then, once, with the application on screen and the microphone
    // question settled (Android takes one request at a time and drops the
    // next): the notifications, and at the next return to the screen the
    // exemption from battery saving.
    const auto askOnce = [aprs, &app, &controller]() {
        static bool notificationsAsked = false, batteryAsked = false;
        if (!aprs->active() || app.applicationState() != Qt::ApplicationActive) return;
#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (aprs->modemAvailable() && aprs->modemEnabled()
            && app.checkPermission(QMicrophonePermission{}) == Qt::PermissionStatus::Undetermined)
            return;
#endif
        if (!notificationsAsked) {
            notificationsAsked = true;
            if (!androidui::notificationsAllowed()) {
                androidui::requestNotifications();
                return;      // its dialog first
            }
        }
        if (!batteryAsked) {
            batteryAsked = true;
            if (!controller.backgroundAllowed()) androidui::requestBackground();
        }
    };
    const auto update = [keepRunning, askOnce]() {
        keepRunning(false);
        askOnce();
    };
    QObject::connect(aprs, &AprsLink::stateChanged, &app, update);
    QObject::connect(aprs, &AprsLink::settingsChanged, &app, update);
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &app, [keepRunning, askOnce](Qt::ApplicationState s) {
        if (s != Qt::ApplicationActive) return;
        keepRunning(true);
        askOnce();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [] { androidui::keepRunning(false, false, QString(), QString(), QString()); });
    // A bearing received while hidden: a notification.
    QObject::connect(&mission, &Mission::bearingReceived, &app,
                     [&app](const QString &callsign, double azimuth, double uncertainty) {
                         if (app.applicationState() == Qt::ApplicationActive) return;
                         androidui::notify(SATER_TR("Bearings received"), SATER_TR("Bearing received"),
                                           SATER_TR("%1: %2° ±%3°").arg(callsign).arg(qRound(azimuth) % 360)
                                               .arg(uncertainty, 0, 'f', 1));
                     });
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &mission,
                     [&controller, &mission](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive) controller.activated();
                         // Android may end a suspended application without
                         // warning: the mission is on disk before that.
                         else mission.saveNow();
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &mission, &Mission::saveNow);

    QObject::connect(&controller, &AppController::languageChanged, &mission, &Mission::retranslate);

    QQmlApplicationEngine engine;
    controller.setEngine(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("mission"), &mission);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    // The module is compiled into the binary under /qt/qml; the URL form
    // works on every Qt 6, loadFromModule() only from 6.5.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SaterMap/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    // Once the window is up: Android starts a foreground service only for
    // an application on screen.
    QTimer::singleShot(0, &app, update);

    const QString shotDir = qEnvironmentVariable("SATER_SCREENSHOT_DIR");
    if (!shotDir.isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
            screenshots::run(window, &controller, &mission, shotDir);
    }
    return app.exec();
}
