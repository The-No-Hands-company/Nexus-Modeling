#include <nexus/app/AppMode.h>
#include <nexus/app/PrimitiveModelingMode.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadDimension.h>
#include <nexus/parametric/FeatureHistory.h>
#include <algorithm>
#include <cstdio>

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
    for(const auto& id : reg.registeredModeIds()) {
        auto mode = reg.create(id);
        if(mode) m_modes.push_back(std::move(mode));
    }
}

bool ModeOrchestrator::switchTo(const std::string& modeId) {
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

// ── Built-in Mode Implementations ─────────────────────────────────────

namespace {

class SelectMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "select"; }
    [[nodiscard]] std::string displayName() const override { return "Select"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"select.pick","Pick","LMB","select","Select"},
                {"select.all","Select All","Ctrl+A","select","Select"}};
    }
};

class SketchMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "sketch"; }
    [[nodiscard]] std::string displayName() const override { return "Sketch"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"sketch.line","Line","L","draw","Sketch"},
                {"sketch.rectangle","Rectangle","R","draw","Sketch"},
                {"sketch.circle","Circle","C","draw","Sketch"},
                {"sketch.arc","Arc","A","draw","Sketch"},
                {"sketch.polyline","Polyline","P","draw","Sketch"}};
    }
    EventResult handleInput(AppContext&, const InputEvent& event) override {
        return event.type==InputEventType::MouseDown&&event.button==0
            ? EventResult::Consumed : EventResult::Unconsumed;
    }
    void renderOverlay(AppContext&) override {}
};

// ── Extrude mode: click and drag to set height, release to create ────
class ExtrudeMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "extrude"; }
    [[nodiscard]] std::string displayName() const override { return "Extrude"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"extrude.create","Extrude","E","modify","Extrude"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        if(fc==0 || m_sketchId == parametric::kInvalidFeatureId) {
            // Find most recent sketch on first use.
            for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
                auto* n = hist.node(i);
                if(n && n->kind==parametric::FeatureKind::Sketch) { m_sketchId=i; break; }
            }
            if(m_sketchId==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
            printf("Extrude: sketch feature %u ready (drag to set height)\n", m_sketchId);
        }
        if(event.type==InputEventType::MouseDown && event.button==0) {
            m_dragging=true; m_startY=event.position.y; return EventResult::Consumed;
        }
        if(event.type==InputEventType::MouseDrag && m_dragging) {
            float dy = event.position.y - m_startY;
            float height = std::max(0.1f, -dy * 0.02f);
            printf("Extrude height: %.2f\r", height); fflush(stdout);
            return EventResult::Consumed;
        }
        if(event.type==InputEventType::MouseUp && m_dragging) {
            m_dragging=false;
            ctx.previewMesh.reset();
            float dy = event.position.y - m_startY;
            float h = std::max(0.1f, -dy * 0.02f);
            geometry::CurveExtrudeDesc desc;
            desc.direction={0,0,1}; desc.height=h; desc.capEnds=true;
            auto fid = ctx.document->addExtrude(m_sketchId, desc);
            if(fid!=parametric::kInvalidFeatureId) { hist.rebuild(); printf("\nExtrude: feature %u (h=%.2f)\n",fid,h); }
            m_sketchId=parametric::kInvalidFeatureId;
            return EventResult::Consumed;
        }
        return EventResult::Unconsumed;
    }
private:
    parametric::FeatureId m_sketchId=parametric::kInvalidFeatureId;
    bool m_dragging=false;
    float m_startY=0;
};

// ── Revolve mode: click and drag to set angle, release to create ──────
class RevolveMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "revolve"; }
    [[nodiscard]] std::string displayName() const override { return "Revolve"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"revolve.create","Revolve","R","modify","Revolve"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        if(fc==0 || m_sketchId==parametric::kInvalidFeatureId) {
            for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
                auto* n=hist.node(i); if(n&&n->kind==parametric::FeatureKind::Sketch){m_sketchId=i;break;}
            }
            if(m_sketchId==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
        }
        if(event.type==InputEventType::MouseDown && event.button==0) {
            m_dragging=true; m_startY=event.position.y; return EventResult::Consumed;
        }
        if(event.type==InputEventType::MouseDrag && m_dragging) {
            float angle = (event.position.y - m_startY) * 2.f;
            printf("Revolve angle: %.0f deg\r", angle); fflush(stdout);
            return EventResult::Consumed;
        }
        if(event.type==InputEventType::MouseUp && m_dragging) {
            m_dragging=false;
            float angle = (event.position.y - m_startY) * 2.f;
            if(std::fabs(angle)<1.f) angle=360.f;
            geometry::RevolveDesc desc;
            desc.axisDirection={0,1,0}; desc.endAngleDeg=angle; desc.capEnds=true;
            auto fid = ctx.document->addRevolve(m_sketchId, desc);
            if(fid!=parametric::kInvalidFeatureId){hist.rebuild();printf("\nRevolve: feature %u (%.1f deg)\n",fid,angle);}
            m_sketchId=parametric::kInvalidFeatureId;
            return EventResult::Consumed;
        }
        return EventResult::Unconsumed;
    }
private:
    parametric::FeatureId m_sketchId=parametric::kInvalidFeatureId;
    bool m_dragging=false;
    float m_startY=0;
};

// ── Fillet mode: click to chamfer the selected feature ─────────────────
class FilletMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "fillet"; }
    [[nodiscard]] std::string displayName() const override { return "Fillet"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"fillet.constant","Constant Radius","F","modify","Fillet"},
                {"fillet.variable","Variable Radius","Shift+F","modify","Fillet"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type != InputEventType::MouseDown || event.button != 0)
            return EventResult::Unconsumed;
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        bool applied = false;
        for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(fc); ++i) {
            auto* node = hist.node(i);
            if(!node || !node->mesh) continue;
            geometry::BevelChamferDesc desc;
            desc.mode = geometry::BevelChamferMode::Chamfer;
            desc.distance = 0.12f;
            desc.sharpAngleDegrees = 20.f;
            geometry::Mesh output;
            auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
            if(report.isSuccess()) { node->mesh.emplace(std::move(output)); applied=true; }
        }
        return applied ? EventResult::Consumed : EventResult::Unconsumed;
    }
};

// ── Dimension mode: two clicks → linear dimension line in 3D ─────────
class DimensionMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "dimension"; }
    [[nodiscard]] std::string displayName() const override { return "Dimension"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"dim.linear","Linear Dim","D","annotate","Dimension"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
        if(!m_firstSet) {
            m_p1=ctx.cursorWorldPos; m_firstSet=true;
            ctx.dimP1=m_p1; ctx.dimP2=m_p1; ctx.dimActive=true;
        } else {
            m_p2=ctx.cursorWorldPos; m_firstSet=false;
            ctx.dimP2=m_p2; ctx.dimActive=true;
            float d=(m_p2-m_p1).length();
            printf("Dimension: %.2f units\n", d);
        }
        return EventResult::Consumed;
    }
    void renderOverlay(AppContext&) override {}
private:
    bool m_firstSet=false;
    Vec3 m_p1{}, m_p2{};
};

// ── Face Edit mode: extrude face along normal ────────────────────────
class FaceEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "face-edit"; }
    [[nodiscard]] std::string displayName() const override { return "Face Edit"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"face.extrude","Extrude Face","Q","modify","Face"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
        size_t fc = ctx.document->history().featureCount();
        if(fc==0) return EventResult::Unconsumed;
        if(ctx.selectedFace==~0u) return EventResult::Unconsumed;
        // Find the feature we want to edit (most recent non-deleted mesh).
        auto& hist = ctx.document->history();
        parametric::FeatureId target = parametric::kInvalidFeatureId;
        for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
            auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted){target=i;break;}
        }
        if(target==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
        auto* node = hist.node(target);
        auto heOpt = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
        if(!heOpt) { printf("Face edit: not manifold\n"); return EventResult::Unconsumed; }
        // Extrude the selected face outward.
        geometry::TweakFaceOptions opts;
        opts.distance = 0.5f;
        opts.direction = geometry::TweakDirection::AlongNormal;
        auto result = geometry::tweakFace(*heOpt, ctx.selectedFace, opts);
        if(result) { node->mesh.emplace(std::move(*result)); printf("Extruded face %u\n",ctx.selectedFace); }
        else printf("Face extrude failed\n");
        return EventResult::Consumed;
    }
};

// ── Edge Edit mode: bevel edge ────────────────────────────────────────
class EdgeEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "edge-edit"; }
    [[nodiscard]] std::string displayName() const override { return "Edge Edit"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"edge.bevel","Bevel","B","modify","Edge"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        if(fc==0) return EventResult::Unconsumed;
        // Find most recent mesh feature.
        parametric::FeatureId target=parametric::kInvalidFeatureId;
        for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
            auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted){target=i;break;}
        }
        if(target==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
        auto* node = hist.node(target);
        geometry::BevelChamferDesc desc;
        desc.mode=geometry::BevelChamferMode::Bevel; desc.distance=0.1f; desc.sharpAngleDegrees=15.f;
        geometry::Mesh output;
        auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
        if(report.isSuccess()) { node->mesh.emplace(std::move(output)); printf("Bevel edge (angle>=15)\n"); }
        return EventResult::Consumed;
    }
};

// ── Vertex Edit mode: move vertex ─────────────────────────────────────
class VertexEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "vertex-edit"; }
    [[nodiscard]] std::string displayName() const override { return "Vertex Edit"; }
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override {
        return {{"vertex.move","Move","G","modify","Vertex"}};
    }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        if(fc==0||ctx.selectedVertex==~0u) return EventResult::Unconsumed;
        parametric::FeatureId target=parametric::kInvalidFeatureId;
        for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
            auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted){target=i;break;}
        }
        if(target==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
        auto* node = hist.node(target);
        auto pos = node->mesh->attributes().positions();
        if(ctx.selectedVertex < pos.size()) {
            pos[ctx.selectedVertex] = ctx.cursorWorldPos;
            node->mesh->attributes().setPositions(std::move(pos));
            printf("Moved vertex %u\n", ctx.selectedVertex);
        }
        return EventResult::Consumed;
    }
};

// ── Boolean mode: click to union selected with most recent ────────────
class BooleanMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "boolean"; }
    [[nodiscard]] std::string displayName() const override { return "Boolean"; }
    std::vector<ActionDescriptor> actions() const override {
        return {{"bool.union","Union","Shift+U","modify","Boolean"},
                {"bool.diff","Difference","Shift+D","modify","Boolean"},
                {"bool.isect","Intersection","Shift+I","modify","Boolean"}};
    }
};

// ── Pattern mode: linear/circular pattern of selected ──────────────────
class PatternMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "pattern"; }
    [[nodiscard]] std::string displayName() const override { return "Pattern"; }
};

// ── Mirror mode: mirror selected across plane ──────────────────────────
class MirrorMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "mirror"; }
    [[nodiscard]] std::string displayName() const override { return "Mirror"; }
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override {
        if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
        auto& hist = ctx.document->history();
        size_t fc = hist.featureCount();
        if(fc==0) return EventResult::Unconsumed;
        auto fid = static_cast<parametric::FeatureId>(fc);
        auto* node = hist.node(fid);
        if(!node||!node->mesh) return EventResult::Unconsumed;
        // Mirror across YZ plane (x=0) by negating X.
        auto pos = node->mesh->attributes().positions();
        for(auto& v:pos) v.x = -v.x;
        node->mesh->attributes().setPositions(std::move(pos));
        printf("Mirror feature %u across YZ plane\n", fid);
        return EventResult::Consumed;
    }
};

} // anonymous namespace

// Mode registration via static constructors
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
