# dsv4l2 DSLLVM Integration Bundle

This bundle contains a minimal set of files to integrate DSLLVM-based
hardening into a v4l2-style camera/IR/metadata library (`dsv4l2`).

## Contents

- `config/dsllvm_dsv4l2_passes.yaml`
  DSLLVM pass configuration:
    - `dsmil.secret_flow`   – secret-flow tracking & allowed sinks.
    - `dsmil.tempest_policy` – TEMPEST state & policy enforcement.
    - `dsmil.constant_time` – constant-time checks on sensitive paths.

- `include/dsv4l2_annotations.h`
  DSLLVM attribute wrappers and base types:
    - `dsv4l2_frame_t`  – frame buffer, tagged as `dsmil_secret`.
    - `dsv4l2_meta_t`   – metadata buffer, tagged as `dsmil_meta`.
    - `dsv4l2_tempest_state_t` – TEMPEST state enum, tagged as `dsmil_tempest`.

- `include/dsv4l2_policy.h`
  Public API surface for TEMPEST and policy-aware capture:
    - `dsv4l2_get_tempest_state()`
    - `dsv4l2_set_tempest_state()`
    - `dsv4l2_policy_check()`
    - `dsv4l2_capture_frame()` (requires TEMPEST check)
    - `dsv4l2_capture_iris()`  (secret region + TEMPEST)
    - `dsv4l2_fused_capture()` (quantum-candidate fusion path)

## Example DSLLVM compile invocation

Assuming `libDSLLVM.so` is your plugin and this bundle is at `dsv4l2_dsllvm/`:

```sh
dsclang -O2 \
  -fplugin=libDSLLVM.so \
  -fplugin-arg-dsllvm-pass-config=dsv4l2_dsllvm/config/dsllvm_dsv4l2_passes.yaml \
  -I dsv4l2_dsllvm/include \
  -c dsv4l2_core.c -o dsv4l2_core.o
```

Integrate the headers into your library, adjust names/types as needed, and
expand the YAML config with any additional rules your DSMIL/DSLLVM setup
requires.
