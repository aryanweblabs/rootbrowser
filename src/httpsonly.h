// ============================================================================
//  httpsonly.h — HTTPS-Only mode (auto-upgrade + block HTTP).
// ============================================================================
#pragma once

#include <QString>
#include <QUrl>

class QWebEnginePage;

namespace HttpsOnly {

// Check if HTTPS-Only mode is enabled (from settings)
bool isEnabled();

// Set the toggle
void setEnabled(bool enabled);

// Called when a navigation request happens. Returns:
//   - QUrl() (empty) → allow as-is
//   - QUrl(newUrl)   → redirect to this URL (auto-upgrade)
//   - QUrl("blocked://") → block and show warning
QUrl handleNavigation(const QUrl& url, QWebEnginePage* page);

// Check if the given URL has been blocked (for showing an interstitial)
bool isBlocked(const QUrl& url);

// Show a warning interstitial page
QString blockPageHtml(const QUrl& originalUrl);

// Clear blocked-URL cache
void clearBlockedCache();

} // namespace HttpsOnly
