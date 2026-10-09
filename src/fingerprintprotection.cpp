// ============================================================================
//  fingerprintprotection.cpp — Anti-fingerprinting JS.
// ============================================================================

#include "fingerprintprotection.h"

namespace FingerprintProtection {

QString injectScript() {
    return QStringLiteral(R"JS(
(function() {
    'use strict';

    // ────────────────────────────────────────────────────────────────
    //  1) Canvas fingerprinting — add tiny consistent noise
    // ────────────────────────────────────────────────────────────────
    const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
    const originalToBlob    = HTMLCanvasElement.prototype.toBlob;
    const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;

    function addNoise(canvas) {
        try {
            const ctx = canvas.getContext('2d');
            if (!ctx) return;
            const w = canvas.width, h = canvas.height;
            if (w === 0 || h === 0) return;
            // Small deterministic noise seeded by size + a fixed salt
            const img = ctx.getImageData(0, 0, Math.min(w, 16), Math.min(h, 16));
            for (let i = 0; i < img.data.length; i += 4) {
                // ±1 on RGB — imperceptible, breaks fingerprint
                img.data[i]     = (img.data[i]     + (i % 2)) & 0xFF;
                img.data[i + 1] = (img.data[i + 1] + ((i+1) % 2)) & 0xFF;
            }
            ctx.putImageData(img, 0, 0);
        } catch (e) {}
    }

    HTMLCanvasElement.prototype.toDataURL = function() {
        try { addNoise(this); } catch (e) {}
        return originalToDataURL.apply(this, arguments);
    };
    HTMLCanvasElement.prototype.toBlob = function() {
        try { addNoise(this); } catch (e) {}
        return originalToBlob.apply(this, arguments);
    };
    CanvasRenderingContext2D.prototype.getImageData = function() {
        try { addNoise(this.canvas); } catch (e) {}
        return originalGetImageData.apply(this, arguments);
    };

    // ────────────────────────────────────────────────────────────────
    //  2) WebGL — spoof vendor + renderer
    // ────────────────────────────────────────────────────────────────
    const getParameterProto = WebGLRenderingContext.prototype.getParameter;
    WebGLRenderingContext.prototype.getParameter = function(p) {
        // UNMASKED_VENDOR_WEBGL = 0x9245
        if (p === 0x9245) return 'RootBrowser';
        // UNMASKED_RENDERER_WEBGL = 0x9246
        if (p === 0x9246) return 'RootBrowser Privacy Engine';
        // VENDOR = 0x1F00 / RENDERER = 0x1F01
        if (p === 0x1F00) return 'RootBrowser';
        if (p === 0x1F01) return 'RootBrowser Privacy Engine';
        return getParameterProto.apply(this, arguments);
    };

    // Same for WebGL2
    if (window.WebGL2RenderingContext) {
        const getParam2 = WebGL2RenderingContext.prototype.getParameter;
        WebGL2RenderingContext.prototype.getParameter = function(p) {
            if (p === 0x9245) return 'RootBrowser';
            if (p === 0x9246) return 'RootBrowser Privacy Engine';
            if (p === 0x1F00) return 'RootBrowser';
            if (p === 0x1F01) return 'RootBrowser Privacy Engine';
            return getParam2.apply(this, arguments);
        };
    }

    // ────────────────────────────────────────────────────────────────
    //  3) AudioContext — add tiny noise to oscillator output
    // ────────────────────────────────────────────────────────────────
    if (window.AudioBuffer) {
        const origGetChannelData = AudioBuffer.prototype.getChannelData;
        AudioBuffer.prototype.getChannelData = function() {
            const data = origGetChannelData.apply(this, arguments);
            if (data && data.length > 0) {
                // ±1e-7 noise — imperceptible but breaks fingerprint
                for (let i = 0; i < Math.min(data.length, 64); ++i) {
                    data[i] = data[i] + (Math.random() - 0.5) * 1e-7;
                }
            }
            return data;
        };
    }

    // ────────────────────────────────────────────────────────────────
    //  4) Timezone — spoof to UTC
    // ────────────────────────────────────────────────────────────────
    try {
        const origResolvedOptions = Intl.DateTimeFormat.prototype.resolvedOptions;
        Intl.DateTimeFormat.prototype.resolvedOptions = function() {
            const opts = origResolvedOptions.apply(this, arguments);
            opts.timeZone = 'UTC';
            return opts;
        };
        Date.prototype.getTimezoneOffset = function() { return 0; };
    } catch (e) {}

    // ────────────────────────────────────────────────────────────────
    //  5) Navigator — reduce hardware info leaks
    // ────────────────────────────────────────────────────────────────
    try {
        Object.defineProperty(navigator, 'hardwareConcurrency', {
            get: () => 4, configurable: true
        });
        Object.defineProperty(navigator, 'deviceMemory', {
            get: () => 8, configurable: true
        });
        Object.defineProperty(navigator, 'platform', {
            get: () => 'Linux x86_64', configurable: true
        });
    } catch (e) {}

    // ────────────────────────────────────────────────────────────────
    //  6) Battery API — spoof static values
    // ────────────────────────────────────────────────────────────────
    try {
        if (navigator.getBattery) {
            navigator.getBattery = function() {
                return Promise.resolve({
                    charging: true, chargingTime: 0,
                    dischargingTime: Infinity, level: 1.0,
                    onchargingchange: null,
                    onchargingtimechange: null,
                    ondischargingtimechange: null,
                    onlevelchange: null,
                    addEventListener: function() {},
                    removeEventListener: function() {}
                });
            };
        }
    } catch (e) {}

    // ────────────────────────────────────────────────────────────────
    //  7) Screen — clamp to common resolution
    // ────────────────────────────────────────────────────────────────
    try {
        Object.defineProperty(screen, 'colorDepth', { get: () => 24 });
        Object.defineProperty(screen, 'pixelDepth', { get: () => 24 });
    } catch (e) {}

})();
)JS");
}

} // namespace FingerprintProtection
