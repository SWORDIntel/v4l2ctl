/*
 * DSV4L2 SQLite Event Sink
 *
 * Stores events in SQLite database for persistent storage and querying.
 * Optional - only compiled if SQLite3 is available.
 */

#include "dsv4l2rt.h"

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SQLite sink context */
typedef struct {
    sqlite3 *db;
    sqlite3_stmt *insert_stmt;
} sqlite_sink_t;

/**
 * SQLite sink callback
 */
static void sqlite_sink_callback(const dsv4l2_event_t *events, size_t count, void *user_data)
{
    sqlite_sink_t *sink = (sqlite_sink_t *)user_data;
    size_t i;

    if (!sink || !sink->db || !sink->insert_stmt) {
        return;
    }

    /* Begin transaction for batch insert */
    sqlite3_exec(sink->db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    /* Insert each event */
    for (i = 0; i < count; i++) {
        const dsv4l2_event_t *ev = &events[i];

        sqlite3_bind_int64(sink->insert_stmt, 1, ev->ts_ns);
        sqlite3_bind_int(sink->insert_stmt, 2, ev->dev_id);
        sqlite3_bind_int(sink->insert_stmt, 3, ev->event_type);
        sqlite3_bind_int(sink->insert_stmt, 4, ev->severity);
        sqlite3_bind_int(sink->insert_stmt, 5, ev->aux);
        sqlite3_bind_int(sink->insert_stmt, 6, ev->layer);
        sqlite3_bind_text(sink->insert_stmt, 7, ev->role, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sink->insert_stmt, 8, ev->mission, -1, SQLITE_TRANSIENT);

        sqlite3_step(sink->insert_stmt);
        sqlite3_reset(sink->insert_stmt);
    }

    /* Commit transaction */
    sqlite3_exec(sink->db, "COMMIT", NULL, NULL, NULL);
}

/**
 * Initialize SQLite sink
 */
int dsv4l2rt_init_sqlite_sink(const char *db_path)
{
    sqlite_sink_t *sink;
    int rc;
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp_ns INTEGER NOT NULL,"
        "  dev_id INTEGER NOT NULL,"
        "  event_type INTEGER NOT NULL,"
        "  severity INTEGER NOT NULL,"
        "  aux INTEGER,"
        "  layer INTEGER,"
        "  role TEXT,"
        "  mission TEXT"
        ");";
    const char *insert_sql =
        "INSERT INTO events (timestamp_ns, dev_id, event_type, severity, aux, layer, role, mission) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    sink = calloc(1, sizeof(*sink));
    if (!sink) {
        return -1;
    }

    /* Open database */
    rc = sqlite3_open(db_path, &sink->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to open SQLite database: %s\n", sqlite3_errmsg(sink->db));
        free(sink);
        return -1;
    }

    /* Create table */
    rc = sqlite3_exec(sink->db, create_table_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create events table: %s\n", sqlite3_errmsg(sink->db));
        sqlite3_close(sink->db);
        free(sink);
        return -1;
    }

    /* Prepare insert statement */
    rc = sqlite3_prepare_v2(sink->db, insert_sql, -1, &sink->insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(sink->db));
        sqlite3_close(sink->db);
        free(sink);
        return -1;
    }

    /* Register sink */
    rc = dsv4l2rt_register_sink(sqlite_sink_callback, sink);
    if (rc != 0) {
        sqlite3_finalize(sink->insert_stmt);
        sqlite3_close(sink->db);
        free(sink);
        return -1;
    }

    return 0;
}

#else  /* !HAVE_SQLITE3 */

/* Stub implementation when SQLite is not available */
int dsv4l2rt_init_sqlite_sink(const char *db_path)
{
    (void)db_path;
    fprintf(stderr, "SQLite sink not available (compile with HAVE_SQLITE3)\n");
    return -1;
}

#endif  /* HAVE_SQLITE3 */
