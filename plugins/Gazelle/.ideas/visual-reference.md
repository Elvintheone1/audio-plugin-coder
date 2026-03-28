# GAZELLE — Visual Reference
*Extracted from Antilope front panel photograph*

---

## Panel & Background

- **Panel base color:** Very dark charcoal — `#0E0E0E` / `#111111` (not pure black)
- **Background artwork:** Dense medieval illuminated manuscript illustrations silk-screened
  across the entire panel in gold — figures, constellations, celestial charts, Latin text
  ("uti mirant stella"), star/asterisk ornaments. This is the defining texture.
- **Artwork color:** Warm gold/brass — `#C4960A` / `#B8860B`. Not bright chrome gold —
  aged, slightly muted, like printed ink on metal.
- **Artwork density:** Heavy in the left and center; less so on the right (functional area)
- **Feel:** Like a medieval cartographer's panel that also happens to control audio.

---

## Control Styles — Neve Rotary Buttons

**Critical detail:** The controls on the Antilope are actual **Neve buttons/rotary selectors
salvaged from Neve hardware units (1073 preamp, EQ, etc.)** — not generic knobs.

### Neve Rotary Form Factor
- **Shape:** Short, wide flat-top cylinder — a "puck", not a traditional tapered knob
- **Cap face:** Solid colored flat top — the color IS the entire face surface
- **Indicator:** Small notch or dot at the edge of the face (not a pointer line)
- **Side wall:** Narrow cylindrical band below the face, slightly darker — visible as a rim
- **Origin:** Neve 1073 / 1084 / 1081 rotary frequency/gain selectors

### CSS/SVG Rendering Strategy (for Gazelle)
```
Outer ring:    dark halo (box-shadow or stroke)    — grounding shadow
Side wall:     narrow band, color × 0.6 brightness — 3D depth
Cap face:      flat colored circle                  — the actual Neve top
Indicator:     tiny arc or dot near face edge       — current value
```

### Color by Section
| Section | Cap Color | Hex (approx) | Neve Source |
|:--------|:----------|:-------------|:------------|
| Filter | Deep brick red | `#B81212` | Neve 1073 input/freq selectors |
| Distortion | Matte black | `#1C1C1C` | Neve gain/pad buttons |
| FX | Cobalt blue | `#1848A0` | Neve EQ/aux selectors |
| Output/misc | Dark grey | `#3A3A3A` | Neve secondary controls |

**Sizes:** Large Neve puck = 52–60px diameter for primary controls.
Secondary = 36–40px. The large red filter controls are the most dominant visual element.

---

## Trigger Buttons (Cherry MX style)

- **Form:** Square mechanical keyboard key caps — tall, chunky, beveled edges
- **Color:** Cream/beige — `#E8E0C8` / `#EAE2CA`
- **LED indicators:** Small colored LED dots visible on key caps (orange/amber when active)
- **Quantity on Antilope:** 4 keys (for 3 CV channels + 1 extra)
- **For Gazelle:** 2 keys — one per filter path (I = red LED, II = blue LED)
- **Placement on Antilope:** Right section, mid-height, prominent cluster

---

## Typography & Labels

- **Font style:** Small, tight, slightly decorative sans-serif caps — not a standard system
  font, has a slightly engraved/etched quality
- **Color:** Same gold as artwork — `#C4960A`
- **Numbering:** Roman numerals (i, ii, iii) for channel/section numbering
- **Brand name:** Gothic blackletter — "Antilope" with elaborate decorated initial "A"
- **Maker's mark:** Small bordered rectangle, bottom right — minimal text inside

---

## Geometric Pattern (bottom right)

- **Style:** Voronoi / geodesic / hexagonal tiling — organic polygon mesh
- **Color:** Tan/copper — `#C4A05A` / `#B89040`
- **Feel:** Like a dried mud crack or a natural crystal structure
- **Size:** Occupies roughly 25% of panel width, bottom right quadrant
- **For Gazelle:** Can be used as a decorative element in the output/VU area

---

## Color Palette Summary

```
Panel background:    #0E0E0E   (very dark charcoal)
Gold artwork/labels: #C4960A   (aged brass/gold)
Filter knobs:        #B81212   (deep brick red)
Distortion knobs:    #1A1A1A   (matte black)
FX knobs:            #1848A0   (cobalt blue)
Trigger button caps: #E8E0C8   (cream/beige)
LED red (active):    #FF3A1A   (trigger I)
LED blue (active):   #2080FF   (trigger II)
Geometric pattern:   #C4A05A   (tan/copper)
Secondary knobs:     #3A3A3A   (dark grey)
```

---

## Section Layout (Antilope → Gazelle mapping)

```
ANTILOPE PANEL (left→right):
├── Filter (red knobs, large)      → Gazelle: ENVELOPE + FILTER
├── Distortion (black knobs)       → Gazelle: DISTORTION
├── FX (blue knobs + display)      → Gazelle: FX ENGINE
└── Pattern recorder (faders+MX)  → Gazelle: OUTPUT + TRIGGER BUTTONS

ANTILOPE bottom strip:
└── CV jacks (gold nuts, labelled) → Gazelle: not applicable (MIDI only)
```

---

## Key Design Rules for Gazelle

1. **Gold artwork IS the background** — not a border, not icons. It underlays everything.
2. **Knob color = section identity** — red/black/blue must be consistent and clear
3. **Latin or archaic text fragments** as subtle decorative elements (not functional)
4. **Roman numerals** for Filter I / Filter II identification
5. **The trigger buttons must feel physical** — raised, square, cream, with LED glow
6. **Maker's mark** bottom right — small bordered box: "AJ / GAZELLE"
7. **No pure black, no pure white** — everything has warmth/age to it
8. **Voronoi pattern** can anchor the output/VU meter area visually
