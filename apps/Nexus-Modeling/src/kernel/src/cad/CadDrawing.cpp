#include <nexus/cad/CadDrawing.h>
#include <nexus/geometry/MeshAnalysis.h>

#include <algorithm>
#include <sstream>

namespace nexus::cad {

// ── Drawing Views ─────────────────────────────────────────────────────────

std::vector<DrawingView> CadDrawing::generateStandardViews(
    const CadDocument& doc) noexcept
{
    (void)doc;
    std::vector<DrawingView> views;
    views.push_back({DrawingViewType::Front, {0,0,10}, {0,0,0}, {0,1,0}, 1.0f, "Front", {}});
    views.push_back({DrawingViewType::Top, {0,10,0}, {0,0,0}, {0,0,-1}, 1.0f, "Top", {}});
    views.push_back({DrawingViewType::Right, {10,0,0}, {0,0,0}, {0,1,0}, 1.0f, "Right", {}});
    views.push_back({DrawingViewType::Isometric, {7,5,7}, {0,0,0}, {0,1,0}, 1.0f, "Isometric", {}});
    return views;
}

DrawingView CadDrawing::generateSectionView(
    const CadDocument& doc, const Vec3& planePoint, const Vec3& planeNormal) noexcept
{
    (void)doc;
    DrawingView view;
    view.type = DrawingViewType::Section;
    view.cameraPosition = planePoint + planeNormal.normalize() * 10.f;
    view.target = planePoint;
    view.label = "A-A";
    return view;
}

geometry::Mesh CadDrawing::exportViewsAsMesh(
    const std::vector<DrawingView>& views) noexcept
{
    (void)views;
    return {};
}

// ── BOM ──────────────────────────────────────────────────────────────────

std::vector<BomItem> CadBOM::generate(const CadAssembly& assembly) noexcept
{
    std::vector<BomItem> items;
    for (size_t i = 0; i < assembly.partCount(); ++i) {
        auto* part = assembly.part(static_cast<uint32_t>(i));
        if (!part) continue;
        BomItem item;
        item.index = static_cast<uint32_t>(i + 1);
        item.partNumber = part->name();
        item.description = "Part";
        item.quantity = 1;
        items.push_back(item);
    }
    return items;
}

std::string CadBOM::exportCSV(const std::vector<BomItem>& items) noexcept
{
    std::ostringstream os;
    os << "Index,Part Number,Description,Material,Quantity,Mass\n";
    for (const auto& item : items) {
        os << item.index << "," << item.partNumber << ","
           << item.description << "," << item.material << ","
           << item.quantity << "," << item.mass << "\n";
    }
    return os.str();
}

std::string CadBOM::exportJSON(const std::vector<BomItem>& items) noexcept
{
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) os << ",";
        os << "{\"index\":" << items[i].index
           << ",\"partNumber\":\"" << items[i].partNumber
           << "\",\"quantity\":" << items[i].quantity << "}";
    }
    os << "]";
    return os.str();
}

// ── Configuration ────────────────────────────────────────────────────────

void CadConfiguration::addParameter(const ConfigurationParameter& param)
{ m_params.push_back(param); }

Configuration CadConfiguration::createConfig(
    const std::string& name,
    const std::vector<std::pair<std::string, double>>& values) const noexcept
{
    Configuration config;
    config.name = name;
    config.values = values;
    return config;
}

bool CadConfiguration::apply(const Configuration& config,
                               CadDocument& doc) const noexcept
{
    (void)config; (void)doc;
    return true;
}

const std::vector<ConfigurationParameter>& CadConfiguration::parameters() const noexcept
{ return m_params; }

// ── Interference ─────────────────────────────────────────────────────────

InterferenceResult CadInterference::detect(const CadAssembly& assembly) noexcept
{
    InterferenceResult result;
    for (size_t i = 0; i < assembly.partCount(); ++i) {
        for (size_t j = i + 1; j < assembly.partCount(); ++j) {
            auto* pa = assembly.part(static_cast<uint32_t>(i));
            auto* pb = assembly.part(static_cast<uint32_t>(j));
            if (!pa || !pb) continue;
            auto meshA = pa->combinedMesh();
            auto meshB = pb->combinedMesh();
            auto collision = meshMeshCollision(meshA, meshB);
            if (collision.colliding) {
                result.interference = true;
                result.interferingParts.emplace_back(
                    static_cast<uint32_t>(i), static_cast<uint32_t>(j));
            }
        }
    }
    return result;
}

InterferenceResult CadInterference::detectBetween(
    const CadPart& partA, const CadPart& partB) noexcept
{
    InterferenceResult result;
    auto collision = meshMeshCollision(partA.combinedMesh(), partB.combinedMesh());
    result.interference = collision.colliding;
    return result;
}

// ── Render Modes ─────────────────────────────────────────────────────────

void CadRenderModes::applyStyle(const CadRenderStyle& style,
                                  CadDocument& doc) noexcept
{ (void)style; (void)doc; }

geometry::Mesh CadRenderModes::extractEdges(const CadDocument& doc) noexcept
{
    (void)doc;
    return {};
}

geometry::Mesh CadRenderModes::generateHiddenLine(
    const CadDocument& doc, const Vec3& viewDirection) noexcept
{
    (void)doc; (void)viewDirection;
    return {};
}

CadRenderStyle CadRenderModes::defaultStyle() noexcept
{ return {CadRenderMode::Shaded, {0,0,0}, {0.7f,0.7f,0.7f}, 1.0f, 0.0f}; }

} // namespace nexus::cad
