/*
 * Ax25.cpp - AX.25 UI frames and their text form.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Ax25.h"

#include <QRegularExpression>

namespace sater::aprs {

namespace {

constexpr int kAddressLength = 7;
constexpr int kMaxDigipeaters = 8;

struct Address {
    QString call;   // without SSID
    int ssid = 0;
    bool flag = false;   // H bit of a digipeater ("has been repeated")
};

bool splitCallsign(const QString &text, Address *out)
{
    static const QRegularExpression re(QStringLiteral("^([A-Z0-9]{1,6})(?:-([0-9]{1,2}))?$"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) return false;
    const int ssid = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
    if (ssid > 15) return false;
    out->call = m.captured(1);
    out->ssid = ssid;
    return true;
}

QString joinCallsign(const Address &a)
{
    return a.ssid ? QStringLiteral("%1-%2").arg(a.call).arg(a.ssid) : a.call;
}

void appendAddress(QByteArray *frame, const Address &a, bool last, bool highBit)
{
    for (int i = 0; i < 6; ++i) {
        const char c = i < a.call.size() ? a.call.at(i).toLatin1() : ' ';
        frame->append(char(uchar(c) << 1));
    }
    uchar ssid = uchar(0x60 | (a.ssid << 1));
    if (highBit) ssid |= 0x80;
    if (last) ssid |= 0x01;
    frame->append(char(ssid));
}

bool readAddress(const QByteArray &frame, int offset, Address *out, bool *last)
{
    QString call;
    for (int i = 0; i < 6; ++i) {
        const uchar b = uchar(frame.at(offset + i));
        if (b & 0x01) return false;               // the extension bit is only in the SSID byte
        const char c = char(b >> 1);
        if (c == ' ') continue;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
        if (call.size() < i) return false;        // a space inside the callsign
        call.append(QLatin1Char(c));
    }
    if (call.isEmpty()) return false;
    const uchar ssid = uchar(frame.at(offset + 6));
    out->call = call;
    out->ssid = (ssid >> 1) & 0x0F;
    out->flag = ssid & 0x80;
    *last = ssid & 0x01;
    return true;
}

} // namespace

QString Packet::toTnc2() const
{
    QString text = source + QLatin1Char('>') + destination;
    for (const QString &p : path) text += QLatin1Char(',') + p;
    return text + QLatin1Char(':') + QString::fromUtf8(info);
}

QString normalizeCallsign(const QString &callsign)
{
    QString c = callsign.trimmed().toUpper();
    if (c.endsWith(QLatin1String("-0"))) c.chop(2);
    return c;
}

bool isValidCallsign(const QString &callsign)
{
    Address a;
    return splitCallsign(normalizeCallsign(callsign), &a);
}

QByteArray encodeFrame(const Packet &packet)
{
    Address dest, src;
    if (!splitCallsign(normalizeCallsign(packet.destination), &dest)) return {};
    if (!splitCallsign(normalizeCallsign(packet.source), &src)) return {};
    QList<Address> digis;
    for (const QString &p : packet.path) {
        QString text = p.trimmed().toUpper();
        const bool used = text.endsWith(QLatin1Char('*'));
        if (used) text.chop(1);
        Address a;
        if (!splitCallsign(normalizeCallsign(text), &a)) return {};
        a.flag = used;
        digis.append(a);
    }
    if (digis.size() > kMaxDigipeaters) return {};
    // TNC2 stars only the last digipeater used; in the frame every one
    // before it has its H bit too (as Dire Wolf's ax25_from_text).
    for (int i = int(digis.size()) - 1, used = 0; i >= 0; --i) {
        used = used || digis.at(i).flag;
        digis[i].flag = used;
    }
    QByteArray frame;
    // Destination with the command bit, source without: an AX.25 v2 command.
    appendAddress(&frame, dest, false, true);
    appendAddress(&frame, src, digis.isEmpty(), false);
    for (int i = 0; i < digis.size(); ++i) appendAddress(&frame, digis.at(i), i == digis.size() - 1, digis.at(i).flag);
    frame.append(char(0x03));
    frame.append(char(0xF0));
    frame.append(packet.info);
    return frame;
}

std::optional<Packet> decodeFrame(const QByteArray &frame)
{
    if (frame.size() < 2 * kAddressLength + 2) return std::nullopt;
    Address dest, src;
    bool last = false;
    if (!readAddress(frame, 0, &dest, &last) || last) return std::nullopt;
    if (!readAddress(frame, kAddressLength, &src, &last)) return std::nullopt;
    Packet p;
    p.destination = joinCallsign(dest);
    p.source = joinCallsign(src);
    int offset = 2 * kAddressLength;
    QList<Address> digis;
    while (!last) {
        if (digis.size() == kMaxDigipeaters || offset + kAddressLength > frame.size()) return std::nullopt;
        Address d;
        if (!readAddress(frame, offset, &d, &last)) return std::nullopt;
        digis.append(d);
        offset += kAddressLength;
    }
    if (offset + 2 > frame.size()) return std::nullopt;
    if (uchar(frame.at(offset)) != 0x03 || uchar(frame.at(offset + 1)) != 0xF0) return std::nullopt;   // UI, no layer 3
    // The text form marks the last repeater that handled the frame.
    int lastUsed = -1;
    for (int i = 0; i < digis.size(); ++i) {
        if (digis.at(i).flag) lastUsed = i;
    }
    for (int i = 0; i < digis.size(); ++i) p.path.append(joinCallsign(digis.at(i)) + (i == lastUsed ? QStringLiteral("*") : QString()));
    p.info = frame.mid(offset + 2);
    return p;
}

std::optional<Packet> parseTnc2(const QByteArray &line)
{
    QByteArray l = line;
    while (l.endsWith('\r') || l.endsWith('\n')) l.chop(1);
    const int colon = l.indexOf(':');
    const int gt = l.indexOf('>');
    if (gt <= 0 || colon <= gt + 1) return std::nullopt;
    Packet p;
    p.source = QString::fromLatin1(l.left(gt)).trimmed().toUpper();
    const QList<QByteArray> header = l.mid(gt + 1, colon - gt - 1).split(',');
    p.destination = QString::fromLatin1(header.first()).trimmed().toUpper();
    for (int i = 1; i < header.size(); ++i) p.path.append(QString::fromLatin1(header.at(i)).trimmed().toUpper());
    p.info = l.mid(colon + 1);
    if (p.source.isEmpty() || p.destination.isEmpty()) return std::nullopt;
    return p;
}

} // namespace sater::aprs
