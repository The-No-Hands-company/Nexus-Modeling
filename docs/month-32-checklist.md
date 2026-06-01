# Month 32 — NGX Evaluation Parameter Wiring (v0.6 Track 2)

Status: **complete**. Wires `DLSSPlugin::upscale()` and `denoise()` to call the real
`NVSDK_NGX_VULKAN_EvaluateFeature` API with populated input parameters. No new tests —
existing DLSS tests exercise the same code paths; Track 2 is a behind-the-SDK-gate
behaviour change, not a new API surface.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.6-planning.md](v0.6-planning.md),
[month-31-checklist.md](month-31-checklist.md).

---

## New entry points loaded

Alongside the six existing NGX pfns, `loadLibrary()` now resolves:

| pfn field | NGX symbol |
|---|---|
| `AllocParameters` | `NVSDK_NGX_AllocateParameters` |
| `DestroyParameters` | `NVSDK_NGX_DestroyParameters` |
| `SetParameterULL` | `NVSDK_NGX_Parameter_SetULL` |
| `SetParameterF` | `NVSDK_NGX_Parameter_SetF` |
| `SetParameterVoidP` | `NVSDK_NGX_Parameter_SetVoidPointer` |

---

## New private methods

### `createFeatureHandle(VkCommandBuffer)`

Calls `NVSDK_NGX_VULKAN_CreateFeature` with `featureId=1` (DLSS4) or `featureId=1000`
(Ray Reconstruction) depending on `m_rrMode`. Stores result in `m_featureHandle`. No-op
if feature creation fails — all downstream calls check `m_featureHandle` before use.

### `evaluateFeature(VkCommandBuffer, void* params)`

Thin wrapper around `NVSDK_NGX_VULKAN_EvaluateFeature(cmd, featureHandle, params, nullptr)`.
No-op when either handle is null.

---

## upscale() wiring

1. Lazy-creates the feature handle on first call.
2. Populates NGX parameters: `Color`, `Depth`, `MotionVectors`, `Output`,
   `RenderSizeX/Y`, `OutputSizeX/Y` (via `SetParameterULL`); `JitterOffsetX/Y`,
   `Reset` (via `SetParameterF`).
3. Calls `evaluateFeature(cmd, params)`.
4. Leaves `output.color = input.color` (handle identity); NGX writes the upscaled
   result into the output texture in place.

---

## denoise() wiring

1. Lazy-creates the feature handle (DLSS4 or DLSS-RR depending on `m_rrMode`).
2. Populates: `Color`, `Albedo`, `Normal`, `MotionVectors`, `Output`
   (via `SetParameterULL`); `ExposureScale` (via `SetParameterF`).
3. Calls `evaluateFeature(cmd, params)`.

---

## Destructor update

`m_featureHandle` and `m_parametersHandle` are now released in the destructor
before `m_ngxHandle` and the SDK shutdown, preventing any use-after-free on
the NGX side.

---

## Exit criteria

- `DLSSPlugin::upscale()` and `denoise()` call `NVSDK_NGX_VULKAN_EvaluateFeature`
  on hardware with a live NGX session.
- SDK-absent path: `m_ngxAvailable == false` → passthrough unchanged.
- No new test file (SDK-gated behaviour; existing Vulkan RT dispatch tests cover
  the live path on Tier-1 hardware).
- Null-backend suite unaffected — baseline remains 2247 before Track 3.
