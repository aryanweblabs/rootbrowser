// ============================================================================
//  commandpalette.cpp — Ctrl+K overlay.
// ============================================================================

#include "commandpalette.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QVariantAnimation>
#include <QFontMetrics>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QGraphicsDropShadowEffect>

namespace PaletteCol {
    const QColor backdrop   = QColor(0, 0, 0, 160);
    const QColor cardBg     = QColor("#141519");
    const QColor cardTop    = QColor("#17181c");
    const QColor border     = QColor("#2e3138");
    const QColor divider    = QColor("#23262c");
    const QColor textBright = QColor("#f4f5f7");
    const QColor text       = QColor("#c8ccd6");
    const QColor textMuted  = QColor("#8a90a0");
    const QColor textFaint  = QColor("#5c606b");
    const QColor searchBg   = QColor("#1c1d21");
    const QColor accent     = QColor("#5d9df1");
    const QColor accentSoft = QColor(93, 157, 241, 30);
    const QColor hover      = QColor(255, 255, 255, 14);
    const QColor selected   = QColor(93, 157, 241, 40);
    const QColor kbdBg      = QColor("#22242b");
    const QColor kbdBorder  = QColor("#3a3d45");
    const QColor catText    = QColor("#5c606b");
}

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_StyledBackground, false);
    setFocusPolicy(Qt::StrongFocus);

    buildUi();
    hide();
}

CommandPalette::~CommandPalette() = default;

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::add(const QString& title,
                         const QString& shortcut,
                         const QString& category,
                         const QString& iconName,
                         std::function<void()> action)
{
    commands_.push_back({title, shortcut, category, iconName, std::move(action)});
}

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::buildUi() {
    setStyleSheet("background:transparent;");

    card_ = new QWidget(this);
    card_->setObjectName("cmdCard");
    card_->setStyleSheet(QString(
        "#cmdCard{background:%1;border:1px solid %2;border-radius:14px;}")
        .arg(PaletteCol::cardBg.name(), PaletteCol::border.name()));

    auto* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(0, 0, 0, 180));
    card_->setGraphicsEffect(shadow);

    auto* v = new QVBoxLayout(card_);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ── Search bar
    {
        auto* searchWrap = new QWidget(card_);
        searchWrap->setFixedHeight(60);
        searchWrap->setStyleSheet(QString(
            "background:%1;"
            "border-top-left-radius:14px;"
            "border-top-right-radius:14px;"
            "border-bottom:1px solid %2;")
            .arg(PaletteCol::cardTop.name(), PaletteCol::divider.name()));

        auto* h = new QHBoxLayout(searchWrap);
        h->setContentsMargins(20, 0, 20, 0);
        h->setSpacing(12);

        // Search icon
        auto* icon = new QLabel(searchWrap);
        icon->setFixedSize(16, 16);
        icon->setStyleSheet(QString(
            "color:%1;background:transparent;font-size:16px;")
            .arg(PaletteCol::textMuted.name()));
        icon->setText(QStringLiteral(">"));
        QFont if_ = icon->font();
        if_.setPixelSize(16);
        if_.setBold(true);
        icon->setFont(if_);
        h->addWidget(icon);

        search_ = new QLineEdit(searchWrap);
        search_->setPlaceholderText(QStringLiteral("Type a command…"));
        search_->setFrame(false);
        QFont sf = search_->font();
        sf.setPixelSize(15);
        search_->setFont(sf);
        search_->setStyleSheet(QString(
            "QLineEdit{background:transparent;border:none;color:%1;}")
            .arg(PaletteCol::textBright.name()));
        QObject::connect(search_, &QLineEdit::textChanged, this,
                         [this](const QString&){ rebuildList(); });
        QObject::connect(search_, &QLineEdit::returnPressed, this, [this]{
            activate(selectedIndex_);
        });
        h->addWidget(search_, 1);

        // Hint label
        auto* hint = new QLabel(QStringLiteral("Esc"), searchWrap);
        hint->setFixedHeight(22);
        hint->setStyleSheet(QString(
            "background:%1;color:%2;border:1px solid %3;"
            "border-radius:5px;padding:0 8px;font-size:10px;")
            .arg(PaletteCol::kbdBg.name(),
                 PaletteCol::textFaint.name(),
                 PaletteCol::kbdBorder.name()));
        h->addWidget(hint);

        v->addWidget(searchWrap);
    }

    // ── List
    {
        auto* scroll = new QScrollArea(card_);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(
            "QScrollArea{background:transparent;border:none;}"
            "QScrollArea > QWidget > QWidget{background:transparent;}"
            "QScrollBar:vertical{background:transparent;width:8px;}"
            "QScrollBar::handle:vertical{background:#242832;"
            "border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,"
            "QScrollBar::sub-line:vertical{height:0;}");

        list_ = new QWidget;
        list_->setStyleSheet("background:transparent;");
        scroll->setWidget(list_);
        v->addWidget(scroll, 1);
    }

    // Install event filter on search for keyboard nav
    search_->installEventFilter(this);
}

// ────────────────────────────────────────────────────────────────────────────
bool CommandPalette::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == search_ && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        switch (ke->key()) {
        case Qt::Key_Down:
            select(selectedIndex_ + 1);
            return true;
        case Qt::Key_Up:
            select(selectedIndex_ - 1);
            return true;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            activate(selectedIndex_);
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ────────────────────────────────────────────────────────────────────────────
bool CommandPalette::matches(const Command& c, const QString& q) const {
    if (q.isEmpty()) return true;
    return c.title.toLower().contains(q) ||
           c.category.toLower().contains(q);
}

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::rebuildList() {
    if (!list_) return;

    // Clear old
    QLayoutItem* item;
    if (auto* oldL = list_->layout()) {
        while ((item = oldL->takeAt(0))) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete oldL;
    }

    auto* v = new QVBoxLayout(list_);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(2);

    const QString q = search_->text().trimmed().toLower();

    visibleIndices_.clear();
    int shown = 0;

    for (int i = 0; i < commands_.size(); ++i) {
        if (!matches(commands_[i], q)) continue;
        visibleIndices_.push_back(i);
    }

    if (visibleIndices_.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("No commands match"), list_);
        empty->setAlignment(Qt::AlignCenter);
        QFont ef = empty->font();
        ef.setPixelSize(12);
        empty->setFont(ef);
        empty->setStyleSheet(QString(
            "color:%1;background:transparent;padding:30px;")
            .arg(PaletteCol::textFaint.name()));
        v->addWidget(empty);
        v->addStretch(1);
        return;
    }

    if (selectedIndex_ >= visibleIndices_.size())
        selectedIndex_ = 0;
    if (selectedIndex_ < 0) selectedIndex_ = 0;

    for (int i = 0; i < visibleIndices_.size(); ++i) {
        const Command& c = commands_[visibleIndices_[i]];
        const bool selected = (i == selectedIndex_);

        auto* row = new QWidget(list_);
        row->setFixedHeight(40);
        row->setProperty("cmdIndex", i);
        row->setStyleSheet(QString(
            "QWidget{background:%1;border-radius:8px;}")
            .arg(selected ? PaletteCol::selected.name()
                          : QStringLiteral("transparent")));
        row->setCursor(Qt::PointingHandCursor);
        row->installEventFilter(this);

        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(12, 0, 12, 0);
        h->setSpacing(12);

        // Title
        auto* title = new QLabel(c.title, row);
        QFont tf = title->font();
        tf.setPixelSize(13);
        title->setFont(tf);
        title->setStyleSheet(QString(
            "color:%1;background:transparent;")
            .arg(selected ? PaletteCol::textBright.name()
                          : PaletteCol::text.name()));
        h->addWidget(title);

        h->addStretch(1);

        // Category
        if (!c.category.isEmpty()) {
            auto* cat = new QLabel(c.category, row);
            QFont cf = cat->font();
            cf.setPixelSize(10);
            cat->setFont(cf);
            cat->setStyleSheet(QString(
                "color:%1;background:transparent;"
                "padding:0 8px;")
                .arg(PaletteCol::catText.name()));
            h->addWidget(cat);
        }

        // Shortcut
        if (!c.shortcut.isEmpty()) {
            auto* kbd = new QLabel(c.shortcut, row);
            QFont kf = kbd->font();
            kf.setPixelSize(11);
            kf.setWeight(QFont::DemiBold);
            kbd->setFont(kf);
            kbd->setStyleSheet(QString(
                "background:%1;color:%2;border:1px solid %3;"
                "border-radius:5px;padding:2px 8px;")
                .arg(PaletteCol::kbdBg.name(),
                     PaletteCol::textMuted.name(),
                     PaletteCol::kbdBorder.name()));
            h->addWidget(kbd);
        }

        v->addWidget(row);
        shown++;
    }

    v->addStretch(1);
    update();
}

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::select(int index) {
    if (visibleIndices_.isEmpty()) return;
    if (index < 0) index = visibleIndices_.size() - 1;
    if (index >= visibleIndices_.size()) index = 0;
    selectedIndex_ = index;
    rebuildList();
}

void CommandPalette::activate(int index) {
    if (index < 0 || index >= visibleIndices_.size()) return;
    const Command& c = commands_[visibleIndices_[index]];

    close();

    // Defer action so palette closes first
    auto action = c.action;
    QTimer::singleShot(0, this, [action]{
        if (action) action();
    });
}

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::open() {
    if (parentWidget()) setGeometry(parentWidget()->rect());

    const int cardW = 600;
    const int cardH = qMin(480, height() - 80);
    const int x = (width() - cardW) / 2;
    const int y = (height() - cardH) / 2;
    card_->setGeometry(x, y, cardW, cardH);

    if (search_) {
        search_->clear();
        search_->setFocus();
    }
    selectedIndex_ = 0;
    rebuildList();

    show();
    raise();
    setFocus();

    // Fade in
    if (!anim_) {
        anim_ = new QVariantAnimation(this);
        anim_->setDuration(140);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(anim_, &QVariantAnimation::valueChanged, this,
                         [this](const QVariant& v){
            opacity_ = v.toReal();
            update();
        });
    }
    anim_->stop();
    anim_->setStartValue(0.0);
    anim_->setEndValue(1.0);
    anim_->start();
}

void CommandPalette::close() {
    if (!anim_) { hide(); return; }
    anim_->stop();
    anim_->setStartValue(opacity_);
    anim_->setEndValue(0.0);
    QObject::disconnect(anim_, &QVariantAnimation::finished, this, nullptr);
    QObject::connect(anim_, &QVariantAnimation::finished, this, [this]{
        hide();
    });
    anim_->start();
}

// ────────────────────────────────────────────────────────────────────────────
void CommandPalette::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor bg = PaletteCol::backdrop;
    bg.setAlpha(int(bg.alpha() * opacity_));
    p.fillRect(rect(), bg);
}

void CommandPalette::mousePressEvent(QMouseEvent* e) {
    if (card_ && !card_->geometry().contains(e->pos()))
        close();
}

void CommandPalette::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { close(); e->accept(); }
}

void CommandPalette::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (card_) {
        const int cardW = 600;
        const int cardH = qMin(480, height() - 80);
        const int x = (width() - cardW) / 2;
        const int y = (height() - cardH) / 2;
        card_->setGeometry(x, y, cardW, cardH);
    }
}
