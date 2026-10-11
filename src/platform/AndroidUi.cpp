/*
 * AndroidUi.cpp - what the interface asks of the Android system.
 *
 * hideSystemBars() and haptic() are AX25Chat's, unchanged.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AndroidUi.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
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

#else

void androidui::hideSystemBars() {}
void androidui::haptic() {}
void androidui::setKeepScreenOn(bool) {}

#endif
