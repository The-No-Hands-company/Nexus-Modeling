#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

enum class UnwrapMethod : uint8_t {
    PCA = 0,
    LSCM = 1,
};

struct UnwrapOptions {
    UnwrapMethod method = UnwrapMethod::LSCM;
    bool normalize = true;
    float scale = 1.f;
    uint32_t pinVertexA = 0;
    uint32_t pinVertexB = 0;
};

class MeshUVUnwrap {
public:
    [[nodiscard]] static Mesh unwrap(const Mesh& mesh, const UnwrapOptions& opts = {});

private:
    [[nodiscard]] static Mesh unwrapPCA(const Mesh& mesh, const UnwrapOptions& opts);
    [[nodiscard]] static Mesh unwrapLSCM(const Mesh& mesh, const UnwrapOptions& opts);
};

} // namespace nexus::geometry
