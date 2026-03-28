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
echo "=== Building AUv3 (simulator) ==="
cmake --build "${BUILD_DIR}" \
    --target "${PLUGIN_NAME}_AUv3" \
    --config Debug \
    -- -sdk iphonesimulator -quiet

ARTEFACTS="${BUILD_DIR}/plugins/${PLUGIN_NAME}/${PLUGIN_NAME}_artefacts/Debug/AUv3"
echo ""
echo "=== Done ==="
echo "AUv3 appex: ${ARTEFACTS}/${PLUGIN_NAME}.appex"
echo ""
echo "To test in Simulator:"
echo "  1. Open Xcode, run the '${PLUGIN_NAME}_Standalone' scheme on a simulator"
echo "  2. Or: open AUM / host app in Simulator and load the AUv3"
