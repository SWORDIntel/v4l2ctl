# V4L2CTL DSLLVM Enhancement – Technical Specification

## 1. Core Data Structures

### 1.1 Device Profile (C struct)

```c
typedef struct {
    char id[64];                  // USB VID:PID or PCI ID
    char role[32];                // "camera", "iris_scanner", "ir_sensor", etc.
    char device_hint[256];        // "/dev/video0"
    char classification[64];      // "UNCLASSIFIED", "SECRET_BIOMETRIC", etc.

    // Format preferences
    uint32_t pixel_format;        // fourcc
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;

    // Control presets (up to 32 controls)
    struct {
        uint32_t id;
        int32_t  value;
    } controls[32];
    int num_controls;

    // TEMPEST control mapping
    struct {
        uint32_t control_id;
        int32_t  disabled_value;
        int32_t  low_value;
        int32_t  high_value;
        int32_t  lockdown_value;
    } tempest_control;

    // Companion metadata device
    char meta_device_path[256];
    uint32_t meta_format;

} dsv4l2_profile_t;
```

### 1.2 Device Handle (C struct)

```c
typedef struct dsv4l2_device {
    int fd;                           // v4l2 device fd
    char dev_path[256];
    dsv4l2_profile_t *profile;        // loaded profile

    // Current state
    dsv4l2_tempest_state_t tempest_state;
    v4l2_format current_format;
    v4l2_streamparm current_parm;

    // Buffer management
    int num_buffers;
    buffer_info *buffers;

    // Metadata companion (optional)
    dsv4l2_meta_handle_t *meta_handle;

} dsv4l2_device_t;
```

### 1.3 Frame Types (DSLLVM-annotated)

```c
// Generic frame (moderate security)
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
} DSMIL_SECRET("generic_frame") dsv4l2_frame_t;

// Biometric frame (high security)
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
} DSMIL_SECRET("biometric_frame") dsv4l2_biometric_frame_t;

// Metadata packet
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
    uint32_t format;  // FLIR, KLV, etc.
} DSMIL_META("radiometric") dsv4l2_meta_t;
```

### 1.4 Event Structure (Runtime Telemetry)

```c
typedef enum {
    DSV4L2_EVENT_CAPTURE_START      = 1,
    DSV4L2_EVENT_CAPTURE_END        = 2,
    DSV4L2_EVENT_TEMPEST_TRANSITION = 3,
    DSV4L2_EVENT_FORMAT_CHANGE      = 4,
    DSV4L2_EVENT_ERROR              = 5,
    DSV4L2_EVENT_POLICY_CHECK       = 6,
} dsv4l2_event_type_t;

typedef enum {
    DSV4L2_SEV_INFO    = 0,
    DSV4L2_SEV_WARNING = 1,
    DSV4L2_SEV_ERROR   = 2,
    DSV4L2_SEV_CRIT    = 3,
} dsv4l2_severity_t;

typedef struct {
    uint64_t            ts_ns;
    uint32_t            dev_id;
    uint16_t            event_type;
    uint16_t            severity;
    uint32_t            aux;          // event-specific data
    char                context[64];  // optional description
} dsv4l2_event_t;
```

## 2. API Specifications

### 2.1 Profile Management

#### C API
```c
// Load profile by device path and role
int dsv4l2_profile_load(
    const char *device_path,
    const char *role,
    dsv4l2_profile_t **out_profile
);

// Load profile by VID:PID
int dsv4l2_profile_load_by_vidpid(
    uint16_t vendor_id,
    uint16_t product_id,
    const char *role,
    dsv4l2_profile_t **out_profile
);

// Apply profile settings to device
int dsv4l2_profile_apply(
    dsv4l2_device_t *dev,
    const dsv4l2_profile_t *profile
);

// Free profile
void dsv4l2_profile_free(dsv4l2_profile_t *profile);
```

#### Python API
```python
from v4l2ctl import Sensor, list_devices_by_role

# Auto-detect and load by role
sensor = Sensor.auto(role="iris_scanner")

# Explicit profile loading
sensor = Sensor("/dev/video0", profile="profiles/iris_scanner.yaml")

# Apply profile settings
sensor.apply_profile()

# List all devices grouped by role
devices = list_devices_by_role()
# {
#   "camera": ["/dev/video0", "/dev/video1"],
#   "iris_scanner": ["/dev/video2"],
#   "ir_sensor": ["/dev/video3"]
# }
```

### 2.2 TEMPEST State Management

#### C API
```c
// Query current TEMPEST state
dsv4l2_tempest_state_t
dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
    DSMIL_TEMPEST_QUERY;

// Set TEMPEST state (with policy check)
int
dsv4l2_set_tempest_state(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t target_state
) DSMIL_TEMPEST_TRANSITION;

// Policy check (called internally by capture)
int
dsv4l2_policy_check_capture(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t current_state,
    const char *context
);

// Discover TEMPEST control (scan vendor controls)
int
dsv4l2_discover_tempest_control(
    dsv4l2_device_t *dev,
    uint32_t *out_control_id
);
```

#### Python API
```python
from v4l2ctl import TempestState

# Get current state
state = sensor.get_tempest_state()

# Set state (raises exception if policy denies)
sensor.set_tempest_state(TempestState.DISABLED)

# Context manager for temporary state change
with sensor.tempest_mode(TempestState.LOW):
    frame = sensor.capture()
# Automatically restores previous state
```

### 2.3 Frame Capture

#### C API
```c
// Generic frame capture
int
dsv4l2_capture_frame(
    dsv4l2_device_t *dev,
    dsv4l2_frame_t *out_frame
) DSMIL_REQUIRES_TEMPEST_CHECK;

// Biometric (iris) capture
int
DSMIL_SECRET_REGION
dsv4l2_capture_iris(
    dsv4l2_device_t *dev,
    dsv4l2_biometric_frame_t *out_frame
) DSMIL_REQUIRES_TEMPEST_CHECK;

// Start/stop streaming
int dsv4l2_start_stream(dsv4l2_device_t *dev);
int dsv4l2_stop_stream(dsv4l2_device_t *dev);
```

#### Python API
```python
# Simple capture
frame_data = sensor.capture()  # returns bytes

# Capture with metadata
frame = sensor.capture_frame()
# Returns: {
#   "data": bytes,
#   "timestamp": int (ns),
#   "sequence": int,
#   "tempest_state": TempestState
# }

# Iris mode (biometric)
iris_sensor = Sensor.auto(role="iris_scanner")
iris_sensor.enter_iris_mode()
iris_frame = iris_sensor.capture_iris()  # DSMIL_SECRET-tagged
```

### 2.4 Metadata Capture

#### C API
```c
// Open metadata stream
int dsv4l2_meta_open(
    const char *device_path,
    dsv4l2_meta_handle_t **out_handle
);

// Read metadata packet
int dsv4l2_meta_read(
    dsv4l2_meta_handle_t *handle,
    dsv4l2_meta_t *out_meta
);

// Close metadata stream
void dsv4l2_meta_close(dsv4l2_meta_handle_t *handle);
```

#### Python API
```python
from v4l2ctl import MetaStream

meta = MetaStream("/dev/video2")
packet = meta.get_packet()
# Returns: {
#   "data": bytes,
#   "timestamp": int (ns),
#   "sequence": int,
#   "format": "FLIR" | "KLV" | "RAW"
# }

# Decoded metadata (if decoder available)
decoded = meta.get_decoded()
# Example for FLIR:
# {
#   "temperature_c": [[21.5, 22.1, ...], ...],
#   "min_temp": 18.2,
#   "max_temp": 35.7
# }
```

### 2.5 Fused Capture

#### C API
```c
int
dsv4l2_fused_capture(
    dsv4l2_device_t *video_dev,
    dsv4l2_meta_handle_t *meta_dev,
    dsv4l2_frame_t *out_frame,
    dsv4l2_meta_t *out_meta
) DSMIL_QUANTUM_CANDIDATE("fused_capture")
  DSMIL_REQUIRES_TEMPEST_CHECK;

// Configure fusion parameters
int dsv4l2_fusion_set_tolerance(
    dsv4l2_device_t *dev,
    uint64_t tolerance_ns
);
```

#### Python API
```python
from v4l2ctl import FusionSensor

fusion = FusionSensor(
    video="/dev/video0",
    meta="/dev/video2",
    tolerance_ms=5  # max timestamp difference
)

frame, meta = fusion.capture()
# Returns tuple:
# (
#   {"data": bytes, "timestamp": int, "sequence": int},
#   {"data": bytes, "timestamp": int, "format": "FLIR", "decoded": {...}}
# )
```

### 2.6 Runtime Telemetry

#### C API
```c
// Initialize runtime
void dsv4l2rt_init(void);

// Emit event
void dsv4l2rt_emit(const dsv4l2_event_t *event);

// Flush events to backend
void dsv4l2rt_flush(void);

// Convenience logging functions
void dsv4l2rt_log_capture_start(uint32_t dev_id);
void dsv4l2rt_log_capture_end(uint32_t dev_id, int rc);
void dsv4l2rt_log_tempest_transition(
    uint32_t dev_id,
    dsv4l2_tempest_state_t old_state,
    dsv4l2_tempest_state_t new_state
);
void dsv4l2rt_log_policy_check(
    uint32_t dev_id,
    const char *context,
    int result
);
```

#### Python API
```python
from v4l2ctl.runtime import EventStream, EventType

# Subscribe to events
events = EventStream()

for event in events:
    if event.type == EventType.TEMPEST_TRANSITION:
        print(f"TEMPEST: {event.old_state} → {event.new_state}")
    elif event.type == EventType.CAPTURE_ERROR:
        print(f"Error: {event.message}")
```

## 3. Profile Format Specification

### 3.1 Full YAML Schema

```yaml
# Device identification (required)
id: "046d:0825"  # USB VID:PID, PCI ID, or "*" for wildcard
role: "iris_scanner"  # camera, iris_scanner, ir_sensor, tempest_cam, mil_stream
device_hint: "/dev/video0"  # preferred device path

# Security classification (required for DSLLVM)
classification: "SECRET_BIOMETRIC"  # UNCLASSIFIED, CONFIDENTIAL, SECRET_BIOMETRIC

# Format preferences
pixel_format: "YUYV"  # fourcc string
resolution: [1280, 720]
fps: 60  # or [numerator, denominator] for fractional

# Control presets (applied on profile load)
controls:
  exposure_auto: 1       # 0=auto, 1=manual
  exposure_absolute: 50
  focus_auto: 0
  focus_absolute: 10
  brightness: 128
  ir_illuminator: 1      # vendor-specific

# TEMPEST control mapping
tempest_control:
  id: 0x9a0902           # v4l2 control ID
  mode_map:
    DISABLED: 0
    LOW: 1
    HIGH: 2
    LOCKDOWN: 3
  auto_detect: true      # scan for TEMPEST|PRIVACY|SECURE controls

# Metadata companion device (optional)
meta_device: "/dev/video2"
meta_format: "META"

# Advanced options
advanced:
  buffer_count: 4
  streaming_mode: "mmap"  # mmap, userptr, dmabuf
  constant_time_required: true  # force DSLLVM constant-time pass
  quantum_candidate: false      # mark for L7/L8 offload
```

### 3.2 Profile Inheritance

```yaml
# Base profile
id: "base_camera"
role: "camera"
pixel_format: "YUYV"
resolution: [640, 480]
fps: 30

---
# Derived profile (inherits from base_camera)
extends: "base_camera"
id: "046d:0825"
role: "iris_scanner"
classification: "SECRET_BIOMETRIC"
resolution: [1280, 720]  # override
fps: 60
controls:
  ir_illuminator: 1
```

## 4. Metadata Format Specifications

### 4.1 FLIR Radiometric Format

Binary layout (little-endian):
```
Offset  Size  Field
------  ----  -----
0       4     Magic ("FLIR")
4       2     Width (pixels)
6       2     Height (pixels)
8       4     Min temperature (float, °C)
12      4     Max temperature (float, °C)
16      W*H*2 Temperature map (uint16, Kelvin * 100)
```

Python decoder:
```python
class FLIRDecoder(MetadataDecoder):
    def matches(self, raw: bytes) -> bool:
        return raw[:4] == b'FLIR'

    def decode(self, raw: bytes) -> dict:
        width, height = struct.unpack('<HH', raw[4:8])
        min_t, max_t = struct.unpack('<ff', raw[8:16])
        temps = np.frombuffer(raw[16:], dtype=np.uint16)
        temps = temps.reshape((height, width)) / 100.0 - 273.15
        return {
            "temperature_c": temps.tolist(),
            "min_temp": min_t,
            "max_temp": max_t
        }
```

### 4.2 KLV (STANAG 4609) Format

Key-Length-Value triplets:
```
Key (1-16 bytes, BER-OID encoding)
Length (1-4 bytes, BER encoding)
Value (Length bytes)
```

Common keys (MISB):
- `0x01` – Checksum
- `0x02` – UNIX timestamp
- `0x0D` – Sensor latitude
- `0x0E` – Sensor longitude
- `0x0F` – Sensor altitude

Python decoder (simplified):
```python
class KLVDecoder(MetadataDecoder):
    def decode(self, raw: bytes) -> dict:
        result = {}
        offset = 0
        while offset < len(raw):
            key = raw[offset]
            length = raw[offset+1]
            value = raw[offset+2:offset+2+length]
            offset += 2 + length

            if key == 0x02:
                result["timestamp"] = struct.unpack('>Q', value)[0]
            elif key == 0x0D:
                result["latitude"] = struct.unpack('>d', value)[0]
            elif key == 0x0E:
                result["longitude"] = struct.unpack('>d', value)[0]
            # ... more keys

        return result
```

## 5. DSLLVM Integration Details

### 5.1 Compilation Flags

```bash
# Standard build (no DSLLVM)
python setup.py build

# DSLLVM-enabled build
DSLLVM_ENABLED=1 \
DSLLVM_PLUGIN=/path/to/libDSLLVM.so \
DSLLVM_CONFIG=config/dsllvm_dsv4l2_passes.yaml \
python setup.py build
```

Modified `setup.py`:
```python
import os

extra_compile_args = []
extra_link_args = []

if os.environ.get('DSLLVM_ENABLED'):
    plugin_path = os.environ['DSLLVM_PLUGIN']
    config_path = os.environ['DSLLVM_CONFIG']

    extra_compile_args += [
        f'-fplugin={plugin_path}',
        f'-fplugin-arg-dsllvm-pass-config={config_path}',
        '-fdsllvm',
    ]
    extra_link_args += ['-ldsv4l2rt']

extensions = [
    Extension(
        name="v4l2ctl",
        sources=['v4l2ctl.pyx', 'dsv4l2/src/*.c'],
        include_dirs=['dsv4l2/include'],
        libraries=['v4l2'],
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args
    ),
]
```

### 5.2 Annotation Examples

#### Secret Frame Buffer
```c
// Buffer is automatically tagged as DSMIL_SECRET
dsv4l2_biometric_frame_t iris_frame;

// This is allowed (encrypted storage)
dsv4l2_store_encrypted(&iris_frame, "/secure/storage");

// This will FAIL at compile time (DSLLVM secret-flow pass)
printf("Iris data: %s\n", iris_frame.data);  // ❌ VIOLATION

// This will FAIL (network send without declassify)
send(sock_fd, iris_frame.data, iris_frame.len, 0);  // ❌ VIOLATION
```

#### TEMPEST Policy Enforcement
```c
int my_custom_capture(dsv4l2_device_t *dev, dsv4l2_frame_t *out) {
    // DSLLVM will ERROR if we don't check TEMPEST state
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);

    if (dsv4l2_policy_check_capture(dev, state, "my_custom_capture") != 0) {
        return -EACCES;
    }

    // ... actual capture ...
}
```

#### Constant-Time Region
```c
DSMIL_SECRET_REGION
int process_iris(const dsv4l2_biometric_frame_t *frame) {
    // This is OK (constant iteration count)
    for (int i = 0; i < IRIS_SIZE; i++) {
        // ...
    }

    // This will FAIL (secret-dependent branch)
    if (frame->data[0] > THRESHOLD) {  // ❌ VIOLATION
        // ...
    }

    return 0;
}
```

## 6. CLI Tool Specification

### 6.1 Command Structure

```
dsv4l2 <command> [options]

Commands:
  scan              Discover and classify all v4l2 devices
  list              List devices with optional filtering
  info              Show detailed device information
  tempest           Manage TEMPEST state
  capture           Capture frames/metadata
  capture-fused     Capture synchronized video+metadata
  profile           Manage device profiles
  test              Run diagnostic tests
```

### 6.2 Command Details

#### `dsv4l2 scan`
```bash
dsv4l2 scan [--json]

# Output:
Role: camera
  /dev/video0  (Logitech HD Webcam)
  /dev/video1  (USB2.0 Camera)

Role: iris_scanner
  /dev/video2  (IrisGuard IG-H100)

Role: ir_sensor
  /dev/video3  (FLIR Lepton 3.5) [metadata: /dev/video4]
```

#### `dsv4l2 tempest`
```bash
# Show current TEMPEST state
dsv4l2 tempest --device /dev/video0

# Set TEMPEST state (with policy check)
dsv4l2 tempest --device /dev/video0 --set DISABLED

# Force unlock (requires explicit --force flag)
dsv4l2 tempest --device /dev/video0 --set DISABLED --force
```

#### `dsv4l2 capture-fused`
```bash
dsv4l2 capture-fused \
  --video /dev/video0 \
  --meta /dev/video2 \
  --frames 10 \
  --out-video frames_%04d.raw \
  --out-meta frames_%04d.json \
  --tolerance-ms 5
```

## 7. Error Handling

### 7.1 Error Codes

```c
#define DSV4L2_OK                  0
#define DSV4L2_E_INVALID_DEVICE   -1
#define DSV4L2_E_NO_PROFILE       -2
#define DSV4L2_E_TEMPEST_DENIED   -3
#define DSV4L2_E_POLICY_VIOLATION -4
#define DSV4L2_E_TIMESTAMP_SKEW   -5
#define DSV4L2_E_META_NOT_FOUND   -6
#define DSV4L2_E_FORMAT_MISMATCH  -7
```

### 7.2 Python Exceptions

```python
class DSV4L2Error(Exception):
    """Base exception for all dsv4l2 errors"""
    pass

class ProfileNotFoundError(DSV4L2Error):
    """No matching profile found for device"""
    pass

class TEMPESTDeniedError(DSV4L2Error):
    """TEMPEST state change denied by policy"""
    pass

class TimestampSkewError(DSV4L2Error):
    """Video and metadata timestamps too far apart"""
    pass
```

## 8. Performance Targets

| Operation | Target Latency | Notes |
|-----------|---------------|-------|
| Profile load | < 1 ms | YAML parsing + control setup |
| TEMPEST state query | < 0.1 ms | Single ioctl |
| TEMPEST state change | < 10 ms | ioctl + policy check |
| Frame capture | < 5 ms overhead | vs baseline v4l2ctl |
| Metadata read | < 2 ms | Single buffer read |
| Fused capture | < 7 ms total | Frame + meta + alignment |
| Event emit | < 0.01 ms | Ring buffer write |

## 9. Testing Strategy

### 9.1 Unit Tests (C)
- Profile loader (valid/malformed YAML)
- TEMPEST state machine (all transitions)
- Event ring buffer (overflow handling)
- Metadata decoders (FLIR, KLV edge cases)

### 9.2 Integration Tests (Python)
- End-to-end capture with v4l2loopback
- Fusion timestamp alignment
- DSLLVM policy violation detection
- Profile inheritance

### 9.3 Hardware Tests
- Real iris scanner
- TEMPEST camera unlock/lock cycles
- IR metadata stream
- Military H.265+KLV stream

---

**Document Version**: 1.0
**Date**: 2025-11-25
**Status**: DRAFT
