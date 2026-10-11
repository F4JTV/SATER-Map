/*
 * KissTcpClient.h - a KISS TNC on the network: Dire Wolf (port 8001), UZ7HO
 * Soundmodem (8100), a TNC behind a serial-to-TCP bridge.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "AprsPort.h"
#include "Kiss.h"

#include <QPointer>
#include <QStringList>
#include <QTcpSocket>

namespace sater::aprs {

class KissTcpClient : public AprsPort
{
    Q_OBJECT
public:
    explicit KissTcpClient(QObject *parent = nullptr);
    ~KissTcpClient() override;

    void setServer(const QString &host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    // The digipeater path of what is sent ("WIDE1-1,WIDE2-1").
    void setPath(const QStringList &path) { m_path = path; }

    bool send(const Packet &packet) override;

protected:
    void open() override;
    void close() override;

private:
    QPointer<QTcpSocket> m_socket;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 8001;
    QStringList m_path = {QStringLiteral("WIDE1-1"), QStringLiteral("WIDE2-1")};
    KissDecoder m_decoder;
};

} // namespace sater::aprs
