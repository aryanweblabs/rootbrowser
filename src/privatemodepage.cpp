// ============================================================================
//  privatemodepage.cpp — Tor private mode launch page.
// ============================================================================

#include "privatemodepage.h"
#include "torcontroller.h"

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

namespace PrivateModePage {

static QString esc(const QString& s) {
    QString out;
    out.reserve(s.size() + 16);
    for (QChar c : s) {
        switch (c.unicode()) {
        case '&': out += QStringLiteral("&amp;"); break;
        case '<': out += QStringLiteral("&lt;");  break;
        case '>': out += QStringLiteral("&gt;");  break;
        case '"': out += QStringLiteral("&quot;");break;
        case '\'':out += QStringLiteral("&#39;"); break;
        default:  out += c; break;
        }
    }
    return out;
}

QString build() {
    auto& T = TorController::instance();

    QString stateStr = "unknown";
    switch (T.state()) {
        case TorController::State::NotRunning:    stateStr = "not-running";    break;
        case TorController::State::Starting:      stateStr = "starting";       break;
        case TorController::State::Bootstrapping: stateStr = "bootstrapping";  break;
        case TorController::State::Ready:         stateStr = "ready";          break;
        case TorController::State::Error:         stateStr = "error";          break;
    }

    QJsonObject root;
    root["state"]      = stateStr;
    root["bootstrap"]  = T.bootstrapPercent();
    root["error"]      = esc(T.errorMessage());
    root["socksPort"]  = T.socksPort();
    root["hasBinary"]  = !TorController::findTorBinary().isEmpty();
    root["torPath"]    = TorController::findTorBinary();

    const QString dataJson =
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    const QString css = QStringLiteral(R"CSS(
        * { box-sizing: border-box; }
        html, body {
            margin: 0; padding: 0; min-height: 100vh;
            background: #0a0a0c; color: #e6e8ec;
            font-family: 'Segoe UI', 'Inter', Ubuntu, 'Noto Sans',
                         Cantarell, 'DejaVu Sans', sans-serif;
            font-size: 13px;
            -webkit-font-smoothing: antialiased;
        }
        .wrap {
            min-height: 100vh;
            display: flex; align-items: center; justify-content: center;
            padding: 40px 24px;
        }
        .card {
            width: 100%; max-width: 540px;
            background: #121316;
            border: 1px solid #1e2026;
            border-radius: 18px;
            padding: 40px 36px 32px;
        }
        .iconBox {
            width: 72px; height: 72px;
            margin: 0 auto 20px;
            border-radius: 50%;
            background: linear-gradient(135deg, #1a1c20 0%, #121316 100%);
            border: 1.5px solid #2c2e34;
            display: flex; align-items: center; justify-content: center;
            position: relative;
        }
        .iconBox svg { width: 32px; height: 32px; color: #8a8f9c; }
        .iconBox.active {
            border-color: #7a5c9e;
            box-shadow: 0 0 0 4px rgba(122, 92, 158, 0.12);
        }
        .iconBox.active svg { color: #a882d1; }
        .h1 {
            text-align: center;
            font-size: 22px; font-weight: 500;
            color: #f4f5f7; margin: 0 0 8px;
            letter-spacing: -0.3px;
        }
        .sub {
            text-align: center;
            color: #8a90a0; font-size: 13px;
            line-height: 1.55;
            margin: 0 0 28px;
        }

        /* ── Status pill ───────────────────────────────────── */
        .statusRow {
            display: flex; align-items: center; justify-content: center;
            gap: 8px;
            padding: 10px 16px;
            background: #17181b;
            border: 1px solid #23262c;
            border-radius: 10px;
            margin-bottom: 24px;
        }
        .statusDot {
            width: 8px; height: 8px; border-radius: 50%;
            background: #5c606b;
            flex-shrink: 0;
        }
        .statusDot.ready  { background: #4aaf7a; }
        .statusDot.busy   { background: #f1c75c; animation: pulse 1s infinite; }
        .statusDot.error  { background: #e0443b; }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.4; }
        }
        .statusText { color: #d0d4dc; font-size: 12px; }
        .statusText b { color: #f1f2f5; font-weight: 500; }

        /* ── Progress bar ──────────────────────────────────── */
        .progressWrap {
            height: 4px; background: #1c1d21;
            border-radius: 2px; overflow: hidden;
            margin-bottom: 24px;
        }
        .progressBar {
            height: 100%; background: linear-gradient(90deg, #7a5c9e, #a882d1);
            border-radius: 2px;
            transition: width 0.4s ease;
            width: 0%;
        }

        /* ── Feature list ──────────────────────────────────── */
        .features {
            background: #17181b;
            border: 1px solid #23262c;
            border-radius: 12px;
            padding: 4px 0;
            margin-bottom: 24px;
        }
        .feature {
            display: flex; align-items: center; gap: 12px;
            padding: 12px 16px;
            font-size: 12.5px;
            color: #d0d4dc;
        }
        .feature + .feature { border-top: 1px solid #1e2026; }
        .feature .check {
            width: 18px; height: 18px; flex-shrink: 0;
            color: #4aaf7a;
        }
        .feature .label { flex: 1; }
        .feature .label small {
            display: block;
            color: #6b7280; font-size: 11px;
            margin-top: 2px;
        }

        /* ── Warning ───────────────────────────────────────── */
        .warning {
            background: rgba(224, 68, 59, 0.08);
            border: 1px solid rgba(224, 68, 59, 0.25);
            border-radius: 10px;
            padding: 12px 16px;
            font-size: 12px;
            color: #e8a8a3;
            line-height: 1.5;
            margin-bottom: 20px;
        }
        .warning b { color: #ffb3b3; }

        /* ── Buttons ───────────────────────────────────────── */
        .btnRow {
            display: flex; gap: 10px;
        }
        .btn {
            flex: 1;
            padding: 13px 18px;
            border-radius: 11px;
            font-size: 13px;
            font-weight: 500;
            cursor: pointer;
            border: 1px solid #2c2e34;
            background: #1c1d21;
            color: #d7d9de;
            transition: all 0.12s;
            display: flex; align-items: center; justify-content: center;
            gap: 8px;
            font-family: inherit;
        }
        .btn:hover { background: #23262c; border-color: #3a3d45; }
        .btn.primary {
            background: #7a5c9e;
            color: #ffffff;
            border-color: #7a5c9e;
            font-weight: 600;
        }
        .btn.primary:hover { background: #a882d1; border-color: #a882d1; }
        .btn.primary:disabled {
            background: #2c2e34; color: #5c606b;
            border-color: #2c2e34;
            cursor: not-allowed;
        }
        .btn svg { width: 15px; height: 15px; }

        .footer {
            margin-top: 24px;
            padding-top: 18px;
            border-top: 1px solid #1e2026;
            color: #5c606b; font-size: 11px;
            text-align: center;
            line-height: 1.6;
        }
        .footer code {
            background: #17181b;
            padding: 1px 6px;
            border-radius: 4px;
            font-family: ui-monospace, Menlo, monospace;
            color: #9aa0ac;
        }
    )CSS");

    const QString html = QStringLiteral(R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Private Mode (Tor) — RootBrowser</title>
<style>%1</style>
</head><body>
<div class="wrap">
    <div class="card">

        <div class="iconBox" id="iconBox">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
                <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
                <circle cx="12" cy="12" r="3"/>
            </svg>
        </div>

        <h1 class="h1">Private Mode with Tor</h1>
        <p class="sub">
            Browse anonymously through the Tor network.<br>
            No history, no cookies, no traces.
        </p>

        <div class="statusRow">
            <div class="statusDot" id="statusDot"></div>
            <div class="statusText" id="statusText">Checking Tor…</div>
        </div>

        <div class="progressWrap">
            <div class="progressBar" id="progressBar"></div>
        </div>

        <div class="features">
            <div class="feature">
                <svg class="check" viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="20 6 9 17 4 12"/>
                </svg>
                <div class="label">
                    Anonymous browsing
                    <small>Your traffic is routed through 3 relays</small>
                </div>
            </div>
            <div class="feature">
                <svg class="check" viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="20 6 9 17 4 12"/>
                </svg>
                <div class="label">
                    No history or cookies
                    <small>Everything is wiped when you close the window</small>
                </div>
            </div>
            <div class="feature">
                <svg class="check" viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="20 6 9 17 4 12"/>
                </svg>
                <div class="label">
                    .onion sites supported
                    <small>Access Tor hidden services directly</small>
                </div>
            </div>
            <div class="feature">
                <svg class="check" viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="20 6 9 17 4 12"/>
                </svg>
                <div class="label">
                    WebRTC disabled
                    <small>Prevents IP address leaks</small>
                </div>
            </div>
        </div>

        <div id="warningBox"></div>

        <div class="btnRow">
            <button class="btn primary" id="launchBtn" disabled>
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/>
                    <polyline points="15 3 21 3 21 9"/>
                    <line x1="10" y1="14" x2="21" y2="3"/>
                </svg>
                Open Private Window
            </button>
        </div>

        <div class="footer">
            Tor circuit: 3 hops · SOCKS port <code>%2</code><br>
            Download speed is slower — this is normal for anonymity.
        </div>
    </div>
</div>

<script>
const DATA = %3;
let state     = DATA.state || 'unknown';
let bootstrap = DATA.bootstrap || 0;
let errorMsg  = DATA.error || '';
let socksPort = DATA.socksPort || 9050;
let hasBinary = DATA.hasBinary || false;
let torPath   = DATA.torPath || '';

const dot    = document.getElementById('statusDot');
const text   = document.getElementById('statusText');
const bar    = document.getElementById('progressBar');
const launch = document.getElementById('launchBtn');
const iconBox= document.getElementById('iconBox');
const warnBox= document.getElementById('warningBox');

function escapeHtml(s) {
    return String(s == null ? '' : s)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// ── Render state ───────────────────────────────────────────────────────
function render() {
    bar.style.width = bootstrap + '%';

    dot.className = 'statusDot';
    if (state === 'ready') {
        dot.classList.add('ready');
        iconBox.classList.add('active');
        text.innerHTML = '<b>Tor is ready</b> · you can browse anonymously';
        launch.disabled = false;
        launch.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/><polyline points="15 3 21 3 21 9"/><line x1="10" y1="14" x2="21" y2="3"/></svg> Open Private Window';
        warnBox.innerHTML = '';
    }
    else if (state === 'starting' || state === 'bootstrapping') {
        dot.classList.add('busy');
        iconBox.classList.add('active');
        text.innerHTML = '<b>Connecting to Tor…</b> ' + bootstrap + '%';
        launch.disabled = true;
        launch.innerHTML = 'Connecting…';
        warnBox.innerHTML = '';
    }
    else if (state === 'error') {
        dot.classList.add('error');
        text.innerHTML = '<b>Tor failed</b> · ' + escapeHtml(errorMsg.split('\n')[0] || 'unknown error');
        launch.disabled = false;
        launch.innerHTML = 'Try again';
        warnBox.innerHTML = '<div class="warning"><b>Error:</b> ' +
                            escapeHtml(errorMsg || 'unknown') + '</div>';
    }
    else {
        dot.classList.add('busy');
        text.innerHTML = '<b>Tor is not running</b> · click below to start';
        launch.disabled = false;
        launch.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="5 3 19 12 5 21 5 3"/></svg> Start Tor';
    }
}

// ── Triggers via URL scheme ────────────────────────────────────────────
function trigger(action) {
    window.location.href = 'rootbrowser-privatemode:' + action;
}

launch.addEventListener('click', () => {
    if (state === 'ready') {
        trigger('open-window');
    } else if (state === 'error') {
        // Retry — attempt start again
        trigger('start-tor');
    } else {
        trigger('start-tor');
    }
});

render();

// ── Poll status every 1.5s while not ready
(function poll() {
    if (state !== 'ready') {
        setTimeout(() => {
            window.location.href = 'rootbrowser-privatemode:refresh';
        }, 1500);
    }
})();
</script>
</body></html>)HTML")
        .arg(css)
        .arg(T.socksPort())
        .arg(dataJson);

    return html;
}

} // namespace PrivateModePage
