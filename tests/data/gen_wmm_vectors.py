#!/usr/bin/env python3
"""Reference declinations for tst_core, from pygeomag (an independent port of
NOAA's World Magnetic Model code) with the WMM2025 coefficients.

    pip install pygeomag==1.1.0
    python3 tests/data/gen_wmm_vectors.py > tests/data/wmm_vectors.json
"""
import json
import random

from pygeomag import GeoMag

gm = GeoMag(coefficients_file="wmm/WMM_2025.COF")
points = [
    (43.7102, 7.2620), (43.2965, 5.3698), (48.8566, 2.3522), (45.7640, 4.8357),
    (44.1, 6.9), (42.7, 9.4), (50.6, 3.06), (47.2, -1.55), (43.6, 1.44),
    (0.0, 0.0), (51.5, -0.12), (40.7, -74.0), (-33.9, 151.2), (64.1, -21.9),
    (-54.8, -68.3), (35.7, 139.7), (-1.3, 36.8), (78.2, 15.6), (-77.8, 166.7),
]
rng = random.Random(2025)
for _ in range(60):
    points.append((rng.uniform(-80, 80), rng.uniform(-180, 180)))
times = [2025.0, 2026.78, 2029.9]
out = []
for i, (lat, lon) in enumerate(points):
    for t in times:
        alt = 0.0 if i % 3 else 1.5
        r = gm.calculate(glat=lat, glon=lon, alt=alt, time=t)
        out.append({"lat": lat, "lon": lon, "alt_km": alt, "year": t, "dec": r.d, "inc": r.i})
print(json.dumps({"model": gm.model, "vectors": out}, indent=1))
