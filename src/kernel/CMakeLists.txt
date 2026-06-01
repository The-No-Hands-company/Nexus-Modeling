# ─────────────────────────────────────────────────────────────────────────────
#  Nexus Graphics Kernel
# ─────────────────────────────────────────────────────────────────────────────

# ── Core abstraction library ──────────────────────────────────────────────────
add_library(nexus_gfx_core STATIC
    src/gfx/RenderContext.cpp
    src/gfx/Device.cpp
    src/memory/GPUAllocator.cpp
    src/animation/AnimationCore.cpp
    src/animation/AnimationSerialization.cpp
    src/animation/SkeletonRetargeter.cpp
    src/geometry/Mesh.cpp
    src/geometry/Meshlet.cpp
    src/geometry/GeometryKernel.cpp
    src/geometry/BooleanOperation.cpp
    src/geometry/BevelChamfer.cpp
    src/geometry/RemeshOperation.cpp
    src/geometry/MeshDiagnosticOverlay.cpp
    src/geometry/MeshIO.cpp
    src/geometry/ExtrudeOperation.cpp
    src/geometry/InsetFacesOperation.cpp
    src/geometry/HardSurfaceWorkflow.cpp
    src/geometry/ModelingShell.cpp
    src/asset/SceneAsset.cpp
    src/asset/SceneAssetImporter.cpp
    src/asset/SceneAssetTextAdapter.cpp
    src/asset/AssetDependencyGraph.cpp
    src/render/Camera.cpp
    src/render/SceneGraph.cpp
    src/render/Renderer.cpp
    src/render/RenderGraphValidator.cpp
    src/render/FrameCaptureExporter.cpp
    src/render/GaussianSplatPass.cpp
    src/sculpt/Brush.cpp
    src/sculpt/SculptSession.cpp
    src/sculpt/StrokeHistorySerialization.cpp
    src/parametric/ConstraintGraph.cpp
    src/parametric/ParametricSolver.cpp
    src/parametric/ParametricSerialization.cpp
    src/parametric/ParametricSamples.cpp
    src/eval/EvalGraph.cpp
    src/eval/ExpressionNode.cpp
    src/eval/ExpressionNodeSerializer.cpp
    src/eval/GeometryNodes.cpp
    src/eval/AnimationNodes.cpp
    src/eval_ext/Subgraph.cpp
    src/eval_ext/SubgraphSerialization.cpp
    src/eval_ext/SubgraphRegistry.cpp
    src/scene/NodeScene.cpp
    src/scene/NodeSceneSerializer.cpp
    src/sim/SimulationCore.cpp
    src/sim/ClothSolver.cpp
    src/sim/FluidSolver.cpp
    src/sim/SimCouplingHarness.cpp
    src/gfx/GaussianSplatting.cpp
    src/render/TemporalAccumulation.cpp
    src/render/DescriptorBinder.cpp
    src/render/ShadowMapTarget.cpp
    src/render/FrameTimingLayer.cpp
    src/script/Expression.cpp
    src/automation/AutomationScript.cpp
)

target_include_directories(nexus_gfx_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(nexus_gfx_core PUBLIC cxx_std_23)

# ── Software rasterizer (CPU, no GPU / display dependency) ────────────────────
add_library(nexus_softrast STATIC
    src/softrast/SoftwareRasterizer.cpp
)
# Expose src/ so consumers can #include <softrast/PixelBuffer.h> etc.
target_include_directories(nexus_softrast
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(nexus_softrast PUBLIC nexus_gfx_core)
target_compile_features(nexus_softrast PUBLIC cxx_std_23)
if(NOT MSVC)
    target_compile_options(nexus_softrast PRIVATE
        -Wall -Wextra -Wpedantic -Werror -O2
    )
endif()

# ── Automation extension (softrast.* + mesh.remesh/weld commands) ────────────
add_library(nexus_automation_ext STATIC
    src/automation/SoftrastExtension.cpp
    src/automation/RemeshExtension.cpp
    src/automation/GeometryExtension.cpp
    src/automation/SculptExtension.cpp
    src/automation/ParametricExtension.cpp
    src/automation/EvalExtension.cpp
    src/automation/NodeSceneExtension.cpp
    src/automation/AnimationExtension.cpp
    src/automation/ExpressionNodeExtension.cpp
    src/automation/SimExtension.cpp
    src/automation/AssetExtension.cpp
    src/automation/SubgraphExtension.cpp
    src/automation/RenderExtension.cpp
    src/automation/FrameCaptureExtension.cpp
    src/automation/SceneGraphExtension.cpp
    src/automation/ScriptExpressionExtension.cpp
    src/automation/NeuralExtension.cpp
    src/automation/ParametricSamplesExtension.cpp
    src/automation/StrokeHistoryExtension.cpp
    src/automation/SimCouplingExtension.cpp
    src/automation/GfxPlannerExtension.cpp
    src/automation/GfxDeviceExtension.cpp
    src/automation/RendererExtension.cpp
    src/automation/FrameSchedulerExtension.cpp
    src/automation/AnimationSerializationExtension.cpp
    src/automation/AssetGraphExtension.cpp
    src/automation/EvalNodesExtension.cpp
    src/automation/GeometryKernelExtension.cpp
)
target_include_directories(nexus_automation_ext
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(nexus_automation_ext PUBLIC nexus_gfx_core nexus_softrast)
target_compile_features(nexus_automation_ext PUBLIC cxx_std_23)
if(NOT MSVC)
    target_compile_options(nexus_automation_ext PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

# Propagate backend selection into Device.cpp (which lives in nexus_gfx_core)
if(NEXUS_BACKEND_VULKAN)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_BACKEND_VULKAN)
endif()
if(NEXUS_BACKEND_NULL)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_BACKEND_NULL)
endif()

# Aggressive optimisation flags per-platform
if(MSVC)
    target_compile_options(nexus_gfx_core PRIVATE /W4 /WX /O2 /arch:AVX2)
else()
    target_compile_options(nexus_gfx_core PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -march=native          # host-side frequency-bound code benefits from native tuning
        -ffast-math            # safe for non-IEEE geometry math
    )
endif()

# ── Vulkan backend ────────────────────────────────────────────────────────────
if(NEXUS_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)

    add_library(nexus_backend_vulkan STATIC
        src/backend/vulkan/VulkanDevice.cpp
        src/backend/vulkan/VulkanSwapchain.cpp
        src/backend/vulkan/VulkanBuffer.cpp
        src/backend/vulkan/VulkanTexture.cpp
        src/backend/vulkan/VulkanShader.cpp
        src/backend/vulkan/VulkanPipeline.cpp
        src/backend/vulkan/VulkanCommandBuffer.cpp
        src/backend/vulkan/VulkanFrameScheduler.cpp
        src/backend/vulkan/VulkanQueue.cpp
        src/backend/vulkan/VulkanSync.cpp
        src/backend/vulkan/VulkanAllocator.cpp
        src/backend/vulkan/VulkanRayTracing.cpp
        src/backend/vulkan/VulkanMeshShader.cpp
    )

    target_include_directories(nexus_backend_vulkan
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(nexus_backend_vulkan
        PUBLIC nexus_gfx_core Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator
               glslang::glslang glslang::SPIRV glslang-default-resource-limits
    )

    target_compile_definitions(nexus_backend_vulkan PUBLIC NEXUS_BACKEND_VULKAN)
    # Request Vulkan 1.4
    target_compile_definitions(nexus_backend_vulkan PUBLIC VK_API_VERSION=VK_MAKE_API_VERSION\(0,1,4,0\))
endif()

# ── Null / headless backend ───────────────────────────────────────────────────
if(NEXUS_BACKEND_NULL)
    add_library(nexus_backend_null INTERFACE)

    target_include_directories(nexus_backend_null
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )

    target_link_libraries(nexus_backend_null INTERFACE nexus_gfx_core)
    target_compile_definitions(nexus_backend_null INTERFACE NEXUS_BACKEND_NULL)
endif()

# ── Neural rendering module (DLSS4 / XeSS / OIDN) ────────────────────────────
add_library(nexus_neural STATIC
    src/neural/OIDNDenoiser.cpp
    src/neural/DLSSPlugin.cpp
    src/neural/XeSSPlugin.cpp
    src/neural/FSR3Plugin.cpp
    src/neural/NeuralRenderer.cpp
)

target_include_directories(nexus_neural
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nexus_neural PUBLIC nexus_gfx_core)

if(NEXUS_BACKEND_VULKAN)
    target_link_libraries(nexus_neural PRIVATE nexus_backend_vulkan)
endif()

# ── NVIDIA Aftermath (GPU crash resilience) ───────────────────────────────────
if(NEXUS_ENABLE_AFTERMATH)
    find_package(Aftermath REQUIRED)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_ENABLE_AFTERMATH)
    target_link_libraries(nexus_gfx_core PUBLIC Aftermath::Aftermath)
endif()

# ── Intel Open Image Denoise (CPU fallback denoiser) ─────────────────────────
if(NEXUS_ENABLE_OIDN)
    find_package(OpenImageDenoise QUIET)
    if(OpenImageDenoise_FOUND)
        target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_OIDN)
        target_link_libraries(nexus_neural PUBLIC OpenImageDenoise)
    else()
        message(WARNING "OpenImageDenoise not found — OIDN denoiser disabled")
    endif()
endif()

# ── DLSS 4 (runtime plugin — no link-time dep) ───────────────────────────────
if(NEXUS_ENABLE_DLSS)
    target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_DLSS)
endif()

# ── XeSS (runtime plugin — no link-time dep) ─────────────────────────────────
if(NEXUS_ENABLE_XESS)
    target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_XESS)
endif()

# ── Install targets ───────────────────────────────────────────────────────────
install(TARGETS nexus_gfx_core nexus_neural ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
