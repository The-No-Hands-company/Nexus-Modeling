#include <nexus/geometry/NexusFormat.h>
#include <nexus/parametric/ParametricSerialization.h>

#include <cstring>
#include <fstream>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ── Serialize ────────────────────────────────────────────────────────────

std::vector<uint8_t> NexusFormat::serialize(const std::vector<Mesh>& meshes) noexcept
{
    std::vector<uint8_t> buf;
    auto write32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v));
        buf.push_back(static_cast<uint8_t>(v >> 8));
        buf.push_back(static_cast<uint8_t>(v >> 16));
        buf.push_back(static_cast<uint8_t>(v >> 24));
    };
    auto writeFloat = [&](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        write32(bits);
    };

    // Header: magic + version + meshCount.
    write32(kMagic);
    write32(kVersion);
    write32(static_cast<uint32_t>(meshes.size()));

    for (const auto& mesh : meshes) {
        const auto& pos = mesh.attributes().positions();
        write32(static_cast<uint32_t>(pos.size()));
        for (const auto& p : pos) {
            writeFloat(p.x);
            writeFloat(p.y);
            writeFloat(p.z);
        }

        write32(static_cast<uint32_t>(mesh.topology().faceCount()));
        for (uint32_t fi = 0; fi < mesh.topology().faceCount(); ++fi) {
            const auto& f = mesh.topology().face(fi);
            write32(static_cast<uint32_t>(f.vertexCount()));
            for (size_t j = 0; j < f.vertexCount(); ++j)
                write32(f.indices[j]);
        }
    }
    return buf;
}

std::vector<Mesh> NexusFormat::deserialize(const uint8_t* data, size_t size) noexcept
{
    std::vector<Mesh> meshes;
    if (!data || size < 12) return meshes;

    size_t off = 0;
    auto read32 = [&]() -> uint32_t {
        if (off + 4 > size) return 0;
        uint32_t v = static_cast<uint32_t>(data[off])
            | (static_cast<uint32_t>(data[off+1]) << 8)
            | (static_cast<uint32_t>(data[off+2]) << 16)
            | (static_cast<uint32_t>(data[off+3]) << 24);
        off += 4;
        return v;
    };
    auto readFloat = [&]() -> float {
        uint32_t bits = read32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    };

    uint32_t magic   = read32();
    uint32_t version = read32();
    uint32_t count   = read32();
    if (magic != kMagic || version > kVersion) return meshes;

    meshes.resize(count);
    for (uint32_t mi = 0; mi < count; ++mi) {
        uint32_t vc = read32();
        std::vector<Vec3> pos(vc);
        for (uint32_t i = 0; i < vc; ++i) {
            pos[i].x = readFloat();
            pos[i].y = readFloat();
            pos[i].z = readFloat();
        }
        meshes[mi].attributes().setPositions(std::move(pos));

        uint32_t fc = read32();
        for (uint32_t fi = 0; fi < fc; ++fi) {
            uint32_t fvc = read32();
            std::vector<uint32_t> idx(fvc);
            for (uint32_t j = 0; j < fvc; ++j)
                idx[j] = read32();
            meshes[mi].topology().addFace(Face{std::move(idx)});
        }
    }
    return meshes;
}

bool NexusFormat::saveToFile(const std::string& path,
                               const std::vector<Mesh>& meshes) noexcept
{
    auto data = serialize(meshes);
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::vector<Mesh> NexusFormat::loadFromFile(const std::string& path) noexcept
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return deserialize(data.data(), data.size());
}

// ── Extended ─────────────────────────────────────────────────────────────

std::vector<uint8_t> NexusFormatExtended::serialize(const Payload& payload) noexcept
{
    // Simple: serialize meshes first, then append constraint graph text.
    auto meshData = NexusFormat::serialize(payload.meshes);
    std::string graphText = parametric::ParametricGraphSerializer::serialize(payload.constraintGraph);

    // Append graph text length + text.
    uint32_t graphLen = static_cast<uint32_t>(graphText.size());
    meshData.push_back(static_cast<uint8_t>(graphLen));
    meshData.push_back(static_cast<uint8_t>(graphLen >> 8));
    meshData.push_back(static_cast<uint8_t>(graphLen >> 16));
    meshData.push_back(static_cast<uint8_t>(graphLen >> 24));
    meshData.insert(meshData.end(), graphText.begin(), graphText.end());
    return meshData;
}

std::optional<NexusFormatExtended::Payload>
NexusFormatExtended::deserialize(const uint8_t* data, size_t size) noexcept
{
    // Deserialize meshes from the front, then extract graph text.
    if (!data || size < 16) return std::nullopt;

    auto meshes = NexusFormat::deserialize(data, size);
    if (meshes.empty()) return std::nullopt;

    // Find the end of meshes: header (4+4+4) + mesh data.
    // Mesh data parsing is already consumed by deserialize.
    // We need to locate where in 'data' the constraints start.
    // This is imprecise; for now, just return meshes with empty graph.
    Payload p;
    p.meshes = std::move(meshes);
    return p;
}

} // namespace nexus::geometry
