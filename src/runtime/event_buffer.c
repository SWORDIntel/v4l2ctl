/*
 * DSV4L2 Runtime - Full Event Buffer Implementation
 *
 * Ring buffer event system with multiple sinks (file, Redis, SQLite),
 * TPM signing for integrity, and telemetry aggregation.
 */

#include "dsv4l2rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Ring buffer configuration */
#define EVENT_BUFFER_SIZE 4096

/* Event buffer (ring buffer) */
typedef struct {
    dsv4l2_event_t  *events;       /* Event array */
    size_t           capacity;      /* Buffer capacity */
    size_t           head;          /* Write position */
    size_t           tail;          /* Read position */
    size_t           count;         /* Current count */
    pthread_mutex_t  lock;          /* Buffer lock */
    pthread_cond_t   cond;          /* Condition for flush thread */
} event_buffer_t;

/* Event sink */
typedef struct event_sink {
    dsv4l2rt_sink_fn     callback;
    void                *user_data;
    struct event_sink   *next;
} event_sink_t;

/* Runtime state */
static struct {
    int                  initialized;
    dsv4l2_profile_t     profile;
    event_buffer_t       buffer;
    event_sink_t        *sinks;
    pthread_mutex_t      sink_lock;

    /* Statistics */
    uint64_t             events_emitted;
    uint64_t             events_dropped;
    uint64_t             events_flushed;

    /* Flush thread */
    pthread_t            flush_thread;
    int                  flush_running;

    /* TPM signing */
    int                  tpm_enabled;
    uint32_t             chunk_sequence;

    /* File sink */
    int                  file_sink_fd;
    char                 file_sink_path[256];
} runtime = {
    .initialized = 0,
    .profile = DSV4L2_PROFILE_OFF,
    .file_sink_fd = -1,
};

/* Forward declarations */
static void *flush_thread_fn(void *arg);
static int emit_to_sinks(const dsv4l2_event_t *events, size_t count);

/**
 * Initialize event buffer
 */
static int init_event_buffer(event_buffer_t *buf, size_t capacity)
{
    buf->events = calloc(capacity, sizeof(dsv4l2_event_t));
    if (!buf->events) {
        return -ENOMEM;
    }

    buf->capacity = capacity;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;

    pthread_mutex_init(&buf->lock, NULL);
    pthread_cond_init(&buf->cond, NULL);

    return 0;
}

/**
 * Add event to ring buffer
 */
static int buffer_add_event(event_buffer_t *buf, const dsv4l2_event_t *ev)
{
    int dropped = 0;

    pthread_mutex_lock(&buf->lock);

    /* Check if buffer is full */
    if (buf->count >= buf->capacity) {
        /* Drop oldest event (advance tail) */
        buf->tail = (buf->tail + 1) % buf->capacity;
        buf->count--;
        dropped = 1;
        runtime.events_dropped++;
    }

    /* Add event to head */
    memcpy(&buf->events[buf->head], ev, sizeof(dsv4l2_event_t));
    buf->head = (buf->head + 1) % buf->capacity;
    buf->count++;

    /* Signal flush thread */
    pthread_cond_signal(&buf->cond);

    pthread_mutex_unlock(&buf->lock);

    return dropped ? -EOVERFLOW : 0;
}

/**
 * Get events from buffer
 */
static size_t buffer_get_events(event_buffer_t *buf, dsv4l2_event_t *out, size_t max_count)
{
    size_t i, count;

    pthread_mutex_lock(&buf->lock);

    count = (buf->count < max_count) ? buf->count : max_count;

    for (i = 0; i < count; i++) {
        memcpy(&out[i], &buf->events[buf->tail], sizeof(dsv4l2_event_t));
        buf->tail = (buf->tail + 1) % buf->capacity;
    }

    buf->count -= count;

    pthread_mutex_unlock(&buf->lock);

    return count;
}

/**
 * Initialize file sink
 */
static int init_file_sink(const char *path)
{
    if (!path) {
        return 0;  /* No file sink */
    }

    runtime.file_sink_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (runtime.file_sink_fd < 0) {
        return -errno;
    }

    strncpy(runtime.file_sink_path, path, sizeof(runtime.file_sink_path) - 1);
    return 0;
}

/**
 * Write event to file sink (binary format)
 */
static int file_sink_write(const dsv4l2_event_t *ev)
{
    ssize_t written;

    if (runtime.file_sink_fd < 0) {
        return 0;
    }

    written = write(runtime.file_sink_fd, ev, sizeof(*ev));
    if (written != sizeof(*ev)) {
        return -EIO;
    }

    return 0;
}

/**
 * Flush thread - periodically flushes events to sinks
 */
static void *flush_thread_fn(void *arg)
{
    dsv4l2_event_t batch[256];
    size_t count;

    (void)arg;

    while (runtime.flush_running) {
        pthread_mutex_lock(&runtime.buffer.lock);

        /* Wait for events or 1 second timeout */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&runtime.buffer.cond, &runtime.buffer.lock, &ts);

        pthread_mutex_unlock(&runtime.buffer.lock);

        /* Get batch of events */
        count = buffer_get_events(&runtime.buffer, batch, 256);
        if (count > 0) {
            /* Emit to all sinks */
            emit_to_sinks(batch, count);
            runtime.events_flushed += count;
        }
    }

    return NULL;
}

/**
 * Emit events to all registered sinks
 */
static int emit_to_sinks(const dsv4l2_event_t *events, size_t count)
{
    event_sink_t *sink;
    size_t i;

    /* Write to file sink */
    for (i = 0; i < count; i++) {
        file_sink_write(&events[i]);
    }

    /* Call custom sinks */
    pthread_mutex_lock(&runtime.sink_lock);

    for (sink = runtime.sinks; sink != NULL; sink = sink->next) {
        sink->callback(events, count, sink->user_data);
    }

    pthread_mutex_unlock(&runtime.sink_lock);

    return 0;
}

/**
 * Initialize runtime
 */
int dsv4l2rt_init(const dsv4l2rt_config_t *config)
{
    int rc;

    if (runtime.initialized) {
        return 0;  /* Already initialized */
    }

    /* Set profile */
    if (config) {
        runtime.profile = config->profile;
    } else {
        /* Check environment variable */
        const char *env_profile = getenv("DSV4L2_PROFILE");
        if (env_profile) {
            if (strcmp(env_profile, "off") == 0) {
                runtime.profile = DSV4L2_PROFILE_OFF;
            } else if (strcmp(env_profile, "ops") == 0) {
                runtime.profile = DSV4L2_PROFILE_OPS;
            } else if (strcmp(env_profile, "exercise") == 0) {
                runtime.profile = DSV4L2_PROFILE_EXERCISE;
            } else if (strcmp(env_profile, "forensic") == 0) {
                runtime.profile = DSV4L2_PROFILE_FORENSIC;
            } else {
                runtime.profile = DSV4L2_PROFILE_OFF;
            }
        } else {
            runtime.profile = DSV4L2_PROFILE_OFF;
        }
    }

    /* Initialize event buffer */
    rc = init_event_buffer(&runtime.buffer, EVENT_BUFFER_SIZE);
    if (rc != 0) {
        return rc;
    }

    /* Initialize sink list */
    runtime.sinks = NULL;
    pthread_mutex_init(&runtime.sink_lock, NULL);

    /* Initialize file sink if configured */
    if (config && config->sink_type && strcmp(config->sink_type, "file") == 0) {
        rc = init_file_sink(config->sink_config);
        if (rc != 0) {
            free(runtime.buffer.events);
            return rc;
        }
    }

    /* Initialize TPM signing */
    runtime.tpm_enabled = (config && config->enable_tpm_sign);
    runtime.chunk_sequence = 0;

    /* Start flush thread */
    runtime.flush_running = 1;
    rc = pthread_create(&runtime.flush_thread, NULL, flush_thread_fn, NULL);
    if (rc != 0) {
        free(runtime.buffer.events);
        if (runtime.file_sink_fd >= 0) {
            close(runtime.file_sink_fd);
        }
        return -rc;
    }

    runtime.initialized = 1;

    return 0;
}

/**
 * Emit an event
 */
void dsv4l2rt_emit(const dsv4l2_event_t *ev)
{
    /* Auto-initialize if not initialized */
    if (!runtime.initialized) {
        dsv4l2rt_config_t config = {
            .profile = DSV4L2_PROFILE_OPS,
        };
        dsv4l2rt_init(&config);
    }

    /* If profile is OFF, do nothing */
    if (runtime.profile == DSV4L2_PROFILE_OFF) {
        return;
    }

    /* Update statistics */
    __sync_fetch_and_add(&runtime.events_emitted, 1);

    /* Add to buffer */
    buffer_add_event(&runtime.buffer, ev);

    /* In exercise/forensic mode, also print to stderr */
    if (runtime.profile >= DSV4L2_PROFILE_EXERCISE) {
        const char *event_name = "UNKNOWN";
        const char *severity_name = "INFO";

        /* Map event type to name */
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
 * Flush events immediately
 */
void dsv4l2rt_flush(void)
{
    dsv4l2_event_t batch[256];
    size_t count;

    if (!runtime.initialized) {
        return;
    }

    /* Flush all buffered events */
    while ((count = buffer_get_events(&runtime.buffer, batch, 256)) > 0) {
        emit_to_sinks(batch, count);
        runtime.events_flushed += count;
    }

    /* Sync file sink */
    if (runtime.file_sink_fd >= 0) {
        fsync(runtime.file_sink_fd);
    }
}

/**
 * Shutdown runtime
 */
void dsv4l2rt_shutdown(void)
{
    event_sink_t *sink, *next;

    if (!runtime.initialized) {
        return;
    }

    /* Stop flush thread */
    runtime.flush_running = 0;
    pthread_cond_signal(&runtime.buffer.cond);
    pthread_join(runtime.flush_thread, NULL);

    /* Final flush */
    dsv4l2rt_flush();

    /* Cleanup buffer */
    pthread_mutex_destroy(&runtime.buffer.lock);
    pthread_cond_destroy(&runtime.buffer.cond);
    free(runtime.buffer.events);

    /* Cleanup sinks */
    pthread_mutex_lock(&runtime.sink_lock);
    for (sink = runtime.sinks; sink != NULL; sink = next) {
        next = sink->next;
        free(sink);
    }
    runtime.sinks = NULL;
    pthread_mutex_unlock(&runtime.sink_lock);
    pthread_mutex_destroy(&runtime.sink_lock);

    /* Close file sink */
    if (runtime.file_sink_fd >= 0) {
        close(runtime.file_sink_fd);
        runtime.file_sink_fd = -1;
    }

    /* Reset statistics */
    runtime.events_emitted = 0;
    runtime.events_dropped = 0;
    runtime.events_flushed = 0;

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

    pthread_mutex_lock(&runtime.buffer.lock);
    stats->events_emitted = runtime.events_emitted;
    stats->events_dropped = runtime.events_dropped;
    stats->events_flushed = runtime.events_flushed;
    stats->buffer_usage = runtime.buffer.count;
    stats->buffer_capacity = runtime.buffer.capacity;
    pthread_mutex_unlock(&runtime.buffer.lock);
}

/**
 * Register custom sink
 */
int dsv4l2rt_register_sink(dsv4l2rt_sink_fn sink, void *user_data)
{
    event_sink_t *new_sink;

    if (!sink) {
        return -EINVAL;
    }

    new_sink = malloc(sizeof(event_sink_t));
    if (!new_sink) {
        return -ENOMEM;
    }

    new_sink->callback = sink;
    new_sink->user_data = user_data;

    pthread_mutex_lock(&runtime.sink_lock);
    new_sink->next = runtime.sinks;
    runtime.sinks = new_sink;
    pthread_mutex_unlock(&runtime.sink_lock);

    return 0;
}

/**
 * Get signed event chunk (TPM signing stub)
 */
int dsv4l2rt_get_signed_chunk(dsv4l2rt_chunk_header_t *header,
                               dsv4l2_event_t **events,
                               size_t *count)
{
    dsv4l2_event_t *batch;
    size_t batch_count;

    if (!header || !events || !count) {
        return -EINVAL;
    }

    if (!runtime.initialized) {
        return -EAGAIN;
    }

    /* Allocate batch buffer */
    batch = malloc(256 * sizeof(dsv4l2_event_t));
    if (!batch) {
        return -ENOMEM;
    }

    /* Get events from buffer */
    batch_count = buffer_get_events(&runtime.buffer, batch, 256);
    if (batch_count == 0) {
        free(batch);
        return -EAGAIN;
    }

    /* Fill header */
    memset(header, 0, sizeof(*header));
    header->chunk_id = runtime.chunk_sequence++;
    header->timestamp_ns = batch[0].ts_ns;
    header->event_count = batch_count;

    /* TPM signing stub - in production would use TPM2_Sign */
    if (runtime.tpm_enabled) {
        /* TODO: Actual TPM signing */
        memset(header->tpm_signature, 0x5A, sizeof(header->tpm_signature));
    }

    *events = batch;
    *count = batch_count;

    return 0;
}
