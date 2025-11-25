    #ifndef DSV4L2_POLICY_H
    #define DSV4L2_POLICY_H

    #include "dsv4l2_annotations.h"

    #ifdef __cplusplus
    extern "C" {
    #endif

    /* Forward declaration for metadata handle (not yet defined) */
    typedef struct dsv4l2_meta_handle dsv4l2_meta_handle_t;

    dsv4l2_tempest_state_t
    dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
        DSMIL_TEMPEST_QUERY;

    int
    dsv4l2_set_tempest_state(dsv4l2_device_t *dev,
                             dsv4l2_tempest_state_t state)
        DSMIL_TEMPEST_TRANSITION;

    int
    dsv4l2_policy_check(dsv4l2_tempest_state_t state,
                        const char *context);

    int
    dsv4l2_capture_frame(dsv4l2_device_t *dev,
                         dsv4l2_frame_t *out)
        DSMIL_REQUIRES_TEMPEST_CHECK;

    int
    DSMIL_SECRET_REGION
    dsv4l2_capture_iris(dsv4l2_device_t *dev,
                        dsv4l2_frame_t *out)
        DSMIL_REQUIRES_TEMPEST_CHECK;

    int
    dsv4l2_fused_capture(dsv4l2_device_t *video_dev,
                         dsv4l2_meta_handle_t *meta_dev,
                         dsv4l2_frame_t *out_frame,
                         dsv4l2_meta_t *out_meta)
        DSMIL_QUANTUM_CANDIDATE("fused_capture");

    #ifdef __cplusplus
    }
    #endif

    #endif /* DSV4L2_POLICY_H */
