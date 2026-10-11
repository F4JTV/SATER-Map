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

} // namespace androidui
