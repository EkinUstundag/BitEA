import pandas as pd
import sys

if len(sys.argv) != 3:
    print("Usage: python summarize.py input.csv output.csv")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]
test_count=20

df = pd.read_csv(input_file)

df["cost"] = pd.to_numeric(df["cost"], errors="coerce")
df["k time"] = pd.to_numeric(df["k time"], errors="coerce")

results = []

for graph_name, group in df.groupby("graph name"):
    min_cost = group["cost"].min()
    min_rows = group[group["cost"] == min_cost]
    time_of_min =min_rows.iloc[0]["k time"]
    results.append({
        "graph name": graph_name,
        "min cost": min_cost,
        "average cost": group["cost"].mean(),
        "time of min": f"{time_of_min:.2f}",
        "count min": f"{len(min_rows)}/{test_count}"
    })

summary_df = pd.DataFrame(results)
summary_df.to_csv(output_file, index=False)

print(f"Summary saved to: {output_file}")