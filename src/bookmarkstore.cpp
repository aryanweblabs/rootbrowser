// ============================================================================
//  bookmarkstore.cpp — moc-free implementation.
// ============================================================================

#include "bookmarkstore.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

BookmarkStore& BookmarkStore::instance() {
    static BookmarkStore s;
    return s;
}

BookmarkStore::BookmarkStore() {
    load();
}

QString BookmarkStore::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/bookmarks.json";
}

void BookmarkStore::load() {
    items_.clear();

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) return;

    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Bookmark b;
        b.url         = o.value("url").toString();
        b.title       = o.value("title").toString();
        b.faviconPath = o.value("faviconPath").toString();
        b.addedAt     = qint64(o.value("addedAt").toDouble(0));
        if (!b.url.isEmpty()) items_.push_back(b);
    }
}

void BookmarkStore::save() const {
    QJsonArray arr;
    for (const Bookmark& b : items_) {
        QJsonObject o;
        o["url"]         = b.url;
        o["title"]       = b.title;
        o["faviconPath"] = b.faviconPath;
        o["addedAt"]     = double(b.addedAt);
        arr.append(o);
    }

    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    f.close();
}

void BookmarkStore::addOrUpdate(const QString& url, const QString& title) {
    if (url.isEmpty()) return;

    for (Bookmark& b : items_) {
        if (b.url == url) {
            b.title = title.isEmpty() ? b.url : title;
            save();
            return;
        }
    }

    Bookmark b;
    b.url     = url;
    b.title   = title.isEmpty() ? url : title;
    b.addedAt = QDateTime::currentSecsSinceEpoch();
    items_.push_back(b);
    save();
}

bool BookmarkStore::remove(const QString& url) {
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].url == url) {
            items_.removeAt(i);
            save();
            return true;
        }
    }
    return false;
}

bool BookmarkStore::contains(const QString& url) const {
    for (const Bookmark& b : items_)
        if (b.url == url) return true;
    return false;
}
