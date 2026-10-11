/*
 * MissionIO.h - reading and writing missions, presets and history.
 *
 * The JSON layout is the Python version's export_json()/import_json() one,
 * key for key: a file written here imports into SATER Map 2.0 (Python)
 * and the other way round.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "MissionData.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace sater::io {

// ---- missions ------------------------------------------------------------
QJsonObject stationToJson(const Station &s);
Station stationFromJson(const QJsonObject &o);
QJsonObject recordToJson(const AzimuthRecord &r);
AzimuthRecord recordFromJson(const QJsonObject &o);

QByteArray missionToJson(const MissionDocument &doc);
// False (and *error set) when the bytes are not a mission file.
bool missionFromJson(const QByteArray &bytes, MissionDocument *doc, QString *error);

// ---- presets (station_presets.json, a bare list) ---------------------------
QByteArray presetsToJson(const QList<StationPreset> &presets);
QList<StationPreset> presetsFromJson(const QByteArray &bytes, bool *ok = nullptr);

// ---- history CSV (semicolon separated, as the Python export) --------------
QByteArray historyToCsv(const QList<AzimuthRecord> &history);

// ---- files -------------------------------------------------------------------
// Write/read a file given as a local path or, on Android, a content:// URI
// string (what the system file picker hands back).
bool writeFile(const QString &pathOrUri, const QByteArray &data, QString *error);
bool readFile(const QString &pathOrUri, QByteArray *data, QString *error);

} // namespace sater::io
