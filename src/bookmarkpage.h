// ============================================================================
//  bookmarkpage.h — Build the rootbrowser://bookmarks page HTML.
// ============================================================================
#pragma once

#include <QString>

namespace BookmarkPage {

// Build the full HTML for the bookmarks page.
// Bookmarks are serialized to JSON and embedded in the page.
QString build();

} // namespace BookmarkPage
