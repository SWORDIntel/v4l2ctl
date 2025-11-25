/*
 * DSV4L2 Metadata Capture Implementation
 *
 * V4L2_BUF_TYPE_META_CAPTURE support with KLV parsing,
 * IR radiometric decoding, and timestamp synchronization.
 */

#include "dsv4l2_metadata.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/* Metadata capture internal structure */
struct dsv4l2_metadata_capture {
    int                      fd;           /* Device fd */
    uint32_t                 dev_id;       /* Device ID for telemetry */
    dsv4l2_meta_format_t     format;       /* Expected format */

    /* Buffer management */
    void                    *buffers[4];   /* mmap'd metadata buffers */
    size_t                   buffer_sizes[4];
    uint32_t                 buffer_count;
    uint32_t                 sequence;     /* Frame sequence */
};

/* MISB STD 0601 UAS Datalink Local Set (16-byte Universal Label) */
const dsv4l2_klv_key_t DSV4L2_KLV_UAS_DATALINK_LS = {
    .bytes = {0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01,
              0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00}
};

/* Predefined KLV keys (simplified - real implementation would have full tags) */
const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_LATITUDE = {
    .bytes = {0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x03,
              0x07, 0x01, 0x02, 0x01, 0x02, 0x04, 0x02, 0x00}
};

const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_LONGITUDE = {
    .bytes = {0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x03,
              0x07, 0x01, 0x02, 0x01, 0x02, 0x04, 0x04, 0x00}
};

const dsv4l2_klv_key_t DSV4L2_KLV_SENSOR_ALTITUDE = {
    .bytes = {0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x03,
              0x07, 0x01, 0x02, 0x01, 0x02, 0x06, 0x02, 0x00}
};

/**
 * Open metadata capture stream
 */
DSV4L2_SENSOR("metadata_capture", "L3", "UNCLASSIFIED")
int dsv4l2_open_metadata(dsv4l2_device_t *dev,
                         dsv4l2_meta_format_t format,
                         dsv4l2_metadata_capture_t **out)
{
    dsv4l2_metadata_capture_t *meta_cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    uint32_t i;
    int rc;

    if (!dev || !out) {
        return -EINVAL;
    }

    /* Allocate metadata capture structure */
    meta_cap = calloc(1, sizeof(*meta_cap));
    if (!meta_cap) {
        return -ENOMEM;
    }

    meta_cap->fd = dev->fd;
    meta_cap->dev_id = 0;  /* TODO: Get from device */
    meta_cap->format = format;
    meta_cap->buffer_count = 4;
    meta_cap->sequence = 0;

    /* Set metadata format */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_META_CAPTURE;

    /* Query current format */
    if (ioctl(meta_cap->fd, VIDIOC_G_FMT, &fmt) < 0) {
        /* Device may not support metadata capture */
        rc = -errno;
        free(meta_cap);
        return rc;
    }

    /* Request metadata buffers */
    memset(&req, 0, sizeof(req));
    req.count = meta_cap->buffer_count;
    req.type = V4L2_BUF_TYPE_META_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(meta_cap->fd, VIDIOC_REQBUFS, &req) < 0) {
        rc = -errno;
        free(meta_cap);
        return rc;
    }

    /* Memory map metadata buffers */
    for (i = 0; i < meta_cap->buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_META_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(meta_cap->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            rc = -errno;
            /* Cleanup mapped buffers */
            while (i > 0) {
                i--;
                munmap(meta_cap->buffers[i], meta_cap->buffer_sizes[i]);
            }
            free(meta_cap);
            return rc;
        }

        meta_cap->buffer_sizes[i] = buf.length;
        meta_cap->buffers[i] = mmap(NULL, buf.length,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED,
                                     meta_cap->fd, buf.m.offset);

        if (meta_cap->buffers[i] == MAP_FAILED) {
            rc = -errno;
            /* Cleanup mapped buffers */
            while (i > 0) {
                i--;
                munmap(meta_cap->buffers[i], meta_cap->buffer_sizes[i]);
            }
            free(meta_cap);
            return rc;
        }

        /* Queue buffer */
        if (ioctl(meta_cap->fd, VIDIOC_QBUF, &buf) < 0) {
            rc = -errno;
            /* Cleanup all buffers */
            for (i = 0; i <= buf.index; i++) {
                munmap(meta_cap->buffers[i], meta_cap->buffer_sizes[i]);
            }
            free(meta_cap);
            return rc;
        }
    }

    /* Start metadata streaming */
    i = V4L2_BUF_TYPE_META_CAPTURE;
    if (ioctl(meta_cap->fd, VIDIOC_STREAMON, &i) < 0) {
        rc = -errno;
        for (i = 0; i < meta_cap->buffer_count; i++) {
            munmap(meta_cap->buffers[i], meta_cap->buffer_sizes[i]);
        }
        free(meta_cap);
        return rc;
    }

    /* Emit metadata stream open event */
    dsv4l2rt_emit_simple(meta_cap->dev_id, DSV4L2_EVENT_DEVICE_OPEN,
                         DSV4L2_SEV_INFO, format);

    *out = meta_cap;
    return 0;
}

/**
 * Close metadata capture stream
 */
void dsv4l2_close_metadata(dsv4l2_metadata_capture_t *meta_cap)
{
    uint32_t i;
    int type;

    if (!meta_cap) {
        return;
    }

    /* Stop streaming */
    type = V4L2_BUF_TYPE_META_CAPTURE;
    ioctl(meta_cap->fd, VIDIOC_STREAMOFF, &type);

    /* Unmap buffers */
    for (i = 0; i < meta_cap->buffer_count; i++) {
        if (meta_cap->buffers[i]) {
            munmap(meta_cap->buffers[i], meta_cap->buffer_sizes[i]);
        }
    }

    /* Emit metadata stream close event */
    dsv4l2rt_emit_simple(meta_cap->dev_id, DSV4L2_EVENT_DEVICE_CLOSE,
                         DSV4L2_SEV_INFO, 0);

    free(meta_cap);
}

/**
 * Capture metadata buffer
 */
DSV4L2_SENSOR("metadata_capture", "L3", "UNCLASSIFIED")
int dsv4l2_capture_metadata(dsv4l2_metadata_capture_t *meta_cap,
                             dsv4l2_metadata_t *out)
{
    struct v4l2_buffer buf;
    int rc = 0;

    if (!meta_cap || !out) {
        return -EINVAL;
    }

    /* Dequeue metadata buffer */
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_META_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(meta_cap->fd, VIDIOC_DQBUF, &buf) < 0) {
        return -errno;
    }

    /* Fill output metadata */
    memset(out, 0, sizeof(*out));
    out->format = meta_cap->format;
    out->timestamp_ns = buf.timestamp.tv_sec * 1000000000ULL +
                        buf.timestamp.tv_usec * 1000ULL;
    out->sequence = buf.sequence;

    /* Copy metadata based on format */
    switch (meta_cap->format) {
    case DSV4L2_META_FORMAT_KLV:
        /* Copy KLV data */
        out->data.klv.length = buf.bytesused;
        out->data.klv.data = malloc(buf.bytesused);
        if (!out->data.klv.data) {
            rc = -ENOMEM;
            goto requeue;
        }
        memcpy(out->data.klv.data, meta_cap->buffers[buf.index], buf.bytesused);
        out->data.klv.timestamp_ns = out->timestamp_ns;
        out->data.klv.sequence = buf.sequence;
        break;

    case DSV4L2_META_FORMAT_IR_TEMP:
    case DSV4L2_META_FORMAT_TELEMETRY:
    case DSV4L2_META_FORMAT_TIMING:
        /* TODO: Format-specific parsing */
        break;

    default:
        rc = -ENOTSUP;
        goto requeue;
    }

    /* Emit metadata capture event */
    dsv4l2rt_emit_simple(meta_cap->dev_id, DSV4L2_EVENT_FRAME_ACQUIRED,
                         DSV4L2_SEV_DEBUG, buf.sequence);

requeue:
    /* Re-queue buffer */
    if (ioctl(meta_cap->fd, VIDIOC_QBUF, &buf) < 0) {
        /* Re-queue failed - problematic but continue */
    }

    return rc;
}

/**
 * Parse KLV metadata
 *
 * Implements basic BER (Basic Encoding Rules) length parsing
 */
int dsv4l2_parse_klv(const dsv4l2_klv_buffer_t *buffer,
                     dsv4l2_klv_item_t **items,
                     size_t *count)
{
    dsv4l2_klv_item_t *item_array = NULL;
    size_t item_count = 0;
    size_t item_capacity = 16;
    size_t pos = 0;

    if (!buffer || !items || !count) {
        return -EINVAL;
    }

    /* Allocate initial item array */
    item_array = malloc(item_capacity * sizeof(dsv4l2_klv_item_t));
    if (!item_array) {
        return -ENOMEM;
    }

    /* Parse KLV triplets */
    while (pos + 16 + 1 < buffer->length) {  /* Key (16) + Length (1+) */
        dsv4l2_klv_item_t *item;
        uint32_t length;
        uint8_t length_byte;

        /* Expand array if needed */
        if (item_count >= item_capacity) {
            size_t new_capacity = item_capacity * 2;
            dsv4l2_klv_item_t *new_array = realloc(item_array,
                                                     new_capacity * sizeof(dsv4l2_klv_item_t));
            if (!new_array) {
                free(item_array);
                return -ENOMEM;
            }
            item_array = new_array;
            item_capacity = new_capacity;
        }

        item = &item_array[item_count];

        /* Parse key (16 bytes) */
        memcpy(item->key.bytes, &buffer->data[pos], 16);
        pos += 16;

        /* Parse length (BER encoding) */
        length_byte = buffer->data[pos++];
        if (length_byte & 0x80) {
            /* Long form length */
            uint32_t num_bytes = length_byte & 0x7F;
            if (num_bytes > 4 || pos + num_bytes > buffer->length) {
                free(item_array);
                return -EINVAL;
            }

            length = 0;
            for (uint32_t i = 0; i < num_bytes; i++) {
                length = (length << 8) | buffer->data[pos++];
            }
        } else {
            /* Short form length */
            length = length_byte;
        }

        /* Validate length */
        if (pos + length > buffer->length) {
            free(item_array);
            return -EINVAL;
        }

        /* Store value pointer */
        item->length = length;
        item->value = &buffer->data[pos];
        pos += length;

        item_count++;
    }

    *items = item_array;
    *count = item_count;
    return 0;
}

/**
 * Find KLV item by key
 */
const dsv4l2_klv_item_t *dsv4l2_find_klv_item(const dsv4l2_klv_item_t *items,
                                               size_t count,
                                               const dsv4l2_klv_key_t *key)
{
    size_t i;

    if (!items || !key) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        if (memcmp(items[i].key.bytes, key->bytes, 16) == 0) {
            return &items[i];
        }
    }

    return NULL;
}

/**
 * Decode IR radiometric data
 */
DSV4L2_SENSOR("ir_sensor", "L3", "CONFIDENTIAL")
int dsv4l2_decode_ir_radiometric(const uint16_t *raw_data,
                                  uint32_t width,
                                  uint32_t height,
                                  const float *calibration,
                                  dsv4l2_ir_radiometric_t *out)
{
    uint32_t i, num_pixels;
    float c1, c2;

    if (!raw_data || !calibration || !out) {
        return -EINVAL;
    }

    num_pixels = width * height;

    /* Allocate temperature map */
    out->temp_map = malloc(num_pixels * sizeof(uint16_t));
    if (!out->temp_map) {
        return -ENOMEM;
    }

    /* Get calibration constants */
    c1 = calibration[0];
    c2 = calibration[1];

    /* Convert raw values to temperature (Kelvin * 100) */
    for (i = 0; i < num_pixels; i++) {
        float raw_val = (float)raw_data[i];
        float temp_kelvin;

        /* Simple linear calibration: T = c1 * raw + c2 */
        temp_kelvin = c1 * raw_val + c2;

        /* Clamp to reasonable range (0-500K) */
        if (temp_kelvin < 0.0f) temp_kelvin = 0.0f;
        if (temp_kelvin > 500.0f) temp_kelvin = 500.0f;

        out->temp_map[i] = (uint16_t)(temp_kelvin * 100.0f);
    }

    out->width = width;
    out->height = height;
    out->emissivity = 0.95f;  /* Default */
    out->ambient_temp = 293.15f;  /* 20°C */
    out->calibration_c1 = c1;
    out->calibration_c2 = c2;

    /* Emit IR decode event */
    dsv4l2rt_emit_simple(0, DSV4L2_EVENT_FRAME_ACQUIRED,
                         DSV4L2_SEV_DEBUG, num_pixels);

    return 0;
}

/**
 * Synchronize frame and metadata timestamps
 */
int dsv4l2_sync_metadata(uint64_t frame_ts,
                          const dsv4l2_metadata_t *meta_buffers,
                          size_t count)
{
    int best_idx = -1;
    uint64_t best_delta = UINT64_MAX;
    uint64_t threshold_ns = 50000000;  /* 50ms threshold */
    size_t i;

    if (!meta_buffers || count == 0) {
        return -1;
    }

    /* Find metadata buffer with closest timestamp */
    for (i = 0; i < count; i++) {
        uint64_t meta_ts = meta_buffers[i].timestamp_ns;
        uint64_t delta;

        if (meta_ts > frame_ts) {
            delta = meta_ts - frame_ts;
        } else {
            delta = frame_ts - meta_ts;
        }

        if (delta < best_delta) {
            best_delta = delta;
            best_idx = (int)i;
        }
    }

    /* Check if within threshold */
    if (best_idx >= 0 && best_delta > threshold_ns) {
        return -1;  /* No match within threshold */
    }

    return best_idx;
}
