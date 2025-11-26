/*
 * dsv4l2_fusion.c
 *
 * Video + Metadata fusion for DSV4L2
 * Stub implementation - to be completed in Phase 5
 */

#include "dsv4l2_fusion.h"
#include <errno.h>

/* Default tolerance: 5ms */
static uint64_t default_tolerance_ns = 5000000ULL;

int
dsv4l2_fused_capture(
    dsv4l2_device_t *video_dev,
    dsv4l2_meta_handle_t *meta_dev,
    dsv4l2_frame_t *out_frame,
    dsv4l2_meta_t *out_meta
) {
    if (!video_dev || !meta_dev || !out_frame || !out_meta) {
        return -EINVAL;
    }

    /* TODO: Implement timestamp-aligned fusion in Phase 5 */
    return -ENOSYS;
}

int dsv4l2_fusion_set_tolerance(
    dsv4l2_device_t *video_dev,
    uint64_t tolerance_ns
) {
    if (!video_dev) {
        return -EINVAL;
    }

    /* TODO: Store per-device tolerance in Phase 5 */
    default_tolerance_ns = tolerance_ns;
    return 0;
}

uint64_t dsv4l2_fusion_get_tolerance(dsv4l2_device_t *video_dev) {
    if (!video_dev) {
        return default_tolerance_ns;
    }
    return default_tolerance_ns;
}
