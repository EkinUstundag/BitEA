import pandas as pd
import sys

if len(sys.argv) != 3:
    print("Usage: python summarize.py input.csv output.csv")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

test_count = 20

# Absolute minimum values for each graph
absolute_minimums = [
    65, 89, 32, 68, 97, 37, 72, 105, 35, 33,
    33, 8, 32, 42, 12, 37, 49, 16, 40, 65,
    18, 43, 73, 23, 47, 73, 24, 66, 76, 27,
    61, 73, 30
]

# Read CSV
df = pd.read_csv(input_file, skipinitialspace=True)

# Convert columns to numeric
df["cost"] = pd.to_numeric(df["cost"], errors="coerce")
df["k time"] = pd.to_numeric(df["k time"], errors="coerce")

results = []

# Process each graph
for graph_name, group in df.groupby("graph name"):

    # Minimum cost found by your algorithm
    min_cost = group["cost"].min()

    # All rows having the minimum cost
    min_rows = group[group["cost"] == min_cost]

    # Time of the first minimum-cost result
    time_of_min = min_rows.iloc[0]["k time"]

    results.append({
        "graph name": graph_name,
        "min cost": min_cost,
        "absolute minimum": None,
        "isMin": None,
        "count min": f"{len(min_rows)}/{test_count}",
        "average cost": group["cost"].mean(),
        "time of min": f"{time_of_min:.2f}"
    })

# Create summary DataFrame
summary_df = pd.DataFrame(results)

# Assign absolute minimum values
summary_df["absolute minimum"] = absolute_minimums

# Check whether the calculated minimum is the absolute minimum
summary_df["isMin"] = (
    summary_df["min cost"] == summary_df["absolute minimum"]
)

# Save output
summary_df.to_csv(output_file, index=False)

print(f"Summary saved to: {output_file}")