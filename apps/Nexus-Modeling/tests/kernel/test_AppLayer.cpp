#include <gtest/gtest.h>

#include <nexus/app/AppMode.h>
#include <nexus/app/ModelingApplication.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/PrimitiveModelingMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/parametric/FeatureHistory.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <cmath>
#include <memory>
#include <string>

using namespace nexus::app;
using namespace nexus::cad;
using namespace nexus::geometry;
using namespace nexus::parametric;

// ── ModeRegistry ─────────────────────────────────────────────────────────────

TEST(AppLayer, ModeRegistryHasAllBuiltinModes)
{
    auto& reg = ModeRegistry::instance();
    EXPECT_TRUE(reg.isRegistered("select"));
    EXPECT_TRUE(reg.isRegistered("sketch"));
    EXPECT_TRUE(reg.isRegistered("extrude"));
    EXPECT_TRUE(reg.isRegistered("revolve"));
    EXPECT_TRUE(reg.isRegistered("fillet"));
    EXPECT_TRUE(reg.isRegistered("dimension"));
    EXPECT_TRUE(reg.isRegistered("modeling"));
    EXPECT_TRUE(reg.isRegistered("face-edit"));
    EXPECT_TRUE(reg.isRegistered("edge-edit"));
    EXPECT_TRUE(reg.isRegistered("vertex-edit"));
    EXPECT_TRUE(reg.isRegistered("boolean"));
    EXPECT_TRUE(reg.isRegistered("pattern"));
    EXPECT_TRUE(reg.isRegistered("mirror"));
}

TEST(AppLayer, ModeRegistryDuplicateOverwrites)
{
    auto& reg = ModeRegistry::instance();
    // Duplicate registration silently overwrites.
    int count = 0;
    reg.registerMode("_dup_test", [&] { count++; return std::unique_ptr<AppMode>{}; });
    reg.registerMode("_dup_test", [&] { count += 10; return std::unique_ptr<AppMode>{}; });
    auto m = reg.create("_dup_test");
    EXPECT_EQ(count, 10); // second factory was used
}

TEST(AppLayer, ModeRegistryCreatesValidModes)
{
    auto& reg = ModeRegistry::instance();
    for(const auto& id : {"select","sketch","modeling","mirror"}) {
        auto mode = reg.create(id);
        ASSERT_NE(mode, nullptr) << "Failed to create mode: " << id;
        EXPECT_EQ(mode->modeId(), id);
    }
}

TEST(AppLayer, ModeRegistryCreatesNullForUnknown)
{
    auto& reg = ModeRegistry::instance();
    EXPECT_EQ(reg.create("nonexistent"), nullptr);
}

// ── ModeOrchestrator ─────────────────────────────────────────────────────────

TEST(AppLayer, OrchestratorStartsWithNoActiveMode)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    EXPECT_EQ(orch.activeModeId(), "none");
}

TEST(AppLayer, OrchestratorRegistersAndSwitches)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();
    EXPECT_TRUE(orch.switchTo("select"));
    EXPECT_EQ(orch.activeModeId(), "select");
}

TEST(AppLayer, OrchestratorSwitchToUnknownFails)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();
    EXPECT_FALSE(orch.switchTo("nonexistent"));
}

TEST(AppLayer, OrchestratorRoutesInput)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();
    orch.switchTo("select");
    InputEvent ev;
    ev.type = InputEventType::MouseDown;
    ev.button = 0;
    orch.routeInput(ev);
    SUCCEED();
}

// ── AppContext ───────────────────────────────────────────────────────────────

TEST(AppLayer, AppContextDefaults)
{
    AppContext ctx;
    EXPECT_EQ(ctx.document, nullptr);
    EXPECT_EQ(ctx.selection, nullptr);
    EXPECT_EQ(ctx.scene, nullptr);
    EXPECT_EQ(ctx.orchestrator, nullptr);
    EXPECT_FLOAT_EQ(ctx.cursorWorldPos.x, 0.f);
    EXPECT_EQ(ctx.workPlane, AppContext::WorkPlane::XZ);
    EXPECT_TRUE(ctx.snapToGrid);
    EXPECT_FALSE(ctx.previewMesh.has_value());
    EXPECT_EQ(ctx.selectedFace, ~0u);
    EXPECT_EQ(ctx.selectedVertex, ~0u);
}

// ── FeatureHistory ───────────────────────────────────────────────────────────

TEST(AppLayer, RemoveFeatureSetsDeleted)
{
    auto sk = ParametricSketchFactory::createSketch();
    CadDocument doc;
    auto fid = doc.addSketch(sk);
    ASSERT_NE(fid, kInvalidFeatureId);
    EXPECT_EQ(doc.history().featureCount(), 1u);

    EXPECT_TRUE(doc.deleteFeature(fid));
    auto* node = doc.history().node(fid);
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->deleted);
}

TEST(AppLayer, HideFeature)
{
    auto sk = ParametricSketchFactory::createSketch();
    CadDocument doc;
    auto fid = doc.addSketch(sk);
    ASSERT_NE(fid, kInvalidFeatureId);

    EXPECT_TRUE(doc.history().setHidden(fid, true));
    auto* node = doc.history().node(fid);
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->hidden);
}

TEST(AppLayer, UnhideAll)
{
    auto sk = ParametricSketchFactory::createSketch();
    CadDocument doc;
    auto f1 = doc.addSketch(sk);
    auto sk2 = ParametricSketchFactory::createSketch();
    auto f2 = doc.addSketch(sk2);
    doc.history().setHidden(f1, true);
    doc.history().setHidden(f2, true);

    EXPECT_TRUE(doc.history().unhideAll());
    EXPECT_FALSE(doc.history().node(f1)->hidden);
    EXPECT_FALSE(doc.history().node(f2)->hidden);
}

// ── PrimitiveModelingMode ────────────────────────────────────────────────────

TEST(AppLayer, PrimitiveModelingModeActions)
{
    PrimitiveModelingMode mode;
    auto actions = mode.actions();
    ASSERT_GE(actions.size(), 6u);
    EXPECT_EQ(actions[0].id, "modeling.box");
    EXPECT_EQ(actions[1].id, "modeling.sphere");
}

TEST(AppLayer, PrimitiveModelingModeExecuteAction)
{
    PrimitiveModelingMode mode;
    AppContext ctx;
    EXPECT_TRUE(mode.executeAction("modeling.box", ctx));
    EXPECT_TRUE(mode.executeAction("modeling.sphere", ctx));
    EXPECT_TRUE(mode.executeAction("modeling.cylinder", ctx));
    EXPECT_TRUE(mode.executeAction("modeling.cone", ctx));
    EXPECT_TRUE(mode.executeAction("modeling.torus", ctx));
    EXPECT_TRUE(mode.executeAction("modeling.plane", ctx));
}

// ── ViewportGrid ─────────────────────────────────────────────────────────────

TEST(AppLayer, GridOptionsDefaults)
{
    GridOptions opts;
    EXPECT_FLOAT_EQ(opts.extent, 10.f);
    EXPECT_FLOAT_EQ(opts.spacing, 1.f);
    EXPECT_TRUE(opts.drawGrid);
    EXPECT_TRUE(opts.drawAxes);
    EXPECT_EQ(opts.workPlane, kWorkPlaneXZ);
}

// ── ModelingApplication ──────────────────────────────────────────────────────

TEST(AppLayer, ModelingApplicationInit)
{
    ModelingApplication app;
    EXPECT_TRUE(app.initialize());
    EXPECT_EQ(app.orchestrator().activeModeId(), "select");
    app.shutdown();
}

TEST(AppLayer, ModelingApplicationModeSwitch)
{
    ModelingApplication app;
    app.initialize();
    EXPECT_TRUE(app.orchestrator().switchTo("modeling"));
    EXPECT_EQ(app.orchestrator().activeModeId(), "modeling");
    EXPECT_TRUE(app.orchestrator().switchTo("sketch"));
    EXPECT_EQ(app.orchestrator().activeModeId(), "sketch");
    app.shutdown();
}

// ── AppMode base class ───────────────────────────────────────────────────────

TEST(AppLayer, AppModeBaseClassActsAsPassthrough)
{
    class TestMode : public AppMode {
    public:
        [[nodiscard]] std::string modeId() const override { return "test"; }
        [[nodiscard]] std::string displayName() const override { return "Test"; }
    };
    TestMode tm;
    EXPECT_EQ(tm.modeId(), "test");
    EXPECT_EQ(tm.displayName(), "Test");
    EXPECT_TRUE(tm.actions().empty());

    AppContext ctx;
    tm.onActivate(ctx);
    EXPECT_TRUE(tm.onDeactivate(ctx));
    EXPECT_EQ(tm.handleInput(ctx, InputEvent{}), EventResult::Unconsumed);
}

// ── Material on FeatureNode ──────────────────────────────────────────────────

TEST(AppLayer, FeatureNodeMaterialDefaults)
{
    FeatureNode node;
    EXPECT_FLOAT_EQ(node.material.albedo[0], 0.7f);
    EXPECT_FLOAT_EQ(node.material.albedo[1], 0.7f);
    EXPECT_FLOAT_EQ(node.material.albedo[2], 0.7f);
    EXPECT_FLOAT_EQ(node.material.albedo[3], 1.f);
    EXPECT_FLOAT_EQ(node.material.roughness, 0.5f);
    EXPECT_FLOAT_EQ(node.material.metallic, 0.f);
}

TEST(AppLayer, FeatureNodeDeletedDefaults)
{
    FeatureNode node;
    EXPECT_FALSE(node.deleted);
    EXPECT_FALSE(node.hidden);
    EXPECT_TRUE(node.dirty);
}

TEST(AppLayer, FeatureHistoryRemoveFeatureNullId)
{
    // Removing a non-existent feature should fail gracefully.
    CadDocument doc;
    EXPECT_FALSE(doc.deleteFeature(999));
}
