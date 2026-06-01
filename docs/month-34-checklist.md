# Month 34 — Variable-Rate Shading (v0.7 Track 1)

Status: **complete**. Delivers `ShadingRate` enum, `setFragmentShadingRate`,
`RendererSettings::enableVRS` + `defaultShadingRate`, `FrameStats::vrsActive`,
VRS wiring in `render()`, and 8 Null-backend tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.7-planning.md](v0.7-planning.md),
[month-33-checklist.md](month-33-checklist.md).

Also stamps the v0.6.0 release (CMakeLists.txt version bump, CHANGELOG date) and
creates `docs/v0.7-planning.md`.

---

## ShadingRate enum

```cpp
enum class ShadingRate : uint8_t {
    Rate1x1 = 0,  // full rate (default / reset value)
    Rate1x2 = 1,
    Rate2x1 = 2,
    Rate2x2 = 3,
    Rate4x2 = 4,
    Rate4x4 = 5,  // maximum coarsening
};
```

Rate1x1 == 0 is API-stable (used as the reset value in tests and the render path).

---

## ICommandBuffer::setFragmentShadingRate

```cpp
virtual void setFragmentShadingRate(ShadingRate rate) { (void)rate; }
```

Default is a no-op. Vulkan override issues `vkCmdSetFragmentShadingRateKHR`.
Only takes effect when `caps().variableRateShading == true`.

---

## Renderer wiring

Before geometry `beginRenderPass`:
```cpp
if (enableVRS && caps().variableRateShading) {
    cmd.setFragmentShadingRate(settings.defaultShadingRate);
    m_stats.vrsActive = true;
}
```
After geometry `endRenderPass`:
```cpp
if (vrsEnabled) cmd.setFragmentShadingRate(ShadingRate::Rate1x1);
```

The reset ensures composite, RT, and denoiser passes always run at full rate.

---

## test_VariableRateShading.cpp (8 tests)

| Test | Behaviour |
|---|---|
| `ShadingRateEnumValuesAreDistinct` | No aliased values |
| `ShadingRate1x1IsZero` | API-freeze: Rate1x1 == 0 |
| `DefaultVRSSettingIsDisabled` | `enableVRS` defaults to `false` |
| `DefaultShadingRateIs2x2` | `defaultShadingRate` defaults to `Rate2x2` |
| `VRSSettingsRoundTrip` | Set/get flag + rate |
| `VRSActiveDefaultsToFalse` | `vrsActive == false` on first render |
| `VRSFlagEnabledButCapsAbsentKeepsVRSInactive` | Null cap blocks dispatch |
| `VRSActiveResetEachFrame` | Per-frame independence |
| `DisablingVRSMidSessionHasImmediateEffect` | Toggle visible next frame |

### Exit criteria

- All 8 tests pass in Null-only CI.
- Baseline advances from 2255 to 2263.
