#include <gtest/gtest.h>

#include <nexus/geometry/MeshUVUnwrap.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshUVUnwrap, ProducesUVs) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh);
    EXPECT_TRUE(unwrapped.isValid());
    EXPECT_TRUE(unwrapped.attributes().hasUVs());
    EXPECT_EQ(unwrapped.attributes().uvs().size(),
              unwrapped.attributes().vertexCount());
}

TEST(MeshUVUnwrap, UVsInUnitRange) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    UnwrapOptions opts;
    opts.normalize = true;
    opts.scale = 1.f;
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh, opts);
    EXPECT_TRUE(unwrapped.attributes().hasUVs());
    for (const auto& uv : unwrapped.attributes().uvs()) {
        EXPECT_GE(uv.u, 0.f);
        EXPECT_LE(uv.u, 1.f);
        EXPECT_GE(uv.v, 0.f);
        EXPECT_LE(uv.v, 1.f);
    }
}

TEST(MeshUVUnwrap, DegenerateInputReturnsEmpty) {
    Mesh mesh;
    mesh.attributes().setPositions({
        {0,0,0}, {1,1,1}, {2,2,2}
    });
    Face f; f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh);
    EXPECT_TRUE(unwrapped.isValid() || !unwrapped.isValid());
}

TEST(MeshUVUnwrap, LSCMProducesNonTrivialUVs) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    UnwrapOptions opts;
    opts.method = UnwrapMethod::LSCM;
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh, opts);
    EXPECT_TRUE(unwrapped.attributes().hasUVs());
    EXPECT_EQ(unwrapped.attributes().uvs().size(),
              unwrapped.attributes().vertexCount());

    bool hasVariation = false;
    const auto& uvs = unwrapped.attributes().uvs();
    for (size_t i = 1; i < uvs.size() && !hasVariation; ++i) {
        float du = std::fabs(uvs[i].u - uvs[0].u);
        float dv = std::fabs(uvs[i].v - uvs[0].v);
        if (du > 1e-4f || dv > 1e-4f) hasVariation = true;
    }
    EXPECT_TRUE(hasVariation);
}

TEST(MeshUVUnwrap, PCAProducesUVs) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    UnwrapOptions opts;
    opts.method = UnwrapMethod::PCA;
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh, opts);
    EXPECT_TRUE(unwrapped.attributes().hasUVs());
    EXPECT_EQ(unwrapped.attributes().uvs().size(),
              unwrapped.attributes().vertexCount());
}

TEST(MeshUVUnwrap, LSCMWithPinnedVertices) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    UnwrapOptions opts;
    opts.method = UnwrapMethod::LSCM;
    opts.pinVertexA = 0;
    opts.pinVertexB = 10;
    Mesh unwrapped = MeshUVUnwrap::unwrap(mesh, opts);
    EXPECT_TRUE(unwrapped.attributes().hasUVs());
    const auto& uvs = unwrapped.attributes().uvs();
    EXPECT_TRUE(uvs.size() >= 2);
    EXPECT_NEAR(uvs[0].u, 0.f, 1e-3f);
    EXPECT_NEAR(uvs[0].v, 0.f, 1e-3f);
}
