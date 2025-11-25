/*
 * DSV4L2 Instrumentation Example
 *
 * This file demonstrates the before/after of adding DSLLVM instrumentation
 * to a v4l2 capture function, following the same pattern as SHRINK.
 *
 * Compile flags:
 *   -fdsv4l2-profile=off|ops|exercise|forensic
 *   -mdsv4l2-mission=<mission_name>
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

/* ========================================================================
 * BEFORE: Plain v4l2 capture (no instrumentation)
 * ======================================================================== */

int capture_frame_plain(int fd, void *buffer, size_t size) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue buffer
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        return -errno;
    }

    // Copy frame data
    memcpy(buffer, (void *)buf.m.offset, size);

    // Requeue buffer
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    return 0;
}

/* ========================================================================
 * AFTER: DSLLVM-instrumented capture with TEMPEST checks
 * ======================================================================== */

/*
 * Annotated capture function for standard camera.
 * DSLLVM will:
 *   1. Verify TEMPEST policy check occurs before frame delivery
 *   2. Inject runtime probes based on -fdsv4l2-profile setting
 *   3. Track this function in static metadata for DSMIL device mapping
 */
DSV4L2_SENSOR("camera", "L3", "UNCLASSIFIED")
int dsv4l2_capture_frame_instrumented(dsv4l2_device_t *dev,
                                       dsv4l2_frame_t *out)
    DSMIL_REQUIRES_TEMPEST_CHECK
{
    int rc;
    struct v4l2_buffer buf;

    /* Event: Capture start
     * DSLLVM injects this call when -fdsv4l2-profile >= ops */
    dsv4l2rt_emit_simple(
        (uint32_t)(uintptr_t)dev->dev_path,  // Device ID (hash in real impl)
        DSV4L2_EVENT_CAPTURE_START,
        DSV4L2_SEV_DEBUG,
        0
    );

    /* TEMPEST policy check (required by DSMIL_REQUIRES_TEMPEST_CHECK)
     * DSLLVM's tempest_policy pass verifies this call exists */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    if (dsv4l2_policy_check(state, "capture_frame") != 0) {
        /* Event: Policy violation */
        dsv4l2rt_emit_simple(
            (uint32_t)(uintptr_t)dev->dev_path,
            DSV4L2_EVENT_POLICY_VIOLATION,
            DSV4L2_SEV_CRITICAL,
            state
        );
        return -EPERM;
    }

    /* Standard v4l2 capture logic */
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue buffer */
    if ((rc = ioctl(dev->fd, VIDIOC_DQBUF, &buf)) < 0) {
        /* Event: Frame dropped / error */
        dsv4l2rt_emit_simple(
            (uint32_t)(uintptr_t)dev->dev_path,
            DSV4L2_EVENT_FRAME_DROPPED,
            DSV4L2_SEV_MEDIUM,
            errno
        );
        return -errno;
    }

    /* Copy frame data into secret-tagged output buffer */
    out->data = (uint8_t *)buf.m.offset;
    out->len  = buf.bytesused;

    /* Event: Frame acquired
     * In 'exercise' or 'forensic' profile, DSLLVM may inject additional
     * metadata capture here (frame size, timestamp, etc.) */
    dsv4l2rt_emit_simple(
        (uint32_t)(uintptr_t)dev->dev_path,
        DSV4L2_EVENT_FRAME_ACQUIRED,
        DSV4L2_SEV_INFO,
        buf.bytesused
    );

    /* Requeue buffer */
    if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    return 0;
}

/* ========================================================================
 * IRIS SCANNER: Secret region with constant-time enforcement
 * ======================================================================== */

/*
 * Iris capture with maximum security annotations.
 * DSLLVM enforces:
 *   - dsmil_secret_region: Constant-time, no secret-dependent branches
 *   - dsmil_requires_tempest_check: TEMPEST policy must be consulted
 *   - Classification: SECRET_BIOMETRIC prevents logging/network egress
 */
DSV4L2_SENSOR("iris_scanner", "L3", "SECRET_BIOMETRIC")
DSMIL_SECRET_REGION
int dsv4l2_capture_iris_instrumented(dsv4l2_device_t *dev,
                                      dsv4l2_frame_t *out)
    DSMIL_REQUIRES_TEMPEST_CHECK
{
    int rc;
    struct v4l2_buffer buf;

    /* Event: Entering iris mode */
    dsv4l2rt_emit_simple(
        (uint32_t)(uintptr_t)dev->dev_path,
        DSV4L2_EVENT_IRIS_CAPTURE,
        DSV4L2_SEV_HIGH,
        0
    );

    /* TEMPEST check is MANDATORY for biometric capture */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    if (state == DSV4L2_TEMPEST_LOCKDOWN) {
        dsv4l2rt_emit_simple(
            (uint32_t)(uintptr_t)dev->dev_path,
            DSV4L2_EVENT_TEMPEST_LOCKDOWN,
            DSV4L2_SEV_CRITICAL,
            state
        );
        return -EPERM;
    }

    if (dsv4l2_policy_check(state, "capture_iris") != 0) {
        return -EPERM;
    }

    /* Capture logic (same as above, but in secret region)
     * DSLLVM's constant_time pass ensures:
     *   - No branches on iris data
     *   - No array indexing with iris pixels
     *   - No timing-dependent operations */
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if ((rc = ioctl(dev->fd, VIDIOC_DQBUF, &buf)) < 0) {
        return -errno;
    }

    /* This buffer is tagged as dsmil_secret("biometric_frame")
     * DSLLVM tracks it through IR and forbids:
     *   - printf/fprintf/syslog of this data
     *   - send/sendto/write without encryption
     *   - storage without dsv4l2_store_encrypted() */
    out->data = (uint8_t *)buf.m.offset;
    out->len  = buf.bytesused;

    if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    return 0;
}

/* ========================================================================
 * FUSED CAPTURE: Quantum candidate for sensor fusion
 * ======================================================================== */

/*
 * Fused video + metadata capture with quantum optimization hooks.
 * DSLLVM marks this as a candidate for L7/L8 redistribution.
 */
DSV4L2_SENSOR("fused_sensor", "L3", "SECRET")
int dsv4l2_fused_capture_instrumented(dsv4l2_device_t *video_dev,
                                       dsv4l2_device_t *meta_dev,
                                       dsv4l2_frame_t *out_frame,
                                       dsv4l2_meta_t *out_meta)
    DSMIL_QUANTUM_CANDIDATE("fused_capture")
    DSMIL_REQUIRES_TEMPEST_CHECK
{
    int rc;

    /* Event: Fused capture start */
    dsv4l2rt_emit_simple(
        (uint32_t)(uintptr_t)video_dev->dev_path,
        DSV4L2_EVENT_FUSED_CAPTURE,
        DSV4L2_SEV_MEDIUM,
        0
    );

    /* TEMPEST check (must check BOTH devices) */
    dsv4l2_tempest_state_t vid_state = dsv4l2_get_tempest_state(video_dev);
    dsv4l2_tempest_state_t meta_state = dsv4l2_get_tempest_state(meta_dev);

    if (vid_state != meta_state) {
        /* State mismatch - policy violation */
        dsv4l2rt_emit_simple(
            (uint32_t)(uintptr_t)video_dev->dev_path,
            DSV4L2_EVENT_POLICY_VIOLATION,
            DSV4L2_SEV_CRITICAL,
            (vid_state << 16) | meta_state
        );
        return -EINVAL;
    }

    if (dsv4l2_policy_check(vid_state, "fused_capture") != 0) {
        return -EPERM;
    }

    /* Capture from both streams
     * DSLLVM can extract features here for L8 advisor or L7 quantum offload:
     *   - Frame correlation scores
     *   - Metadata fusion quality
     *   - Anomaly detection inputs */
    rc = dsv4l2_capture_frame_instrumented(video_dev, out_frame);
    if (rc < 0) return rc;

    /* Metadata read (simplified) */
    out_meta->data = NULL;  // Would read from meta device
    out_meta->len = 0;

    return 0;
}

/* ========================================================================
 * TEMPEST STATE TRANSITION: Instrumented control function
 * ======================================================================== */

/*
 * TEMPEST state change with event hooks.
 * DSLLVM ensures all transitions are logged and policy-checked.
 */
DSV4L2_TEMPEST_CONTROL
int dsv4l2_set_tempest_state_instrumented(dsv4l2_device_t *dev,
                                           dsv4l2_tempest_state_t new_state)
    DSMIL_TEMPEST_TRANSITION
{
    int rc;
    struct v4l2_control ctrl;

    /* Query current state */
    dsv4l2_tempest_state_t old_state = dsv4l2_get_tempest_state(dev);

    /* Event: TEMPEST transition attempt */
    dsv4l2_event_t ev = {
        .ts_ns = 0,  // Runtime fills this
        .dev_id = (uint32_t)(uintptr_t)dev->dev_path,
        .event_type = DSV4L2_EVENT_TEMPEST_TRANSITION,
        .severity = DSV4L2_SEV_CRITICAL,
        .aux = (old_state << 16) | new_state,
        .layer = dev->layer,
    };
    strncpy(ev.role, dev->role, sizeof(ev.role) - 1);
    dsv4l2rt_emit(&ev);

    /* Apply transition via v4l2 control */
    ctrl.id = 0x9a0902;  // TEMPEST control ID (device-specific)
    ctrl.value = new_state;

    if ((rc = ioctl(dev->fd, VIDIOC_S_CTRL, &ctrl)) < 0) {
        return -errno;
    }

    return 0;
}
