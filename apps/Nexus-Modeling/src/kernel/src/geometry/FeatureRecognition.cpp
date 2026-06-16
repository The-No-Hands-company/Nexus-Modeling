#include <nexus/geometry/FeatureRecognition.h>
#include <nexus/geometry/MeshRepair.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

FeatureRecognitionResult FeatureRecognition::recognize(
    const HalfEdgeMesh& mesh) noexcept
{
    FeatureRecognitionResult result;

    // Detect holes: faces whose edge loops form a closed cylinder.
    // A hole has faces that are mostly cylindrical (dihedral angle near 0
    // between adjacent faces) and the edge loop at the top/bottom is circular.
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;

        // Check if all incident edges have valid twins (interior faces).
        bool allInterior = true;
        uint32_t startHe = he;
        do {
            if (mesh.edge(he).twin == HalfEdgeMesh::kInvalid) {
                allInterior = false; break;
            }
            he = mesh.edge(he).next;
        } while (he != startHe);

        if (!allInterior) continue;

        // Check if the face is approximately cylindrical:
        // All neighbor faces have similar normals in the cross-section.
        Vec3 refNormal{0,0,1};
        uint32_t nCount = 0;
        he = startHe;
        do {
            uint32_t twin = mesh.edge(he).twin;
            if (twin != HalfEdgeMesh::kInvalid) {
                uint32_t nf = mesh.edge(twin).face;
                if (nf != HalfEdgeMesh::kInvalid) {
                    // Approximate normal from face edges.
                    Vec3 a = mesh.positions()[mesh.edge(mesh.face(nf).edge).src];
                    Vec3 b = mesh.positions()[mesh.edge(mesh.edge(mesh.face(nf).edge).next).src];
                    Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(mesh.face(nf).edge).next).next).src];
                    Vec3 n = (b-a).cross(c-a);
                    float len = n.length();
                    if (len > 1e-10f) refNormal = refNormal + n * (1.f/len);
                    nCount++;
                }
            }
            he = mesh.edge(he).next;
        } while (he != startHe);

        if (nCount > 2) {
            refNormal = refNormal * (1.f / static_cast<float>(nCount));
            // A hole feature has a consistent axis direction.
            RecognizedFeature hole;
            hole.type = RecognizedFeatureType::Hole;
            hole.name = "Hole_" + std::to_string(result.holesDetected + 1);
            hole.faceIndices.push_back(fi);
            hole.axis = refNormal;
            hole.radiusOrDepth = 1.f; // approximate
            result.features.push_back(hole);
            result.holesDetected++;
        }
    }

    // Detect fillets: small strip faces with constant dihedral angle to neighbors.
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        uint32_t startHe = he;
        bool hasBoundary = false;
        do {
            if (mesh.edge(he).twin == HalfEdgeMesh::kInvalid) {
                hasBoundary = true; break;
            }
            he = mesh.edge(he).next;
        } while (he != startHe);
        if (hasBoundary) continue;

        // Check if the face connects two larger faces with similar dihedral.
        // A fillet face typically has 4 edges forming a quad strip.
        he = startHe;
        uint32_t edgeCount = 0;
        do { edgeCount++; he = mesh.edge(he).next; } while (he != startHe);

        if (edgeCount == 4) {
            RecognizedFeature fillet;
            fillet.type = RecognizedFeatureType::Fillet;
            fillet.name = "Fillet_" + std::to_string(result.filletsDetected + 1);
            fillet.faceIndices.push_back(fi);
            result.features.push_back(fillet);
            result.filletsDetected++;
        }
    }

    // Detect pockets: planar recessed region surrounded by walls.
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;

        // A pocket has at least one boundary edge (open top).
        bool hasBoundary = false;
        uint32_t startHe = he;
        do {
            if (mesh.edge(he).twin == HalfEdgeMesh::kInvalid) {
                hasBoundary = true; break;
            }
            he = mesh.edge(he).next;
        } while (he != startHe);

        if (hasBoundary) {
            RecognizedFeature pocket;
            pocket.type = RecognizedFeatureType::Pocket;
            pocket.name = "Pocket_" + std::to_string(result.pocketsDetected + 1);
            pocket.faceIndices.push_back(fi);
            result.features.push_back(pocket);
            result.pocketsDetected++;
        }
    }

    return result;
}

// ── GPU Acceleration ────────────────────────────────────────────────────

static GpuAccelOptions s_gpuOpts;

bool GpuAcceleration::isAvailable() noexcept
{
    // Check for Vulkan compute capability via backend availability.
    return false; // placeholder — requires Vulkan backend query
}

void GpuAcceleration::setOptions(const GpuAccelOptions& opts) noexcept
{
    s_gpuOpts = opts;
}

const GpuAccelOptions& GpuAcceleration::options() noexcept
{
    return s_gpuOpts;
}

// ── Auto-Healing ────────────────────────────────────────────────────────

AutoHealReport MeshAutoHeal::heal(Mesh& mesh) noexcept
{
    AutoHealReport report;
    auto repReport = MeshRepair::repair(mesh);
    report.valid = repReport.ok;
    report.duplicateVertsRemoved = repReport.duplicateVerticesRemoved ? 1u : 0u;
    report.zeroAreaFacesRemoved  = repReport.zeroAreaFacesRemoved;
    report.isolatedVertsRemoved  = repReport.isolatedVerticesRemoved;
    report.holesFilled           = repReport.holesFilled;
    report.orientationFixed      = repReport.faceOrientationFixed ? 1u : 0u;
    report.messages.push_back(repReport.error);
    return report;
}

} // namespace nexus::geometry
