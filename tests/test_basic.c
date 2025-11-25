/*
 * DSV4L2 Basic Test Program
 *
 * Tests core functionality:
 * - Device enumeration
 * - Device open/close
 * - TEMPEST state management
 * - Format querying
 * - Frame capture (if device available)
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* Function declarations from core library */
extern int dsv4l2_open(const char *path, const char *role, dsv4l2_device_t **out);
extern void dsv4l2_close(dsv4l2_device_t *dev);
extern int dsv4l2_list_devices(dsv4l2_device_t ***devices, size_t *count);
extern int dsv4l2_get_info(dsv4l2_device_t *dev,
                            char *driver, size_t driver_len,
                            char *card, size_t card_len,
                            char *bus, size_t bus_len);
extern int dsv4l2_enum_formats(dsv4l2_device_t *dev, uint32_t **formats, size_t *count);
extern int dsv4l2_get_resolution(dsv4l2_device_t *dev, uint32_t *width, uint32_t *height);
extern int dsv4l2_request_buffers(dsv4l2_device_t *dev, uint32_t count);
extern int dsv4l2_mmap_buffers(dsv4l2_device_t *dev);
extern int dsv4l2_queue_buffer(dsv4l2_device_t *dev, uint32_t index);
extern void dsv4l2_fourcc_to_string(uint32_t fourcc, char *str);
extern const char *dsv4l2_tempest_state_name(dsv4l2_tempest_state_t state);

int main(int argc, char **argv)
{
    dsv4l2_device_t **devices = NULL;
    dsv4l2_device_t *dev = NULL;
    size_t dev_count = 0;
    int rc;
    dsv4l2rt_config_t rt_config;

    printf("DSV4L2 Basic Test Program\n");
    printf("==========================\n\n");

    /* Initialize runtime with exercise profile */
    memset(&rt_config, 0, sizeof(rt_config));
    rt_config.profile = DSV4L2_PROFILE_EXERCISE;
    rt_config.mission = "test";
    dsv4l2rt_init(&rt_config);

    printf("Runtime initialized (profile: EXERCISE)\n\n");

    /* Test 1: List devices */
    printf("Test 1: Enumerating devices...\n");
    rc = dsv4l2_list_devices(&devices, &dev_count);
    if (rc < 0) {
        fprintf(stderr, "ERROR: Failed to list devices: %d\n", rc);
        return 1;
    }

    printf("Found %zu video device(s)\n\n", dev_count);

    if (dev_count == 0) {
        printf("No devices found. Tests requiring a device will be skipped.\n");
        free(devices);
        dsv4l2rt_shutdown();
        return 0;
    }

    /* Test 2: Device info */
    printf("Test 2: Device information\n");
    for (size_t i = 0; i < dev_count; i++) {
        char driver[32], card[64], bus[64];

        dsv4l2_get_info(devices[i], driver, sizeof(driver),
                        card, sizeof(card), bus, sizeof(bus));

        printf("  Device %zu:\n", i);
        printf("    Path:   %s\n", devices[i]->dev_path);
        printf("    Driver: %s\n", driver);
        printf("    Card:   %s\n", card);
        printf("    Bus:    %s\n", bus);
        printf("    Role:   %s\n", devices[i]->role);
        printf("    Layer:  L%u\n", devices[i]->layer);
        printf("\n");
    }

    /* Use first device for remaining tests */
    dev = devices[0];

    /* Test 3: TEMPEST state */
    printf("Test 3: TEMPEST state management\n");
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    printf("  Current TEMPEST state: %s\n", dsv4l2_tempest_state_name(state));

    /* Try to set TEMPEST state (may fail if hardware doesn't support it) */
    printf("  Attempting to set TEMPEST to LOW...\n");
    rc = dsv4l2_set_tempest_state(dev, DSV4L2_TEMPEST_LOW);
    if (rc == 0) {
        printf("  Success! TEMPEST set to LOW\n");
        state = dsv4l2_get_tempest_state(dev);
        printf("  Verified state: %s\n", dsv4l2_tempest_state_name(state));
    } else if (rc == -ENOTSUP) {
        printf("  Device does not support TEMPEST control (expected for most webcams)\n");
    } else {
        printf("  Failed: %d\n", rc);
    }
    printf("\n");

    /* Test 4: Format enumeration */
    printf("Test 4: Format enumeration\n");
    uint32_t *formats = NULL;
    size_t fmt_count = 0;
    rc = dsv4l2_enum_formats(dev, &formats, &fmt_count);
    if (rc == 0) {
        printf("  Supported formats (%zu):\n", fmt_count);
        for (size_t i = 0; i < fmt_count; i++) {
            char fourcc[5];
            dsv4l2_fourcc_to_string(formats[i], fourcc);
            printf("    %zu. %s (0x%08x)\n", i + 1, fourcc, formats[i]);
        }
        free(formats);
    } else {
        printf("  Failed to enumerate formats: %d\n", rc);
    }
    printf("\n");

    /* Test 5: Resolution query */
    printf("Test 5: Current resolution\n");
    uint32_t width, height;
    rc = dsv4l2_get_resolution(dev, &width, &height);
    if (rc == 0) {
        printf("  Resolution: %ux%u\n", width, height);
    } else {
        printf("  Failed to get resolution: %d\n", rc);
    }
    printf("\n");

    /* Test 6: Buffer setup */
    printf("Test 6: Buffer management\n");
    rc = dsv4l2_request_buffers(dev, 4);
    if (rc == 0) {
        printf("  Requested 4 buffers: SUCCESS\n");

        rc = dsv4l2_mmap_buffers(dev);
        if (rc == 0) {
            printf("  Mapped buffers: SUCCESS\n");

            /* Queue all buffers */
            for (uint32_t i = 0; i < 4; i++) {
                dsv4l2_queue_buffer(dev, i);
            }
            printf("  Queued 4 buffers: SUCCESS\n");
        } else {
            printf("  Failed to map buffers: %d\n", rc);
        }
    } else {
        printf("  Failed to request buffers: %d\n", rc);
    }
    printf("\n");

    /* Test 7: Single frame capture */
    printf("Test 7: Frame capture\n");
    dsv4l2_frame_t frame;
    rc = dsv4l2_capture_frame(dev, &frame);
    if (rc == 0) {
        printf("  Captured frame: %zu bytes\n", frame.len);
        printf("  Frame data pointer: %p\n", (void *)frame.data);
    } else {
        printf("  Failed to capture frame: %d\n", rc);
        if (rc == -EPERM) {
            printf("  (Policy violation - check TEMPEST state)\n");
        }
    }
    printf("\n");

    /* Test 8: Runtime statistics */
    printf("Test 8: Runtime statistics\n");
    dsv4l2rt_stats_t stats;
    dsv4l2rt_get_stats(&stats);
    printf("  Events emitted: %lu\n", stats.events_emitted);
    printf("  Events dropped: %lu\n", stats.events_dropped);
    printf("  Events flushed: %lu\n", stats.events_flushed);
    printf("\n");

    /* Cleanup */
    printf("Cleaning up...\n");
    for (size_t i = 0; i < dev_count; i++) {
        dsv4l2_close(devices[i]);
    }
    free(devices);

    dsv4l2rt_shutdown();

    printf("\nAll tests completed!\n");

    return 0;
}
