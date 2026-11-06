# -*- coding: utf-8 -*-
"""
批量地理编码：用高德 API 把 stops.csv 里的站点名换成真实坐标
"""
import csv
import json
import time
import math
import urllib.parse
import urllib.request
import os
import shutil
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IN_FILE = os.path.join(SCRIPT_DIR, "stops.csv")
OUT_FILE = os.path.join(SCRIPT_DIR, "stops_geocoded.csv")
BAK_FILE = os.path.join(SCRIPT_DIR, "stops_original.csv")
REPORT_FILE = os.path.join(SCRIPT_DIR, "geocode_report.txt")

# 北碚区中心和判定范围
ADCODE_BEIBEI = "500109"
CENTER_LNG, CENTER_LAT = 106.4373, 29.8058
MAX_DIST = 8.0  # 0.01-deg unit (≈ 8km，足够覆盖整个北碚主城)


def query_amap(keywords, types=None):
    """调用高德 POI 搜索"""
    params = {
        "keywords": keywords,
        "city": "500000",
        "output": "json",
        "offset": "15",
        "key": KEY,
    }
    if types:
        params["types"] = types
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        if data.get("status") == "1":
            return data.get("pois", []) or []
    except Exception as e:
        print(f"    [ERROR] {e}")
    return []


def pick_best(pois, query_name):
    """从结果列表里挑最佳匹配：在北碚区 + 距离北碚中心近 + 名字匹配"""
    best_poi, best_score = None, -1e9
    for poi in pois:
        # 高德 API 实际返回的是 adname 字段（如"北碚区"），adcode 经常为 None
        adname = poi.get("adname") or ""
        if "北碚" not in adname:
            continue
        loc = poi.get("location", "").split(",")
        if len(loc) != 2:
            continue
        try:
            lng, lat = float(loc[0]), float(loc[1])
        except ValueError:
            continue
        dlng = (lng - CENTER_LNG) * 100
        dlat = (lat - CENTER_LAT) * 100
        dist = math.sqrt(dlng * dlng + dlat * dlat)
        if dist > MAX_DIST:
            continue
        score = 10.0 - dist * 0.5
        # 去掉括号后的纯名字
        clean = poi["name"]
        import re
        clean_pure = re.sub(r"\(.*?\)", "", clean)
        if clean_pure == query_name:
            score += 30
        elif query_name in clean_pure or clean_pure in query_name:
            score += 15
        if score > best_score:
            best_score = score
            best_poi = poi
    return best_poi


def main():
    # 备份原始文件
    if not os.path.exists(BAK_FILE):
        shutil.copy(IN_FILE, BAK_FILE)
        print("Backed up original to stops_original.csv")

    with open(IN_FILE, "r", encoding="utf-8") as f:
        lines = [ln.rstrip("\n") for ln in f.readlines()]
    header = lines[0]
    rows = [ln.split(",") for ln in lines[1:] if ln.strip()]

    results = [header]
    report = []
    ok_bus, ok_poi, miss = 0, 0, 0
    total = len(rows)

    for i, row in enumerate(rows, 1):
        if len(row) < 4:
            results.append(",".join(row))
            continue
        stop_id, name, old_lng, old_lat = row[0], row[1], row[2], row[3]
        new_lng, new_lat = old_lng, old_lat
        matched, status = "", "MISS"

        # Pass 1: 限定公交站类型
        pois = query_amap("北碚区" + name, types="150700")
        best = pick_best(pois, name)
        if best:
            loc = best["location"].split(",")
            new_lng, new_lat = loc[0], loc[1]
            matched = best["name"]
            status = "BUS"
            ok_bus += 1
        else:
            # Pass 2: 不限类型
            pois2 = query_amap("北碚 " + name)
            best2 = pick_best(pois2, name)
            if best2:
                loc = best2["location"].split(",")
                new_lng, new_lat = loc[0], loc[1]
                matched = best2["name"]
                status = "POI"
                ok_poi += 1
            else:
                miss += 1

        results.append(f"{stop_id},{name},{new_lng},{new_lat}")
        report.append(
            f"[{status}] id={stop_id} name={name} old=({old_lng},{old_lat}) "
            f"new=({new_lng},{new_lat}) matched={matched}"
        )
        print(f"[{i:>3}/{total}] {name:<20} {status:<5} ({new_lng},{new_lat})  {matched}")

        time.sleep(0.12)  # 限速

    with open(OUT_FILE, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(results) + "\n")
    with open(REPORT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")

    print()
    print("=== Done ===")
    print(f"Bus stop match : {ok_bus}")
    print(f"General POI    : {ok_poi}")
    print(f"MISS (kept old): {miss}")
    print(f"Output: {OUT_FILE}")


if __name__ == "__main__":
    main()
