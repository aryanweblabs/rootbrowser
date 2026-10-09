// ============================================================================
//  findinpage.cpp — Robust Chrome-style Find in Page.
// ============================================================================

#include "findinpage.h"

#include <QLineEdit>
#include <QLabel>
#include <QAbstractButton>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineFindTextResult>
#include <QFontMetrics>
#include <QVariant>
#include <QPointer>
#include <QPixmap>

// ============================================================================
//  FindIcon — monochrome vector icons
// ============================================================================
namespace FindIcon {

enum class Kind { Prev, Next, Close };

static void paint(QPainter& p, Kind k, const QRectF& r, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = qMin(r.width(), r.height());
    const QPointF ctr = r.center();

    p.setPen(QPen(c, qMax<qreal>(1.4, s * 0.10),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    switch (k) {
    case Kind::Prev:
        p.drawLine(QPointF(ctr.x(), ctr.y() + s * 0.20),
                   QPointF(ctr.x(), ctr.y() - s * 0.20));
        p.drawLine(QPointF(ctr.x() - s * 0.20, ctr.y() - s * 0.02),
                   QPointF(ctr.x(), ctr.y() - s * 0.22));
        p.drawLine(QPointF(ctr.x() + s * 0.20, ctr.y() - s * 0.02),
                   QPointF(ctr.x(), ctr.y() - s * 0.22));
        break;
    case Kind::Next:
        p.drawLine(QPointF(ctr.x(), ctr.y() - s * 0.20),
                   QPointF(ctr.x(), ctr.y() + s * 0.20));
        p.drawLine(QPointF(ctr.x() - s * 0.20, ctr.y() + s * 0.02),
                   QPointF(ctr.x(), ctr.y() + s * 0.22));
        p.drawLine(QPointF(ctr.x() + s * 0.20, ctr.y() + s * 0.02),
                   QPointF(ctr.x(), ctr.y() + s * 0.22));
        break;
    case Kind::Close:
        p.drawLine(QPointF(ctr.x() - s * 0.18, ctr.y() - s * 0.18),
                   QPointF(ctr.x() + s * 0.18, ctr.y() + s * 0.18));
        p.drawLine(QPointF(ctr.x() + s * 0.18, ctr.y() - s * 0.18),
                   QPointF(ctr.x() - s * 0.18, ctr.y() + s * 0.18));
        break;
    }
    p.restore();
}

} // namespace FindIcon

// ============================================================================
//  FindIconButton — global scope
// ============================================================================
class FindIconButton : public QAbstractButton {
public:
    explicit FindIconButton(FindIcon::Kind k, QWidget* parent = nullptr)
        : QAbstractButton(parent), kind_(k)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(28, 28);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
    }

    void setKind(FindIcon::Kind k) { kind_ = k; update(); }

protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::HoverEnter) { hover_ = true; update(); }
        else if (e->type() == QEvent::HoverLeave) { hover_ = false; update(); }
        return QAbstractButton::event(e);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (hover_ && isEnabled()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 22));
            p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);
        }

        const QColor c = isEnabled()
            ? (hover_ ? QColor("#ffffff") : QColor("#b4b8c2"))
            : QColor(255, 255, 255, 55);

        FindIcon::paint(p, kind_, QRectF(4, 4, width() - 8, height() - 8), c);
    }

private:
    FindIcon::Kind kind_;
    bool hover_ = false;
};

// ============================================================================
//  FindBar
// ============================================================================
FindBar::FindBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("FindBar");
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground);

    setFixedHeight(42);
    setMinimumWidth(360);
    setMaximumWidth(520);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 4, 6, 4);
    layout->setSpacing(6);

    // Search icon (left)
    auto* searchIcon = new QLabel(this);
    searchIcon->setFixedSize(18, 18);
    searchIcon->setPixmap([]() {
        QPixmap pm(36, 36);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#8a8f9c"), 2.0,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(5, 5, 11, 11));
        p.drawLine(QPointF(14, 14), QPointF(18, 18));
        p.end();
        return pm;
    }());
    layout->addWidget(searchIcon);

    // Line edit
    edit_ = new QLineEdit(this);
    edit_->setPlaceholderText("Find in page");
    edit_->setFrame(false);
    edit_->setMinimumWidth(140);
    QFont ef = edit_->font();
    ef.setPixelSize(13);
    edit_->setFont(ef);
    edit_->setStyleSheet(
        "QLineEdit{background:transparent;border:none;color:#e6e8ec;"
        "selection-background-color:#3d4d6a;selection-color:#ffffff;}"
        "QLineEdit::placeholder{color:#6b7280;}");
    layout->addWidget(edit_, 1);

    // Match counter
    match_ = new QLabel(this);
    match_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont mf = match_->font();
    mf.setPixelSize(11);
    match_->setFont(mf);
    match_->setStyleSheet("color:#8a8f9c;background:transparent;");
    match_->setMinimumWidth(64);
    match_->setText("");
    layout->addWidget(match_);

    // Buttons
    prev_  = new FindIconButton(FindIcon::Kind::Prev, this);
    next_  = new FindIconButton(FindIcon::Kind::Next, this);
    close_ = new FindIconButton(FindIcon::Kind::Close, this);

    prev_->setToolTip("Previous match (Shift+Enter)");
    next_->setToolTip("Next match (Enter)");
    close_->setToolTip("Close (Esc)");

    layout->addWidget(prev_);
    layout->addWidget(next_);
    layout->addWidget(close_);

    // Signals
    connect(edit_, &QLineEdit::textChanged, this, [this](const QString& t){
        QTimer::singleShot(60, this, [this, t]{
            if (t != edit_->text()) return;
            doFind(t, true, true);
        });
    });

    connect(edit_, &QLineEdit::returnPressed, this, [this]{
        doFind(edit_->text(), true, false);
    });

    connect(prev_,  &QAbstractButton::clicked, this, [this]{ findPrevious(); });
    connect(next_,  &QAbstractButton::clicked, this, [this]{ findNext(); });
    connect(close_, &QAbstractButton::clicked, this, [this]{ close(); });

    edit_->installEventFilter(this);

    hide();
}

FindBar::~FindBar() = default;

void FindBar::attach(QWebEngineView* view) {
    if (view_ == view) return;

    if (view_ && view_->page())
        view_->page()->findText(QString());

    view_ = view;

    if (!lastQuery_.isEmpty() && view_ && view_->page())
        doFind(lastQuery_, true, false);
}

bool FindBar::isOpen() const {
    return isVisible();
}

void FindBar::open() {
    if (isVisible()) {
        edit_->setFocus();
        edit_->selectAll();
        return;
    }

    show();
    raise();
    edit_->setFocus();

    if (view_ && view_->page()) {
        QPointer<FindBar> self(this);
        QPointer<QWebEngineView> safeView(view_);

        view_->page()->runJavaScript(
            "(function(){ try { return window.getSelection().toString(); } "
            "catch(e){ return ''; } })();",
            [self, safeView](const QVariant& v){
                if (!self) return;
                if (!self->isVisible()) return;
                if (!safeView) return;

                const QString sel = v.toString().trimmed();
                const bool ok = !sel.isEmpty() && sel.length() < 200
                                && sel.indexOf(QChar(10)) < 0;
                if (ok) {
                    self->edit_->setText(sel);
                    self->edit_->selectAll();
                } else {
                    self->edit_->selectAll();
                }
            });
    } else {
        edit_->selectAll();
    }
}

void FindBar::close() {
    clearHighlights();
    hide();

    edit_->clear();
    lastQuery_.clear();
    current_ = 0;
    total_   = 0;
    updateMatchLabel(0, 0);

    if (onClose) onClose();
}

void FindBar::findNext() {
    if (!view_ || !view_->page()) return;
    if (edit_->text().isEmpty()) return;
    doFind(edit_->text(), true, false);
}

void FindBar::findPrevious() {
    if (!view_ || !view_->page()) return;
    if (edit_->text().isEmpty()) return;
    doFind(edit_->text(), false, false);
}

void FindBar::doFind(const QString& text, bool forward, bool isNewSearch) {
    if (!view_ || !view_->page()) return;

    if (text.isEmpty()) {
        clearHighlights();
        current_ = 0;
        total_   = 0;
        updateMatchLabel(0, 0);
        lastQuery_.clear();
        return;
    }

    if (text != lastQuery_) {
        lastQuery_ = text;
        isNewSearch = true;
    }

    QWebEnginePage::FindFlags flags;
    if (!forward) flags |= QWebEnginePage::FindBackward;
    if (caseSensitive_) flags |= QWebEnginePage::FindCaseSensitively;

    view_->page()->findText(text, flags,
        [this, isNewSearch](const QWebEngineFindTextResult& result){
            const int n = result.numberOfMatches();
            const int a = result.activeMatch();
            total_ = n;
            current_ = isNewSearch ? ((n > 0) ? 1 : 0) : a;
            updateMatchLabel(current_, total_);
        });
}

void FindBar::updateMatchLabel(int current, int total) {
    if (!match_) return;

    if (total == 0) {
        if (edit_->text().isEmpty()) {
            match_->setText("");
            match_->setStyleSheet("color:#8a8f9c;background:transparent;");
        } else {
            match_->setText("No results");
            match_->setStyleSheet("color:#c8866a;background:transparent;");
        }
        return;
    }

    match_->setText(QString("%1 / %2").arg(current).arg(total));
    match_->setStyleSheet("color:#8a8f9c;background:transparent;");
}

void FindBar::clearHighlights() {
    if (view_ && view_->page())
        view_->page()->findText(QString());
}

void FindBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = r.height() / 2.0;

    p.setPen(QPen(QColor("#3a3f4c"), 1));
    p.setBrush(QColor("#20222a"));
    p.drawRoundedRect(r, radius, radius);

    p.setPen(QPen(QColor(255, 255, 255, 14), 1));
    p.drawLine(QPointF(r.left() + radius * 0.4, r.top() + 1),
               QPointF(r.right() - radius * 0.4, r.top() + 1));
}

void FindBar::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Escape:
        close();
        e->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (e->modifiers() & Qt::ShiftModifier) findPrevious();
        else findNext();
        e->accept();
        return;
    case Qt::Key_F3:
        if (e->modifiers() & Qt::ShiftModifier) findPrevious();
        else findNext();
        e->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(e);
}

bool FindBar::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == edit_) {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            switch (ke->key()) {
            case Qt::Key_Escape:
                close();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (ke->modifiers() & Qt::ShiftModifier) findPrevious();
                else findNext();
                return true;
            case Qt::Key_F3:
                if (ke->modifiers() & Qt::ShiftModifier) findPrevious();
                else findNext();
                return true;
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}
