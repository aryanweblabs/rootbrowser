// ============================================================================
//  historystore.cpp — Persistent history storage.
// ============================================================================

#include "historystore.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <algorithm>

// ────────────────────────────────────────────────────────────────────────────
HistoryStore& HistoryStore::instance() {
    static HistoryStore s;
    return s;
}

HistoryStore::HistoryStore() {
    load();
}

QString HistoryStore::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/history.json";
}

// ────────────────────────────────────────────────────────────────────────────
void HistoryStore::load() {
    items_.clear();

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return;

    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value("items").toArray();

    items_.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.url        = o.value("url").toString();
        e.title      = o.value("title").toString();
        e.visitedAt  = qint64(o.value("visitedAt").toDouble(0));
        e.visitCount = o.value("visitCount").toInt(1);
        if (e.url.isEmpty()) continue;
        items_.push_back(e);
    }

    // Sort newest first
    std::sort(items_.begin(), items_.end(),
              [](const HistoryEntry& a, const HistoryEntry& b){
        return a.visitedAt > b.visitedAt;
    });
}

void HistoryStore::save() const {
    QJsonArray arr;

    for (const HistoryEntry& e : items_) {
        QJsonObject o;
        o["url"]        = e.url;
        o["title"]      = e.title;
        o["visitedAt"]  = double(e.visitedAt);
        o["visitCount"] = e.visitCount;
        arr.append(o);
    }

    QJsonObject root;
    root["version"] = 1;
    root["items"]   = arr;

    // Atomic write
    const QString path = filePath();
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
void HistoryStore::record(const QString& url, const QString& title) {
    if (url.isEmpty()) return;

    // Skip internal pages
    if (url.startsWith("rootbrowser://") ||
        url.startsWith("rootbrowser:") ||
        url.startsWith("about:") ||
        url.startsWith("data:"))
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QString cleanTitle = title.isEmpty() ? url : title;

    // Dedupe within 24h: bump the count instead of creating a new entry.
    const qint64 k24h = 24 * 60 * 60;
    for (HistoryEntry& e : items_) {
        if (e.url == url && (now - e.visitedAt) < k24h) {
            e.visitedAt  = now;
            e.title      = cleanTitle;
            e.visitCount += 1;
            // Move to front (it's newest now)
            std::stable_sort(items_.begin(), items_.end(),
                             [](const HistoryEntry& a, const HistoryEntry& b){
                return a.visitedAt > b.visitedAt;
            });
            save();
            return;
        }
    }

    // Fresh entry
    HistoryEntry e;
    e.url        = url;
    e.title      = cleanTitle;
    e.visitedAt  = now;
    e.visitCount = 1;

    items_.prepend(e);
    evictIfNeeded();
    save();
}

void HistoryStore::evictIfNeeded() {
    if (items_.size() <= kMaxEntries) return;

    // Remove oldest beyond the cap
    items_.erase(items_.begin() + kMaxEntries, items_.end());
}

// ────────────────────────────────────────────────────────────────────────────
void HistoryStore::remove(const QString& url) {
    const int before = items_.size();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&url](const HistoryEntry& e){
        return e.url == url;
    }), items_.end());

    if (items_.size() != before) save();
}

void HistoryStore::clearOlderThan(int days) {
    if (days <= 0) {
        clearAll();
        return;
    }

    const qint64 cutoff = QDateTime::currentSecsSinceEpoch()
                        - qint64(days) * 24 * 60 * 60;

    const int before = items_.size();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [cutoff](const HistoryEntry& e){
        return e.visitedAt < cutoff;
    }), items_.end());

    if (items_.size() != before) save();
}

void HistoryStore::clearAll() {
    items_.clear();
    save();
}

// ────────────────────────────────────────────────────────────────────────────
QVector<HistoryEntry> HistoryStore::recent(int limit) const {
    QVector<HistoryEntry> out;
    const int n = qMin(limit, items_.size());
    out.reserve(n);
    for (int i = 0; i < n; ++i) out.push_back(items_[i]);
    return out;
}

QVector<HistoryEntry> HistoryStore::search(const QString& query,
                                           qint64 sinceEpochSec) const
{
    QVector<HistoryEntry> out;
    const QString q = query.trimmed().toLower();

    for (const HistoryEntry& e : items_) {
        if (sinceEpochSec > 0 && e.visitedAt < sinceEpochSec) continue;
        if (q.isEmpty() ||
            e.url.toLower().contains(q) ||
            e.title.toLower().contains(q))
        {
            out.push_back(e);
        }
    }
    return out;
}

// ────────────────────────────────────────────────────────────────────────────
int HistoryStore::uniqueSites() const {
    QHash<QString, bool> seen;
    for (const HistoryEntry& e : items_) {
        QUrl u(e.url);
        const QString host = u.host().toLower();
        if (!host.isEmpty()) seen.insert(host, true);
    }
    return seen.size();
}

qint64 HistoryStore::oldestTimestamp() const {
    if (items_.isEmpty()) return 0;
    qint64 oldest = items_.first().visitedAt;
    for (const HistoryEntry& e : items_)
        if (e.visitedAt < oldest) oldest = e.visitedAt;
    return oldest;
}

// ────────────────────────────────────────────────────────────────────────────
QVector<HistoryStore::SiteStat> HistoryStore::topSites(int limit) const {
    QHash<QString, SiteStat> agg;

    for (const HistoryEntry& e : items_) {
        QUrl u(e.url);
        QString host = u.host().toLower();
        if (host.startsWith("www.")) host = host.mid(4);
        if (host.isEmpty()) continue;

        auto& s = agg[host];
        if (s.host.isEmpty()) {
            s.host = host;
            s.url = e.url;
        }
        s.visits += e.visitCount;
        if (e.visitedAt > s.lastVisit) {
            s.lastVisit = e.visitedAt;
            s.title = e.title;
            s.url = e.url;
        }
    }

    QVector<SiteStat> out;
    out.reserve(agg.size());
    for (const auto& s : agg) out.push_back(s);

    std::sort(out.begin(), out.end(),
              [](const SiteStat& a, const SiteStat& b){
        return a.visits > b.visits;
    });

    if (out.size() > limit) out.resize(limit);
    return out;
}

qint64 HistoryStore::fileSize() const {
    QFileInfo fi(filePath());
    return fi.exists() ? fi.size() : 0;
}
