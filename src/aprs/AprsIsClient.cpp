/*
 * AprsIsClient.cpp - the APRS-IS network.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AprsIsClient.h"

#include <QRegularExpression>

namespace sater::aprs {

namespace {
// Servers send a "#" keepalive every 20 s or so.
constexpr int kSilenceMs = 120 * 1000;
constexpr int kMaxLine = 1024;
} // namespace

AprsIsClient::AprsIsClient(QObject *parent)
    : AprsPort(parent)
{
    m_watchdog.setSingleShot(true);
    m_watchdog.setInterval(kSilenceMs);
    connect(&m_watchdog, &QTimer::timeout, this, [this]() {
        emit note(tr("APRS-IS: no data from the server for two minutes, reconnecting."));
        close();
        setState(State::Off, tr("No data from the server"));
        scheduleRetry();
    });
}

AprsIsClient::~AprsIsClient()
{
    close();
}

void AprsIsClient::setServer(const QString &host, quint16 port)
{
    const QString h = host.trimmed().isEmpty() ? QStringLiteral("rotate.aprs2.net") : host.trimmed();
    if (h == m_host && port == m_port) return;
    m_host = h;
    m_port = port ? port : 14580;
    if (enabled()) {
        close();
        setState(State::Off);
        open();
    }
}

void AprsIsClient::setLogin(const QString &callsign, int passcode)
{
    const QString c = normalizeCallsign(callsign);
    if (c == m_callsign && passcode == m_passcode) return;
    m_callsign = c;
    m_passcode = passcode;
    if (enabled()) {
        close();
        setState(State::Off);
        open();
    }
}

void AprsIsClient::setFilterCallsigns(const QStringList &callsigns)
{
    QStringList list;
    for (const QString &c : callsigns) {
        const QString n = normalizeCallsign(c);
        if (n.isEmpty()) continue;
        // "F4AAA" means any SSID of F4AAA.
        const QString entry = n.contains(QLatin1Char('-')) ? n : n + QLatin1Char('*');
        if (!list.contains(entry)) list.append(entry);
    }
    if (list == m_filterCalls) return;
    m_filterCalls = list;
    if (m_socket && state() == State::Connected) writeLine("#filter " + filter().toLatin1());
}

QString AprsIsClient::filter() const
{
    // Nothing to receive yet: a filter that matches no one rather than the
    // whole feed.
    if (m_filterCalls.isEmpty()) return QStringLiteral("b/N0CALL");
    return QStringLiteral("b/") + m_filterCalls.join(QLatin1Char('/'));
}

void AprsIsClient::setSoftware(const QString &name, const QString &version)
{
    m_software = name;
    m_version = version;
}

bool AprsIsClient::canSend() const
{
    return state() == State::Connected && m_verified;
}

QByteArray AprsIsClient::lineFor(const Packet &packet)
{
    Packet p = packet;
    p.path = {QStringLiteral("TCPIP*")};
    return p.toTnc2().toUtf8();
}

bool AprsIsClient::send(const Packet &packet)
{
    if (!canSend()) return false;
    writeLine(lineFor(packet));
    return true;
}

void AprsIsClient::open()
{
    close();
    m_verified = false;
    m_loggedIn = false;
    m_serverLines = 0;
    m_refusal.clear();
    m_buffer.clear();
    m_discarding = false;
    auto *socket = new QTcpSocket(this);
    m_socket = socket;
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        if (socket != m_socket) return;
        // The retry delay starts again only once the server has accepted
        // the login: one that refuses it is not hammered every 5 s.
        const QString call = m_callsign.isEmpty() ? QStringLiteral("N0CALL") : m_callsign;
        const int pass = m_callsign.isEmpty() ? -1 : m_passcode;
        writeLine(QStringLiteral("user %1 pass %2 vers %3 %4 filter %5")
                      .arg(call)
                      .arg(pass)
                      .arg(m_software, m_version.isEmpty() ? QStringLiteral("0") : m_version, filter())
                      .toLatin1());
        setState(State::Connected, tr("Logging in"));
        m_watchdog.start();
    });
    connect(socket, &QTcpSocket::readyRead, this, &AprsIsClient::onReadyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        if (socket != m_socket) return;
        const QString error = socket->errorString();
        emit note(tr("APRS-IS: %1").arg(error));
        // A server that hangs up on a refused login: its reason, not "closed".
        const QString why = m_loggedIn || m_refusal.isEmpty() ? error : m_refusal;
        close();
        setState(State::Off, why);
        scheduleRetry();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        if (socket != m_socket) return;
        const QString why = m_loggedIn || m_refusal.isEmpty() ? tr("Disconnected by the server") : m_refusal;
        close();
        setState(State::Off, why);
        scheduleRetry();
    });
    setState(State::Connecting, QStringLiteral("%1:%2").arg(m_host).arg(m_port));
    socket->connectToHost(m_host, m_port);
}

void AprsIsClient::close()
{
    m_watchdog.stop();
    m_verified = false;
    if (QTcpSocket *s = m_socket) {
        m_socket = nullptr;
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }
}

void AprsIsClient::writeLine(const QByteArray &line)
{
    if (!m_socket) return;
    m_socket->write(line + "\r\n");
}

void AprsIsClient::onReadyRead()
{
    if (!m_socket) return;
    m_watchdog.start();
    m_buffer += m_socket->readAll();
    for (;;) {
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        if (m_discarding) {
            m_discarding = false;   // the end of an over-long line
            continue;
        }
        handleLine(line.trimmed());
    }
    if (m_buffer.size() > kMaxLine) {
        m_buffer.clear();
        m_discarding = true;
    }
}

void AprsIsClient::handleLine(const QByteArray &line)
{
    if (line.isEmpty()) return;
    if (line.startsWith('#')) {
        // "# logresp F4JTV-9 verified, server T2FRANCE"
        static const QRegularExpression logresp(QStringLiteral("^#\\s*logresp\\s+(\\S+)\\s+(\\w+)(?:,\\s*server\\s+(\\S+))?"));
        const QRegularExpressionMatch m = logresp.match(QString::fromLatin1(line));
        if (m.hasMatch()) {
            m_verified = m.captured(2) == QLatin1String("verified");
            m_loggedIn = true;
            resetRetry();
            const QString server = m.captured(3);
            setState(State::Connected, m_verified ? tr("Logged in to %1").arg(server)
                                                  : tr("Logged in to %1, receive only").arg(server));
            emit note(tr("APRS-IS: %1").arg(QString::fromLatin1(line.mid(1)).trimmed()));
        } else if (!m_loggedIn && m_serverLines++ > 0) {
            // Not the banner: the server's reason for not logging us in.
            m_refusal = QString::fromUtf8(line.mid(1)).trimmed();
            setState(State::Connected, m_refusal);
            emit note(tr("APRS-IS: %1").arg(m_refusal));
        }
        return;
    }
    if (const auto p = parseTnc2(line)) emit packetReceived(*p);
}

} // namespace sater::aprs
