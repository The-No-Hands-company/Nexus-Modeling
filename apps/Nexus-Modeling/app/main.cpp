// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Main Entry Point
//
//  Windowed DCC application with GLFW, Vulkan rendering, and full CAD pipeline.
// ────────────────────────────────────────────────────────────────────────────

#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <nexus/app/ModelingApplication.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/SketchPreview.h>
#include <nexus/app/SelectionHighlight.h>
#include <nexus/app/TransformGizmo.h>
#include <nexus/app/EditorUI.h>
#include <nexus/cad/CadViewer.h>
#include <imgui_impl_glfw.h>
#include <nexus/cad/CadAutoConstraintSketch.h>
#include <nexus/geometry/MeshBoolean.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

using namespace nexus::app;
using namespace nexus::cad;
using namespace nexus::geometry;
using namespace nexus::parametric;

// ── Global state ───────────────────────────────────────────────────────────

struct AppState {
    ModelingApplication* app = nullptr;
    CadViewer* viewer = nullptr;
    CadAutoConstraintSketch* sketcher = nullptr;
    GLFWwindow* window = nullptr;
    int width = 1280, height = 720;
    bool sketchActive = false;
    enum class SketchPrim { Point, Rectangle, Circle };
    SketchPrim sketchPrimitive = SketchPrim::Point;
    // For two-click primitives (rect: 1st=corner, circle: 1st=center).
    bool sketchPrimPending = false;
    double sketchPrimX = 0, sketchPrimY = 0;
    float lastMouseX = 0, lastMouseY = 0;
    bool mouseDragging = false;
    bool snapToGrid = true;
    FeatureId selectedId = kInvalidFeatureId;
    std::vector<FeatureId> selectedIds; // multi-select support
    Vec3 selectedColor{0.4f, 0.55f, 0.7f};
    std::vector<Vec3> colorPalette = {
        {0.4f,0.55f,0.7f}, {0.7f,0.4f,0.4f}, {0.4f,0.7f,0.4f},
        {0.7f,0.7f,0.4f}, {0.7f,0.4f,0.7f}, {0.4f,0.7f,0.7f},
        {0.9f,0.6f,0.3f}, {0.5f,0.3f,0.7f}, {0.3f,0.5f,0.1f},
    };
    TransformGizmo gizmo;
    TransformGizmo::Axis activeGizmoAxis = TransformGizmo::Axis::None;
    bool gizmoDragging = false;
    Vec3 gizmoWorldCenter{};  // center of selected object world space
    Vec3 gizmoStartWorldPos{}; // world pos under mouse at drag start
    bool ortho = false;       // orthographic projection toggle
    bool lighting = false;     // Phong lighting toggle
    float gridSpacing = 1.f;  // grid spacing
    float gridExtent = 10.f;  // grid half-extent

    // Animation timeline.
    bool animPlaying = false;
    int  animFrame = 0, animMaxFrame = 100;
    float animTime = 0;
    struct Keyframe { int frame; Vec3 position; };
    std::unordered_map<FeatureId, std::vector<Keyframe>> keyframes;
    float lastFrameTime = 0;
    std::optional<nexus::geometry::Mesh> clipboardMesh;

    enum class SnapMode { Vertex, EdgeMidpoint, FaceCenter };
    SnapMode snapMode = SnapMode::Vertex;
};

// Möller–Trumbore ray-triangle intersection.
// Returns true and sets 't' to the intersection distance if hit.
static bool rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
                                  const Vec3& v0, const Vec3& v1, const Vec3& v2,
                                  float& t) {
    Vec3 e1 = v1 - v0, e2 = v2 - v0;
    Vec3 h = rd.cross(e2);
    float a = e1.dot(h);
    if(std::fabs(a) < 1e-7f) return false;
    float f = 1.f / a;
    Vec3 s = ro - v0;
    float u = f * s.dot(h);
    if(u < 0.f || u > 1.f) return false;
    Vec3 q = s.cross(e1);
    float v = f * rd.dot(q);
    if(v < 0.f || u + v > 1.f) return false;
    float tt = f * e2.dot(q);
    if(tt < 1e-4f) return false;
    t = tt;
    return true;
}

// Full ray-cast: returns feature ID + face/edge/vertex indices + hit point.
static FeatureId pickSubObject(const nexus::cad::CadDocument& doc,
                               const Vec3& rayOrigin, const Vec3& rayDir,
                               uint32_t& outFace, uint32_t& outVertex, Vec3& outHit) {
    using namespace nexus::parametric;
    FeatureId best = kInvalidFeatureId;
    outFace = ~0u; outVertex = ~0u;
    float bestT = 1e30f;
    size_t fc = doc.history().featureCount();
    for(FeatureId i=1; i<=static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if(!node || !node->mesh || node->deleted || node->hidden) continue;
        const auto& pos = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if(face.vertexCount() < 3) continue;
            for(size_t j=0; j+2 < face.vertexCount(); ++j) {
                const Vec3& p0 = pos[face.indices[0]];
                const Vec3& p1 = pos[face.indices[j+1]];
                const Vec3& p2 = pos[face.indices[j+2]];
                float t;
                if(rayTriangleIntersect(rayOrigin, rayDir, p0, p1, p2, t) && t < bestT) {
                    best = i; bestT = t; outFace = fi;
                    outHit = rayOrigin + rayDir * t;
                    // Find nearest vertex of the hit face to the hit point.
                    outVertex = face.indices[0];
                    float bestVDist = (pos[outVertex] - outHit).lengthSq();
                    for(size_t k=1; k<face.vertexCount(); ++k) {
                        float d2 = (pos[face.indices[k]] - outHit).lengthSq();
                        if(d2 < bestVDist) { bestVDist=d2; outVertex=face.indices[k]; }
                    }
                }
            }
        }
    }
    return best;
}

// Ray-cast against all document features, return nearest feature ID.
static FeatureId pickFeature(const nexus::cad::CadDocument& doc,
                              const Vec3& rayOrigin, const Vec3& rayDir) {
    uint32_t fa, ve; Vec3 hp;
    return pickSubObject(doc, rayOrigin, rayDir, fa, ve, hp);
}
static void screenToWorldRay(float mx, float my, Vec3& origin, Vec3& direction) {
    GLdouble mv[16], proj[16];
    GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);
    double winY = vp[3] - my; // flip Y
    double nearX, nearY, nearZ, farX, farY, farZ;
    gluUnProject(mx, winY, 0.0, mv, proj, vp, &nearX, &nearY, &nearZ);
    gluUnProject(mx, winY, 1.0, mv, proj, vp, &farX, &farY, &farZ);
    origin = Vec3{(float)nearX, (float)nearY, (float)nearZ};
    Vec3 farPt{(float)farX, (float)farY, (float)farZ};
    direction = (farPt - origin).normalize();
}

// Snap to nearest geometry feature based on current snap mode.
static Vec3 snapToGeometry(const nexus::cad::CadDocument& doc, const Vec3& pos,
                           AppState::SnapMode mode, float radius = 2.f) {
    Vec3 best = pos;
    float bestDist = radius;
    size_t fc = doc.history().featureCount();
    for(FeatureId i=1; i<=static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if(!node||!node->mesh||node->deleted||node->hidden) continue;
        const auto& verts = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();
        if(mode == AppState::SnapMode::Vertex) {
            for(const auto& v : verts) {
                float d = (v-pos).length();
                if(d < bestDist) { bestDist=d; best=v; }
            }
        } else if(mode == AppState::SnapMode::EdgeMidpoint) {
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                for(size_t j=0; j<face.vertexCount(); ++j) {
                    size_t k = (j+1)%face.vertexCount();
                    Vec3 mid = (verts[face.indices[j]] + verts[face.indices[k]]) * 0.5f;
                    float d = (mid-pos).length();
                    if(d < bestDist) { bestDist=d; best=mid; }
                }
            }
        } else if(mode == AppState::SnapMode::FaceCenter) {
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                Vec3 center{};
                for(size_t j=0; j<face.vertexCount(); ++j) center = center + verts[face.indices[j]];
                center = center * (1.f / (float)face.vertexCount());
                float d = (center-pos).length();
                if(d < bestDist) { bestDist=d; best=center; }
            }
        }
    }
    return best;
}

// Keep old function for vertex-only snap.
static Vec3 snapToNearestVertex(const nexus::cad::CadDocument& doc, const Vec3& pos, float radius) {
    return snapToGeometry(doc, pos, AppState::SnapMode::Vertex, radius);
}

// Project a world position onto the active gizmo axis and compute delta.
static float axisDisplacement(const Vec3& prevWorldPos, const Vec3& curWorldPos,
                               TransformGizmo::Axis axis) {
    Vec3 axisDir{};
    if(axis == TransformGizmo::Axis::X) axisDir = {1,0,0};
    else if(axis == TransformGizmo::Axis::Y) axisDir = {0,1,0};
    else if(axis == TransformGizmo::Axis::Z) axisDir = {0,0,1};
    else return 0;
    Vec3 delta = curWorldPos - prevWorldPos;
    return delta.dot(axisDir);
}

static AppState* g_state = nullptr;
static SelectionHighlight g_highlight;

// ── Callbacks ──────────────────────────────────────────────────────────────

void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);
    if(ImGui::GetIO().WantCaptureKeyboard) return;

    auto& s = *g_state;
    if(action != GLFW_PRESS) return;

    s.app->onKeyDown(key);

    // Mode shortcuts.
    if(key == GLFW_KEY_ESCAPE) {
        s.app->orchestrator().switchTo("select");
        if(s.sketchActive) { s.sketcher->endSketch(); s.sketchActive = false; }
    }
    if(key == GLFW_KEY_S) {
        if(!s.sketchActive) { s.sketcher->beginSketch(); s.sketchActive = true; }
        s.app->orchestrator().switchTo("sketch");
    }
    if(key == GLFW_KEY_E) {
        if(s.sketchActive) {
            // Finalize sketch and extrude it.
            s.sketcher->endSketch();
            s.sketchActive = false;
            auto& profile = s.sketcher->profile();
            if(!profile.points.empty()) {
                nexus::geometry::CurveExtrudeDesc desc;
                desc.direction = {0,0,1};
                desc.height = 3.0f;
                auto skId = s.app->document().addSketch(profile);
                if(skId != kInvalidFeatureId) {
                    auto fid = s.app->document().addExtrude(skId, desc);
                    auto* node = s.app->document().history().node(fid);
                    if(node) { node->dirty = false; s.app->document().history().rebuild(); }
                    s.selectedId = fid;
                    printf("Extruded sketch: feature %u\n", fid);
                }
            } else {
                printf("Sketch empty — nothing to extrude\n");
            }
        }
        s.app->orchestrator().switchTo("extrude");
    }
    if(key == GLFW_KEY_F) {
        if(s.sketchActive) { s.sketcher->endSketch(); s.sketchActive = false; }
        s.app->orchestrator().switchTo("fillet");
    }
    if(key == GLFW_KEY_M) s.app->orchestrator().switchTo("modeling");

    // Sketch primitive selection (in sketch mode: 1=point, 2=rect, 3=circle).
    if(s.sketchActive && key >= GLFW_KEY_1 && key <= GLFW_KEY_3) {
        s.sketchPrimPending = false;
        s.sketchPrimitive = (key == GLFW_KEY_1) ? AppState::SketchPrim::Point :
                           (key == GLFW_KEY_2) ? AppState::SketchPrim::Rectangle : AppState::SketchPrim::Circle;
        const char* nm = (key==GLFW_KEY_1)?"Point":(key==GLFW_KEY_2)?"Rect":"Circle";
        printf("Sketch: %s\n", nm);
    }

    // Face / Edge / Vertex edit modes.
    if(key == GLFW_KEY_Q) { s.app->orchestrator().switchTo("face-edit"); printf("Face edit mode\n"); }
    if(key == GLFW_KEY_B) { s.app->orchestrator().switchTo("edge-edit"); printf("Edge edit mode\n"); }
    if(key == GLFW_KEY_V) { s.app->orchestrator().switchTo("vertex-edit"); printf("Vertex edit mode\n"); }

    // Boolean / Pattern / Mirror modes.
    if(key == GLFW_KEY_K) { s.app->orchestrator().switchTo("boolean"); printf("Boolean mode\n"); }
    if(key == GLFW_KEY_P) { s.app->orchestrator().switchTo("pattern"); printf("Pattern mode\n"); }
    if(key == GLFW_KEY_N) { s.app->orchestrator().switchTo("mirror"); printf("Mirror mode\n"); }

    // Revolve mode (overrides R=rotate gizmo toggle when not in select).
    if(key == GLFW_KEY_R && s.app->orchestrator().activeModeId() != "select") {
        s.app->orchestrator().switchTo("revolve"); printf("Revolve mode\n");
    }
    // Dimension mode.
    if(key == GLFW_KEY_D) { s.app->orchestrator().switchTo("dimension"); printf("Dimension mode\n"); }

    // Delete selected.
    if(key == GLFW_KEY_DELETE || key == GLFW_KEY_X) {
        if(s.selectedId != kInvalidFeatureId) {
            s.app->document().deleteFeature(s.selectedId);
            s.selectedId = kInvalidFeatureId;
            printf("Deleted feature\n");
        }
    }

    // Grid snap toggle.
    if(key == GLFW_KEY_G) {
        s.snapToGrid = !s.snapToGrid;
        s.app->context().snapToGrid = s.snapToGrid;
        printf("Grid snap: %s\n", s.snapToGrid ? "ON" : "OFF");
    }
    // Working plane cycle (Tab).
    if(key == GLFW_KEY_TAB && !(mods & GLFW_MOD_SHIFT)) {
        auto& wp = s.app->context().workPlane;
        if(wp == AppContext::WorkPlane::XZ) wp = AppContext::WorkPlane::XY;
        else if(wp == AppContext::WorkPlane::XY) wp = AppContext::WorkPlane::YZ;
        else wp = AppContext::WorkPlane::XZ;
        const char* nm = (wp==AppContext::WorkPlane::XZ)?"XZ":(wp==AppContext::WorkPlane::XY)?"XY":"YZ";
        printf("Work plane: %s\n", nm);
    }
    // Snap mode cycle (Shift+Tab).
    if(key == GLFW_KEY_TAB && (mods & GLFW_MOD_SHIFT)) {
        if(s.snapMode == AppState::SnapMode::Vertex) s.snapMode = AppState::SnapMode::EdgeMidpoint;
        else if(s.snapMode == AppState::SnapMode::EdgeMidpoint) s.snapMode = AppState::SnapMode::FaceCenter;
        else s.snapMode = AppState::SnapMode::Vertex;
        const char* nm = (s.snapMode==AppState::SnapMode::Vertex)?"Vertex":
                         (s.snapMode==AppState::SnapMode::EdgeMidpoint)?"EdgeMid":"FaceCenter";
        printf("Snap: %s\n", nm);
    }
    // Grid spacing ([/] keys).
    if(key == GLFW_KEY_LEFT_BRACKET)  { s.gridSpacing = std::max(0.1f, s.gridSpacing * 0.5f); printf("Grid: %.1f\n", s.gridSpacing); }
    if(key == GLFW_KEY_RIGHT_BRACKET) { s.gridSpacing = std::min(16.f, s.gridSpacing * 2.f);  printf("Grid: %.1f\n", s.gridSpacing); }

    // Undo/Redo (Ctrl+Z / Ctrl+Y).
    bool ctrl = (glfwGetKey(s.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                 glfwGetKey(s.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
    if(ctrl && key == GLFW_KEY_Z) { s.app->document().undo(); printf("Undo\n"); return; }
    if(ctrl && key == GLFW_KEY_Y) { s.app->document().redo(); printf("Redo\n"); return; }

    // Boolean operations (Shift+U/D/I in select mode).
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    if(shift && s.app->orchestrator().activeModeId() == "select" &&
       (key == GLFW_KEY_U || key == GLFW_KEY_D || key == GLFW_KEY_I)) {
        auto fc = s.app->document().history().featureCount();
        if(fc >= 2) {
            FeatureId idB = static_cast<FeatureId>(fc);
            FeatureId idA = s.selectedId;
            if(idA == kInvalidFeatureId || idA == idB) idA = static_cast<FeatureId>(fc-1);
            auto* nA = s.app->document().history().node(idA);
            auto* nB = s.app->document().history().node(idB);
            if(nA && nB && nA->mesh && nB->mesh) {
                BooleanOp op = (key == GLFW_KEY_U) ? BooleanOp::Union :
                               (key == GLFW_KEY_D) ? BooleanOp::Difference : BooleanOp::Intersection;
                auto result = MeshBoolean::compute(*nA->mesh, *nB->mesh, op);
                if(result.ok) {
                    auto sk = ParametricSketchFactory::createSketch();
                    auto fid = s.app->document().addSketch(sk);
                    auto* n = s.app->document().history().node(fid);
                    if(n) { n->mesh.emplace(std::move(result.result)); n->dirty = false; }
                    s.selectedId = fid;
                    const char* nm = (op==BooleanOp::Union)?"Union":(op==BooleanOp::Difference)?"Diff":"Intersect";
                    printf("Boolean %s: %u\n", nm, fid);
                } else {
                    printf("Boolean: %s\n", result.error.c_str());
                }
            }
        }
        return;
    }

    // Duplicate selected (Ctrl+D).
    if(ctrl && key == GLFW_KEY_D && s.selectedId != kInvalidFeatureId) {
        auto* node = s.app->document().history().node(s.selectedId);
        if(node && node->mesh) {
            Mesh copy = *node->mesh;
            // Offset the duplicate.
            auto pos = copy.attributes().positions();
            for(auto& v : pos) { v.x += 2.f; v.y += 2.f; }
            copy.attributes().setPositions(std::move(pos));
            auto sk = ParametricSketchFactory::createSketch();
            auto newId = s.app->document().addSketch(sk);
            auto* newNode = s.app->document().history().node(newId);
            if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty = false; }
            s.selectedId = newId;
            printf("Duplicated feature %u → %u (offset +2,+2)\n", node->id, newId);
        }
        return;
    }
    // Copy (Ctrl+C).
    if(ctrl && key == GLFW_KEY_C && s.selectedId != kInvalidFeatureId) {
        auto* node = s.app->document().history().node(s.selectedId);
        if(node && node->mesh) { s.clipboardMesh = *node->mesh; printf("Copied %u\n", s.selectedId); }
        return;
    }
    // Paste (Ctrl+V).
    if(ctrl && key == GLFW_KEY_V && s.clipboardMesh.has_value()) {
        Mesh copy = *s.clipboardMesh;
        auto pos = copy.attributes().positions();
        for(auto& v : pos) { v.x += 3.f; v.y += 3.f; }
        copy.attributes().setPositions(std::move(pos));
        auto sk = ParametricSketchFactory::createSketch();
        auto fid = s.app->document().addSketch(sk);
        auto* n = s.app->document().history().node(fid);
        if(n) { n->mesh.emplace(std::move(copy)); n->dirty = false; }
        s.selectedId = fid;
        printf("Pasted → feature %u\n", fid);
        return;
    }

    // Save (Ctrl+S).
    if(ctrl && key == GLFW_KEY_S) {
        auto data = s.app->document().serialize();
        FILE* f = fopen("scene.nxm", "wb");
        if(f) { fwrite(data.data(), 1, data.size(), f); fclose(f); printf("Saved scene.nxm\n"); }
        return;
    }

    // Load (Ctrl+O).
    if(ctrl && key == GLFW_KEY_O) {
        FILE* f = fopen("scene.nxm", "rb");
        if(f) { fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            std::vector<uint8_t> d(sz); fread(d.data(), 1, sz, f); fclose(f);
            s.app->document().deserialize(d.data(), d.size());
            s.selectedId = kInvalidFeatureId;
            printf("Loaded scene.nxm (%ld bytes)\n", sz);
        }
        return;
    }

    // Transform mode toggle (W=translate, E=scale, R=rotate).
    if(key == GLFW_KEY_W) { s.gizmo.setMode(TransformGizmo::Mode::Translate); printf("Gizmo: translate\n"); }
    if(key == GLFW_KEY_E) { s.gizmo.setMode(TransformGizmo::Mode::Scale);     printf("Gizmo: scale\n"); }
    if(key == GLFW_KEY_R) { s.gizmo.setMode(TransformGizmo::Mode::Rotate);    printf("Gizmo: rotate\n"); }

    // Alt+D deselect.
    if(key == GLFW_KEY_A) { s.selectedId = kInvalidFeatureId; printf("Deselected\n"); }
    // View all (Home key).
    if(key == GLFW_KEY_HOME) {
        // Compute union bounds of all features.
        nexus::render::Aabb total{{1e10,1e10,1e10},{-1e10,-1e10,-1e10}};
        auto& hist = s.app->document().history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n = hist.node(i);
            if(!n||!n->mesh||n->deleted) continue;
            auto b = n->mesh->computeBounds();
            if(b.min.x < total.min.x) total.min.x = b.min.x;
            if(b.min.y < total.min.y) total.min.y = b.min.y;
            if(b.min.z < total.min.z) total.min.z = b.min.z;
            if(b.max.x > total.max.x) total.max.x = b.max.x;
            if(b.max.y > total.max.y) total.max.y = b.max.y;
            if(b.max.z > total.max.z) total.max.z = b.max.z;
        }
        auto extents = total.extents();
        float d = std::max({extents.x, extents.y, extents.z}) * 3.f + 5.f;
        if(d>1e8) d=10.f; // empty scene
        s.viewer->camera().lookAt(total.center(), d);
        printf("View all (dist %.1f)\n", d);
    }
    // Frame selected (F in select mode).
    if(key == GLFW_KEY_F && s.app->orchestrator().activeModeId() == "select" && s.selectedId != kInvalidFeatureId) {
        auto* n = s.app->document().history().node(s.selectedId);
        if(n && n->mesh) {
            auto b = n->mesh->computeBounds();
            auto extents = b.extents();
            float d = std::max({extents.x, extents.y, extents.z}) * 5.f + 2.f;
            s.viewer->camera().lookAt(b.center(), d);
            printf("Frame selected (dist %.1f)\n", d);
        }
    }
    // Viewport: ortho toggle.
    if(key == GLFW_KEY_KP_5 || (key == GLFW_KEY_O && !s.sketchActive && s.app->orchestrator().activeModeId()!="modeling")) {
        s.ortho = !s.ortho;
        printf("Projection: %s\n", s.ortho?"Orthographic":"Perspective");
    }
    // Lighting toggle.
    if(key == GLFW_KEY_L) {
        s.lighting = !s.lighting;
        printf("Lighting: %s\n", s.lighting?"ON":"OFF");
    }
    // Animation: play/pause, insert/delete keyframe, frame navigation.
    if(key == GLFW_KEY_SPACE) { s.animPlaying = !s.animPlaying; printf("Anim: %s\n", s.animPlaying?"PLAY":"pause"); }
    if(key == GLFW_KEY_LEFT)  { s.animFrame = std::max(0, s.animFrame-1); s.animTime = (float)s.animFrame; }
    if(key == GLFW_KEY_RIGHT) { s.animFrame = std::min(s.animMaxFrame, s.animFrame+1); s.animTime = (float)s.animFrame; }
    if(key == GLFW_KEY_I && s.selectedId != kInvalidFeatureId) {
        auto& kfs = s.keyframes[s.selectedId];
        Vec3 pos{0,0,0};
        auto* n = s.app->document().history().node(s.selectedId);
        if(n && n->mesh) pos = n->mesh->computeBounds().center();
        kfs.push_back({s.animFrame, pos});
        printf("Keyframe: feat %u frame %d\n", s.selectedId, s.animFrame);
    }
    if(key == GLFW_KEY_K && s.selectedId != kInvalidFeatureId) {
        auto it = s.keyframes.find(s.selectedId);
        if(it != s.keyframes.end()) {
            it->second.clear(); s.keyframes.erase(it);
            printf("Keyframes cleared for feature %u\n", s.selectedId);
        }
    }
    // Standard views (NumPad).
    if(key == GLFW_KEY_KP_0) { s.viewer->camera().viewIsometric(); printf("View: Isometric\n"); }
    if(key == GLFW_KEY_KP_7) { s.viewer->camera().viewTop(); printf("View: Top\n"); }
    if(key == GLFW_KEY_KP_1) { s.viewer->camera().viewFront(); printf("View: Front\n"); }
    if(key == GLFW_KEY_KP_3) { s.viewer->camera().viewRight(); printf("View: Right\n"); }

    // Hide/Isolate/Unhide.
    bool _ctrl = (glfwGetKey(s.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                  glfwGetKey(s.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
    bool _shift = (mods & GLFW_MOD_SHIFT) != 0;
    if(key == GLFW_KEY_H && !_ctrl && !_shift) {
        // Hide selected
        for(auto id : s.selectedIds) s.app->document().history().setHidden(id, true);
        printf("Hidden %zu feature(s)\n", s.selectedIds.size());
        s.selectedIds.clear(); s.selectedId = kInvalidFeatureId;
    }
    if(key == GLFW_KEY_H && _shift) {
        // Isolate selected: hide all except selected
        auto& hist = s.app->document().history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n = hist.node(i);
            if(!n||n->deleted) continue;
            bool inSel = std::find(s.selectedIds.begin(),s.selectedIds.end(),i)!=s.selectedIds.end();
            hist.setHidden(i, !inSel);
        }
        printf("Isolated %zu feature(s)\n", s.selectedIds.size());
    }
    if(key == GLFW_KEY_H && _ctrl) {
        s.app->document().history().unhideAll();
        printf("All unhidden\n");
    }

    // Color palette 1-9 in select mode.
    if(s.app->orchestrator().activeModeId() == "select") {
        int ci = key - GLFW_KEY_1;
        if(ci >= 0 && ci < (int)s.colorPalette.size()) {
            s.selectedColor = s.colorPalette[ci];
            printf("Color: palette[%d]\n", ci);
        }
    }
}

void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
    if(ImGui::GetIO().WantCaptureMouse) return;

    auto& s = *g_state;
    double mx, my; glfwGetCursorPos(s.window, &mx, &my);

    if(action == GLFW_PRESS) {
        s.app->onMouseDown(button, (float)mx, (float)my);
        s.mouseDragging = true;
        s.lastMouseX = (float)mx;
        s.lastMouseY = (float)my;

        // Select mode: pick gizmo axis first, then select object.
        if(button == GLFW_MOUSE_BUTTON_LEFT &&
           s.app->orchestrator().activeModeId() == "select") {
            // If we have a selection, try gizmo pick.
            if(s.selectedId != kInvalidFeatureId) {
                auto* node = s.app->document().history().node(s.selectedId);
                if(node && node->mesh) {
                    Vec3 center = node->mesh->computeBounds().center();
                    Vec3 rayOrigin, rayDir;
                    screenToWorldRay((float)mx, (float)my, rayOrigin, rayDir);
                    auto axis = s.gizmo.pickAxis(center, rayOrigin, rayDir);
                    if(axis != TransformGizmo::Axis::None) {
                        s.gizmoDragging = true;
                        s.activeGizmoAxis = axis;
                        s.gizmoWorldCenter = center;
                        s.gizmoStartWorldPos = rayOrigin + rayDir * 5.f;
                        printf("Gizmo drag start (axis %d)\n", (int)axis);
                        return;
                    }
                }
            }
            // No gizmo hit: ray-cast selection.
            s.gizmoDragging = false;
            {
                Vec3 rayOrigin, rayDir;
                screenToWorldRay((float)mx, (float)my, rayOrigin, rayDir);
                uint32_t faceIdx=~0u, vertIdx=~0u; Vec3 hit;
                auto fid = pickSubObject(s.app->document(), rayOrigin, rayDir, faceIdx, vertIdx, hit);
                if(fid != kInvalidFeatureId) {
                    // Store sub-object selection.
                    s.app->context().selectedFace = faceIdx;
                    s.app->context().selectedVertex = vertIdx;
                    s.app->context().hitPoint = hit;
                    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
                    if(shift) {
                        // Toggle in multi-selection.
                        auto it = std::find(s.selectedIds.begin(), s.selectedIds.end(), fid);
                        if(it!=s.selectedIds.end()) s.selectedIds.erase(it);
                        else s.selectedIds.push_back(fid);
                        s.selectedId = fid; // primary selection
                    } else {
                        s.selectedIds.clear();
                        s.selectedIds.push_back(fid);
                        s.selectedId = fid;
                    }
                    printf("Selected feature %u (%zu in set)\n", fid, s.selectedIds.size());
                }
            }
        }

        // Modeling mode: place primitive snapped to grid.
        if(s.app->orchestrator().activeModeId() == "modeling" && button == GLFW_MOUSE_BUTTON_LEFT) {
            // Route to modeling mode for placement.
        }
    } else {
        s.app->onMouseUp(button, (float)mx, (float)my);

        if(s.gizmoDragging) {
            s.gizmoDragging = false;
            s.activeGizmoAxis = TransformGizmo::Axis::None;
            printf("Gizmo drag end\n");
            return;
        }

        s.mouseDragging = false;

        // Sketch mode: add point or multi-click primitive.
        if(s.sketchActive && button == GLFW_MOUSE_BUTTON_LEFT) {
            auto& cwp = s.app->context().cursorWorldPos;
            double sx = cwp.x, sy = cwp.z;
            if(s.snapToGrid) { sx = std::round(sx); sy = std::round(sy); }
            switch(s.sketchPrimitive) {
                case AppState::SketchPrim::Point:
                    (void)s.sketcher->addPoint(sx, sy);
                    printf("Sketch point: (%.2f, %.2f)\n", sx, sy);
                    break;
                case AppState::SketchPrim::Rectangle:
                    if(!s.sketchPrimPending) {
                        s.sketchPrimPending = true;
                        s.sketchPrimX = sx; s.sketchPrimY = sy;
                        printf("Rect corner 1: (%.2f, %.2f) — click opposite corner\n", sx, sy);
                    } else {
                        double ox = s.sketchPrimX, oy = s.sketchPrimY;
                        double w = sx - ox, h = sy - oy;
                        if(std::fabs(w) < 0.1 && std::fabs(h) < 0.1) break;
                        (void)s.sketcher->addRectangle(ox, oy, w, h);
                        s.sketchPrimPending = false;
                        printf("Rect: (%.2f,%.2f) w=%.2f h=%.2f\n", ox, oy, w, h);
                    }
                    break;
                case AppState::SketchPrim::Circle:
                    if(!s.sketchPrimPending) {
                        s.sketchPrimPending = true;
                        s.sketchPrimX = sx; s.sketchPrimY = sy;
                        printf("Circle center: (%.2f, %.2f) — click edge\n", sx, sy);
                    } else {
                        double dx = sx - s.sketchPrimX, dy = sy - s.sketchPrimY;
                        double r = std::sqrt(dx*dx + dy*dy);
                        if(r < 0.1) break;
                        (void)s.sketcher->addCircle(s.sketchPrimX, s.sketchPrimY, r);
                        s.sketchPrimPending = false;
                        printf("Circle: cx=%.2f cy=%.2f r=%.2f\n", s.sketchPrimX, s.sketchPrimY, r);
                    }
                    break;
            }
        }
    }
}

void cursorPosCallback(GLFWwindow* w, double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
    if(ImGui::GetIO().WantCaptureMouse) return;
    auto& s = *g_state;
    if(s.gizmoDragging) {
        Vec3 rayOrigin, rayDir;
        screenToWorldRay((float)x, (float)y, rayOrigin, rayDir);
        Vec3 curWorldPos = rayOrigin + rayDir * 5.f;
        float amount = axisDisplacement(s.gizmoStartWorldPos, curWorldPos, s.activeGizmoAxis);
        if(std::fabs(amount) > 0.0005f) {
            switch(s.gizmo.mode()) {
                case TransformGizmo::Mode::Translate:
                    if(s.snapToGrid) amount = std::round(amount);
                    s.gizmo.translate(s.gizmoWorldCenter, s.activeGizmoAxis, amount,
                                      s.app->document(), s.selectedId);
                    break;
                case TransformGizmo::Mode::Scale: {
                    float factor = 1.f + amount * 0.3f;
                    if(factor < 0.01f) factor = 0.01f;
                    s.gizmo.scale(s.gizmoWorldCenter, s.activeGizmoAxis, factor,
                                  s.app->document(), s.selectedId);
                    break;
                }
                case TransformGizmo::Mode::Rotate: {
                    float angle = amount * 0.5f;
                    s.gizmo.rotate(s.gizmoWorldCenter, s.activeGizmoAxis, angle,
                                   s.app->document(), s.selectedId);
                    break;
                }
            }
            s.gizmoStartWorldPos = curWorldPos;
        }
        s.app->onMouseMove((float)x, (float)y);
        s.lastMouseX = (float)x; s.lastMouseY = (float)y;
        return;
    }
    if(s.mouseDragging) {
        float dx = (float)(x - s.lastMouseX);
        float dy = (float)(y - s.lastMouseY);
        s.viewer->camera().orbit(dx * 3.f, dy * 3.f);
    }
    s.app->onMouseMove((float)x, (float)y);
    // Send MouseDrag instead of MouseMove when a button is held.
    if(s.mouseDragging) {
        InputEvent ev; ev.type=InputEventType::MouseDrag; ev.position={(float)x,(float)y,0};
        s.app->orchestrator().routeInput(ev);
    }
    s.lastMouseX = (float)x; s.lastMouseY = (float)y;

    // Compute world-space intersection with active working plane.
    {
        Vec3 origin, dir;
        screenToWorldRay((float)x, (float)y, origin, dir);
        // Intersect with the active working plane.
        Vec3 planeN{0,1,0}; // XZ default
        auto wp = s.app->context().workPlane;
        if(wp == AppContext::WorkPlane::XY) planeN = {0,0,1};
        else if(wp == AppContext::WorkPlane::YZ) planeN = {1,0,0};
        float denom = dir.dot(planeN);
        if(std::fabs(denom) > 1e-6f) {
            float t = -origin.dot(planeN) / denom;
            if(t > 0) {
                s.app->context().cursorWorldPos = origin + dir * t;
                bool ctrl = (glfwGetKey(s.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                             glfwGetKey(s.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
                if(ctrl) {
                    s.app->context().cursorWorldPos = snapToGeometry(
                        s.app->document(), s.app->context().cursorWorldPos, s.snapMode, 1.5f);
                }
                s.app->context().viewportWidth = (float)s.width;
                s.app->context().viewportHeight = (float)s.height;
            }
        }
    }
}

void scrollCallback(GLFWwindow* w, double xoff, double yoff) {
    ImGui_ImplGlfw_ScrollCallback(w, xoff, yoff);
    if(ImGui::GetIO().WantCaptureMouse) return;
    g_state->viewer->camera().zoom((float)yoff * 100.f);
}

void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_state->width = w; g_state->height = h;
    glViewport(0, 0, w, h);
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    // Init GLFW.
    if(!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API); // simple OpenGL for now
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Nexus Modeling", nullptr, nullptr);
    if(!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    EditorUI::initialize(window);

    // Init application.
    ModelingApplication mdlApp;
    CadViewer viewerApp(mdlApp.document(), mdlApp.selection());
    CadAutoConstraintSketch sketchApp(mdlApp.document());

    AppState state;
    state.app = &mdlApp;
    state.viewer = &viewerApp;
    state.sketcher = &sketchApp;
    state.window = window;
    g_state = &state;

    if(!state.app->initialize()) { glfwTerminate(); return 1; }
    state.app->context().snapToGrid = state.snapToGrid;

    // Set callbacks.
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    printf("Nexus Modeling — Window opened. 1280x720\n");
    printf("  Keys: ESC=select  S=sketch  E=extrude  F=fillet\n");
    printf("  Mouse: drag=orbit  scroll=zoom  click=sketch point\n");

    // Render loop.
    while(!glfwWindowShouldClose(window)) {
        EditorUI::beginFrame();

        // 3D view setup.
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Lighting setup.
        if(state.lighting) {
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            GLfloat ambient[]  = {0.2f, 0.2f, 0.25f, 1.f};
            GLfloat diffuse[]  = {0.7f, 0.65f, 0.6f, 1.f};
            GLfloat lightPos[] = {5.f, 10.f, 8.f, 0.f}; // directional from upper-right
            glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            GLfloat matAmb[] = {0.3f,0.3f,0.35f,1.f};
            GLfloat matDif[] = {0.7f,0.7f,0.7f,1.f};
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDif);
            glShadeModel(GL_SMOOTH);
            glEnable(GL_NORMALIZE);
        } else {
            glDisable(GL_LIGHTING);
        }

        // Projection and modelview for the camera.
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)state.width / (float)state.height;
        if(state.ortho) {
            float dist = state.viewer->camera().distance();
            float halfH = dist * 0.6f;
            float halfW = halfH * aspect;
            glOrtho(-halfW, halfW, -halfH, halfH, 0.1, 1000.0);
        } else {
            gluPerspective(45.0, aspect, 0.1, 1000.0);
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        auto& cam = state.viewer->camera();
        gluLookAt(cam.position().x, cam.position().y, cam.position().z,
                  cam.target().x,   cam.target().y,   cam.target().z,
                  cam.up().x,       cam.up().y,       cam.up().z);

        // Animation playback.
        if(state.animPlaying) {
            float now = (float)glfwGetTime();
            if(now - state.lastFrameTime > 0.05f) { // ~20fps
                state.lastFrameTime = now;
                state.animFrame++;
                if(state.animFrame > state.animMaxFrame) state.animFrame = 0;
                state.animTime = (float)state.animFrame;
            }
        }
        // Apply keyframe interpolation to meshes.
        auto& kfs = state.keyframes;
        for(auto& [fid, keys] : kfs) {
            if(keys.empty()) continue;
            auto* node = state.app->document().history().node(fid);
            if(!node || !node->mesh) continue;
            // Find bracketing keyframes.
            const decltype(keys)::value_type* prev = nullptr, *next = nullptr;
            for(auto& k : keys) {
                if(k.frame <= state.animFrame) prev = &k;
                if(k.frame >= state.animFrame && !next) next = &k;
            }
            if(!prev && !next) continue;
            if(!next) next = prev;
            if(!prev) prev = next;
            Vec3 target;
            if(prev == next) target = prev->position;
            else {
                float t = (prev->frame == next->frame) ? 0.f :
                    (float)(state.animFrame - prev->frame) / (float)(next->frame - prev->frame);
                target = prev->position + (next->position - prev->position) * std::clamp(t, 0.f, 1.f);
            }
            // Move mesh to target.
            auto currentCenter = node->mesh->computeBounds().center();
            Vec3 delta = target - currentCenter;
            auto pos = node->mesh->attributes().positions();
            for(auto& v : pos) { v.x += delta.x; v.y += delta.y; v.z += delta.z; }
            node->mesh->attributes().setPositions(std::move(pos));
        }
        GridOptions gridOpts;
        gridOpts.workPlane = (state.app->context().workPlane == AppContext::WorkPlane::XZ) ? kWorkPlaneXZ :
                             (state.app->context().workPlane == AppContext::WorkPlane::XY) ? kWorkPlaneXY : kWorkPlaneYZ;
        gridOpts.spacing = state.gridSpacing;
        gridOpts.extent = state.gridExtent;
        ViewportGrid grid;
        grid.render(gridOpts);

        // Draw all document feature meshes with per-feature color + selection.
        for(size_t i=1; i<=state.app->document().history().featureCount(); ++i) {
            auto* node = state.app->document().history().node(static_cast<FeatureId>(i));
            if(!node || !node->mesh || node->deleted || node->hidden) continue;
            const auto& pos = node->mesh->attributes().positions();
            const auto& topo = node->mesh->topology();

            bool isSelected = (static_cast<FeatureId>(i) == state.selectedId);
            Vec3 color = isSelected ? Vec3{1.f, 0.6f, 0.1f} : state.selectedColor;
            // Use per-feature material albedo for color when not selected.
            if(!isSelected) {
                color.x = node->material.albedo[0];
                color.y = node->material.albedo[1];
                color.z = node->material.albedo[2];
            }

            // Shaded fill with color.
            glColor3f(color.x, color.y, color.z);
            if(state.lighting) {
                GLfloat matAmb[] = {color.x*0.4f, color.y*0.4f, color.z*0.4f, 1.f};
                GLfloat matDif[] = {color.x, color.y, color.z, 1.f};
                GLfloat matSpec[] = {node->material.roughness*0.5f, node->material.roughness*0.5f, node->material.roughness*0.5f, 1.f};
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDif);
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (1.f - node->material.roughness) * 128.f);
            }
            glBegin(GL_TRIANGLES);
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                for(size_t j=0; j+2<face.vertexCount(); ++j) {
                    auto& v0 = pos[face.indices[0]];
                    auto& v1 = pos[face.indices[j+1]];
                    auto& v2 = pos[face.indices[j+2]];
                    if(state.lighting) {
                        Vec3 fn = (v1-v0).cross(v2-v0).normalize();
                        glNormal3f(fn.x, fn.y, fn.z);
                    }
                    glVertex3f(v0.x, v0.y, v0.z);
                    glVertex3f(v1.x, v1.y, v1.z);
                    glVertex3f(v2.x, v2.y, v2.z);
                }
            }
            glEnd();

            // Wireframe overlay.
            glColor3f(0.1f, 0.1f, 0.15f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glBegin(GL_TRIANGLES);
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                for(size_t j=0; j+2<face.vertexCount(); ++j) {
                    auto& v0 = pos[face.indices[0]];
                    auto& v1 = pos[face.indices[j+1]];
                    auto& v2 = pos[face.indices[j+2]];
                    glVertex3f(v0.x, v0.y, v0.z);
                    glVertex3f(v1.x, v1.y, v1.z);
                    glVertex3f(v2.x, v2.y, v2.z);
                }
            }
            glEnd();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // Selection highlight with gizmo handle.
            if(isSelected) {
                g_highlight.render(state.app->document(), state.selectedId);
            }
        }

        // Draw ghost preview mesh if set.
        auto& preview = state.app->context().previewMesh;
        if(preview.has_value() && preview->isValid()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.8f, 0.6f, 0.2f, 0.35f);
            const auto& pp = preview->attributes().positions();
            const auto& pt = preview->topology();
            glBegin(GL_TRIANGLES);
            for(uint32_t fi=0; fi<pt.faceCount(); ++fi) {
                const auto& face = pt.face(fi);
                for(size_t j=0; j+2<face.vertexCount(); ++j) {
                    auto& v0=pp[face.indices[0]], &v1=pp[face.indices[j+1]], &v2=pp[face.indices[j+2]];
                    glVertex3f(v0.x,v0.y,v0.z); glVertex3f(v1.x,v1.y,v1.z); glVertex3f(v2.x,v2.y,v2.z);
                }
            }
            glEnd();
            glDisable(GL_BLEND);
        }

        // Draw sketch points if active.
        if(state.sketchActive) {
            SketchPreview sketchPreview;
            sketchPreview.render(*state.sketcher);
        }

        // Draw dimension overlay.
        auto& dimCtx = state.app->context();
        if(dimCtx.dimActive) {
            auto& p1 = dimCtx.dimP1, &p2 = dimCtx.dimP2;
            glLineWidth(2.f);
            glColor3f(1.f, 0.9f, 0.25f);
            glBegin(GL_LINES);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
            glEnd();
            float cr = 0.15f;
            glBegin(GL_LINES);
            glVertex3f(p1.x-cr,p1.y,p1.z); glVertex3f(p1.x+cr,p1.y,p1.z);
            glVertex3f(p1.x,p1.y-cr,p1.z); glVertex3f(p1.x,p1.y+cr,p1.z);
            glVertex3f(p1.x,p1.y,p1.z-cr); glVertex3f(p1.x,p1.y,p1.z+cr);
            glVertex3f(p2.x-cr,p2.y,p2.z); glVertex3f(p2.x+cr,p2.y,p2.z);
            glVertex3f(p2.x,p2.y-cr,p2.z); glVertex3f(p2.x,p2.y+cr,p2.z);
            glVertex3f(p2.x,p2.y,p2.z-cr); glVertex3f(p2.x,p2.y,p2.z+cr);
            glEnd();
            glLineWidth(1.f);
        }

        // Editor UI (menu bar, toolbar, status bar, panels).
        EditorUI::renderMenuBar(state.app->context(), state.gizmo,
                                 state.app->orchestrator().viewport());
        EditorUI::renderToolbar(state.app->context(), state.gizmo);
        EditorUI::renderOutliner(state.app->context(), state.selectedId);
        EditorUI::renderProperties(state.app->context(), state.selectedId);
        EditorUI::renderContextMenu(state.app->context(), state.gizmo, state.selectedId);
        EditorUI::renderStatusBar(state.app->context(), state.snapToGrid,
                                   state.selectedId);
        EditorUI::renderTimeline(state.selectedId, state.animPlaying,
                                  state.animFrame, state.animMaxFrame);
        EditorUI::endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    EditorUI::shutdown();
    glfwTerminate();
    return 0;
}
