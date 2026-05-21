// SPDX-License-Identifier: MIT
// Copyright (c) 2025 The No-Hands Company.
//
// nexus/render/GBufferMeshShaders.h
// ─────────────────────────────────────────────────────────────────────────────
// Inline GLSL source strings for the GBuffer mesh-shader family.
//
// These constants define the production GBuffer pass contract when the
// geometry is submitted via mesh tasks (VK_EXT_mesh_shader).  Callers are
// responsible for compiling these strings into VkShaderModule objects using
// nexus::gfx::ShaderCompiler and composing the resulting modules into
// VkPipeline objects.
//
// ─── GPU-visible buffer contract ─────────────────────────────────────────────
//
//  Descriptor set 0, binding 0 — vertex storage buffer (std430 readonly)
//    uint vb[];
//
//    Interleaved packed-float vertex data produced by
//    GeometryKernel::MeshUploadContract.  Default assumed stride:
//      • offset  0: position  (3 × float  = 3 uints)
//      • offset 12: normal    (3 × float  = 3 uints)
//    Total: 6 uints (24 bytes) per vertex.  The stride is a compile-time
//    constant in the mesh shaders below; override via specialization constant
//    when additional attributes (tangents, UVs) are present.
//
//  Descriptor set 0, binding 1 — meshlet blob storage buffer (std430 readonly)
//    uint mb[];
//
//    Binary layout produced by GeometryKernel::buildMeshletUploadBlob:
//
//      Header  [0..3] — 4 × uint32:
//        mb[0]  meshletCount
//        mb[1]  vertexIndexCount
//        mb[2]  primitiveByteCount
//        mb[3]  reserved
//
//      Meshlet array  [4 .. 4 + meshletCount*11)
//        Per meshlet: 11 consecutive uint32 values (matches C++ Meshlet
//        struct layout; scalar floats used for bounds to avoid std430
//        vec3 alignment mismatch):
//          mb[mBase + 0]  vertexOffset
//          mb[mBase + 1]  vertexCount
//          mb[mBase + 2]  primitiveOffset
//          mb[mBase + 3]  primitiveCount
//          mb[mBase + 4]  uintBitsToFloat → boundsMin.x
//          mb[mBase + 5]  uintBitsToFloat → boundsMin.y
//          mb[mBase + 6]  uintBitsToFloat → boundsMin.z
//          mb[mBase + 7]  uintBitsToFloat → boundsMax.x
//          mb[mBase + 8]  uintBitsToFloat → boundsMax.y
//          mb[mBase + 9]  uintBitsToFloat → boundsMax.z
//          mb[mBase +10]  lodLevel
//        where  mBase = 4 + meshletIndex * 11
//
//      Vertex index array  [4 + meshletCount*11 .. + vertexIndexCount)
//        uint32, global vertex index per local meshlet vertex slot.
//
//      Primitive byte array  (starts at uint offset 4 + meshletCount*11 + vertexIndexCount)
//        Packed uint8, 3 bytes per triangle: local vertex indices (i0, i1, i2)
//        within the meshlet's vertex slot.
//
// ─── Push constants (128 bytes — Vulkan minimum guaranteed) ──────────────────
//
//    layout(push_constant) uniform PC {
//      mat4 mvp;            // offset  0 (64 bytes) — model-view-projection,
//                           //   column-major, reversed-Z (depth 1→0)
//      mat4 normalMatrix;   // offset 64 (64 bytes) — mat3 extracted from
//                           //   upper-left 3×3 for normal transform
//                           //   (typically world-matrix inverse-transpose)
//    } pc;
//
// ─── GBuffer attachment layout ───────────────────────────────────────────────
//
//    location 0  R16G16B16A16_Float   albedo.rgb  | materialID (encoded as float in .a)
//    location 1  R16G16B16A16_Float   world-space normal.xyz  | roughness in .w
//    location 2  R16G16_Float         screen-space motion vector (.xy)
//
// ─── Dispatch conventions ────────────────────────────────────────────────────
//
//  No-task path  (kGBufferMesh):
//    vkCmdDrawMeshTasksEXT(meshletCount, 1, 1)
//    gl_WorkGroupID.x  == meshlet index
//
//  Task + mesh path  (kGBufferTask + kGBufferMeshWithTask):
//    vkCmdDrawMeshTasksEXT(ceil(meshletCount / 32), 1, 1)
//    The task shader performs sphere-based frustum culling and emits one
//    mesh work group per surviving meshlet.

#pragma once

#include <string_view>

namespace nexus::render::mesh_shader_glsl {

// ─────────────────────────────────────────────────────────────────────────────
// kGBufferTask
//   Task (amplification) shader, local_size_x = 32.
//   Reads meshlet bounds, runs a conservative sphere-vs-frustum clip-space
//   cull, compacts surviving indices into the task payload, and emits the
//   corresponding mesh tasks.
//
//   Dispatch: vkCmdDrawMeshTasksEXT(ceil(meshletCount / 32), 1, 1)
//   Pair with: kGBufferMeshWithTask + kGBufferFrag
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kGBufferTask = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;

// Meshlet blob — same binding as the mesh shader.
layout(set = 0, binding = 1, std430) readonly buffer MeshletBlob { uint mb[]; };

layout(push_constant) uniform PC {
    mat4 mvp;
    mat4 normalMatrix;
} pc;

// Compact list of visible meshlet global indices, written by this workgroup
// and consumed by the mesh shader.
taskPayloadSharedEXT uint visibleMeshletIndices[32];
shared uint s_visibleCount;

void main()
{
    if (gl_LocalInvocationID.x == 0u) s_visibleCount = 0u;
    barrier();

    const uint meshletCount = mb[0u];
    const uint meshletIndex = gl_WorkGroupID.x * 32u + gl_LocalInvocationID.x;

    if (meshletIndex < meshletCount) {
        const uint mBase = 4u + meshletIndex * 11u;
        const vec3 bMin = vec3(uintBitsToFloat(mb[mBase + 4u]),
                               uintBitsToFloat(mb[mBase + 5u]),
                               uintBitsToFloat(mb[mBase + 6u]));
        const vec3 bMax = vec3(uintBitsToFloat(mb[mBase + 7u]),
                               uintBitsToFloat(mb[mBase + 8u]),
                               uintBitsToFloat(mb[mBase + 9u]));

        // Conservative sphere-vs-clip-space frustum test.
        const vec3  center = (bMin + bMax) * 0.5;
        const float radius = length(bMax - center);
        const vec4  clip   = pc.mvp * vec4(center, 1.0);
        const float w      = abs(clip.w) + 1e-4;
        const bool visible = abs(clip.x) <= (w + radius)
                          && abs(clip.y) <= (w + radius)
                          && clip.z      >= -radius
                          && clip.z      <= (w + radius);

        if (visible) {
            const uint slot = atomicAdd(s_visibleCount, 1u);
            visibleMeshletIndices[slot] = meshletIndex;
        }
    }

    barrier();
    EmitMeshTasksEXT(s_visibleCount, 1u, 1u);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// kGBufferMesh
//   Mesh shader for the no-task path.  One workgroup processes one meshlet.
//   gl_WorkGroupID.x is treated directly as the meshlet index.
//   local_size_x = 64 matches the upper meshlet vertex limit (64).
//
//   Dispatch: vkCmdDrawMeshTasksEXT(meshletCount, 1, 1)
//   Pair with: kGBufferFrag
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kGBufferMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require

// local_size_x must cover max_vertices (64) and max_primitives (81 for 64-vertex
// meshlet with approximately 1.27× triangles per vertex is a common upper bound;
// 84 triangles * 3 / 2 ≈ 126 edge constraint → practical cap 81 tris for 64 verts).
layout(local_size_x = 128) in;
layout(triangles, max_vertices = 64, max_primitives = 81) out;

// Per-vertex outputs → interpolated in fragment shader.
layout(location = 0) out vec4 outAlbedoMat[];        // rgb=albedo  a=matID
layout(location = 1) out vec4 outNormalRoughness[];  // rgb=worldNormal*0.5+0.5  a=roughness
layout(location = 2) out vec2 outVelocity[];         // screen-space motion

// Vertex storage buffer: interleaved [pos.xyz, nrm.xyz] per vertex.
// Stride: 6 uints (24 bytes).  See header-level comment for full layout spec.
layout(set = 0, binding = 0, std430) readonly buffer VertexBuffer { uint vb[]; };

// Meshlet blob.  See header-level comment for full binary layout spec.
layout(set = 0, binding = 1, std430) readonly buffer MeshletBlob  { uint mb[]; };

layout(push_constant) uniform PC {
    mat4 mvp;           // model-view-projection (reversed-Z)
    mat4 normalMatrix;  // upper 3x3 used as normal transform matrix
} pc;

void main()
{
    const uint meshletIndex    = gl_WorkGroupID.x;
    const uint meshletCount    = mb[0u];
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

    // Offset of the vertex index sub-array within mb[] (uint units).
    const uint vertexIndexBase = 4u + meshletCount * 11u;
    // Offset of the primitive byte sub-array within mb[] (uint units).
    // Primitive bytes start immediately after the uint32 vertex index array.
    const uint primitiveBase   = vertexIndexBase + vertexIndexCount;

    const uint tid = gl_LocalInvocationID.x;

    // ── Vertex processing ────────────────────────────────────────────────────
    if (tid < vertexCount) {
        const uint globalVert = mb[vertexIndexBase + vertexOffset + tid];
        // Vertex stride: 6 uints = pos(3f) + nrm(3f) = 24 bytes.
        const uint vBase = globalVert * 6u;

        const vec3 pos = vec3(uintBitsToFloat(vb[vBase + 0u]),
                              uintBitsToFloat(vb[vBase + 1u]),
                              uintBitsToFloat(vb[vBase + 2u]));
        const vec3 nrm = vec3(uintBitsToFloat(vb[vBase + 3u]),
                              uintBitsToFloat(vb[vBase + 4u]),
                              uintBitsToFloat(vb[vBase + 5u]));

        gl_MeshVerticesEXT[tid].gl_Position = pc.mvp * vec4(pos, 1.0);

        const vec3 worldNormal = normalize(mat3(pc.normalMatrix) * nrm);
        outAlbedoMat[tid]        = vec4(0.7, 0.7, 0.7, 0.0);        // albedo: gray; matID: 0
        outNormalRoughness[tid]  = vec4(worldNormal * 0.5 + 0.5, 0.5);
        outVelocity[tid]         = vec2(0.0);
    }

    // ── Primitive processing ─────────────────────────────────────────────────
    if (tid < primitiveCount) {
        // Primitive layout: 3 consecutive bytes per triangle (local vertex indices).
        // The byte array starts at primitiveBase in the uint mb[] view.
        // Read each index byte by-address within the packed uint words.
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
// kGBufferMeshWithTask
//   Mesh shader for use with kGBufferTask (task-payload path).
//   Reads its meshlet index from the task payload written by kGBufferTask
//   rather than from gl_WorkGroupID.x directly.
//   SSBO bindings and push constant layout are identical to kGBufferMesh.
//
//   Dispatch (via task): vkCmdDrawMeshTasksEXT(ceil(meshletCount / 32), 1, 1)
//   Pair with: kGBufferTask + kGBufferFrag
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kGBufferMeshWithTask = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 128) in;
layout(triangles, max_vertices = 64, max_primitives = 81) out;

layout(location = 0) out vec4 outAlbedoMat[];
layout(location = 1) out vec4 outNormalRoughness[];
layout(location = 2) out vec2 outVelocity[];

layout(set = 0, binding = 0, std430) readonly buffer VertexBuffer { uint vb[]; };
layout(set = 0, binding = 1, std430) readonly buffer MeshletBlob  { uint mb[]; };

layout(push_constant) uniform PC {
    mat4 mvp;
    mat4 normalMatrix;
} pc;

// Payload written by kGBufferTask: compact list of visible meshlet indices.
taskPayloadSharedEXT uint visibleMeshletIndices[32];

void main()
{
    // gl_WorkGroupID.x is the index into the visible list emitted by the task.
    const uint meshletIndex     = visibleMeshletIndices[gl_WorkGroupID.x];
    const uint meshletCount     = mb[0u];
    const uint vertexIndexCount = mb[1u];

    const uint mBase           = 4u + meshletIndex * 11u;
    const uint vertexOffset    = mb[mBase + 0u];
    const uint vertexCount     = mb[mBase + 1u];
    const uint primitiveOffset = mb[mBase + 2u];
    const uint primitiveCount  = mb[mBase + 3u];

    SetMeshOutputsEXT(vertexCount, primitiveCount);

    const uint vertexIndexBase = 4u + meshletCount * 11u;
    const uint primitiveBase   = vertexIndexBase + vertexIndexCount;

    const uint tid = gl_LocalInvocationID.x;

    if (tid < vertexCount) {
        const uint globalVert = mb[vertexIndexBase + vertexOffset + tid];
        const uint vBase      = globalVert * 6u;

        const vec3 pos = vec3(uintBitsToFloat(vb[vBase + 0u]),
                              uintBitsToFloat(vb[vBase + 1u]),
                              uintBitsToFloat(vb[vBase + 2u]));
        const vec3 nrm = vec3(uintBitsToFloat(vb[vBase + 3u]),
                              uintBitsToFloat(vb[vBase + 4u]),
                              uintBitsToFloat(vb[vBase + 5u]));

        gl_MeshVerticesEXT[tid].gl_Position = pc.mvp * vec4(pos, 1.0);

        const vec3 worldNormal = normalize(mat3(pc.normalMatrix) * nrm);
        outAlbedoMat[tid]        = vec4(0.7, 0.7, 0.7, 0.0);
        outNormalRoughness[tid]  = vec4(worldNormal * 0.5 + 0.5, 0.5);
        outVelocity[tid]         = vec2(0.0);
    }

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
// kGBufferFrag
//   Fragment shader for both mesh paths (kGBufferMesh and kGBufferMeshWithTask).
//   Receives interpolated per-vertex data and writes the three GBuffer
//   attachments.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view kGBufferFrag = R"GLSL(
#version 460

layout(location = 0) in vec4 inAlbedoMat;
layout(location = 1) in vec4 inNormalRoughness;
layout(location = 2) in vec2 inVelocity;

// GBuffer outputs — formats defined by Renderer::ensureGBuffer:
//   attachment 0  R16G16B16A16_Float  albedo.rgb + materialID in .a
//   attachment 1  R16G16B16A16_Float  world-space normal.xyz + roughness in .w
//   attachment 2  R16G16_Float        screen-space motion vector
layout(location = 0) out vec4 outAlbedoMaterial;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec2 outVelocity;

void main()
{
    outAlbedoMaterial = inAlbedoMat;
    outNormal         = inNormalRoughness;
    outVelocity       = inVelocity;
}
)GLSL";

} // namespace nexus::render::mesh_shader_glsl
