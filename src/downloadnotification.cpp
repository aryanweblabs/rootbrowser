// ============================================================================
//  downloadnotification.cpp — Download completion notifications.
//
//  Uses QSystemTrayIcon::showMessage() for native OS notifications on Linux
//  (via DBus), Windows, and macOS. Falls back to no-op if not available.
// ============================================================================

#include "downloadnotification.h"

#include <QSystemTrayIcon>
#include <QApplication>
#include <QWidget>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

// ============================================================================
//  Singleton
// ============================================================================
DownloadNotification& DownloadNotification::instance() {
    static DownloadNotification s;
    return s;
}

DownloadNotification::DownloadNotification() = default;

// ============================================================================
//  Init — creates a hidden system tray icon to emit notifications
// ============================================================================
void DownloadNotification::init(QWidget* parent) {
    parent_ = parent;

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug("[DownloadNotification] System tray not available — "
               "notifications disabled");
        enabled_ = false;
        return;
    }

    if (tray_) return;

    // Create a small 64×64 icon programmatically — green checkmark
    QPixmap iconPix(64, 64);
    iconPix.fill(Qt::transparent);
    {
        QPainter p(&iconPix);
        p.setRenderHint(QPainter::Antialiasing);

        // Circle background
        QLinearGradient g(0, 0, 64, 64);
        g.setColorAt(0.0, QColor("#4aaf7a"));
        g.setColorAt(1.0, QColor("#3a8e60"));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(QRectF(4, 4, 56, 56));

        // Checkmark
        p.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(20, 32), QPointF(28, 42));
        p.drawLine(QPointF(28, 42), QPointF(46, 22));
    }

    tray_ = new QSystemTrayIcon(QIcon(iconPix), parent_);
    tray_->setToolTip(QStringLiteral("RootBrowser"));
    tray_->show();

    // Click handler — clicking notification triggers "Open" action
    QObject::connect(tray_, &QSystemTrayIcon::messageClicked, this, [this]{
        // Route to most recent pending download
        if (lastId_.isEmpty()) return;
        if (onOpen) onOpen(lastId_);
        lastId_.clear();
    });

    qDebug("[DownloadNotification] Initialized (system tray available)");
}

bool DownloadNotification::supportsNativeNotifications() const {
    return QSystemTrayIcon::isSystemTrayAvailable() &&
           QSystemTrayIcon::supportsMessages();
}

// ============================================================================
//  Helpers
// ============================================================================
QString DownloadNotification::humanBytes(qint64 bytes) {
    if (bytes < 0) return QStringLiteral("—");
    if (bytes == 0) return QStringLiteral("0 B");
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    return QString::number(v, 'f', (u == 0 ? 0 : 1)) + " " + units[u];
}

// ============================================================================
//  Notify: download complete
// ============================================================================
void DownloadNotification::notifyComplete(const QString& id,
                                          const QString& fileName,
                                          qint64 bytes,
                                          const QString& fullPath)
{
    Q_UNUSED(fullPath);

    if (!enabled_) return;

    // Store for click routing
    lastId_ = id;

    const QString title = QStringLiteral("Download complete");
    const QString body = QString("%1  ·  %2")
                            .arg(fileName, humanBytes(bytes));

    if (tray_ && QSystemTrayIcon::supportsMessages()) {
        tray_->showMessage(
            title,
            body,
            QSystemTrayIcon::Information,
            8000);   // 8 seconds
    } else {
        qDebug() << "[DownloadNotification]" << title << ":" << body;
    }
}

// ============================================================================
//  Notify: download failed
// ============================================================================
void DownloadNotification::notifyError(const QString& fileName,
                                       const QString& reason)
{
    if (!enabled_) return;

    const QString title = QStringLiteral("Download failed");
    const QString body = QString("%1  ·  %2").arg(fileName, reason);

    if (tray_ && QSystemTrayIcon::supportsMessages()) {
        tray_->showMessage(
            title,
            body,
            QSystemTrayIcon::Warning,
            6000);
    } else {
        qDebug() << "[DownloadNotification]" << title << ":" << body;
    }
}

// ============================================================================
//  Notify: batch (multiple downloads finished together)
// ============================================================================
void DownloadNotification::notifyBatch(int count) {
    if (!enabled_ || count <= 0) return;

    const QString title = QStringLiteral("Downloads complete");
    const QString body = (count == 1)
        ? QStringLiteral("1 file downloaded")
        : QString("%1 files downloaded").arg(count);

    if (tray_ && QSystemTrayIcon::supportsMessages()) {
        tray_->showMessage(
            title,
            body,
            QSystemTrayIcon::Information,
            6000);
    }
}
