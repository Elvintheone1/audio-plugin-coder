#!/bin/bash
# ==============================================================================
# APC macOS Setup Script
# Checks and installs all build dependencies for audio-plugin-coder on macOS
# Usage: bash scripts/setup-mac.sh
# ==============================================================================
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log()    { echo -e "${CYAN}[APC]${NC} $1"; }
ok()     { echo -e "${GREEN}[OK]${NC} $1"; }
warn()   { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()    { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

echo ""
echo "=============================================="
echo "  APC macOS Setup"
echo "=============================================="
echo ""

# ------------------------------------------------------------------------------
# 1. Xcode Command Line Tools
# ------------------------------------------------------------------------------
log "Checking Xcode Command Line Tools..."
if xcode-select -p &>/dev/null; then
    ok "Xcode CLT: $(xcode-select -p)"
else
    warn "Xcode Command Line Tools not found. Installing..."
    xcode-select --install
    echo "Please complete the Xcode CLT installation dialog, then re-run this script."
    exit 0
fi

# Check full Xcode (required for AU builds)
if [ -d "/Applications/Xcode.app" ]; then
    XCODE_VER=$(xcodebuild -version 2>/dev/null | head -1)
    ok "Xcode: $XCODE_VER"
else
    warn "Full Xcode.app not found at /Applications/Xcode.app"
    warn "AU plugin builds require full Xcode. Download from App Store."
    warn "VST3 builds will still work with Xcode CLT only."
fi

# ------------------------------------------------------------------------------
# 2. Homebrew
# ------------------------------------------------------------------------------
log "Checking Homebrew..."
if command -v brew &>/dev/null; then
    BREW_VER=$(brew --version | head -1)
    ok "Homebrew: $BREW_VER"
else
    warn "Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    # Apple Silicon path
    if [ -f /opt/homebrew/bin/brew ]; then
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
    ok "Homebrew installed."
fi

# ------------------------------------------------------------------------------
# 3. CMake >= 3.22
# ------------------------------------------------------------------------------
log "Checking CMake..."
if command -v cmake &>/dev/null; then
    CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
    REQUIRED="3.22"
    if [ "$(printf '%s\n' "$REQUIRED" "$CMAKE_VER" | sort -V | head -1)" = "$REQUIRED" ]; then
        ok "CMake: $CMAKE_VER"
    else
        warn "CMake $CMAKE_VER found but >= $REQUIRED required. Upgrading..."
        brew upgrade cmake
    fi
else
    warn "CMake not found. Installing via Homebrew..."
    brew install cmake
    ok "CMake installed: $(cmake --version | head -1)"
fi

# ------------------------------------------------------------------------------
# 4. Git
# ------------------------------------------------------------------------------
log "Checking Git..."
if command -v git &>/dev/null; then
    ok "Git: $(git --version)"
else
    warn "Git not found. Installing via Homebrew..."
    brew install git
    ok "Git installed."
fi

# ------------------------------------------------------------------------------
# 5. Git submodules (JUCE + other _tools)
# ------------------------------------------------------------------------------
log "Initializing Git submodules..."
cd "$REPO_ROOT"
git submodule update --init --recursive
ok "Submodules initialized."

# Verify JUCE
if [ -f "$REPO_ROOT/_tools/JUCE/modules/juce_core/juce_core.h" ]; then
    ok "JUCE: found at _tools/JUCE"
else
    err "JUCE not found at _tools/JUCE/modules/juce_core/juce_core.h — check submodule config."
fi

# ------------------------------------------------------------------------------
# 6. pluginval (macOS binary)
# ------------------------------------------------------------------------------
log "Checking pluginval..."
PLUGINVAL_PATH="$REPO_ROOT/_tools/pluginval/pluginval.app"
PLUGINVAL_BIN="$REPO_ROOT/_tools/pluginval/pluginval"

if [ -d "$PLUGINVAL_PATH" ] || [ -f "$PLUGINVAL_BIN" ]; then
    ok "pluginval: found in _tools/pluginval/"
else
    warn "pluginval not found. Downloading macOS binary..."
    mkdir -p "$REPO_ROOT/_tools/pluginval"
    PLUGINVAL_URL="https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip"
    TEMP_ZIP="/tmp/pluginval_mac.zip"
    curl -L "$PLUGINVAL_URL" -o "$TEMP_ZIP"
    unzip -q "$TEMP_ZIP" -d "$REPO_ROOT/_tools/pluginval/"
    rm "$TEMP_ZIP"
    if [ -d "$PLUGINVAL_PATH" ] || [ -f "$PLUGINVAL_BIN" ]; then
        ok "pluginval downloaded."
    else
        warn "pluginval download may have failed. Manual install: https://github.com/Tracktion/pluginval/releases"
    fi
fi

# ------------------------------------------------------------------------------
# 7. Plugin install directories
# ------------------------------------------------------------------------------
log "Checking plugin install directories..."
for DIR in \
    "$HOME/Library/Audio/Plug-Ins/VST3" \
    "$HOME/Library/Audio/Plug-Ins/Components"; do
    if [ -d "$DIR" ]; then
        ok "Exists: $DIR"
    else
        mkdir -p "$DIR"
        ok "Created: $DIR"
    fi
done

# ------------------------------------------------------------------------------
# Done
# ------------------------------------------------------------------------------
echo ""
echo "=============================================="
echo -e "${GREEN}  macOS Setup Complete!${NC}"
echo "=============================================="
echo ""
echo "Next steps:"
echo "  Build a plugin:    bash scripts/build-and-install-mac.sh <PluginName>"
echo "  System check:      bash scripts/system-check-mac.sh"
echo "  AU cache bust:     bash scripts/au-cache-bust-mac.sh"
echo ""
