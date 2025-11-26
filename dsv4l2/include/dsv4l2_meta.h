#ifndef DSV4L2_META_H
#define DSV4L2_META_H

#include "dsv4l2_annotations.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Metadata handle
 */
typedef struct dsv4l2_meta_handle dsv4l2_meta_handle_t;

/**
 * Metadata packet
 */
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
    uint32_t format;  /* FLIR, KLV, RAW, etc. */
} DSMIL_META("radiometric") dsv4l2_meta_t;

/**
 * Metadata format types
 */
#define DSV4L2_META_FORMAT_RAW   0
#define DSV4L2_META_FORMAT_FLIR  1
#define DSV4L2_META_FORMAT_KLV   2

/**
 * Open metadata stream
 *
 * @param device_path Path to metadata device (e.g., "/dev/video2")
 * @param out_handle Output metadata handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_meta_open(
    const char *device_path,
    dsv4l2_meta_handle_t **out_handle
);

/**
 * Close metadata stream
 *
 * @param handle Metadata handle
 */
void dsv4l2_meta_close(dsv4l2_meta_handle_t *handle);

/**
 * Read metadata packet
 *
 * @param handle Metadata handle
 * @param out_meta Output metadata packet
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_meta_read(
    dsv4l2_meta_handle_t *handle,
    dsv4l2_meta_t *out_meta
);

/**
 * Start metadata streaming
 *
 * @param handle Metadata handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_meta_start_stream(dsv4l2_meta_handle_t *handle);

/**
 * Stop metadata streaming
 *
 * @param handle Metadata handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_meta_stop_stream(dsv4l2_meta_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_META_H */
