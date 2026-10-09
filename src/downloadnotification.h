// ============================================================================
//  downloadnotification.h — Native + custom download notifications.
// ============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class QSystemTrayIcon;
class QWidget;

// ============================================================================
//  DownloadNotification — singleton notifier
// ============================================================================
class DownloadNotification : public QObject {
public:
    static DownloadNotification& instance();

    // Initialize with a system tray icon. Call this once from main.cpp.
    void init(QWidget* parent = nullptr);

    // Called by DownloadManager when a download completes.
    // `id` used for click action routing.
    void notifyComplete(const QString& id,
                        const QString& fileName,
                        qint64 bytes,
                        const QString& fullPath);

    // Called when a download fails or is cancelled.
    void notifyError(const QString& fileName, const QString& reason);

    // Bulk: notify when multiple downloads finish.
    void notifyBatch(int count);

    // Set callbacks for user actions (called from main.cpp)
    std::function<void(const QString& id)> onOpen;
    std::function<void(const QString& id)> onShowFolder;

    // Enable / disable
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    // Whether notifications are supported natively
    bool supportsNativeNotifications() const;

private:
    DownloadNotification();
    ~DownloadNotification() = default;
    DownloadNotification(const DownloadNotification&) = delete;
    DownloadNotification& operator=(const DownloadNotification&) = delete;

    static QString humanBytes(qint64 bytes);

    QSystemTrayIcon* tray_ = nullptr;
    QWidget*         parent_ = nullptr;
    QString          lastId_;
    bool enabled_ = true;
};
