# DSV4L2 – DSLLVM-Enhanced V4L2 Sensor Library

**Phase 1 Implementation** – Foundation & DSLLVM Integration

## Overview

DSV4L2 is a C library that extends libv4l2 with:
- **Role-aware device profiles** (YAML-based configuration)
- **TEMPEST state management** (security mode control)
- **Metadata capture** (V4L2_BUF_TYPE_META_CAPTURE)
- **Video + metadata fusion** (timestamp-aligned)
- **DSLLVM integration** (secret-flow tracking, policy enforcement)
- **Runtime telemetry** (event logging for DSMIL)

## Status

This is a **Phase 1 stub implementation**. Core functionality includes:

✅ **Implemented**:
- Header definitions for all APIs
- Runtime telemetry (libdsv4l2rt)
- YAML profile format specification
- Stub C implementations (compile-ready)
- Makefile with optional DSLLVM support

⏳ **Pending** (Phase 2+):
- Actual v4l2 device I/O
- YAML profile parsing
- Control enumeration and mapping
- Metadata capture
- Fusion algorithms

## Directory Structure

```
dsv4l2/
├── include/               # Public headers
│   ├── dsv4l2_annotations.h    # DSLLVM attribute macros
│   ├── dsv4l2_core.h           # Core device API
│   ├── dsv4l2_fusion.h         # Video+metadata fusion
│   ├── dsv4l2_meta.h           # Metadata capture
│   ├── dsv4l2_policy.h         # TEMPEST + policy
│   ├── dsv4l2_profiles.h       # Profile management
│   └── dsv4l2rt.h              # Runtime telemetry
├── src/                   # Implementation
│   ├── dsv4l2_core.c
│   ├── dsv4l2_fusion.c
│   ├── dsv4l2_meta.c
│   ├── dsv4l2_profiles.c
│   ├── dsv4l2_tempest.c
│   └── dsv4l2rt.c              # Runtime (functional)
├── profiles/              # Device profiles (YAML)
│   ├── iris_scanner.yaml       # Biometric iris capture
│   ├── tempest_camera.yaml     # Security-hardened camera
│   ├── ir_metadata.yaml        # Thermal/radiometric sensor
│   └── mil_stream.yaml         # Military video + KLV
├── Makefile               # Build system
└── README.md              # This file
```

## Building

### Standard Build (no DSLLVM)
```bash
cd dsv4l2
make
```

Produces:
- `lib/libdsv4l2.a` (static)
- `lib/libdsv4l2.so` (shared)

### DSLLVM Build
```bash
cd dsv4l2
make dsllvm DSLLVM_PLUGIN=/path/to/libDSLLVM.so
```

Requires:
- DSLLVM compiler (`dsclang`)
- Pass config: `../config/dsllvm_dsv4l2_passes.yaml`

### Installation
```bash
sudo make install
```

Installs to:
- `/usr/local/lib/libdsv4l2.{a,so}`
- `/usr/local/include/dsv4l2/*.h`

## Usage Example

```c
#include <dsv4l2/dsv4l2_core.h>
#include <dsv4l2/dsv4l2_tempest.h>
#include <dsv4l2/dsv4l2rt.h>

int main() {
    /* Initialize runtime */
    dsv4l2rt_init();

    /* Open device */
    dsv4l2_device_t *dev;
    int rc = dsv4l2_open_device("/dev/video0", NULL, &dev);
    if (rc != 0) {
        fprintf(stderr, "Failed to open device: %d\n", rc);
        return 1;
    }

    /* Check TEMPEST state */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    printf("TEMPEST state: %d\n", state);

    /* Capture frame (stub - will return -ENOSYS in Phase 1) */
    dsv4l2_frame_t frame;
    rc = dsv4l2_capture_frame(dev, &frame);

    /* Close device */
    dsv4l2_close_device(dev);

    /* Flush events */
    dsv4l2rt_flush();
    dsv4l2rt_shutdown();

    return 0;
}
```

Compile:
```bash
gcc -o test test.c -I/usr/local/include -L/usr/local/lib -ldsv4l2 -pthread
```

## Profiles

Profiles are YAML files that define device-specific configuration:

### Iris Scanner Example
```yaml
id: "builtin_iris"
role: "iris_scanner"
classification: "SECRET_BIOMETRIC"

pixel_format: "MJPG"      # MJPEG for bandwidth
resolution: [1280, 720]
fps: 30

controls:
  exposure_auto: 1         # Manual
  focus_auto: 0
  focus_absolute: 10       # Close focus

tempest_control:
  auto_detect: true
```

See `profiles/` directory for complete examples.

## DSLLVM Integration

### Annotations

DSV4L2 uses DSLLVM attributes to enforce security:

- **`DSMIL_SECRET("tag")`** – Mark biometric/sensitive data
- **`DSMIL_TEMPEST_QUERY`** – Query TEMPEST state
- **`DSMIL_TEMPEST_TRANSITION`** – Change TEMPEST state
- **`DSMIL_REQUIRES_TEMPEST_CHECK`** – Enforce policy check
- **`DSMIL_SECRET_REGION`** – Constant-time enforcement
- **`DSMIL_QUANTUM_CANDIDATE("tag")`** – Mark for offload

### Passes

Configured in `../config/dsllvm_dsv4l2_passes.yaml`:

1. **Secret Flow** – Prevents logging/leakage of biometric data
2. **TEMPEST Policy** – Enforces state checks before capture
3. **Constant Time** – Prevents timing side-channels in iris code

## Runtime Telemetry

libdsv4l2rt provides event logging:

```c
dsv4l2rt_log_capture_start(dev_id);
dsv4l2rt_log_tempest_transition(dev_id, old_state, new_state);
dsv4l2rt_log_policy_check(dev_id, context, result);
dsv4l2rt_flush();  // Print to stderr
```

Future: flush to Redis, SQLite, or DSMIL fabric.

## Next Steps (Phase 2)

1. Implement YAML profile parsing (libyaml)
2. Complete v4l2 device I/O
3. Control enumeration and TEMPEST discovery
4. Profile application logic
5. Unit tests

## License

Same as parent v4l2ctl project (MIT).

## Contact

See parent project for contact information.

---

**Phase**: 1 of 10
**Status**: ✅ Foundation Complete
**Next**: Phase 2 (Profiles & TEMPEST)
