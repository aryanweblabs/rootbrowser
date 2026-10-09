<div align="center">

# 🌐 RootBrowser

### A modern, privacy-focused web browser built with Qt6 & Chromium

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6.2%2B-green.svg)](https://qt.io/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)]()
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)

**Fast · Private · Beautiful · Open Source**

[Features](#-features) · [Screenshots](#-screenshots) · [Build](#-build-from-source) · [Contributing](#-contributing)

</div>

---

## ✨ Features

<table>
<tr>
<td width="50%">

### 🔒 Privacy First
- **Tor Private Mode** — Browse anonymously through 3-hop Tor circuits
- **Ad & Tracker Blocker** — 100+ curated filter rules, zero config
- **Fingerprint Protection** — Canvas, WebGL, Audio, Timezone spoofing
- **HTTPS-Only Mode** — Auto-upgrade + block insecure sites
- **Do Not Track** — Optional DNT header

</td>
<td width="50%">

### ⚡ Modern UX
- **Chromium-based** — Full web compatibility
- **Command Palette** — `Ctrl+K` for instant actions
- **URL Autocomplete** — Smart suggestions from history & bookmarks
- **Find in Page** — Chrome-style search bar
- **Rotating Wallpapers** — Sunset & night themes

</td>
</tr>
<tr>
<td width="50%">

### 📥 Download Manager
- Pause / Resume / Cancel / Retry
- Live speed & ETA tracking
- Native OS notifications
- In-app sliding toasts
- Persistent across restarts

</td>
<td width="50%">

### 📚 Bookmarks & History
- Persistent JSON storage
- Full-text search
- Day-grouped history view
- Most-visited top sites
- Import/export friendly

</td>
</tr>
</table>

---

## 📸 Screenshots

### 🏠 Home Page
![RootBrowser Home](screenshots/home.png)

### 📑 Bookmarks Manager
![Bookmarks](screenshots/bookmarks.png)

### 📥 Downloads
![Downloads](screenshots/downloads.png)

### 🕵️ Private Mode (Tor)
![Private Mode](screenshots/private.png)

### ⚙️ Settings
![Settings](screenshots/settings.png)

---

## 🛠️ Build from Source

### Prerequisites

| Requirement | Version |
|-------------|---------|
| C++ Compiler | GCC 9+ / Clang 10+ / MSVC 2019+ |
| Qt | 6.2 or newer |
| QtWebEngine | Included with Qt 6 |
| Git | Any recent version |

### 🐧 Linux (Debian / Ubuntu)

```bash
# 1. Install dependencies
sudo apt update
sudo apt install -y \
    build-essential pkg-config git \
    qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
    libqt6webenginecore6-bin libgl1-mesa-dev libx11-xcb-dev \
    libxkbcommon-dev libegl1-mesa-dev libfontconfig1-dev \
    libfreetype6-dev librsvg2-bin imagemagick

# 2. Clone the repository
git clone https://github.com/aryanweblabs/rootbrowser.git
cd rootbrowser

# 3. Compile
g++ -std=c++17 -O2 -fPIC src/*.cpp -o rootbrowser \
    $(pkg-config --cflags --libs Qt6WebEngineWidgets Qt6Widgets)

# 4. Run
./rootbrowser
