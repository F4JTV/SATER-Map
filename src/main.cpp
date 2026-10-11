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
#endif
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

    const QString shotDir = qEnvironmentVariable("SATER_SCREENSHOT_DIR");
    if (!shotDir.isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
            screenshots::run(window, &controller, &mission, shotDir);
    }
    return app.exec();
}
