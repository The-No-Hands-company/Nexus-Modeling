# Month 35 — Volumetric Lighting Pass (v0.7 Track 2)

Status: **complete**. Delivers `VolumetricSettings`, `setVolumetricSettings`,
`FrameStats::volumetricActive` + `volumetricFroxelCount`, froxel dispatch in
`render()`, and 8 Null-backend tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.7-planning.md](v0.7-planning.md),
[month-34-checklist.md](month-34-checklist.md).

---

## VolumetricSettings

```cpp
struct VolumetricSettings {
    bool     enabled                  = false;
    float    fogDensity               = 0.02f;
    float    scatteringCoeff          = 0.5f;
    float    extinctionCoeff          = 0.02f;
    uint32_t froxelSlices             = 64;
    uint32_t froxelResolutionDivisor  = 8;
};
```

---

## Froxel dispatch

After the shadow pass and before the RT reflections block:

```cpp
if (m_impl->volumetric.enabled) {
    const uint32_t froxelW = max(1u, extent.width  / divisor);
    const uint32_t froxelH = max(1u, extent.height / divisor);
    cmd.dispatch((froxelW+7)/8, (froxelH+7)/8, froxelSlices);
    m_stats.volumetricActive      = true;
    m_stats.volumetricFroxelCount = froxelW * froxelH * froxelSlices;
}
```

One thread-group per 8×8 froxel tile, `froxelSlices` depth groups. Uses the
existing graphics command buffer — no new async-compute slot needed.

---

## test_VolumetricLighting.cpp (8 tests)

| Test | Behaviour |
|---|---|
| `VolumetricDefaultIsDisabled` | `enabled == false` by default |
| `VolumetricActiveDefaultsToFalse` | Stats zero on first render |
| `VolumetricSettingsRoundTrip` | All fields persisted |
| `EnabledVolumetricSetsActiveFlag` | `volumetricActive == true` when enabled |
| `EnabledVolumetricPopulatesFroxelCount` | Count > 0 when enabled |
| `DisabledVolumetricProducesZeroFroxels` | Count == 0 when disabled |
| `VolumetricStatsResetEachFrame` | Toggle between frames |
| `VolumetricFroxelCountMatchesExpectedFormula` | Count ≥ froxelSlices at divisor=1 |

### Exit criteria

- All 8 tests pass in Null-only CI.
- Baseline advances from 2263 to 2271.
