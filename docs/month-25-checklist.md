# Month 25 — Hardware Perf Gates + RT Dispatch Test Fix

Status: **complete**. Delivers v0.4 Track 3 (hardware perf gates) and fixes the
`test_VulkanRTDispatch.cpp` bug introduced in Month 24.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.4-planning.md](v0.4-planning.md),
[month-24-checklist.md](month-24-checklist.md).

---

## Track A — test_VulkanRTDispatch fix

### Problem

`test_VulkanRTDispatch.cpp::TraceRaysDispatchSetsRTReflectionsActiveOnTier1` called
`IDevice::createRTPipeline(RTPipelineDesc)` — a method that does not exist in the current
API surface. This would have caused a compile error when building with `NEXUS_BACKEND_VULKAN`.

### Fix

Replaced the problematic test with `RTGateFourConditionsOnVulkan`:
- Requires Tier-1 RT hardware (skips otherwise).
- Enables `enableRTReflect + HybridRT` mode but does NOT register an RT pipeline.
- Asserts:
  - `activeRenderMode == HybridRT` (mode survives downgrade on real hardware).
  - `rtReflectionsActive == false` (gate condition 4 — no valid pipeline — blocks dispatch).
- This verifies that the RT gate logic runs correctly on live Vulkan without needing
  `createRTPipeline`. When that API lands, a full dispatch test will replace this.

Test count: 8 (unchanged).

---

## Track B — perf_smoke Hardware Perf Gates

### New CLI flags

| Flag | Type | Effect |
|---|---|---|
| `--rt` | boolean | Enables `RendererSettings::enableRTReflect + HybridRT` mode |
| `--neural-backend <name>` | string | Attaches a `NeuralRendererFactory::create()` renderer with upscaling + denoising enabled |
| `--gpu-ceiling-ms <value>` | double | Asserts `gpuTimeMs < value`; skipped when `gpuTimeMs == 0` (no real GPU); exit 3 on violation |
| `--neural-overhead-ceiling-ms <value>` | double | Triggers two-phase run; asserts delta < value; exit 3 on violation |

### Skip behavior

When `--backend vulkan` is requested but Vulkan is unavailable (no ICD, or compiled without
`NEXUS_BACKEND_VULKAN`), the binary prints `SKIPPED: <reason>` and exits with code 0.
This ensures the new ctest entries do not fail headless CI.

### New ctest entries

```
KernelPerfSmoke.VulkanRT
  Command: nexus_kernel_perf_smoke --backend vulkan --frames 120 --rt --gpu-ceiling-ms 33
  Labels:  perf;vulkan;rt
  Timeout: 120s

KernelPerfSmoke.DLSSSteadyState
  Command: nexus_kernel_perf_smoke --backend vulkan --frames 64 --neural-backend dlss --neural-overhead-ceiling-ms 4
  Labels:  perf;vulkan;neural
  Timeout: 120s
```

Ceiling semantics:
- `gpu-ceiling-ms 33` — 30 fps GPU budget floor; only enforced when `gpuTimeMs > 0`.
- `neural-overhead-ceiling-ms 4` — DLSS steady-state overhead ceiling; measured as
  `avg_frame_ms(with_dlss) - avg_frame_ms(without)`.

### Report fields added

- `rt_reflections_active` — whether `FrameStats::rtReflectionsActive` was true last frame.
- `upscaling_active` — whether `FrameStats::upscalingActive` was true last frame.
- `denoising_active` — whether `FrameStats::denoisingActive` was true last frame.
- `neural_overhead_ms` — measured overhead (only present when `--neural-overhead-ceiling-ms` is set).

### CMake changes

- `nexus_neural` added to `nexus_kernel_perf_smoke` link libraries (required for
  `NeuralRendererFactory` and `NeuralBackend`).

### Exit criteria

- `KernelPerfSmoke.VulkanRT` and `KernelPerfSmoke.DLSSSteadyState` appear in `ctest -N`.
- Both tests pass (exit 0) in headless Null-only CI (Vulkan skipped).
- On Vulkan hardware: GPU ceiling and overhead ceiling gates enforce their budgets.
- `test_VulkanRTDispatch.cpp` compiles cleanly with `NEXUS_BACKEND_VULKAN=ON`.
