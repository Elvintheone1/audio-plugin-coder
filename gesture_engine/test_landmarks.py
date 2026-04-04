"""
Phase A.1 validation script — MediaPipe Tasks API (0.10.18+)
Prints all 8 gesture feature values to terminal in real time.
Press Q to quit.
"""

import cv2
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision as mp_vision
import math
import urllib.request
import os

MODEL_PATH = os.path.join(os.path.dirname(__file__), "hand_landmarker.task")
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"

def ensure_model():
    if not os.path.exists(MODEL_PATH):
        print("Downloading hand_landmarker.task (~8MB)...")
        urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
        print("Done.")

# --- Feature extraction ---

def angle_between(a, b, c):
    ax, ay = a.x - b.x, a.y - b.y
    cx, cy = c.x - b.x, c.y - b.y
    dot = ax * cx + ay * cy
    mag = math.sqrt(ax*ax + ay*ay) * math.sqrt(cx*cx + cy*cy)
    if mag < 1e-6:
        return 0.0
    return math.degrees(math.acos(max(-1.0, min(1.0, dot / mag))))

def dist2d(a, b):
    return math.sqrt((a.x - b.x)**2 + (a.y - b.y)**2)

def finger_curl(lm, mcp_i, pip_i, tip_i):
    angle = angle_between(lm[mcp_i], lm[pip_i], lm[tip_i])
    return 1.0 - min(1.0, angle / 180.0)

def extract_features(lm):
    wrist      = lm[0]
    thumb_tip  = lm[4]
    index_mcp  = lm[5]
    middle_mcp = lm[9]
    ring_mcp   = lm[13]
    pinky_mcp  = lm[17]

    hand_x = wrist.x
    hand_y = 1.0 - wrist.y

    # hand_size in normalized coords: larger = closer to camera
    # typical range: ~0.15 (far) to ~0.55 (very close)
    hand_size = max(dist2d(wrist, middle_mcp), 1e-6)
    hand_z = max(0.0, min(1.0, (hand_size - 0.08) / 0.20))

    spread_angles = [
        angle_between(index_mcp, wrist, middle_mcp),
        angle_between(middle_mcp, wrist, ring_mcp),
        angle_between(ring_mcp, wrist, pinky_mcp),
    ]
    finger_spread = min(1.0, sum(spread_angles) / (3 * 30.0))
    pinch_raw = dist2d(thumb_tip, lm[8]) / hand_size
    pinch = max(0.0, min(1.0, 1.0 - pinch_raw * 2.5))

    dx = middle_mcp.x - wrist.x
    dy = middle_mcp.y - wrist.y
    tilt_deg = math.degrees(math.atan2(dx, -dy))
    wrist_tilt = max(0.0, min(1.0, (tilt_deg + 90.0) / 180.0))

    curl_index = finger_curl(lm, 5, 6, 8)
    curl_ring  = finger_curl(lm, 13, 14, 16)

    return {
        "hand_x":     round(hand_x, 3),
        "hand_y":     round(hand_y, 3),
        "hand_z":     round(hand_z, 3),
        "spread":     round(finger_spread, 3),
        "pinch":      round(pinch, 3),
        "wrist_tilt": round(wrist_tilt, 3),
        "curl_index": round(curl_index, 3),
        "curl_ring":  round(curl_ring, 3),
    }

HAND_CONNECTIONS = [
    (0,1),(1,2),(2,3),(3,4),
    (0,5),(5,6),(6,7),(7,8),
    (0,9),(9,10),(10,11),(11,12),
    (0,13),(13,14),(14,15),(15,16),
    (0,17),(17,18),(18,19),(19,20),
    (5,9),(9,13),(13,17),
]

def draw_landmarks(frame, lm_list):
    """Draw skeleton manually from landmark list."""
    h, w = frame.shape[:2]
    connections = HAND_CONNECTIONS
    pts = [(int(lm.x * w), int(lm.y * h)) for lm in lm_list]
    for a, b in connections:
        cv2.line(frame, pts[a], pts[b], (0, 255, 120), 2)
    for p in pts:
        cv2.circle(frame, p, 4, (255, 255, 255), -1)

# --- Main ---

def main():
    ensure_model()

    base_options = mp_python.BaseOptions(model_asset_path=MODEL_PATH)
    options = mp_vision.HandLandmarkerOptions(
        base_options=base_options,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.5,
        min_tracking_confidence=0.5,
    )
    detector = mp_vision.HandLandmarker.create_from_options(options)

    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    print("Running — hold your hand in front of the camera. Press Q to quit.")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
        result = detector.detect(mp_image)

        if result.hand_landmarks:
            lm = result.hand_landmarks[0]
            features = extract_features(lm)
            draw_landmarks(frame, lm)

            line = "  ".join(f"{k}={v:.3f}" for k, v in features.items())
            print(f"\r{line}", end="", flush=True)

            y = 30
            for k, v in features.items():
                cv2.putText(frame, f"{k}: {v:.3f}", (10, y),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 120), 1)
                y += 22
        else:
            print("\r[no hand detected]                              ", end="", flush=True)

        cv2.imshow("GestureCV — landmark test (Q to quit)", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
    detector.close()
    print()

if __name__ == "__main__":
    main()
