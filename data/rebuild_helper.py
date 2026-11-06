# -*- coding: utf-8 -*-
"""
人工重建辅助工具
用法：
  1. 在 routes_input/ 目录下创建 .txt 文件，例如 520路.txt
  2. 文件第一行：线路名（如 520路）
  3. 第二行：颜色（如 #e53935）
  4. 第三行：类型（bus 或 metro）
  5. 第四行起：每行一个站点名（按行驶顺序）
  6. 运行：python rebuild_helper.py
  7. 输出 stops_rebuilt.csv 和 routes_rebuilt.csv
"""
import csv
import os
import sys
import re

sys.stdout.reconfigure(encoding='utf-8')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MASTER_FILE = os.path.join(SCRIPT_DIR, "master_stops_beibei.csv")
INPUT_DIR = os.path.join(SCRIPT_DIR, "routes_input")
OUT_STOPS = os.path.join(SCRIPT_DIR, "stops_rebuilt.csv")
OUT_ROUTES = os.path.join(SCRIPT_DIR, "routes_rebuilt.csv")
MISSING_REPORT = os.path.join(SCRIPT_DIR, "missing_stops.txt")


def normalize(name):
    """归一化站点名：只去掉"(公交站)"后缀，保留"(地铁站)"等区分标记"""
    n = re.sub(r"[\(\(]公交站[\)\)]", "", name)
    n = re.sub(r"[\(\(]招呼站[\)\)]", "", n)
    return n.strip()


def load_master():
    """加载真实北碚站点库 → name -> (lng, lat)"""
    m = {}
    if not os.path.exists(MASTER_FILE):
        print(f"⚠ 找不到 {MASTER_FILE}，没有参考坐标")
        return m
    with open(MASTER_FILE, "r", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 3: continue
            name, lng, lat = row[0], row[1], row[2]
            m[normalize(name)] = (lng, lat)
    return m


def fuzzy_find(name, master):
    """模糊匹配 master 中的站点"""
    n = normalize(name)
    if n in master:
        return master[n], "exact"
    # 退化：包含匹配
    for k, v in master.items():
        if k in n or n in k:
            return v, f"近似匹配: {k}"
    return None, "未找到"


def parse_input_file(path):
    """解析输入文件
    注释：以 '# ' 开头（井号 + 空格）才算注释；'#e53935' 这种颜色码不算注释
    """
    with open(path, "r", encoding="utf-8") as f:
        raw = f.readlines()
    lines = []
    for ln in raw:
        s = ln.strip()
        if not s: continue
        if s.startswith("# "): continue   # 真正的注释（井号+空格）
        if s == "#": continue
        lines.append(s)
    if len(lines) < 4:
        print(f"⚠ {path} 格式错误，至少需要 4 行")
        return None
    return {
        "name": lines[0],
        "color": lines[1],
        "type": lines[2],
        "stops": lines[3:]
    }


def main():
    if not os.path.exists(INPUT_DIR):
        os.makedirs(INPUT_DIR)
        # 创建一个示例输入文件
        example = os.path.join(INPUT_DIR, "_example.txt")
        with open(example, "w", encoding="utf-8") as f:
            f.write("""# 这是示例。请把这个文件改名为"520路.txt"后修改内容。
# 第一行：线路名
520路
# 第二行：颜色
#e53935
# 第三行：类型 (bus 或 metro)
bus
# 第四行起：每行一个站点名（按行驶顺序）
缙云山
农庄
标本馆
三花石
北泉村大弯
北泉
团山堡
五路口
新重百
区门诊
中山路
北碚公交车站
""")
        print(f"✓ 已创建 {INPUT_DIR} 目录和示例文件")
        print(f"  请把站点序列填入新建文件（如 520路.txt），然后再运行本脚本。")
        return

    master = load_master()
    print(f"加载参考库: {len(master)} 个真实站点")

    # 处理所有 .txt 文件（跳过 _example.txt）
    txt_files = [f for f in os.listdir(INPUT_DIR)
                 if f.endswith(".txt") and not f.startswith("_")]
    if not txt_files:
        print(f"⚠ {INPUT_DIR} 目录里没有有效的 .txt 文件")
        return

    all_stops = {}  # name -> (id, lng, lat)
    next_id = 1
    routes = []
    missing = []  # 记录未在 master 找到的站

    for fname in sorted(txt_files):
        path = os.path.join(INPUT_DIR, fname)
        data = parse_input_file(path)
        if not data: continue

        rname = data["name"]
        print()
        print(f"=== {rname} ===")

        stop_ids = []
        for sname in data["stops"]:
            n = normalize(sname)
            if n not in all_stops:
                # 分配新 ID
                coord, hint = fuzzy_find(n, master)
                if coord:
                    all_stops[n] = (next_id, coord[0], coord[1])
                    print(f"  #{next_id:>3} {n:<18} ✓ {hint} ({coord[0]},{coord[1]})")
                else:
                    all_stops[n] = (next_id, "0.0", "0.0")
                    missing.append(f"{rname}: {n}")
                    print(f"  #{next_id:>3} {n:<18} ✗ 未找到坐标（请人工填写）")
                next_id += 1
            stop_ids.append(all_stops[n][0])

        rid = "R" + rname.replace("路", "").replace("路线", "").replace("轨道", "M")
        routes.append({
            "id": rid, "name": rname,
            "stop_sequence": "-".join(str(s) for s in stop_ids),
            "color": data["color"], "type": data["type"]
        })

    # 输出 stops_rebuilt.csv
    with open(OUT_STOPS, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["stop_id", "name", "lng", "lat"])
        for name, (sid, lng, lat) in sorted(all_stops.items(), key=lambda kv: kv[1][0]):
            w.writerow([sid, name, lng, lat])

    # 输出 routes_rebuilt.csv
    with open(OUT_ROUTES, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["route_id", "route_name", "stop_sequence", "color", "type"])
        for r in routes:
            w.writerow([r["id"], r["name"], r["stop_sequence"], r["color"], r["type"]])

    # 输出缺失报告
    if missing:
        with open(MISSING_REPORT, "w", encoding="utf-8") as f:
            f.write("\n".join(missing))

    print()
    print("=" * 40)
    print(f"✓ 处理完成：{len(routes)} 条线路，{len(all_stops)} 个站点")
    print(f"✓ 已输出：")
    print(f"   {OUT_STOPS}")
    print(f"   {OUT_ROUTES}")
    if missing:
        print(f"⚠ {len(missing)} 个站点未找到坐标，详见: {MISSING_REPORT}")
        print(f"  请手工编辑 {OUT_STOPS} 给这些站点填坐标")


if __name__ == "__main__":
    main()
