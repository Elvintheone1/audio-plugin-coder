#!/bin/bash
# ==============================================================================
# APC macOS pluginval Integration
# Runs pluginval against a built plugin artifact
# Usage: bash scripts/pluginval-mac.sh <PluginName> [--strict] [--verbose]
# ==============================================================================
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_NAME="$1"
STRICT=0
VERBOSE=0

if [ -z "$PLUGIN_NAME" ]; then
    echo "Usage: bash scripts/pluginval-mac.sh <PluginName> [--strict] [--verbose]"
    exit 1
fi

for arg in "$@"; do
    [ "$arg" = "--strict"  ] && STRICT=1
    [ "$arg" = "--verbose" ] && VERBOSE=1
done

GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

# ------------------------------------------------------------------------------
# State management integration (writes results to status.json if present)
# ------------------------------------------------------------------------------
PLUGIN_PATH="$REPO_ROOT/plugins/$PLUGIN_NAME"
STATE_FILE="$PLUGIN_PATH/status.json"
STATE_SCRIPT="$REPO_ROOT/scripts/state-management-mac.sh"
HAS_STATE=0
if [[ -f "$STATE_FILE" && -f "$STATE_SCRIPT" ]]; then
    # shellcheck source=scripts/state-management-mac.sh
    source "$STATE_SCRIPT"
    HAS_STATE=1
fi

# ------------------------------------------------------------------------------
# Locate pluginval binary
# ------------------------------------------------------------------------------
PLUGINVAL_APP="$REPO_ROOT/_tools/pluginval/pluginval.app/Contents/MacOS/pluginval"
PLUGINVAL_BIN="$REPO_ROOT/_tools/pluginval/pluginval"

if [ -f "$PLUGINVAL_APP" ]; then
    PLUGINVAL="$PLUGINVAL_APP"
elif [ -f "$PLUGINVAL_BIN" ]; then
    PLUGINVAL="$PLUGINVAL_BIN"
else
    echo -e "${YELLOW}[WARN]${NC} pluginval not found in _tools/pluginval/."
    echo "  Run: bash scripts/setup-mac.sh"
    echo "  Skipping validation."
    exit 0
fi

# ------------------------------------------------------------------------------
# Locate plugin artifacts
# ------------------------------------------------------------------------------
BUILD_DIR="$REPO_ROOT/build"
ARTEFACTS_DIR="$BUILD_DIR/plugins/$PLUGIN_NAME/${PLUGIN_NAME}_artefacts/Release"

VST3_PATH="$ARTEFACTS_DIR/VST3/$PLUGIN_NAME.vst3"
AU_PATH="$ARTEFACTS_DIR/AU/$PLUGIN_NAME.component"

echo ""
echo -e "${CYAN}=============================================="
echo "  pluginval: $PLUGIN_NAME"
echo -e "==============================================${NC}"
echo ""

ARGS=()
[ $STRICT  -eq 1 ] && ARGS+=("--strictness-level" "5")
[ $VERBOSE -eq 1 ] && ARGS+=("--verbose")
ARGS+=("--validate-in-process")

OVERALL_PASS=true
VST3_RESULT="skip"
AU_RESULT="skip"
VST3_DURATION=0
AU_DURATION=0

# ------------------------------------------------------------------------------
# VST3
# ------------------------------------------------------------------------------
if [ -d "$VST3_PATH" ]; then
    echo -e "${CYAN}Testing VST3...${NC}"
    START=$(date +%s)
    if "$PLUGINVAL" "${ARGS[@]}" "$VST3_PATH"; then
        END=$(date +%s)
        VST3_DURATION=$((END-START))
        VST3_RESULT="pass"
        echo -e "${GREEN}[PASS]${NC} VST3 passed in ${VST3_DURATION}s"
    else
        END=$(date +%s)
        VST3_DURATION=$((END-START))
        VST3_RESULT="fail"
        echo -e "${RED}[FAIL]${NC} VST3 failed in ${VST3_DURATION}s"
        OVERALL_PASS=false
    fi
else
    echo -e "${YELLOW}[SKIP]${NC} VST3 not found at $VST3_PATH"
fi

echo ""

# ------------------------------------------------------------------------------
# AU (prefer installed location; fall back to build dir)
# ------------------------------------------------------------------------------
AU_INSTALLED="$HOME/Library/Audio/Plug-Ins/Components/$PLUGIN_NAME.component"
if [ -d "$AU_INSTALLED" ]; then
    echo -e "${CYAN}Testing AU (from installed location)...${NC}"
    START=$(date +%s)
    if "$PLUGINVAL" "${ARGS[@]}" "$AU_INSTALLED"; then
        END=$(date +%s)
        AU_DURATION=$((END-START))
        AU_RESULT="pass"
        echo -e "${GREEN}[PASS]${NC} AU passed in ${AU_DURATION}s"
    else
        END=$(date +%s)
        AU_DURATION=$((END-START))
        AU_RESULT="fail"
        echo -e "${RED}[FAIL]${NC} AU failed in ${AU_DURATION}s"
        OVERALL_PASS=false
    fi
elif [ -d "$AU_PATH" ]; then
    echo -e "${CYAN}Testing AU (from build dir)...${NC}"
    START=$(date +%s)
    if "$PLUGINVAL" "${ARGS[@]}" "$AU_PATH"; then
        END=$(date +%s)
        AU_DURATION=$((END-START))
        AU_RESULT="pass"
        echo -e "${GREEN}[PASS]${NC} AU passed in ${AU_DURATION}s"
    else
        END=$(date +%s)
        AU_DURATION=$((END-START))
        AU_RESULT="fail"
        echo -e "${RED}[FAIL]${NC} AU failed in ${AU_DURATION}s"
        OVERALL_PASS=false
    fi
else
    echo -e "${YELLOW}[SKIP]${NC} AU not found (not built or not installed)"
fi

# ------------------------------------------------------------------------------
# Write results to status.json (if state management is available)
# ------------------------------------------------------------------------------
if [[ $HAS_STATE -eq 1 ]]; then
    LAST_RUN="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    PASSED_VAL="$($OVERALL_PASS && echo true || echo false)"

    APCSTATE_PATH="$STATE_FILE" \
    APCSTATE_PASSED="$PASSED_VAL" \
    APCSTATE_VST3="$VST3_RESULT" \
    APCSTATE_AU="$AU_RESULT" \
    APCSTATE_VST3_DUR="$VST3_DURATION" \
    APCSTATE_AU_DUR="$AU_DURATION" \
    APCSTATE_LAST_RUN="$LAST_RUN" \
    python3 <<'PYEOF'
import json, os

path     = os.environ['APCSTATE_PATH']
passed   = os.environ['APCSTATE_PASSED'] == 'true'
last_run = os.environ['APCSTATE_LAST_RUN']

with open(path) as f:
    d = json.load(f)

d.setdefault('validation', {})['tests_passed'] = passed
d['last_modified'] = last_run

d['validation']['pluginval_results'] = {
    'passed':          passed,
    'last_run':        last_run,
    'vst3':            os.environ['APCSTATE_VST3'],
    'au':              os.environ['APCSTATE_AU'],
    'vst3_duration_s': int(os.environ['APCSTATE_VST3_DUR']),
    'au_duration_s':   int(os.environ['APCSTATE_AU_DUR']),
}

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${CYAN}[state]${NC} Results written to status.json"
fi

# ------------------------------------------------------------------------------
# Result
# ------------------------------------------------------------------------------
echo ""
if $OVERALL_PASS; then
    echo -e "${GREEN}=============================================="
    echo "  All pluginval tests PASSED"
    echo -e "==============================================${NC}"
    exit 0
else
    echo -e "${RED}=============================================="
    echo "  pluginval tests FAILED"
    echo -e "==============================================${NC}"
    exit 1
fi
