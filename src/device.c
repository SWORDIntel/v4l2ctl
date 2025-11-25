/*
 * DSV4L2 Device Management
 *
 * DSLLVM-annotated v4l2 device wrapper providing:
 * - Device open/close with role awareness
 * - Device enumeration and capability querying
 * - Profile loading and device classification
 * - Telemetry integration
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"
#include "dsv4l2_profiles.h"
#include "dsv4l2_dsmil.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

/* Internal device structure (extends public dsv4l2_device_t) */
typedef struct dsv4l2_device_internal {
    dsv4l2_device_t public;          /* Public device handle */

    /* Internal state */
    struct v4l2_capability cap;      /* Device capabilities */
    dsv4l2_tempest_state_t tempest;  /* Current TEMPEST state */
    int tempest_ctrl_id;             /* v4l2 control ID for TEMPEST */

    /* Profile information */
    char *profile_path;              /* Path to loaded profile */
    char *classification;            /* Security classification */

    /* Runtime state */
    int streaming;                   /* 1 if streaming active */
    uint32_t dev_id;                 /* Device ID (hash) */
} dsv4l2_device_internal_t;

/* Forward declarations */
static uint32_t hash_device_path(const char *path);
static int load_device_profile(const char *path, const char *role,
                                dsv4l2_device_internal_t *dev);

/**
 * Open a v4l2 device with role classification
 *
 * @param path Device path (e.g. "/dev/video0")
 * @param role Device role (e.g. "camera", "iris_scanner")
 * @param out Output device handle
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("generic", "L3", "UNCLASSIFIED")
int dsv4l2_open(const char *path, const char *role, dsv4l2_device_t **out)
{
    int rc;
    dsv4l2_device_internal_t *dev = NULL;

    if (!path || !role || !out) {
        return -EINVAL;
    }

    /* Allocate device structure */
    dev = calloc(1, sizeof(dsv4l2_device_internal_t));
    if (!dev) {
        return -ENOMEM;
    }

    /* Open device */
    dev->public.fd = open(path, O_RDWR | O_NONBLOCK);
    if (dev->public.fd < 0) {
        rc = -errno;
        free(dev);
        return rc;
    }

    /* Store device info */
    dev->public.dev_path = strdup(path);
    dev->public.role = strdup(role);
    dev->public.layer = 3;  /* L3 = sensor/device layer */
    dev->dev_id = hash_device_path(path);

    /* Query capabilities */
    if (ioctl(dev->public.fd, VIDIOC_QUERYCAP, &dev->cap) < 0) {
        rc = -errno;
        close(dev->public.fd);
        free((void *)dev->public.dev_path);
        free((void *)dev->public.role);
        free(dev);
        return rc;
    }

    /* Verify this is a video capture device */
    if (!(dev->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        close(dev->public.fd);
        free((void *)dev->public.dev_path);
        free((void *)dev->public.role);
        free(dev);
        return -ENODEV;
    }

    /* Load device profile (if available) */
    load_device_profile(path, role, dev);

    /* Check clearance - must have sufficient clearance for device */
    rc = dsv4l2_check_clearance(role, dev->classification ? dev->classification : "UNCLASSIFIED");
    if (rc != 0) {
        /* Access denied - insufficient clearance */
        dsv4l2rt_emit_simple(dev->dev_id, DSV4L2_EVENT_POLICY_VIOLATION,
                             DSV4L2_SEV_CRITICAL, 0);
        close(dev->public.fd);
        free((void *)dev->public.dev_path);
        free((void *)dev->public.role);
        free(dev->classification);
        free(dev->profile_path);
        free(dev);
        return -EPERM;
    }

    /* Initialize TEMPEST state to DISABLED */
    dev->tempest = DSV4L2_TEMPEST_DISABLED;
    dev->tempest_ctrl_id = 0x9a0902;  /* Default control ID */

    /* Emit device open event */
    dsv4l2rt_emit_simple(dev->dev_id, DSV4L2_EVENT_DEVICE_OPEN,
                         DSV4L2_SEV_INFO, 0);

    *out = &dev->public;
    return 0;
}

/**
 * Close a v4l2 device
 *
 * @param dev Device handle
 */
void dsv4l2_close(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;

    if (!dev) {
        return;
    }

    internal = (dsv4l2_device_internal_t *)dev;

    /* Emit device close event */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_DEVICE_CLOSE,
                         DSV4L2_SEV_INFO, 0);

    /* Close file descriptor */
    if (dev->fd >= 0) {
        close(dev->fd);
    }

    /* Free allocated memory */
    free((void *)dev->dev_path);
    free((void *)dev->role);
    free(internal->profile_path);
    free(internal->classification);
    free(internal);
}

/**
 * List all v4l2 devices on the system
 *
 * @param devices Output array of device handles (caller must free)
 * @param count Output device count
 * @return 0 on success, negative errno on error
 */
int dsv4l2_list_devices(dsv4l2_device_t ***devices, size_t *count)
{
    DIR *dir;
    struct dirent *entry;
    dsv4l2_device_t **dev_list = NULL;
    size_t dev_count = 0;
    size_t dev_capacity = 16;
    int rc;

    if (!devices || !count) {
        return -EINVAL;
    }

    /* Allocate initial device list */
    dev_list = calloc(dev_capacity, sizeof(dsv4l2_device_t *));
    if (!dev_list) {
        return -ENOMEM;
    }

    /* Scan /dev for video devices */
    dir = opendir("/dev");
    if (!dir) {
        free(dev_list);
        return -errno;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[256];
        struct stat st;

        /* Check if this is a video device */
        if (strncmp(entry->d_name, "video", 5) != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);

        /* Verify it's a character device */
        if (stat(path, &st) < 0 || !S_ISCHR(st.st_mode)) {
            continue;
        }

        /* Try to open it */
        rc = dsv4l2_open(path, "camera", &dev_list[dev_count]);
        if (rc == 0) {
            dev_count++;

            /* Expand array if needed */
            if (dev_count >= dev_capacity) {
                dev_capacity *= 2;
                dev_list = realloc(dev_list, dev_capacity * sizeof(dsv4l2_device_t *));
                if (!dev_list) {
                    closedir(dir);
                    return -ENOMEM;
                }
            }
        }
    }

    closedir(dir);

    *devices = dev_list;
    *count = dev_count;
    return 0;
}

/**
 * Get device capabilities
 *
 * @param dev Device handle
 * @param cap Output capabilities structure
 * @return 0 on success, negative errno on error
 */
int dsv4l2_get_capabilities(dsv4l2_device_t *dev, struct v4l2_capability *cap)
{
    dsv4l2_device_internal_t *internal;

    if (!dev || !cap) {
        return -EINVAL;
    }

    internal = (dsv4l2_device_internal_t *)dev;
    memcpy(cap, &internal->cap, sizeof(*cap));

    return 0;
}

/**
 * Get device information (driver, card, bus)
 *
 * @param dev Device handle
 * @param driver Output buffer for driver name (optional)
 * @param card Output buffer for card name (optional)
 * @param bus Output buffer for bus info (optional)
 * @return 0 on success
 */
int dsv4l2_get_info(dsv4l2_device_t *dev,
                    char *driver, size_t driver_len,
                    char *card, size_t card_len,
                    char *bus, size_t bus_len)
{
    dsv4l2_device_internal_t *internal;

    if (!dev) {
        return -EINVAL;
    }

    internal = (dsv4l2_device_internal_t *)dev;

    if (driver && driver_len > 0) {
        strncpy(driver, (const char *)internal->cap.driver, driver_len - 1);
        driver[driver_len - 1] = '\0';
    }

    if (card && card_len > 0) {
        strncpy(card, (const char *)internal->cap.card, card_len - 1);
        card[card_len - 1] = '\0';
    }

    if (bus && bus_len > 0) {
        strncpy(bus, (const char *)internal->cap.bus_info, bus_len - 1);
        bus[bus_len - 1] = '\0';
    }

    return 0;
}

/**
 * Get internal device structure (for internal use by other modules)
 *
 * @param dev Public device handle
 * @return Internal device structure
 */
dsv4l2_device_internal_t *dsv4l2_get_internal(dsv4l2_device_t *dev)
{
    return (dsv4l2_device_internal_t *)dev;
}

/* ========================================================================
 * Internal helper functions
 * ======================================================================== */

/**
 * Simple hash function for device paths
 * Used as device ID for telemetry
 */
static uint32_t hash_device_path(const char *path)
{
    uint32_t hash = 5381;
    int c;

    while ((c = *path++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/**
 * Load device profile from profiles/ directory
 *
 * Tries to find a matching profile by device ID or role
 */
static int load_device_profile(const char *path, const char *role,
                                dsv4l2_device_internal_t *dev)
{
    const dsv4l2_device_profile_t *profile = NULL;

    (void)path;  /* TODO: extract USB VID:PID from sysfs */

    /* Try to find profile by role */
    profile = dsv4l2_find_profile_by_role(role);

    if (profile) {
        /* Apply profile settings */
        dev->classification = strdup(profile->classification);
        dev->tempest_ctrl_id = profile->tempest_ctrl_id;
        dev->profile_path = strdup(profile->filename);
        return 0;
    }

    /* No profile found - use defaults based on role */
    if (strcmp(role, "iris_scanner") == 0) {
        dev->classification = strdup("SECRET_BIOMETRIC");
        dev->tempest_ctrl_id = 0x9a0902;
    } else if (strcmp(role, "ir_sensor") == 0) {
        dev->classification = strdup("SECRET");
        dev->tempest_ctrl_id = 0x9a0902;
    } else if (strcmp(role, "tempest_cam") == 0) {
        dev->classification = strdup("TEMPEST_ONLY");
        dev->tempest_ctrl_id = 0x9a0902;
    } else {
        dev->classification = strdup("UNCLASSIFIED");
        dev->tempest_ctrl_id = 0;  /* No TEMPEST control */
    }

    return 0;
}
