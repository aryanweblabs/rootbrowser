// ============================================================================
//  privatebrowser.h — Standalone Tor-connected private browser window.
// ============================================================================
#pragma once

#include <QWidget>
#include <QUrl>
#include <QVector>

class QWebEngineProfile;
class QWebEngineView;
class QStackedWidget;
class QLineEdit;
class QLabel;
class QTabBar;

// ----------------------------------------------------------------------------
//  PrivateBrowser — a complete isolated browser with its own tabs.
// ----------------------------------------------------------------------------
class PrivateBrowser : public QWidget {
public:
    explicit PrivateBrowser(QWidget* parent = nullptr);
    ~PrivateBrowser() override;

    void openUrl(const QUrl& url);

private:
    // ── Tab data
    struct Tab {
        QWebEngineView* view = nullptr;
        bool onHome = true;
        QString title;
    };

    // ── Setup
    void buildUi();
    void setupProfile();

    // ── Tab ops
    int  addTab(bool onHome = true);
    void closeTab(int index);
    void switchTab(int index);
    void refreshCurrent();

    // ── Navigation
    void navigate(const QString& input);
    void goBack();
    void goForward();
    void reload();
    void goHome();

    // ── UI updates
    void updateToolbar();
    void updateTitle(int index);

    // ── URL scheme handler
    bool handleSpecialScheme(const QUrl& url);

    // ── Slots
    void onUrlChanged(int index, const QUrl& url);
    void onTitleChanged(int index, const QString& title);
    void onLoadFinished(int index, bool ok);

    // ── Members
    QWebEngineProfile* profile_ = nullptr;
    QStackedWidget*    stack_ = nullptr;
    QTabBar*           tabBar_ = nullptr;
    QLineEdit*         addr_ = nullptr;
    QLabel*            statusLabel_ = nullptr;
    QVector<Tab>       tabs_;
    int                current_ = -1;
};
