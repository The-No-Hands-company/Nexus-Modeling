cmake_minimum_required(VERSION 3.28)

# ── Fetch GoogleTest ──────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(nexus_kernel_tests
    kernel/test_Types.cpp
    kernel/test_TypesExtended.cpp
    kernel/test_AnimationCore.cpp
    kernel/test_AnimationSerialization.cpp
    kernel/test_ProceduralGeometry.cpp
    kernel/test_ProceduralAnimation.cpp
    kernel/test_Mesh.cpp
        kernel/test_BooleanOperation.cpp
    kernel/test_BevelChamfer.cpp
    kernel/test_RemeshOperation.cpp
    kernel/test_HardSurfaceWorkflow.cpp
    kernel/test_ModelingShell.cpp
    kernel/test_MeshDiagnosticOverlay.cpp
    kernel/test_MeshIO.cpp
    kernel/test_Meshlet.cpp
    kernel/test_InsetFacesOperation.cpp
    kernel/test_SceneAsset.cpp
    kernel/test_SceneAssetImporter.cpp
    kernel/test_SceneAssetTextAdapter.cpp
    kernel/test_AssetDependencyGraph.cpp
    kernel/test_GeometryKernel.cpp
    kernel/test_TopologyGolden.cpp
    kernel/test_RenderPipelineV1.cpp
    kernel/test_ParametricFoundation.cpp
    kernel/test_NullDevice.cpp
    kernel/test_NullDeviceExtended.cpp
    kernel/test_Camera.cpp
    kernel/test_CameraExtended.cpp
    kernel/test_SceneGraph.cpp
    kernel/test_SceneGraphExtended.cpp
    kernel/test_RenderContext.cpp
    kernel/test_RenderContextExtended.cpp
    kernel/test_Renderer.cpp
    kernel/test_RendererBehavior.cpp
    kernel/test_NeuralRenderer.cpp
    kernel/test_EvalGraph.cpp
    kernel/test_Subgraph.cpp
    kernel/test_SubgraphSerialization.cpp
    kernel/test_SubgraphPortContracts.cpp
    kernel/test_SubgraphRegistry.cpp
    kernel/test_StrokeHistorySerialization.cpp
    kernel/test_Expression.cpp
    kernel/test_ExpressionNode.cpp
    kernel/test_GeometryNodes.cpp
    kernel/test_NodeSceneSerializer.cpp
    kernel/test_NodeScene.cpp
    kernel/test_SimulationCore.cpp
    kernel/test_ClothSolver.cpp
    kernel/test_FluidSolver.cpp
    kernel/test_GaussianSplatting.cpp
    kernel/test_GaussianSplatPass.cpp
    kernel/test_SculptSession.cpp
    kernel/test_SculptSlice2.cpp
    kernel/test_SculptSlice3.cpp
    kernel/test_SimCouplingHarness.cpp
    kernel/test_SimCouplingSync.cpp
    kernel/test_ExpressionNodeSerializer.cpp
    kernel/test_AutomationScript.cpp
    kernel/test_ApiFreezeAudit.cpp
    kernel/test_ScenarioArtifacts.cpp
    kernel/test_CapabilityDashboardContracts.cpp
    kernel/test_ToolingApp.cpp
    kernel/test_SoftwareRasterizer.cpp
    kernel/test_SoftrastExtension.cpp
    kernel/test_SoftrastScenario.cpp
    kernel/test_RemeshExtension.cpp
    kernel/test_GeometryExtension.cpp
    kernel/test_SculptExtension.cpp
    kernel/test_ParametricExtension.cpp
    kernel/test_EvalExtension.cpp
    kernel/test_NodeSceneExtension.cpp
    kernel/test_AnimationExtension.cpp
    kernel/test_ExpressionNodeExtension.cpp
    kernel/test_SimExtension.cpp
    kernel/test_AssetExtension.cpp
    kernel/test_SubgraphExtension.cpp
    kernel/test_RenderExtension.cpp
    kernel/test_FrameCaptureExtension.cpp
    kernel/test_SceneGraphExtension.cpp
    kernel/test_ScriptExpressionExtension.cpp
    kernel/test_NeuralExtension.cpp
    kernel/test_ParametricSamplesExtension.cpp
    kernel/test_StrokeHistoryExtension.cpp
    kernel/test_SimCouplingExtension.cpp
    kernel/test_GfxPlannerExtension.cpp
    kernel/test_GfxDeviceExtension.cpp
    kernel/test_RendererExtension.cpp
    kernel/test_FrameSchedulerExtension.cpp
    kernel/test_AnimationSerializationExtension.cpp
    kernel/test_AssetGraphExtension.cpp
    kernel/test_EvalNodesExtension.cpp
    kernel/test_GeometryKernelExtension.cpp
    kernel/test_DescriptorBinder.cpp
    kernel/test_ShadowMapTarget.cpp
    kernel/test_RendererIntegration.cpp
    kernel/test_PassOrdering.cpp
    kernel/test_RenderPathParity.cpp
    kernel/test_FrameTimingLayer.cpp
    kernel/test_TemporalAccumulation.cpp
    kernel/test_NeuralDenoiser.cpp
    kernel/test_NeuralUpscaler.cpp
    kernel/test_RTProductionPath.cpp
    kernel/test_RTPipelineCreation.cpp
    kernel/test_SceneBVH.cpp
    kernel/test_MeshShaderProductionPath.cpp
    kernel/test_DLSSRayReconstruction.cpp
    kernel/test_FSR3Upscaler.cpp
    kernel/test_RTShaderBindingTable.cpp
    kernel/test_VariableRateShading.cpp
    kernel/test_VolumetricLighting.cpp
    kernel/test_MSAAResolve.cpp
    kernel/test_ScreenSpaceAO.cpp
    kernel/test_ScreenSpaceReflections.cpp
    kernel/test_BloomPostProcess.cpp
    kernel/test_DepthOfField.cpp
    kernel/test_MotionBlur.cpp
    kernel/test_ToneMapping.cpp
    kernel/test_IBLighting.cpp
    kernel/test_OITransparency.cpp
    kernel/test_FSR4Upscaler.cpp
    kernel/test_SubsurfaceScattering.cpp
    kernel/test_ContactShadows.cpp
    kernel/test_TiledDeferredLighting.cpp
    kernel/test_ClusteredLighting.cpp
    kernel/test_SSGI.cpp
    kernel/test_DecalRendering.cpp
    kernel/test_RayTracedGI.cpp
    kernel/test_AtmosphericScattering.cpp
    kernel/test_LightShafts.cpp
    kernel/test_MultiBounceRTGI.cpp
    kernel/test_HBAO.cpp
    kernel/test_VarianceShadowMaps.cpp
    kernel/test_ReSTIRGI.cpp
    kernel/test_CascadedVSMBlending.cpp
    kernel/test_LensFlare.cpp
    kernel/test_ReSTIRPT.cpp
    kernel/test_CameraLensEffects.cpp
    kernel/test_ScreenSpaceCaustics.cpp
    kernel/test_SpectralDispersion.cpp
    kernel/test_PhotonMapping.cpp
    kernel/test_AutoExposure.cpp
    kernel/test_MultiSpectral.cpp
    kernel/test_BDPT.cpp
    kernel/test_AutoWhiteBalance.cpp
    kernel/test_Polarisation.cpp
    kernel/test_Fluorescence.cpp
    kernel/test_SpectralUpsampling.cpp
    kernel/test_MuellerBSDF.cpp
    kernel/test_PhosphorescenceDecay.cpp
    kernel/test_HyperspectralIBL.cpp
    kernel/test_Plenoptic.cpp
    kernel/test_WaveOptics.cpp
    kernel/test_SpectralMedia.cpp
    kernel/test_NeRF.cpp
    kernel/test_GaussianSplatSpectral.cpp
    kernel/test_LightFieldDisplay.cpp
    kernel/test_InstantNGP.cpp
    kernel/test_DynamicNeRF.cpp
    kernel/test_HolographicWavefront.cpp
    kernel/test_NeuralSceneFlow.cpp
    kernel/test_EyeboxSteering.cpp
    kernel/test_NeRFWild.cpp
    kernel/test_DeformableSurface.cpp
    kernel/test_VarifocalHolographic.cpp
    kernel/test_NeRFRelighting.cpp
    kernel/test_HalfEdgeMesh.cpp
    kernel/test_RobustPredicates.cpp
    kernel/test_ConstraintDOF.cpp
    kernel/test_MeshTopologyAnalysis.cpp
    kernel/test_Delaunay2D.cpp
    kernel/test_ConstraintPropagation.cpp
    kernel/test_ConstrainedDelaunay2D.cpp
    kernel/test_Voronoi2D.cpp
    kernel/test_SubdivisionSurface.cpp
    kernel/test_MeshDecimator.cpp
    kernel/test_MeshRepair.cpp
    kernel/test_NurbsCurve.cpp
    kernel/test_NurbsSurface.cpp
    kernel/test_SurfaceTrim.cpp
    kernel/test_SweepLoft.cpp
    kernel/test_RailLoft.cpp
    kernel/test_SurfaceCurveIntersect.cpp
    kernel/test_TrimBoolean.cpp
)

add_executable(nexus_kernel_perf_smoke
    kernel/perf_smoke.cpp
)

if(NEXUS_BACKEND_VULKAN)
    target_sources(nexus_kernel_tests PRIVATE
        kernel/test_VulkanShaderCompiler.cpp
        kernel/test_VulkanPipeline.cpp
        kernel/test_VulkanFrameScheduler.cpp
        kernel/test_VulkanRegression.cpp
        kernel/test_VulkanRTDispatch.cpp
    )
endif()

target_link_libraries(nexus_kernel_tests
    PRIVATE nexus_gfx_core nexus_backend_null nexus_neural nexus_softrast nexus_automation_ext GTest::gtest_main
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:nexus_backend_vulkan>
)

add_dependencies(nexus_kernel_tests nexus_tooling_app)

target_compile_features(nexus_kernel_tests PRIVATE cxx_std_23)
target_compile_features(nexus_kernel_perf_smoke PRIVATE cxx_std_23)

target_compile_definitions(nexus_kernel_tests PRIVATE
    NEXUS_TESTS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    NEXUS_TOOLING_APP_PATH="$<TARGET_FILE:nexus_tooling_app>"
)

target_link_libraries(nexus_kernel_perf_smoke
    PRIVATE nexus_gfx_core nexus_backend_null nexus_neural
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:nexus_backend_vulkan>
)

target_compile_definitions(nexus_kernel_perf_smoke PRIVATE
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:NEXUS_BACKEND_VULKAN>
)

gtest_discover_tests(nexus_kernel_tests)
add_test(NAME KernelPerfSmoke.Null COMMAND nexus_kernel_perf_smoke --frames 120)
add_test(NAME KernelPerfSmoke.Determinism COMMAND nexus_kernel_perf_smoke --frames 64 --determinism-runs 3)

# Hardware perf gates — skip cleanly (exit 0 "SKIPPED") when Vulkan is absent.
# Ceilings are only enforced when real GPU timing is available (gpuTimeMs > 0).
add_test(NAME KernelPerfSmoke.VulkanRT
    COMMAND nexus_kernel_perf_smoke
        --backend vulkan --frames 120 --rt --gpu-ceiling-ms 33
)
set_tests_properties(KernelPerfSmoke.VulkanRT PROPERTIES
    LABELS  "perf;vulkan;rt"
    TIMEOUT 120
)

add_test(NAME KernelPerfSmoke.DLSSSteadyState
    COMMAND nexus_kernel_perf_smoke
        --backend vulkan --frames 64 --neural-backend dlss --neural-overhead-ceiling-ms 4
)
set_tests_properties(KernelPerfSmoke.DLSSSteadyState PROPERTIES
    LABELS  "perf;vulkan;neural"
    TIMEOUT 120
)

# ─────────────────────────────────────────────────────────────────────────────
#  PR-19 — Week 4 Day 4: CI app smoke integration
#
#  Drives tools/app_ci_smoke.sh against the freshly built tooling app. The
#  script exercises the headless surfaces (help, scenarios, starter-model,
#  editor seams, session panel, persistence) and asserts marker strings.
#  Registered as a ctest entry so CI participates without manual steps.
# ─────────────────────────────────────────────────────────────────────────────
add_test(
    NAME ToolingAppCISmoke
    COMMAND
        ${CMAKE_SOURCE_DIR}/tools/app_ci_smoke.sh
        $<TARGET_FILE:nexus_tooling_app>
        ${CMAKE_BINARY_DIR}/app_ci_smoke_state
)
set_tests_properties(ToolingAppCISmoke PROPERTIES
    LABELS "tooling;smoke"
    TIMEOUT 60
)
