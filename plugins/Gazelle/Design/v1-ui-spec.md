# GAZELLE — UI Specification v1

## Overview
- **Window:** 1000 × 500 px
- **Framework:** WebView (HTML/CSS/SVG/Canvas)
- **Theme:** Antilope-inspired — dark charcoal panel, gold manuscript artwork, Neve puck controls
- **Sections (left→right):** ENV | FILTER | DIST | FX | OUT

---

## Section Boundaries

| Section | x Start | x End | Width | Divider |
|:--------|:--------|:------|:------|:--------|
| ENV | 0 | 160 | 160 | gold line at x=160 |
| FILTER | 161 | 470 | 309 | gold line at x=470 |
| DIST | 471 | 670 | 199 | gold line at x=670 |
| FX | 671 | 855 | 184 | gold line at x=855 |
| OUT | 856 | 1000 | 144 | — |

---

## Control Layout (cx = knob center x, cy = knob center y)

### ENV Section
| Parameter | cx | cy | Size | Color | Control |
|:----------|:---|:---|:-----|:------|:--------|
| attack | 80 | 82 | 52px | #3A3A3A | Neve puck |
| decay | 80 | 212 | 52px | #3A3A3A | Neve puck |
| env_shape | 80 | 358 | 40px | #3A3A3A | Neve puck |

### FILTER Section
| Parameter | cx | cy | Size | Color | Control |
|:----------|:---|:---|:-----|:------|:--------|
| cutoff | 224 | 82 | 62px | #B81212 | Neve puck (large) |
| resonance | 355 | 82 | 62px | #B81212 | Neve puck (large) |
| spread | 224 | 217 | 52px | #B81212 | Neve puck |
| filter_mode | 355 | 217 | 52px | #B81212 | Neve puck |
| filter1_level | 215 | 358 | 40px | #B81212 | Neve puck |
| filter2_level | 295 | 358 | 40px | #B81212 | Neve puck |
| trigger I | 440 | 118 | 50×50 | cream | Cherry MX (red LED) |
| trigger II | 440 | 285 | 50×50 | cream | Cherry MX (blue LED) |

### DIST Section
| Parameter | cx | cy | Size | Color | Control |
|:----------|:---|:---|:-----|:------|:--------|
| drive | 510 | 82 | 52px | #1C1C1C | Neve puck |
| dist_amount | 620 | 82 | 52px | #1C1C1C | Neve puck |
| dist_feedback | 510 | 217 | 52px | #1C1C1C | Neve puck |
| eq_tilt | 620 | 217 | 52px | #1C1C1C | Neve puck |
| feedback_path | 565 | 358 | 44×22 | toggle | Red/Blue toggle switch |

### FX Section
| Parameter | cx | cy | Size | Color | Control |
|:----------|:---|:---|:-----|:------|:--------|
| fx_type | 760 | 72 | 3×button | blue | TAPE / RING / PLATE |
| fx_p1 | 718 | 200 | 52px | #1848A0 | Neve puck |
| fx_p2 | 816 | 200 | 52px | #1848A0 | Neve puck |
| fx_wet | 763 | 345 | 40px | #1848A0 | Neve puck |

### OUT Section
| Element | Position | Notes |
|:--------|:---------|:------|
| VU meter canvas | x=868, y=60, 110×350 | L+R bars, peak hold, dB labels |
| Voronoi pattern | x=856, y=380 | SVG decorative, tan/copper |
| Maker's mark | bottom-right | "AJ / GAZELLE" bordered |

---

## Special Controls

### Cherry MX Trigger Buttons
- 50×50px, cream/beige body, raised with bottom shadow (physical key feel)
- Press → `translateY(4px)` + reduced shadow
- LED dot (8px) bottom-right of cap: red (filter I), blue (filter II)
- LED glows on press, fades after 120ms release
- Sends `triggerPing` event to JUCE backend

### FX Type Selector
- Three inline buttons: TAPE | RING | PLATE
- Active button: blue glow, bright border, white text
- Inactive: dark blue background, muted text
- P1/P2 label updates dynamically based on active type:
  - TAPE: Time / Fdbk
  - RING: Freq / Depth
  - PLATE: Pre-D / Decay

### Feedback Path Toggle
- Pill-shaped toggle, 44×22px
- OFF (left, red thumb): DIRECT — dist output feeds back to dist input
- ON (right, blue thumb): THROUGH FX — FX output feeds back to dist input
- Text label below updates: "DIRECT" / "THROUGH FX"

---

## Background Artwork
- Gold SVG manuscript illustrations at opacity 0.18
- Elements: star ornaments, constellation lines, scrollwork curves, Latin text fragments, stylized figure
- Double border rectangle (thin, gold, low opacity)
- Artwork density: heavy left/center, lighter right

---

## Typography
- Plugin title: "✦ GAZELLE" — 16px, letter-spacing 6px, gold, top-left
- Section labels: 9px, letter-spacing 3px, gold 70% opacity, positioned in each section
- Control labels: 8px, letter-spacing 2px, gold, below each puck
- Value displays: 7px, gold 55% opacity, below label
- Roman numerals on MX labels: "i" / "ii"
