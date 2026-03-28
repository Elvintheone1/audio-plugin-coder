#!/bin/bash
# build-ios-sim.sh — Build Splice AUv3 for iOS Simulator (arm64 + x86_64)
# No code signing required for simulator builds.
# Usage: bash scripts/build-ios-sim.sh [PluginName]
set -euo pipefail

PLUGIN_NAME="${1:-Splice}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-ios-sim"
JUCE_DIR="${REPO_ROOT}/_tools/JUCE"

echo "=== iOS Simulator Build: ${PLUGIN_NAME} ==="
echo "Repo root : ${REPO_ROOT}"
echo "Build dir : ${BUILD_DIR}"

# Require Xcode
if ! xcodebuild -version &>/dev/null; then
    echo "ERROR: Xcode not found. Install Xcode from the App Store."
    exit 1
fi

# Configure
cmake -G Xcode \
    -B "${BUILD_DIR}" \
    -S "${REPO_ROOT}" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="14.0" \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -DAPC_ENABLE_VISAGE=OFF

echo ""
echo "=== Building AUv3 + Standalone (simulator) ==="
cmake --build "${BUILD_DIR}" \
    --target "${PLUGIN_NAME}_AUv3" "${PLUGIN_NAME}_Standalone" \
    --config Debug \
    -- -sdk iphonesimulator -quiet

ARTEFACTS="${BUILD_DIR}/plugins/${PLUGIN_NAME}/${PLUGIN_NAME}_artefacts/Debug"
echo ""
echo "=== Done ==="
echo "AUv3 appex : ${ARTEFACTS}/AUv3/${PLUGIN_NAME}.appex"
echo "Standalone : ${ARTEFACTS}/Standalone/${PLUGIN_NAME}.app"
echo ""
echo "To install and run:"
echo "  xcrun simctl install booted \"${ARTEFACTS}/Standalone/${PLUGIN_NAME}.app\""
echo "  xcrun simctl launch booted \$(defaults read \"${ARTEFACTS}/Standalone/${PLUGIN_NAME}.app/Info\" CFBundleIdentifier 2>/dev/null || echo com.aj.splice)"
