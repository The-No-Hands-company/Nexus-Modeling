#include <nexus/app/EditorUI.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/geometry/MeshBoolean.h>

#include <cstdio>
#include <vector>

namespace nexus::app {

void EditorUI::initialize(GLFWwindow* w) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(w, false);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
}

void EditorUI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Helper: place a primitive mesh into the document as a feature.
static void placePrimitive(AppContext& ctx, int primType) {
    using namespace nexus::geometry;
    Mesh prim;
    const char* name = "?";
    switch(primType) {
        case 0: prim = primitives::makeBox(2,2,2);     name="Box"; break;
        case 1: prim = primitives::makeSphere(1.5f);   name="Sphere"; break;
        case 2: prim = primitives::makeCylinder(1,3);  name="Cylinder"; break;
        case 3: prim = primitives::makeCone(1,3);      name="Cone"; break;
        case 4: prim = primitives::makeTorus(2,0.5f);  name="Torus"; break;
        case 5: prim = primitives::makePlane(4,4);     name="Plane"; break;
    }
    if(!prim.isValid() || !ctx.document) return;
    // Position at cursor world position.
    auto pos = prim.attributes().positions();
    auto& cwp = ctx.cursorWorldPos;
    for(auto& v : pos) { v.x += cwp.x; v.y += cwp.y; v.z += cwp.z; }
    prim.attributes().setPositions(std::move(pos));
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(prim)); node->dirty = false; }
    printf("Placed %s at (%.2f, %.2f, %.2f)\n", name, cwp.x, cwp.y, cwp.z);
}

static void exportObj(AppContext& ctx) {
    FILE* f = fopen("export.obj","w");
    if(!f) { printf("Cannot open export.obj\n"); return; }
    fprintf(f,"# Nexus Modeling export\n");
    size_t totalV=0,totalF=0;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(fc); ++i) {
        auto* n = hist.node(i);
        if(!n||!n->mesh||n->deleted||n->hidden) continue;
        const auto& pos = n->mesh->attributes().positions();
        const auto& topo = n->mesh->topology();
        uint32_t voff = static_cast<uint32_t>(totalV);
        for(const auto& v : pos) fprintf(f,"v %f %f %f\n",v.x,v.y,v.z);
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            fprintf(f,"f");
            for(size_t j=0; j<face.vertexCount(); ++j)
                fprintf(f," %u", face.indices[j]+1+voff);
            fprintf(f,"\n");
        }
        totalV += pos.size(); totalF += topo.faceCount();
    }
    fclose(f);
    printf("Exported OBJ: %zu verts, %zu faces → export.obj\n", totalV, totalF);
}

static void importObj(AppContext& ctx) {
    FILE* f = fopen("import.obj","r");
    if(!f) { printf("Cannot open import.obj\n"); return; }
    std::vector<Vec3> verts;
    std::vector<std::vector<uint32_t>> faces;
    char line[512];
    while(fgets(line,sizeof(line),f)) {
        if(line[0]=='v' && line[1]==' ') {
            float x,y,z; if(sscanf(line+2,"%f%f%f",&x,&y,&z)==3) verts.emplace_back(x,y,z);
        } else if(line[0]=='f' && line[1]==' ') {
            std::vector<uint32_t> fi; char* p=line+2; int idx;
            while(sscanf(p,"%d",&idx)==1) {
                fi.push_back(static_cast<uint32_t>(idx>0?idx-1:0));
                while(*p && *p!=' ') ++p;
                while(*p==' ') ++p;
                if(!*p||*p=='\n') break;
            }
            if(fi.size()>=3) faces.push_back(std::move(fi));
        }
    }
    fclose(f);
    if(verts.empty()||faces.empty()) { printf("OBJ empty\n"); return; }
    nexus::geometry::Mesh mesh;
    mesh.attributes().setPositions(std::move(verts));
    for(auto& fc : faces) {
        nexus::geometry::Face face; face.indices = std::move(fc);
        mesh.topology().addFace(std::move(face));
    }
    (void)mesh.computeVertexNormals();
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(mesh)); node->dirty=false; }
    printf("Imported OBJ: %zu faces → feature %u\n", faces.size(), fid);
}

// Apply boolean operation between selected feature and most recent other feature.
static void applyBool(AppContext& ctx, FeatureId selectedId, nexus::geometry::BooleanOp op) {
    using namespace nexus::geometry;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    if(fc < 2 || selectedId == nexus::parametric::kInvalidFeatureId) {
        printf("Boolean: need at least 2 features with 1 selected\n"); return;
    }
    FeatureId otherId = static_cast<FeatureId>(fc); // most recent
    if(otherId == selectedId) {
        otherId = static_cast<FeatureId>(fc - 1);
        if(otherId == nexus::parametric::kInvalidFeatureId) { printf("Boolean: no second feature\n"); return; }
    }
    auto* nodeA = hist.node(selectedId);
    auto* nodeB = hist.node(otherId);
    if(!nodeA || !nodeB || !nodeA->mesh || !nodeB->mesh) {
        printf("Boolean: both features must have meshes\n"); return;
    }
    auto result = MeshBoolean::compute(*nodeA->mesh, *nodeB->mesh, op);
    if(!result.ok) { printf("Boolean failed: %s\n", result.error.c_str()); return; }
    const char* opName = (op==BooleanOp::Union)?"Union":(op==BooleanOp::Difference)?"Difference":"Intersection";
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = hist.node(fid);
    if(node) { node->mesh.emplace(std::move(result.result)); node->dirty = false; }
    printf("Boolean %s: feature %u = %u %s %u\n", opName, fid, selectedId, opName, otherId);
}

bool EditorUI::renderMenuBar(AppContext& ctx, TransformGizmo& gizmo,
                               ViewportController& viewport) {
    bool action = false;
    if(!ImGui::BeginMainMenuBar()) return false;

    if(ImGui::BeginMenu("File")) {
        if(ImGui::MenuItem("Save", "Ctrl+S")) {
            auto data = ctx.document->serialize();
            FILE* f = fopen("scene.nxm","wb");
            if(f){fwrite(data.data(),1,data.size(),f);fclose(f); printf("Saved\n");}
            action=true;
        }
        if(ImGui::MenuItem("Load", "Ctrl+O")) {
            FILE* f = fopen("scene.nxm","rb");
            if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
                std::vector<uint8_t> d(sz);fread(d.data(),1,sz,f);fclose(f);
                (void)ctx.document->deserialize(d.data(),d.size()); printf("Loaded\n");}
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Export OBJ")) {
            exportObj(ctx);
            action=true;
        }
        if(ImGui::MenuItem("Import OBJ")) {
            importObj(ctx);
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Quit")) { glfwSetWindowShouldClose(glfwGetCurrentContext(),1); }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Edit")) {
        if(ImGui::MenuItem("Undo","Ctrl+Z")) { ctx.document->undo(); action=true; }
        if(ImGui::MenuItem("Redo","Ctrl+Y"))  { ctx.document->redo(); action=true; }
        ImGui::Separator();
        if(ImGui::MenuItem("Delete","Del")) { if(ctx.document->canUndo()) ctx.document->undo(); action=true; }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("View")) {
        const char* vp[]={"Wireframe","Solid","Shaded","Material","Rendered","XRay","HiddenLine"};
        for(int i=0;i<7;++i)
            if(ImGui::MenuItem(vp[i],nullptr,static_cast<int>(viewport.mode())==i))
                viewport.setMode(static_cast<ViewportMode>(i));
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Create")) {
        if(ImGui::MenuItem("Box"))      placePrimitive(ctx,0);
        if(ImGui::MenuItem("Sphere"))   placePrimitive(ctx,1);
        if(ImGui::MenuItem("Cylinder")) placePrimitive(ctx,2);
        if(ImGui::MenuItem("Cone"))     placePrimitive(ctx,3);
        if(ImGui::MenuItem("Torus"))    placePrimitive(ctx,4);
        if(ImGui::MenuItem("Plane"))    placePrimitive(ctx,5);
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Mode")) {
        const char* ids[]={"select","sketch","extrude","revolve","fillet","modeling","dimension","face-edit","edge-edit","vertex-edit","boolean","pattern","mirror"};
        const char* names[]={"Select","Sketch","Extrude","Revolve","Fillet","Modeling","Dimension","FaceEdit","EdgeEdit","VertexEdit","Boolean","Pattern","Mirror"};
        const int n = 13;
        for(int i=0;i<n;++i) {
            bool active = ctx.orchestrator && ctx.orchestrator->activeModeId()==ids[i];
            if(ImGui::MenuItem(names[i],nullptr,active)) {
                if(ctx.orchestrator) ctx.orchestrator->switchTo(ids[i]);
                printf("Mode: %s\n", ids[i]);
            }
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Gizmo")) {
        bool t = gizmo.mode()==TransformGizmo::Mode::Translate;
        bool s = gizmo.mode()==TransformGizmo::Mode::Scale;
        bool r = gizmo.mode()==TransformGizmo::Mode::Rotate;
        if(ImGui::MenuItem("Translate","W",&t)) gizmo.setMode(TransformGizmo::Mode::Translate);
        if(ImGui::MenuItem("Scale","E",&s))     gizmo.setMode(TransformGizmo::Mode::Scale);
        if(ImGui::MenuItem("Rotate","R",&r))    gizmo.setMode(TransformGizmo::Mode::Rotate);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return action;
}

void EditorUI::renderToolbar(AppContext& ctx, TransformGizmo& gizmo) {
    ImGui::Begin("Toolbar",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ctx.orchestrator) {
        if(ImGui::Button("Select"))   ctx.orchestrator->switchTo("select");
        ImGui::SameLine();
        if(ImGui::Button("Sketch"))   ctx.orchestrator->switchTo("sketch");
        ImGui::SameLine();
        if(ImGui::Button("Modeling")) ctx.orchestrator->switchTo("modeling");
        ImGui::SameLine();
    }
    ImGui::Separator(); ImGui::SameLine();
    if(ImGui::Button("Translate")) gizmo.setMode(TransformGizmo::Mode::Translate);
    ImGui::SameLine();
    if(ImGui::Button("Rotate")) gizmo.setMode(TransformGizmo::Mode::Rotate);
    ImGui::SameLine();
    if(ImGui::Button("Scale")) gizmo.setMode(TransformGizmo::Mode::Scale);
    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
    if(ImGui::SmallButton("Box"))      { placePrimitive(ctx,0); } ImGui::SameLine();
    if(ImGui::SmallButton("Sphere"))   { placePrimitive(ctx,1); } ImGui::SameLine();
    if(ImGui::SmallButton("Cyl"))      { placePrimitive(ctx,2); } ImGui::SameLine();
    if(ImGui::SmallButton("Cone"))     { placePrimitive(ctx,3); } ImGui::SameLine();
    if(ImGui::SmallButton("Torus"))    { placePrimitive(ctx,4); } ImGui::SameLine();
    if(ImGui::SmallButton("Plane"))    { placePrimitive(ctx,5); }
    ImGui::End();
}

void EditorUI::renderContextMenu(AppContext& ctx, TransformGizmo& gizmo, FeatureId selectedId) {
    (void)gizmo;
    if(!ImGui::BeginPopupContextVoid("ViewportContext", ImGuiPopupFlags_MouseButtonRight)) return;
    if(ImGui::MenuItem("Create Box"))      placePrimitive(ctx,0);
    if(ImGui::MenuItem("Create Sphere"))   placePrimitive(ctx,1);
    if(ImGui::MenuItem("Create Cylinder")) placePrimitive(ctx,2);
    if(ImGui::MenuItem("Create Cone"))     placePrimitive(ctx,3);
    if(ImGui::MenuItem("Create Torus"))    placePrimitive(ctx,4);
    if(ImGui::MenuItem("Create Plane"))    placePrimitive(ctx,5);
    ImGui::Separator();
    if(ImGui::MenuItem("Delete Selected")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId)
            ctx.document->deleteFeature(selectedId);
    }
    if(ImGui::MenuItem("Instance")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                nexus::geometry::Mesh copy = *n->mesh;
                auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
                auto fid = ctx.document->addSketch(sk);
                auto* newNode = ctx.document->history().node(fid);
                if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
                printf("Instanced %u → %u\n", selectedId, fid);
            }
        }
    }
    ImGui::Separator();
    if(ImGui::BeginMenu("Boolean")) {
        if(ImGui::MenuItem("Union", "Shift+U"))     applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Union);
        if(ImGui::MenuItem("Difference", "Shift+D")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Difference);
        if(ImGui::MenuItem("Intersection", "Shift+I")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Intersection);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if(ImGui::MenuItem("View All", nullptr, false, true)) {
        auto& cam = ctx.cameraPosition;
        cam = {0,0,10};
    }
    ImGui::EndPopup();
}

void EditorUI::renderStatusBar(const AppContext& ctx, bool snapToGrid, FeatureId selectedId) {
    ImGui::Begin("Status",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize);
    int fc = ctx.document ? (int)ctx.document->history().featureCount() : 0;
    const char* mode = ctx.orchestrator ? ctx.orchestrator->activeModeId().c_str() : "none";
    const char* plane = (ctx.workPlane == AppContext::WorkPlane::XZ) ? "XZ" :
                        (ctx.workPlane == AppContext::WorkPlane::XY) ? "XY" : "YZ";
    ImGui::Text("Objects: %d | Sel: %llu | Snap: %s | Plane: %s | Mode: %s", fc,
        static_cast<unsigned long long>(selectedId), snapToGrid ? "ON" : "OFF", plane, mode);
    ImGui::End();
}

void EditorUI::renderOutliner(AppContext& ctx, FeatureId& selectedId) {
    ImGui::Begin("Outliner");
    const auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    for(FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* n = hist.node(i);
        if(!n) continue;
        const char* kindStr = "?";
        switch(n->kind) {
            case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
            case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
            case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
        }
        char label[128];
        (void)snprintf(label,sizeof(label),"[%u] %s",i,kindStr);
        bool sel = (selectedId == i);
        if(ImGui::Selectable(label,sel) && !sel) { selectedId = i; }
    }
    ImGui::End();
}

void EditorUI::renderProperties(AppContext& ctx, FeatureId selectedId) {
    ImGui::Begin("Inspector");
    if(selectedId == nexus::parametric::kInvalidFeatureId || ctx.document->history().featureCount() == 0) {
        ImGui::TextUnformatted("No object selected.");
        ImGui::End(); return;
    }
    auto* n = ctx.document->history().node(selectedId);
    if(!n || !n->mesh) { ImGui::TextUnformatted("Selected object not found."); ImGui::End(); return; }

    const char* kindStr = "?";
    switch(n->kind) {
        case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
        case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
        case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
    }
    ImGui::Text("Feature ID: %u", selectedId);
    ImGui::Text("Type: %s", kindStr);
    // Editable name.
    char nameBuf[128];
    snprintf(nameBuf,sizeof(nameBuf),"%s",n->name.c_str());
    if(ImGui::InputText("Name",nameBuf,sizeof(nameBuf))) {
        ctx.document->history().setName(selectedId, nameBuf);
    }
    ImGui::Text("Vertices: %zu", n->mesh->attributes().vertexCount());
    ImGui::Text("Faces: %zu", n->mesh->topology().faceCount());

    ImGui::Separator();
    if(ImGui::Button("Instance")) {
        nexus::geometry::Mesh copy = *n->mesh;
        auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
        auto fid = ctx.document->addSketch(sk);
        auto* newNode = ctx.document->history().node(fid);
        if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
        printf("Instanced feature %u → %u\n", selectedId, fid);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Transform");

    auto bounds = n->mesh->computeBounds();
    Vec3 center = bounds.center();
    float pos[3] = {center.x, center.y, center.z};
    if(ImGui::DragFloat3("Position", pos, 0.1f)) {
        Vec3 delta{pos[0]-center.x, pos[1]-center.y, pos[2]-center.z};
        auto verts = n->mesh->attributes().positions();
        for(auto& v : verts) { v.x+=delta.x; v.y+=delta.y; v.z+=delta.z; }
        n->mesh->attributes().setPositions(std::move(verts));
    }

    Vec3 extents = bounds.extents();
    float scl[3] = {extents.x*2, extents.y*2, extents.z*2};
    ImGui::Text("Size: %.2f x %.2f x %.2f", scl[0], scl[1], scl[2]);

    ImGui::Separator();
    ImGui::TextUnformatted("Material (PBR)");
    ImGui::ColorEdit4("Albedo", n->material.albedo);
    ImGui::DragFloat("Roughness", &n->material.roughness, 0.01f, 0.f, 1.f);
    ImGui::DragFloat("Metallic", &n->material.metallic, 0.01f, 0.f, 1.f);

    ImGui::End();
}

void EditorUI::renderTimeline(FeatureId /*selectedId*/, bool& playing,
                                int& frame, int maxFrame) {
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ImGui::Button(playing ? "Pause" : "Play")) playing = !playing;
    ImGui::SameLine();
    if(ImGui::Button("|<")) frame = 0;
    ImGui::SameLine();
    if(ImGui::Button("<"))  frame = std::max(0, frame-1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragInt("##frame", &frame, 1.f, 0, maxFrame, "frame %d");
    ImGui::SameLine();
    if(ImGui::Button(">"))  frame = std::min(maxFrame, frame+1);
    ImGui::SameLine();
    if(ImGui::Button(">|")) frame = maxFrame;
    ImGui::SameLine();
    ImGui::Text("I=key   K=clear");
    ImGui::End();
}

} // namespace nexus::app
