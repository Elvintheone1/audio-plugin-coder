# Style Guide v1
**Plugin:** Splice | **Date:** 2026-03-20

---

## Visual Language
Clean, minimalistic, cubistic. Inspired by Slate + Ash Cycles.
Muted, sophisticated color palette on a very dark background. Typography-forward. No gradients except in SEQ lane cells (value encoding). No drop shadows. Flat geometry.

---

## Color Palette

### Background
| Role | Hex | Usage |
|------|-----|-------|
| Body background | `#1A1A1A` | Plugin window |
| Block background | `#252525` | Section/lane blocks |
| Cell inactive | `#141414` | Muted/off slices |
| Panel | `#222222` | Controls area |
| Border | `#333333` | Dividers, borders |
| Hover/selected | `#2A2A2A` | Buttons, ctrl tabs |

### Section Colors (Loop page)
| Section | Hex | Hue |
|---------|-----|-----|
| I | `#D4604A` | Terracotta / coral |
| II | `#4A7BA8` | Slate blue |
| III | `#5A8A6A` | Sage green |
| IV | `#B87888` | Dusty rose |

Section color lit (playing): `color-mix(in srgb, <section-color>, white 40%)`
Fallback explicit lits:
- I lit: `#DF8C7A`
- II lit: `#7AA0C0`
- III lit: `#84A892`
- IV lit: `#CDA4B0`

### SEQ Lane Colors
| Role | Hex |
|------|-----|
| Cell step (base) | `#2A2A2A` |
| Cell step value (full) | `#888888` |
| Cell active/playhead | `#4A8AA8` |
| Cell value gradient | `#222` → `#888` (7 stops) |

### Text
| Role | Hex |
|------|-----|
| Primary | `#E8E8E8` |
| Secondary | `#888888` |
| Dim | `#555555` |
| Disabled | `#3A3A3A` |

### Navigation
| Role | Hex |
|------|-----|
| Active page tab bg | `#2E6B8A` |
| Active page tab text | `#E8E8E8` |
| Inactive tab text | `#888888` |

---

## Typography

**Font stack:** `-apple-system, BlinkMacSystemFont, 'SF Pro Display', 'Segoe UI', Inter, sans-serif`

| Role | Size | Weight | Transform | Tracking |
|------|------|--------|-----------|---------|
| Plugin name | 15px | 600 | uppercase | 0.05em |
| Reel name | 12px | 400 | none | 0 |
| Section header (I–IV) | 9px | 600 | uppercase | 0.1em |
| Tab label | 9–11px | 500 | uppercase | 0.08em |
| Ctrl group label | 9px | 600 | uppercase | 0.08em |
| Parameter label | 9px | 400 | uppercase | 0.06em |
| Parameter value | 11px | 400 | none | 0 |
| Grid number | 24px | 300 | none | 0 |
| Lane name | 10px | 600 | uppercase | 0.1em |
| Lane value | 10px | 400 | none | 0 |

---

## Spacing & Geometry

| Element | Value |
|---------|-------|
| Window padding (L/R) | 28px |
| Section block gap | 14px |
| Block border-radius | 4px |
| Cell border-radius | 2px |
| Cell gap | 3px |
| Controls area height | 160px |
| Top bar height | 52px |
| SEQ global bar height | 48px |

---

## Control Styles

### Direction buttons (4 per group)
- 28×28px squares
- Background: `#2A2A2A`
- Active border: `1px solid #555555`
- Active text: `#E8E8E8`
- Inactive text: `#555555`
- Border-radius: 3px
- Symbols: → ← ↔ ~ (forward / reverse / pingpong / random)

### Envelope sliders
- Vertical orientation
- Height: 60px
- Accent color: `#E8E8E8`
- Label below, 9px, dim

### Page/mode buttons
- Minimal, no background unless active
- Active state: filled bg or border

---

## State Variations

### Cells — Loop page
```
Active + playing:  section-color-lit (#DF8C7A / #7AA0C0 / #84A892 / #CDA4B0)
Active + idle:     section-color     (#D4604A / #4A7BA8 / #5A8A6A / #B87888)
Inactive (muted):  #141414
```

### Cells — SEQ page
```
Playhead:  #4A8AA8
Value 0.9: #888888
Value 0.7: #666666
Value 0.5: #444444
Value 0.3: #333333
Value 0.1: #222222
Empty:     #2A2A2A
```
