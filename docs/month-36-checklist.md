# Month 36 — MSAA Resolve (v0.7 Track 3)

Status: **complete**. Delivers `DeviceCapabilities::maxMsaaSamples`,
`RendererSettings::msaaSamples`, `FrameStats::msaaSamples`, `resolveTexture`,
and 7 Null-backend tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.7-planning.md](v0.7-planning.md),
[month-35-checklist.md](month-35-checklist.md).

---

## DeviceCapabilities::maxMsaaSamples

```cpp
uint8_t maxMsaaSamples = 1;  // Null backend: 1 (no MSAA)
```

Vulkan backend reports the minimum of `VkPhysicalDeviceProperties::limits::framebufferColorSampleCounts`
and `framebufferDepthSampleCounts`.

---

## RendererSettings::msaaSamples + clamping

```cpp
uint8_t msaaSamples = 1;  // 1 = off; clamped to caps().maxMsaaSamples each frame
```

`render()` stats reset block:
```cpp
m_stats.msaaSamples = std::min(m_settings.msaaSamples, m_ctx.caps().maxMsaaSamples);
```

When `msaaSamples > 1`, the Vulkan backend allocates GBuffer textures with the
specified sample count and issues `resolveTexture` before the composite pass. This
Vulkan implementation is wired in the Vulkan backend integration milestone; the Null
backend clamps to 1 and the resolve call is a no-op.

---

## ICommandBuffer::resolveTexture

```cpp
virtual void resolveTexture(TextureHandle src, TextureHandle dst) { (void)src; (void)dst; }
```

Default no-op. Vulkan override: `vkCmdResolveImage` when src sample count > 1.

---

## test_MSAAResolve.cpp (7 tests)

| Test | Behaviour |
|---|---|
| `NullBackendReportsMaxMsaaSamplesOfOne` | `caps().maxMsaaSamples == 1` |
| `DefaultMsaaSamplesIsOne` | Setting defaults to 1 |
| `MsaaSamplesSettingRoundTrip` | Set 4 → get 4 |
| `MsaaSamplesStatIsOneWhenSettingIsOne` | Stat == 1 with default settings |
| `MsaaSamplesStatClampedToMaxCaps` | Request 4 → stat == 1 on Null |
| `MsaaSamplesStatNeverExceedsMaxCaps` | Hard upper bound enforced |
| `ResolveTextureWithInvalidHandlesDoesNotCrash` | No-op on Null backend |

### Exit criteria

- All 7 tests pass in Null-only CI.
- Baseline advances from 2271 to 2278.
- `FrameStats::msaaSamples` never exceeds `caps().maxMsaaSamples`.
