# DSV4L2 Makefile
# DSLLVM-instrumented sensor stack build system

# Compiler selection
CC ?= gcc
# For DSLLVM build, use: make CC=dsclang DSLLVM=1
DSLLVM ?= 0

# Optional TPM2 hardware support
# Set to 1 to enable TPM2-TSS integration: make HAVE_TPM2=1
HAVE_TPM2 ?= 0

# Optional coverage analysis
# Set to 1 to enable gcov coverage: make COVERAGE=1
COVERAGE ?= 0

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
LIB_DIR = lib
TEST_DIR = tests

# Output
LIB_NAME = libdsv4l2
STATIC_LIB = $(LIB_DIR)/$(LIB_NAME).a
SHARED_LIB = $(LIB_DIR)/$(LIB_NAME).so
RUNTIME_LIB = $(LIB_DIR)/libdsv4l2rt.a

# Source files
CORE_SRCS = $(SRC_DIR)/device.c \
            $(SRC_DIR)/tempest.c \
            $(SRC_DIR)/buffer.c \
            $(SRC_DIR)/capture.c \
            $(SRC_DIR)/format.c \
            $(SRC_DIR)/profiles/profile_loader.c \
            $(SRC_DIR)/policy/dsmil_bridge.c \
            $(SRC_DIR)/metadata.c

RUNTIME_SRCS = $(SRC_DIR)/runtime/event_buffer.c \
               $(SRC_DIR)/runtime/tpm_sign.c

# Object files
CORE_OBJS = $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_OBJS = $(RUNTIME_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -g -fPIC
CFLAGS += -I$(INC_DIR) -I/usr/include/libv4l2
LDFLAGS = -lpthread

# DSLLVM flags (if enabled)
ifeq ($(DSLLVM),1)
    DSLLVM_PLUGIN = libDSLLVM.so
    DSLLVM_CONFIG = config/dsllvm_dsv4l2_passes.yaml
    PROFILE ?= ops
    MISSION ?= dev

    CFLAGS += -fplugin=$(DSLLVM_PLUGIN)
    CFLAGS += -fplugin-arg-dsllvm-pass-config=$(DSLLVM_CONFIG)
    CFLAGS += -fdsv4l2-profile=$(PROFILE)
    CFLAGS += -mdsv4l2-mission=$(MISSION)
endif

# TPM2 flags (if enabled)
ifeq ($(HAVE_TPM2),1)
    CFLAGS += -DHAVE_TPM2
    LDFLAGS += -ltss2-esys -ltss2-rc -ltss2-mu -lcrypto
endif

# Coverage flags (if enabled)
ifeq ($(COVERAGE),1)
    CFLAGS += --coverage -fprofile-arcs -ftest-coverage
    LDFLAGS += --coverage -lgcov
endif

# CLI tool
CLI_BIN = bin/dsv4l2
CLI_SRC = $(SRC_DIR)/cli/main.c

# Targets
.PHONY: all clean libs core runtime test install cli coverage coverage-clean coverage-report fuzz fuzz-run fuzz-clean fuzz-ai fuzz-ai-run fuzz-ai-analyze fuzz-ai-clean perf perf-build perf-run perf-baseline perf-clean

all: libs cli

libs: core runtime

core: $(STATIC_LIB) $(SHARED_LIB)

runtime: $(RUNTIME_LIB)

# Create directories
$(BUILD_DIR) $(LIB_DIR):
	@mkdir -p $@

$(BUILD_DIR)/runtime $(BUILD_DIR)/profiles $(BUILD_DIR)/policy:
	@mkdir -p $@

# Build core library (static)
$(STATIC_LIB): $(CORE_OBJS) | $(LIB_DIR)
	@echo "AR $@"
	@ar rcs $@ $^

# Build core library (shared)
$(SHARED_LIB): $(CORE_OBJS) | $(LIB_DIR)
	@echo "LD $@"
	@$(CC) -shared -o $@ $^ $(LDFLAGS)

# Build runtime library
$(RUNTIME_LIB): $(RUNTIME_OBJS) | $(LIB_DIR)
	@echo "AR $@"
	@ar rcs $@ $^

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR) $(BUILD_DIR)/runtime $(BUILD_DIR)/profiles $(BUILD_DIR)/policy
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Build CLI tool
cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_SRC) libs | bin
	@echo "CC $@"
	@$(CC) $(CFLAGS) $(CLI_SRC) -L$(LIB_DIR) -ldsv4l2 -ldsv4l2rt $(LDFLAGS) -o $@

bin:
	@mkdir -p bin

# Test programs
test: libs
	@echo "Building tests..."
	@$(MAKE) -C $(TEST_DIR)

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(LIB_DIR) bin
	@$(MAKE) -C $(TEST_DIR) clean 2>/dev/null || true

# Install
install: libs
	@echo "Installing libraries to /usr/local..."
	@install -d /usr/local/lib
	@install -m 644 $(STATIC_LIB) /usr/local/lib/
	@install -m 755 $(SHARED_LIB) /usr/local/lib/
	@install -m 644 $(RUNTIME_LIB) /usr/local/lib/
	@install -d /usr/local/include/dsv4l2
	@install -m 644 $(INC_DIR)/*.h /usr/local/include/dsv4l2/
	@ldconfig

# Coverage analysis
.PHONY: coverage coverage-clean coverage-report

coverage:
	@echo "Building with coverage instrumentation..."
	@$(MAKE) clean
	@$(MAKE) COVERAGE=1 all test
	@echo "Running tests..."
	@./scripts/run_coverage.sh

coverage-clean:
	@echo "Cleaning coverage data..."
	@find . -name "*.gcda" -delete
	@find . -name "*.gcno" -delete
	@find . -name "*.gcov" -delete
	@rm -rf coverage_html

coverage-report: coverage
	@echo "Generating HTML coverage report..."
	@mkdir -p coverage_html
	@lcov --capture --directory . --output-file coverage_html/coverage.info --rc lcov_branch_coverage=1
	@lcov --remove coverage_html/coverage.info '/usr/*' --output-file coverage_html/coverage.info
	@genhtml coverage_html/coverage.info --output-directory coverage_html --branch-coverage
	@echo "Coverage report generated in coverage_html/index.html"

# Fuzzing with AFL
.PHONY: fuzz fuzz-run fuzz-clean

fuzz:
	@echo "Building fuzzing harness with AFL..."
	@if ! command -v afl-gcc &> /dev/null; then \
		echo "Error: AFL not installed. Install with:"; \
		echo "  Ubuntu/Debian: sudo apt-get install afl++"; \
		echo "  Or build from source: https://github.com/AFLplusplus/AFLplusplus"; \
		exit 1; \
	fi
	@$(MAKE) clean
	@CC=afl-gcc $(MAKE) STATIC_LIB
	@echo "CC fuzz/fuzz_klv_parser.c"
	@afl-gcc -I$(INC_DIR) -O2 -g fuzz/fuzz_klv_parser.c -L$(LIB_DIR) -ldsv4l2 -o fuzz/fuzz_klv_parser

fuzz-run: fuzz
	@echo "Starting AFL fuzzing session..."
	@echo "Input seeds: fuzz/seeds/"
	@echo "Output: fuzz/findings/"
	@echo ""
	@echo "Press Ctrl+C to stop fuzzing"
	@echo ""
	@mkdir -p fuzz/findings
	@afl-fuzz -i fuzz/seeds -o fuzz/findings -- ./fuzz/fuzz_klv_parser

fuzz-clean:
	@echo "Cleaning fuzzing artifacts..."
	@rm -rf fuzz/findings fuzz/fuzz_klv_parser

# AI-Guided Fuzzing with OpenVINO
.PHONY: fuzz-ai fuzz-ai-run fuzz-ai-analyze fuzz-ai-clean

fuzz-ai:
	@echo "Building AI-guided fuzzing harness..."
	@$(MAKE) libs
	@echo "CC fuzz/fuzz_ai_guided.c"
	@$(CC) $(CFLAGS) -I$(INC_DIR) -O2 -g fuzz/fuzz_ai_guided.c -L$(LIB_DIR) -ldsv4l2 -ldsv4l2rt $(LDFLAGS) -o fuzz/fuzz_ai_guided
	@echo "AI-guided fuzzing harness built: fuzz/fuzz_ai_guided"

fuzz-ai-run: fuzz-ai
	@echo "Starting OpenVINO-accelerated distributed fuzzing..."
	@./scripts/run_ai_fuzz.sh

fuzz-ai-analyze:
	@echo "Analyzing crashes for exploitability..."
	@if [ -d fuzz/findings_ai ]; then \
		for device_dir in fuzz/findings_ai/*; do \
			if [ -d "$$device_dir/crashes" ]; then \
				crash_count=$$(find "$$device_dir/crashes" -name "*.bin" 2>/dev/null | wc -l); \
				if [ "$$crash_count" -gt 0 ]; then \
					echo "Analyzing $$crash_count crashes in $$device_dir..."; \
					python3 fuzz/exploit_detector.py \
						fuzz/fuzz_ai_guided \
						"$$device_dir/crashes" \
						-o "$$device_dir/analysis.json"; \
				fi; \
			fi; \
		done; \
	else \
		echo "No fuzzing findings to analyze. Run 'make fuzz-ai-run' first."; \
	fi

fuzz-ai-clean:
	@echo "Cleaning AI fuzzing artifacts..."
	@rm -rf fuzz/findings_ai fuzz/fuzz_ai_guided fuzz/feedback.json

# Performance benchmarking
.PHONY: perf perf-build perf-run perf-baseline perf-clean

perf-build:
	@echo "Building performance benchmark..."
	@$(MAKE) libs
	@mkdir -p perf
	@$(CC) $(CFLAGS) perf/benchmark.c -L$(LIB_DIR) -ldsv4l2 -ldsv4l2rt $(LDFLAGS) -o perf/benchmark

perf-run: perf-build
	@echo "Running performance regression test..."
	@./scripts/run_perf.sh

perf-baseline: perf-build
	@echo "Creating new performance baseline..."
	@mkdir -p perf
	@LD_LIBRARY_PATH=lib:$$LD_LIBRARY_PATH ./perf/benchmark perf/baseline.json
	@echo "Baseline saved to perf/baseline.json"

perf-clean:
	@echo "Cleaning performance artifacts..."
	@rm -rf perf/benchmark perf/current.json

perf: perf-run

# Help
help:
	@echo "DSV4L2 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all             - Build core and runtime libraries (default)"
	@echo "  libs            - Same as all"
	@echo "  core            - Build core library only"
	@echo "  runtime         - Build runtime library only"
	@echo "  test            - Build test programs"
	@echo "  clean           - Remove build artifacts"
	@echo "  install         - Install to /usr/local"
	@echo "  coverage        - Build with coverage and run tests"
	@echo "  coverage-report - Generate HTML coverage report"
	@echo "  coverage-clean  - Remove coverage data files"
	@echo "  fuzz            - Build AFL fuzzing harness"
	@echo "  fuzz-run        - Run AFL fuzzing session"
	@echo "  fuzz-clean      - Remove fuzzing artifacts"
	@echo "  fuzz-ai         - Build AI-guided fuzzing harness"
	@echo "  fuzz-ai-run     - Run OpenVINO-accelerated distributed fuzzing"
	@echo "  fuzz-ai-analyze - Analyze crashes for exploitability"
	@echo "  fuzz-ai-clean   - Remove AI fuzzing artifacts"
	@echo "  perf            - Run performance regression tests"
	@echo "  perf-baseline   - Create new performance baseline"
	@echo "  perf-clean      - Remove performance artifacts"
	@echo ""
	@echo "Standard build:"
	@echo "  make"
	@echo ""
	@echo "DSLLVM build:"
	@echo "  make CC=dsclang DSLLVM=1 PROFILE=ops MISSION=operation_name"
	@echo ""
	@echo "TPM2 hardware build:"
	@echo "  make HAVE_TPM2=1"
	@echo ""
	@echo "Coverage analysis:"
	@echo "  make coverage-report"
	@echo ""
	@echo "Fuzzing with AFL:"
	@echo "  make fuzz-run"
	@echo ""
	@echo "AI-guided fuzzing with OpenVINO:"
	@echo "  make fuzz-ai-run"
	@echo ""
	@echo "Performance regression testing:"
	@echo "  make perf"
	@echo ""
	@echo "Variables:"
	@echo "  CC        - Compiler (default: gcc)"
	@echo "  DSLLVM    - Enable DSLLVM (0 or 1, default: 0)"
	@echo "  HAVE_TPM2 - Enable TPM2-TSS hardware support (0 or 1, default: 0)"
	@echo "  COVERAGE  - Enable gcov/lcov coverage (0 or 1, default: 0)"
	@echo "  PROFILE   - Instrumentation profile (off|ops|exercise|forensic, default: ops)"
	@echo "  MISSION   - Mission context tag (default: dev)"
