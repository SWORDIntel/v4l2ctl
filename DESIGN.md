# DSV4L2: DSLLVM-Enhanced Sensor Stack

**DSMIL-aware video/sensor front-end with TEMPEST enforcement, secret tracking, and quantum optimization hooks**

---

## 0. Purpose

Turn `v4l2ctl` into a **DSLLVM-instrumented sensor front-end** that:

* Understands roles: `camera`, `iris_scanner`, `ir_sensor`, `tempest_cam`, `mil_stream`
* Exposes **TEMPEST state** as a security primitive, not a random control
* Handles **IR / metadata-only / KLV / mil-grade streams** cleanly
* Uses **DSLLVM annotations + passes** to enforce:
  * Secret handling (`dsmil_secret`)
  * TEMPEST policy (`dsmil_tempest`)
  * Side-channel controls (constant-time, no secret-dependent branches)
  * AI/quantum optimization hooks (`dsmil_quantum_candidate`)

---

## 1. Architecture Overview

### 1.1 Components

* **`libdsv4l2_core`**
  DSLLVM-compiled C library wrapping v4l2:
  * Device discovery, formats, buffers, controls
  * Annotated with DSLLVM attributes for security and performance

* **`libdsv4l2_profiles`**
  Profile loader (YAML/JSON) defining **role-aware device configs**

* **`libdsv4l2_policy`**
  DSMIL policy bridge:
  * Maps DSMIL layer / THREATCON → TEMPEST state, resolution limits, metadata redaction rules

* **`libdsv4l2_meta`**
  Metadata plane & IR radiometric decoding:
  * `V4L2_BUF_TYPE_META_CAPTURE`
  * KLV/radiometric plugin interface

* **`libdsv4l2rt`**
  Runtime instrumentation library (mirrors SHRINK pattern):
  * Event buffering and telemetry emission
  * Integration with DSMIL fabric (Redis/SQLite/SHRINK/MEMSHADOW)
  * TPM-signed event chunks for forensic integrity

* **`dsv4l2_cli`**
  Thin CLI on top of the DSLLVM-built libraries

---

## 2. DSLLVM Annotations & Types

### 2.1 Core Types (C)

Header: `include/dsv4l2_annotations.h`

```c
// Frame / buffer classes
typedef struct {
    uint8_t *data;
    size_t   len;
} dsv4l2_frame_t __attribute__((dsmil_secret("biometric_frame")));

typedef struct {
    uint8_t *data;
    size_t   len;
} dsv4l2_meta_t __attribute__((dsmil_meta("radiometric")));

typedef enum {
    DSV4L2_TEMPEST_DISABLED = 0,
    DSV4L2_TEMPEST_LOW      = 1,
    DSV4L2_TEMPEST_HIGH     = 2,
    DSV4L2_TEMPEST_LOCKDOWN = 3,
} dsv4l2_tempest_state_t __attribute__((dsmil_tempest));

typedef struct {
    int         fd;
    const char *dev_path;
    const char *role;
    uint32_t    layer;
} dsv4l2_device_t;
```

DSLLVM uses these attributes to:

* Track **biometric/secret data** across IR
* Forbid logging, unsafe copies, or network egress of `dsmil_secret` objects without explicit declassify calls
* Enforce "TEMPEST state must be consulted before capture" rules

### 2.2 Security & Side-Channel Passes

Custom DSLLVM passes (configured in `config/dsllvm_dsv4l2_passes.yaml`):

1. **Secret Flow Checker (L5 advisor input)**

   * Ensures `dsv4l2_frame_t` / iris frames only flow to:
     * Trusted biometric pipeline
     * Encrypted storage
     * Approved in-memory DSLLVM transforms
   * Flags:
     * Printf/logging of secret buffers
     * Unannotated memcpy to untrusted regions
     * Network sends without `dsmil_declassify()`

2. **TEMPEST Policy Pass**

   * Every public `capture()` call must:
     * Read current `dsv4l2_tempest_state_t`
     * Call a `dsv4l2_policy_check()` before delivering output
   * If not, build fails

3. **Constant-Time Guard Pass**

   * Regions annotated `__attribute__((dsmil_secret_region))` must not:
     * Branch on secret values
     * Use secret as array index without masking
   * Applied to iris processing and any metadata deemed sensitive

4. **Quantum Candidate Tagging**

   * Mark hot paths (IR radiometric decoding, metadata fusion scoring) with:
     `__attribute__((dsmil_quantum_candidate("sensor_fusion")))`
   * These become candidates for L7/L8 redistribution to quantum/accelerator nodes later

---

## 3. Role-Aware Profiles (DSLLVM-Aware)

### 3.1 Profile Format

Profiles live in `profiles/` and define:

```yaml
id: "046d:0825"
role: "iris_scanner"
device_hint: "/dev/video0"

pixel_format: "YUYV"
resolution: [1280, 720]
fps: 60

# Security classification
classification: "SECRET_BIOMETRIC"

controls:
  exposure_auto: 1
  exposure_absolute: 50
  focus_auto: 0
  focus_absolute: 10

tempest_control:
  id: 0x9a0902
  mode_map:
    DISABLED: 0
    LOW: 1
    HIGH: 2
    LOCKDOWN: 3

meta_device: "/dev/video2"
meta_format: "META"
```

### 3.2 DSLLVM Usage

* At load time, classification drives DSLLVM **policy tables**:
  * `SECRET_BIOMETRIC` → enforce `dsmil_secret` rules
  * `TEMPEST_ONLY` → block capture unless DSMIL says otherwise
* Profiles can specify **which fields** in decoded metadata should become `dsmil_secret` (e.g. GPS, face/iris bounding boxes)

---

## 4. TEMPEST State Machine (DSLLVM-Enforced)

### 4.1 API

Header: `include/dsv4l2_policy.h`

```c
dsv4l2_tempest_state_t
dsv4l2_get_tempest_state(dsv4l2_device_t *dev)
  __attribute__((dsmil_tempest_query));

int
dsv4l2_set_tempest_state(dsv4l2_device_t *dev,
                         dsv4l2_tempest_state_t state)
  __attribute__((dsmil_tempest_transition));

int
dsv4l2_capture_frame(dsv4l2_device_t *dev,
                     dsv4l2_frame_t *out)
  __attribute__((dsmil_requires_tempest_check));
```

### 4.2 DSLLVM Rules

* Any function marked `dsmil_requires_tempest_check` must:
  * Call `dsv4l2_get_tempest_state()` and `dsv4l2_policy_check()` in the same IR region
  * DSLLVM rejects builds where a frame is returned while state is `LOCKDOWN`

**Your "stuck TEMPEST camera"** becomes a device whose profile and policy are explicit; DSLLVM guarantees every consumer honors that state.

---

## 5. Metadata, IR Sensors & Military Streams

### 5.1 Metadata Plane

C API:

```c
int dsv4l2_meta_open(const char *path, dsv4l2_meta_handle_t **out);
int dsv4l2_meta_read(dsv4l2_meta_handle_t *h, dsv4l2_meta_t *out)
  __attribute__((dsmil_secret("meta_if_contains_coords")));
```

* DSLLVM can dynamically classify metadata as secret if decoders tag fields like coordinates, unit IDs, etc.

### 5.2 Fusion Sensor

```c
int dsv4l2_fused_capture(dsv4l2_device_t *video_dev,
                         dsv4l2_meta_handle_t *meta_dev,
                         dsv4l2_frame_t *out_frame,
                         dsv4l2_meta_t *out_meta)
  __attribute__((dsmil_quantum_candidate("fused_capture")));
```

* This function is tagged as a **quantum candidate** for later offload (e.g. fusion scoring, anomaly detection) while DSLLVM logs features for ONNX export

### 5.3 Mil / STANAG-Style Streams

* Decoders live in plugins but **interface types** (frames + metadata) are DSLLVM-annotated
* DSLLVM auditors can verify:
  * KLV fields with sensitive info obey secret/TEMPEST rules
  * Certain fields (e.g. targeting data) **never** exit certain DSMIL layers

---

## 6. Iris Scanner Mode

### 6.1 DSLLVM-Safe Capture

```c
int dsv4l2_enter_iris_mode(dsv4l2_device_t *dev);

int dsv4l2_capture_iris(dsv4l2_device_t *dev,
                        dsv4l2_frame_t *out)
  __attribute__((dsmil_secret_region,
                 dsmil_requires_tempest_check));
```

* `dsmil_secret_region`: DSLLVM enforces constant-time + safe sinks for iris data
* Iris buffers are **never** allowed to:
  * Be logged
  * Leave via plain sockets
  * Be stored unencrypted

---

## 7. DSLLVM Instrumentation & Runtime

Following the same pattern as SHRINK, but for cameras.

### 7.1 Compiler Flags

Add DSLLVM flags mirroring SHRINK's style:

* **`-fdsv4l2-profile=off|ops|exercise|forensic`**

  * `off`      – no runtime probes, only static metadata
  * `ops`      – minimal counters (frames, errors, TEMPEST transitions)
  * `exercise` – per-stream stats, sampling of metadata
  * `forensic` – maximal logging (within policy): per-event detail, full audit

* **`-fdsv4l2-metadata`**
  Emit only **static** metadata (no runtime), for offline analysis

* **`-mdsv4l2-mission=<name>`**
  Tag outputs with mission context (goes into metadata sections and runtime events)

DSLLVM understands these and enables the right passes + runtime hooks.

### 7.2 Source-Level Instrumentation Annotations

```c
// Device / Role / Layer
__attribute__((dsv4l2_sensor("iris_scanner","L3","SECRET_BIOMETRIC")))
int dsv4l2_open_iris(dsv4l2_device_t *dev);

// TEMPEST control annotation
__attribute__((dsv4l2_tempest_control))
int dsv4l2_set_tempest_state(dsv4l2_device_t *dev, int state);

// Metadata stream
__attribute__((dsv4l2_meta_stream("ir_radiometric")))
int dsv4l2_meta_read(dsv4l2_device_t *meta_dev, void *buf, size_t len);

// Event hooks
void dsv4l2_on_capture_start(dsv4l2_device_t *dev)
  __attribute__((dsv4l2_event("capture_start","medium")));

void dsv4l2_on_tempest_transition(dsv4l2_device_t *dev, int old, int new_state)
  __attribute__((dsv4l2_event("tempest_transition","critical")));
```

### 7.3 DSLLVM Instrumentation Passes

1. **Sensor Role Pass**
   * Reads `dsv4l2_sensor(...)` attrs and emits static metadata sections
   * Links sensors to DSMIL device IDs (Layer/Device mapping)

2. **TEMPEST Policy Pass**
   * Any function annotated as a capture (`dsv4l2_capture_*`) must:
     * Read TEMPEST state (`dsv4l2_get_tempest_state()`)
     * Call a policy hook (e.g. `dsv4l2_policy_check()`)
     * Be rejected at build time if state checks are missing

3. **Secret Flow Pass**
   * Tracks `dsmil_secret("biometric_frame")` across IR
   * Forbids printing/logging, network sends without declassify, copies into unannotated structs

4. **Instrumentation Injection Pass**
   * Based on `-fdsv4l2-profile`:
     * Injects calls to `libdsv4l2rt` at:
       * Capture start/stop
       * TEMPEST state changes
       * Format/resolution changes
       * Errors / dropped frames

### 7.4 Runtime (`libdsv4l2rt`)

A tiny C runtime, analogous to `libshrinkrt`:

Header: `include/dsv4l2rt.h`

* **Per-process ring buffers** of events:

  ```c
  typedef struct {
      uint64_t ts_ns;
      uint32_t dev_id;
      uint16_t event_type;   // capture_start, tempest_transition, etc.
      uint16_t severity;
      uint32_t aux;          // e.g. new TEMPEST state, error code
  } dsv4l2_event_t;
  ```

* **APIs DSLLVM-injected code uses:**

  ```c
  void dsv4l2rt_emit(const dsv4l2_event_t *ev);
  void dsv4l2rt_flush(void);   // push into DSMIL telemetry (Redis/SQLite/etc)
  ```

* **Integration points:**
  * For ops: counters, heatmaps, anomaly detection on sensor behaviour
  * For SHRINK: correlate operator actions with sensor transitions
  * For MEMSHADOW: long-term memory of hardware patterns

* **Optional**: TPM-signed event chunks for forensic integrity (same as SHRINK)

See `examples/instrumented_capture_example.c` for before/after code examples.

---

## 8. DSLLVM + AI / Quantum Hooks

### 8.1 Feature Extraction for L8/L5

DSLLVM extracts:

* Frame stats: entropy, brightness maps, noise
* Metadata features: movement vectors, temperature histograms, KLV tags
* Policy events: TEMPEST transitions, resolution cuts, redactions

Exported as ONNX-friendly feature vectors for:

* L8 "advisor" scoring: misconfig detection, anomaly alerts
* L7 quantum candidate routines (e.g. for heavy sensor fusion workloads)

---

## 9. CLI (backed by DSLLVM-built libs)

Examples:

```bash
# Discover & classify devices
dsv4l2 scan

# Show TEMPEST state
dsv4l2 list --role camera --show-tempest

# Attempt to unlock TEMPEST under DSMIL policy
dsv4l2 tempest --device /dev/video0 --state DISABLED

# Capture fused frame+meta (with DSLLVM policies enforced)
dsv4l2 capture-fused --video /dev/video0 --meta /dev/video2 \
    --out frame.raw --meta-out frame.meta.json
```

All CLI calls are thin wrappers; DSLLVM guarantees the underlying path respects annotations and policies.

---

## 10. What You Get

**Same pattern as SHRINK, but for cameras:**

* One DSLLVM knob (`-fdsv4l2-profile`) controls how "loud" the instrumentation is
* Source attributes give you **role, layer, classification, and secret semantics**
* DSLLVM passes enforce policy + inject runtime calls
* `libdsv4l2rt` turns v4l2ctl from "just a C library" into a **sensor telemetry node** in the DSMIL fabric, fully TEMPEST-aware

**Every pixel, every metadata field, every TEMPEST state is typed, annotated, and enforced at IR level.**

---

## 11. Build Instructions

### 11.1 Standard Build (Plain GCC/Clang)

For development without DSLLVM instrumentation:

```bash
# Install dependencies
apt-get install libv4l-dev   # Debian/Ubuntu
dnf install libv4l-devel     # Fedora
pacman -S v4l-utils          # Arch

# Install Cython
pip3 install cython

# Build
python3 setup.py build
python3 setup.py install
```

### 11.2 DSLLVM Build (Instrumented)

Assuming `libDSLLVM.so` is your plugin and this bundle is at `dsv4l2_dsllvm/`:

```bash
dsclang -O2 \
  -fplugin=libDSLLVM.so \
  -fplugin-arg-dsllvm-pass-config=config/dsllvm_dsv4l2_passes.yaml \
  -fdsv4l2-profile=ops \
  -mdsv4l2-mission=operation_name \
  -I include \
  -c dsv4l2_core.c -o dsv4l2_core.o
```

Link with `libdsv4l2rt` for runtime telemetry.

---

## 12. Implementation Roadmap

### Phase 1: Core Infrastructure ✅
- [x] Extract DSLLVM bundle (annotations, policy headers, pass config)
- [x] Create runtime header (`dsv4l2rt.h`)
- [x] Extend annotations with instrumentation attributes
- [x] Create instrumented capture examples

### Phase 2: Core Library (TODO)
- [ ] Implement `libdsv4l2_core` - v4l2 wrapper with DSLLVM annotations
- [ ] Implement functions declared in `dsv4l2_policy.h`
- [ ] Device discovery and enumeration
- [ ] Buffer management with secret tracking

### Phase 3: Profile System (TODO)
- [ ] `libdsv4l2_profiles` - YAML profile loader
- [ ] Device matching and classification engine
- [ ] Role-aware configuration loading
- [ ] Example profiles for common devices

### Phase 4: Policy Layer (TODO)
- [ ] `libdsv4l2_policy` - DSMIL policy bridge
- [ ] THREATCON → TEMPEST state mapping
- [ ] Policy check implementations
- [ ] Authorization framework

### Phase 5: Metadata & Fusion (TODO)
- [ ] `libdsv4l2_meta` - metadata capture interface
- [ ] KLV decoder plugin system
- [ ] IR radiometric decoding
- [ ] Fused capture implementation

### Phase 6: Runtime Implementation (TODO)
- [ ] `libdsv4l2rt` - event buffering and emission
- [ ] Redis/SQLite sink implementations
- [ ] TPM-signed event chunks
- [ ] SHRINK/MEMSHADOW integration

### Phase 7: CLI (TODO)
- [ ] `dsv4l2_cli` - command-line tool
- [ ] scan, list, tempest, capture commands
- [ ] Profile management commands

### Phase 8: Testing & Validation (TODO)
- [ ] Unit tests for core library
- [ ] Integration tests with real v4l2 devices
- [ ] DSLLVM pass validation
- [ ] Security policy enforcement tests

---

## 13. Files in This Repository

```
.
├── DESIGN.md                          # This file
├── README.md                          # DSLLVM integration guide
├── README.rst                         # Original v4l2ctl readme
├── LICENSE
├── setup.py                           # Python build configuration
├── v4l2ctl.pyx                        # Cython interface (original)
├── v4l2ctl.c                          # Generated C code (original)
├── v4l2.pxd                           # v4l2 definitions (original)
│
├── config/
│   └── dsllvm_dsv4l2_passes.yaml     # DSLLVM pass configuration
│
├── include/
│   ├── dsv4l2_annotations.h          # DSLLVM attribute macros & types
│   ├── dsv4l2_policy.h               # Policy API declarations
│   └── dsv4l2rt.h                    # Runtime instrumentation API
│
├── examples/
│   └── instrumented_capture_example.c # Before/after instrumentation examples
│
└── profiles/                          # Device profiles (TODO)
    └── (YAML profiles to be added)
```

---

## 14. References

* DSLLVM Documentation (internal)
* SHRINK Instrumentation Pattern (internal)
* MEMSHADOW Integration (internal)
* DSMIL Layer Architecture (internal)
* v4l2 API: https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/
* Original pyv4l2: https://github.com/pupil-labs/pyv4l2
