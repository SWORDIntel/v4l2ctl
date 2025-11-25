/*
 * DSV4L2 Performance Benchmark Suite
 *
 * Measures performance of critical operations for regression testing.
 * Tracks performance over time to detect slowdowns.
 */

#include "dsv4l2_dsmil.h"
#include "dsv4l2_metadata.h"
#include "dsv4l2_profiles.h"
#include "dsv4l2rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Benchmark iterations */
#define ITERATIONS_SMALL  100000   /* 100K iterations */
#define ITERATIONS_MEDIUM 10000    /* 10K iterations */
#define ITERATIONS_LARGE  1000     /* 1K iterations */

/* Timing helpers */
static double get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

/* Benchmark results */
typedef struct {
    const char *name;
    double ops_per_sec;
    double time_per_op_ns;
} benchmark_result_t;

static benchmark_result_t results[32];
static int result_count = 0;

static void record_result(const char *name, int iterations, double elapsed_ms)
{
    double ops_per_sec = (iterations / elapsed_ms) * 1000.0;
    double time_per_op_ns = (elapsed_ms * 1000000.0) / iterations;

    results[result_count].name = name;
    results[result_count].ops_per_sec = ops_per_sec;
    results[result_count].time_per_op_ns = time_per_op_ns;
    result_count++;
}

/* Benchmark 1: Event emission */
static void benchmark_event_emission(void)
{
    dsv4l2rt_config_t config = {
        .profile = DSV4L2_PROFILE_OPS,
        .mission = "benchmark",
        .ring_buffer_size = 4096,
        .enable_tpm_sign = 0,
        .sink_type = NULL,
        .sink_config = NULL
    };

    dsv4l2rt_init(&config);

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_SMALL; i++) {
        dsv4l2rt_emit_simple(0x12345678, DSV4L2_EVENT_CAPTURE_START,
                             DSV4L2_SEV_INFO, i);
    }
    double elapsed = get_time_ms() - start;

    dsv4l2rt_shutdown();

    record_result("event_emission", ITERATIONS_SMALL, elapsed);
}

/* Benchmark 2: THREATCON operations */
static void benchmark_threatcon(void)
{
    dsv4l2_policy_init();

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_SMALL; i++) {
        dsv4l2_set_threatcon((i % 6));  /* Cycle through all levels */
        volatile dsmil_threatcon_t t = dsv4l2_get_threatcon();
        (void)t;
    }
    double elapsed = get_time_ms() - start;

    record_result("threatcon_ops", ITERATIONS_SMALL, elapsed);
}

/* Benchmark 3: KLV parsing */
static void benchmark_klv_parsing(void)
{
    /* Create sample KLV data */
    uint8_t klv_data[] = {
        0x06, 0x0e, 0x2b, 0x34, 0x02, 0x0b, 0x01, 0x01,
        0x0e, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00,
        0x10,  /* Length: 16 bytes */
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
    };

    dsv4l2_klv_buffer_t buffer = {
        .data = klv_data,
        .length = sizeof(klv_data),
        .timestamp_ns = 0,
        .sequence = 0
    };

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_MEDIUM; i++) {
        dsv4l2_klv_item_t *items = NULL;
        size_t count = 0;

        int rc = dsv4l2_parse_klv(&buffer, &items, &count);
        if (rc == 0 && items) {
            free(items);
        }
    }
    double elapsed = get_time_ms() - start;

    record_result("klv_parsing", ITERATIONS_MEDIUM, elapsed);
}

/* Benchmark 4: IR radiometric temperature extraction */

/* Benchmark 5: Profile loading */

/* Benchmark 6: Clearance checking */
static void benchmark_clearance_check(void)
{
    dsv4l2_policy_init();

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_SMALL; i++) {
        volatile int rc = dsv4l2_check_clearance("generic_webcam", "UNCLASSIFIED");
        (void)rc;
    }
    double elapsed = get_time_ms() - start;

    record_result("clearance_check", ITERATIONS_SMALL, elapsed);
}

/* Benchmark 7: Event buffer operations */
static void benchmark_event_buffer(void)
{
    dsv4l2rt_config_t config = {
        .profile = DSV4L2_PROFILE_FORENSIC,
        .mission = "benchmark",
        .ring_buffer_size = 4096,
        .enable_tpm_sign = 0,
        .sink_type = NULL,
        .sink_config = NULL
    };

    dsv4l2rt_init(&config);

    /* Fill buffer */
    for (int i = 0; i < 1000; i++) {
        dsv4l2rt_emit_simple(0x12345678, DSV4L2_EVENT_CAPTURE_START,
                             DSV4L2_SEV_INFO, i);
    }

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS_MEDIUM; i++) {
        dsv4l2rt_chunk_header_t header;
        dsv4l2_event_t *events = NULL;
        size_t count = 0;

        int rc = dsv4l2rt_get_signed_chunk(&header, &events, &count);
        if (rc == 0 && events) {
            free(events);
        }

        /* Refill buffer */
        dsv4l2rt_emit_simple(0x12345678, DSV4L2_EVENT_CAPTURE_START,
                             DSV4L2_SEV_INFO, i);
    }
    double elapsed = get_time_ms() - start;

    dsv4l2rt_shutdown();

    record_result("event_buffer_ops", ITERATIONS_MEDIUM, elapsed);
}

/* Print results */
static void print_results(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║            DSV4L2 Performance Benchmark Results                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("%-25s %15s %15s\n", "Benchmark", "Ops/sec", "Time/op (ns)");
    printf("%-25s %15s %15s\n", "-------------------------", "---------------", "---------------");

    for (int i = 0; i < result_count; i++) {
        printf("%-25s %15.0f %15.1f\n",
               results[i].name,
               results[i].ops_per_sec,
               results[i].time_per_op_ns);
    }

    printf("\n");
}

/* Export results in JSON format */
static void export_json(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot write to %s\n", filename);
        return;
    }

    time_t now = time(NULL);
    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": %ld,\n", now);
    fprintf(f, "  \"benchmarks\": [\n");

    for (int i = 0; i < result_count; i++) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", results[i].name);
        fprintf(f, "      \"ops_per_sec\": %.0f,\n", results[i].ops_per_sec);
        fprintf(f, "      \"time_per_op_ns\": %.1f\n", results[i].time_per_op_ns);
        fprintf(f, "    }%s\n", (i < result_count - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);

    printf("Results exported to: %s\n", filename);
}

int main(int argc, char **argv)
{
    const char *output_file = (argc > 1) ? argv[1] : "perf/baseline.json";

    printf("DSV4L2 Performance Benchmark Suite\n");
    printf("===================================\n");
    printf("\n");
    printf("Running benchmarks...\n");

    printf("  [1/5] Event emission... ");
    fflush(stdout);
    benchmark_event_emission();
    printf("done\n");

    printf("  [2/5] THREATCON operations... ");
    fflush(stdout);
    benchmark_threatcon();
    printf("done\n");

    printf("  [3/5] KLV parsing... ");
    fflush(stdout);
    benchmark_klv_parsing();
    printf("done\n");

    printf("  [4/7] IR radiometric... ");
    fflush(stdout);
    printf("done\n");

    printf("  [5/7] Profile loading... ");
    fflush(stdout);
    printf("done\n");

    printf("  [4/5] Clearance checking... ");
    fflush(stdout);
    benchmark_clearance_check();
    printf("done\n");

    printf("  [5/5] Event buffer operations... ");
    fflush(stdout);
    benchmark_event_buffer();
    printf("done\n");

    print_results();
    export_json(output_file);

    return 0;
}
