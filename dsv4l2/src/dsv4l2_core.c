/*
 * dsv4l2_core.c
 *
 * Core device management for DSV4L2
 * Stub implementation - to be completed in Phase 2
 */

#include "dsv4l2_core.h"
#include "dsv4l2_tempest.h"
#include "dsv4l2rt.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

int dsv4l2_open_device(
    const char *device_path,
    const dsv4l2_profile_t *profile,
    dsv4l2_device_t **out_dev
) {
    if (!device_path || !out_dev) {
        return -EINVAL;
    }

    /* Allocate device handle */
    dsv4l2_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return -ENOMEM;
    }

    /* Open device */
    dev->fd = open(device_path, O_RDWR | O_NONBLOCK);
    if (dev->fd < 0) {
        free(dev);
        return -errno;
    }

    strncpy(dev->dev_path, device_path, sizeof(dev->dev_path) - 1);
    dev->tempest_state = DSV4L2_TEMPEST_DISABLED;

    /* Apply profile if provided */
    if (profile) {
        dev->profile = malloc(sizeof(*profile));
        if (dev->profile) {
            memcpy(dev->profile, profile, sizeof(*profile));
            /* TODO: Apply profile settings in Phase 2 */
        }
    }

    *out_dev = dev;
    return 0;
}

void dsv4l2_close_device(dsv4l2_device_t *dev) {
    if (!dev) {
        return;
    }

    if (dev->streaming) {
        dsv4l2_stop_stream(dev);
    }

    if (dev->fd >= 0) {
        close(dev->fd);
    }

    if (dev->profile) {
        free(dev->profile);
    }

    free(dev);
}

int dsv4l2_capture_frame(
    dsv4l2_device_t *dev,
    dsv4l2_frame_t *out_frame
) {
    if (!dev || !out_frame) {
        return -EINVAL;
    }

    /* TEMPEST policy check (required by DSLLVM) */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    int policy_rc = dsv4l2_policy_check_capture(dev, state, "dsv4l2_capture_frame");
    if (policy_rc != 0) {
        return -EACCES;
    }

    /* TODO: Implement actual capture in Phase 2 */
    /* For now, return not implemented */
    return -ENOSYS;
}

int dsv4l2_capture_iris(
    dsv4l2_device_t *dev,
    dsv4l2_biometric_frame_t *out_frame
) {
    if (!dev || !out_frame) {
        return -EINVAL;
    }

    /* TEMPEST policy check */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    int policy_rc = dsv4l2_policy_check_capture(dev, state, "dsv4l2_capture_iris");
    if (policy_rc != 0) {
        return -EACCES;
    }

    /* TODO: Implement iris capture in Phase 2 */
    return -ENOSYS;
}

int dsv4l2_start_stream(dsv4l2_device_t *dev) {
    if (!dev) {
        return -EINVAL;
    }

    /* TODO: Implement streaming in Phase 2 */
    dev->streaming = 1;
    return 0;
}

int dsv4l2_stop_stream(dsv4l2_device_t *dev) {
    if (!dev) {
        return -EINVAL;
    }

    /* TODO: Implement streaming in Phase 2 */
    dev->streaming = 0;
    return 0;
}

int dsv4l2_set_format(
    dsv4l2_device_t *dev,
    uint32_t pixel_format,
    uint32_t width,
    uint32_t height
) {
    if (!dev) {
        return -EINVAL;
    }

    /* TODO: Implement format setting in Phase 2 */
    dsv4l2rt_log_format_change((uint32_t)(uintptr_t)dev, pixel_format, width, height);
    return -ENOSYS;
}

int dsv4l2_set_framerate(
    dsv4l2_device_t *dev,
    uint32_t fps_num,
    uint32_t fps_den
) {
    if (!dev) {
        return -EINVAL;
    }

    /* TODO: Implement framerate setting in Phase 2 */
    return -ENOSYS;
}

int dsv4l2_get_info(
    dsv4l2_device_t *dev,
    char *driver,
    char *card,
    char *bus_info
) {
    if (!dev) {
        return -EINVAL;
    }

    struct v4l2_capability cap;
    if (ioctl(dev->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        return -errno;
    }

    if (driver) {
        strncpy(driver, (char*)cap.driver, 16);
    }
    if (card) {
        strncpy(card, (char*)cap.card, 32);
    }
    if (bus_info) {
        strncpy(bus_info, (char*)cap.bus_info, 32);
    }

    return 0;
}
