/*
 * DSV4L2 Metadata System Tests
 *
 * Test KLV parsing, IR radiometric decoding, and timestamp sync
 */

#include "dsv4l2_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        tests_failed++; \
    } \
} while (0)

/**
 * Create test KLV buffer with simple items
 */
static int create_test_klv_buffer(dsv4l2_klv_buffer_t *buffer)
{
    size_t total_len;
    uint8_t *data;
    size_t pos = 0;

    /* Calculate total length:
     * - Item 1: 16-byte key + 1-byte length (short form) + 8 bytes data = 25
     * - Item 2: 16-byte key + 1-byte length (short form) + 4 bytes data = 21
     */
    total_len = 25 + 21;

    data = malloc(total_len);
    if (!data) {
        return -ENOMEM;
    }

    /* Item 1: UAS Datalink LS key with 8-byte value */
    memcpy(&data[pos], DSV4L2_KLV_UAS_DATALINK_LS.bytes, 16);
    pos += 16;
    data[pos++] = 0x08;  /* Short form length = 8 */
    data[pos++] = 0x01;
    data[pos++] = 0x02;
    data[pos++] = 0x03;
    data[pos++] = 0x04;
    data[pos++] = 0x05;
    data[pos++] = 0x06;
    data[pos++] = 0x07;
    data[pos++] = 0x08;

    /* Item 2: Sensor latitude key with 4-byte value */
    memcpy(&data[pos], DSV4L2_KLV_SENSOR_LATITUDE.bytes, 16);
    pos += 16;
    data[pos++] = 0x04;  /* Short form length = 4 */
    data[pos++] = 0xAA;
    data[pos++] = 0xBB;
    data[pos++] = 0xCC;
    data[pos++] = 0xDD;

    buffer->data = data;
    buffer->length = total_len;
    buffer->timestamp_ns = 1000000000ULL;
    buffer->sequence = 1;

    return 0;
}

/**
 * Test KLV parsing
 */
static void test_klv_parsing(void)
{
    dsv4l2_klv_buffer_t buffer;
    dsv4l2_klv_item_t *items = NULL;
    size_t count = 0;
    int rc;
    const dsv4l2_klv_item_t *found;

    printf("\n=== Testing KLV Parsing ===\n");

    /* Create test buffer */
    rc = create_test_klv_buffer(&buffer);
    TEST_ASSERT(rc == 0, "Create test KLV buffer");

    if (rc != 0) {
        return;
    }

    /* Parse KLV buffer */
    rc = dsv4l2_parse_klv(&buffer, &items, &count);
    TEST_ASSERT(rc == 0, "Parse KLV buffer");
    TEST_ASSERT(count == 2, "Parse 2 KLV items");

    if (rc == 0 && count == 2) {
        /* Verify first item */
        TEST_ASSERT(memcmp(items[0].key.bytes, DSV4L2_KLV_UAS_DATALINK_LS.bytes, 16) == 0,
                    "First item has UAS Datalink LS key");
        TEST_ASSERT(items[0].length == 8, "First item length = 8");
        TEST_ASSERT(items[0].value[0] == 0x01, "First item value[0] = 0x01");

        /* Verify second item */
        TEST_ASSERT(memcmp(items[1].key.bytes, DSV4L2_KLV_SENSOR_LATITUDE.bytes, 16) == 0,
                    "Second item has Sensor Latitude key");
        TEST_ASSERT(items[1].length == 4, "Second item length = 4");
        TEST_ASSERT(items[1].value[0] == 0xAA, "Second item value[0] = 0xAA");

        /* Test finding items by key */
        found = dsv4l2_find_klv_item(items, count, &DSV4L2_KLV_UAS_DATALINK_LS);
        TEST_ASSERT(found != NULL, "Find UAS Datalink LS item");
        TEST_ASSERT(found->length == 8, "Found item has correct length");

        found = dsv4l2_find_klv_item(items, count, &DSV4L2_KLV_SENSOR_LATITUDE);
        TEST_ASSERT(found != NULL, "Find Sensor Latitude item");

        found = dsv4l2_find_klv_item(items, count, &DSV4L2_KLV_SENSOR_LONGITUDE);
        TEST_ASSERT(found == NULL, "Longitude item not found (expected)");
    }

    /* Cleanup */
    free(items);
    free(buffer.data);
}

/**
 * Test IR radiometric decoding
 */
static void test_ir_radiometric(void)
{
    uint16_t raw_data[100];
    dsv4l2_ir_radiometric_t ir_data;
    float calibration[2];
    int rc;
    size_t i;

    printf("\n=== Testing IR Radiometric Decoding ===\n");

    /* Create test raw data (10x10 sensor) */
    for (i = 0; i < 100; i++) {
        raw_data[i] = 1000 + i * 10;  /* Simulated raw ADC values */
    }

    /* Set calibration constants (T = 0.1 * raw + 200) */
    calibration[0] = 0.1f;   /* c1 */
    calibration[1] = 200.0f; /* c2 */

    /* Decode radiometric data */
    rc = dsv4l2_decode_ir_radiometric(raw_data, 10, 10, calibration, &ir_data);
    TEST_ASSERT(rc == 0, "Decode IR radiometric data");

    if (rc == 0) {
        TEST_ASSERT(ir_data.width == 10, "IR width = 10");
        TEST_ASSERT(ir_data.height == 10, "IR height = 10");
        TEST_ASSERT(ir_data.temp_map != NULL, "Temperature map allocated");

        if (ir_data.temp_map) {
            /* Verify first pixel: T = 0.1 * 1000 + 200 = 300K */
            /* Stored as Kelvin * 100 = 30000 */
            TEST_ASSERT(ir_data.temp_map[0] == 30000, "First pixel temperature correct");

            /* Verify last pixel: T = 0.1 * 1990 + 200 = 399K */
            /* Stored as Kelvin * 100 = 39900 */
            TEST_ASSERT(ir_data.temp_map[99] == 39900, "Last pixel temperature correct");

            /* Verify calibration stored */
            TEST_ASSERT(ir_data.calibration_c1 == 0.1f, "Calibration c1 stored");
            TEST_ASSERT(ir_data.calibration_c2 == 200.0f, "Calibration c2 stored");

            free(ir_data.temp_map);
        }
    }
}

/**
 * Test timestamp synchronization
 */
static void test_timestamp_sync(void)
{
    dsv4l2_metadata_t meta_buffers[5];
    uint64_t frame_ts;
    int idx;

    printf("\n=== Testing Timestamp Synchronization ===\n");

    /* Create test metadata buffers with different timestamps */
    meta_buffers[0].timestamp_ns = 1000000000ULL;  /* 1.0s */
    meta_buffers[1].timestamp_ns = 1100000000ULL;  /* 1.1s */
    meta_buffers[2].timestamp_ns = 1200000000ULL;  /* 1.2s */
    meta_buffers[3].timestamp_ns = 1300000000ULL;  /* 1.3s */
    meta_buffers[4].timestamp_ns = 1400000000ULL;  /* 1.4s */

    /* Test exact match */
    frame_ts = 1200000000ULL;
    idx = dsv4l2_sync_metadata(frame_ts, meta_buffers, 5);
    TEST_ASSERT(idx == 2, "Exact timestamp match (index 2)");

    /* Test closest match (10ms delta) */
    frame_ts = 1210000000ULL;  /* 1.21s */
    idx = dsv4l2_sync_metadata(frame_ts, meta_buffers, 5);
    TEST_ASSERT(idx == 2, "Closest timestamp match (10ms delta)");

    /* Test closest match (40ms delta) */
    frame_ts = 1140000000ULL;  /* 1.14s */
    idx = dsv4l2_sync_metadata(frame_ts, meta_buffers, 5);
    TEST_ASSERT(idx == 1, "Closest timestamp match (40ms delta)");

    /* Test out of range (>50ms threshold) */
    frame_ts = 500000000ULL;  /* 0.5s - too far from any metadata */
    idx = dsv4l2_sync_metadata(frame_ts, meta_buffers, 5);
    TEST_ASSERT(idx == -1, "Out of range returns -1");

    /* Test edge cases */
    idx = dsv4l2_sync_metadata(1000000000ULL, NULL, 5);
    TEST_ASSERT(idx == -1, "NULL metadata buffers returns -1");

    idx = dsv4l2_sync_metadata(1000000000ULL, meta_buffers, 0);
    TEST_ASSERT(idx == -1, "Zero count returns -1");
}

/**
 * Test metadata format types
 */
static void test_metadata_formats(void)
{
    dsv4l2_metadata_t metadata;

    printf("\n=== Testing Metadata Format Types ===\n");

    /* Test format enum values */
    TEST_ASSERT(DSV4L2_META_FORMAT_UNKNOWN == 0, "UNKNOWN format = 0");
    TEST_ASSERT(DSV4L2_META_FORMAT_KLV == 1, "KLV format = 1");
    TEST_ASSERT(DSV4L2_META_FORMAT_IR_TEMP == 2, "IR_TEMP format = 2");
    TEST_ASSERT(DSV4L2_META_FORMAT_TELEMETRY == 3, "TELEMETRY format = 3");
    TEST_ASSERT(DSV4L2_META_FORMAT_TIMING == 4, "TIMING format = 4");

    /* Test metadata structure */
    memset(&metadata, 0, sizeof(metadata));
    metadata.format = DSV4L2_META_FORMAT_KLV;
    metadata.timestamp_ns = 1234567890ULL;
    metadata.sequence = 42;

    TEST_ASSERT(metadata.format == DSV4L2_META_FORMAT_KLV, "Set KLV format");
    TEST_ASSERT(metadata.timestamp_ns == 1234567890ULL, "Set timestamp");
    TEST_ASSERT(metadata.sequence == 42, "Set sequence");
}

/**
 * Main test runner
 */
int main(void)
{
    printf("DSV4L2 Metadata System Tests\n");
    printf("=============================\n");

    /* Run test suites */
    test_klv_parsing();
    test_ir_radiometric();
    test_timestamp_sync();
    test_metadata_formats();

    /* Print summary */
    printf("\n=============================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    }

    printf("\nAll tests PASSED!\n");
    return 0;
}
