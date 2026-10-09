// ============================================================================
//  downloadtoast.cpp — In-app sliding toast notification.
// ============================================================================

#include "downloadtoast.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>
#include <QFontMetrics>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>

// ============================================================================
//  Palette
// ============================================================================
namespace ToastCol {
    const QColor bg          = QColor(24, 25, 29, 250);
    const QColor bgHover     = QColor(28, 30, 35, 252);
    const QColor border      = QColor("#2e3138");
    const QColor textBright  = QColor("#f1f2f5");
    const QColor text        = QColor("#c8ccd6");
    const QColor textMuted   = QColor("#8a90a0");
    const QColor textFaint   = QColor("#5c606b");
    const QColor accent      = QColor("#5d9df1");
    const QColor success     = QColor("#4aaf7a");
    const QColor successSoft = QColor(74, 175, 122, 40);
    const QColor danger      = QColor("#e0443b");
    const QColor btnBg       = QColor(255, 255, 255, 12);
    const QColor btnBgHover  = QColor(255, 255, 255, 24);
    const QColor btnBorder   = QColor(255, 255, 255, 20);
}

// ============================================================================
//  Helper: human bytes
// ============================================================================
static QString humanBytes(qint64 n) {
    if (n < 0) return QStringLiteral("—");
    if (n == 0) return QStringLiteral("0 B");
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(n);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    return QString::number(v, 'f', (u == 0 ? 0 : 1)) + " " + units[u];
}

// ============================================================================
//  Custom button (icon or text)
// ============================================================================
class ToastButton : public QWidget {
public:
    explicit ToastButton(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent), text_(text)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(32);
        setMinimumWidth(80);
    }

    std::function<void()> onClick;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        p.setPen(QPen(ToastCol::btnBorder, 1));
        p.setBrush(hover_ ? ToastCol::btnBgHover : ToastCol::btnBg);
        p.drawRoundedRect(r, 6, 6);

        QFont f = font();
        f.setPixelSize(11.5);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.setPen(hover_ ? ToastCol::textBright : ToastCol::text);

        p.drawText(rect(), Qt::AlignCenter, text_);
    }

    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override       { hover_ = false; update(); }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && onClick) onClick();
    }

private:
    QString text_;
    bool hover_ = false;
};

// ============================================================================
//  Custom close button (× icon)
// ============================================================================
class ToastCloseButton : public QWidget {
public:
    explicit ToastCloseButton(QWidget* parent = nullptr) : QWidget(parent) {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFixedSize(24, 24);
    }

    std::function<void()> onClick;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (hover_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 20));
            p.drawRoundedRect(rect(), 6, 6);
        }

        const qreal s = width() * 0.35;
        const QPointF c = rect().center();

        p.setPen(QPen(hover_ ? QColor("#ffffff") : ToastCol::textFaint,
                      1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c.x() - s/2, c.y() - s/2),
                   QPointF(c.x() + s/2, c.y() + s/2));
        p.drawLine(QPointF(c.x() + s/2, c.y() - s/2),
                   QPointF(c.x() - s/2, c.y() + s/2));
    }

    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override       { hover_ = false; update(); }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && onClick) onClick();
    }

private:
    bool hover_ = false;
};

// ============================================================================
//  DownloadToast
// ============================================================================
DownloadToast::DownloadToast(const QString& id,
                             const QString& fileName,
                             qint64 bytes,
                             const QString& fullPath,
                             QWidget* parent)
    : QWidget(parent),
      id_(id),
      fileName_(fileName),
      bytes_(bytes),
      fullPath_(fullPath)
{
    setFixedWidth(kWidth);
    setFixedHeight(height_);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, false);
    setMouseTracking(true);

    buildUi();

    // Drop shadow for premium look
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);
}

DownloadToast::~DownloadToast() = default;

// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::buildUi() {
    // Main vertical layout
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(10);

    // ── Header row: checkmark + title + close button
    {
        auto* h = new QHBoxLayout;
        h->setSpacing(10);

        // Checkmark circle (custom-painted)
        auto* icon = new QWidget(this);
        icon->setFixedSize(28, 28);
        icon->installEventFilter(this);
        h->addWidget(icon);

        // Title + file name
        auto* meta = new QVBoxLayout;
        meta->setSpacing(2);

        titleLabel_ = new QLabel(QStringLiteral("Download complete"), this);
        QFont tf = titleLabel_->font();
        tf.setPixelSize(13);
        tf.setWeight(QFont::DemiBold);
        titleLabel_->setFont(tf);
        titleLabel_->setStyleSheet(
            QString("color:%1;background:transparent;").arg(ToastCol::textBright.name()));
        meta->addWidget(titleLabel_);

        fileLabel_ = new QLabel(fileName_, this);
        QFont ff = fileLabel_->font();
        ff.setPixelSize(12);
        fileLabel_->setFont(ff);
        fileLabel_->setStyleSheet(
            QString("color:%1;background:transparent;").arg(ToastCol::text.name()));
        fileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        meta->addWidget(fileLabel_);

        h->addLayout(meta, 1);

        // Close button
        auto* close = new ToastCloseButton(this);
        close->onClick = [this]{ dismissAnimated(); };
        h->addWidget(close, 0, Qt::AlignTop);

        v->addLayout(h);
    }

    // ── Meta line: size · time
    {
        metaLabel_ = new QLabel(
            QString("%1  ·  just now").arg(humanBytes(bytes_)), this);
        QFont mf = metaLabel_->font();
        mf.setPixelSize(11);
        metaLabel_->setFont(mf);
        metaLabel_->setStyleSheet(
            QString("color:%1;background:transparent;").arg(ToastCol::textMuted.name()));
        v->addWidget(metaLabel_);
    }

    v->addSpacing(4);

    // ── Action buttons row
    {
        auto* h = new QHBoxLayout;
        h->setSpacing(8);

        auto* openBtn = new ToastButton(QStringLiteral("Open"), this);
        openBtn->onClick = [this]{
            if (onOpen) onOpen(id_);
            dismissAnimated();
        };
        h->addWidget(openBtn);

        auto* folderBtn = new ToastButton(QStringLiteral("Show folder"), this);
        folderBtn->onClick = [this]{
            if (onShowFolder) onShowFolder(id_);
            dismissAnimated();
        };
        h->addWidget(folderBtn);

        h->addStretch(1);
        v->addLayout(h);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Custom icon painting (checkmark in green circle) — via event filter
// ────────────────────────────────────────────────────────────────────────────
bool DownloadToast::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::Paint) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w->size() == QSize(28, 28)) {
            QPainter p(w);
            p.setRenderHint(QPainter::Antialiasing);

            // Soft glow background
            QRadialGradient glow(QPointF(14, 14), 16);
            glow.setColorAt(0.0, ToastCol::successSoft);
            glow.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(QRectF(0, 0, 28, 28));

            // Green circle
            p.setBrush(ToastCol::success);
            p.drawEllipse(QRectF(3, 3, 22, 22));

            // Checkmark
            p.setPen(QPen(Qt::white, 2.4, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(9, 14), QPointF(12.5, 17.5));
            p.drawLine(QPointF(12.5, 17.5), QPointF(19, 10));
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ────────────────────────────────────────────────────────────────────────────
//  Paint background + progress bar
// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);

    // Card background
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, hovered_ ? ToastCol::bgHover : ToastCol::bg);
    g.setColorAt(1.0, QColor(18, 19, 22, 252));

    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(r, 14, 14);

    // Border
    p.setPen(QPen(ToastCol::border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 14, 14);

    // Top accent line (green for success)
    p.setPen(QPen(ToastCol::success, 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(r.left() + 16, r.top() + 1),
               QPointF(r.left() + 60, r.top() + 1));

    // Progress bar (auto-dismiss)
    if (progress_ > 0.0 && progress_ < 1.0 && !hovered_) {
        const qreal barW = r.width() * (1.0 - progress_);
        QRectF bar(r.left(), r.bottom() - 3, barW, 2.5);

        // Gradient
        QLinearGradient pg(bar.topLeft(), bar.topRight());
        pg.setColorAt(0.0, ToastCol::success);
        pg.setColorAt(1.0, QColor(74, 175, 122, 100));

        p.setPen(Qt::NoPen);
        p.setBrush(pg);
        p.drawRoundedRect(bar, 1.25, 1.25);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Hover — pauses auto-dismiss
// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::enterEvent(QEnterEvent*) {
    hovered_ = true;
    if (dismissTimer_) dismissTimer_->stop();
    update();
}

void DownloadToast::leaveEvent(QEvent*) {
    hovered_ = false;
    if (!dismissTimer_ || !dismissTimer_->isActive()) {
        startAutoDismiss();
    }
    update();
}

void DownloadToast::mousePressEvent(QMouseEvent* e) {
    // Click anywhere on toast → open file
    if (e->button() == Qt::LeftButton) {
        if (onOpen) onOpen(id_);
        dismissAnimated();
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Show with slide-in animation
// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::showAnimated() {
    show();
    raise();

    if (!slideAnim_) {
        slideAnim_ = new QVariantAnimation(this);
        slideAnim_->setDuration(320);
        slideAnim_->setEasingCurve(QEasingCurve::OutCubic);

        connect(slideAnim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v){
            move(int(v.toReal()), y());
        });
    }

    // Animate from off-screen right to target position
    slideAnim_->stop();
    slideAnim_->setStartValue(double(x() + 40));
    slideAnim_->setEndValue(double(hoverTargetX_));
    slideAnim_->start();

    startAutoDismiss();
}

// ────────────────────────────────────────────────────────────────────────────
//  Dismiss with slide-out
// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::dismissAnimated() {
    if (dismissTimer_) dismissTimer_->stop();
    if (progressTimer_) progressTimer_->stop();

    if (!slideAnim_) return;

    slideAnim_->stop();
    slideAnim_->setStartValue(double(x()));
    slideAnim_->setEndValue(double(x() + 60));

    // Disconnect old connections to avoid multiple triggers
    disconnect(slideAnim_, &QVariantAnimation::finished, this, nullptr);

    connect(slideAnim_, &QVariantAnimation::finished, this, [this]{
        hide();
        if (onDismiss) onDismiss(this);
    });

    slideAnim_->start();
}

// ────────────────────────────────────────────────────────────────────────────
//  Auto-dismiss after kDurationMs
// ────────────────────────────────────────────────────────────────────────────
void DownloadToast::startAutoDismiss() {
    if (!dismissTimer_) {
        dismissTimer_ = new QTimer(this);
        dismissTimer_->setSingleShot(true);
        connect(dismissTimer_, &QTimer::timeout, this, [this]{
            dismissAnimated();
        });
    }

    if (!progressTimer_) {
        progressTimer_ = new QTimer(this);
        progressTimer_->setInterval(50);
        connect(progressTimer_, &QTimer::timeout, this, [this]{
            progress_ += 50.0 / kDurationMs;
            if (progress_ >= 1.0) {
                progress_ = 1.0;
                progressTimer_->stop();
            }
            update();
        });
    }

    // Reset progress
    progress_ = 0.0;
    progressTimer_->start();
    dismissTimer_->start(kDurationMs);
}

void DownloadToast::stopAutoDismiss() {
    if (dismissTimer_) dismissTimer_->stop();
    if (progressTimer_) progressTimer_->stop();
}

// ============================================================================
//  DownloadToastManager
// ============================================================================
DownloadToastManager& DownloadToastManager::instance() {
    static DownloadToastManager mgr;
    return mgr;
}

DownloadToastManager::~DownloadToastManager() {
    clearAll();
}

void DownloadToastManager::show(const QString& id,
                                 const QString& fileName,
                                 qint64 bytes,
                                 const QString& fullPath)
{
    if (!parent_) return;

    auto* toast = new DownloadToast(id, fileName, bytes, fullPath, parent_);

    // Wire callbacks
    toast->onOpen       = onOpen;
    toast->onShowFolder = onShowFolder;
    toast->onDismiss    = [this](DownloadToast* t){ removeToast(t); };

    toasts_.push_back(toast);

    // Position stack
    repositionAll();

    // Animate in
    toast->showAnimated();
}

void DownloadToastManager::removeToast(DownloadToast* toast) {
    toasts_.removeAll(toast);
    toast->deleteLater();
    repositionAll();
}

void DownloadToastManager::clearAll() {
    for (auto* t : toasts_) {
        t->hide();
        t->deleteLater();
    }
    toasts_.clear();
}

// ────────────────────────────────────────────────────────────────────────────
//  Position stack in top-right corner of parent
// ────────────────────────────────────────────────────────────────────────────
void DownloadToastManager::reposition(QWidget* parent) {
    if (parent) parent_ = parent;
    if (!parent_) return;
    repositionAll();
}

void DownloadToastManager::repositionAll() {
    if (!parent_) return;

    const int margin = 16;
    const int spacing = 10;

    int y = 70;   // start below toolbar area

    // Top-right corner of parent window
    const int parentW = parent_->width();

    for (auto* t : toasts_) {
        const int x = parentW - t->width() - margin;
        t->move(x, y);

        // Store target X for slide animation to know where to land
        t->hoverTargetX_ = x;
        t->offscreenX_ = x + 60;

        y += t->height() + spacing;
    }
}
