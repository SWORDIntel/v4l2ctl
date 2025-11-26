/*
 * dsv4l2rt.c
 *
 * Minimal runtime implementation for DSV4L2 event telemetry
 * This version logs to stderr; can be extended for Redis/SQLite/etc.
 */

#include "dsv4l2rt.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define EVENT_RING_SIZE 1024

/* Ring buffer for events */
static dsv4l2_event_t event_ring[EVENT_RING_SIZE];
static size_t ring_head = 0;
static size_t ring_tail = 0;
static pthread_mutex_t ring_lock = PTHREAD_MUTEX_INITIALIZER;

static int initialized = 0;

/**
 * Get current monotonic time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Event type to string
 */
static const char* event_type_str(dsv4l2_event_type_t type) {
    switch (type) {
        case DSV4L2_EVENT_CAPTURE_START:      return "CAPTURE_START";
        case DSV4L2_EVENT_CAPTURE_END:        return "CAPTURE_END";
        case DSV4L2_EVENT_TEMPEST_TRANSITION: return "TEMPEST_TRANSITION";
        case DSV4L2_EVENT_FORMAT_CHANGE:      return "FORMAT_CHANGE";
        case DSV4L2_EVENT_ERROR:              return "ERROR";
        case DSV4L2_EVENT_POLICY_CHECK:       return "POLICY_CHECK";
        default:                              return "UNKNOWN";
    }
}

/**
 * Severity to string
 */
static const char* severity_str(dsv4l2_severity_t sev) {
    switch (sev) {
        case DSV4L2_SEV_INFO:    return "INFO";
        case DSV4L2_SEV_WARNING: return "WARN";
        case DSV4L2_SEV_ERROR:   return "ERROR";
        case DSV4L2_SEV_CRIT:    return "CRIT";
        default:                 return "?";
    }
}

void dsv4l2rt_init(void) {
    if (initialized) {
        return;
    }

    pthread_mutex_lock(&ring_lock);
    ring_head = 0;
    ring_tail = 0;
    initialized = 1;
    pthread_mutex_unlock(&ring_lock);

    fprintf(stderr, "[DSV4L2RT] Runtime initialized\n");
}

void dsv4l2rt_emit(const dsv4l2_event_t *event) {
    if (!event) {
        return;
    }

    if (!initialized) {
        dsv4l2rt_init();
    }

    pthread_mutex_lock(&ring_lock);

    /* Add to ring buffer */
    size_t next_head = (ring_head + 1) % EVENT_RING_SIZE;
    if (next_head != ring_tail) {
        event_ring[ring_head] = *event;
        ring_head = next_head;
    } else {
        /* Ring overflow - drop oldest event */
        ring_tail = (ring_tail + 1) % EVENT_RING_SIZE;
        event_ring[ring_head] = *event;
        ring_head = next_head;
    }

    pthread_mutex_unlock(&ring_lock);
}

void dsv4l2rt_flush(void) {
    if (!initialized) {
        return;
    }

    pthread_mutex_lock(&ring_lock);

    /* Print all events in ring buffer */
    while (ring_tail != ring_head) {
        dsv4l2_event_t *ev = &event_ring[ring_tail];

        fprintf(stderr, "[DSV4L2RT] ts=%llu dev=%u type=%s sev=%s aux=%u",
                (unsigned long long)ev->ts_ns,
                ev->dev_id,
                event_type_str((dsv4l2_event_type_t)ev->event_type),
                severity_str((dsv4l2_severity_t)ev->severity),
                ev->aux);

        if (ev->context[0] != '\0') {
            fprintf(stderr, " context=\"%s\"", ev->context);
        }

        fprintf(stderr, "\n");

        ring_tail = (ring_tail + 1) % EVENT_RING_SIZE;
    }

    pthread_mutex_unlock(&ring_lock);
}

void dsv4l2rt_shutdown(void) {
    dsv4l2rt_flush();
    initialized = 0;
}

/* Convenience logging functions */

void dsv4l2rt_log_capture_start(uint32_t dev_id) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_CAPTURE_START,
        .severity  = DSV4L2_SEV_INFO,
        .aux       = 0,
        .context   = ""
    };
    dsv4l2rt_emit(&ev);
}

void dsv4l2rt_log_capture_end(uint32_t dev_id, int rc) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_CAPTURE_END,
        .severity  = (rc == 0) ? DSV4L2_SEV_INFO : DSV4L2_SEV_ERROR,
        .aux       = (uint32_t)rc,
        .context   = ""
    };
    dsv4l2rt_emit(&ev);
}

void dsv4l2rt_log_tempest_transition(
    uint32_t dev_id,
    dsv4l2_tempest_state_t old_state,
    dsv4l2_tempest_state_t new_state
) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_TEMPEST_TRANSITION,
        .severity  = DSV4L2_SEV_CRIT,
        .aux       = ((uint32_t)old_state << 16) | (uint32_t)new_state,
        .context   = ""
    };
    dsv4l2rt_emit(&ev);
}

void dsv4l2rt_log_policy_check(
    uint32_t dev_id,
    const char *context,
    int result
) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_POLICY_CHECK,
        .severity  = (result == 0) ? DSV4L2_SEV_INFO : DSV4L2_SEV_WARNING,
        .aux       = (uint32_t)result,
        .context   = ""
    };
    if (context) {
        strncpy(ev.context, context, sizeof(ev.context) - 1);
        ev.context[sizeof(ev.context) - 1] = '\0';
    }
    dsv4l2rt_emit(&ev);
}

void dsv4l2rt_log_format_change(
    uint32_t dev_id,
    uint32_t pixel_format,
    uint32_t width,
    uint32_t height
) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_FORMAT_CHANGE,
        .severity  = DSV4L2_SEV_INFO,
        .aux       = pixel_format,
        .context   = ""
    };
    snprintf(ev.context, sizeof(ev.context), "%ux%u", width, height);
    dsv4l2rt_emit(&ev);
}

void dsv4l2rt_log_error(
    uint32_t dev_id,
    int error_code,
    const char *message
) {
    dsv4l2_event_t ev = {
        .ts_ns     = get_time_ns(),
        .dev_id    = dev_id,
        .event_type= DSV4L2_EVENT_ERROR,
        .severity  = DSV4L2_SEV_ERROR,
        .aux       = (uint32_t)error_code,
        .context   = ""
    };
    if (message) {
        strncpy(ev.context, message, sizeof(ev.context) - 1);
        ev.context[sizeof(ev.context) - 1] = '\0';
    }
    dsv4l2rt_emit(&ev);
}
