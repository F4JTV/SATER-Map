/*
 * Translator.h - the French interface, compiled in.
 *
 * The strings are written in English in the code (tr(), qsTr(),
 * SATER_TR()); this translator looks each one up in catalog_fr.inc,
 * whatever its context, so one entry covers a string used in C++ and QML
 * alike.  No .qm file to build or ship: nothing needed from Qt Linguist on
 * any of the three platforms.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QString>
#include <QTranslator>

class CatalogTranslator : public QTranslator
{
    Q_OBJECT
public:
    explicit CatalogTranslator(QObject *parent = nullptr);

    bool isEmpty() const override { return m_catalog.isEmpty(); }
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;

    // The French text for an English source string, or a null string.
    QString lookup(const QString &source) const { return m_catalog.value(source); }
    int size() const { return int(m_catalog.size()); }

private:
    QHash<QString, QString> m_catalog;
};
