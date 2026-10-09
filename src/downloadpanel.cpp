// ============================================================================
//  downloadpanel.cpp — Clean minimal download panel.
//  Monochrome icons only. No color, no emoji, no gradients.
// ============================================================================

#include "downloadpanel.h"
#include "downloadmanager.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QTimer>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QUrl>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <cmath>

// ============================================================================
//  Palette (all monochrome)
// ============================================================================
namespace Col {
    const QColor bg           = QColor("#141519");
    const QColor bgHover      = QColor("#1c1d21");
    const QColor border       = QColor("#26282f");
    const QColor divider      = QColor("#22242a");

    const QColor textPrimary  = QColor("#e6e8ec");
    const QColor text         = QColor("#a8adb8");
    const QColor textMuted    = QColor("#7a7f8c");
    const QColor textFaint    = QColor("#4d525d");

    const QColor icon         = QColor("#a8adb8");
    const QColor iconHover    = QColor("#e6e8ec");
    const QColor iconFaint    = QColor("#4d525d");

    const QColor barBg        = QColor("#22242a");
    const QColor barFill      = QColor("#5f6570");
    const QColor barFillDone  = QColor("#7a8290");
    const QColor barFillErr   = QColor("#5a4040");
    const QColor barFillPause = QColor("#4a4d55");
}

// ============================================================================
//  Formatting helpers
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

static QString humanSpeed(double bps) {
    if (bps <= 1.0) return QString();
    return humanBytes(qint64(bps)) + "/s";
}

static QString humanEta(qint64 sec) {
    if (sec < 0) return QString();
    if (sec < 60)   return QString("%1s").arg(sec);
    if (sec < 3600) return QString("%1m %2s").arg(sec / 60).arg(sec % 60);
    return QString("%1h %2m").arg(sec / 3600).arg((sec % 3600) / 60);
}

// ============================================================================
//  Minimal monochrome SVG-style icons
//  All drawn with simple strokes — clean and unique.
// ============================================================================
namespace Icon {

enum Kind { Pause, Play, Close, Open, Folder, Retry, Trash, Check, Download };

static void paint(QPainter& p, Kind k, const QRectF& r, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    const qreal s = qMin(r.width(), r.height());
    const QPointF ctr = r.center();

    p.setPen(QPen(c, qMax<qreal>(1.4, s * 0.09),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    switch (k) {

    // Pause: two vertical bars
    case Pause:
        p.drawLine(QPointF(ctr.x() - s * 0.14, ctr.y() - s * 0.22),
                   QPointF(ctr.x() - s * 0.14, ctr.y() + s * 0.22));
        p.drawLine(QPointF(ctr.x() + s * 0.14, ctr.y() - s * 0.22),
                   QPointF(ctr.x() + s * 0.14, ctr.y() + s * 0.22));
        break;

    // Play: right triangle
    case Play: {
        QPainterPath path;
        path.moveTo(ctr.x() - s * 0.14, ctr.y() - s * 0.22);
        path.lineTo(ctr.x() + s * 0.20, ctr.y());
        path.lineTo(ctr.x() - s * 0.14, ctr.y() + s * 0.22);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }

    // Close: X
    case Close:
        p.drawLine(QPointF(ctr.x() - s * 0.16, ctr.y() - s * 0.16),
                   QPointF(ctr.x() + s * 0.16, ctr.y() + s * 0.16));
        p.drawLine(QPointF(ctr.x() + s * 0.16, ctr.y() - s * 0.16),
                   QPointF(ctr.x() - s * 0.16, ctr.y() + s * 0.16));
        break;

    // Open: square with arrow pointing out
    case Open: {
        p.drawLine(QPointF(ctr.x() - s * 0.20, ctr.y() - s * 0.02),
                   QPointF(ctr.x() - s * 0.20, ctr.y() + s * 0.20));
        p.drawLine(QPointF(ctr.x() - s * 0.20, ctr.y() + s * 0.20),
                   QPointF(ctr.x() + s * 0.02, ctr.y() + s * 0.20));
        p.drawLine(QPointF(ctr.x() - s * 0.06, ctr.y() + s * 0.02),
                   QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.20));
        p.drawLine(QPointF(ctr.x() + s * 0.04, ctr.y() - s * 0.20),
                   QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.20));
        p.drawLine(QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.20),
                   QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.04));
        break;
    }

    // Folder: standard folder shape
    case Folder: {
        QPainterPath path;
        path.moveTo(ctr.x() - s * 0.22, ctr.y() + s * 0.16);
        path.lineTo(ctr.x() - s * 0.22, ctr.y() - s * 0.14);
        path.lineTo(ctr.x() - s * 0.06, ctr.y() - s * 0.14);
        path.lineTo(ctr.x() + s * 0.00, ctr.y() - s * 0.06);
        path.lineTo(ctr.x() + s * 0.22, ctr.y() - s * 0.06);
        path.lineTo(ctr.x() + s * 0.22, ctr.y() + s * 0.16);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }

    // Retry: circular arrow
    case Retry:
        p.drawArc(QRectF(ctr.x() - s * 0.20, ctr.y() - s * 0.20,
                         s * 0.40, s * 0.40), 70 * 16, 250 * 16);
        {
            QPointF a[3] = {
                QPointF(ctr.x() + s * 0.06, ctr.y() - s * 0.24),
                QPointF(ctr.x() + s * 0.22, ctr.y() - s * 0.14),
                QPointF(ctr.x() + s * 0.06, ctr.y() - s * 0.02),
            };
            p.drawPolyline(a, 3);
        }
        break;

    // Trash: bin with lid
    case Trash:
        p.drawLine(QPointF(ctr.x() - s * 0.18, ctr.y() - s * 0.14),
                   QPointF(ctr.x() + s * 0.18, ctr.y() - s * 0.14));
        p.drawLine(QPointF(ctr.x() - s * 0.07, ctr.y() - s * 0.14),
                   QPointF(ctr.x() - s * 0.07, ctr.y() - s * 0.22));
        p.drawLine(QPointF(ctr.x() + s * 0.07, ctr.y() - s * 0.14),
                   QPointF(ctr.x() + s * 0.07, ctr.y() - s * 0.22));
        p.drawLine(QPointF(ctr.x() - s * 0.07, ctr.y() - s * 0.22),
                   QPointF(ctr.x() + s * 0.07, ctr.y() - s * 0.22));
        p.drawLine(QPointF(ctr.x() - s * 0.14, ctr.y() - s * 0.10),
                   QPointF(ctr.x() - s * 0.12, ctr.y() + s * 0.22));
        p.drawLine(QPointF(ctr.x() + s * 0.14, ctr.y() - s * 0.10),
                   QPointF(ctr.x() + s * 0.12, ctr.y() + s * 0.22));
        p.drawLine(QPointF(ctr.x() - s * 0.12, ctr.y() + s * 0.22),
                   QPointF(ctr.x() + s * 0.12, ctr.y() + s * 0.22));
        break;

    // Check: tick mark
    case Check:
        p.drawLine(QPointF(ctr.x() - s * 0.18, ctr.y() + s * 0.02),
                   QPointF(ctr.x() - s * 0.04, ctr.y() + s * 0.16));
        p.drawLine(QPointF(ctr.x() - s * 0.04, ctr.y() + s * 0.16),
                   QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.16));
        break;

    // Download: arrow down + tray
    case Download:
        p.drawLine(QPointF(ctr.x(), ctr.y() - s * 0.22),
                   QPointF(ctr.x(), ctr.y() + s * 0.08));
        p.drawLine(QPointF(ctr.x() - s * 0.12, ctr.y() - s * 0.04),
                   QPointF(ctr.x(), ctr.y() + s * 0.08));
        p.drawLine(QPointF(ctr.x() + s * 0.12, ctr.y() - s * 0.04),
                   QPointF(ctr.x(), ctr.y() + s * 0.08));
        p.drawLine(QPointF(ctr.x() - s * 0.16, ctr.y() + s * 0.20),
                   QPointF(ctr.x() + s * 0.16, ctr.y() + s * 0.20));
        break;
    }

    p.restore();
}

} // namespace Icon

// ============================================================================
//  DownloadButton (toolbar icon)
// ============================================================================
DownloadButton::DownloadButton(QWidget* parent)
    : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(36, 36);

    pulseAnim_ = new QVariantAnimation(this);
    pulseAnim_->setDuration(1400);
    pulseAnim_->setStartValue(0.0);
    pulseAnim_->setEndValue(1.0);
    pulseAnim_->setLoopCount(-1);
    QObject::connect(pulseAnim_, &QVariantAnimation::valueChanged,
                     this, [this](const QVariant& v){
        pulse_ = v.toReal();
        update();
    });
}

void DownloadButton::setActive(bool active) {
    if (active_ == active) return;
    active_ = active;
    if (active_) pulseAnim_->start();
    else { pulseAnim_->stop(); pulse_ = 0.0; }
    update();
}

void DownloadButton::setBadgeCount(int n) {
    if (badge_ == n) return;
    badge_ = qMax(0, n);
    update();
}

bool DownloadButton::event(QEvent* e) {
    if (e->type() == QEvent::HoverEnter) { hover_ = true; update(); }
    else if (e->type() == QEvent::HoverLeave) { hover_ = false; update(); }
    return QAbstractButton::event(e);
}

void DownloadButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (hover_ || active_) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, hover_ ? 20 : 10));
        p.drawRoundedRect(rect(), 10, 10);
    }

    const QRectF r = rect();
    const QPointF c = r.center();
    const qreal s = qMin(r.width(), r.height());

    if (active_) {
        const qreal ringR = s * (0.32 + 0.16 * pulse_);
        const int alpha = int(120 * (1.0 - pulse_));
        p.setPen(QPen(QColor(200, 205, 215, alpha), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, ringR, ringR);
    }

    const QColor col = active_ ? QColor("#ffffff")
                               : (hover_ ? QColor("#ffffff") : QColor("#b4b8c2"));
    Icon::paint(p, Icon::Download,
                QRectF(c.x() - s * 0.28, c.y() - s * 0.28,
                       s * 0.56, s * 0.56), col);

    // Badge (monochrome)
    if (badge_ > 0) {
        const qreal br = 7.0;
        const QPointF bc(r.right() - br - 2, r.top() + br + 2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e6e8ec"));
        p.drawEllipse(bc, br, br);

        QFont f = font();
        f.setPixelSize(9);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor("#141519"));
        const QString t = badge_ > 9 ? QStringLiteral("9+") : QString::number(badge_);
        p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2),
                   Qt::AlignCenter, t);
    }
}

// ============================================================================
//  DownloadPanel
// ============================================================================
static constexpr int kHeaderH  = 52;
static constexpr int kRowH     = 76;
static constexpr int kPadX     = 18;
static constexpr int kPadY     = 12;
static constexpr int kIconSize = 24;
static constexpr int kActBtn   = 30;
static constexpr int kActGap   = 4;
static constexpr int kRadius   = 12;

DownloadPanel::DownloadPanel(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(420);

    liveTimer_ = new QTimer(this);
    liveTimer_->setInterval(500);
    QObject::connect(liveTimer_, &QTimer::timeout, this, [this]{ tick(); });
}

// ────────────────────────────────────────────────────────────────────────────
void DownloadPanel::refresh() {
    rebuildRows();
    update();
}

void DownloadPanel::tick() {
    const auto& items = DownloadManager::instance().items();

    bool anyActive = false;
    for (const auto& it : items) {
        if (it.state == int(DownloadState::InProgress)
         || it.state == int(DownloadState::Queued)
         || it.state == int(DownloadState::Paused)) {
            anyActive = true;
            break;
        }
    }

    if (!anyActive) liveTimer_->stop();
    else if (!liveTimer_->isActive()) liveTimer_->start();

    if (rows_.size() != items.size())
        rebuildRows();

    update();
}

void DownloadPanel::rebuildRows() {
    rows_.clear();

    QVector<DownloadItem> sorted = DownloadManager::instance().items();
    std::sort(sorted.begin(), sorted.end(),
              [](const DownloadItem& a, const DownloadItem& b) {
        auto rank = [](int s) {
            switch (DownloadState(s)) {
                case DownloadState::InProgress: return 0;
                case DownloadState::Queued:     return 1;
                case DownloadState::Paused:     return 2;
                default:                         return 3;
            }
        };
        const int ra = rank(a.state), rb = rank(b.state);
        if (ra != rb) return ra < rb;
        return b.startedAt > a.startedAt;
    });

    int y = kHeaderH;
    for (const auto& it : sorted) {
        Row r;
        r.id = it.id;
        r.y  = y;
        r.h  = kRowH;

        struct ActDef { QString name; QString tip; };
        QVector<ActDef> defs;

        switch (DownloadState(it.state)) {
            case DownloadState::InProgress:
                defs.push_back({"pause",  "Pause"});
                defs.push_back({"cancel", "Cancel"});
                break;
            case DownloadState::Queued:
                defs.push_back({"cancel", "Cancel"});
                break;
            case DownloadState::Paused:
                defs.push_back({"resume", "Resume"});
                defs.push_back({"cancel", "Cancel"});
                break;
            case DownloadState::Completed:
                defs.push_back({"open",   "Open file"});
                defs.push_back({"folder", "Show in folder"});
                break;
            case DownloadState::Cancelled:
            case DownloadState::Interrupted:
            case DownloadState::Failed:
                defs.push_back({"retry",  "Retry"});
                defs.push_back({"remove", "Remove"});
                break;
        }

        int bx = width() - kPadX - kActBtn;
        for (int i = defs.size() - 1; i >= 0; --i) {
            ActionBtn b;
            b.action  = defs[i].name;
            b.tooltip = defs[i].tip;
            b.rect    = QRect(bx, y + (kRowH - kActBtn) / 2,
                              kActBtn, kActBtn);
            r.actions.push_front(b);
            bx -= kActBtn + kActGap;
        }

        rows_.push_back(r);
        y += kRowH;
    }

    const int totalH = qMin(y + 10, 620);
    setFixedHeight(qMax(totalH, kHeaderH + 80));
}

int DownloadPanel::rowAt(int y) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (y >= rows_[i].y && y < rows_[i].y + rows_[i].h) return i;
    return -1;
}

const DownloadPanel::ActionBtn* DownloadPanel::actionAt(const QPoint& p,
                                                       const Row** outRow) const
{
    for (int i = 0; i < rows_.size(); ++i) {
        const Row& r = rows_[i];
        for (const auto& b : r.actions) {
            if (b.rect.contains(p)) {
                if (outRow) *outRow = &r;
                return &b;
            }
        }
    }
    if (outRow) *outRow = nullptr;
    return nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
void DownloadPanel::popupUnder(QWidget* anchor) {
    rebuildRows();
    update();

    int x = 0, y = 0;
    if (anchor) {
        const QPoint bottomLeft =
            anchor->mapToGlobal(QPoint(0, anchor->height() + 8));
        x = bottomLeft.x() + anchor->width() - width();
        y = bottomLeft.y();

        QScreen* screen = QGuiApplication::screenAt(bottomLeft);
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen) {
            const QRect sr = screen->availableGeometry();
            if (x + width() > sr.right() - 10)
                x = sr.right() - width() - 10;
            if (x < sr.left() + 10)
                x = sr.left() + 10;
            if (y + height() > sr.bottom() - 10)
                y = anchor->mapToGlobal(QPoint(0, -height() - 8)).y();
            if (y < sr.top() + 10)
                y = sr.top() + 10;
        }
    } else if (auto* screen = QGuiApplication::primaryScreen()) {
        const QRect sr = screen->availableGeometry();
        x = sr.right() - width() - 12;
        y = 60;
    }

    move(x, y);
    if (windowHandle())
        windowHandle()->setPosition(x, y);

    show();
    raise();
    activateWindow();
    setFocus();
    tick();
}

// ============================================================================
//  Paint — Action button
// ============================================================================
void DownloadPanel::paintActionBtn(QPainter& p, const ActionBtn& btn,
                                   bool hovered)
{
    // Background on hover
    if (hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 16));
        p.drawRoundedRect(btn.rect, 7, 7);
    }

    // Choose icon kind
    Icon::Kind kind = Icon::Close;
    if      (btn.action == "pause")  kind = Icon::Pause;
    else if (btn.action == "resume") kind = Icon::Play;
    else if (btn.action == "cancel") kind = Icon::Close;
    else if (btn.action == "open")   kind = Icon::Open;
    else if (btn.action == "folder") kind = Icon::Folder;
    else if (btn.action == "retry")  kind = Icon::Retry;
    else if (btn.action == "remove") kind = Icon::Trash;

    // Icon color
    const QColor ic = hovered ? Col::iconHover : Col::icon;

    // Draw icon centered
    const qreal inner = kIconSize * 0.72;
    const QRectF ir(btn.rect.center().x() - inner / 2,
                    btn.rect.center().y() - inner / 2,
                    inner, inner);
    Icon::paint(p, kind, ir, ic);
}

// ============================================================================
//  Paint — Row
// ============================================================================
void DownloadPanel::paintRow(QPainter& p, const Row& r, const DownloadItem& it) {
    const bool hovered = (&r == (hoverRow_ >= 0 && hoverRow_ < rows_.size()
                                  ? &rows_[hoverRow_] : nullptr));

    // Row hover background
    if (hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(Col::bgHover);
        p.drawRoundedRect(QRectF(8, r.y + 2, width() - 16, r.h - 4), 8, 8);
    }

    // ── Left status icon
    const bool done = (it.state == int(DownloadState::Completed));
    const bool paused = (it.state == int(DownloadState::Paused));
    const bool errored = (it.state == int(DownloadState::Cancelled)
                       || it.state == int(DownloadState::Interrupted)
                       || it.state == int(DownloadState::Failed));

    const QPointF iconCenter(kPadX + kIconSize / 2.0,
                             r.y + r.h / 2.0);

    Icon::Kind statusIcon = Icon::Download;
    QColor statusCol = Col::icon;

    if (done) {
        statusIcon = Icon::Check;
    } else if (paused) {
        statusIcon = Icon::Pause;
    } else if (errored) {
        statusIcon = Icon::Close;
    }

    Icon::paint(p, statusIcon,
                QRectF(iconCenter.x() - kIconSize / 2.0,
                       iconCenter.y() - kIconSize / 2.0,
                       kIconSize, kIconSize),
                statusCol);

    // ── Text region
    const qreal textX = kPadX + kIconSize + 14;
    const qreal textW = width() - textX - kPadX - (2 * kActBtn + 2 * kActGap + 8);

    // Filename
    QFont nf = font();
    nf.setPixelSize(13);
    nf.setWeight(QFont::Medium);
    p.setFont(nf);
    p.setPen(Col::textPrimary);
    const QRectF nameR(textX, r.y + 14, textW, 18);
    p.drawText(nameR, Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(nf).elidedText(it.fileName, Qt::ElideMiddle,
                                           int(nameR.width())));

    // ── Status line
    QString status;
    switch (DownloadState(it.state)) {
        case DownloadState::Queued:
            status = QStringLiteral("Waiting…");
            break;
        case DownloadState::InProgress: {
            const QString spd = humanSpeed(it.speedBps);
            const QString eta = humanEta(it.etaSeconds);
            if (it.totalBytes > 0) {
                status = QString("%1 / %2")
                            .arg(humanBytes(it.receivedBytes),
                                 humanBytes(it.totalBytes));
            } else {
                status = humanBytes(it.receivedBytes);
            }
            if (!spd.isEmpty()) status += " · " + spd;
            if (!eta.isEmpty()) status += " · " + eta;
            break;
        }
        case DownloadState::Paused:
            status = QStringLiteral("Paused · %1 / %2")
                        .arg(humanBytes(it.receivedBytes),
                             humanBytes(it.totalBytes));
            break;
        case DownloadState::Completed:
            status = QStringLiteral("Completed · ") + humanBytes(it.receivedBytes);
            break;
        case DownloadState::Cancelled:
            status = QStringLiteral("Cancelled");
            break;
        case DownloadState::Interrupted:
            status = QStringLiteral("Interrupted");
            break;
        case DownloadState::Failed:
            status = QStringLiteral("Failed");
            break;
    }

    QFont sf = font();
    sf.setPixelSize(11);
    p.setFont(sf);
    p.setPen(Col::textMuted);
    const QRectF statusR(textX, r.y + 34, textW, 14);
    p.drawText(statusR, Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(sf).elidedText(status, Qt::ElideRight,
                                           int(statusR.width())));

    // ── Progress bar (thin line)
    const qreal barY = r.y + kRowH - 12;
    const QRectF barBg(textX, barY, textW, 2);

    p.setPen(Qt::NoPen);
    p.setBrush(Col::barBg);
    p.drawRect(barBg);

    const double pct = done ? 100.0 : it.progress();
    if (pct > 0.0) {
        const QRectF barFg(barBg.x(), barBg.y(),
                           barBg.width() * (pct / 100.0), barBg.height());
        p.setBrush(done ? Col::barFillDone : Col::barFill);
        p.drawRect(barFg);
    }

    // ── Action buttons (only on hover)
    if (hovered) {
        for (int bi = 0; bi < r.actions.size(); ++bi) {
            paintActionBtn(p, r.actions[bi], bi == hoverAction_);
        }
    }
}

// ============================================================================
//  Paint — Main
// ============================================================================
void DownloadPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ── Panel card
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);

    p.setPen(Qt::NoPen);
    p.setBrush(Col::bg);
    p.drawPath(path);

    p.setPen(QPen(Col::border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // ── Header
    QFont hf = font();
    hf.setPixelSize(13);
    hf.setWeight(QFont::DemiBold);
    p.setFont(hf);
    p.setPen(Col::textPrimary);
    p.drawText(QRectF(kPadX, 0, width() - 2 * kPadX, kHeaderH),
               Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Downloads"));

    // Count on right
    const auto& items = DownloadManager::instance().items();
    QFont cf = font();
    cf.setPixelSize(11);
    cf.setWeight(QFont::Normal);
    p.setFont(cf);
    p.setPen(Col::textMuted);
    p.drawText(QRectF(kPadX, 0, width() - 2 * kPadX, kHeaderH),
               Qt::AlignVCenter | Qt::AlignRight,
               QString::number(items.size()));

    // Divider
    p.setPen(QPen(Col::divider, 1));
    p.drawLine(QPointF(0, kHeaderH), QPointF(width(), kHeaderH));

    // ── Empty state
    if (rows_.isEmpty()) {
        const QRectF emptyRect(0, kHeaderH, width(), height() - kHeaderH);

        // Icon
        const qreal is = 40;
        const QRectF iconRect(emptyRect.center().x() - is / 2,
                              emptyRect.center().y() - is / 2 - 12,
                              is, is);
        Icon::paint(p, Icon::Download, iconRect, Col::iconFaint);

        // Text
        QFont ef = font();
        ef.setPixelSize(12);
        p.setFont(ef);
        p.setPen(Col::textMuted);
        p.drawText(QRectF(0, iconRect.bottom() + 12, width(), 18),
                   Qt::AlignCenter, QStringLiteral("No downloads"));
        return;
    }

    // ── Rows
    for (const Row& r : rows_) {
        const DownloadItem* it = nullptr;
        for (const auto& x : items) if (x.id == r.id) { it = &x; break; }
        if (!it) continue;
        paintRow(p, r, *it);
    }
}

// ============================================================================
//  Interaction
// ============================================================================
void DownloadPanel::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    const Row* row = nullptr;
    const ActionBtn* btn = actionAt(pos, &row);

    int newHoverRow = -1;
    int newHoverAction = -1;

    if (row && btn) {
        for (int i = 0; i < rows_.size(); ++i) {
            if (&rows_[i] == row) {
                newHoverRow = i;
                for (int bi = 0; bi < row->actions.size(); ++bi) {
                    if (&row->actions[bi] == btn) {
                        newHoverAction = bi;
                        break;
                    }
                }
                break;
            }
        }
    } else {
        newHoverRow = rowAt(pos.y());
    }

    if (newHoverRow != hoverRow_ || newHoverAction != hoverAction_) {
        hoverRow_ = newHoverRow;
        hoverAction_ = newHoverAction;

        setCursor(newHoverAction >= 0 ? Qt::PointingHandCursor
                                      : Qt::ArrowCursor);
        update();
    }
}

void DownloadPanel::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;

    const QPoint pos = e->position().toPoint();
    const Row* row = nullptr;
    const ActionBtn* btn = actionAt(pos, &row);

    if (btn && row) {
        performAction(btn->action, row->id);
        return;
    }

    const int i = rowAt(pos.y());
    if (i >= 0) {
        const Row& r = rows_[i];
        const auto& items = DownloadManager::instance().items();
        for (const auto& it : items) {
            if (it.id != r.id) continue;

            if (it.state == int(DownloadState::Completed)) {
                DownloadManager::instance().openFile(it.id);
                hide();
            } else if (it.state == int(DownloadState::InProgress)) {
                DownloadManager::instance().pause(it.id);
            } else if (it.state == int(DownloadState::Paused)) {
                DownloadManager::instance().resume(it.id);
            } else if (it.state == int(DownloadState::Interrupted)
                    || it.state == int(DownloadState::Cancelled)
                    || it.state == int(DownloadState::Failed)) {
                DownloadManager::instance().retry(it.id);
            }
            break;
        }
        update();
    }
}

void DownloadPanel::contextMenuEvent(QContextMenuEvent* e) {
    const int i = rowAt(e->pos().y());
    if (i < 0) return;
    showRowMenu(e->globalPos(), rows_[i].id);
}

void DownloadPanel::leaveEvent(QEvent*) {
    hoverRow_ = -1;
    hoverAction_ = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void DownloadPanel::resizeEvent(QResizeEvent*) {
    rebuildRows();
}

void DownloadPanel::focusOutEvent(QFocusEvent*) {
    if (isVisible()) {
        QTimer::singleShot(80, this, [this]{
            if (!underMouse())
                hide();
        });
    }
}

bool DownloadPanel::event(QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QWidget::event(e);
}

// ============================================================================
//  Actions
// ============================================================================
void DownloadPanel::performAction(const QString& action, const QString& id) {
    auto& dm = DownloadManager::instance();

    if      (action == "pause")  dm.pause(id);
    else if (action == "resume") dm.resume(id);
    else if (action == "cancel") dm.cancel(id);
    else if (action == "remove") dm.remove(id);
    else if (action == "retry")  dm.retry(id);
    else if (action == "open")   dm.openFile(id);
    else if (action == "folder") dm.openFolder(id);

    if (action == "remove" || action == "cancel" || action == "retry")
        rebuildRows();

    update();
}

void DownloadPanel::showRowMenu(const QPoint& globalPos, const QString& id) {
    const auto& items = DownloadManager::instance().items();
    const DownloadItem* it = nullptr;
    for (const auto& x : items) if (x.id == id) { it = &x; break; }
    if (!it) return;

    QMenu menu(this);

    switch (DownloadState(it->state)) {
        case DownloadState::InProgress:
            menu.addAction("Pause",  [this, id]{ performAction("pause",  id); });
            menu.addAction("Cancel", [this, id]{ performAction("cancel", id); });
            break;
        case DownloadState::Paused:
            menu.addAction("Resume", [this, id]{ performAction("resume", id); });
            menu.addAction("Cancel", [this, id]{ performAction("cancel", id); });
            break;
        case DownloadState::Queued:
            menu.addAction("Cancel", [this, id]{ performAction("cancel", id); });
            break;
        case DownloadState::Completed:
            menu.addAction("Open file",       [this, id]{ performAction("open",   id); });
            menu.addAction("Show in folder",  [this, id]{ performAction("folder", id); });
            menu.addSeparator();
            menu.addAction("Remove from list",[this, id]{ performAction("remove", id); });
            break;
        default:
            menu.addAction("Retry",            [this, id]{ performAction("retry",  id); });
            menu.addAction("Remove from list", [this, id]{ performAction("remove", id); });
            break;
    }

    menu.addSeparator();
    menu.addAction("Copy source URL", [it]{
        QApplication::clipboard()->setText(it->sourceUrl);
    });

    menu.exec(globalPos);
}
