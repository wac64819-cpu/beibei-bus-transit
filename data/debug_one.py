# -*- coding: utf-8 -*-
"""单点调试：查一个站点看 API 返回什么"""
import json, urllib.parse, urllib.request, sys

# 强制 stdout utf-8 输出
import os
sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
def query(kw, types=None):
    params = {"keywords": kw, "city": "500000", "output": "json", "offset": "15", "key": KEY}
    if types: params["types"] = types
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    print("URL:", url)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    print("status:", data.get("status"), "count:", data.get("count"), "info:", data.get("info"))
    pois = data.get("pois") or []
    print(f"返回 {len(pois)} 个 POI:")
    for p in pois[:10]:
        print(f"  - name={p.get('name')!r} adcode={p.get('adcode')} adname={p.get('adname')} loc={p.get('location')}")

print("===== 测试 1: 北碚区西南大学 + types=150700 =====")
query("北碚区西南大学", "150700")

print()
print("===== 测试 2: 北碚区西南大学 不限类型 =====")
query("北碚区西南大学")

print()
print("===== 测试 3: 北碚区 北碚火车站 + types=150700 =====")
query("北碚区北碚火车站", "150700")
