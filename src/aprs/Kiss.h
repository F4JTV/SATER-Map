/*
 * Kiss.h - KISS framing, as TNCs and soundcard modems (Dire Wolf, UZ7HO
 * Soundmodem) exchange AX.25 frames over TCP or a serial line.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QByteArray>
#include <QList>

namespace sater::aprs {

// A data frame for the TNC's radio port (0 for the first).
QByteArray kissEncode(const QByteArray &frame, int port = 0);

// Bytes as they come from the TNC, frames as they are complete.
class KissDecoder
{
public:
    // The data frames completed by these bytes (other KISS commands are
    // dropped).  port receives each frame's radio port when not null.
    QList<QByteArray> feed(const QByteArray &bytes, QList<int> *ports = nullptr);
    void reset() { m_buffer.clear(); m_inFrame = false; }

private:
    QByteArray m_buffer;
    bool m_inFrame = false;
};

} // namespace sater::aprs
