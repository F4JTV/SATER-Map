/*
 * ModemSettings.cpp - the built-in modem's settings and its direwolf.conf.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ModemSettings.h"

#include "Ax25.h"
#include "core/Tr.h"

#include <QFile>
#include <QRegularExpression>

#include <algorithm>

namespace sater::aprs {

ModemSettings ModemSettings::normalized() const
{
    ModemSettings s = *this;
    s.audioIn = s.audioIn.trimmed().isEmpty() ? QStringLiteral("default") : s.audioIn.trimmed();
    s.audioOut = s.audioOut.trimmed().isEmpty() ? s.audioIn : s.audioOut.trimmed();
    s.ptt = s.ptt.trimmed().toLower();
    if (s.ptt != QLatin1String("cm108") && s.ptt != QLatin1String("rts") && s.ptt != QLatin1String("dtr"))
        s.ptt = QStringLiteral("none");
    s.pttDevice = s.pttDevice.trimmed();
    s.gpio = std::clamp(s.gpio, 1, 8);
    // Dire Wolf counts in units of 10 ms, 0 to 255.
    s.txDelayMs = std::clamp(s.txDelayMs, 0, 2550) / 10 * 10;
    s.txTailMs = std::clamp(s.txTailMs, 0, 2550) / 10 * 10;
    return s;
}

bool ModemSettings::operator==(const ModemSettings &o) const
{
    return audioIn == o.audioIn && audioOut == o.audioOut && ptt == o.ptt && pttDevice == o.pttDevice
           && gpio == o.gpio && txDelayMs == o.txDelayMs && txTailMs == o.txTailMs;
}

int modemSampleRate()
{
#ifdef Q_OS_ANDROID
    return 48000;
#else
    return 44100;
#endif
}

QString defaultSerialPort()
{
#ifdef Q_OS_WIN
    return QStringLiteral("COM3");
#else
    return QStringLiteral("/dev/ttyUSB0");
#endif
}

QString modemConfigText(const QString &callsign, const ModemSettings &settings)
{
    const ModemSettings s = settings.normalized();
    const QString call = isValidCallsign(callsign) ? normalizeCallsign(callsign) : QStringLiteral("NOCALL");
    // Both on the system default: no ADEVICE line, Dire Wolf's own default
    // on every platform (Windows would look for a device called "default").
    const QString adevice = (s.audioIn == QLatin1String("default") && s.audioOut == QLatin1String("default"))
        ? QStringLiteral("#ADEVICE default      (the system default device)")
        : QStringLiteral("ADEVICE  ") + (s.audioIn == s.audioOut ? s.audioIn : s.audioIn + QLatin1Char(' ') + s.audioOut);
    QString ptt;
    const QString serial = s.pttDevice.isEmpty() ? defaultSerialPort() : s.pttDevice;
    if (s.ptt == QLatin1String("cm108")) {
        ptt = QStringLiteral("PTT      CM108 %1").arg(s.gpio);
        if (!s.pttDevice.isEmpty()) ptt += QLatin1Char(' ') + s.pttDevice;
    } else if (s.ptt == QLatin1String("rts")) {
        ptt = QStringLiteral("PTT      %1 RTS").arg(serial);
    } else if (s.ptt == QLatin1String("dtr")) {
        ptt = QStringLiteral("PTT      %1 DTR").arg(serial);
    } else {
        ptt = QStringLiteral("# No PTT line: the radio's VOX keys the transmitter.");
    }
    return QStringLiteral(
               "# direwolf.conf written by SATER Map from its APRS settings (built-in modem).\n"
               "# Rewritten each time the modem starts: change the settings in the program.\n"
               "\n"
               "%1\n"
               "ARATE    %2\n"
               "ACHANNELS 1\n"
               "CHANNEL  0\n"
               "MYCALL   %3\n"
               "MODEM    1200\n"
               "%4\n"
               "\n"
               "# Channel access, in units of 10 ms except PERSIST (0-255).\n"
               "TXDELAY  %5\n"
               "TXTAIL   %6\n"
               "PERSIST  63\n"
               "SLOTTIME 10\n"
               "FULLDUP  OFF\n")
        .arg(adevice)
        .arg(modemSampleRate())
        .arg(call, ptt)
        .arg(s.txDelayMs / 10)
        .arg(s.txTailMs / 10);
}

QList<AudioDevice> parseAsoundCards(const QString &text)
{
    // " 1 [Device         ]: USB-Audio - USB Audio Device"
    static const QRegularExpression line(QStringLiteral("^\\s*(\\d+)\\s+\\[([^\\]]+)\\]\\s*:\\s*(.*)$"));
    QList<AudioDevice> out;
    for (const QString &l : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = line.match(l);
        if (!m.hasMatch()) continue;
        const QString id = m.captured(2).trimmed();
        QString description = m.captured(3).trimmed();
        const int dash = description.indexOf(QStringLiteral(" - "));
        if (dash >= 0) description = description.mid(dash + 3).trimmed();
        // Dire Wolf keeps 29 characters of a device name; a card id has up
        // to 15, and DEV is 0 when not given.
        const QString value = QStringLiteral("plughw:CARD=%1").arg(id);
        out.append({value, QStringLiteral("%1  (%2)").arg(description, value)});
    }
    return out;
}

QList<AudioDevice> audioDevices()
{
    QList<AudioDevice> out{{QStringLiteral("default"), SATER_TR("System default")}};
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QFile cards(QStringLiteral("/proc/asound/cards"));
    if (cards.open(QIODevice::ReadOnly | QIODevice::Text)) out += parseAsoundCards(QString::fromUtf8(cards.readAll()));
#endif
    return out;
}

} // namespace sater::aprs
