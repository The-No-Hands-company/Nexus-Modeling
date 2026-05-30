#include <nexus/eval/GeometryNodes.h>
#include <nexus/geometry/Mesh.h>

namespace nexus::eval {

bool GeometryTransformNode::compute(
    const nexus::geometry::Mesh& inputGeometry,
    const nexus::render::Transform& transform,
    nexus::geometry::Mesh& outGeometry)
{
    outGeometry = inputGeometry;
    const auto mat = transform.toMatrix();
    return outGeometry.applyTransform(mat);
}

bool GeometryMergeNode::compute(
    const nexus::geometry::Mesh& geometryA,
    const nexus::geometry::Mesh& geometryB,
    nexus::geometry::Mesh& outGeometry)
{
    outGeometry = geometryA;
    return outGeometry.appendMesh(geometryB);
}

bool GeometryBooleanNode::compute(
    const nexus::geometry::Mesh& meshA,
    const nexus::geometry::Mesh& meshB,
    nexus::geometry::BooleanOperationType operation,
    nexus::geometry::Mesh& outGeometry)
{
    const auto report = nexus::geometry::BooleanOperation::compute(meshA, meshB, operation, outGeometry);
    return report.valid;
}

bool GeometryRemeshNode::compute(
    const nexus::geometry::Mesh& input,
    const nexus::geometry::RemeshDesc& desc,
    nexus::geometry::Mesh& outGeometry)
{
    const auto report = nexus::geometry::RemeshOperation::apply(input, desc, outGeometry);
    return report.valid;
}

bool GeometryBevelNode::compute(
    const nexus::geometry::Mesh& input,
    const nexus::geometry::BevelChamferDesc& desc,
    nexus::geometry::Mesh& outGeometry)
{
    const auto report = nexus::geometry::BevelChamferOperation::apply(input, desc, outGeometry);
    return report.valid;
}

bool GeometryInsetNode::compute(
    const nexus::geometry::Mesh& input,
    const nexus::geometry::InsetDesc& desc,
    nexus::geometry::Mesh& outGeometry)
{
    const auto report = nexus::geometry::InsetFacesOperation::applyToAllFaces(input, desc, outGeometry);
    return report.valid;
}

}  // namespace nexus::eval
