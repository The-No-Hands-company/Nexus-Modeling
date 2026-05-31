# Alpha API Contract Summary (1.0-alpha)

Date: 2026-05-31

This page summarizes the 1.0-alpha public API contract using the enforced manifest and the domain ownership model.

## Sources of Truth

1. Public API manifest: tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
2. Ownership map: docs/kernel-capability-map.md
3. API freeze policy: docs/api-freeze-alpha.md
4. Enforcement test: tests/kernel/test_ApiFreezeAudit.cpp

## Contract Scope

The 1.0-alpha contract is all public headers under:

1. src/kernel/include/nexus

Manifest status at v0.3:

1. Total public headers: 93
2. Audit enforcement: required in CI via ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace
3. Drift policy: any add/remove requires manifest update in the same change

## Domain Ownership Mapping

Domain assignment is rooted in folder/API boundaries and ownership rules from docs/kernel-capability-map.md.

| Domain | Primary Owner | Secondary Owner | API Roots Covered | Header Count |
|---|---|---|---|---:|
| Geometry and Parametric | Geometry Core | Runtime and Evaluation | src/kernel/include/nexus/geometry, src/kernel/include/nexus/parametric | 14 |
| Rendering and GFX | Render Core | Platform Backend | src/kernel/include/nexus/gfx, src/kernel/include/nexus/render | 18 |
| Animation | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/animation | 3 |
| Simulation | Simulation Core | Evaluation and Animation | src/kernel/include/nexus/sim | 4 |
| Asset and Data | Asset and Data Core | Integration and Cloud | src/kernel/include/nexus/asset | 4 |
| Scripting and Automation | Scripting and Integration Core | Domain owners per API | src/kernel/include/nexus/automation | 28 |
| Procedural Evaluation Graph | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/eval, src/kernel/include/nexus/eval_ext | 8 |
| Neural Integrations | Render Core | Platform Backend | src/kernel/include/nexus/neural | 1 |
| Scene and Node Graph | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/scene | 2 |
| Scripting Expression | Scripting and Integration Core | Evaluation and Animation Core | src/kernel/include/nexus/script | 1 |
| Sculpt | Geometry Core | Simulation Core | src/kernel/include/nexus/sculpt | 3 |
| Kernel Entry and Cross-Domain Root | Asset and Data Core | Domain owners per API | src/kernel/include/nexus/Kernel.h | 1 |
| Vulkan Public Compiler Interface | Render Core | Platform Backend | src/kernel/include/nexus/gfx/vulkan | 1 |

Total: 93 headers. (39 at alpha baseline; 54 added across M1–M4 milestones.)

## Frozen Public Header Inventory

1. src/kernel/include/nexus/Kernel.h
2. src/kernel/include/nexus/animation/AnimationCore.h
3. src/kernel/include/nexus/animation/AnimationSerialization.h
4. src/kernel/include/nexus/animation/SkeletonRetargeter.h
5. src/kernel/include/nexus/asset/AssetDependencyGraph.h
6. src/kernel/include/nexus/asset/SceneAsset.h
7. src/kernel/include/nexus/asset/SceneAssetImporter.h
8. src/kernel/include/nexus/asset/SceneAssetTextAdapter.h
9. src/kernel/include/nexus/automation/AutomationScript.h
10. src/kernel/include/nexus/automation/EvalExtension.h
11. src/kernel/include/nexus/automation/GeometryExtension.h
12. src/kernel/include/nexus/automation/AnimationExtension.h
13. src/kernel/include/nexus/automation/AssetExtension.h
14. src/kernel/include/nexus/automation/ExpressionNodeExtension.h
15. src/kernel/include/nexus/automation/NodeSceneExtension.h
16. src/kernel/include/nexus/automation/SimExtension.h
17. src/kernel/include/nexus/automation/SubgraphExtension.h
18. src/kernel/include/nexus/automation/RenderExtension.h
19. src/kernel/include/nexus/automation/FrameCaptureExtension.h
20. src/kernel/include/nexus/automation/SceneGraphExtension.h
21. src/kernel/include/nexus/automation/ScriptExpressionExtension.h
22. src/kernel/include/nexus/automation/NeuralExtension.h
23. src/kernel/include/nexus/automation/ParametricSamplesExtension.h
24. src/kernel/include/nexus/automation/StrokeHistoryExtension.h
25. src/kernel/include/nexus/automation/SimCouplingExtension.h
26. src/kernel/include/nexus/automation/GfxPlannerExtension.h
27. src/kernel/include/nexus/automation/GfxDeviceExtension.h
28. src/kernel/include/nexus/automation/RendererExtension.h
29. src/kernel/include/nexus/automation/FrameSchedulerExtension.h
30. src/kernel/include/nexus/automation/AnimationSerializationExtension.h
31. src/kernel/include/nexus/automation/AssetGraphExtension.h
32. src/kernel/include/nexus/automation/EvalNodesExtension.h
33. src/kernel/include/nexus/automation/GeometryKernelExtension.h
34. src/kernel/include/nexus/automation/ParametricExtension.h
35. src/kernel/include/nexus/automation/RemeshExtension.h
36. src/kernel/include/nexus/automation/SculptExtension.h
37. src/kernel/include/nexus/automation/SoftrastExtension.h
38. src/kernel/include/nexus/eval/AnimationNodes.h
39. src/kernel/include/nexus/eval/EvalGraph.h
40. src/kernel/include/nexus/eval/ExpressionNode.h
41. src/kernel/include/nexus/eval/ExpressionNodeSerializer.h
42. src/kernel/include/nexus/eval/GeometryNodes.h
43. src/kernel/include/nexus/eval_ext/Subgraph.h
44. src/kernel/include/nexus/eval_ext/SubgraphRegistry.h
45. src/kernel/include/nexus/eval_ext/SubgraphSerialization.h
46. src/kernel/include/nexus/geometry/BevelChamfer.h
47. src/kernel/include/nexus/geometry/BooleanOperation.h
48. src/kernel/include/nexus/geometry/ExtrudeOperation.h
49. src/kernel/include/nexus/geometry/GeometryKernel.h
50. src/kernel/include/nexus/geometry/HardSurfaceWorkflow.h
51. src/kernel/include/nexus/geometry/InsetFacesOperation.h
52. src/kernel/include/nexus/geometry/Mesh.h
53. src/kernel/include/nexus/geometry/MeshDiagnosticOverlay.h
54. src/kernel/include/nexus/geometry/MeshIO.h
55. src/kernel/include/nexus/geometry/Meshlet.h
56. src/kernel/include/nexus/geometry/ModelingShell.h
57. src/kernel/include/nexus/geometry/RemeshOperation.h
58. src/kernel/include/nexus/gfx/Allocator.h
59. src/kernel/include/nexus/gfx/CommandBuffer.h
60. src/kernel/include/nexus/gfx/Device.h
61. src/kernel/include/nexus/gfx/FrameScheduler.h
62. src/kernel/include/nexus/gfx/GaussianSplatting.h
63. src/kernel/include/nexus/gfx/RenderContext.h
64. src/kernel/include/nexus/gfx/Swapchain.h
65. src/kernel/include/nexus/gfx/Types.h
66. src/kernel/include/nexus/gfx/vulkan/ShaderCompiler.h
67. src/kernel/include/nexus/neural/NeuralRenderer.h
68. src/kernel/include/nexus/parametric/ConstraintGraph.h
69. src/kernel/include/nexus/parametric/ParametricSamples.h
70. src/kernel/include/nexus/parametric/ParametricSerialization.h
71. src/kernel/include/nexus/parametric/ParametricSolver.h
72. src/kernel/include/nexus/render/Camera.h
73. src/kernel/include/nexus/render/DescriptorBinder.h
74. src/kernel/include/nexus/render/FrameCaptureExporter.h
75. src/kernel/include/nexus/render/FrameTimingLayer.h
76. src/kernel/include/nexus/render/GBufferMeshShaders.h
77. src/kernel/include/nexus/render/GaussianSplatPass.h
78. src/kernel/include/nexus/render/RenderGraphValidator.h
79. src/kernel/include/nexus/render/Renderer.h
80. src/kernel/include/nexus/render/SceneGraph.h
81. src/kernel/include/nexus/render/ShadowMapTarget.h
82. src/kernel/include/nexus/render/ShadowMeshShaders.h
83. src/kernel/include/nexus/render/TemporalAccumulation.h
84. src/kernel/include/nexus/scene/NodeScene.h
85. src/kernel/include/nexus/scene/NodeSceneSerializer.h
86. src/kernel/include/nexus/script/Expression.h
87. src/kernel/include/nexus/sculpt/Brush.h
88. src/kernel/include/nexus/sculpt/SculptSession.h
89. src/kernel/include/nexus/sculpt/StrokeHistorySerialization.h
90. src/kernel/include/nexus/sim/ClothSolver.h
91. src/kernel/include/nexus/sim/FluidSolver.h
92. src/kernel/include/nexus/sim/SimCouplingHarness.h
93. src/kernel/include/nexus/sim/SimulationCore.h

## Change-Control Rules for Alpha Tag Window

1. Additive-only changes are allowed when domain ownership, behavior tests, and docs updates are included.
2. Removals/renames require explicit compatibility rationale and migration notes.
3. Any manifest drift without intentional update fails API audit.
4. Cross-domain API changes require reviewers from each impacted domain.

## v0.3 Domain Summary

Headers added since alpha baseline (39 → 93, +54):

- **Automation extensions** (+28): per-domain scripting extension headers in `nexus/automation/`
  covering eval, geometry, animation, asset, expression nodes, node scene, simulation, subgraph,
  render, frame capture, scene graph, script expression, neural, parametric samples, stroke history,
  simulation coupling, GFX planner/device, renderer, frame scheduler, animation serialization,
  asset graph, eval nodes, geometry kernel, parametric, remesh, sculpt, and softrast extensions.
- **Eval and subgraph** (+7): `AnimationNodes`, `ExpressionNode`, `ExpressionNodeSerializer`,
  `GeometryNodes`; `eval_ext/Subgraph`, `SubgraphRegistry`, `SubgraphSerialization`.
- **Geometry** (+5): `HardSurfaceWorkflow`, `Meshlet`, `ModelingShell` added; `ExtrudeOperation`
  and `InsetFacesOperation` promoted.
- **Render** (+6): `DescriptorBinder`, `FrameTimingLayer`, `GBufferMeshShaders`, `GaussianSplatPass`,
  `ShadowMapTarget`, `ShadowMeshShaders`.
- **Asset** (+1): `SceneAssetTextAdapter`.
- **Scene** (+2): `NodeScene`, `NodeSceneSerializer`.
- **Script** (+1): `Expression`.
- **Sculpt** (+3): `Brush`, `SculptSession`, `StrokeHistorySerialization`.
- **Sim** (+3): `ClothSolver`, `FluidSolver`, `SimCouplingHarness`.

## Verification Commands

1. cmake --build build -j$(nproc)
2. ./build/tests/nexus_kernel_tests --gtest_filter="ApiFreezeAudit.*"
3. ./build/tests/nexus_kernel_tests
