# -*- coding: utf-8 -*-
"""
为 routes.csv 里 bus 线路的每对相邻站点抓取真实道路曲线
用高德 /v3/direction/driving API
输出 web/road_polylines.json 供地图加载
"""
import csv
import json
import math
import os
import sys
import time
import urllib.parse
import urllib.request

# 缓存校验阈值：若缓存曲线的端点离当前 stops.csv 站点 > 此距离（米），视为陈旧重抓
STALE_THRESHOLD_M = 200.0


def haversine_m(lng1, lat1, lng2, lat2):
    R = 6371000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlng / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))

sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STOPS_FILE = os.path.join(SCRIPT_DIR, "stops.csv")
ROUTES_FILE = os.path.join(SCRIPT_DIR, "routes.csv")
OUT_FILE = os.path.join(SCRIPT_DIR, "..", "web", "road_polylines.json")
OUT_FILE = os.path.abspath(OUT_FILE)


def load_stops():
    """stop_id -> (lng, lat)"""
    stops = {}
    with open(STOPS_FILE, "r", encoding="utf-8") as f:
        r = csv.reader(f); next(r)
        for row in r:
            if len(row) < 4: continue
            stops[int(row[0])] = (row[2], row[3])
    return stops


def load_routes():
    """[(route_id, route_name, [stop_ids], type)]"""
    routes = []
    with open(ROUTES_FILE, "r", encoding="utf-8") as f:
        r = csv.reader(f); next(r)
        for row in r:
            if len(row) < 4: continue
            rid = row[0]; rname = row[1]
            seq = [int(x) for x in row[2].split("-") if x.strip()]
            rtype = row[4] if len(row) >= 5 else "bus"
            routes.append((rid, rname, seq, rtype))
    return routes


def fetch_driving_polyline(origin_lng, origin_lat, dest_lng, dest_lat):
    """调高德驾车导航 API，返回沿道路的 [(lat,lng),...] 列表"""
    params = {
        "origin": f"{origin_lng},{origin_lat}",
        "destination": f"{dest_lng},{dest_lat}",
        "strategy": "0",  # 速度优先
        "output": "json",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/direction/driving?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        return None, f"网络错误: {e}"
    if data.get("status") != "1":
        return None, f"API 错误: {data.get('info')}"
    paths = (data.get("route") or {}).get("paths") or []
    if not paths:
        return None, "无路径"
    steps = paths[0].get("steps") or []
    coords = []  # [lat, lng] 格式（Leaflet 习惯）
    for s in steps:
        line = s.get("polyline") or ""
        for pt in line.split(";"):
            if not pt: continue
            xy = pt.split(",")
            if len(xy) == 2:
                try:
                    lng = float(xy[0]); lat = float(xy[1])
                    coords.append([lat, lng])
                except ValueError:
                    pass
    return coords, "OK"


def main():
    stops = load_stops()
    routes = load_routes()

    # 加载已存在的（增量更新）
    polylines = {}
    if os.path.exists(OUT_FILE):
        try:
            with open(OUT_FILE, "r", encoding="utf-8") as f:
                polylines = json.load(f)
        except Exception:
            polylines = {}

    total_pairs = 0
    new_pairs = 0
    cached_pairs = 0
    refetched_stale = 0
    failed_pairs = 0

    # 先做一次缓存清理：删掉 stops.csv 里已经没有的 stop_id 涉及的缓存
    valid_ids = set(stops.keys())
    orphan_keys = []
    for key in list(polylines.keys()):
        try:
            a_s, b_s = key.split("-")
            if int(a_s) not in valid_ids or int(b_s) not in valid_ids:
                orphan_keys.append(key)
        except ValueError:
            orphan_keys.append(key)
    for k in orphan_keys:
        del polylines[k]
    if orphan_keys:
        print(f"清理 {len(orphan_keys)} 条孤儿缓存（站点 ID 已不存在）")

    for rid, rname, seq, rtype in routes:
        if rtype != "bus":
            print(f"  跳过 {rname} (类型 {rtype})")
            continue
        print(f"=== {rname} ({len(seq)} 站) ===")
        for i in range(len(seq) - 1):
            a, b = seq[i], seq[i+1]
            key = f"{a}-{b}"
            total_pairs += 1
            if a not in stops or b not in stops:
                print(f"  跳过 {key}: 缺站点")
                failed_pairs += 1
                continue
            o_lng, o_lat = stops[a]
            d_lng, d_lat = stops[b]
            # 校验缓存是否仍然有效：检查曲线两端是否仍贴近当前站点坐标
            if key in polylines:
                pts = polylines[key]
                if pts and len(pts) >= 2:
                    first_lat, first_lng = pts[0][0], pts[0][1]
                    last_lat, last_lng = pts[-1][0], pts[-1][1]
                    d_start = haversine_m(float(o_lng), float(o_lat), first_lng, first_lat)
                    d_end = haversine_m(float(d_lng), float(d_lat), last_lng, last_lat)
                    if d_start <= STALE_THRESHOLD_M and d_end <= STALE_THRESHOLD_M:
                        cached_pairs += 1
                        continue
                    else:
                        print(f"  ⟳ {key:<10} 缓存陈旧 (端点偏移 {d_start:.0f}m/{d_end:.0f}m)，重抓")
                        del polylines[key]
                        refetched_stale += 1
                else:
                    del polylines[key]
            coords, msg = fetch_driving_polyline(o_lng, o_lat, d_lng, d_lat)
            if coords:
                polylines[key] = coords
                new_pairs += 1
                print(f"  ✓ {key:<10} ({len(coords)} 点)")
            else:
                failed_pairs += 1
                print(f"  ✗ {key:<10} {msg}")
            time.sleep(0.15)  # 限速

    # 保存
    os.makedirs(os.path.dirname(OUT_FILE), exist_ok=True)
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        json.dump(polylines, f, ensure_ascii=False)

    print()
    print(f"=== 完成 ===")
    print(f"总对数: {total_pairs}, 新抓: {new_pairs}, 缓存复用: {cached_pairs}, 陈旧重抓: {refetched_stale}, 失败: {failed_pairs}")
    print(f"输出: {OUT_FILE}")


if __name__ == "__main__":
    main()
