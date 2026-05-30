// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation camera.* and sgraph.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/SceneGraphExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerSceneGraphCommands(h);
    return h;
}

static bool run(ScriptBatchHarness& h, const std::string& name,
                std::initializer_list<std::pair<std::string,std::string>> args,
                std::vector<std::string>& msgs)
{
    ScriptCommand cmd;
    cmd.name = name;
    for (auto& [k, v] : args) cmd.args[k] = v;
    ScriptContext ctx;
    msgs.clear();
    return h.registry().execute(ctx, cmd, msgs);
}

static bool hasMsg(const std::vector<std::string>& msgs, const std::string& sub)
{
    for (const auto& m : msgs)
        if (m.find(sub) != std::string::npos) return true;
    return false;
}

// ── camera.set_perspective ────────────────────────────────────────────────────

TEST(SceneGraphExtension, CameraSetPerspectiveDefaults)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.set_perspective", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "camera.set_perspective ok"));
    EXPECT_TRUE(hasMsg(msgs, "fov=60"));
    EXPECT_TRUE(hasMsg(msgs, "near=0.1"));
}

TEST(SceneGraphExtension, CameraSetPerspectiveCustom)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.set_perspective",
        {{"fov","90"},{"aspect","1.5"},{"near","0.01"},{"far","5000"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "fov=90"));
    EXPECT_TRUE(hasMsg(msgs, "near=0.010"));
    EXPECT_TRUE(hasMsg(msgs, "far=5000"));
}

// ── camera.set_ortho ──────────────────────────────────────────────────────────

TEST(SceneGraphExtension, CameraSetOrthoDefaults)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.set_ortho", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "camera.set_ortho ok"));
    EXPECT_TRUE(hasMsg(msgs, "width=10"));
    EXPECT_TRUE(hasMsg(msgs, "height=10"));
}

TEST(SceneGraphExtension, CameraSetOrthoCustom)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.set_ortho",
        {{"width","20"},{"height","15"},{"near","0.5"},{"far","500"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=20"));
    EXPECT_TRUE(hasMsg(msgs, "height=15"));
}

// ── camera.look_at ────────────────────────────────────────────────────────────

TEST(SceneGraphExtension, CameraLookAtDefaults)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.look_at", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "camera.look_at ok"));
    EXPECT_TRUE(hasMsg(msgs, "eye="));
    EXPECT_TRUE(hasMsg(msgs, "target="));
}

TEST(SceneGraphExtension, CameraLookAtCustom)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.look_at",
        {{"ex","10"},{"ey","5"},{"ez","10"},
         {"tx","0"},{"ty","0"},{"tz","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "eye=10.000000,5.000000,10.000000"));
    EXPECT_TRUE(hasMsg(msgs, "target=0.000000,0.000000,0.000000"));
}

// ── camera.describe ───────────────────────────────────────────────────────────

TEST(SceneGraphExtension, CameraDescribeDefault)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "camera.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "camera.describe"));
    EXPECT_TRUE(hasMsg(msgs, "px="));
    EXPECT_TRUE(hasMsg(msgs, "py="));
    EXPECT_TRUE(hasMsg(msgs, "pz="));
}

TEST(SceneGraphExtension, CameraDescribeAfterLookAt)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "camera.look_at",
        {{"ex","3"},{"ey","4"},{"ez","5"}}, msgs);
    EXPECT_TRUE(run(h, "camera.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "px=3.000000"));
    EXPECT_TRUE(hasMsg(msgs, "py=4.000000"));
    EXPECT_TRUE(hasMsg(msgs, "pz=5.000000"));
}

// ── sgraph.create_node ────────────────────────────────────────────────────────

TEST(SceneGraphExtension, CreateNodeRequiresName)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.create_node", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SceneGraphExtension, CreateNodeOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sgraph.create_node", {{"name","cube"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sgraph.create_node ok"));
    EXPECT_TRUE(hasMsg(msgs, "name=cube"));
    EXPECT_TRUE(hasMsg(msgs, "id="));
}

TEST(SceneGraphExtension, CreateNodeDuplicateNameFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sgraph.create_node", {{"name","cube"}}, msgs);
    EXPECT_FALSE(run(h, "sgraph.create_node", {{"name","cube"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SceneGraphExtension, CreateNodeWithParent)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node", {{"name","parent_node"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.create_node",
        {{"name","child_node"},{"parent","parent_node"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "name=child_node"));
}

TEST(SceneGraphExtension, CreateNodeBadParentFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.create_node",
        {{"name","child"},{"parent","nonexistent"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

// ── sgraph.remove_node ────────────────────────────────────────────────────────

TEST(SceneGraphExtension, RemoveNodeRequiresName)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.remove_node", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SceneGraphExtension, RemoveNodeNotFoundFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.remove_node", {{"name","ghost"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SceneGraphExtension, RemoveNodeOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sgraph.create_node", {{"name","doomed"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.remove_node", {{"name","doomed"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sgraph.remove_node ok name=doomed"));
}

TEST(SceneGraphExtension, RemoveNodeReducesCount)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node", {{"name","a"}}, msgs);
    ok = run(h, "sgraph.create_node", {{"name","b"}}, msgs);
    ok = run(h, "sgraph.remove_node", {{"name","a"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=1"));
}

// ── sgraph.set_transform ──────────────────────────────────────────────────────

TEST(SceneGraphExtension, SetTransformRequiresName)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.set_transform", {}, msgs));
}

TEST(SceneGraphExtension, SetTransformNotFoundFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.set_transform", {{"name","ghost"}}, msgs));
}

TEST(SceneGraphExtension, SetTransformOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sgraph.create_node", {{"name","box"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.set_transform",
        {{"name","box"},{"tx","1"},{"ty","2"},{"tz","3"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sgraph.set_transform ok name=box"));
}

TEST(SceneGraphExtension, SetTransformAppearsInDescribe)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node",    {{"name","box"}}, msgs);
    ok = run(h, "sgraph.set_transform",  {{"name","box"},{"tx","5"},{"ty","6"},{"tz","7"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "tx=5.000000"));
    EXPECT_TRUE(hasMsg(msgs, "ty=6.000000"));
    EXPECT_TRUE(hasMsg(msgs, "tz=7.000000"));
}

// ── sgraph.set_visible ────────────────────────────────────────────────────────

TEST(SceneGraphExtension, SetVisibleRequiresName)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sgraph.set_visible", {}, msgs));
}

TEST(SceneGraphExtension, SetVisibleHide)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sgraph.create_node", {{"name","box"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.set_visible", {{"name","box"},{"visible","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "visible=0"));
}

TEST(SceneGraphExtension, SetVisibleShow)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node",  {{"name","box"}}, msgs);
    ok = run(h, "sgraph.set_visible",  {{"name","box"},{"visible","0"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.set_visible", {{"name","box"},{"visible","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "visible=1"));
}

// ── sgraph.clear ─────────────────────────────────────────────────────────────

TEST(SceneGraphExtension, ClearEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sgraph.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sgraph.clear ok"));
}

TEST(SceneGraphExtension, ClearResetsNodeCount)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node", {{"name","a"}}, msgs);
    ok = run(h, "sgraph.create_node", {{"name","b"}}, msgs);
    ok = run(h, "sgraph.clear", {}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=0"));
}

// ── sgraph.describe ───────────────────────────────────────────────────────────

TEST(SceneGraphExtension, DescribeEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=0"));
}

TEST(SceneGraphExtension, DescribeAfterCreate)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sgraph.create_node", {{"name","sphere"}}, msgs);
    ok = run(h, "sgraph.create_node", {{"name","light"}},  msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=2"));
    EXPECT_TRUE(hasMsg(msgs, "name=sphere"));
    EXPECT_TRUE(hasMsg(msgs, "name=light"));
}

TEST(SceneGraphExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sgraph.create_node", {{"name","z_node"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── Integration: full scene build + camera setup ──────────────────────────────

TEST(SceneGraphExtension, FullScenePipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Configure perspective camera
    ok = run(h, "camera.set_perspective",
        {{"fov","75"},{"aspect","1.777"},{"near","0.1"},{"far","1000"}}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "fov=75"));

    // Position camera
    ok = run(h, "camera.look_at",
        {{"ex","0"},{"ey","5"},{"ez","10"},{"tx","0"},{"ty","0"},{"tz","0"}}, msgs);

    // Create scene hierarchy: root → scene_root → [mesh, light]
    ok = run(h, "sgraph.create_node", {{"name","scene_root"}}, msgs);
    ok = run(h, "sgraph.create_node", {{"name","mesh"},{"parent","scene_root"}}, msgs);
    ok = run(h, "sgraph.create_node", {{"name","light"},{"parent","scene_root"}}, msgs);

    // Position the light
    ok = run(h, "sgraph.set_transform",
        {{"name","light"},{"tx","5"},{"ty","10"},{"tz","0"}}, msgs);

    // Hide the mesh temporarily
    ok = run(h, "sgraph.set_visible", {{"name","mesh"},{"visible","0"}}, msgs);

    // Describe graph — 3 nodes
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=3"));
    EXPECT_TRUE(hasMsg(msgs, "name=mesh"));
    EXPECT_TRUE(hasMsg(msgs, "name=light"));
    EXPECT_TRUE(hasMsg(msgs, "visible=0"));  // mesh is hidden

    // Remove mesh
    ok = run(h, "sgraph.remove_node", {{"name","mesh"}}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=2"));

    // Clear and verify
    ok = run(h, "sgraph.clear", {}, msgs);
    EXPECT_TRUE(run(h, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=0"));

    // Camera describe still works after graph clear
    EXPECT_TRUE(run(h, "camera.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "py=5.000000"));
}

TEST(SceneGraphExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "sgraph.create_node", {{"name","exclusive"}}, msgs);

    // h2 should have empty graph
    EXPECT_TRUE(run(h2, "sgraph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=0"));
}
