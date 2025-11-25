/*
 * DSV4L2 Core Library API
 *
 * Additional functions beyond the policy API
 */

#ifndef DSV4L2_CORE_H
#define DSV4L2_CORE_H

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Device Management
 * ======================================================================== */

/**
 * Open a v4l2 device with role classification
 */
int dsv4l2_open(const char *path, const char *role, dsv4l2_device_t **out);

/**
 * Close a v4l2 device
 */
void dsv4l2_close(dsv4l2_device_t *dev);

/**
 * List all v4l2 devices on the system
 */
int dsv4l2_list_devices(dsv4l2_device_t ***devices, size_t *count);

/**
 * Get device capabilities
 */
int dsv4l2_get_capabilities(dsv4l2_device_t *dev, struct v4l2_capability *cap);

/**
 * Get device information (driver, card, bus)
 */
int dsv4l2_get_info(dsv4l2_device_t *dev,
                    char *driver, size_t driver_len,
                    char *card, size_t card_len,
                    char *bus, size_t bus_len);

/* ========================================================================
 * TEMPEST State Management
 * ======================================================================== */

/**
 * Get TEMPEST state name (for display/logging)
 */
const char *dsv4l2_tempest_state_name(dsv4l2_tempest_state_t state);

/* ========================================================================
 * Format and Resolution
 * ======================================================================== */

/**
 * Enumerate supported pixel formats
 */
int dsv4l2_enum_formats(dsv4l2_device_t *dev, uint32_t **formats, size_t *count);

/**
 * Get current video format
 */
int dsv4l2_get_format(dsv4l2_device_t *dev, struct v4l2_format *fmt);

/**
 * Set video format
 */
int dsv4l2_set_format(dsv4l2_device_t *dev, struct v4l2_format *fmt);

/**
 * Enumerate supported frame sizes for a pixel format
 */
int dsv4l2_enum_frame_sizes(dsv4l2_device_t *dev, uint32_t pixel_fmt,
                             uint32_t **widths, uint32_t **heights,
                             size_t *count);

/**
 * Set video resolution
 */
int dsv4l2_set_resolution(dsv4l2_device_t *dev, uint32_t width, uint32_t height);

/**
 * Get current resolution
 */
int dsv4l2_get_resolution(dsv4l2_device_t *dev, uint32_t *width, uint32_t *height);

/**
 * Get pixel format fourcc as string
 */
void dsv4l2_fourcc_to_string(uint32_t fourcc, char *str);

/* ========================================================================
 * Buffer Management
 * ======================================================================== */

/**
 * Request buffers from the device
 */
int dsv4l2_request_buffers(dsv4l2_device_t *dev, uint32_t count);

/**
 * Memory-map all buffers
 */
int dsv4l2_mmap_buffers(dsv4l2_device_t *dev);

/**
 * Queue a buffer for capture
 */
int dsv4l2_queue_buffer(dsv4l2_device_t *dev, uint32_t index);

/**
 * Dequeue a filled buffer
 */
int dsv4l2_dequeue_buffer(dsv4l2_device_t *dev, struct v4l2_buffer *buf);

/**
 * Get pointer to mapped buffer data
 */
int dsv4l2_get_buffer(dsv4l2_device_t *dev, uint32_t index,
                      void **start, size_t *length);

/**
 * Unmap and release all buffers
 */
void dsv4l2_release_buffers(dsv4l2_device_t *dev);

/* ========================================================================
 * Capture Operations
 * ======================================================================== */

/**
 * Start streaming
 */
int dsv4l2_start_streaming(dsv4l2_device_t *dev);

/**
 * Stop streaming
 */
int dsv4l2_stop_streaming(dsv4l2_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_CORE_H */
