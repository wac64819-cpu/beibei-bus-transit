# -*- coding: utf-8 -*-
"""
批量抓取 routes.csv 里所有线路的沿途站点参考数据
输出 routes_reference.csv 给用户人工校准用
"""
import csv
import json
import time
import urllib.parse
import urllib.request
import os
import sys

sys.stdout.reconfigure(encoding='utf-8')
KEY = os.environ.get("AMAP_API_KEY", "")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IN_FILE = os.path.join(SCRIPT_DIR, "routes.csv")
OUT_FILE = os.path.join(SCRIPT_DIR, "routes_reference.csv")
SUMMARY_FILE = os.path.join(SCRIPT_DIR, "routes_summary.txt")


def query_route_stops(route_name):
    """查询某线路的沿途站点（POI search type=150700）"""
    params = {
        "keywords": route_name,
        "city": "500000",
        "types": "150700",
        "output": "json",
        "offset": "30",
        "extensions": "all",
        "key": KEY,
    }
    url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        return {"status": "0", "info": str(e), "pois": []}


def main():
    with open(IN_FILE, "r", encoding="utf-8") as f:
        lines = [ln.rstrip("\n") for ln in f.readlines() if ln.strip()]
    header = lines[0]
    rows = [ln.split(",") for ln in lines[1:]]

    out_rows = []
    summary = []
    out_rows.append(["route_id", "route_name", "stop_name", "lng", "lat", "adname", "passes_lines"])

    total = len(rows)
    for i, row in enumerate(rows, 1):
        if len(row) < 4:
            continue
        route_id, route_name = row[0], row[1]
        route_type = row[4] if len(row) >= 5 else "bus"

        print(f"[{i:>3}/{total}] 抓取 {route_name} ({route_type}) ...")
        data = query_route_stops(route_name)

        pois = data.get("pois") or []
        beibei_count = 0
        for p in pois:
            adname = p.get("adname") or ""
            if "北碚" not in adname:
                continue
            beibei_count += 1
            loc = (p.get("location") or "").split(",")
            if len(loc) != 2:
                continue
            # 经过线路：从 address 字段简化抽取
            addr = (p.get("address") or "").replace("路;", "路, ").replace(";", ", ")
            out_rows.append([
                route_id, route_name,
                p.get("name") or "",
                loc[0], loc[1],
                adname,
                addr[:200]
            ])

        summary.append(f"{route_id}  {route_name:<10}  共 {len(pois)} POI  北碚区 {beibei_count} 个")
        time.sleep(0.2)

    # 写出 CSV
    with open(OUT_FILE, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        for r in out_rows:
            w.writerow(r)

    with open(SUMMARY_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(summary))

    print()
    print("=== Done ===")
    print(f"输出: {OUT_FILE}")
    print(f"摘要: {SUMMARY_FILE}")
    print()
    print("=== 各线路覆盖情况 ===")
    for s in summary:
        print(s)


if __name__ == "__main__":
    main()
