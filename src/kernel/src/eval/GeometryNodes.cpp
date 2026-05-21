#include <nexus/eval/GeometryNodes.h>
#include <nexus/geometry/Mesh.h>

namespace nexus::eval {

bool GeometryTransformNode::compute(
    const nexus::geometry::Mesh& inputGeometry,
    const nexus::render::Transform& transform,
    nexus::geometry::Mesh& outGeometry)
{
    // Start from an exact input copy, then apply the transform in-place.
    outGeometry = inputGeometry;

    // Compute the transformation matrix from the transform
    const auto mat = transform.toMatrix();

    // Apply the transform via the mesh's applyTransform contract
    return outGeometry.applyTransform(mat);
}

bool GeometryMergeNode::compute(
    const nexus::geometry::Mesh& geometryA,
    const nexus::geometry::Mesh& geometryB,
    nexus::geometry::Mesh& outGeometry)
{
    // Deterministic merge: start with A, then append B
    outGeometry = geometryA;
    return outGeometry.appendMesh(geometryB);
}

}  // namespace nexus::eval
