/*
 * AprsPort.cpp - the common part of the APRS ports.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AprsPort.h"

#include <algorithm>
#include <iterator>

namespace sater::aprs {

AprsPort::AprsPort(QObject *parent)
    : QObject(parent)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this]() {
        if (m_enabled && m_state == State::Off) open();
    });
}

void AprsPort::setEnabled(bool on)
{
    if (on == m_enabled) {
        if (on && m_state == State::Off && !m_retry.isActive()) open();
        return;
    }
    m_enabled = on;
    m_retry.stop();
    m_retries = 0;
    if (on) {
        open();
    } else {
        close();
        setState(State::Off);
    }
}

void AprsPort::setState(State s, const QString &detail)
{
    if (s == m_state && detail == m_detail) return;
    m_state = s;
    m_detail = detail;
    emit stateChanged();
}

void AprsPort::scheduleRetry()
{
    if (!m_enabled) return;
    static const int delays[] = {5, 10, 20, 30, 60};
    const int secs = delays[std::min(m_retries, int(std::size(delays)) - 1)];
    ++m_retries;
    m_retry.start(secs * 1000);
}

} // namespace sater::aprs
