# Month 24 — VulkanNeuralRenderer + Live RT Dispatch Tests

Status: **complete**. Delivers v0.4 Tracks 1 and 2: neural renderer backend
reporting fixes, `NeuralRendererFactory`, and skip-safe Vulkan RT dispatch tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.4-planning.md](v0.4-planning.md),
[month-23-checklist.md](month-23-checklist.md).

---

## Track 1 — Neural Renderer Backend Reporting Fixes

### Problem

`DLSSPlugin`, `XeSSPlugin`, and `OIDNDenoiser` all returned `DenoiserBackend::None` /
`UpscalerBackend::None` from their `active*()` accessors regardless of whether the
SDK initialised successfully. This meant `FrameStats::activeDenoiser` / `activeUpscaler`
were always `None` on real hardware even when DLSS or XeSS was running.

### Fixes

| File | Change |
|---|---|
| `src/kernel/src/neural/DLSSPlugin.h` | `activeDenoiser()` → DLSS4 when `m_ngxAvailable`; `activeUpscaler()` → DLSS4 when `m_ngxAvailable` |
| `src/kernel/src/neural/XeSSPlugin.h` | `activeUpscaler()` → XeSS when `m_available` |
| `src/kernel/src/neural/OIDNDenoiser.cpp` | `activeDenoiser()` → `OIDN_CPU` when `m_device != nullptr` |

### NeuralRendererFactory

| Deliverable | Location |
|---|---|
| `NeuralBackend` enum | `src/kernel/include/nexus/neural/NeuralRenderer.h` |
| `NeuralRendererFactory::create(NeuralBackend, IDevice&)` declaration | same header |
| `NeuralRendererFactory::create` implementation | `src/kernel/src/neural/NeuralRenderer.cpp` |

Behaviour:
- `Auto` — `createNeuralRenderer` priority chain (DLSS4 → XeSS → OIDN → Bilinear).
- `DLSS4` — calls `createNeuralRenderer(preferDLSS=true, preferXeSS=false)`; falls back to Bilinear if NGX absent.
- `XeSS` — calls `createNeuralRenderer(preferDLSS=false, preferXeSS=true)`; falls back to Bilinear if XeSS absent.
- `OIDN_CPU` — constructs `OIDNDenoiser` directly.
- `Bilinear` — constructs `DeterministicFallbackNeuralRenderer`.
- Never returns `nullptr`.

### Exit criteria

- `DLSSPlugin` reports `DLSS4` backends when `m_ngxAvailable == true`.
- `XeSSPlugin` reports `XeSS` upscaler when `m_available == true`.
- `OIDNDenoiser` reports `OIDN_CPU` when `m_device != nullptr`.
- `NeuralRendererFactory::create(Bilinear, dev)` always returns non-null with `Bilinear` upscaler.

---

## Track 2 — Live Vulkan RT Dispatch Tests

### Deliverables

New `tests/kernel/test_VulkanRTDispatch.cpp` — 8 tests, all skip-safe:

- `VulkanContextCreatesWithRTEnabled` — basic Vulkan availability guard.
- `DeviceReportsRayTracingCapabilities` — reads `rayTracingTier` from `DeviceCapabilities`.
- `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` — renders with RT pipeline on Tier-1
  hardware; asserts `FrameStats::rtReflectionsActive == true`.
- `NeuralRendererFactoryAutoSelectReturnsNonNull` — factory `Auto` always produces a valid renderer.
- `NeuralRendererFactoryBilinearAlwaysAvailable` — factory `Bilinear` always reports `Bilinear` upscaler.
- `DLSSUpscalerActiveWhenSDKPresent` — end-to-end: DLSS upscaler registered, frame rendered,
  `upscalingActive` and `activeUpscaler == DLSS4` asserted. Skips if NGX absent.
- `XeSSUpscalerActiveWhenSDKPresent` — same for XeSS. Skips if XeSS absent.
- `OIDNCPUDenoiserActiveWhenDeviceAvailable` — end-to-end: OIDN denoiser registered, frame
  rendered, `denoisingActive` and `activeDenoiser == OIDN_CPU` asserted. Skips if
  `NEXUS_ENABLE_OIDN=OFF`.

Registered in `tests/CMakeLists.txt` under the `if(NEXUS_BACKEND_VULKAN)` block.

### Exit criteria

- All 8 tests discovered when `NEXUS_BACKEND_VULKAN=ON`.
- Tests skip cleanly (not fail) in Null-only CI.
- On a Tier-1 RT device, `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` passes.
- `NeuralRendererFactoryBilinearAlwaysAvailable` always passes (no hardware needed).
