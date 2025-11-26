# Makefile for libdsv4l2
# DSLLVM-enhanced sensor management library

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -fPIC -Iinclude -I../include
LDFLAGS = -pthread

# DSLLVM support (optional)
ifdef DSLLVM_ENABLED
    CC = dsclang
    DSLLVM_PLUGIN ?= /usr/lib/libDSLLVM.so
    DSLLVM_CONFIG ?= ../config/dsllvm_dsv4l2_passes.yaml
    CFLAGS += -fplugin=$(DSLLVM_PLUGIN) \
              -fplugin-arg-dsllvm-pass-config=$(DSLLVM_CONFIG) \
              -fdsllvm
endif

SRC_DIR = src
OBJ_DIR = obj
LIB_DIR = lib

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET = $(LIB_DIR)/libdsv4l2.a
SHARED_TARGET = $(LIB_DIR)/libdsv4l2.so

.PHONY: all clean install

all: $(TARGET) $(SHARED_TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $(OBJS)
	@echo "Built static library: $@"

$(SHARED_TARGET): $(OBJS) | $(LIB_DIR)
	$(CC) -shared $(LDFLAGS) -o $@ $(OBJS)
	@echo "Built shared library: $@"

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR)

install: $(TARGET) $(SHARED_TARGET)
	install -d /usr/local/lib
	install -m 644 $(TARGET) /usr/local/lib/
	install -m 755 $(SHARED_TARGET) /usr/local/lib/
	install -d /usr/local/include/dsv4l2
	install -m 644 include/*.h /usr/local/include/dsv4l2/
	ldconfig

# Build with DSLLVM
dsllvm:
	@$(MAKE) DSLLVM_ENABLED=1

# Show build configuration
config:
	@echo "CC       = $(CC)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "SRCS     = $(SRCS)"
	@echo "OBJS     = $(OBJS)"
	@echo "TARGET   = $(TARGET)"
