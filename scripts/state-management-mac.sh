#!/bin/bash
# =============================================================================
# APC State Management — macOS bash port of scripts/state-management.ps1
# =============================================================================
#
# Source this file to get state management functions:
#   source scripts/state-management-mac.sh
#
# Functions:
#   new_plugin_state    <PluginName> <PluginPath>
#   get_plugin_state    <PluginPath>
#   update_plugin_state <PluginPath> [--phase <p>] [--notes <n>]
#                                    [--framework <fw>] [--version <v>]
#                                    [--set <dotted.key> <value>]  (repeatable)
#   test_plugin_state   <PluginPath> [--required-phase <p>]
#                                    [--required-file <relpath>]  (repeatable)
#   backup_plugin_state  <PluginPath>
#   restore_plugin_state <PluginPath> [BackupFile]
#   add_state_error     <PluginPath> "ErrorMessage"
#   set_plugin_framework <PluginPath> <webview|visage> "Rationale"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMPLATE_PATH="$REPO_ROOT/templates/status-template.json"

# Lifecycle phases in order (index position used for phase-gate comparison)
_STATE_PHASES=("ideation" "plan" "design" "code" "ship" "complete")
_STATE_FRAMEWORKS=("visage" "webview" "pending")

_SM_GREEN='\033[0;32m'
_SM_YELLOW='\033[1;33m'
_SM_RED='\033[0;31m'
_SM_NC='\033[0m'

# ---------------------------------------------------------------------------
# _sm_phase_index <phase>  → prints integer index (0-based), or -1 if unknown
# ---------------------------------------------------------------------------
_sm_phase_index() {
    local p="$1" i=0
    for ph in "${_STATE_PHASES[@]}"; do
        [[ "$ph" == "$p" ]] && echo $i && return 0
        ((i++))
    done
    echo -1
}

# ---------------------------------------------------------------------------
# new_plugin_state <PluginName> <PluginPath>
#   Initialises a fresh status.json from templates/status-template.json.
# ---------------------------------------------------------------------------
new_plugin_state() {
    local plugin_name="$1"
    local plugin_path="$2"
    local status_path="$plugin_path/status.json"

    if [[ ! -f "$TEMPLATE_PATH" ]]; then
        echo -e "${_SM_RED}[state] Template not found: $TEMPLATE_PATH${_SM_NC}" >&2
        return 1
    fi

    local now
    now="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

    APCSTATE_TEMPLATE="$TEMPLATE_PATH" \
    APCSTATE_OUT="$status_path" \
    APCSTATE_NAME="$plugin_name" \
    APCSTATE_NOW="$now" \
    python3 <<'PYEOF'
import json, os

with open(os.environ['APCSTATE_TEMPLATE']) as f:
    d = json.load(f)

d['plugin_name'] = os.environ['APCSTATE_NAME']
d['created_at']  = os.environ['APCSTATE_NOW']
d['last_modified'] = os.environ['APCSTATE_NOW']

with open(os.environ['APCSTATE_OUT'], 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${_SM_GREEN}[state] Initialised status.json for $plugin_name → $status_path${_SM_NC}"
}

# ---------------------------------------------------------------------------
# get_plugin_state <PluginPath>  → prints status.json to stdout
# ---------------------------------------------------------------------------
get_plugin_state() {
    local status_path="$1/status.json"
    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found: $status_path${_SM_NC}" >&2
        return 1
    fi
    cat "$status_path"
}

# ---------------------------------------------------------------------------
# update_plugin_state <PluginPath> [options]
#
# Options (all optional, combinable):
#   --phase <p>          Set current_phase and append entry to phase_history
#   --notes <text>       Notes for the phase_history entry (use with --phase)
#   --framework <fw>     Set ui_framework + framework_selection.decision
#   --version <v>        Set version string
#   --set <key> <val>    Set arbitrary field; key uses dot-notation for nesting
#                        e.g. --set validation.tests_passed true
#                        Values 'true'/'false' become JSON booleans.
#                        Integer/float strings are coerced to numbers.
# ---------------------------------------------------------------------------
update_plugin_state() {
    local plugin_path="$1"
    shift

    local status_path="$plugin_path/status.json"
    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found${_SM_NC}" >&2
        return 1
    fi

    local phase="" phase_notes="" framework="" version=""
    local keys_str="" vals_str=""   # colon-delimited; values must not contain ':'

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --phase)     phase="$2";        shift 2 ;;
            --notes)     phase_notes="$2";  shift 2 ;;
            --framework) framework="$2";    shift 2 ;;
            --version)   version="$2";      shift 2 ;;
            --set)
                keys_str="${keys_str}${2}:"
                vals_str="${vals_str}${3}:"
                shift 3
                ;;
            *)
                echo -e "${_SM_RED}[state] Unknown option: $1${_SM_NC}" >&2
                return 1
                ;;
        esac
    done

    local now
    now="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

    APCSTATE_PATH="$status_path" \
    APCSTATE_PHASE="$phase" \
    APCSTATE_NOTES="$phase_notes" \
    APCSTATE_FRAMEWORK="$framework" \
    APCSTATE_VERSION="$version" \
    APCSTATE_NOW="$now" \
    APCSTATE_KEYS="$keys_str" \
    APCSTATE_VALS="$vals_str" \
    python3 <<'PYEOF'
import json, os

path      = os.environ['APCSTATE_PATH']
phase     = os.environ.get('APCSTATE_PHASE', '')
notes     = os.environ.get('APCSTATE_NOTES', '')
framework = os.environ.get('APCSTATE_FRAMEWORK', '')
version   = os.environ.get('APCSTATE_VERSION', '')
now       = os.environ.get('APCSTATE_NOW', '')
keys      = [k for k in os.environ.get('APCSTATE_KEYS', '').split(':') if k]
vals      = [v for v in os.environ.get('APCSTATE_VALS', '').split(':') if v]

with open(path) as f:
    d = json.load(f)

# Apply --set key/value pairs (dot-notation for nested fields)
for k, v in zip(keys, vals):
    parts = k.split('.')
    node = d
    for part in parts[:-1]:
        node = node.setdefault(part, {})
    # Coerce type
    if v.lower() == 'true':
        v = True
    elif v.lower() == 'false':
        v = False
    else:
        try:    v = int(v)
        except ValueError:
            try: v = float(v)
            except ValueError: pass
    node[parts[-1]] = v

# Phase update
if phase:
    d['current_phase'] = phase
    entry = {'phase': phase, 'completed_at': now}
    if notes:
        entry['notes'] = notes
    d.setdefault('phase_history', []).append(entry)

# Framework update
if framework:
    d['ui_framework'] = framework
    d.setdefault('framework_selection', {})['decision'] = framework

# Version update
if version:
    d['version'] = version

d['last_modified'] = now

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    local summary="${_SM_GREEN}[state] Updated"
    [[ -n "$phase"     ]] && summary+=" — phase: $phase"
    [[ -n "$framework" ]] && summary+=", framework: $framework"
    [[ -n "$version"   ]] && summary+=", version: $version"
    echo -e "${summary}${_SM_NC}"
}

# ---------------------------------------------------------------------------
# test_plugin_state <PluginPath> [options]
#   Returns 0 if all checks pass, 1 otherwise.
#
# Options:
#   --required-phase <p>        Current phase must be >= p in lifecycle order
#   --required-file  <relpath>  File must exist relative to PluginPath
#                               (repeatable)
# ---------------------------------------------------------------------------
test_plugin_state() {
    local plugin_path="$1"
    shift

    local status_path="$plugin_path/status.json"
    local required_phase=""
    local -a required_files=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --required-phase) required_phase="$2"; shift 2 ;;
            --required-file)  required_files+=("$2"); shift 2 ;;
            *) echo -e "${_SM_RED}[state] Unknown option: $1${_SM_NC}" >&2; return 1 ;;
        esac
    done

    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found: $status_path${_SM_NC}" >&2
        return 1
    fi

    local current_phase
    current_phase="$(python3 -c "
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
print(d.get('current_phase', ''))
" "$status_path")"

    if [[ -n "$required_phase" ]]; then
        local req_idx cur_idx
        req_idx="$(_sm_phase_index "$required_phase")"
        cur_idx="$(_sm_phase_index "$current_phase")"
        if [[ "$req_idx" -lt 0 ]]; then
            echo -e "${_SM_RED}[state] Invalid required phase: $required_phase${_SM_NC}" >&2
            return 1
        fi
        if [[ "$cur_idx" -lt "$req_idx" ]]; then
            echo -e "${_SM_RED}[state] Phase gate: current='$current_phase', need at least '$required_phase'${_SM_NC}" >&2
            return 1
        fi
    fi

    local f
    for f in "${required_files[@]}"; do
        if [[ ! -f "$plugin_path/$f" ]]; then
            echo -e "${_SM_RED}[state] Required file missing: $f${_SM_NC}" >&2
            return 1
        fi
    done

    echo -e "${_SM_GREEN}[state] Validation passed (phase: $current_phase)${_SM_NC}"
    return 0
}

# ---------------------------------------------------------------------------
# backup_plugin_state <PluginPath>
#   Copies status.json to <PluginPath>/_state_backups/status_backup_<ts>.json
#   Prints the backup file path on success.
# ---------------------------------------------------------------------------
backup_plugin_state() {
    local plugin_path="$1"
    local status_path="$plugin_path/status.json"

    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found${_SM_NC}" >&2
        return 1
    fi

    local backup_dir="$plugin_path/_state_backups"
    mkdir -p "$backup_dir"

    local timestamp backup_file
    timestamp="$(date +"%Y%m%d_%H%M%S")"
    backup_file="$backup_dir/status_backup_${timestamp}.json"
    cp "$status_path" "$backup_file"

    # Record backup path in error_recovery
    APCSTATE_PATH="$status_path" \
    APCSTATE_BACKUP="$backup_file" \
    python3 <<'PYEOF'
import json, os

path, bkp = os.environ['APCSTATE_PATH'], os.environ['APCSTATE_BACKUP']
with open(path) as f:
    d = json.load(f)

er = d.setdefault('error_recovery', {})
er['last_backup']        = bkp
er['rollback_available'] = True

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${_SM_YELLOW}[state] Backed up → $backup_file${_SM_NC}"
    echo "$backup_file"
}

# ---------------------------------------------------------------------------
# restore_plugin_state <PluginPath> [BackupFile]
#   Restores status.json from a backup.  If BackupFile is omitted, uses the
#   most recent file in <PluginPath>/_state_backups/.
# ---------------------------------------------------------------------------
restore_plugin_state() {
    local plugin_path="$1"
    local backup_file="${2:-}"
    local status_path="$plugin_path/status.json"
    local backup_dir="$plugin_path/_state_backups"

    if [[ ! -d "$backup_dir" ]]; then
        echo -e "${_SM_RED}[state] No backup directory found at $backup_dir${_SM_NC}" >&2
        return 1
    fi

    if [[ -z "$backup_file" ]]; then
        backup_file="$(ls -t "$backup_dir"/status_backup_*.json 2>/dev/null | head -1)"
        if [[ -z "$backup_file" ]]; then
            echo -e "${_SM_RED}[state] No backup files found${_SM_NC}" >&2
            return 1
        fi
    fi

    if [[ ! -f "$backup_file" ]]; then
        echo -e "${_SM_RED}[state] Backup file not found: $backup_file${_SM_NC}" >&2
        return 1
    fi

    cp "$backup_file" "$status_path"

    APCSTATE_PATH="$status_path" \
    APCSTATE_BACKUP="$backup_file" \
    python3 <<'PYEOF'
import json, os, datetime

path, src = os.environ['APCSTATE_PATH'], os.environ['APCSTATE_BACKUP']
with open(path) as f:
    d = json.load(f)

er = d.setdefault('error_recovery', {})
er['rollback_available'] = False
log = er.setdefault('error_log', [])
ts  = datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
log.append(f"Rollback performed from {src} at {ts}")

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${_SM_GREEN}[state] Restored from $backup_file${_SM_NC}"
}

# ---------------------------------------------------------------------------
# add_state_error <PluginPath> "ErrorMessage"
#   Appends a timestamped entry to error_recovery.error_log in status.json.
# ---------------------------------------------------------------------------
add_state_error() {
    local plugin_path="$1"
    local error_msg="$2"
    local status_path="$plugin_path/status.json"

    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found${_SM_NC}" >&2
        return 1
    fi

    APCSTATE_PATH="$status_path" \
    APCSTATE_MSG="$error_msg" \
    python3 <<'PYEOF'
import json, os, datetime

path, msg = os.environ['APCSTATE_PATH'], os.environ['APCSTATE_MSG']
with open(path) as f:
    d = json.load(f)

er  = d.setdefault('error_recovery', {})
log = er.setdefault('error_log', [])
ts  = datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
log.append(f"{ts}: {msg}")

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${_SM_YELLOW}[state] Error logged for $(basename "$plugin_path")${_SM_NC}"
}

# ---------------------------------------------------------------------------
# set_plugin_framework <PluginPath> <webview|visage> "Rationale"
#   Sets ui_framework and framework_selection fields.
# ---------------------------------------------------------------------------
set_plugin_framework() {
    local plugin_path="$1"
    local framework="$2"
    local rationale="$3"
    local status_path="$plugin_path/status.json"

    # Validate framework value
    local valid=0
    local fw
    for fw in "${_STATE_FRAMEWORKS[@]}"; do
        [[ "$fw" == "$framework" ]] && valid=1 && break
    done
    if [[ $valid -eq 0 ]]; then
        echo -e "${_SM_RED}[state] Invalid framework: '$framework'  (valid: ${_STATE_FRAMEWORKS[*]})${_SM_NC}" >&2
        return 1
    fi

    if [[ ! -f "$status_path" ]]; then
        echo -e "${_SM_RED}[state] status.json not found${_SM_NC}" >&2
        return 1
    fi

    local now
    now="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

    APCSTATE_PATH="$status_path" \
    APCSTATE_FRAMEWORK="$framework" \
    APCSTATE_RATIONALE="$rationale" \
    APCSTATE_NOW="$now" \
    python3 <<'PYEOF'
import json, os

path      = os.environ['APCSTATE_PATH']
framework = os.environ['APCSTATE_FRAMEWORK']
rationale = os.environ['APCSTATE_RATIONALE']
now       = os.environ['APCSTATE_NOW']

with open(path) as f:
    d = json.load(f)

d['ui_framework']  = framework
d['last_modified'] = now

fs = d.setdefault('framework_selection', {})
fs['decision']                = framework
fs['rationale']               = rationale
fs['implementation_strategy'] = 'single-pass'

with open(path, 'w') as f:
    json.dump(d, f, indent=2)
    f.write('\n')
PYEOF

    echo -e "${_SM_GREEN}[state] Framework set to '$framework' for $(basename "$plugin_path")${_SM_NC}"
}
