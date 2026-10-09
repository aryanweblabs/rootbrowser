// ============================================================================
//  bookmarkstore.h — Persistent bookmark storage (moc-free).
//  Saves to ~/.local/share/RootBrowser/bookmarks.json
// ============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QDateTime>

struct Bookmark {
    QString url;
    QString title;
    QString faviconPath;   // reserved (unused currently)
    qint64 addedAt = 0;    // unix epoch seconds
};

class BookmarkStore {
public:
    static BookmarkStore& instance();

    // Add or update a bookmark. If a bookmark for the same URL exists,
    // its title is updated; otherwise a new one is appended.
    void addOrUpdate(const QString& url, const QString& title);

    // Remove a bookmark by URL. Returns true if removed.
    bool remove(const QString& url);

    // Check if a URL is bookmarked.
    bool contains(const QString& url) const;

    // All bookmarks (unsorted).
    QVector<Bookmark> all() const { return items_; }

    // Number of bookmarks.
    int count() const { return items_.size(); }

private:
    BookmarkStore();
    void load();
    void save() const;

    static QString filePath();

    QVector<Bookmark> items_;
};
