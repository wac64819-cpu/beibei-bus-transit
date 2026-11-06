# -*- coding: utf-8 -*-
"""
整合 routes_reference.csv 里所有 POI 数据，去重生成"真实北碚站点全集"
给用户作为校准参考
"""
import csv
import os
import sys
import re

sys.stdout.reconfigure(encoding='utf-8')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IN_FILE = os.path.join(SCRIPT_DIR, "routes_reference.csv")
EXISTING_STOPS = os.path.join(SCRIPT_DIR, "stops.csv")
OUT_FILE = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")


def clean_name(name):
    """去掉括号后缀，如 '北碚公交车站(公交站)' -> '北碚公交车站'"""
    return re.sub(r"[\(\(].*?[\)\)]", "", name).strip()


def main():
    # 读取已有 stops.csv
    existing = {}  # name (cleaned) -> (id, lng, lat)
    if os.path.exists(EXISTING_STOPS):
        with open(EXISTING_STOPS, "r", encoding="utf-8") as f:
            reader = csv.reader(f)
            next(reader)  # skip header
            for row in reader:
                if len(row) >= 4:
                    existing[clean_name(row[1])] = (row[0], row[2], row[3])

    # 读取 routes_reference.csv
    # 用 (clean_name, lng, lat) 三元组去重，但允许同名不同地点的算两个
    unique = {}  # (clean_name) -> { 'name': raw_name, 'lng', 'lat', 'lines': set, 'count': 1 }
    with open(IN_FILE, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            if len(row) < 6: continue
            route_id, route_name, stop_name, lng, lat, adname = row[:6]
            passes = row[6] if len(row) > 6 else ""
            cname = clean_name(stop_name)
            if cname not in unique:
                unique[cname] = {
                    'raw_name': cname,
                    'lng': lng,
                    'lat': lat,
                    'lines': set(),
                    'occurrences': 0,
                }
            unique[cname]['occurrences'] += 1
            # 解析经过线路
            for L in re.findall(r"\d+路", passes):
                unique[cname]['lines'].add(L)
            # 也加上当前 route_name
            for L in re.findall(r"\d+路", route_name):
                unique[cname]['lines'].add(L)

    # 排序：经过线路多的在前
    sorted_stops = sorted(unique.values(), key=lambda s: -len(s['lines']))

    # 输出 CSV
    with open(OUT_FILE, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["name", "lng", "lat", "passing_lines_count", "passing_lines", "is_in_existing"])
        for s in sorted_stops:
            in_existing = "YES" if s['raw_name'] in existing else "NO"
            lines_str = ", ".join(sorted(s['lines']))
            w.writerow([
                s['raw_name'], s['lng'], s['lat'],
                len(s['lines']), lines_str, in_existing
            ])

    # 打印摘要
    total = len(sorted_stops)
    in_csv = sum(1 for s in sorted_stops if s['raw_name'] in existing)
    not_in_csv = total - in_csv

    print(f"=== 整合完成 ===")
    print(f"真实北碚公交站点总数: {total}")
    print(f"  已在 stops.csv: {in_csv}")
    print(f"  新发现可补充: {not_in_csv}")
    print(f"输出文件: {OUT_FILE}")
    print()
    print(f"=== 最重要的 15 个枢纽站（经过线路最多）===")
    for s in sorted_stops[:15]:
        mark = "✓" if s['raw_name'] in existing else " "
        print(f"  [{mark}] {s['raw_name']:<20} ({s['lng']},{s['lat']})  经过 {len(s['lines'])} 条线路")


if __name__ == "__main__":
    main()
