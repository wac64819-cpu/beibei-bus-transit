# -*- coding: utf-8 -*-
"""
小规模重建演示：
  1. 从 master_stops_beibei.csv 选 30 个最重要的站点
  2. 为 5 条数据完整的线路（520/521/511/583/580）重建数据
  3. 用最近邻启发式给站点排序（近似实际行驶顺序）
  4. 输出 stops_v2.csv / routes_v2.csv  （不覆盖原文件）
"""
import csv
import os
import sys
import re
import math

sys.stdout.reconfigure(encoding='utf-8')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER_FILE = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")
REFERENCE_FILE = os.path.join(SCRIPT_DIR, "routes_reference.csv")
OUT_STOPS = os.path.join(SCRIPT_DIR, "stops_v2.csv")
OUT_ROUTES = os.path.join(SCRIPT_DIR, "routes_v2.csv")

# 选取这 5 条数据覆盖最好的线路做演示
TARGET_ROUTES = ["520路", "521路", "511路", "583路", "580路"]
N_STOPS = 30  # 取前 30 个最重要的枢纽

# 调色板
COLORS = {
    "520路": "#e53935",
    "521路": "#7cb342",
    "511路": "#3949ab",
    "583路": "#ab47bc",
    "580路": "#26a69a",
}


def haversine(lng1, lat1, lng2, lat2):
    R = 6371.0
    p = math.pi / 180
    a = (0.5 - math.cos((lat2-lat1)*p)/2
         + math.cos(lat1*p) * math.cos(lat2*p)
         * (1 - math.cos((lng2-lng1)*p)) / 2)
    return 2 * R * math.asin(math.sqrt(a))


def clean(name):
    return re.sub(r"[\(\(].*?[\)\)]", "", name).strip()


def load_master():
    """返回 [{name, lng, lat, lines:set}]"""
    stops = []
    with open(MASTER_FILE, "r", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 5: continue
            name, lng, lat, n, lines = row[:5]
            stops.append({
                "name": name,
                "lng": float(lng),
                "lat": float(lat),
                "lines": set(re.findall(r"\d+路", lines)),
            })
    return stops


def load_reference_for_routes(target_routes):
    """每条 target_route 返回它的 POI 列表"""
    by_route = {r: [] for r in target_routes}
    with open(REFERENCE_FILE, "r", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 6: continue
            rid, rname, sname, lng, lat, adname = row[:6]
            if rname in by_route:
                by_route[rname].append({
                    "name": clean(sname),
                    "lng": float(lng),
                    "lat": float(lat),
                })
    return by_route


def nearest_neighbor_order(points, start=None):
    """从 start 开始做最近邻排序，近似实际路径顺序"""
    if not points:
        return []
    remaining = points[:]
    if start is None:
        # 选最西南角的点作起点（粗略）
        start = min(remaining, key=lambda p: p["lng"] + p["lat"])
    ordered = [start]
    remaining.remove(start)
    while remaining:
        last = ordered[-1]
        nxt = min(remaining, key=lambda p: haversine(p["lng"], p["lat"], last["lng"], last["lat"]))
        ordered.append(nxt)
        remaining.remove(nxt)
    return ordered


def main():
    master = load_master()
    print(f"加载 master: {len(master)} 个真实站点")

    # 1) 选 N_STOPS 个最重要的（经过线路数最多）
    master_sorted = sorted(master, key=lambda s: -len(s["lines"]))
    chosen = master_sorted[:N_STOPS]
    print(f"选取最重要的 {N_STOPS} 个站")

    # 给新站点分配 ID（1-N_STOPS）
    name_to_id = {}
    new_stops = []
    for i, s in enumerate(chosen, 1):
        name_to_id[s["name"]] = i
        new_stops.append({
            "id": i, "name": s["name"],
            "lng": s["lng"], "lat": s["lat"]
        })

    # 2) 为每条目标线路重建序列
    refs = load_reference_for_routes(TARGET_ROUTES)
    new_routes = []
    for rname in TARGET_ROUTES:
        pois = refs.get(rname, [])
        # 只保留出现在 chosen 30 个里的站点
        on_route = [p for p in pois if p["name"] in name_to_id]
        if len(on_route) < 2:
            print(f"  跳过 {rname}: 在 30 个站里只有 {len(on_route)} 个匹配")
            continue
        # 最近邻排序
        ordered = nearest_neighbor_order(on_route)
        seq = "-".join(str(name_to_id[p["name"]]) for p in ordered)
        rid = "R" + rname.replace("路", "")
        new_routes.append({
            "id": rid, "name": rname,
            "stop_sequence": seq,
            "color": COLORS.get(rname, "#1976d2"),
            "type": "bus"
        })
        print(f"  {rname}: {len(ordered)} 站  序列: {seq}")

    # 3) 输出文件
    with open(OUT_STOPS, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["stop_id", "name", "lng", "lat"])
        for s in new_stops:
            w.writerow([s["id"], s["name"], f"{s['lng']:.6f}", f"{s['lat']:.6f}"])

    with open(OUT_ROUTES, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["route_id", "route_name", "stop_sequence", "color", "type"])
        for r in new_routes:
            w.writerow([r["id"], r["name"], r["stop_sequence"], r["color"], r["type"]])

    print()
    print(f"=== 输出 ===")
    print(f"{OUT_STOPS}  ({len(new_stops)} 站点)")
    print(f"{OUT_ROUTES}  ({len(new_routes)} 线路)")
    print()
    print(f"=== 30 个站点列表 ===")
    for s in new_stops:
        print(f"  #{s['id']:>2} {s['name']:<18} ({s['lng']},{s['lat']})")


if __name__ == "__main__":
    main()
