/*
 * Tr.h - translatable strings outside QObject classes.
 *
 * Every user-visible string is written in English and goes through Qt's
 * translation machinery; the French catalog (src/i18n/catalog_fr.inc) is
 * compiled in.  scripts/list_tr_strings.py finds SATER_TR(), tr() and
 * qsTr() calls and checks that each has a French entry.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QCoreApplication>

#define SATER_TR(text) QCoreApplication::translate("SaterMap", text)
