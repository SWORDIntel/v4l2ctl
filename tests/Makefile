# DSV4L2 Tests Makefile

CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -g
CFLAGS += -I../include
LDFLAGS = -L../lib -ldsv4l2 -ldsv4l2rt -lpthread

# Test programs
TESTS = test_basic test_profiles test_policy

.PHONY: all clean

all: $(TESTS)

test_basic: test_basic.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	@rm -f $(TESTS)

# Run tests
run: all
	@echo "Running tests..."
	@./test_basic

.PHONY: run

test_profiles: test_profiles.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

test_policy: test_policy.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
