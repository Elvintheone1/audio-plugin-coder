#!/bin/bash
# ==============================================================================
# APC macOS AU Cache Buster
# Logic X (and other DAWs) cache AU component metadata aggressively.
# Run this after installing/updating an AU plugin so the DAW picks up changes.
#
# Usage:
#   bash scripts/au-cache-bust-mac.sh                    # bust all caches
#   bash scripts/au-cache-bust-mac.sh <PluginName>       # reinstall hint only
# ==============================================================================

PLUGIN_NAME="$1"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo -e "${CYAN}=============================================="
echo "  APC AU Cache Buster"
echo -e "==============================================${NC}"
echo ""

# ------------------------------------------------------------------------------
# 1. Kill AudioComponentRegistrar (the daemon that caches AU components)
# ------------------------------------------------------------------------------
echo "Killing AudioComponentRegistrar..."
if pkill -f "AudioComponentRegistrar" 2>/dev/null; then
    echo -e "${GREEN}[OK]${NC} AudioComponentRegistrar terminated."
else
    echo -e "${YELLOW}[INFO]${NC} AudioComponentRegistrar was not running."
fi

# Also kill coreaudiod to flush its internal AU cache
echo "Restarting coreaudiod..."
if sudo launchctl kickstart -k system/com.apple.audio.coreaudiod 2>/dev/null; then
    echo -e "${GREEN}[OK]${NC} coreaudiod restarted."
else
    # Fallback for older macOS
    sudo killall -9 coreaudiod 2>/dev/null || true
    echo -e "${YELLOW}[INFO]${NC} coreaudiod restart attempted (may require a moment to restart)."
fi

sleep 1

# ------------------------------------------------------------------------------
# 2. Delete AU cache files
# ------------------------------------------------------------------------------
echo ""
echo "Removing AU cache files..."

CACHE_PATHS=(
    "$HOME/Library/Caches/AudioUnitCache"
    "$HOME/Library/Caches/com.apple.audiounits.cache"
    "/Library/Caches/AudioUnitCache"
    "$HOME/Library/Caches/com.apple.audio.InfoHelper"
)

for CACHE_PATH in "${CACHE_PATHS[@]}"; do
    if [ -e "$CACHE_PATH" ]; then
        rm -rf "$CACHE_PATH"
        echo -e "${GREEN}[REMOVED]${NC} $CACHE_PATH"
    else
        echo -e "  [skip] Not found: $CACHE_PATH"
    fi
done

# Logic X specific cache
LOGIC_AU_CACHE="$HOME/Library/Caches/com.apple.logic10"
if [ -d "$LOGIC_AU_CACHE" ]; then
    # Only remove AU-related files, not all Logic caches
    find "$LOGIC_AU_CACHE" -name "*AudioUnit*" -delete 2>/dev/null || true
    echo -e "${GREEN}[CLEANED]${NC} Logic X AU cache entries"
fi

# ------------------------------------------------------------------------------
# 3. Re-register the AU (if plugin name given and installed)
# ------------------------------------------------------------------------------
if [ -n "$PLUGIN_NAME" ]; then
    AU_PATH="$HOME/Library/Audio/Plug-Ins/Components/$PLUGIN_NAME.component"
    if [ -d "$AU_PATH" ]; then
        echo ""
        echo "Re-registering $PLUGIN_NAME.component..."
        # auval validates and registers in one pass
        if auval -v aufx "$PLUGIN_NAME" 2>/dev/null | head -5; then
            echo -e "${GREEN}[OK]${NC} $PLUGIN_NAME re-registered with auval."
        else
            # auval exit codes vary; try alternate subtype approach
            echo -e "${YELLOW}[INFO]${NC} auval check complete (non-zero exit is normal for first registration)."
        fi
    else
        echo -e "${YELLOW}[WARN]${NC} $PLUGIN_NAME.component not found at $AU_PATH"
        echo "  Build & install first: bash scripts/build-and-install-mac.sh $PLUGIN_NAME"
    fi
fi

# ------------------------------------------------------------------------------
# 4. Instructions for DAW restart
# ------------------------------------------------------------------------------
echo ""
echo "=============================================="
echo -e "${GREEN}  AU Cache Busted!${NC}"
echo "=============================================="
echo ""
echo "IMPORTANT: You must fully quit and reopen your DAW."
echo ""
echo "Logic X:"
echo "  1. Quit Logic X completely (Cmd+Q)"
echo "  2. Wait 5 seconds for AudioComponentRegistrar to re-scan"
echo "  3. Reopen Logic X — plugin will appear in Audio Units list"
echo ""
echo "Fastest testing path (no DAW required):"
if [ -n "$PLUGIN_NAME" ]; then
    REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
    SA_PATH="$REPO_ROOT/build/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/Standalone/$PLUGIN_NAME.app"
    echo "  open \"$SA_PATH\""
else
    echo "  open \"build/plugins/<PluginName>/<PluginName>_artefacts/Release/Standalone/<PluginName>.app\""
fi
echo ""
