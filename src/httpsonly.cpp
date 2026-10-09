// ============================================================================
//  httpsonly.cpp — HTTPS-Only implementation.
// ============================================================================

#include "httpsonly.h"
#include "settingsstore.h"

#include <QUrl>
#include <QSet>
#include <QString>
#include <QWebEnginePage>
#include <QTimer>

namespace HttpsOnly {

// ────────────────────────────────────────────────────────────────────────────
//  Blocked URL cache (so we don't loop on auto-upgrade fails)
// ────────────────────────────────────────────────────────────────────────────
static QSet<QString> g_blocked;

// ────────────────────────────────────────────────────────────────────────────
bool isEnabled() {
    return SettingsStore::instance().getBool("privacy.httpsOnly", true);
}

void setEnabled(bool enabled) {
    SettingsStore::instance().setBool("privacy.httpsOnly", enabled);
    if (!enabled) g_blocked.clear();
}

void clearBlockedCache() {
    g_blocked.clear();
}

// ────────────────────────────────────────────────────────────────────────────
QUrl handleNavigation(const QUrl& url, QWebEnginePage* page) {
    Q_UNUSED(page);

    if (!isEnabled()) return QUrl();   // pass-through

    // Skip internal schemes
    const QString scheme = url.scheme().toLower();
    if (scheme != "http") return QUrl();

    // Skip localhost / 127.0.0.1 / .local — user may want plain HTTP
    const QString host = url.host().toLower();
    if (host == "localhost" || host == "127.0.0.1" || host == "::1" ||
        host.endsWith(".local"))
        return QUrl();

    // Build upgraded URL
    QUrl upgraded = url;
    upgraded.setScheme("https");
    upgraded.setPort(-1);   // default HTTPS port

    // Skip if we already tried and failed
    if (g_blocked.contains(upgraded.toString()))
        return QUrl();      // let it load over HTTP (already tried)

    // Upgrade
    return upgraded;
}

// ────────────────────────────────────────────────────────────────────────────
bool isBlocked(const QUrl& url) {
    return g_blocked.contains(url.toString());
}

// ────────────────────────────────────────────────────────────────────────────
//  Called when HTTPS upgrade fails — mark as blocked
//  (main.cpp hooks this to loadFinished signal)
// ────────────────────────────────────────────────────────────────────────────
void markUpgradeFailed(const QUrl& httpsUrl) {
    g_blocked.insert(httpsUrl.toString());
}

// ────────────────────────────────────────────────────────────────────────────
//  Block page HTML (shown when user tries to visit HTTP and it's blocked)
// ────────────────────────────────────────────────────────────────────────────
QString blockPageHtml(const QUrl& originalUrl) {
    const QString urlStr = originalUrl.toString().toHtmlEscaped();

    return QStringLiteral(R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<title>HTTPS-Only — RootBrowser</title>
<style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    html, body {
        background: #0a0a0c;
        color: #e6e8ec;
        font-family: 'Segoe UI', 'Inter', Ubuntu, 'Noto Sans',
                     Cantarell, 'DejaVu Sans', sans-serif;
        min-height: 100vh;
    }
    .wrap {
        min-height: 100vh;
        display: flex; align-items: center; justify-content: center;
        padding: 40px 24px;
    }
    .card {
        max-width: 480px; width: 100%;
        background: #141519;
        border: 1px solid #23262c;
        border-radius: 18px;
        padding: 40px 32px 32px;
        text-align: center;
    }
    .icon {
        width: 64px; height: 64px;
        margin: 0 auto 20px;
        border-radius: 50%;
        background: rgba(90, 140, 216, 0.12);
        border: 1.5px solid rgba(90, 140, 216, 0.35);
        display: flex; align-items: center; justify-content: center;
    }
    .icon svg { width: 30px; height: 30px; color: #5a8cd8; }
    h1 {
        font-size: 22px; font-weight: 500;
        color: #f4f5f7; margin: 0 0 10px;
        letter-spacing: -0.3px;
    }
    p {
        color: #8a90a0; font-size: 13px;
        line-height: 1.6;
        margin: 0 0 26px;
    }
    .url {
        display: block;
        background: #1c1d21;
        border: 1px solid #2c2e34;
        border-radius: 8px;
        padding: 10px 14px;
        font-family: ui-monospace, Menlo, Consolas, monospace;
        font-size: 12px;
        color: #c8ccd6;
        margin-bottom: 24px;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
        text-align: left;
    }
    .btnRow {
        display: flex; gap: 10px; justify-content: center;
    }
    .btn {
        padding: 11px 20px;
        border-radius: 10px;
        font-size: 13px;
        font-weight: 500;
        cursor: pointer;
        border: 1px solid #2c2e34;
        background: #1c1d21;
        color: #d7d9de;
        transition: all 0.12s;
        font-family: inherit;
        text-decoration: none;
    }
    .btn:hover { background: #23262c; border-color: #3a3d45; }
    .btn.primary {
        background: #5a8cd8;
        color: #ffffff;
        border-color: #5a8cd8;
    }
    .btn.primary:hover { background: #74a3e8; border-color: #74a3e8; }
    .footer {
        margin-top: 26px;
        padding-top: 20px;
        border-top: 1px solid #1e2026;
        color: #5c606b; font-size: 11px;
        line-height: 1.7;
    }
</style>
</head><body>
<div class="wrap">
    <div class="card">
        <div class="icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
                <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
                <line x1="12" y1="8" x2="12" y2="13"/>
                <circle cx="12" cy="16.5" r="0.5" fill="currentColor"/>
            </svg>
        </div>
        <h1>Insecure Connection Blocked</h1>
        <p>
            RootBrowser's HTTPS-Only mode blocked this site because it uses
            an unencrypted HTTP connection. Your data would have been visible
            to anyone on your network.
        </p>
        <code class="url">%1</code>
        <div class="btnRow">
            <a class="btn primary" href="javascript:location.href='rootbrowser://settings'">
                Open Settings
            </a>
            <a class="btn" href="javascript:history.back()">
                Go Back
            </a>
        </div>
        <div class="footer">
            You can disable HTTPS-Only mode in<br>
            Settings → Privacy → HTTPS-Only
        </div>
    </div>
</div>
</body></html>)HTML").arg(urlStr);
}

} // namespace HttpsOnly
