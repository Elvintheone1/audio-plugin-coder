#!/bin/bash
# build-ios-device.sh — Build Splice AUv3 for iOS Device (arm64)
# Requires a valid Apple Developer account and signing identity.
# Usage: bash scripts/build-ios-device.sh [PluginName] [TeamID] [SignIdentity]
#   TeamID        — 10-char team identifier (default: ZJX5RG4BDH)
#   SignIdentity  — code sign identity string (default: "Apple Development")
set -euo pipefail

PLUGIN_NAME="${1:-Splice}"
TEAM_ID="${2:-ZJX5RG4BDH}"
SIGN_ID="${3:-Apple Development}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-ios-device"

echo "=== iOS Device Build: ${PLUGIN_NAME} ==="
echo "Repo root : ${REPO_ROOT}"
echo "Build dir : ${BUILD_DIR}"
echo "Team ID   : ${TEAM_ID}"
echo "Sign ID   : ${SIGN_ID}"

# Require Xcode
if ! xcodebuild -version &>/dev/null; then
    echo "ERROR: Xcode not found. Install Xcode from the App Store."
    exit 1
fi

# Check signing identity exists
if ! security find-identity -v -p codesigning | grep -q "${TEAM_ID}"; then
    echo "WARNING: No certificate found for team ${TEAM_ID}"
    echo "  Run: security find-identity -v -p codesigning"
    echo "  Ensure your Apple Development certificate is installed in Keychain."
fi

# Configure (skip if already configured — avoids losing Xcode signing overrides)
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -G Xcode \
        -B "${BUILD_DIR}" \
        -S "${REPO_ROOT}" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES="arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="14.0" \
        -DAPC_IOS_TEAM_ID="${TEAM_ID}" \
        -DAPC_IOS_CODE_SIGN_IDENTITY="${SIGN_ID}" \
        -DAPC_ENABLE_VISAGE=OFF
else
    echo "[skip] Build dir already configured — reusing ${BUILD_DIR}"
fi

echo ""
echo "=== Building AUv3 + Standalone (device) ==="
cmake --build "${BUILD_DIR}" \
    --target "${PLUGIN_NAME}_AUv3" "${PLUGIN_NAME}_Standalone" \
    --config Release \
    -- -sdk iphoneos -quiet -allowProvisioningUpdates

ARTEFACTS="${BUILD_DIR}/plugins/${PLUGIN_NAME}/${PLUGIN_NAME}_artefacts/Release"
echo ""
echo "=== Done ==="
echo "AUv3 appex : ${ARTEFACTS}/AUv3/${PLUGIN_NAME}.appex"
echo "Standalone : ${ARTEFACTS}/Standalone/${PLUGIN_NAME}.app"
echo ""
echo "To install on device:"
echo "  Open Xcode → Window → Devices and Simulators"
echo "  Drag ${PLUGIN_NAME}.app onto your connected device"
echo "  Or use: xcrun devicectl device install app --device <udid> \"${ARTEFACTS}/Standalone/${PLUGIN_NAME}.app\""
