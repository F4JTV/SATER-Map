/*
 * AprsLink.h - APRS for the mission: the field teams' bearings in, this
 * station's bearing (and those it took down for others) out.
 *
 * Bearings travel as standard APRS DF reports (aprs/DfReport.h) over
 * APRS-IS (internet), a KISS TNC on the network (Dire Wolf or Soundmodem on
 * the command post's PC) and, on Android, the modem built into the
 * application (aprs/ModemPort.h, when built with SATER_MODEM).  A report
 * is taken in only from the operation's callsigns (Mission::team): the
 * station of that callsign moves to where the bearing was taken, gets the
 * bearing, and the bearing goes into the history.
 *
 * The bearing of a station that has no APRS (read out by telephone) goes
 * out as an APRS object named after it, from this station's callsign; an
 * object from one of the operation's callsigns updates the station it
 * names, as that station's own report would.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "aprs/AprsPort.h"
#include "aprs/ModemSettings.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantList>

class Mission;
namespace sater::aprs {
class AprsIsClient;
class KissTcpClient;
class ModemPort;
struct DfReport;
}

class AprsLink : public QObject
{
    Q_OBJECT
    // This station: callsign with SSID, APRS-IS passcode (empty: receive only).
    Q_PROPERTY(QString myCallsign READ myCallsign WRITE setMyCallsign NOTIFY settingsChanged)
    Q_PROPERTY(QString passcode READ passcode WRITE setPasscode NOTIFY settingsChanged)
    Q_PROPERTY(QString rfPath READ rfPath WRITE setRfPath NOTIFY settingsChanged)

    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setIsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString isHost READ isHost WRITE setIsHost NOTIFY settingsChanged)
    Q_PROPERTY(int isPort READ isPort WRITE setIsPort NOTIFY settingsChanged)
    Q_PROPERTY(QString isState READ isState NOTIFY stateChanged)          // off, connecting, connected
    Q_PROPERTY(QString isDetail READ isDetail NOTIFY stateChanged)
    Q_PROPERTY(bool isCanSend READ isCanSend NOTIFY stateChanged)

    Q_PROPERTY(bool kissEnabled READ kissEnabled WRITE setKissEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString kissHost READ kissHost WRITE setKissHost NOTIFY settingsChanged)
    Q_PROPERTY(int kissPort READ kissPort WRITE setKissPort NOTIFY settingsChanged)
    Q_PROPERTY(QString kissState READ kissState NOTIFY stateChanged)
    Q_PROPERTY(QString kissDetail READ kissDetail NOTIFY stateChanged)

    // The built-in modem: there (built with SATER_MODEM), on, its state,
    // the transmitter keyed, and its settings (aprs/ModemSettings.h).
    Q_PROPERTY(bool modemAvailable READ modemAvailable CONSTANT)
    Q_PROPERTY(bool modemEnabled READ modemEnabled WRITE setModemEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString modemState READ modemState NOTIFY stateChanged)
    Q_PROPERTY(QString modemDetail READ modemDetail NOTIFY stateChanged)
    Q_PROPERTY(bool modemTransmitting READ modemTransmitting NOTIFY stateChanged)
    Q_PROPERTY(QString modemAudioIn READ modemAudioIn WRITE setModemAudioIn NOTIFY settingsChanged)
    Q_PROPERTY(QString modemAudioOut READ modemAudioOut WRITE setModemAudioOut NOTIFY settingsChanged)
    Q_PROPERTY(QString modemPtt READ modemPtt WRITE setModemPtt NOTIFY settingsChanged)   // none, cm108, rts, dtr
    Q_PROPERTY(QString modemPttDevice READ modemPttDevice WRITE setModemPttDevice NOTIFY settingsChanged)
    Q_PROPERTY(int modemGpio READ modemGpio WRITE setModemGpio NOTIFY settingsChanged)
    Q_PROPERTY(int modemTxDelay READ modemTxDelay WRITE setModemTxDelay NOTIFY settingsChanged)   // ms
    Q_PROPERTY(int modemTxTail READ modemTxTail WRITE setModemTxTail NOTIFY settingsChanged)     // ms

    // A bearing can go out now, by some way.
    Q_PROPERTY(bool canSend READ canSend NOTIFY stateChanged)
    // Something is on (for an indicator).
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    // The latest packets and events, newest first: {time, kind, text}; kind
    // is "bearing" (taken in), "ignored", "sent", "other", "note".
    Q_PROPERTY(QVariantList log READ log NOTIFY logChanged)
    Q_PROPERTY(int bearingsReceived READ bearingsReceived NOTIFY logChanged)

public:
    explicit AprsLink(Mission *mission, QObject *parent = nullptr);
    ~AprsLink() override;

    QString myCallsign() const { return m_myCallsign; }
    void setMyCallsign(const QString &c);
    QString passcode() const { return m_passcode; }
    void setPasscode(const QString &p);
    QString rfPath() const { return m_rfPath; }
    void setRfPath(const QString &p);

    bool isEnabled() const;
    void setIsEnabled(bool on);
    QString isHost() const;
    void setIsHost(const QString &h);
    int isPort() const;
    void setIsPort(int p);
    QString isState() const;
    QString isDetail() const;
    bool isCanSend() const;

    bool kissEnabled() const;
    void setKissEnabled(bool on);
    QString kissHost() const;
    void setKissHost(const QString &h);
    int kissPort() const;
    void setKissPort(int p);
    QString kissState() const;
    QString kissDetail() const;

    bool modemAvailable() const;
    bool modemEnabled() const { return m_modemEnabled; }
    void setModemEnabled(bool on);
    QString modemState() const;
    QString modemDetail() const;
    bool modemTransmitting() const;
    QString modemAudioIn() const { return m_modemSettings.audioIn; }
    void setModemAudioIn(const QString &v);
    QString modemAudioOut() const { return m_modemSettings.audioOut; }
    void setModemAudioOut(const QString &v);
    QString modemPtt() const { return m_modemSettings.ptt; }
    void setModemPtt(const QString &v);
    QString modemPttDevice() const { return m_modemSettings.pttDevice; }
    void setModemPttDevice(const QString &v);
    int modemGpio() const { return m_modemSettings.gpio; }
    void setModemGpio(int v);
    int modemTxDelay() const { return m_modemSettings.txDelayMs; }
    void setModemTxDelay(int ms);
    int modemTxTail() const { return m_modemSettings.txTailMs; }
    void setModemTxTail(int ms);
    // The sound cards, [{value, label}], the system default first.
    Q_INVOKABLE QVariantList modemAudioDevices() const;

    bool canSend() const;
    bool active() const;
    QVariantList log() const { return m_log; }
    int bearingsReceived() const { return m_received; }

    // Is this station's row this station's own (its bearing can be sent)?
    Q_INVOKABLE bool isMine(const QString &callsign) const;
    // Send the bearing of a station row by every way that can send, and
    // record it in the history: this station's own row as a DF report,
    // another as an object named after its callsign.  False, with a status
    // message, if it could not go out.
    Q_INVOKABLE bool sendBearing(int row);
    // The team list helpers.
    Q_INVOKABLE bool addTeamCallsign(const QString &callsign);
    Q_INVOKABLE void removeTeamCallsign(int index);
    // The callsigns of the station table added to the team; how many.
    Q_INVOKABLE int addTeamFromStations();
    Q_INVOKABLE void clearLog();

    // A packet as if received (the ports call it; so do the tests).
    void handlePacket(const sater::aprs::Packet &packet, const QString &via);
    // The ports, for the tests.
    sater::aprs::AprsIsClient *aprsIs() const { return m_is; }
    sater::aprs::KissTcpClient *kiss() const { return m_kiss; }
    sater::aprs::ModemPort *modem() const { return m_modem; }

signals:
    void settingsChanged();
    void stateChanged();
    void logChanged();

private:
    void loadSettings();
    void saveSettings() const;
    void applyLogin();
    void applyModemSettings(const sater::aprs::ModemSettings &s);
    void addLog(const QString &kind, const QString &text);
    // Note a report as received (or sent); false if it already was.
    bool remember(const sater::aprs::DfReport &report);
    QStringList pathList() const;
    static QString stateName(sater::aprs::AprsPort::State s);

    QPointer<Mission> m_mission;
    sater::aprs::AprsIsClient *m_is = nullptr;
    sater::aprs::KissTcpClient *m_kiss = nullptr;
    sater::aprs::ModemPort *m_modem = nullptr;     // null without SATER_MODEM
    bool m_modemEnabled = false;
    sater::aprs::ModemSettings m_modemSettings;
    QString m_myCallsign;
    QString m_passcode;
    QString m_rfPath = QStringLiteral("WIDE1-1,WIDE2-1");
    QVariantList m_log;
    int m_received = 0;
    QHash<QString, qint64> m_seen;      // packet -> when, msecs (duplicates by RF and APRS-IS)
};
