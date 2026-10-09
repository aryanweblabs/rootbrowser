// ============================================================================
//  bookmarksbar.cpp — Horizontal bookmarks bar.
// ============================================================================

#include "bookmarksbar.h"
#include "bookmarkstore.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFontMetrics>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QTimer>
#include <algorithm>

// ============================================================================
//  Palette
// ============================================================================
namespace BarCol {
    const QColor bg          = QColor("#141519");
    const QColor bgHover     = QColor(255, 255, 255, 18);
    const QColor bgPressed   = QColor(255, 255, 255, 28);
    const QColor border      = QColor("#1e2026");
    const QColor text        = QColor("#c8ccd6");
    const QColor textHover   = QColor("#ffffff");
    const QColor textFaint   = QColor("#5c606b");
    const QColor icon        = QColor("#8a90a0");
    const QColor iconHover   = QColor("#ffffff");
    const QColor folder      = QColor("#8a90a0");
    const QColor emptyText   = QColor("#4d525d");
}

// ============================================================================
//  Default bookmark set (shown when store is empty)
// ============================================================================
static QVector<BarItem> defaultItems() {
    QVector<BarItem> items;
    // Leave empty — bar hides if no bookmarks
    return items;
}

// ============================================================================
//  BookmarksBar
// ============================================================================
BookmarksBar::BookmarksBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kBarHeight);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setStyleSheet("background:transparent;");

    loadFromStore();
    rebuildLayout();
}

// ────────────────────────────────────────────────────────────────────────────
//  Load items from BookmarkStore
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::loadFromStore() {
    items_.clear();

    auto& store = BookmarkStore::instance();
    const auto all = store.all();

    for (const Bookmark& bm : all) {
        BarItem item;
        item.url         = bm.url;
        item.title       = bm.title.isEmpty() ? bm.url : bm.title;
        item.faviconPath = bm.faviconPath;
        item.isFolder    = false;
        items_.push_back(item);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Public reload / refresh
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::reload() {
    loadFromStore();
    rebuildLayout();
    update();
}

void BookmarksBar::refresh() {
    reload();
}

void BookmarksBar::toggleVisible() {
    setVisible(!isVisible());
}

// ────────────────────────────────────────────────────────────────────────────
//  Layout computation
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::rebuildLayout() {
    // If no bookmarks, hide the bar
    if (items_.isEmpty()) {
        setFixedHeight(0);
        return;
    }
    setFixedHeight(kBarHeight);
}

QRect BookmarksBar::itemRect(int index) const {
    if (index < 0 || index >= items_.size()) return QRect();

    QFontMetrics fm(font());
    int x = kPadding - scrollOffset_;

    for (int i = 0; i < index; ++i) {
        const auto& it = items_[i];
        int textW = fm.horizontalAdvance(it.title);
        int w = kIconSize + 6 + textW + 16;   // icon + gap + text + padding
        w = std::clamp(w, kMinItemW, kMaxItemW);
        x += w + kItemSpacing;
    }

    const auto& it = items_[index];
    int textW = fm.horizontalAdvance(it.title);
    int w = kIconSize + 6 + textW + 16;
    w = std::clamp(w, kMinItemW, kMaxItemW);

    return QRect(x, (height() - kItemHeight) / 2, w, kItemHeight);
}

int BookmarksBar::itemAt(int x) const {
    for (int i = 0; i < items_.size(); ++i) {
        if (itemRect(i).contains(x, height() / 2))
            return i;
    }
    return -1;
}

// ────────────────────────────────────────────────────────────────────────────
//  Paint
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), BarCol::bg);

    // Bottom border
    p.setPen(QPen(BarCol::border, 1));
    p.drawLine(0, height() - 1, width(), height() - 1);

    // Empty state
    if (items_.isEmpty()) {
        QFont f = font();
        f.setPixelSize(11);
        f.setItalic(true);
        p.setFont(f);
        p.setPen(BarCol::emptyText);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No bookmarks yet — press Ctrl+D to add one"));
        return;
    }

    // Items
    for (int i = 0; i < items_.size(); ++i) {
        const QRect r = itemRect(i);
        if (r.right() < 0 || r.left() > width()) continue;   // off-screen
        paintItem(p, items_[i], r, i == hoverIndex_);
    }
}

void BookmarksBar::paintItem(QPainter& p, const BarItem& item,
                             const QRect& rect, bool hovered)
{
    // Hover background
    if (hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(BarCol::bgHover);
        p.drawRoundedRect(rect, 6, 6);
    }

    // Icon
    const QRect iconRect(rect.left() + 8,
                         rect.center().y() - kIconSize / 2,
                         kIconSize, kIconSize);

    if (item.isFolder) {
        // Folder icon
        p.setPen(QPen(hovered ? BarCol::iconHover : BarCol::icon, 1.5,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        QPainterPath folder;
        const qreal s = kIconSize;
        const QPointF c = iconRect.center();
        folder.moveTo(c.x() - s * 0.4, c.y() + s * 0.3);
        folder.lineTo(c.x() - s * 0.4, c.y() - s * 0.15);
        folder.lineTo(c.x() - s * 0.1, c.y() - s * 0.15);
        folder.lineTo(c.x() + s * 0.0, c.y() - s * 0.05);
        folder.lineTo(c.x() + s * 0.4, c.y() - s * 0.05);
        folder.lineTo(c.x() + s * 0.4, c.y() + s * 0.3);
        folder.closeSubpath();
        p.drawPath(folder);
    } else {
        // Draw a small globe icon (fallback) — favicon would be nice but skip for now
        p.setPen(QPen(hovered ? BarCol::iconHover : BarCol::icon, 1.3,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(iconRect.adjusted(2, 2, -2, -2));
        p.drawLine(QPointF(iconRect.left() + 4, iconRect.center().y()),
                   QPointF(iconRect.right() - 4, iconRect.center().y()));
        p.drawEllipse(iconRect.adjusted(5, 2, -5, -2));
    }

    // Text
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);

    const QRect textRect(iconRect.right() + 6,
                         rect.top(),
                         rect.right() - iconRect.right() - 6 - 8,
                         rect.height());

    QFontMetrics fm(f);
    const QString elided = fm.elidedText(item.title, Qt::ElideRight,
                                          textRect.width());

    p.setPen(hovered ? BarCol::textHover : BarCol::text);
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Tooltip for full title on hover
    if (hovered && fm.horizontalAdvance(item.title) > textRect.width()) {
        QToolTip::showText(
            mapToGlobal(QPoint(rect.center().x(), rect.bottom())),
            item.title, this);
    } else {
        QToolTip::hideText();
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Mouse handling
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::mouseMoveEvent(QMouseEvent* e) {
    const int idx = itemAt(int(e->position().x()));
    if (idx != hoverIndex_) {
        hoverIndex_ = idx;
        setCursor(idx >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void BookmarksBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int idx = itemAt(int(e->position().x()));
    if (idx < 0) return;
    // Visual pressed feedback
    update();
}

void BookmarksBar::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;

    const int idx = itemAt(int(e->position().x()));
    if (idx < 0 || idx != hoverIndex_) return;

    if (items_[idx].isFolder) {
        showFolderMenu(idx, e->globalPosition().toPoint());
        return;
    }

    openItem(idx);
}

void BookmarksBar::leaveEvent(QEvent*) {
    if (hoverIndex_ != -1) {
        hoverIndex_ = -1;
        unsetCursor();
        update();
    }
}

void BookmarksBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    update();
}

void BookmarksBar::wheelEvent(QWheelEvent* e) {
    // Horizontal scroll
    const int delta = e->angleDelta().y() / 2;
    scrollOffset_ = std::max(0, scrollOffset_ - delta);
    update();
    e->accept();
}

void BookmarksBar::contextMenuEvent(QContextMenuEvent* e) {
    const int idx = itemAt(e->pos().x());
    if (idx < 0) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu{background:#1c1d21;color:#d7d9de;border:1px solid #2c2e34;"
        "padding:6px;border-radius:10px;}"
        "QMenu::item{padding:7px 24px 7px 28px;color:#d7d9de;border-radius:6px;}"
        "QMenu::item:selected{background:#2b2d34;color:#ffffff;}"
        "QMenu::separator{height:1px;background:#2c2e34;margin:6px 8px;}");

    const auto& item = items_[idx];

    QAction* openAction = menu.addAction(
        item.isFolder ? QStringLiteral("Open all in tabs")
                      : QStringLiteral("Open"));
    QAction* newTabAction = menu.addAction(QStringLiteral("Open in new tab"));
    QAction* newWinAction = menu.addAction(QStringLiteral("Open in new window"));
    menu.addSeparator();
    QAction* copyUrlAction = menu.addAction(QStringLiteral("Copy URL"));
    QAction* editAction = menu.addAction(QStringLiteral("Edit…"));
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(QStringLiteral("Delete"));

    QAction* chosen = menu.exec(e->globalPos());
    if (!chosen) return;

    if (chosen == openAction) {
        openItem(idx);
    } else if (chosen == newTabAction) {
        openItemInNewTab(idx);
    } else if (chosen == newWinAction) {
        // Open in new window — we don't have that concept, open in new tab
        openItemInNewTab(idx);
    } else if (chosen == copyUrlAction) {
        QApplication::clipboard()->setText(items_[idx].url);
    } else if (chosen == editAction) {
        editBookmark(idx);
    } else if (chosen == deleteAction) {
        deleteBookmark(idx);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Actions
// ────────────────────────────────────────────────────────────────────────────
void BookmarksBar::openItem(int index) {
    if (index < 0 || index >= items_.size()) return;
    const auto& item = items_[index];

    if (item.isFolder) {
        // Open all children in new tabs
        for (const auto& child : item.children) {
            if (onOpenInNewTab) onOpenInNewTab(child.url);
        }
        return;
    }

    if (onOpenUrl) onOpenUrl(item.url);
}

void BookmarksBar::openItemInNewTab(int index) {
    if (index < 0 || index >= items_.size()) return;
    const auto& item = items_[index];

    if (item.isFolder) {
        for (const auto& child : item.children) {
            if (onOpenInNewTab) onOpenInNewTab(child.url);
        }
        return;
    }

    if (onOpenInNewTab) onOpenInNewTab(item.url);
}

void BookmarksBar::editBookmark(int index) {
    if (index < 0 || index >= items_.size()) return;
    const auto& item = items_[index];

    bool ok = false;
    const QString newTitle = QInputDialog::getText(
        this,
        QStringLiteral("Edit Bookmark"),
        QStringLiteral("Title:"),
        QLineEdit::Normal,
        item.title,
        &ok);

    if (!ok || newTitle.isEmpty()) return;

    // Update via store
    auto& store = BookmarkStore::instance();
    store.addOrUpdate(item.url, newTitle);   // updates title

    reload();
    if (onOpenUrl) { /* trigger refresh elsewhere */ }
}

void BookmarksBar::deleteBookmark(int index) {
    if (index < 0 || index >= items_.size()) return;
    const auto& item = items_[index];

    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Delete Bookmark"),
        QStringLiteral("Delete \"%1\"?").arg(item.title),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    auto& store = BookmarkStore::instance();
    store.remove(item.url);
    reload();
}

void BookmarksBar::showFolderMenu(int index, const QPoint& globalPos) {
    if (index < 0 || index >= items_.size()) return;
    const auto& folder = items_[index];

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu{background:#1c1d21;color:#d7d9de;border:1px solid #2c2e34;"
        "padding:6px;border-radius:10px;}"
        "QMenu::item{padding:7px 24px 7px 28px;color:#d7d9de;border-radius:6px;}"
        "QMenu::item:selected{background:#2b2d34;color:#ffffff;}");

    for (const auto& child : folder.children) {
        QAction* a = menu.addAction(child.title);
        const QString url = child.url;
        QObject::connect(a, &QAction::triggered, this, [this, url]{
            if (onOpenUrl) onOpenUrl(url);
        });
    }

    menu.exec(globalPos);
}
