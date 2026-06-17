#include <nexus/app/AppMode.h>
#include <nexus/app/BooleanMode.h>
#include <nexus/app/DimensionMode.h>
#include <nexus/app/EdgeEditMode.h>
#include <nexus/app/ExtrudeMode.h>
#include <nexus/app/FaceEditMode.h>
#include <nexus/app/FilletMode.h>
#include <nexus/app/MirrorMode.h>
#include <nexus/app/PatternMode.h>
#include <nexus/app/PrimitiveModelingMode.h>
#include <nexus/app/RevolveMode.h>
#include <nexus/app/SelectMode.h>
#include <nexus/app/SketchMode.h>
#include <nexus/app/VertexEditMode.h>

namespace nexus::app {

// ── Viewport ──────────────────────────────────────────────────────────

std::string ViewportController::modeName(ViewportMode m) noexcept {
    switch(m){case ViewportMode::Wireframe:return"Wireframe";case ViewportMode::Solid:return"Solid";
    case ViewportMode::Shaded:return"Shaded";case ViewportMode::MaterialPreview:return"MaterialPreview";
    case ViewportMode::Rendered:return"Rendered";case ViewportMode::XRay:return"XRay";
    case ViewportMode::HiddenLine:return"HiddenLine";}return"Solid";
}
void ViewportController::applyToScene(nexus::render::SceneGraph&) noexcept {}

// ── Registry ──────────────────────────────────────────────────────────

ModeRegistry& ModeRegistry::instance() noexcept {
    static ModeRegistry reg;
    return reg;
}

void ModeRegistry::registerMode(const std::string& modeId, Factory factory) {
    m_factories[modeId] = std::move(factory);
}

std::unique_ptr<AppMode> ModeRegistry::create(const std::string& modeId) const {
    auto it = m_factories.find(modeId);
    return (it != m_factories.end()) ? it->second() : nullptr;
}

std::vector<std::string> ModeRegistry::registeredModeIds() const noexcept {
    std::vector<std::string> ids;
    for(const auto&[k,v]:m_factories) ids.push_back(k);
    return ids;
}

bool ModeRegistry::isRegistered(const std::string& modeId) const noexcept {
    return m_factories.find(modeId) != m_factories.end();
}

// ── Orchestrator ──────────────────────────────────────────────────────

ModeOrchestrator::ModeOrchestrator(AppContext& ctx) : m_ctx(ctx) {}

void ModeOrchestrator::registerBuiltinModes() {
    auto& reg = ModeRegistry::instance();
    // Clear any previously registered modes.
    m_modes.clear();
    for(const auto& id : reg.registeredModeIds()) {
        auto mode = reg.create(id);
        if(mode) m_modes.push_back(std::move(mode));
    }
}

bool ModeOrchestrator::switchTo(const std::string& modeId) {
    // Skip if already active.
    if(m_active && m_active->modeId() == modeId) return true;
    for(auto& m : m_modes) {
        if(m->modeId()==modeId){
            if(m_active && !m_active->onDeactivate(m_ctx)) return false; // current mode rejects exit
            m_active=m.get();
            m_active->onActivate(m_ctx);
            return true;
        }
    }
    return false;
}

std::string ModeOrchestrator::activeModeId() const noexcept {
    return m_active?m_active->modeId():"none";
}

bool ModeOrchestrator::executeAction(const std::string& actionId) {
    if(m_active) return m_active->executeAction(actionId, m_ctx);
    for(auto& o : m_overlayStack)
        if(o->executeAction(actionId, m_ctx)) return true;
    return false;
}

void ModeOrchestrator::pushOverlay(std::unique_ptr<AppMode> overlay) {
    overlay->onActivate(m_ctx);
    m_overlayStack.push_back(std::move(overlay));
}

void ModeOrchestrator::popOverlay() {
    if(m_overlayStack.empty())return;
    m_overlayStack.back()->onDeactivate(m_ctx);
    m_overlayStack.pop_back();
}

EventResult ModeOrchestrator::routeInput(const InputEvent& event) {
    for(auto it=m_overlayStack.rbegin();it!=m_overlayStack.rend();++it){
        EventResult r=(*it)->handleInput(m_ctx,event);
        if(r==EventResult::Consumed && (*it)->wantsExclusiveInput()) return EventResult::Consumed;
    }
    if(m_active) return m_active->handleInput(m_ctx,event);
    return EventResult::Unconsumed;
}

void ModeOrchestrator::renderOverlays() {
    if(m_active) m_active->renderOverlay(m_ctx);
    for(auto& o:m_overlayStack) o->renderOverlay(m_ctx);
}

void ModeOrchestrator::renderToolVisuals() {
    if(m_active) m_active->renderToolVisuals(m_ctx);
}

// Mode registration via static constructors — class definitions in separate files
namespace {
    struct AutoRegister {
        AutoRegister() {
            auto& reg = ModeRegistry::instance();
            if(!reg.isRegistered("select"))   reg.registerMode("select",   []{return std::make_unique<SelectMode>();});
            if(!reg.isRegistered("sketch"))   reg.registerMode("sketch",   []{return std::make_unique<SketchMode>();});
            if(!reg.isRegistered("extrude"))  reg.registerMode("extrude",  []{return std::make_unique<ExtrudeMode>();});
            if(!reg.isRegistered("revolve"))  reg.registerMode("revolve",  []{return std::make_unique<RevolveMode>();});
            if(!reg.isRegistered("fillet"))   reg.registerMode("fillet",   []{return std::make_unique<FilletMode>();});
            if(!reg.isRegistered("dimension"))reg.registerMode("dimension",[]{return std::make_unique<DimensionMode>();});
            if(!reg.isRegistered("modeling")) reg.registerMode("modeling", []{return std::make_unique<PrimitiveModelingMode>();});
            if(!reg.isRegistered("face-edit")) reg.registerMode("face-edit", []{return std::make_unique<FaceEditMode>();});
            if(!reg.isRegistered("edge-edit")) reg.registerMode("edge-edit", []{return std::make_unique<EdgeEditMode>();});
            if(!reg.isRegistered("vertex-edit")) reg.registerMode("vertex-edit", []{return std::make_unique<VertexEditMode>();});
            if(!reg.isRegistered("boolean")) reg.registerMode("boolean", []{return std::make_unique<BooleanMode>();});
            if(!reg.isRegistered("pattern")) reg.registerMode("pattern", []{return std::make_unique<PatternMode>();});
            if(!reg.isRegistered("mirror")) reg.registerMode("mirror", []{return std::make_unique<MirrorMode>();});
        }
    } autoReg;
}

} // namespace nexus::app
