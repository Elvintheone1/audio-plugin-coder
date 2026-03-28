# Splice — Parameter Specification
**Version:** v0.4 | **Date:** 2026-03-20 | **Phase:** Ideation (complete)

---

## Architecture Summary

```
AUDIO ENGINE
└── Reel (1 bar, 4/4, tempo-synced)
    └── 4 sections (quarter notes, default — post-v1: 3–7)
        └── Each section: 1–16 slices (Grid control)
            Max: 4 × 16 = 64 slices

PLAYBACK MODES
├── POLY  — chromatic keyboard, time-stretched, polyphonic (default)
├── SLICE — sampler map C1 upward, one key per slice
├── ARP   — arpeggiate held keys, tempo-synced, Hold option
└── SEQ   — cartesian sequencer page (POS / VEL / PITCH / DIR)
```

---

## JUCE Parameters

### GROUP 1: Global
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `playback_mode` | Mode | Choice | poly / slice / arp / seq | poly |
| `grid` | Grid | Int | 1–16 | 4 |

---

### GROUP 2: Direction
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `reel_dir` | Reel Direction | Choice | fwd / rev / pingpong / random | fwd |
| `slice_dir` | Slice Direction | Choice | fwd / rev / pingpong / random | fwd |

---

### GROUP 3: Amp Envelope (global, re-triggers per slice event)
| ID | Name | Type | Range | Default | Unit |
|:---|:-----|:-----|:------|:--------|:-----|
| `amp_attack` | Attack | Float | 0.001–2.0 | 0.005 | s |
| `amp_decay` | Decay | Float | 0.001–2.0 | 0.1 | s |
| `amp_sustain` | Sustain | Float | 0.0–1.0 | 1.0 | — |
| `amp_release` | Release | Float | 0.001–4.0 | 0.1 | s |

---

### GROUP 4: Filter (global)
| ID | Name | Type | Range | Default | Unit |
|:---|:-----|:-----|:------|:--------|:-----|
| `filter_cutoff` | Cutoff | Float | 20–20000 | 18000 | Hz |
| `filter_res` | Resonance | Float | 0.0–1.0 | 0.0 | — |
| `filter_type` | Type | Choice | LP / BP / HP | LP | — |

---

### GROUP 5: Filter Envelope (global, re-triggers per slice event)
| ID | Name | Type | Range | Default | Unit |
|:---|:-----|:-----|:------|:--------|:-----|
| `fenv_depth` | Depth | Float | -1.0–1.0 | 0.0 | — |
| `fenv_attack` | Attack | Float | 0.001–2.0 | 0.01 | s |
| `fenv_decay` | Decay | Float | 0.001–4.0 | 0.2 | s |

---

### GROUP 6: ARP
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `arp_rate` | Rate | Choice | 1/4 / 1/8 / 1/16 / 1/32 | 1/16 |
| `arp_hold` | Hold | Bool | on / off | off |
| `arp_pattern` | Pattern | Choice | up / down / updown / as-played / random | up |

---

### GROUP 7: Mod Matrix (one slot per parameter — LFO OR MIDI CC)

**LFO:**
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `lfo_shape` | Shape | Choice | sine / tri / square / saw / random | sine |
| `lfo_rate` | Rate | Float | 0.01–20 Hz or synced divisions | synced |
| `lfo_sync` | Sync | Bool | on / off | on |
| `lfo_depth` | Depth | Float | 0.0–1.0 | 0.0 |

**MIDI CC:**
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `cc_number` | CC | Int | 0–127 | 1 (mod wheel) |
| `cc_depth` | Depth | Float | 0.0–1.0 | 0.0 |

---

### GROUP 8: SEQ — Global
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `seq_step_length` | Step Length | Choice | 1/4 / 1/8 / 1/16 / 1/32 | 1/16 |

---

### GROUP 9: SEQ — Lanes (×4)

Always a 2D grid. Default 16 steps (4×4). Step count variable → grid shape changes:
```
16 steps → 4×4
8 steps  → 4×2
4 steps  → 2×2
```

| Lane | Function | Value type |
|:-----|:---------|:-----------|
| **POS** | Which slice is triggered at each step | Slice index (0–63) |
| **VEL** | Velocity per step | 0–127 |
| **PITCH** | Pitch offset per step | Semitones |
| **DIR** | Slice direction per step | fwd / rev |

Per-lane settings:
| ID | Name | Type | Values | Default |
|:---|:-----|:-----|:-------|:--------|
| `lane_N_steps` | Steps | Int | 1–16 | 16 |
| `lane_N_pattern` | Pattern | Choice | forward / reverse / snake / diagonal / random / drunk | snake |

**Traversal patterns:**
- **Forward** — L→R, top to bottom
- **Reverse** — R→L, bottom to top
- **Snake** — L→R row 1, R→L row 2 (boustrophedon)
- **Diagonal** — diagonal lines top-left → bottom-right
- **Random** — random cell each step
- **Drunk** — random walk to adjacent cell (Brownian)

**Lane grid state (serialized, not JUCE params):**
- `lane_N_values[16]` — value per step (float)

---

### GROUP 10: Output
| ID | Name | Type | Range | Default |
|:---|:-----|:-----|:------|:--------|
| `output_vol` | Volume | Float | 0.0–1.0 | 0.75 |

---

## Grid State (serialized, not JUCE params)
| Property | Type | Notes |
|:---------|:-----|:------|
| `slice_active[64]` | Bool | Click to toggle on/off. Black = muted, skipped on playback. |

---

## Post-v1 (not in scope)
- Section count: 3–7 (currently fixed at 4)
- Smear — overlapping slice playback
- FX — reverb, delay
- Motion sequencer — automation via mod wheel / XY pad

---

## Complexity Assessment (v1)
- **Grid + serialization:** medium
- **Time-stretch (Poly/Arp):** high
- **4-lane 2D cartesian sequencer:** high
- **Mod matrix (LFO or CC per parameter):** medium
- **Filter + filter envelope:** medium
- **File loading:** medium
- **WebView UI:** high
- **Overall:** ⚠️ High — scope carefully at /plan
