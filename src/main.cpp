// ============================================================================
//  RootBrowser — main.cpp (bookmarks + downloads + download panel)
//
//  Build:
//    g++ -std=c++17 -O2 -fPIC \
//        main.cpp connector.cpp viewpagesource.cpp webadblocker.cpp \
//        bookmarkstore.cpp bookmarkpage.cpp \
//        downloadmanager.cpp downloadpage.cpp downloadpanel.cpp \
//        -o graphite \
//        $(pkg-config --cflags --libs Qt6WebEngineWidgets Qt6Widgets)
//    ./graphite
// ============================================================================

#include "connector.h"
#include "webadblocker.h"
#include "bookmarkstore.h"
#include "historystore.h"
#include "settingsstore.h"
#include "torcontroller.h"
#include "privatemodepage.h"
#include "privatebrowser.h"
#include "privatehomepage.h"
#include "settingspage.h"
#include "historypage.h"
#include "bookmarkpage.h"
#include "downloadmanager.h"
#include "downloadpage.h"
#include "downloadpanel.h"
#include "downloadnotification.h"
#include "downloadtoast.h"
#include "findinpage.h"
#include "commandpalette.h"
#include "httpsonly.h"
#include "fingerprintprotection.h"
#include "shortcuthelp.h"
#include "urlautocomplete.h"
#include "bookmarksbar.h"
#include "background.h"

#include <QApplication>
#include <QDebug>
#include <QNetworkProxy>
#include <QWidget>
#include <QAbstractButton>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QContextMenuEvent>
#include <QChildEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QGraphicsDropShadowEffect>
#include <QElapsedTimer>
#include <QDateTime>
#include <QLocale>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QPalette>
#include <QWindow>
#include <QImage>
#include <QStandardPaths>
#include <QIcon>
#include <QPixmap>
#include <QClipboard>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QFileDialog>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <functional>
#include <algorithm>
#include <cmath>

static const char* kAppName = "RootBrowser";
static const double kPi = 3.14159265358979323846;

// ============================================================================
//  YouTube ad neutralization script
// ============================================================================
static const char* kYouTubeAdNeutralizer = R"JS(
(function() {
    'use strict';
    const AD_KEYS = [
        'adPlacements', 'playerAds', 'adSlots', 'adBreakHeartbeatParams',
        'adThrottled', 'adSafetyReason', 'importantForAds', 'adParams'
    ];
    function stripAds(obj) {
        if (!obj || typeof obj !== 'object') return obj;
        for (const k of AD_KEYS) { try { delete obj[k]; } catch (e) {} }
        if (obj.playerResponse) stripAds(obj.playerResponse);
        return obj;
    }
    let _ytInitialPlayerResponse = undefined;
    try {
        Object.defineProperty(window, 'ytInitialPlayerResponse', {
            configurable: true,
            get() { return _ytInitialPlayerResponse; },
            set(v) { _ytInitialPlayerResponse = stripAds(v); }
        });
    } catch (e) {}
    const origFetch = window.fetch;
    if (origFetch) {
        window.fetch = function(input, init) {
            const url = (typeof input === 'string') ? input
                       : (input && input.url) ? input.url : '';
            const p = origFetch.apply(this, arguments);
            if (url.indexOf('/youtubei/v1/player') === -1) return p;
            return p.then(resp => resp.clone().json().then(data => {
                stripAds(data);
                return new Response(JSON.stringify(data), {
                    status: resp.status, statusText: resp.statusText,
                    headers: resp.headers
                });
            }).catch(() => resp));
        };
    }
    const skipObserver = new MutationObserver(() => {
        const skip = document.querySelector('.ytp-ad-skip-button, .ytp-skip-ad-button');
        if (skip) { try { skip.click(); } catch (e) {} }
        const overlay = document.querySelector('.ytp-ad-overlay-close-button');
        if (overlay) { try { overlay.click(); } catch (e) {} }
    });
    try {
        skipObserver.observe(document.documentElement, { childList: true, subtree: true });
    } catch (e) {}
})();
)JS";

// ============================================================================
//  FaviconLoader
// ============================================================================
class FaviconLoader : public QObject {
public:
    static FaviconLoader& instance() { static FaviconLoader f; return f; }

    QPixmap get(const QString& pageUrl, std::function<void(const QPixmap&)> ready = {}) {
        const QString key = cacheKey(pageUrl);
        if (key.isEmpty()) return QPixmap();
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            if (ready && !it->isNull()) ready(*it);
            return *it;
        }
        if (pending_.contains(key)) {
            if (ready) callbacks_[key].append(ready);
            return QPixmap();
        }
        pending_.insert(key);
        callbacks_[key].clear();
        if (ready) callbacks_[key].append(ready);
        if (loadFromDisk(key)) return QPixmap();
        fetch(key, 1);
        return QPixmap();
    }

private:
    FaviconLoader() { nam_ = new QNetworkAccessManager(this); }

    static QString cacheKey(const QString& pageUrl) {
        QUrl u = QUrl::fromUserInput(pageUrl);
        QString host = u.host().toLower();
        if (host.startsWith("www.")) host = host.mid(4);
        return host;
    }
    static QString diskPath(const QString& key) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/favicons";
        QDir().mkpath(dir);
        return dir + "/" + key + ".png";
    }
    bool loadFromDisk(const QString& key) {
        const QString path = diskPath(key);
        if (!QFile::exists(path)) return false;
        QPixmap pm;
        if (!pm.load(path) || pm.isNull()) return false;
        cache_.insert(key, pm);
        pending_.remove(key);
        notify(key, pm);
        return true;
    }
    void saveToDisk(const QString& key, const QPixmap& pm) { pm.save(diskPath(key), "PNG"); }

    QString sourceUrl(int n, const QString& key) const {
        switch (n) {
        case 1: return QString("https://%1/favicon.ico").arg(key);
        case 2: return QString("https://icons.duckduckgo.com/ip3/%1.ico").arg(key);
        case 3: return QString("https://www.google.com/s2/favicons?domain=%1&sz=64").arg(key);
        default: return QString();
        }
    }

    void fetch(const QString& key, int source) {
        const QString url = sourceUrl(source, key);
        if (url.isEmpty()) { pending_.remove(key); callbacks_[key].clear(); return; }
        QNetworkRequest req{QUrl(url)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (X11; Linux x86_64) RootBrowser/1.0");
        req.setTransferTimeout(6000);
        QNetworkReply* r = nam_->get(req);
        QObject::connect(r, &QNetworkReply::finished, this, [this, r, key, source]() {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) { fetch(key, source + 1); return; }
            const QByteArray data = r->readAll();
            QPixmap pm;
            if (!pm.loadFromData(data) || pm.isNull() || pm.width() < 8) {
                fetch(key, source + 1); return;
            }
            QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
            bool hasContent = false;
            for (int y = 0; y < img.height() && !hasContent; y += 2)
                for (int x = 0; x < img.width(); x += 2)
                    if (qAlpha(img.pixel(x, y)) > 30) { hasContent = true; break; }
            if (!hasContent) { fetch(key, source + 1); return; }
            if (pm.width() < 32)
                pm = pm.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            cache_.insert(key, pm);
            saveToDisk(key, pm);
            pending_.remove(key);
            notify(key, pm);
        });
    }
    void notify(const QString& key, const QPixmap& pm) {
        if (!callbacks_.contains(key)) return;
        for (auto& cb : callbacks_[key]) if (cb) cb(pm);
        callbacks_[key].clear();
    }
    QNetworkAccessManager* nam_ = nullptr;
    QHash<QString, QPixmap> cache_;
    QHash<QString, QList<std::function<void(const QPixmap&)>>> callbacks_;
    QSet<QString> pending_;
};

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
static QColor lerpColor(const QColor& a, const QColor& b, qreal t) {
    t = std::clamp<qreal>(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

struct Engine { const char* name; const char* url; };
static const Engine kEngines[] = {
    {"Google",     "https://www.google.com/search?q=%1"},
    {"DuckDuckGo", "https://duckduckgo.com/?q=%1"},
    {"Bing",       "https://www.bing.com/search?q=%1"},
    {"Brave",      "https://search.brave.com/search?q=%1"},
};
static const int kEngineCount = 4;
static int g_engine = 0;

static QString normalizeInput(QString s) {
    s = s.trimmed();
    if (s.isEmpty()) return QString();

    if (s.startsWith("rootbrowser://", Qt::CaseInsensitive)
        || s.startsWith("rootbrowser:", Qt::CaseInsensitive)
        || s.startsWith("rootbrowser-goto:", Qt::CaseInsensitive)
        || s.startsWith("rootbrowser-bookmark-del:", Qt::CaseInsensitive)
        || s.startsWith("rootbrowser-dl-", Qt::CaseInsensitive))
        return s;

    if (s.contains("://") || s.startsWith("about:") || s.startsWith("file:")) return s;
    const bool hasSpace = s.contains(' ');
    if (!hasSpace && (s.startsWith("localhost") || s.startsWith("127.0.0.1")))
        return "http://" + s;
    if (!hasSpace && s.contains('.'))
        return "https://" + s;
    return QString(kEngines[g_engine].url)
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(s)));
}

static QString titleFor(const QString& u) {
    if (u.isEmpty()) return "New Tab";
    QUrl q(u);
    QUrlQuery qq(q);
    const QString term = qq.queryItemValue("q", QUrl::FullyDecoded);
    if (!term.isEmpty()) return term;
    QString h = q.host();
    if (h.startsWith("www.")) h = h.mid(4);
    return h.isEmpty() ? u : h;
}

// ----------------------------------------------------------------------------
//  Toolbar icons
// ----------------------------------------------------------------------------
enum class Ic { Back, Forward, Reload, Home, Menu, Plus, Close, Search, Full, Exit };

static void drawIcon(QPainter& p, Ic k, const QRectF& r, const QColor& col) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = r.width();
    auto P = [&](qreal x, qreal y) { return QPointF(r.x() + x * s, r.y() + y * s); };
    p.setPen(QPen(col, std::max<qreal>(1.5, s * 0.085),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (k) {
    case Ic::Back: {
        p.drawLine(P(.82, .5), P(.2, .5));
        QPointF pts[3] = {P(.48, .24), P(.2, .5), P(.48, .76)};
        p.drawPolyline(pts, 3);
        break;
    }
    case Ic::Forward: {
        p.drawLine(P(.18, .5), P(.8, .5));
        QPointF pts[3] = {P(.52, .24), P(.8, .5), P(.52, .76)};
        p.drawPolyline(pts, 3);
        break;
    }
    case Ic::Reload: {
        QRectF a(r.x() + .2 * s, r.y() + .2 * s, .6 * s, .6 * s);
        p.drawArc(a, 110 * 16, 290 * 16);
        const qreal th = 40.0 * kPi / 180.0, rad = .3 * s;
        const QPointF c = P(.5, .5);
        const QPointF e(c.x() + rad * std::cos(th), c.y() - rad * std::sin(th));
        const QPointF t(-std::sin(th), -std::cos(th));
        const QPointF tip(e.x() + t.x() * .08 * s, e.y() + t.y() * .08 * s);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const qreal ph = sgn * 40.0 * kPi / 180.0;
            const QPointF b(-t.x(), -t.y());
            const QPointF w(b.x() * std::cos(ph) - b.y() * std::sin(ph),
                            b.x() * std::sin(ph) + b.y() * std::cos(ph));
            p.drawLine(tip, QPointF(tip.x() + w.x() * .22 * s, tip.y() + w.y() * .22 * s));
        }
        break;
    }
    case Ic::Home: {
        QPointF roof[3] = {P(.16, .52), P(.5, .2), P(.84, .52)};
        p.drawPolyline(roof, 3);
        QPointF body[4] = {P(.27, .45), P(.27, .8), P(.73, .8), P(.73, .45)};
        p.drawPolyline(body, 4);
        break;
    }
    case Ic::Menu: {
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        const qreal d = s * .075;
        p.drawEllipse(P(.5, .2), d, d);
        p.drawEllipse(P(.5, .5), d, d);
        p.drawEllipse(P(.5, .8), d, d);
        break;
    }
    case Ic::Plus:
        p.drawLine(P(.5, .22), P(.5, .78));
        p.drawLine(P(.22, .5), P(.78, .5));
        break;
    case Ic::Close:
        p.drawLine(P(.28, .28), P(.72, .72));
        p.drawLine(P(.72, .28), P(.28, .72));
        break;
    case Ic::Search:
        p.drawEllipse(P(.45, .45), .24 * s, .24 * s);
        p.drawLine(P(.63, .63), P(.82, .82));
        break;
    case Ic::Full: {
        QPointF a[3] = {P(.2, .38), P(.2, .2), P(.38, .2)};
        QPointF b[3] = {P(.62, .2), P(.8, .2), P(.8, .38)};
        QPointF c[3] = {P(.2, .62), P(.2, .8), P(.38, .8)};
        QPointF d[3] = {P(.62, .8), P(.8, .8), P(.8, .62)};
        p.drawPolyline(a, 3); p.drawPolyline(b, 3); p.drawPolyline(c, 3); p.drawPolyline(d, 3);
        break;
    }
    case Ic::Exit: {
        QPointF a[3] = {P(.38, .2), P(.38, .38), P(.2, .38)};
        QPointF b[3] = {P(.62, .2), P(.62, .38), P(.8, .38)};
        QPointF c[3] = {P(.38, .8), P(.38, .62), P(.2, .62)};
        QPointF d[3] = {P(.62, .8), P(.62, .62), P(.8, .62)};
        p.drawPolyline(a, 3); p.drawPolyline(b, 3); p.drawPolyline(c, 3); p.drawPolyline(d, 3);
        break;
    }
    }
    p.restore();
}

// ----------------------------------------------------------------------------
//  Small building blocks
// ----------------------------------------------------------------------------
class IconButton : public QAbstractButton {
public:
    explicit IconButton(Ic k, QWidget* parent = nullptr, int box = 36)
        : QAbstractButton(parent), kind_(k), box_(box) {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(box, box);
    }
    void setKind(Ic k) { kind_ = k; update(); }
protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::HoverEnter) { hover_ = true; update(); }
        else if (e->type() == QEvent::HoverLeave) { hover_ = false; update(); }
        return QAbstractButton::event(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool en = isEnabled();

        if (en && (hover_ || isDown())) {
            // Purple-blue glow
            QRadialGradient glow(rect().center(), width() * 0.75);
            glow.setColorAt(0.0, QColor(100, 130, 200, 70));
            glow.setColorAt(1.0, QColor(100, 130, 200, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(rect().adjusted(-4, -4, 4, 4));

            // Pill background
            p.setPen(QPen(QColor(120, 150, 220, 60), 1));
            p.setBrush(QColor(60, 80, 140, isDown() ? 100 : 55));
            p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 9, 9);
        }

        QColor c = !en ? QColor(255, 255, 255, 48)
                       : (hover_ ? QColor("#e8eeff") : QColor("#a8afc4"));
        const qreal ic = box_ * 0.55;
        drawIcon(p, kind_, QRectF((width() - ic) / 2.0, (height() - ic) / 2.0, ic, ic), c);
    }
private:
    Ic kind_;
    int box_;
    bool hover_ = false;
};

class Panel : public QWidget {
public:
    Panel(const QColor& c, bool bottomLine, QWidget* parent = nullptr)
        : QWidget(parent), c_(c), line_(bottomLine) {}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Semi-transparent background — lets wallpaper hint through
        QColor bg = c_;
        bg.setAlpha(235);
        p.fillRect(rect(), bg);

        // Subtle gradient tint (purple-blue for night theme)
        QLinearGradient tint(0, 0, width(), 0);
        tint.setColorAt(0.0, QColor(80, 100, 180, 8));
        tint.setColorAt(0.5, QColor(100, 80, 160, 6));
        tint.setColorAt(1.0, QColor(60, 90, 160, 10));
        p.fillRect(rect(), tint);

        // Top highlight line
        p.setPen(QPen(QColor(255, 255, 255, 10), 1));
        p.drawLine(0, 0, width(), 0);

        // Bottom subtle border
        if (line_) {
            QLinearGradient border(0, 0, width(), 0);
            border.setColorAt(0.0, QColor(60, 70, 100, 0));
            border.setColorAt(0.5, QColor(60, 70, 100, 120));
            border.setColorAt(1.0, QColor(60, 70, 100, 0));
            p.setPen(QPen(QBrush(border), 1));
            p.drawLine(0, height() - 1, width(), height() - 1);
        }
    }
private:
    QColor c_;
    bool line_;
};

class AddressEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
protected:
    void focusInEvent(QFocusEvent* e) override {
        QLineEdit::focusInEvent(e);
        if (e->reason() == Qt::MouseFocusReason)
            QTimer::singleShot(0, this, [this] { selectAll(); });
        else
            selectAll();
    }
};

// ----------------------------------------------------------------------------
//  Tab strip
// ----------------------------------------------------------------------------
class TabStrip : public QWidget {
public:
    std::function<void(int)> onCurrent, onClose;
    std::function<void()> onNew, onMin, onMaxToggle, onCloseWin, onDrag;

    explicit TabStrip(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(46);
        setMouseTracking(true);
    }
    int addTab(const QString& t) {
        titles_.push_back(t);
        icons_.push_back(QPixmap());
        privates_.push_back(false);
        update();
        return int(titles_.size()) - 1;
    }

    void setPrivate(int index, bool priv) {
        if (index < 0) return;
        while (privates_.size() <= index) privates_.push_back(false);
        privates_[index] = priv;
        update();
    }
    void removeTab(int i) {
        if (i < 0 || i >= titles_.size()) return;
        titles_.removeAt(i);
        icons_.removeAt(i);
        if (i < privates_.size()) privates_.removeAt(i);
        if (cur_ >= titles_.size()) cur_ = int(titles_.size()) - 1;
        hover_ = -1; hoverClose_ = false;
        update();
    }
    void setTitle(int i, const QString& t) { if (i >= 0 && i < titles_.size()) { titles_[i] = t; update(); } }
    void setIcon(int i, const QPixmap& pm) { if (i >= 0 && i < icons_.size()) { icons_[i] = pm; update(); } }
    void setNote(const QString& n) { note_ = n; update(); }
    void setMaximized(bool m) { maximized_ = m; update(); }
    void setCurrent(int i) { cur_ = i; update(); }
    int count() const { return int(titles_.size()); }

protected:
    static constexpr int kCtl = 124;

    qreal tabW() const {
        const int n = std::max<int>(1, int(titles_.size()));
        return std::clamp<qreal>((width() - 12 - 50 - kCtl - 30) / n, 84.0, 240.0);
    }
    QRectF tabRect(int i) const { return QRectF(10 + i * tabW(), 9, tabW() - 2, height() - 9); }
    QRectF closeRect(int i) const {
        QRectF r = tabRect(i);
        return QRectF(r.right() - 28, r.center().y() - 10, 20, 20);
    }
    bool closeVisible(int i) const { return i == cur_ || tabW() >= 112; }
    QRectF plusRect() const {
        qreal x = 10 + titles_.size() * tabW() + 4;
        x = std::min<qreal>(x, width() - kCtl - 40);
        return QRectF(x, 12, 30, 30);
    }
    QRectF ctlRect(int k) const {
        const qreal right = width() - 8 - (2 - k) * 38;
        return QRectF(right - 36, 8, 36, 30);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(10, 12, 18, 240));
        // Purple tint
        QLinearGradient tt(0, 0, 0, height());
        tt.setColorAt(0.0, QColor(60, 70, 130, 12));
        tt.setColorAt(1.0, QColor(40, 50, 100, 4));
        p.fillRect(rect(), tt);
        QFont f = font();
        f.setPixelSize(13);
        p.setFont(f);
        const QFontMetrics fm(f);

        for (int i = 0; i < titles_.size(); ++i) {
            const QRectF r = tabRect(i);
            const bool act = (i == cur_);
            p.setPen(Qt::NoPen);
            if (act) {
                QLinearGradient tg(r.topLeft(), r.bottomLeft());
                tg.setColorAt(0.0, QColor(40, 44, 58, 240));
                tg.setColorAt(1.0, QColor(26, 30, 42, 250));
                p.setPen(QPen(QColor(120, 150, 220, 90), 1));
                p.setBrush(tg);
                p.drawRoundedRect(r, 10, 10);
                // Purple-blue accent line at top
                p.setPen(QPen(QColor(120, 160, 240, 220), 2.0,
                              Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(r.left() + 14, r.top() + 1.2),
                           QPointF(r.right() - 14, r.top() + 1.2));
            } else if (i == hover_) {
                p.setBrush(QColor(255, 255, 255, 14));
                p.drawRoundedRect(r.adjusted(0, 3, 0, -6), 10, 10);
            }

            const QRectF fav(r.x() + 13, r.center().y() - 8, 17, 17);
            if (!icons_[i].isNull()) {
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                p.drawPixmap(fav.toRect(), icons_[i]);
            } else {
                QLinearGradient g(fav.topLeft(), fav.bottomRight());
                g.setColorAt(0, QColor("#99a1b3"));
                g.setColorAt(1, QColor("#4f5565"));
                p.setBrush(g);
                p.drawEllipse(fav);
                QFont ff = f; ff.setPixelSize(10); ff.setBold(true);
                p.setFont(ff);
                p.setPen(QColor("#0c0c0e"));
                p.drawText(fav, Qt::AlignCenter, titles_[i].left(1).toUpper());
                p.setFont(f);
            }

            const bool showClose = closeVisible(i);
            const qreal tx = fav.right() + 9;
            const qreal tw = r.right() - tx - (showClose ? 32 : 12);
            p.setPen(act ? QColor("#f1f2f5") : QColor("#9aa0ac"));
            p.drawText(QRectF(tx, r.y(), tw, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(titles_[i], Qt::ElideRight, int(tw)));

            if (showClose) {
                const QRectF cr = closeRect(i);
                if (i == hover_ && hoverClose_) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(255, 255, 255, 30));
                    p.drawEllipse(cr);
                }
                drawIcon(p, Ic::Close, cr.adjusted(4, 4, -4, -4),
                         (i == hover_ && hoverClose_) ? QColor("#ffffff") : QColor("#8d929e"));
            }
        }

        const QRectF pr = plusRect();
        if (hoverPlus_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 22));
            p.drawRoundedRect(pr, 10, 10);
        }
        drawIcon(p, Ic::Plus, pr.adjusted(7, 7, -7, -7),
                 hoverPlus_ ? QColor("#ffffff") : QColor("#9aa0ac"));

        for (int k = 0; k < 3; ++k) {
            const QRectF cr = ctlRect(k);
            const bool h = (hoverCtl_ == k);
            if (h) {
                p.setPen(Qt::NoPen);
                p.setBrush(k == 2 ? QColor(226, 70, 70) : QColor(255, 255, 255, 24));
                p.drawRoundedRect(cr, 9, 9);
            }
            p.setPen(QPen(h ? QColor("#ffffff") : QColor("#9aa0ac"), 1.4,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            const QPointF c = cr.center();
            if (k == 0) {
                p.drawLine(c + QPointF(-5, 0), c + QPointF(5, 0));
            } else if (k == 1) {
                if (maximized_) {
                    p.drawRoundedRect(QRectF(c.x() - 5, c.y() - 2, 7, 7), 1.5, 1.5);
                    p.drawLine(QPointF(c.x() - 2, c.y() - 5), QPointF(c.x() + 5, c.y() - 5));
                    p.drawLine(QPointF(c.x() + 5, c.y() - 5), QPointF(c.x() + 5, c.y() + 2));
                } else {
                    p.drawRoundedRect(QRectF(c.x() - 5, c.y() - 5, 10, 10), 2, 2);
                }
            } else {
                p.drawLine(c + QPointF(-4.5, -4.5), c + QPointF(4.5, 4.5));
                p.drawLine(c + QPointF(4.5, -4.5), c + QPointF(-4.5, 4.5));
            }
        }

        if (!note_.isEmpty()) {
            const qreal w = fm.horizontalAdvance(note_) + 30;
            const QRectF nr(width() - kCtl - w - 6, 10, w, 30);
            p.setPen(QPen(QColor(255, 255, 255, 24), 1));
            p.setBrush(QColor("#1f2025"));
            p.drawRoundedRect(nr, 15, 15);
            p.setPen(QColor("#d4d8e1"));
            p.drawText(nr, Qt::AlignCenter, note_);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const QPointF pos = e->position();
        int h = -1; bool hc = false;
        for (int i = 0; i < titles_.size(); ++i) {
            if (tabRect(i).contains(pos)) {
                h = i;
                hc = closeVisible(i) && closeRect(i).contains(pos);
                break;
            }
        }
        const bool hp = plusRect().contains(pos);
        int ctl = -1;
        for (int k = 0; k < 3; ++k) if (ctlRect(k).contains(pos)) ctl = k;
        if (h != hover_ || hc != hoverClose_ || hp != hoverPlus_ || ctl != hoverCtl_) {
            hover_ = h; hoverClose_ = hc; hoverPlus_ = hp; hoverCtl_ = ctl;
            setCursor((h >= 0 || hp || ctl >= 0) ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
    void leaveEvent(QEvent*) override {
        hover_ = -1; hoverClose_ = false; hoverPlus_ = false; hoverCtl_ = -1;
        update();
    }
    void mousePressEvent(QMouseEvent* e) override {
        const QPointF pos = e->position();
        if (e->button() == Qt::MiddleButton) {
            for (int i = 0; i < titles_.size(); ++i)
                if (tabRect(i).contains(pos)) { if (onClose) onClose(i); return; }
            return;
        }
        if (e->button() != Qt::LeftButton) return;
        for (int k = 0; k < 3; ++k) {
            if (ctlRect(k).contains(pos)) {
                if (k == 0 && onMin) onMin();
                else if (k == 1 && onMaxToggle) onMaxToggle();
                else if (k == 2 && onCloseWin) onCloseWin();
                return;
            }
        }
        for (int i = 0; i < titles_.size(); ++i) {
            if (tabRect(i).contains(pos)) {
                if (closeVisible(i) && closeRect(i).contains(pos)) {
                    if (onClose) onClose(i);
                } else if (i != cur_) {
                    cur_ = i; update();
                    if (onCurrent) onCurrent(i);
                }
                return;
            }
        }
        if (plusRect().contains(pos)) { if (onNew) onNew(); return; }
        if (onDrag) onDrag();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        const QPointF pos = e->position();
        for (int i = 0; i < titles_.size(); ++i) if (tabRect(i).contains(pos)) return;
        for (int k = 0; k < 3; ++k) if (ctlRect(k).contains(pos)) return;
        if (!plusRect().contains(pos) && onMaxToggle) onMaxToggle();
    }

private:
    QStringList titles_;
    QVector<bool> privates_;
    QVector<QPixmap> icons_;
    QString note_;
    int cur_ = 0, hover_ = -1, hoverCtl_ = -1;
    bool hoverClose_ = false, hoverPlus_ = false, maximized_ = false;
};

// ----------------------------------------------------------------------------
//  Home page widgets
// ----------------------------------------------------------------------------
class Tile : public QWidget {
public:
    std::function<void(const QString&)> onClick;

    Tile(const QString& label, const QString& url, const QColor& accent, QWidget* parent = nullptr)
        : QWidget(parent), label_(label), url_(url), accent_(accent) {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setMinimumSize(118, 108);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        anim_ = new QVariantAnimation(this);
        anim_->setDuration(150);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            hov_ = v.toReal(); update();
        });
        FaviconLoader::instance().get(url_, [this](const QPixmap& pm) {
            icon_ = pm;
            update();
        });
    }

protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::HoverEnter) animateTo(1.0);
        else if (e->type() == QEvent::HoverLeave) animateTo(0.0);
        return QWidget::event(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick)
            onClick(url_);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = QRectF(rect()).adjusted(2, 5, -2, -2);
        r.translate(0, -5.0 * hov_);

        p.setPen(QPen(QColor(255, 255, 255, int(20 + 40 * hov_)), 1));
        QColor baseTop(24, 28, 38, 180);
        QColor baseBot(16, 20, 28, 200);
        QColor hoverTop(32, 38, 52, 200);
        QColor hoverBot(22, 28, 38, 220);
        QLinearGradient tileG(r.topLeft(), r.bottomRight());
        tileG.setColorAt(0, lerpColor(baseTop, hoverTop, hov_));
        tileG.setColorAt(1, lerpColor(baseBot, hoverBot, hov_));
        p.setBrush(tileG);
        p.drawRoundedRect(r, 18, 18);

        const QRectF b(r.center().x() - 22, r.y() + 15, 44, 44);

        if (!icon_.isNull()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, int(8 + 10 * hov_)));
            p.drawRoundedRect(b, 12, 12);
            QPixmap scaled = icon_.scaled(int(b.width()) - 12, int(b.height()) - 12,
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            const QRectF target(b.center().x() - scaled.width() / 2.0,
                                b.center().y() - scaled.height() / 2.0,
                                scaled.width(), scaled.height());
            p.drawPixmap(target, scaled, QRectF(scaled.rect()));
        } else {
            QLinearGradient g(b.topLeft(), b.bottomRight());
            g.setColorAt(0, accent_.lighter(112));
            g.setColorAt(1, accent_.darker(160));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRoundedRect(b, 13, 13);
            QFont f = font();
            f.setPixelSize(19); f.setBold(true);
            p.setFont(f);
            p.setPen(QColor(255, 255, 255, 235));
            p.drawText(b, Qt::AlignCenter, label_.left(1).toUpper());
        }

        QFont f = font();
        f.setPixelSize(13); f.setBold(false);
        p.setFont(f);
        p.setPen(lerpColor(QColor("#aeb3be"), QColor("#ffffff"), hov_));
        p.drawText(QRectF(r.x(), b.bottom() + 9, r.width(), 22), Qt::AlignCenter, label_);
    }

private:
    void animateTo(qreal v) {
        anim_->stop();
        anim_->setStartValue(hov_);
        anim_->setEndValue(v);
        anim_->start();
    }
    QString label_, url_;
    QColor accent_;
    QPixmap icon_;
    qreal hov_ = 0.0;
    QVariantAnimation* anim_;
};

class SearchBox : public QWidget {
public:
    std::function<void(const QString&)> onSubmit;
    std::function<void()> onEngine;

    explicit SearchBox(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(68);
        auto* l = new QHBoxLayout(this);
        l->setContentsMargins(24, 0, 20, 0);
        l->setSpacing(10);

        eng_ = new QPushButton(this);
        eng_->setCursor(Qt::PointingHandCursor);
        eng_->setFocusPolicy(Qt::NoFocus);
        eng_->setFixedHeight(30);
        eng_->setToolTip("Change search engine");
        eng_->setStyleSheet(
            "QPushButton{background:rgba(255,255,255,0.06);border:none;border-radius:15px;"
            "padding:0 14px;color:#c5c9d2;font-size:12px;}"
            "QPushButton:hover{background:rgba(255,255,255,0.12);color:#ffffff;}");
        refreshEngine();

        edit_ = new QLineEdit(this);
        edit_->setPlaceholderText("Search the web or type a URL");
        edit_->setStyleSheet("QLineEdit{background:transparent;border:none;}");
        QFont f = edit_->font();
        f.setPixelSize(17);
        edit_->setFont(f);
        QPalette pal = edit_->palette();
        pal.setColor(QPalette::Text, QColor("#f2f3f6"));
        pal.setColor(QPalette::PlaceholderText, QColor(160, 166, 182, 140));
        pal.setColor(QPalette::Highlight, QColor("#59627a"));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        edit_->setPalette(pal);
        edit_->installEventFilter(this);

        go_ = new IconButton(Ic::Search, this, 40);

        l->addWidget(eng_);
        l->addWidget(edit_, 1);
        l->addWidget(go_);

        QObject::connect(edit_, &QLineEdit::returnPressed, this, [this] { submit(); });
        QObject::connect(go_, &QAbstractButton::clicked, this, [this] { submit(); });
        QObject::connect(eng_, &QAbstractButton::clicked, this, [this] {
            g_engine = (g_engine + 1) % kEngineCount;
            refreshEngine();
            if (onEngine) onEngine();
        });

        anim_ = new QVariantAnimation(this);
        anim_->setDuration(180);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            glow_ = v.toReal(); update();
        });
    }

    void refreshEngine() { eng_->setText(kEngines[g_engine].name); }
    void clear() { edit_->clear(); }
    void focusEdit() { edit_->setFocus(); }

    QLineEdit* lineEdit() const { return edit_; }

protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (o == edit_) {
            if (e->type() == QEvent::FocusIn) animateTo(1.0);
            else if (e->type() == QEvent::FocusOut) animateTo(0.0);
        }
        return QWidget::eventFilter(o, e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(8, 8, -8, -8);
        const qreal rad = r.height() / 2.0;
        p.setBrush(Qt::NoBrush);
        if (glow_ > 0.01) {
            for (int i = 4; i >= 1; --i) {
                p.setPen(QPen(QColor(120, 160, 240, int(30.0 * glow_ / i)), i * 2.5));
                p.drawRoundedRect(r, rad, rad);
            }
        }
        p.setPen(QPen(lerpColor(QColor("#4a4e5a"), QColor("#8a94ac"), glow_), 1.2));
        p.setBrush(lerpColor(QColor(20, 24, 32, 220), QColor(15, 18, 25, 240), glow_));
        p.drawRoundedRect(r, rad, rad);
    }

private:
    void animateTo(qreal v) {
        anim_->stop();
        anim_->setStartValue(glow_);
        anim_->setEndValue(v);
        anim_->start();
    }
    void submit() {
        const QString t = edit_->text().trimmed();
        if (!t.isEmpty() && onSubmit) onSubmit(t);
    }
    QPushButton* eng_;
    QLineEdit* edit_;
    IconButton* go_;
    QVariantAnimation* anim_;
    qreal glow_ = 0.0;
};

class HomePage : public QWidget {
public:
    std::function<void(const QString&)> onNavigate;
    std::function<void(const QPoint&)> onContextMenu;

    explicit HomePage(QWidget* parent = nullptr) : QWidget(parent) {
        clk_.start();

        clock_ = new QLabel(this);
        clock_->setAlignment(Qt::AlignCenter);
        setLabelColor(clock_, QColor("#f4f5f7"));

        // Subtle drop shadow for clock (better legibility on any wallpaper)
        {
            auto* shadow = new QGraphicsDropShadowEffect(clock_);
            shadow->setBlurRadius(24);
            shadow->setOffset(0, 2);
            shadow->setColor(QColor(0, 0, 0, 120));
            clock_->setGraphicsEffect(shadow);
        }


        greet_ = new QLabel(this);
        greet_->setAlignment(Qt::AlignCenter);
        setLabelColor(greet_, QColor("#8b909d"));

        // Greeting shadow
        {
            auto* shadow = new QGraphicsDropShadowEffect(greet_);
            shadow->setBlurRadius(16);
            shadow->setOffset(0, 1);
            shadow->setColor(QColor(0, 0, 0, 140));
            greet_->setGraphicsEffect(shadow);
        }


        hint_ = new QLabel("Ctrl+T  New tab      Ctrl+L  Address bar      F11  Full screen", this);
        hint_->setAlignment(Qt::AlignCenter);
        setLabelColor(hint_, QColor("#5b5f6a"));
        QFont hf = hint_->font(); hf.setPixelSize(12); hint_->setFont(hf);

        search_ = new SearchBox(this);
        search_->setMaximumWidth(700);
        search_->onSubmit = [this](const QString& t) { if (onNavigate) onNavigate(t); };

        // ── URL autocomplete for home search bar
        homeAutocomplete_ = new UrlAutocomplete(this);
        homeAutocomplete_->setTheme(Theme::HomePage);
        homeAutocomplete_->attach(search_->lineEdit());

        // Change submit behavior — use autocomplete when visible
        search_->onSubmit = [this](const QString& t) {
            if (homeAutocomplete_ && homeAutocomplete_->isVisible()) {
                const QString sel = homeAutocomplete_->selectedUrl();
                if (!sel.isEmpty()) {
                    homeAutocomplete_->hideDropdown();
                    if (onNavigate) onNavigate(sel);
                    return;
                }
            }
            if (onNavigate) onNavigate(t);
        };

        auto* grid = new QWidget(this);
        grid->setMaximumWidth(800);
        auto* g = new QGridLayout(grid);
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(14);
        g->setVerticalSpacing(14);
        struct T { const char* n; const char* u; const char* c; };
        static const T tiles[] = {
            {"GitHub",         "https://github.com",               "#6e7681"},
            {"YouTube",        "https://www.youtube.com",          "#e0443b"},
            {"Wikipedia",      "https://www.wikipedia.org",        "#8e949f"},
            {"Reddit",         "https://www.reddit.com",           "#e8602c"},
            {"Stack Overflow", "https://stackoverflow.com",        "#e07a1f"},
            {"Hacker News",    "https://news.ycombinator.com",     "#ee7418"},
            {"Gmail",          "https://mail.google.com",          "#d9483b"},
            {"Maps",           "https://maps.google.com",          "#2f9e5b"},
        };
        for (int i = 0; i < 8; ++i) {
            auto* t = new Tile(tiles[i].n, tiles[i].u, QColor(tiles[i].c), grid);
            t->onClick = [this](const QString& u) { if (onNavigate) onNavigate(u); };
            g->addWidget(t, i / 4, i % 4);
        }

        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(24, 16, 24, 14);
        v->setSpacing(0);
        v->addStretch(3);
        v->addWidget(clock_);
        v->addWidget(greet_);
        v->addSpacing(34);
        auto* sr = new QHBoxLayout;
        sr->addStretch(1); sr->addWidget(search_, 100); sr->addStretch(1);
        v->addLayout(sr);
        v->addSpacing(34);
        auto* gr = new QHBoxLayout;
        gr->addStretch(1); gr->addWidget(grid, 100); gr->addStretch(1);
        v->addLayout(gr);
        v->addStretch(4);
        v->addWidget(hint_);

        tickClock();
        updateFonts();

        tick_ = new QTimer(this);
        QObject::connect(tick_, &QTimer::timeout, this, [this] { tickClock(); });
        anim_ = new QTimer(this);
        anim_->setInterval(40);
        QObject::connect(anim_, &QTimer::timeout, this, [this] { update(); });
    }

    void clearSearch() { search_->clear(); }

    void refreshBackground() {
        bg_ = QPixmap();  // force re-render
        update();
    }
    void focusSearch() { search_->focusEdit(); }
    void hideAutocomplete() { if (homeAutocomplete_) homeAutocomplete_->hideDropdown(); }

protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        tick_->start(1000); anim_->start(); tickClock();
    }
    void hideEvent(QHideEvent* e) override {
        QWidget::hideEvent(e);
        tick_->stop(); anim_->stop();
    }
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); updateFonts(); }

    void contextMenuEvent(QContextMenuEvent* ev) override {
        if (onContextMenu) {
            onContextMenu(ev->pos());
            ev->accept();
        } else {
            QWidget::contextMenuEvent(ev);
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        const QSize targetSize = size();
        if (bg_.size() != targetSize) {
            bg_ = AppBackground::render(targetSize.width(), targetSize.height());
        }
        if (!bg_.isNull()) {
            p.drawPixmap(0, 0, bg_);
        }
    }

private:
    static void setLabelColor(QLabel* l, const QColor& c) {
        QPalette p = l->palette();
        p.setColor(QPalette::WindowText, c);
        l->setPalette(p);
    }
    void updateFonts() {
        QFont f = clock_->font();
        f.setPixelSize(std::clamp(int(height() * 0.12), 52, 120));
        f.setWeight(QFont::Normal);
        clock_->setFont(f);
        QFont g = greet_->font();
        g.setPixelSize(std::clamp(int(height() * 0.026), 14, 19));
        greet_->setFont(g);
    }
    void tickClock() {
        const QTime t = QTime::currentTime();
        clock_->setText(t.toString("HH:mm"));
        const int h = t.hour();
        const char* g = h < 5 ? "Good night" : h < 12 ? "Good morning"
                                  : h < 17 ? "Good afternoon" : "Good evening";
        greet_->setText(QString("%1   \u00B7   %2")
                            .arg(QLocale().toString(QDate::currentDate(), "dddd, d MMMM"))
                            .arg(g));
    }

    QLabel *clock_, *greet_, *hint_;
    SearchBox* search_;
    UrlAutocomplete* homeAutocomplete_ = nullptr;
    QTimer *tick_, *anim_;
    QElapsedTimer clk_;
    QPixmap bg_;
};

// ----------------------------------------------------------------------------
//  WebView
// ----------------------------------------------------------------------------
class WebView : public QWebEngineView {
public:
    std::function<void(const QPoint&)> onContextMenu;

    explicit WebView(QWidget* parent = nullptr) : QWebEngineView(parent) {
        installEventFilter(this);
        QTimer::singleShot(0, this, [this]{ installOnChildren(); });
        QTimer::singleShot(500, this, [this]{ installOnChildren(); });
    }

protected:
    void installOnChildren() {
        for (auto* w : findChildren<QWidget*>()) {
            if (!w->property("ctxFiltered").toBool()) {
                w->installEventFilter(this);
                w->setProperty("ctxFiltered", true);
            }
        }
    }

    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::ContextMenu) {
            installOnChildren();
            auto* ce = static_cast<QContextMenuEvent*>(ev);
            const QPoint local = mapFromGlobal(ce->globalPos());
            if (onContextMenu) {
                onContextMenu(local);
                return true;
            }
        } else if (ev->type() == QEvent::ChildAdded) {
            if (auto* child = static_cast<QChildEvent*>(ev)->child()) {
                if (auto* w = qobject_cast<QWidget*>(child)) {
                    w->installEventFilter(this);
                    w->setProperty("ctxFiltered", true);
                    for (auto* sub : w->findChildren<QWidget*>()) {
                        sub->installEventFilter(this);
                        sub->setProperty("ctxFiltered", true);
                    }
                }
            }
        }
        return QWebEngineView::eventFilter(obj, ev);
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        if (onContextMenu) {
            onContextMenu(event->pos());
            event->accept();
        } else {
            QWebEngineView::contextMenuEvent(event);
        }
    }
};

// ----------------------------------------------------------------------------
//  WebPage
// ----------------------------------------------------------------------------
class WebPage : public QWebEnginePage {
public:
    std::function<QWebEnginePage*()> onNewWindow;
    std::function<bool(const QUrl&)> onSpecialScheme;

    WebPage(QWebEngineProfile* prof, QObject* parent)
        : QWebEnginePage(prof, parent) {}

protected:
    QWebEnginePage* createWindow(WebWindowType) override {
        return onNewWindow ? onNewWindow() : nullptr;
    }

    bool acceptNavigationRequest(const QUrl& url,
                                 NavigationType type,
                                 bool isMainFrame) override
    {
        if (isMainFrame && onSpecialScheme && onSpecialScheme(url))
            return false;

        // HTTPS-Only mode: auto-upgrade http:// → https://
        if (isMainFrame && type != QWebEnginePage::NavigationTypeTyped) {
            QUrl upgraded = HttpsOnly::handleNavigation(url, this);
            if (!upgraded.isEmpty() && upgraded != url) {
                QTimer::singleShot(0, this, [this, upgraded]{
                    setUrl(upgraded);
                });
                return false;
            }
        }

        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class LoadBar : public QWidget {
public:
    explicit LoadBar(QWidget* parent = nullptr) : QWidget(parent) { setFixedHeight(2); }
    void setValue(int v) { v_ = v; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        if (v_ <= 0 || v_ >= 100) return;
        QPainter p(this);
        const int w = width() * v_ / 100;
        QLinearGradient g(0, 0, std::max(1, w), 0);
        g.setColorAt(0, QColor("#6b7690"));
        g.setColorAt(1, QColor("#cfd6ea"));
        p.fillRect(QRect(0, 0, w, height()), g);
    }
private:
    int v_ = 0;
};

// ----------------------------------------------------------------------------
//  Browser
// ----------------------------------------------------------------------------
// ============================================================================
//  PrivateWindow — a standalone Tor-connected incognito window.
// ============================================================================
class PrivateWindow : public QWidget {
public:
    explicit PrivateWindow(QWidget* parent = nullptr);
    ~PrivateWindow() override;

    void loadUrl(const QUrl& url);

private:
    void setupProfile();
    void updateTorStatus();

    QWebEngineProfile* profile_ = nullptr;
    QWebEngineView*    view_    = nullptr;
    QLineEdit*         addr_    = nullptr;
    QLabel*            status_  = nullptr;
    QTimer*            statusTimer_ = nullptr;
};

PrivateWindow::PrivateWindow(QWidget* parent)
    : QWidget(parent)
{
    
    // ── Route traffic through Tor (SOCKS5)
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName("127.0.0.1");
        proxy.setPort(quint16(TorController::instance().socksPort()));
        QNetworkProxy::setApplicationProxy(proxy);
    }
setWindowTitle("RootBrowser — Private Mode (Tor)");
    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose);
    resize
