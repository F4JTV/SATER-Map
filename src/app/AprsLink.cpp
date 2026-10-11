/*
 * AprsLink.cpp - APRS for the mission.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AprsLink.h"

#include "Mission.h"
#include "aprs/AprsIsClient.h"
#include "aprs/DfReport.h"
#include "aprs/KissTcpClient.h"
#include "core/Tr.h"
#ifdef SATER_MODEM
#include "aprs/ModemPort.h"
#endif

#include <QCoreApplication>
#include <QSettings>
#include <QTimer>

using namespace sater;
using namespace sater::aprs;

namespace {
// The destination ("tocall") of what SATER Map sends: the experimental range.
const QString kToCall = QStringLiteral("APZSAT");
constexpr int kMaxLog = 300;
// The same report heard by RF and from APRS-IS, or repeated by digipeaters.
constexpr qint64 kDuplicateMs = 10 * 60 * 1000;
} // namespace

AprsLink::AprsLink(Mission *mission, QObject *parent)
    : QObject(parent)
    , m_mission(mission)
    , m_is(new AprsIsClient(this))
    , m_kiss(new KissTcpClient(this))
{
    m_is->setSoftware(QStringLiteral("SATER_Map"), QCoreApplication::applicationVersion());
    QList<QPair<AprsPort *, QString>> ports = {{m_is, QStringLiteral("APRS-IS")}, {m_kiss, QStringLiteral("RF")}};
#ifdef SATER_MODEM
    m_modem = new ModemPort(this);
    ports.append({m_modem, QStringLiteral("Modem")});
    connect(m_modem, &ModemPort::transmittingChanged, this, &AprsLink::stateChanged);
#endif
    for (const auto &[port, via] : ports) {
        connect(port, &AprsPort::stateChanged, this, &AprsLink::stateChanged);
        connect(port, &AprsPort::packetReceived, this, [this, via = via](const Packet &p) { handlePacket(p, via); });
        connect(port, &AprsPort::note, this, [this](const QString &t) { addLog(QStringLiteral("note"), t); });
    }
    if (m_mission) {
        connect(m_mission, &Mission::teamChanged, this, [this]() { m_is->setFilterCallsigns(m_mission->team()); });
        m_is->setFilterCallsigns(m_mission->team());
    }
    loadSettings();
}

AprsLink::~AprsLink()
{
    saveSettings();
}

// ---- settings ----------------------------------------------------------------------

void AprsLink::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("aprs"));
    m_myCallsign = normalizeCallsign(s.value(QStringLiteral("callsign")).toString());
    m_passcode = s.value(QStringLiteral("passcode")).toString();
    m_rfPath = s.value(QStringLiteral("path"), m_rfPath).toString();
    m_is->setServer(s.value(QStringLiteral("isHost"), QStringLiteral("rotate.aprs2.net")).toString(),
                    quint16(s.value(QStringLiteral("isPort"), 14580).toInt()));
    m_kiss->setServer(s.value(QStringLiteral("kissHost"), QStringLiteral("127.0.0.1")).toString(),
                      quint16(s.value(QStringLiteral("kissPort"), 8001).toInt()));
    m_kiss->setPath(pathList());
    applyLogin();
    const bool is = s.value(QStringLiteral("isEnabled"), false).toBool();
    const bool kiss = s.value(QStringLiteral("kissEnabled"), false).toBool();
    ModemSettings m;
    m.audioIn = s.value(QStringLiteral("modemAudioIn"), m.audioIn).toString();
    m.audioOut = s.value(QStringLiteral("modemAudioOut"), m.audioOut).toString();
    m.ptt = s.value(QStringLiteral("modemPtt"), m.ptt).toString();
    m.pttDevice = s.value(QStringLiteral("modemPttDevice"), m.pttDevice).toString();
    m.gpio = s.value(QStringLiteral("modemGpio"), m.gpio).toInt();
    m.txDelayMs = s.value(QStringLiteral("modemTxDelay"), m.txDelayMs).toInt();
    m.txTailMs = s.value(QStringLiteral("modemTxTail"), m.txTailMs).toInt();
    m_modemSettings = m.normalized();
    m_modemEnabled = s.value(QStringLiteral("modemEnabled"), false).toBool();
    s.endGroup();
    m_is->setEnabled(is);
    m_kiss->setEnabled(kiss);
#ifdef SATER_MODEM
    m_modem->setCallsign(m_myCallsign);
    m_modem->setPath(pathList());
    m_modem->setSettings(m_modemSettings);
    // Once the event loop runs, the window up: on Android the modem may
    // first ask for the microphone, which needs the activity on screen.
    QTimer::singleShot(0, this, [this]() { m_modem->setEnabled(m_modemEnabled); });
#endif
}

void AprsLink::saveSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("aprs"));
    s.setValue(QStringLiteral("callsign"), m_myCallsign);
    s.setValue(QStringLiteral("passcode"), m_passcode);
    s.setValue(QStringLiteral("path"), m_rfPath);
    s.setValue(QStringLiteral("isEnabled"), m_is->enabled());
    s.setValue(QStringLiteral("isHost"), m_is->host());
    s.setValue(QStringLiteral("isPort"), int(m_is->port()));
    s.setValue(QStringLiteral("kissEnabled"), m_kiss->enabled());
    s.setValue(QStringLiteral("kissHost"), m_kiss->host());
    s.setValue(QStringLiteral("kissPort"), int(m_kiss->port()));
    s.setValue(QStringLiteral("modemEnabled"), m_modemEnabled);
    s.setValue(QStringLiteral("modemAudioIn"), m_modemSettings.audioIn);
    s.setValue(QStringLiteral("modemAudioOut"), m_modemSettings.audioOut);
    s.setValue(QStringLiteral("modemPtt"), m_modemSettings.ptt);
    s.setValue(QStringLiteral("modemPttDevice"), m_modemSettings.pttDevice);
    s.setValue(QStringLiteral("modemGpio"), m_modemSettings.gpio);
    s.setValue(QStringLiteral("modemTxDelay"), m_modemSettings.txDelayMs);
    s.setValue(QStringLiteral("modemTxTail"), m_modemSettings.txTailMs);
    s.endGroup();
}

void AprsLink::applyLogin()
{
    bool ok = false;
    const int pass = m_passcode.trimmed().toInt(&ok);
    m_is->setLogin(m_myCallsign, ok ? pass : -1);
}

void AprsLink::setMyCallsign(const QString &c)
{
    const QString n = normalizeCallsign(c);
    if (n == m_myCallsign) return;
    m_myCallsign = n;
    applyLogin();
#ifdef SATER_MODEM
    m_modem->setCallsign(n);
#endif
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setPasscode(const QString &p)
{
    if (p.trimmed() == m_passcode) return;
    m_passcode = p.trimmed();
    applyLogin();
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setRfPath(const QString &p)
{
    const QString v = p.trimmed().toUpper().remove(QLatin1Char(' '));
    if (v == m_rfPath) return;
    m_rfPath = v;
    m_kiss->setPath(pathList());
#ifdef SATER_MODEM
    m_modem->setPath(pathList());
#endif
    saveSettings();
    emit settingsChanged();
}

QStringList AprsLink::pathList() const
{
    QStringList out;
    for (const QString &p : m_rfPath.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (isValidCallsign(p)) out.append(normalizeCallsign(p));
    }
    return out;
}

bool AprsLink::isEnabled() const { return m_is->enabled(); }
QString AprsLink::isHost() const { return m_is->host(); }
int AprsLink::isPort() const { return m_is->port(); }
QString AprsLink::isState() const { return stateName(m_is->state()); }
QString AprsLink::isDetail() const { return m_is->detail(); }
bool AprsLink::isCanSend() const { return m_is->canSend(); }
bool AprsLink::kissEnabled() const { return m_kiss->enabled(); }
QString AprsLink::kissHost() const { return m_kiss->host(); }
int AprsLink::kissPort() const { return m_kiss->port(); }
QString AprsLink::kissState() const { return stateName(m_kiss->state()); }
QString AprsLink::kissDetail() const { return m_kiss->detail(); }

void AprsLink::setIsEnabled(bool on)
{
    if (on == m_is->enabled()) return;
    m_is->setEnabled(on);
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setIsHost(const QString &h)
{
    m_is->setServer(h, m_is->port());
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setIsPort(int p)
{
    m_is->setServer(m_is->host(), quint16(p));
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setKissEnabled(bool on)
{
    if (on == m_kiss->enabled()) return;
    m_kiss->setEnabled(on);
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setKissHost(const QString &h)
{
    m_kiss->setServer(h, m_kiss->port());
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setKissPort(int p)
{
    m_kiss->setServer(m_kiss->host(), quint16(p));
    saveSettings();
    emit settingsChanged();
}

// ---- the built-in modem -----------------------------------------------------------------

bool AprsLink::modemAvailable() const
{
#ifdef SATER_MODEM
    return true;
#else
    return false;
#endif
}

void AprsLink::setModemEnabled(bool on)
{
    if (on == m_modemEnabled) return;
    m_modemEnabled = on;
#ifdef SATER_MODEM
    m_modem->setEnabled(on);
#endif
    saveSettings();
    emit settingsChanged();
    emit stateChanged();
}

QString AprsLink::modemState() const
{
#ifdef SATER_MODEM
    return stateName(m_modem->state());
#else
    return QStringLiteral("off");
#endif
}

QString AprsLink::modemDetail() const
{
#ifdef SATER_MODEM
    return m_modem->detail();
#else
    return QString();
#endif
}

bool AprsLink::modemTransmitting() const
{
#ifdef SATER_MODEM
    return m_modem->transmitting();
#else
    return false;
#endif
}

void AprsLink::applyModemSettings(const ModemSettings &settings)
{
    const ModemSettings s = settings.normalized();
    if (s == m_modemSettings) return;
    m_modemSettings = s;
#ifdef SATER_MODEM
    m_modem->setSettings(s);
#endif
    saveSettings();
    emit settingsChanged();
}

void AprsLink::setModemAudioIn(const QString &v)
{
    ModemSettings s = m_modemSettings;
    // One sound card for both ways unless told otherwise.
    if (s.audioOut == s.audioIn) s.audioOut = v;
    s.audioIn = v;
    applyModemSettings(s);
}

void AprsLink::setModemAudioOut(const QString &v)
{
    ModemSettings s = m_modemSettings;
    s.audioOut = v;
    applyModemSettings(s);
}

void AprsLink::setModemPtt(const QString &v)
{
    ModemSettings s = m_modemSettings;
    s.ptt = v;
    applyModemSettings(s);
}

void AprsLink::setModemPttDevice(const QString &v)
{
    ModemSettings s = m_modemSettings;
    s.pttDevice = v;
    applyModemSettings(s);
}

void AprsLink::setModemGpio(int v)
{
    ModemSettings s = m_modemSettings;
    s.gpio = v;
    applyModemSettings(s);
}

void AprsLink::setModemTxDelay(int ms)
{
    ModemSettings s = m_modemSettings;
    s.txDelayMs = ms;
    applyModemSettings(s);
}

void AprsLink::setModemTxTail(int ms)
{
    ModemSettings s = m_modemSettings;
    s.txTailMs = ms;
    applyModemSettings(s);
}

QVariantList AprsLink::modemAudioDevices() const
{
    QVariantList out;
    QStringList values;
    for (const AudioDevice &d : audioDevices()) {
        QVariantMap m;
        m.insert(QStringLiteral("value"), d.value);
        m.insert(QStringLiteral("label"), d.label);
        out.append(m);
        values.append(d.value);
    }
    // A device of the settings no longer plugged in stays choosable.
    for (const QString &v : {m_modemSettings.audioIn, m_modemSettings.audioOut}) {
        if (values.contains(v)) continue;
        QVariantMap m;
        m.insert(QStringLiteral("value"), v);
        m.insert(QStringLiteral("label"), SATER_TR("%1 (not found)").arg(v));
        out.append(m);
        values.append(v);
    }
    return out;
}

QString AprsLink::stateName(AprsPort::State s)
{
    switch (s) {
    case AprsPort::State::Connected: return QStringLiteral("connected");
    case AprsPort::State::Connecting: return QStringLiteral("connecting");
    case AprsPort::State::Off: break;
    }
    return QStringLiteral("off");
}

bool AprsLink::canSend() const
{
    bool modem = false;
#ifdef SATER_MODEM
    modem = m_modem->canSend();
#endif
    return isValidCallsign(m_myCallsign) && (m_is->canSend() || m_kiss->canSend() || modem);
}

bool AprsLink::active() const
{
    return m_is->enabled() || m_kiss->enabled() || (modemAvailable() && m_modemEnabled);
}

// ---- team -------------------------------------------------------------------------------

bool AprsLink::isMine(const QString &callsign) const
{
    if (!isValidCallsign(m_myCallsign)) return false;
    const QString c = normalizeCallsign(callsign);
    if (c == m_myCallsign) return true;
    // A row without SSID is this station whatever its SSID.
    return !c.contains(QLatin1Char('-')) && c == m_myCallsign.section(QLatin1Char('-'), 0, 0);
}

bool AprsLink::addTeamCallsign(const QString &callsign)
{
    if (!m_mission) return false;
    const QString c = normalizeCallsign(callsign);
    if (!isValidCallsign(c)) return false;
    QStringList team = m_mission->team();
    if (team.contains(c)) return false;
    team.append(c);
    m_mission->setTeam(team);
    return true;
}

void AprsLink::removeTeamCallsign(int index)
{
    if (!m_mission) return;
    QStringList team = m_mission->team();
    if (index < 0 || index >= team.size()) return;
    team.removeAt(index);
    m_mission->setTeam(team);
}

int AprsLink::addTeamFromStations()
{
    if (!m_mission) return 0;
    int n = 0;
    for (const QString &c : m_mission->stations()->callsigns()) n += addTeamCallsign(c);
    return n;
}

// ---- in and out ---------------------------------------------------------------------------

void AprsLink::addLog(const QString &kind, const QString &text)
{
    QVariantMap e;
    e.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    e.insert(QStringLiteral("kind"), kind);
    e.insert(QStringLiteral("text"), text);
    m_log.prepend(e);
    while (m_log.size() > kMaxLog) m_log.removeLast();
    emit logChanged();
}

void AprsLink::clearLog()
{
    m_log.clear();
    emit logChanged();
}

void AprsLink::handlePacket(const Packet &packet, const QString &via)
{
    const QString line = packet.toTnc2();
    const auto report = parseDfReport(packet);
    if (!report) {
        addLog(QStringLiteral("other"), QStringLiteral("[%1] %2").arg(via, line));
        return;
    }
    // Our own report, back from a digipeater or from APRS-IS: this
    // station's row already has it (and its compass reading).  The very
    // callsign and SSID only: F4ABC is another station than F4ABC-7.
    if (isValidCallsign(m_myCallsign) && report->source == m_myCallsign) {
        addLog(QStringLiteral("other"), SATER_TR("[%1] %2 (our own)").arg(via, line));
        return;
    }
    if (!m_mission || m_mission->teamEntryFor(report->source).isEmpty()) {
        addLog(QStringLiteral("ignored"), SATER_TR("[%1] %2 (not in the operation)").arg(via, line));
        return;
    }
    if (!remember(*report)) {
        addLog(QStringLiteral("other"), SATER_TR("[%1] %2 (already received)").arg(via, line));
        return;
    }
    // The sender's own row: F4ABC-1 and F4ABC-2 are two teams even when
    // the operation lists F4ABC.
    m_mission->applyReceivedBearing(report->source, report->lat, report->lon, report->bearing, report->uncertainty,
                                    report->signal, report->time);
    ++m_received;
    addLog(QStringLiteral("bearing"), QStringLiteral("[%1] %2").arg(via, line));
}

bool AprsLink::remember(const DfReport &report)
{
    // The report itself, whichever way and path it came by.
    const QString key = report.source + QLatin1Char('|') + QString::number(report.lat, 'f', 5) + QLatin1Char('|')
                        + QString::number(report.lon, 'f', 5) + QLatin1Char('|') + QString::number(report.bearing)
                        + QLatin1Char('|') + report.time.toString(Qt::ISODate);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_seen.begin(); it != m_seen.end();) {
        if (now - it.value() > kDuplicateMs) it = m_seen.erase(it);
        else ++it;
    }
    if (m_seen.contains(key)) return false;
    m_seen.insert(key, now);
    return true;
}

bool AprsLink::sendBearing(int row)
{
    if (!m_mission) return false;
    StationModel *stations = m_mission->stations();
    if (row < 0 || row >= stations->count()) return false;
    const Station s = stations->stations().at(row);
    if (!isValidCallsign(m_myCallsign)) {
        m_mission->showStatus(SATER_TR("Set your callsign in the APRS settings first."));
        return false;
    }
    if (!s.hasBearing()) {
        m_mission->showStatus(SATER_TR("S0: no bearing to send."));
        return false;
    }
    DfReport r;
    r.lat = s.lat;
    r.lon = s.lon;
    r.bearing = s.trueAzimuth();
    r.uncertainty = s.uncertainty;
    r.signal = s.signal;
    r.time = QDateTime::currentDateTimeUtc();
    Packet p;
    p.source = m_myCallsign;
    p.destination = kToCall;
    p.info = encodeDfReport(r, m_mission->azimuthLengthKm());
    QStringList ways;
    bool radio = m_kiss->canSend() && m_kiss->send(p);
#ifdef SATER_MODEM
    radio = (m_modem->canSend() && m_modem->send(p)) || radio;
#endif
    if (radio) ways << SATER_TR("radio");
    if (m_is->canSend() && m_is->send(p)) ways << QStringLiteral("APRS-IS");
    if (ways.isEmpty()) {
        m_mission->showStatus(SATER_TR("The bearing could not be sent: no APRS connection can send."));
        addLog(QStringLiteral("note"), SATER_TR("Not sent: no APRS connection can send."));
        return false;
    }
    // Heard back (another station's APRS-IS feed, a digipeater): known.
    if (const auto sent = parseDfReport(p)) remember(*sent);
    Packet shown = p;
    shown.path = pathList();
    addLog(QStringLiteral("sent"), QStringLiteral("[%1] %2").arg(ways.join(QStringLiteral(" + ")), shown.toTnc2()));
    m_mission->recordStation(row);
    m_mission->showStatus(SATER_TR("Bearing sent (%1): %2° ±%3°")
                              .arg(ways.join(QStringLiteral(", ")))
                              .arg(qRound(normalizeAzimuth(r.bearing)) % 360)
                              .arg(s.uncertainty, 0, 'f', 1));
    return true;
}
