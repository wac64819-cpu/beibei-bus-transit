# -*- coding: utf-8 -*-
"""
一键工作流：
1. 跑 rebuild_helper.py 解析 routes_input/*.txt → stops_rebuilt.csv / routes_rebuilt.csv
2. 覆盖到正式 stops.csv / routes.csv
3. 跑 fetch_road_polylines.py（带陈旧缓存自动校验）

任何时候新增或修改线路后，只跑这一个脚本即可。
"""
import os, sys, shutil, subprocess

sys.stdout.reconfigure(encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))

def run(name, *args):
    print(f"\n========== {name} ==========")
    r = subprocess.run([sys.executable, os.path.join(HERE, name), *args])
    if r.returncode != 0:
        print(f"!! {name} 失败，终止"); sys.exit(r.returncode)

def main():
    # 1. rebuild
    run("rebuild_helper.py")

    # 2. 覆盖
    src_stops = os.path.join(HERE, "stops_rebuilt.csv")
    src_routes = os.path.join(HERE, "routes_rebuilt.csv")
    dst_stops = os.path.join(HERE, "stops.csv")
    dst_routes = os.path.join(HERE, "routes.csv")
    if not (os.path.exists(src_stops) and os.path.exists(src_routes)):
        print("!! rebuild 没生成 _rebuilt 文件"); sys.exit(1)
    shutil.copyfile(src_stops, dst_stops)
    shutil.copyfile(src_routes, dst_routes)
    print(f"已覆盖 stops.csv / routes.csv")

    # 3. fetch（自带校验，陈旧自动重抓）
    run("fetch_road_polylines.py")

    print("\n✓ 全部完成。重启程序即可看到新数据。")

if __name__ == "__main__":
    main()
