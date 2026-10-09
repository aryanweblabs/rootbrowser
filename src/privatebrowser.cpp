// ============================================================================
//  privatebrowser.cpp — Standalone private browser window.
// ============================================================================

#include "privatebrowser.h"
#include "privatehomepage.h"
#include "torcontroller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTabBar>
#include <QToolButton>
#include <QShortcut>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QNetworkProxy>
#include <QUrl>

namespace PrivCol {
    const QColor bg           = QColor("#0e0b14");
    const QColor toolbarBg    = QColor("#17131e");
    const QColor toolbarBorder= QColor("#2c2238");
    const QColor accent       = QColor("#a882d1");
    const QColor text         = QColor("#e6e8ec");
    const QColor textMuted    = QColor("#8a90a0");
    const QColor surface      = QColor("#1a1523");
}

// ────────────────────────────────────────────────────────────────────────────
//  PrivatePage — navigation interceptor for rootbrowser-privatenav:*
// ────────────────────────────────────────────────────────────────────────────
bool PrivateBrowser::PrivatePage::acceptNavigationRequest(
    const QUrl& url, NavigationType type, bool isMainFrame)
{
    if (isMainFrame && onSpecialScheme && onSpecialScheme(url))
        return false;
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

// ────────────────────────────────────────────────────────────────────────────
//  TabBarProxyStyle — force-draw a clean X close button.
// ────────────────────────────────────────────────────────────────────────────
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QPainter>
#include <QPainterPath>

namespace {

class TabBarProxyStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    QRect subElementRect(SubElement elem, const QStyleOption* opt,
                         const QWidget* widget) const override
    {
        if (elem == SE_TabBarTabRightButton && opt) {
            QRect base = QProxyStyle::subElementRect(elem, opt, widget);
            // Anchor to the right edge of the tab
            const QRect r = opt->rect;
            const int size = 14;
            const int pad = 8;
            return QRect(r.right() - size - pad,
                         r.center().y() - size / 2,
                         size, size);
        }
        return QProxyStyle::subElementRect(elem, opt, widget);
    }

    void drawPrimitive(PrimitiveElement elem, const QStyleOption* opt,
                       QPainter* p, const QWidget* widget) const override
    {
        if (elem == PE_IndicatorTabClose && p) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);

            const QRect r = opt->rect;
            const bool hovered = (opt->state & State_MouseOver);

            if (hovered) {
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(224, 68, 59, 70));
                p->drawRoundedRect(r.adjusted(-1, -1, 1, 1), 4, 4);
            }

            const QColor col = hovered ? QColor("#ffffff") : QColor("#8a90a0");
            p->setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
            p->setBrush(Qt::NoBrush);

            const QPointF c = r.center();
            const qreal h = 4.0;
            p->drawLine(QPointF(c.x() - h, c.y() - h),
                        QPointF(c.x() + h, c.y() + h));
            p->drawLine(QPointF(c.x() + h, c.y() - h),
                        QPointF(c.x() - h, c.y() + h));

            p->restore();
            return;
        }
        QProxyStyle::drawPrimitive(elem, opt, p, widget);
    }
};

} // anonymous namespace

PrivateBrowser::PrivateBrowser(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Private Mode (Tor) — RootBrowser");
    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(900, 600);
    resize(1200, 780);

    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName("127.0.0.1");
        proxy.setPort(quint16(TorController::instance().socksPort()));
        QNetworkProxy::setApplicationProxy(proxy);
    }

    buildUi();
    setupProfile();

    auto sc = [this](const QString& seq, std::function<void()> fn) {
        auto* s = new QShortcut(QKeySequence(seq), this);
        QObject::connect(s, &QShortcut::activated, this, [fn]{ fn(); });
    };

    sc("Ctrl+T",         [this]{ addTab(true); });
    sc("Ctrl+W",         [this]{ closeTab(current_); });
    sc("Ctrl+L",         [this]{ addr_->setFocus(); addr_->selectAll(); });
    sc("Ctrl+Tab",       [this]{ if (tabs_.size() > 1) switchTab((current_ + 1) % tabs_.size()); });
    sc("Ctrl+Shift+Tab", [this]{ if (tabs_.size() > 1) switchTab((current_ - 1 + tabs_.size()) % tabs_.size()); });
    sc("Alt+Left",       [this]{ goBack(); });
    sc("Alt+Right",      [this]{ goForward(); });
    sc("F5",             [this]{ reload(); });

    addTab(true);

    if (parent) {
        const QRect pg = parent->geometry();
        const QPoint c = pg.center() - QPoint(width() / 2, height() / 2);
        move(c);
    }
}

PrivateBrowser::~PrivateBrowser() {
    QNetworkProxy::setApplicationProxy(QNetworkProxy::DefaultProxy);
}

void PrivateBrowser::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar with tabs
    auto* topBar = new QWidget(this);
    topBar->setFixedHeight(44);
    topBar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(PrivCol::toolbarBg.name(), PrivCol::toolbarBorder.name()));

    auto* topL = new QHBoxLayout(topBar);
    topL->setContentsMargins(12, 0, 8, 0);
    topL->setSpacing(8);

    auto* dot = new QLabel(topBar);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet("background:#a882d1;border-radius:5px;");
    topL->addWidget(dot);

    auto* label = new QLabel("Private Mode (Tor)", topBar);
    QFont lf = label->font();
    lf.setPixelSize(12);
    lf.setWeight(QFont::DemiBold);
    label->setFont(lf);
    label->setStyleSheet("color:#a882d1;letter-spacing:0.3px;");
    topL->addWidget(label);
    topL->addSpacing(20);

    tabBar_ = new QTabBar(topBar);
    tabBar_->setDrawBase(false);
    tabBar_->setStyle(new TabBarProxyStyle("Fusion"));
    tabBar_->setExpanding(false);
    tabBar_->setTabsClosable(true);
    tabBar_->setStyleSheet(QString(
        "QTabBar{"
        "  background:transparent;"
        "}"
        "QTabBar::tab{"
        "  background:transparent;"
        "  color:%1;"
        "  padding:8px 28px 8px 16px;"
        "  margin-right:2px;"
        "  border:none;"
        "  border-top-left-radius:8px;"
        "  border-top-right-radius:8px;"
        "  font-size:12px;"
        "  min-width:140px;"
        "}"
        "QTabBar::tab:selected{"
        "  background:%2;"
        "  color:%3;"
        "}"
        "QTabBar::tab:hover:!selected{"
        "  background:rgba(64,116,217,0.10);"
        "}"
        "QTabBar::close-button{"
        "  subcontrol-position:right;"
        "  subcontrol-origin:padding;"
        "  margin-right:6px;"
        "  width:14px;"
        "  height:14px;"
        "  background:transparent;"
        "  border-radius:3px;"
        "}"
        "QTabBar::close-button:hover{"
        "  background:rgba(224,68,59,0.30);"
        "}")
        .arg(PrivCol::textMuted.name(),
             PrivCol::surface.name(),
             PrivCol::text.name()));
    topL->addWidget(tabBar_, 1);

    auto* addBtn = new QToolButton(topBar);
    addBtn->setText("+");
    addBtn->setFixedSize(28, 28);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QToolButton{background:transparent;border:1px solid transparent;"
        "  border-radius:6px;color:%1;font-size:16px;font-weight:600;}"
        "QToolButton:hover{background:rgba(168,130,209,0.12);color:%2;}")
        .arg(PrivCol::textMuted.name(), PrivCol::accent.name()));
    QObject::connect(addBtn, &QToolButton::clicked, this, [this]{ addTab(true); });
    topL->addWidget(addBtn);

    root->addWidget(topBar);

    QObject::connect(tabBar_, &QTabBar::currentChanged, this, [this](int i){
        if (i >= 0 && i < tabs_.size()) switchTab(i);
    });
    QObject::connect(tabBar_, &QTabBar::tabCloseRequested, this, [this](int i){
        closeTab(i);
    });

    // ── Toolbar
    auto* tb = new QWidget(this);
    tb->setFixedHeight(48);
    tb->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(PrivCol::toolbarBg.name(), PrivCol::toolbarBorder.name()));

    auto* tbL = new QHBoxLayout(tb);
    tbL->setContentsMargins(12, 0, 12, 0);
    tbL->setSpacing(4);

    auto makeBtn = [&](const QString& icon, const QString& tip) -> QToolButton* {
        auto* b = new QToolButton(tb);
        b->setText(icon);
        b->setToolTip(tip);
        b->setFixedSize(32, 32);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QToolButton{background:transparent;border:none;border-radius:6px;"
            "  color:%1;font-size:16px;}"
            "QToolButton:hover{background:rgba(168,130,209,0.12);color:%2;}")
            .arg(PrivCol::textMuted.name(), PrivCol::text.name()));
        return b;
    };

    auto* backBtn = makeBtn("<-", "Back");
    auto* fwdBtn  = makeBtn("->", "Forward");
    auto* reloadBtn = makeBtn("R", "Reload");
    auto* homeBtn = makeBtn("H", "Home");

    QObject::connect(backBtn, &QToolButton::clicked, this, [this]{ goBack(); });
    QObject::connect(fwdBtn, &QToolButton::clicked, this, [this]{ goForward(); });
    QObject::connect(reloadBtn, &QToolButton::clicked, this, [this]{ reload(); });
    QObject::connect(homeBtn, &QToolButton::clicked, this, [this]{ goHome(); });

    tbL->addWidget(backBtn);
    tbL->addWidget(fwdBtn);
    tbL->addWidget(reloadBtn);
    tbL->addWidget(homeBtn);
    tbL->addSpacing(8);

    addr_ = new QLineEdit(tb);
    addr_->setPlaceholderText("Search privately or enter .onion address");
    addr_->setFixedHeight(32);
    addr_->setStyleSheet(QString(
        "QLineEdit{background:#12101a;border:1px solid %1;border-radius:16px;"
        "  padding:0 14px;color:%2;font-size:13px;}"
        "QLineEdit:focus{border:1px solid %3;background:#1a1523;}")
        .arg(PrivCol::toolbarBorder.name(), PrivCol::text.name(), PrivCol::accent.name()));
    QObject::connect(addr_, &QLineEdit::returnPressed, this, [this]{
        navigate(addr_->text());
    });
    tbL->addWidget(addr_, 1);

    statusLabel_ = new QLabel(tb);
    statusLabel_->setStyleSheet(QString("color:%1;font-size:11px;padding:0 12px;")
        .arg(PrivCol::accent.name()));
    tbL->addWidget(statusLabel_);

    root->addWidget(tb);

    stack_ = new QStackedWidget(this);
    stack_->setStyleSheet(QString("background:%1;").arg(PrivCol::bg.name()));
    root->addWidget(stack_, 1);
}

void PrivateBrowser::setupProfile() {
    profile_ = new QWebEngineProfile(this);
    profile_->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    auto* st = profile_->settings();
    st->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    st->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    st->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);
    st->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
}

int PrivateBrowser::addTab(bool onHome) {
    Tab t;
    t.onHome = onHome;
    t.title = onHome ? "Private Home" : "Loading...";

    t.view = new QWebEngineView(stack_);
    auto* pg = new PrivatePage(profile_, t.view);
        pg->onSpecialScheme = [this](const QUrl& u) -> bool {
            const QString s = u.toString();
            if (s.startsWith("rootbrowser-privatenav:", Qt::CaseInsensitive)) {
                const QString encoded = s.mid(QStringLiteral("rootbrowser-privatenav:").length());
                const QString target = QUrl::fromPercentEncoding(encoded.toUtf8());
                if (target.startsWith("#")) return true;
                // Defer navigation to next event loop tick
                QTimer::singleShot(0, this, [this, target]{ navigate(target); });
                return true;
            }
            return false;
        };
    pg->setBackgroundColor(PrivCol::bg);
    t.view->setPage(pg);
    stack_->addWidget(t.view);

    tabs_.push_back(t);
    const int idx = tabs_.size() - 1;

    tabBar_->addTab(t.title);

    QObject::connect(t.view, &QWebEngineView::urlChanged, this,
                     [this, idx](const QUrl& u){ onUrlChanged(idx, u); });
    QObject::connect(t.view, &QWebEngineView::titleChanged, this,
                     [this, idx](const QString& s){ onTitleChanged(idx, s); });
    QObject::connect(t.view, &QWebEngineView::loadFinished, this,
                     [this, idx](bool ok){ onLoadFinished(idx, ok); });

    switchTab(idx);
    return idx;
}

void PrivateBrowser::closeTab(int index) {
    if (index < 0 || index >= tabs_.size()) return;
    if (tabs_.size() == 1) { close(); return; }

    Tab& t = tabs_[index];
    if (t.view) {
        stack_->removeWidget(t.view);
        t.view->deleteLater();
        t.view = nullptr;
    }
    tabs_.removeAt(index);
    tabBar_->removeTab(index);

    if (current_ >= tabs_.size()) current_ = tabs_.size() - 1;
    switchTab(current_);
}

void PrivateBrowser::switchTab(int index) {
    if (index < 0 || index >= tabs_.size()) return;
    current_ = index;
    tabBar_->setCurrentIndex(index);

    Tab& t = tabs_[index];

    if (t.onHome) {
        t.view->setHtml(PrivateHomePage::build(),
                        QUrl(QStringLiteral("rootbrowser://privatehome")));
    }

    stack_->setCurrentWidget(t.view);
    updateToolbar();
    updateTitle(index);

    if (t.view) t.view->setFocus();
    else addr_->setFocus();
}

void PrivateBrowser::refreshCurrent() { switchTab(current_); }

void PrivateBrowser::navigate(const QString& input) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return;

    QString target;
    if (trimmed.contains("://") || trimmed.startsWith("about:") ||
        trimmed.startsWith("file:") || trimmed.startsWith("rootbrowser:")) {
        target = trimmed;
    } else if (trimmed.contains('.') && !trimmed.contains(' ')) {
        target = "https://" + trimmed;
    } else {
        target = "https://duckduckgo.com/?q=" +
                 QString::fromUtf8(QUrl::toPercentEncoding(trimmed));
    }

    Tab& t = tabs_[current_];
    t.onHome = false;
    stack_->setCurrentWidget(t.view);
    t.view->load(QUrl(target));
    t.view->setFocus();
}

void PrivateBrowser::goBack() {
    Tab& t = tabs_[current_];
    if (t.view && t.view->history()->canGoBack()) t.view->back();
}

void PrivateBrowser::goForward() {
    Tab& t = tabs_[current_];
    if (t.view && t.view->history()->canGoForward()) t.view->forward();
}

void PrivateBrowser::reload() {
    Tab& t = tabs_[current_];
    if (t.view && !t.onHome) t.view->reload();
    else switchTab(current_);
}

void PrivateBrowser::goHome() {
    tabs_[current_].onHome = true;
    switchTab(current_);
}

void PrivateBrowser::onUrlChanged(int index, const QUrl& url) {
    if (index == current_) {
        const QString s = url.toString();
        if (!s.startsWith("rootbrowser://privatehome") && s != "about:blank")
            addr_->setText(s);
        else
            addr_->clear();
    }
}

void PrivateBrowser::onTitleChanged(int index, const QString& title) {
    if (index < 0 || index >= tabs_.size()) return;
    tabs_[index].title = title.isEmpty() ? "Loading..." : title;
    updateTitle(index);
}

void PrivateBrowser::onLoadFinished(int index, bool ok) {
    Q_UNUSED(ok);
    if (index == current_) updateToolbar();
}

void PrivateBrowser::updateTitle(int index) {
    if (index != current_) return;
    const Tab& t = tabs_[index];
    QString title = t.title;
    if (title.isEmpty()) title = "Private Tab";
    tabBar_->setTabText(index, title);
    setWindowTitle(title + " — Private Mode (Tor)");
}

void PrivateBrowser::updateToolbar() {
    auto& T = TorController::instance();
    if (T.isReady()) {
        statusLabel_->setText("Tor OK");
        statusLabel_->setStyleSheet("color:#4aaf7a;font-size:11px;padding:0 12px;");
    } else if (T.state() == TorController::State::Bootstrapping
            || T.state() == TorController::State::Starting) {
        statusLabel_->setText(QString("%1%").arg(T.bootstrapPercent()));
        statusLabel_->setStyleSheet("color:#f1c75c;font-size:11px;padding:0 12px;");
    } else {
        statusLabel_->setText("No Tor");
        statusLabel_->setStyleSheet("color:#e0443b;font-size:11px;padding:0 12px;");
    }
}

void PrivateBrowser::openUrl(const QUrl& url) {
    navigate(url.toString());
}
