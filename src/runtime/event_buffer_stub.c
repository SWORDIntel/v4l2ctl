/*
 * DSV4L2 Runtime - Minimal Event Buffer Implementation
 *
 * This is a minimal stub for Phase 2 to allow linking.
 * Full implementation will be done in Phase 6.
 *
 * For now, just prints events to stderr (if enabled).
 */

#include "dsv4l2rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* Runtime state */
static struct {
    int initialized;
    dsv4l2_profile_t profile;
    pthread_mutex_t lock;
    uint64_t events_emitted;
    uint64_t events_dropped;
} runtime = {
    .initialized = 0,
    .profile = DSV4L2_PROFILE_OFF,
};

/**
 * Initialize runtime (stub)
 */
int dsv4l2rt_init(const dsv4l2rt_config_t *config)
{
    if (runtime.initialized) {
        return 0;  /* Already initialized */
    }

    pthread_mutex_init(&runtime.lock, NULL);

    if (config) {
        runtime.profile = config->profile;
    } else {
        runtime.profile = DSV4L2_PROFILE_OFF;
    }

    runtime.initialized = 1;

    return 0;
}

/**
 * Emit an event (stub - just counts events, optionally prints)
 */
void dsv4l2rt_emit(const dsv4l2_event_t *ev)
{
    /* Auto-initialize if not initialized */
    if (!runtime.initialized) {
        dsv4l2rt_config_t config = {
            .profile = DSV4L2_PROFILE_OPS,  /* Default to ops */
        };
        dsv4l2rt_init(&config);
    }

    /* If profile is OFF, do nothing */
    if (runtime.profile == DSV4L2_PROFILE_OFF) {
        return;
    }

    pthread_mutex_lock(&runtime.lock);
    runtime.events_emitted++;
    pthread_mutex_unlock(&runtime.lock);

    /* In exercise/forensic mode, print events to stderr */
    if (runtime.profile >= DSV4L2_PROFILE_EXERCISE) {
        const char *event_name = "UNKNOWN";
        const char *severity_name = "INFO";

        /* Map event type to name (abbreviated for stub) */
        switch (ev->event_type) {
            case DSV4L2_EVENT_DEVICE_OPEN:          event_name = "DEVICE_OPEN"; break;
            case DSV4L2_EVENT_DEVICE_CLOSE:         event_name = "DEVICE_CLOSE"; break;
            case DSV4L2_EVENT_CAPTURE_START:        event_name = "CAPTURE_START"; break;
            case DSV4L2_EVENT_CAPTURE_STOP:         event_name = "CAPTURE_STOP"; break;
            case DSV4L2_EVENT_FRAME_ACQUIRED:       event_name = "FRAME_ACQUIRED"; break;
            case DSV4L2_EVENT_FRAME_DROPPED:        event_name = "FRAME_DROPPED"; break;
            case DSV4L2_EVENT_TEMPEST_TRANSITION:   event_name = "TEMPEST_TRANSITION"; break;
            case DSV4L2_EVENT_TEMPEST_QUERY:        event_name = "TEMPEST_QUERY"; break;
            case DSV4L2_EVENT_TEMPEST_LOCKDOWN:     event_name = "TEMPEST_LOCKDOWN"; break;
            case DSV4L2_EVENT_FORMAT_CHANGE:        event_name = "FORMAT_CHANGE"; break;
            case DSV4L2_EVENT_RESOLUTION_CHANGE:    event_name = "RESOLUTION_CHANGE"; break;
            case DSV4L2_EVENT_ERROR:                event_name = "ERROR"; break;
            case DSV4L2_EVENT_POLICY_VIOLATION:     event_name = "POLICY_VIOLATION"; break;
            default:                                event_name = "UNKNOWN"; break;
        }

        /* Map severity */
        switch (ev->severity) {
            case DSV4L2_SEV_DEBUG:    severity_name = "DEBUG"; break;
            case DSV4L2_SEV_INFO:     severity_name = "INFO"; break;
            case DSV4L2_SEV_MEDIUM:   severity_name = "MEDIUM"; break;
            case DSV4L2_SEV_HIGH:     severity_name = "HIGH"; break;
            case DSV4L2_SEV_CRITICAL: severity_name = "CRITICAL"; break;
        }

        fprintf(stderr, "[DSV4L2] %s [%s] dev=%08x aux=%u role=%s\n",
                event_name, severity_name, ev->dev_id, ev->aux, ev->role);
    }
}

/**
 * Emit a simple event (convenience wrapper)
 */
void dsv4l2rt_emit_simple(uint32_t dev_id,
                          dsv4l2_event_type_t type,
                          dsv4l2_severity_t severity,
                          uint32_t aux)
{
    dsv4l2_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.dev_id = dev_id;
    ev.event_type = type;
    ev.severity = severity;
    ev.aux = aux;

    /* Get timestamp */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        ev.ts_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }

    dsv4l2rt_emit(&ev);
}

/**
 * Flush events (stub - nothing to flush for now)
 */
void dsv4l2rt_flush(void)
{
    /* No-op in stub */
}

/**
 * Shutdown runtime (stub)
 */
void dsv4l2rt_shutdown(void)
{
    if (!runtime.initialized) {
        return;
    }

    pthread_mutex_destroy(&runtime.lock);
    runtime.initialized = 0;
}

/**
 * Get current profile level
 */
dsv4l2_profile_t dsv4l2rt_get_profile(void)
{
    return runtime.profile;
}

/**
 * Get runtime statistics
 */
void dsv4l2rt_get_stats(dsv4l2rt_stats_t *stats)
{
    if (!stats) {
        return;
    }

    pthread_mutex_lock(&runtime.lock);
    stats->events_emitted = runtime.events_emitted;
    stats->events_dropped = runtime.events_dropped;
    stats->events_flushed = 0;  /* Stub doesn't flush */
    stats->buffer_usage = 0;
    stats->buffer_capacity = 0;
    pthread_mutex_unlock(&runtime.lock);
}

/**
 * Register custom sink (stub - not implemented)
 */
int dsv4l2rt_register_sink(dsv4l2rt_sink_fn sink, void *user_data)
{
    (void)sink;
    (void)user_data;
    return -1;  /* Not implemented in stub */
}

/**
 * Get signed event chunk (stub - not implemented)
 */
int dsv4l2rt_get_signed_chunk(dsv4l2rt_chunk_header_t *header,
                               dsv4l2_event_t **events,
                               size_t *count)
{
    (void)header;
    (void)events;
    (void)count;
    return -1;  /* Not implemented in stub */
}
