/*
 * Translator.cpp - the French interface, compiled in.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "Translator.h"

namespace {

struct Entry {
    const char *source;
    const char *french;
};

const Entry kCatalog[] = {
#include "catalog_fr.inc"
};

} // namespace

CatalogTranslator::CatalogTranslator(QObject *parent)
    : QTranslator(parent)
{
    for (const Entry &e : kCatalog) m_catalog.insert(QString::fromUtf8(e.source), QString::fromUtf8(e.french));
}

QString CatalogTranslator::translate(const char *context, const char *sourceText, const char *disambiguation,
                                     int n) const
{
    Q_UNUSED(context)
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)
    if (!sourceText) return QString();
    return m_catalog.value(QString::fromUtf8(sourceText));
}
