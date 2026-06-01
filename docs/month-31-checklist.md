# Month 31 — FSR 3 Upscaler Integration (v0.6 Track 1)

Status: **complete**. Delivers v0.6 Track 1: `NeuralBackend::FSR3`, `FSR3Plugin`,
factory wiring, `preferFSR` activation, `perf_smoke` token, and 7 Null-backend tests.
Also stamps the v0.5.0 release (CMakeLists.txt version bump, CHANGELOG date) and
creates `docs/v0.6-planning.md`.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.6-planning.md](v0.6-planning.md),
[month-30-checklist.md](month-30-checklist.md).

---

## v0.5 Release Stamp

- `CMakeLists.txt` version bumped: `0.4.0` → `0.5.0`.
- `CHANGELOG.md` `[v0.5]` stamped `2026-06-01`; `[v0.6] — upcoming` section opened.
- `ROADMAP.md` Next 4-6 Weeks updated to point at v0.6 and `docs/v0.6-planning.md`.

---

## Track 1 — NeuralBackend::FSR3

New enum value in `NeuralBackend` (value = 5, API-stable):

```cpp
FSR3,  // AMD FidelityFX Super Resolution 3; falls back to Bilinear if unavailable
```

Ordering: `Auto=0, DLSS4=1, DLSS_RR=2, XeSS=3, OIDN_CPU=4, FSR3=5, Bilinear=6`.

---

## Track 1 — FSR3Plugin

New `INeuralRenderer` implementation in `src/kernel/src/neural/`:

- `FSR3Plugin()` — default-constructible; calls `loadLibrary()` when
  `NEXUS_ENABLE_FSR3` is defined; sets `m_available = true` when all three
  FFX entry points (`Create`, `Destroy`, `Dispatch`) resolve.
- `activeUpscaler()` → `FSR3` when available.
- `activeDenoiser()` → `FSR3` when available.
- `upscale()` / `denoise()` — passthrough until full `ffxFsr3UpscalerContextDispatch`
  parameter wiring in the Vulkan FFX backend integration milestone.
- Null backend: `NEXUS_ENABLE_FSR3` absent → `m_available = false` → factory falls back.

---

## Track 1 — createNeuralRenderer preferFSR activation

`(void)preferFSR` stub removed. FSR3 probe inserted between XeSS and OIDN in the
auto-select priority chain:

```
DLSS4 → XeSS → FSR3 → OIDN_CPU → DeterministicFallback
```

`NeuralRendererFactory::create(FSR3, device)` uses `FSR3Plugin` directly; falls back
to `DeterministicFallbackNeuralRenderer` when the SDK is absent.

---

## Track 1 — test_FSR3Upscaler.cpp (7 tests)

All run on Null backend.

| Test | Behaviour verified |
|---|---|
| `NeuralBackendFSR3IsDistinctFromOtherBackends` | Enum values not aliased |
| `UpscalerBackendFSR3IsDistinctFromOthers` | Enum values not aliased |
| `FactoryCreateFSR3ReturnsNonNull` | Factory never returns null |
| `FactoryCreateFSR3FallsBackWhenSDKAbsent` | Fallback does not report FSR3 upscaler |
| `FSR3RendererAttachesWithoutCrash` | `setNeuralRenderer` + `render` no-throw |
| `FSR3FallbackDoesNotActivateUpscalingOnNullBackend` | Reported upscaler is not FSR3 |
| `NeuralBackendFSR3EnumValueIsStable` | `NeuralBackend::FSR3 == 5` (API freeze) |

### Exit criteria

- All 7 tests pass in Null-only CI.
- Baseline test count advances from 2240 to 2247.
- `NeuralRendererFactory::create(FSR3, device)` returns non-null on all platforms.
- `perf_smoke --neural-backend fsr3` accepted without parse error.
- `preferFSR` is no longer a dead parameter in `createNeuralRenderer`.
