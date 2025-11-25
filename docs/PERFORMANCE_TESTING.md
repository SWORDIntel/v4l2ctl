# Performance Regression Testing Guide

## Overview

The DSV4L2 project includes a comprehensive **performance regression testing framework** to track performance over time and detect slowdowns before they reach production. The system benchmarks critical operations and compares results against established baselines.

## Why Performance Testing?

Performance regressions can creep into codebases gradually through:
- Inefficient algorithms in new features
- Unnecessary memory allocations
- Poor caching strategies
- Lock contention in concurrent code
- Accidental O(n²) operations

This framework helps catch these issues early by:
- Tracking performance metrics over time
- Detecting regressions automatically in CI/CD
- Providing actionable data for optimization
- Establishing performance budgets

## Benchmarks

The performance suite measures 5 critical operations:

### 1. Event Emission (100,000 iterations)
**Metric**: Operations/sec for `dsv4l2rt_emit_simple()`

Tests the performance of the runtime event emission system, which is used for telemetry and logging throughout the application.

**Typical Performance**: ~5 million ops/sec (~200 ns/op)

### 2. THREATCON Operations (100,000 iterations)
**Metric**: Set/get operations for DSMIL threat condition levels

Tests the policy system's ability to query and modify threat levels, which happens frequently during security state transitions.

**Typical Performance**: ~128 million ops/sec (~8 ns/op)

### 3. KLV Parsing (10,000 iterations)
**Metric**: Parse operations for MISB KLV metadata

Tests the metadata parser performance on typical KLV packets (33 bytes). This is critical for real-time video metadata processing.

**Typical Performance**: ~60 million ops/sec (~17 ns/op)

### 4. Clearance Checking (100,000 iterations)
**Metric**: Authorization checks for device access

Tests the policy enforcement system's ability to validate clearance levels, which occurs on every device access.

**Typical Performance**: ~32 million ops/sec (~31 ns/op)

### 5. Event Buffer Operations (10,000 iterations)
**Metric**: Get signed chunk operations from ring buffer

Tests the forensic event buffer system, including event retrieval and optional TPM signing preparation.

**Typical Performance**: ~131,000 ops/sec (~7.6 µs/op)

## Quick Start

### Create Baseline

First, establish a performance baseline for your system:

```bash
make perf-baseline
```

This:
1. Builds the performance benchmark
2. Runs all 5 benchmarks
3. Saves results to `perf/baseline.json`

**Output**:
```
DSV4L2 Performance Benchmark Suite
===================================

Running benchmarks...
  [1/5] Event emission... done
  [2/5] THREATCON operations... done
  [3/5] KLV parsing... done
  [4/5] Clearance checking... done
  [5/5] Event buffer operations... done

╔══════════════════════════════════════════════════════════════════╗
║            DSV4L2 Performance Benchmark Results                 ║
╚══════════════════════════════════════════════════════════════════╝

Benchmark                         Ops/sec    Time/op (ns)
------------------------- --------------- ---------------
event_emission                    4939522           202.4
threatcon_ops                   128361015             7.8
klv_parsing                      60591716            16.5
clearance_check                  32531173            30.7
event_buffer_ops                   131463          7606.7

Results exported to: perf/baseline.json
```

### Run Regression Test

After making changes, check for performance regressions:

```bash
make perf
```

This:
1. Builds the benchmark (if needed)
2. Runs all benchmarks
3. Compares results against baseline
4. Reports any regressions (>10% slower)

**Example Output (No Regressions)**:
```
Running performance benchmarks...

Comparing against baseline...

Benchmark                  Baseline          Current         Change       Status
------------------------- ---------------  ---------------  -----------  ----------
clearance_check              32531173         32789456       +0.8%        OK
event_buffer_ops               131463           132891       +1.1%        OK
event_emission                4939522          4955321       +0.3%        OK
klv_parsing                  60591716         61234567       +1.1%        OK
threatcon_ops               128361015        129045678       +0.5%        OK

================================================================================

✓ No significant performance changes detected.
```

**Example Output (Regression Detected)**:
```
Benchmark                  Baseline          Current         Change       Status
------------------------- ---------------  ---------------  -----------  ----------
clearance_check              32531173         30123456       -7.4%        OK
event_buffer_ops               131463            98765      -24.9%        REGRESSION
event_emission                4939522          4955321       +0.3%        OK
klv_parsing                  60591716         55432100       -8.5%        OK
threatcon_ops               128361015        129045678       +0.5%        OK

================================================================================

⚠️  PERFORMANCE REGRESSIONS DETECTED (1):
  - event_buffer_ops: -24.9% slower

Performance regression detected!
```

## Detailed Usage

### Build Benchmark Only

```bash
make perf-build
```

Creates `perf/benchmark` executable without running it.

### Run Benchmark Manually

```bash
# Run with default output (perf/baseline.json)
LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH ./perf/benchmark

# Run with custom output file
LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH ./perf/benchmark perf/my_results.json
```

### Update Baseline

If performance changes are intentional (e.g., optimizations):

```bash
cp perf/current.json perf/baseline.json
```

Or recreate baseline:

```bash
make perf-baseline
```

### Clean Performance Artifacts

```bash
make perf-clean
```

Removes `perf/benchmark` and `perf/current.json` (keeps baseline).

## Understanding Results

### JSON Output Format

Results are saved in JSON format for easy parsing:

```json
{
  "timestamp": 1700000000,
  "benchmarks": [
    {
      "name": "event_emission",
      "ops_per_sec": 4939522,
      "time_per_op_ns": 202.4
    },
    ...
  ]
}
```

### Metrics Explained

#### Operations Per Second (ops/sec)
Higher is better. Indicates throughput - how many operations can be performed per second.

#### Time Per Operation (ns/op)
Lower is better. Indicates latency - how long each individual operation takes.

### Regression Threshold

By default, a **10% slowdown** triggers a regression warning. This can be adjusted:

```bash
# Custom threshold: 5% slowdown
PERF_THRESHOLD=5 make perf

# Custom threshold: 20% slowdown
PERF_THRESHOLD=20 make perf
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Performance

on: [push, pull_request]

jobs:
  performance:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build DSV4L2
        run: make

      - name: Download baseline
        run: |
          # Download from artifacts or repo
          wget https://example.com/baselines/perf_baseline.json -O perf/baseline.json

      - name: Run performance tests
        run: make perf

      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: performance-results
          path: perf/current.json
```

### GitLab CI

```yaml
performance:
  stage: test
  script:
    - make
    - make perf-baseline  # First run creates baseline
    - make perf            # Subsequent runs check regression
  artifacts:
    paths:
      - perf/baseline.json
      - perf/current.json
    reports:
      metrics: perf/current.json
```

### Jenkins

```groovy
stage('Performance') {
    steps {
        sh 'make'
        sh 'make perf || true'  // Don't fail build on regression
    }
    post {
        always {
            archiveArtifacts artifacts: 'perf/*.json'
            // Parse and trend over time
        }
    }
}
```

## Performance Budgets

Establish performance budgets for critical operations:

| Operation | Budget | Rationale |
|-----------|--------|-----------|
| Event emission | >4M ops/sec | Telemetry overhead <25% |
| THREATCON ops | >100M ops/sec | Negligible latency |
| KLV parsing | >50M ops/sec | Real-time metadata processing |
| Clearance check | >30M ops/sec | Sub-microsecond authorization |
| Event buffer | >100K ops/sec | 10 µs max forensic overhead |

If benchmarks fall below these thresholds, investigate immediately.

## Troubleshooting

### High Variance Between Runs

**Symptoms**: Results vary by >5% between runs

**Causes**:
- CPU frequency scaling
- Background processes
- Thermal throttling
- Insufficient warmup

**Solutions**:
```bash
# Disable CPU frequency scaling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Run multiple times and average
for i in {1..5}; do
  make perf-baseline
  mv perf/baseline.json perf/baseline_$i.json
done
```

### Benchmark Too Fast (>1B ops/sec)

**Cause**: Compiler optimized away the benchmark

**Solution**: Use `volatile` to prevent optimization (already done in benchmark.c)

### Benchmark Too Slow (<1K ops/sec)

**Causes**:
- Debug build
- Profiling enabled
- Valgrind/sanitizers active

**Solution**: Use optimized build without instrumentation

### Results Don't Match Baseline System

**Cause**: Different hardware/CPU

**Solution**: Create separate baselines per system:
```bash
make perf-baseline
mv perf/baseline.json perf/baseline_$(hostname).json
```

## Interpreting Regressions

### Small Regression (<5%)
- May be noise or system variance
- Monitor over time
- Investigate if consistent

### Medium Regression (5-15%)
- Likely real performance issue
- Review recent changes
- Profile with `perf`/`gprof`

### Large Regression (>15%)
- Serious performance problem
- Investigate immediately
- May indicate algorithmic issue

## Profiling Tools

When regressions are detected, use these tools to investigate:

### perf (Linux)

```bash
# Profile benchmark
perf record -g ./perf/benchmark
perf report

# Flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### gprof

```bash
# Build with profiling
make clean
CFLAGS="-pg" make perf-build

# Run and analyze
./perf/benchmark
gprof ./perf/benchmark gmon.out > analysis.txt
```

### Valgrind/Callgrind

```bash
valgrind --tool=callgrind ./perf/benchmark
kcachegrind callgrind.out.*
```

## Best Practices

### 1. Establish Baselines Early

Create baseline on stable, representative hardware before development.

### 2. Run Before Every Release

Include performance testing in release checklist.

### 3. Track Trends Over Time

Store baseline history to see long-term trends:

```bash
# Save historical baselines
git add perf/baseline.json
git commit -m "Update performance baseline - release v1.2.0"
```

### 4. Set Clear Thresholds

Define what level of regression is acceptable:
- <5%: Warning (investigate but don't block)
- 5-10%: Review required
- >10%: Block merge/release

### 5. Benchmark on Target Hardware

Performance characteristics differ between:
- Development laptops
- CI servers
- Production hardware

Always validate on production-like systems.

### 6. Isolate Benchmark Runs

For accurate results:
- Run on idle system
- Disable CPU frequency scaling
- Close background applications
- Use consistent power state

### 7. Automate Everything

Manual performance testing is error-prone. Automate in CI/CD pipeline.

## Advanced Usage

### Micro-benchmarks

Add custom benchmarks to `perf/benchmark.c`:

```c
static void benchmark_my_function(void)
{
    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_MEDIUM; i++) {
        my_function(arg1, arg2);
    }
    double elapsed = get_time_ms() - start;

    record_result("my_function", ITERATIONS_MEDIUM, elapsed);
}
```

### Performance Comparison

Compare performance across branches:

```bash
# Baseline (main branch)
git checkout main
make perf-baseline
cp perf/baseline.json perf/baseline_main.json

# Feature branch
git checkout feature-branch
make perf-baseline
cp perf/baseline.json perf/baseline_feature.json

# Compare
./scripts/compare_perf.py perf/baseline_main.json perf/baseline_feature.json
```

### Statistical Analysis

For rigorous analysis, run multiple iterations:

```bash
#!/bin/bash
for i in {1..20}; do
  LD_LIBRARY_PATH=lib ./perf/benchmark perf/run_$i.json
done

# Compute statistics (mean, stddev, confidence intervals)
./scripts/analyze_stats.py perf/run_*.json
```

## Performance Optimization Tips

When regressions are found, consider:

### 1. Algorithmic Improvements
- Use better data structures (hash tables vs linear search)
- Reduce algorithmic complexity (O(n²) → O(n log n))
- Cache expensive computations

### 2. Memory Optimization
- Reduce allocations (pool allocators)
- Improve cache locality (struct layout)
- Avoid unnecessary copies

### 3. Concurrency
- Reduce lock contention
- Use lock-free data structures
- Parallelize independent operations

### 4. Compiler Optimization
- Enable LTO (Link-Time Optimization)
- Use PGO (Profile-Guided Optimization)
- Review compiler warnings

### 5. I/O Optimization
- Batch operations
- Use buffering
- Avoid syscalls in hot paths

## References

- **Google Benchmark**: https://github.com/google/benchmark
- **Criterion (Rust)**: https://github.com/bheisler/criterion.rs
- **Linux perf**: https://perf.wiki.kernel.org/
- **Brendan Gregg's Performance**: https://www.brendangregg.com/

## See Also

- `TESTING.md` - Test suite documentation
- `COVERAGE_ANALYSIS.md` - Code coverage guide
- `perf/benchmark.c` - Benchmark source code
- `scripts/run_perf.sh` - Regression test runner
