#include <gtest/gtest.h>

#include <nexus/geometry/MeshSelfIntersection.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshSelfIntersection, CleanQuadNoIntersections) {
    Mesh mesh = makePlane(2.f, 2.f, 1, 1);
    SelfIntersectionResult r = MeshSelfIntersection::detect(mesh);
    EXPECT_TRUE(r.intersections.empty());
}

TEST(MeshSelfIntersection, OverlappingFacesDetected) {
    Mesh mesh;
    mesh.attributes().setPositions({
        {0,0,0}, {2,0,0}, {0,0,2},
        {0.5f, 0, 0.5f}, {1.5f, 0, 0.5f}, {0.5f, 0, 1.5f}
    });
    Face f1; f1.indices = {0, 1, 2};
    Face f2; f2.indices = {3, 4, 5};
    mesh.topology().addFace(f1);
    mesh.topology().addFace(f2);
    SelfIntersectionResult r = MeshSelfIntersection::detect(mesh);
    EXPECT_GT(r.intersections.size(), 0u);
}

TEST(MeshSelfIntersection, EmptyMeshOk) {
    Mesh mesh;
    SelfIntersectionResult r = MeshSelfIntersection::detect(mesh);
    EXPECT_TRUE(r.intersections.empty());
}

TEST(MeshSelfIntersection, SingleTriangleOk) {
    Mesh mesh;
    mesh.attributes().setPositions({{0,0,0}, {1,0,0}, {0,0,1}});
    Face f; f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    SelfIntersectionResult r = MeshSelfIntersection::detect(mesh);
    EXPECT_TRUE(r.intersections.empty());
}
