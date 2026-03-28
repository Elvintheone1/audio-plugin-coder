# macOS Build Protocols for APC
**Platform:** macOS 10.15+ | Xcode | JUCE 8 | CMake | Bash

---

## 1. SHELL & SCRIPT CONVENTIONS

- **Shell:** Always use `bash` (not `zsh`) for APC scripts — `#!/bin/bash`
- **Paths:** Use forward slashes. Use `REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"` to get absolute root
- **macOS scripts:** All macOS equivalents of `.ps1` scripts live in `scripts/` with `-mac.sh` suffix:
  - `setup-mac.sh` — dependency installation (Homebrew, cmake, git submodules, pluginval)
  - `system-check-mac.sh` — dependency validation (`--json` flag for JSON output)
  - `build-and-install-mac.sh` — full build + install pipeline
  - `pluginval-mac.sh` — AU/VST3 validation
  - `au-cache-bust-mac.sh` — Logic X / DAW cache invalidation

---

## 2. CMAKE CONFIGURATION

### Required Flags for macOS
```bash
cmake -G Xcode \
    -B build \
    -S . \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15" \
    -DAPC_ENABLE_VISAGE=OFF
```

### CRITICAL: No Per-Target XCODE_ATTRIBUTE_ARCHS
**FORBIDDEN** — causes libSharedCode.a linker failure:
```cmake
# NEVER DO THIS:
set_target_properties(Plugin PROPERTIES
    XCODE_ATTRIBUTE_ARCHS "x86_64;arm64"
)
```

**CORRECT:** Pass `CMAKE_OSX_ARCHITECTURES` at configure time only (see above).

### WKWebView (macOS WebView)
- macOS uses system `WKWebView` — no `NEEDS_WEBVIEW2` flag needed
- `NEEDS_WEB_BROWSER` flag is for Linux only
- CMake config for macOS WebView plugin:
```cmake
if(APPLE)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)   # macOS uses WKWebView (no flag required)
endif()

juce_add_plugin(PluginName
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    ...
)
```

### Deployment Target
- Minimum: **10.15** (WKWebView with JUCE native integration requires Catalina+)
- Set both in CMake flag AND in Xcode project settings
- If targeting older macOS: use JUCE's non-native WebView fallback (not recommended)

---

## 3. BUILD TARGETS

```bash
# Build VST3 + AU + Standalone
cmake --build build \
    --target ${PLUGIN_NAME}_VST3 ${PLUGIN_NAME}_AU ${PLUGIN_NAME}_Standalone \
    --config Release \
    -- -quiet
```

- Target suffix format: `<PluginName>_VST3`, `<PluginName>_AU`, `<PluginName>_Standalone`
- Always use `--config Release` — Debug builds include extra JUCE assertions
- `-quiet` suppresses Xcode noise (use `-verbose` for debugging)

---

## 4. PLUGIN ARTIFACT PATHS

After a successful build, artifacts live at:
```
build/plugins/<Name>/<Name>_artefacts/Release/
    VST3/<Name>.vst3          # Bundle directory
    AU/<Name>.component       # Bundle directory
    Standalone/<Name>.app     # Standalone app
```

### Install Locations
```
~/Library/Audio/Plug-Ins/VST3/<Name>.vst3          # User VST3
~/Library/Audio/Plug-Ins/Components/<Name>.component # User AU
```

System-wide (requires sudo, not recommended for development):
```
/Library/Audio/Plug-Ins/VST3/
/Library/Audio/Plug-Ins/Components/
```

---

## 5. AU PLUGIN TESTING PROTOCOL

**Logic X caches AU plugins aggressively.** After every install:

### Step 1: Bust the cache
```bash
bash scripts/au-cache-bust-mac.sh <PluginName>
```
This kills `AudioComponentRegistrar`, removes `~/Library/Caches/AudioUnitCache`, and restarts `coreaudiod`.

### Step 2: Fully quit DAW
- Logic X: **Cmd+Q** (not just close project window)
- Ableton: **Cmd+Q**
- Reopen after 5 seconds

### Step 3: Validate with auval
```bash
# <type> is aufx for effect, aumu for instrument, aumx for MIDI
auval -v aufx BdEy AJzz
#            ^--- OSType (4-char, from CMakeLists: PLUGIN_CODE)
#                  ^--- Manufacturer (4-char, from CMakeLists: PLUGIN_MANUFACTURER_CODE)
```

### Fastest Testing Path (No DAW)
```bash
open "build/plugins/<Name>/<Name>_artefacts/Release/Standalone/<Name>.app"
```
Standalone app uses the same audio engine and UI — use this for rapid iteration.

---

## 6. CODE SIGNING

### Development (ad-hoc signing)
JUCE builds are automatically code-signed with your developer certificate if Xcode is configured. For development without a certificate:
```bash
# Sign ad-hoc (no certificate needed, not Gatekeeper-notarized)
codesign --deep -f -s - path/to/Plugin.component
codesign --deep -f -s - path/to/Plugin.vst3
```

### Distribution (notarization)
- Requires Apple Developer account
- VST3 and AU must both be notarized for Gatekeeper clearance
- Use `xcrun notarytool` (not deprecated `altool`)
- Out of scope for development builds; handle in `/ship` phase

---

## 7. WEBVIEW (WKWebView) SPECIFICS

### Resource Provider Root
JUCE 8's `WebBrowserComponent` on macOS uses WKWebView with a custom URL scheme.
`getResourceProviderRoot()` returns a string like `"juce://localhost/"`.

### JS → C++ Interop
Same API as Windows/Linux — `window.__JUCE__.postMessage(...)`, `addValueChangedListener()`.
No platform-specific JS code needed.

### Debugging the WebView on macOS
Enable Safari Web Inspector for WKWebView:
```bash
# Enable remote inspection (once)
defaults write com.apple.Safari WebKitDeveloperExtrasEnabledPreferenceKey -bool true
```
Then in the plugin's WebView: right-click → "Inspect Element" (only in Debug builds or when
`juce::WebBrowserComponent::Options::withNativeIntegrationEnabled()` is combined with the inspection flag).

---

## 8. UNIVERSAL BINARY (Apple Silicon + Intel)

### What "Universal" means
- `x86_64` — Intel Mac
- `arm64` — Apple Silicon (M1/M2/M3)
- `x86_64;arm64` — Fat binary, runs natively on both

### Verification
```bash
# Check if binary is truly universal
file build/plugins/<Name>/<Name>_artefacts/Release/VST3/<Name>.vst3/Contents/MacOS/<Name>
# Expected output: "Mach-O universal binary with 2 architectures: [x86_64:Mach-O 64-bit bundle x86_64] [arm64]"

lipo -archs build/plugins/<Name>/<Name>_artefacts/Release/VST3/<Name>.vst3/Contents/MacOS/<Name>
# Expected: "x86_64 arm64"
```

### If build fails with "missing arm64 slice"
Usually caused by a third-party static library that is x86_64 only.
- Check `lipo -archs <library>` for each linked static lib
- Either build that lib as universal or exclude it from AU/Universal targets

---

## 9. DEPENDENCY QUICK REFERENCE

| Tool | Install | Min Version |
|------|---------|-------------|
| Xcode.app | App Store | 14+ (for AU) |
| Xcode CLT | `xcode-select --install` | Any |
| CMake | `brew install cmake` | 3.22 |
| Git | `brew install git` | 2.x |
| JUCE | `git submodule update --init --recursive` | 8.x |
| pluginval | `bash scripts/setup-mac.sh` | latest |

---

## 10. COMMON ERRORS & RESOLUTIONS

### "libPlugin_SharedCode.a not found" (linker)
**Cause:** Per-target `XCODE_ATTRIBUTE_ARCHS` conflicts with global `CMAKE_OSX_ARCHITECTURES`.
**Fix:** Remove any `set_target_properties(... XCODE_ATTRIBUTE_ARCHS ...)` from CMakeLists.

### "Plugin not found" in Logic X after install
**Cause:** AU cache stale.
**Fix:** `bash scripts/au-cache-bust-mac.sh <PluginName>` then fully quit/reopen Logic.

### "WKWebView: Could not load URL" in plugin
**Cause:** Resource provider not returning data, or URL scheme mismatch.
**Fix:** Ensure `getResource()` handles the root URL (`/` → `/index.html`) and all `BinaryData` references are correct.

### Build succeeds but AU doesn't appear in DAW
**Cause:** `auval` validation failing silently, or wrong `PLUGIN_CODE`/`PLUGIN_MANUFACTURER_CODE`.
**Fix:**
```bash
auval -v aufx <PLUGIN_CODE> <MANUFACTURER_CODE>
# Codes are 4-character OSTypes from CMakeLists.txt
```
Check for spaces in OSType strings (must be exactly 4 chars, no spaces unless intentional).

### "codesign: error: The binary is not signed"
**Cause:** macOS Ventura+ requires AU components to be signed.
**Fix:** `codesign --deep -f -s - <path>.component`

---

## 11. SCRIPT USAGE SUMMARY

```bash
# First-time setup
bash scripts/setup-mac.sh

# Validate environment
bash scripts/system-check-mac.sh

# Build and install a plugin
bash scripts/build-and-install-mac.sh BeadyEye

# Bust AU cache after install
bash scripts/au-cache-bust-mac.sh BeadyEye

# Validate with pluginval
bash scripts/pluginval-mac.sh BeadyEye [--strict] [--verbose]

# Quick test (no DAW needed)
open "build/plugins/BeadyEye/BeadyEye_artefacts/Release/Standalone/BeadyEye.app"
```
