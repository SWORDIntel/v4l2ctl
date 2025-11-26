#ifndef DSV4L2_TEMPEST_H
#define DSV4L2_TEMPEST_H

#include "dsv4l2_annotations.h"
#include "dsv4l2_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get current TEMPEST state
 *
 * @param dev Device handle
 * @return Current TEMPEST state
 */
dsv4l2_tempest_state_t
dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
    DSMIL_TEMPEST_QUERY;

/**
 * Set TEMPEST state
 *
 * @param dev Device handle
 * @param target_state Desired TEMPEST state
 * @return 0 on success, negative error code on failure
 */
int
dsv4l2_set_tempest_state(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t target_state
) DSMIL_TEMPEST_TRANSITION;

/**
 * Policy check for capture
 * Called internally before capture operations
 *
 * @param dev Device handle
 * @param current_state Current TEMPEST state
 * @param context Operation context (for logging)
 * @return 0 if allowed, negative error code if denied
 */
int
dsv4l2_policy_check_capture(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t current_state,
    const char *context
);

/**
 * Discover TEMPEST control by scanning vendor controls
 * Looks for controls with names matching TEMPEST|PRIVACY|SECURE patterns
 *
 * @param dev Device handle
 * @param out_control_id Output control ID (if found)
 * @return 0 if found, -1 if not found
 */
int
dsv4l2_discover_tempest_control(
    dsv4l2_device_t *dev,
    uint32_t *out_control_id
);

/**
 * Apply TEMPEST control mapping from profile
 *
 * @param dev Device handle
 * @return 0 on success, negative error code on failure
 */
int
dsv4l2_apply_tempest_mapping(dsv4l2_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_TEMPEST_H */
