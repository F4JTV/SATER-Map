/*
 * AppController.cpp - theme, language, platform facts and small services.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AppController.h"

#include "i18n/Translator.h"
#include "platform/AndroidUi.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>

namespace {
const QStringList kThemes = {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("red"),
                             QStringLiteral("amber")};
}

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_translator(new CatalogTranslator(this))
{
    QSettings s;
    const QString theme = s.value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString();
    m_theme = kThemes.contains(theme) ? theme : QStringLiteral("light");
    const QString lang = s.value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    m_language = (lang == QLatin1String("en") || lang == QLatin1String("fr")) ? lang : QStringLiteral("auto");
    m_keepScreenOn = s.value(QStringLiteral("ui/keepScreenOn"), false).toBool();
    applyLanguage();
    androidui::setKeepScreenOn(m_keepScreenOn);
}

AppController::~AppController() = default;

void AppController::setTheme(const QString &theme)
{
    if (!kThemes.contains(theme) || theme == m_theme) return;
    m_theme = theme;
    QSettings().setValue(QStringLiteral("ui/theme"), m_theme);
    emit themeChanged();
}

void AppController::setLanguage(const QString &language)
{
    const QString lang = (language == QLatin1String("en") || language == QLatin1String("fr")) ? language
                                                                                              : QStringLiteral("auto");
    if (lang == m_language) return;
    m_language = lang;
    QSettings().setValue(QStringLiteral("ui/language"), m_language);
    applyLanguage();
    if (m_engine) m_engine->retranslate();
    emit languageChanged();
}

void AppController::applyLanguage()
{
    bool wantFrench = m_language == QLatin1String("fr");
    if (m_language == QLatin1String("auto")) wantFrench = QLocale::system().language() == QLocale::French;
    if (wantFrench == m_installed) return;
    if (wantFrench) QCoreApplication::installTranslator(m_translator);
    else QCoreApplication::removeTranslator(m_translator);
    m_installed = wantFrench;
}

void AppController::setKeepScreenOn(bool on)
{
    if (on == m_keepScreenOn) return;
    m_keepScreenOn = on;
    QSettings().setValue(QStringLiteral("ui/keepScreenOn"), on);
    androidui::setKeepScreenOn(on);
    emit keepScreenOnChanged();
}

bool AppController::android() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

QString AppController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

bool AppController::flatRendering() const
{
    return QQuickWindow::graphicsApi() == QSGRendererInterface::Software;
}

void AppController::copyToClipboard(const QString &text)
{
    if (QClipboard *cb = QGuiApplication::clipboard()) cb->setText(text);
}

void AppController::haptic()
{
    androidui::haptic();
}

void AppController::activated()
{
    androidui::hideSystemBars();
    androidui::setKeepScreenOn(m_keepScreenOn);
}
