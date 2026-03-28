# GAZELLE — Style Guide v1

---

## Color Palette

```css
:root {
  --panel:        #0E0E0E;   /* charcoal panel — not pure black */
  --gold:         #C4960A;   /* aged brass — labels, artwork, dividers */
  --gold-dim:     #C4960A55; /* muted gold — value displays, borders */
  --gold-faint:   #C4960A22; /* ghost gold — VU border, subtle lines */

  --red:          #B81212;   /* Neve filter cap — deep brick red */
  --red-wall:     #6A0808;   /* side wall of red puck */
  --blue:         #1848A0;   /* Neve FX cap — cobalt blue */
  --blue-wall:    #0C2050;   /* side wall of blue puck */
  --grey:         #3A3A3A;   /* Neve ENV/OUT cap — dark grey */
  --grey-wall:    #1A1A1A;   /* side wall of grey puck */
  --black-cap:    #1C1C1C;   /* Neve DIST cap — matte black */
  --black-wall:   #0A0A0A;   /* side wall of black puck */

  --cream:        #E8E0C8;   /* Cherry MX key cap top */
  --cream-mid:    #D0C8A0;   /* Cherry MX key cap mid */
  --cream-dark:   #6A5A30;   /* Cherry MX key bottom shadow */

  --led-red:      #FF3A1A;   /* Trigger I LED active */
  --led-blue:     #2080FF;   /* Trigger II LED active */
  --led-off-r:    #3A0808;   /* Trigger I LED inactive */
  --led-off-b:    #081830;   /* Trigger II LED inactive */

  --voronoi:      #C4A05A;   /* Geodesic pattern — tan/copper */
}
```

---

## Neve Puck Knob Anatomy (SVG)

```
     ╔═══════════╗       ← outer shadow ring (#080808)
    ║ ╔═════════╗ ║      ← side wall (wall color, 5px band)
    ║ ║         ║ ║
    ║ ║  CAP    ║ ║      ← cap face (section color, radial gradient)
    ║ ║  FACE  •║ ║      ← indicator dot (#C4960A / #FFE080, orbits edge)
    ║ ╚═════════╝ ║
     ╚═══════════╝
```

**Rendering per size:**
| Size | Outer r | Wall band | Face r | Indicator orbit r |
|:-----|:--------|:----------|:-------|:------------------|
| 62px | 30 | 5px | 23 | 17 |
| 52px | 25 | 4px | 19 | 14 |
| 40px | 19 | 3px | 14 | 10 |
| 48px | 23 | 4px | 17 | 12 |

**Radial gradient:** center 40% lighter → edge = cap color (gives subtle dome illusion)

**Indicator dot:** 2.5px outer (#C4960A), 1.5px inner (#FFE080) — two circles for glow effect

**Value → angle mapping:**
- Min = −135° (7 o'clock), Max = +135° (5 o'clock), Center = 0° (12 o'clock)
- `angle_deg = -135 + normalValue * 270`
- `dot_x = cx + orbitR * sin(angle_rad)`
- `dot_y = cy − orbitR * cos(angle_rad)`

---

## Cherry MX Trigger Button

```css
.mx-btn {
  width: 50px; height: 50px;
  background: linear-gradient(150deg, #F2EAD0, #E0D4A8 40%, #C8BC88);
  border-radius: 3px;
  box-shadow:
    inset 0 1px 2px rgba(255,255,255,0.4),  /* top highlight */
    0 5px 0 #6A5A30,                          /* physical depth */
    0 7px 10px rgba(0,0,0,0.7);              /* ground shadow */
}
.mx-btn:active {
  transform: translateY(4px);
  box-shadow:
    inset 0 1px 2px rgba(255,255,255,0.2),
    0 1px 0 #6A5A30,
    0 2px 4px rgba(0,0,0,0.5);
}
```

LED: 8×8px circle, bottom-right of cap. Off = very dark. On = bright + glow.

---

## FX Type Selector Buttons

```css
.fx-type-btn {
  padding: 5px 10px;
  background: #0C1828;
  border: 1px solid #1848A033;
  color: #1848A0;
  font: 9px 'Courier New', monospace;
  letter-spacing: 2px;
  text-transform: uppercase;
}
.fx-type-btn.active {
  background: #103060;
  border-color: #1848A0;
  color: #C4C4FF;
  box-shadow: 0 0 8px rgba(24,72,160,0.5), inset 0 0 4px rgba(24,72,160,0.3);
}
```

---

## Feedback Path Toggle

```
[OFF] ●────────  →  DIRECT (red thumb, #B81212)
[ON]  ────────●  →  THROUGH FX (blue thumb, #1848A0)
```

Pill shape: 44×22px, border-radius 11px. Thumb: 16×16px circle, transition 200ms.

---

## Section Dividers

Vertical gold gradient lines:
```css
background: linear-gradient(to bottom,
  transparent,
  #C4960A55 15%,
  #C4960A99 50%,
  #C4960A55 85%,
  transparent
);
width: 1px; height: 480px; top: 10px;
```

---

## Typography

| Use | Size | Spacing | Color | Style |
|:----|:-----|:--------|:------|:------|
| Plugin title | 16px | 6px | #C4960A | bold uppercase |
| Section labels | 9px | 3px | #C4960A at 70% | uppercase |
| Control labels | 8px | 2px | #C4960A | uppercase |
| Value displays | 7px | 1px | #C4960A at 55% | normal |
| MX roman numeral | 10px | 1px | #C4960A | normal |
| Toggle sub-label | 6px | 2px | #C4960A at 55% | uppercase |
| Latin art text | 9–11px | normal | #C4960A at 35–40% | italic serif |

Font stack: `'Courier New', Courier, monospace` — for the engraved hardware-label feel.

---

## Background Manuscript Artwork (SVG, opacity 0.18)

Elements:
1. **Stars** — 5/6-pointed polygon ornaments, scattered (6–8 instances)
2. **Constellation** — dot clusters connected by thin lines (left quadrant)
3. **Scrollwork** — `<path>` quadratic bezier curves, bottom edge
4. **Latin text** — "uti mirant stella", "resonat · vibrat", "vox machinae"
5. **Stylized figure** — simple line-art human (circles + lines, far left)
6. **Double border** — thin rect × 2, 2px and 5px inset from edges

All elements: stroke/fill = #C4960A, varying opacity (0.25–0.9 for ornaments, 0.3–0.5 for lines)

---

## VU Meter (Canvas)

- **Size:** 110 × 350 px
- **Segments:** 20 per channel, 2 channels (L + R, side by side)
- **Colors:**
  - 0 to −6 dB (top 15%): red `#FF2020` lit / `#1A0808` dark
  - −6 to −12 dB (mid 25%): gold `#C4960A` lit / `#141008` dark
  - Below −12 dB (bottom 60%): blue `#1848A0` lit / `#080E18` dark
- **Peak hold:** 1px gold horizontal line, holds 60 frames then decays
- **dB labels:** right edge, 7px monospace, gold 33% opacity
- **Decay:** 0.015 per frame (~25ms at 60fps)

---

## Voronoi Pattern (SVG decorative)

- Position: OUT section, lower half
- Size: ~145 × 115 px
- Stroke: #C4A05A, 0.7px, no fill
- Opacity: 0.12 on the SVG element
- ~12 organic polygon cells (hand-crafted coordinates)

---

## Interaction

| Action | Behaviour |
|:-------|:----------|
| Drag up on puck | Increase value |
| Drag down on puck | Decrease value |
| Shift + drag | Fine control (5× slower) |
| Double-click puck | Reset to default |
| Click MX button | Trigger ping, LED flash 120ms |
| Click FX button | Switch algorithm, update P1/P2 labels |
| Click toggle | Switch feedback path, update sub-label |
