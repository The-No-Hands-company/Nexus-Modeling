#include <nexus/cad/CadCommand.h>

namespace nexus::cad {

// ── AddSketchCommand ─────────────────────────────────────────────────────

AddSketchCommand::AddSketchCommand(parametric::SketchProfile sketch)
    : m_sketch(std::move(sketch))
{}

std::string AddSketchCommand::description() const
{
    return "Add Sketch";
}

bool AddSketchCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addSketch(m_sketch);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().rebuild();
    return true;
}

bool AddSketchCommand::undo(CadDocument& doc)
{
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    // Features are not individually removable yet — just mark dirty.
    (void)doc;
    m_executed = false;
    return true;
}

// ── AddExtrudeCommand ────────────────────────────────────────────────────

AddExtrudeCommand::AddExtrudeCommand(parametric::FeatureId sketchId,
                                       geometry::CurveExtrudeDesc desc)
    : m_sketchId(sketchId), m_desc(desc)
{}

std::string AddExtrudeCommand::description() const
{
    return "Extrude";
}

bool AddExtrudeCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addExtrude(
        parametric::SketchProfile{}, m_desc);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().setDirection(m_createdId, m_desc.direction);
    doc.history().setHeight(m_createdId, m_desc.height);
    doc.history().rebuild();
    return true;
}

bool AddExtrudeCommand::undo(CadDocument& doc)
{
    (void)doc;
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = false;
    return true;
}

// ── AddRevolveCommand ────────────────────────────────────────────────────

AddRevolveCommand::AddRevolveCommand(parametric::FeatureId sketchId,
                                       geometry::RevolveDesc desc)
    : m_sketchId(sketchId), m_desc(desc)
{}

std::string AddRevolveCommand::description() const
{
    return "Revolve";
}

bool AddRevolveCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addRevolve(
        parametric::SketchProfile{}, m_desc);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().setAxis(m_createdId, m_desc.axisOrigin, m_desc.axisDirection);
    doc.history().setCapEnds(m_createdId, m_desc.capEnds);
    doc.history().rebuild();
    return true;
}

bool AddRevolveCommand::undo(CadDocument& doc)
{
    (void)doc;
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = false;
    return true;
}

// ── SetHeightCommand ─────────────────────────────────────────────────────

SetHeightCommand::SetHeightCommand(parametric::FeatureId id,
                                     float oldHeight, float newHeight)
    : m_featureId(id), m_oldHeight(oldHeight), m_newHeight(newHeight)
{}

std::string SetHeightCommand::description() const
{
    return "Set Height";
}

bool SetHeightCommand::execute(CadDocument& doc)
{
    if (!doc.history().setHeight(m_featureId, m_newHeight)) return false;
    m_executed = true;
    doc.history().rebuild();
    return true;
}

bool SetHeightCommand::undo(CadDocument& doc)
{
    doc.history().setHeight(m_featureId, m_oldHeight);
    doc.history().rebuild();
    m_executed = false;
    return true;
}

} // namespace nexus::cad
