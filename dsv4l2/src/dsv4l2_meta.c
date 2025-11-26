/*
 * dsv4l2_meta.c
 *
 * Metadata capture for DSV4L2
 * Stub implementation - to be completed in Phase 4
 */

#include "dsv4l2_meta.h"
#include <stdlib.h>
#include <errno.h>

struct dsv4l2_meta_handle {
    int fd;
    char *device_path;
};

int dsv4l2_meta_open(
    const char *device_path,
    dsv4l2_meta_handle_t **out_handle
) {
    if (!device_path || !out_handle) {
        return -EINVAL;
    }

    dsv4l2_meta_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return -ENOMEM;
    }

    /* TODO: Open V4L2_BUF_TYPE_META_CAPTURE device in Phase 4 */
    handle->fd = -1;

    *out_handle = handle;
    return -ENOSYS;  /* Not yet implemented */
}

void dsv4l2_meta_close(dsv4l2_meta_handle_t *handle) {
    if (!handle) {
        return;
    }
    free(handle);
}

int dsv4l2_meta_read(
    dsv4l2_meta_handle_t *handle,
    dsv4l2_meta_t *out_meta
) {
    if (!handle || !out_meta) {
        return -EINVAL;
    }

    /* TODO: Implement metadata read in Phase 4 */
    return -ENOSYS;
}

int dsv4l2_meta_start_stream(dsv4l2_meta_handle_t *handle) {
    if (!handle) {
        return -EINVAL;
    }
    return -ENOSYS;
}

int dsv4l2_meta_stop_stream(dsv4l2_meta_handle_t *handle) {
    if (!handle) {
        return -EINVAL;
    }
    return -ENOSYS;
}
