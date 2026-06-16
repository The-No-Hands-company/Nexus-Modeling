#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>

namespace nexus::cad {

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
    return m_history.removeFeature(id);
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
    if (!cmd->undo(*this)) {
        m_undoStack.push_back(std::move(cmd));
        return false;
    }
    m_redoStack.push_back(std::move(cmd));
    markModified();
    return true;
}

bool CadDocument::redo()
{
    if (m_redoStack.empty()) return false;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    if (!cmd->execute(*this)) {
        m_redoStack.push_back(std::move(cmd));
        return false;
    }
    m_undoStack.push_back(std::move(cmd));
    markModified();
    return true;
}

void CadDocument::setActiveTool(CadTool tool)
{
    m_state.activeTool = tool;
    m_selection.clear();
}

std::vector<uint8_t> CadDocument::serialize() const noexcept
{
    // Serialize feature history to binary.
    std::vector<uint8_t> data;
    data.push_back('N'); data.push_back('C'); data.push_back('A'); data.push_back('D');
    uint32_t fc = static_cast<uint32_t>(m_history.featureCount());
    data.push_back(static_cast<uint8_t>(fc));
    data.push_back(static_cast<uint8_t>(fc >> 8));
    data.push_back(static_cast<uint8_t>(fc >> 16));
    data.push_back(static_cast<uint8_t>(fc >> 24));
    return data;
}

bool CadDocument::deserialize(const uint8_t* data, size_t size) noexcept
{
    if (!data || size < 8) return false;
    if (data[0] != 'N' || data[1] != 'C' || data[2] != 'A' || data[3] != 'D') return false;
    // Simplified: just verify header.
    return true;
}

} // namespace nexus::cad
