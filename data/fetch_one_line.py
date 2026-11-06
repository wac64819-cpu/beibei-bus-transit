# -*- coding: utf-8 -*-
"""用高德 API 抓取一条北碚区公交线路（含完整站点序列和坐标）"""
import json
import urllib.parse
import urllib.request
import sys

sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
# 试抓 520 路（北碚区主干线）
import os
LINE_NAME = "520路"
CITY = "500000"  # 重庆市


def http_get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


def search_line_via_place_api(line_name):
    """
    方案 A: 用 POI 文本搜索 + type=150700（公交站）
    搜"<线路名>"会返回沿途所有站点，再按 lineroute 字段重组
    """
    params = {
        "keywords": line_name,
        "city": CITY,
        "types": "150700",
        "output": "json",
        "offset": "25",
        "extensions": "all",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    print(f"[请求] {url}")
    data = http_get(url)
    return data


def search_line_official(line_name):
    """
    方案 B: 用专用公交线路 API（如果可用）
    """
    params = {
        "keywords": line_name,
        "city": CITY,
        "output": "json",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/bus/linename?" + urllib.parse.urlencode(params)
    print(f"[请求] {url}")
    try:
        data = http_get(url)
        return data
    except Exception as e:
        print(f"[错误] {e}")
        return None


print(f"===== 方案 A: POI 文本搜索抓站点 - 查询 {LINE_NAME} =====")
data_a = search_line_via_place_api(LINE_NAME)
print(f"Status: {data_a.get('status')}, Count: {data_a.get('count')}")
pois = data_a.get("pois") or []
print(f"返回 {len(pois)} 个 POI（仅显示前 5 个）:")
for p in pois[:5]:
    print(f"  名称: {p.get('name')}")
    print(f"    坐标: {p.get('location')}")
    print(f"    行政区: {p.get('adname')}")
    print(f"    经过线路: {(p.get('address') or '')[:120]}")
    print()

print()
print(f"===== 方案 B: 专用公交线路 API - 查询 {LINE_NAME} =====")
data_b = search_line_official(LINE_NAME)
if data_b:
    print(f"原始返回 keys: {list(data_b.keys())[:5]}")
    if "buslines" in data_b:
        lines = data_b["buslines"]
        print(f"找到 {len(lines)} 条线路:")
        for L in lines[:3]:
            print(f"  线路: {L.get('name')}")
            print(f"  起讫: {L.get('start_stop')} -> {L.get('end_stop')}")
            print(f"  类型: {L.get('type')}")
            print(f"  长度: {L.get('distance')}")
            print(f"  票价: {L.get('basic_price')} ~ {L.get('total_price')}")
            stops = L.get("busstops") or []
            print(f"  共 {len(stops)} 个站点:")
            for j, s in enumerate(stops[:10]):
                print(f"    {j+1}. {s.get('name')}  @ {s.get('location')}")
            if len(stops) > 10:
                print(f"    ... 还有 {len(stops)-10} 个站点")
            print()
    else:
        print("返回内容:")
        print(json.dumps(data_b, ensure_ascii=False, indent=2)[:500])
