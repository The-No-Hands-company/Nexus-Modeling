#include <nexus/cad/CadInfrastructure.h>

namespace nexus::cad {

// ── DependencyGraph ────────────────────────────────────────────────
std::vector<DependencyEdge> CadDependencyGraph::build(const CadDocument& doc) noexcept {
    std::vector<DependencyEdge> e;
    for(size_t i=1;i<=doc.history().featureCount();++i){
        auto id=static_cast<parametric::FeatureId>(i);
        auto p=doc.history().parent(id);
        if(p!=parametric::kInvalidFeatureId) e.push_back({p,id,DependencyEdge::Parent});
    }
    return e;
}
std::vector<parametric::FeatureId> CadDependencyGraph::downstream(const CadDocument& doc, parametric::FeatureId fid) noexcept {
    std::vector<parametric::FeatureId> r;
    for(size_t i=1;i<=doc.history().featureCount();++i){
        auto id=static_cast<parametric::FeatureId>(i);
        if(doc.history().parent(id)==fid) r.push_back(id);
    }
    return r;
}
std::vector<parametric::FeatureId> CadDependencyGraph::upstream(const CadDocument& doc, parametric::FeatureId fid) noexcept {
    std::vector<parametric::FeatureId> r; auto p=doc.history().parent(fid);
    while(p!=parametric::kInvalidFeatureId){r.push_back(p);p=doc.history().parent(p);}
    return r;
}
std::vector<parametric::FeatureId> CadDependencyGraph::buildOrder(const CadDocument& doc) noexcept {
    std::vector<parametric::FeatureId> o;
    for(size_t i=1;i<=doc.history().featureCount();++i) o.push_back(static_cast<parametric::FeatureId>(i));
    return o;
}
bool CadDependencyGraph::hasCycles(const CadDocument&) noexcept { return false; }

// ── LayerManager ───────────────────────────────────────────────────
uint32_t CadLayerManager::createLayer(const std::string& n, uint32_t c) { m_layers.push_back({n,c,true,false,{}}); return static_cast<uint32_t>(m_layers.size())-1; }
void CadLayerManager::assignToLayer(uint32_t id, const std::vector<parametric::FeatureId>& fids) { if(id<m_layers.size()) m_layers[id].features.insert(m_layers[id].features.end(),fids.begin(),fids.end()); }
void CadLayerManager::setVisible(uint32_t id, bool v) { if(id<m_layers.size()) m_layers[id].visible=v; }
void CadLayerManager::setLocked(uint32_t id, bool l) { if(id<m_layers.size()) m_layers[id].locked=l; }
const std::vector<CadLayer>& CadLayerManager::layers() const noexcept { return m_layers; }
CadLayer* CadLayerManager::layer(uint32_t id) noexcept { return (id<m_layers.size())?&m_layers[id]:nullptr; }

// ── Bookmarks ──────────────────────────────────────────────────────
void CadBookmarks::save(const std::string& n, size_t d) { m_bookmarks.push_back({n,d}); }
bool CadBookmarks::restore(CadDocument&, const std::string&) { return true; }
std::vector<std::string> CadBookmarks::names() const noexcept { std::vector<std::string> n; for(auto& b:m_bookmarks) n.push_back(b.name); return n; }
void CadBookmarks::clear() { m_bookmarks.clear(); }

// ── XRef ───────────────────────────────────────────────────────────
bool CadXRefManager::attach(const std::string& p, const std::string& n) { m_refs.push_back({p,n}); return true; }
bool CadXRefManager::reload(const std::string& n) { for(auto& r:m_refs) if(r.name==n){r.loaded=true;return true;} return false; }
void CadXRefManager::detach(const std::string& n) { m_refs.erase(std::remove_if(m_refs.begin(),m_refs.end(),[&](auto& r){return r.name==n;}),m_refs.end()); }
bool CadXRefManager::anyModified() const noexcept { for(auto& r:m_refs) if(r.modified) return true; return false; }
const std::vector<CadXRef>& CadXRefManager::refs() const noexcept { return m_refs; }

// ── SketcherDiagnostics ───────────────────────────────────────────
std::vector<SketcherDiagnostic> CadSketcherDiagnostics::analyze(const parametric::ConstraintGraph& g) noexcept {
    std::vector<SketcherDiagnostic> d;
    auto dof=g.analyzeDegreesOfFreedom();
    if(dof.likelyOverconstrained) d.push_back({0,SketcherDiagnostic::OverConstrained,"Over-constrained"});
    else if(dof.estimatedRemainingDOF>0) d.push_back({0,SketcherDiagnostic::UnderConstrained,"Under-constrained"});
    return d;
}
std::unordered_map<parametric::ParametricEntityId,uint32_t> CadSketcherDiagnostics::constraintDegrees(const parametric::ConstraintGraph& g) noexcept {
    std::unordered_map<parametric::ParametricEntityId,uint32_t> deg;
    for(auto& c:g.distanceConstraints()){deg[c.entityA]++;deg[c.entityB]++;}
    return deg;
}
parametric::DegreesOfFreedom CadSketcherDiagnostics::dofSummary(const parametric::ConstraintGraph& g) noexcept { return g.analyzeDegreesOfFreedom(); }
std::string CadSketcherDiagnostics::generateReport(const parametric::ConstraintGraph& g) noexcept {
    auto dof=g.analyzeDegreesOfFreedom();
    return "DOF: "+std::to_string(dof.estimatedRemainingDOF)+(dof.likelyOverconstrained?" (over-constrained)":"");
}

}
