// ============================================================================
//  downloadpanel.h — Clean minimal download panel.
// ============================================================================
#pragma once

#include <QWidget>
#include <QAbstractButton>
#include <QVector>
#include <QString>
#include <QRect>
#include <QPoint>
#include <QEvent>

class QTimer;
class QVariantAnimation;
class QMouseEvent;
class QResizeEvent;
class QPaintEvent;
class QKeyEvent;
class QFocusEvent;

// ----------------------------------------------------------------------------
//  DownloadButton — toolbar icon.
// ----------------------------------------------------------------------------
class DownloadButton : public QAbstractButton {
public:
    explicit DownloadButton(QWidget* parent = nullptr);
    void setActive(bool active);
    void setBadgeCount(int n);
    bool isActive() const { return active_; }

protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent*) override;

private:
    bool  hover_  = false;
    bool  active_ = false;
    int   badge_  = 0;
    qreal pulse_  = 0.0;
    QVariantAnimation* pulseAnim_ = nullptr;
};

// ----------------------------------------------------------------------------
//  DownloadPanel — clean list with simple actions.
// ----------------------------------------------------------------------------
class DownloadPanel : public QWidget {
public:
    explicit DownloadPanel(QWidget* parent = nullptr);

    void refresh();
    void popupUnder(QWidget* anchor);
    void tick();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void focusOutEvent(QFocusEvent* e) override;
    bool event(QEvent* e) override;

private:
    struct ActionBtn {
        QRect   rect;
        QString action;
        QString tooltip;
    };

    struct Row {
        QString id;
        int     y = 0;
        int     h = 0;
        QVector<ActionBtn> actions;
    };

    void rebuildRows();
    int  rowAt(int y) const;
    const ActionBtn* actionAt(const QPoint& p, const Row** outRow = nullptr) const;

    void paintRow(QPainter& p, const Row& r, const struct DownloadItem& it);
    void paintActionBtn(QPainter& p, const ActionBtn& btn, bool hovered);

    void performAction(const QString& action, const QString& id);
    void showRowMenu(const QPoint& globalPos, const QString& id);

    QVector<Row> rows_;
    int  hoverRow_    = -1;
    int  hoverAction_ = -1;

    QTimer* liveTimer_ = nullptr;
};
