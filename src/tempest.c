/*
 * DSV4L2 TEMPEST State Management
 *
 * Implements electromagnetic security primitives:
 * - TEMPEST state query/transition
 * - Policy checks before capture
 * - Telemetry for state changes
 *
 * DSLLVM enforces that all capture operations call these functions.
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

/* Forward declaration of internal device structure */
typedef struct dsv4l2_device_internal dsv4l2_device_internal_t;
extern dsv4l2_device_internal_t *dsv4l2_get_internal(dsv4l2_device_t *dev);

/* Internal accessor for TEMPEST state */
struct dsv4l2_device_internal {
    dsv4l2_device_t public;
    struct v4l2_capability cap;
    dsv4l2_tempest_state_t tempest;
    int tempest_ctrl_id;
    char *profile_path;
    char *classification;
    int streaming;
    uint32_t dev_id;
};

/**
 * Get current TEMPEST state of a device
 *
 * @param dev Device handle
 * @return Current TEMPEST state
 *
 * This function is annotated with DSMIL_TEMPEST_QUERY so DSLLVM knows
 * it queries TEMPEST state. Every capture function MUST call this.
 */
DSMIL_TEMPEST_QUERY
dsv4l2_tempest_state_t dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_control ctrl;

    if (!dev) {
        return DSV4L2_TEMPEST_DISABLED;
    }

    internal = dsv4l2_get_internal(dev);

    /* If device doesn't have TEMPEST control, return DISABLED */
    if (internal->tempest_ctrl_id == 0) {
        return DSV4L2_TEMPEST_DISABLED;
    }

    /* Query v4l2 control */
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = internal->tempest_ctrl_id;

    if (ioctl(dev->fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        /* If control read fails, return cached state */
        return internal->tempest;
    }

    /* Update cached state */
    switch (ctrl.value) {
        case 0: internal->tempest = DSV4L2_TEMPEST_DISABLED; break;
        case 1: internal->tempest = DSV4L2_TEMPEST_LOW; break;
        case 2: internal->tempest = DSV4L2_TEMPEST_HIGH; break;
        case 3: internal->tempest = DSV4L2_TEMPEST_LOCKDOWN; break;
        default: internal->tempest = DSV4L2_TEMPEST_DISABLED; break;
    }

    /* Emit query event (low priority) */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_TEMPEST_QUERY,
                         DSV4L2_SEV_DEBUG, internal->tempest);

    return internal->tempest;
}

/**
 * Set TEMPEST state of a device
 *
 * @param dev Device handle
 * @param state New TEMPEST state
 * @return 0 on success, negative errno on error
 *
 * This function is annotated with DSMIL_TEMPEST_TRANSITION so DSLLVM
 * knows it changes TEMPEST state. Transitions are logged with high severity.
 */
DSV4L2_TEMPEST_CONTROL
DSMIL_TEMPEST_TRANSITION
int dsv4l2_set_tempest_state(dsv4l2_device_t *dev,
                              dsv4l2_tempest_state_t new_state)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_control ctrl;
    dsv4l2_tempest_state_t old_state;
    dsv4l2_event_t ev;

    if (!dev) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    /* If device doesn't have TEMPEST control, reject */
    if (internal->tempest_ctrl_id == 0) {
        return -ENOTSUP;
    }

    /* Get current state */
    old_state = dsv4l2_get_tempest_state(dev);

    /* Validate new state */
    if (new_state < DSV4L2_TEMPEST_DISABLED ||
        new_state > DSV4L2_TEMPEST_LOCKDOWN) {
        return -EINVAL;
    }

    /* Set v4l2 control */
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = internal->tempest_ctrl_id;
    ctrl.value = new_state;

    if (ioctl(dev->fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        return -errno;
    }

    /* Update cached state */
    internal->tempest = new_state;

    /* Emit TEMPEST transition event (CRITICAL severity) */
    memset(&ev, 0, sizeof(ev));
    ev.dev_id = internal->dev_id;
    ev.event_type = DSV4L2_EVENT_TEMPEST_TRANSITION;
    ev.severity = DSV4L2_SEV_CRITICAL;
    ev.aux = (old_state << 16) | new_state;  /* Pack old and new state */
    ev.layer = dev->layer;
    strncpy(ev.role, dev->role, sizeof(ev.role) - 1);

    dsv4l2rt_emit(&ev);

    /* If entering LOCKDOWN, emit additional event */
    if (new_state == DSV4L2_TEMPEST_LOCKDOWN) {
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_TEMPEST_LOCKDOWN,
                             DSV4L2_SEV_CRITICAL, 0);
    }

    return 0;
}

/**
 * Check if capture is allowed under current TEMPEST policy
 *
 * @param state Current TEMPEST state
 * @param context Capture context string (for logging)
 * @return 0 if allowed, -EPERM if blocked
 *
 * This function enforces TEMPEST policy:
 * - LOCKDOWN: No capture allowed
 * - HIGH/LOW/DISABLED: Capture allowed (with different shielding levels)
 *
 * Every DSMIL_REQUIRES_TEMPEST_CHECK function must call this.
 */
int dsv4l2_policy_check(dsv4l2_tempest_state_t state, const char *context)
{
    /* LOCKDOWN blocks all capture */
    if (state == DSV4L2_TEMPEST_LOCKDOWN) {
        return -EPERM;
    }

    /* All other states allow capture */
    /* (Future: could add more complex policy checks here) */
    (void)context;  /* Unused for now, but could be logged */

    return 0;
}

/**
 * Get TEMPEST state name (for display/logging)
 *
 * @param state TEMPEST state
 * @return String name of state
 */
const char *dsv4l2_tempest_state_name(dsv4l2_tempest_state_t state)
{
    switch (state) {
        case DSV4L2_TEMPEST_DISABLED: return "DISABLED";
        case DSV4L2_TEMPEST_LOW:      return "LOW";
        case DSV4L2_TEMPEST_HIGH:     return "HIGH";
        case DSV4L2_TEMPEST_LOCKDOWN: return "LOCKDOWN";
        default:                      return "UNKNOWN";
    }
}
