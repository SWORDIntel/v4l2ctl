/*
 * DSV4L2 DSMIL Integration API
 *
 * THREATCON to TEMPEST mapping and policy enforcement
 */

#ifndef DSV4L2_DSMIL_H
#define DSV4L2_DSMIL_H

#include "dsv4l2_annotations.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DSMIL THREATCON levels */
typedef enum {
    THREATCON_NORMAL    = 0,  /* No heightened threat */
    THREATCON_ALPHA     = 1,  /* General threat */
    THREATCON_BRAVO     = 2,  /* Increased threat */
    THREATCON_CHARLIE   = 3,  /* Significant threat */
    THREATCON_DELTA     = 4,  /* Severe threat */
    THREATCON_EMERGENCY = 5,  /* Critical threat */
} dsmil_threatcon_t;

/* Layer policy structure */
typedef struct {
    uint32_t layer;                     /* DSMIL layer (0-8) */
    uint32_t max_width;                 /* Max resolution width */
    uint32_t max_height;                /* Max resolution height */
    dsv4l2_tempest_state_t min_tempest; /* Minimum TEMPEST state */
} dsv4l2_layer_policy_t;

/**
 * Initialize DSMIL policy subsystem
 */
void dsv4l2_policy_init(void);

/**
 * Get current THREATCON level
 */
dsmil_threatcon_t dsv4l2_get_threatcon(void);

/**
 * Set THREATCON level
 */
int dsv4l2_set_threatcon(dsmil_threatcon_t level);

/**
 * Apply THREATCON to device (maps to TEMPEST state)
 */
int dsv4l2_apply_threatcon(dsv4l2_device_t *dev);

/**
 * Get layer policy
 */
int dsv4l2_get_layer_policy(uint32_t layer, dsv4l2_layer_policy_t **policy);

/**
 * Check if capture is allowed for device
 */
int dsv4l2_check_capture_allowed(dsv4l2_device_t *dev, const char *context);

/**
 * Check clearance level
 */
int dsv4l2_check_clearance(const char *role, const char *classification);

/**
 * Get THREATCON name (for display/logging)
 */
const char *dsv4l2_threatcon_name(dsmil_threatcon_t level);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_DSMIL_H */
