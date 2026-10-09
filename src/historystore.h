// ============================================================================
//  historystore.h — Persistent browsing history (moc-free).
//  Saves to ~/.local/share/RootBrowser/history.json
//  Caps at 5000 entries (oldest evicted automatically).
// ============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QHash>

// ────────────────────────────────────────────────────────────────────────────
//  HistoryEntry — a single visit.
// ────────────────────────────────────────────────────────────────────────────
struct HistoryEntry {
    QString url;
    QString title;
    qint64  visitedAt = 0;      // unix epoch seconds
    int     visitCount = 1;     // total visits to this URL
};

// ────────────────────────────────────────────────────────────────────────────
//  HistoryStore — singleton, thread-safe on the GUI thread.
// ────────────────────────────────────────────────────────────────────────────
class HistoryStore {
public:
    static HistoryStore& instance();

    // Record a visit. If the URL already exists within the last 24h,
    // it bumps the visit count and updates the timestamp instead of
    // creating a new entry. Older duplicates get a new entry.
    void record(const QString& url, const QString& title);

    // Remove a single URL (all entries with that URL).
    void remove(const QString& url);

    // Remove all entries older than `days` days. days <= 0 → clear all.
    void clearOlderThan(int days);

    // Clear everything.
    void clearAll();

    // Queries
    QVector<HistoryEntry> all() const { return items_; }
    QVector<HistoryEntry> recent(int limit = 100) const;
    QVector<HistoryEntry> search(const QString& query,
                                 qint64 sinceEpochSec = 0) const;

    int   count() const { return items_.size(); }
    int   uniqueSites() const;
    qint64 oldestTimestamp() const;

    // For "most visited" aggregation
    struct SiteStat {
        QString host;
        QString title;
        QString url;
        int     visits = 0;
        qint64  lastVisit = 0;
    };
    QVector<SiteStat> topSites(int limit = 50) const;

    // Total size on disk
    qint64 fileSize() const;

    static QString filePath();
    static constexpr int kMaxEntries = 5000;

private:
    HistoryStore();
    ~HistoryStore() = default;
    HistoryStore(const HistoryStore&) = delete;
    HistoryStore& operator=(const HistoryStore&) = delete;

    void load();
    void save() const;
    void evictIfNeeded();

    QVector<HistoryEntry> items_;    // sorted newest-first
};
