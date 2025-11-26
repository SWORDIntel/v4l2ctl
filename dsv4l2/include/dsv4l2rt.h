#ifndef DSV4L2RT_H
#define DSV4L2RT_H

#include "dsv4l2_annotations.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event types
 */
typedef enum {
    DSV4L2_EVENT_CAPTURE_START      = 1,
    DSV4L2_EVENT_CAPTURE_END        = 2,
    DSV4L2_EVENT_TEMPEST_TRANSITION = 3,
    DSV4L2_EVENT_FORMAT_CHANGE      = 4,
    DSV4L2_EVENT_ERROR              = 5,
    DSV4L2_EVENT_POLICY_CHECK       = 6,
} dsv4l2_event_type_t;

/**
 * Event severity
 */
typedef enum {
    DSV4L2_SEV_INFO    = 0,
    DSV4L2_SEV_WARNING = 1,
    DSV4L2_SEV_ERROR   = 2,
    DSV4L2_SEV_CRIT    = 3,
} dsv4l2_severity_t;

/**
 * Event structure
 */
typedef struct {
    uint64_t            ts_ns;        /* monotonic timestamp (ns) */
    uint32_t            dev_id;       /* device identifier */
    uint16_t            event_type;   /* dsv4l2_event_type_t */
    uint16_t            severity;     /* dsv4l2_severity_t */
    uint32_t            aux;          /* event-specific data */
    char                context[64];  /* optional context string */
} dsv4l2_event_t;

/**
 * Initialize runtime (idempotent)
 */
void dsv4l2rt_init(void);

/**
 * Emit event to ring buffer
 *
 * @param event Event structure
 */
void dsv4l2rt_emit(const dsv4l2_event_t *event);

/**
 * Flush events to backend (stderr, file, redis, etc.)
 */
void dsv4l2rt_flush(void);

/**
 * Shutdown runtime
 */
void dsv4l2rt_shutdown(void);

/**
 * Convenience logging functions
 * These are called by DSLLVM-instrumented code
 */

void dsv4l2rt_log_capture_start(uint32_t dev_id)
    DSMIL_ATTR(dsv4l2_event("capture_start", "medium"));

void dsv4l2rt_log_capture_end(uint32_t dev_id, int rc)
    DSMIL_ATTR(dsv4l2_event("capture_end", "medium"));

void dsv4l2rt_log_tempest_transition(
    uint32_t dev_id,
    dsv4l2_tempest_state_t old_state,
    dsv4l2_tempest_state_t new_state
) DSMIL_ATTR(dsv4l2_event("tempest_transition", "critical"));

void dsv4l2rt_log_policy_check(
    uint32_t dev_id,
    const char *context,
    int result
) DSMIL_ATTR(dsv4l2_event("policy_check", "high"));

void dsv4l2rt_log_format_change(
    uint32_t dev_id,
    uint32_t pixel_format,
    uint32_t width,
    uint32_t height
) DSMIL_ATTR(dsv4l2_event("format_change", "medium"));

void dsv4l2rt_log_error(
    uint32_t dev_id,
    int error_code,
    const char *message
) DSMIL_ATTR(dsv4l2_event("error", "high"));

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2RT_H */
