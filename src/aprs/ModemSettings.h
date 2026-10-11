/*
 * ModemSettings.h - the modem built into the program (Dire Wolf): the
 * sound card it uses, how it keys the transmitter, and the direwolf.conf
 * written from that.  Compiled in every build, so the settings and the
 * file can be checked without the modem.
 *
 * The modem sends and hears APRS: 1200 baud AFSK on channel 0, one sound
 * card.  The audio goes through the radio interface (a USB sound card such
 * as a Digirig, or the headset jack); the transmitter is keyed by the
 * interface's VOX, a CM108 sound card's GPIO, or the RTS or DTR line of a
 * serial adapter.  On Android the USB device is found by the program and
 * the device name is not used.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QList>
#include <QString>

namespace sater::aprs {

struct ModemSettings
{
    QString audioIn = QStringLiteral("default");     // ADEVICE input
    QString audioOut = QStringLiteral("default");    // ADEVICE output
    QString ptt = QStringLiteral("none");             // none (VOX), cm108, rts, dtr
    QString pttDevice;                                // serial port for rts/dtr (desktop)
    int gpio = 3;                                     // CM108 GPIO, 1..8
    int txDelayMs = 300;                              // flags before the frame (TXDELAY)
    int txTailMs = 100;                               // flags after it (TXTAIL)

    // Values brought into range, unknown PTT methods to none.
    ModemSettings normalized() const;
    bool operator==(const ModemSettings &o) const;
    bool operator!=(const ModemSettings &o) const { return !(*this == o); }
};

// A phone's audio runs at 48 kHz (another rate has Oboe resample both
// ways); 44.1 kHz elsewhere, as Dire Wolf's own default.
int modemSampleRate();

// The direwolf.conf for these settings; callsign goes into MYCALL.
QString modemConfigText(const QString &callsign, const ModemSettings &settings);

// The serial port a PTT line goes to when none was given.
QString defaultSerialPort();

// A sound card the modem can use: what goes into ADEVICE, what the
// operator reads.
struct AudioDevice
{
    QString value;
    QString label;
};

// The system default first, then (Linux) the ALSA cards by name, stable
// across reboots unlike card numbers.  On Android only the default: the
// system routes audio to a USB sound card on its own.
QList<AudioDevice> audioDevices();
// /proc/asound/cards, given its text (tests).
QList<AudioDevice> parseAsoundCards(const QString &text);

} // namespace sater::aprs
