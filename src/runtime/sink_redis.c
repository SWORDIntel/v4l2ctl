/*
 * DSV4L2 Redis Event Sink
 *
 * Publishes events to Redis pub/sub for real-time monitoring.
 * Optional - only compiled if hiredis is available.
 */

#include "dsv4l2rt.h"

#ifdef HAVE_HIREDIS
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Redis sink context */
typedef struct {
    redisContext *ctx;
    char channel[64];
} redis_sink_t;

/**
 * Redis sink callback
 */
static void redis_sink_callback(const dsv4l2_event_t *events, size_t count, void *user_data)
{
    redis_sink_t *sink = (redis_sink_t *)user_data;
    size_t i;
    char message[512];

    if (!sink || !sink->ctx) {
        return;
    }

    /* Publish each event */
    for (i = 0; i < count; i++) {
        const dsv4l2_event_t *ev = &events[i];

        /* Format event as JSON-like string */
        snprintf(message, sizeof(message),
                 "{\"ts\":%llu,\"dev\":%u,\"type\":%u,\"sev\":%u,\"aux\":%u,\"role\":\"%s\"}",
                 (unsigned long long)ev->ts_ns,
                 ev->dev_id,
                 ev->event_type,
                 ev->severity,
                 ev->aux,
                 ev->role);

        /* Publish to channel */
        redisCommand(sink->ctx, "PUBLISH %s %s", sink->channel, message);
    }
}

/**
 * Initialize Redis sink
 */
int dsv4l2rt_init_redis_sink(const char *host, int port, const char *channel)
{
    redis_sink_t *sink;
    int rc;

    sink = calloc(1, sizeof(*sink));
    if (!sink) {
        return -1;
    }

    /* Connect to Redis */
    sink->ctx = redisConnect(host, port);
    if (sink->ctx == NULL || sink->ctx->err) {
        if (sink->ctx) {
            fprintf(stderr, "Redis connection error: %s\n", sink->ctx->errstr);
            redisFree(sink->ctx);
        } else {
            fprintf(stderr, "Redis connection error: can't allocate context\n");
        }
        free(sink);
        return -1;
    }

    /* Set channel name */
    strncpy(sink->channel, channel ? channel : "dsv4l2:events", sizeof(sink->channel) - 1);

    /* Register sink */
    rc = dsv4l2rt_register_sink(redis_sink_callback, sink);
    if (rc != 0) {
        redisFree(sink->ctx);
        free(sink);
        return -1;
    }

    return 0;
}

#else  /* !HAVE_HIREDIS */

/* Stub implementation when Redis is not available */
int dsv4l2rt_init_redis_sink(const char *host, int port, const char *channel)
{
    (void)host;
    (void)port;
    (void)channel;
    fprintf(stderr, "Redis sink not available (compile with HAVE_HIREDIS)\n");
    return -1;
}

#endif  /* HAVE_HIREDIS */
