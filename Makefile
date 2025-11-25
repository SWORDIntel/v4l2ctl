# DSV4L2 Makefile
# DSLLVM-instrumented sensor stack build system

# Compiler selection
CC ?= gcc
# For DSLLVM build, use: make CC=dsclang DSLLVM=1
DSLLVM ?= 0

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

RUNTIME_SRCS = $(SRC_DIR)/runtime/event_buffer.c

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

# CLI tool
CLI_BIN = bin/dsv4l2
CLI_SRC = $(SRC_DIR)/cli/main.c

# Targets
.PHONY: all clean libs core runtime test install cli

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

# Help
help:
	@echo "DSV4L2 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build core and runtime libraries (default)"
	@echo "  libs       - Same as all"
	@echo "  core       - Build core library only"
	@echo "  runtime    - Build runtime library only"
	@echo "  test       - Build test programs"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install to /usr/local"
	@echo ""
	@echo "Standard build:"
	@echo "  make"
	@echo ""
	@echo "DSLLVM build:"
	@echo "  make CC=dsclang DSLLVM=1 PROFILE=ops MISSION=operation_name"
	@echo ""
	@echo "Variables:"
	@echo "  CC       - Compiler (default: gcc)"
	@echo "  DSLLVM   - Enable DSLLVM (0 or 1, default: 0)"
	@echo "  PROFILE  - Instrumentation profile (off|ops|exercise|forensic, default: ops)"
	@echo "  MISSION  - Mission context tag (default: dev)"
