# DSV4L2 Tests Makefile

CC ?= gcc
HAVE_TPM2 ?= 0
COVERAGE ?= 0

CFLAGS = -Wall -Wextra -O2 -g
CFLAGS += -I../include
LDFLAGS = -L../lib -ldsv4l2 -ldsv4l2rt -lpthread

# TPM2 support
ifeq ($(HAVE_TPM2),1)
    CFLAGS += -DHAVE_TPM2
    LDFLAGS += -ltss2-esys -ltss2-rc -ltss2-mu -lcrypto
endif

# Coverage support
ifeq ($(COVERAGE),1)
    CFLAGS += --coverage -fprofile-arcs -ftest-coverage
    LDFLAGS += --coverage -lgcov
endif

# Test programs
TESTS = test_basic test_profiles test_policy test_metadata test_runtime test_integration test_tpm test_hardware_detect

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

test_metadata: test_metadata.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -lm -o $@

test_runtime: test_runtime.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

test_integration: test_integration.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

test_tpm: test_tpm.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

test_hardware_detect: test_hardware_detect.c
	@echo "CC $@"
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
