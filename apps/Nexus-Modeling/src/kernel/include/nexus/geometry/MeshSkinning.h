#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

enum class SkinningMethod : uint8_t {
    LBS,  // Linear Blend Skinning (default)
    DQS,  // Dual Quaternion Skinning
};

struct DualQuat {
    nexus::render::Vec4 real;  // rotation quaternion (x, y, z, w)
    nexus::render::Vec4 dual;  // dual part

    [[nodiscard]] static DualQuat fromTransform(const nexus::render::Mat4& mat) noexcept;
};

struct SkinningOptions {
    SkinningMethod method = SkinningMethod::LBS;
};

class MeshSkinning {
public:
    [[nodiscard]] static Mesh deform(const Mesh& mesh,
                                      const std::vector<nexus::render::Mat4>& boneTransforms,
                                      const SkinningOptions& options = {});

    [[nodiscard]] static Mesh deformDQS(const Mesh& mesh,
                                         const std::vector<nexus::render::Mat4>& boneTransforms);
};

} // namespace nexus::geometry
