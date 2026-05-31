# Month 28 — Mesh Shader Production Path (v0.5 Track 4)

Status: **complete**. Delivers v0.5 Track 4: `RendererSettings::enableMeshShaders`,
`FrameStats::meshShaderDrawCalls`, per-frame counter reset, and pipeline-slot smoke tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.5-planning.md](v0.5-planning.md),
[month-27-checklist.md](month-27-checklist.md).

---

## Track 4 — RendererSettings::enableMeshShaders

New flag in `RendererSettings` (default `true`):

```cpp
bool enableMeshShaders = true;  // use mesh shader path when caps allow
```

When `false`, the renderer skips `drawMeshTasks` dispatch in both the geometry and shadow
passes, even on hardware that reports `caps().meshShaders == true`. This gives callers a
runtime escape hatch without needing to swap pipelines.

---

## Track 4 — FrameStats::meshShaderDrawCalls

New field in `FrameStats`:

```cpp
uint32_t meshShaderDrawCalls = 0;  // draw calls dispatched via drawMeshTasks this frame
```

### Population

- Reset to `0` at the top of `Renderer::render()` alongside `drawCalls` and `shadowDrawCalls`.
- `submitShadowMeshPackets` and `submitGeometryPackets` each accept an `uint32_t& outMeshShaderDraws`
  out-parameter. When `m_settings.enableMeshShaders && m_ctx.caps().meshShaders` holds for a
  given packet, `++outMeshShaderDraws` fires.
- Call sites accumulate the per-pass locals into `m_stats.meshShaderDrawCalls` after submission.

### Null backend

`caps().meshShaders == false` on the Null backend, so `meshShaderDrawCalls` is always 0 in CI.
On a real mesh-shader–capable GPU, the count reflects the number of `drawMeshTasks` calls fired
across both shadow and geometry passes in that frame.

---

## Track 4 — test_MeshShaderProductionPath.cpp (8 tests)

All run on Null backend; no GPU hardware required.

| Test | Behaviour verified |
|---|---|
| `DefaultMeshShaderSettingIsEnabled` | `RendererSettings::enableMeshShaders` defaults to `true` |
| `EnableMeshShadersRoundTripOnSettings` | Flag toggles cleanly via `setSettings` |
| `MeshShaderDrawCallsDefaultIsZero` | Counter is 0 on empty scene render |
| `MeshShaderFlagEnabledWithoutCapDoesNotDispatch` | Null caps block dispatch even with flag on |
| `MeshShaderFlagDisabledPreventsMeshTaskDispatch` | Flag off → counter stays 0 |
| `MeshShaderDrawCallsIsolatedFromRasterDrawCalls` | `meshShaderDrawCalls` is a distinct counter |
| `MeshShaderDrawCallsResetEachFrame` | Counter is independently 0 across two frames |
| `MeshShaderPipelineSlotSeparateFromGeometryPipeline` | `createMeshShaderPipeline` returns valid handle; `setFallbackMeshPipeline` no-throws |
| `ShadowMeshPipelineSlotAcceptsHandle` | `setShadowMeshPipeline` accepts valid handle |

Registered in `tests/CMakeLists.txt`.

### Exit criteria

- All 8 tests pass in Null-only CI.
- Baseline test count advances from 2225 to 2233.
- `RendererSettings::enableMeshShaders` gates mesh dispatch independently of hardware caps.
- `FrameStats::meshShaderDrawCalls` is reset each frame and incremented per `drawMeshTasks` call.
