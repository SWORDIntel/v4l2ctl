# V4L2CTL DSLLVM Enhancement Implementation Plan

## Executive Summary

Transform v4l2ctl from a simple v4l2 wrapper into a DSLLVM-instrumented, DSMIL-integrated sensor stack supporting:
- **TEMPEST-capable cameras** (currently stuck in secure mode)
- **Iris scanners** (biometric capture with SECRET classification)
- **IR metadata-only sensors** (thermal/radiometric data)
- **Military video streams** (10/12/14/16-bit formats, H.264/H.265 with KLV metadata)

## Current State Analysis

### Existing v4l2ctl Capabilities
- ✅ V4L2 device enumeration (`list_devices()`)
- ✅ Frame capture via MMAP (`Frame` class)
- ✅ Control enumeration and manipulation (`V4l2.get_controls()`, `set_control()`)
- ✅ Format and resolution management
- ✅ Frame rate control
- ✅ Cython-based Python bindings

### Gaps to Address
- ❌ No role-based device classification (camera vs iris vs IR vs TEMPEST)
- ❌ No TEMPEST security state awareness
- ❌ No metadata-plane capture (V4L2_BUF_TYPE_META_CAPTURE)
- ❌ No fused video+metadata synchronization
- ❌ No DSLLVM annotations or secret-flow tracking
- ❌ No biometric-specific capture modes
- ❌ No military format support (10/12/14/16-bit, KLV)
- ❌ No policy integration with DSMIL

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Python API Layer (v4l2ctl)                    │
├─────────────────────────────────────────────────────────────────┤
│              Cython Bindings (v4l2ctl.pyx)                       │
├─────────────────────────────────────────────────────────────────┤
│         DSV4L2 Enhanced Layer (NEW)                              │
│  ┌────────────┬────────────┬────────────┬──────────────┐        │
│  │  Profiles  │  TEMPEST   │  Metadata  │  Fusion      │        │
│  │  System    │  State     │  Capture   │  Engine      │        │
│  └────────────┴────────────┴────────────┴──────────────┘        │
├─────────────────────────────────────────────────────────────────┤
│         DSLLVM Runtime (libdsv4l2rt)                             │
│  • Event telemetry  • Secret tracking  • Policy hooks            │
├─────────────────────────────────────────────────────────────────┤
│              libv4l2 (existing dependency)                       │
└─────────────────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Foundation & DSLLVM Integration

#### 1.1 Project Structure Setup
Create new directory structure:
```
v4l2ctl/
├── dsv4l2/                  # NEW: Enhanced sensor layer
│   ├── include/
│   │   ├── dsv4l2_annotations.h  ✅ (already extracted)
│   │   ├── dsv4l2_policy.h       ✅ (already extracted)
│   │   ├── dsv4l2_core.h         ❌ (to create)
│   │   ├── dsv4l2_tempest.h      ❌ (to create)
│   │   ├── dsv4l2_meta.h         ❌ (to create)
│   │   ├── dsv4l2_fusion.h       ❌ (to create)
│   │   └── dsv4l2rt.h            ❌ (to create)
│   ├── src/
│   │   ├── dsv4l2_core.c         ❌
│   │   ├── dsv4l2_tempest.c      ❌
│   │   ├── dsv4l2_meta.c         ❌
│   │   ├── dsv4l2_fusion.c       ❌
│   │   ├── dsv4l2_profiles.c     ❌
│   │   └── dsv4l2rt.c            ❌
│   └── profiles/                 # Device role definitions
│       ├── iris_scanner.yaml     ❌
│       ├── tempest_camera.yaml   ❌
│       ├── ir_metadata.yaml      ❌
│       └── mil_stream.yaml       ❌
├── config/
│   └── dsllvm_dsv4l2_passes.yaml ✅ (already extracted)
├── v4l2ctl.pyx               ✅ (existing, to enhance)
├── v4l2.pxd                  ✅ (existing, to enhance)
└── setup.py                  ✅ (existing, to modify)
```

#### 1.2 DSLLVM Build Integration
- Modify `setup.py` to support optional DSLLVM compilation
- Add compile flags: `-fdsllvm`, `-fplugin=libDSLLVM.so`
- Link against `libdsv4l2rt.a`

#### 1.3 Runtime Library (libdsv4l2rt)
Implement minimal event telemetry runtime:
- Event types: `CAPTURE_START`, `CAPTURE_END`, `TEMPEST_TRANSITION`, `FORMAT_CHANGE`, `ERROR`
- Ring buffer for per-process events
- Flush to: stderr (debug), SQLite (local), Redis (DSMIL fabric)
- Optional TPM signing for forensic integrity

### Phase 2: Role-Aware Device Profiles

#### 2.1 Profile System Design
YAML-based device descriptors keyed by:
- USB VID:PID
- PCI ID
- Device path pattern (`/dev/video*`)

Example profile structure:
```yaml
id: "046d:0825"
role: "iris_scanner"
device_hint: "/dev/video0"
classification: "SECRET_BIOMETRIC"

pixel_format: "YUYV"
resolution: [1280, 720]
fps: 60

controls:
  exposure_auto: 1        # manual
  exposure_absolute: 50
  focus_auto: 0
  focus_absolute: 10

tempest_control:
  id: 0x9a0902           # vendor-specific control
  mode_map:
    DISABLED: 0
    LOW: 1
    HIGH: 2
    LOCKDOWN: 3

meta_device: "/dev/video2"  # companion metadata stream
meta_format: "META"
```

#### 2.2 Profile Loader
- C function: `dsv4l2_profile_load(device_path, role) -> profile_t`
- Python API: `Sensor.auto(role="iris_scanner")`
- Automatic VID:PID matching via udev enumeration

### Phase 3: TEMPEST State Machine

#### 3.1 State Model
```
DISABLED ──┬──> LOW ────> HIGH ────> LOCKDOWN
           │                            │
           └────────────────────────────┘
                   (unlock requires policy)
```

#### 3.2 Implementation
Core functions (in `dsv4l2_tempest.c`):
```c
dsv4l2_tempest_state_t
dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
    DSMIL_TEMPEST_QUERY;

int
dsv4l2_set_tempest_state(dsv4l2_device_t *dev,
                         dsv4l2_tempest_state_t state)
    DSMIL_TEMPEST_TRANSITION;
```

#### 3.3 Control Mapping
- Discover vendor controls via `VIDIOC_QUERYCTRL`
- Tag controls by name regex: `TEMPEST|PRIVACY|SECURE|SHUTTER`
- Map to profile's `tempest_control.id`

#### 3.4 Policy Integration
```c
int dsv4l2_policy_check_capture(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t current_state,
    const char *context
);
```
- Hook into DSMIL layer/threatcon context
- Enforce: no capture allowed in `LOCKDOWN` state
- DSLLVM verifies policy check happens before `VIDIOC_DQBUF`

### Phase 4: Metadata Plane Support

#### 4.1 Extend v4l2.pxd
Add missing V4L2 metadata types:
```cython
enum:
    V4L2_BUF_TYPE_META_CAPTURE
    V4L2_CAP_META_CAPTURE

cdef struct v4l2_meta_format:
    __u32 dataformat
    __u32 buffersize
```

#### 4.2 Metadata Stream Class
New Cython class in `v4l2ctl.pyx`:
```cython
cdef class MetaStream:
    cdef int fd
    cdef v4l2_format fmt
    cdef buffer_info *buffers

    cpdef bytes get_packet(self)
    # Returns raw metadata bytes + timestamp
```

#### 4.3 Decoders (Plugin System)
Define plugin interface:
```python
class MetadataDecoder:
    def matches(self, raw: bytes) -> bool: ...
    def decode(self, raw: bytes) -> dict: ...
```

Stock decoders:
- **FLIR radiometric** (temperature maps)
- **KLV-like** (key-length-value mil streams)
- **Vendor-specific IR calibration**

### Phase 5: Fused Video + Metadata Capture

#### 5.1 Fusion Engine
C implementation (`dsv4l2_fusion.c`):
```c
int dsv4l2_fused_capture(
    dsv4l2_device_t *video_dev,
    dsv4l2_meta_handle_t *meta_dev,
    dsv4l2_frame_t *out_frame,
    dsv4l2_meta_t *out_meta
) DSMIL_QUANTUM_CANDIDATE("fused_capture");
```

Alignment strategy:
- Timestamp-based synchronization (V4L2 buffer timestamps)
- Sequence number matching (V4L2 `v4l2_buffer.sequence`)
- Configurable tolerance window (±1 frame)

#### 5.2 Python API
```python
from v4l2ctl import FusionSensor

sensor = FusionSensor(
    video='/dev/video0',
    meta='/dev/video2'
)
frame, meta = sensor.capture()
temp_map = meta["radiometric"]["temperature_c"]
```

### Phase 6: Iris Scanner Mode

#### 6.1 Biometric-Optimized Capture
Profile-driven preset:
- **Lock exposure & gain** (tight bounds for repeatability)
- **Fix focus** to close distance (30-50cm)
- **Prefer monochrome** formats (GREY, Y16)
- **Enable IR illuminator** (vendor control)
- **ROI crop** to center/ellipse if supported

#### 6.2 Secret Region Enforcement
```c
int dsv4l2_capture_iris(
    dsv4l2_device_t *dev,
    dsv4l2_biometric_frame_t *out
) DSMIL_SECRET_REGION
  DSMIL_REQUIRES_TEMPEST_CHECK;
```

DSLLVM guarantees:
- No logging of iris buffers
- No network sends without `dsmil_declassify()`
- Constant-time processing (no secret-dependent branches)

#### 6.3 Python API
```python
iris_cam = Sensor.auto(role="iris_scanner")
iris_cam.enter_iris_mode()  # apply profile preset
frame = iris_cam.capture()  # DSLLVM-protected secret buffer
```

### Phase 7: Military Video Formats

#### 7.1 Extended Pixel Format Support
Add to `v4l2.pxd`:
```cython
enum: V4L2_PIX_FMT_Y10     # 10-bit grayscale
enum: V4L2_PIX_FMT_Y12     # 12-bit
enum: V4L2_PIX_FMT_Y14     # 14-bit (custom)
enum: V4L2_PIX_FMT_Y16     # 16-bit
enum: V4L2_PIX_FMT_H265    # H.265/HEVC
```

#### 7.2 KLV Metadata Support
- Extend `MetaStream` to handle embedded KLV in H.264/H.265 SEI
- Parser plugin: `KLVDecoder` (STANAG 4609 / MISB standards)
- Extract fields: GPS, altitude, sensor orientation, targeting data

#### 7.3 Security Tagging
Mark sensitive KLV fields as `DSMIL_SECRET`:
- GPS coordinates
- Unit identifiers
- Targeting crosshairs

### Phase 8: CLI Tools

#### 8.1 Sensor Management CLI
```bash
# Discover & classify devices
dsv4l2 scan

# Show TEMPEST state of all cameras
dsv4l2 list --role camera --show-tempest

# Attempt to unlock TEMPEST under DSMIL policy
dsv4l2 tempest --device /dev/video0 --state DISABLED

# Capture fused frame+meta
dsv4l2 capture-fused \
  --video /dev/video0 \
  --meta /dev/video2 \
  --out frame.raw \
  --meta-out frame.meta.json
```

#### 8.2 Implementation
- Thin Python wrapper over `dsv4l2` C library
- Uses `argparse` for CLI parsing
- Output formats: JSON, binary, human-readable

### Phase 9: Testing & Validation

#### 9.1 Unit Tests
Test coverage:
- Profile loading (valid/invalid YAML)
- TEMPEST state transitions (all valid edges)
- Metadata decoding (FLIR, KLV stubs)
- Fusion timestamp alignment
- Policy enforcement (DSLLVM pass verification)

#### 9.2 Integration Tests
Using `v4l2loopback` virtual devices:
- Simulate TEMPEST camera with fake control
- Inject metadata stream
- Verify fusion correctness
- Measure timestamp skew

#### 9.3 DSLLVM Verification
Build with DSLLVM passes enabled:
```bash
DSLLVM_CONFIG=config/dsllvm_dsv4l2_passes.yaml python setup.py build
```
Verify:
- No secret leakage warnings (SARIF log)
- TEMPEST policy checks enforced
- Constant-time guarantees on iris paths

### Phase 10: Documentation

#### 10.1 User Documentation
- **Quick Start**: Basic frame capture
- **Role Profiles**: How to define custom sensors
- **TEMPEST Guide**: Managing security states
- **Metadata Fusion**: Synchronizing video + IR/KLV
- **DSLLVM Integration**: Building with security passes

#### 10.2 Example Profiles
- `iris_scanner.yaml` – SECRET_BIOMETRIC iris camera
- `tempest_camera.yaml` – Rugged secure camera
- `ir_metadata.yaml` – Thermal/radiometric sensor
- `mil_stream.yaml` – H.265 + KLV mil video

#### 10.3 API Reference
Auto-generated from docstrings:
- Sphinx for Python API
- Doxygen for C headers

## Implementation Timeline

### Sprint 1 (Foundation)
- ✅ Extract DSLLVM bundle (completed)
- Create directory structure
- Implement `libdsv4l2rt` stub
- Modify `setup.py` for DSLLVM build

### Sprint 2 (Profiles & TEMPEST)
- Implement profile loader
- Create TEMPEST state machine
- Vendor control discovery & mapping
- Example profiles (iris, TEMPEST, IR)

### Sprint 3 (Metadata & Fusion)
- Extend `v4l2.pxd` for metadata types
- Implement `MetaStream` class
- Build fusion engine with timestamp alignment
- Add decoder plugins (FLIR, KLV stubs)

### Sprint 4 (Iris & Military Formats)
- Iris mode presets
- Secret region enforcement
- Extended pixel format support
- KLV parser

### Sprint 5 (CLI & Testing)
- Build `dsv4l2` CLI tool
- Unit test suite
- Integration tests with `v4l2loopback`
- DSLLVM verification

### Sprint 6 (Documentation & Polish)
- User documentation
- API reference
- Example code
- Performance tuning

## Success Criteria

### Functional
- [ ] Camera stuck in TEMPEST mode can be unlocked via policy
- [ ] IR metadata-only device streams to Python
- [ ] Iris scanner captures biometric frames with SECRET tagging
- [ ] Fused video+metadata synchronized within ±1 frame
- [ ] Military formats (Y16, H.265+KLV) supported

### Security
- [ ] DSLLVM secret-flow pass: no violations
- [ ] DSLLVM TEMPEST policy pass: all checks enforced
- [ ] Constant-time pass: iris path verified
- [ ] No biometric data logged or sent unencrypted

### Performance
- [ ] Frame capture latency: ≤ +10% vs baseline v4l2ctl
- [ ] Fusion overhead: ≤ 5ms per frame pair
- [ ] Profile load time: ≤ 1ms

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Vendor-specific TEMPEST controls vary | Profile-driven mapping; fallback to generic privacy controls |
| Metadata timestamp skew | Configurable tolerance window; hardware sync where available |
| DSLLVM annotation overhead | Conditional compilation; disable for non-secure builds |
| v4l2loopback limitations | Supplement with hardware test rigs |

## Open Questions

1. **DSMIL Integration Point**: Where does the policy hook call DSMIL? Redis? gRPC? Local function?
2. **Quantum Candidate Handling**: What triggers offload of `dsv4l2_fused_capture` to L7/L8?
3. **TPM Signing**: Is event log signing required for all modes or only forensic profile?
4. **IR Format Support**: Which specific thermal formats are in scope (FLIR Lepton? LWIR?)?

## Next Steps

1. Review and approve this plan
2. Begin Sprint 1 implementation
3. Set up CI/CD for DSLLVM builds
4. Identify hardware test devices (iris scanner, TEMPEST camera, IR sensor)

---

**Document Version**: 1.0
**Date**: 2025-11-25
**Status**: DRAFT – Awaiting approval
