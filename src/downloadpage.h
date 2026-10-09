// ============================================================================
//  downloadpage.h — Build the rootbrowser://downloads page HTML.
// ============================================================================
#pragma once

#include <QString>

namespace DownloadPage {

// Build the full HTML for the downloads page.
// Downloads are serialized to JSON and embedded in the page.
// Actions (pause/resume/cancel/retry/remove/open/folder) are triggered via
// custom URL schemes handled by main.cpp:
//     rootbrowser-dl-pause:<id>
//     rootbrowser-dl-resume:<id>
//     rootbrowser-dl-cancel:<id>
//     rootbrowser-dl-remove:<id>
//     rootbrowser-dl-retry:<id>
//     rootbrowser-dl-open:<id>
//     rootbrowser-dl-folder:<id>
//     rootbrowser-dl-folder:pick
//     rootbrowser-dl-clear:all
QString build();

} // namespace DownloadPage
