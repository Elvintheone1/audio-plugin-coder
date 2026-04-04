"""
GestureCV Gesture Engine
Captures hand landmarks, filters them, and emits OSC /gesture/lanes to the plugin.

Usage:
    python3 gesture_engine/main.py [--config gesture_engine/config.yaml] [--verbose]
"""

import argparse
import os
import sys
import time

import cv2
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision as mp_vision

from one_euro import OneEuroFilter
from feature_extractor import extract, LANE_NAMES
from osc_sender import OscSender

MODEL_PATH = os.path.join(os.path.dirname(__file__), "hand_landmarker.task")

HAND_CONNECTIONS = [
    (0,1),(1,2),(2,3),(3,4),
    (0,5),(5,6),(6,7),(7,8),
    (0,9),(9,10),(10,11),(11,12),
    (0,13),(13,14),(14,15),(15,16),
    (0,17),(17,18),(18,19),(19,20),
    (5,9),(9,13),(13,17),
]


def load_config(path):
    import yaml
    with open(path) as f:
        return yaml.safe_load(f)


def draw_skeleton(frame, lm_list):
    h, w = frame.shape[:2]
    pts = [(int(lm.x * w), int(lm.y * h)) for lm in lm_list]
    for a, b in HAND_CONNECTIONS:
        cv2.line(frame, pts[a], pts[b], (0, 255, 120), 2)
    for p in pts:
        cv2.circle(frame, p, 4, (255, 255, 255), -1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=os.path.join(os.path.dirname(__file__), "config.yaml"))
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--no-window", action="store_true", help="Headless mode, no OpenCV window")
    args = parser.parse_args()

    cfg = load_config(args.config)

    osc = OscSender(host=cfg["output_osc_host"], port=cfg["output_osc_port"])
    print(f"OSC → {cfg['output_osc_host']}:{cfg['output_osc_port']}")

    # One-euro filter per lane
    filters = [
        OneEuroFilter(mincutoff=cfg["one_euro_mincutoff"], beta=cfg["one_euro_beta"])
        for _ in LANE_NAMES
    ]

    base_options = mp_python.BaseOptions(model_asset_path=MODEL_PATH)
    options = mp_vision.HandLandmarkerOptions(
        base_options=base_options,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.5,
        min_tracking_confidence=0.5,
    )
    detector = mp_vision.HandLandmarker.create_from_options(options)

    cap = cv2.VideoCapture(cfg["camera_index"])
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    target_dt = 1.0 / cfg["target_fps"]
    last_time = time.monotonic()
    smoothed = [0.0] * 8

    print("Running. Press Q to quit.")

    while cap.isOpened():
        loop_start = time.monotonic()

        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
        result = detector.detect(mp_image)

        now = time.monotonic()
        dt = now - last_time
        last_time = now

        if result.hand_landmarks:
            lm = result.hand_landmarks[0]
            raw = extract(lm)
            smoothed = [filters[i](raw[i], dt) for i in range(8)]
            osc.send_lanes(smoothed)

            if not args.no_window:
                draw_skeleton(frame, lm)
                y = 30
                for i, name in enumerate(LANE_NAMES):
                    cv2.putText(frame, f"{name}: {smoothed[i]:.3f}", (10, y),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 120), 1)
                    y += 22

            if args.verbose:
                line = "  ".join(f"{n}={v:.3f}" for n, v in zip(LANE_NAMES, smoothed))
                print(f"\r{line}", end="", flush=True)
        else:
            if args.verbose:
                print("\r[no hand]                                           ", end="", flush=True)

        if not args.no_window:
            cv2.imshow("GestureCV (Q to quit)", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        else:
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        # Throttle to target FPS
        elapsed = time.monotonic() - loop_start
        sleep_t = target_dt - elapsed
        if sleep_t > 0:
            time.sleep(sleep_t)

    cap.release()
    cv2.destroyAllWindows()
    detector.close()
    print("\nStopped.")


if __name__ == "__main__":
    # Run from repo root: python3 gesture_engine/main.py
    sys.path.insert(0, os.path.dirname(__file__))
    main()
