// ============================================================================
//  bookmarksbar.h — Horizontal bookmarks bar under the toolbar.
// ============================================================================
#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

class QPushButton;
class QMenu;

// ============================================================================
//  BookmarkBarItem — one bookmark (or folder) displayed as a button
// ============================================================================
struct BarItem {
    QString url;
    QString title;
    QString faviconPath;   // optional
    bool    isFolder = false;
    QVector<BarItem> children;   // for folders
};

// ============================================================================
//  BookmarksBar — the bar itself
// ============================================================================
class BookmarksBar : public QWidget {
public:
    explicit BookmarksBar(QWidget* parent = nullptr);

    // Reload items from BookmarkStore + update layout
    void reload();

    // Called by main.cpp when bookmarks change
    void refresh();

    // Show/hide with animation
    void toggleVisible();

    // Home page use ke liye — click handler
    std::function<void(const QString& url)> onOpenUrl;
    std::function<void(const QString& url)> onOpenInNewTab;

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    // Build items from BookmarkStore
    void loadFromStore();

    // Layout
    void rebuildLayout();
    int  itemAt(int x) const;
    QRect itemRect(int index) const;

    // Paint
    void paintItem(QPainter& p, const BarItem& item,
                   const QRect& rect, bool hovered);

    // Actions
    void openItem(int index);
    void openItemInNewTab(int index);
    void editBookmark(int index);
    void deleteBookmark(int index);
    void showFolderMenu(int index, const QPoint& globalPos);

    // Data
    QVector<BarItem> items_;
    int hoverIndex_ = -1;
    int scrollOffset_ = 0;

    // Layout constants
    static constexpr int kBarHeight   = 32;
    static constexpr int kItemHeight  = 26;
    static constexpr int kPadding     = 6;
    static constexpr int kItemSpacing = 2;
    static constexpr int kIconSize    = 16;
    static constexpr int kMaxItemW    = 180;
    static constexpr int kMinItemW    = 60;
};
