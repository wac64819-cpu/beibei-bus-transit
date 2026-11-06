# -*- coding: utf-8 -*-
"""向 master_stops_beibei.csv 追加蔡家以南通往主城方向的站点"""
import json, time, math, os, sys, urllib.parse, urllib.request
sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")

# 中心放在蔡家以南，半径放宽到 10km 覆盖到主城北部
CENTER_LNG, CENTER_LAT = 106.50, 29.69
MAX_KM = 10.0

import os
STOPS = ["礼嘉", "礼嘉立交", "鸳鸯立交", "大竹林", "园博园", "龙头寺", "蔡家南", "礼嘉天街"]

def query(kw):
    params = {"keywords":"重庆 "+kw, "city":"500000", "output":"json", "offset":"20", "key":KEY}
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url, headers={"User-Agent":"Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as r:
            d = json.loads(r.read().decode("utf-8"))
        if d.get("status") == "1": return d.get("pois",[]) or []
    except Exception as e: print(f"  ERR {e}")
    return []

def hav(a,b,c,d):
    R=6371.0; p1,p2=math.radians(b),math.radians(d)
    dl=math.radians(d-b); dlng=math.radians(c-a)
    aa=math.sin(dl/2)**2+math.cos(p1)*math.cos(p2)*math.sin(dlng/2)**2
    return 2*R*math.asin(math.sqrt(aa))

def best(pois, name):
    bp=None; bs=-1e9
    for p in pois:
        loc=(p.get("location") or "").split(",")
        if len(loc)!=2: continue
        try: lng,lat=float(loc[0]),float(loc[1])
        except: continue
        d=hav(lng,lat,CENTER_LNG,CENTER_LAT)
        if d>MAX_KM: continue
        score=10.0-d
        clean=p.get("name","")
        if name in clean or clean in name: score+=20
        adname=p.get("adname","")
        # 偏好两江新区/北碚区
        if "两江" in adname or "北碚" in adname or "渝北" in adname: score+=5
        if score>bs: bs=score; bp=(p,lng,lat,d)
    return bp

with open(MASTER,"r",encoding="utf-8") as f: lines=f.read().splitlines()
existing={ln.split(",")[0] for ln in lines[1:] if ln.strip()}
new=[]
for n in STOPS:
    if n in existing: print(f"  SKIP {n}"); continue
    b=best(query(n), n)
    if b:
        p,lng,lat,d=b
        print(f"  ✓ {n:<8} -> ({lng:.6f},{lat:.6f})  dist={d:.2f}km  {p['name']} [{p.get('adname','')}]")
        new.append(f'{n},{lng:.6f},{lat:.6f},0,"",NO')
    else: print(f"  ✗ {n:<8} 找不到")
    time.sleep(0.15)
if new:
    with open(MASTER,"a",encoding="utf-8") as f:
        for r in new: f.write(r+"\n")
    print(f"\n追加 {len(new)} 站")
