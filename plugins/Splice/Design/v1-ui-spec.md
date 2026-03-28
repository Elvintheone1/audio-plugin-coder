# UI Specification v1
**Plugin:** Splice | **Date:** 2026-03-20 | **Framework:** WebView

---

## Window
- **Size:** 1000 × 600 px
- **Pages:** 2 — Loop (main), Seq (cartesian sequencer)
- **Navigation:** Tab buttons top-center (Loop / Seq), identical to Cycles

---

## Layout Structure

```
┌─────────────────────────────────────────────────────────────────┐
│  TOP BAR (52px)                                                  │
│  [Splice]  [reel name]    [Loop] [Seq]    [Poly][Slice][Arp]    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  GRID AREA (flex, ~340px)                                        │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                        │
│  │  I   │  │  II  │  │ III  │  │  IV  │                        │
│  │      │  │      │  │      │  │      │                        │
│  │ ████ │  │ ████ │  │ ████ │  │ ████ │                        │
│  │ ████ │  │ ████ │  │ ████ │  │ ████ │                        │
│  └──────┘  └──────┘  └──────┘  └──────┘                        │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│  CONTROLS AREA (160px)                                           │
│  [Playhead/Signal/Random] │ [Amp Env/Filter/Filter Env]         │
│  Direction + Grid          │ ADSR or Filter params               │
└─────────────────────────────────────────────────────────────────┘

SEQ PAGE — same top bar, grid area shows 4 lane blocks:
┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐
│ POS  │  │ VEL  │  │PITCH │  │ DIR  │
│      │  │      │  │      │  │      │
│ ████ │  │ ████ │  │ ████ │  │ ████ │  (gray gradient cells)
│ ████ │  │ ████ │  │ ████ │  │ ████ │
└──────┘  └──────┘  └──────┘  └──────┘
[Step Length: /16]  [On / Gen / Reset]
```

---

## Controls — Loop Page

### Top Bar
- Left: `Splice` (plugin name, uppercase, bold) + reel filename
- Center: `Loop` / `Seq` toggle tabs
- Right: `Poly` / `Slice` / `Arp` mode buttons (SEQ mode is the Seq page itself)

### Grid Area
- 4 section blocks in a row, equal width, ~320px tall
- Gap between blocks: 14px
- Padding: 28px left/right, 20px top, 12px bottom
- Each block: rounded 4px corners, dark background `#252525`
- Section header: Roman numeral (I, II, III, IV), 9px, dim
- Cell grid inside: fills remaining height

### Cell behavior
- Active: section color
- Inactive (muted): near-black `#141414`
- Currently playing: brightened section color (+40% white mix)
- Click to toggle active ↔ inactive

### Controls Area (bottom, 160px)
Split into two equal panels separated by vertical border:

**Left panel — Playhead tab:**
- Tabs: `Playhead` / `Signal` / `Random`
- Playhead content: two direction groups (Reel, Slice) + Grid control
  - Direction buttons: → ← ↔ ~ (4 symbol buttons per group)
  - Grid: large number display (`/4`, `/8`, `/16`, `/32`, `/64`)

**Right panel — Envelope tab:**
- Tabs: `Amp Env` / `Filter` / `Filter Env`
- Amp Env: 4 vertical sliders (A/D/S/R) with labels below
- Filter: Cutoff (slider), Resonance (slider), Type (LP/BP/HP buttons)
- Filter Env: Depth (bipolar slider), Attack, Decay

---

## Controls — SEQ Page

### Grid Area
- 4 lane blocks in a row (POS / VEL / PITCH / DIR), same proportions as Loop page
- Each block: dark `#252525` background
- Lane header: lane name (left) + current step value (right, dim)
- Cell grid: 4×4 default (variable based on step count)
- Cell colors: gray gradient by value (dark = low, light = high)
- Active/playing cell: accent blue `#4A8AA8`
- DIR lane: binary (bright = fwd, dim = rev)

### Lane footer (inside each block)
- `Steps: N` + `Pattern: Snake` — 8px dim labels

### SEQ global controls (bottom strip, 48px)
- `Step Length: /16`
- `On / Gen / Reset` (per-lane actions, shown globally for now)

---

## Interactions

| Action | Result |
|--------|--------|
| Click Loop/Seq tab | Switch page |
| Click Poly/Slice/Arp | Set playback mode |
| Click slice cell | Toggle active/inactive (black) |
| Click ctrl tab | Switch parameter panel |
| Click direction button | Set direction |
| Click lane cell (SEQ) | Edit value (v2 feature) |

---

## Color Palette

See `v1-style-guide.md` for full palette.

---

## Typography
- Font: System sans-serif (`-apple-system, 'SF Pro Display', 'Segoe UI', Inter, sans-serif`)
- Plugin name: 15px, weight 600, letter-spacing 0.05em, uppercase
- Section headers: 9px, weight 600, letter-spacing 0.1em, uppercase
- Tab labels: 9–11px, weight 500, letter-spacing 0.08em, uppercase
- Control labels: 9px, letter-spacing 0.06em, uppercase
- Values: 11–24px (varies by control)
