#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/Mesh.h>

#include <algorithm>
#include <cstring>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

CadDocument::CadDocument() = default;
CadDocument::~CadDocument() = default;

parametric::FeatureId CadDocument::addSketch(parametric::SketchProfile sketch)
{
    auto cmd = std::make_unique<AddSketchCommand>(std::move(sketch));
    if (!executeCommand(std::move(cmd))) return parametric::kInvalidFeatureId;
    return static_cast<parametric::FeatureId>(m_history.featureCount());
}

parametric::FeatureId CadDocument::addExtrude(parametric::FeatureId sketchId,
                                               geometry::CurveExtrudeDesc desc)
{
    auto cmd = std::make_unique<AddExtrudeCommand>(sketchId, desc);
    if (!executeCommand(std::move(cmd))) return parametric::kInvalidFeatureId;
    return static_cast<parametric::FeatureId>(m_history.featureCount());
}

parametric::FeatureId CadDocument::addRevolve(parametric::FeatureId sketchId,
                                               geometry::RevolveDesc desc)
{
    auto cmd = std::make_unique<AddRevolveCommand>(sketchId, desc);
    if (!executeCommand(std::move(cmd))) return parametric::kInvalidFeatureId;
    return static_cast<parametric::FeatureId>(m_history.featureCount());
}

bool CadDocument::deleteFeature(parametric::FeatureId id)
{
    auto cmd = std::make_unique<DeleteFeatureCommand>(id);
    return executeCommand(std::move(cmd));
}

bool CadDocument::executeCommand(std::unique_ptr<CadCommand> cmd)
{
    if (!cmd->execute(*this)) return false;
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear();
    markModified();
    return true;
}

bool CadDocument::undo()
{
    if (m_undoStack.empty()) return false;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->undo(*this);
    m_redoStack.push_back(std::move(cmd));
    markModified();
    return true;
}

bool CadDocument::redo()
{
    if (m_redoStack.empty()) return false;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->execute(*this);
    m_undoStack.push_back(std::move(cmd));
    markModified();
    return true;
}

void CadDocument::setActiveTool(CadTool tool)
{
    m_state.activeTool = tool;
    m_selection.clear();
}

std::vector<std::string> CadDocument::undoDescriptions() const noexcept {
    std::vector<std::string> descs;
    for(auto& cmd : m_undoStack) descs.push_back(cmd->description());
    std::reverse(descs.begin(), descs.end());
    return descs;
}
std::vector<std::string> CadDocument::redoDescriptions() const noexcept {
    std::vector<std::string> descs;
    for(auto& cmd : m_redoStack) descs.push_back(cmd->description());
    return descs;
}

// ── Serialization ──────────────────────────────────────────────────────────

static void write32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v>>8));
    out.push_back(static_cast<uint8_t>(v>>16));
    out.push_back(static_cast<uint8_t>(v>>24));
}
static void writeFloat(std::vector<uint8_t>& out, float v) {
    uint32_t raw; std::memcpy(&raw, &v, 4);
    write32(out, raw);
}
static uint32_t read32(const uint8_t*& p, const uint8_t* end) {
    if(p + 4 > end) return 0;
    uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1])<<8)
               | (static_cast<uint32_t>(p[2])<<16) | (static_cast<uint32_t>(p[3])<<24);
    p += 4; return v;
}
static float readFloat(const uint8_t*& p, const uint8_t* end) {
    uint32_t raw = read32(p, end); float v; std::memcpy(&v, &raw, 4); return v;
}

std::vector<uint8_t> CadDocument::serialize() const noexcept
{
    std::vector<uint8_t> data;
    data.push_back('N'); data.push_back('X'); data.push_back('D'); data.push_back('0');
    write32(data, 1); // version
    write32(data, static_cast<uint32_t>(m_history.featureCount()));

    for(size_t idx=1; idx<=m_history.featureCount(); ++idx) {
        auto* node = m_history.node(static_cast<parametric::FeatureId>(idx));
        if(!node) continue;
        write32(data, node->id);
        write32(data, static_cast<uint32_t>(node->kind));
        uint16_t nLen = static_cast<uint16_t>(std::min(node->name.size(), size_t(65535)));
        data.push_back(static_cast<uint8_t>(nLen));
        data.push_back(static_cast<uint8_t>(nLen>>8));
        for(uint16_t i=0; i<nLen; ++i) data.push_back(static_cast<uint8_t>(node->name[i]));
        write32(data, node->parent);
        data.push_back(node->deleted ? 1 : 0);
        data.push_back(node->hidden ? 1 : 0);
        writeFloat(data, node->material.albedo[0]);
        writeFloat(data, node->material.albedo[1]);
        writeFloat(data, node->material.albedo[2]);
        writeFloat(data, node->material.albedo[3]);
        writeFloat(data, node->material.roughness);
        writeFloat(data, node->material.metallic);
        data.push_back(node->mesh.has_value() ? 1 : 0);
        if(node->mesh) {
            const auto& pos = node->mesh->attributes().positions();
            const auto& topo = node->mesh->topology();
            write32(data, static_cast<uint32_t>(pos.size()));
            for(const auto& v : pos) { writeFloat(data, v.x); writeFloat(data, v.y); writeFloat(data, v.z); }
            write32(data, static_cast<uint32_t>(topo.faceCount()));
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                write32(data, static_cast<uint32_t>(face.indices.size()));
                for(auto idx : face.indices) write32(data, idx);
            }
        }
    }
    return data;
}

bool CadDocument::deserialize(const uint8_t* d, size_t size) noexcept
{
    if(!d || size < 8) return false;
    if(d[0]!='N'||d[1]!='X'||d[2]!='D'||d[3]!='0') return false;
    const uint8_t* p = d + 4;
    const uint8_t* end = d + size;
    uint32_t version = read32(p, end);
    (void)version;
    uint32_t fc = read32(p, end);
    const uint32_t kMaxFeatures = 1000000;
    if(fc > kMaxFeatures) return false;

    // Clear existing state.
    while(m_history.featureCount() > 0) {
        m_history.removeFeature(static_cast<parametric::FeatureId>(m_history.featureCount()));
    }
    m_history.unhideAll();
    m_undoStack.clear(); m_redoStack.clear();

    for(uint32_t idx=0; idx<fc; ++idx) {
        (void)read32(p, end); // id (recreated below)
        uint32_t kindRaw = read32(p, end);
        uint16_t nLen = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1])<<8);
        p += 2; if(p + nLen > end) return false;
        std::string name(reinterpret_cast<const char*>(p), nLen); p += nLen;
        uint32_t parent = read32(p, end);
        bool deleted = (*p++) != 0;
        bool hidden = (*p++) != 0;
        parametric::FeatureNode::Material mat;
        mat.albedo[0] = readFloat(p, end); mat.albedo[1] = readFloat(p, end);
        mat.albedo[2] = readFloat(p, end); mat.albedo[3] = readFloat(p, end);
        mat.roughness = readFloat(p, end); mat.metallic = readFloat(p, end);
        uint8_t hasMesh = *p++;

        parametric::SketchProfile sk;
        parametric::FeatureId fid;
        auto kind = static_cast<parametric::FeatureKind>(kindRaw);
        if(kind == parametric::FeatureKind::Sketch)
            fid = m_history.addSketch(sk);
        else if(kind == parametric::FeatureKind::Extrude)
            fid = m_history.addExtrude(sk);
        else if(kind == parametric::FeatureKind::Revolve)
            fid = m_history.addRevolve(sk);
        else
            fid = m_history.addSketch(sk);

        auto* node = m_history.node(fid);
        if(!node) continue;
        node->name = std::move(name);
        node->parent = static_cast<parametric::FeatureId>(parent);
        node->material = mat;
        node->deleted = deleted;
        node->hidden = hidden;
        node->dirty = false;

        if(hasMesh && p < end) {
            uint32_t vc = read32(p, end);
            const uint32_t kMaxVerts = 100000000;
            if(vc > kMaxVerts) continue;
            std::vector<Vec3> pos(vc);
            for(uint32_t i=0; i<vc; ++i) {
                pos[i].x = readFloat(p, end); pos[i].y = readFloat(p, end); pos[i].z = readFloat(p, end);
            }
            uint32_t faceCount = read32(p, end);
            const uint32_t kMaxFaces = 100000000;
            if(faceCount > kMaxFaces) continue;
            node->mesh.emplace();
            node->mesh->attributes().setPositions(std::move(pos));
            for(uint32_t fi=0; fi<faceCount; ++fi) {
                uint32_t ic = read32(p, end);
                if(ic < 3 || ic > 256) { for(uint32_t j=0;j<ic;++j) read32(p,end); continue; }
                std::vector<uint32_t> idx(ic);
                for(uint32_t j=0; j<ic; ++j) idx[j] = read32(p, end);
                geometry::Face face; face.indices = std::move(idx);
                node->mesh->topology().addFace(std::move(face));
            }
        }
    }
    return true;
}

} // namespace nexus::cad
