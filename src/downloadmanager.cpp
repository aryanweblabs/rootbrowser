// ============================================================================
//  downloadmanager.cpp — Central download manager (moc-free, Qt 6.2+).
//
//  Design notes
//  ------------
//  · QWebEngineDownloadRequest lifecycle is quirky across Qt versions. We
//    avoid relying on its State enum; instead we track our own DownloadState
//    and use two periodic timers:
//
//      pollTimer_  (500 ms) — refresh bytes, detect isFinished()
//      speedTimer_ (1000 ms) — compute smoothed speed + ETA
//
//  · Pause/resume: QtWebEngine's pause()/resume() only work while the
//    request is alive. Once cancelled/finished, we must re-download from
//    sourceUrl. retry() does that.
//
//  · Persistence: items_ is JSON-serialized after every mutation. Speed
//    and ETA are recomputed on load (set to 0 / -1).
//
//  · State enum is written as an int to disk. Never renumber — only append.
// ============================================================================

#include "downloadmanager.h"
#include "downloadnotification.h"
#include "downloadtoast.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QRandomGenerator>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>

#include <algorithm>
#include <cmath>

// ────────────────────────────────────────────────────────────────────────────
//  Constants
// ────────────────────────────────────────────────────────────────────────────
static constexpr int kPollIntervalMs  = 500;
static constexpr int kSpeedIntervalMs = 1000;

// ────────────────────────────────────────────────────────────────────────────
//  Singleton
// ────────────────────────────────────────────────────────────────────────────
DownloadManager& DownloadManager::instance() {
    static DownloadManager s;
    return s;
}

DownloadManager::DownloadManager() {
    defaultFolder_ = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultFolder_.isEmpty())
        defaultFolder_ = QDir::homePath() + "/Downloads";
    QDir().mkpath(defaultFolder_);

    load();

    // 500 ms poll: bytes + completion detection
    pollTimer_ = new QTimer();
    pollTimer_->setInterval(kPollIntervalMs);
    QObject::connect(pollTimer_, &QTimer::timeout,
                     [this] { pollRequests(); });
    pollTimer_->start();

    // 1 Hz speed/ETA tick
    speedTimer_ = new QTimer();
    speedTimer_->setInterval(kSpeedIntervalMs);
    QObject::connect(speedTimer_, &QTimer::timeout,
                     [this] { tick(); });
    speedTimer_->start();
}

DownloadManager::~DownloadManager() {
    if (pollTimer_)  pollTimer_->stop();
    if (speedTimer_) speedTimer_->stop();
}

// ────────────────────────────────────────────────────────────────────────────
//  Paths & persistence
// ────────────────────────────────────────────────────────────────────────────
QString DownloadManager::settingsPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/downloads.json";
}

void DownloadManager::load() {
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return;
    const QJsonObject root = doc.object();

    defaultFolder_ = root.value("defaultFolder").toString(defaultFolder_);
    if (defaultFolder_.isEmpty())
        defaultFolder_ = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(defaultFolder_);

    items_.clear();
    for (const QJsonValue& v : root.value("items").toArray()) {
        const QJsonObject o = v.toObject();
        DownloadItem it;
        it.id            = o.value("id").toString();
        it.fileName      = o.value("fileName").toString();
        it.fullPath      = o.value("fullPath").toString();
        it.sourceUrl     = o.value("sourceUrl").toString();
        it.totalBytes    = qint64(o.value("totalBytes").toDouble(0));
        it.receivedBytes = qint64(o.value("receivedBytes").toDouble(0));
        it.startedAt     = qint64(o.value("startedAt").toDouble(0));
        it.finishedAt    = qint64(o.value("finishedAt").toDouble(0));
        it.pausedAt      = qint64(o.value("pausedAt").toDouble(0));
        it.state         = o.value("state").toInt(int(DownloadState::Queued));
        it.mimeType      = o.value("mimeType").toString();
        if (it.id.isEmpty()) continue;

        // Sanitize: any download that was mid-flight when the app died is
        // now unrecoverable (its QWebEngineDownloadRequest pointer is gone).
        if (it.state == int(DownloadState::InProgress)
         || it.state == int(DownloadState::Queued)
         || it.state == int(DownloadState::Paused))
        {
            it.state = int(DownloadState::Interrupted);
            if (it.finishedAt == 0)
                it.finishedAt = QDateTime::currentSecsSinceEpoch();
        }

        // Derived fields are not persisted.
        it.speedBps   = 0.0;
        it.etaSeconds = -1;

        items_.push_back(it);
    }
}

void DownloadManager::save() const {
    QJsonArray arr;
    for (const DownloadItem& it : items_) {
        QJsonObject o;
        o["id"]            = it.id;
        o["fileName"]      = it.fileName;
        o["fullPath"]      = it.fullPath;
        o["sourceUrl"]     = it.sourceUrl;
        o["totalBytes"]    = double(it.totalBytes);
        o["receivedBytes"] = double(it.receivedBytes);
        o["startedAt"]     = double(it.startedAt);
        o["finishedAt"]    = double(it.finishedAt);
        o["pausedAt"]      = double(it.pausedAt);
        o["state"]         = it.state;
        o["mimeType"]      = it.mimeType;
        arr.append(o);
    }
    QJsonObject root;
    root["version"]       = 2;
    root["defaultFolder"] = defaultFolder_;
    root["items"]         = arr;

    // Atomic write: write to .tmp then rename.
    const QString path = settingsPath();
    const QString tmp  = path + ".tmp";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.flush();
    f.close();

    QFile::remove(path);
    QFile::rename(tmp, path);
}

// ────────────────────────────────────────────────────────────────────────────
//  Config
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::setDefaultFolder(const QString& path) {
    if (path.isEmpty()) return;
    defaultFolder_ = path;
    QDir().mkpath(defaultFolder_);
    save();
}

// ────────────────────────────────────────────────────────────────────────────
//  Attach to a profile
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::attach(QWebEngineProfile* profile) {
    if (!profile) return;
    QObject::connect(profile, &QWebEngineProfile::downloadRequested,
                     profile, [this](QWebEngineDownloadRequest* req) {
        handleNewRequest(req);
    });
}

// ────────────────────────────────────────────────────────────────────────────
//  ID generator
// ────────────────────────────────────────────────────────────────────────────
QString DownloadManager::makeId() const {
    return QStringLiteral("%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->generate(), 8, 16, QChar('0'));
}

// ────────────────────────────────────────────────────────────────────────────
//  Handle new download request
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::handleNewRequest(QWebEngineDownloadRequest* req) {
    if (!req) return;

    QDir().mkpath(defaultFolder_);

    // Ensure a unique filename (append " (1)", " (2)" if a file exists).
    QString baseName = req->downloadFileName();
    if (baseName.isEmpty()) baseName = "download";
    const QString basePath = defaultFolder_ + "/" + baseName;

    QString target = basePath;
    if (QFile::exists(target)) {
        QFileInfo fi(basePath);
        const QString stem = fi.completeBaseName();
        const QString ext  = fi.suffix().isEmpty() ? QString() : "." + fi.suffix();
        for (int i = 1; i < 10000; ++i) {
            target = QString("%1/%2 (%3)%4").arg(defaultFolder_, stem)
                                              .arg(i).arg(ext);
            if (!QFile::exists(target)) break;
        }
    }

    const QString chosenName = QFileInfo(target).fileName();
    req->setDownloadDirectory(defaultFolder_);
    req->setDownloadFileName(chosenName);
    req->accept();

    DownloadItem it;
    it.id            = makeId();
    it.fileName      = chosenName;
    it.fullPath      = target;
    it.sourceUrl     = req->url().toString();
    it.totalBytes    = req->totalBytes();
    it.receivedBytes = req->receivedBytes();
    it.startedAt     = QDateTime::currentSecsSinceEpoch();
    it.state         = int(DownloadState::InProgress);
    it.mimeType      = req->mimeType();

    const int idx = items_.size();
    items_.push_back(it);
    live_.insert(it.id, QPointer<QWebEngineDownloadRequest>(req));

    // Live byte counters — no enum dependencies.
    QObject::connect(req, &QWebEngineDownloadRequest::receivedBytesChanged,
                     req, [this, id = it.id]() {
        const int i = indexOf(id);
        if (i < 0) return;
        if (auto lp = live_.value(id)) {
            items_[i].receivedBytes = lp->receivedBytes();
            const qint64 tot = lp->totalBytes();
            if (tot > 0) items_[i].totalBytes = tot;
        }
    });

    // Also wire the totalBytesChanged signal for servers that omit
    // Content-Length initially.
    QObject::connect(req, &QWebEngineDownloadRequest::totalBytesChanged,
                     req, [this, id = it.id]() {
        const int i = indexOf(id);
        if (i < 0) return;
        if (auto lp = live_.value(id)) {
            const qint64 tot = lp->totalBytes();
            if (tot > 0) items_[i].totalBytes = tot;
        }
    });

    Q_UNUSED(idx);
    save();

    qInfo() << "[DownloadManager] started:" << it.id
            << it.fileName << "->" << target;
}

// ────────────────────────────────────────────────────────────────────────────
//  Poll: 500 ms — refresh bytes + detect terminal state
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::pollRequests() {
    bool dirty = false;
    QList<QString> toDetach;

    for (auto it = live_.begin(); it != live_.end(); ++it) {
        const QString id = it.key();
        QPointer<QWebEngineDownloadRequest>& lp = it.value();

        // If the pointer died (deleted by Qt), mark as interrupted.
        if (!lp) {
            const int i = indexOf(id);
            if (i >= 0 && !items_[i].isFinished()) {
                items_[i].state = int(DownloadState::Interrupted);
                items_[i].finishedAt = QDateTime::currentSecsSinceEpoch();
                dirty = true;
            }
            toDetach.append(id);
            continue;
        }

        const int i = indexOf(id);
        if (i < 0) { toDetach.append(id); continue; }

        // Refresh byte counters unconditionally (harmless when finished).
        items_[i].receivedBytes = lp->receivedBytes();
        const qint64 tot = lp->totalBytes();
        if (tot > 0) items_[i].totalBytes = tot;

        // Detect terminal state once.
        if (lp->isFinished() && !items_[i].isFinished()) {
            items_[i].finishedAt = QDateTime::currentSecsSinceEpoch();
            items_[i].fullPath   = lp->downloadDirectory() + "/" + lp->downloadFileName();
            items_[i].fileName   = lp->downloadFileName();
            items_[i].receivedBytes = lp->receivedBytes();
            const qint64 t = lp->totalBytes();
            if (t > 0) items_[i].totalBytes = t;

            // Map Qt's State enum → our DownloadState.
            //   Qt: DownloadRequested=0, DownloadInProgress=1,
            //       DownloadCompleted=2, DownloadCancelled=3,
            //       DownloadInterrupted=4
            // Qt 6.5+ added DownloadInterrupted as 4; older Qt lacks it.
            const int st = int(lp->state());
            switch (st) {
                case 2:
                    items_[i].state = int(DownloadState::Completed);
                    // Native notification (if available)
                    DownloadNotification::instance().notifyComplete(
                        items_[i].id,
                        items_[i].fileName,
                        items_[i].receivedBytes,
                        items_[i].fullPath);
                    // In-app toast (always works)
                    DownloadToastManager::instance().show(
                        items_[i].id,
                        items_[i].fileName,
                        items_[i].receivedBytes,
                        items_[i].fullPath);
                    break;
                case 3: items_[i].state = int(DownloadState::Cancelled);   break;
                case 4: items_[i].state = int(DownloadState::Interrupted); break;
                default:
                    // Safety net: if isFinished() but state is ambiguous,
                    // decide by comparing byte counts.
                    if (items_[i].totalBytes > 0 &&
                        items_[i].receivedBytes >= items_[i].totalBytes)
                        items_[i].state = int(DownloadState::Completed);
                    else
                        items_[i].state = int(DownloadState::Interrupted);
                    break;
            }

            items_[i].speedBps   = 0.0;
            items_[i].etaSeconds = -1;
            dirty = true;
            toDetach.append(id);

            qInfo() << "[DownloadManager] finished:" << id
                    << items_[i].fileName
                    << "state=" << items_[i].state
                    << "(" << items_[i].receivedBytes << "/"
                    << items_[i].totalBytes << ")";
        }
    }

    // Clean up dead pointers after iteration.
    for (const QString& id : toDetach) {
        live_.remove(id);
        samples_.remove(id);
    }

    if (dirty) save();
}

// ────────────────────────────────────────────────────────────────────────────
//  Speed / ETA tick: 1 Hz
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::tick() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool dirty = false;

    for (int i = 0; i < items_.size(); ++i) {
        DownloadItem& it = items_[i];

        if (it.state != int(DownloadState::InProgress)) {
            it.speedBps   = 0.0;
            it.etaSeconds = -1;
            continue;
        }

        const qint64 bytes = it.receivedBytes;
        SpeedSample& s = samples_[it.id];

        if (s.when == 0) {
            // First sample — establish baseline.
            s.bytes = bytes;
            s.when  = now;
            continue;
        }

        const qint64 dtMs   = now - s.when;
        const qint64 dBytes = bytes - s.bytes;

        if (dtMs >= 250) {
            const double instBps = double(dBytes) * 1000.0 / double(dtMs);

            // Exponential smoothing (α = 0.3) to avoid jitter.
            const double alpha = 0.3;
            if (it.speedBps <= 0.0)
                it.speedBps = instBps;
            else
                it.speedBps = alpha * instBps + (1.0 - alpha) * it.speedBps;

            s.bytes = bytes;
            s.when  = now;

            // ETA
            if (it.totalBytes > 0 && it.speedBps > 1.0) {
                const qint64 remaining = it.totalBytes - it.receivedBytes;
                if (remaining > 0)
                    it.etaSeconds = qint64(std::ceil(double(remaining) / it.speedBps));
                else
                    it.etaSeconds = 0;
            } else {
                it.etaSeconds = -1;
            }

            dirty = true;  // speed/eta changes are worth persisting for UI,
                           // but they're NOT serialized — so this is a no-op.
        }
    }
    Q_UNUSED(dirty);
}

// ────────────────────────────────────────────────────────────────────────────
//  Lookup helpers
// ────────────────────────────────────────────────────────────────────────────
int DownloadManager::indexOf(const QString& id) const {
    for (int i = 0; i < items_.size(); ++i)
        if (items_[i].id == id) return i;
    return -1;
}

void DownloadManager::setState(int idx, DownloadState s) {
    if (idx < 0 || idx >= items_.size()) return;
    items_[idx].state = int(s);
}

void DownloadManager::detachLive(int idx) {
    if (idx < 0 || idx >= items_.size()) return;
    live_.remove(items_[idx].id);
    samples_.remove(items_[idx].id);
}

// ────────────────────────────────────────────────────────────────────────────
//  Counts
// ────────────────────────────────────────────────────────────────────────────
int DownloadManager::activeCount() const {
    int n = 0;
    for (const auto& it : items_)
        if (it.state == int(DownloadState::InProgress)
         || it.state == int(DownloadState::Queued)) ++n;
    return n;
}

int DownloadManager::pausedCount() const {
    int n = 0;
    for (const auto& it : items_)
        if (it.state == int(DownloadState::Paused)) ++n;
    return n;
}

int DownloadManager::finishedCount() const {
    int n = 0;
    for (const auto& it : items_)
        if (it.isFinished()) ++n;
    return n;
}

// ────────────────────────────────────────────────────────────────────────────
//  Actions — pause / resume / cancel / remove / retry
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::pause(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    if (!items_[i].canPause()) return;

    if (auto lp = live_.value(id)) {
        lp->pause();
        items_[i].state     = int(DownloadState::Paused);
        items_[i].pausedAt  = QDateTime::currentSecsSinceEpoch();
        items_[i].speedBps  = 0.0;
        items_[i].etaSeconds = -1;
        samples_.remove(id);
        save();
    }
}

void DownloadManager::resume(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    if (!items_[i].canResume()) return;

    if (auto lp = live_.value(id)) {
        lp->resume();
        items_[i].state      = int(DownloadState::InProgress);
        items_[i].pausedAt   = 0;
        // Reset speed baseline — next tick will measure fresh.
        samples_.remove(id);
        save();
    }
}

void DownloadManager::cancel(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;

    if (auto lp = live_.value(id)) {
        lp->cancel();
    }
    items_[i].state      = int(DownloadState::Cancelled);
    items_[i].finishedAt = QDateTime::currentSecsSinceEpoch();
    items_[i].speedBps   = 0.0;
    items_[i].etaSeconds = -1;
    detachLive(i);
    save();
}

void DownloadManager::remove(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    if (!items_[i].canRemove()) return;   // don't remove in-flight

    live_.remove(id);
    samples_.remove(id);
    items_.removeAt(i);
    save();
}

void DownloadManager::retry(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    if (!items_[i].canRetry()) return;
    if (items_[i].sourceUrl.isEmpty()) return;

    // Remove the stale record and re-issue via the profile.
    // We do NOT have a live profile handle here, so we go through
    // QDesktopServices → the browser's profile will intercept the request
    // via downloadRequested and we start fresh.
    //
    // The old (interrupted) item is cleaned up so we don't accumulate
    // duplicates in the UI.
    const QString url = items_[i].sourceUrl;
    const QString oldId = items_[i].id;

    live_.remove(oldId);
    samples_.remove(oldId);
    items_.removeAt(i);
    save();

    QDesktopServices::openUrl(QUrl(url));
}

void DownloadManager::openFile(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    if (!items_[i].canOpen()) return;
    if (!QFile::exists(items_[i].fullPath)) {
        qWarning() << "[DownloadManager] file missing:" << items_[i].fullPath;
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(items_[i].fullPath));
}

void DownloadManager::openFolder(const QString& id) {
    const int i = indexOf(id);
    if (i < 0) return;
    const QString folder = QFileInfo(items_[i].fullPath).absolutePath();
    if (folder.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

// ────────────────────────────────────────────────────────────────────────────
//  Bulk operations
// ────────────────────────────────────────────────────────────────────────────
void DownloadManager::pauseAll() {
    for (const DownloadItem& it : items_)
        if (it.canPause()) pause(it.id);
}

void DownloadManager::resumeAll() {
    for (const DownloadItem& it : items_)
        if (it.canResume()) resume(it.id);
}

void DownloadManager::cancelAll() {
    for (const DownloadItem& it : items_)
        if (it.canCancel()) cancel(it.id);
}

void DownloadManager::clearFinished() {
    for (int i = items_.size() - 1; i >= 0; --i) {
        if (items_[i].isFinished()) {
            live_.remove(items_[i].id);
            samples_.remove(items_[i].id);
            items_.removeAt(i);
        }
    }
    save();
}

void DownloadManager::clearAll() {
    // Cancel anything still running first.
    for (const DownloadItem& it : items_)
        if (it.canCancel()) {
            if (auto lp = live_.value(it.id)) lp->cancel();
        }
    live_.clear();
    samples_.clear();
    items_.clear();
    save();
}

// ────────────────────────────────────────────────────────────────────────────
//  Diagnostics
// ────────────────────────────────────────────────────────────────────────────
QString DownloadManager::stateName(const QString& id) const {
    const int i = indexOf(id);
    if (i < 0) return QStringLiteral("Unknown");
    switch (DownloadState(items_[i].state)) {
        case DownloadState::Queued:      return QStringLiteral("Queued");
        case DownloadState::InProgress:  return QStringLiteral("In progress");
        case DownloadState::Paused:      return QStringLiteral("Paused");
        case DownloadState::Completed:   return QStringLiteral("Completed");
        case DownloadState::Cancelled:   return QStringLiteral("Cancelled");
        case DownloadState::Interrupted: return QStringLiteral("Interrupted");
        case DownloadState::Failed:      return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}
