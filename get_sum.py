from pathlib import Path
ans = 0

for fn in Path("./results").glob("bw_*.txt"):
    with fn.open(mode="r") as f:
        for row in f.readlines():
            ans += float(row.strip())

print("sum = " + str(ans))
