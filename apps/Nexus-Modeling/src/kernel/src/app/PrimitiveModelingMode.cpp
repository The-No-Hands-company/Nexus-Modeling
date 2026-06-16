#include <nexus/app/PrimitiveModelingMode.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cmath>
#include <cstdio>
#include <numbers>

namespace nexus::app {

using namespace nexus::geometry;

void PrimitiveModelingMode::onActivate(AppContext& ctx) {
    (void)ctx;
    printf("Modeling mode active. Keys: 1=Box 2=Sphere 3=Cylinder 4=Cone 5=Torus 6=Plane  Click=Place\n");
}

EventResult PrimitiveModelingMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type == InputEventType::KeyDown) {
        switch(event.key) {
            case '1': m_activePrimitive = PrimitiveType::Box;    printf("Primitive: Box\n");    break;
            case '2': m_activePrimitive = PrimitiveType::Sphere; printf("Primitive: Sphere\n"); break;
            case '3': m_activePrimitive = PrimitiveType::Cylinder; printf("Primitive: Cylinder\n"); break;
            case '4': m_activePrimitive = PrimitiveType::Cone;   printf("Primitive: Cone\n");   break;
            case '5': m_activePrimitive = PrimitiveType::Torus;  printf("Primitive: Torus\n");  break;
            case '6': m_activePrimitive = PrimitiveType::Plane;  printf("Primitive: Plane\n");  break;
        }
        return EventResult::Consumed;
    }

    // Click to place the current primitive at cursor position.
    if(event.type == InputEventType::MouseDown && event.button == 0) {
        auto& cwp = ctx.cursorWorldPos;
        // Quantize to nearest unit for grid snap.
        Vec3 snapped = cwp;
        if(ctx.snapToGrid) {
            snapped.x = std::round(snapped.x);
            snapped.y = std::round(snapped.y);
            snapped.z = std::round(snapped.z);
        }
        printf("Placing %s at (%.2f, %.2f, %.2f)\n",
               primitiveName(m_activePrimitive).c_str(),
               snapped.x, snapped.y, snapped.z);

        if(ctx.selection) ctx.selection->clear();

        Mesh prim = createPrimitive(m_activePrimitive);
        if(prim.isValid()) {
            // Translate to snapped cursor world position.
            auto pos = prim.attributes().positions();
            for(auto& v : pos) { v.x += snapped.x; v.y += snapped.y; v.z += snapped.z; }
            prim.attributes().setPositions(std::move(pos));

            if(ctx.document) {
                auto sk = parametric::ParametricSketchFactory::createSketch();
                auto featId = ctx.document->addSketch(sk);
                if(featId != parametric::kInvalidFeatureId) {
                    auto* node = ctx.document->history().node(featId);
                    if(node) {
                        node->mesh.emplace(std::move(prim));
                        node->dirty = false;
                        printf("Placed %s (feature %llu, %zu verts)\n",
                               primitiveName(m_activePrimitive).c_str(),
                               static_cast<unsigned long long>(featId),
                               node->mesh->attributes().vertexCount());
                    }
                }
            }
        }
        return EventResult::Consumed;
    }
    return EventResult::Unconsumed;
}

void PrimitiveModelingMode::renderToolVisuals(AppContext&) {
    // Could draw a preview of the primitive at cursor position.
}

std::vector<ActionDescriptor> PrimitiveModelingMode::actions() const {
    return {
        {"modeling.box",    "Box",    "1", "", "Create", false},
        {"modeling.sphere", "Sphere", "2", "", "Create", false},
        {"modeling.cylinder","Cylinder","3","","Create", false},
        {"modeling.cone",   "Cone",   "4", "", "Create", false},
        {"modeling.torus",  "Torus",  "5", "", "Create", false},
        {"modeling.plane",  "Plane",  "6", "", "Create", false},
    };
}

bool PrimitiveModelingMode::executeAction(const std::string& actionId, AppContext&) {
    if(actionId == "modeling.box")      m_activePrimitive = PrimitiveType::Box;
    else if(actionId == "modeling.sphere") m_activePrimitive = PrimitiveType::Sphere;
    else if(actionId == "modeling.cylinder") m_activePrimitive = PrimitiveType::Cylinder;
    else if(actionId == "modeling.cone")     m_activePrimitive = PrimitiveType::Cone;
    else if(actionId == "modeling.torus")    m_activePrimitive = PrimitiveType::Torus;
    else if(actionId == "modeling.plane")    m_activePrimitive = PrimitiveType::Plane;
    return true;
}

geometry::Mesh PrimitiveModelingMode::createPrimitive(PrimitiveType type) const noexcept {
    switch(type) {
        case PrimitiveType::Box:      return primitives::makeBox(2.0f, 2.0f, 2.0f);
        case PrimitiveType::Sphere:   return primitives::makeSphere(1.5f, 32, 24);
        case PrimitiveType::Cylinder: return primitives::makeCylinder(1.0f, 3.0f, 32);
        case PrimitiveType::Cone:     return primitives::makeCone(1.0f, 3.0f, 32);
        case PrimitiveType::Torus:    return primitives::makeTorus(2.0f, 0.5f, 24, 16);
        case PrimitiveType::Plane:    return primitives::makePlane(4.0f, 4.0f);
    }
    return {};
}

std::string PrimitiveModelingMode::primitiveName(PrimitiveType type) const noexcept {
    switch(type) {
        case PrimitiveType::Box:      return "Box";
        case PrimitiveType::Sphere:   return "Sphere";
        case PrimitiveType::Cylinder: return "Cylinder";
        case PrimitiveType::Cone:     return "Cone";
        case PrimitiveType::Torus:    return "Torus";
        case PrimitiveType::Plane:    return "Plane";
    }
    return "Unknown";
}

} // namespace nexus::app
