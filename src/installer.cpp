// ============================================================================
//  RootBrowser Installer — Final
//
//  · Always fetches fresh icon + source from GitHub
//  · Cleans up old icons before installing new ones
//  · Handcrafted globe+play logo on main page
//  · Static Linux (Tux) and Windows 11 logos on OS choice
//  · No emoji, vector icons everywhere
//  · Bounded, muted, professional design
//
//  Build:
//    g++ -std=c++17 -O2 -fPIC installer.cpp -o rootbrowser-installer \
//        $(pkg-config --cflags --libs Qt6Widgets)
//  Run:
//    sudo ./rootbrowser-installer
// ============================================================================

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QTimer>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QVariantAnimation>
#include <QElapsedTimer>
#include <QDateTime>
#include <QLinearGradient>
#include <QRadialGradient>
#include <functional>
#include <cmath>

static const char* kAppName      = "RootBrowser Installer";
static const char* kGitHubUser   = "IndianInstituteOfHacking";
static const char* kRepoName     = "a2";
static const char* kGitHubUrl    = "https://github.com/IndianInstituteOfHacking/a2.git";
static const char* kInstallDir   = "/opt/rootbrowser";
static const char* kBinName      = "rootbrowser";
static const char* kDisplayName  = "RootBrowser";
static const char* kVersion      = "1.0.0";

#ifdef Q_OS_WIN
    static constexpr bool kIsWindows = true;
#else
    static constexpr bool kIsWindows = false;
#endif

// ============================================================================
//  Palette — muted, professional
// ============================================================================
namespace Col {
    const QColor bg          = QColor("#0b0c10");
    const QColor bgSubtle    = QColor("#0e1014");
    const QColor surface     = QColor("#13151b");
    const QColor surfaceAlt  = QColor("#171a21");
    const QColor surfaceHi   = QColor("#1d2028");
    const QColor border      = QColor("#242832");
    const QColor borderSoft  = QColor("#1c1f27");
    const QColor textPrimary = QColor("#e6e8ee");
    const QColor textBright  = QColor("#f7f8fb");
    const QColor textMuted   = QColor("#8a90a0");
    const QColor textFaint   = QColor("#565b69");
    const QColor accent      = QColor("#5a8cd8");
    const QColor accentHi    = QColor("#74a3e8");
    const QColor accentLo    = QColor("#3d6bb8");
    const QColor success     = QColor("#4a9a6e");
    const QColor danger      = QColor("#b85858");
    const QColor warning     = QColor("#b8904a");
    const QColor logoGreen   = QColor("#3cc47a");
    const QColor logoTeal    = QColor("#2ba9a0");
    const QColor logoCyan    = QColor("#4db8d8");
    const QColor logoBg      = QColor("#0a1420");
    const QColor logoGrid    = QColor("#1e2a38");
}

// ============================================================================
//  IconKind — vector icons
// ============================================================================
enum class IconKind {
    Package, Git, Qt, Globe, Cpu, Image, Font, Download,
    Hammer, Folder, Rocket, Target, Refresh, Broom,
    Search, Shield, Check
};

static void paintIcon(QPainter& p, IconKind k, const QRectF& r, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = r.width();
    auto P = [&](qreal x, qreal y) { return QPointF(r.x() + x * s, r.y() + y * s); };
    QPen pen(c, std::max<qreal>(1.4, s * 0.10), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (k == IconKind::Package) {
        p.drawLine(P(.2,.4), P(.5,.22));
        p.drawLine(P(.5,.22), P(.8,.4));
        p.drawLine(P(.8,.4), P(.8,.75));
        p.drawLine(P(.8,.75), P(.5,.9));
        p.drawLine(P(.5,.9), P(.2,.75));
        p.drawLine(P(.2,.75), P(.2,.4));
        p.drawLine(P(.2,.4), P(.5,.58));
        p.drawLine(P(.5,.58), P(.8,.4));
        p.drawLine(P(.5,.58), P(.5,.9));
    } else if (k == IconKind::Git) {
        p.drawEllipse(P(.28,.25), .09*s, .09*s);
        p.drawEllipse(P(.28,.75), .09*s, .09*s);
        p.drawEllipse(P(.72,.5),  .09*s, .09*s);
        p.drawLine(P(.28,.34), P(.28,.66));
        p.drawLine(P(.28,.58), P(.63,.5));
    } else if (k == IconKind::Qt) {
        p.drawRoundedRect(QRectF(r.x()+.2*s, r.y()+.2*s, .6*s, .6*s), 3, 3);
        p.setBrush(c);
        p.drawEllipse(P(.5,.5), .08*s, .08*s);
    } else if (k == IconKind::Globe) {
        p.drawEllipse(P(.5,.5), .35*s, .35*s);
        p.drawLine(P(.15,.5), P(.85,.5));
        p.drawEllipse(P(.5,.5), .15*s, .35*s);
    } else if (k == IconKind::Cpu) {
        p.drawRect(QRectF(r.x()+.28*s, r.y()+.28*s, .44*s, .44*s));
        for (int i = 0; i < 4; ++i) {
            qreal t = .32 + i * .12;
            p.drawLine(P(t,.18), P(t,.28));
            p.drawLine(P(t,.72), P(t,.82));
            p.drawLine(P(.18,t), P(.28,t));
            p.drawLine(P(.72,t), P(.82,t));
        }
    } else if (k == IconKind::Image) {
        p.drawRect(QRectF(r.x()+.18*s, r.y()+.28*s, .64*s, .44*s));
        p.drawEllipse(P(.36,.42), .05*s, .05*s);
        QPointF pts[5] = {P(.22,.68), P(.44,.48), P(.6,.62), P(.7,.54), P(.8,.68)};
        p.drawPolyline(pts, 5);
    } else if (k == IconKind::Font) {
        p.drawLine(P(.25,.78), P(.5,.22));
        p.drawLine(P(.5,.22), P(.75,.78));
        p.drawLine(P(.35,.58), P(.65,.58));
    } else if (k == IconKind::Download) {
        p.drawLine(P(.5,.18), P(.5,.62));
        p.drawLine(P(.32,.48), P(.5,.66));
        p.drawLine(P(.68,.48), P(.5,.66));
        p.drawLine(P(.22,.78), P(.78,.78));
    } else if (k == IconKind::Hammer) {
        p.drawLine(P(.3,.3), P(.7,.7));
        p.drawLine(P(.24,.36), P(.36,.24));
        p.drawLine(P(.7,.6), P(.84,.74));
        p.drawLine(P(.6,.3), P(.7,.2));
        p.drawLine(P(.74,.24), P(.84,.34));
    } else if (k == IconKind::Folder) {
        QPointF pts[7] = {P(.16,.75), P(.16,.3), P(.42,.3),
                          P(.5,.4), P(.84,.4), P(.84,.75), P(.16,.75)};
        p.drawPolyline(pts, 7);
    } else if (k == IconKind::Rocket) {
        p.drawLine(P(.5,.16), P(.68,.5));
        p.drawLine(P(.5,.16), P(.32,.5));
        p.drawLine(P(.32,.5), P(.5,.62));
        p.drawLine(P(.68,.5), P(.5,.62));
        p.drawLine(P(.42,.68), P(.58,.68));
        p.drawLine(P(.44,.78), P(.56,.78));
    } else if (k == IconKind::Target) {
        p.drawEllipse(P(.5,.5), .32*s, .32*s);
        p.drawEllipse(P(.5,.5), .18*s, .18*s);
        p.setBrush(c);
        p.drawEllipse(P(.5,.5), .06*s, .06*s);
    } else if (k == IconKind::Refresh) {
        p.drawArc(QRectF(r.x()+.22*s, r.y()+.22*s, .56*s, .56*s), 60*16, 260*16);
        QPointF a[3] = { P(.72,.22), P(.86,.34), P(.68,.40) };
        p.drawPolyline(a, 3);
    } else if (k == IconKind::Broom) {
        p.drawLine(P(.6,.2), P(.4,.55));
        p.drawLine(P(.32,.55), P(.68,.55));
        p.drawLine(P(.32,.55), P(.26,.8));
        p.drawLine(P(.68,.55), P(.74,.8));
        p.drawLine(P(.26,.8), P(.74,.8));
        p.drawLine(P(.42,.62), P(.4,.78));
        p.drawLine(P(.5,.62), P(.5,.78));
        p.drawLine(P(.58,.62), P(.6,.78));
    } else if (k == IconKind::Search) {
        p.drawEllipse(P(.42,.42), .22*s, .22*s);
        p.drawLine(P(.6,.6), P(.82,.82));
    } else if (k == IconKind::Shield) {
        QPointF pts[7] = {P(.5,.18), P(.8,.3), P(.8,.58),
                          P(.5,.84), P(.2,.58), P(.2,.3), P(.5,.18)};
        p.drawPolyline(pts, 7);
    } else if (k == IconKind::Check) {
        QPointF pts[3] = {P(.22,.5), P(.42,.72), P(.8,.28)};
        p.drawPolyline(pts, 3);
    }

    p.restore();
}

// ============================================================================
//  MainLogo — globe + play + arcs
// ============================================================================
class MainLogo : public QWidget {
public:
    explicit MainLogo(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    QPixmap render(int size) const {
        QPixmap pm(256, 256);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal cx = 128, cy = 128;

        QLinearGradient bgG(0, 0, 256, 256);
        bgG.setColorAt(0, QColor("#0e1a26"));
        bgG.setColorAt(1, Col::logoBg);
        p.setBrush(bgG);
        p.setPen(QPen(QColor("#18293a"), 2));
        p.drawRoundedRect(QRectF(4, 4, 248, 248), 52, 52);

        p.setPen(QPen(Col::logoGrid, 1.6));
        p.setBrush(Qt::NoBrush);
        const qreal globeR = 78;
        p.drawEllipse(QPointF(cx, cy), globeR, globeR);
        for (qreal y = -globeR; y <= globeR; y += globeR/2.2) {
            const qreal r = std::sqrt(std::max(0.0, globeR*globeR - y*y));
            p.drawLine(QPointF(cx - r, cy + y), QPointF(cx + r, cy + y));
        }
        for (qreal x = -globeR; x <= globeR; x += globeR/3.0) {
            const qreal t = x / globeR;
            const qreal rx = std::abs(t) * globeR;
            p.drawEllipse(QPointF(cx, cy), rx, globeR);
        }

        QRadialGradient innerGlow(QPointF(cx, cy), globeR);
        innerGlow.setColorAt(0, QColor(Col::logoTeal.red(), Col::logoTeal.green(),
                                        Col::logoTeal.blue(), 30));
        innerGlow.setColorAt(1, QColor(0, 0, 0, 0));
        p.setBrush(innerGlow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, cy), globeR, globeR);

        p.setBrush(QColor("#081018"));
        p.setPen(QPen(QColor("#1a2a38"), 1.5));
        p.drawEllipse(QPointF(cx, cy), 52, 52);

        QLinearGradient playG(QPointF(cx - 20, cy - 20), QPointF(cx + 24, cy + 20));
        playG.setColorAt(0, Col::logoGreen);
        playG.setColorAt(1, Col::logoTeal);
        QPainterPath chv;
        chv.moveTo(cx - 18, cy - 26);
        chv.lineTo(cx + 22, cy);
        chv.lineTo(cx - 18, cy + 26);
        chv.lineTo(cx - 18, cy + 6);
        chv.lineTo(cx + 4,  cy);
        chv.lineTo(cx - 18, cy - 6);
        chv.closeSubpath();
        p.setBrush(playG);
        p.setPen(Qt::NoPen);
        p.drawPath(chv);

        auto drawArc = [&](qreal startDeg, qreal spanDeg, const QColor& c) {
            QPen arcPen(c, 10, Qt::SolidLine, Qt::RoundCap);
            p.setPen(arcPen);
            p.setBrush(Qt::NoBrush);
            const qreal arcR = 96;
            QRectF rect(cx - arcR, cy - arcR, arcR * 2, arcR * 2);
            p.drawArc(rect, int(startDeg * 16), int(spanDeg * 16));
        };
        drawArc(35, 60, Col::logoTeal);
        drawArc(215, 60, Col::logoTeal);

        p.setPen(Qt::NoPen);
        p.setBrush(Col::logoGreen);
        p.drawEllipse(QPointF(cx, cy - globeR - 18), 8, 8);
        p.setBrush(Col::logoCyan);
        p.drawEllipse(QPointF(cx - globeR - 18, cy), 6, 6);
        p.drawEllipse(QPointF(cx + globeR + 18, cy), 6, 6);

        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawPixmap(0, 0, render(width()));
    }
};

// ============================================================================
//  Background
// ============================================================================
class Background : public QWidget {
public:
    explicit Background(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, Col::bgSubtle);
        g.setColorAt(1, Col::bg);
        p.fillRect(rect(), g);
        const QPointF c(width() * 0.5, height() * 0.2);
        QRadialGradient rg(c, std::max(width(), height()) * 0.7);
        rg.setColorAt(0, QColor(Col::accent.red(), Col::accent.green(),
                                Col::accent.blue(), 10));
        rg.setColorAt(1, QColor(0, 0, 0, 0));
        p.fillRect(rect(), rg);
    }
};

// ============================================================================
//  Button
// ============================================================================
class Button : public QPushButton {
public:
    Button(const QString& text, bool primary, QWidget* parent = nullptr)
        : QPushButton(text, parent), primary_(primary)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(42);
        setMinimumWidth(180);
        anim_ = new QVariantAnimation(this);
        anim_->setDuration(140);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v){ animT_ = v.toReal(); update(); });
    }
protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::Enter) animate(1.0);
        if (e->type() == QEvent::Leave) animate(0.0);
        return QPushButton::event(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool en = isEnabled();
        const bool prs = isDown();
        const qreal t = animT_;
        QColor bg, fg, border;
        if (!en) { bg = Col::surface; fg = Col::textFaint; border = Col::borderSoft; }
        else if (primary_) {
            bg = prs ? Col::accentLo
                     : QColor::fromRgbF(
                         Col::accent.redF()   + (Col::accentHi.redF()   - Col::accent.redF())   * t,
                         Col::accent.greenF() + (Col::accentHi.greenF() - Col::accent.greenF()) * t,
                         Col::accent.blueF()  + (Col::accentHi.blueF()  - Col::accent.blueF())  * t);
            fg = Col::textBright;
            border = Qt::transparent;
        } else {
            bg = prs ? Col::surfaceHi
                     : QColor::fromRgbF(
                         Col::surface.redF()   + (Col::surfaceAlt.redF()   - Col::surface.redF())   * t,
                         Col::surface.greenF() + (Col::surfaceAlt.greenF() - Col::surface.greenF()) * t,
                         Col::surface.blueF()  + (Col::surfaceAlt.blueF()  - Col::surface.blueF())  * t);
            fg = Col::textPrimary;
            border = Col::border;
        }
        QRectF r = rect().adjusted(1, 1, -1, -1);
        p.setPen(QPen(border, 1));
        p.setBrush(bg);
        p.drawRoundedRect(r, 8, 8);
        QFont f = font();
        f.setPixelSize(13);
        f.setWeight(primary_ ? QFont::DemiBold : QFont::Normal);
        p.setFont(f);
        p.setPen(fg);
        p.drawText(rect(), Qt::AlignCenter, text());
    }
private:
    void animate(qreal v) {
        anim_->stop(); anim_->setStartValue(animT_); anim_->setEndValue(v); anim_->start();
    }
    bool primary_;
    qreal animT_ = 0.0;
    QVariantAnimation* anim_ = nullptr;
};

// ============================================================================
//  WindowControls
// ============================================================================
class WindowControls : public QWidget {
public:
    std::function<void()> onMin, onMax, onClose;
    explicit WindowControls(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(126, 34);
        setMouseTracking(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 3; ++i) {
            QRectF r(i * 42, 0, 38, 34);
            bool hov = (hover_ == i);
            if (hov) {
                p.setPen(Qt::NoPen);
                p.setBrush(i == 2 ? Col::danger : Col::surfaceHi);
                p.drawRoundedRect(r.adjusted(5, 6, -5, -6), 6, 6);
            }
            p.setPen(QPen(hov ? Qt::white : Col::textMuted, 1.3,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            QPointF c = r.center();
            if (i == 0) p.drawLine(QPointF(c.x()-5, c.y()+3), QPointF(c.x()+5, c.y()+3));
            else if (i == 1) p.drawRoundedRect(QRectF(c.x()-4.5, c.y()-4.5, 9, 9), 1.2, 1.2);
            else {
                p.drawLine(QPointF(c.x()-4, c.y()-4), QPointF(c.x()+4, c.y()+4));
                p.drawLine(QPointF(c.x()+4, c.y()-4), QPointF(c.x()-4, c.y()+4));
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        int h = -1;
        for (int i = 0; i < 3; ++i)
            if (QRectF(i*42, 0, 38, 34).contains(e->position())) { h = i; break; }
        if (h != hover_) { hover_ = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
    }
    void leaveEvent(QEvent*) override { hover_ = -1; update(); }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        int h = -1;
        for (int i = 0; i < 3; ++i)
            if (QRectF(i*42, 0, 38, 34).contains(e->position())) { h = i; break; }
        if (h == 0 && onMin)   onMin();
        if (h == 1 && onMax)   onMax();
        if (h == 2 && onClose) onClose();
    }
private:
    int hover_ = -1;
};

// ============================================================================
//  DepCard
// ============================================================================
class DepCard : public QFrame {
public:
    DepCard(IconKind kind, const QString& name, const QString& detail,
            const QString& size, QWidget* parent = nullptr)
        : QFrame(parent), kind_(kind)
    {
        setFixedHeight(60);
        setStyleSheet(QString("DepCard{background:%1;border:1px solid %2;border-radius:8px;}")
                          .arg(Col::surface.name(), Col::borderSoft.name()));
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(14, 0, 14, 0);
        row->setSpacing(12);
        iconWidget_ = new QWidget(this);
        iconWidget_->setFixedSize(32, 32);
        iconWidget_->installEventFilter(this);
        row->addWidget(iconWidget_);
        auto* col = new QVBoxLayout;
        col->setSpacing(1);
        auto* nameL = new QLabel(name, this);
        nameL->setStyleSheet(QString("color:%1;font-size:12.5px;font-weight:600;")
                                .arg(Col::textPrimary.name()));
        auto* detailL = new QLabel(detail, this);
        detailL->setStyleSheet(QString("color:%1;font-size:11px;")
                                  .arg(Col::textMuted.name()));
        col->addWidget(nameL);
        col->addWidget(detailL);
        row->addLayout(col, 1);
        auto* sizeL = new QLabel(size, this);
        sizeL->setStyleSheet(QString("color:%1;font-size:10.5px;background:%2;"
                                      "border-radius:5px;padding:4px 9px;font-weight:500;")
                                .arg(Col::textMuted.name(), Col::surfaceHi.name()));
        row->addWidget(sizeL, 0, Qt::AlignVCenter);
    }
protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (o == iconWidget_ && e->type() == QEvent::Paint) {
            QPainter p(iconWidget_);
            paintIcon(p, kind_, QRectF(4, 4, 24, 24), Col::accentHi);
            return true;
        }
        return QFrame::eventFilter(o, e);
    }
private:
    IconKind kind_;
    QWidget* iconWidget_ = nullptr;
};

// ============================================================================
//  OSLogo — static Tux and Windows 11
// ============================================================================
class OSLogo : public QWidget {
public:
    enum Kind { Linux, Windows };
    explicit OSLogo(Kind k, QWidget* parent = nullptr)
        : QWidget(parent), kind_(k) {
        setFixedSize(84, 84);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (kind_ == Linux) drawTux(p, rect());
        else                 drawWindows(p, rect());
    }
private:
    Kind kind_;

    static void drawTux(QPainter& p, const QRectF& box) {
        const qreal s = box.width() / 100.0;
        const QPointF O = box.topLeft();
        auto P = [&](qreal x, qreal y) { return QPointF(O.x() + x*s, O.y() + y*s); };
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e8a028"));
        QPainterPath lf;
        lf.moveTo(P(32,82)); lf.quadTo(P(22,86), P(24,93));
        lf.quadTo(P(28,96), P(40,94)); lf.quadTo(P(42,86), P(40,82));
        lf.closeSubpath(); p.drawPath(lf);
        QPainterPath rf;
        rf.moveTo(P(60,82)); rf.quadTo(P(70,86), P(68,93));
        rf.quadTo(P(64,96), P(52,94)); rf.quadTo(P(50,86), P(52,82));
        rf.closeSubpath(); p.drawPath(rf);
        QRadialGradient bg(P(45,50), 55*s);
        bg.setColorAt(0, QColor("#2c2e34"));
        bg.setColorAt(0.6, QColor("#15161a"));
        bg.setColorAt(1, QColor("#090a0c"));
        p.setBrush(bg);
        QPainterPath body;
        body.moveTo(P(20,30));
        body.cubicTo(P(10,40), P(10,70), P(28,88));
        body.lineTo(P(72,88));
        body.cubicTo(P(90,70), P(90,40), P(80,30));
        body.cubicTo(P(78,16), P(68,6), P(50,6));
        body.cubicTo(P(32,6), P(22,16), P(20,30));
        body.closeSubpath(); p.drawPath(body);
        QRadialGradient blg(P(50,55), 30*s);
        blg.setColorAt(0, QColor("#ffffff"));
        blg.setColorAt(0.7, QColor("#f0f0e9"));
        blg.setColorAt(1, QColor("#c9c8bc"));
        p.setBrush(blg);
        QPainterPath belly;
        belly.moveTo(P(32,45));
        belly.cubicTo(P(30,70), P(38,84), P(50,84));
        belly.cubicTo(P(62,84), P(70,70), P(68,45));
        belly.cubicTo(P(66,38), P(58,36), P(50,36));
        belly.cubicTo(P(42,36), P(34,38), P(32,45));
        belly.closeSubpath(); p.drawPath(belly);
        p.setBrush(Qt::white);
        p.drawEllipse(P(42,22), 6*s, 7.5*s);
        p.drawEllipse(P(58,22), 6*s, 7.5*s);
        p.setBrush(QColor("#0a0a0a"));
        p.drawEllipse(P(43,24), 3*s, 3.8*s);
        p.drawEllipse(P(59,24), 3*s, 3.8*s);
        p.setBrush(QColor(255,255,255,230));
        p.drawEllipse(P(41.5,22), 1.2*s, 1.4*s);
        p.drawEllipse(P(57.5,22), 1.2*s, 1.4*s);
        QLinearGradient bkg(P(46,30), P(54,42));
        bkg.setColorAt(0, QColor("#f0c040"));
        bkg.setColorAt(1, QColor("#dc8618"));
        p.setBrush(bkg);
        QPainterPath beak;
        beak.moveTo(P(44,33)); beak.lineTo(P(50,44)); beak.lineTo(P(56,33));
        beak.quadTo(P(50,31), P(44,33)); beak.closeSubpath();
        p.drawPath(beak);
        p.setBrush(QColor("#1a1b1f"));
        QPainterPath fL;
        fL.moveTo(P(24,38));
        fL.cubicTo(P(8,48), P(6,68), P(20,76));
        fL.cubicTo(P(24,70), P(22,55), P(24,38));
        fL.closeSubpath(); p.drawPath(fL);
        QPainterPath fR;
        fR.moveTo(P(76,38));
        fR.cubicTo(P(92,48), P(94,68), P(80,76));
        fR.cubicTo(P(76,70), P(78,55), P(76,38));
        fR.closeSubpath(); p.drawPath(fR);
    }

    static void drawWindows(QPainter& p, const QRectF& box) {
        const qreal s = box.width();
        const QPointF c = box.center();
        const qreal total = s * 0.66;
        const qreal gap = total * 0.05;
        const qreal sq = (total - gap) / 2.0;
        const qreal x0 = c.x() - total/2.0;
        const qreal y0 = c.y() - total/2.0;
        auto sqd = [&](qreal x, qreal y, const QColor& a, const QColor& b) {
            QRectF r(x, y, sq, sq);
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0, a); g.setColorAt(1, b);
            p.setPen(Qt::NoPen); p.setBrush(g); p.drawRect(r);
        };
        sqd(x0, y0,               QColor("#4b9cd8"), QColor("#3b8acc"));
        sqd(x0+sq+gap, y0,         QColor("#4596d4"), QColor("#3584c6"));
        sqd(x0, y0+sq+gap,         QColor("#3d8ecb"), QColor("#2f7ebf"));
        sqd(x0+sq+gap, y0+sq+gap,  QColor("#3989c7"), QColor("#2b78b8"));
    }
};

// ============================================================================
//  OSChoiceCard
// ============================================================================
class OSChoiceCard : public QFrame {
public:
    std::function<void()> onClick;
    bool selected_ = false;
    OSLogo::Kind kind_;
    explicit OSChoiceCard(OSLogo::Kind k, QWidget* parent = nullptr)
        : QFrame(parent), kind_(k)
    {
        setFixedHeight(200);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        hoverAnim_ = new QVariantAnimation(this);
        hoverAnim_->setDuration(160);
        hoverAnim_->setEasingCurve(QEasingCurve::OutCubic);
        connect(hoverAnim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v){ hoverT_ = v.toReal(); update(); });
        logo_ = new OSLogo(k, this);
        logo_->setGeometry((width()-84)/2, 24, 84, 84);
    }
    void resizeEvent(QResizeEvent*) override {
        logo_->setGeometry((width()-84)/2, 24, 84, 84);
    }
    void setSelected(bool s) { selected_ = s; update(); }
    bool isSelected() const { return selected_; }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(3, 3, -3, -3);
        QColor bg, border;
        if (selected_) {
            bg = QColor(Col::accent.red(), Col::accent.green(), Col::accent.blue(),
                        int(24 + 8*hoverT_));
            border = Col::accent;
        } else {
            bg = QColor(255,255,255, int(5 + 6*hoverT_));
            border = Col::border;
        }
        p.setPen(QPen(border, selected_ ? 2 : 1));
        p.setBrush(bg);
        p.drawRoundedRect(r, 12, 12);
        QFont f = font();
        f.setPixelSize(17); f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(selected_ ? Col::accentHi : Col::textBright);
        const QString title = (kind_ == OSLogo::Linux) ? "Linux" : "Windows";
        p.drawText(QRectF(r.x(), 120, r.width(), 26), Qt::AlignCenter, title);
        f.setPixelSize(11.5); f.setWeight(QFont::Normal);
        p.setFont(f);
        p.setPen(Col::textMuted);
        const QString subtitle = (kind_ == OSLogo::Linux)
            ? "Ubuntu, Debian, Fedora, Arch\napt / dnf / pacman"
            : "Windows 10 / 11\nwinget / Chocolatey";
        p.drawText(QRectF(r.x()+10, 152, r.width()-20, 40),
                   Qt::AlignCenter | Qt::TextWordWrap, subtitle);
        if (selected_) {
            const QPointF c(r.right()-22, r.top()+22);
            p.setPen(Qt::NoPen); p.setBrush(Col::accent); p.drawEllipse(c, 10, 10);
            p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(c.x()-4, c.y()+0.5), QPointF(c.x()-1, c.y()+3.5));
            p.drawLine(QPointF(c.x()-1, c.y()+3.5), QPointF(c.x()+5, c.y()-3.5));
        }
    }
    void enterEvent(QEnterEvent*) override {
        hoverAnim_->stop(); hoverAnim_->setStartValue(hoverT_);
        hoverAnim_->setEndValue(1.0); hoverAnim_->start();
    }
    void leaveEvent(QEvent*) override {
        hoverAnim_->stop(); hoverAnim_->setStartValue(hoverT_);
        hoverAnim_->setEndValue(0.0); hoverAnim_->start();
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && onClick) onClick();
    }
private:
    OSLogo* logo_ = nullptr;
    qreal hoverT_ = 0.0;
    QVariantAnimation* hoverAnim_ = nullptr;
};

// ============================================================================
//  Installer
// ============================================================================
class Installer : public QWidget {
public:
    Installer() {
        setWindowTitle(kAppName);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setMinimumSize(920, 640);
        resize(1080, 760);
        buildUi();
        centerOnScreen();
    }
protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (bg_)       bg_->setGeometry(rect());
        if (stack_)    stack_->setGeometry(rect());
        if (controls_) controls_->move(width()-controls_->width()-10, 8);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && e->position().y() < 46) {
            dragPos_ = e->globalPosition().toPoint() - frameGeometry().topLeft();
            dragging_ = true;
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_ && (e->buttons() & Qt::LeftButton))
            move(e->globalPosition().toPoint() - dragPos_);
    }
    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = false; }
private:
    void buildUi() {
        bg_ = new Background(this);
        bg_->setGeometry(rect());
        bg_->lower();
        stack_ = new QStackedWidget(this);
        stack_->setStyleSheet("background:transparent;");
        stack_->setGeometry(rect());
        stack_->addWidget(buildWelcome());
        stack_->addWidget(buildOsChoice());
        stack_->addWidget(buildDeps());
        stack_->addWidget(buildRunning());
        stack_->addWidget(buildDone());
        stack_->addWidget(buildFailed());
        controls_ = new WindowControls(this);
        controls_->onMin   = [this]{ showMinimized(); };
        controls_->onMax   = [this]{ if (isMaximized()) showNormal(); else showMaximized(); };
        controls_->onClose = [this]{ close(); };
        controls_->move(width()-controls_->width()-10, 8);
    }
    void centerOnScreen() {
        if (auto* s = QGuiApplication::primaryScreen()) {
            const QRect g = s->availableGeometry();
            move(g.center().x() - width()/2, g.center().y() - height()/2);
        }
    }

    // ── Welcome ─────────────────────────────────────────────────────────
    QWidget* buildWelcome() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);
        auto* logoRow = new QHBoxLayout;
        logoRow->addStretch(1);
        auto* logo = new MainLogo(page);
        logo->setFixedSize(140, 140);
        logoRow->addWidget(logo);
        logoRow->addStretch(1);
        v->addLayout(logoRow);
        v->addSpacing(28);
        auto* title = new QLabel("RootBrowser", page);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QString("color:%1;font-size:48px;font-weight:200;"
                    "letter-spacing:-1.2px;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(title);
        auto* version = new QLabel(QString("Version %1").arg(kVersion), page);
        version->setAlignment(Qt::AlignCenter);
        version->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:600;letter-spacing:3px;"
                    "background:transparent;").arg(Col::textMuted.name()));
        v->addSpacing(8);
        v->addWidget(version);
        v->addSpacing(26);
        auto* desc = new QLabel(
            "A modern, cross-platform web browser built with Qt6 and Chromium.", page);
        desc->setAlignment(Qt::AlignCenter);
        desc->setStyleSheet(
            QString("color:%1;font-size:13.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(desc);
        v->addSpacing(4);
        auto* desc2 = new QLabel(
            QString("The wizard will guide you through setup on your %1 system.")
                .arg(kIsWindows ? "Windows" : "Linux"), page);
        desc2->setAlignment(Qt::AlignCenter);
        desc2->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textFaint.name()));
        v->addWidget(desc2);
        v->addSpacing(40);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* cancel = new Button("Cancel", false, page);
        auto* next   = new Button("Get Started", true, page);
        btnRow->addWidget(cancel);
        btnRow->addWidget(next);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        v->addStretch(3);
        auto* footer = new QLabel(
            QString("RootBrowser · github.com/%1/%2")
                .arg(kGitHubUser, kRepoName), page);
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;")
                .arg(Col::textFaint.name()));
        v->addWidget(footer);
        connect(cancel, &QPushButton::clicked, this, &QWidget::close);
        connect(next, &QPushButton::clicked, this, [this]{ stack_->setCurrentIndex(1); });
        return page;
    }

    // ── OS Choice ───────────────────────────────────────────────────────
    QWidget* buildOsChoice() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 48, 72, 40);
        v->setSpacing(0);
        auto* heading = new QLabel("Choose your operating system", page);
        heading->setAlignment(Qt::AlignCenter);
        heading->setStyleSheet(
            QString("color:%1;font-size:24px;font-weight:400;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(heading);
        v->addSpacing(8);
        auto* sub = new QLabel(
            "The wizard uses the correct package manager for your system.", page);
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(sub);
        v->addSpacing(36);
        auto* cards = new QHBoxLayout;
        cards->setSpacing(20);
        cards->addStretch(1);
        linuxCard_ = new OSChoiceCard(OSLogo::Linux, page);
        winCard_   = new OSChoiceCard(OSLogo::Windows, page);
        linuxCard_->setMinimumWidth(260);
        winCard_->setMinimumWidth(260);
        cards->addWidget(linuxCard_);
        cards->addWidget(winCard_);
        cards->addStretch(1);
        if (kIsWindows) { selectedOs_ = OS::Windows; winCard_->setSelected(true); }
        else            { selectedOs_ = OS::Linux;   linuxCard_->setSelected(true); }
        linuxCard_->onClick = [this]{
            selectedOs_ = OS::Linux;
            linuxCard_->setSelected(true);
            winCard_->setSelected(false);
            hideWarn();
        };
        winCard_->onClick = [this]{
            selectedOs_ = OS::Windows;
            winCard_->setSelected(true);
            linuxCard_->setSelected(false);
            hideWarn();
        };
        v->addLayout(cards);
        v->addSpacing(20);
        warnLabel_ = new QLabel("", page);
        warnLabel_->setAlignment(Qt::AlignCenter);
        warnLabel_->setWordWrap(true);
        warnLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:rgba(184,88,88,0.10);"
                    "border:1px solid rgba(184,88,88,0.30);border-radius:8px;"
                    "padding:10px;").arg(Col::danger.name()));
        warnLabel_->setVisible(false);
        v->addWidget(warnLabel_);
        v->addStretch(1);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* back = new Button("Back", false, page);
        auto* next = new Button("Continue", true, page);
        btnRow->addWidget(back);
        btnRow->addWidget(next);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        connect(back, &QPushButton::clicked, this, [this]{ stack_->setCurrentIndex(0); });
        connect(next, &QPushButton::clicked, this, [this]{
            if ((kIsWindows && selectedOs_ == OS::Linux) ||
                (!kIsWindows && selectedOs_ == OS::Windows)) {
                warnLabel_->setText(
                    QString("You selected %1 but you are running %2. "
                            "Installation steps for %1 will not work here.")
                        .arg(selectedOs_ == OS::Linux ? "Linux" : "Windows",
                             kIsWindows ? "Windows" : "Linux"));
                warnLabel_->setVisible(true);
                return;
            }
            rebuildDepsList();
            stack_->setCurrentIndex(2);
        });
        return page;
    }
    void hideWarn() { if (warnLabel_) warnLabel_->setVisible(false); }

    // ── Deps ────────────────────────────────────────────────────────────
    QWidget* buildDeps() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 40, 72, 28);
        v->setSpacing(0);
        auto* heading = new QLabel("Installation plan", page);
        heading->setAlignment(Qt::AlignCenter);
        heading->setStyleSheet(
            QString("color:%1;font-size:22px;font-weight:400;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::textBright.name()));
        depsSubLabel_ = new QLabel("", page);
        depsSubLabel_->setAlignment(Qt::AlignCenter);
        depsSubLabel_->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(heading);
        v->addSpacing(6);
        v->addWidget(depsSubLabel_);
        v->addSpacing(20);
        auto* scroll = new QScrollArea(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(
            "QScrollArea{background:transparent;border:none;}"
            "QScrollArea > QWidget > QWidget{background:transparent;}"
            "QScrollBar:vertical{background:transparent;width:8px;}"
            "QScrollBar::handle:vertical{background:#242832;border-radius:4px;min-height:40px;}"
            "QScrollBar::handle:vertical:hover{background:#333846;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
        depsListWidget_ = new QWidget;
        depsListWidget_->setStyleSheet("background:transparent;");
        depsListLayout_ = new QVBoxLayout(depsListWidget_);
        depsListLayout_->setContentsMargins(0, 0, 10, 0);
        depsListLayout_->setSpacing(6);
        scroll->setWidget(depsListWidget_);
        v->addWidget(scroll, 1);
        v->addSpacing(14);
        depsInfoLabel_ = new QLabel("", page);
        depsInfoLabel_->setAlignment(Qt::AlignCenter);
        depsInfoLabel_->setWordWrap(true);
        depsInfoLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:rgba(184,144,74,0.08);"
                    "border:1px solid rgba(184,144,74,0.25);border-radius:8px;"
                    "padding:10px;").arg(Col::warning.name()));
        v->addWidget(depsInfoLabel_);
        v->addSpacing(14);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* back = new Button("Back", false, page);
        auto* next = new Button("Download and Install", true, page);
        btnRow->addWidget(back);
        btnRow->addWidget(next);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        connect(back, &QPushButton::clicked, this, [this]{ stack_->setCurrentIndex(1); });
        connect(next, &QPushButton::clicked, this, [this]{
            stack_->setCurrentIndex(3);
            startInstall();
        });
        return page;
    }

    // ── Running ─────────────────────────────────────────────────────────
    QWidget* buildRunning() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 48, 72, 36);
        v->setSpacing(0);
        auto* heading = new QLabel("Installing RootBrowser", page);
        heading->setAlignment(Qt::AlignCenter);
        heading->setStyleSheet(
            QString("color:%1;font-size:19px;font-weight:400;"
                    "background:transparent;").arg(Col::textBright.name()));
        v->addWidget(heading);
        v->addSpacing(6);
        stepLabel_ = new QLabel("Preparing", page);
        stepLabel_->setAlignment(Qt::AlignCenter);
        stepLabel_->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;")
                .arg(Col::accent.name()));
        v->addWidget(stepLabel_);
        v->addSpacing(16);
        progress_ = new QProgressBar(page);
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(false);
        progress_->setFixedHeight(6);
        progress_->setStyleSheet(
            QString("QProgressBar{background:%1;border:none;border-radius:3px;}"
                    "QProgressBar::chunk{background:%2;border-radius:3px;}")
                .arg(Col::surfaceHi.name(), Col::accent.name()));
        v->addWidget(progress_);
        v->addSpacing(18);
        terminal_ = new QPlainTextEdit(page);
        terminal_->setReadOnly(true);
        terminal_->setFrameStyle(QFrame::NoFrame);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPixelSize(11.5);
        terminal_->setFont(mono);
        terminal_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;"
                    "border:1px solid %3;border-radius:8px;padding:12px;}")
                .arg(Col::surface.name(), Col::textPrimary.name(), Col::border.name()));
        terminal_->verticalScrollBar()->setStyleSheet(
            "QScrollBar:vertical{background:transparent;width:8px;}"
            "QScrollBar::handle:vertical{background:#242832;border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
        v->addWidget(terminal_, 1);
        v->addSpacing(12);
        statusLabel_ = new QLabel("Starting", page);
        statusLabel_->setAlignment(Qt::AlignCenter);
        statusLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(statusLabel_);
        return page;
    }

    // ── Done ────────────────────────────────────────────────────────────
    QWidget* buildDone() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);
        auto* icon = new QLabel(page);
        icon->setFixedSize(64, 64);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(makeSuccessIcon(64));
        v->addWidget(icon, 0, Qt::AlignHCenter);
        v->addSpacing(18);
        doneTitle_ = new QLabel("Installation complete", page);
        doneTitle_->setAlignment(Qt::AlignCenter);
        doneTitle_->setStyleSheet(
            QString("color:%1;font-size:26px;font-weight:300;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(doneTitle_);
        v->addSpacing(6);
        auto* sub = new QLabel(
            "RootBrowser is now installed on your system.", page);
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(sub);
        v->addSpacing(26);
        doneInfoCard_ = new QLabel(page);
        doneInfoCard_->setTextFormat(Qt::RichText);
        doneInfoCard_->setAlignment(Qt::AlignLeft);
        doneInfoCard_->setStyleSheet(
            QString("QLabel{background:%1;border:1px solid %2;"
                    "border-radius:8px;padding:16px 20px;}")
                .arg(Col::surface.name(), Col::borderSoft.name()));
        doneInfoCard_->setMaximumWidth(540);
        auto* cardRow = new QHBoxLayout;
        cardRow->addStretch(1);
        cardRow->addWidget(doneInfoCard_);
        cardRow->addStretch(1);
        v->addLayout(cardRow);
        v->addSpacing(26);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        doneLaunchBtn_ = new Button("Launch RootBrowser", true, page);
        auto* closeBtn = new Button("Close", false, page);
        btnRow->addWidget(closeBtn);
        btnRow->addWidget(doneLaunchBtn_);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        v->addStretch(3);
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
        connect(doneLaunchBtn_, &QPushButton::clicked, this, [this]{
            if (selectedOs_ == OS::Windows) {
#ifdef Q_OS_WIN
                QProcess::startDetached("cmd", {"/c", "start", "", kDisplayName});
#endif
            } else {
                QProcess::startDetached("/usr/local/bin/rootbrowser");
            }
        });
        return page;
    }

    // ── Failed ──────────────────────────────────────────────────────────
    QWidget* buildFailed() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);
        auto* icon = new QLabel(page);
        icon->setFixedSize(64, 64);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(makeErrorIcon(64));
        v->addWidget(icon, 0, Qt::AlignHCenter);
        v->addSpacing(18);
        auto* title = new QLabel("Installation failed", page);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QString("color:%1;font-size:26px;font-weight:300;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::danger.name()));
        v->addWidget(title);
        v->addSpacing(6);
        auto* sub = new QLabel(
            "Something went wrong. Check the log below for details.", page);
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(sub);
        v->addSpacing(18);
        failedLog_ = new QPlainTextEdit(page);
        failedLog_->setReadOnly(true);
        failedLog_->setMaximumHeight(190);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPixelSize(11.5);
        failedLog_->setFont(mono);
        failedLog_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;"
                    "border:1px solid %3;border-radius:8px;padding:12px;}")
                .arg(Col::surface.name(), Col::textMuted.name(), Col::border.name()));
        v->addWidget(failedLog_);
        v->addSpacing(18);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* closeBtn = new Button("Close", false, page);
        auto* retryBtn = new Button("Retry", true, page);
        btnRow->addWidget(closeBtn);
        btnRow->addWidget(retryBtn);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        v->addStretch(3);
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
        connect(retryBtn, &QPushButton::clicked, this, [this]{
            if (terminal_) terminal_->clear();
            if (progress_) progress_->setValue(0);
            stack_->setCurrentIndex(3);
            startInstall();
        });
        return page;
    }

    // ── Deps builders ───────────────────────────────────────────────────
    void rebuildDepsList() {
        QLayoutItem* item;
        while ((item = depsListLayout_->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        if (selectedOs_ == OS::Linux) {
            depsSubLabel_->setText("Running on Linux · using apt package manager");
            depsInfoLabel_->setText(
                "Root privileges are required for apt and installation to /opt.");
            addDepsLinux();
        } else {
            depsSubLabel_->setText("Running on Windows · using winget (chocolatey fallback)");
            depsInfoLabel_->setText(
                "Administrator privileges are required for installation.");
            addDepsWindows();
        }
        depsListLayout_->addStretch(1);
    }

    void addDepsLinux() {
        struct D { IconKind k; const char* n; const char* d; const char* s; };
        static const D items[] = {
            {IconKind::Package, "Update package index",  "apt-get update",                                     "30 s"},
            {IconKind::Hammer,  "Build tools",           "build-essential, pkg-config, curl, ca-certificates", "200 MB"},
            {IconKind::Qt,      "Qt6 Widgets",           "qt6-base-dev · GUI framework",                       "80 MB"},
            {IconKind::Globe,   "Qt6 WebEngine",         "qt6-webengine-dev · Chromium engine",                "450 MB"},
            {IconKind::Cpu,     "WebEngine runtime",     "libqt6webenginecore6-bin",                           "120 MB"},
            {IconKind::Image,   "Graphics libraries",    "Mesa, XCB, xkbcommon, EGL",                          "40 MB"},
            {IconKind::Font,    "Font libraries",        "fontconfig, freetype",                               "15 MB"},
            {IconKind::Image,   "SVG and image tools",   "librsvg2-bin, imagemagick (for icon sizes)",         "30 MB"},
            {IconKind::Git,     "Download source",       "Clone from GitHub",                                  "1 MB"},
            {IconKind::Hammer,  "Compile browser",       "g++ -std=c++17 -O2",                                 "2 min"},
            {IconKind::Folder,  "Install to /opt",       "Binary + wrapper script",                            "5 MB"},
            {IconKind::Rocket,  "Desktop entry",         "rootbrowser.desktop",                                "1 KB"},
            {IconKind::Refresh, "Purge old icons",       "Remove all old icon files and caches",               "1 s"},
            {IconKind::Download,"Download fresh icon",   "Fetch latest SVG from GitHub",                       "1 s"},
            {IconKind::Target,  "Render icon sizes",     "Generate 48/64/128/256 PNGs from SVG",               "2 s"},
            {IconKind::Refresh, "Refresh caches",        "Icon cache, desktop database",                       "1 s"},
            {IconKind::Broom,   "Cleanup",               "Remove temporary files",                             "1 s"},
        };
        for (const auto& it : items)
            depsListLayout_->addWidget(new DepCard(it.k, it.n, it.d, it.s, depsListWidget_));
    }

    void addDepsWindows() {
        struct D { IconKind k; const char* n; const char* d; const char* s; };
        static const D items[] = {
            {IconKind::Search,  "Check winget",          "Verify winget availability", "5 s"},
            {IconKind::Search,  "Check Chocolatey",      "Fallback package manager",   "5 s"},
            {IconKind::Package, "Visual C++ runtime",    "VCRedist 2015+ x64",         "25 MB"},
            {IconKind::Qt,      "Qt6 Widgets",           "Qt6 base framework",         "120 MB"},
            {IconKind::Globe,   "Qt6 WebEngine",         "Chromium engine DLLs",       "500 MB"},
            {IconKind::Git,     "Download source",       "Clone from GitHub",          "1 MB"},
            {IconKind::Hammer,  "Compile browser",       "cl.exe /std:c++17 /O2",      "1 min"},
            {IconKind::Folder,  "Install to ProgramData","Binary under ProgramData",   "5 MB"},
            {IconKind::Download,"Download fresh icon",   "Fetch latest ICO from GitHub","1 s"},
            {IconKind::Rocket,  "Start Menu shortcut",   "RootBrowser.lnk",            "1 KB"},
            {IconKind::Target,  "Desktop shortcut",      "Optional shortcut",          "1 KB"},
            {IconKind::Refresh, "PATH entry",            "Adds RootBrowser to PATH",   "1 s"},
            {IconKind::Broom,   "Cleanup",               "Remove temporary files",     "1 s"},
        };
        for (const auto& it : items)
            depsListLayout_->addWidget(new DepCard(it.k, it.n, it.d, it.s, depsListWidget_));
    }

    // ── Terminal ────────────────────────────────────────────────────────
    void appendTerm(const QString& line, const QColor& c) {
        if (!terminal_) return;
        QString s = line;
        s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
        terminal_->appendHtml(
            QString("<span style='color:%1'>%2</span>").arg(c.name(), s));
        auto* sb = terminal_->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
    void setStatus(const QString& s, const QColor& c = Col::textMuted) {
        if (!statusLabel_) return;
        statusLabel_->setText(s);
        statusLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:transparent;").arg(c.name()));
    }
    void setStepTitle(const QString& s) { if (stepLabel_) stepLabel_->setText(s); }

    // ── Install ─────────────────────────────────────────────────────────
    struct Step { QString title; QString script; int progress; };

    void startInstall() {
        if (running_) return;
        running_ = true;
        if (terminal_) terminal_->clear();
        if (progress_) progress_->setValue(0);
        if (selectedOs_ == OS::Linux) startInstallLinux();
        else                          startInstallWindows();
    }

    void startInstallLinux() {
        const QString installDir = kInstallDir;
        const QString bin = kBinName;
        const QString user = kGitHubUser;
        const QString repo = kRepoName;

        QVector<Step> steps = {
            {"Updating package index",
             "apt-get update -qq 2>&1", 4},

            {"Installing build tools",
             "DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "
             "build-essential pkg-config git curl ca-certificates 2>&1", 10},

            {"Installing Qt6 Widgets",
             "DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "
             "qt6-base-dev 2>&1", 18},

            {"Installing Qt6 WebEngine",
             "DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "
             "qt6-webengine-dev qt6-webengine-dev-tools libqt6webenginecore6-bin 2>&1", 35},

            {"Installing graphics and font libraries",
             "DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "
             "libgl1-mesa-dev libx11-xcb-dev libxkbcommon-dev libegl1-mesa-dev "
             "libfontconfig1-dev libfreetype6-dev 2>&1", 42},

            {"Installing SVG and image tools",
             "DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "
             "librsvg2-bin imagemagick 2>&1", 46},

            {"Cloning source",
             "rm -rf /tmp/rb-src && git clone --depth=1 '" +
             QString(kGitHubUrl) + "' /tmp/rb-src 2>&1", 52},

            {"Compiling",
             "cd /tmp/rb-src && SRC=. ; "
             "if [ ! -f main.cpp ]; then "
             "  SRC=$(dirname $(find . -maxdepth 2 -name main.cpp | head -n1)); "
             "fi ; cd \"$SRC\" && CPP=$(ls *.cpp) && "
             "g++ -std=c++17 -O2 -fPIC $CPP -o " + bin + " "
             "$(pkg-config --cflags --libs Qt6WebEngineWidgets Qt6Widgets) 2>&1", 70},

            {"Installing binary",
             "set -e ; mkdir -p '" + installDir + "' ; "
             "cd /tmp/rb-src && SRC=. ; "
             "if [ ! -f main.cpp ]; then "
             "  SRC=$(dirname $(find . -maxdepth 2 -name main.cpp | head -n1)); "
             "fi ; cd \"$SRC\" && "
             "cp " + bin + " '" + installDir + "/' ; "
             "chmod 755 '" + installDir + "/" + bin + "' ; "
             "printf '#!/usr/bin/env bash\\nexec \"%s\" \"$@\"\\n' "
             "'" + installDir + "/" + bin + "' "
             "> /usr/local/bin/" + bin + " ; "
             "chmod 755 /usr/local/bin/" + bin + " 2>&1", 78},

            {"Creating desktop entry",
             "mkdir -p /usr/share/applications ; "
             "cat > /usr/share/applications/" + bin + ".desktop << 'DESK'\n"
             "[Desktop Entry]\nVersion=1.0\nType=Application\n"
             "Name=" + QString(kDisplayName) + "\n"
             "GenericName=Web Browser\n"
             "Comment=Modern cross-platform web browser\n"
             "Exec=" + bin + " %u\nTerminal=false\n"
             "Categories=Network;WebBrowser;\n"
             "MimeType=text/html;text/xml;application/xhtml+xml;"
             "x-scheme-handler/http;x-scheme-handler/https;\n"
             "StartupNotify=true\nIcon=" + bin + "\nDESK\n"
             "chmod 644 /usr/share/applications/" + bin + ".desktop 2>&1", 82},

            // ── NEW: purge old icons ──
            {"Purging old icons and caches",
             "set +e ; "
             "sudo rm -f /usr/share/icons/hicolor/48x48/apps/" + bin + ".png ; "
             "sudo rm -f /usr/share/icons/hicolor/64x64/apps/" + bin + ".png ; "
             "sudo rm -f /usr/share/icons/hicolor/128x128/apps/" + bin + ".png ; "
             "sudo rm -f /usr/share/icons/hicolor/256x256/apps/" + bin + ".png ; "
             "sudo rm -f /usr/share/icons/hicolor/scalable/apps/" + bin + ".svg ; "
             "sudo rm -f /usr/share/pixmaps/" + bin + ".png ; "
             "rm -f ~/.local/share/icons/hicolor/48x48/apps/" + bin + ".png ; "
             "rm -f ~/.local/share/icons/hicolor/64x64/apps/" + bin + ".png ; "
             "rm -f ~/.local/share/icons/hicolor/128x128/apps/" + bin + ".png ; "
             "rm -f ~/.local/share/icons/hicolor/256x256/apps/" + bin + ".png ; "
             "rm -f ~/.local/share/icons/hicolor/scalable/apps/" + bin + ".svg ; "
             "sudo rm -f /usr/share/icons/hicolor/icon-theme.cache ; "
             "rm -f ~/.local/share/icons/hicolor/icon-theme.cache ; "
             "rm -rf ~/.cache/icon-cache.kcache ~/.cache/thumbnails ~/.cache/gnome-shell ; "
             "echo 'Old icons purged' 2>&1", 86},

            // ── NEW: download fresh icon from GitHub ──
            {"Downloading latest icon from GitHub",
             "set +e ; "
             "URL_MAIN='https://raw.githubusercontent.com/" + user + "/" + repo + "/main/" + bin + ".svg' ; "
             "URL_MASTER='https://raw.githubusercontent.com/" + user + "/" + repo + "/master/" + bin + ".svg' ; "
             "DEST=/usr/share/icons/hicolor/scalable/apps/" + bin + ".svg ; "
             "sudo mkdir -p /usr/share/icons/hicolor/scalable/apps ; "
             "echo 'Trying main branch...' ; "
             "if sudo curl -fsSL \"$URL_MAIN\" -o \"$DEST\" && [ -s \"$DEST\" ] ; then "
             "    echo 'Downloaded from main branch' ; "
             "elif sudo curl -fsSL \"$URL_MASTER\" -o \"$DEST\" && [ -s \"$DEST\" ] ; then "
             "    echo 'Downloaded from master branch' ; "
             "else "
             "    echo 'Download failed - writing fallback' ; "
             "    printf '%s\\n' '<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 128 128\"><rect width=\"128\" height=\"128\" rx=\"26\" fill=\"#4f83e0\"/><path d=\"M32 44 h64 v12 h-26 v36 h-12 v-36 h-26 z\" fill=\"#ffffff\"/></svg>' | sudo tee \"$DEST\" > /dev/null ; "
             "fi ; "
             "sudo chmod 644 \"$DEST\" ; "
             "sudo chown root:root \"$DEST\" ; "
             "echo '--- First 3 lines of installed SVG ---' ; "
             "head -3 \"$DEST\" ; "
             "echo '--- File size ---' ; "
             "ls -la \"$DEST\" 2>&1", 90},

            // ── NEW: render PNG sizes + refresh caches ──
            {"Rendering icon sizes and refreshing caches",
             "set +e ; "
             "SVG=/usr/share/icons/hicolor/scalable/apps/" + bin + ".svg ; "
             "for SIZE in 48 64 128 256 ; do "
             "    DIR=/usr/share/icons/hicolor/${SIZE}x${SIZE}/apps ; "
             "    sudo mkdir -p \"$DIR\" ; "
             "    if command -v rsvg-convert > /dev/null 2>&1 ; then "
             "        sudo rsvg-convert -w $SIZE -h $SIZE \"$SVG\" -o \"$DIR/" + bin + ".png\" 2>/dev/null && echo \"Generated ${SIZE}x${SIZE} PNG (rsvg)\" ; "
             "    elif command -v inkscape > /dev/null 2>&1 ; then "
             "        sudo inkscape \"$SVG\" --export-type=png --export-filename=\"$DIR/" + bin + ".png\" --export-width=$SIZE --export-height=$SIZE 2>/dev/null && echo \"Generated ${SIZE}x${SIZE} PNG (inkscape)\" ; "
             "    elif command -v convert > /dev/null 2>&1 ; then "
             "        sudo convert -background none -resize ${SIZE}x${SIZE} \"$SVG\" \"$DIR/" + bin + ".png\" 2>/dev/null && echo \"Generated ${SIZE}x${SIZE} PNG (imagemagick)\" ; "
             "    else "
             "        echo 'No SVG renderer found - SVG only' ; "
             "        break ; "
             "    fi ; "
             "    sudo chmod 644 \"$DIR/" + bin + ".png\" 2>/dev/null ; "
             "    sudo chown root:root \"$DIR/" + bin + ".png\" 2>/dev/null ; "
             "done ; "
             "sudo cp -f \"$SVG\" /usr/share/pixmaps/" + bin + ".svg 2>/dev/null ; "
             "sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null ; "
             "sudo update-desktop-database /usr/share/applications 2>/dev/null ; "
             "echo 'Caches refreshed' 2>&1", 95},

            {"Cleaning up",
             "rm -rf /tmp/rb-src 2>&1 ; echo Done", 100},
        };
        runStepChain(steps, 0);
    }

    void startInstallWindows() {
        QVector<Step> steps = {
            {"Checking winget", "where winget || echo not-found", 5},
            {"Checking Chocolatey", "where choco || echo not-found", 8},
            {"Installing Visual C++ runtime",
             "winget install --id Microsoft.VCRedist.2015+.x64 --silent "
             "--accept-package-agreements --accept-source-agreements", 20},
            {"Downloading source",
             "if exist \"%TEMP%\\rb-src\" rmdir /S /Q \"%TEMP%\\rb-src\" ; "
             "git clone --depth=1 " + QString(kGitHubUrl) + " \"%TEMP%\\rb-src\"", 45},
            {"Compiling",
             "cd /d \"%TEMP%\\rb-src\" && "
             "where cl && cl /std:c++17 /EHsc /O2 /Fegraphite.exe *.cpp "
             "/I\"%QTDIR%\\include\" "
             "/link /LIBPATH:\"%QTDIR%\\lib\" Qt6Widgets.lib Qt6WebEngineWidgets.lib "
             "|| echo (MSVC toolchain not detected)", 70},
            {"Installing",
             "mkdir \"%ProgramData%\\RootBrowser\" 2>nul ; "
             "copy /Y \"%TEMP%\\rb-src\\rootbrowser.exe\" "
             "\"%ProgramData%\\RootBrowser\\\" 2>nul || echo done", 82},
            {"Adding to PATH",
             "setx PATH \"%PATH%;%ProgramData%\\RootBrowser\" /M", 86},
            {"Start Menu shortcut",
             "powershell -Command \"$s=(New-Object -COM WScript.Shell)."
             "CreateShortcut('%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\RootBrowser.lnk');"
             "$s.TargetPath='%ProgramData%\\RootBrowser\\rootbrowser.exe';$s.Save()\"", 92},
            {"Cleaning up",
             "rmdir /S /Q \"%TEMP%\\rb-src\" 2>nul ; echo Done", 100},
        };
        runStepChain(steps, 0);
    }

    void runStepChain(const QVector<Step>& steps, int idx) {
        if (idx >= steps.size()) {
            running_ = false;
            onInstallSuccess();
            return;
        }
        const Step& step = steps[idx];
        setStepTitle(QString("Step %1 of %2   %3")
                     .arg(idx + 1).arg(steps.size()).arg(step.title));
        appendTerm("", Col::textPrimary);
        appendTerm(QString("  %1   [%2/%3]")
                    .arg(step.title).arg(idx + 1).arg(steps.size()), Col::accent);

        proc_ = new QProcess(this);
        proc_->setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("DEBIAN_FRONTEND", "noninteractive");
        env.insert("LANG", "C.UTF-8");
        proc_->setProcessEnvironment(env);

        connect(proc_, &QProcess::readyReadStandardOutput, this, [this]{
            const QByteArray d = proc_->readAllStandardOutput();
            const QString txt = QString::fromUtf8(d);
            for (const QString& ln : txt.split('\n')) {
                if (ln.trimmed().isEmpty()) continue;
                QColor c = Col::textMuted;
                if (ln.contains("Setting up") || ln.contains("Unpacking")
                    || ln.contains("Downloaded") || ln.contains("Generated")
                    || ln.contains("purged") || ln.contains("refreshed")
                    || ln.contains("done"))
                    c = Col::textPrimary;
                else if (ln.contains("error", Qt::CaseInsensitive)
                         || ln.contains("E:") || ln.contains("failed", Qt::CaseInsensitive)
                         || ln.contains("Download failed"))
                    c = Col::danger;
                else if (ln.contains("warning", Qt::CaseInsensitive) || ln.contains("W:"))
                    c = Col::warning;
                appendTerm("    " + ln, c);
            }
        });

        connect(proc_,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this, idx, step, steps](int code, QProcess::ExitStatus st){
            proc_->deleteLater();
            proc_ = nullptr;
            if (code == 0 && st == QProcess::NormalExit) {
                appendTerm(QString("    done  %1").arg(step.title), Col::success);
                if (progress_) progress_->setValue(step.progress);
                setStatus("Completed · " + step.title, Col::success);
                QTimer::singleShot(140, this, [this, idx, steps]{
                    runStepChain(steps, idx + 1);
                });
            } else {
                appendTerm(QString("    failed  %1 (exit %2)").arg(step.title).arg(code),
                           Col::danger);
                setStatus("Failed · " + step.title, Col::danger);
                running_ = false;
                if (failedLog_ && terminal_)
                    failedLog_->setPlainText(terminal_->toPlainText());
                stack_->setCurrentIndex(5);
            }
        });

        appendTerm(QString("  $ %1").arg(
                       step.script.left(160) +
                       (step.script.length() > 160 ? "..." : "")), Col::textFaint);

        if (kIsWindows) proc_->start("cmd.exe", { "/C", step.script });
        else            proc_->start("/bin/sh", { "-c", step.script });
    }

    void onInstallSuccess() {
        running_ = false;
        stack_->setCurrentIndex(4);
        if (selectedOs_ == OS::Windows) {
            doneInfoCard_->setText(QString(
                "<div style='color:%1;font-size:12px;line-height:1.85'>"
                "<b style='color:%2'>How to launch</b><br>"
                "&nbsp;&nbsp;·&nbsp; Start Menu &mdash; search <b>RootBrowser</b><br>"
                "&nbsp;&nbsp;·&nbsp; Direct &mdash; "
                "<span style='font-family:monospace;background:%3;padding:2px 8px;"
                "border-radius:4px;'>C:\\ProgramData\\RootBrowser\\rootbrowser.exe</span>"
                "</div>")
                .arg(Col::textPrimary.name(), Col::textBright.name(),
                     Col::surfaceHi.name()));
        } else {
            doneInfoCard_->setText(QString(
                "<div style='color:%1;font-size:12px;line-height:1.85'>"
                "<b style='color:%2'>How to launch</b><br>"
                "&nbsp;&nbsp;·&nbsp; Terminal &mdash; "
                "<span style='font-family:monospace;background:%3;padding:2px 8px;"
                "border-radius:4px;'>rootbrowser</span><br>"
                "&nbsp;&nbsp;·&nbsp; App menu &mdash; search <b>RootBrowser</b><br>"
                "&nbsp;&nbsp;·&nbsp; Direct &mdash; "
                "<span style='font-family:monospace;background:%3;padding:2px 8px;"
                "border-radius:4px;'>rootbrowser https://example.com</span>"
                "</div>")
                .arg(Col::textPrimary.name(), Col::textBright.name(),
                     Col::surfaceHi.name()));
        }
    }

    // ── Success / Error icons ───────────────────────────────────────────
    static QPixmap makeSuccessIcon(int size) {
        QPixmap pm(160, 160);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Col::success);
        p.drawEllipse(QRectF(0, 0, 160, 160));
        p.setPen(QPen(Qt::white, 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(48, 82, 70, 106);
        p.drawLine(70, 106, 114, 56);
        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    static QPixmap makeErrorIcon(int size) {
        QPixmap pm(160, 160);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Col::danger);
        p.drawEllipse(QRectF(0, 0, 160, 160));
        p.setPen(QPen(Qt::white, 12, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(56, 56, 104, 104);
        p.drawLine(104, 56, 56, 104);
        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // ── Members ─────────────────────────────────────────────────────────
    QStackedWidget* stack_ = nullptr;
    Background*     bg_ = nullptr;
    WindowControls* controls_ = nullptr;
    enum class OS { Linux, Windows };
    OS selectedOs_ = kIsWindows ? OS::Windows : OS::Linux;
    OSChoiceCard* linuxCard_ = nullptr;
    OSChoiceCard* winCard_ = nullptr;
    QLabel* warnLabel_ = nullptr;
    QLabel* depsSubLabel_ = nullptr;
    QLabel* depsInfoLabel_ = nullptr;
    QWidget* depsListWidget_ = nullptr;
    QVBoxLayout* depsListLayout_ = nullptr;
    QPlainTextEdit* terminal_ = nullptr;
    QProgressBar*   progress_ = nullptr;
    QLabel*         statusLabel_ = nullptr;
    QLabel*         stepLabel_ = nullptr;
    QLabel*     doneTitle_ = nullptr;
    QLabel*     doneInfoCard_ = nullptr;
    Button*     doneLaunchBtn_ = nullptr;
    QPlainTextEdit* failedLog_ = nullptr;
    QProcess* proc_ = nullptr;
    bool running_ = false;
    QPoint dragPos_;
    bool dragging_ = false;
};

// ============================================================================
//  main
// ============================================================================
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(kAppName);
    app.setStyle("Fusion");

    QFont f = app.font();
    f.setFamilies({"Inter", "Segoe UI Variable Text", "Segoe UI", "Ubuntu",
                   "Noto Sans", "Cantarell", "DejaVu Sans"});
    f.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(f);

    QPalette pal;
    pal.setColor(QPalette::Window,          Col::bg);
    pal.setColor(QPalette::WindowText,      Col::textPrimary);
    pal.setColor(QPalette::Base,            Col::surface);
    pal.setColor(QPalette::Text,            Col::textPrimary);
    pal.setColor(QPalette::Button,          Col::surface);
    pal.setColor(QPalette::ButtonText,      Col::textPrimary);
    pal.setColor(QPalette::Highlight,       Col::accent);
    pal.setColor(QPalette::HighlightedText, Col::textBright);
    app.setPalette(pal);

    app.setStyleSheet(
        "QToolTip{background:#1d2028;color:#e6e8ee;border:1px solid #242832;"
        "padding:6px 10px;border-radius:6px;}");

    Installer w;
    w.show();
    return app.exec();
}
