// ============================================================================
//  bookmarkpage.cpp — In-tab bookmarks manager.
// ============================================================================

#include "bookmarkpage.h"
#include "bookmarkstore.h"

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>

namespace BookmarkPage {

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
    QJsonArray arr;
    for (const Bookmark& b : BookmarkStore::instance().all()) {
        QJsonObject o;
        o["url"]     = b.url;
        o["title"]   = b.title;
        o["favicon"] = b.faviconPath;
        o["addedAt"] = double(b.addedAt);
        arr.append(o);
    }
    const QString bookmarksJson =
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));

    const QString css = QStringLiteral(R"CSS(
        * { box-sizing: border-box; }
        html, body {
            margin: 0; padding: 0; min-height: 100%;
            background: #0c0c0e; color: #e6e8ec;
            font-family: 'Segoe UI', 'Inter', Ubuntu, 'Noto Sans',
                         Cantarell, 'DejaVu Sans', sans-serif;
        }
        .page {
            max-width: 1100px; margin: 0 auto;
            padding: 40px 28px 60px;
        }
        .header {
            display: flex; align-items: center; gap: 14px;
            margin-bottom: 28px;
        }
        .icon {
            width: 44px; height: 44px; border-radius: 12px;
            background: linear-gradient(135deg, #5d9df1 0%, #3d6dd1 100%);
            display: flex; align-items: center; justify-content: center;
            box-shadow: 0 4px 16px rgba(93,157,241,0.35);
            flex-shrink: 0;
        }
        .icon svg { width: 24px; height: 24px; }
        .h1 {
            margin: 0; font-size: 26px; font-weight: 500;
            letter-spacing: -0.3px; color: #f4f5f7;
        }
        .count {
            color: #6b7280; font-size: 13px;
            margin-left: 6px;
        }
        .actions {
            margin-left: auto; display: flex; gap: 10px;
        }
        .input {
            background: #1c1d21;
            border: 1px solid #2c2e34;
            border-radius: 18px;
            padding: 10px 16px;
            color: #e6e8ec; font-size: 13px;
            outline: none; width: 260px;
            transition: border-color 0.15s;
        }
        .input:focus { border-color: #5d9df1; }
        .input::placeholder { color: #6b7280; }

        .sort {
            background: #1c1d21; color: #c8ccd6;
            border: 1px solid #2c2e34; border-radius: 18px;
            padding: 10px 14px; font-size: 13px;
            cursor: pointer; outline: none;
        }
        .sort:hover { background: #232429; }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
            gap: 12px;
        }
        .card {
            display: flex; align-items: center; gap: 12px;
            padding: 12px 14px;
            background: #141519;
            border: 1px solid #23262c;
            border-radius: 12px;
            cursor: pointer;
            transition: background 0.12s, border-color 0.12s, transform 0.12s;
            position: relative;
            text-decoration: none;
            color: inherit;
        }
        .card:hover {
            background: #1c1d21;
            border-color: #2e3138;
            transform: translateY(-1px);
        }
        .fav {
            width: 32px; height: 32px; border-radius: 8px;
            background: #23262c;
            display: flex; align-items: center; justify-content: center;
            font-weight: 600; font-size: 13px; color: #9aa0ac;
            flex-shrink: 0;
            overflow: hidden;
        }
        .fav img { width: 100%; height: 100%; object-fit: contain; }

        .meta { flex: 1; min-width: 0; }
        .title {
            color: #f1f2f5; font-size: 13px; font-weight: 500;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
            margin-bottom: 3px;
        }
        .url {
            color: #6b7280; font-size: 11px;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
        }

        .del {
            width: 24px; height: 24px; border-radius: 6px;
            background: transparent; border: none;
            color: #6b7280; cursor: pointer;
            display: flex; align-items: center; justify-content: center;
            flex-shrink: 0; opacity: 0;
            transition: opacity 0.12s, background 0.12s, color 0.12s;
        }
        .card:hover .del { opacity: 1; }
        .del:hover { background: #2e3138; color: #e0443b; }

        .empty {
            text-align: center; padding: 80px 20px;
            color: #9aa0ac;
        }
        .empty svg {
            width: 64px; height: 64px;
            color: #2c2e34; margin-bottom: 20px;
        }
        .empty h2 {
            color: #f1f2f5; font-weight: 400;
            font-size: 20px; margin: 0 0 8px 0;
        }
        .empty p { margin: 0; font-size: 13px; }
        .kbd {
            display: inline-block; padding: 2px 7px; border-radius: 4px;
            background: #1c1d21; border: 1px solid #2c2e34;
            font-family: ui-monospace, Menlo, monospace; font-size: 11px;
            color: #c8ccd6;
        }
    )CSS");

    const QString html = QStringLiteral(R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bookmarks — RootBrowser</title>
<style>%1</style>
</head><body>
<div class="page">

    <div class="header">
        <div class="icon">
            <svg viewBox="0 0 24 24" fill="#ffffff">
                <path d="M17 3H7a2 2 0 0 0-2 2v16l7-3 7 3V5a2 2 0 0 0-2-2z"/>
            </svg>
        </div>
        <h1 class="h1">Bookmarks<span class="count" id="count"></span></h1>
        <div class="actions">
            <input id="search" class="input" type="text"
                   placeholder="Search bookmarks..." autocomplete="off">
            <select id="sort" class="sort">
                <option value="recent">Recent first</option>
                <option value="alpha">A → Z</option>
            </select>
        </div>
    </div>

    <div id="content"></div>
</div>

<script>
const BOOKMARKS = %2;

const content = document.getElementById('content');
const search  = document.getElementById('search');
const sort    = document.getElementById('sort');
const countEl = document.getElementById('count');

let currentSort = 'recent';

function esc(s) {
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function domainOf(url) {
    try {
        const u = new URL(url);
        return u.hostname.replace(/^www\./, '');
    } catch (e) {
        return url;
    }
}

function faviconHTML(bm) {
    const d = domainOf(bm.url);
    const letter = (bm.title && bm.title[0]) || (d[0] || '?');
    const src = 'https://www.google.com/s2/favicons?sz=64&domain=' +
                encodeURIComponent(d);
    return `
        <div class="fav">
            <img src="${src}" alt=""
                 onerror="this.replaceWith(document.createTextNode('${esc(letter.toUpperCase())}'))">
        </div>`;
}

function render() {
    const q = search.value.trim().toLowerCase();

    let list = BOOKMARKS.slice();

    if (q) {
        list = list.filter(b =>
            b.title.toLowerCase().includes(q) ||
            b.url.toLowerCase().includes(q));
    }

    if (currentSort === 'recent') {
        list.sort((a, b) => (b.addedAt || 0) - (a.addedAt || 0));
    } else if (currentSort === 'alpha') {
        list.sort((a, b) =>
            (a.title || a.url).localeCompare(b.title || b.url));
    }

    countEl.textContent = ' · ' + BOOKMARKS.length;

    if (list.length === 0) {
        content.innerHTML = `
            <div class="empty">
                <svg viewBox="0 0 24 24" fill="none"
                     stroke="currentColor" stroke-width="1.5">
                    <path d="M17 3H7a2 2 0 0 0-2 2v16l7-3 7 3V5a2 2 0 0 0-2-2z"/>
                </svg>
                <h2>${BOOKMARKS.length ? 'No matches' : 'No bookmarks yet'}</h2>
                <p>${BOOKMARKS.length
                    ? 'Try a different search term.'
                    : 'Press <span class="kbd">Ctrl+D</span> on any page to add a bookmark.'}</p>
            </div>`;
        return;
    }

    let out = '<div class="grid">';
    for (const bm of list) {
        out += `
            <div class="card" data-url="${esc(bm.url)}">
                ${faviconHTML(bm)}
                <div class="meta">
                    <div class="title">${esc(bm.title || bm.url)}</div>
                    <div class="url">${esc(domainOf(bm.url))}</div>
                </div>
                <button class="del" title="Remove bookmark"
                        data-del="${esc(bm.url)}">
                    <svg width="14" height="14" viewBox="0 0 24 24"
                         fill="none" stroke="currentColor" stroke-width="2"
                         stroke-linecap="round">
                        <line x1="6" y1="6" x2="18" y2="18"/>
                        <line x1="6" y1="18" x2="18" y2="6"/>
                    </svg>
                </button>
            </div>`;
    }
    out += '</div>';
    content.innerHTML = out;

    // Click handlers
    document.querySelectorAll('.card').forEach(card => {
        card.addEventListener('click', (e) => {
            if (e.target.closest('.del')) return;
            const url = card.getAttribute('data-url');
            window.location.href = 'rootbrowser-goto:' + url;
        });
    });

    document.querySelectorAll('.del').forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            const url = btn.getAttribute('data-del');
            window.location.href = 'rootbrowser-bookmark-del:' + url;
        });
    });
}

search.addEventListener('input', render);
sort.addEventListener('change', () => {
    currentSort = sort.value;
    render();
});

render();
setTimeout(() => search.focus(), 80);
</script>
</body></html>)HTML")
        .arg(css, bookmarksJson);

    return html;
}

} // namespace BookmarkPage
