/*
 * AprsPort.h - a way in and out for APRS packets: APRS-IS (internet), a
 * KISS TNC over TCP (Dire Wolf, Soundmodem), the modem built into the
 * Android application.  Each reconnects on its own while enabled.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "Ax25.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace sater::aprs {

class AprsPort : public QObject
{
    Q_OBJECT
public:
    enum class State { Off, Connecting, Connected };
    Q_ENUM(State)

    explicit AprsPort(QObject *parent = nullptr);

    State state() const { return m_state; }
    // Why the last attempt failed, or what the server said; empty when fine.
    QString detail() const { return m_detail; }
    bool enabled() const { return m_enabled; }
    // Start (or stop) connecting, and reconnecting after a failure.
    void setEnabled(bool on);

    // Can a packet go out now (connected; for APRS-IS, logged in with a
    // valid passcode)?
    virtual bool canSend() const { return m_state == State::Connected; }
    // Send a packet; false if it cannot go out.  The port sets the path it
    // needs (TCPIP* on APRS-IS, the radio path on RF).
    virtual bool send(const Packet &packet) = 0;

signals:
    void stateChanged();
    void packetReceived(const sater::aprs::Packet &packet);
    // A line for the log (what the server says, errors).
    void note(const QString &text);

protected:
    virtual void open() = 0;
    virtual void close() = 0;
    void setState(State s, const QString &detail = QString());
    // After a failure: try again, waiting longer each time (5 s to 60 s).
    void scheduleRetry();
    void resetRetry() { m_retries = 0; }

private:
    State m_state = State::Off;
    QString m_detail;
    bool m_enabled = false;
    int m_retries = 0;
    QTimer m_retry;
};

} // namespace sater::aprs
