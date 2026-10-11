/*
 * AppController.h - the application side of the interface ("app" in QML):
 * theme, language, platform facts and small services.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class CatalogTranslator;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // True when the French catalog is in use.
    Q_PROPERTY(bool french READ french NOTIFY languageChanged)
    Q_PROPERTY(bool keepScreenOn READ keepScreenOn WRITE setKeepScreenOn NOTIFY keepScreenOnChanged)
    Q_PROPERTY(bool android READ android CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    // The software scene graph (headless tests, old GPUs): no shadows.
    Q_PROPERTY(bool flatRendering READ flatRendering CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    // The engine whose bindings are re-evaluated on a language change.
    void setEngine(QQmlEngine *engine) { m_engine = engine; }

    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    QString language() const { return m_language; }
    void setLanguage(const QString &language);
    bool french() const { return m_installed; }
    bool keepScreenOn() const { return m_keepScreenOn; }
    void setKeepScreenOn(bool on);
    bool android() const;
    QString version() const;
    QString qtVersion() const;
    bool flatRendering() const;

    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void haptic();
    // Called when the application comes to the foreground.
    void activated();

signals:
    void themeChanged();
    void languageChanged();
    void keepScreenOnChanged();

private:
    void applyLanguage();

    QString m_theme = QStringLiteral("light");
    QString m_language = QStringLiteral("auto");
    bool m_keepScreenOn = false;
    bool m_installed = false;
    CatalogTranslator *m_translator = nullptr;
    QPointer<QQmlEngine> m_engine;
};
