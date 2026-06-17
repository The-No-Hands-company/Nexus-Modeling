#include <nexus/cad/CadDesignRule.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/parametric/FeatureHistory.h>

namespace nexus::cad {

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

namespace {
    [[nodiscard]] float computeFaceArea(const Mesh& mesh, uint32_t faceIndex) noexcept {
        const auto& topo = mesh.topology();
        const auto& pos = mesh.attributes().positions();
        if(faceIndex >= topo.faceCount()) return 0.f;
        const auto& face = topo.face(faceIndex);
        if(face.vertexCount() < 3 || !face.indicesInBounds(pos.size())) return 0.f;
        Vec3 n{};
        for(size_t j=0; j+2 < face.vertexCount(); ++j)
            n = n + (pos[face.indices[j+1]] - pos[face.indices[0]])
                    .cross(pos[face.indices[j+2]] - pos[face.indices[0]]);
        return n.length() * 0.5f;
    }
}

void CadDesignChecker::addRule(const DesignRule& r) { m_rules.push_back(r); }

std::vector<DesignRuleViolation> CadDesignChecker::check(const CadDocument& doc) const noexcept {
    std::vector<DesignRuleViolation> violations;
    for(size_t i=1; i<=doc.history().featureCount(); ++i) {
        auto fid = static_cast<parametric::FeatureId>(i);
        auto* node = doc.history().node(fid);
        if(!node || !node->mesh || node->deleted) continue;
        if(!node->mesh->isValid()) {
            violations.push_back({"invalid-mesh", "Feature " + std::to_string(i) + " has invalid mesh",
                                  DesignRuleSeverity::Error, {fid}});
            continue;
        }
        const auto& topo = node->mesh->topology();
        const auto& pos = node->mesh->attributes().positions();
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if(face.vertexCount() < 3)
                violations.push_back({"degenerate-face", "Feature "+std::to_string(i)+" face "+std::to_string(fi)+" <3 verts",
                                      DesignRuleSeverity::Warning, {fid}});
            else if(face.vertexCount() > 64)
                violations.push_back({"large-face", "Feature "+std::to_string(i)+" face "+std::to_string(fi)+" >64 verts",
                                      DesignRuleSeverity::Warning, {fid}});
            if(face.vertexCount() >= 3 && face.indicesInBounds(pos.size())) {
                float area = computeFaceArea(*node->mesh, fi);
                if(area < 1e-8f)
                    violations.push_back({"zero-area", "Feature "+std::to_string(i)+" face "+std::to_string(fi)+" zero area",
                                          DesignRuleSeverity::Warning, {fid}});
            }
        }
    }
    return violations;
}

std::vector<DesignRule> CadDesignChecker::builtinRules() noexcept {
    std::vector<DesignRule> rules;
    for(const auto& def : allDesignRules()) {
        DesignRule r; r.id=def.id; r.description=def.description;
        r.severity=(def.severity==0)?DesignRuleSeverity::Info:(def.severity==1)?DesignRuleSeverity::Warning:(def.severity==2)?DesignRuleSeverity::Error:DesignRuleSeverity::Critical;
        rules.push_back(r);
    }
    return rules;
}
const std::vector<DesignRule>& CadDesignChecker::rules() const noexcept { return m_rules; }
size_t CadDesignChecker::errorCount(const std::vector<DesignRuleViolation>& v) const noexcept {
    size_t c=0; for(auto& x:v) if(x.severity==DesignRuleSeverity::Error||x.severity==DesignRuleSeverity::Critical)c++; return c;
}
size_t CadDesignChecker::warningCount(const std::vector<DesignRuleViolation>& v) const noexcept {
    size_t c=0; for(auto& x:v) if(x.severity==DesignRuleSeverity::Warning)c++; return c;
}

}
