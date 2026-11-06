# -*- coding: utf-8 -*-
"""查询北碚 6 号线 3 个地铁站坐标"""
import json, urllib.parse, urllib.request, sys
sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
# 北碚区 6 号线主要站点
STATIONS = ["北碚", "状元碑", "天生"]

def query(name):
    # 类型 150500 = 地铁站（高德 POI 分类码）
import os
    params = {
        "keywords": "北碚" + name,
        "city": "500000",
        "types": "150500",
        "output": "json",
        "offset": "10",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))

for name in STATIONS:
    print(f"=== 查询: {name} ===")
    data = query(name)
    pois = data.get("pois") or []
    # 偏好包含"地铁站"或"轨道"或"6号线"的
    for p in pois[:8]:
        n = p.get("name", "")
        loc = p.get("location", "")
        ad = p.get("adname", "")
        print(f"  - {n}  @ {loc}  [{ad}]")
    print()
