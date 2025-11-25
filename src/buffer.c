/*
 * DSV4L2 Buffer Management
 *
 * Handles v4l2 buffer allocation, queueing, and memory mapping.
 * Biometric devices have buffers tagged with DSMIL_SECRET_REGION.
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2rt.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
typedef struct dsv4l2_device_internal dsv4l2_device_internal_t;
extern dsv4l2_device_internal_t *dsv4l2_get_internal(dsv4l2_device_t *dev);

/* Buffer structure */
typedef struct {
    void *start;
    size_t length;
} dsv4l2_buffer_t;

/* Internal device structure (partial - just what we need) */
struct dsv4l2_device_internal {
    dsv4l2_device_t public;
    struct v4l2_capability cap;
    dsv4l2_tempest_state_t tempest;
    int tempest_ctrl_id;
    char *profile_path;
    char *classification;
    int streaming;
    uint32_t dev_id;

    /* Buffer management (added here) */
    dsv4l2_buffer_t *buffers;
    uint32_t buffer_count;
};

/**
 * Request buffers from the device
 *
 * @param dev Device handle
 * @param count Number of buffers to request
 * @return 0 on success, negative errno on error
 */
int dsv4l2_request_buffers(dsv4l2_device_t *dev, uint32_t count)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_requestbuffers req;

    if (!dev || count == 0) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    /* Request buffers */
    memset(&req, 0, sizeof(req));
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev->fd, VIDIOC_REQBUFS, &req) < 0) {
        return -errno;
    }

    /* Allocate buffer tracking array */
    internal->buffers = calloc(req.count, sizeof(dsv4l2_buffer_t));
    if (!internal->buffers) {
        return -ENOMEM;
    }

    internal->buffer_count = req.count;

    return 0;
}

/**
 * Memory-map all buffers
 *
 * @param dev Device handle
 * @return 0 on success, negative errno on error
 */
int dsv4l2_mmap_buffers(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;
    uint32_t i;

    if (!dev) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    if (!internal->buffers || internal->buffer_count == 0) {
        return -EINVAL;
    }

    /* Map each buffer */
    for (i = 0; i < internal->buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            return -errno;
        }

        internal->buffers[i].length = buf.length;
        internal->buffers[i].start = mmap(NULL, buf.length,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED,
                                          dev->fd, buf.m.offset);

        if (internal->buffers[i].start == MAP_FAILED) {
            return -errno;
        }
    }

    return 0;
}

/**
 * Queue a buffer for capture
 *
 * @param dev Device handle
 * @param index Buffer index
 * @return 0 on success, negative errno on error
 */
int dsv4l2_queue_buffer(dsv4l2_device_t *dev, uint32_t index)
{
    dsv4l2_device_internal_t *internal;
    struct v4l2_buffer buf;

    if (!dev) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    if (index >= internal->buffer_count) {
        return -EINVAL;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    return 0;
}

/**
 * Dequeue a filled buffer
 *
 * @param dev Device handle
 * @param buf Output buffer information
 * @return 0 on success, negative errno on error
 */
int dsv4l2_dequeue_buffer(dsv4l2_device_t *dev, struct v4l2_buffer *buf)
{
    if (!dev || !buf) {
        return -EINVAL;
    }

    memset(buf, 0, sizeof(*buf));
    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev->fd, VIDIOC_DQBUF, buf) < 0) {
        return -errno;
    }

    return 0;
}

/**
 * Get pointer to mapped buffer data
 *
 * @param dev Device handle
 * @param index Buffer index
 * @param start Output pointer to buffer start
 * @param length Output buffer length
 * @return 0 on success, negative errno on error
 */
int dsv4l2_get_buffer(dsv4l2_device_t *dev, uint32_t index,
                      void **start, size_t *length)
{
    dsv4l2_device_internal_t *internal;

    if (!dev || !start || !length) {
        return -EINVAL;
    }

    internal = dsv4l2_get_internal(dev);

    if (index >= internal->buffer_count) {
        return -EINVAL;
    }

    *start = internal->buffers[index].start;
    *length = internal->buffers[index].length;

    return 0;
}

/**
 * Unmap and release all buffers
 *
 * @param dev Device handle
 */
void dsv4l2_release_buffers(dsv4l2_device_t *dev)
{
    dsv4l2_device_internal_t *internal;
    uint32_t i;

    if (!dev) {
        return;
    }

    internal = dsv4l2_get_internal(dev);

    if (!internal->buffers) {
        return;
    }

    /* Unmap all buffers */
    for (i = 0; i < internal->buffer_count; i++) {
        if (internal->buffers[i].start != NULL &&
            internal->buffers[i].start != MAP_FAILED) {
            munmap(internal->buffers[i].start, internal->buffers[i].length);
        }
    }

    free(internal->buffers);
    internal->buffers = NULL;
    internal->buffer_count = 0;
}
