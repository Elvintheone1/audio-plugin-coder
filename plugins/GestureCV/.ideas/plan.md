# GestureCV — Implementation Plan

## Complexity Score: 3 / 5 (Advanced)

## Two Deliverables

This plugin ships as two things:
1. **`gesture_engine/`** — Python package (standalone process)
2. **`plugins/GestureCV/`** — JUCE VST3/AU plugin (router + UI)

---

## Implementation Strategy: Phased (score ≥ 3)

### Phase A: Python Gesture Engine

**Phase A.1 — Scaffold & Camera**
- [ ] `gesture_engine/` directory at repo root
- [ ] `requirements.txt`: `opencv-python`, `mediapipe`, `python-osc`, `python-rtmidi`, `pyyaml`
- [ ] `config.yaml` with all defaults from parameter-spec
- [ ] `gesture_engine/main.py` — argument parsing, config load, main loop scaffold
- [ ] Camera capture loop with `cv2.VideoCapture` and FPS throttle

**Phase A.2 — Landmark Extraction**
- [ ] MediaPipe Hands integration (single hand, model_complexity=1)
- [ ] Feature extractor: compute all 8 gesture features from landmarks
- [ ] Unit test: print feature values to stdout with fixed test image

**Phase A.3 — One-Euro Filter**
- [ ] `gesture_engine/one_euro.py` — pure Python one-euro filter class
- [ ] 8 filter instances, one per feature lane
- [ ] Verified: smooth output on real webcam input

**Phase A.4 — OSC Output**
- [ ] `python-osc` sender: `/gesture/lanes [f×8]` bundle to localhost:9000
- [ ] Graceful reconnect loop (no crash if plugin not running)
- [ ] MIDI CC fallback mode (rtmidi virtual port)

**Phase A.5 — Config & Launch**
- [ ] `config.yaml` hot-reload optional (nice to have)
- [ ] Shell launcher: `scripts/launch-gesture-engine.sh`
- [ ] macOS: add Continuity Camera setup notes to README

---

### Phase B: JUCE Plugin

**Phase B.1 — Project Scaffold**
- [ ] `plugins/GestureCV/CMakeLists.txt` — MIDI Effect plugin type
- [ ] `PluginProcessor.h/.cpp` — skeleton with APVTS, 8 atomic lane values
- [ ] `PluginEditor.h/.cpp` — empty WebView shell
- [ ] Verify builds cleanly with `bash scripts/build-and-install-mac.sh GestureCV`

**Phase B.2 — OSC Receiver + Lane Buffer**
- [ ] `juce::OSCReceiver` setup in Processor constructor
- [ ] `oscMessageReceived` callback: parse `/gesture/lanes`, write to `laneValues[8]` atomics
- [ ] Port read from APVTS `osc_port` param
- [ ] Auto-reconnect: re-bind on port change

**Phase B.3 — Lane Processing (processBlock)**
- [ ] Read atomics → per-lane smooth (1-pole IIR from `lane{N}_smooth`)
- [ ] `applyCurve()` function
- [ ] Scale + offset + clamp
- [ ] Quantize → MIDI CC emit (only on value change)
- [ ] Test: verify MIDI CC output in a DAW

**Phase B.4 — MIDI CC Input Mode**
- [ ] Parse incoming MIDI CC in processBlock
- [ ] CC → lane index reverse lookup
- [ ] Toggle between OSC and MIDI input modes

**Phase B.5 — UI (WebView)**
- [ ] `index.html` scaffold: dark theme, 8-lane layout
- [ ] Lane meters: animated bars reading from JS polling via `__JUCE__` native callbacks
- [ ] CC assignment dropdowns per lane
- [ ] Curve editor: SVG S-curve preview updating on drag
- [ ] Connection indicator: green ring pulses when OSC packets arrive
- [ ] Scale/offset sliders per lane (collapsible panel)

---

## Dependencies

**Python (gesture_engine)**
- `opencv-python` >= 4.8
- `mediapipe` >= 0.10
- `python-osc` >= 1.8
- `python-rtmidi` >= 1.5
- `pyyaml` >= 6.0

**JUCE Modules**
- `juce_audio_processors`
- `juce_audio_basics`
- `juce_osc`
- `juce_gui_extra` (WebBrowserComponent)

**CMake flags (macOS)**
- `NEEDS_WEB_BROWSER FALSE` (WKWebView, no flag needed on macOS)
- Plugin type: `IS_MIDI_EFFECT TRUE`

---

## Risk Assessment

**High Risk**
- **Threading: OSC → audio thread handoff.** OSC arrives on message thread; `processBlock` runs on audio thread. Must use `std::atomic<float>` array — no locks. If we accidentally use a mutex or notify, we risk priority inversion and dropouts.
- **MIDI Effect plugin type.** `IS_MIDI_EFFECT TRUE` in JUCE disables audio ports entirely. Some DAWs (Logic) route MIDI effects differently from instrument MIDI FX. Test in both Logic and Ableton early.
- **MediaPipe on Apple Silicon.** MediaPipe Python package has had ARM wheel availability issues. Verify `pip install mediapipe` works on M1/M2 before starting Phase A.2.

**Medium Risk**
- **OSC packet loss.** UDP is fire-and-forget. At 60fps we can tolerate occasional drops (last known value held). But if Python process lags, UI meters will freeze. Add timestamp field to OSC bundle for staleness detection.
- **Continuity Camera index stability.** Index 0 is not guaranteed to be Continuity Camera if other webcams are connected. May need a camera-picker in config or a scan-and-select in the launcher.
- **WebView 60Hz meter redraws.** Polling JS timers at 60fps can tax the UI thread. Use `requestAnimationFrame` for meter redraws, not `setInterval`.

**Low Risk**
- Per-lane math (smooth/curve/scale/offset) — trivial float operations
- MIDI CC output — JUCE handles this cleanly
- OSC receiver binding — well-tested JUCE module

---

## Recommended Implementation Order

1. Get Python engine running and printing gesture values to terminal — validate MediaPipe works on your machine
2. Add OSC output — verify with a simple OSC monitor before touching JUCE
3. Build JUCE scaffold + OSC receiver — log received values to debug console
4. Wire processBlock lane processing + MIDI CC output — test in DAW
5. Build UI last — all the interesting logic is already working by then

---

## Files to Create

```
gesture_engine/
├── main.py
├── feature_extractor.py
├── one_euro.py
├── osc_sender.py
├── midi_sender.py
├── config.yaml
└── requirements.txt

plugins/GestureCV/
├── CMakeLists.txt
├── status.json                     ← already exists
├── .ideas/                         ← already exists
└── Source/
    ├── PluginProcessor.h
    ├── PluginProcessor.cpp
    ├── PluginEditor.h
    ├── PluginEditor.cpp
    └── ui/
        └── public/
            └── index.html

scripts/
└── launch-gesture-engine.sh
```
