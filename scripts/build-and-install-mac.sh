#!/bin/bash
# ==============================================================================
# APC macOS Build + Install Script
# Usage: bash scripts/build-and-install-mac.sh <PluginName> [--skip-tests]
#
# Generator auto-detection (multi-machine safe):
#   1. Ninja   — preferred (fast, works on all machines)
#   2. Xcode   — fallback when Xcode generator works (stable Xcode installs)
#   3. Unix Makefiles — last resort (Xcode 26 beta / broken xcodebuild)
#
# Architecture: defaults to native arch. Override with ARCHS env var:
#   ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye
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
YELLOW='\033[0;33m'
NC='\033[0m'

echo ""
echo -e "${CYAN}=== APC macOS Build: $PLUGIN_NAME ===${NC}"
echo ""

# ------------------------------------------------------------------------------
# ARCHITECTURE: default to native (arm64 on Apple Silicon, x86_64 on Intel).
# Set ARCHS env var to override, e.g. ARCHS="arm64;x86_64" for universal binary.
# ------------------------------------------------------------------------------
if [ -z "$ARCHS" ]; then
    ARCHS="$(uname -m)"   # arm64 or x86_64
fi
NCPU=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

# ------------------------------------------------------------------------------
# GENERATOR AUTO-DETECTION
# Priority: Ninja > Xcode (healthy xcodebuild) > Unix Makefiles
# ------------------------------------------------------------------------------
_pick_generator() {
    # 1. Ninja — fast, works everywhere, no xcodebuild dependency
    if command -v ninja >/dev/null 2>&1; then
        echo "Ninja"
        return
    fi

    # 2. Xcode — only if xcodebuild actually works (healthy Xcode install)
    if xcodebuild -version >/dev/null 2>&1; then
        echo "Xcode"
        return
    fi

    # 3. Unix Makefiles — fallback for broken xcodebuild (Xcode 26 beta, etc.)
    echo "Unix Makefiles"
}

CMAKE_GENERATOR="$(_pick_generator)"
echo -e "${CYAN}[Generator] Using: $CMAKE_GENERATOR  (arch: $ARCHS, cpus: $NCPU)${NC}"

# ------------------------------------------------------------------------------
# CACHE INVALIDATION
# If a cached build used a different generator, wipe CMakeCache + CMakeFiles
# so CMake doesn't error on generator mismatch.
# ------------------------------------------------------------------------------
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_GEN=$(grep -m1 "^CMAKE_GENERATOR:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null \
                 | cut -d= -f2 || echo "")
    if [ -n "$CACHED_GEN" ] && [ "$CACHED_GEN" != "$CMAKE_GENERATOR" ]; then
        echo -e "${YELLOW}  (generator changed: \"$CACHED_GEN\" → \"$CMAKE_GENERATOR\" — clearing cache)${NC}"
        rm -f "$BUILD_DIR/CMakeCache.txt"
        rm -rf "$BUILD_DIR/CMakeFiles"
    fi
fi

# ------------------------------------------------------------------------------
echo "[1/5] Configuring CMake..."
# ------------------------------------------------------------------------------

# Compiler flags: always resolve via xcrun so paths stay SDK-relative (no hardcoding).
CLANG=$(xcrun --find clang 2>/dev/null || echo "/usr/bin/clang")
CLANGXX=$(xcrun --find clang++ 2>/dev/null || echo "/usr/bin/clang++")

# Common cmake flags (shared across all generators)
CMAKE_FLAGS=(
    -B "$BUILD_DIR"
    -S "$REPO_ROOT"
    -DCMAKE_C_COMPILER="$CLANG"
    -DCMAKE_CXX_COMPILER="$CLANGXX"
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"
    -DAPC_ENABLE_VISAGE=OFF
)

# Non-Xcode generators need CMAKE_BUILD_TYPE; Xcode uses --config at build time
if [ "$CMAKE_GENERATOR" != "Xcode" ]; then
    CMAKE_FLAGS+=(-DCMAKE_BUILD_TYPE=Release)
fi

cmake -G "$CMAKE_GENERATOR" "${CMAKE_FLAGS[@]}" \
    || { echo "❌ CMake configure failed. Check output above."; exit 1; }

# ------------------------------------------------------------------------------
echo "[2/5] Building $PLUGIN_NAME (Release)..."
# ------------------------------------------------------------------------------

# Build trailing args differ by generator
if [ "$CMAKE_GENERATOR" = "Xcode" ]; then
    BUILD_EXTRA=(--config Release -- -quiet)
else
    BUILD_EXTRA=(-- -j"$NCPU")
fi

cmake --build "$BUILD_DIR" \
    --target "${PLUGIN_NAME}_VST3" "${PLUGIN_NAME}_AU" "${PLUGIN_NAME}_Standalone" \
    "${BUILD_EXTRA[@]}" \
    || { echo "❌ Build failed. Check output above."; exit 1; }

echo -e "${GREEN}** BUILD SUCCEEDED **${NC}"

VST3_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/VST3/$PLUGIN_NAME.vst3"
AU_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/AU/$PLUGIN_NAME.component"
SA_PATH="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release/Standalone/$PLUGIN_NAME.app"

# ------------------------------------------------------------------------------
echo "[3/5] Installing..."
# ------------------------------------------------------------------------------
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
# ------------------------------------------------------------------------------
bash "$REPO_ROOT/scripts/au-cache-bust-mac.sh" "$PLUGIN_NAME" 2>/dev/null || true

# ------------------------------------------------------------------------------
echo "[5/5] Done."
# ------------------------------------------------------------------------------
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
