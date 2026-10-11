"""Generate reference vectors from the original Python SATER Map pure functions."""
import ast, json, random, sys
from math import cos, degrees, radians, sin, tan, atan2, sqrt, pi, asin, floor, log
from dataclasses import dataclass, field
from typing import Optional, List, Tuple, Dict

src_path = sys.argv[1]
out_path = sys.argv[2]
src = open(src_path, encoding="utf-8").read()
tree = ast.parse(src)
wanted = {"dms_to_dd", "dd_to_dms", "dd_to_dms_str", "lat_lon_to_utm", "lat_lon_to_mgrs",
          "calc_endpoint_haversine", "line_intersection", "calculate_all_intersections",
          "distance_km", "smallest_enclosing_circle", "calculate_uncertainty_circle",
          "lat_lon_to_tile", "get_tiles_for_bounds"}
ns = dict(cos=cos, degrees=degrees, radians=radians, sin=sin, tan=tan, atan2=atan2, sqrt=sqrt,
          pi=pi, asin=asin, floor=floor, log=log, Optional=Optional, List=List, Tuple=Tuple, Dict=Dict)

@dataclass
class Station:
    callsign: str
    lat: float
    lon: float
    azimuth: float
    uncertainty: float = 5.0
    signal: str = "S5"
ns["Station"] = Station
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in wanted:
        exec(compile(ast.Module([node], []), src_path, "exec"), ns)

rnd = random.Random(20261010)
out = {"dms": [], "utm": [], "endpoint": [], "scenarios": [], "tiles": []}
pts = [(43.7, 7.25), (43.2965, 5.3698), (48.8566, 2.3522), (45.0, 6.0), (44.1, 3.0), (-33.9, 18.4),
       (60.0, 5.0), (75.0, 15.0), (0.5, -0.5), (51.5, -0.12), (43.66, 7.21), (43.9, 6.9)]
for _ in range(40):
    pts.append((round(rnd.uniform(-79, 83), 6), round(rnd.uniform(-179, 179), 6)))
for lat, lon in pts:
    out["dms"].append({"lat": lat, "lon": lon,
                       "lat_dms": list(ns["dd_to_dms"](lat, "lat")), "lon_dms": list(ns["dd_to_dms"](lon, "lon")),
                       "lat_str": ns["dd_to_dms_str"](lat, "lat"), "lon_str": ns["dd_to_dms_str"](lon, "lon")})
    z, l, e, n = ns["lat_lon_to_utm"](lat, lon)
    out["utm"].append({"lat": lat, "lon": lon, "zone": z, "letter": l, "e": e, "n": n,
                       "mgrs": ns["lat_lon_to_mgrs"](lat, lon)})
for (d, m, s, h) in [(43, 42, 0.0, "N"), (7, 15, 30.5, "E"), (5, 22, 11.3, "W"), (33, 54, 0.0, "S"), (0, 0, 0.1, "N")]:
    out.setdefault("dms_to_dd", []).append({"d": d, "m": m, "s": s, "h": h, "dd": ns["dms_to_dd"](d, m, s, h)})
for lat, lon in pts[:12]:
    for az in (0, 37, 90, 145.5, 200, 271, 359):
        for dist in (10, 100, 500):
            la, lo = ns["calc_endpoint_haversine"](lat, lon, az, dist)
            out["endpoint"].append({"lat": lat, "lon": lon, "az": az, "dist": dist, "elat": la, "elon": lo})
# scenarios
for k in range(60):
    nst = rnd.randint(2, 6)
    clat, clon = rnd.uniform(42.5, 49.5), rnd.uniform(-1.0, 8.0)
    stations = []
    for i in range(nst):
        slat = clat + rnd.uniform(-0.4, 0.4)
        slon = clon + rnd.uniform(-0.5, 0.5)
        # aim roughly at a target near the centre
        tlat, tlon = clat + rnd.uniform(-0.05, 0.05), clon + rnd.uniform(-0.05, 0.05)
        az = degrees(atan2((tlon - slon) * cos(radians(slat)), tlat - slat)) % 360
        az = round(az + rnd.uniform(-6, 6)) % 360
        stations.append(Station(f"ST{i}", round(slat, 6), round(slon, 6), float(az), 5.0))
    length = rnd.choice([10, 50, 100, 250, 500])
    inter = ns["calculate_all_intersections"](stations, length)
    circ = ns["calculate_uncertainty_circle"](inter)
    out["scenarios"].append({"length": length,
                             "stations": [{"lat": s.lat, "lon": s.lon, "az": s.azimuth} for s in stations],
                             "intersections": inter, "circle": list(circ) if circ else None,
                             "mgrs": ns["lat_lon_to_mgrs"](circ[0], circ[1]) if circ else None})
for (a, b, c, d, z1, z2) in [(43.5, 7.0, 43.9, 7.5, 10, 12), (46.0, 2.0, 47.0, 3.5, 6, 9)]:
    t = ns["get_tiles_for_bounds"](a, b, c, d, z1, z2)
    out["tiles"].append({"minLat": a, "minLon": b, "maxLat": c, "maxLon": d, "z1": z1, "z2": z2,
                         "count": len(t), "first": list(t[0]), "last": list(t[-1])})
json.dump(out, open(out_path, "w"), indent=1)
print("scenarios with circle:", sum(1 for s in out["scenarios"] if s["circle"]), "of", len(out["scenarios"]))
