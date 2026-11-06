# -*- coding: utf-8 -*-
import json, urllib.parse, urllib.request, sys
sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
# 6 号线在西南大学到蔡家之间的候选站
STATIONS = ["大磨滩", "悦来", "悦来商务中心", "礼嘉新城", "嘉州路", "翠云", "曹家湾"]

import os
def query(name):
    params = {"keywords": name, "city": "500000", "types": "150500",
              "output": "json", "offset": "3", "key": KEY}
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))

for name in STATIONS:
    data = query(name)
    pois = data.get("pois") or []
    if pois:
        p = pois[0]
        n = p.get("name", "")
        if "(地铁站)" in n:
            print(f"  ⭐ {n}  @ {p.get('location')}  [{p.get('adname')}]")
        else:
            print(f"     {n}  @ {p.get('location')}  [{p.get('adname')}]")
    else:
        print(f"  无 {name}")
