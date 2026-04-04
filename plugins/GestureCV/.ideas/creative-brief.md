# GestureCV — Creative Brief

## Hook
*Play your DAW with your hands — no knobs, no clicks, no MIDI controller.*

## Description
GestureCV is a two-component system that turns a standard webcam (or iPhone via Continuity Camera) into a real-time gesture-to-modulation controller for any DAW.

**Component 1 — Gesture Engine (Python, standalone process)**
A headless Python process that:
- Captures camera feed via OpenCV (Continuity Camera shows up as a standard webcam index)
- Runs MediaPipe Hands to extract 21 3D landmarks per hand (wrist + 4 knuckles × 4 fingers + thumb)
- Computes high-level gesture features: hand XYZ position, finger spread, individual finger curl, pinch distance, wrist tilt
- Applies a one-euro filter to each feature stream to suppress jitter without lag
- Emits MIDI CC messages and/or OSC packets at ~60 fps

**Component 2 — GestureCV Plugin (JUCE, VST3/AU)**
A MIDI/CV routing plugin that:
- Receives MIDI CC or OSC from the Python process
- Hosts a visual feedback UI: live gesture feature meters, mapping matrix, per-lane curve editor
- Routes incoming gesture lanes to outgoing MIDI CC numbers (user-configurable)
- Applies per-lane scale, offset, curve (linear / exponential / S-curve), and smoothing
- Outputs standard MIDI CC — works with any DAW's automation/modulation system

## Character
Not a sound generator. A performance instrument. Like a Theremin crossed with a mod matrix. The aesthetic should feel like a motion-capture studio: dark, technical, with glowing data-streams and real-time meters.

## Architecture Decision (from prior session)
The gesture pipeline (OpenCV, MediaPipe, filtering) lives in the Python process. The JUCE plugin is a pure MIDI/CV router — it never touches camera data. They communicate via OSC (UDP localhost) or virtual MIDI port. This separation is deliberate: embedding a camera pipeline inside a plugin DSP process would be architecturally painful and unsupported.

## Target Users
- Experimental producers who want hands-free parameter modulation during performance
- Composers who want to "conduct" effects in real-time
- Accessibility users who cannot use traditional controllers
