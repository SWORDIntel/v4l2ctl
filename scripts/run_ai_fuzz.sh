#!/bin/bash
#
# DSV4L2 AI-Guided Distributed Fuzzing Runner
#
# Orchestrates OpenVINO-accelerated fuzzing across all available
# Intel Architecture compute resources for zero-day exploit discovery.
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Directories
FUZZ_DIR="fuzz"
SEEDS_DIR="$FUZZ_DIR/seeds"
FINDINGS_DIR="$FUZZ_DIR/findings_ai"
HARNESS="$FUZZ_DIR/fuzz_ai_guided"
FUZZER_SCRIPT="$FUZZ_DIR/openvino_fuzzer.py"
DETECTOR_SCRIPT="$FUZZ_DIR/exploit_detector.py"

# Default duration (seconds)
DURATION=${FUZZ_DURATION:-3600}

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   DSV4L2 AI-Guided Distributed Fuzzing Framework      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check prerequisites
echo "Checking prerequisites..."

# Check if fuzzing harness exists
if [ ! -f "$HARNESS" ]; then
    echo -e "${RED}Error: Fuzzing harness not found${NC}"
    echo "Build with: make fuzz-ai"
    exit 1
fi

# Check for Python 3
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: Python 3 not found${NC}"
    exit 1
fi

# Check for seed files
if [ ! -d "$SEEDS_DIR" ] || [ -z "$(ls -A $SEEDS_DIR/*.bin 2>/dev/null)" ]; then
    echo -e "${YELLOW}Warning: No seed files found in $SEEDS_DIR${NC}"
    echo "Generating seed files..."
    cd "$SEEDS_DIR"
    ./generate_seeds.sh
    cd - > /dev/null
fi

echo -e "${GREEN}✓${NC} All prerequisites met"
echo ""

# Check for OpenVINO
echo "Detecting compute resources..."
if python3 -c "import openvino" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} OpenVINO detected - AI-guided fuzzing enabled"
    HAVE_OPENVINO=1
else
    echo -e "${YELLOW}⊘${NC} OpenVINO not found - using standard fuzzing"
    HAVE_OPENVINO=0
fi

# Check for NumPy
if python3 -c "import numpy" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} NumPy detected"
else
    echo -e "${YELLOW}⊘${NC} NumPy not found - using fallback mutations"
fi

echo ""

# Display fuzzing configuration
echo "Fuzzing Configuration:"
echo "  Target:   $HARNESS"
echo "  Seeds:    $SEEDS_DIR ($(ls -1 $SEEDS_DIR/*.bin 2>/dev/null | wc -l) files)"
echo "  Output:   $FINDINGS_DIR"
echo "  Duration: ${DURATION}s ($(($DURATION / 60)) minutes)"
echo ""

# Create findings directory
mkdir -p "$FINDINGS_DIR"

# Start distributed fuzzing
echo -e "${BLUE}Starting distributed fuzzing...${NC}"
echo ""

if [ "$HAVE_OPENVINO" -eq 1 ]; then
    # OpenVINO-accelerated fuzzing
    echo "Running OpenVINO-accelerated multi-device fuzzing..."
    python3 "$FUZZER_SCRIPT" \
        "$HARNESS" \
        -i "$SEEDS_DIR" \
        -o "$FINDINGS_DIR" \
        -t "$DURATION" &
    FUZZER_PID=$!
else
    # Fallback: Standard AFL fuzzing
    echo "Running standard AFL fuzzing (fallback)..."
    if command -v afl-fuzz &> /dev/null; then
        timeout "${DURATION}s" afl-fuzz \
            -i "$SEEDS_DIR" \
            -o "$FINDINGS_DIR" \
            -- "$HARNESS" || true
    else
        echo -e "${RED}Error: AFL not found and OpenVINO unavailable${NC}"
        exit 1
    fi
fi

# Wait for fuzzing to complete
if [ -n "$FUZZER_PID" ]; then
    echo "Fuzzing in progress (PID: $FUZZER_PID)..."
    echo "Press Ctrl+C to stop"
    wait $FUZZER_PID || true
fi

echo ""
echo -e "${GREEN}Fuzzing completed!${NC}"
echo ""

# Analyze crashes
echo "Analyzing crashes for exploitability..."
echo ""

TOTAL_CRASHES=0
CRITICAL_CRASHES=0

# Search all device directories for crashes
for device_dir in "$FINDINGS_DIR"/*; do
    if [ -d "$device_dir/crashes" ]; then
        crash_count=$(find "$device_dir/crashes" -name "*.bin" 2>/dev/null | wc -l)
        if [ "$crash_count" -gt 0 ]; then
            echo "Found $crash_count crashes in $(basename $device_dir)"
            TOTAL_CRASHES=$((TOTAL_CRASHES + crash_count))

            # Run exploit detector
            if [ -f "$DETECTOR_SCRIPT" ]; then
                device_name=$(basename "$device_dir")
                analysis_file="$device_dir/analysis.json"

                python3 "$DETECTOR_SCRIPT" \
                    "$HARNESS" \
                    "$device_dir/crashes" \
                    -o "$analysis_file" || true

                # Count critical crashes
                if [ -f "$analysis_file" ]; then
                    critical=$(python3 -c "import json; data=json.load(open('$analysis_file')); print(sum(1 for x in data if x.get('severity') == 'critical'))" 2>/dev/null || echo "0")
                    CRITICAL_CRASHES=$((CRITICAL_CRASHES + critical))
                fi
            fi
        fi
    fi
done

# Print final summary
echo ""
echo "╔════════════════════════════════════════════════════════╗"
echo "║              Fuzzing Campaign Summary                  ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

if [ "$TOTAL_CRASHES" -eq 0 ]; then
    echo -e "${GREEN}✓ No crashes detected - target appears stable${NC}"
    echo ""
    exit 0
fi

echo -e "${RED}⚠️  CRASHES DETECTED${NC}"
echo ""
echo "Total Crashes:    $TOTAL_CRASHES"
echo "Critical Crashes: $CRITICAL_CRASHES"
echo ""

if [ "$CRITICAL_CRASHES" -gt 0 ]; then
    echo -e "${RED}CRITICAL: Potentially exploitable vulnerabilities found!${NC}"
    echo ""
    echo "Immediate Actions Required:"
    echo "  1. Review crash files in: $FINDINGS_DIR/*/crashes/"
    echo "  2. Examine exploit analyses: $FINDINGS_DIR/*/analysis.json"
    echo "  3. Reproduce crashes: ./$HARNESS <crash_file>"
    echo "  4. Debug with GDB: gdb --args ./$HARNESS <crash_file>"
    echo "  5. Fix vulnerabilities and re-fuzz"
    echo ""
    exit 1
else
    echo "Review crashes in: $FINDINGS_DIR"
    echo ""
    exit 0
fi
