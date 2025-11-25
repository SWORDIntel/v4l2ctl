/*
 * DSV4L2 CLI Tool
 *
 * Command-line interface for DSLLVM-enhanced Video4Linux2 operations.
 *
 * Commands:
 *   scan    - Scan for v4l2 devices
 *   list    - List available devices with profiles
 *   info    - Show detailed device information
 *   capture - Capture frames from a device
 *   monitor - Monitor runtime events
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2_core.h"
#include "dsv4l2_profiles.h"
#include "dsv4l2_dsmil.h"
#include "dsv4l2_metadata.h"
#include "dsv4l2rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

/* Command function prototypes */
static int cmd_scan(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_capture(int argc, char **argv);
static int cmd_monitor(int argc, char **argv);

/* Command table */
typedef struct {
    const char *name;
    const char *description;
    int (*handler)(int argc, char **argv);
} command_t;

static const command_t commands[] = {
    { "scan",    "Scan for v4l2 devices",                     cmd_scan },
    { "list",    "List devices with profile information",     cmd_list },
    { "info",    "Show detailed device information",          cmd_info },
    { "capture", "Capture frames from a device",              cmd_capture },
    { "monitor", "Monitor runtime events",                    cmd_monitor },
    { NULL, NULL, NULL }
};

/**
 * Print usage information
 */
static void print_usage(const char *progname)
{
    const command_t *cmd;

    printf("Usage: %s <command> [options]\n\n", progname);
    printf("DSV4L2 - DSLLVM-Enhanced Video4Linux2 Tool\n\n");
    printf("Commands:\n");

    for (cmd = commands; cmd->name != NULL; cmd++) {
        printf("  %-10s %s\n", cmd->name, cmd->description);
    }

    printf("\nGlobal Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("\nEnvironment Variables:\n");
    printf("  DSV4L2_PROFILE    Instrumentation profile (off/ops/exercise/forensic)\n");
    printf("  DSV4L2_CLEARANCE  User clearance level (UNCLASSIFIED/CONFIDENTIAL/SECRET/TOP_SECRET)\n");
}

/**
 * Scan command - discover v4l2 devices
 */
static int cmd_scan(int argc, char **argv)
{
    dsv4l2_device_t **devices = NULL;
    size_t count = 0;
    int rc;
    size_t i;

    (void)argc;
    (void)argv;

    printf("Scanning for v4l2 devices...\n\n");

    rc = dsv4l2_list_devices(&devices, &count);
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to list devices: %s\n", strerror(-rc));
        return 1;
    }

    if (count == 0) {
        printf("No v4l2 devices found.\n");
        return 0;
    }

    printf("Found %zu device(s):\n\n", count);

    for (i = 0; i < count; i++) {
        printf("Device %zu:\n", i + 1);
        printf("  Path:  %s\n", devices[i]->dev_path);
        printf("  Role:  %s\n", devices[i]->role);
        printf("  Layer: L%d\n", devices[i]->layer);
        printf("\n");

        /* Close device */
        dsv4l2_close(devices[i]);
    }

    free(devices);
    return 0;
}

/**
 * List command - show devices with profile info
 */
static int cmd_list(int argc, char **argv)
{
    dsv4l2_device_t **devices = NULL;
    size_t count = 0;
    int rc;
    size_t i;
    int verbose = 0;

    /* Parse options */
    for (i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
    }

    rc = dsv4l2_list_devices(&devices, &count);
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to list devices: %s\n", strerror(-rc));
        return 1;
    }

    if (count == 0) {
        printf("No devices found.\n");
        return 0;
    }

    printf("%-15s %-20s %-15s %-10s\n", "PATH", "ROLE", "CLASSIFICATION", "LAYER");
    printf("%-15s %-20s %-15s %-10s\n", "----", "----", "--------------", "-----");

    for (i = 0; i < count; i++) {
        printf("%-15s %-20s %-15s L%-9d\n",
               devices[i]->dev_path,
               devices[i]->role,
               "UNCLASSIFIED",  /* Would need to query from device internal */
               devices[i]->layer);

        if (verbose) {
            printf("  FD: %d\n", devices[i]->fd);
        }

        dsv4l2_close(devices[i]);
    }

    free(devices);
    return 0;
}

/**
 * Info command - detailed device information
 */
static int cmd_info(int argc, char **argv)
{
    const char *device_path = "/dev/video0";
    const char *role = "camera";
    dsv4l2_device_t *dev = NULL;
    dsv4l2_tempest_state_t tempest;
    int rc;

    /* Parse arguments */
    if (argc >= 2) {
        device_path = argv[1];
    }
    if (argc >= 3) {
        role = argv[2];
    }

    printf("Device Information: %s\n", device_path);
    printf("==================\n\n");

    /* Open device */
    rc = dsv4l2_open(device_path, role, &dev);
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to open device: %s\n", strerror(-rc));
        return 1;
    }

    printf("Path:         %s\n", dev->dev_path);
    printf("Role:         %s\n", dev->role);
    printf("Layer:        L%d\n", dev->layer);
    printf("File Descriptor: %d\n", dev->fd);

    /* Get TEMPEST state */
    tempest = dsv4l2_get_tempest_state(dev);
    printf("TEMPEST State: ");
    switch (tempest) {
        case DSV4L2_TEMPEST_DISABLED: printf("DISABLED\n"); break;
        case DSV4L2_TEMPEST_LOW:      printf("LOW\n"); break;
        case DSV4L2_TEMPEST_HIGH:     printf("HIGH\n"); break;
        case DSV4L2_TEMPEST_LOCKDOWN: printf("LOCKDOWN\n"); break;
        default:                       printf("UNKNOWN (%d)\n", tempest); break;
    }

    /* Get current THREATCON */
    dsmil_threatcon_t threatcon = dsv4l2_get_threatcon();
    printf("THREATCON:     %s\n", dsv4l2_threatcon_name(threatcon));

    dsv4l2_close(dev);
    return 0;
}

/**
 * Capture command - acquire frames
 */
static int cmd_capture(int argc, char **argv)
{
    const char *device_path = "/dev/video0";
    const char *role = "camera";
    const char *output_file = NULL;
    dsv4l2_device_t *dev = NULL;
    dsv4l2_frame_t frame;
    int num_frames = 1;
    int rc;
    int i;

    /* Parse arguments */
    struct option long_options[] = {
        {"device",  required_argument, 0, 'd'},
        {"role",    required_argument, 0, 'r'},
        {"output",  required_argument, 0, 'o'},
        {"count",   required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:r:o:n:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                device_path = optarg;
                break;
            case 'r':
                role = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'n':
                num_frames = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s capture [-d device] [-r role] [-o output] [-n count]\n", argv[0]);
                return 1;
        }
    }

    printf("Capturing %d frame(s) from %s (role: %s)\n", num_frames, device_path, role);

    /* Open device */
    rc = dsv4l2_open(device_path, role, &dev);
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to open device: %s\n", strerror(-rc));
        return 1;
    }

    /* Start streaming */
    rc = dsv4l2_start_streaming(dev);
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to start streaming: %s\n", strerror(-rc));
        dsv4l2_close(dev);
        return 1;
    }

    /* Capture frames */
    for (i = 0; i < num_frames; i++) {
        rc = dsv4l2_capture_frame(dev, &frame);
        if (rc != 0) {
            fprintf(stderr, "Error: Failed to capture frame %d: %s\n", i + 1, strerror(-rc));
            break;
        }

        printf("Frame %d: %zu bytes\n", i + 1, frame.len);

        /* Write to file if specified */
        if (output_file && frame.data) {
            FILE *f = fopen(output_file, i == 0 ? "wb" : "ab");
            if (f) {
                fwrite(frame.data, 1, frame.len, f);
                fclose(f);
            }
        }

        /* Free frame data */
        free(frame.data);
    }

    /* Stop streaming */
    dsv4l2_stop_streaming(dev);

    dsv4l2_close(dev);

    if (output_file) {
        printf("\nFrames written to: %s\n", output_file);
    }

    return 0;
}

/**
 * Monitor command - watch runtime events
 */
static int cmd_monitor(int argc, char **argv)
{
    dsv4l2rt_config_t config;
    dsv4l2rt_stats_t stats;
    int duration = 10;  /* Default 10 seconds */

    (void)argc;
    (void)argv;

    printf("Monitoring DSV4L2 runtime events...\n");
    printf("Press Ctrl+C to stop\n\n");

    /* Initialize runtime in forensic mode */
    memset(&config, 0, sizeof(config));
    config.profile = DSV4L2_PROFILE_FORENSIC;

    dsv4l2rt_init(&config);

    /* Monitor for specified duration */
    sleep(duration);

    /* Get final statistics */
    dsv4l2rt_get_stats(&stats);

    printf("\nRuntime Statistics:\n");
    printf("  Events Emitted: %llu\n", (unsigned long long)stats.events_emitted);
    printf("  Events Dropped: %llu\n", (unsigned long long)stats.events_dropped);
    printf("  Events Flushed: %llu\n", (unsigned long long)stats.events_flushed);
    printf("  Buffer Usage:   %zu / %zu\n", stats.buffer_usage, stats.buffer_capacity);

    dsv4l2rt_shutdown();

    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char **argv)
{
    const command_t *cmd;
    const char *cmd_name;

    /* Check for help or version */
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("dsv4l2 version 1.0.0\n");
        printf("DSLLVM-Enhanced Video4Linux2 Tool\n");
        return 0;
    }

    /* Find and execute command */
    cmd_name = argv[1];

    for (cmd = commands; cmd->name != NULL; cmd++) {
        if (strcmp(cmd->name, cmd_name) == 0) {
            return cmd->handler(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "Error: Unknown command '%s'\n", cmd_name);
    fprintf(stderr, "Run '%s --help' for usage information\n", argv[0]);
    return 1;
}
