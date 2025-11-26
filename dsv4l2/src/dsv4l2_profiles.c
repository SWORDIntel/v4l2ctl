/*
 * dsv4l2_profiles.c
 *
 * Profile loading and management for DSV4L2
 * Stub implementation - to be completed in Phase 2
 */

#include "dsv4l2_profiles.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char profile_dir[256] = "dsv4l2/profiles";

int dsv4l2_profile_load_from_file(
    const char *filepath,
    dsv4l2_profile_t **out_profile
) {
    if (!filepath || !out_profile) {
        return -EINVAL;
    }

    /* TODO: Implement YAML parsing in Phase 2 */
    /* For now, return stub profile */

    dsv4l2_profile_t *profile = calloc(1, sizeof(*profile));
    if (!profile) {
        return -ENOMEM;
    }

    /* Stub: fill with defaults */
    strncpy(profile->role, "camera", sizeof(profile->role));
    strncpy(profile->classification, "UNCLASSIFIED", sizeof(profile->classification));

    *out_profile = profile;
    return -ENOSYS;  /* Indicate not fully implemented */
}

int dsv4l2_profile_load(
    const char *device_path,
    const char *role,
    dsv4l2_profile_t **out_profile
) {
    if (!device_path || !role || !out_profile) {
        return -EINVAL;
    }

    /* TODO: Implement profile search by device + role in Phase 2 */
    return -ENOSYS;
}

int dsv4l2_profile_load_by_vidpid(
    uint16_t vendor_id,
    uint16_t product_id,
    const char *role,
    dsv4l2_profile_t **out_profile
) {
    if (!role || !out_profile) {
        return -EINVAL;
    }

    /* TODO: Implement profile search by VID:PID in Phase 2 */
    return -ENOSYS;
}

int dsv4l2_profile_apply(
    dsv4l2_device_t *dev,
    const dsv4l2_profile_t *profile
) {
    if (!dev || !profile) {
        return -EINVAL;
    }

    /* TODO: Implement profile application (format, controls, etc.) in Phase 2 */
    return -ENOSYS;
}

void dsv4l2_profile_free(dsv4l2_profile_t *profile) {
    if (profile) {
        free(profile);
    }
}

const char* dsv4l2_get_profile_dir(void) {
    return profile_dir;
}

void dsv4l2_set_profile_dir(const char *dir_path) {
    if (dir_path) {
        strncpy(profile_dir, dir_path, sizeof(profile_dir) - 1);
        profile_dir[sizeof(profile_dir) - 1] = '\0';
    }
}
