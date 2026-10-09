// ============================================================================
//  background.h — Graphite mountain wallpaper for the home tab.
// ============================================================================
#pragma once

#include <QWidget>

namespace AppBackground {

// Generate the mountain background as a QPixmap at the given size.
// Cached — repeated calls with same size are cheap.
QPixmap render(int width, int height);

// Draw directly into a painter (no caching).
void paint(QPainter& p, int width, int height);

// Clear the internal cache (call on theme change).
void clearCache();

} // namespace AppBackground
