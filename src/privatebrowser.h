// ============================================================================
//  privatebrowser.h — Standalone Tor-connected private browser window.
// ============================================================================
#pragma once

#include <QWidget>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <functional>
#include <QVector>

class QWebEngineProfile;
class QWebEngineView;
class QStackedWidget;
class QLineEdit;
class QLabel;
class QTabBar;

class PrivateBrowser : public QWidget {
public:
    explicit PrivateBrowser(QWidget* parent = nullptr);
    ~PrivateBrowser() override;

    void openUrl(const QUrl& url);

private:
    struct Tab {
        QWebEngineView* view = nullptr;
        bool onHome = true;
        QString title;
    };

    void buildUi();
    void setupProfile();

    int  addTab(bool onHome = true);
    void closeTab(int index);
    void switchTab(int index);
    void refreshCurrent();

    void navigate(const QString& input);
    void goBack();
    void goForward();
    void reload();
    void goHome();

    void updateToolbar();
    void updateTitle(int index);

    void onUrlChanged(int index, const QUrl& url);
    void onTitleChanged(int index, const QString& title);
    void onLoadFinished(int index, bool ok);

    // ── Custom page that intercepts internal URL schemes
    class PrivatePage : public QWebEnginePage {
    public:
        using QWebEnginePage::QWebEnginePage;
        std::function<bool(const QUrl&)> onSpecialScheme;
    protected:
        bool acceptNavigationRequest(const QUrl& url,
                                     NavigationType type,
                                     bool isMainFrame) override;
    };

    QWebEngineProfile* profile_ = nullptr;
    QStackedWidget*    stack_ = nullptr;
    QTabBar*           tabBar_ = nullptr;
    QLineEdit*         addr_ = nullptr;
    QLabel*            statusLabel_ = nullptr;
    QVector<Tab>       tabs_;
    int                current_ = -1;
};
