#include <gtest/gtest.h>

#include <nexus/geometry/MeshHarmonicField.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using namespace nexus::geometry;

static Mesh makeStripMesh()
{
    auto plane = primitives::makePlane(10.f, 0.1f, 10, 1);
    (void)plane.topology().triangulate();
    return plane;
}

static void constrainEdgesX(const Mesh& mesh, float minVal, float maxVal,
                            HarmonicConstraints& constraints)
{
    const auto& pos = mesh.attributes().positions();
    float minX = 1e9f, maxX = -1e9f;
    for (const auto& p : pos) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    for (size_t i = 0; i < pos.size(); ++i) {
        if (std::fabs(pos[i].x - minX) < 1e-4f) {
            constraints.constrainedVertices.push_back(static_cast<uint32_t>(i));
            constraints.constrainedValues.push_back(minVal);
        } else if (std::fabs(pos[i].x - maxX) < 1e-4f) {
            constraints.constrainedVertices.push_back(static_cast<uint32_t>(i));
            constraints.constrainedValues.push_back(maxVal);
        }
    }
}

TEST(MeshHarmonicField, LinearRampInteriorInterpolates)
{
    auto mesh = makeStripMesh();
    ASSERT_TRUE(mesh.isValid());
    size_t V = mesh.attributes().vertexCount();
    ASSERT_GT(V, 10u);

    HarmonicConstraints constraints;
    constrainEdgesX(mesh, 0.f, 1.f, constraints);
    ASSERT_GT(constraints.constrainedVertices.size(), 0u);

    auto field = MeshHarmonicField::solve(mesh, constraints);
    ASSERT_EQ(field.size(), V);

    for (size_t i = 0; i < V; ++i) {
        bool constrained = false;
        for (size_t c = 0; c < constraints.constrainedVertices.size(); ++c) {
            if (constraints.constrainedVertices[c] == i) {
                EXPECT_NEAR(field[i], constraints.constrainedValues[c], 1e-4f);
                constrained = true;
                break;
            }
        }
        if (!constrained) {
            EXPECT_GE(field[i], -0.1f);
            EXPECT_LE(field[i], 1.1f);
        }
    }
}

TEST(MeshHarmonicField, AllFixedReturnsConstraintValues)
{
    auto mesh = makeStripMesh();
    ASSERT_TRUE(mesh.isValid());
    size_t V = mesh.attributes().vertexCount();
    ASSERT_GT(V, 5u);

    HarmonicConstraints constraints;
    for (uint32_t i = 0; i < static_cast<uint32_t>(V); ++i) {
        constraints.constrainedVertices.push_back(i);
        constraints.constrainedValues.push_back(static_cast<float>(i) + 0.5f);
    }

    auto field = MeshHarmonicField::solve(mesh, constraints);
    ASSERT_EQ(field.size(), V);

    for (size_t i = 0; i < V; ++i) {
        EXPECT_NEAR(field[i], static_cast<float>(i) + 0.5f, 1e-4f);
    }
}

TEST(MeshHarmonicField, ExtremaAreOnBoundary)
{
    auto mesh = makeStripMesh();
    ASSERT_TRUE(mesh.isValid());
    size_t V = mesh.attributes().vertexCount();
    ASSERT_GT(V, 5u);

    HarmonicConstraints constraints;
    constrainEdgesX(mesh, 0.f, 1.f, constraints);
    ASSERT_GT(constraints.constrainedVertices.size(), 0u);

    auto field = MeshHarmonicField::solve(mesh, constraints);
    ASSERT_EQ(field.size(), V);

    float fieldMin = 1e9f, fieldMax = -1e9f;
    float constraintMin = 1e9f, constraintMax = -1e9f;

    for (float v : constraints.constrainedValues) {
        if (v < constraintMin) constraintMin = v;
        if (v > constraintMax) constraintMax = v;
    }

    for (float v : field) {
        if (v < fieldMin) fieldMin = v;
        if (v > fieldMax) fieldMax = v;
    }

    EXPECT_GE(fieldMin, constraintMin - 1e-4f);
    EXPECT_LE(fieldMax, constraintMax + 1e-4f);
}

} // namespace
