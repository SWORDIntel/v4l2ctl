# DSV4L2 Implementation Breakdown

**Task decomposition for building the complete DSLLVM-instrumented sensor stack**

---

## Phase 1: Foundation ✅ COMPLETE

### What We Have
- [x] DSLLVM bundle extracted (headers, config, docs)
- [x] `dsv4l2_annotations.h` - All attribute macros and base types
- [x] `dsv4l2_policy.h` - API declarations for TEMPEST and capture
- [x] `dsv4l2rt.h` - Runtime telemetry API
- [x] `config/dsllvm_dsv4l2_passes.yaml` - Complete pass configuration
- [x] `DESIGN.md` - Full architecture specification
- [x] `examples/instrumented_capture_example.c` - Code patterns

---

## Phase 2: Core v4l2 Wrapper (PRIORITY 1)

### Goal
Create `libdsv4l2_core` - the DSLLVM-annotated v4l2 wrapper library

### Files to Create

#### 2.1 Device Management (`src/device.c`)
```c
Functions to implement:
- dsv4l2_open(const char *path, const char *role, dsv4l2_device_t **out)
- dsv4l2_close(dsv4l2_device_t *dev)
- dsv4l2_list_devices(dsv4l2_device_t ***devices, size_t *count)
- dsv4l2_get_capabilities(dsv4l2_device_t *dev, struct v4l2_capability *cap)
```

**Key Improvements:**
- Add DSMIL_DEVICE annotation to device handles
- Emit DSV4L2_EVENT_DEVICE_OPEN/CLOSE via runtime
- Load device profile from profiles/ directory
- Set TEMPEST control ID from profile

**Complexity**: Medium
**Dependencies**: None
**Estimated lines**: ~300

---

#### 2.2 Format & Resolution (`src/format.c`)
```c
Functions to implement:
- dsv4l2_enum_formats(dsv4l2_device_t *dev, uint32_t **formats, size_t *count)
- dsv4l2_get_format(dsv4l2_device_t *dev, struct v4l2_format *fmt)
- dsv4l2_set_format(dsv4l2_device_t *dev, struct v4l2_format *fmt)
- dsv4l2_enum_frame_sizes(dsv4l2_device_t *dev, uint32_t pixel_fmt, ...)
- dsv4l2_enum_frame_rates(dsv4l2_device_t *dev, uint32_t pixel_fmt, ...)
```

**Key Improvements:**
- Emit DSV4L2_EVENT_FORMAT_CHANGE when format changes
- Emit DSV4L2_EVENT_RESOLUTION_CHANGE when resolution changes
- Policy check: enforce max resolution based on DSMIL layer
- DSLLVM metadata: track supported formats in static metadata

**Complexity**: Low-Medium
**Dependencies**: device.c
**Estimated lines**: ~400

---

#### 2.3 Buffer Management (`src/buffer.c`)
```c
Functions to implement:
- dsv4l2_request_buffers(dsv4l2_device_t *dev, uint32_t count)
- dsv4l2_query_buffer(dsv4l2_device_t *dev, uint32_t index, ...)
- dsv4l2_queue_buffer(dsv4l2_device_t *dev, uint32_t index)
- dsv4l2_dequeue_buffer(dsv4l2_device_t *dev, struct v4l2_buffer *buf)
- dsv4l2_mmap_buffers(dsv4l2_device_t *dev)
```

**Key Improvements:**
- Tag mmap'd regions with DSMIL_SECRET_REGION for biometric devices
- Prevent buffer copies outside of allowed sinks
- Track buffer usage stats for telemetry

**Complexity**: Medium-High
**Dependencies**: device.c
**Estimated lines**: ~500

---

#### 2.4 Capture Implementation (`src/capture.c`)
```c
Functions to implement (matching dsv4l2_policy.h):
- dsv4l2_capture_frame(dsv4l2_device_t *dev, dsv4l2_frame_t *out)
- dsv4l2_capture_iris(dsv4l2_device_t *dev, dsv4l2_frame_t *out)
- dsv4l2_start_streaming(dsv4l2_device_t *dev)
- dsv4l2_stop_streaming(dsv4l2_device_t *dev)
```

**Key Improvements:**
- **CRITICAL**: Must call dsv4l2_get_tempest_state() before capture
- **CRITICAL**: Must call dsv4l2_policy_check() before returning frame
- Emit DSV4L2_EVENT_CAPTURE_START/STOP
- Emit DSV4L2_EVENT_FRAME_ACQUIRED for each frame
- Emit DSV4L2_EVENT_FRAME_DROPPED on errors
- Annotate iris capture with DSMIL_SECRET_REGION
- Use instrumented example from examples/instrumented_capture_example.c

**Complexity**: Medium
**Dependencies**: device.c, buffer.c, policy.c
**Estimated lines**: ~600

---

#### 2.5 TEMPEST State Management (`src/tempest.c`)
```c
Functions to implement (matching dsv4l2_policy.h):
- dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
- dsv4l2_set_tempest_state(dsv4l2_device_t *dev, dsv4l2_tempest_state_t state)
- dsv4l2_policy_check(dsv4l2_tempest_state_t state, const char *context)
```

**Key Improvements:**
- Read TEMPEST control ID from device profile
- Emit DSV4L2_EVENT_TEMPEST_TRANSITION with old→new state
- Emit DSV4L2_EVENT_TEMPEST_LOCKDOWN when entering LOCKDOWN
- Annotate with DSMIL_TEMPEST_QUERY/TRANSITION
- Policy check: reject capture in LOCKDOWN unless authorized

**Complexity**: Low-Medium
**Dependencies**: device.c
**Estimated lines**: ~250

---

### Phase 2 Summary

**Total files**: 5 source files + 1 header
**Total estimated lines**: ~2,050
**Build system**: Need CMakeLists.txt or Makefile

**Critical path**:
1. device.c (foundation)
2. tempest.c (security primitive)
3. buffer.c (memory management)
4. capture.c (core functionality)
5. format.c (configuration)

---

## Phase 3: Profile System (PRIORITY 2)

### Goal
Create `libdsv4l2_profiles` - YAML profile loader for role-aware device configs

### Files to Create

#### 3.1 Profile Loader (`src/profile_loader.c`)
```c
Functions:
- dsv4l2_load_profile(const char *device_id, dsv4l2_profile_t **out)
- dsv4l2_match_device(const char *path, dsv4l2_profile_t **out)
- dsv4l2_profile_free(dsv4l2_profile_t *profile)
```

**Key Improvements:**
- Parse YAML files from profiles/ directory
- Match devices by USB VID:PID or device path
- Load role, classification, TEMPEST mapping
- Cache profiles for performance

**Complexity**: Medium
**Dependencies**: libyaml or similar
**Estimated lines**: ~400

---

#### 3.2 Example Profiles (`profiles/*.yaml`)
```yaml
Create profiles for:
- Generic webcam (UNCLASSIFIED, camera role)
- Iris scanner (SECRET_BIOMETRIC, iris_scanner role)
- IR camera (SECRET, ir_sensor role)
- TEMPEST-aware camera (tempest_cam role)
```

**Complexity**: Low
**Estimated files**: 4-6 profiles
**Lines per profile**: ~30-50

---

### Phase 3 Summary

**Total files**: 1 source + 4-6 YAML profiles
**Estimated lines**: ~600 total
**Dependencies**: libyaml

---

## Phase 4: Policy Layer (PRIORITY 3)

### Goal
Create `libdsv4l2_policy` - DSMIL policy bridge

### Files to Create

#### 4.1 DSMIL Layer Mapping (`src/dsmil_bridge.c`)
```c
Functions:
- dsv4l2_get_layer_policy(uint32_t layer, dsv4l2_layer_policy_t **out)
- dsv4l2_check_capture_allowed(dsv4l2_device_t *dev, const char *context)
- dsv4l2_apply_threatcon(uint32_t threatcon)
```

**Key Improvements:**
- Map DSMIL THREATCON → TEMPEST states
- Enforce max resolution by layer (L3=1080p, L5=4K, etc.)
- Apply metadata redaction rules by classification
- Integrate with DSMIL policy service (Redis/API)

**Complexity**: Medium-High
**Dependencies**: Core library, DSMIL fabric connection
**Estimated lines**: ~500

---

#### 4.2 Authorization (`src/authorization.c`)
```c
Functions:
- dsv4l2_authorize_unlock(dsv4l2_device_t *dev, const char *credential)
- dsv4l2_check_clearance(const char *role, const char *classification)
```

**Key Improvements:**
- TPM-based authorization for TEMPEST unlock
- Clearance level checking for biometric access
- Audit trail for authorization attempts

**Complexity**: High
**Dependencies**: TPM2 library, DSMIL clearance service
**Estimated lines**: ~600

---

### Phase 4 Summary

**Total files**: 2 source files
**Estimated lines**: ~1,100
**Dependencies**: DSMIL fabric, TPM2

---

## Phase 5: Metadata & Fusion (PRIORITY 4)

### Goal
Create `libdsv4l2_meta` - metadata plane & IR radiometric decoding

### Files to Create

#### 5.1 Metadata Capture (`src/meta_capture.c`)
```c
Functions:
- dsv4l2_meta_open(const char *path, dsv4l2_meta_handle_t **out)
- dsv4l2_meta_read(dsv4l2_meta_handle_t *h, dsv4l2_meta_t *out)
- dsv4l2_meta_close(dsv4l2_meta_handle_t *h)
```

**Key Improvements:**
- Handle V4L2_BUF_TYPE_META_CAPTURE
- Tag metadata with DSMIL_META("radiometric")
- Emit DSV4L2_EVENT_META_READ

**Complexity**: Medium
**Dependencies**: Core library
**Estimated lines**: ~350

---

#### 5.2 Fused Capture (`src/fused_capture.c`)
```c
Functions (matching dsv4l2_policy.h):
- dsv4l2_fused_capture(video_dev, meta_dev, out_frame, out_meta)
```

**Key Improvements:**
- Synchronize video + metadata streams
- Check TEMPEST state matches on both devices
- Tag with DSMIL_QUANTUM_CANDIDATE("fused_capture")
- Emit DSV4L2_EVENT_FUSED_CAPTURE

**Complexity**: High
**Dependencies**: Core, metadata
**Estimated lines**: ~450

---

#### 5.3 KLV Decoder Plugin (`src/klv_decoder.c`)
```c
Plugin interface for military metadata:
- dsv4l2_register_klv_decoder(const char *format, decoder_fn fn)
- dsv4l2_decode_klv(const uint8_t *data, size_t len, ...)
```

**Key Improvements:**
- Plugin system for STANAG/MISB decoders
- Tag sensitive KLV fields as DSMIL_SECRET
- Extract features for L8 advisor

**Complexity**: High
**Dependencies**: Metadata capture
**Estimated lines**: ~700

---

### Phase 5 Summary

**Total files**: 3 source files
**Estimated lines**: ~1,500
**Dependencies**: V4L2 metadata support

---

## Phase 6: Runtime Implementation (PRIORITY 5)

### Goal
Implement `libdsv4l2rt` - actual runtime library (not just header)

### Files to Create

#### 6.1 Event Buffer (`src/runtime/event_buffer.c`)
```c
Implement from dsv4l2rt.h:
- dsv4l2rt_init(config)
- dsv4l2rt_emit(event)
- dsv4l2rt_emit_simple(dev_id, type, severity, aux)
- dsv4l2rt_flush()
- dsv4l2rt_shutdown()
```

**Key Improvements:**
- Lock-free ring buffer for low overhead
- Thread-safe event emission
- Auto-flush on buffer fill or timeout
- Profile-based filtering (ops/exercise/forensic)

**Complexity**: Medium-High
**Dependencies**: None
**Estimated lines**: ~600

---

#### 6.2 Sink Implementations (`src/runtime/sinks/`)
```c
Files:
- redis_sink.c - Redis pub/sub for DSMIL fabric
- sqlite_sink.c - Local SQLite database
- file_sink.c - JSON/SARIF file output
- shrink_sink.c - SHRINK integration
```

**Key Improvements:**
- Pluggable sink architecture
- Batch write for performance
- Automatic reconnection on failure
- SARIF format for security violations

**Complexity**: Medium per sink
**Dependencies**: libhiredis, sqlite3, SHRINK API
**Estimated lines**: ~400 per sink = ~1,600

---

#### 6.3 TPM Signing (`src/runtime/tpm_sign.c`)
```c
Functions:
- dsv4l2rt_get_signed_chunk(header, events, count)
- dsv4l2rt_verify_chunk(header, signature)
```

**Key Improvements:**
- TPM2-signed event chunks for forensic integrity
- SHA-384 hashing of event batches
- Chunk headers with monotonic IDs

**Complexity**: High
**Dependencies**: tpm2-tss library
**Estimated lines**: ~500

---

### Phase 6 Summary

**Total files**: 6 source files
**Estimated lines**: ~2,700
**Dependencies**: libhiredis, sqlite3, tpm2-tss, SHRINK

---

## Phase 7: CLI Tool (PRIORITY 6)

### Goal
Create `dsv4l2_cli` - command-line interface

### Commands to Implement

```bash
dsv4l2 scan                          # Discover devices
dsv4l2 list [--role=X] [--tempest]   # List devices
dsv4l2 info /dev/video0              # Device info
dsv4l2 tempest /dev/video0 --state=LOW   # Set TEMPEST
dsv4l2 capture /dev/video0 -o frame.raw  # Capture frame
dsv4l2 capture-fused --video /dev/video0 --meta /dev/video2
dsv4l2 profile load iris_scanner.yaml    # Load profile
```

**Key Improvements:**
- Thin wrappers around libdsv4l2_core
- JSON output mode for scripting
- Verbose mode shows telemetry events
- Integration with profile system

**Complexity**: Low-Medium
**Dependencies**: All libraries above
**Estimated lines**: ~800

---

## Phase 8: Testing & Validation (PRIORITY 7)

### Test Suites

#### 8.1 Unit Tests (`tests/unit/`)
```c
Files:
- test_device.c - Device open/close/list
- test_tempest.c - TEMPEST state machine
- test_capture.c - Capture operations
- test_profile.c - Profile loading
- test_runtime.c - Event emission
```

**Estimated tests**: ~50
**Estimated lines**: ~2,000

---

#### 8.2 Integration Tests (`tests/integration/`)
```c
Files:
- test_end_to_end.c - Full capture pipeline
- test_policy_enforcement.c - DSLLVM policy violations
- test_telemetry.c - Runtime event flow
- test_fused_capture.c - Video + metadata sync
```

**Estimated tests**: ~20
**Estimated lines**: ~1,500

---

#### 8.3 DSLLVM Validation (`tests/dsllvm/`)
```c
Files:
- test_secret_flow.c - Verify secret leak prevention
- test_tempest_enforcement.c - Verify mandatory checks
- test_constant_time.c - Verify timing safety
```

**Complexity**: High (requires DSLLVM compiler)
**Estimated tests**: ~15
**Estimated lines**: ~1,000

---

### Phase 8 Summary

**Total test files**: ~12
**Estimated lines**: ~4,500
**Dependencies**: Check/GTest framework

---

## Overall Implementation Summary

| Phase | Component | Files | Lines | Complexity | Dependencies |
|-------|-----------|-------|-------|------------|--------------|
| 1 ✅ | Foundation | 7 | ~1,500 | Low | None |
| 2 | Core Library | 5 | ~2,050 | Medium | libv4l2 |
| 3 | Profiles | 6 | ~600 | Medium | libyaml |
| 4 | Policy | 2 | ~1,100 | High | DSMIL, TPM2 |
| 5 | Metadata/Fusion | 3 | ~1,500 | High | V4L2 meta |
| 6 | Runtime | 6 | ~2,700 | High | Redis, SQLite, TPM2 |
| 7 | CLI | 1 | ~800 | Medium | All above |
| 8 | Testing | 12 | ~4,500 | Medium | Test framework |
| **TOTAL** | | **42** | **~14,750** | | |

---

## Critical Path

1. **Phase 2.1-2.2**: Device + TEMPEST (foundation) - 1-2 days
2. **Phase 2.4**: Capture implementation - 1-2 days
3. **Phase 6.1**: Runtime core - 1-2 days
4. **Phase 3**: Profiles - 1 day
5. **Phase 7**: Basic CLI - 1 day
6. **Phases 4-5-6**: Advanced features - 3-5 days
7. **Phase 8**: Testing - 2-3 days

**Total estimated time**: 10-15 days for full implementation

---

## Recommended Approach

### Week 1: Core Functionality
- Days 1-2: Phase 2 (device, TEMPEST, capture)
- Day 3: Phase 6.1 (basic runtime)
- Day 4: Phase 3 (profiles)
- Day 5: Phase 7 (basic CLI) + validation

### Week 2: Advanced Features
- Days 6-7: Phase 4 (policy layer)
- Days 8-9: Phase 5 (metadata/fusion)
- Day 10: Phase 6.2-6.3 (advanced runtime)

### Week 3: Polish & Testing
- Days 11-13: Phase 8 (comprehensive testing)
- Days 14-15: Documentation, examples, integration validation

---

## Next Immediate Steps

1. Create `src/` directory structure
2. Set up build system (CMakeLists.txt)
3. Implement Phase 2.1 (device.c) first
4. Implement Phase 2.5 (tempest.c) second
5. Create minimal runtime stub for linking
6. Build first working example

**Ready to start?** Let me know which phase to begin with!
