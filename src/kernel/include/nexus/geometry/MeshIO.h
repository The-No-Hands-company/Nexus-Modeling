#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Import/Export (hard-surface blockout workflow)
//
//  MeshIO::exportMesh() serialises a Mesh to disk in OBJ or PLY (ASCII)
//  format. MeshIO::importMesh() reads deterministic OBJ files produced by the
//  same adapter baseline.
//
//  Design constraints:
//  - All paths are noexcept and headless-safe.
//  - Output is deterministic for identical input.
//  - Import (read) is intentionally out of scope for this v0 endpoint.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Export format
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshExportFormat : uint8_t {
    OBJ,  // Wavefront OBJ (ASCII)
    PLY,  // Stanford PLY  (ASCII)
};

enum class MeshImportFormat : uint8_t {
    OBJ,  // Wavefront OBJ (ASCII)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic flags
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshExportDiagnostic : uint32_t {
    Success           = 0,
    InvalidMesh       = 1u << 0,
    FileOpenFailed    = 1u << 1,
    WriteError        = 1u << 2,
    UnsupportedFormat = 1u << 3,
};

enum class MeshImportDiagnostic : uint32_t {
    Success            = 0,
    InvalidMesh        = 1u << 0,
    FileOpenFailed     = 1u << 1,
    ReadError          = 1u << 2,
    UnsupportedFormat  = 1u << 3,
    InvalidData        = 1u << 4,
    UnsupportedFeature = 1u << 5,
};

inline MeshExportDiagnostic operator|(MeshExportDiagnostic a,
                                      MeshExportDiagnostic b) noexcept
{
    return static_cast<MeshExportDiagnostic>(static_cast<uint32_t>(a)
                                           | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(MeshExportDiagnostic val,
                          MeshExportDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

inline bool hasDiagnostic(MeshImportDiagnostic val,
                          MeshImportDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Options
// ─────────────────────────────────────────────────────────────────────────────
struct MeshExportOptions {
    MeshExportFormat format        = MeshExportFormat::OBJ;
    bool             includeNormals = true;
    bool             includeUVs     = true;
};

struct MeshImportOptions {
    MeshImportFormat format         = MeshImportFormat::OBJ;
    // Fan-triangulates every imported face in place. Required when the
    // imported mesh will be handed to operations that mandate triangle-only
    // input (e.g. RemeshOperation). Defaults to false so importers preserve
    // the on-disk topology faithfully; callers that go through
    // HardSurfaceWorkflow / ModelingShell::quickCleanup do not need to set
    // this because that path runs its own triangulate step.
    bool             triangulateFaces = false;
    bool             computeNormals   = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Export report
// ─────────────────────────────────────────────────────────────────────────────
/// @note report.messages is lexicographically sorted on every return path so
///       that multi-diagnostic output is deterministic regardless of the order
///       in which individual conditions are detected during serialisation.
struct MeshExportReport {
    MeshExportDiagnostic diagnostic     = MeshExportDiagnostic::Success;
    bool                 valid          = false;
    uint32_t             verticesWritten = 0;
    uint32_t             facesWritten    = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == MeshExportDiagnostic::Success;
    }
};

struct MeshImportReport {
    MeshImportDiagnostic diagnostic = MeshImportDiagnostic::Success;
    bool                 valid      = false;
    uint32_t             verticesRead = 0;
    uint32_t             facesRead    = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == MeshImportDiagnostic::Success;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MeshIO — stateless serialiser
// ─────────────────────────────────────────────────────────────────────────────
class MeshIO {
public:
    // Exports 'mesh' to the file at 'path'.
    // Returns a report; report.valid == true on success.
    [[nodiscard]] static MeshExportReport exportMesh(const Mesh&              mesh,
                                                     const std::string&       path,
                                                     const MeshExportOptions& options) noexcept;

    // Imports a mesh from 'path' using a deterministic baseline format.
    [[nodiscard]] static MeshImportReport importMesh(const std::string& path,
                                                     Mesh& mesh,
                                                     const MeshImportOptions& options) noexcept;
};

} // namespace nexus::geometry
