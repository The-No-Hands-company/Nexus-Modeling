#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Format — native .nxm binary interchange format
//
//  Binary format for fast save/load of meshes, constraint graphs, and
//  feature histories within the Nexus Modeling ecosystem.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/parametric/ConstraintGraph.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexus::geometry {

class NexusFormat {
public:
    static constexpr uint32_t kMagic   = 0x4D58454E; // "NEXM"
    static constexpr uint32_t kVersion = 1;

    // Serialize a set of meshes to a binary buffer.
    [[nodiscard]] static std::vector<uint8_t> serialize(
        const std::vector<Mesh>& meshes) noexcept;

    // Deserialize back to meshes.
    [[nodiscard]] static std::vector<Mesh> deserialize(
        const uint8_t* data, size_t size) noexcept;

    // Save/load to file.
    [[nodiscard]] static bool saveToFile(const std::string& path,
                                          const std::vector<Mesh>& meshes) noexcept;
    [[nodiscard]] static std::vector<Mesh> loadFromFile(
        const std::string& path) noexcept;
};

// ──────────── Extended Nexus Format (w/ parametric data) ─────────────────

class NexusFormatExtended {
public:
    struct Payload {
        std::vector<Mesh> meshes;
        parametric::ConstraintGraph constraintGraph;
    };

    [[nodiscard]] static std::vector<uint8_t> serialize(
        const Payload& payload) noexcept;
    [[nodiscard]] static std::optional<Payload> deserialize(
        const uint8_t* data, size_t size) noexcept;
};

} // namespace nexus::geometry
