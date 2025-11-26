#ifndef DSV4L2_FUSION_H
#define DSV4L2_FUSION_H

#include "dsv4l2_annotations.h"
#include "dsv4l2_core.h"
#include "dsv4l2_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Capture fused (video + metadata) frame pair
 * Synchronized by timestamp within tolerance window
 *
 * @param video_dev Video device handle
 * @param meta_dev Metadata device handle
 * @param out_frame Output video frame
 * @param out_meta Output metadata packet
 * @return 0 on success, negative error code on failure
 */
int
dsv4l2_fused_capture(
    dsv4l2_device_t *video_dev,
    dsv4l2_meta_handle_t *meta_dev,
    dsv4l2_frame_t *out_frame,
    dsv4l2_meta_t *out_meta
) DSMIL_QUANTUM_CANDIDATE("fused_capture")
  DSMIL_REQUIRES_TEMPEST_CHECK;

/**
 * Set fusion tolerance (max timestamp difference)
 *
 * @param video_dev Video device handle
 * @param tolerance_ns Tolerance in nanoseconds
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_fusion_set_tolerance(
    dsv4l2_device_t *video_dev,
    uint64_t tolerance_ns
);

/**
 * Get fusion tolerance
 *
 * @param video_dev Video device handle
 * @return Current tolerance in nanoseconds
 */
uint64_t dsv4l2_fusion_get_tolerance(dsv4l2_device_t *video_dev);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_FUSION_H */
