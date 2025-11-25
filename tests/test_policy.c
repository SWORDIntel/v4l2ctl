/*
 * DSV4L2 Policy Enforcement Tests
 *
 * Test THREATCON mapping, clearance checking, and layer policies
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2_profiles.h"
#include "dsv4l2_dsmil.h"
#include "dsv4l2rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        tests_failed++; \
    } \
} while (0)

/**
 * Test THREATCON to TEMPEST mapping
 */
static void test_threatcon_mapping(void)
{
    dsmil_threatcon_t level;
    const char *name;

    printf("\n=== Testing THREATCON Mapping ===\n");

    /* Test THREATCON names */
    level = THREATCON_NORMAL;
    name = dsv4l2_threatcon_name(level);
    TEST_ASSERT(strcmp(name, "NORMAL") == 0, "THREATCON_NORMAL name");

    level = THREATCON_EMERGENCY;
    name = dsv4l2_threatcon_name(level);
    TEST_ASSERT(strcmp(name, "EMERGENCY") == 0, "THREATCON_EMERGENCY name");

    /* Test THREATCON get/set */
    dsv4l2_policy_init();

    dsv4l2_set_threatcon(THREATCON_CHARLIE);
    level = dsv4l2_get_threatcon();
    TEST_ASSERT(level == THREATCON_CHARLIE, "Set/Get THREATCON CHARLIE");

    dsv4l2_set_threatcon(THREATCON_NORMAL);
    level = dsv4l2_get_threatcon();
    TEST_ASSERT(level == THREATCON_NORMAL, "Reset to THREATCON NORMAL");
}

/**
 * Test clearance checking
 */
static void test_clearance_checking(void)
{
    int rc;

    printf("\n=== Testing Clearance Checking ===\n");

    /* Test UNCLASSIFIED access (should always succeed) */
    rc = dsv4l2_check_clearance("generic_webcam", "UNCLASSIFIED");
    TEST_ASSERT(rc == 0, "UNCLASSIFIED access allowed");

    /* Test SECRET access without clearance (should fail) */
    unsetenv("DSV4L2_CLEARANCE");  /* Clear environment */
    rc = dsv4l2_check_clearance("iris_scanner", "SECRET_BIOMETRIC");
    TEST_ASSERT(rc == -EPERM, "SECRET access denied without clearance");

    /* Test SECRET access with clearance (should succeed) */
    setenv("DSV4L2_CLEARANCE", "SECRET", 1);

    /* Note: Need to restart process for clearance cache to refresh
     * In real test, would use fork() or clear cache */
    printf("  [INFO] In production, clearance would be verified at process start\n");

    /* Test role-based clearance requirements */
    unsetenv("DSV4L2_CLEARANCE");
    rc = dsv4l2_check_clearance("ir_sensor", "UNCLASSIFIED");
    TEST_ASSERT(rc == -EPERM, "IR sensor requires CONFIDENTIAL clearance");

    rc = dsv4l2_check_clearance("tempest_cam", "UNCLASSIFIED");
    TEST_ASSERT(rc == -EPERM, "TEMPEST camera requires TOP_SECRET clearance");
}

/**
 * Test layer policy enforcement
 */
static void test_layer_policies(void)
{
    int rc;
    dsv4l2_layer_policy_t *policy;

    printf("\n=== Testing Layer Policies ===\n");

    /* Test getting layer policies */
    rc = dsv4l2_get_layer_policy(3, &policy);
    TEST_ASSERT(rc == 0, "Get L3 layer policy");
    if (rc == 0) {
        TEST_ASSERT(policy->layer == 3, "L3 policy layer number");
        TEST_ASSERT(policy->max_width == 1280, "L3 max width 1280");
        TEST_ASSERT(policy->max_height == 720, "L3 max height 720");
    }

    rc = dsv4l2_get_layer_policy(7, &policy);
    TEST_ASSERT(rc == 0, "Get L7 layer policy");
    if (rc == 0) {
        TEST_ASSERT(policy->layer == 7, "L7 policy layer number");
        TEST_ASSERT(policy->max_width == 3840, "L7 max width 3840");
        TEST_ASSERT(policy->min_tempest == DSV4L2_TEMPEST_HIGH, "L7 requires TEMPEST HIGH");
    }

    /* Test invalid layer */
    rc = dsv4l2_get_layer_policy(99, &policy);
    TEST_ASSERT(rc == -EINVAL, "Invalid layer returns EINVAL");
}

/**
 * Test capture authorization
 */
static void test_capture_authorization(void)
{
    printf("\n=== Testing Capture Authorization ===\n");

    /* Note: Would need actual device handles for full testing
     * These are placeholder tests for the API */

    printf("  [INFO] Capture authorization requires device handles\n");
    printf("  [INFO] Full testing requires hardware or mock devices\n");

    /* Test LOCKDOWN blocking */
    dsv4l2_set_threatcon(THREATCON_EMERGENCY);
    printf("  [INFO] THREATCON EMERGENCY -> LOCKDOWN would block all capture\n");

    dsv4l2_set_threatcon(THREATCON_NORMAL);
}

/**
 * Test profile loading with security metadata
 */
static void test_profile_security(void)
{
    const dsv4l2_device_profile_t *profile;

    printf("\n=== Testing Profile Security Metadata ===\n");

    /* Test iris scanner profile (SECRET) */
    profile = dsv4l2_find_profile_by_role("iris_scanner");
    TEST_ASSERT(profile != NULL, "Find iris_scanner profile");
    if (profile) {
        TEST_ASSERT(strstr(profile->classification, "SECRET") != NULL,
                    "Iris scanner has SECRET classification");
        TEST_ASSERT(profile->layer == 3, "Iris scanner on L3");
    }

    /* Test generic webcam profile (UNCLASSIFIED) */
    profile = dsv4l2_find_profile_by_role("generic_webcam");
    TEST_ASSERT(profile != NULL, "Find generic_webcam profile");
    if (profile) {
        TEST_ASSERT(strcmp(profile->classification, "UNCLASSIFIED") == 0,
                    "Generic webcam is UNCLASSIFIED");
    }

    /* Test tempest camera profile (TOP_SECRET) */
    profile = dsv4l2_find_profile_by_role("tempest_cam");
    TEST_ASSERT(profile != NULL, "Find tempest_cam profile");
    if (profile) {
        TEST_ASSERT(strstr(profile->classification, "TOP_SECRET") != NULL,
                    "TEMPEST camera has TOP_SECRET classification");
    }
}

/**
 * Main test runner
 */
int main(void)
{
    printf("DSV4L2 Policy Enforcement Tests\n");
    printf("================================\n");

    /* Initialize runtime in verbose mode */
    setenv("DSV4L2_PROFILE", "exercise", 1);

    /* Run test suites */
    test_threatcon_mapping();
    test_clearance_checking();
    test_layer_policies();
    test_capture_authorization();
    test_profile_security();

    /* Print summary */
    printf("\n================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    }

    printf("\nAll tests PASSED!\n");
    return 0;
}
