/*
 * DSV4L2 Capture Operations
 *
 * Implements frame capture with DSLLVM enforcement:
 * - TEMPEST checks before capture (MANDATORY)
 * - Policy validation
 * - Telemetry for all capture events
 * - Secret region annotation for biometric capture
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

/* Forward declarations */
typedef struct dsv4l2_device_internal dsv4l2_device_internal_t;
extern dsv4l2_device_internal_t *dsv4l2_get_internal(dsv4l2_device_t *dev);

/* Buffer management functions */
extern int dsv4l2_dequeue_buffer(dsv4l2_device_t *dev, struct v4l2_buffer *buf);
extern int dsv4l2_queue_buffer(dsv4l2_device_t *dev, uint32_t index);
extern int dsv4l2_get_buffer(dsv4l2_device_t *dev, uint32_t index,
                              void **start, size_t *length);

/* Internal device structure (partial) */
struct dsv4l2_device_internal {
    dsv4l2_device_t public;
    struct v4l2_capability cap;
    dsv4l2_tempest_state_t tempest;
    int tempest_ctrl_id;
    char *profile_path;
    char *classification;
    int streaming;
    uint32_t dev_id;
    void *buffers;
    uint32_t buffer_count;
};

/**
 * Start streaming
 *
 * @param dev Device handle
 * @return 0 on success, negative errno on error
 */
int dsv4l2_start_streaming(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;
    enum v4l2_buf_type type;

    if (!dev) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    if (internal->streaming) {
        return 0;  /* Already streaming */
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &type) < 0) {
        return -errno;
    }

    internal->streaming = 1;

    /* Emit streaming start event */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_CAPTURE_START,
                         DSV4L2_SEV_INFO, 0);

    return 0;
}

/**
 * Stop streaming
 *
 * @param dev Device handle
 * @return 0 on success, negative errno on error
 */
int dsv4l2_stop_streaming(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;
    enum v4l2_buf_type type;

    if (!dev) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    if (!internal->streaming) {
        return 0;  /* Not streaming */
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMOFF, &type) < 0) {
        return -errno;
    }

    internal->streaming = 0;

    /* Emit streaming stop event */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_CAPTURE_STOP,
                         DSV4L2_SEV_INFO, 0);

    return 0;
}

/**
 * Capture a single frame (standard camera)
 *
 * This function demonstrates DSLLVM enforcement:
 * - Annotated with DSMIL_REQUIRES_TEMPEST_CHECK
 * - MUST call dsv4l2_get_tempest_state() and dsv4l2_policy_check()
 * - DSLLVM will reject build if checks are missing
 *
 * @param dev Device handle
 * @param out Output frame buffer
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("camera", "L3", "UNCLASSIFIED")
DSMIL_REQUIRES_TEMPEST_CHECK
int dsv4l2_capture_frame(dsv4l2_device_t *dev, dsv4l2_frame_t *out)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_buffer buf;
    void *buffer_start;
    size_t buffer_len;
    int rc;

    if (!dev || !out) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    /* CRITICAL: TEMPEST state check (REQUIRED by DSLLVM) */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    if (dsv4l2_policy_check(state, "capture_frame") != 0) {
        /* Policy violation: emit event */
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_POLICY_VIOLATION,
                             DSV4L2_SEV_CRITICAL, state);
        return -EPERM;
    }

    /* Ensure streaming is active */
    if (!internal->streaming) {
        rc = dsv4l2_start_streaming(dev);
        if (rc < 0) {
            return rc;
        }
    }

    /* Dequeue buffer */
    rc = dsv4l2_dequeue_buffer(dev, &buf);
    if (rc < 0) {
        /* Emit frame dropped event */
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_FRAME_DROPPED,
                             DSV4L2_SEV_MEDIUM, -rc);
        return rc;
    }

    /* Get buffer pointer */
    rc = dsv4l2_get_buffer(dev, buf.index, &buffer_start, &buffer_len);
    if (rc < 0) {
        dsv4l2_queue_buffer(dev, buf.index);
        return rc;
    }

    /* Fill output frame */
    out->data = (uint8_t *)buffer_start;
    out->len = buf.bytesused;

    /* Emit frame acquired event */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_FRAME_ACQUIRED,
                         DSV4L2_SEV_INFO, buf.bytesused);

    /* Requeue buffer */
    dsv4l2_queue_buffer(dev, buf.index);

    return 0;
}

/**
 * Capture iris frame (biometric mode)
 *
 * This function has maximum security annotations:
 * - DSV4L2_SENSOR with "iris_scanner" role and "SECRET_BIOMETRIC" classification
 * - DSMIL_SECRET_REGION: Enforces constant-time operations
 * - DSMIL_REQUIRES_TEMPEST_CHECK: Mandatory TEMPEST check
 *
 * DSLLVM enforces:
 * - No secret-dependent branches
 * - No array indexing with secret values
 * - No logging/network egress of biometric data
 * - TEMPEST check before capture
 *
 * @param dev Device handle
 * @param out Output frame buffer (tagged as dsmil_secret)
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("iris_scanner", "L3", "SECRET_BIOMETRIC")
DSMIL_SECRET_REGION
DSMIL_REQUIRES_TEMPEST_CHECK
int dsv4l2_capture_iris(dsv4l2_device_t *dev, dsv4l2_frame_t *out)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_buffer buf;
    void *buffer_start;
    size_t buffer_len;
    int rc;

    if (!dev || !out) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    /* Emit iris capture event (HIGH severity) */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_IRIS_CAPTURE,
                         DSV4L2_SEV_HIGH, 0);

    /* CRITICAL: TEMPEST check is MANDATORY for biometric capture */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);

    /* LOCKDOWN specifically blocks biometric capture */
    if (state == DSV4L2_TEMPEST_LOCKDOWN) {
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_TEMPEST_LOCKDOWN,
                             DSV4L2_SEV_CRITICAL, state);
        return -EPERM;
    }

    /* General policy check */
    if (dsv4l2_policy_check(state, "capture_iris") != 0) {
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_POLICY_VIOLATION,
                             DSV4L2_SEV_CRITICAL, state);
        return -EPERM;
    }

    /* Ensure streaming is active */
    if (!internal->streaming) {
        rc = dsv4l2_start_streaming(dev);
        if (rc < 0) {
            return rc;
        }
    }

    /* Dequeue buffer (in secret region - constant-time enforced) */
    rc = dsv4l2_dequeue_buffer(dev, &buf);
    if (rc < 0) {
        return rc;
    }

    /* Get buffer pointer */
    rc = dsv4l2_get_buffer(dev, buf.index, &buffer_start, &buffer_len);
    if (rc < 0) {
        dsv4l2_queue_buffer(dev, buf.index);
        return rc;
    }

    /* Fill output frame (this buffer is tagged as dsmil_secret) */
    /* DSLLVM tracks this through IR and forbids:
     * - printf/fprintf/syslog of this data
     * - send/sendto/write without encryption
     * - storage without dsv4l2_store_encrypted() */
    out->data = (uint8_t *)buffer_start;
    out->len = buf.bytesused;

    /* Requeue buffer */
    dsv4l2_queue_buffer(dev, buf.index);

    return 0;
}

/**
 * Fused video + metadata capture
 *
 * Annotated as quantum candidate for L7/L8 offload.
 * DSLLVM can extract features for ML/AI processing.
 *
 * @param video_dev Video device handle
 * @param meta_dev Metadata device handle (currently unused)
 * @param out_frame Output video frame
 * @param out_meta Output metadata (currently unused)
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("fused_sensor", "L3", "SECRET")
DSMIL_QUANTUM_CANDIDATE("fused_capture")
DSMIL_REQUIRES_TEMPEST_CHECK
int dsv4l2_fused_capture(dsv4l2_device_t *video_dev,
                         dsv4l2_meta_handle_t *meta_dev,
                         dsv4l2_frame_t *out_frame,
                         dsv4l2_meta_t *out_meta)
{
    dsv4l2_device_internal_t *internal;
    int rc;

    if (!video_dev || !out_frame) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(video_dev);

    /* Emit fused capture event */
    dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_FUSED_CAPTURE,
                         DSV4L2_SEV_MEDIUM, 0);

    /* TEMPEST check for video device */
    dsv4l2_tempest_state_t vid_state = dsv4l2_get_tempest_state(video_dev);

    /* If metadata device provided, check its TEMPEST state too */
    /* (Metadata device support to be implemented in Phase 5) */
    (void)meta_dev;

    /* Policy check */
    if (dsv4l2_policy_check(vid_state, "fused_capture") != 0) {
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_POLICY_VIOLATION,
                             DSV4L2_SEV_CRITICAL, vid_state);
        return -EPERM;
    }

    /* Capture video frame */
    rc = dsv4l2_capture_frame(video_dev, out_frame);
    if (rc < 0) {
        return rc;
    }

    /* Capture metadata (stub for Phase 5) */
    if (out_meta) {
        out_meta->data = NULL;
        out_meta->len = 0;
    }

    return 0;
}
