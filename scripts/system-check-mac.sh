#!/bin/bash
# ==============================================================================
# APC macOS System Check
# Validates all build dependencies and reports status (JSON output optional)
# Usage: bash scripts/system-check-mac.sh [--json]
# ==============================================================================

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JSON_MODE=0
[ "$1" = "--json" ] && JSON_MODE=1

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

# Helpers
pass() { echo -e "  ${GREEN}[PASS]${NC} $1"; }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; }

check_version_ge() {
    # Returns 0 if $1 >= $2 (semver)
    [ "$(printf '%s\n' "$2" "$1" | sort -V | head -1)" = "$2" ]
}

# Accumulate JSON fields
JSON_FIELDS=()

# ==============================================================================
# 1. Platform
# ==============================================================================
ARCH=$(uname -m)
MACOS_VER=$(sw_vers -productVersion)
JSON_FIELDS+=("\"platform\":{\"os\":\"macOS\",\"version\":\"$MACOS_VER\",\"arch\":\"$ARCH\"}")

# ==============================================================================
# 2. Xcode / CLT
# ==============================================================================
check_xcode() {
    local clt_ok=false xcode_ok=false clt_path="" xcode_ver=""
    if clt_path=$(xcode-select -p 2>/dev/null); then
        clt_ok=true
    fi
    if xcode_ver=$(xcodebuild -version 2>/dev/null | head -1); then
        xcode_ok=true
    fi
    if $xcode_ok; then
        pass "Xcode: $xcode_ver"
        JSON_FIELDS+=("\"xcode\":{\"found\":true,\"version\":\"$xcode_ver\",\"ok\":true}")
    elif $clt_ok; then
        warn "Xcode CLT only (no Xcode.app). AU builds require full Xcode."
        JSON_FIELDS+=("\"xcode\":{\"found\":true,\"clt_only\":true,\"ok\":false}")
    else
        fail "Xcode Command Line Tools not found. Run: xcode-select --install"
        JSON_FIELDS+=("\"xcode\":{\"found\":false,\"ok\":false}")
    fi
}

# ==============================================================================
# 3. CMake
# ==============================================================================
check_cmake() {
    local required="3.22"
    if command -v cmake &>/dev/null; then
        local ver
        ver=$(cmake --version | head -1 | awk '{print $3}')
        if check_version_ge "$ver" "$required"; then
            pass "CMake: $ver"
            JSON_FIELDS+=("\"cmake\":{\"found\":true,\"version\":\"$ver\",\"ok\":true}")
        else
            fail "CMake $ver < required $required. Run: brew upgrade cmake"
            JSON_FIELDS+=("\"cmake\":{\"found\":true,\"version\":\"$ver\",\"ok\":false}")
        fi
    else
        fail "CMake not found. Run: brew install cmake"
        JSON_FIELDS+=("\"cmake\":{\"found\":false,\"ok\":false}")
    fi
}

# ==============================================================================
# 4. Git
# ==============================================================================
check_git() {
    if command -v git &>/dev/null; then
        local ver
        ver=$(git --version | awk '{print $3}')
        pass "Git: $ver"
        JSON_FIELDS+=("\"git\":{\"found\":true,\"version\":\"$ver\",\"ok\":true}")
    else
        fail "Git not found. Run: brew install git"
        JSON_FIELDS+=("\"git\":{\"found\":false,\"ok\":false}")
    fi
}

# ==============================================================================
# 5. JUCE (submodule)
# ==============================================================================
check_juce() {
    local juce_header="$REPO_ROOT/_tools/JUCE/modules/juce_core/juce_core.h"
    if [ -f "$juce_header" ]; then
        pass "JUCE: found at _tools/JUCE"
        JSON_FIELDS+=("\"juce\":{\"found\":true,\"path\":\"_tools/JUCE\",\"ok\":true}")
    else
        fail "JUCE not found. Run: git submodule update --init --recursive"
        JSON_FIELDS+=("\"juce\":{\"found\":false,\"ok\":false}")
    fi
}

# ==============================================================================
# 6. pluginval
# ==============================================================================
check_pluginval() {
    local pv_app="$REPO_ROOT/_tools/pluginval/pluginval.app"
    local pv_bin="$REPO_ROOT/_tools/pluginval/pluginval"
    if [ -d "$pv_app" ] || [ -f "$pv_bin" ]; then
        pass "pluginval: found in _tools/pluginval/"
        JSON_FIELDS+=("\"pluginval\":{\"found\":true,\"ok\":true}")
    else
        warn "pluginval not found. Run: bash scripts/setup-mac.sh"
        JSON_FIELDS+=("\"pluginval\":{\"found\":false,\"ok\":false}")
    fi
}

# ==============================================================================
# 7. Plugin install directories
# ==============================================================================
check_install_dirs() {
    local all_ok=true
    for DIR in \
        "$HOME/Library/Audio/Plug-Ins/VST3" \
        "$HOME/Library/Audio/Plug-Ins/Components"; do
        if [ -d "$DIR" ]; then
            pass "Install dir: $DIR"
        else
            warn "Missing: $DIR (will be created on first install)"
            all_ok=false
        fi
    done
    if $all_ok; then
        JSON_FIELDS+=("\"install_dirs\":{\"ok\":true}")
    else
        JSON_FIELDS+=("\"install_dirs\":{\"ok\":false}")
    fi
}

# ==============================================================================
# Run all checks
# ==============================================================================
if [ $JSON_MODE -eq 0 ]; then
    echo ""
    echo "=============================================="
    echo "  APC macOS System Check"
    echo "=============================================="
    echo ""
    echo -e "${CYAN}Platform:${NC} macOS $MACOS_VER ($ARCH)"
    echo ""
fi

check_xcode
check_cmake
check_git
check_juce
check_pluginval
check_install_dirs

# ==============================================================================
# JSON output
# ==============================================================================
if [ $JSON_MODE -eq 1 ]; then
    echo "{"
    for i in "${!JSON_FIELDS[@]}"; do
        if [ $i -lt $((${#JSON_FIELDS[@]} - 1)) ]; then
            echo "  ${JSON_FIELDS[$i]},"
        else
            echo "  ${JSON_FIELDS[$i]}"
        fi
    done
    echo "}"
else
    echo ""
    echo "=============================================="
    echo "  Run 'bash scripts/setup-mac.sh' to fix any issues."
    echo "=============================================="
    echo ""
fi
