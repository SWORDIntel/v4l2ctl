#ifndef DSV4L2_PROFILES_H
#define DSV4L2_PROFILES_H

#include "dsv4l2_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load profile from YAML file
 *
 * @param filepath Path to YAML profile file
 * @param out_profile Output profile structure
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_profile_load_from_file(
    const char *filepath,
    dsv4l2_profile_t **out_profile
);

/**
 * Load profile by device path and role
 * Searches in profile directory for matching profile
 *
 * @param device_path Device path (e.g., "/dev/video0")
 * @param role Device role (e.g., "iris_scanner")
 * @param out_profile Output profile structure
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_profile_load(
    const char *device_path,
    const char *role,
    dsv4l2_profile_t **out_profile
);

/**
 * Load profile by USB VID:PID
 *
 * @param vendor_id USB vendor ID
 * @param product_id USB product ID
 * @param role Device role
 * @param out_profile Output profile structure
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_profile_load_by_vidpid(
    uint16_t vendor_id,
    uint16_t product_id,
    const char *role,
    dsv4l2_profile_t **out_profile
);

/**
 * Apply profile settings to device
 *
 * @param dev Device handle
 * @param profile Profile to apply
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_profile_apply(
    dsv4l2_device_t *dev,
    const dsv4l2_profile_t *profile
);

/**
 * Free profile structure
 *
 * @param profile Profile to free
 */
void dsv4l2_profile_free(dsv4l2_profile_t *profile);

/**
 * Get default profile directory
 *
 * @return Path to profile directory
 */
const char* dsv4l2_get_profile_dir(void);

/**
 * Set profile directory
 *
 * @param dir_path Directory path
 */
void dsv4l2_set_profile_dir(const char *dir_path);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_PROFILES_H */
