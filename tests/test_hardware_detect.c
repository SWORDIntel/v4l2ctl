/*
 * DSV4L2 Hardware Detection Test
 *
 * Tests interaction with real V4L2 hardware devices.
 * Gracefully skips tests when no hardware is available.
 */

#include "dsv4l2_dsmil.h"
#include "dsv4l2_profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

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

/**
 * Check if any V4L2 capture devices exist
 */
static int find_v4l2_device(char *path, size_t path_len)
{
    for (int i = 0; i < 10; i++) {
        snprintf(path, path_len, "/dev/video%d", i);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISCHR(st.st_mode)) {
            continue;
        }

        int fd = open(path, O_RDWR);
        if (fd < 0) {
            continue;
        }

        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            if (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
                close(fd);
                return 1;  /* Found capture device */
            }
        }

        close(fd);
    }

    return 0;  /* No devices */
}

/**
 * Test: Device detection
 */
static void test_device_detection(void)
{
    char device_path[32];

    printf("\n=== Test 1: V4L2 Device Detection ===\n");

    int found = find_v4l2_device(device_path, sizeof(device_path));

    if (found) {
        printf("    Found device: %s\n", device_path);
        TEST_ASSERT(1, "V4L2 capture device detected");
    } else {
        TEST_SKIP("No V4L2 devices available");
    }
}

/**
 * Test: Device capabilities query
 */
static void test_device_capabilities(void)
{
    char device_path[32];

    printf("\n=== Test 2: Device Capabilities ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    struct v4l2_capability cap;
    int rc = ioctl(fd, VIDIOC_QUERYCAP, &cap);

    TEST_ASSERT(rc == 0, "Query capabilities succeeded");

    if (rc == 0) {
        printf("    Driver: %s\n", cap.driver);
        printf("    Card: %s\n", cap.card);
        printf("    Bus: %s\n", cap.bus_info);

        TEST_ASSERT(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE,
                    "Device supports video capture");
        TEST_ASSERT(cap.capabilities & V4L2_CAP_STREAMING,
                    "Device supports streaming");
    }

    close(fd);
}

/**
 * Test: Format enumeration
 */
static void test_format_enumeration(void)
{
    char device_path[32];

    printf("\n=== Test 3: Format Enumeration ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int format_count = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        format_count++;
        fmt.index++;

        if (format_count == 1) {
            printf("    First format: %.4s - %s\n",
                   (char *)&fmt.pixelformat, fmt.description);
        }
    }

    TEST_ASSERT(format_count > 0, "Device supports at least one format");
    printf("    Total formats: %d\n", format_count);

    close(fd);
}

/**
 * Test: Get/Set format
 */
static void test_format_operations(void)
{
    char device_path[32];

    printf("\n=== Test 4: Format Get/Set ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    /* Get current format */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int rc = ioctl(fd, VIDIOC_G_FMT, &fmt);
    TEST_ASSERT(rc == 0, "Get format succeeded");

    if (rc == 0) {
        printf("    Current: %.4s %ux%u\n",
               (char *)&fmt.fmt.pix.pixelformat,
               fmt.fmt.pix.width,
               fmt.fmt.pix.height);

        /* Try setting a common format */
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

        rc = ioctl(fd, VIDIOC_S_FMT, &fmt);
        if (rc == 0) {
            TEST_ASSERT(1, "Set format succeeded (640x480 YUYV)");
        } else {
            /* Format may not be supported, not a failure */
            printf("    ⊘ YUYV 640x480 not supported\n");
            tests_skipped++;
        }
    }

    close(fd);
}

/**
 * Test: Buffer allocation
 */
static void test_buffer_allocation(void)
{
    char device_path[32];

    printf("\n=== Test 5: Buffer Allocation ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    /* Request buffers */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    int rc = ioctl(fd, VIDIOC_REQBUFS, &req);

    if (rc == 0) {
        TEST_ASSERT(1, "Buffer allocation succeeded");
        TEST_ASSERT(req.count >= 2, "Allocated at least 2 buffers");
        printf("    Allocated: %u buffers\n", req.count);
    } else {
        TEST_SKIP("Buffer allocation not supported");
    }

    close(fd);
}

/**
 * Test: DSV4L2 profile matching
 */
static void test_profile_matching(void)
{
    char device_path[32];

    printf("\n=== Test 6: Profile Matching ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
        close(fd);
        TEST_SKIP("Cannot query capabilities");
        return;
    }

    /* Check if device matches any profile */
    size_t profile_count = dsv4l2_get_profile_count();

    if (profile_count > 0) {
        printf("    Loaded %zu profiles\n", profile_count);

        int matched = 0;
        for (size_t i = 0; i < profile_count; i++) {
            const dsv4l2_device_profile_t *profile = dsv4l2_get_profile(i);
            if (profile == NULL) {
                continue;
            }

            /* Simple vendor/model matching */
            if (strstr((char *)cap.card, profile->vendor) ||
                strstr((char *)cap.card, profile->model)) {
                printf("    Matched profile: %s\n", profile->id);
                matched = 1;
                break;
            }
        }

        if (matched) {
            TEST_ASSERT(1, "Device matched a profile");
        } else {
            printf("    ⊘ No matching profile (generic device)\n");
            tests_skipped++;
        }
    } else {
        TEST_SKIP("No profiles available");
    }

    close(fd);
}

int main(void)
{
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║          DSV4L2 Hardware Detection Tests              ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    printf("\nNote: These tests require real V4L2 hardware.\n");
    printf("Tests will be skipped if no devices are available.\n");

    test_device_detection();
    test_device_capabilities();
    test_format_enumeration();
    test_format_operations();
    test_buffer_allocation();
    test_profile_matching();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║         Hardware Detection Test Summary               ║\n");
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
