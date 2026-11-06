# -*- coding: utf-8 -*-
"""向 master_stops_beibei.csv 追加北碚东部站点（中心 29.815, 106.49）"""
import json, time, math, os, sys, urllib.parse, urllib.request

sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")

CENTER_LNG, CENTER_LAT = 106.490, 29.810
MAX_KM = 6.0

import os
EAST_STOPS = ["东阳互通", "东阳镇", "山水庭源", "石坝", "施家梁", "梅街", "东阳街"]

def query(keywords):
    params = {"keywords":"重庆 北碚 "+keywords,"city":"500000","output":"json","offset":"20","key":KEY}
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url, headers={"User-Agent":"Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as r:
            d = json.loads(r.read().decode("utf-8"))
        if d.get("status") == "1": return d.get("pois",[]) or []
    except Exception as e: print(f"  ERROR: {e}")
    return []

def haversine(lng1,lat1,lng2,lat2):
    R=6371.0; p1,p2=math.radians(lat1),math.radians(lat2)
    dl=math.radians(lat2-lat1); dlng=math.radians(lng2-lng1)
    a=math.sin(dl/2)**2+math.cos(p1)*math.cos(p2)*math.sin(dlng/2)**2
    return 2*R*math.asin(math.sqrt(a))

def best(pois, name):
    bp=None; bs=-1e9
    for p in pois:
        loc=(p.get("location") or "").split(",")
        if len(loc)!=2: continue
        try: lng,lat=float(loc[0]),float(loc[1])
        except: continue
        dist=haversine(lng,lat,CENTER_LNG,CENTER_LAT)
        if dist>MAX_KM: continue
        score=10.0-dist
        clean=p.get("name","")
        if name in clean or clean in name: score+=20
        if score>bs: bs=score; bp=(p,lng,lat,dist)
    return bp

with open(MASTER,"r",encoding="utf-8") as f: lines=f.read().splitlines()
existing={ln.split(",")[0] for ln in lines[1:] if ln.strip()}
new=[]
for name in EAST_STOPS:
    if name in existing: print(f"  SKIP {name}"); continue
    pois=query(name); b=best(pois,name)
    if b:
        p,lng,lat,dist=b
        print(f"  ✓ {name:<10} -> ({lng:.6f},{lat:.6f})  dist={dist:.2f}km  {p['name']}")
        new.append(f'{name},{lng:.6f},{lat:.6f},0,"",NO')
    else: print(f"  ✗ {name:<10} 找不到")
    time.sleep(0.15)
if new:
    with open(MASTER,"a",encoding="utf-8") as f:
        for r in new: f.write(r+"\n")
    print(f"\n追加 {len(new)} 站")
