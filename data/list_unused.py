import csv, sys
sys.stdout.reconfigure(encoding='utf-8')
with open('master_stops_beibei.csv', encoding='utf-8') as f:
    r = csv.reader(f); next(r)
    master = [(row[0], row[4] if len(row)>=5 else '') for row in r if row[0].strip()]
with open('stops.csv', encoding='utf-8') as f:
    r = csv.reader(f); next(r)
    used = {row[1] for row in r if len(row) >= 2}
unused = [(n, lines) for n, lines in master if n not in used]
print(f'Master {len(master)}, used {len(used)}, unused {len(unused)}\n')
for n, lines in unused[:40]:
    print(f'  {n:<20}  {lines[:60]}')
