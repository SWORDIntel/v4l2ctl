/*
 * DSV4L2 Profile System API
 *
 * Device profile loading and management
 */

#ifndef DSV4L2_PROFILES_H
#define DSV4L2_PROFILES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Profile structure */
typedef struct {
    char id[32];                    /* USB VID:PID or identifier */
    char vendor[64];
    char model[128];
    char role[32];
    char classification[32];
    uint32_t layer;
    int tempest_ctrl_id;

    /* Format settings */
    char pixel_format[8];
    uint32_t width;
    uint32_t height;
    uint32_t fps;

    /* Profile metadata */
    char filename[256];
} dsv4l2_device_profile_t;

/**
 * Find a profile by device ID (USB VID:PID)
 */
const dsv4l2_device_profile_t *dsv4l2_find_profile(const char *id);

/**
 * Find a profile by role
 */
const dsv4l2_device_profile_t *dsv4l2_find_profile_by_role(const char *role);

/**
 * Get total number of loaded profiles
 */
size_t dsv4l2_get_profile_count(void);

/**
 * Get profile by index
 */
const dsv4l2_device_profile_t *dsv4l2_get_profile(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_PROFILES_H */
