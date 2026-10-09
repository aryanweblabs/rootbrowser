// ============================================================================
//  downloadmanager.h — Central download manager (moc-free, Qt 6.2+).
//  · Robust pause/resume/cancel state machine
//  · Live byte/speed/ETA tracking
//  · Persists everything to disk (survives app restart)
// ============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QHash>
#include <QPointer>

class QTimer;
class QWebEngineProfile;
class QWebEngineDownloadRequest;

// ────────────────────────────────────────────────────────────────────────────
//  Download states. The values are stable across app restarts (persisted as
//  ints in JSON), so NEVER renumber existing entries — only append new ones.
// ────────────────────────────────────────────────────────────────────────────
enum class DownloadState : int {
    Queued      = 0,   // accepted but not yet started
    InProgress  = 1,
    Paused      = 2,
    Completed   = 3,
    Cancelled   = 4,
    Interrupted = 5,   // network error / server dropped
    Failed      = 6    // unrecoverable
};

struct DownloadItem {
    QString id;
    QString fileName;
    QString fullPath;
    QString sourceUrl;
    qint64  totalBytes    = 0;
    qint64  receivedBytes = 0;
    qint64  startedAt     = 0;   // unix epoch seconds
    qint64  finishedAt    = 0;
    qint64  pausedAt      = 0;
    int     state         = int(DownloadState::Queued);
    QString mimeType;

    // Live-derived (not persisted, but useful for UI)
    double  speedBps      = 0.0; // bytes per second (smoothed)
    qint64  etaSeconds    = -1;  // -1 = unknown

    bool isActive() const {
        return state == int(DownloadState::InProgress)
            || state == int(DownloadState::Queued);
    }
    bool isPaused() const { return state == int(DownloadState::Paused); }
    bool isFinished() const {
        return state == int(DownloadState::Completed)
            || state == int(DownloadState::Cancelled)
            || state == int(DownloadState::Interrupted)
            || state == int(DownloadState::Failed);
    }
    bool canPause() const { return state == int(DownloadState::InProgress); }
    bool canResume() const { return state == int(DownloadState::Paused); }
    bool canCancel() const {
        return state == int(DownloadState::InProgress)
            || state == int(DownloadState::Paused)
            || state == int(DownloadState::Queued);
    }
    bool canRemove() const { return isFinished(); }
    bool canOpen() const { return state == int(DownloadState::Completed); }
    bool canRetry() const {
        return state == int(DownloadState::Cancelled)
            || state == int(DownloadState::Interrupted)
            || state == int(DownloadState::Failed);
    }

    double progress() const {
        if (totalBytes <= 0) return 0.0;
        return qBound(0.0, double(receivedBytes) / double(totalBytes) * 100.0, 100.0);
    }
};

class DownloadManager {
public:
    static DownloadManager& instance();

    // ── Config ─────────────────────────────────────────────────────────
    QString defaultFolder() const { return defaultFolder_; }
    void    setDefaultFolder(const QString& path);
    void    attach(QWebEngineProfile* profile);

    // ── Query ──────────────────────────────────────────────────────────
    QVector<DownloadItem> items() const { return items_; }
    int  count() const { return items_.size(); }
    int  activeCount() const;      // InProgress + Queued
    int  pausedCount() const;
    int  finishedCount() const;

    // ── Actions ────────────────────────────────────────────────────────
    void pause (const QString& id);
    void resume(const QString& id);
    void cancel(const QString& id);
    void remove(const QString& id);      // only finished
    void retry (const QString& id);      // re-download from sourceUrl
    void openFile (const QString& id);
    void openFolder(const QString& id);

    // Bulk operations
    void pauseAll();
    void resumeAll();
    void cancelAll();
    void clearFinished();
    void clearAll();

    // ── Diagnostics ────────────────────────────────────────────────────
    QString stateName(const QString& id) const;

private:
    DownloadManager();
    ~DownloadManager();

    // Non-copyable
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    void load();
    void save() const;
    static QString settingsPath();

    void handleNewRequest(QWebEngineDownloadRequest* req);
    void tick();                    // 1 Hz: update speed/ETA
    void pollRequests();            // 500 ms: detect completion/cancel

    int     indexOf(const QString& id) const;
    QString makeId() const;

    // Internal state helpers
    void setState(int idx, DownloadState s);
    void detachLive(int idx);

    // ── Data ───────────────────────────────────────────────────────────
    QString defaultFolder_;
    QVector<DownloadItem> items_;

    // Live pointers to in-flight requests
    QHash<QString, QPointer<QWebEngineDownloadRequest>> live_;

    // Speed tracking per id (last sample)
    struct SpeedSample { qint64 bytes = 0; qint64 when = 0; };
    QHash<QString, SpeedSample> samples_;

    QTimer* pollTimer_ = nullptr;
    QTimer* speedTimer_ = nullptr;
};
