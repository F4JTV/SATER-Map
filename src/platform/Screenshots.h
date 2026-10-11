/*
 * Screenshots.h - render the interface to PNG files and quit.
 *
 * SATER_SCREENSHOT_DIR=<dir> sater-map: a sample mission, then the window
 * at desktop size and at phone size in each theme, plus the dialogs.
 * Used to check the layouts headless (QT_QPA_PLATFORM=offscreen).
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QString>

class AppController;
class Mission;
class QQuickWindow;

namespace screenshots {

void run(QQuickWindow *window, AppController *app, Mission *mission, const QString &dir);

} // namespace screenshots
