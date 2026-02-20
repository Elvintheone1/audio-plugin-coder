#!/bin/bash
# ==============================================================================
# APC macOS Build + Install Script
# Usage: bash scripts/build-and-install-mac.sh <PluginName> [--skip-tests]
# ==============================================================================
set -e

PLUGIN_NAME="$1"
SKIP_TESTS=0
for arg in "$@"; do
    [[ "$arg" == "--skip-tests" ]] && SKIP_TESTS=1
done

if [ -z "$PLUGIN_NAME" ]; then
    echo "Usage: bash scripts/build-and-install-mac.sh <PluginName> [--skip-tests]"
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}=== APC macOS Build: $PLUGIN_NAME ===${NC}"
echo ""

# ------------------------------------------------------------------------------
echo "[1/5] Configuring CMake..."
cmake -G Xcode \
    -B "$BUILD_DIR" \
    -S "$REPO_ROOT" \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15" \
    -DAPC_ENABLE_VISAGE=OFF \
    2>&1 | tail -20

# ------------------------------------------------------------------------------
echo "[2/5] Building $PLUGIN_NAME (Release)..."
cmake --build "$BUILD_DIR" \
    --target "${PLUGIN_NAME}_VST3" "${PLUGIN_NAME}_AU" "${PLUGIN_NAME}_Standalone" \
    --config Release \
    -- -quiet \
    2>&1 | tail -40

echo -e "${GREEN}** BUILD SUCCEEDED **${NC}"

VST3_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/VST3/$PLUGIN_NAME.vst3"
AU_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/AU/$PLUGIN_NAME.component"
SA_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/Standalone/$PLUGIN_NAME.app"

# ------------------------------------------------------------------------------
echo "[3/5] Installing..."
mkdir -p ~/Library/Audio/Plug-Ins/VST3
mkdir -p ~/Library/Audio/Plug-Ins/Components

if [ -d "$VST3_PATH" ]; then
    cp -r "$VST3_PATH" ~/Library/Audio/Plug-Ins/VST3/
    echo -e "  ${GREEN}INSTALLED VST3${NC} → ~/Library/Audio/Plug-Ins/VST3/$PLUGIN_NAME.vst3"
fi
if [ -d "$AU_PATH" ]; then
    cp -r "$AU_PATH" ~/Library/Audio/Plug-Ins/Components/
    echo -e "  ${GREEN}INSTALLED AU${NC}   → ~/Library/Audio/Plug-Ins/Components/$PLUGIN_NAME.component"
fi

# ------------------------------------------------------------------------------
echo "[4/5] Busting AU cache..."
bash "$REPO_ROOT/scripts/au-cache-bust-mac.sh" "$PLUGIN_NAME" 2>/dev/null || true

# ------------------------------------------------------------------------------
echo "[5/5] Done."
echo ""
echo -e "${GREEN}=== Build complete! ===${NC}"
echo ""
echo "Standalone (fastest test — no DAW needed):"
echo "  open \"$SA_PATH\""
echo ""
if [ $SKIP_TESTS -eq 0 ]; then
    echo "Run pluginval:"
    echo "  bash scripts/pluginval-mac.sh $PLUGIN_NAME"
    echo ""
fi
echo "To use in a DAW: fully quit and reopen Logic/Ableton."
echo "AU cache has been flushed. AudioComponentRegistrar will re-scan on DAW launch."
