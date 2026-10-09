// ============================================================================
//  fingerprintprotection.h — Canvas/WebGL/fonts anti-fingerprinting.
// ============================================================================
#pragma once

#include <QString>

namespace FingerprintProtection {

// Returns JS to inject at document creation.
// Normalizes fingerprintable surfaces: canvas, WebGL, audio, timezone, fonts.
QString injectScript();

} // namespace FingerprintProtection
