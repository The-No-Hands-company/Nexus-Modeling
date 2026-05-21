// SPDX-License-Identifier: MIT
// Copyright (c) 2025 The No-Hands Company.
//
// nexus/render/ShadowMeshShaders.h
// ─────────────────────────────────────────────────────────────────────────────
// Inline GLSL source strings for the shadow mesh-shader pass (depth-only).
//
// Callers compile these strings into VkShaderModule objects via
// nexus::gfx::ShaderCompiler and use the resulting modules to build a
// depth-only VkPipeline (no color attachments).  The pipeline handle is
// then registered with Renderer::setShadowMeshPipeline().
//
// ─── GPU-visible buffer contract ─────────────────────────────────────────────
//
//  Descriptor set 0, binding 0 — vertex storage buffer (std430 readonly)
//    uint vb[];
//
//    Same interleaved vertex buffer produced by GeometryKernel.  Only
//    position (first 3 floats) is read; normal and any further attributes
//    are ignored.  Assumed stride: 6 uints (pos.xyz + nrm.xyz = 24 bytes).
//
//  Descriptor set 0, binding 1 — meshlet blob storage buffer (std430 readonly)
//    uint mb[];
//
//    Identical binary layout to GBufferMeshShaders.h.  See that header for
//    the full per-field description.
//
// ─── Push constants (64 bytes) ───────────────────────────────────────────────
//
//    layout(push_constant) uniform PC {
//      mat4 lightViewProj;  // offset 0 (64 bytes)
//                           //   column-major light view-projection matrix
//                           //   reversed-Z (depth 1→0)
//    } pc;
//
// ─── Dispatch convention ─────────────────────────────────────────────────────
//
//  No-task path  (kShadowMesh):
//    vkCmdDrawMeshTasksEXT(meshletCount, 1, 1)
//    gl_WorkGroupID.x  == meshlet index

#pragma once

#include <string_view>

namespace nexus::render::mesh_shader_glsl {

// ─────────────────────────────────────────────────────────────────────────────
// kShadowMesh
//   Depth-only mesh shader, one workgroup per meshlet.
//   Writes only gl_Position (depth); no colour output attributes needed.
//   local_size_x = 128 to cover both 64-vertex and 81-primitive slots.
//
//   Dispatch: vkCmdDrawMeshTasksEXT(meshletCount, 1, 1)
//   Pair with: kShadowFrag  (empty fragment shader for depth-only pass)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kShadowMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 128) in;
layout(triangles, max_vertices = 64, max_primitives = 81) out;

// No per-vertex outputs other than gl_Position; depth is written implicitly.

layout(set = 0, binding = 0, std430) readonly buffer VertexBuffer { uint vb[]; };
layout(set = 0, binding = 1, std430) readonly buffer MeshletBlob  { uint mb[]; };

layout(push_constant) uniform PC {
    mat4 lightViewProj;   // light view-projection, reversed-Z
} pc;

void main()
{
    const uint meshletIndex     = gl_WorkGroupID.x;
    const uint meshletCount     = mb[0u];
    const uint vertexIndexCount = mb[1u];

    if (meshletIndex >= meshletCount) {
        SetMeshOutputsEXT(0u, 0u);
        return;
    }

    const uint mBase           = 4u + meshletIndex * 11u;
    const uint vertexOffset    = mb[mBase + 0u];
    const uint vertexCount     = mb[mBase + 1u];
    const uint primitiveOffset = mb[mBase + 2u];
    const uint primitiveCount  = mb[mBase + 3u];

    SetMeshOutputsEXT(vertexCount, primitiveCount);

    const uint vertexIndexBase = 4u + meshletCount * 11u;
    const uint primitiveBase   = vertexIndexBase + vertexIndexCount;

    const uint tid = gl_LocalInvocationID.x;

    // ── Vertex processing ────────────────────────────────────────────────────
    if (tid < vertexCount) {
        const uint globalVert = mb[vertexIndexBase + vertexOffset + tid];
        const uint vBase      = globalVert * 6u;  // stride: pos(3) + nrm(3)

        const vec3 pos = vec3(uintBitsToFloat(vb[vBase + 0u]),
                              uintBitsToFloat(vb[vBase + 1u]),
                              uintBitsToFloat(vb[vBase + 2u]));

        gl_MeshVerticesEXT[tid].gl_Position = pc.lightViewProj * vec4(pos, 1.0);
    }

    // ── Primitive processing ─────────────────────────────────────────────────
    if (tid < primitiveCount) {
        const uint b0 = (primitiveOffset + tid) * 3u;
        const uint b1 = b0 + 1u;
        const uint b2 = b0 + 2u;

        gl_PrimitiveTriangleIndicesEXT[tid] = uvec3(
            (mb[primitiveBase + b0 / 4u] >> ((b0 % 4u) * 8u)) & 0xFFu,
            (mb[primitiveBase + b1 / 4u] >> ((b1 % 4u) * 8u)) & 0xFFu,
            (mb[primitiveBase + b2 / 4u] >> ((b2 % 4u) * 8u)) & 0xFFu
        );
    }
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// kShadowFrag
//   Minimal fragment shader for depth-only shadow pipelines.
//   No outputs; depth is written implicitly through gl_FragDepth rasterization.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kShadowFrag = R"GLSL(
#version 460
void main() {}
)GLSL";

} // namespace nexus::render::mesh_shader_glsl
