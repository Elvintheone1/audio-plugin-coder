"""
One-Euro Filter — low-latency smoothing for noisy signals.
Reference: Casiez et al. 2012, "1€ Filter: A Simple Speed-based Low-pass Filter for Noisy Input in Interactive Systems"

Usage:
    f = OneEuroFilter(mincutoff=1.0, beta=0.01)
    smoothed = f(raw_value, dt)   # dt in seconds
"""

import math


class _LowPassFilter:
    def __init__(self):
        self._y = None
        self._a = None

    def set_alpha(self, alpha):
        self._a = max(0.0, min(1.0, alpha))

    def filter(self, value):
        if self._y is None:
            self._y = value
        else:
            self._y = self._a * value + (1.0 - self._a) * self._y
        return self._y

    @property
    def last(self):
        return self._y


class OneEuroFilter:
    def __init__(self, mincutoff=1.0, beta=0.01, dcutoff=1.0):
        """
        mincutoff: minimum cutoff frequency (Hz). Lower = smoother at rest, more lag.
        beta:      speed coefficient. Higher = less lag on fast movements.
        dcutoff:   cutoff for the derivative filter (usually leave at 1.0).
        """
        self.mincutoff = mincutoff
        self.beta = beta
        self.dcutoff = dcutoff
        self._x = _LowPassFilter()
        self._dx = _LowPassFilter()
        self._last_time = None

    def _alpha(self, cutoff, dt):
        tau = 1.0 / (2.0 * math.pi * cutoff)
        return 1.0 / (1.0 + tau / dt)

    def __call__(self, value, dt=None):
        """
        value: raw input
        dt:    time delta in seconds since last call (default: assume 1/60)
        """
        if dt is None or dt <= 0:
            dt = 1.0 / 60.0

        prev = self._x.last if self._x.last is not None else value
        dx = (value - prev) / dt

        self._dx.set_alpha(self._alpha(self.dcutoff, dt))
        edx = self._dx.filter(dx)

        cutoff = self.mincutoff + self.beta * abs(edx)
        self._x.set_alpha(self._alpha(cutoff, dt))
        return self._x.filter(value)
