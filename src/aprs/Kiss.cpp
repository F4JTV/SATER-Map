/*
 * Kiss.cpp - KISS framing.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Kiss.h"

namespace sater::aprs {

namespace {
constexpr char kFend = char(0xC0);
constexpr char kFesc = char(0xDB);
constexpr char kTfend = char(0xDC);
constexpr char kTfesc = char(0xDD);
constexpr int kMaxFrame = 2048;
} // namespace

QByteArray kissEncode(const QByteArray &frame, int port)
{
    QByteArray out;
    out.reserve(frame.size() + 8);
    out.append(kFend);
    out.append(char((port & 0x0F) << 4));   // command 0: data
    for (char c : frame) {
        if (c == kFend) {
            out.append(kFesc);
            out.append(kTfend);
        } else if (c == kFesc) {
            out.append(kFesc);
            out.append(kTfesc);
        } else {
            out.append(c);
        }
    }
    out.append(kFend);
    return out;
}

QList<QByteArray> KissDecoder::feed(const QByteArray &bytes, QList<int> *ports)
{
    QList<QByteArray> frames;
    for (char c : bytes) {
        if (c == kFend) {
            if (m_inFrame && m_buffer.size() > 1) {
                // Unescape; the first byte is the port and the command.
                QByteArray data;
                data.reserve(m_buffer.size());
                bool escaped = false;
                bool bad = false;
                for (int i = 1; i < m_buffer.size(); ++i) {
                    const char b = m_buffer.at(i);
                    if (escaped) {
                        if (b == kTfend) data.append(kFend);
                        else if (b == kTfesc) data.append(kFesc);
                        else bad = true;
                        escaped = false;
                    } else if (b == kFesc) {
                        escaped = true;
                    } else {
                        data.append(b);
                    }
                }
                const uchar type = uchar(m_buffer.at(0));
                if (!bad && !escaped && (type & 0x0F) == 0) {
                    frames.append(data);
                    if (ports) ports->append(type >> 4);
                }
            }
            m_buffer.clear();
            m_inFrame = true;
            continue;
        }
        if (!m_inFrame) continue;
        if (m_buffer.size() >= kMaxFrame) {
            // Not KISS, or a frame lost its end: wait for the next FEND.
            m_buffer.clear();
            m_inFrame = false;
            continue;
        }
        m_buffer.append(c);
    }
    return frames;
}

} // namespace sater::aprs
