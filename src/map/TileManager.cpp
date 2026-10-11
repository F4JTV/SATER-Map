/*
 * TileManager.cpp - map tiles: memory, the tiles folder, the network.
 *
 * This file is part of SATER Map.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "TileManager.h"

#include "core/TileProviders.h"

#include <algorithm>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>

#include <memory>

namespace {

// Tiles on the screen at once and a margin around them.
#if defined(Q_OS_ANDROID)
constexpr int kMemoryKb = 48 * 1024;
constexpr qint64 kHttpCacheBytes = 150LL * 1024 * 1024;
#else
constexpr int kMemoryKb = 160 * 1024;
constexpr qint64 kHttpCacheBytes = 400LL * 1024 * 1024;
#endif
constexpr int kMaxNetwork = 4;     // parallel requests while browsing
constexpr int kMaxDisk = 2;
constexpr int kMaxBulk = 2;        // parallel requests of a bulk download
constexpr int kRetrySeconds = 30;

QThreadPool *decodePool()
{
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(2);
        return p;
    }();
    return pool;
}

QImage decode(const QByteArray &bytes)
{
    // No format hint: satellite tiles are JPEG data in .png files.
    QImage img = QImage::fromData(bytes);
    if (!img.isNull() && img.format() != QImage::Format_ARGB32_Premultiplied && img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(img.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
    return img;
}

// A tile with its labels drawn over it (just the tile when there are none).
QImage compose(const QByteArray &tile, const QByteArray &labels)
{
    QImage img = decode(tile);
    if (img.isNull() || labels.isEmpty()) return img;
    const QImage overlay = decode(labels);
    if (overlay.isNull()) return img;
    if (img.format() != QImage::Format_ARGB32_Premultiplied && img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(QImage::Format_RGB32);
    QPainter p(&img);
    p.drawImage(img.rect(), overlay);
    p.end();
    return img;
}

} // namespace

TileManager::TileManager(QObject *parent)
    : QObject(parent)
{
    m_memory.setMaxCost(kMemoryKb);
    auto *cache = new QNetworkDiskCache(&m_net);
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty()) dir = QDir::tempPath() + QStringLiteral("/SATER_Map");
    cache->setCacheDirectory(dir + QStringLiteral("/http-tiles"));
    cache->setMaximumCacheSize(kHttpCacheBytes);
    m_net.setCache(cache);
}

TileManager::~TileManager()
{
    cancelDownload();
    // Decoding tasks post back to this object: let them finish first.
    decodePool()->waitForDone();
}

QString TileManager::userAgent()
{
    return QStringLiteral("SATER_Map/%1 (Amateur Radio Emergency Service)").arg(QCoreApplication::applicationVersion());
}

void TileManager::setTilesDir(const QString &dir)
{
    const QString clean = QDir::cleanPath(dir);
    if (clean == m_tilesDir) return;
    m_tilesDir = clean;
    QDir().mkpath(m_tilesDir);
    m_memory.clear();
    m_failed.clear();
    emit tilesDirChanged();
    recount();
    emit tileArrived();
}

QString TileManager::keyOf(const QString &provider, int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3/%4").arg(provider).arg(z).arg(x).arg(y);
}

QString TileManager::tilePath(const QString &provider, int z, int x, int y) const
{
    return QStringLiteral("%1/%2/%3/%4/%5.png").arg(m_tilesDir, provider).arg(z).arg(x).arg(y);
}

void TileManager::beginFrame()
{
    ++m_frame;
}

QImage TileManager::peek(const QString &provider, int z, int x, int y) const
{
    const QImage *img = m_memory.object(keyOf(provider, z, x, y));
    return img ? *img : QImage();
}

QImage TileManager::tile(const QString &provider, int z, int x, int y, bool offline)
{
    const QString key = keyOf(provider, z, x, y);
    if (const QImage *img = m_memory.object(key)) return *img;
    if (m_inFlight.contains(key)) return QImage();

    auto failed = m_failed.constFind(key);
    if (failed != m_failed.constEnd()) {
        if (failed.value().secsTo(QDateTime::currentDateTimeUtc()) < kRetrySeconds) return QImage();
        m_failed.remove(key);
    }
    Pending &p = m_queue[key];
    p.provider = provider;
    p.z = z;
    p.x = x;
    p.y = y;
    p.frame = m_frame;
    p.offline = offline;
    pump();
    return QImage();
}

void TileManager::pump()
{
    // tile() is called from MapView::paint(), on the scene graph's render
    // thread wherever Qt uses one (Direct3D on Windows, Android, most Linux
    // GPU drivers), while the GUI thread is blocked: reading the queue and
    // the memory cache there is safe, but the network replies must be
    // created on the thread of the network manager.  The requests are
    // started from the GUI thread, once for all the tiles of a frame.
    if (QThread::currentThread() != thread()) {
        if (!m_pumpPosted.exchange(true)) {
            QMetaObject::invokeMethod(this, [this]() {
                m_pumpPosted = false;
                pump();
            }, Qt::QueuedConnection);
        }
        return;
    }
    // Requests the last frames did not renew are off the screen now.
    for (auto it = m_queue.begin(); it != m_queue.end();) {
        if (it->frame + 2 < m_frame) it = m_queue.erase(it);
        else ++it;
    }
    while (!m_queue.isEmpty()) {
        // Newest first: what the user looks at now.
        auto best = m_queue.end();
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (best == m_queue.end() || it->frame > best->frame) best = it;
        }
        const QString key = best.key();
        const Pending p = best.value();
        const bool onDisk = QFileInfo::exists(tilePath(p.provider, p.z, p.x, p.y));
        if (onDisk) {
            if (m_diskRunning >= kMaxDisk) break;
            m_queue.erase(best);
            startDiskLoad(key, p);
        } else {
            if (m_netRunning >= kMaxNetwork) break;
            m_queue.erase(best);
            startNetworkLoad(key, p);
        }
    }
}

void TileManager::startDiskLoad(const QString &key, const Pending &p)
{
    m_inFlight.insert(key);
    ++m_diskRunning;
    const QString path = tilePath(p.provider, p.z, p.x, p.y);
    decodePool()->start([this, key, path]() {
        QFile f(path);
        QImage img;
        if (f.open(QIODevice::ReadOnly)) img = decode(f.readAll());
        QMetaObject::invokeMethod(this, [this, key, img]() {
            --m_diskRunning;
            onDecoded(key, img, true, !img.isNull(), QString());
        }, Qt::QueuedConnection);
    });
}

void TileManager::requestTile(const QString &provider, int z, int x, int y, QNetworkRequest::CacheLoadControl load,
                              QList<QPointer<QNetworkReply>> *track, Fetched done)
{
    const sater::TileProvider &prov = sater::tileProvider(provider);
    const int index = m_requestIndex++;
    QStringList urls = {sater::tileUrl(prov, z, x, y, index)};
    if (!prov.labelsTemplate.isEmpty()) urls.append(sater::labelsUrl(prov, z, x, y, index));

    struct State {
        QByteArray bytes[2];
        QString error;
        int pending = 0;
    };
    auto state = std::make_shared<State>();
    state->pending = int(urls.size());
    for (int i = 0; i < urls.size(); ++i) {
        QNetworkRequest req{QUrl(urls.at(i))};
        req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, load);
        req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        req.setTransferTimeout(20000);
        QNetworkReply *reply = m_net.get(req);
        if (track) track->append(reply);
        connect(reply, &QNetworkReply::finished, this, [reply, track, state, i, done]() {
            reply->deleteLater();
            if (track) track->removeAll(reply);
            if (reply->error() != QNetworkReply::NoError) {
                if (state->error.isEmpty()) state->error = reply->errorString();
            } else {
                state->bytes[i] = reply->readAll();
            }
            if (--state->pending == 0) done(state->bytes[0], state->bytes[1], state->error);
        });
    }
}

void TileManager::startNetworkLoad(const QString &key, const Pending &p)
{
    m_inFlight.insert(key);
    ++m_netRunning;
    requestTile(p.provider, p.z, p.x, p.y, p.offline ? QNetworkRequest::AlwaysCache : QNetworkRequest::PreferCache,
                nullptr, [this, key](const QByteArray &tile, const QByteArray &labels, const QString &error) {
        --m_netRunning;
        if (!error.isEmpty()) {
            onDecoded(key, QImage(), false, false, error);
            return;
        }
        ++m_diskRunning; // decoding takes a disk slot
        decodePool()->start([this, key, tile, labels]() {
            const QImage img = compose(tile, labels);
            QMetaObject::invokeMethod(this, [this, key, img]() {
                --m_diskRunning;
                onDecoded(key, img, false, !img.isNull(), QString());
            }, Qt::QueuedConnection);
        });
    });
}

void TileManager::onDecoded(const QString &key, const QImage &image, bool fromDisk, bool ok, const QString &error)
{
    Q_UNUSED(fromDisk)
    Q_UNUSED(error)
    m_inFlight.remove(key);
    if (ok) {
        m_memory.insert(key, new QImage(image), int(image.sizeInBytes() / 1024) + 1);
        emit tileArrived();
    } else {
        m_failed.insert(key, QDateTime::currentDateTimeUtc());
    }
    pump();
}

// ---- bulk download ------------------------------------------------------------

void TileManager::startDownload(const QString &provider, const QList<Key> &tiles)
{
    cancelDownload();
    m_dl = Download();
    m_dl.active = true;
    m_dl.provider = provider;
    m_dl.tiles = tiles;
    m_dl.total = int(tiles.size());
    emit downloadChanged();
    if (tiles.isEmpty()) {
        finishDownload();
        return;
    }
    for (int i = 0; i < kMaxBulk; ++i) downloadNext();
}

void TileManager::downloadNext()
{
    while (m_dl.active && m_dl.next < m_dl.tiles.size()) {
        if (m_dl.running >= kMaxBulk) return;
        const Key k = m_dl.tiles.at(m_dl.next++);
        const QString path = tilePath(m_dl.provider, k.z, k.x, k.y);
        if (QFileInfo::exists(path)) {
            ++m_dl.done; // already there (the Python "(cache)")
            emit downloadChanged();
            continue;
        }
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            ++m_dl.done;
            ++m_dl.failed;
            m_dl.lastError = tr("Cannot create the folder %1").arg(QFileInfo(path).absolutePath());
            emit downloadChanged();
            continue;
        }
        ++m_dl.running;
        const QString host = QUrl(sater::tileUrl(sater::tileProvider(m_dl.provider), k.z, k.x, k.y)).host();
        requestTile(m_dl.provider, k.z, k.x, k.y, QNetworkRequest::PreferCache, &m_dl.replies,
                    [this, path, host](const QByteArray &tile, const QByteArray &labels, const QString &error) {
            if (!m_dl.active) return;
            --m_dl.running;
            ++m_dl.done;
            if (!error.isEmpty()) {
                ++m_dl.failed;
                m_dl.lastError = error;
            } else {
                // The tile as it came, or with its labels: one image, so the
                // folder keeps the Python layout and works offline as it is.
                QByteArray bytes = tile;
                QImage img;
                if (labels.isEmpty()) {
                    img = QImage::fromData(tile);
                } else {
                    img = compose(tile, labels);
                    QBuffer buffer(&bytes);
                    bytes.clear();
                    if (img.isNull() || !buffer.open(QIODevice::WriteOnly) || !img.save(&buffer, "PNG")) img = QImage();
                }
                if (img.isNull()) {
                    ++m_dl.failed;
                    m_dl.lastError = tr("The server did not send an image (%1)").arg(host);
                } else {
                    QSaveFile f(path);
                    if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit()) {
                        ++m_dl.saved;
                    } else {
                        ++m_dl.failed;
                        m_dl.lastError = f.errorString();
                    }
                }
            }
            emit downloadChanged();
            downloadNext();
        });
    }
    if (m_dl.active && m_dl.running == 0 && m_dl.next >= m_dl.tiles.size()) finishDownload();
}

void TileManager::finishDownload()
{
    m_dl.active = false;
    emit downloadChanged();
    emit downloadFinished(m_dl.saved, m_dl.failed);
    m_memory.clear(); // files may replace what was shown
    m_failed.clear();
    recount();
    emit tileArrived();
}

void TileManager::cancelDownload()
{
    if (!m_dl.active) return;
    m_dl.active = false;
    const auto replies = m_dl.replies;
    m_dl.replies.clear();
    for (const QPointer<QNetworkReply> &r : replies) {
        if (r) r->abort();
    }
    m_dl.running = 0;
    emit downloadChanged();
    emit downloadFinished(m_dl.saved, m_dl.failed);
    recount();
}

void TileManager::recount()
{
    const QString dir = m_tilesDir;
    if (dir.isEmpty()) return;
    decodePool()->start([this, dir]() {
        int n = 0;
        QDirIterator it(dir, {QStringLiteral("*.png")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++n;
        }
        QMetaObject::invokeMethod(this, [this, n]() {
            if (n != m_storedCount) {
                m_storedCount = n;
                emit storedTileCountChanged();
            }
        }, Qt::QueuedConnection);
    });
}

int TileManager::storedTilesFor(const QString &provider) const
{
    int n = 0;
    QDirIterator it(m_tilesDir + QLatin1Char('/') + provider, {QStringLiteral("*.png")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}
