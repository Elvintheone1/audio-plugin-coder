# GestureCV — DSP Architecture Specification

## System Overview

Two completely separate processes. The JUCE plugin never touches camera data.

```
[Camera / Continuity Camera]
         │
         ▼
┌─────────────────────────────────┐
│   Python: gesture_engine.py     │
│   ─────────────────────────     │
│   OpenCV → MediaPipe Hands      │
│   21 landmarks × 3D (x,y,z)    │
│   ↓                             │
│   Feature Extractor             │
│   (8 gesture feature floats)    │
│   ↓                             │
│   One-Euro Filter × 8           │
│   ↓                             │
│   OSC encoder / MIDI CC sender  │
└─────────────┬───────────────────┘
              │ UDP localhost:9000 (OSC)
              │  OR virtual MIDI port
              ▼
┌─────────────────────────────────────────────────────┐
│   JUCE Plugin: GestureCV (VST3 / AU)                │
│   ──────────────────────────────────────────────    │
│   OSC Receiver (juce::OSCReceiver)                  │
│    OR MIDI CC Input (processBlock incoming MIDI)    │
│   ↓                                                 │
│   Lane Buffer (8 floats, 0.0–1.0)                   │
│   ↓                                                 │
│   Per-Lane Processing (×8 parallel):                │
│     → Plugin-side Smoother (1-pole IIR)             │
│     → Curve Shape (log/linear/exp)                  │
│     → Scale × Offset clamp                         │
│     → CC quantize (float → 0–127 int)               │
│   ↓                                                 │
│   MIDI CC Output (processBlock outgoing MIDI)       │
│   ↓                                                 │
│   DAW Automation / Instrument Parameters            │
└─────────────────────────────────────────────────────┘
```

---

## Component 1: Python Gesture Engine

### 1.1 Camera Capture
- `cv2.VideoCapture(camera_index)` — camera_index from config.yaml (0 = Continuity Camera)
- Frame resize to 640×480 before passing to MediaPipe (performance)
- Target loop: 60 fps with `time.sleep` throttle

### 1.2 MediaPipe Hands
- `mp.solutions.hands.Hands(max_num_hands=1, model_complexity=1)`
- Extracts 21 landmarks per hand: `[wrist, thumb_cmc, thumb_mcp, thumb_ip, thumb_tip, index_mcp, ..., pinky_tip]`
- Each landmark: `(x, y, z)` normalized to [0,1] in camera frame; z is depth (negative = closer)

### 1.3 Feature Extractor
Derived from raw landmarks:

| Feature | Computation |
|---------|-------------|
| `hand_x` | `wrist.x` |
| `hand_y` | `1.0 - wrist.y` (flip Y: up = higher value) |
| `hand_z` | `clamp(1.0 + wrist.z * 5.0, 0, 1)` (depth, closer = higher) |
| `finger_spread` | mean of 4 inter-MCP angles (index–middle, middle–ring, ring–pinky) normalized |
| `pinch` | `1.0 - clamp(dist(thumb_tip, index_tip) / max_pinch_dist, 0, 1)` |
| `wrist_tilt` | angle of vector (wrist → middle_mcp) relative to vertical, normalized |
| `curl_index` | `1.0 - angle(index_mcp, index_pip, index_tip) / 180°` |
| `curl_ring` | same formula for ring finger |

### 1.4 One-Euro Filter
Applied per-feature. Parameters from config.yaml:
- `mincutoff` (default 1.0): controls steady-state smoothing
- `beta` (default 0.01): controls lag reduction on fast motion
- One instance per feature lane (8 total)

### 1.5 Output Encoding
**OSC mode** (primary):
- Packet format: `/gesture/lanes [f f f f f f f f]` — 8 floats in one bundle
- Sent to `osc_host:osc_port` at every processed frame
- Library: `python-osc`

**MIDI CC mode** (fallback):
- Float 0.0–1.0 → int 0–127 via `int(value * 127)`
- Send on virtual port `GestureCV` (rtmidi)
- CC numbers from `config.yaml`; default order matches parameter-spec defaults

---

## Component 2: JUCE Plugin

### 2.1 OSC Receiver Thread
- `juce::OSCReceiver` listening on UDP port (from `osc_port` param)
- Callback on message thread: parse `/gesture/lanes` → write to `laneValues[8]` atomic float array
- Port auto-reconnect if Python process restarts

### 2.2 MIDI Input Handler (alternative mode)
- In `processBlock`, iterate incoming MIDI messages
- Filter CC messages on channel `midi_ch`; map CC number → lane index via reverse lookup table
- Write to same `laneValues[8]` atomic float array

### 2.3 Lane Buffer
```
std::atomic<float> laneValues[8];   // written by OSC/MIDI thread
float laneSmoothed[8];              // written in processBlock (audio thread)
```
Lock-free: OSC writes atomics; processBlock reads atomics into audio-thread-local smoothed values.

### 2.4 Per-Lane Processing (audio thread, processBlock)
For each of 8 lanes:

```
Step 1 — Read: raw = laneValues[lane].load()
Step 2 — Smooth: smoothed = smoothed + (raw - smoothed) * smoothCoeff
           smoothCoeff derived from lane{N}_smooth param + global_smooth
Step 3 — Curve: curved = applyCurve(smoothed, lane{N}_curve)
           curve < 0: logarithmic (fine control at low values)
           curve = 0: linear passthrough
           curve > 0: exponential (fine control at high values)
Step 4 — Scale+Offset: out = clamp(curved * scale + offset, 0, 1)
Step 5 — Quantize: ccVal = juce::roundToInt(out * 127)
Step 6 — Emit: if ccVal != lastCcVal[lane], send MIDI CC(lane{N}_cc, ccVal, midi_ch)
```

### 2.5 MIDI CC Output
- Generated in `processBlock` — added to outgoing MIDI buffer
- DAW routes this like any MIDI CC source (mod wheel, expression pedal, etc.)
- Plugin type: **MIDI Effect** (not instrument, not audio FX) — passes audio through dry

### 2.6 applyCurve Function
```
float applyCurve(float x, float curve) {
    if (curve == 0.0f) return x;
    if (curve > 0.0f)  return std::pow(x, 1.0f + curve * 3.0f);   // exponential
    else               return 1.0f - std::pow(1.0f - x, 1.0f - curve * 3.0f); // log
}
```

---

## Plugin Signal Flow (JUCE)

```
[OSC Thread]  laneValues[8] atomics
                     │
              processBlock()
                     │
              ┌──────▼──────────────────────────────────┐
              │  For each lane (0–7):                    │
              │  raw → smooth → curve → scale+offset    │
              │  → quantize → MIDI CC out                │
              └──────────────────────────────────────────┘
                     │
              [DAW MIDI routing]
```

Audio: passed through unmodified (plugin is MIDI effect only).

---

## Class Structure (JUCE)

```
GestureCVProcessor : juce::AudioProcessor
  - OSCReceiver oscReceiver
  - std::atomic<float> laneValues[8]
  - float laneSmoothed[8]
  - int lastCcVal[8]
  - AudioProcessorValueTreeState params
  - void processBlock(buffer, midiMessages)
  - void oscMessageReceived(OSCMessage)

GestureCVEditor : juce::AudioProcessorEditor
  - WebBrowserComponent webView  (OR Visage panels — TBD)
  - Lane meters (8× animated bars showing laneValues)
  - Mapping matrix (8× CC selectors)
  - Curve editor per lane
  - Connection status indicator (green = OSC receiving)
```

---

## Complexity Assessment

**Score: 3 / 5 (Advanced)**

**Factors:**
- Cross-process IPC (OSC UDP) with lock-free handoff to audio thread — non-trivial threading model
- Python process is a separate deliverable (not just JUCE code)
- 8-lane parallel processing with per-lane curves, smoothing, CC output
- Plugin type is MIDI Effect — some DAWs handle this differently (Logic requires AU MIDI FX wrapper)
- OSC receiver must be robust to dropped packets and Python process restarts
- UI needs live meters at ~60Hz update rate — requires careful timer/repaint design

**Not complex:**
- DSP itself is trivial (no audio processing, just float math)
- No synthesis, convolution, or spectral processing
