# -*- coding: utf-8 -*-
"""第二批蔡家片区站点（更细密）"""
import json, time, math, os, sys, urllib.parse, urllib.request
sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")
CJ_LNG, CJ_LAT = 106.490, 29.740
MAX_KM = 6.0

import os
CANDIDATES = [
    "蔡家中学", "蔡家加油站", "蔡家智慧新城", "联东U谷",
    "横山小学", "蔡家新城", "嘉树湾", "云汉大道",
    "大悦城", "嘉陵江畔", "蔡家小学", "蔡家公交首末站",
    "凤居路", "万沛路", "兰花路"
]

def query(kw):
    p = {"keywords":"重庆 "+kw, "city":"500000", "output":"json", "offset":"20", "key":KEY}
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(p)
    try:
        req = urllib.request.Request(url, headers={"User-Agent":"Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as r:
            d = json.loads(r.read().decode("utf-8"))
        if d.get("status") == "1": return d.get("pois", []) or []
    except Exception as e:
        print(f"  ERROR: {e}")
    return []

def haversine(lng1, lat1, lng2, lat2):
    R = 6371.0; p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lat2-lat1); dlng = math.radians(lng2-lng1)
    a = math.sin(dl/2)**2 + math.cos(p1)*math.cos(p2)*math.sin(dlng/2)**2
    return 2*R*math.asin(math.sqrt(a))

def best(pois, name):
    bp = None; bs = -1e9
    for p in pois:
        loc = (p.get("location") or "").split(",")
        if len(loc) != 2: continue
        try: lng, lat = float(loc[0]), float(loc[1])
        except: continue
        d = haversine(lng, lat, CJ_LNG, CJ_LAT)
        if d > MAX_KM: continue
        s = 10.0 - d
        clean = p.get("name", "")
        if name in clean or clean in name: s += 20
        if s > bs: bs = s; bp = (p, lng, lat, d)
    return bp

with open(MASTER, "r", encoding="utf-8") as f:
    lines = f.read().splitlines()
existing_names = {ln.split(",")[0] for ln in lines[1:] if ln.strip()}
existing_coords = []
for ln in lines[1:]:
    if not ln.strip(): continue
    parts = ln.split(",")
    try: existing_coords.append((float(parts[1]), float(parts[2]), parts[0]))
    except: pass

new = []
for name in CANDIDATES:
    if name in existing_names:
        print(f"  SKIP {name} (已存在)"); continue
    pois = query(name)
    b = best(pois, name)
    if not b:
        print(f"  ✗ {name:<14} 找不到"); time.sleep(0.15); continue
    p, lng, lat, dist = b
    # 检查是否与已有站点过近（< 100m）→ 跳过避免重复
    too_close = False
    for elng, elat, ename in existing_coords:
        if haversine(lng, lat, elng, elat) * 1000 < 100:
            print(f"  ✗ {name:<14} 与 {ename} 太近 (<100m)"); too_close = True; break
    if too_close:
        time.sleep(0.15); continue
    print(f"  ✓ {name:<14} -> ({lng:.6f},{lat:.6f})  dist={dist:.2f}km  {p['name']}")
    new.append((name, f'{name},{lng:.6f},{lat:.6f},0,"",NO'))
    existing_coords.append((lng, lat, name))
    time.sleep(0.15)

if new:
    with open(MASTER, "a", encoding="utf-8") as f:
        for _, row in new: f.write(row + "\n")
    print(f"\n追加 {len(new)} 站到 master")
else:
    print("\n没有新增")
