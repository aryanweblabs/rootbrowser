// ============================================================================
//  connector.h — public API of the context-menu system.
// ============================================================================
#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineContextMenuRequest>

class QMenu;
class QWidget;
class QWebEnginePage;

// ============================================================================
//  Host — main.cpp implements this ONCE.
// ============================================================================
class Host {
public:
    virtual ~Host() = default;

    virtual void openUrlInNewTab(const QUrl& u) = 0;
    virtual void openSearchInNewTab(const QString& q) = 0;
    virtual void openHtmlInNewTab(const QString& html, const QString& title) = 0;  // NEW
    virtual void saveCurrentPage(QWebEnginePage* p) = 0;
    virtual void printCurrentPage(QWebEnginePage* p) = 0;
    virtual void viewPageSource(QWebEnginePage* p) = 0;
    virtual void inspectPage(QWebEnginePage* p) = 0;
    virtual void toggleFullscreen() = 0;
};

namespace ContextMenu {

void setHost(Host* h);
void attach(QWebEngineProfile* profile, QObject* parent);

void populateMenu(QMenu* menu,
                  QWebEnginePage* page,
                  QWebEngineContextMenuRequest* req);

// For non-web contexts (home page)
void populateMenuForHost(QMenu* menu, Host* host);

void openDevTools(QWebEnginePage* page);
void savePageAs(QWidget* dialogParent, QWebEnginePage* page);
void printPageToPdf(QWebEnginePage* page);
void openPageSource(QWebEnginePage* page);

} // namespace ContextMenu
