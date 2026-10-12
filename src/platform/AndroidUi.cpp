/*
 * AndroidUi.cpp - what the interface asks of the Android system.
 *
 * hideSystemBars() and haptic() are AX25Chat's, unchanged; the service
 * that keeps the application running in the background and the device
 * helpers are AX25Chess's (android/src/org/satermap/app).
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AndroidUi.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

void androidui::hideSystemBars()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        if (QNativeInterface::QAndroidApplication::sdkVersion() >= 30) {
            QJniObject controller = window.callObjectMethod("getInsetsController", "()Landroid/view/WindowInsetsController;");
            if (!controller.isValid()) return;
            const jint statusBars = QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", "statusBars", "()I");
            const jint navigationBars = QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", "navigationBars", "()I");
            // BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            controller.callMethod<void>("setSystemBarsBehavior", "(I)V", static_cast<jint>(2));
            controller.callMethod<void>("hide", "(I)V", static_cast<jint>(statusBars | navigationBars));
        } else {
            // Before Android 11: the old visibility flags (immersive sticky,
            // hide navigation, fullscreen, layout stable/hide/fullscreen).
            QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
            if (!decor.isValid()) return;
            const jint flags = 0x00001000 | 0x00000004 | 0x00000002 | 0x00000100 | 0x00000200 | 0x00000400;
            decor.callMethod<void>("setSystemUiVisibility", "(I)V", flags);
        }
    });
}

void androidui::haptic()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        if (!decor.isValid()) return;
        // HapticFeedbackConstants.VIRTUAL_KEY: needs no VIBRATE permission.
        decor.callMethod<jboolean>("performHapticFeedback", "(I)Z", static_cast<jint>(1));
    });
}

void androidui::setKeepScreenOn(bool on)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON: no permission.
        const jint flag = 0x00000080;
        if (on) window.callMethod<void>("addFlags", "(I)V", flag);
        else window.callMethod<void>("clearFlags", "(I)V", flag);
    });
}

namespace {
const char *const kKeepAlive = "org/satermap/app/KeepAliveService";
const char *const kDeviceServices = "org/satermap/app/DeviceServices";
}

void androidui::installServices()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;
    QJniObject::callStaticMethod<void>(kDeviceServices, "setContext", "(Landroid/content/Context;)V", context.object());
}

void androidui::keepRunning(bool on, bool microphone, const QString &title, const QString &text, const QString &channel,
                            bool check)
{
    static int last = -1;            // -1 never, 0 off, 1 on, 2 on with the microphone
    const int want = on ? (microphone ? 2 : 1) : 0;
    // Started or changed only on screen (ForegroundServiceStartNotAllowed,
    // and no microphone type for an application in the background).
    if (want != 0 && QGuiApplication::applicationState() != Qt::ApplicationActive) return;
    if (check) {
        const bool runs = QJniObject::callStaticMethod<jboolean>(kKeepAlive, "isRunning", "()Z");
        const bool hears = QJniObject::callStaticMethod<jboolean>(kKeepAlive, "hasMicrophone", "()Z");
        if (want == 0 ? !runs : (runs && (want == 1 || hears))) {
            last = want;
            return;
        }
    } else if (want == last) {
        return;
    }
    last = want;
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on, microphone, title, text, channel] {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid()) return;
        if (on) {
            QJniObject::callStaticMethod<void>(kKeepAlive, "start",
                "(Landroid/content/Context;ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                context.object(), jboolean(microphone),
                QJniObject::fromString(title).object<jstring>(),
                QJniObject::fromString(text).object<jstring>(),
                QJniObject::fromString(channel).object<jstring>());
        } else {
            QJniObject::callStaticMethod<void>(kKeepAlive, "stop", "(Landroid/content/Context;)V", context.object());
        }
    });
}

bool androidui::backgroundAllowed()
{
    return QJniObject::callStaticMethod<jboolean>(kDeviceServices, "ignoringBatteryOptimizations", "()Z");
}

void androidui::requestBackground()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject::callStaticMethod<void>(kDeviceServices, "requestIgnoreBatteryOptimizations", "()V");
    });
}

bool androidui::notificationsAllowed()
{
    return QJniObject::callStaticMethod<jboolean>(kDeviceServices, "notificationsAllowed", "()Z");
}

void androidui::requestNotifications()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject::callStaticMethod<void>(kDeviceServices, "requestNotificationPermission", "()V");
    });
}

void androidui::notify(const QString &channel, const QString &title, const QString &text)
{
    QJniObject::callStaticMethod<void>(kDeviceServices, "notifyBearing",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString(channel).object<jstring>(),
        QJniObject::fromString(title).object<jstring>(),
        QJniObject::fromString(text).object<jstring>());
}

#else

void androidui::hideSystemBars() {}
void androidui::haptic() {}
void androidui::setKeepScreenOn(bool) {}
void androidui::installServices() {}
void androidui::keepRunning(bool, bool, const QString &, const QString &, const QString &, bool) {}
bool androidui::backgroundAllowed() { return true; }
void androidui::requestBackground() {}
bool androidui::notificationsAllowed() { return true; }
void androidui::requestNotifications() {}
void androidui::notify(const QString &, const QString &, const QString &) {}

#endif
