# V4L2 Hardware Testing Guide for DSV4L2

## Overview

The DSV4L2 project includes a **V4L2 Hardware Detection Test Suite** that validates interaction with real Video4Linux2 capture devices. These tests verify that the DSV4L2 library can correctly interact with physical webcams, cameras, and other V4L2-compatible hardware.

## Design Philosophy

The hardware test suite is designed to:

- **Gracefully handle missing hardware**: All tests skip if no V4L2 devices are available
- **Non-destructive testing**: Tests only query device capabilities, never modify settings permanently
- **Cross-platform compatibility**: Works on any Linux system with V4L2 support
- **CI/CD friendly**: Returns success (exit 0) even when no hardware is present

## What is Tested

The hardware detection suite includes 6 categories of tests:

### 1. Device Detection
**Purpose**: Verify that V4L2 capture devices can be discovered

**What it does**:
- Scans `/dev/video0` through `/dev/video9` for V4L2 devices
- Checks device node exists and is a character device
- Opens device and queries `V4L2_CAP_VIDEO_CAPTURE` capability
- Reports first discovered capture device

**Expected behavior**:
- **With hardware**: Reports device path (e.g., `/dev/video0`)
- **Without hardware**: Gracefully skips test

### 2. Device Capabilities Query
**Purpose**: Verify `VIDIOC_QUERYCAP` ioctl works correctly

**What it does**:
- Opens discovered V4L2 device
- Queries device capabilities with `VIDIOC_QUERYCAP`
- Verifies `V4L2_CAP_VIDEO_CAPTURE` flag is set
- Verifies `V4L2_CAP_STREAMING` flag is set
- Reports driver name, card name, bus info

**Expected output**:
```
Driver: uvcvideo
Card: HD Pro Webcam C920
Bus: usb-0000:00:14.0-1
```

### 3. Format Enumeration
**Purpose**: Verify `VIDIOC_ENUM_FMT` ioctl enumerates supported pixel formats

**What it does**:
- Iterates through all supported formats using `VIDIOC_ENUM_FMT`
- Counts total formats available
- Reports first format (FourCC and description)
- Verifies at least one format is supported

**Expected output**:
```
First format: YUYV - YUYV 4:2:2
Total formats: 3
```

### 4. Format Get/Set Operations
**Purpose**: Verify `VIDIOC_G_FMT` and `VIDIOC_S_FMT` ioctls work

**What it does**:
- Gets current format with `VIDIOC_G_FMT`
- Reports current format (FourCC, width, height)
- Attempts to set 640x480 YUYV format
- Gracefully handles unsupported formats

**Expected output**:
```
Current: YUYV 1920x1080
Set format succeeded (640x480 YUYV)
```

### 5. Buffer Allocation
**Purpose**: Verify `VIDIOC_REQBUFS` ioctl can allocate memory-mapped buffers

**What it does**:
- Requests 4 MMAP buffers with `VIDIOC_REQBUFS`
- Verifies at least 2 buffers were allocated
- Reports actual buffer count allocated

**Expected output**:
```
Buffer allocation succeeded
Allocated at least 2 buffers
Allocated: 4 buffers
```

### 6. Profile Matching
**Purpose**: Verify device can be matched to DSV4L2 profiles

**What it does**:
- Loads all DSV4L2 device profiles
- Attempts to match device card name to vendor/model strings
- Reports matched profile ID if found
- Gracefully handles generic unrecognized devices

**Expected output**:
```
Loaded 3 profiles
Matched profile: usb:046d:082d
Device matched a profile
```

## Quick Start

### Build Hardware Tests

```bash
make tests
```

This builds all test suites, including `tests/test_hardware_detect`.

### Run Hardware Detection Tests

```bash
# Run hardware detection test
LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./tests/test_hardware_detect
```

### Expected Output (No Hardware)

```
╔════════════════════════════════════════════════════════╗
║          DSV4L2 Hardware Detection Tests              ║
╚════════════════════════════════════════════════════════╝

Note: These tests require real V4L2 hardware.
Tests will be skipped if no devices are available.

=== Test 1: V4L2 Device Detection ===
  ⊘ No V4L2 devices available (skipped)

=== Test 2: Device Capabilities ===
  ⊘ No V4L2 devices available (skipped)

[...]

╔════════════════════════════════════════════════════════╗
║         Hardware Detection Test Summary               ║
╚════════════════════════════════════════════════════════╝

  Total Tests:   6
  ✓ Passed:      0
  ✗ Failed:      0
  ⊘ Skipped:     6

  Status: ✓ ALL TESTS PASSED
```

**Exit code**: 0 (success)

### Expected Output (With Hardware)

```
╔════════════════════════════════════════════════════════╗
║          DSV4L2 Hardware Detection Tests              ║
╚════════════════════════════════════════════════════════╝

Note: These tests require real V4L2 hardware.
Tests will be skipped if no devices are available.

=== Test 1: V4L2 Device Detection ===
    Found device: /dev/video0
  ✓ V4L2 capture device detected

=== Test 2: Device Capabilities ===
    Driver: uvcvideo
    Card: HD Pro Webcam C920
    Bus: usb-0000:00:14.0-1
  ✓ Query capabilities succeeded
  ✓ Device supports video capture
  ✓ Device supports streaming

=== Test 3: Format Enumeration ===
    First format: YUYV - YUYV 4:2:2
    Total formats: 3
  ✓ Device supports at least one format

=== Test 4: Format Get/Set ===
    Current: YUYV 1920x1080
  ✓ Get format succeeded
  ✓ Set format succeeded (640x480 YUYV)

=== Test 5: Buffer Allocation ===
  ✓ Buffer allocation succeeded
  ✓ Allocated at least 2 buffers
    Allocated: 4 buffers

=== Test 6: Profile Matching ===
    Loaded 3 profiles
    Matched profile: usb:046d:082d
  ✓ Device matched a profile

╔════════════════════════════════════════════════════════╗
║         Hardware Detection Test Summary               ║
╚════════════════════════════════════════════════════════╝

  Total Tests:   10
  ✓ Passed:      10
  ✗ Failed:      0
  ⊘ Skipped:     0

  Status: ✓ ALL TESTS PASSED
```

**Exit code**: 0 (success)

## Running on Systems Without Hardware

The hardware test suite is designed to pass on systems without V4L2 devices (e.g., cloud CI runners, headless servers).

```bash
./tests/test_hardware_detect
# Exit code: 0
# All tests skipped gracefully
```

This ensures:
- CI/CD pipelines don't fail on hardware-less runners
- Developers without webcams can still run the test suite
- Test coverage remains consistent across environments

## Running with Real Hardware

### Prerequisites

1. **Linux kernel with V4L2 support**: Most modern Linux distributions include V4L2 in the kernel
2. **V4L2 capture device**: Webcam, USB camera, or video capture card
3. **Device permissions**: User must have read/write access to `/dev/videoN`

### Check Available Devices

```bash
# List V4L2 devices
ls -l /dev/video*

# Check device capabilities (requires v4l-utils)
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video0 --all
```

### Grant Device Permissions

If you get "Permission denied" errors:

```bash
# Temporary: Change device permissions (requires sudo)
sudo chmod a+rw /dev/video0

# Permanent: Add user to video group
sudo usermod -a -G video $USER
# Logout and login for changes to take effect
```

### Run Tests

```bash
# Build tests
make tests

# Run hardware detection test
LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./tests/test_hardware_detect
```

## Continuous Integration

### GitHub Actions

Example workflow for testing with hardware:

```yaml
name: Hardware Tests

on: [push, pull_request]

jobs:
  test-no-hardware:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build
        run: make all

      - name: Run hardware tests (no devices)
        run: |
          LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./tests/test_hardware_detect
          # Should pass with all tests skipped
```

### GitLab CI

```yaml
test:hardware:
  stage: test
  script:
    - make all
    - LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./tests/test_hardware_detect
  allow_failure: false  # Should always pass (skips if no hardware)
```

## Virtual V4L2 Devices

For CI/CD testing with simulated hardware, use **v4l2loopback**:

### Install v4l2loopback

```bash
# Ubuntu/Debian
sudo apt-get install v4l2loopback-dkms v4l2loopback-utils

# Load module
sudo modprobe v4l2loopback
```

### Create Virtual Device

```bash
# Create /dev/video0 virtual device
sudo modprobe v4l2loopback video_nr=0 card_label="Virtual Camera"

# Verify device exists
ls -l /dev/video0
v4l2-ctl --device=/dev/video0 --all
```

### Feed Test Pattern

```bash
# Install GStreamer
sudo apt-get install gstreamer1.0-tools gstreamer1.0-plugins-base

# Stream test pattern to virtual device
gst-launch-1.0 videotestsrc ! v4l2sink device=/dev/video0
```

### Run Tests with Virtual Device

```bash
LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH" ./tests/test_hardware_detect

# Expected: All tests pass with virtual device
```

## Troubleshooting

### No Devices Detected

**Symptoms**: All tests skip, no `/dev/video*` devices

**Causes**:
- No V4L2 hardware connected
- Kernel module not loaded
- Device permissions

**Solutions**:

```bash
# Check if devices exist
ls -l /dev/video*

# Check kernel modules
lsmod | grep video

# Load USB video driver
sudo modprobe uvcvideo

# Check USB devices
lsusb | grep -i camera
```

### Permission Denied

**Symptoms**: "Cannot open device" errors

**Solution**:

```bash
# Check current permissions
ls -l /dev/video0

# Add user to video group
sudo usermod -a -G video $USER

# Or temporarily change permissions
sudo chmod a+rw /dev/video0
```

### Device Busy

**Symptoms**: "Device or resource busy" errors

**Cause**: Another application is using the camera

**Solution**:

```bash
# Find processes using video device
sudo fuser /dev/video0

# Kill process (replace PID)
kill <PID>

# Or close applications (Chrome, Skype, Zoom, etc.)
```

### Format Not Supported

**Symptoms**: "Set format succeeded (640x480 YUYV)" shows as skipped

**Cause**: Device doesn't support YUYV 640x480

**Solution**: This is expected behavior. The test gracefully handles unsupported formats.

### Profile Matching Fails

**Symptoms**: "No matching profile (generic device)"

**Cause**: Device not in DSV4L2 profile database

**Solution**: This is expected for unrecognized devices. Add profile to `profiles/` directory if needed.

## Adding New Hardware Tests

### Test Template

```c
static void test_new_feature(void)
{
    char device_path[32];

    printf("\n=== Test N: New Feature ===\n");

    if (!find_v4l2_device(device_path, sizeof(device_path))) {
        TEST_SKIP("No V4L2 devices available");
        return;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        TEST_SKIP("Cannot open device");
        return;
    }

    /* Test implementation */
    int rc = /* V4L2 operation */;
    TEST_ASSERT(rc == 0, "Operation succeeded");

    close(fd);
}
```

### Add to main()

```c
int main(void)
{
    /* Existing tests */
    test_device_detection();
    test_device_capabilities();
    /* ... */

    /* New test */
    test_new_feature();

    /* Print summary */
    /* ... */
}
```

## Hardware Test Best Practices

### 1. Always Check for Hardware

Every test must check for device availability before proceeding:

```c
if (!find_v4l2_device(device_path, sizeof(device_path))) {
    TEST_SKIP("No V4L2 devices available");
    return;
}
```

### 2. Graceful Degradation

Handle unsupported features gracefully:

```c
int rc = ioctl(fd, VIDIOC_S_FMT, &fmt);
if (rc == 0) {
    TEST_ASSERT(1, "Set format succeeded");
} else {
    printf("    ⊘ Format not supported\n");
    tests_skipped++;
}
```

### 3. Non-Destructive Testing

- **Query capabilities**: Use `VIDIOC_QUERYCAP`, `VIDIOC_ENUM_FMT`
- **Avoid streaming**: Don't call `VIDIOC_STREAMON`
- **Restore settings**: If changing formats, restore original

### 4. Device Independence

Tests must work on any V4L2 device:

- Don't assume specific resolutions
- Don't require specific pixel formats
- Don't assume specific capabilities (autofocus, exposure, etc.)

### 5. Close File Descriptors

Always close devices:

```c
int fd = open(device_path, O_RDWR);
/* ... test code ... */
close(fd);
```

## V4L2 API Reference

### Common ioctls Used

| ioctl | Purpose |
|-------|---------|
| `VIDIOC_QUERYCAP` | Query device capabilities |
| `VIDIOC_ENUM_FMT` | Enumerate supported formats |
| `VIDIOC_G_FMT` | Get current format |
| `VIDIOC_S_FMT` | Set format |
| `VIDIOC_REQBUFS` | Request buffers |
| `VIDIOC_QUERYBUF` | Query buffer |
| `VIDIOC_QBUF` | Queue buffer |
| `VIDIOC_DQBUF` | Dequeue buffer |
| `VIDIOC_STREAMON` | Start streaming |
| `VIDIOC_STREAMOFF` | Stop streaming |

### Capability Flags

| Flag | Meaning |
|------|---------|
| `V4L2_CAP_VIDEO_CAPTURE` | Video capture support |
| `V4L2_CAP_VIDEO_OUTPUT` | Video output support |
| `V4L2_CAP_STREAMING` | Streaming I/O support |
| `V4L2_CAP_READWRITE` | Read/write I/O support |

### Common Pixel Formats

| FourCC | Description |
|--------|-------------|
| `V4L2_PIX_FMT_YUYV` | YUYV 4:2:2 |
| `V4L2_PIX_FMT_MJPEG` | Motion JPEG |
| `V4L2_PIX_FMT_H264` | H.264 |
| `V4L2_PIX_FMT_RGB24` | RGB 24-bit |

## See Also

- **TESTING.md** - Overview of all test suites
- **TPM_INTEGRATION.md** - TPM2 hardware testing
- **COVERAGE_ANALYSIS.md** - Code coverage testing
- **Linux V4L2 API**: https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html
- **v4l-utils**: https://git.linuxtv.org/v4l-utils.git

## Summary

The DSV4L2 hardware detection test suite provides:

✓ **Graceful hardware detection** - Skips when no devices available
✓ **Non-destructive testing** - Only queries capabilities, never modifies settings
✓ **CI/CD friendly** - Passes on hardware-less systems
✓ **Real hardware validation** - Verifies DSV4L2 works with actual V4L2 devices
✓ **Profile matching** - Tests device-to-profile mapping
✓ **Comprehensive coverage** - 6 categories of V4L2 operations

For questions or issues, see the troubleshooting section above.
