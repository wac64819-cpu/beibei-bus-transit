# -*- coding: utf-8 -*-
"""向 master_stops_beibei.csv 追加蔡家片区站点（中心 29.74, 106.49）"""
import csv, json, time, math, os, sys
import urllib.parse, urllib.request

sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")

# 蔡家区域中心
import os
CJ_LNG, CJ_LAT = 106.490, 29.740
MAX_KM = 5.0  # 5 km 范围内

# 蔡家片区候选站点（公交站名）
CAIJIA_STOPS = [
    "蔡家岗", "凤栖路", "童家溪", "蔡家公园",
    "嘉树路", "永合", "白云苑", "蔡家立交",
]

def query(keywords):
    params = {
        "keywords": "重庆 北碚 " + keywords,
        "city": "500000",
        "output": "json", "offset": "20",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as resp:
            d = json.loads(resp.read().decode("utf-8"))
        if d.get("status") == "1":
            return d.get("pois", []) or []
    except Exception as e:
        print(f"  ERROR: {e}")
    return []

def haversine(lng1, lat1, lng2, lat2):
    R = 6371.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lat2-lat1); dlng = math.radians(lng2-lng1)
    a = math.sin(dl/2)**2 + math.cos(p1)*math.cos(p2)*math.sin(dlng/2)**2
    return 2*R*math.asin(math.sqrt(a))

def best_near_caijia(pois, name):
    best = None; best_score = -1e9
    for p in pois:
        loc = (p.get("location") or "").split(",")
        if len(loc) != 2: continue
        try:
            lng, lat = float(loc[0]), float(loc[1])
        except: continue
        dist = haversine(lng, lat, CJ_LNG, CJ_LAT)
        if dist > MAX_KM: continue
        score = 10.0 - dist
        clean = p.get("name", "")
        if name in clean or clean in name: score += 20
        if "公交" in (p.get("typecode","") or "") or "公交" in (p.get("type","") or ""): score += 5
        if score > best_score:
            best_score = score; best = (p, lng, lat, dist)
    return best

# 读取 master
with open(MASTER, "r", encoding="utf-8") as f:
    lines = f.read().splitlines()
header = lines[0]
existing_names = set()
for ln in lines[1:]:
    if ln.strip():
        existing_names.add(ln.split(",")[0])

new_rows = []
for name in CAIJIA_STOPS:
    if name in existing_names:
        print(f"  SKIP {name} (已存在)"); continue
    pois = query(name)
    best = best_near_caijia(pois, name)
    if best:
        p, lng, lat, dist = best
        print(f"  ✓ {name:<10} -> ({lng:.6f},{lat:.6f})  dist={dist:.2f}km  matched={p['name']}")
        # name,lng,lat,passing_lines_count,passing_lines,is_in_existing
        new_rows.append(f'{name},{lng:.6f},{lat:.6f},0,"",NO')
    else:
        print(f"  ✗ {name:<10} 找不到")
    time.sleep(0.15)

if new_rows:
    with open(MASTER, "a", encoding="utf-8") as f:
        for r in new_rows:
            f.write(r + "\n")
    print(f"\n追加 {len(new_rows)} 站到 master_stops_beibei.csv")
else:
    print("\n没有新增")
