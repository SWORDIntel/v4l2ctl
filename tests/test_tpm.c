/*
 * DSV4L2 TPM Integration Test
 *
 * Tests TPM2 hardware signing and verification.
 * When HAVE_TPM2=0, tests should return -ENOSYS.
 * When HAVE_TPM2=1 with hardware, tests should perform real signing.
 */

#include "dsv4l2rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  ✓ %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  ✗ %s\n", msg); \
        tests_failed++; \
    } \
} while (0)

#define TEST_SKIP(msg) do { \
    printf("  ⊘ %s (skipped)\n", msg); \
    tests_skipped++; \
} while (0)

static void test_tpm_init(void)
{
    int rc;

    printf("\n=== Test 1: TPM Initialization ===\n");

    /* Test TPM init with default key */
    rc = dsv4l2_tpm_init(0x81010001);

#ifdef HAVE_TPM2
    if (rc == 0) {
        TEST_ASSERT(1, "TPM2 initialized successfully");
    } else {
        printf("  ⊘ TPM2 hardware not available (rc=%d)\n", rc);
        tests_skipped++;
    }
#else
    TEST_ASSERT(rc == -ENOSYS, "TPM2 returns ENOSYS when not compiled");
#endif

    /* Cleanup */
    dsv4l2_tpm_cleanup();
}

static void test_tpm_signing(void)
{
    dsv4l2_event_t events[10];
    uint8_t signature[256];
    int rc, i;

    printf("\n=== Test 2: TPM Signing ===\n");

    /* Create sample events */
    for (i = 0; i < 10; i++) {
        events[i].ts_ns = 1000000000ULL * i;
        events[i].dev_id = 0x12345678;
        events[i].event_type = DSV4L2_EVENT_CAPTURE_START;
        events[i].severity = DSV4L2_SEV_INFO;
        events[i].aux = i;
        events[i].layer = 0;
        snprintf(events[i].role, sizeof(events[i].role), "test");
        snprintf(events[i].mission, sizeof(events[i].mission), "tpm_test");
    }

    /* Test signing */
    rc = dsv4l2_tpm_sign_events(events, 10, signature);

#ifdef HAVE_TPM2
    if (rc == 0) {
        TEST_ASSERT(1, "TPM2 signing succeeded");

        /* Check that signature is not all zeros */
        int all_zero = 1;
        for (i = 0; i < 256; i++) {
            if (signature[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        TEST_ASSERT(!all_zero, "Signature contains non-zero data");
    } else if (rc == -EIO || rc == -ENOENT) {
        TEST_SKIP("TPM2 hardware not available");
    } else {
        TEST_ASSERT(0, "Unexpected error from TPM2 signing");
    }
#else
    TEST_ASSERT(rc == -ENOSYS, "TPM2 signing returns ENOSYS when not compiled");
#endif
}

static void test_tpm_verification(void)
{
    dsv4l2_event_t events[10];
    uint8_t signature[256];
    int rc, i;

    printf("\n=== Test 3: TPM Signature Verification ===\n");

    /* Create sample events */
    for (i = 0; i < 10; i++) {
        events[i].ts_ns = 1000000000ULL * i;
        events[i].dev_id = 0x12345678;
        events[i].event_type = DSV4L2_EVENT_CAPTURE_START;
        events[i].severity = DSV4L2_SEV_INFO;
        events[i].aux = i;
        events[i].layer = 0;
        snprintf(events[i].role, sizeof(events[i].role), "test");
        snprintf(events[i].mission, sizeof(events[i].mission), "tpm_test");
    }

    /* Sign events */
    rc = dsv4l2_tpm_sign_events(events, 10, signature);
    if (rc != 0) {
        TEST_SKIP("Cannot verify - signing failed");
        return;
    }

    /* Verify signature */
    rc = dsv4l2_tpm_verify_signature(events, 10, signature);

#ifdef HAVE_TPM2
    if (rc == 0) {
        TEST_ASSERT(1, "Valid signature verified successfully");
    } else if (rc == -EIO || rc == -ENOENT) {
        TEST_SKIP("TPM2 hardware not available");
    } else {
        TEST_ASSERT(0, "Signature verification failed unexpectedly");
    }

    /* Test invalid signature detection */
    signature[0] ^= 0xFF;  /* Corrupt signature */
    rc = dsv4l2_tpm_verify_signature(events, 10, signature);
    TEST_ASSERT(rc == -EBADMSG, "Corrupted signature detected");
#else
    TEST_ASSERT(rc == -ENOSYS, "TPM2 verification returns ENOSYS when not compiled");
#endif
}

static void test_runtime_integration(void)
{
    dsv4l2rt_config_t config = {
        .profile = DSV4L2_PROFILE_OPS,
        .mission = "tpm_test",
        .ring_buffer_size = 256,
        .enable_tpm_sign = 1,
        .sink_type = NULL,
        .sink_config = NULL
    };
    dsv4l2rt_chunk_header_t header;
    dsv4l2_event_t *events;
    size_t count;
    int rc, i;

    printf("\n=== Test 4: Runtime Integration with TPM ===\n");

    /* Initialize runtime with TPM enabled */
    rc = dsv4l2rt_init(&config);
    TEST_ASSERT(rc == 0, "Runtime initialized with TPM enabled");

    /* Emit some events */
    for (i = 0; i < 10; i++) {
        dsv4l2rt_emit_simple(0x12345678, DSV4L2_EVENT_CAPTURE_START, DSV4L2_SEV_INFO, i);
    }

    /* Get signed chunk */
    rc = dsv4l2rt_get_signed_chunk(&header, &events, &count);
    TEST_ASSERT(rc == 0, "Retrieved signed event chunk");
    TEST_ASSERT(count == 10, "Correct event count in chunk");

#ifdef HAVE_TPM2
    /* Verify the signature if TPM is available */
    rc = dsv4l2_tpm_verify_signature(events, count, header.tpm_signature);
    if (rc == 0) {
        TEST_ASSERT(1, "Chunk signature verified successfully");
    } else if (rc == -ENOSYS || rc == -EIO) {
        TEST_SKIP("TPM2 hardware not available for verification");
    } else {
        TEST_ASSERT(0, "Chunk signature verification failed");
    }
#else
    printf("  ⊘ Signature verification skipped (TPM2 not compiled)\n");
    tests_skipped++;
#endif

    free(events);
    dsv4l2rt_shutdown();
}

int main(void)
{
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║            DSV4L2 TPM Integration Tests               ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

#ifdef HAVE_TPM2
    printf("\nTPM2 Support: ✓ ENABLED (Hardware required for full tests)\n");
#else
    printf("\nTPM2 Support: ✗ DISABLED (Fallback mode - tests will verify ENOSYS)\n");
#endif

    test_tpm_init();
    test_tpm_signing();
    test_tpm_verification();
    test_runtime_integration();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║              TPM Integration Test Summary             ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  Total Tests:   %d\n", tests_passed + tests_failed + tests_skipped);
    printf("  ✓ Passed:      %d\n", tests_passed);
    printf("  ✗ Failed:      %d\n", tests_failed);
    printf("  ⊘ Skipped:     %d\n", tests_skipped);
    printf("\n");

    if (tests_failed == 0) {
        printf("  Status: ✓ ALL TESTS PASSED\n");
    } else {
        printf("  Status: ✗ SOME TESTS FAILED\n");
    }
    printf("\n");

    return (tests_failed > 0) ? 1 : 0;
}
