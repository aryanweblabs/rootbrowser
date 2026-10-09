// ============================================================================
//  downloadtoast.h — In-app sliding toast for download notifications.
// ============================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <functional>

class QTimer;
class QLabel;
class QProgressBar;
class QVariantAnimation;

// ============================================================================
//  DownloadToast — single toast notification
// ============================================================================
class DownloadToast : public QWidget {
    friend class DownloadToastManager;
public:
    explicit DownloadToast(const QString& id,
                           const QString& fileName,
                           qint64 bytes,
                           const QString& fullPath,
                           QWidget* parent = nullptr);
    ~DownloadToast() override;

    // Actions (called on button click)
    std::function<void(const QString& id)> onOpen;
    std::function<void(const QString& id)> onShowFolder;
    std::function<void(DownloadToast*)> onDismiss;

    // Show with slide-in animation
    void showAnimated();

    // Dismiss with slide-out animation
    void dismissAnimated();

    QString id() const { return id_; }
    int height() const { return height_; }

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    void buildUi();
    void startAutoDismiss();
    void stopAutoDismiss();

    QString id_;
    QString fileName_;
    qint64  bytes_ = 0;
    QString fullPath_;

    QLabel* titleLabel_ = nullptr;
    QLabel* fileLabel_ = nullptr;
    QLabel* metaLabel_ = nullptr;
    QWidget* btnOpen_ = nullptr;
    QWidget* btnFolder_ = nullptr;
    QWidget* btnClose_ = nullptr;

    QVariantAnimation* slideAnim_ = nullptr;
    QTimer* dismissTimer_ = nullptr;
    QTimer* progressTimer_ = nullptr;

    int height_ = 130;
    int hoverTargetX_ = 0;
    int offscreenX_ = 0;
    qreal progress_ = 0.0;   // 0..1 progress bar for auto-dismiss
    bool  hovered_ = false;

    static constexpr int kWidth = 360;
    static constexpr int kMargin = 16;
    static constexpr int kDurationMs = 8000;   // auto-dismiss after 8s
};

// ============================================================================
//  DownloadToastManager — coordinates stacking multiple toasts
// ============================================================================
class DownloadToastManager {
public:
    static DownloadToastManager& instance();

    // Show a toast for a completed download
    void show(const QString& id,
              const QString& fileName,
              qint64 bytes,
              const QString& fullPath);

    // Callbacks
    std::function<void(const QString& id)> onOpen;
    std::function<void(const QString& id)> onShowFolder;

    // Called on each window resize to reposition
    void reposition(QWidget* parent);

    // Clear all toasts (e.g. on browser close)
    void clearAll();

private:
    DownloadToastManager() = default;
    ~DownloadToastManager();

    void removeToast(DownloadToast* toast);
    void repositionAll();

    QVector<DownloadToast*> toasts_;
    QWidget* parent_ = nullptr;
};
