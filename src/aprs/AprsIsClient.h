/*
 * AprsIsClient.h - the APRS-IS network (internet).
 *
 * Logs in with the station's callsign: with its passcode the client may
 * send, without (passcode -1) it only receives.  The server forwards only
 * the packets the filter asks for: here the operation's callsigns, as a
 * "budlist" (b/ then the callsigns separated by slashes, each with a star
 * for any SSID), changed on the fly when the list changes.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "AprsPort.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

namespace sater::aprs {

class AprsIsClient : public AprsPort
{
    Q_OBJECT
public:
    explicit AprsIsClient(QObject *parent = nullptr);
    ~AprsIsClient() override;

    void setServer(const QString &host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    // The login: callsign (with SSID) and passcode (-1: receive only).
    void setLogin(const QString &callsign, int passcode);
    // The callsigns to receive (a trailing * for any SSID).
    void setFilterCallsigns(const QStringList &callsigns);
    QString filter() const;
    void setSoftware(const QString &name, const QString &version);

    bool verified() const { return m_verified; }
    bool canSend() const override;
    bool send(const Packet &packet) override;

    // A packet as the server would forward it (for the tests).
    static QByteArray lineFor(const Packet &packet);

protected:
    void open() override;
    void close() override;

private:
    void onReadyRead();
    void handleLine(const QByteArray &line);
    void writeLine(const QByteArray &line);

    QPointer<QTcpSocket> m_socket;
    QString m_host = QStringLiteral("rotate.aprs2.net");
    quint16 m_port = 14580;
    QString m_callsign;
    int m_passcode = -1;
    QStringList m_filterCalls;
    QString m_software = QStringLiteral("SATER_Map");
    QString m_version;
    bool m_verified = false;
    // The server answered the login (logresp); before that, its lines
    // after the banner say why it refuses ("# Invalid username format").
    bool m_loggedIn = false;
    int m_serverLines = 0;
    QString m_refusal;
    QByteArray m_buffer;
    // A line longer than any server sends is dropped to its end, not read
    // from the middle as if it were a line of its own.
    bool m_discarding = false;
    QTimer m_watchdog;      // no line for a while: the link is dead
};

} // namespace sater::aprs
