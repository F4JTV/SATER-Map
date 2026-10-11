/*
 * Ax25.h - AX.25 UI frames, the frames APRS is carried in, and their text
 * form "SRC>DEST,DIGI1*,DIGI2:info" (the "TNC2" format of APRS-IS).
 *
 * The frames are those a KISS TNC exchanges: addresses, control 0x03, PID
 * 0xF0 and the information field, without the flags and the FCS.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace sater::aprs {

struct Packet {
    QString source;          // "F4AAA-7"
    QString destination;     // "APZSAT"
    QStringList path;        // "WIDE1-1", "F1ZZZ-3*" (* = repeated)
    QByteArray info;         // the APRS data

    QString toTnc2() const;
};

// "F4AAA-7" is a valid AX.25 address: 1 to 6 letters or digits, SSID 0-15.
bool isValidCallsign(const QString &callsign);
// "F4AAA-0" -> "F4AAA", upper case, trimmed.
QString normalizeCallsign(const QString &callsign);

// The frame of a packet (addresses, 0x03, 0xF0, info); empty if an address
// is not valid.
QByteArray encodeFrame(const Packet &packet);
// A received frame; nothing if it is not a UI frame with valid addresses.
std::optional<Packet> decodeFrame(const QByteArray &frame);

// "SRC>DEST,PATH:info" (a third-party header "}..." is unwrapped by the
// caller when it wants the inner packet).
std::optional<Packet> parseTnc2(const QByteArray &line);

} // namespace sater::aprs
