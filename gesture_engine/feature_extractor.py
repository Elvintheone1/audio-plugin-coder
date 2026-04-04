"""
Gesture feature extractor — computes 8 normalized floats from MediaPipe hand landmarks.
All output values are in [0.0, 1.0].
"""

import math


LANE_NAMES = ["hand_x", "hand_y", "hand_z", "spread", "pinch", "wrist_tilt", "curl_index", "curl_ring"]


def _angle_between(a, b, c):
    ax, ay = a.x - b.x, a.y - b.y
    cx, cy = c.x - b.x, c.y - b.y
    dot = ax * cx + ay * cy
    mag = math.sqrt(ax*ax + ay*ay) * math.sqrt(cx*cx + cy*cy)
    if mag < 1e-6:
        return 0.0
    return math.degrees(math.acos(max(-1.0, min(1.0, dot / mag))))


def _dist2d(a, b):
    return math.sqrt((a.x - b.x)**2 + (a.y - b.y)**2)


def _finger_curl(lm, mcp_i, pip_i, tip_i):
    angle = _angle_between(lm[mcp_i], lm[pip_i], lm[tip_i])
    return 1.0 - min(1.0, angle / 180.0)


def extract(lm) -> list[float]:
    """
    lm: list of 21 NormalizedLandmark from MediaPipe HandLandmarker result.
    Returns list of 8 floats matching LANE_NAMES order.
    """
    wrist      = lm[0]
    thumb_tip  = lm[4]
    index_mcp  = lm[5]
    index_tip  = lm[8]
    middle_mcp = lm[9]
    ring_mcp   = lm[13]
    pinky_mcp  = lm[17]

    hand_x = wrist.x
    hand_y = 1.0 - wrist.y

    hand_size = max(_dist2d(wrist, middle_mcp), 1e-6)
    hand_z = max(0.0, min(1.0, (hand_size - 0.08) / 0.20))

    spread_angles = [
        _angle_between(index_mcp, wrist, middle_mcp),
        _angle_between(middle_mcp, wrist, ring_mcp),
        _angle_between(ring_mcp, wrist, pinky_mcp),
    ]
    finger_spread = min(1.0, sum(spread_angles) / (3 * 30.0))

    pinch_raw = _dist2d(thumb_tip, index_tip) / hand_size
    pinch = max(0.0, min(1.0, 1.0 - pinch_raw * 2.5))

    dx = middle_mcp.x - wrist.x
    dy = middle_mcp.y - wrist.y
    tilt_deg = math.degrees(math.atan2(dx, -dy))
    wrist_tilt = max(0.0, min(1.0, (tilt_deg + 90.0) / 180.0))

    curl_index = _finger_curl(lm, 5, 6, 8)
    curl_ring  = _finger_curl(lm, 13, 14, 16)

    return [hand_x, hand_y, hand_z, finger_spread, pinch, wrist_tilt, curl_index, curl_ring]
