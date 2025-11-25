/*
 * DSV4L2 Metadata Capture
 *
 * Support for V4L2_BUF_TYPE_META_CAPTURE with KLV decoding,
 * IR radiometric data, and sensor fusion primitives.
 *
 * Metadata Types:
 * - KLV (Key-Length-Value): MISB STD 0601/0603 metadata
 * - IR Radiometric: Temperature maps, emissivity, calibration
 * - Telemetry: GPS, IMU, compass, altitude
 * - Timing: Frame timestamps, sync tokens
 */

#ifndef DSV4L2_METADATA_H
#define DSV4L2_METADATA_H

#include "dsv4l2_annotations.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Metadata format types */
typedef enum {
    DSV4L2_META_FORMAT_UNKNOWN  = 0,
    DSV4L2_META_FORMAT_KLV      = 1,  /* MISB KLV metadata */
    DSV4L2_META_FORMAT_IR_TEMP  = 2,  /* IR temperature map */
    DSV4L2_META_FORMAT_TELEMETRY = 3, /* GPS/IMU telemetry */
    DSV4L2_META_FORMAT_TIMING   = 4,  /* Timestamp sync */
} dsv4l2_meta_format_t;

/* KLV metadata buffer */
typedef struct {
    uint8_t  *data;           /* Raw KLV data */
    size_t    length;         /* Data length in bytes */
    uint64_t  timestamp_ns;   /* Capture timestamp */
    uint32_t  sequence;       /* Frame sequence number */
} dsv4l2_klv_buffer_t;

/* KLV key (16-byte UL) */
typedef struct {
    uint8_t bytes[16];
} dsv4l2_klv_key_t;

/* Parsed KLV item */
typedef struct {
    dsv4l2_klv_key_t key;     /* Universal Label */
    uint32_t         length;   /* Value length */
    const uint8_t   *value;    /* Value pointer (into buffer) */
} dsv4l2_klv_item_t;

/* IR radiometric data */
typedef struct {
    uint16_t *temp_map;        /* Temperature map (Kelvin * 100) */
    uint32_t  width;           /* Map width */
    uint32_t  height;          /* Map height */
    float     emissivity;      /* Surface emissivity (0.0-1.0) */
    float     ambient_temp;    /* Ambient temperature (Kelvin) */
    float     calibration_c1;  /* Calibration constant 1 */
    float     calibration_c2;  /* Calibration constant 2 */
    uint64_t  timestamp_ns;    /* Capture timestamp */
} dsv4l2_ir_radiometric_t;

/* Telemetry data */
typedef struct {
    double   latitude;         /* Degrees, WGS84 */
    double   longitude;        /* Degrees, WGS84 */
    float    altitude;         /* Meters above MSL */
    float    heading;          /* Degrees true north */
    float    pitch;            /* Degrees */
    float    roll;             /* Degrees */
    float    velocity[3];      /* m/s (x, y, z) */
    uint64_t timestamp_ns;     /* Capture timestamp */
} dsv4l2_telemetry_t;

/* Metadata buffer (generic container) */
typedef struct {
    dsv4l2_meta_format_t format;
    uint64_t             timestamp_ns;
    uint32_t             sequence;
    union {
        dsv4l2_klv_buffer_t    klv;
        dsv4l2_ir_radiometric_t ir;
        dsv4l2_telemetry_t     telemetry;
    } data;
} dsv4l2_metadata_t;

/* Metadata capture handle */
typedef struct dsv4l2_metadata_capture dsv4l2_metadata_capture_t;

/**
 * Open metadata capture stream
 *
 * Opens a V4L2_BUF_TYPE_META_CAPTURE stream for the device.
 * Must be called after dsv4l2_open().
 *
 * @param dev Device handle
 * @param format Expected metadata format
 * @param out Output metadata capture handle
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("metadata_capture", "L3", "UNCLASSIFIED")
int dsv4l2_open_metadata(dsv4l2_device_t *dev,
                         dsv4l2_meta_format_t format,
                         dsv4l2_metadata_capture_t **out);

/**
 * Close metadata capture stream
 *
 * @param meta_cap Metadata capture handle
 */
void dsv4l2_close_metadata(dsv4l2_metadata_capture_t *meta_cap);

/**
 * Capture metadata buffer
 *
 * Dequeues a metadata buffer. Blocks if no buffer available.
 *
 * @param meta_cap Metadata capture handle
 * @param out Output metadata buffer
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("metadata_capture", "L3", "UNCLASSIFIED")
int dsv4l2_capture_metadata(dsv4l2_metadata_capture_t *meta_cap,
                             dsv4l2_metadata_t *out);

/**
 * Parse KLV metadata
 *
 * Parses raw KLV buffer into individual items.
 *
 * @param buffer Raw KLV data
 * @param items Output array of KLV items (caller must free)
 * @param count Output item count
 * @return 0 on success, negative errno on error
 */
int dsv4l2_parse_klv(const dsv4l2_klv_buffer_t *buffer,
                     dsv4l2_klv_item_t **items,
                     size_t *count);

/**
 * Get KLV item by key
 *
 * Searches parsed KLV items for specific key.
 *
 * @param items KLV items array
 * @param count Item count
 * @param key Key to search for
 * @return Pointer to item, or NULL if not found
 */
const dsv4l2_klv_item_t *dsv4l2_find_klv_item(const dsv4l2_klv_item_t *items,
                                               size_t count,
                                               const dsv4l2_klv_key_t *key);

/**
 * Decode IR radiometric data
 *
 * Converts raw IR sensor data to temperature map using calibration.
 *
 * @param raw_data Raw sensor data
 * @param width Image width
 * @param height Image height
 * @param calibration Device calibration data
 * @param out Output radiometric data
 * @return 0 on success, negative errno on error
 */
DSV4L2_SENSOR("ir_sensor", "L3", "CONFIDENTIAL")
int dsv4l2_decode_ir_radiometric(const uint16_t *raw_data,
                                  uint32_t width,
                                  uint32_t height,
                                  const float *calibration,
                                  dsv4l2_ir_radiometric_t *out);

/**
 * Synchronize frame and metadata timestamps
 *
 * Finds metadata buffer closest to frame timestamp for fusion.
 *
 * @param frame_ts Frame timestamp (nanoseconds)
 * @param meta_buffers Array of metadata buffers
 * @param count Number of metadata buffers
 * @return Index of closest metadata buffer, or -1 if none within threshold
 */
int dsv4l2_sync_metadata(uint64_t frame_ts,
                          const dsv4l2_metadata_t *meta_buffers,
                          size_t count);

/* Predefined KLV keys (MISB STD 0601) */
extern const dsv4l2_klv_key_t DSV4L2_KLV_UAS_DATALINK_LS;  /* UAS Datalink LS */
extern const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_LATITUDE;  /* Tag 13 */
extern const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_LONGITUDE; /* Tag 14 */
extern const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_ALTITUDE;  /* Tag 15 */

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_METADATA_H */
