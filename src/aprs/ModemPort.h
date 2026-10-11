/*
 * ModemPort.h - APRS by radio through the modem built into the program:
 * Dire Wolf's core (src/modem, from AX25Chess), a sound card and a PTT
 * line.  This is how the phone sends its bearing to the command post with
 * nothing but an audio interface between it and the radio.
 *
 * Only built with SATER_MODEM (the default on Android).  The core is one
 * per process; so is this port.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "AprsPort.h"
#include "ModemSettings.h"

#include <QStringList>

class DirewolfEngine;

namespace sater::aprs {

class ModemPort : public AprsPort
{
    Q_OBJECT
public:
    explicit ModemPort(QObject *parent = nullptr);
    ~ModemPort() override;

    // MYCALL of the next start (the frames sent carry their own source).
    void setCallsign(const QString &callsign) { m_callsign = callsign; }
    // A change restarts a running modem.
    void setSettings(const ModemSettings &settings);
    ModemSettings settings() const { return m_settings; }
    // The digipeater path of what is sent ("WIDE1-1,WIDE2-1").
    void setPath(const QStringList &path) { m_path = path; }
    // Tests: start with this direwolf.conf as it is, not one written from
    // the settings.  Empty: back to the settings.
    void setConfigFile(const QString &path) { m_ownFile = path; }

    bool canSend() const override;
    bool send(const Packet &packet) override;
    // The transmitter is keyed right now.
    bool transmitting() const { return m_transmitting; }
    // The core has stopped (not starting, running or stopping).
    bool idle() const;
    // Frames still waiting for the channel.
    int queuedFrames() const;
    // Where the generated direwolf.conf goes.
    static QString configPath();
    // Dire Wolf's version compiled in.
    static QString coreVersion();

signals:
    void transmittingChanged();

protected:
    void open() override;
    void close() override;

private:
    DirewolfEngine *engine();
    void startEngine();
    // The settings changed: a restart now, or once the transmitter is
    // released, or a new try at once after a failure.
    void applySettings();

    DirewolfEngine *m_engine = nullptr;
    QString m_callsign;
    ModemSettings m_settings;
    QStringList m_path = {QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-1")};
    QString m_ownFile;
    QString m_fault;            // why the core stopped on its own
    bool m_restart = false;     // start again once the core has stopped
    bool m_settingsPending = false;   // restart once this transmission is over
    bool m_transmitting = false;
};

} // namespace sater::aprs
