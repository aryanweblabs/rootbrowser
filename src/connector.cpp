// ============================================================================
//  connector.cpp — Right-click menu + DevTools + Save/Print for RootBrowser.
// ============================================================================
#include "connector.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QHash>
#include <QUrl>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QMainWindow>
#include <QPointer>
#include <QVariant>
#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineDownloadRequest>
#include <QPageLayout>
#include <QPageSize>
#include <cmath>

// ============================================================================
//  Icons — vector, tinted, no emoji.
// ============================================================================
namespace CtxIcons {

enum class K {
    Undo, Redo, Cut, Copy, Paste, SelectAll,
    OpenNewTab, OpenHere, CopyLink,
    ImageOpen, ImageCopy, ImageAddr,
    Search,
    Back, Forward, Reload, Source, ZoomIn, ZoomOut, ZoomReset,
    Save, Print, Inspect, Fullscreen
};

static void paint(QPainter& p, K k, const QRectF& r, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = r.width();
    auto P = [&](qreal x, qreal y) { return QPointF(r.x() + x * s, r.y() + y * s); };
    QPen pen(c, std::max<qreal>(1.4, s * 0.09), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (k) {
    case K::Undo:
        p.drawArc(QRectF(r.x() + .22*s, r.y() + .35*s, .52*s, .44*s), 0, 180 * 16);
        { QPointF a[3] = { P(.28,.36), P(.16,.54), P(.34,.58) }; p.drawPolyline(a, 3); }
        break;
    case K::Redo:
        p.drawArc(QRectF(r.x() + .26*s, r.y() + .35*s, .52*s, .44*s), 0, 180 * 16);
        { QPointF a[3] = { P(.72,.36), P(.84,.54), P(.66,.58) }; p.drawPolyline(a, 3); }
        break;
    case K::Cut:
        p.drawLine(P(.30,.22), P(.62,.70));
        p.drawLine(P(.70,.22), P(.38,.70));
        p.drawEllipse(P(.32,.78), .09*s, .09*s);
        p.drawEllipse(P(.68,.78), .09*s, .09*s);
        break;
    case K::Copy:
        p.drawRoundedRect(QRectF(r.x() + .26*s, r.y() + .22*s, .50*s, .58*s), 2.0, 2.0);
        p.drawRoundedRect(QRectF(r.x() + .14*s, r.y() + .34*s, .50*s, .58*s), 2.0, 2.0);
        break;
    case K::Paste:
        p.drawRoundedRect(QRectF(r.x() + .22*s, r.y() + .28*s, .56*s, .56*s), 2.5, 2.5);
        p.drawLine(P(.34,.28), P(.34,.18));
        p.drawLine(P(.34,.18), P(.66,.18));
        p.drawLine(P(.66,.18), P(.66,.28));
        p.drawLine(P(.42,.46), P(.58,.46));
        p.drawLine(P(.42,.58), P(.58,.58));
        p.drawLine(P(.42,.70), P(.52,.70));
        break;
    case K::SelectAll:
        p.drawRect(QRectF(r.x() + .18*s, r.y() + .18*s, .64*s, .64*s));
        p.drawLine(P(.28,.36), P(.72,.36));
        p.drawLine(P(.28,.50), P(.72,.50));
        p.drawLine(P(.28,.64), P(.58,.64));
        break;
    case K::OpenNewTab:
        p.drawRect(QRectF(r.x() + .16*s, r.y() + .24*s, .56*s, .56*s));
        p.drawLine(P(.48,.36), P(.84,.36));
        p.drawLine(P(.84,.36), P(.84,.16));
        p.drawLine(P(.84,.16), P(.52,.16));
        p.drawLine(P(.50,.50), P(.78,.22));
        break;
    case K::OpenHere:
        p.drawRect(QRectF(r.x() + .16*s, r.y() + .24*s, .56*s, .56*s));
        p.drawLine(P(.32,.52), P(.68,.52));
        p.drawLine(P(.56,.40), P(.68,.52));
        p.drawLine(P(.56,.64), P(.68,.52));
        break;
    case K::CopyLink:
        p.drawEllipse(QRectF(r.x() + .14*s, r.y() + .36*s, .30*s, .30*s));
        p.drawEllipse(QRectF(r.x() + .56*s, r.y() + .36*s, .30*s, .30*s));
        p.drawLine(P(.38,.50), P(.62,.50));
        break;
    case K::ImageOpen:
        p.drawRect(QRectF(r.x() + .16*s, r.y() + .24*s, .56*s, .56*s));
        p.drawEllipse(P(.34,.40), .06*s, .06*s);
        { QPointF t[3] = { P(.20,.76), P(.48,.48), P(.72,.76) }; p.drawPolyline(t, 3); }
        break;
    case K::ImageCopy:
        p.drawRect(QRectF(r.x() + .14*s, r.y() + .20*s, .54*s, .58*s));
        p.drawEllipse(P(.30,.34), .05*s, .05*s);
        { QPointF t[3] = { P(.18,.72), P(.42,.48), P(.62,.72) }; p.drawPolyline(t, 3); }
        break;
    case K::ImageAddr:
        p.drawRect(QRectF(r.x() + .14*s, r.y() + .24*s, .56*s, .50*s));
        p.drawLine(P(.30,.40), P(.70,.40));
        p.drawLine(P(.30,.54), P(.62,.54));
        p.drawLine(P(.30,.64), P(.52,.64));
        break;
    case K::Search:
        p.drawEllipse(P(.42,.42), .22*s, .22*s);
        p.drawLine(P(.58,.58), P(.80,.80));
        break;
    case K::Back:
        p.drawLine(P(.82,.50), P(.22,.50));
        { QPointF a[3] = { P(.46,.26), P(.22,.50), P(.46,.74) }; p.drawPolyline(a, 3); }
        break;
    case K::Forward:
        p.drawLine(P(.18,.50), P(.78,.50));
        { QPointF a[3] = { P(.54,.26), P(.78,.50), P(.54,.74) }; p.drawPolyline(a, 3); }
        break;
    case K::Reload:
        p.drawArc(QRectF(r.x() + .22*s, r.y() + .22*s, .56*s, .56*s), 60 * 16, 260 * 16);
        { QPointF a[3] = { P(.72,.22), P(.84,.34), P(.68,.40) }; p.drawPolyline(a, 3); }
        break;
    case K::Source:
        p.drawLine(P(.30,.28), P(.14,.50));
        p.drawLine(P(.14,.50), P(.30,.72));
        p.drawLine(P(.70,.28), P(.86,.50));
        p.drawLine(P(.86,.50), P(.70,.72));
        p.drawLine(P(.58,.22), P(.42,.78));
        break;
    case K::ZoomIn:
        p.drawEllipse(P(.42,.42), .22*s, .22*s);
        p.drawLine(P(.58,.58), P(.80,.80));
        p.drawLine(P(.42,.32), P(.42,.52));
        p.drawLine(P(.32,.42), P(.52,.42));
        break;
    case K::ZoomOut:
        p.drawEllipse(P(.42,.42), .22*s, .22*s);
        p.drawLine(P(.58,.58), P(.80,.80));
        p.drawLine(P(.32,.42), P(.52,.42));
        break;
    case K::ZoomReset:
        p.drawEllipse(P(.42,.42), .22*s, .22*s);
        p.drawLine(P(.58,.58), P(.80,.80));
        p.drawLine(P(.34,.42), P(.50,.42));
        p.drawLine(P(.42,.34), P(.42,.50));
        break;
    case K::Save:
        p.drawRoundedRect(QRectF(r.x() + .16*s, r.y() + .16*s, .68*s, .68*s), 2.5, 2.5);
        p.drawRect(QRectF(r.x() + .30*s, r.y() + .16*s, .40*s, .24*s));
        p.drawRect(QRectF(r.x() + .26*s, r.y() + .56*s, .48*s, .28*s));
        break;
    case K::Print:
        p.drawRoundedRect(QRectF(r.x() + .16*s, r.y() + .40*s, .68*s, .36*s), 2.0, 2.0);
        p.drawRect(QRectF(r.x() + .28*s, r.y() + .16*s, .44*s, .26*s));
        p.drawRect(QRectF(r.x() + .30*s, r.y() + .66*s, .40*s, .20*s));
        break;
    case K::Inspect:
        p.drawRect(QRectF(r.x() + .14*s, r.y() + .22*s, .44*s, .44*s));
        p.drawEllipse(P(.62,.62), .20*s, .20*s);
        p.drawLine(P(.76,.76), P(.90,.90));
        break;
    case K::Fullscreen: {
        QPointF a[3] = { P(.22,.40), P(.22,.22), P(.40,.22) };
        QPointF b[3] = { P(.60,.22), P(.78,.22), P(.78,.40) };
        QPointF c[3] = { P(.22,.60), P(.22,.78), P(.40,.78) };
        QPointF d[3] = { P(.60,.78), P(.78,.78), P(.78,.60) };
        p.drawPolyline(a,3); p.drawPolyline(b,3); p.drawPolyline(c,3); p.drawPolyline(d,3);
        break;
    }
    }
    p.restore();
}

static QIcon icon(K k) {
    static QHash<int, QIcon> cache;
    const int key = int(k);
    auto it = cache.find(key);
    if (it != cache.end()) return *it;

    QIcon out;
    const int sizes[] = { 16, 20, 24, 32 };
    const QColor normal("#c8ccd6");
    const QColor disabled("#5c606b");

    for (int sz : sizes) {
        QPixmap pm(sz, sz); pm.fill(Qt::transparent);
        { QPainter p(&pm); paint(p, k, QRectF(0,0,sz,sz), normal); }
        out.addPixmap(pm, QIcon::Normal);

        QPixmap pmd(sz, sz); pmd.fill(Qt::transparent);
        { QPainter p(&pmd); paint(p, k, QRectF(0,0,sz,sz), disabled); }
        out.addPixmap(pmd, QIcon::Disabled);
    }
    cache.insert(key, out);
    return out;
}

} // namespace CtxIcons

// ============================================================================
//  DevToolsWindow
// ============================================================================
class DevToolsWindow : public QMainWindow {
public:
    explicit DevToolsWindow(QWebEnginePage* target, QWidget* parent = nullptr)
        : QMainWindow(parent), target_(target)
    {
        setWindowTitle("DevTools");
        resize(1024, 640);
        setAttribute(Qt::WA_DeleteOnClose);

        auto* host = new QWebEngineView(this);
        setCentralWidget(host);

        auto* dt = new QWebEnginePage(target->profile(), this);
        target->setDevToolsPage(dt);
        host->setPage(dt);
    }
private:
    QPointer<QWebEnginePage> target_;
};

// ============================================================================
//  ContextMenu implementation
// ============================================================================
namespace ContextMenu {

static Host* g_host = nullptr;
static QHash<QWebEnginePage*, QPointer<DevToolsWindow>> g_devTools;

void setHost(Host* h) { g_host = h; }

void attach(QWebEngineProfile* profile, QObject* parent) {
    Q_UNUSED(profile);
    Q_UNUSED(parent);
}

void populateMenu(QMenu* menu,
                  QWebEnginePage* page,
                  QWebEngineContextMenuRequest* req)
{
    if (!menu || !page) return;
    menu->clear();

    using K = CtxIcons::K;

    auto add = [&](const QString& label, K kind, auto slot) -> QAction* {
        QAction* a = menu->addAction(CtxIcons::icon(kind), label);
        QObject::connect(a, &QAction::triggered, std::move(slot));
        return a;
    };

    // ── Context-specific items (only if req is valid) ─────────────────
    if (req) {

        // 1) EDITABLE FIELD
        if (req->isContentEditable()) {
            const bool hasSel = !req->selectedText().isEmpty();

            QAction* u = add("Undo", K::Undo, [page]{ page->triggerAction(QWebEnginePage::Undo); });
            u->setEnabled(page->action(QWebEnginePage::Undo)->isEnabled());

            QAction* r = add("Redo", K::Redo, [page]{ page->triggerAction(QWebEnginePage::Redo); });
            r->setEnabled(page->action(QWebEnginePage::Redo)->isEnabled());

            menu->addSeparator();

            QAction* cu = add("Cut", K::Cut, [page]{ page->triggerAction(QWebEnginePage::Cut); });
            cu->setEnabled(hasSel);

            QAction* co = add("Copy", K::Copy, [page]{ page->triggerAction(QWebEnginePage::Copy); });
            co->setEnabled(hasSel);

            QAction* pa = add("Paste", K::Paste, [page]{ page->triggerAction(QWebEnginePage::Paste); });
            pa->setEnabled(page->action(QWebEnginePage::Paste)->isEnabled());

            menu->addSeparator();

            add("Select All", K::SelectAll, [page]{ page->triggerAction(QWebEnginePage::SelectAll); });
            return;
        }

        // 2) LINK
        if (req->mediaType() == QWebEngineContextMenuRequest::MediaTypeNone &&
            !req->linkUrl().isEmpty())
        {
            const QUrl link = req->linkUrl();
            add("Open link in new tab", K::OpenNewTab, [link]{
                if (g_host) g_host->openUrlInNewTab(link);
            });
            add("Open link", K::OpenHere, [page, link]{ page->setUrl(link); });
            add("Copy link address", K::CopyLink, [link]{
                QApplication::clipboard()->setText(link.toString());
            });
            return;
        }

        // 3) IMAGE
        if (req->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage) {
            const QUrl src = req->mediaUrl();
            if (!src.isEmpty()) {
                add("Open image in new tab", K::ImageOpen, [src]{
                    if (g_host) g_host->openUrlInNewTab(src);
                });
                add("Copy image address", K::ImageAddr, [src]{
                    QApplication::clipboard()->setText(src.toString());
                });
                menu->addSeparator();
            }
            add("Copy image", K::ImageCopy, [page]{
                page->triggerAction(QWebEnginePage::CopyImageToClipboard);
            });
            if (!req->linkUrl().isEmpty()) {
                menu->addSeparator();
                const QUrl link = req->linkUrl();
                add("Open link in new tab", K::OpenNewTab, [link]{
                    if (g_host) g_host->openUrlInNewTab(link);
                });
            }
            return;
        }

        // 4) TEXT SELECTION
        if (!req->selectedText().isEmpty()) {
            const QString sel = req->selectedText();
            add("Copy", K::Copy, [sel]{
                QApplication::clipboard()->setText(sel);
            });
            QString s = sel.simplified();
            if (s.length() > 24) s = s.left(24) + "…";
            add(QString("Search the web for \"%1\"").arg(s), K::Search, [sel]{
                if (g_host) g_host->openSearchInNewTab(sel);
            });
            menu->addSeparator();
        }
    }

    // ── 5) PAGE — ALWAYS shown ────────────────────────────────────────
    QAction* b = add("Back", K::Back, [page]{ page->triggerAction(QWebEnginePage::Back); });
    b->setEnabled(page->action(QWebEnginePage::Back)->isEnabled());

    QAction* f = add("Forward", K::Forward, [page]{ page->triggerAction(QWebEnginePage::Forward); });
    f->setEnabled(page->action(QWebEnginePage::Forward)->isEnabled());

    add("Reload", K::Reload, [page]{ page->triggerAction(QWebEnginePage::Reload); });

    menu->addSeparator();

    add("Save page as…", K::Save, [page]{
        if (g_host) g_host->saveCurrentPage(page);
    });
    add("Print…", K::Print, [page]{
        if (g_host) g_host->printCurrentPage(page);
    });

    menu->addSeparator();

    add("View page source", K::Source, [page]{
        if (g_host) g_host->viewPageSource(page);
    });
    add("Inspect element", K::Inspect, [page]{
        if (g_host) g_host->inspectPage(page);
    });

    menu->addSeparator();

    QMenu* zoom = menu->addMenu("Zoom");
    zoom->menuAction()->setIcon(CtxIcons::icon(K::ZoomIn));
    {
        QAction* zi = zoom->addAction(CtxIcons::icon(K::ZoomIn), "Zoom in");
        QObject::connect(zi, &QAction::triggered, [page]{
            page->setZoomFactor(qMin(5.0, page->zoomFactor() + 0.1));
        });
        QAction* zo = zoom->addAction(CtxIcons::icon(K::ZoomOut), "Zoom out");
        QObject::connect(zo, &QAction::triggered, [page]{
            page->setZoomFactor(qMax(0.25, page->zoomFactor() - 0.1));
        });
        QAction* zr = zoom->addAction(CtxIcons::icon(K::ZoomReset), "Reset zoom");
        QObject::connect(zr, &QAction::triggered, [page]{ page->setZoomFactor(1.0); });
    }

    add("Full screen", K::Fullscreen, []{
        if (g_host) g_host->toggleFullscreen();
    });
}

// ── Menu for non-web contexts (home page) ──────────────────────────
void populateMenuForHost(QMenu* menu, Host* host) {
    if (!menu) return;
    menu->clear();

    using K = CtxIcons::K;

    auto add = [&](const QString& label, K kind, auto slot) -> QAction* {
        QAction* a = menu->addAction(CtxIcons::icon(kind), label);
        QObject::connect(a, &QAction::triggered, std::move(slot));
        return a;
    };

    QAction* b = add("Back", K::Back, []{});
    b->setEnabled(false);
    QAction* f = add("Forward", K::Forward, []{});
    f->setEnabled(false);

    add("Reload", K::Reload, []{});

    menu->addSeparator();

    QAction* sv = add("Save page as…", K::Save, []{});
    sv->setEnabled(false);
    QAction* pr = add("Print…", K::Print, []{});
    pr->setEnabled(false);

    menu->addSeparator();

    QAction* src = add("View page source", K::Source, []{});
    src->setEnabled(false);
    QAction* insp = add("Inspect element", K::Inspect, []{});
    insp->setEnabled(false);

    menu->addSeparator();

    add("Full screen", K::Fullscreen, [host]{
        if (host) host->toggleFullscreen();
    });
}

// ── DevTools ───────────────────────────────────────────────────────
void openDevTools(QWebEnginePage* page) {
    if (!page) return;

    if (auto it = g_devTools.find(page); it != g_devTools.end() && *it) {
        (*it)->show();
        (*it)->raise();
        (*it)->activateWindow();
        return;
    }

    auto* win = new DevToolsWindow(page);
    g_devTools.insert(page, win);
    win->show();
    win->raise();
    win->activateWindow();
}

// ── Save / Print / Source ─────────────────────────────────────────
void savePageAs(QWidget* dialogParent, QWebEnginePage* page) {
    if (!page) return;
    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        + "/" + (page->title().isEmpty() ? QString("page") : page->title())
        + ".mhtml";
    const QString path = QFileDialog::getSaveFileName(
        dialogParent, QObject::tr("Save page"), suggested,
        QObject::tr("Web Archive (*.mhtml);;All files (*)"));
    if (path.isEmpty()) return;

    page->download(QUrl(page->url().toString()), path);
}

void printPageToPdf(QWebEnginePage* page) {
    if (!page) return;
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        + "/" + (page->title().isEmpty() ? QString("page") : page->title())
        + ".pdf";
    page->printToPdf(path, QPageLayout(QPageSize(QPageSize::A4),
                                       QPageLayout::Portrait,
                                       QMarginsF(10, 10, 10, 10)));
}

// ── View page source — synchronous via setHtml in a new tab ────────
void openPageSource(QWebEnginePage* page) {
    if (!page) return;
    page->toHtml([page](const QString& html) {
        if (!g_host) return;

        const QString body = html.toHtmlEscaped();
        const QString wrapped = QString(
            "<!doctype html><html><head><meta charset='utf-8'>"
            "<title>Page source</title>"
            "<style>"
            "html,body{margin:0;padding:0;background:#0e0e11;color:#d7d9de;}"
            "pre{margin:0;padding:18px 22px;"
            "font:13px/1.55 ui-monospace,'JetBrains Mono','Fira Code',Menlo,Consolas,monospace;"
            "white-space:pre-wrap;word-break:break-word;tab-size:4;}"
            "</style></head><body><pre>%1</pre></body></html>")
            .arg(body);

        g_host->openHtmlInNewTab(wrapped, QStringLiteral("Page source"));
    });
}

} // namespace ContextMenu
