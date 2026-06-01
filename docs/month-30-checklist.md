# Month 30 — DLSS Ray Reconstruction (v0.5 Track 3)

Status: **complete**. Delivers v0.5 Track 3: `DenoiserBackend::DLSS_RR`,
`NeuralBackend::DLSS_RR`, `DLSSPlugin` RR mode, factory entry, `perf_smoke` token,
and 6 Null-backend tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.5-planning.md](v0.5-planning.md),
[month-29-checklist.md](month-29-checklist.md).

---

## Track 3 — DenoiserBackend::DLSS_RR

New enum value in `NeuralRenderer.h`:

```cpp
DLSS_RR,  // NVIDIA DLSS Ray Reconstruction (NVSDK_NGX_Feature_RayReconstruction)
```

Distinct from `DLSS4` (temporal denoiser). The Ray Reconstruction feature is the
per-pixel neural denoiser designed for RT content where the signal/noise ratio is
very low on a per-frame basis.

---

## Track 3 — NeuralBackend::DLSS_RR

New enum value in `NeuralBackend`:

```cpp
DLSS_RR,  // DLSS Ray Reconstruction; falls back to Bilinear if unavailable
```

Value = 2 (stable; documented in `test_DLSSRayReconstruction.cpp` as an API-freeze guard).

---

## Track 3 — DLSSPlugin Ray Reconstruction mode

`DLSSPlugin` constructor gains `bool rrMode = false`:

```cpp
DLSSPlugin(VkInstance, VkPhysicalDevice, VkDevice, bool rrMode = false);
```

- `activeDenoiser()`: returns `DLSS_RR` when `m_rrMode && m_ngxAvailable`.
- `denoise()`: branches on `m_rrMode` to the RR evaluation path (currently passthrough;
  full `NVSDK_NGX_VULKAN_EvaluateFeature(…, RayReconstruction)` wiring deferred to the
  Vulkan RT integration milestone).

---

## Track 3 — NeuralRendererFactory::create(DLSS_RR)

```
DLSS_RR → DLSSPlugin(rrMode=true) if NEXUS_BACKEND_VULKAN + NEXUS_ENABLE_DLSS + NGX init OK
        → DeterministicFallbackNeuralRenderer otherwise
```

Never returns nullptr. CI skip contract: SDK-absent path falls back silently to
`DeterministicFallbackNeuralRenderer` (no exception, no abort).

---

## Track 3 — test_DLSSRayReconstruction.cpp (6 tests)

All run on Null backend; no GPU hardware required.

| Test | Behaviour verified |
|---|---|
| `DenoiserBackendDLSSRRIsDistinctFromDLSS4` | Enum values are not equal |
| `NeuralBackendDLSSRRIsDistinctFromDLSS4` | Enum values are not equal |
| `FactoryCreateDLSSRRReturnsNonNullOnNullBackend` | Factory never returns null |
| `FactoryCreateDLSSRRFallsBackWhenSDKAbsent` | Fallback does not report DLSS_RR |
| `DLSSRRRendererAttachesWithoutCrash` | `setNeuralRenderer` + `render` no-throw |
| `DLSSRRFallbackDoesNotActivateDenoisingOnNullBackend` | `denoisingActive == false` on fallback |
| `DLSSRRNeuralBackendEnumValueIsStable` | `NeuralBackend::DLSS_RR == 2` (API freeze) |

### Exit criteria

- All 6 tests pass in Null-only CI.
- Baseline test count advances from 2234 to 2240.
- `NeuralRendererFactory::create(DLSS_RR, device)` returns non-null on all platforms.
- Null-backend suite unaffected — all 2234 prior tests still pass.
- `perf_smoke --neural-backend dlss-rr` accepted without parse error.
