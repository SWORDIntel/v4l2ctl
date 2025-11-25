#ifndef DSV4L2RT_H
#define DSV4L2RT_H

/*
 * DSV4L2 Runtime - DSLLVM instrumentation runtime for v4l2 sensor telemetry
 *
 * This runtime provides event buffering and telemetry integration for
 * DSLLVM-instrumented sensor code. It mirrors the SHRINK pattern:
 *   - Per-process ring buffers for events
 *   - Structured event types with timestamps and severity
 *   - Integration into DSMIL fabric (Redis/SQLite/telemetry mesh)
 *   - Optional TPM-signed event chunks for forensic integrity
 *
 * Controlled via DSLLVM compile flags:
 *   -fdsv4l2-profile=off|ops|exercise|forensic
 *   -fdsv4l2-metadata       (static metadata only, no runtime)
 *   -mdsv4l2-mission=<name> (mission context tagging)
 */

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Event Types
 * ======================================================================== */

typedef enum {
    DSV4L2_EVENT_DEVICE_OPEN          = 0x0001,
    DSV4L2_EVENT_DEVICE_CLOSE         = 0x0002,
    DSV4L2_EVENT_CAPTURE_START        = 0x0010,
    DSV4L2_EVENT_CAPTURE_STOP         = 0x0011,
    DSV4L2_EVENT_FRAME_ACQUIRED       = 0x0012,
    DSV4L2_EVENT_FRAME_DROPPED        = 0x0013,
    DSV4L2_EVENT_TEMPEST_TRANSITION   = 0x0020,
    DSV4L2_EVENT_TEMPEST_QUERY        = 0x0021,
    DSV4L2_EVENT_TEMPEST_LOCKDOWN     = 0x0022,
    DSV4L2_EVENT_FORMAT_CHANGE        = 0x0030,
    DSV4L2_EVENT_RESOLUTION_CHANGE    = 0x0031,
    DSV4L2_EVENT_FPS_CHANGE           = 0x0032,
    DSV4L2_EVENT_CONTROL_CHANGE       = 0x0033,
    DSV4L2_EVENT_IRIS_MODE_ENTER      = 0x0040,
    DSV4L2_EVENT_IRIS_MODE_EXIT       = 0x0041,
    DSV4L2_EVENT_IRIS_CAPTURE         = 0x0042,
    DSV4L2_EVENT_META_READ            = 0x0050,
    DSV4L2_EVENT_FUSED_CAPTURE        = 0x0051,
    DSV4L2_EVENT_ERROR                = 0x0100,
    DSV4L2_EVENT_POLICY_VIOLATION     = 0x0101,
    DSV4L2_EVENT_SECRET_LEAK_ATTEMPT  = 0x0102,
} dsv4l2_event_type_t;

typedef enum {
    DSV4L2_SEV_DEBUG    = 0,
    DSV4L2_SEV_INFO     = 1,
    DSV4L2_SEV_MEDIUM   = 2,
    DSV4L2_SEV_HIGH     = 3,
    DSV4L2_SEV_CRITICAL = 4,
} dsv4l2_severity_t;

/* ========================================================================
 * Event Structure
 * ======================================================================== */

typedef struct {
    uint64_t ts_ns;              // Nanosecond timestamp (CLOCK_MONOTONIC)
    uint32_t dev_id;             // Device ID (hash of path or profile ID)
    uint16_t event_type;         // dsv4l2_event_type_t
    uint16_t severity;           // dsv4l2_severity_t
    uint32_t aux;                // Event-specific data (state, error code, etc.)
    uint32_t layer;              // DSMIL layer (L0-L8)
    char     role[16];           // Device role (camera, iris_scanner, etc.)
    char     mission[32];        // Mission context (from -mdsv4l2-mission)
} dsv4l2_event_t;

/* ========================================================================
 * Runtime Configuration
 * ======================================================================== */

typedef enum {
    DSV4L2_PROFILE_OFF       = 0,  // No instrumentation
    DSV4L2_PROFILE_OPS       = 1,  // Minimal counters (frames, errors, transitions)
    DSV4L2_PROFILE_EXERCISE  = 2,  // Per-stream stats, metadata sampling
    DSV4L2_PROFILE_FORENSIC  = 3,  // Maximal logging (within policy)
} dsv4l2_profile_t;

typedef struct {
    dsv4l2_profile_t profile;
    const char      *mission;
    size_t           ring_buffer_size;  // Number of events to buffer
    int              enable_tpm_sign;   // TPM-signed event chunks for forensic integrity
    const char      *sink_type;         // "redis", "sqlite", "syslog", "file"
    const char      *sink_config;       // Connection string or path
} dsv4l2rt_config_t;

/* ========================================================================
 * Runtime API (called by DSLLVM-injected code)
 * ======================================================================== */

/**
 * Initialize the DSV4L2 runtime.
 * Called automatically at library load if -fdsv4l2-profile != off.
 */
int dsv4l2rt_init(const dsv4l2rt_config_t *config);

/**
 * Emit an event into the ring buffer.
 * DSLLVM-injected code calls this at instrumentation points.
 */
void dsv4l2rt_emit(const dsv4l2_event_t *ev);

/**
 * Emit a simple event with minimal overhead (ops profile).
 */
void dsv4l2rt_emit_simple(uint32_t dev_id,
                          dsv4l2_event_type_t type,
                          dsv4l2_severity_t severity,
                          uint32_t aux);

/**
 * Flush buffered events to the configured sink.
 * Can be called explicitly or triggered automatically by runtime.
 */
void dsv4l2rt_flush(void);

/**
 * Shutdown the runtime and flush remaining events.
 */
void dsv4l2rt_shutdown(void);

/* ========================================================================
 * Query API (for introspection)
 * ======================================================================== */

/**
 * Get current instrumentation profile level.
 */
dsv4l2_profile_t dsv4l2rt_get_profile(void);

/**
 * Get event buffer stats (for monitoring).
 */
typedef struct {
    uint64_t events_emitted;
    uint64_t events_dropped;
    uint64_t events_flushed;
    size_t   buffer_usage;
    size_t   buffer_capacity;
} dsv4l2rt_stats_t;

void dsv4l2rt_get_stats(dsv4l2rt_stats_t *stats);

/* ========================================================================
 * Integration Hooks (for DSMIL fabric)
 * ======================================================================== */

/**
 * Custom sink callback type.
 * Allows integration with Redis, SHRINK, MEMSHADOW, etc.
 */
typedef void (*dsv4l2rt_sink_fn)(const dsv4l2_event_t *events,
                                  size_t count,
                                  void *user_data);

/**
 * Register a custom sink for events.
 */
int dsv4l2rt_register_sink(dsv4l2rt_sink_fn sink, void *user_data);

/* ========================================================================
 * TPM / Forensic Support
 * ======================================================================== */

typedef struct {
    uint64_t chunk_id;
    uint64_t timestamp_ns;
    size_t   event_count;
    uint8_t  tpm_signature[256];  // TPM-signed hash of event chunk
} dsv4l2rt_chunk_header_t;

/**
 * Retrieve signed event chunk for forensic export.
 * Only available if enable_tpm_sign=1 in config.
 */
int dsv4l2rt_get_signed_chunk(dsv4l2rt_chunk_header_t *header,
                               dsv4l2_event_t **events,
                               size_t *count);

/**
 * Initialize TPM2 context and load signing key.
 * Only available when compiled with HAVE_TPM2.
 *
 * @param key_handle TPM2 persistent key handle (default: 0x81010001)
 * @return 0 on success, -errno on failure
 */
int dsv4l2_tpm_init(uint32_t key_handle);

/**
 * Cleanup TPM2 context.
 */
void dsv4l2_tpm_cleanup(void);

/**
 * Sign event chunk with TPM2 hardware.
 *
 * @param events Array of events to sign
 * @param count Number of events
 * @param signature Output buffer for signature (must be 256 bytes)
 * @return 0 on success, -errno on failure (-ENOSYS if TPM2 not available)
 */
int dsv4l2_tpm_sign_events(const dsv4l2_event_t *events, size_t count,
                            uint8_t signature[256]);

/**
 * Verify TPM2 signature for forensic validation.
 *
 * @param events Array of events that were signed
 * @param count Number of events
 * @param signature Signature to verify
 * @return 0 if valid, -EBADMSG if invalid, -errno on error
 */
int dsv4l2_tpm_verify_signature(const dsv4l2_event_t *events, size_t count,
                                 const uint8_t signature[256]);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2RT_H */
