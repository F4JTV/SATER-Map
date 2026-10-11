/*
 * KissTcpClient.cpp - a KISS TNC on the network.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// The socket headers first: on Windows, winsock2.h must come before
// anything that could bring in windows.h (and with it the old winsock.h).
#include <QtGlobal>
#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

#include "KissTcpClient.h"

namespace sater::aprs {

namespace {

// A TNC never answers what it is sent, and says nothing on a quiet
// channel: a PC that left the Wi-Fi without closing the connection would
// leave it looking up for hours.  TCP keepalive probes after 20 s of
// silence (every 5 s, 3 times) find a dead peer within about 35 s, and on
// Linux and Android data left unacknowledged for 15 s ends the connection.
// Each option is set only where the system defines it.
void watchConnection(QTcpSocket *socket)
{
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    const qintptr fd = socket->socketDescriptor();
    if (fd < 0) return;
    const auto set = [fd](int option, int value) {
#if defined(Q_OS_WIN)
        ::setsockopt(SOCKET(fd), IPPROTO_TCP, option, reinterpret_cast<const char *>(&value), sizeof value);
#else
        ::setsockopt(int(fd), IPPROTO_TCP, option, &value, sizeof value);
#endif
    };
#if defined(TCP_KEEPIDLE)
    set(TCP_KEEPIDLE, 20);
#elif defined(TCP_KEEPALIVE)
    set(TCP_KEEPALIVE, 20);
#endif
#if defined(TCP_KEEPINTVL)
    set(TCP_KEEPINTVL, 5);
#endif
#if defined(TCP_KEEPCNT)
    set(TCP_KEEPCNT, 3);
#endif
#if defined(TCP_USER_TIMEOUT)
    set(TCP_USER_TIMEOUT, 15000);
#endif
    Q_UNUSED(set);
}

} // namespace

KissTcpClient::KissTcpClient(QObject *parent)
    : AprsPort(parent)
{
}

KissTcpClient::~KissTcpClient()
{
    close();
}

void KissTcpClient::setServer(const QString &host, quint16 port)
{
    const QString h = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    if (h == m_host && port == m_port) return;
    m_host = h;
    m_port = port ? port : 8001;
    if (enabled()) {
        close();
        setState(State::Off);
        open();
    }
}

bool KissTcpClient::send(const Packet &packet)
{
    if (!canSend() || !m_socket) return false;
    Packet p = packet;
    p.path = m_path;
    const QByteArray frame = encodeFrame(p);
    if (frame.isEmpty()) return false;
    m_socket->write(kissEncode(frame));
    return true;
}

void KissTcpClient::open()
{
    close();
    m_decoder.reset();
    auto *socket = new QTcpSocket(this);
    m_socket = socket;
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        if (socket != m_socket) return;
        resetRetry();
        watchConnection(socket);
        setState(State::Connected, QStringLiteral("%1:%2").arg(m_host).arg(m_port));
        emit note(tr("KISS: connected to %1:%2").arg(m_host).arg(m_port));
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (socket != m_socket) return;
        for (const QByteArray &frame : m_decoder.feed(socket->readAll())) {
            if (const auto p = decodeFrame(frame)) emit packetReceived(*p);
        }
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        if (socket != m_socket) return;
        const QString why = socket->errorString();
        emit note(tr("KISS: %1").arg(why));
        close();
        setState(State::Off, why);
        scheduleRetry();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        if (socket != m_socket) return;
        close();
        setState(State::Off, tr("Disconnected"));
        scheduleRetry();
    });
    setState(State::Connecting, QStringLiteral("%1:%2").arg(m_host).arg(m_port));
    socket->connectToHost(m_host, m_port);
}

void KissTcpClient::close()
{
    if (QTcpSocket *s = m_socket) {
        m_socket = nullptr;
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }
}

} // namespace sater::aprs
