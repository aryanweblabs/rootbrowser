// ============================================================================
//  historypage.cpp — Beautiful, feature-rich history page.
// ============================================================================

#include "historypage.h"
#include "historystore.h"

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QUrl>

namespace HistoryPage {

// ────────────────────────────────────────────────────────────────────────────
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

// Scan the favicon cache dir for a matching host PNG.
static QString faviconFor(const QString& url) {
    static QHash<QString, QString> cache;
    static QString faviconDir;

    if (faviconDir.isEmpty()) {
        faviconDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                   + "/favicons";
    }

    QUrl u(url);
    QString host = u.host().toLower();
    if (host.startsWith("www.")) host = host.mid(4);
    if (host.isEmpty()) return QString();

    if (cache.contains(host)) return cache.value(host);

    const QString path = faviconDir + "/" + host + ".png";
    QString result;
    if (QFileInfo::exists(path)) {
        // Return file:// URL — WebEngine can load local files from the app.
        result = QUrl::fromLocalFile(path).toString();
    }
    cache.insert(host, result);
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
QString build() {
    const auto& entries = HistoryStore::instance().all();

    // ── Serialize to JSON
    QJsonArray arr;
    for (const HistoryEntry& e : entries) {
        QJsonObject o;
        o["url"]        = e.url;
        o["title"]      = e.title;
        o["visitedAt"]  = double(e.visitedAt);
        o["visitCount"] = e.visitCount;
        o["favicon"]    = faviconFor(e.url);
        arr.append(o);
    }

    // ── Top sites
    QJsonArray topArr;
    for (const auto& s : HistoryStore::instance().topSites(12)) {
        QJsonObject o;
        o["host"]      = s.host;
        o["title"]     = s.title;
        o["url"]       = s.url;
        o["visits"]    = s.visits;
        o["lastVisit"] = double(s.lastVisit);
        o["favicon"]   = faviconFor(s.url);
        topArr.append(o);
    }

    // ── Stats
    QJsonObject stats;
    stats["total"]       = entries.size();
    stats["uniqueSites"] = HistoryStore::instance().uniqueSites();
    stats["oldest"]      = double(HistoryStore::instance().oldestTimestamp());
    stats["fileSize"]    = double(HistoryStore::instance().fileSize());

    // ── Week count
    {
        const qint64 weekAgo = QDateTime::currentSecsSinceEpoch() - 7 * 24 * 3600;
        int weekCount = 0;
        for (const HistoryEntry& e : entries)
            if (e.visitedAt >= weekAgo) ++weekCount;
        stats["thisWeek"] = weekCount;
    }

    QJsonObject root;
    root["items"] = arr;
    root["top"]   = topArr;
    root["stats"] = stats;

    const QString dataJson =
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    // ── CSS
    const QString css = QStringLiteral(R"CSS(
        * { box-sizing: border-box; }
        html, body {
            margin: 0; padding: 0; min-height: 100%;
            background: #0c0c0e; color: #e6e8ec;
            font-family: 'Segoe UI', 'Inter', Ubuntu, 'Noto Sans',
                         Cantarell, 'DejaVu Sans', sans-serif;
            font-size: 13px;
            -webkit-font-smoothing: antialiased;
        }
        .page { max-width: 1180px; margin: 0 auto; padding: 40px 28px 80px; }

        /* ── Header ───────────────────────────────────────────── */
        .header {
            display: flex; align-items: center; gap: 14px;
            margin-bottom: 26px;
        }
        .icon {
            width: 44px; height: 44px; border-radius: 12px;
            background: #17181b;
            border: 1px solid #23262c;
            display: flex; align-items: center; justify-content: center;
            flex-shrink: 0;
        }
        .icon svg { width: 22px; height: 22px; color: #b4b8c2; }
        .titleBlock { flex: 1; min-width: 0; }
        .h1 {
            margin: 0; font-size: 24px; font-weight: 500;
            letter-spacing: -0.3px; color: #f4f5f7;
        }
        .subtitle {
            color: #6b7280; font-size: 12px; margin-top: 2px;
        }
        .actions { display: flex; gap: 10px; flex-shrink: 0; }
        .btn {
            background: #1c1d21; color: #d7d9de;
            border: 1px solid #2c2e34; border-radius: 10px;
            padding: 8px 14px; font-size: 12px; cursor: pointer;
            transition: background 0.12s, border-color 0.12s;
            display: flex; align-items: center; gap: 6px;
        }
        .btn:hover { background: #23262c; border-color: #3a3d45; }
        .btn.danger:hover {
            background: #4a2020; border-color: #7a2d2d; color: #ffb3b3;
        }
        .btn svg { width: 14px; height: 14px; }

        /* ── Stats bar ─────────────────────────────────────────── */
        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
            gap: 10px;
            margin-bottom: 22px;
        }
        .stat {
            background: #141519;
            border: 1px solid #23262c;
            border-radius: 10px;
            padding: 12px 14px;
        }
        .stat .label {
            color: #6b7280; font-size: 11px;
            text-transform: uppercase; letter-spacing: 0.5px;
            margin-bottom: 4px;
        }
        .stat .value {
            color: #f1f2f5; font-size: 20px; font-weight: 500;
            letter-spacing: -0.3px;
        }
        .stat .value small {
            color: #9aa0ac; font-size: 12px; font-weight: 400;
            margin-left: 4px; letter-spacing: 0;
        }

        /* ── Toolbar (search + filter) ────────────────────────── */
        .toolbar {
            display: flex; gap: 10px; align-items: center;
            margin-bottom: 18px;
            position: sticky; top: 0; z-index: 20;
            background: #0c0c0e;
            padding: 12px 0;
            margin-left: -28px; margin-right: -28px;
            padding-left: 28px; padding-right: 28px;
            border-bottom: 1px solid #1a1c22;
        }
        .search-wrap {
            flex: 1; position: relative;
        }
        .search-wrap svg {
            position: absolute; left: 12px; top: 50%;
            transform: translateY(-50%);
            width: 15px; height: 15px;
            color: #6b7280; pointer-events: none;
        }
        .input {
            width: 100%;
            background: #141519;
            border: 1px solid #2c2e34;
            border-radius: 10px;
            padding: 10px 14px 10px 36px;
            color: #e6e8ec; font-size: 13px;
            outline: none;
            transition: border-color 0.15s, background 0.15s;
        }
        .input:focus { border-color: #5d9df1; background: #17181b; }
        .input::placeholder { color: #5c606b; }
        .select {
            background: #141519; color: #c8ccd6;
            border: 1px solid #2c2e34; border-radius: 10px;
            padding: 10px 14px; font-size: 12px;
            cursor: pointer; outline: none;
        }
        .select:hover { background: #1c1d21; }

        /* ── Top sites ─────────────────────────────────────────── */
        .topSites {
            margin-bottom: 28px;
        }
        .sectionTitle {
            color: #9aa0ac; font-size: 11px;
            text-transform: uppercase; letter-spacing: 0.8px;
            margin-bottom: 12px;
        }
        .topGrid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
            gap: 10px;
        }
        .topCard {
            background: #141519;
            border: 1px solid #23262c;
            border-radius: 10px;
            padding: 12px;
            cursor: pointer;
            transition: all 0.12s;
            display: flex; flex-direction: column;
            align-items: center; gap: 8px;
            text-align: center;
        }
        .topCard:hover {
            background: #1c1d21;
            border-color: #2e3138;
            transform: translateY(-1px);
        }
        .topFav {
            width: 32px; height: 32px; border-radius: 8px;
            background: #23262c;
            display: flex; align-items: center; justify-content: center;
            overflow: hidden;
        }
        .topFav img { width: 100%; height: 100%; object-fit: contain; }
        .topFav .letter {
            color: #9aa0ac; font-weight: 600; font-size: 14px;
        }
        .topName {
            color: #e6e8ec; font-size: 12px; font-weight: 500;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
            max-width: 100%;
        }
        .topCount {
            color: #6b7280; font-size: 10px;
        }

        /* ── History list ──────────────────────────────────────── */
        .dayGroup {
            margin-bottom: 22px;
        }
        .dayHeader {
            display: flex; align-items: center; gap: 10px;
            padding: 6px 0 10px;
            cursor: pointer;
            user-select: none;
        }
        .dayHeader .caret {
            color: #6b7280; font-size: 10px;
            transition: transform 0.15s;
            display: inline-block;
        }
        .dayHeader.collapsed .caret { transform: rotate(-90deg); }
        .dayLabel {
            color: #d7d9de; font-size: 13px; font-weight: 500;
        }
        .dayCount {
            color: #6b7280; font-size: 11px;
            background: #17181b; padding: 2px 8px; border-radius: 10px;
        }
        .dayHeader .spacer { flex: 1; }
        .dayDeleteAll {
            background: transparent; border: none;
            color: #5c606b; font-size: 11px; cursor: pointer;
            padding: 4px 8px; border-radius: 6px;
            opacity: 0;
            transition: opacity 0.12s, background 0.12s, color 0.12s;
        }
        .dayHeader:hover .dayDeleteAll { opacity: 1; }
        .dayDeleteAll:hover {
            background: #2e3138; color: #e0443b;
        }

        .entries { display: flex; flex-direction: column; gap: 2px; }
        .entries.collapsed { display: none; }

        .entry {
            display: flex; align-items: center; gap: 12px;
            padding: 9px 12px;
            background: transparent;
            border-radius: 8px;
            cursor: pointer;
            transition: background 0.1s;
            position: relative;
        }
        .entry:hover { background: #141519; }
        .entryFav {
            width: 20px; height: 20px; flex-shrink: 0;
            display: flex; align-items: center; justify-content: center;
            overflow: hidden;
        }
        .entryFav img { width: 100%; height: 100%; object-fit: contain; }
        .entryFav .letter {
            color: #6b7280; font-weight: 600; font-size: 11px;
            text-transform: uppercase;
        }
        .entryTime {
            color: #6b7280; font-size: 11px;
            font-variant-numeric: tabular-nums;
            flex-shrink: 0; width: 52px;
        }
        .entryMeta { flex: 1; min-width: 0; }
        .entryTitle {
            color: #e6e8ec; font-size: 13px;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
            line-height: 1.35;
        }
        .entryUrl {
            color: #5c606b; font-size: 11px;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
            margin-top: 1px;
        }
        .entryCount {
            color: #6b7280; font-size: 10px;
            background: #17181b; padding: 2px 7px; border-radius: 8px;
            flex-shrink: 0;
        }
        .entryCount.hidden { display: none; }
        .entryDelete {
            width: 24px; height: 24px; border-radius: 6px;
            background: transparent; border: none;
            color: #6b7280; cursor: pointer;
            display: flex; align-items: center; justify-content: center;
            flex-shrink: 0; opacity: 0;
            transition: opacity 0.1s, background 0.1s, color 0.1s;
        }
        .entry:hover .entryDelete { opacity: 1; }
        .entryDelete:hover { background: #2e3138; color: #e0443b; }
        .entryDelete svg { width: 12px; height: 12px; }

        /* ── Empty states ──────────────────────────────────────── */
        .empty {
            text-align: center; padding: 80px 20px;
            color: #9aa0ac;
        }
        .empty svg {
            width: 56px; height: 56px;
            color: #2c2e34; margin-bottom: 18px;
        }
        .empty h2 {
            color: #f1f2f5; font-weight: 400;
            font-size: 18px; margin: 0 0 6px 0;
        }
        .empty p { margin: 0; font-size: 13px; color: #6b7280; }

        /* ── Highlight matched text ────────────────────────────── */
        mark {
            background: transparent; color: #7ab0ff;
            font-weight: 500;
            padding: 0;
        }

        /* ── Footer ────────────────────────────────────────────── */
        .footer {
            margin-top: 40px; padding-top: 20px;
            border-top: 1px solid #1a1c22;
            text-align: center;
            color: #4d525d; font-size: 11px;
        }
    )CSS");

    // ── HTML
    const QString html = QStringLiteral(R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>History — RootBrowser</title>
<style>%1</style>
</head><body>
<div class="page">

    <!-- Header -->
    <div class="header">
        <div class="icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
                <circle cx="12" cy="12" r="9"/>
                <polyline points="12 7 12 12 15 14"/>
            </svg>
        </div>
        <div class="titleBlock">
            <h1 class="h1">History</h1>
            <div class="subtitle" id="subtitle">Loading…</div>
        </div>
        <div class="actions">
            <button class="btn" id="clearBtn">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                     stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="3 6 5 6 21 6"/>
                    <path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>
                </svg>
                Clear…
            </button>
        </div>
    </div>

    <!-- Stats -->
    <div class="stats" id="stats"></div>

    <!-- Top sites -->
    <div class="topSites" id="topSites"></div>

    <!-- Toolbar -->
    <div class="toolbar">
        <div class="search-wrap">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <circle cx="11" cy="11" r="7"/>
                <line x1="21" y1="21" x2="16.65" y2="16.65"/>
            </svg>
            <input id="search" class="input" type="text"
                   placeholder="Search history…" autocomplete="off">
        </div>
        <select id="range" class="select">
            <option value="all">All time</option>
            <option value="today">Today</option>
            <option value="week">This week</option>
            <option value="month">This month</option>
        </select>
        <select id="sort" class="select">
            <option value="recent">Recent first</option>
            <option value="oldest">Oldest first</option>
            <option value="alpha">A → Z</option>
            <option value="visits">Most visited</option>
        </select>
    </div>

    <!-- History list -->
    <div id="content"></div>

    <div class="footer">
        RootBrowser · History capped at 5000 entries
    </div>
</div>

<script>
const DATA = %2;
const ALL = DATA.items || [];
const TOP = DATA.top || [];
const STATS = DATA.stats || {};

const contentEl  = document.getElementById('content');
const searchEl   = document.getElementById('search');
const rangeEl    = document.getElementById('range');
const sortEl     = document.getElementById('sort');
const statsEl    = document.getElementById('stats');
const topEl      = document.getElementById('topSites');
const subtitleEl = document.getElementById('subtitle');

// ── Helpers ─────────────────────────────────────────────────────────────
function esc(s) {
    return String(s == null ? '' : s)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function escRegex(s) {
    return String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function highlight(text, query) {
    const t = esc(text);
    if (!query) return t;
    const re = new RegExp('(' + escRegex(esc(query)) + ')', 'gi');
    return t.replace(re, '<mark>$1</mark>');
}

function domainOf(url) {
    try {
        const u = new URL(url);
        return u.hostname.replace(/^www\./, '');
    } catch (e) {
        return url || '';
    }
}

function fullDomain(url) {
    try {
        const u = new URL(url);
        let path = u.pathname;
        if (path.length > 42) path = path.substring(0, 42) + '…';
        return u.hostname.replace(/^www\./, '') + (u.search ? u.search.substring(0, 30) : '') + path;
    } catch (e) { return url; }
}

function humanBytes(n) {
    if (!n || n < 0) return '0 B';
    const u = ['B','KB','MB','GB'];
    let v = n, i = 0;
    while (v >= 1024 && i < 3) { v /= 1024; i++; }
    return v.toFixed(i === 0 ? 0 : 1) + ' ' + u[i];
}

function plural(n, word) {
    return n + ' ' + word + (n === 1 ? '' : 's');
}

// ── Date helpers ────────────────────────────────────────────────────────
function startOfDay(d) {
    const c = new Date(d);
    c.setHours(0, 0, 0, 0);
    return c;
}

function dayLabel(ts) {
    const now = new Date();
    const then = new Date(ts * 1000);

    const today = startOfDay(now);
    const thatDay = startOfDay(then);
    const diffDays = Math.floor((today - thatDay) / (24 * 3600 * 1000));

    if (diffDays === 0) return 'Today';
    if (diffDays === 1) return 'Yesterday';
    if (diffDays < 7) return then.toLocaleDateString(undefined, { weekday: 'long' });
    if (then.getFullYear() === now.getFullYear())
        return then.toLocaleDateString(undefined, { month: 'long', day: 'numeric' });
    return then.toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' });
}

function dayKey(ts) {
    const d = new Date(ts * 1000);
    return d.getFullYear() + '-' + (d.getMonth()+1) + '-' + d.getDate();
}

function timeOf(ts) {
    const d = new Date(ts * 1000);
    return d.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit', hour12: false });
}

// ── Stats rendering ─────────────────────────────────────────────────────
function renderStats() {
    const total = STATS.total || 0;
    const week  = STATS.thisWeek || 0;
    const sites = STATS.uniqueSites || 0;
    const size  = STATS.fileSize || 0;

    statsEl.innerHTML = `
        <div class="stat">
            <div class="label">Total visits</div>
            <div class="value">${total.toLocaleString()}</div>
        </div>
        <div class="stat">
            <div class="label">This week</div>
            <div class="value">${week.toLocaleString()}</div>
        </div>
        <div class="stat">
            <div class="label">Unique sites</div>
            <div class="value">${sites.toLocaleString()}</div>
        </div>
        <div class="stat">
            <div class="label">Storage</div>
            <div class="value">${humanBytes(size)}</div>
        </div>
    `;

    subtitleEl.textContent = total === 0
        ? 'No browsing history'
        : plural(total, 'visit') + ' · ' + plural(sites, 'site');
}

// ── Top sites rendering ─────────────────────────────────────────────────
function renderTop() {
    if (TOP.length === 0) {
        topEl.innerHTML = '';
        return;
    }

    let html = '<div class="sectionTitle">Most visited</div><div class="topGrid">';
    for (const t of TOP) {
        const letter = (t.host || '?')[0].toUpperCase();
        const favHTML = t.favicon
            ? `<img src="${esc(t.favicon)}" alt="" onerror="this.style.display='none';this.nextElementSibling.style.display='block'"><span class="letter" style="display:none">${letter}</span>`
            : `<span class="letter">${letter}</span>`;
        html += `
            <div class="topCard" data-url="${esc(t.url)}">
                <div class="topFav">${favHTML}</div>
                <div class="topName">${esc(t.host)}</div>
                <div class="topCount">${plural(t.visits, 'visit')}</div>
            </div>`;
    }
    html += '</div>';
    topEl.innerHTML = html;

    document.querySelectorAll('.topCard').forEach(card => {
        card.addEventListener('click', () => {
            const url = card.getAttribute('data-url');
            if (url) window.location.href = 'rootbrowser-goto:' + url;
        });
    });
}

// ── Filtering ───────────────────────────────────────────────────────────
function rangeCutoff() {
    const now = Math.floor(Date.now() / 1000);
    switch (rangeEl.value) {
        case 'today': return now - 24*3600;
        case 'week':  return now - 7*24*3600;
        case 'month': return now - 30*24*3600;
        default:      return 0;
    }
}

function filtered() {
    const q = searchEl.value.trim().toLowerCase();
    const cutoff = rangeCutoff();

    let list = ALL.filter(e => {
        if (e.visitedAt < cutoff) return false;
        if (q) {
            const t = (e.title || '').toLowerCase();
            const u = (e.url   || '').toLowerCase();
            if (!t.includes(q) && !u.includes(q)) return false;
        }
        return true;
    });

    // Sort
    switch (sortEl.value) {
        case 'oldest':
            list.sort((a,b) => (a.visitedAt||0) - (b.visitedAt||0));
            break;
        case 'alpha':
            list.sort((a,b) => (a.title || a.url).localeCompare(b.title || b.url));
            break;
        case 'visits':
            list.sort((a,b) => (b.visitCount||0) - (a.visitCount||0) || (b.visitedAt||0) - (a.visitedAt||0));
            break;
        case 'recent':
        default:
            list.sort((a,b) => (b.visitedAt||0) - (a.visitedAt||0));
    }
    return list;
}

// ── Group by day ────────────────────────────────────────────────────────
function groupByDay(list) {
    const groups = [];
    let curKey = null;
    let curGroup = null;

    for (const e of list) {
        const k = dayKey(e.visitedAt);
        if (k !== curKey) {
            curKey = k;
            curGroup = { key: k, label: dayLabel(e.visitedAt), entries: [] };
            groups.push(curGroup);
        }
        curGroup.entries.push(e);
    }
    return groups;
}

// ── Main render ─────────────────────────────────────────────────────────
function render() {
    const q = searchEl.value.trim();
    const list = filtered();

    if (list.length === 0) {
        if (ALL.length === 0) {
            contentEl.innerHTML = `
                <div class="empty">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                         stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                        <circle cx="12" cy="12" r="9"/>
                        <polyline points="12 7 12 12 15 14"/>
                    </svg>
                    <h2>No history yet</h2>
                    <p>Pages you visit will appear here.</p>
                </div>`;
        } else {
            contentEl.innerHTML = `
                <div class="empty">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                         stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                        <circle cx="11" cy="11" r="7"/>
                        <line x1="21" y1="21" x2="16.65" y2="16.65"/>
                    </svg>
                    <h2>No matches</h2>
                    <p>Try a different search or time range.</p>
                </div>`;
        }
        return;
    }

    const groups = groupByDay(list);
    let html = '';

    for (const g of groups) {
        html += `
        <div class="dayGroup">
            <div class="dayHeader" data-day="${esc(g.key)}">
                <span class="caret">▼</span>
                <span class="dayLabel">${esc(g.label)}</span>
                <span class="dayCount">${g.entries.length}</span>
                <span class="spacer"></span>
                <button class="dayDeleteAll" data-day="${esc(g.key)}"
                        title="Delete this day">
                    Delete day
                </button>
            </div>
            <div class="entries" data-day="${esc(g.key)}">`;

        for (const e of g.entries) {
            const dom = domainOf(e.url);
            const letter = (dom[0] || '?').toUpperCase();
            const favHTML = e.favicon
                ? `<img src="${esc(e.favicon)}" alt="" onerror="this.style.display='none';this.nextElementSibling.style.display='block'"><span class="letter" style="display:none">${letter}</span>`
                : `<span class="letter">${letter}</span>`;

            const showCount = e.visitCount && e.visitCount > 1;

            html += `
                <div class="entry" data-url="${esc(e.url)}">
                    <div class="entryFav">${favHTML}</div>
                    <div class="entryTime">${timeOf(e.visitedAt)}</div>
                    <div class="entryMeta">
                        <div class="entryTitle">${highlight(e.title || e.url, q)}</div>
                        <div class="entryUrl">${highlight(fullDomain(e.url), q)}</div>
                    </div>
                    <div class="entryCount ${showCount ? '' : 'hidden'}"
                         title="Visit count">
                        ${showCount ? e.visitCount + '×' : ''}
                    </div>
                    <button class="entryDelete" data-url="${esc(e.url)}"
                            title="Remove from history">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                             stroke-width="2" stroke-linecap="round">
                            <line x1="6" y1="6" x2="18" y2="18"/>
                            <line x1="6" y1="18" x2="18" y2="6"/>
                        </svg>
                    </button>
                </div>`;
        }
        html += `</div></div>`;
    }

    contentEl.innerHTML = html;
    wireEvents();
}

// ── Event wiring ────────────────────────────────────────────────────────
function wireEvents() {
    // Collapse/expand day groups
    document.querySelectorAll('.dayHeader').forEach(header => {
        header.addEventListener('click', (ev) => {
            if (ev.target.closest('.dayDeleteAll')) return;
            const day = header.getAttribute('data-day');
            const entries = document.querySelector(`.entries[data-day="${day}"]`);
            if (!entries) return;
            header.classList.toggle('collapsed');
            entries.classList.toggle('collapsed');
        });
    });

    // Delete a whole day
    document.querySelectorAll('.dayDeleteAll').forEach(btn => {
        btn.addEventListener('click', (ev) => {
            ev.stopPropagation();
            const day = btn.getAttribute('data-day');
            if (!confirm('Delete all history for this day?')) return;
            window.location.href = 'rootbrowser-history-day-del:' + day;
        });
    });

    // Click entry → navigate
    document.querySelectorAll('.entry').forEach(entry => {
        entry.addEventListener('click', (ev) => {
            if (ev.target.closest('.entryDelete')) return;
            const url = entry.getAttribute('data-url');
            if (url) window.location.href = 'rootbrowser-goto:' + url;
        });
    });

    // Delete single entry
    document.querySelectorAll('.entryDelete').forEach(btn => {
        btn.addEventListener('click', (ev) => {
            ev.stopPropagation();
            const url = btn.getAttribute('data-url');
            if (!url) return;
            window.location.href = 'rootbrowser-history-del:' + encodeURIComponent(url);
        });
    });
}

// ── Toolbar events ──────────────────────────────────────────────────────
searchEl.addEventListener('input', render);
rangeEl.addEventListener('change', render);
sortEl.addEventListener('change', render);

document.getElementById('clearBtn').addEventListener('click', () => {
    const choice = prompt(
        'Clear history:\n\n' +
        '  Type "all" for everything\n' +
        '  Type "hour" for last hour\n' +
        '  Type "today" for today\n' +
        '  Type "week" for this week\n' +
        '  Type "month" for this month\n\n' +
        'Or press Cancel to abort.',
        'all');

    if (!choice) return;
    const c = choice.trim().toLowerCase();

    let path = '';
    switch (c) {
        case 'all':   path = 'rootbrowser-history-clear:all';   break;
        case 'hour':  path = 'rootbrowser-history-clear:hour';  break;
        case 'today': path = 'rootbrowser-history-clear:today'; break;
        case 'week':  path = 'rootbrowser-history-clear:week';  break;
        case 'month': path = 'rootbrowser-history-clear:month'; break;
        default: return;
    }
    window.location.href = path;
});

// ── Initial render ──────────────────────────────────────────────────────
renderStats();
renderTop();
render();
setTimeout(() => searchEl.focus(), 80);
</script>
</body></html>)HTML")
        .arg(css, dataJson);

    return html;
}

} // namespace HistoryPage
