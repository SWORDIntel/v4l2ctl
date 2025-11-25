/*
 * DSV4L2 Format and Resolution Management
 *
 * Handles video format configuration:
 * - Format enumeration and selection
 * - Resolution configuration
 * - Frame rate control
 * - Telemetry for format changes
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
typedef struct dsv4l2_device_internal dsv4l2_device_internal_t;
extern dsv4l2_device_internal_t *dsv4l2_get_internal(dsv4l2_device_t *dev);

/* Internal device structure (partial) */
struct dsv4l2_device_internal {
    dsv4l2_device_t public;
    struct v4l2_capability cap;
    dsv4l2_tempest_state_t tempest;
    int tempest_ctrl_id;
    char *profile_path;
    char *classification;
    int streaming;
    uint32_t dev_id;
    void *buffers;
    uint32_t buffer_count;
};

/**
 * Enumerate supported pixel formats
 *
 * @param dev Device handle
 * @param formats Output array of pixel format fourccs (caller must free)
 * @param count Output format count
 * @return 0 on success, negative errno on error
 */
int dsv4l2_enum_formats(dsv4l2_device_t *dev, uint32_t **formats, size_t *count)
{
    struct v4l2_fmtdesc fmt;
    uint32_t *fmt_list = NULL;
    size_t fmt_count = 0;
    size_t fmt_capacity = 16;

    if (!dev || !formats || !count) {
        return -EINVAL;
    }

    /* Allocate initial format list */
    fmt_list = calloc(fmt_capacity, sizeof(uint32_t));
    if (!fmt_list) {
        return -ENOMEM;
    }

    /* Enumerate formats */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmt.index = 0; ; fmt.index++) {
        if (ioctl(dev->fd, VIDIOC_ENUM_FMT, &fmt) < 0) {
            break;  /* No more formats */
        }

        fmt_list[fmt_count++] = fmt.pixelformat;

        /* Expand array if needed */
        if (fmt_count >= fmt_capacity) {
            fmt_capacity *= 2;
            fmt_list = realloc(fmt_list, fmt_capacity * sizeof(uint32_t));
            if (!fmt_list) {
                return -ENOMEM;
            }
        }
    }

    *formats = fmt_list;
    *count = fmt_count;

    return 0;
}

/**
 * Get current video format
 *
 * @param dev Device handle
 * @param fmt Output format structure
 * @return 0 on success, negative errno on error
 */
int dsv4l2_get_format(dsv4l2_device_t *dev, struct v4l2_format *fmt)
{
    if (!dev || !fmt) {
        return -EINVAL;
    }

    memset(fmt, 0, sizeof(*fmt));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(dev->fd, VIDIOC_G_FMT, fmt) < 0) {
        return -errno;
    }

    return 0;
}

/**
 * Set video format
 *
 * @param dev Device handle
 * @param fmt Format structure
 * @return 0 on success, negative errno on error
 */
int dsv4l2_set_format(dsv4l2_device_t *dev, struct v4l2_format *fmt)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_format old_fmt;

    if (!dev || !fmt) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    /* Get current format for comparison */
    dsv4l2_get_format(dev, &old_fmt);

    /* Set new format */
    if (ioctl(dev->fd, VIDIOC_S_FMT, fmt) < 0) {
        return -errno;
    }

    /* Emit format change event if pixel format changed */
    if (old_fmt.fmt.pix.pixelformat != fmt->fmt.pix.pixelformat) {
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_FORMAT_CHANGE,
                             DSV4L2_SEV_INFO, fmt->fmt.pix.pixelformat);
    }

    /* Emit resolution change event if resolution changed */
    if (old_fmt.fmt.pix.width != fmt->fmt.pix.width ||
        old_fmt.fmt.pix.height != fmt->fmt.pix.height) {
        uint32_t res = (fmt->fmt.pix.width << 16) | fmt->fmt.pix.height;
        dsv4l2rt_emit_simple(internal->dev_id, DSV4L2_EVENT_RESOLUTION_CHANGE,
                             DSV4L2_SEV_INFO, res);
    }

    return 0;
}

/**
 * Enumerate supported frame sizes for a pixel format
 *
 * @param dev Device handle
 * @param pixel_fmt Pixel format fourcc
 * @param widths Output array of widths (caller must free)
 * @param heights Output array of heights (caller must free)
 * @param count Output size count
 * @return 0 on success, negative errno on error
 */
int dsv4l2_enum_frame_sizes(dsv4l2_device_t *dev, uint32_t pixel_fmt,
                             uint32_t **widths, uint32_t **heights,
                             size_t *count)
{
    struct v4l2_frmsizeenum frmsize;
    uint32_t *width_list = NULL;
    uint32_t *height_list = NULL;
    size_t size_count = 0;
    size_t size_capacity = 16;

    if (!dev || !widths || !heights || !count) {
        return -EINVAL;
    }

    /* Allocate initial arrays */
    width_list = calloc(size_capacity, sizeof(uint32_t));
    height_list = calloc(size_capacity, sizeof(uint32_t));
    if (!width_list || !height_list) {
        free(width_list);
        free(height_list);
        return -ENOMEM;
    }

    /* Enumerate frame sizes */
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = pixel_fmt;

    for (frmsize.index = 0; ; frmsize.index++) {
        if (ioctl(dev->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) < 0) {
            break;  /* No more sizes */
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            width_list[size_count] = frmsize.discrete.width;
            height_list[size_count] = frmsize.discrete.height;
            size_count++;

            /* Expand arrays if needed */
            if (size_count >= size_capacity) {
                size_capacity *= 2;
                width_list = realloc(width_list, size_capacity * sizeof(uint32_t));
                height_list = realloc(height_list, size_capacity * sizeof(uint32_t));
                if (!width_list || !height_list) {
                    free(width_list);
                    free(height_list);
                    return -ENOMEM;
                }
            }
        }
    }

    *widths = width_list;
    *heights = height_list;
    *count = size_count;

    return 0;
}

/**
 * Set video resolution
 *
 * @param dev Device handle
 * @param width Width in pixels
 * @param height Height in pixels
 * @return 0 on success, negative errno on error
 */
int dsv4l2_set_resolution(dsv4l2_device_t *dev, uint32_t width, uint32_t height)
{
    struct v4l2_format fmt;
    int rc;

    if (!dev) {
        return -EINVAL;
    }

    /* Get current format */
    rc = dsv4l2_get_format(dev, &fmt);
    if (rc < 0) {
        return rc;
    }

    /* Update resolution */
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;

    /* Set format (which will emit events) */
    return dsv4l2_set_format(dev, &fmt);
}

/**
 * Get current resolution
 *
 * @param dev Device handle
 * @param width Output width
 * @param height Output height
 * @return 0 on success, negative errno on error
 */
int dsv4l2_get_resolution(dsv4l2_device_t *dev, uint32_t *width, uint32_t *height)
{
    struct v4l2_format fmt;
    int rc;

    if (!dev || !width || !height) {
        return -EINVAL;
    }

    rc = dsv4l2_get_format(dev, &fmt);
    if (rc < 0) {
        return rc;
    }

    *width = fmt.fmt.pix.width;
    *height = fmt.fmt.pix.height;

    return 0;
}

/**
 * Get pixel format fourcc as string
 *
 * @param fourcc Pixel format fourcc
 * @param str Output string buffer (must be at least 5 bytes)
 */
void dsv4l2_fourcc_to_string(uint32_t fourcc, char *str)
{
    if (!str) {
        return;
    }

    str[0] = (fourcc >> 0) & 0xFF;
    str[1] = (fourcc >> 8) & 0xFF;
    str[2] = (fourcc >> 16) & 0xFF;
    str[3] = (fourcc >> 24) & 0xFF;
    str[4] = '\0';
}
