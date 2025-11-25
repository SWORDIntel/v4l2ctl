# V4L2CTL DSLLVM Enhancement – Project Summary

## Overview

This project transforms the existing v4l2ctl library (a simple Python/Cython wrapper for libv4l2) into a comprehensive, DSLLVM-instrumented sensor management stack capable of handling:

- **Cameras stuck in TEMPEST mode** – Security state management and policy-driven unlocking
- **Iris scanners** – Biometric capture with SECRET classification and constant-time guarantees
- **IR metadata-only sensors** – Thermal/radiometric data streams
- **Military video streams** – Extended formats (10/12/14/16-bit), H.264/H.265 with KLV metadata

## Problem Statement

Based on the ChatGPT conversation, you have:

1. A camera stuck in TEMPEST-only mode that needs policy-controlled state management
2. An IR sensor outputting only metadata (no video frames)
3. Requirements for iris scanner support with biometric-grade security
4. Need for military-type video stream support with embedded metadata

The current v4l2ctl library lacks:
- Role-aware device classification
- TEMPEST state machine
- Metadata-plane capture (V4L2_BUF_TYPE_META_CAPTURE)
- DSLLVM annotations for secret tracking and policy enforcement
- Synchronized video+metadata fusion

## Solution Architecture

### Core Components

1. **DSV4L2 Layer** (NEW)
   - C library with DSLLVM annotations
   - Role-based device profiles (YAML)
   - TEMPEST state machine
   - Metadata capture and fusion
   - Runtime telemetry (libdsv4l2rt)

2. **Enhanced v4l2ctl** (MODIFIED)
   - Python bindings to DSV4L2
   - Backward-compatible with existing API
   - New classes: `Sensor`, `MetaStream`, `FusionSensor`

3. **DSLLVM Integration**
   - Secret-flow tracking (biometric frames)
   - TEMPEST policy enforcement
   - Constant-time guarantees for sensitive code

4. **CLI Tools**
   - Device discovery and classification
   - TEMPEST state management
   - Fused capture utility

### Key Features

#### 1. Role-Aware Profiles

Devices are classified by role:
- `camera` – Generic video capture
- `iris_scanner` – Biometric capture (SECRET_BIOMETRIC)
- `ir_sensor` – Thermal/radiometric metadata
- `tempest_cam` – Security-hardened camera
- `mil_stream` – Military video with KLV metadata

Each role has a YAML profile defining:
- Pixel formats and resolutions
- Control presets (exposure, focus, etc.)
- TEMPEST control mapping
- Security classification
- Companion metadata device

#### 2. TEMPEST State Machine

States:
- `DISABLED` – Normal operation
- `LOW` – Basic shutter/IR filter enabled
- `HIGH` – Full privacy mode
- `LOCKDOWN` – No capture allowed

Policy integration:
- Hooks into DSMIL layer/threatcon context
- DSLLVM enforces state checks before capture
- Audit trail of all state transitions

#### 3. Metadata Capture & Fusion

New capabilities:
- `V4L2_BUF_TYPE_META_CAPTURE` support
- Timestamp-based synchronization
- Pluggable decoders (FLIR, KLV, custom)
- Fused capture API returns aligned (frame, metadata) pairs

#### 4. Iris Scanner Support

Biometric-optimized mode:
- Fixed exposure/focus for repeatability
- IR illuminator control
- Monochrome formats (GREY, Y16)
- DSLLVM secret-region enforcement (no logging, constant-time)

#### 5. Military Formats

Extended support:
- 10/12/14/16-bit grayscale
- H.264/H.265 with embedded KLV (STANAG 4609)
- MISB metadata standard parsing
- Secret-tagged GPS/targeting data

## Documentation Deliverables

### Created Documents

1. **IMPLEMENTATION_PLAN.md** (43KB)
   - 10 implementation phases
   - Sprint breakdown (6 sprints)
   - Success criteria
   - Risk mitigation
   - Directory structure

2. **TECHNICAL_SPEC.md** (36KB)
   - Complete API specifications (C and Python)
   - Data structure definitions
   - Profile format (YAML schema)
   - Metadata format specs (FLIR, KLV)
   - DSLLVM integration details
   - CLI tool specifications
   - Performance targets
   - Testing strategy

3. **Existing Bundle Files** (extracted)
   - `README.md` – DSLLVM bundle overview
   - `include/dsv4l2_annotations.h` – DSLLVM attribute macros
   - `include/dsv4l2_policy.h` – Public API surface
   - `config/dsllvm_dsv4l2_passes.yaml` – DSLLVM pass configuration

## Implementation Status

### Phase Breakdown

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Foundation & DSLLVM Integration | ⏳ READY TO START |
| 2 | Role-Aware Profiles | 📋 PLANNED |
| 3 | TEMPEST State Machine | 📋 PLANNED |
| 4 | Metadata Capture | 📋 PLANNED |
| 5 | Fusion Engine | 📋 PLANNED |
| 6 | Iris Scanner Mode | 📋 PLANNED |
| 7 | Military Formats | 📋 PLANNED |
| 8 | CLI Tools | 📋 PLANNED |
| 9 | Testing | 📋 PLANNED |
| 10 | Documentation | 📋 PLANNED |

### Sprint Timeline

**Sprint 1** (Foundation): 1-2 weeks
- Directory structure
- libdsv4l2rt stub
- Modified setup.py with DSLLVM support

**Sprint 2** (Profiles & TEMPEST): 1-2 weeks
- Profile loader
- TEMPEST state machine
- Example profiles (iris, TEMPEST, IR)

**Sprint 3** (Metadata & Fusion): 2-3 weeks
- Metadata capture
- Fusion engine
- Decoder plugins

**Sprint 4** (Iris & Military): 1-2 weeks
- Iris mode
- Extended formats
- KLV parser

**Sprint 5** (CLI & Testing): 1-2 weeks
- CLI tool
- Test suite
- DSLLVM verification

**Sprint 6** (Documentation & Polish): 1 week
- User docs
- API reference
- Examples

**Total**: 8-12 weeks

## Next Steps

### Immediate Actions

1. **Review Planning Documents**
   - Validate architecture decisions
   - Confirm requirements match your hardware
   - Identify any missing features

2. **Hardware Inventory**
   - Identify actual devices:
     - Stuck TEMPEST camera (model, VID:PID)
     - IR metadata sensor (model, interface)
     - Iris scanner (if available)
   - Check v4l2-ctl capabilities for each

3. **Development Environment Setup**
   - Install DSLLVM toolchain
   - Set up v4l2loopback for testing
   - Prepare test datasets (sample frames, metadata)

4. **Begin Sprint 1**
   - Create directory structure
   - Implement libdsv4l2rt stub
   - Modify setup.py
   - First test build

### Questions to Address

1. **DSMIL Integration Point**
   - How should dsv4l2_policy_check() call into DSMIL?
   - Redis, gRPC, local function?

2. **Quantum Candidate Handling**
   - What triggers offload of fused_capture to L7/L8?
   - Feature extraction format?

3. **Hardware Specifics**
   - Exact VID:PID of TEMPEST camera
   - IR sensor metadata format (FLIR, proprietary?)
   - Required KLV keys for military streams

4. **Security Requirements**
   - TPM signing needed for all modes or just forensic?
   - Encryption requirements for biometric storage?
   - Network transport security (mTLS, IPSEC?)

## Success Criteria (From Plan)

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

## File Inventory

```
v4l2ctl/
├── IMPLEMENTATION_PLAN.md    ✅ 43KB – 10 phases, sprint timeline
├── TECHNICAL_SPEC.md          ✅ 36KB – Complete API specs
├── PROJECT_SUMMARY.md         ✅ This file
├── README.md                  ✅ DSLLVM bundle overview
├── config/
│   └── dsllvm_dsv4l2_passes.yaml  ✅ DSLLVM pass config
├── include/
│   ├── dsv4l2_annotations.h   ✅ DSLLVM macros
│   └── dsv4l2_policy.h        ✅ Public API surface
├── v4l2ctl.pyx                ✅ Existing Cython bindings
├── v4l2.pxd                   ✅ Existing v4l2 definitions
├── setup.py                   ✅ Existing build script
└── README.rst                 ✅ Original v4l2ctl README
```

### To Be Created (Sprint 1)

```
dsv4l2/
├── include/
│   ├── dsv4l2_core.h
│   ├── dsv4l2_tempest.h
│   ├── dsv4l2_meta.h
│   ├── dsv4l2_fusion.h
│   └── dsv4l2rt.h
├── src/
│   ├── dsv4l2_core.c
│   ├── dsv4l2_tempest.c
│   ├── dsv4l2_meta.c
│   ├── dsv4l2_fusion.c
│   ├── dsv4l2_profiles.c
│   └── dsv4l2rt.c
└── profiles/
    ├── iris_scanner.yaml
    ├── tempest_camera.yaml
    ├── ir_metadata.yaml
    └── mil_stream.yaml
```

## Key Design Decisions

1. **Layered Architecture**: DSV4L2 as C layer, v4l2ctl as Python bindings
   - Rationale: DSLLVM requires C compilation; Python for ease of use

2. **YAML Profiles**: Device configuration in human-editable files
   - Rationale: Flexibility, no recompilation for new devices

3. **Pluggable Decoders**: Metadata decoders as Python plugins
   - Rationale: Vendor formats vary; extensibility required

4. **DSLLVM as Optional**: Can build without DSLLVM for development
   - Rationale: Not all users have DSLLVM; graceful degradation

5. **Backward Compatibility**: Existing v4l2ctl API unchanged
   - Rationale: Don't break existing users

## Resources

### Documentation
- **IMPLEMENTATION_PLAN.md**: Detailed phase descriptions
- **TECHNICAL_SPEC.md**: API reference, data structures
- **config/dsllvm_dsv4l2_passes.yaml**: DSLLVM pass settings

### External References
- V4L2 API: https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/
- STANAG 4609: https://gwg.nga.mil/misb/
- FLIR Lepton: https://lepton.flir.com/
- v4l2loopback: https://github.com/umlaeute/v4l2loopback

## Contact & Support

For questions or clarifications during implementation:
1. Review IMPLEMENTATION_PLAN.md for phase details
2. Check TECHNICAL_SPEC.md for API definitions
3. Consult ChatGPT conversation for original requirements

---

**Project Status**: PLANNING COMPLETE – READY FOR IMPLEMENTATION
**Next Milestone**: Sprint 1 (Foundation & DSLLVM Integration)
**Date**: 2025-11-25
