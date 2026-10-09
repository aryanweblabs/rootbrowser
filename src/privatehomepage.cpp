// ============================================================================
//  privatehomepage.cpp — Modern graphite-blue private mode home.
//  Big branding. No tile icons. Just search.
// ============================================================================

#include "privatehomepage.h"
#include "torcontroller.h"

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

namespace PrivateHomePage {

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

    const bool torReady = T.isReady();
    const bool torBusy  = (T.state() == TorController::State::Starting
                        || T.state() == TorController::State::Bootstrapping);

    QString statusText;
    QString statusClass;
    if (torReady) {
        statusText  = QStringLiteral("Connected via Tor");
        statusClass = QStringLiteral("ready");
    } else if (torBusy) {
        statusText  = QStringLiteral("Connecting — %1%").arg(T.bootstrapPercent());
        statusClass = QStringLiteral("busy");
    } else {
        statusText  = QStringLiteral("Tor not connected");
        statusClass = QStringLiteral("error");
    }

    QJsonObject root;
    root["torReady"] = torReady;
    root["torBusy"]  = torBusy;

    const QString dataJson =
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    // ── CSS
    const QString css = QStringLiteral(R"CSS(
        * { box-sizing: border-box; margin: 0; padding: 0; }

        html, body {
            background: #0a0d14;
            color: #e8ebf0;
            font-family: 'Segoe UI Variable Text', 'Inter', 'Segoe UI',
                         Ubuntu, 'Noto Sans', Cantarell, 'DejaVu Sans', sans-serif;
            font-size: 13px;
            -webkit-font-smoothing: antialiased;
            min-height: 100vh;
            overflow: hidden;
        }

        /* ── Background gradient glow ─────────────────── */
        body::before {
            content: '';
            position: fixed;
            top: -20%;
            left: 50%;
            transform: translateX(-50%);
            width: 900px;
            height: 900px;
            background: radial-gradient(circle,
                        rgba(64, 116, 217, 0.13) 0%,
                        rgba(64, 116, 217, 0.05) 35%,
                        transparent 65%);
            pointer-events: none;
            z-index: 0;
        }
        body::after {
            content: '';
            position: fixed;
            bottom: -30%;
            left: 20%;
            width: 700px;
            height: 700px;
            background: radial-gradient(circle,
                        rgba(93, 157, 241, 0.06) 0%,
                        transparent 60%);
            pointer-events: none;
            z-index: 0;
        }

        .page {
            position: relative;
            z-index: 1;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 40px 32px;
        }

        /* ── Hero ─────────────────────────────────────── */
        .hero {
            text-align: center;
            margin-bottom: 56px;
        }

        .heroBadge {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 7px 16px;
            background: rgba(64, 116, 217, 0.08);
            border: 1px solid rgba(64, 116, 217, 0.22);
            border-radius: 100px;
            font-size: 12px;
            color: #8ba5d6;
            margin-bottom: 28px;
            font-weight: 500;
        }
        .heroBadge .dot {
            width: 7px;
            height: 7px;
            border-radius: 50%;
            background: #5c606b;
            flex-shrink: 0;
        }
        .heroBadge.ready .dot {
            background: #58c48b;
            box-shadow: 0 0 8px rgba(88, 196, 139, 0.6);
        }
        .heroBadge.busy .dot {
            background: #f1c75c;
            box-shadow: 0 0 8px rgba(241, 199, 92, 0.6);
        }
        .heroBadge.error .dot {
            background: #e0443b;
            box-shadow: 0 0 8px rgba(224, 68, 59, 0.6);
        }

        .heroTitle {
            font-size: 84px;
            font-weight: 200;
            letter-spacing: -3px;
            line-height: 1;
            color: #f5f7fa;
            margin-bottom: 20px;
        }
        .heroTitle b {
            font-weight: 500;
            background: linear-gradient(135deg, #5d9df1 0%, #4074d9 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .heroSub {
            font-size: 15px;
            color: #7c8699;
            font-weight: 400;
            max-width: 480px;
            margin: 0 auto;
            line-height: 1.6;
        }

        /* ── Search ───────────────────────────────────── */
        .searchWrap {
            width: 100%;
            max-width: 640px;
            margin-bottom: 40px;
        }
        .search {
            display: flex;
            align-items: center;
            background: rgba(20, 26, 38, 0.85);
            backdrop-filter: blur(12px);
            border: 1px solid #1f2734;
            border-radius: 30px;
            padding: 6px 6px 6px 24px;
            transition: all 0.2s ease;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
        }
        .search:focus-within {
            border-color: #4074d9;
            background: rgba(24, 32, 48, 0.9);
            box-shadow: 0 8px 40px rgba(64, 116, 217, 0.25),
                        0 0 0 4px rgba(64, 116, 217, 0.08);
        }
        .search svg.icon {
            width: 17px;
            height: 17px;
            color: #5d9df1;
            flex-shrink: 0;
            margin-right: 14px;
        }
        .search input {
            flex: 1;
            background: transparent;
            border: none;
            outline: none;
            color: #e8ebf0;
            font-size: 15px;
            padding: 14px 0;
            font-family: inherit;
            font-weight: 400;
        }
        .search input::placeholder {
            color: #5a6579;
        }
        .search button.go {
            width: 42px;
            height: 42px;
            border-radius: 50%;
            background: linear-gradient(135deg, #5d9df1 0%, #4074d9 100%);
            border: none;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
            transition: all 0.15s ease;
            box-shadow: 0 4px 12px rgba(64, 116, 217, 0.35);
        }
        .search button.go:hover {
            transform: translateX(2px);
            box-shadow: 0 4px 16px rgba(64, 116, 217, 0.5);
        }
        .search button.go svg {
            width: 16px;
            height: 16px;
            color: #ffffff;
        }

        /* ── Quick actions ────────────────────────────── */
        .quickActions {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: center;
            margin-bottom: 56px;
        }
        .chip {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 9px 16px;
            background: rgba(20, 26, 38, 0.6);
            border: 1px solid #1f2734;
            border-radius: 100px;
            color: #8ba5d6;
            font-size: 12.5px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.15s ease;
            font-family: inherit;
        }
        .chip:hover {
            background: rgba(64, 116, 217, 0.12);
            border-color: rgba(64, 116, 217, 0.35);
            color: #a8c1e8;
        }
        .chip .dot {
            width: 5px;
            height: 5px;
            border-radius: 50%;
            background: #4074d9;
        }

        /* ── Footer ───────────────────────────────────── */
        .footer {
            position: fixed;
            bottom: 24px;
            left: 0;
            right: 0;
            text-align: center;
            color: #4a5568;
            font-size: 11.5px;
            letter-spacing: 0.3px;
        }
        .footer kbd {
            background: rgba(31, 39, 52, 0.8);
            border: 1px solid #2a3444;
            border-radius: 5px;
            padding: 2px 7px;
            font-family: ui-monospace, Menlo, Consolas, monospace;
            font-size: 10px;
            color: #8ba5d6;
            margin: 0 3px;
            font-weight: 500;
        }

        @media (max-width: 640px) {
            .heroTitle { font-size: 56px; letter-spacing: -2px; }
            .heroSub { font-size: 13px; }
        }
    )CSS");

    // ── HTML
    const QString html = QStringLiteral(R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Private Mode — RootBrowser</title>
<style>%1</style>
</head><body>
<div class="page">

    <div class="hero">
        <div class="heroBadge %2">
            <span class="dot"></span>
            <span>%3</span>
        </div>

        <div class="heroTitle">
            RootBrowser <b>Private Mode</b>
        </div>

        <div class="heroSub">
            Anonymous browsing through the Tor network.
            No history. No cookies. No traces.
        </div>
    </div>

    <div class="searchWrap">
        <div class="search">
            <svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <circle cx="11" cy="11" r="7"/>
                <line x1="21" y1="21" x2="16.65" y2="16.65"/>
            </svg>
            <input id="query" type="text"
                   placeholder="Search privately through Tor"
                   autocomplete="off" autofocus spellcheck="false">
            <button class="go" id="go" title="Search">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round">
                    <line x1="5" y1="12" x2="19" y2="12"/>
                    <polyline points="12 5 19 12 12 19"/>
                </svg>
            </button>
        </div>
    </div>

    <div class="quickActions">
        <button class="chip" data-url="https://check.torproject.org">
            <span class="dot"></span>
            Verify connection
        </button>
        <button class="chip" data-url="http://juhanurmihxlp77nkq76byazcldy2hlmovfu2epvl5ankdibsot4csyd.onion/">
            <span class="dot"></span>
            Onion search
        </button>
        <button class="chip" data-url="https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/">
            <span class="dot"></span>
            Private search
        </button>
    </div>

    <div class="footer">
        <kbd>Ctrl+T</kbd> New tab
        <kbd>Ctrl+L</kbd> Address bar
        <kbd>Ctrl+Shift+N</kbd> New private window
    </div>
</div>

<script>
const DATA = %4;

function navigate(q) {
    if (!q) return;

    const isUrl = q.includes('://') ||
                  /\.(com|org|net|io|info|dev|onion|xyz)(\/|$)/i.test(q) ||
                  q.startsWith('localhost') ||
                  /^\d+\.\d+\.\d+\.\d+/.test(q);

    let target;
    if (isUrl) {
        if (q.includes('://'))         target = q;
        else if (q.includes('.onion')) target = 'http://' + q;
        else                            target = 'https://' + q;
    } else {
        target = 'https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/?q='
                 + encodeURIComponent(q);
    }

    window.location.href = 'rootbrowser-privatenav:' + encodeURIComponent(target);
}

document.getElementById('go').addEventListener('click', () => {
    navigate(document.getElementById('query').value.trim());
});

document.getElementById('query').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        e.preventDefault();
        navigate(e.target.value.trim());
    }
});

document.querySelectorAll('.chip').forEach(chip => {
    chip.addEventListener('click', () => {
        const url = chip.dataset.url;
        if (url) window.location.href = 'rootbrowser-privatenav:' + encodeURIComponent(url);
    });
});

if (DATA.torBusy) {
    setTimeout(() => location.reload(), 1500);
}
</script>
</body></html>)HTML")
        .arg(css)
        .arg(statusClass)
        .arg(esc(statusText))
        .arg(dataJson);

    return html;
}

} // namespace PrivateHomePage
