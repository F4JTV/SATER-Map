/*
 * ModemPort.cpp - APRS by radio through the built-in modem.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ModemPort.h"

#include "modem/AndroidPtt.h"
#include "modem/DirewolfEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#define SATER_MIC_PERMISSION 1
#endif

namespace sater::aprs {

namespace {

// Dire Wolf's tables (tocalls.yaml, symbols-new.txt) are compiled in;
// its core looks for them in the current directory, so they are written
// to the data folder, which becomes the current directory.  Once.
void prepareTables()
{
    static bool done = false;
    if (done) return;
    done = true;
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                            .filePath(QStringLiteral("direwolf-data"));
    if (!QDir().mkpath(dir)) return;
    for (const QString &name : {QStringLiteral("tocalls.yaml"), QStringLiteral("symbols-new.txt")}) {
        QFile source(QStringLiteral(":/direwolf-data/") + name);
        if (!source.exists()) continue;
        const QString target = QDir(dir).filePath(name);
        if (QFileInfo(target).size() == source.size()) continue;
        QFile::remove(target);
        if (source.copy(target)) QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
    QDir::setCurrent(dir);
}

QString modemNote(const QString &text)
{
    return ModemPort::tr("Modem: %1").arg(text);
}

} // namespace

ModemPort::ModemPort(QObject *parent)
    : AprsPort(parent)
{
    // On Android the transmitter is keyed from Java (UsbPtt): the core
    // learns about it before it can start.  Nothing elsewhere.
    androidptt::install();
}

ModemPort::~ModemPort()
{
    m_restart = false;
    // The engine (a child) stops the core and waits for it.
}

QString ModemPort::configPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("direwolf.conf"));
}

QString ModemPort::coreVersion()
{
    return DirewolfEngine::direwolfVersion();
}

DirewolfEngine *ModemPort::engine()
{
    if (m_engine) return m_engine;
    m_engine = new DirewolfEngine(this);
    connect(m_engine, &DirewolfEngine::started, this, [this]() {
        resetRetry();
        const QString what = m_engine->channelDescription(0);
        setState(State::Connected, what);
        emit note(modemNote(tr("started, %1").arg(what)));
    });
    connect(m_engine, &DirewolfEngine::startFailed, this, [this](const QString &reason) {
        m_restart = false;
        emit note(modemNote(reason));
        setState(State::Off, reason);
        scheduleRetry();
    });
    connect(m_engine, &DirewolfEngine::faulted, this, [this](int, const QString &reason) {
        m_fault = reason;
        emit note(modemNote(reason));
    });
    connect(m_engine, &DirewolfEngine::stopped, this, [this](bool afterFault) {
        if (m_transmitting) {
            m_transmitting = false;
            emit transmittingChanged();
        }
        if (m_restart && enabled()) {
            m_restart = false;
            startEngine();
            return;
        }
        m_restart = false;
        if (afterFault && enabled()) {
            setState(State::Off, m_fault.isEmpty() ? tr("Stopped after a fault") : m_fault);
            scheduleRetry();
        } else if (state() != State::Off) {
            setState(State::Off);
        }
    });
    connect(m_engine, &DirewolfEngine::pttChanged, this, [this](int chan, bool on) {
        if (chan != 0 || on == m_transmitting) return;
        m_transmitting = on;
        emit transmittingChanged();
        if (!on && m_settingsPending) applySettings();
    });
    connect(m_engine, &DirewolfEngine::frameReceived, this, [this](const DwReceivedFrame &rx) {
        if (rx.chan != 0) return;
        if (const auto p = decodeFrame(rx.frame)) emit packetReceived(*p);
    });
    connect(m_engine, &DirewolfEngine::logMessage, this, [this](DirewolfEngine::LogLevel level, const QString &line) {
        // Errors only: the core also tells every frame it sends.  With
        // SATER_MODEM_LOG set, every line goes to the program's own log
        // (logcat on Android), for a modem that will not behave.
        static const bool all = qEnvironmentVariableIsSet("SATER_MODEM_LOG");
        const QString text = line.trimmed();
        if (all) qInfo().noquote() << "direwolf:" << text;
        if (level == DirewolfEngine::Error && !text.isEmpty()) emit note(modemNote(text));
    });
    return m_engine;
}

void ModemPort::setSettings(const ModemSettings &settings)
{
    const ModemSettings s = settings.normalized();
    if (s == m_settings) return;
    m_settings = s;
    applySettings();
}

void ModemPort::applySettings()
{
    m_settingsPending = false;
    if (!enabled()) return;
    if (!m_engine || m_engine->state() == DirewolfEngine::State::Stopped) {
        // Off after a failure (a sound card not found...): the new
        // settings are tried now, not at the next retry.
        if (state() == State::Off) {
            resetRetry();
            open();
        }
        return;
    }
    // A bearing on the air, or waiting for the channel, goes out first;
    // the restart follows when the transmitter is released.
    if (m_transmitting || queuedFrames() > 0) {
        m_settingsPending = true;
        // Waiting for a busy channel: looked at again until the frame has
        // gone (or been given up by the core).
        if (!m_transmitting) {
            QTimer::singleShot(1000, this, [this]() {
                if (m_settingsPending) applySettings();
            });
        }
        return;
    }
    // Running (or on its way): stop, and start again with the new file.
    m_restart = true;
    setState(State::Connecting, tr("Restarting"));
    m_engine->stop();
}

bool ModemPort::idle() const
{
    return !m_engine || m_engine->state() == DirewolfEngine::State::Stopped;
}

int ModemPort::queuedFrames() const
{
    return m_engine && m_engine->isRunning() ? m_engine->txQueueFrames(0) : 0;
}

bool ModemPort::canSend() const
{
    return state() == State::Connected && m_engine && m_engine->isRunning();
}

bool ModemPort::send(const Packet &packet)
{
    if (!canSend()) return false;
    Packet p = packet;
    p.path = m_path;
    const QByteArray frame = encodeFrame(p);
    if (frame.isEmpty()) return false;
    return m_engine->transmit(0, frame);
}

void ModemPort::open()
{
    DirewolfEngine *e = engine();
    if (e->state() == DirewolfEngine::State::Stopping) {
        m_restart = true;
        setState(State::Connecting, tr("Restarting"));
        return;
    }
    if (e->state() != DirewolfEngine::State::Stopped) return;
#ifdef SATER_MIC_PERMISSION
    // The receiver records: Android asks the operator once.
    const QMicrophonePermission microphone;
    switch (qApp->checkPermission(microphone)) {
    case Qt::PermissionStatus::Granted:
        break;
    case Qt::PermissionStatus::Undetermined:
        setState(State::Connecting, tr("Waiting for the microphone permission"));
        qApp->requestPermission(microphone, this, [this](const QPermission &p) {
            if (!enabled()) return;
            if (p.status() == Qt::PermissionStatus::Granted) startEngine();
            else setState(State::Off, tr("No microphone permission: the modem cannot hear the radio"));
        });
        return;
    case Qt::PermissionStatus::Denied:
        setState(State::Off, tr("No microphone permission: allow it in the Android settings of SATER Map"));
        return;
    }
#endif
    startEngine();
}

void ModemPort::startEngine()
{
    DirewolfEngine *e = engine();
    if (e->state() != DirewolfEngine::State::Stopped) return;
    prepareTables();
    m_fault.clear();
    QString path = m_ownFile;
    if (path.isEmpty()) {
        // Dire Wolf keeps the first 29 characters of a device name.
        for (const QString &device : {m_settings.audioIn, m_settings.audioOut}) {
            if (device.size() > 29) emit note(modemNote(tr("the sound card name %1 is too long (29 characters at most)").arg(device)));
        }
        path = configPath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
            || file.write(modemConfigText(m_callsign, m_settings).toUtf8()) < 0 || !file.commit()) {
            const QString why = tr("Cannot write %1: %2").arg(path, file.errorString());
            emit note(modemNote(why));
            setState(State::Off, why);
            return;
        }
    }
    setState(State::Connecting, tr("Starting"));
    e->start(path);
}

void ModemPort::close()
{
    m_restart = false;
    m_settingsPending = false;
    if (m_engine) m_engine->stop();
}

} // namespace sater::aprs
