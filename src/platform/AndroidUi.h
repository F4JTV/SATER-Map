/*
 * AndroidUi.h - what the interface asks of the Android system.
 *
 * Full screen: the status and navigation bars are hidden, and come back
 * transiently on a swipe from the edge - AX25Chat's behaviour, its code
 * unchanged.  Done from C++ through the activity's window, so the
 * application needs no activity subclass.  No-ops on other platforms.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QString>

namespace androidui {

// Hide the system bars.  Called at start and whenever the application
// comes back to the foreground: the bars return after a swipe or a switch
// to another application, and would otherwise stay.
void hideSystemBars();

// A short "virtual key" vibration, as the system keyboard gives.  Follows
// the phone's own touch feedback setting.
void haptic();

// Keep the screen on while the application is in front (a map consulted
// during a mission must not go dark).
void setKeepScreenOn(bool on);

// Hand the application context to the Java helpers (DeviceServices.java);
// once, at start.
void installServices();

// The foreground service that keeps the process running minimised or
// with the phone locked (KeepAliveService.java): on while APRS is, with
// the microphone type while the built-in modem may listen.  The texts are
// those of its notification.  Android starts (or changes) such a service
// only for an application on screen: while hidden only a stop goes out,
// the rest waits for the next call with the application back.  Calls that
// change nothing are not sent; with check, what runs is compared with
// what is wanted (after the system stopped the service, say).
void keepRunning(bool on, bool microphone, const QString &title, const QString &text, const QString &channel,
                 bool check = false);

// The exemption from battery optimisation (Doze would cut the network
// with the screen off): granted, and the system's dialog asking for it.
bool backgroundAllowed();
void requestBackground();

// The notification permission (Android 13 and later): granted, and the
// request.
bool notificationsAllowed();
void requestNotifications();

// A notification (a bearing received while the application is hidden).
void notify(const QString &channel, const QString &title, const QString &text);

} // namespace androidui
