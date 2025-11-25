#!/bin/bash
#
# DSV4L2 Performance Regression Test Runner
#
# Runs performance benchmarks and compares against baseline to detect
# performance regressions.
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Threshold for regression detection (default: 10% slower)
THRESHOLD=${PERF_THRESHOLD:-10}

# Baseline file
BASELINE="perf/baseline.json"
CURRENT="perf/current.json"

echo "╔════════════════════════════════════════════════════════╗"
echo "║      DSV4L2 Performance Regression Test Runner        ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Check if benchmark binary exists
if [ ! -f "perf/benchmark" ]; then
    echo "Building performance benchmark..."
    make perf-build
fi

# Run benchmarks
echo "Running performance benchmarks..."
echo ""
LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./perf/benchmark "$CURRENT"
echo ""

# Check if baseline exists
if [ ! -f "$BASELINE" ]; then
    echo -e "${YELLOW}No baseline found. Creating baseline from current run...${NC}"
    cp "$CURRENT" "$BASELINE"
    echo "Baseline saved to: $BASELINE"
    echo ""
    echo "Run this script again to compare against baseline."
    exit 0
fi

# Compare against baseline
echo "Comparing against baseline..."
echo ""

# Parse JSON and compare (simple awk-based parser)
python3 << 'EOF'
import json
import sys

# Load data
with open('perf/baseline.json', 'r') as f:
    baseline = json.load(f)

with open('perf/current.json', 'r') as f:
    current = json.load(f)

# Create lookup tables
baseline_map = {b['name']: b for b in baseline['benchmarks']}
current_map = {c['name']: c for c in current['benchmarks']}

# Compare
print(f"{'Benchmark':<25} {'Baseline':<15} {'Current':<15} {'Change':<12} {'Status'}")
print(f"{'-'*25} {'-'*15} {'-'*15} {'-'*12} {'-'*10}")

regressions = []
improvements = []
threshold = 10  # 10% threshold

for name in sorted(baseline_map.keys()):
    if name not in current_map:
        continue

    baseline_ops = baseline_map[name]['ops_per_sec']
    current_ops = current_map[name]['ops_per_sec']

    change_pct = ((current_ops - baseline_ops) / baseline_ops) * 100.0

    status = "OK"
    if change_pct < -threshold:
        status = "REGRESSION"
        regressions.append((name, change_pct))
    elif change_pct > threshold:
        status = "IMPROVED"
        improvements.append((name, change_pct))

    print(f"{name:<25} {baseline_ops:>13.0f}   {current_ops:>13.0f}   {change_pct:>+10.1f}%  {status}")

print("")
print("="*80)
print("")

if regressions:
    print(f"⚠️  PERFORMANCE REGRESSIONS DETECTED ({len(regressions)}):")
    for name, change in regressions:
        print(f"  - {name}: {change:.1f}% slower")
    print("")
    sys.exit(1)
elif improvements:
    print(f"✓ Performance improved in {len(improvements)} benchmark(s)!")
    for name, change in improvements:
        print(f"  - {name}: {change:.1f}% faster")
    print("")
    sys.exit(0)
else:
    print("✓ No significant performance changes detected.")
    print("")
    sys.exit(0)
EOF

# Check Python script exit code
exit_code=$?

if [ $exit_code -eq 1 ]; then
    echo -e "${RED}Performance regression detected!${NC}"
    echo "Review the changes above and investigate slow benchmarks."
    echo ""
    echo "To update baseline (if intentional):"
    echo "  cp $CURRENT $BASELINE"
    exit 1
elif [ $exit_code -eq 0 ]; then
    echo -e "${GREEN}All performance benchmarks passed!${NC}"
    exit 0
else
    echo -e "${RED}Error running performance comparison${NC}"
    exit 1
fi
