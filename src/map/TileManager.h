/*
 * TileManager.h - map tiles: memory, the tiles folder, the network.
 *
 * Where a tile comes from, first match wins:
 *   1. memory (a few hundred decoded tiles);
 *   2. the tiles folder, <tiles>/<provider>/<z>/<x>/<y>.png - the layout of
 *      the Python version, so tiles it downloaded are used as they are
 *      (satellite tiles are JPEG data under a .png name there too);
 *   3. the network, through an HTTP disk cache - never in offline mode,
 *      where only what is already on disk is shown.
 *
 * "Download visible tiles" writes into the tiles folder, which nothing
 * ever prunes; the HTTP cache is size-capped.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include <atomic>
#include <functional>

class QNetworkReply;

class TileManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString tilesDir READ tilesDir WRITE setTilesDir NOTIFY tilesDirChanged)
    Q_PROPERTY(int storedTileCount READ storedTileCount NOTIFY storedTileCountChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadChanged)
    Q_PROPERTY(int downloadTotal READ downloadTotal NOTIFY downloadChanged)
    Q_PROPERTY(int downloadDone READ downloadDone NOTIFY downloadChanged)
    Q_PROPERTY(int downloadFailed READ downloadFailed NOTIFY downloadChanged)
    Q_PROPERTY(int downloadSaved READ downloadSaved NOTIFY downloadChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY downloadChanged)

public:
    explicit TileManager(QObject *parent = nullptr);
    ~TileManager() override;

    QString tilesDir() const { return m_tilesDir; }
    void setTilesDir(const QString &dir);
    int storedTileCount() const { return m_storedCount; }

    // The path of a tile in the tiles folder.
    QString tilePath(const QString &provider, int z, int x, int y) const;

    // ---- drawing ----------------------------------------------------------
    // Call before requesting the tiles of a new frame: requests not renewed
    // for a few frames are dropped from the queue.
    void beginFrame();
    // The decoded tile, or a null image while it loads (tileArrived() is
    // emitted when it is in).  offline: never touch the network.
    QImage tile(const QString &provider, int z, int x, int y, bool offline);
    // The tile if it is in memory, without loading anything.
    QImage peek(const QString &provider, int z, int x, int y) const;

    // ---- bulk download -----------------------------------------------------
    struct Key {
        int z = 0;
        int x = 0;
        int y = 0;
    };
    // Store every tile of the list in the tiles folder.
    void startDownload(const QString &provider, const QList<Key> &tiles);
    Q_INVOKABLE void cancelDownload();
    bool downloading() const { return m_dl.active; }
    int downloadTotal() const { return m_dl.total; }
    int downloadDone() const { return m_dl.done; }
    int downloadFailed() const { return m_dl.failed; }
    int downloadSaved() const { return m_dl.saved; }
    QString lastError() const { return m_dl.lastError; }

    // Count the tiles in the folder again (in the background).
    Q_INVOKABLE void recount();
    // Tiles of one provider stored on disk (counted synchronously).
    Q_INVOKABLE int storedTilesFor(const QString &provider) const;

    static QString userAgent();

signals:
    void tilesDirChanged();
    void tileArrived();
    void storedTileCountChanged();
    void downloadChanged();
    void downloadFinished(int saved, int failed);

private:
    struct Pending {
        QString provider;
        int z = 0, x = 0, y = 0;
        quint64 frame = 0;
        bool offline = false;
    };
    static QString keyOf(const QString &provider, int z, int x, int y);
    void pump();
    void startDiskLoad(const QString &key, const Pending &p);
    void startNetworkLoad(const QString &key, const Pending &p);
    // The request of one tile and, for a provider with a labels layer, of
    // its labels tile; done gets the bytes of both (labels empty when there
    // are none) once all have arrived, or the first error.  The replies are
    // added to track, when given, while they run.
    using Fetched = std::function<void(const QByteArray &tile, const QByteArray &labels, const QString &error)>;
    void requestTile(const QString &provider, int z, int x, int y, QNetworkRequest::CacheLoadControl load,
                     QList<QPointer<QNetworkReply>> *track, Fetched done);
    void onDecoded(const QString &key, const QImage &image, bool fromDisk, bool ok, const QString &error);
    void downloadNext();
    void finishDownload();

    QString m_tilesDir;
    QNetworkAccessManager m_net;
    QCache<QString, QImage> m_memory;
    QHash<QString, Pending> m_queue;          // waiting
    QSet<QString> m_inFlight;                 // disk or network
    QHash<QString, QDateTime> m_failed;       // retried after a while
    quint64 m_frame = 0;
    std::atomic<bool> m_pumpPosted{false};
    int m_netRunning = 0;
    int m_diskRunning = 0;
    int m_storedCount = 0;
    int m_requestIndex = 0;

    struct Download {
        bool active = false;
        QString provider;
        QList<Key> tiles;
        int next = 0;
        int running = 0;
        int total = 0;
        int done = 0;
        int failed = 0;
        int saved = 0;
        QString lastError;
        QList<QPointer<QNetworkReply>> replies;
    } m_dl;
};
