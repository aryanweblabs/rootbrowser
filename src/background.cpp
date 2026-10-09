// ============================================================================
//  background.cpp — Loads embedded background.png wallpaper.
// ============================================================================

#include "background.h"
#include "settingsstore.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPixmap>
#include <QImage>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <QDateTime>

namespace AppBackground {

// ============================================================================
//  Lazy-initialized caches (QGuiApplication-safe)
// ============================================================================
static QPixmap& basePngRef() {
    static QPixmap pm;
    return pm;
}

static bool& baseTriedRef() {
    static bool tried = false;
    return tried;
}

static QHash<QString, QPixmap>& cacheRef() {
    static QHash<QString, QPixmap> cache;
    return cache;
}

// ============================================================================
//  PNG search paths
// ============================================================================
static QStringList pngSearchPaths() {
    QStringList paths;

    // 1. Embedded resources (rotation — 2 wallpapers)
    paths << ":/background.png";
    paths << ":/nighttheme.png";

    // 2. Next to executable
    const QString appDir = QCoreApplication::applicationDirPath();
    paths << appDir + "/background.png";
    paths << appDir + "/assets/background.png";

    // 3. Working directory
    paths << QDir::currentPath() + "/background.png";
    paths << QDir::currentPath() + "/assets/background.png";

    // 4. User config dir
    paths << QDir::homePath() + "/.config/rootbrowser/background.png";

    return paths;
}

// ============================================================================
//  Load PNG (lazy)
// ============================================================================
static QPixmap loadBasePng() {
    QPixmap& base  = basePngRef();
    bool&    tried = baseTriedRef();

    if (tried) return base;
    tried = true;

    // Read user preference
    QString preference = SettingsStore::instance().getString("appearance.wallpaper", "random");

    // Determine which paths to try (in order)
    QStringList paths;
    if (preference == "sunset") {
        paths << ":/background.png";
    } else if (preference == "night") {
        paths << ":/nighttheme.png";
    } else {
        // Random — shuffle both
        paths << ":/background.png";
        paths << ":/nighttheme.png";
        for (int i = paths.size() - 1; i > 0; --i) {
            int j = QRandomGenerator::global()->bounded(i + 1);
            paths.swapItemsAt(i, j);
        }
    }

    // Try loading
    for (const QString& path : paths) {
        QFile f(path);
        if (!f.exists() && !path.startsWith(":/")) continue;

        QPixmap pm;
        if (pm.load(path)) {
            base = pm;
            qDebug("[Background] Loaded (%s): %s",
                   qPrintable(preference), qPrintable(path));
            return base;
        }
    }

    qDebug("[Background] No PNG loaded — using fallback");
    return QPixmap();
}

// ============================================================================
//  Clear cache
// ============================================================================
void clearCache() {
    cacheRef().clear();
    baseTriedRef() = false;
    basePngRef() = QPixmap();
}

// ============================================================================
//  Compose scene
// ============================================================================
static void composeScene(QPainter& p, int w, int h) {
    const QPixmap base = loadBasePng();

    if (!base.isNull()) {
        // Scale to cover (like CSS background-size: cover)
        QPixmap scaled = base.scaled(QSize(w, h),
                                     Qt::KeepAspectRatioByExpanding,
                                     Qt::SmoothTransformation);

        const int xOff = (scaled.width()  - w) / 2;
        const int yOff = (scaled.height() - h) / 2;

        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(QRect(0, 0, w, h), scaled, QRect(xOff, yOff, w, h));
    } else {
        // Fallback: graphite gradient
        QLinearGradient g(0, 0, 0, h);
        g.setColorAt(0.0, QColor("#0a0b0e"));
        g.setColorAt(0.6, QColor("#0f1116"));
        g.setColorAt(1.0, QColor("#171a21"));
        p.fillRect(QRect(0, 0, w, h), g);
    }

    // Readability vignette (top + bottom)
    {
        QLinearGradient top(0, 0, 0, h * 0.15);
        top.setColorAt(0.0, QColor(0, 0, 0, 90));
        top.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(QRect(0, 0, w, int(h * 0.22) + 1), top);

        QLinearGradient bot(0, h * 0.30, 0, h);
        bot.setColorAt(0.0, QColor(0, 0, 0, 0));
        bot.setColorAt(0.5, QColor(0, 0, 0, 40));
        bot.setColorAt(0.8, QColor(0, 0, 0, 100));
        bot.setColorAt(1.0, QColor(0, 0, 0, 180));
        p.fillRect(QRect(0, int(h * 0.30), w, h - int(h * 0.30) + 1), bot);

        // Extra bottom strip for footer hints
        QLinearGradient strip(0, h - 60, 0, h);
        strip.setColorAt(0.0, QColor(0, 0, 0, 0));
        strip.setColorAt(1.0, QColor(0, 0, 0, 140));
        p.fillRect(QRect(0, h - 60, w, 60), strip);
    }

    // Subtle blue tint
    {
        QLinearGradient tint(0, 0, 0, h);
        tint.setColorAt(0.0, QColor(20, 30, 60, 12));
        tint.setColorAt(0.5, QColor(15, 20, 40, 6));
        tint.setColorAt(1.0, QColor(10, 15, 30, 16));
        p.fillRect(QRect(0, 0, w, h), tint);
    }
}

// ============================================================================
//  Public API
// ============================================================================
void paint(QPainter& p, int w, int h) {
    if (w <= 0 || h <= 0) return;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    composeScene(p, w, h);
}

QPixmap render(int w, int h) {
    if (w <= 0 || h <= 0) return QPixmap();

    auto& cache = cacheRef();
    const QString key = QString("%1x%2").arg(w).arg(h);

    auto it = cache.find(key);
    if (it != cache.end()) return *it;

    QPixmap pm(w, h);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    paint(p, w, h);
    p.end();

    cache.insert(key, pm);
    return pm;
}

} // namespace AppBackground
