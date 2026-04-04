# GestureCV — Parameter Spec

## Gesture Feature Lanes (Python → Plugin, 8 lanes)

These are the computed gesture features emitted by the Python process as MIDI CC values (0–127):

| Lane ID | Name | Description | Raw Range |
|---------|------|-------------|-----------|
| `hand_x` | Hand X | Horizontal hand position in camera frame | 0.0–1.0 |
| `hand_y` | Hand Y | Vertical hand position in camera frame | 0.0–1.0 |
| `hand_z` | Hand Z | Depth estimate (hand size proxy) | 0.0–1.0 |
| `finger_spread` | Spread | Mean inter-finger angle (open=1, fist=0) | 0.0–1.0 |
| `pinch` | Pinch | Thumb-index tip distance (normalized) | 0.0–1.0 |
| `wrist_tilt` | Tilt | Wrist roll angle | 0.0–1.0 |
| `curl_index` | Index Curl | Index finger curl (extended=0, curled=1) | 0.0–1.0 |
| `curl_ring` | Ring Curl | Ring finger curl | 0.0–1.0 |

---

## JUCE Plugin Parameters (per lane, 8 lanes × 5 params = 40 total)

Each lane has an identical set of routing parameters:

| ID Pattern | Name | Type | Range | Default | Notes |
|------------|------|------|-------|---------|-------|
| `lane{N}_cc` | CC Number | Int | 0–127 | see defaults | Target MIDI CC number |
| `lane{N}_scale` | Scale | Float | 0.0–2.0 | 1.0 | Output multiplier |
| `lane{N}_offset` | Offset | Float | -1.0–1.0 | 0.0 | DC shift before output |
| `lane{N}_curve` | Curve | Float | -1.0–1.0 | 0.0 | neg=log, 0=linear, pos=exp |
| `lane{N}_smooth` | Smooth | Float | 0.0–1.0 | 0.3 | Plugin-side additional smoothing |

### Default CC Assignments
| Lane | Default CC | Common Use |
|------|------------|------------|
| hand_x | CC 1 (Mod Wheel) | Vibrato, cutoff sweep |
| hand_y | CC 11 (Expression) | Volume, reverb send |
| hand_z | CC 74 (Brightness) | Filter cutoff |
| finger_spread | CC 71 (Resonance) | Filter Q |
| pinch | CC 73 (Attack) | Envelope attack |
| wrist_tilt | CC 91 (Reverb) | Effect depth |
| curl_index | CC 70 (Variation) | User-defined |
| curl_ring | CC 72 (Release) | Envelope release |

---

## Global Plugin Parameters

| ID | Name | Type | Range | Default | Notes |
|----|------|------|-------|---------|-------|
| `input_mode` | Input Mode | Enum | OSC / MIDI | OSC | How plugin receives gesture data |
| `osc_port` | OSC Port | Int | 1024–65535 | 9000 | UDP port to listen on |
| `midi_ch` | MIDI Channel | Int | 1–16 | 1 | Output MIDI channel |
| `global_smooth` | Global Smooth | Float | 0.0–1.0 | 0.0 | Added on top of per-lane smooth |
| `hand_select` | Hand | Enum | Left / Right / Both | Right | Which hand to track |

---

## Python Process Config (not JUCE params, stored in config.yaml)

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| `camera_index` | Int | 0 | Webcam index (0 = Continuity Camera default) |
| `target_fps` | Int | 60 | Capture loop target |
| `one_euro_mincutoff` | Float | 1.0 | One-euro filter: lower = smoother |
| `one_euro_beta` | Float | 0.01 | One-euro filter: higher = less lag on fast moves |
| `output_osc_host` | String | 127.0.0.1 | Target for OSC packets |
| `output_osc_port` | Int | 9000 | Must match plugin `osc_port` |
| `output_midi_port` | String | GestureCV | Virtual MIDI port name |
